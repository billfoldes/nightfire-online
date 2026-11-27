// Co-op on Driving.xbe's mounted guns (Paris's van, Alpine Escape's JL-7): in a session the other players each have a
// reticle and fire player 1's gun with it, from where player 1's camera looks.
//
// A shot is a task (vtable 0x18C520, 0x18 bytes: +4 the barrel it leaves, +8 the weapon, +0xC, +0x10, +0x14 the gun)
// that its destructor 0x4AC40 resolves at once: player 1's gun ([0x239220]+0x478 set; the gun's +0x138 bit 2, the
// enemies' guns have bit 1 alone) fires from the camera [[0x1EC488]+4] (+0x40 its place) along its aim, the unit
// vector +0x180 (w 0; 0x81890(1.0) returns it), through the hitscan 0xB5C90 with the hit's effects (0x749B0), the
// sound and the tracer.
// Player 1's come from the gun's animation (0xBB2B0 -> 0x22DD0), the weapon table's +0x3C ticks apart ([0x23923C]:
// +4 the weapon, +0x10 the table, 0x54 a weapon), the barrels taking turns. Another player's shot runs a copy of
// player 1's last task with the camera's aim turned to theirs for the call: the game's own shot, its damage and
// effects, on every machine at the same tick (0xBBDC0, the weapons' update, each frame). Their aim is kept as
// tangents right and up of the camera's, the same on every machine whatever its screen; the camera has no roll (its
// mount's right +0x10 is up x forward, y up, left handed). Their machine draws the reticle where its view projects
// that direction (GameInfo::ui; 0xE46D0's angles).
// In the car (Enemies Vanquished) the same: player 1's car shots are tasks with no gun (+0x14 0) from the car's two
// muzzles (+4), the shooter the weapon's +0x28, the player [[0x234E40]]; 0x4AC40 takes a muzzle's world matrix
// (0x8ECA0, returning to 0x4ACC9) and fires along its row 1. Another player's copy turns that row toward the point
// their reticle covers, 100 units along the chase camera's view (the view's camera [[[0x1EC488]]+4]: +0x10 right,
// +0x20 up, +0x30 forward, +0x40 its place).
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "core/snapshot.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace net = nf::net;

namespace {
constexpr uint32_t kGame = 0x239220, kPovMode = 0x478, kPaused = 0x47C, kState = 0x234E24, kTime = 0x234E34;
constexpr uint32_t kCameras = 0x1EC488, kWeapons = 0x23923C, kControlMap = 0x1E455C;
constexpr uint32_t kShotVtable = 0x18C520, kCamPlace = 0x40, kCamAim = 0x180;
constexpr float kMaxX = 1.20f, kMaxY = 0.70f;  // the aim's reach, tangents (the game's 4:3 view is 0.65 by 0.49): past
                                               // the screen's edge, as each player's own view turns with it (OwnView)
constexpr float kStickRate = 0.016f;           // tangent a tick at full deflection
constexpr float kMouseRate = 0.0008f;          // tangent a count (default.xbe's radians a count)
constexpr int kMaxExtra = net::kMaxPlayers;

float GetF(uint32_t va) {
    float f;
    std::memcpy(&f, &kernel::G<uint32_t>(va), 4);
    return f;
}
void SetF(uint32_t va, float f) { std::memcpy(&kernel::G<uint32_t>(va), &f, 4); }

struct Aim {
    float x = 0, y = 0;  // tangents across the gun's view (player 1's)
    uint32_t next = 0;   // the game time the gun may fire for them again
    float dir[3] = {};   // where it points in the world (0 none yet): the gun's turning does not carry it
};
Aim g_aim[kMaxExtra];
uint32_t g_task[6];      // player 1's last shot
uint32_t g_barrel[2];    // the barrels their shots left from
int g_barrels = 0, g_turn = 0;
bool g_have_task = false;
uint32_t g_task_time = 0, g_task_gun_class = 0, g_task_gap = 0;  // g_task_gap: the game time between two of them

std::atomic<uint32_t> g_shown{0};  // the players whose reticles are drawn, a bit each (g_show_x/y theirs)
std::atomic<float> g_show_x[kMaxExtra], g_show_y[kMaxExtra], g_tan_x{0.65f}, g_tan_y{0.49f};

// NF_COOP_TEST=x,y (the harness, alone): a second player aiming there and firing whenever the gun can
bool g_test = false;
float g_test_x = 0, g_test_y = 0;
bool g_in_shot = false;
uint32_t Camera() {
    uint32_t cams = kernel::R32(kCameras);
    return cams ? kernel::R32(cams + 4) : 0;
}

// player 1 is on the gun, in play
bool OnGun() {
    uint32_t game = kernel::R32(kGame);
    return game && kernel::G<uint8_t>(game + kPovMode) && !kernel::R32(game + kPaused) && kernel::R32(kState) != 3 &&
           kernel::R32(kControlMap + 4) == 1 && !nf::MenuUp() && Camera();
}

// the camera the frame is drawn from (in the car the chase camera)
uint32_t ViewCam() {
    uint32_t cams = kernel::R32(kCameras), view = cams ? kernel::R32(cams) : 0;
    uint32_t cam = view && mem::IsCommitted(view) ? kernel::R32(view + 4) : 0;
    return cam && mem::IsCommitted(cam) ? cam : 0;
}

uint32_t Player() {
    uint32_t p = kernel::R32(0x234E40);
    return p ? kernel::R32(p) : 0;
}

// player 1 drives, in play
bool InCar() {
    uint32_t game = kernel::R32(kGame);
    return game && !kernel::G<uint8_t>(game + kPovMode) && !kernel::R32(game + kPaused) && kernel::R32(kState) != 3 &&
           kernel::R32(kControlMap + 4) == 0 && !nf::MenuUp() && ViewCam() && Player();
}

// the copied shot's gun still the one it came from (its class, player 1's flag)
bool GunKept() {
    uint32_t gun = g_task[5];
    return gun && mem::IsCommitted(gun) && kernel::R32(gun) == g_task_gun_class && (kernel::R32(gun + 0x138) & 2);
}

bool PlayerShot(uint32_t task) {
    uint32_t game = kernel::R32(kGame), gun = kernel::R32(task + 0x14);
    return game && kernel::G<uint8_t>(game + kPovMode) && kernel::R32(task) == kShotVtable && gun &&
           (kernel::R32(gun + 0x138) & 2);
}

// the weapon's shots' interval (w: player 1's current one)
int Interval(int weapon = -1) {
    uint32_t mgr = kernel::R32(kWeapons);
    if (!mgr || !kernel::R32(mgr + 0x10)) return 4;
    uint32_t w = weapon >= 0 ? uint32_t(weapon) : kernel::R32(mgr + 4);
    if (w >= 0x20) return 4;
    int ticks = int(kernel::R32(kernel::R32(mgr + 0x10) + w * 0x54 + 0x3C));
    return ticks > 0 && ticks < 600 ? ticks : 4;
}

// player 1's shots' spacing on the gun as it fires them (2026-11-14: the weapon table's interval was slower)
uint32_t GunGap() { return g_task_gap ? std::clamp(g_task_gap, 1u, 60u) : uint32_t(Interval()); }

void Basis(uint32_t cam, float f[3], float r[3], float u[3]) {
    for (int i = 0; i < 3; i++) f[i] = GetF(cam + kCamAim + 4 * i);
    float len = std::sqrt(f[0] * f[0] + f[2] * f[2]);
    if (len < 1e-4f) len = 1e-4f;
    r[0] = f[2] / len, r[1] = 0, r[2] = -f[0] / len;
    u[0] = f[1] * r[2] - f[2] * r[1], u[1] = f[2] * r[0] - f[0] * r[2], u[2] = f[0] * r[1] - f[1] * r[0];
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_0004AC40);

namespace {
// a copy of player 1's last shot, from a barrel in turn, along the camera's aim turned (x, y) tangents
void Shoot(X86Context& ctx, uint8_t* base, float x, float y) {
    uint32_t cam = Camera();
    float f[3], r[3], u[3], d[3];
    Basis(cam, f, r, u);
    float len = 0;
    for (int i = 0; i < 3; i++) d[i] = f[i] + x * r[i] + y * u[i], len += d[i] * d[i];
    len = std::sqrt(len);
    uint32_t was[4];
    for (int i = 0; i < 4; i++) was[i] = kernel::R32(cam + kCamAim + 4 * i);
    for (int i = 0; i < 3; i++) SetF(cam + kCamAim + 4 * i, d[i] / len);

    uint32_t saved_eax = ctx.eax, saved_ebx = ctx.ebx, saved_ecx = ctx.ecx, saved_edx = ctx.edx;
    uint32_t saved_esi = ctx.esi, saved_edi = ctx.edi, saved_ebp = ctx.ebp, saved_esp = ctx.esp;
    ctx.esp = (ctx.esp - 0x40) & ~0xFu;
    uint32_t task = ctx.esp;
    for (int i = 0; i < 6; i++) kernel::W32(task + 4 * i, g_task[i]);
    if (g_barrels) kernel::W32(task + 4, g_barrel[g_turn++ % g_barrels]);
    ctx.esp -= 4;
    kernel::W32(ctx.esp, 0);
    ctx.ecx = task;
    g_in_shot = true;
    __imp__sub_0004AC40(ctx, base);
    g_in_shot = false;
    ctx.eax = saved_eax, ctx.ebx = saved_ebx, ctx.ecx = saved_ecx, ctx.edx = saved_edx;
    ctx.esi = saved_esi, ctx.edi = saved_edi, ctx.ebp = saved_ebp, ctx.esp = saved_esp;

    for (int i = 0; i < 4; i++) kernel::W32(cam + kCamAim + 4 * i, was[i]);
}

float Stick(int16_t v) {
    float s = v / 32767.0f;
    constexpr float kDead = 0.24f;
    if (std::fabs(s) < kDead) return 0;
    s = (std::fabs(s) - kDead) / (1 - kDead) * (s < 0 ? -1.0f : 1.0f);
    return s * std::fabs(s);
}

// On the gun an aim keeps its direction in the world as player 1 turns the gun (2026-11-27, John: everyone's own view
// was dragged along with player 1's): its tangents across the gun's view are worked out again from it each tick,
// then held to the aim's reach, which alone moves it when the gun turns away far enough
float Dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
void UnitBasis(uint32_t cam, float f[3], float r[3], float u[3]) {  // a unit forward, so the round trip is exact
    Basis(cam, f, r, u);
    float len = std::max(1e-4f, std::sqrt(Dot(f, f)));
    for (int i = 0; i < 3; i++) f[i] /= len;
    u[0] = f[1] * r[2] - f[2] * r[1], u[1] = f[2] * r[0] - f[0] * r[2], u[2] = f[0] * r[1] - f[1] * r[0];
}
void FromWorld(Aim& a, uint32_t cam) {
    if (!Dot(a.dir, a.dir)) return;
    float f[3], r[3], u[3];
    UnitBasis(cam, f, r, u);
    float ahead = Dot(a.dir, f);
    if (ahead < 0.2f) {  // far behind the gun's view (a cutscene's camera, a new gun): its centre
        a.x = a.y = 0;
        return;
    }
    a.x = std::clamp(Dot(a.dir, r) / ahead, -kMaxX, kMaxX);
    a.y = std::clamp(Dot(a.dir, u) / ahead, -kMaxY, kMaxY);
}
void ToWorld(Aim& a, uint32_t cam) {
    float f[3], r[3], u[3], len = 0;
    UnitBasis(cam, f, r, u);
    for (int i = 0; i < 3; i++) a.dir[i] = f[i] + a.x * r[i] + a.y * u[i], len += a.dir[i] * a.dir[i];
    len = std::sqrt(len);
    for (float& v : a.dir) v /= len;
}

// their aim moved by this tick's input, and whether they hold the trigger
bool Steer(int player, Aim& a) {
    if (g_test && player == 1) {
        a.x = g_test_x, a.y = g_test_y;
        return true;
    }
    input::Pad p;
    if (!net::PadFor(player, p) || net::PlayerLeft(player)) return false;
    uint32_t cam = OnGun() ? Camera() : 0;
    if (cam) FromWorld(a, cam);
    float mx, my;
    if (net::KeyboardFor(player) && net::MouseFor(player, mx, my)) {
        a.x += mx * kMouseRate;
        a.y += my * kMouseRate;
    }
    float sx = Stick(p.lx) + Stick(p.rx), sy = Stick(p.ly) + Stick(p.ry);
    a.x = std::clamp(a.x + std::clamp(sx, -1.0f, 1.0f) * kStickRate, -kMaxX, kMaxX);
    a.y = std::clamp(a.y + std::clamp(sy, -1.0f, 1.0f) * kStickRate, -kMaxY, kMaxY);
    if (cam) ToWorld(a, cam);
    return p.analog[7] > 64 || (InCar() && p.analog[0] > 64);  // the car's fire is A
}

// The car fires as player 1's held trigger fires it (0xBC461, the weapons' update): its fire request for the current
// weapon (0xBB2B0, thiscall on the weapons' manager: the rounds, the weapon's interval, the firing flag 4 in its table
// entry's +0x14), the machine gun's after a flag of [0x239220]'s (0xB72F0, its +0x434 + 0x16). The next update fires
// it, the shot, its tracer and its sound the game's own, its rounds the pool's (2026-11-18, John: copies of player 1's
// shot tasks made a hit and a sound, a tracer at first, no rounds drawn). Player 1's trigger held (the manager's +0x24,
// +0x25) fires it already
constexpr uint32_t kFireRequest = 0xBB2B0, kWeaponUsed = 0xB72F0, kMachineGunFlag = 0x16, kNoName = 0x189FB1;
void FireCar(X86Context& ctx) {
    uint32_t mgr = kernel::R32(kWeapons);
    if (!mgr || kernel::G<uint8_t>(mgr + 0x24) || kernel::G<uint8_t>(mgr + 0x25)) return;
    uint32_t weapon = kernel::R32(mgr + 4), ecx = ctx.ecx;
    if (weapon == 0) {
        ctx.ecx = kernel::R32(0x239220);
        kernel::CallGuest(ctx, kWeaponUsed, {kMachineGunFlag, kNoName});
    }
    ctx.ecx = mgr;
    kernel::CallGuest(ctx, kFireRequest, {weapon});
    ctx.ecx = ecx;
}

// The car's gun sound: the player's car hears the fire event too (0x66640, the car's input: 0x1F sets its +0x25B,
// 0x20 clears it, while [[0x23F44C]+0x2B0] is 0) and its sound plays the machine gun while that is set; the weapons'
// shot (0x4AC40) plays none for the player's own car. Another player's trigger holds it as player 1's does, and on
// release it is player 1's again (the manager's +0x25, the same event's; 2026-11-19, John: player 2's car machine gun
// silent on both PCs, measured headless by the WAV's loudness)
constexpr uint32_t kCarTrigger = 0x25B, kHud = 0x23F44C, kHudBusy = 0x2B0;
bool g_car_held = false;  // another player's trigger holds the car's
uint32_t g_fired_by = 0;  // the other players whose trigger fired the car's weapon this tick, a bit each

// A guided torpedo (Deep Descent) is steered from [0x239220]+0x47C, the missile 0xB80B0 puts there (0 again when it
// is gone): the game's steering events (0x2C..0x31, 0xB7B09) steer it, and fire (0x1F) sets it off. Fired by
// another player's trigger (the shot follows their request by a tick, the manager's held flags player 1's own), it
// takes that player's controller as seat 1's while it flies (DrivingSeat, src/common/game_info.cpp; 2026-11-19, John:
// player 1 steered player 2's torpedo)
std::atomic<int> g_guided_by{0};

// this machine's player's own view on the mounted gun (own_view): their aim's tangents, false for player 1's view
bool OwnView(float& x, float& y) {
    int me = net::LocalPlayer();
    if (!nf::own_view || !net::Active() || me <= 0 || me >= kMaxExtra || me == net::Driver() || !OnGun()) return false;
    x = g_aim[me].x, y = g_aim[me].y;
    return x != 0 || y != 0;
}

// the camera's basis turned by the aim (x, y): forward, right and up
void Turned(uint32_t cam, float x, float y, float f[3], float r[3], float u[3]) {
    float f0[3], r0[3], u0[3];
    Basis(cam, f0, r0, u0);
    float len = 0;
    for (int i = 0; i < 3; i++) f[i] = f0[i] + x * r0[i] + y * u0[i], len += f[i] * f[i];
    len = std::sqrt(len);
    for (int i = 0; i < 3; i++) f[i] /= len;
    float h = std::max(1e-4f, std::sqrt(f[0] * f[0] + f[2] * f[2]));
    r[0] = f[2] / h, r[1] = 0, r[2] = -f[0] / h;
    u[0] = f[1] * r[2] - f[2] * r[1], u[1] = f[2] * r[0] - f[0] * r[2], u[2] = f[0] * r[1] - f[1] * r[0];
}

// the aim (x, y) as tangents across the view turned by (mx, my); false behind it
bool Across(float mx, float my, float& x, float& y) {
    uint32_t cam = Camera();
    float f0[3], r0[3], u0[3], f[3], r[3], u[3], d[3];
    Basis(cam, f0, r0, u0);
    Turned(cam, mx, my, f, r, u);
    for (int i = 0; i < 3; i++) d[i] = f0[i] + x * r0[i] + y * u0[i];
    float df = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
    if (df < 0.05f) return false;
    x = (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / df;
    y = (d[0] * u[0] + d[1] * u[1] + d[2] * u[2]) / df;
    return true;
}

void HoldCarTrigger(bool others) {
    uint32_t car = Player(), mgr = kernel::R32(kWeapons), hud = kernel::R32(kHud);
    if (!car || !mgr) return;
    if (others && !(hud && kernel::G<uint8_t>(hud + kHudBusy))) {
        kernel::G<uint8_t>(car + kCarTrigger) = 1;
        g_car_held = true;
    } else if (g_car_held) {
        kernel::G<uint8_t>(car + kCarTrigger) = kernel::G<uint8_t>(mgr + 0x25);
        g_car_held = false;
    }
}

// A mounted gun's sound is the vehicle's too, from the weapons manager's +0x27 instead (0x6A840 on a gun of type
// 0x18, 0x1B or 0x1D: Alpine Escape's turret): the fire request 0xBB2B0 sets it as the gun's animation starts and the
// release event 0x20 clears it. Held while another player's trigger fires the gun, player 1's own (+0x25) on release
// (2026-11-19, John: Alpine Escape's turret silent for player 2 once its shutter is up)
constexpr uint32_t kGunFiring = 0x27;
bool g_gun_held = false;

void HoldGunTrigger(bool others) {
    uint32_t mgr = kernel::R32(kWeapons);
    if (!mgr) return;
    if (others) {
        kernel::G<uint8_t>(mgr + kGunFiring) = 1;
        g_gun_held = true;
    } else if (g_gun_held) {
        kernel::G<uint8_t>(mgr + kGunFiring) = kernel::G<uint8_t>(mgr + 0x25);
        g_gun_held = false;
    }
}

// A session's ammo is one pool (2026-11-14, John): each weapon's rounds, the weapon table's +0x18 (the game fires while
// +0x18 - +0x1C is above 0, 0xBBDC0; Deep Descent's torpedoes entry 21, 30; Enemies Vanquished's missiles entry 2, 30;
// Alpine Escape's turret entry 24, about 3000). What the game gives (the mission's start, a pickup) comes once more
// for each other player, a count of 1000 or more (a turret's) as it is; the others' copied shots take player 1's
// current weapon's rounds and stop at none
constexpr int kWeaponSlots = 0x20;
constexpr uint32_t kSlotSize = 0x54, kRounds = 0x18;
constexpr int32_t kPlenty = 1000;
int32_t g_rounds[kWeaponSlots];  // each weapon's rounds as last seen, after the others' share
uint32_t g_rounds_time = 0;

uint32_t RoundsAt(int w) {
    uint32_t mgr = kernel::R32(kWeapons), table = mgr ? kernel::R32(mgr + 0x10) : 0;
    if (w < 0 || w >= kWeaponSlots || !table || !mem::IsCommitted(table + uint32_t(w) * kSlotSize + kRounds)) return 0;
    return table + uint32_t(w) * kSlotSize + kRounds;
}

void ShareRounds(int players) {
    uint32_t now = kernel::R32(kTime);
    if (now < g_rounds_time) std::fill(std::begin(g_rounds), std::end(g_rounds), 0);  // the mission again: given anew
    g_rounds_time = now;
    if (!RoundsAt(kWeaponSlots - 1)) return;
    static int said = 0;
    for (int w = 0; w < kWeaponSlots; w++) {
        int32_t& r = kernel::G<int32_t>(RoundsAt(w));
        if (players > 1 && r > g_rounds[w] && r < kPlenty) {
            int32_t more = (r - std::max(g_rounds[w], 0)) * (players - 1);
            if (said < 16) {
                said++;
                LOGR("driving co-op: weapon %d given %d rounds: %d for %d players", w, r - std::max(g_rounds[w], 0),
                     r - std::max(g_rounds[w], 0) + more, players);
            }
            r += more;
        }
        g_rounds[w] = r;
    }
}

// a round of the weapon (w: player 1's current one) for another player's shot: false when it has none
bool TakeRound(int weapon = -1) {
    uint32_t mgr = kernel::R32(kWeapons), at = mgr ? RoundsAt(weapon >= 0 ? weapon : int(kernel::R32(mgr + 4))) : 0;
    if (!at) return true;
    int32_t& r = kernel::G<int32_t>(at);
    if (r <= 0) return false;
    r--;
    g_rounds[(at - kernel::R32(mgr + 0x10) - kRounds) / kSlotSize] = r;
    return true;
}

// player 1's car weapon as it changes, and another player's trigger on one with no rounds, for the log (2026-11-18,
// John: Enemies Vanquished's car gun fired a few rounds for player 2, then none)
void SayCar(int p, bool fire) {
    static int said = 0, last_weapon = -2;
    static bool last_empty[kMaxExtra];
    if (said >= 40) return;
    uint32_t mgr = kernel::R32(kWeapons);
    int w = mgr ? int(kernel::R32(mgr + 4)) : -1;
    int32_t rounds = RoundsAt(w) ? kernel::G<int32_t>(RoundsAt(w)) : -1;
    if (w != last_weapon) {
        last_weapon = w, said++;
        LOGR("driving co-op: player 1's car weapon %d (%d rounds) at session tick %u", w, rounds, net::Tick());
    }
    bool empty = fire && rounds == 0;
    if (empty == last_empty[p]) return;
    last_empty[p] = empty;
    if (empty) {
        said++;
        LOGR("driving co-op: player %d's car trigger fires nothing at session tick %u: weapon %d has no rounds", p + 1,
             net::Tick(), w);
    }
}

void ExtraPlayers(X86Context& ctx, uint8_t* base) {
    int players = g_test ? 2 : (net::Active() ? std::min(net::Players(), kMaxExtra) : 1);
    uint32_t now = kernel::R32(kTime);
    if (net::Active()) ShareRounds(players);
    // a mission started again, or the gun gone: the copied shot's objects may be too (a cutscene's camera between
    // two of its shots keeps them: the shot copied before one served after it)
    if (g_have_task && (now < g_task_time || !GunKept())) g_have_task = false, g_barrels = 0;
    bool car = players > 1 && !OnGun() && InCar();
    bool on = players > 1 && (OnGun() || car);
    int me = net::LocalPlayer();
    if (!on) {
        if (g_car_held) HoldCarTrigger(false);
        if (g_gun_held) HoldGunTrigger(false);
        g_shown = 0;
        for (Aim& a : g_aim) a.next = 0, a.dir[0] = a.dir[1] = a.dir[2] = 0;
        return;
    }
    static bool said = false;
    int driver = net::Driver();  // player 1's seat: their trigger is the gun's own
    bool others_fire = false, gun_fire = false;
    g_fired_by = 0;
    for (int p = 1; p < players; p++) {
        if (p == driver) continue;
        Aim& a = g_aim[p];
        bool fire = Steer(p, a);
        if (car) {
            SayCar(p, fire);
            if (!fire) continue;
            others_fire = true;
            g_fired_by |= 1u << p;
            static bool said_car = false;
            if (!said_car) {
                said_car = true;
                LOGR("driving co-op: player %d fires the car's guns at session tick %u", p + 1, net::Tick());
            }
            FireCar(ctx);
            continue;
        }
        if (!fire || !g_have_task) continue;
        uint32_t mgr = kernel::R32(kWeapons), at = mgr ? RoundsAt(int(kernel::R32(mgr + 4))) : 0;
        if (!at || kernel::G<int32_t>(at) > 0) gun_fire = true;
        if (now < a.next) continue;
        if (!said) {
            said = true;
            LOGR("driving co-op: player %d fires the mounted gun (%u apart, the weapon table's %d) at session tick %u",
                 p + 1, GunGap(), Interval(), net::Tick());
        }
        a.next = now + GunGap();
        if (TakeRound()) Shoot(ctx, base, a.x, a.y);
    }
    if (car || g_car_held) HoldCarTrigger(car && others_fire);
    HoldGunTrigger(!car && gun_fire);
    // the car's missiles find their targets: a reticle on a mounted gun alone (2026-11-14, John). Every machine has
    // every player's aim; the others' are drawn with others_reticles (2026-11-26, John)
    // with this machine's own view (OwnView) its player's reticle is the view's centre, and the others' (player 1's
    // too, whose crosshair the game draws at the centre of player 1's view) where they cross this one
    float mx, my;
    bool own = OwnView(mx, my);
    uint32_t shown = 0;
    for (int p = 0; !car && p < players; p++) {
        bool p1 = p == driver;
        if ((p1 && !own) || (p != me && !nf::others_reticles) || net::PlayerLeft(p)) continue;
        float x = p1 ? 0.0f : g_aim[p].x, y = p1 ? 0.0f : g_aim[p].y;
        if (own && !Across(mx, my, x, y)) continue;
        g_show_x[p] = x / std::max(0.05f, g_tan_x.load());
        g_show_y[p] = y / std::max(0.05f, g_tan_y.load());
        shown |= 1u << p;
    }
    static bool said_mine = false, said_others = false;
    if (!said_mine && (shown & (1u << me))) {
        said_mine = true;
        LOGR("driving co-op: this machine's player %d has a reticle on the mounted gun", me + 1);
    }
    if (!said_others && (shown & ~(1u << me))) {
        said_others = true;
        LOGR("driving co-op: the other players' reticles are shown on the mounted gun");
    }
    g_shown = shown;
}
}  // namespace

namespace nf {
int Reticles(Mark* out, int max) {
    static const uint32_t kColours[] = {0xFFFFFFFFu, 0xFFFFB040u, 0xFF60E060u, 0xFF40E0FFu, 0xFFFF60FFu,
                                        0xFF4090FFu, 0xFFFFFF60u, 0xFFA0A0FFu, 0xFF60FFC0u, 0xFFC0C0C0u};  // ImGui's ABGR
    uint32_t shown = g_shown;
    int me = net::LocalPlayer(), n = 0;
    for (int p = 0; p < kMaxExtra && n < max; p++)
        if (shown & (1u << p))
            out[n++] = {std::clamp(g_show_x[p].load(), -1.0f, 1.0f), std::clamp(g_show_y[p].load(), -1.0f, 1.0f),
                        kColours[std::min(p, 9)], p == me};
    return n;
}

// the gun's camera turned to this machine's player's aim while a view or the HUD's markers copy it (0x96980 takes its
// rows +0x10 right, +0x20 up, +0x30 forward): their picture alone; what the game decides stays player 1's camera's
bool TurnOwnView(uint32_t cam, uint32_t saved[12]) {
    float mx, my;
    if (!cam || cam != Camera() || !OwnView(mx, my)) return false;
    float f0[3], r0[3], u0[3], f[3], r[3], u[3];
    Basis(cam, f0, r0, u0);
    Turned(cam, mx, my, f, r, u);
    for (int row = 0; row < 3; row++) {
        uint32_t at = cam + 0x10 + 0x10 * uint32_t(row);
        float v[3], a = 0, b = 0, c = 0;
        for (int i = 0; i < 3; i++) saved[row * 4 + i] = kernel::R32(at + 4 * i), v[i] = GetF(at + 4 * i);
        for (int i = 0; i < 3; i++) a += r0[i] * v[i], b += u0[i] * v[i], c += f0[i] * v[i];
        for (int i = 0; i < 3; i++) SetF(at + 4 * i, r[i] * a + u[i] * b + f[i] * c);
    }
    return true;
}
void RestoreView(uint32_t cam, const uint32_t saved[12]) {
    for (int row = 0; row < 3; row++)
        for (int i = 0; i < 3; i++) kernel::W32(cam + 0x10 + 0x10 * uint32_t(row) + 4 * uint32_t(i), saved[row * 4 + i]);
}

void ViewAngles(float tan_x, float tan_y) {
    if (tan_x > 0.01f && tan_y > 0.01f) g_tan_x = tan_x, g_tan_y = tan_y;
}

// what steers the others' shots, for a rejoining machine
void SaveCoopState(snapshot::Writer& w) {
    w.Pod(g_aim), w.Pod(g_task), w.Pod(g_barrel), w.Pod(g_barrels), w.Pod(g_turn), w.Pod(g_have_task);
    w.Pod(g_task_time), w.Pod(g_task_gun_class), w.Pod(g_task_gap), w.Pod(g_rounds), w.Pod(g_rounds_time);
    w.Pod(g_car_held), w.Pod(g_fired_by), w.Pod(g_gun_held);
    int guided = g_guided_by;
    w.Pod(guided);
}
void LoadCoopState(snapshot::Reader& r) {
    r.Pod(g_aim), r.Pod(g_task), r.Pod(g_barrel), r.Pod(g_barrels), r.Pod(g_turn), r.Pod(g_have_task);
    r.Pod(g_task_time), r.Pod(g_task_gun_class), r.Pod(g_task_gap), r.Pod(g_rounds), r.Pod(g_rounds_time);
    r.Pod(g_car_held), r.Pod(g_fired_by), r.Pod(g_gun_held);
    int guided = 0;
    r.Pod(guided);
    g_guided_by = guided;
}

int GuidedBy() {
    if (!net::Active()) return 0;
    uint32_t game = kernel::R32(kGame);
    return game && kernel::R32(game + kPaused) ? g_guided_by.load() : 0;
}

bool GunWaitsForShot() { return net::Active() && OnGun() && !g_have_task; }
}  // namespace nf

// the players holding a trigger this tick, for the log ("1 2")
static std::string Triggers() {
    std::string s;
    for (int p = 0; p < net::Players(); p++) {
        input::Pad pad;
        if (net::PadFor(p, pad) && pad.analog[7] > 64) s += (s.empty() ? "" : " ") + std::to_string(p + 1);
    }
    return s.empty() ? "none" : s;
}

X86_FUNC(sub_0004AC40) {  // a shot's task resolved: player 1's kept for the others
    uint32_t task = ctx.ecx;
    if (task && !g_in_shot && PlayerShot(task)) {
        // the gun's own shots after a pause in them: whose trigger they come from (a player who cannot fire shows)
        static int said = 0;
        if (net::Active() && said < 12 && (!g_have_task || kernel::R32(kTime) - g_task_time > 120)) {
            said++;
            LOGR("driving co-op: the gun fires at tick %u, the triggers held: %s", net::Tick(), Triggers().c_str());
        }
        for (int i = 0; i < 6; i++) g_task[i] = kernel::R32(task + 4 * i);
        uint32_t barrel = g_task[1];
        if (!(g_barrels > 0 && g_barrel[0] == barrel) && !(g_barrels > 1 && g_barrel[1] == barrel)) {
            if (g_barrels < 2) g_barrel[g_barrels++] = barrel;
            else g_barrel[0] = g_barrel[1], g_barrel[1] = barrel;
        }
        uint32_t now = kernel::R32(kTime);
        if (g_have_task && now > g_task_time && now - g_task_time < 60) g_task_gap = now - g_task_time;
        g_have_task = true;
        g_task_time = now;
        g_task_gun_class = kernel::R32(g_task[5]);
    }
    __imp__sub_0004AC40(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_000B80B0);
X86_FUNC(sub_000B80B0) {  // thiscall (missile): the guided missile, 0 none
    uint32_t missile = kernel::R32(ctx.esp + 4), mgr = kernel::R32(kWeapons);
    int by = 0;
    bool own = mgr && (kernel::G<uint8_t>(mgr + 0x24) || kernel::G<uint8_t>(mgr + 0x25) || kernel::G<uint8_t>(mgr + 0x26));
    if (missile && net::Active() && !own && g_fired_by)
        for (int p = 1; p < kMaxExtra; p++)
            if (g_fired_by & (1u << p)) {
                by = p;
                break;
            }
    if (missile && net::Active()) {
        static int said = 0;
        if (said < 8) {
            said++;
            LOGR("driving co-op: a guided torpedo at session tick %u, steered by player %d", net::Tick(), by + 1);
        }
    }
    g_guided_by = missile ? by : 0;
    __imp__sub_000B80B0(ctx, base);
}

// headless tests: a file dumptick in the run folder holding a session tick writes physical memory to
// mem_tick_<tick>.bin there (and the low 128 MB of virtual memory to mem_tick_<tick>_virtual.bin) when the mission's frame reaches it, as default.xbe's snapshot point does (game/hooks.cpp)
static void DumpAtTick() {
    static uint32_t frames = 0;
    static std::vector<uint32_t> dues;  // the file's ticks, commas between them
    if (++frames % 30 == 0)
        if (FILE* f = std::fopen("dumptick", "rb")) {
            char t[256] = {};
            std::fread(t, 1, sizeof t - 1, f);
            std::fclose(f);
            std::remove("dumptick");
            dues.clear();
            for (char* p = t; *p;) {
                char* end;
                uint32_t v = uint32_t(std::strtoul(p, &end, 0));
                if (end == p) break;
                dues.push_back(v);
                p = *end == ',' ? end + 1 : end;
            }
        }
    if (dues.empty() || !net::Active() || net::Tick() < dues.front()) return;
    uint32_t due = dues.front();
    dues.erase(dues.begin());
    if (net::Tick() > due) {
        LOGE("driving co-op: tick %u passed before its memory dump (now %u)", due, net::Tick());
        return;
    }
    std::string name = "mem_tick_" + std::to_string(net::Tick()) + ".bin";
    static const std::vector<uint8_t> zeros(0x1000);
    // NF_DUMP_RAM=1: physical memory copied at each tick and written after the last (a file's write would pause
    // this machine long enough to change what a timing-sensitive drift does)
    static const bool ram = std::getenv("NF_DUMP_RAM") != nullptr;
    static std::vector<std::pair<std::string, std::vector<uint8_t>>> kept;
    if (ram) {
        std::vector<uint8_t> copy(64u << 20);
        for (uint32_t va = 0x80000000u; va < 0x84000000u; va += 0x1000)
            std::memcpy(copy.data() + (va - 0x80000000u), mem::IsCommitted(va) ? kernel::HostPtr(va) : zeros.data(),
                        0x1000);
        kept.emplace_back(name, std::move(copy));
        std::vector<uint8_t> image(0x240000);  // the xbe's image, its globals (0x10000-0x250000)
        for (uint32_t va = 0x10000; va < 0x250000; va += 0x1000)
            std::memcpy(image.data() + (va - 0x10000), mem::IsCommitted(va) ? kernel::HostPtr(va) : zeros.data(), 0x1000);
        kept.emplace_back("mem_tick_" + std::to_string(net::Tick()) + "_image.bin", std::move(image));
        if (!dues.empty()) return;
        for (auto& [file, bytes] : kept)
            if (FILE* f = std::fopen(file.c_str(), "wb")) {
                std::fwrite(bytes.data(), 1, bytes.size(), f);
                std::fclose(f);
            }
        LOGR("driving co-op: memory at %zu ticks written", kept.size());
        kept.clear();
        return;
    }
    auto write = [&](const std::string& file, uint32_t lo, uint32_t hi) {
        if (FILE* f = std::fopen(file.c_str(), "wb")) {
            for (uint32_t va = lo; va < hi; va += 0x1000)
                std::fwrite(mem::IsCommitted(va) ? kernel::HostPtr(va) : zeros.data(), 1, 0x1000, f);
            std::fclose(f);
        }
    };
    write(name, 0x80000000u, 0x84000000u);
    write("mem_tick_" + std::to_string(net::Tick()) + "_virtual.bin", 0, 128u << 20);  // the image and its heaps
    LOGR("driving co-op: memory at tick %u to %s", net::Tick(), name.c_str());
}

X86_EXTERN_FUNC(__imp__sub_000BBDC0);
X86_FUNC(sub_000BBDC0) {  // the weapons' update, each frame of play
    static const bool test = [] {
        const char* e = std::getenv("NF_COOP_TEST");
        if (!e || !*e) return false;
        g_test_x = float(std::atof(e));
        const char* comma = std::strchr(e, ',');
        g_test_y = comma ? float(std::atof(comma + 1)) : 0.0f;
        return true;
    }();
    g_test = test;
    __imp__sub_000BBDC0(ctx, base);
    ExtraPlayers(ctx, base);
    DumpAtTick();
    // the game's time, the gun's camera (its place and aim) and the one drawn (its place): machines apart show here
    if (net::Active()) {
        uint32_t cam = Camera(), shown = ViewCam(), view[10] = {kernel::R32(kTime)};
        for (int i = 0; cam && i < 3; i++)
            view[1 + i] = kernel::R32(cam + kCamPlace + 4 * i), view[4 + i] = kernel::R32(cam + kCamAim + 4 * i);
        for (int i = 0; shown && i < 3; i++) view[7 + i] = kernel::R32(shown + kCamPlace + 4 * i);
        net::CheckState(view, 10);
    }
}
