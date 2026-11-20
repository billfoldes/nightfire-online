// Ultrawide, field of view, full-screen per-player online view, and 10-player slot expansion on foot.
//
// A camera's projection is set by 0x25190 (cdecl: camera index into 0x1F661C, vertical field of view in radians):
// the angle at +0x104, the aspect at +0x100 (16:9 with the widescreen flag 0x1F6610, else 4:3; halved or doubled for
// a split screen's viewports) and pixel scales at +0xEC (x) and +0xF0 (y) from the angle. The D3D projection (0xDD950)
// and the culling planes (0xDB5C0, the globals 0x29EA60.. and the camera's +0x6C) are both built from +0x100 and
// +0x104, so both get the widened aspect and the scaled angle for the call alone: what is drawn and what is culled
// agree, and the fields stay the game's (the scope's zoom adds to +0x104 and sets it again, a cutscene camera reads
// +0x100). Ultrawide leaves the pixel scales alone: the HUD markers (0xDB230) and the aim ray (0xB85A0) are laid out
// in the game's 16:9 space, which the runtime squeezes into the middle of the frame as the 3D widens around it. The
// field of view changes the angle itself, so they follow it (r = tan(fov/2) over tan(scaled/2)).
//   In an online session with full-screen view on (net::FullScreenView), every machine gives all player cameras
// the full 640x480 screen rectangle (0x25B80, 0x25190) and 1-player HUD layout (0xB1AB0) so guest memory stays
// identical across the session, while 0xDD950 suppresses host GPU draws for remote players' camera passes (held until
// the next pass or the end of 0xDAC20, never by writing guest memory) and 0xE7130 suppresses the split-screen divider
// bars.
//   For 5 to 10 online players, slots 4..9 (the 6 AI bot slots in the 10-slot combatant tables at 0x25FE38 and
// 0x262738) are spawned as full first-person human players (0xACAB0 instead of 0x1B170) and bank-switched through
// hardware slot 3 (0x1F661C[3], 0x1F6654[3], 0x2774B8[3], 0x1FE6D0[3]) during their per-frame update, camera, HUD,
// respawn, damage, and render passes.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "core/snapshot.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace d3d {
bool SuppressDraws(bool on);  // sdk/runtime/xdk/d3d8.cpp
}

X86_EXTERN_FUNC(__imp__sub_00011440);
X86_FUNC(sub_00011440);
X86_EXTERN_FUNC(__imp__sub_000118F0);
X86_EXTERN_FUNC(__imp__sub_00017A60);
X86_EXTERN_FUNC(__imp__sub_000DB3D0);
X86_EXTERN_FUNC(__imp__sub_0002C5B0);
X86_EXTERN_FUNC(__imp__sub_000DABB0);
X86_EXTERN_FUNC(__imp__sub_0006DCB0);
X86_EXTERN_FUNC(__imp__sub_000A3EB0);
X86_EXTERN_FUNC(__imp__sub_000DB320);
X86_EXTERN_FUNC(__imp__sub_0001A1E0);
X86_EXTERN_FUNC(__imp__sub_00037C10);
X86_EXTERN_FUNC(__imp__sub_00043070);
X86_EXTERN_FUNC(__imp__sub_0006ABC0);
X86_EXTERN_FUNC(__imp__sub_0006B500);
X86_EXTERN_FUNC(__imp__sub_00025BF0);
X86_EXTERN_FUNC(__imp__sub_00025B80);
X86_EXTERN_FUNC(__imp__sub_00025190);
X86_EXTERN_FUNC(__imp__sub_00025D10);
X86_EXTERN_FUNC(__imp__sub_0006CF50);
X86_EXTERN_FUNC(__imp__sub_0006EAD0);
X86_EXTERN_FUNC(__imp__sub_0007EB90);
X86_EXTERN_FUNC(__imp__sub_0009CC50);
X86_EXTERN_FUNC(__imp__sub_0009F0B0);
X86_EXTERN_FUNC(__imp__sub_000AC390);
X86_EXTERN_FUNC(__imp__sub_0001B170);
X86_EXTERN_FUNC(__imp__sub_0001B4F0);
X86_EXTERN_FUNC(__imp__sub_00024150);
X86_EXTERN_FUNC(__imp__sub_00024FD0);
X86_EXTERN_FUNC(__imp__sub_000A25F0);
X86_EXTERN_FUNC(__imp__sub_000CB580);
X86_EXTERN_FUNC(__imp__sub_000CBDF0);
X86_EXTERN_FUNC(__imp__sub_000CC1C0);
X86_EXTERN_FUNC(__imp__sub_000A84D0);
X86_EXTERN_FUNC(__imp__sub_000A2990);
X86_EXTERN_FUNC(__imp__sub_0009D3D0);
X86_FUNC(sub_0001B170);
X86_FUNC(sub_0001B4F0);
X86_EXTERN_FUNC(__imp__sub_000A8A60);
X86_EXTERN_FUNC(__imp__sub_000ACAB0);
X86_EXTERN_FUNC(__imp__sub_000AC7E0);
X86_EXTERN_FUNC(__imp__sub_000A9720);
X86_EXTERN_FUNC(__imp__sub_000AD520);
X86_EXTERN_FUNC(__imp__sub_000ADD00);
X86_EXTERN_FUNC(__imp__sub_000AE1C0);
X86_EXTERN_FUNC(__imp__sub_000AE870);
X86_EXTERN_FUNC(__imp__sub_000B1AB0);
X86_EXTERN_FUNC(__imp__sub_000B2750);
X86_EXTERN_FUNC(__imp__sub_000B2870);
X86_EXTERN_FUNC(__imp__sub_000B2B50);
X86_EXTERN_FUNC(__imp__sub_000B54D0);
X86_EXTERN_FUNC(__imp__sub_000B5C30);
X86_EXTERN_FUNC(__imp__sub_000B5E80);
X86_EXTERN_FUNC(__imp__sub_000B5F40);
X86_EXTERN_FUNC(__imp__sub_000B6190);
X86_EXTERN_FUNC(__imp__sub_000B66C0);
X86_EXTERN_FUNC(__imp__sub_000B69E0);
X86_EXTERN_FUNC(__imp__sub_000BE0B0);
X86_EXTERN_FUNC(__imp__sub_000BE910);
X86_EXTERN_FUNC(__imp__sub_000CEB30);
X86_EXTERN_FUNC(__imp__sub_000DAC20);
X86_EXTERN_FUNC(__imp__sub_000DB5C0);
X86_EXTERN_FUNC(__imp__sub_000DD950);
X86_EXTERN_FUNC(__imp__sub_000E0EC0);
X86_EXTERN_FUNC(__imp__sub_000E7130);

namespace net = nf::net;

namespace {
using kernel::R32;
using kernel::W32;

constexpr uint32_t kCameras = 0x1F661C;       // the cameras by index (0-3 the players, 4-5 the 2D HUD/effects)
constexpr uint32_t kOuterActors = 0x1F6654;   // 4 dwords: outer player actor pointers (0..3)
constexpr uint32_t kInnerStates = 0x2774B8;   // 4 dwords: inner player state pointers [outer+0xBC] (0..3)
constexpr uint32_t kControls = 0x1FE6D0;      // 4 * 0x158 bytes: decoded per-player controller state
constexpr uint32_t kControlSize = 0x158;
constexpr uint32_t kCombatantCfg = 0x25FE38;    // 10 * 0x30 bytes: name (+0), team (+0x20), model (+0x24), handicap (+0x2C)
constexpr uint32_t kCombatantState = 0x262738;  // 10 * 0x30 bytes: kills (+4), deaths (+8), score (+0x18), actor (+0x1C), attacker (+0x20)
constexpr uint32_t kCombatantSize = 0x30;
constexpr uint32_t kPlaytimeStats = 0x278E98;   // 10 * 0x38 bytes: per-combatant playtime stats (0xB0590 / 0xB05C0)
constexpr uint32_t kPlaytimeSize = 0x38;
constexpr uint32_t kWidescreen = 0x1F6610;
constexpr uint32_t kIsMultiplayer = 0x260018;
constexpr uint32_t kMissionFailed = 0x1DF19A;  // a byte: the mission failed screen is asked for (0x9C2BC opens it)
constexpr uint32_t kMusicCues = 0x29A180;      // 0x40 dwords the music player (0xCA620) reads, set by 0xCBCE0
constexpr uint32_t kIsTeamPlay = 0x260024;
constexpr uint32_t kPlayerCount = 0x260044;
constexpr uint32_t kBotCount = 0x260048;
constexpr uint32_t kBotsEnabled = 0x245280;
constexpr uint32_t kMenuBotCount = 0x245281;
constexpr uint32_t kMenuBots = 0x245282;  // the bot menu's 6 entries, 0x12 bytes: team (+0x0F), character (+0x10)
constexpr uint32_t kBotSlots = 6;         // slots 4..9
constexpr uint32_t kBotState = 0x768;     // a bot's state: 0x1D98E0 + (slot - 4) * 0x768 for those six (0x1B170)
constexpr uint32_t kCombatants = 0x26002C;  // the match's players and bots (0x9CC50's last-one-standing count)
constexpr uint32_t kWorldRoot = 0x1F6674;
constexpr uint32_t kLeft = 0xDC, kRight = 0xE0, kTop = 0xE4, kBottom = 0xE8;
constexpr uint32_t kScaleX = 0xEC, kScaleY = 0xF0, kWidth = 0xF8, kHeight = 0xFC, kAspect = 0x100, kFov = 0x104;

float GetF(uint32_t va) {
    float f;
    std::memcpy(&f, &kernel::G<uint32_t>(va), 4);
    return f;
}
void SetF(uint32_t va, float f) { std::memcpy(&kernel::G<uint32_t>(va), &f, 4); }
uint32_t FloatBits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}

float Scaled(float fov) { return std::min(fov * nf::FovScale(), 2.9f); }  // short of 180 degrees

bool ValidPtr(uint32_t va, uint32_t size = 4) {
    return va >= 0x10000 && mem::IsCommitted(va) && mem::IsCommitted(va + size - 1);
}

void ForceFullRect(uint32_t cam) {
    if (!ValidPtr(cam, 0x108)) return;
    SetF(cam + kLeft, 0.0f);
    SetF(cam + kRight, 640.0f);
    SetF(cam + kTop, 0.0f);
    SetF(cam + kBottom, 480.0f);
    SetF(cam + kWidth, 640.0f);
    SetF(cam + kHeight, 480.0f);
}

struct PlayerSlotState {
    uint32_t cam = 0;
    uint32_t outer = 0;
    uint32_t inner = 0;
    uint8_t ctrl[kControlSize] = {};
    bool ctrl_init = false;
    uint16_t icon_fade[2] = {};  // the full HUD's icon fades, 0x2790AC and 0x2790B0 (sub_000B2B50)
};

PlayerSlotState g_slot[net::kMaxPlayers] = {};
int g_active_slot = -1;  // when >= 4, this combatant slot (4..9) is currently mapped into hardware slot 3

// the glows 0x6DCB0 and 0xA3EB0 decide to draw over the walls, by object (see sub_000DABB0)
enum : uint8_t { kLampGlow = 1, kOwnGlow = 2 };
static std::unordered_map<uint32_t, uint8_t> g_glows;

void SwapGuestBytes(uint32_t va_a, uint32_t va_b, uint32_t size) {
    uint8_t tmp[64];
    uint8_t* a = &kernel::G<uint8_t>(va_a);
    uint8_t* b = &kernel::G<uint8_t>(va_b);
    std::memcpy(tmp, a, size);
    std::memcpy(a, b, size);
    std::memcpy(b, tmp, size);
}

void SwapAttackerRefs(int s) {
    for (uint32_t i = 0; i < 10; i++) {
        int16_t& att = kernel::G<int16_t>(kCombatantState + i * kCombatantSize + 0x20);
        if (att == 3) att = int16_t(s);
        else if (att == s) att = 3;
    }
}

// Maps extra human player s (4..9) into hardware slot 3 (0x1F661C[3], 0x1F6654[3], 0x2774B8[3], 0x1FE6D0[3])
// and swaps combatant table rows 3 <-> s so all 4-slot and 10-slot lookups using [inner+0x8DE] == 3 access player s.
void MapSlot3(int s) {
    if (s < 4 || s >= net::kMaxPlayers || g_active_slot == s) return;
    g_slot[3].cam = R32(kCameras + 3 * 4);
    g_slot[3].outer = R32(kOuterActors + 3 * 4);
    g_slot[3].inner = R32(kInnerStates + 3 * 4);
    std::memcpy(g_slot[3].ctrl, &kernel::G<uint8_t>(kControls + 3 * kControlSize), kControlSize);

    W32(kCameras + 3 * 4, g_slot[s].cam);
    W32(kOuterActors + 3 * 4, g_slot[s].outer);
    W32(kInnerStates + 3 * 4, g_slot[s].inner);
    std::memcpy(&kernel::G<uint8_t>(kControls + 3 * kControlSize), g_slot[s].ctrl, kControlSize);

    SwapGuestBytes(kCombatantCfg + 3 * kCombatantSize, kCombatantCfg + uint32_t(s) * kCombatantSize, kCombatantSize);
    SwapGuestBytes(kCombatantState + 3 * kCombatantSize, kCombatantState + uint32_t(s) * kCombatantSize, kCombatantSize);
    SwapGuestBytes(kPlaytimeStats + 3 * kPlaytimeSize, kPlaytimeStats + uint32_t(s) * kPlaytimeSize, kPlaytimeSize);
    SwapAttackerRefs(s);

    if (ValidPtr(g_slot[s].inner, 0x8E0)) kernel::G<uint8_t>(g_slot[s].inner + 0x8DE) = 3;
    if (ValidPtr(g_slot[3].inner, 0x8E0)) kernel::G<uint8_t>(g_slot[3].inner + 0x8DE) = uint8_t(s);
    g_active_slot = s;
}

void UnmapSlot3() {
    int s = g_active_slot;
    if (s < 4 || s >= net::kMaxPlayers) return;
    g_slot[s].cam = R32(kCameras + 3 * 4);
    g_slot[s].outer = R32(kOuterActors + 3 * 4);
    g_slot[s].inner = R32(kInnerStates + 3 * 4);
    std::memcpy(g_slot[s].ctrl, &kernel::G<uint8_t>(kControls + 3 * kControlSize), kControlSize);

    W32(kCameras + 3 * 4, g_slot[3].cam);
    W32(kOuterActors + 3 * 4, g_slot[3].outer);
    W32(kInnerStates + 3 * 4, g_slot[3].inner);
    std::memcpy(&kernel::G<uint8_t>(kControls + 3 * kControlSize), g_slot[3].ctrl, kControlSize);

    SwapGuestBytes(kCombatantCfg + 3 * kCombatantSize, kCombatantCfg + uint32_t(s) * kCombatantSize, kCombatantSize);
    SwapGuestBytes(kCombatantState + 3 * kCombatantSize, kCombatantState + uint32_t(s) * kCombatantSize, kCombatantSize);
    SwapGuestBytes(kPlaytimeStats + 3 * kPlaytimeSize, kPlaytimeStats + uint32_t(s) * kPlaytimeSize, kPlaytimeSize);
    SwapAttackerRefs(s);

    if (ValidPtr(g_slot[s].inner, 0x8E0)) kernel::G<uint8_t>(g_slot[s].inner + 0x8DE) = uint8_t(s);
    if (ValidPtr(g_slot[3].inner, 0x8E0)) kernel::G<uint8_t>(g_slot[3].inner + 0x8DE) = 3;
    g_active_slot = -1;
}

struct ScopedPlayerSlot {
    int prev = -1;
    bool changed = false;
    explicit ScopedPlayerSlot(int s) : prev(g_active_slot) {
        int want = (s >= 4 && s < net::kMaxPlayers) ? s : -1;
        if (want == prev) return;
        changed = true;
        if (prev >= 4) UnmapSlot3();
        if (want >= 4) MapSlot3(want);
    }
    ~ScopedPlayerSlot() {
        if (!changed) return;
        if (g_active_slot >= 4) UnmapSlot3();
        if (prev >= 4) MapSlot3(prev);
    }
};

int TruePlayerFromInner(uint32_t inner) {
    if (!ValidPtr(inner, 0x8E0)) return -1;
    if (g_active_slot >= 4) {
        if (inner == R32(kInnerStates + 3 * 4) || inner == g_slot[g_active_slot].inner) {
            kernel::G<uint8_t>(inner + 0x8DF) = uint8_t(g_active_slot);
            return g_active_slot;
        }
        if (inner == g_slot[3].inner) return 3;
    }
    for (int s = 4; s < net::kMaxPlayers; s++) {
        if (inner == g_slot[s].inner) return s;
    }
    int s = int(kernel::G<int8_t>(inner + 0x8DF));
    if (s >= 0 && s < net::kMaxPlayers) return s;
    int p = int(kernel::G<int8_t>(inner + 0x8DE));
    if (p == 3 && g_active_slot >= 4) return g_active_slot;
    return p;
}

int TruePlayerFromOuter(uint32_t outer) {
    if (!ValidPtr(outer, 0xE0) || kernel::G<uint8_t>(outer + 0xDB) == 0x02 || kernel::G<uint8_t>(outer + 0xDB) == 0x11)
        return -1;
    return TruePlayerFromInner(R32(outer + 0xBC));
}

// puts every register back, eax too (as kernel::CallGuest does): a hook that calls the game after its original
// returns the original's eax, as 0xACAB0's callers need the actor it spawned
template <class Fn>
uint32_t CallGuest(X86Context& ctx, uint8_t* base, Fn fn, std::initializer_list<uint32_t> args) {
    uint32_t saved_eax = ctx.eax, saved_ebx = ctx.ebx, saved_ecx = ctx.ecx, saved_edx = ctx.edx;
    uint32_t saved_esi = ctx.esi, saved_edi = ctx.edi, saved_ebp = ctx.ebp, saved_esp = ctx.esp;
    for (auto it = std::rbegin(args); it != std::rend(args); ++it) {
        ctx.esp -= 4;
        W32(ctx.esp, *it);
    }
    ctx.esp -= 4;
    W32(ctx.esp, 0);
    fn(ctx, base);
    uint32_t ret = ctx.eax;
    ctx.eax = saved_eax, ctx.ebx = saved_ebx, ctx.ecx = saved_ecx, ctx.edx = saved_edx;
    ctx.esi = saved_esi, ctx.edi = saved_edi, ctx.ebp = saved_ebp, ctx.esp = saved_esp;
    return ret;
}

int PlayerCameraIndex(uint32_t cam, int* active_count = nullptr) {
    int idx = -1, count = 0;
    for (int p = 0; p < 4; p++) {
        uint32_t c = R32(kCameras + 4 * uint32_t(p));
        if (c) count++;
        if (cam && c == cam) idx = (p == 3 && g_active_slot >= 4) ? g_active_slot : p;
    }
    for (int s = 4; s < net::kMaxPlayers; s++) {
        if (g_slot[s].cam) {
            count++;
            if (cam && g_slot[s].cam == cam) idx = s;
        }
    }
    if (active_count) *active_count = count;
    return idx;
}

bool IsCoopCampaign() {
    uint32_t lvl = R32(0x1F658C);
    return net::Active() && !R32(kIsMultiplayer) && lvl != 0 && lvl != 0x7000048u && net::Players() > 1;
}

int CoopPlayers() {
    return std::clamp(net::Players(), 1, 4);
}

// A co-op player whose machine left the session (net::PlayerLeft) drops out: drawn in no camera, never hurt, never
// a target, and carried along with the first player still playing, so the mission's scripts that follow player 1
// (its triggers, the teleports after a cutscene) still work when the host is the one who left
bool CoopGone(int s) { return s >= 0 && s < 4 && IsCoopCampaign() && net::PlayerLeft(s); }

bool AlivePlayer(uint32_t a) {
    return ValidPtr(a, 0xE0) && kernel::G<int16_t>(a + 0xD0) == 1 && ValidPtr(R32(a + 0xBC), 0x8E0) &&
           GetF(R32(a + 0xBC) + 0x824) > 0.0f;
}

// a player's HUD parts ([inner+0x770]: 22 of 0x20 bytes, +0x24 shown): out of a match a death (0xAC690) hides them
// all but 12, its red view, and only a new level shows them again; a co-op respawn puts back those shown before
constexpr uint32_t kHudParts = 22, kHudPartSize = 0x20;
uint8_t g_hud_shown[4][kHudParts];
void KeepHud(uint32_t inner, int s) {
    uint32_t t = R32(inner + 0x770);
    if (!t) return;
    for (uint32_t k = 0; k < kHudParts; k++) g_hud_shown[s][k] = kernel::G<uint8_t>(t + k * kHudPartSize + 0x24);
}
void RestoreHud(uint32_t inner, int s) {
    uint32_t t = ValidPtr(inner, 0x8E0) ? R32(inner + 0x770) : 0;
    if (!t || s < 0 || s >= 4) return;
    for (uint32_t k = 0; k < kHudParts; k++) kernel::G<uint8_t>(t + k * kHudPartSize + 0x24) = g_hud_shown[s][k];
}

// a player's weapons ([outer+0xB8]: +0x52 held, +0x53 to draw next, +0x54 the last; 0x47 none): a death (0xB9B90)
// holsters it and the camera (0xA8A60) hides the first-person view of a player holding none; a co-op respawn draws
// the one held before, as a match's loadout (0x9D5B0) does
constexpr uint8_t kNoWeapon = 0x47;
uint8_t g_weapon_held[4] = {kNoWeapon, kNoWeapon, kNoWeapon, kNoWeapon};
void KeepWeapon(uint32_t outer, int s) {
    uint32_t w = R32(outer + 0xB8);
    if (ValidPtr(w, 0x60) && kernel::G<uint8_t>(w + 0x52) != kNoWeapon) g_weapon_held[s] = kernel::G<uint8_t>(w + 0x52);
}
void DrawWeapon(uint32_t outer, int s) {
    uint32_t w = ValidPtr(outer, 0xE0) ? R32(outer + 0xB8) : 0;
    if (!ValidPtr(w, 0x60) || s < 0 || s >= 4) return;
    kernel::G<uint8_t>(w + 0x52) = kNoWeapon;
    kernel::G<uint8_t>(w + 0x54) = kNoWeapon;
    kernel::G<uint8_t>(w + 0x53) = g_weapon_held[s];
}

bool TeammateAlive(int s) {
    for (int i = 0; i < CoopPlayers(); i++)
        if (i != s && !CoopGone(i) && AlivePlayer(R32(kOuterActors + uint32_t(i) * 4))) return true;
    return false;
}

// the first co-op player still playing and alive but s, or 0
uint32_t CoopCompanion(int s) {
    for (int i = 0; i < CoopPlayers(); i++) {
        uint32_t a = R32(kOuterActors + uint32_t(i) * 4);
        if (i != s && !CoopGone(i) && AlivePlayer(a)) return a;
    }
    return 0;
}

// a co-op campaign's player cameras: their draw switches (+0x20 world, +0x29 3D, +0x2A 2D), their draw lists (+0x08,
// counts +0x14, room +0x1A), their fade (+0x205 on, +0x203 alpha) and their screen rectangle
void LogCoopCameras(const char* when) {
    for (int s = 0; s < 4; s++) {
        uint32_t cam = R32(kCameras + 4 * uint32_t(s));
        if (!ValidPtr(cam, 0x208)) continue;
        auto u8 = [&](uint32_t o) { return unsigned(kernel::G<uint8_t>(cam + o)); };
        auto u16 = [&](uint32_t o) { return unsigned(kernel::G<uint16_t>(cam + o)); };
        LOGR("nightfire: co-op camera %d %s: %08X world %u 3D %u 2D %u lists %08X counts %u/%u/%u room %u/%u/%u fade %u "
             "alpha %u rect %.0f,%.0f-%.0f,%.0f",
             s, when, cam, u8(0x20), u8(0x29), u8(0x2A), R32(cam + 0x08), u16(0x14), u16(0x16), u16(0x18), u16(0x1A),
             u16(0x1C), u16(0x1E), u8(0x205), u8(0x203), GetF(cam + kLeft), GetF(cam + kTop), GetF(cam + kRight),
             GetF(cam + kBottom));
    }
}

uint32_t g_coop_spawn_pos = 0;
uint32_t g_coop_spawn_rot = 0;
uint32_t g_coop_spawn_start = 0;  // player 1's start point (0xACAB0's fourth argument)
bool g_coop_respawning = false;

// A player's movement state is [outer+0xD2] (0xA9720 sets it; 0 on foot). The spawn (0xACAB0) takes the start
// point's +0x38: 2 floats the player (state 8, through 0xAC7E0) and zeroes the level's gravity, the vector
// 0x1F6648 every spawn sets to (0, -9.8, 0) first; Equinox (0700001B) starts so. The co-op players spawn from a
// copy of player 1's start when it floats them (else from none, as before), are respawned and carried in their
// teammate's state, and follow player 1 between floating and on foot (2026-11-19, John: player 2 did not float in
// the final mission's space part; headless their state 0, and gravity back on for every player)
constexpr uint32_t kMoveState = 0xD2, kStartMode = 0x38;
constexpr int16_t kFloating = 8;
constexpr uint32_t kFloatStart = 2;

uint32_t MoveMode(uint32_t mate) {
    return ValidPtr(mate, 0xE0) && kernel::G<int16_t>(mate + kMoveState) == kFloating ? uint32_t(kFloating) : 0u;
}
uint32_t g_coop_frames = 0;  // frames drawn since the level's cameras were made
uint32_t g_coop_default_model = 0;
int g_applied_char[net::kMaxPlayers] = {-2, -2, -2, -2, -2, -2, -2, -2, -2, -2};

// The first-person hands. Every weapon has its view model (0x18D024 + 0x10C * weapon + 0x58); weapon 1's, [0x18D188],
// is the bare hands, with its mode [0x190939], and a player's skin [inner + 0x8F5] retextures the hands on every
// weapon's model. A campaign level's spawn (0xACAB0) sets the mission's hands (mode 1, skin by level), a match's the
// multiplayer hands 050000B0 (mode 2) with the character's skin (0x1637C0 + 8). In a co-op campaign a player wearing
// a multiplayer character gets a match's hands and everyone else the mission's: the global is that player's while
// their update and camera run (HandsScope), the mission's otherwise
struct Hands {
    uint32_t model;
    uint8_t mode, skin;
};
uint32_t g_mission_hands = 0;
uint8_t g_mission_skin = 0;

int WornCharacter(uint32_t outer) {
    uint32_t b8 = ValidPtr(outer, 0xE0) ? R32(outer + 0xB8) : 0;
    uint32_t rec = ValidPtr(b8, 0x80) ? R32(b8 + 0x74) : 0;
    uint32_t model = ValidPtr(rec, 4) ? R32(rec) : 0;
    for (uint32_t i = 0; i < 29 && model; i++)
        if (R32(0x1637C0u + i * 0x10u) == model) return int(i);
    return -1;
}

bool HandsFor(X86Context& ctx, uint8_t* base, uint32_t outer, Hands& h) {
    int ch = WornCharacter(outer);
    if (ch >= 0 && CallGuest(ctx, base, __imp__sub_0006B500, {0x50000B0u})) {
        h = {0x50000B0u, 2, uint8_t(R32(0x1637C8u + uint32_t(ch) * 0x10u))};
        return true;
    }
    if (!g_mission_hands || !IsCoopCampaign()) return false;
    h = {g_mission_hands, 1, g_mission_skin};
    return true;
}

struct HandsScope {
    uint32_t model = 0;
    uint8_t mode = 0;
    bool on = false;
    HandsScope(X86Context& ctx, uint8_t* base, uint32_t outer) {
        Hands h;
        if (!IsCoopCampaign() || !HandsFor(ctx, base, outer, h)) return;
        model = R32(0x18D188), mode = kernel::G<uint8_t>(0x190939), on = true;
        W32(0x18D188, h.model);
        kernel::G<uint8_t>(0x190939) = h.mode;
    }
    ~HandsScope() {
        if (!on) return;
        W32(0x18D188, model);
        kernel::G<uint8_t>(0x190939) = mode;
    }
};

// a player's skin, and with weapon 1 out their view model, made their hands (0x118F0 rebuilds the view with the skin).
// A view model built with the other hands (outside HandsScope: the spawn's) is rebuilt as the weapon change does
// (0xB7DF0): 0x17A60 frees its instance and the attachments its record counts ([rec + 0x29], at [b8 + 0xA0]), then
// 0x11440 builds the new one. A model whose record changes under its attachments leaves them behind on the next
// weapon change, drawn in the player's camera alone where they last were
void ApplyPlayerHands(X86Context& ctx, uint8_t* base, uint32_t outer, int slot) {
    uint32_t inner = ValidPtr(outer, 0xE0) ? R32(outer + 0xBC) : 0;
    uint32_t b8 = ValidPtr(outer, 0xE0) ? R32(outer + 0xB8) : 0;
    Hands h;
    if (!ValidPtr(inner, 0x900) || !ValidPtr(b8, 0x80) || !HandsFor(ctx, base, outer, h)) return;
    uint32_t vm = R32(inner + 0x778);
    uint32_t vm_b8 = ValidPtr(vm, 0xE0) ? R32(vm + 0xB8) : 0;
    uint32_t rec = CallGuest(ctx, base, __imp__sub_0006B500, {h.model});
    uint32_t other = CallGuest(ctx, base, __imp__sub_0006B500, {h.model == 0x50000B0u ? g_mission_hands : 0x50000B0u});
    bool bare = kernel::G<int8_t>(b8 + 0x52) == 1 && ValidPtr(vm_b8, 0xF0) && ValidPtr(rec, 0x30) && other &&
                R32(vm_b8 + 0x74) == other;
    uint8_t old_skin = kernel::G<uint8_t>(inner + 0x8F5);
    if (old_skin == h.skin && !bare) return;
    kernel::G<uint8_t>(inner + 0x8F5) = h.skin;
    if (bare) {
        CallGuest(ctx, base, __imp__sub_00017A60, {vm});
        CallGuest(ctx, base, __imp__sub_00011440, {vm, h.model});
        CallGuest(ctx, base, __imp__sub_000DB3D0, {vm});
        vm_b8 = R32(vm + 0xB8);
    }
    if (ValidPtr(vm, 0xE0) && vm_b8) CallGuest(ctx, base, __imp__sub_000118F0, {vm, uint32_t(h.skin)});
    LOGR("nightfire: player %d's hands: model %08X, skin %u (was %u)%s", slot + 1, h.model, h.skin, old_skin,
         bare ? ", view model swapped" : "");
}

// retextures a view model with a skin (cdecl: view model, skin): its [b8+0x88] is the skin's texture set (0x1634C0's
// entry). The watch gadgets' model (0500004B, the laser's) has its watch body in the mission skins' textures alone, so a multiplayer character's skin left only the dial's marks on the wrist: the watch keeps the mission's
constexpr uint32_t kWatchModel = 0x500004Bu;
X86_FUNC(sub_000118F0) {
    uint32_t vm = R32(ctx.esp + 4);
    uint32_t b8 = ValidPtr(vm, 0xE0) ? R32(vm + 0xB8) : 0;
    uint32_t rec = ValidPtr(b8, 0x80) ? R32(b8 + 0x74) : 0;
    if (IsCoopCampaign() && g_mission_hands && ValidPtr(rec, 4) && R32(rec) == kWatchModel &&
        R32(ctx.esp + 8) != g_mission_skin) {
        LOG_FIRST(4, "nightfire: the watch keeps the mission's skin %u (not %u)", g_mission_skin, R32(ctx.esp + 8));
        W32(ctx.esp + 8, g_mission_skin);
    }
    __imp__sub_000118F0(ctx, base);
}

// one line per character slot: the menu's name, the 0x1637C0 entry (model, +4, +8, need flag) and whether the model
// is in memory now, so a mismatch between the F1 list and the model a player gets can be read off the log
void LogCharacterTable(X86Context& ctx, uint8_t* base, const char* when) {
    for (uint32_t i = 0; i < 29; i++) {
        uint32_t e = 0x1637C0u + i * 0x10u;
        uint32_t model = R32(e);
        uint32_t loaded = model ? CallGuest(ctx, base, __imp__sub_0006B500, {model}) : 0;
        LOGR("nightfire: diag table %s: [%2u] \"%s\" model %08X +4 %08X +8 %08X need %u loaded %08X", when, i + 1,
             net::CharacterChoiceName(i + 1), model, R32(e + 4), R32(e + 8), unsigned(kernel::G<uint8_t>(e + 0xC)),
             loaded);
    }
    for (int s = 0; s < CoopPlayers(); s++)
        LOGR("nightfire: diag table %s: player %d chose %d", when, s + 1, net::CharacterFor(s) + 1);
}

// a player's chosen character, when the level registered it as it loaded (src/game/characters.cpp; a match's archive
// has them all), else the mission's model: choice 1, Bond, plays as the mission's own model, and a choice made during
// a level waits for the next one
uint32_t ResolveCoopModel(X86Context& ctx, uint8_t* base, int slot, uint32_t default_model) {
    int ch = net::CharacterFor(slot);
    if (ch <= 0 || ch >= 29) return default_model;
    uint32_t model = R32(0x1637C0u + uint32_t(ch) * 0x10u);
    if (model && CallGuest(ctx, base, __imp__sub_0006B500, {model})) return model;
    LOGR("nightfire: player %d's choice %d (%s) is not in this level; the mission's model %08X until the next one",
         slot + 1, ch + 1, net::CharacterChoiceName(uint32_t(ch) + 1), default_model);
    return default_model;
}

void ApplyActorCharacterModel(X86Context& ctx, uint8_t* base, uint32_t actor, int slot, uint32_t target_model) {
    if (!ValidPtr(actor, 0xE0) || !target_model) return;
    uint32_t ebp = CallGuest(ctx, base, __imp__sub_0006B500, {target_model});
    uint32_t b8 = R32(actor + 0xB8);
    if (!ValidPtr(ebp, 0x30) || !ValidPtr(b8, 0xF0)) return;
    W32(b8 + 0x74, ebp);
    W32(ebp, target_model);
    uint32_t flags = R32(b8 + 0xA8);
    if (kernel::G<uint8_t>(ebp + 0x2C) == 1)
        flags &= ~0x400u;
    else
        flags |= 0x400u;
    W32(b8 + 0xA8, flags);
    ApplyPlayerHands(ctx, base, actor, slot);
}

void ForceAllPlayerRects() {
    for (uint32_t i = 0; i < 4; i++) ForceFullRect(R32(kCameras + 4 * i));
    for (int s = 0; s < net::kMaxPlayers; s++) ForceFullRect(g_slot[s].cam);
}

uint8_t HudViewForPlayer(int player) {
    if (player != net::LocalPlayer()) return 0xFE;
    uint32_t cam4 = R32(kCameras + 4 * 4);
    return (ValidPtr(cam4, 0x30) && kernel::G<uint8_t>(cam4 + 0x29)) ? 4 : 5;
}

void TagSpriteView(uint32_t sp, int player) {
    if (!ValidPtr(sp, 0x40)) return;
    uint8_t& cur = kernel::G<uint8_t>(sp + 0x39);
    if (player != net::LocalPlayer()) {
        if (cur <= 5) cur = 0xFE;
    } else if (cur == 0xFE) {
        cur = HudViewForPlayer(player);
    }
}

void TagSpriteArray(uint32_t arr, uint16_t count, int player, uint16_t max_count) {
    if (!count || count > max_count) count = max_count;
    if (!ValidPtr(arr, uint32_t(count) * 4)) return;
    for (uint16_t i = 0; i < count; i++) {
        uint32_t sp = R32(arr + uint32_t(i) * 4);
        if (!ValidPtr(sp, 0x40)) continue;  // a group's array has empty slots between its sprites (0xB6190 skips them)
        TagSpriteView(sp, player);
    }
}

void TagGroupSprites(uint32_t group, int player, uint16_t max_count) {
    if (!ValidPtr(group, 0x1C)) return;
    TagSpriteArray(R32(group + 0x08), kernel::G<uint16_t>(group + 0x18), player, max_count);
}

// a multiplayer match is up or loading ([0x1F65D4] is the mode 0xBEC90 copies into [0x260018] as a level starts)
bool MatchLoading() { return R32(kIsMultiplayer) || R32(0x1F65D4); }

void InitDefaultCombatant(int s, bool force_player_name = false) {
    uint32_t cfg = kCombatantCfg + uint32_t(s) * kCombatantSize;
    char* name = reinterpret_cast<char*>(&kernel::G<uint8_t>(cfg));
    if (net::PlayerName(s)[0]) std::snprintf(name, 31, "%s", net::PlayerName(s));
    else if (force_player_name || !name[0]) std::snprintf(name, 31, "Player %d", s + 1);
    if (R32(kIsTeamPlay)) W32(cfg + 0x20, uint32_t(s & 1));
    int chosen = net::CharacterFor(s);
    if (chosen >= 0 && chosen < 29) {
        W32(cfg + 0x24, uint32_t(chosen));
    } else if (s > 0 && R32(cfg + 0x24) == 0) {
        W32(cfg + 0x24, uint32_t(s % 6));
    }
    W32(cfg + 0x28, R32(kCombatantCfg + 0x28));  // inherit radar mode from Player 1
    if (!g_slot[s].ctrl_init) {
        std::memcpy(g_slot[s].ctrl, &kernel::G<uint8_t>(kControls), kControlSize);
        std::memset(g_slot[s].ctrl + 0x14, 0, kControlSize - 0x18);
        g_slot[s].ctrl[0x156] = uint8_t(std::min(s, 3));
        g_slot[s].ctrl_init = true;
    }
}

// a match's combatants as the session chose them (the spawn, 0xACAB0, takes each one's model from +0x24): a player's
// choice, else the menu's, else for the players after the first one of the first six characters
// the session's players' names: each one's net_name; with none, Player N in co-op and the game's own in a match
// (its codename screen's, or Player N)
void SessionNames(bool coop) {
    if (!net::Active()) return;
    int total = coop ? CoopPlayers() : std::clamp(net::Players(), 1, net::kMaxPlayers);
    for (int s = 0; s < total; s++) {
        char* name = reinterpret_cast<char*>(&kernel::G<uint8_t>(kCombatantCfg + uint32_t(s) * kCombatantSize));
        const char* typed = net::PlayerName(s);
        if (typed[0]) std::snprintf(name, 31, "%s", typed);
        else if (coop) std::snprintf(name, 31, "Player %d", s + 1);
    }
}

void ApplyCharacterSelections() {
    if (!MatchLoading()) return;
    int total = net::Active() ? std::clamp(net::Players(), 1, net::kMaxPlayers) : int(std::min<uint32_t>(R32(kPlayerCount), 4));
    SessionNames(false);
    for (int s = 0; s < net::kMaxPlayers; s++) {
        int ch = net::CharacterFor(s);
        uint32_t cfg = kCombatantCfg + uint32_t(s) * kCombatantSize;
        int32_t handicap;  // the spawn (0xACD7E) starts a match's combatant at 100 + cfg+0x2C
        if (s < total && net::HandicapFor(s, handicap)) W32(cfg + 0x2C, uint32_t(handicap));
        if (ch >= 0 && ch < 29) {
            W32(cfg + 0x24, uint32_t(ch));
            if (s >= 4) kernel::G<uint8_t>(0x245292u + uint32_t(s - 4) * 0x12u) = uint8_t(ch);
        } else if (s < total && s > 0 && R32(cfg + 0x24) == 0) {
            W32(cfg + 0x24, uint32_t(s % 6));
        }
    }
}

// the camera at [esp+4] widened and its angle scaled while call runs
template <class Fn>
void Widened(uint32_t cam, Fn call) {
    float k = R32(kWidescreen) ? nf::Ultrawide() : 1.0f, s = nf::FovScale();
    if (!cam || (k <= 1.0f + 1e-3f && s == 1.0f)) return call();
    float aspect = GetF(cam + kAspect), fov = GetF(cam + kFov);
    SetF(cam + kAspect, aspect * k);
    SetF(cam + kFov, Scaled(fov));
    call();
    SetF(cam + kAspect, aspect);
    SetF(cam + kFov, fov);
}
}  // namespace

namespace nf {
int CameraOwner(uint32_t cam, int* active_count = nullptr) { return PlayerCameraIndex(cam, active_count); }
int CoopCampaignPlayers() { return IsCoopCampaign() ? CoopPlayers() : 0; }
}  // namespace nf

// initializes the global 2D sprite pool at 0x29AA98 (512 sprites, 0x44 bytes each). Each player's HUD takes about
// 170 of them, and 0xCEDF0 answers a full pool with no sprite, which the HUD's builders (0xB5C30) and the menus' text
// (0x90FC0) write through: a 5+ player match gets 2048, a co-op level 1024 (level 7000002 peaks at 536 with 3
// players, 769 with 4 and every pause up; 7000008 passed 512 with 2, both pauses up and a PC Settings page), so a
// campaign level keeps the rest of its heap for its scene and sound archives
X86_FUNC(sub_000CEB30) {
    uint32_t count = 0;
    if (net::Active() && MatchLoading() && net::Players() > 4) count = 0x800;
    else if (net::Active() && !MatchLoading() && net::Players() > 1) count = 0x400;
    if (count) {
        ctx.eax = CallGuest(ctx, base, __imp__sub_0006EAD0, {0x29AA98u, 0x44u, count});
        ctx.esp += 4;
        return;
    }
    __imp__sub_000CEB30(ctx, base);
}

// a match starting (0xBDDF0 calls it for the multiplayer maps): it sets need flags at 0x1637CC from the combatants,
// which nothing reads (a match's archive holds all 29 characters); the session's choices go into the combatants
X86_FUNC(sub_0001A1E0) {
    ApplyCharacterSelections();
    __imp__sub_0001A1E0(ctx, base);
    ApplyCharacterSelections();
}

// attaches a 3D skeletal character model to an actor (cdecl: actor, model_id):
// in campaign co-op, if a player selected a character model that is loaded in this level's archive,
// use it (or map their choice across the human character models packed in this mission's archive)
static bool g_in_11440 = false;
// in a spawn (0xACAB0) the hands wait for its end, where the mission's skin is read from the player the game made
static bool g_in_spawn = false;
X86_FUNC(sub_00011440) {
    uint32_t actor = R32(ctx.esp + 4);
    uint32_t model_id = R32(ctx.esp + 8);
    bool is_player = ValidPtr(actor, 0xE0) && kernel::G<uint8_t>(actor + 0xDB) == 0x03;
    int slot = -1;
    if (IsCoopCampaign() && is_player && !g_in_11440) {
        if (model_id && model_id != 0x50000B0u && !g_coop_default_model) g_coop_default_model = model_id;
        for (int s = 0; s < 4; s++) {
            if (R32(kOuterActors + uint32_t(s) * 4) == actor) {
                slot = s;
                break;
            }
        }
        if (slot >= 0 && slot < 4) {
            uint32_t chosen_model = ResolveCoopModel(ctx, base, slot, g_coop_default_model ? g_coop_default_model : model_id);
            if (chosen_model) {
                W32(ctx.esp + 8, chosen_model);
                model_id = chosen_model;
            }
            g_applied_char[slot] = net::CharacterFor(slot);
        }
    }
    uint32_t ebp = is_player && net::Active() ? CallGuest(ctx, base, __imp__sub_0006B500, {model_id}) : 0;
    uint8_t old_2a = 0, old_2b = 0;
    if (ValidPtr(ebp, 0x30)) {
        old_2a = kernel::G<uint8_t>(ebp + 0x2A);
        old_2b = kernel::G<uint8_t>(ebp + 0x2B);
        if (old_2a < 16) kernel::G<uint8_t>(ebp + 0x2A) = 16;
        if (old_2b < 16) kernel::G<uint8_t>(ebp + 0x2B) = 16;
    }
    bool prev_in = g_in_11440;
    g_in_11440 = true;
    __imp__sub_00011440(ctx, base);
    g_in_11440 = prev_in;
    if (ValidPtr(ebp, 0x30)) {
        kernel::G<uint8_t>(ebp + 0x2A) = old_2a;
        kernel::G<uint8_t>(ebp + 0x2B) = old_2b;
    }
    if (IsCoopCampaign() && is_player && slot >= 0 && slot < 4 && !g_in_spawn) {
        ApplyPlayerHands(ctx, base, actor, slot);
    }
}

// the world's visibility setup (0xCBDF0 sizes the per-camera buffers through 0xC5E30), the per-camera visibility
// pass (0xCC1C0, tail-jumps into 0xCAD40) and the per-frame cull pass (0xCB580) all take their camera count as
// [0x260018] ? [0x260044] : 1, so in a campaign level only camera 0 ever gets a visible set and every other
// player's camera draws an empty world
static void WithCoopCameraCount(decltype(__imp__sub_000CBDF0)* fn, X86Context& ctx, uint8_t* base) {
    bool coop = IsCoopCampaign();
    uint32_t was = R32(kIsMultiplayer);
    if (coop) W32(kIsMultiplayer, 1);
    fn(ctx, base);
    if (coop) W32(kIsMultiplayer, was);
}

X86_FUNC(sub_000CBDF0) { WithCoopCameraCount(__imp__sub_000CBDF0, ctx, base); }
X86_FUNC(sub_000CC1C0) { WithCoopCameraCount(__imp__sub_000CC1C0, ctx, base); }
X86_FUNC(sub_000CB580) { WithCoopCameraCount(__imp__sub_000CB580, ctx, base); }

// updates a camera's screen fade tint (cdecl: cam_idx):
// in singleplayer campaign, only camera 0 is ticked by the engine's fade updater, leaving Players 2..4 stuck on a
// solid black fade overlay ([cam+0x203]=0xFF, [cam+0x205]=1 set by sub_000243B0 in sub_000ACAB0) and leaving all
// player cameras black if a cinematic fade on camera 4 finishes via sub_000C3915(0, -22.0f, 0xFF, 1).
X86_FUNC(sub_00024FD0) {
    uint32_t idx = R32(ctx.esp + 4) & 0xFFFF;
    __imp__sub_00024FD0(ctx, base);
    if (idx == 0 && IsCoopCampaign()) {
        int total = CoopPlayers();
        for (int s = 1; s < total; s++) {
            CallGuest(ctx, base, __imp__sub_00024FD0, {uint32_t(s)});
        }
    }
}

// enables/disables a camera (cdecl: cam_idx, enable_3d, enable_2d, copy_from_cam):
// when a campaign cutscene activates or deactivates camera 7 (cinematic camera) and toggles camera 0,
// mirror the 3D/2D enable state onto co-op Players 2..4's cameras so cutscenes and gameplay transitions match.
X86_FUNC(sub_00024150) {
    uint32_t idx = R32(ctx.esp + 4) & 0xFFFF;
    uint32_t en3d = R32(ctx.esp + 8) & 0xFF;
    uint32_t en2d = R32(ctx.esp + 0xC) & 0xFF;
    uint32_t from = R32(ctx.esp + 0x10);
    __imp__sub_00024150(ctx, base);
    if (idx == 0 && from == 0 && IsCoopCampaign()) {
        int total = CoopPlayers();
        for (int s = 1; s < total; s++) {
            uint32_t cam = R32(kCameras + uint32_t(s) * 4);
            if (!ValidPtr(cam, 0x30) || !R32(kOuterActors + uint32_t(s) * 4)) continue;
            CallGuest(ctx, base, __imp__sub_00024150, {uint32_t(s), en3d, en2d, 0u});
        }
    }
}

// sets a camera's screen rectangle (cdecl: index, float x, float y, float w, float h) and calls 0x25190
X86_FUNC(sub_00025B80) {
    uint32_t i = R32(ctx.esp + 4) & 0xFFFF;
    if (net::FullScreenView() && i < 4) {
        SetF(ctx.esp + 8, 0.0f);
        SetF(ctx.esp + 0xC, 0.0f);
        SetF(ctx.esp + 0x10, 640.0f);
        SetF(ctx.esp + 0x14, 480.0f);
    }
    __imp__sub_00025B80(ctx, base);
}

// sets up all cameras for the level/match: in online multiplayer or 4-player co-op campaign, ensures cameras exist
X86_FUNC(sub_00025D10) {
    for (auto& s : g_slot) s = PlayerSlotState{};
    for (int& c : g_applied_char) c = -2;
    g_coop_default_model = 0;
    g_mission_hands = 0;
    g_active_slot = -1;
    g_coop_frames = 0;
    g_glows.clear();
    bool mp = R32(kIsMultiplayer) != 0;
    bool coop = IsCoopCampaign();
    if (net::Active() && (mp || coop)) {
        uint32_t base_humans = uint32_t(std::clamp(net::Players(), 1, 4));
        uint32_t cur_humans = R32(kPlayerCount);
        if (cur_humans < base_humans) {
            for (uint32_t i = cur_humans; i < base_humans; i++) {
                InitDefaultCombatant(int(i), true);
                std::memcpy(&kernel::G<uint8_t>(kControls + i * kControlSize), g_slot[i].ctrl, kControlSize);
            }
            W32(kPlayerCount, base_humans);
        }
    }
    __imp__sub_00025D10(ctx, base);
    if (!net::Active() || (!mp && !coop)) return;
    int total = coop ? CoopPlayers() : std::clamp(net::Players(), 1, net::kMaxPlayers);
    // camera 0 is made as singleplayer makes it: with [0x260018] set, 0x25BF0 builds every camera the way a split
    // screen match does (its draw lists at 0x24150 halved for one), which a campaign level's scene does not fit in;
    // the other players' cameras are made after it the same way
    if (coop) {
        uint32_t world = R32(kWorldRoot);
        for (int s = 1; s < total; s++) {
            if (R32(kCameras + 4 * uint32_t(s))) continue;
            CallGuest(ctx, base, __imp__sub_00025BF0, {uint32_t(s), world, 0u, 0u, 0u, 0x280u, 0x1E0u});
        }
        // the split screen 0x25D10 lays out for this many players (sub_00025B80 makes each full screen online)
        for (int s = 0; s < total; s++) {
            float x = 0, y = 0, w = 640, h = 480;
            if (total == 2 && R32(0x1F660C) == 1) x = s * 320.0f, w = 320;
            else if (total == 2) y = s * 240.0f, h = 240;
            else if (total > 2) x = (s & 1) * 320.0f, y = (s >> 1) * 240.0f, w = 320, h = 240;
            CallGuest(ctx, base, sub_00025B80, {uint32_t(s), FloatBits(x), FloatBits(y), FloatBits(w), FloatBits(h)});
        }
        LogCoopCameras("made");
    }
    for (int i = 0; i < std::min(total, 4); i++) {
        g_slot[i].cam = R32(kCameras + 4 * uint32_t(i));
        std::memcpy(g_slot[i].ctrl, &kernel::G<uint8_t>(kControls + uint32_t(i) * kControlSize), kControlSize);
        g_slot[i].ctrl_init = true;
    }
    if (mp && total > 4) {
        uint32_t world = R32(kWorldRoot);
        uint32_t cam3 = g_slot[3].cam;
        for (int s = 4; s < total; s++) {
            InitDefaultCombatant(s, true);
            W32(kCameras + 3 * 4, 0);
            CallGuest(ctx, base, __imp__sub_00025BF0, {3u, world, 0u, 0u, 0u, 0x280u, 0x1E0u});
            CallGuest(ctx, base, sub_00025B80, {3u, FloatBits(0.0f), FloatBits(0.0f), FloatBits(640.0f), FloatBits(480.0f)});
            g_slot[s].cam = R32(kCameras + 3 * 4);
        }
        W32(kCameras + 3 * 4, cam3);
    }
}

X86_FUNC(sub_00025190) {
    uint32_t i = R32(ctx.esp + 4) & 0xFFFF;
    float fov = GetF(ctx.esp + 8);
    uint32_t cam = i < 9 ? R32(kCameras + 4 * i) : 0;
    uint32_t saved[4] = {};
    bool full = net::FullScreenView() && i < 4 && cam != 0;
    if (full) {
        ForceFullRect(cam);
        // 0x25190 counts non-null player cameras at 0x1F661C[0..3] and halves or doubles the aspect when the count is 2;
        // hiding the other three for the call gives every player camera the 1-player full-screen aspect and pixel scales
        for (uint32_t j = 0; j < 4; j++) {
            saved[j] = R32(kCameras + 4 * j);
            if (j != i) W32(kCameras + 4 * j, 0);
        }
    }
    __imp__sub_00025190(ctx, base);
    if (full) {
        for (uint32_t j = 0; j < 4; j++) W32(kCameras + 4 * j, saved[j]);
    }
    if (!cam || GetF(cam + kFov) != fov || nf::FovScale() == 1.0f) return;  // not set (no camera, no size)
    float r = std::tan(fov / 2) / std::tan(Scaled(fov) / 2);
    SetF(cam + kScaleX, GetF(cam + kScaleX) * r);
    SetF(cam + kScaleY, GetF(cam + kScaleY) * r);
}

static uint32_t SlotActor(int s) { return R32(kCombatantState + uint32_t(s) * kCombatantSize + 0x1C); }

// a bot below slot 4 (the session's bots past the game's six, in the slots between its players and 4): the k-th one's
// slot, -1 none. Its state is in net::BotBlocks(), a block a slot, not the game's six (which start at slot 4)
static int LowBotSlot(int k) {
    uint32_t blocks = net::BotBlocks();
    if (!blocks || !net::Active() || !R32(kIsMultiplayer)) return -1;
    for (int s = std::max(net::Players(), 0); s < 4; s++) {
        uint32_t a = SlotActor(s);
        if (!ValidPtr(a, 0xC0) || !ValidPtr(R32(a + 0xBC), 0x978)) continue;
        uint32_t state = R32(R32(a + 0xBC) + 0x974);
        if (state >= blocks && state < blocks + 4 * kBotState && k-- == 0) return s;
    }
    return -1;
}

// a bot's name as the game names its six (a "Bot 1" of slots 4..9, in the disc's language): its number n
static void BotName(int s, int n) {
    std::string name;
    for (int k = 4; k < 4 + int(kBotSlots) && name.empty(); k++) {
        name.assign(reinterpret_cast<const char*>(&kernel::G<uint8_t>(kCombatantCfg + uint32_t(k) * kCombatantSize)), 0, 31);
        size_t stem = name.find_last_not_of("0123456789") + 1;
        name.resize(stem < name.size() ? stem : 0);
    }
    if (name.empty()) name = "Bot ";
    std::snprintf(reinterpret_cast<char*>(&kernel::G<uint8_t>(kCombatantCfg + uint32_t(s) * kCombatantSize)), 31, "%s%d",
                  name.c_str(), n);
}

int g_low_bots = 0;  // the match's bots below slot 4 (SpawnLowBots)

// the bots past the game's six (net_bots): spawned as 0xA25F0 spawns its own (0x9D3D0's spawn point for the slot's
// team, 0x1B170 with the bot menu's entry, the first ones' again), in slots players..3
static void SpawnLowBots(X86Context& ctx, uint8_t* base, int players, int count) {
    if (!net::BotBlocks()) {
        uint32_t va = mem::AllocContiguous(4 * kBotState, 0, 0xFFFFFFFF, 16);
        if (!va) {
            LOGE("nightfire: no memory for the bots below slot 4");
            return;
        }
        net::SetBotBlocks(va);
    }
    bool menu = kernel::G<uint8_t>(kBotsEnabled) != 0;
    int spawned = 0;
    for (int k = 0; k < count && players + k < 4; k++) {
        int s = players + k;
        uint32_t cfg = kCombatantCfg + uint32_t(s) * kCombatantSize;
        uint32_t entry = menu ? kMenuBots + uint32_t(k % kBotSlots) * 0x12 : 0;
        W32(kCombatantState + uint32_t(s) * kCombatantSize + 0x1C, 0);
        if (!nf::BotsName(ctx, int(kBotSlots) + k, cfg)) BotName(s, int(kBotSlots) + k + 1);
        if (menu) W32(cfg + 0x20, kernel::G<uint8_t>(entry + 0x0F));
        else if (R32(kIsTeamPlay)) W32(cfg + 0x20, uint32_t(s & 1));
        uint32_t team = kernel::G<uint16_t>(cfg + 0x20);
        uint32_t spot = CallGuest(ctx, base, __imp__sub_0009D3D0, {team, 0u}) & 0xFFFF;
        uint32_t actor = CallGuest(ctx, base, sub_0001B170,
                                   {uint32_t(s), 0x261D58 + spot * 0x1C, 0x261D64 + spot * 0x1C, 0u, entry, 0u});
        if (actor) spawned++;
    }
    W32(kCombatants, R32(kCombatants) + uint32_t(spawned));
    g_low_bots = spawned;
    LOGR("nightfire: %d bots in the slots below 4", spawned);
}

// spawns all players (0..3) and bots (4..9) for a multiplayer match
X86_FUNC(sub_000A25F0) {
    ApplyCharacterSelections();
    if (net::Active() && R32(kIsMultiplayer)) {
        g_low_bots = 0;
        nf::BotsMatchStart(ctx);
        int total = std::clamp(net::Players(), 1, net::kMaxPlayers);
        for (int s = 0; s < std::min(total, 4); s++) {
            int ch = net::CharacterFor(s);
            if (ch >= 0 && ch < 29) W32(kCombatantCfg + uint32_t(s) * kCombatantSize + 0x24, uint32_t(ch));
        }
        // the game spawns its bots (0x1B170, from the bot menu's count, six at most) in slots 4..9: the players past 4
        // take the first of those slots (sub_0001B170 makes them players) and the bots the rest; the bots past those
        // (net_bots, up to 10 in all) go in the slots between the players and 4 (SpawnLowBots)
        bool menu = kernel::G<uint8_t>(kBotsEnabled) != 0;
        uint32_t menu_bots = menu ? kernel::G<uint8_t>(kMenuBotCount) : R32(kBotCount);
        int asked = net::Bots();
        uint32_t want = std::min<uint32_t>(asked >= 0 ? uint32_t(asked) : menu_bots, uint32_t(net::kMaxPlayers - total));
        uint32_t past_four = uint32_t(std::max(total - 4, 0));
        uint32_t bots = std::min<uint32_t>(want, kBotSlots - past_four);
        uint32_t low = want - bots;
        for (int s = 4; s < total; s++) InitDefaultCombatant(s, true);
        if (menu) kernel::G<uint8_t>(kMenuBotCount) = uint8_t(past_four + bots);
        else W32(kBotCount, past_four + bots);
        LOGR("nightfire: a match of %d players and %u bots%s", total, bots + low,
             asked < 0 && want < menu_bots ? " (the bot menu's other ones have no slot left)" : "");
        __imp__sub_000A25F0(ctx, base);
        // in the match the count is read as whether there are bots at all: the menu's own again, unless that says
        // otherwise than the match has
        if (menu && (menu_bots != 0) == (want != 0)) kernel::G<uint8_t>(kMenuBotCount) = uint8_t(menu_bots);
        else if (menu) kernel::G<uint8_t>(kMenuBotCount) = uint8_t(std::min(want, kBotSlots));
        if (low) SpawnLowBots(ctx, base, total, int(low));
        return;
    }
    __imp__sub_000A25F0(ctx, base);
}

// The match's pages list its players as slots 0..[0x260044] and its bots as 4..4 + [0x260048]: the Debriefing
// (0x8B890, sorted by score, the first four shown; its list has [0x26002C] places, those past the slots stale stack)
// and 0x80590. The bots below slot 4 are counted as players for them: listed by their slot's name and character
// (0x25FE38), with places for all (Player 1 first with no points, a "Bond" of zeros, John 2026-11-18). They are the
// slots right after the players (SpawnLowBots); [0x260018] is 0 again on the Debriefing
struct LowBotsListed {
    uint32_t players = R32(kPlayerCount);
    LowBotsListed() {
        if (net::Active() && g_low_bots > 0 && players < 4)
            W32(kPlayerCount, std::min<uint32_t>(players + uint32_t(g_low_bots), 4));
    }
    ~LowBotsListed() { W32(kPlayerCount, players); }
};

X86_EXTERN_FUNC(__imp__sub_0008B890);
X86_FUNC(sub_0008B890) {
    LowBotsListed listed;
    __imp__sub_0008B890(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_00080590);
X86_FUNC(sub_00080590) {
    LowBotsListed listed;
    __imp__sub_00080590(ctx, base);
}

// spawns a human player (cdecl: slot, &pos, &rot, &vel)
X86_FUNC(sub_000ACAB0) {
    int slot = int(kernel::G<int16_t>(ctx.esp + 4));
    bool coop = IsCoopCampaign();
    if (slot == 0 && coop) {
        g_coop_spawn_pos = R32(ctx.esp + 8);
        g_coop_spawn_rot = R32(ctx.esp + 0xC);
        g_coop_spawn_start = R32(ctx.esp + 0x10);
    }
    if (net::FullScreenView()) ForceAllPlayerRects();
    bool prev_spawn = g_in_spawn;
    g_in_spawn = true;
    __imp__sub_000ACAB0(ctx, base);
    g_in_spawn = prev_spawn;
    uint32_t outer = ctx.eax;
    if (!ValidPtr(outer, 0xE0)) return;
    uint32_t inner = R32(outer + 0xBC);
    int true_slot = (slot == 3 && g_active_slot >= 4) ? g_active_slot : slot;
    if (true_slot >= 0 && true_slot < net::kMaxPlayers) {
        g_slot[true_slot].outer = outer;
        g_slot[true_slot].inner = inner;
        if (ValidPtr(inner, 0x8E0))
            kernel::G<uint8_t>(inner + 0x8DF) = uint8_t(true_slot);
    }
    if (true_slot >= 0 && true_slot < 4 && coop) {
        W32(kCombatantState + uint32_t(true_slot) * kCombatantSize + 0x1C, outer);
    }
    if (coop && kernel::G<uint8_t>(0x190939) == 1 && ValidPtr(inner, 0x900)) {  // the spawn set the mission's hands
        g_mission_hands = R32(0x18D188);
        g_mission_skin = kernel::G<uint8_t>(inner + 0x8F5);
    }
    if (net::Active() && true_slot >= 0 && true_slot < net::kMaxPlayers) ApplyPlayerHands(ctx, base, outer, true_slot);
}

// spawns the player in a singleplayer campaign level: in online co-op, also spawns Players 2..4 beside Player 1
X86_FUNC(sub_000AE870) {
    g_coop_spawn_pos = 0;
    g_coop_spawn_rot = 0;
    g_coop_spawn_start = 0;
    __imp__sub_000AE870(ctx, base);
    if (!IsCoopCampaign() || !ValidPtr(g_coop_spawn_pos, 12) || !ValidPtr(g_coop_spawn_rot, 12)) return;
    int total = CoopPlayers();
    uint32_t p0 = R32(kOuterActors);
    if (!ValidPtr(p0, 0xE0)) return;
    if (!g_coop_default_model) {
        uint32_t b8 = R32(p0 + 0xB8);
        uint32_t ebp = ValidPtr(b8, 0x80) ? R32(b8 + 0x74) : 0;
        if (ValidPtr(ebp, 4) && R32(ebp) != 0 && R32(ebp) != 0x50000B0u) g_coop_default_model = R32(ebp);
    }
    W32(kPlayerCount, uint32_t(total));
    W32(kCombatantState + 0x1C, p0);

    float bx = GetF(g_coop_spawn_pos + 0), by = GetF(g_coop_spawn_pos + 4), bz = GetF(g_coop_spawn_pos + 8);
    float rx = GetF(g_coop_spawn_rot + 0), ry = GetF(g_coop_spawn_rot + 4), rz = GetF(g_coop_spawn_rot + 8);
    static constexpr float kOffsets[4][2] = {{0.0f, 0.0f}, {0.9f, -0.6f}, {-0.9f, -0.6f}, {0.0f, -1.2f}};

    uint32_t scratch = (ctx.esp - 0x1000) & ~15u;
    uint32_t pos_va = scratch, rot_va = scratch + 0x20, vel_va = scratch + 0x40;
    std::memset(&kernel::G<uint8_t>(scratch), 0, 0x100);
    SetF(rot_va + 0, rx), SetF(rot_va + 4, ry), SetF(rot_va + 8, rz);
    uint32_t start_va = vel_va;
    if (ValidPtr(g_coop_spawn_start, 0x40) && R32(g_coop_spawn_start + kStartMode) == kFloatStart) {
        start_va = scratch + 0x80;
        std::memcpy(&kernel::G<uint8_t>(start_va), &kernel::G<uint8_t>(g_coop_spawn_start), 0x40);
        LOGR("nightfire: co-op players start floating, as player 1 does");
    }

    for (int s = 1; s < total; s++) {
        InitDefaultCombatant(s, true);
        std::memcpy(&kernel::G<uint8_t>(kControls + uint32_t(s) * kControlSize), g_slot[s].ctrl, kControlSize);
        W32(kOuterActors + uint32_t(s) * 4, 0);
        SetF(pos_va + 0, bx + kOffsets[s][0]);
        SetF(pos_va + 4, by);
        SetF(pos_va + 8, bz + kOffsets[s][1]);
        uint32_t actor = CallGuest(ctx, base, sub_000ACAB0, {uint32_t(s), pos_va, rot_va, start_va});
        W32(kCombatantState + uint32_t(s) * kCombatantSize + 0x1C, actor);
        uint32_t cam = R32(kCameras + uint32_t(s) * 4);
        if (ValidPtr(cam, 0x208)) {
            kernel::G<uint8_t>(cam + 0x203) = 0;
            kernel::G<uint8_t>(cam + 0x205) = 0;
        }
        LOGR("nightfire: co-op campaign spawned player %d (actor %08X)", s + 1, actor);
    }
    for (int s = 0; s < total; s++) {
        uint32_t actor = R32(kOuterActors + uint32_t(s) * 4);
        if (!ValidPtr(actor, 0xE0)) continue;
        uint32_t target_model = ResolveCoopModel(ctx, base, s, g_coop_default_model ? g_coop_default_model : 0x5000007u);
        g_applied_char[s] = net::CharacterFor(s);
        if (!target_model) continue;
        ApplyActorCharacterModel(ctx, base, actor, s, target_model);
        LOGR("nightfire: co-op player %d model set to %08X (choice %d)", s + 1, target_model, g_applied_char[s] + 1);
    }
    LogCharacterTable(ctx, base, "after spawn");
    LogCoopCameras("spawned");
}

// teleports/positions a human player at spawn or after a cutscene (cdecl: outer_actor, &pos, &rot, anim_flag):
// in co-op campaign, when mission scripts teleport Player 1 after an intro cutscene, bring Players 2..4 along
X86_FUNC(sub_000AC7E0) {
    uint32_t actor = R32(ctx.esp + 4);
    uint32_t pos = R32(ctx.esp + 8);
    uint32_t rot = R32(ctx.esp + 0xC);
    uint32_t flag = R32(ctx.esp + 0x10);
    __imp__sub_000AC7E0(ctx, base);
    if (g_coop_respawning || !IsCoopCampaign() || actor != R32(kOuterActors) || !ValidPtr(pos, 12) || !ValidPtr(rot, 12))
        return;
    int total = CoopPlayers();
    float bx = GetF(pos + 0), by = GetF(pos + 4), bz = GetF(pos + 8);
    static constexpr float kOffsets[4][2] = {{0.0f, 0.0f}, {0.9f, -0.6f}, {-0.9f, -0.6f}, {0.0f, -1.2f}};
    uint32_t pos_va = (ctx.esp - 0x800) & ~15u;
    g_coop_respawning = true;
    for (int s = 1; s < total; s++) {
        uint32_t a = R32(kOuterActors + uint32_t(s) * 4);
        if (!ValidPtr(a, 0xE0) || kernel::G<uint16_t>(a + 0xD0) != 1) continue;
        SetF(pos_va + 0, bx + kOffsets[s][0]);
        SetF(pos_va + 4, by);
        SetF(pos_va + 8, bz + kOffsets[s][1]);
        CallGuest(ctx, base, __imp__sub_000AC7E0, {a, pos_va, rot, flag});
    }
    g_coop_respawning = false;
}

// sets a player's movement state (cdecl: outer_actor, state): player 1 starting or ending a float takes the other
// co-op players on foot or floating with them
X86_FUNC(sub_000A9720) {
    static bool mirroring = false;
    uint32_t actor = R32(ctx.esp + 4);
    int16_t to = int16_t(R32(ctx.esp + 8));
    int16_t from = ValidPtr(actor, 0xE0) ? kernel::G<int16_t>(actor + kMoveState) : -1;
    __imp__sub_000A9720(ctx, base);
    bool floats = to == kFloating, floated = from == kFloating;
    if (mirroring || !IsCoopCampaign() || actor != R32(kOuterActors) || floats == floated ||
        (to != 0 && !floats) || (from != 0 && !floated) || kernel::G<uint16_t>(actor + 0xD0) != 1)
        return;
    mirroring = true;
    for (int s = 1; s < CoopPlayers(); s++) {
        uint32_t a = R32(kOuterActors + uint32_t(s) * 4);
        if (!ValidPtr(a, 0xE0) || kernel::G<uint16_t>(a + 0xD0) != 1 || kernel::G<int16_t>(a + kMoveState) != from) continue;
        CallGuest(ctx, base, __imp__sub_000A9720, {a, uint32_t(uint16_t(to))});
    }
    mirroring = false;
    static int said = 0;
    if (said++ < 8) LOGR("nightfire: player 1 %s: the co-op players with them", floats ? "floats" : "is on foot again");
}

// a bot played as the session says (sub_0001B170, slot >= net::Players()): the bot menu's entry for it, counted from
// the first slot past the players (the game counts from slot 4, which players past 4 take; Bots 7 and 8, below 4,
// have the first entries again, the game reading slot - 4's), as the host's bot settings make it (BotsEntry). The
// spawn reads the entry within the call (into the bot's state, 0x1D98E0 + (slot - 4) * 0x768: a bot below slot 4 has
// none), so it is given a copy in the game's own default-entry scratch (0x1DC550, what the spawn builds when it is
// given none)
static void BotEntry(X86Context& ctx, int slot) {
    constexpr uint32_t kScratch = 0x1DC550, kEntrySize = 0x12;
    int first = std::max(net::Players(), 4);
    int bot = slot >= first ? slot - first : int(kBotSlots) + slot - net::Players();
    if (bot < 0 || bot >= int(kBotSlots) + 2) return;
    uint32_t cfg = R32(ctx.esp + 0x14);
    uint32_t cfg_slot = kCombatantCfg + uint32_t(slot) * kCombatantSize;
    if (cfg) {
        std::memmove(&kernel::G<uint8_t>(kScratch), &kernel::G<uint8_t>(kMenuBots + uint32_t(bot % int(kBotSlots)) * kEntrySize),
                     kEntrySize);
        nf::BotsEntry(bot, kScratch);
        W32(cfg_slot + 0x20, kernel::G<uint8_t>(kScratch + 0x0F));  // its team, which the caller took from slot - 4's
    } else {  // the spawn's own default entry: 0x163636's 16 bytes, the slot's team, character 1
        std::memmove(&kernel::G<uint8_t>(kScratch), &kernel::G<uint8_t>(0x163636), 16);
        kernel::G<uint8_t>(kScratch + 0x10) = 1;
        nf::BotsEntry(bot, kScratch);
        kernel::G<uint8_t>(kScratch + 0x0F) = kernel::G<uint8_t>(cfg_slot + 0x20);
    }
    W32(ctx.esp + 0x14, kScratch);
    LOGI("nightfire: the bot in slot %d is Bot %d, character %u (team %u, accuracy %u, speed %u)", slot, bot + 1,
         kernel::G<uint8_t>(kScratch + 0x10), kernel::G<uint8_t>(kScratch + 0x0F), kernel::G<uint8_t>(kScratch),
         kernel::G<uint8_t>(kScratch + 6));
}

// spawns a bot in slot 4..9 (cdecl: slot, &pos, &rot, existing_actor, bot_cfg, flag):
// when slot < net::Players(), spawns a real first-person human player via sub_000ACAB0 instead!
X86_FUNC(sub_0001B170) {
    int slot = int(kernel::G<int16_t>(ctx.esp + 4));
    if (!net::Active() || slot < 4 || slot >= net::Players() || slot >= net::kMaxPlayers) {
        if (net::Active() && R32(kIsMultiplayer) && slot >= net::Players() && slot < net::kMaxPlayers) BotEntry(ctx, slot);
        __imp__sub_0001B170(ctx, base);
        return;
    }
    uint32_t pos = R32(ctx.esp + 8);
    uint32_t rot = R32(ctx.esp + 0xC);
    InitDefaultCombatant(slot, true);
    int chosen = net::CharacterFor(slot);
    if (chosen >= 0 && chosen < 29) W32(kCombatantCfg + uint32_t(slot) * kCombatantSize + 0x24, uint32_t(chosen));
    ScopedPlayerSlot scope(slot);
    W32(kOuterActors + 3 * 4, 0);
    uint32_t zero_buf = (ctx.esp - 0x1000) & ~15u;
    std::memset(&kernel::G<uint8_t>(zero_buf), 0, 0x80);
    uint32_t actor = CallGuest(ctx, base, sub_000ACAB0, {3u, pos, rot, zero_buf});
    ctx.esp += 4;  // cdecl return
    ctx.eax = actor;
    LOGR("nightfire: spawned human player %d in combatant slot %d (actor %08X)", slot + 1, slot, actor);
}

// round-reset for slot 4..9 (cdecl: actor, slot, flag): if slot holds a human player, respawn via 0x9F0B0
X86_FUNC(sub_0001B4F0) {
    uint32_t actor = R32(ctx.esp + 4);
    int slot = int(kernel::G<int16_t>(ctx.esp + 8));
    if (net::Active() && slot >= 4 && slot < net::Players() && TruePlayerFromOuter(actor) == slot) {
        ScopedPlayerSlot scope(slot);
        uint32_t res = CallGuest(ctx, base, __imp__sub_0009F0B0, {actor, 3u});
        ctx.esp += 4;
        ctx.eax = res;
        return;
    }
    __imp__sub_0001B4F0(ctx, base);
}

// a match's restart: its players and the bots in slots 4..9 respawned; the bots below 4 the same
X86_FUNC(sub_000A2990) {
    std::array<std::pair<int, uint32_t>, 4> low{};
    int n = 0;
    for (int s; n < 4 && (s = LowBotSlot(n)) >= 0; n++) low[n] = {s, SlotActor(s)};
    __imp__sub_000A2990(ctx, base);
    for (int i = 0; i < n; i++) CallGuest(ctx, base, sub_0001B4F0, {low[i].second, uint32_t(low[i].first), 0u});
}

// respawns a player or bot (cdecl: actor, slot)
X86_FUNC(sub_0009F0B0) {
    uint32_t actor = R32(ctx.esp + 4);
    int s = TruePlayerFromOuter(actor);
    if (IsCoopCampaign() && s >= 0 && s < 4) {
        int total = CoopPlayers();
        uint32_t target = 0;
        for (int i = 0; i < total; i++) {
            if (i == s || CoopGone(i)) continue;
            uint32_t a = R32(kOuterActors + uint32_t(i) * 4);
            if (AlivePlayer(a)) {
                target = a;
                break;
            }
        }
        if (target && ValidPtr(actor, 0xE0)) {
            uint32_t inner = R32(actor + 0xBC);
            uint32_t scratch = (ctx.esp - 0x800) & ~15u;
            uint32_t pos_va = scratch, rot_va = scratch + 0x20;
            // never on the teammate: two players in one place hold each other there
            static constexpr float kOffsets[4][2] = {{0.0f, 1.2f}, {0.9f, -0.6f}, {-0.9f, -0.6f}, {0.0f, -1.2f}};
            SetF(pos_va + 0, GetF(target + 0x24) + kOffsets[s][0]);
            SetF(pos_va + 4, GetF(target + 0x28));
            SetF(pos_va + 8, GetF(target + 0x2C) + kOffsets[s][1]);
            SetF(rot_va + 0, GetF(target + 0x3C));
            SetF(rot_va + 4, GetF(target + 0x40));
            SetF(rot_va + 8, GetF(target + 0x44));
            if (ValidPtr(inner, 0x8E0)) {
                SetF(inner + 0x824, 100.0f);
                SetF(inner + 0x848, 100.0f);
                SetF(inner + 0x840, 0.0f);
                SetF(inner + 0x844, 0.0f);
                kernel::G<uint8_t>(inner + 0x110) = 0;
                kernel::G<uint16_t>(inner + 0x10C) = 0;
                SetF(inner + 0x838, 0.0f);
            }
            kernel::G<uint8_t>(actor + 0xDB) = 0x03;
            W32(actor + 0xCC, R32(actor + 0xCC) & ~0x20u);
            g_coop_respawning = true;
            CallGuest(ctx, base, __imp__sub_000AC7E0, {actor, pos_va, rot_va, MoveMode(target)});
            g_coop_respawning = false;
            RestoreHud(inner, s);
            DrawWeapon(actor, s);
            ctx.eax = 1;
            ctx.esp += 4;
            LOGR("nightfire: co-op player %d respawned beside teammate", s + 1);
            return;
        }
        // all co-op players are down -> let sub_0009F0B0 return 0 so sub_000AC690 triggers Mission Failed
        ctx.eax = 0;
        ctx.esp += 4;
        return;
    }
    if (net::Active() && s >= 4 && s < net::kMaxPlayers) {
        ScopedPlayerSlot scope(s);
        uint32_t esp = ctx.esp;
        uint16_t was = kernel::R16(esp + 8);
        kernel::W16(esp + 8, 3);
        __imp__sub_0009F0B0(ctx, base);
        kernel::W16(esp + 8, was);
        return;
    }
    __imp__sub_0009F0B0(ctx, base);
}

// decodes controller/keyboard/mouse inputs each tick into 0x1FE6D0[0..3], plus g_slot[4..9].ctrl
X86_FUNC(sub_0006CF50) {
    static bool names_loaded = false;
    if (!names_loaded && R32(0x1F658C) != 0) {
        uint32_t saved_eax = ctx.eax, saved_ecx = ctx.ecx, saved_edx = ctx.edx;
        int found = 0;
        for (uint32_t i = 0; i < 29; i++) {
            uint32_t va = CallGuest(ctx, base, __imp__sub_0007EB90, {i});
            if (ValidPtr(va, 2) && kernel::G<char>(va) != 0) {
                net::SetCharacterName(i, &kernel::G<char>(va));
                found++;
            }
        }
        ctx.eax = saved_eax, ctx.ecx = saved_ecx, ctx.edx = saved_edx;
        if (found == 29) {
            names_loaded = true;
            for (uint32_t i = 0; i < 29; i++)
                LOGR("nightfire: diag name %s, table model %08X", net::CharacterChoiceName(i + 1),
                     R32(0x1637C0u + i * 0x10u));
        }
    }
    // a campaign's row 0 reads the controller that pressed START (0x7CD50, 0x86690): with the joiner's, two rows
    // would read one pad, so in co-op each player's row reads their own controller (rumble follows the port too)
    if (IsCoopCampaign()) {
        for (int i = 0; i < CoopPlayers(); i++) {
            uint8_t& port = kernel::G<uint8_t>(kControls + uint32_t(i) * kControlSize + 0x156);
            if (port != i) {
                LOGR("nightfire: co-op player %d read controller %u; now controller %d", i + 1, port + 1u, i + 1);
                port = uint8_t(i);
            }
        }
    }
    __imp__sub_0006CF50(ctx, base);
    if (!net::Active()) return;
    int total = std::clamp(net::Players(), 1, net::kMaxPlayers);
    for (int i = 0; i < std::min(total, 4); i++) {
        std::memcpy(g_slot[i].ctrl, &kernel::G<uint8_t>(kControls + uint32_t(i) * kControlSize), kControlSize);
        g_slot[i].ctrl_init = true;
    }
    if (total <= 4) return;
    for (int s = 4; s < total; s++) {
        if (!g_slot[s].ctrl_init) InitDefaultCombatant(s);
        std::memcpy(&kernel::G<uint8_t>(kControls + 3 * kControlSize), g_slot[s].ctrl, kControlSize);
        kernel::G<uint8_t>(kControls + 3 * kControlSize + 0x156) = 3;
        net::SetExtraInputPass(true, s);
        CallGuest(ctx, base, __imp__sub_0006CF50, {});
        net::SetExtraInputPass(false);
        std::memcpy(g_slot[s].ctrl, &kernel::G<uint8_t>(kControls + 3 * kControlSize), kControlSize);
    }
    for (int i = 0; i < 4; i++)
        std::memcpy(&kernel::G<uint8_t>(kControls + uint32_t(i) * kControlSize), g_slot[i].ctrl, kControlSize);
}

bool g_was_gone[4] = {};  // the player was dropped out (CoopGone) at their last update

// a dropped-out co-op player (CoopGone) each frame: hidden from every camera, full health, at their companion's place
void HideGone(X86Context& ctx, uint8_t* base, uint32_t outer) {
    uint32_t inner = R32(outer + 0xBC);
    uint32_t vm = ValidPtr(inner, 0x900) ? R32(inner + 0x778) : 0;
    for (uint32_t obj : {outer, vm})
        if (ValidPtr(obj, 0xE0)) CallGuest(ctx, base, __imp__sub_000DB320, {obj, 0u});
}

void CarryGone(X86Context& ctx, uint8_t* base, uint32_t outer, int s) {
    if (!g_was_gone[s]) {
        LOGR("nightfire: co-op player %d left the game: they drop out (hidden, carried with the others)", s + 1);
    }
    HideGone(ctx, base, outer);
    W32(outer + 0xCC, R32(outer + 0xCC) | 0x20u);  // as a dead player: nothing stands on them or bumps into them
    uint32_t inner = R32(outer + 0xBC);
    if (ValidPtr(inner, 0x8E0) && kernel::G<int16_t>(outer + 0xD0) == 1) SetF(inner + 0x824, 100.0f);
    uint32_t mate = CoopCompanion(s);
    if (!mate || kernel::G<int16_t>(outer + 0xD0) != 1) return;
    uint32_t scratch = (ctx.esp - 0x800) & ~15u, pos = scratch, rot = scratch + 0x20;
    for (uint32_t k = 0; k < 12; k += 4) W32(pos + k, R32(mate + 0x24 + k)), W32(rot + k, R32(mate + 0x3C + k));
    g_coop_respawning = true;
    CallGuest(ctx, base, __imp__sub_000AC7E0, {outer, pos, rot, MoveMode(mate)});
    g_coop_respawning = false;
}

// per-frame human player update (cdecl: outer_actor)
// a session player whose health runs out under their own pause stays alive on a sliver of it (0xAC390's hook)
// until it closes: a dying player's update is not run while paused (pause.cpp), and without it their model
// leaves the level's rooms ([outer+0xB8]+0x134) the first time another player's camera looks away
constexpr float kHeldHealth = 0.01f;
bool g_death_held[4] = {};
uint16_t g_down_ticks[4] = {};  // a downed co-op player's frames to their respawn

// a dropped-out co-op player whose machine rejoined (net.cpp): their cameras give them back their body and their
// model each frame (0xA8A60), the flag CarryGone set goes
void ComeBack(uint32_t outer, int s) {
    LOGR("nightfire: co-op player %d is back in the game", s + 1);
    if (kernel::G<int16_t>(outer + 0xD0) == 1) W32(outer + 0xCC, R32(outer + 0xCC) & ~0x20u);
}

X86_FUNC(sub_000AD520) {
    uint32_t outer = R32(ctx.esp + 4);
    int s = TruePlayerFromOuter(outer);
    ScopedPlayerSlot scope(s);
    if (s >= 0 && s < 4 && g_death_held[s] && !nf::OwnPause(s)) {
        g_death_held[s] = false;
        if (ValidPtr(outer, 0xE0) && ValidPtr(R32(outer + 0xBC), 0x8E0)) SetF(R32(outer + 0xBC) + 0x824, 0.0f);
    }
    if (net::Active() && s >= 0 && s < net::kMaxPlayers && ValidPtr(outer, 0xE0)) {
        if (IsCoopCampaign() && !g_coop_default_model) {
            uint32_t p0 = R32(kOuterActors);
            uint32_t p0_b8 = ValidPtr(p0, 0xE0) ? R32(p0 + 0xB8) : 0;
            uint32_t p0_ebp = ValidPtr(p0_b8, 0x80) ? R32(p0_b8 + 0x74) : 0;
            if (ValidPtr(p0_ebp, 4) && R32(p0_ebp) != 0 && R32(p0_ebp) != 0x50000B0u) g_coop_default_model = R32(p0_ebp);
        }
        int ch = net::CharacterFor(s);
        if (ch != g_applied_char[s]) {
            g_applied_char[s] = ch;
            // a level has the characters it registered as it loaded (a match all 29): another waits for the next level
            uint32_t chosen = ch >= 0 && ch < 29 ? R32(0x1637C0u + uint32_t(ch) * 0x10u) : 0;
            if (chosen && !CallGuest(ctx, base, __imp__sub_0006B500, {chosen})) chosen = 0;
            uint32_t target_model = 0;
            if (IsCoopCampaign()) {
                target_model = ch <= 0 ? (g_coop_default_model ? g_coop_default_model : 0x5000007u) : chosen;
            } else if (ch >= 0 && ch < 29) {
                W32(kCombatantCfg + uint32_t(s) * kCombatantSize + 0x24, uint32_t(ch));
                target_model = chosen;
            }
            if (target_model) {
                ApplyActorCharacterModel(ctx, base, outer, s, target_model);
                LOGR("nightfire: player %d changed character in-game -> choice %d (model %08X)", s + 1, ch + 1,
                     target_model);
            } else if (ch > 0 && ch < 29) {
                LOGR("nightfire: player %d's choice %d (%s) is not in this level; it waits for the next one", s + 1,
                     ch + 1, net::CharacterChoiceName(uint32_t(ch) + 1));
            }
        }
        if (IsCoopCampaign()) ApplyPlayerHands(ctx, base, outer, s);
    }
    // a co-op player down (dying, [outer+0xD0] 2, or 3) while a teammate is alive is respawned beside them 180
    // frames on, counted while their own pause is closed (a death under it waits for it)
    bool coop_down = false;
    if (IsCoopCampaign() && s >= 0 && s < 4) {
        int16_t state = ValidPtr(outer, 0xE0) ? kernel::G<int16_t>(outer + 0xD0) : 0;
        coop_down = (state == 2 || state == 3) && TeammateAlive(s);
        if (state == 1 && ValidPtr(R32(outer + 0xBC), 0x8E0)) {
            KeepHud(R32(outer + 0xBC), s);
            KeepWeapon(outer, s);
        }
        if (!coop_down) g_down_ticks[s] = 0;
        else if (!nf::OwnPause(s) && (++g_down_ticks[s] >= 180 || CoopGone(s))) {
            g_down_ticks[s] = 0;
            CallGuest(ctx, base, sub_0009F0B0, {outer, uint32_t(s)});
        }
    }
    HandsScope hands(ctx, base, outer);
    nf::PausedUpdate paused(outer);  // src/game/pause.cpp
    __imp__sub_000AD520(ctx, base);
    // the end of a death (0xAC690: its animation done, or button 0x19 on any controller, a teammate's CONTINUE too)
    // asks for the mission failed screen ([0x1DF19A]); in co-op only the last player down does
    if (coop_down) kernel::G<uint8_t>(kMissionFailed) = 0;
    if (s < 0 || s >= 4) return;
    bool gone = CoopGone(s);
    if (gone) CarryGone(ctx, base, outer, s);
    else if (g_was_gone[s] && ValidPtr(outer, 0xE0)) ComeBack(outer, s);
    g_was_gone[s] = gone;
}

// a player's death (cdecl: outer, killer): out of a match it sets the music's death cue (0xCBCE0(6, 1), [0x29A198]),
// which fades every sound out for good (0xCA805, 0xC4F90) as a level that ends there needs; a co-op player
// with a teammate alive is respawned and the level goes on, so it is cleared
X86_FUNC(sub_000AC390) {
    uint32_t outer = R32(ctx.esp + 4);
    int s = TruePlayerFromOuter(outer);
    uint32_t inner = ValidPtr(outer, 0xE0) ? R32(outer + 0xBC) : 0;
    if (s >= 0 && s < 4 && nf::OwnPause(s) && ValidPtr(inner, 0x8E0) && kernel::G<int16_t>(outer + 0xD0) == 1 &&
        GetF(inner + 0x824) <= 0.0f) {
        g_death_held[s] = true;
        SetF(inner + 0x824, kHeldHealth);
        ctx.eax &= ~0xFFu;
        ctx.esp += 4;
        return;
    }
    __imp__sub_000AC390(ctx, base);
    if (IsCoopCampaign() && s >= 0 && s < 4 && TeammateAlive(s)) W32(kMusicCues + 6 * 4, 0);
}

// a co-op level's state every 60 ticks against the other machines' (net::CheckState, "net: the games differ"): the
// level, the random numbers (0xD5310's 0x18CDF8, 0x18CDFC), the objects made (0x1DF828), the characters set up
// (0x1E5860) and the players' places
static void CoopCheck() {
    uint32_t words[5 + 3 * 4] = {R32(0x1F658C), R32(0x18CDF8), R32(0x18CDFC), R32(0x1DF828), R32(0x1E5860)};
    size_t n = 5;
    for (int s = 0; s < CoopPlayers() && s < 4; s++) {
        uint32_t a = R32(kOuterActors + uint32_t(s) * 4);
        for (int i = 0; i < 3; i++) words[n++] = ValidPtr(a, 0x30) ? R32(a + 0x24 + 4 * uint32_t(i)) : 0;
    }
    net::CheckState(words, n);
}

// per-frame human player camera update (cdecl: outer_actor)
X86_FUNC(sub_000A8A60) {
    uint32_t outer = R32(ctx.esp + 4);
    int s = TruePlayerFromOuter(outer);
    if (s == 0 && IsCoopCampaign()) CoopCheck();
    ScopedPlayerSlot scope(s);
    HandsScope hands(ctx, base, outer);
    __imp__sub_000A8A60(ctx, base);
    if (CoopGone(s)) HideGone(ctx, base, outer);
}

// a dropped-out co-op player takes a hit with health no hit can take (0xADD00 answers on the x87 stack, so it runs)
struct Unhurt {
    uint32_t inner = 0;
    Unhurt(int s, uint32_t in) {
        if (CoopGone(s) && ValidPtr(in, 0x8E0)) inner = in, SetF(inner + 0x824, 1e9f);
    }
    ~Unhurt() {
        if (inner) SetF(inner + 0x824, 100.0f);
    }
};

// projectile/explosion hit on a human player (cdecl: outer_actor, hit_info)
X86_FUNC(sub_000AE1C0) {
    int s = TruePlayerFromOuter(R32(ctx.esp + 4));
    ScopedPlayerSlot scope(s);
    uint32_t outer = R32(ctx.esp + 4);
    Unhurt unhurt(s, ValidPtr(outer, 0xE0) ? R32(outer + 0xBC) : 0);
    __imp__sub_000AE1C0(ctx, base);
}

// the player whose shot or blow a hit is: its attacker ([hit+0x4C]) a type 5 actor whose +0xBC's +0x24 is the
// shooter's actor (a bullet from player 1 at player 2 in co-op), else -1
int HitByPlayer(uint32_t hit) {
    uint32_t att = ValidPtr(hit, 0x50) ? R32(hit + 0x4C) : 0;
    if (!ValidPtr(att, 0xE0) || kernel::G<uint8_t>(att + 0xDB) != 0x05) return -1;
    uint32_t in = R32(att + 0xBC);
    uint32_t owner = ValidPtr(in, 0x28) ? R32(in + 0x24) : 0;
    if (!ValidPtr(owner, 0xE0) || kernel::G<uint8_t>(owner + 0xDB) != 0x03) return -1;
    return TruePlayerFromOuter(owner);
}

// bullet/melee damage on a human player (cdecl: outer_actor, inner_state, hit_info): with co-op friendly fire off,
// a teammate's hit is not taken at all (no damage, no flinch); the handler answers 0x15D2E8's float, as its
// ignored hits do (0xAE16B)
X86_FUNC(sub_000ADD00) {
    int s = TruePlayerFromInner(R32(ctx.esp + 8));
    if (s >= 0 && IsCoopCampaign() && !net::CoopFriendlyFire()) {
        int by = HitByPlayer(R32(ctx.esp + 12));
        if (by >= 0 && by != s) {
            x87_push(ctx, double(GetF(0x15D2E8)));
            ctx.esp += 4;
            return;
        }
    }
    ScopedPlayerSlot scope(s);
    Unhurt unhurt(s, R32(ctx.esp + 8));
    __imp__sub_000ADD00(ctx, base);
}

// campaign enemy target acquisition (cdecl: npc_inner): in co-op campaign, pick the closest alive player (0..3)
X86_FUNC(sub_00037C10) {
    if (!IsCoopCampaign()) {
        __imp__sub_00037C10(ctx, base);
        return;
    }
    uint32_t npc = R32(ctx.esp + 4);
    uint32_t npc_outer = ValidPtr(npc, 0x10) ? R32(npc + 0x0C) : 0;
    uint32_t p0_outer = R32(kOuterActors);
    uint32_t best = CoopGone(0) ? CoopCompanion(0) : p0_outer;
    if (!best) best = p0_outer;
    if (ValidPtr(npc_outer, 0x30)) {
        float nx = GetF(npc_outer + 0x24), ny = GetF(npc_outer + 0x28), nz = GetF(npc_outer + 0x2C);
        float best_d2 = 1e30f;
        int total = CoopPlayers();
        for (int s = 0; s < total; s++) {
            uint32_t a = R32(kOuterActors + uint32_t(s) * 4);
            if (CoopGone(s) || !ValidPtr(a, 0xE0) || kernel::G<int16_t>(a + 0xD0) != 1) continue;
            uint32_t in = R32(a + 0xBC);
            if (!ValidPtr(in, 0x8E0) || GetF(in + 0x824) <= 0.0f) continue;
            float dx = GetF(a + 0x24) - nx, dy = GetF(a + 0x28) - ny, dz = GetF(a + 0x2C) - nz;
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best_d2) best_d2 = d2, best = a;
        }
    }
    if (best && best != p0_outer) W32(kOuterActors, best);
    __imp__sub_00037C10(ctx, base);
    if (best && best != p0_outer) W32(kOuterActors, p0_outer);
}

// NPC vision cone / alert detection: in co-op campaign, let NPCs spot all players 0..3 via the 0x262754 loop
X86_FUNC(sub_00043070) {
    if (!IsCoopCampaign()) {
        __imp__sub_00043070(ctx, base);
        return;
    }
    uint32_t was = R32(0x26001C);
    W32(0x26001C, 1);
    __imp__sub_00043070(ctx, base);
    W32(0x26001C, was);
}

// weapon/ammo/armor pickup update (cdecl: pickup_obj)
X86_FUNC(sub_000A84D0) {
    if (!net::Active() || net::Players() <= 4) {
        __imp__sub_000A84D0(ctx, base);
        return;
    }
    uint32_t pickup = R32(ctx.esp + 4);
    int best_slot = 3;
    if (ValidPtr(pickup, 0x70)) {
        float px = GetF(pickup + 0x60), py = GetF(pickup + 0x64), pz = GetF(pickup + 0x68);
        float best_d2 = 1e30f;
        for (int s = 3; s < net::Players() && s < net::kMaxPlayers; s++) {
            uint32_t act = (s == 3) ? R32(kOuterActors + 3 * 4) : g_slot[s].outer;
            if (!ValidPtr(act, 0xE0) || kernel::G<uint16_t>(act + 0xD0) != 1) continue;
            float dx = GetF(act + 0x24) - px, dy = GetF(act + 0x28) - py, dz = GetF(act + 0x2C) - pz;
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best_d2) best_d2 = d2, best_slot = s;
        }
    }
    ScopedPlayerSlot scope(best_slot);
    // hide human actors (type 0x03/0x12) sitting in the bot array 0x262814[0..5] so the bot pickup loop skips them
    uint32_t saved_bots[6] = {};
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t va = kCombatantState + (4 + i) * kCombatantSize + 0x1C;
        uint32_t a = R32(va);
        if (ValidPtr(a, 0xE0) && (kernel::G<uint8_t>(a + 0xDB) == 0x03 || kernel::G<uint8_t>(a + 0xDB) == 0x12)) {
            saved_bots[i] = a;
            W32(va, 0);
        }
    }
    __imp__sub_000A84D0(ctx, base);
    for (uint32_t i = 0; i < 6; i++) {
        if (saved_bots[i]) W32(kCombatantState + (4 + i) * kCombatantSize + 0x1C, saved_bots[i]);
    }
}

// match rules / elimination check
X86_FUNC(sub_0009CC50) {
    uint32_t was = R32(kPlayerCount);
    if (net::Active() && R32(kIsMultiplayer) && uint32_t(net::Players()) > was)
        W32(kPlayerCount, uint32_t(std::min(net::Players(), net::kMaxPlayers)));
    __imp__sub_0009CC50(ctx, base);
    if (R32(kPlayerCount) != was) W32(kPlayerCount, was);
}

// HUD layout per player count: reads [0x260044] (1..4 players) and player index at [esp+0x14]
X86_FUNC(sub_000B1AB0) {
    if (!net::FullScreenView()) return __imp__sub_000B1AB0(ctx, base);
    ForceAllPlayerRects();
    uint32_t esp = ctx.esp;
    uint32_t was_count = R32(kPlayerCount);
    uint16_t was_player = kernel::R16(esp + 0x14);
    if (was_count > 1) W32(kPlayerCount, 1);
    kernel::W16(esp + 0x14, 0);
    __imp__sub_000B1AB0(ctx, base);
    kernel::W16(esp + 0x14, was_player);
    if (was_count > 1) W32(kPlayerCount, was_count);
}

// positions a HUD sprite (cdecl: sprite, player_index): in 2-player mode shifts player 2's icons down by 44 px
X86_FUNC(sub_000B2750) {
    if (!net::FullScreenView()) return __imp__sub_000B2750(ctx, base);
    ForceAllPlayerRects();
    uint32_t esp = ctx.esp;
    uint32_t sp = R32(esp + 4);
    int player = int(kernel::G<int16_t>(esp + 8));
    if (player == 3 && g_active_slot >= 4) player = g_active_slot;
    uint32_t was_count = R32(kPlayerCount);
    uint32_t was_arg = R32(esp + 8);
    if (was_count > 1) W32(kPlayerCount, 1);
    W32(esp + 8, 0);
    __imp__sub_000B2750(ctx, base);
    W32(esp + 8, was_arg);
    if (was_count > 1) W32(kPlayerCount, was_count);
    if (player >= 0 && player < net::kMaxPlayers)
        TagSpriteView(sp, player);
}

// creates and positions a HUD widget group's sprites for a player (cdecl: actor, group, def)
X86_FUNC(sub_000B5C30) {
    if (net::FullScreenView()) ForceAllPlayerRects();
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    __imp__sub_000B5C30(ctx, base);
    if (!net::FullScreenView()) return;
    int player = TruePlayerFromInner(actor);
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 64);
}

X86_FUNC(sub_000B5E80) {
    if (net::FullScreenView()) ForceAllPlayerRects();
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    __imp__sub_000B5E80(ctx, base);
    if (!net::FullScreenView()) return;
    int player = TruePlayerFromInner(actor);
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 64);
}

X86_FUNC(sub_000B5F40) {
    if (net::FullScreenView()) ForceAllPlayerRects();
    uint32_t actor = ctx.eax, group = ctx.edi;
    __imp__sub_000B5F40(ctx, base);
    if (!net::FullScreenView()) return;
    int player = TruePlayerFromInner(actor);
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 64);
}

X86_FUNC(sub_000B66C0) {
    if (net::FullScreenView()) ForceAllPlayerRects();
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    __imp__sub_000B66C0(ctx, base);
    if (!net::FullScreenView()) return;
    int player = TruePlayerFromInner(actor);
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 64);
}

X86_FUNC(sub_000B69E0) {
    if (net::FullScreenView()) ForceAllPlayerRects();
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    __imp__sub_000B69E0(ctx, base);
    if (!net::FullScreenView() || !ValidPtr(group, 0x1C)) return;
    int player = TruePlayerFromInner(actor);
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 33);
    uint32_t def = R32(group + 0x00);
    uint16_t dots = ValidPtr(def, 0x18) ? kernel::G<uint16_t>(def + 0x16) : 32;
    TagSpriteArray(R32(group + 0x04), dots, player, 32);
}

// updates a player's multiplayer HUD (cdecl: actor, group, score): score text, health bar, death blood wipe, 4 red hit wedges
X86_FUNC(sub_000B2870) {
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    __imp__sub_000B2870(ctx, base);
    if (!net::FullScreenView()) return;
    int player = TruePlayerFromInner(actor);
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 64);
}

// updates a player's guided missile view (cdecl: actor, group, the player's object): beside its sprites the group has
// three 3D objects ([group+0x04], the red sight around the missile), shown while the player steers it ([obj+0xD2]
// 10) and turned with the missile ([actor+0x808]). They are the world's objects in camera 5 ([obj+0xD4] 0x20), the 2D
// pass every machine draws: in full-screen view another machine's player's are in no camera, as their HUD's sprites
// are hidden (0xB6190), a draw-only difference (LogLeftObjects passes over them)
X86_EXTERN_FUNC(__imp__sub_000B4760);
namespace {
std::unordered_set<uint32_t> g_sights;  // those objects, LogLeftObjects' to pass over
}
X86_FUNC(sub_000B4760) {
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    __imp__sub_000B4760(ctx, base);
    if (!net::FullScreenView() || !ValidPtr(group, 0x1C) || !ValidPtr(actor, 0x900)) return;
    int player = TruePlayerFromInner(actor);
    uint32_t sight = R32(group + 0x04);
    if (player < 0 || player >= net::kMaxPlayers || !ValidPtr(sight, 12)) return;
    bool remote = player != net::LocalPlayer();
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t obj = R32(sight + i * 4);
        if (!ValidPtr(obj, 0xE0)) continue;
        uint16_t& mask = kernel::G<uint16_t>(obj + 0xD4);
        if (g_sights.insert(obj).second)
            LOG_FIRST(12, "nightfire: player %d's missile sight %u: object %08X cameras %04X", player + 1, i, obj, mask);
        // a rejoin's snapshot of the host's memory brings this machine's own sights with the host's 0
        mask = remote ? 0 : 0x20;
    }
}

// updates a player's full HUD (cdecl: actor, group, score): health/armor segments, directional hit wedges, status icons.
// Its crouch icon (0x3000173 while [actor+0xD2] is 4, then 0x3000174 fading out) and the icon picked by
// [actor+0x8EF] (0x3000064-6A) fade on one timer each for every player (0x2790AC, 0x2790B0): in co-op each player
// keeps their own, or one crouching shows the others their standing icon
X86_FUNC(sub_000B2B50) {
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    int player = TruePlayerFromInner(actor);
    constexpr uint32_t kFades[2] = {0x2790AC, 0x2790B0};
    if (IsCoopCampaign() && player >= 0 && player < net::kMaxPlayers) {
        uint16_t was[2];
        for (int i = 0; i < 2; i++) was[i] = kernel::R16(kFades[i]), kernel::W16(kFades[i], g_slot[player].icon_fade[i]);
        __imp__sub_000B2B50(ctx, base);
        for (int i = 0; i < 2; i++) g_slot[player].icon_fade[i] = kernel::R16(kFades[i]), kernel::W16(kFades[i], was[i]);
    } else {
        __imp__sub_000B2B50(ctx, base);
    }
    if (!net::FullScreenView()) return;
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 64);
}

// Co-op name tags. A match's HUD (0xB6AA0 builds a player's from the 22 groups of 0x1812F0, a level's from
// 0x180EE0's) has the radar group (entry 14, 0x1806D8: 0xB69E0 makes it, 0xB54D0 updates it), whose dots are also
// the name tags: each other combatant (0x262754, 0x30 apart) on screen and in sight gets a dot as their name
// (0x25FE38, 0x30 apart) over their head, with [0x26005C] set. In co-op a level's HUD gets the group too, its ring
// hidden and no radar targets (0x9DA80 from 0xB5561), so the dots are the names alone: net_name, else Player N
constexpr uint32_t kLevelHud = 0x180EE0, kRadarGroup = 0x1806D8;
constexpr uint32_t kRadarEntry = kLevelHud + 14 * 4;
constexpr uint32_t kRadarTargets = 0xB5566;  // 0xB54D0's call of 0x9DA80, returning to it

X86_EXTERN_FUNC(__imp__sub_000B6AA0);
X86_FUNC(sub_000B6AA0) {
    if (!IsCoopCampaign() || R32(kRadarEntry) != 0) {
        __imp__sub_000B6AA0(ctx, base);
        return;
    }
    W32(kRadarEntry, kRadarGroup);
    __imp__sub_000B6AA0(ctx, base);
    W32(kRadarEntry, 0);
}

X86_EXTERN_FUNC(__imp__sub_0009DA80);
X86_FUNC(sub_0009DA80) {
    if (R32(ctx.esp) == kRadarTargets && IsCoopCampaign()) {
        ctx.eax = 0;
        ctx.esp += 4;  // cdecl: the return address only
        return;
    }
    __imp__sub_0009DA80(ctx, base);
}

// updates a player's radar ring ([group+0x08]) and radar dots ([group+0x04]) (cdecl: actor, group, obj)
X86_FUNC(sub_000B54D0) {
    uint32_t actor = R32(ctx.esp + 4), group = R32(ctx.esp + 8);
    bool coop = IsCoopCampaign();
    if (coop || R32(kIsMultiplayer)) SessionNames(coop);
    __imp__sub_000B54D0(ctx, base);
    if (coop && ValidPtr(group, 0x1C)) {
        uint32_t ring = ValidPtr(R32(group + 0x08), 4) ? R32(R32(group + 0x08)) : 0;
        if (ValidPtr(ring, 0x3C)) kernel::G<uint8_t>(ring + 0x38) = 0xFF;
    }
    if (!net::FullScreenView() || !ValidPtr(group, 0x1C)) return;
    int player = TruePlayerFromInner(actor);
    if (player < 0 || player >= net::kMaxPlayers) return;
    TagGroupSprites(group, player, 33);
    uint32_t def = R32(group + 0x00);
    uint16_t dots = ValidPtr(def, 0x18) ? kernel::G<uint16_t>(def + 0x16) : 32;
    TagSpriteArray(R32(group + 0x04), dots, player, 32);
}

// master per-frame HUD update for a player (cdecl: actor, obj): updates all 22 widget groups at [actor+0x770]+0x08
X86_FUNC(sub_000B6190) {
    uint32_t actor = R32(ctx.esp + 4);
    int player = TruePlayerFromInner(actor);
    ScopedPlayerSlot scope(player);
    __imp__sub_000B6190(ctx, base);
    if (!net::FullScreenView() || player < 0 || player >= net::kMaxPlayers) return;
    uint32_t hud = R32(actor + 0x770);
    if (!ValidPtr(hud, 0x2C8)) return;
    // a remote player's sprites are also hidden as 0xB6190 hides a switched off group's ([sprite+0x38] = 0xFF), as
    // their view tag alone leaves any sprite a widget moves to another view (its weapon name, its ammo) drawn
    bool remote = player != net::LocalPlayer();
    auto hide = [&](uint32_t sp) {
        if (remote && ValidPtr(sp, 0x40)) kernel::G<uint8_t>(sp + 0x38) = 0xFF;
    };
    auto each = [&](uint32_t arr, uint16_t count, uint16_t max_count) {
        if (!count || count > max_count) count = max_count;
        if (!ValidPtr(arr, uint32_t(count) * 4)) return;
        for (uint16_t i = 0; i < count; i++) hide(R32(arr + uint32_t(i) * 4));
    };
    TagSpriteView(R32(hud + 0x00), player);
    hide(R32(hud + 0x00));
    // a group: +0x00 its definition, +0x04 more sprites (the radar's dots), +0x08 its sprites, +0x0C its update,
    // +0x18 its sprite count
    for (uint32_t i = 0; i < 0x16; i++) {
        uint32_t group = hud + 0x08 + i * 0x20;
        TagGroupSprites(group, player, 64);
        each(R32(group + 0x08), kernel::G<uint16_t>(group + 0x18), 64);
        if (R32(group + 0x0C) == 0x000B54D0u) {
            uint32_t def = R32(group + 0x00);
            uint16_t dots = ValidPtr(def, 0x18) ? kernel::G<uint16_t>(def + 0x16) : 32;
            TagSpriteArray(R32(group + 0x04), dots, player, 32);
            each(R32(group + 0x04), dots, 32);
        }
    }
}

X86_FUNC(sub_000DD950) {  // the D3D viewport, projection and view matrices for a camera pass
    uint32_t cam = R32(ctx.esp + 4);
    int active = 0;
    int player = PlayerCameraIndex(cam, &active);
    if (net::FullScreenView() && player >= 0 && active > 1) {
        ForceFullRect(cam);
        SetF(cam + kAspect, R32(kWidescreen) ? (16.0f / 9.0f) : (4.0f / 3.0f));
    }
    Widened(cam, [&] { __imp__sub_000DD950(ctx, base); });
    if (net::FullScreenView() && active > 1 && player >= 0) {
        d3d::SuppressDraws(player != net::LocalPlayer());
        static bool said = false;
        if (!said) {
            said = true;
            LOGR("nightfire: online full-screen view active (drawing player %d of %d at 640x480)",
                 net::LocalPlayer() + 1, active);
        }
    } else {
        d3d::SuppressDraws(false);
    }
}

X86_FUNC(sub_000DB5C0) {  // the culling planes
    uint32_t cam = R32(ctx.esp + 4);
    int active = 0;
    int player = PlayerCameraIndex(cam, &active);
    if (net::FullScreenView() && player >= 0 && active > 1) {
        SetF(cam + kAspect, R32(kWidescreen) ? (16.0f / 9.0f) : (4.0f / 3.0f));
    }
    Widened(cam, [&] { __imp__sub_000DB5C0(ctx, base); });
}

// Two updates decide once a frame, from camera 0, whether a glow is drawn over the walls: a lamp's (0x6DCB0: data
// [lamp+0xBC], kind word 1, its glow [data+0x24]; line from camera 0 (+0xB8) through player 1's room [+0x20]) and a
// glow's own (0xA3EB0; line through camera 0's room [+0x04]). A clear line (0x2C5B0: from, to (+0x24), room, two
// objects it ignores, flags 8) sets the glow's +0xCC 0x2000, which 0xDABB0 lists in the camera's mode 2 list, drawn in
// 0xDAC20's last pass over a cleared depth buffer; blocked, it is drawn with the world. Every camera shares that
// flag, so with more player cameras player 1's view put the castle's window glows over the walls of the others
// (07000005). After 0xDABB0 (camera in eax) builds another player's lists, its mode 2 list keeps only the glows that
// player's own line reaches.
X86_FUNC(sub_0006DCB0) {
    uint32_t lamp = R32(ctx.esp + 4);
    __imp__sub_0006DCB0(ctx, base);
    if (!ValidPtr(lamp, 0xE0)) return;
    uint32_t data = R32(lamp + 0xBC);
    if (ValidPtr(data, 0x28) && kernel::G<uint16_t>(data) == 1 && R32(data + 0x24)) g_glows[R32(data + 0x24)] = kLampGlow;
}

X86_FUNC(sub_000A3EB0) {
    uint32_t glow = R32(ctx.esp + 4);
    __imp__sub_000A3EB0(ctx, base);
    if (glow) g_glows[glow] = kOwnGlow;
}

X86_FUNC(sub_000DABB0) {
    uint32_t cam = ctx.eax;
    __imp__sub_000DABB0(ctx, base);
    int active = 0;
    PlayerCameraIndex(0, &active);
    if (active < 2 || g_glows.empty() || !ValidPtr(cam, 0xC4)) return;
    uint32_t hw = 4;
    for (uint32_t k = 1; k < 4; k++)
        if (R32(kCameras + 4 * k) == cam) hw = k;
    uint32_t actor = hw < 4 ? R32(kOuterActors + 4 * hw) : 0;
    if (!ValidPtr(actor, 0xE0)) return;
    uint32_t list = R32(cam + 0x10), n = kernel::G<uint16_t>(cam + 0x18), kept = 0;
    if (!ValidPtr(list, n * 12)) return;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t e = list + i * 12, obj = R32(e + 4);
        auto g = g_glows.find(obj);
        if (g != g_glows.end() && ValidPtr(obj, 0xE0)) {
            uint32_t room = g->second == kLampGlow ? R32(actor + 0x20) : R32(cam + 0x04);
            if (!(CallGuest(ctx, base, __imp__sub_0002C5B0, {cam + 0xB8, obj + 0x24, room, obj, actor, 8}) & 0xFF))
                continue;
        }
        if (kept != i)
            for (uint32_t w = 0; w < 12; w += 4) W32(list + kept * 12 + w, R32(e + w));
        kept++;
    }
    if (kept != n) {
        kernel::G<uint16_t>(cam + 0x18) = uint16_t(kept);
        static bool said = false;
        if (!said) {
            said = true;
            LOGR("nightfire: glows over the walls are decided per player camera (camera %u hid %u)", hw, n - kept);
        }
    }
}

// The world's objects (a list from [0x1DF34C], next at +0x14) are drawn in the cameras whose bits their +0xD4 holds
// (0xDA490 tests it against the pass's [0x29E80C]): every frame a player's camera update (0xA8A60) gives their body
// every camera but theirs (0xDB3A0) and their first-person model (inner + 0x778) theirs alone (0xDB380), both passed
// on to the model's attachments ([b8 + 0xA0], [rec + 0x29] of them). An object drawn in some cameras only that is
// none of those was left behind by a model rebuilt under it (ApplyPlayerHands), and is reported once
void LogLeftObjects() {
    static std::vector<uint32_t> seen;
    static uint32_t level = 0;
    if (level != R32(0x1F658C)) level = R32(0x1F658C), seen.clear(), g_sights.clear();
    if (seen.size() >= 20) return;
    auto players = [&](uint32_t obj) {
        for (int s = 0; s < CoopPlayers(); s++) {
            uint32_t outer = R32(kOuterActors + uint32_t(s) * 4);
            uint32_t inner = ValidPtr(outer, 0xE0) ? R32(outer + 0xBC) : 0;
            uint32_t vm = ValidPtr(inner, 0x900) ? R32(inner + 0x778) : 0;
            for (uint32_t p : {outer, vm}) {
                if (!ValidPtr(p, 0xE0)) continue;
                if (obj == p) return true;
                uint32_t b8 = R32(p + 0xB8), rec = ValidPtr(b8, 0xA4) ? R32(b8 + 0x74) : 0;
                uint32_t kids = ValidPtr(b8, 0xA4) ? R32(b8 + 0xA0) : 0;
                uint32_t count = ValidPtr(rec, 0x2A) ? kernel::G<uint8_t>(rec + 0x29) : 0;
                for (uint32_t i = 0; i < count && ValidPtr(kids, (i + 1) * 4); i++)
                    if (R32(kids + i * 4) == obj) return true;
            }
        }
        return false;
    };
    uint32_t n = 0;
    for (uint32_t obj = R32(0x1DF34C); ValidPtr(obj, 0xE0) && n < 4096; obj = R32(obj + 0x14), n++) {
        uint16_t mask = kernel::G<uint16_t>(obj + 0xD4) & 0x1F;
        if (mask == 0x1F || mask == 0 || players(obj) || g_sights.count(obj)) continue;
        if (kernel::G<uint8_t>(obj + 0xDB) == 0x05) continue;  // a missile: not in the camera of the player who steers it
        if (std::find(seen.begin(), seen.end(), obj) != seen.end()) continue;
        seen.push_back(obj);
        LOGR("nightfire: co-op object %08X (type %02X) is drawn in cameras %02X alone and is no player's, at %.1f,%.1f,%.1f",
             obj, kernel::G<uint8_t>(obj + 0xDB), mask, GetF(obj + 0x24), GetF(obj + 0x28), GetF(obj + 0x2C));
    }
}

// the frame's camera passes: each pass starts with 0xDD950 for its camera (the game skips the call when the camera
// is the one already set), so a remote player's suppression holds through its 3D, its 2D list (0xD9410 sets a full
// screen viewport through 0xDE610 first) and its hit flash (0xDE560, the camera's tint at +0x200 while +0x205 is set)
// until the next camera's pass; it ends with the frame
X86_FUNC(sub_000DAC20) {
    int local = net::FullScreenView() ? net::LocalPlayer() : -1;
    ScopedPlayerSlot scope(local);
    if (IsCoopCampaign()) {
        uint32_t cam0 = R32(kCameras);
        uint32_t cam4 = R32(kCameras + 4 * 4);
        bool cutscene = ValidPtr(cam0, 0x30) && kernel::G<uint8_t>(cam0 + 0x29) == 0;
        int total = CoopPlayers();
        for (int s = 0; s < total; s++) {
            uint32_t cam = R32(kCameras + uint32_t(s) * 4);
            if (!ValidPtr(cam, 0x208)) continue;
            // Clear stuck fade-to-black on player cameras if camera 4 (HUD/cinematic fade) or camera 0 is already clear
            if (kernel::G<uint8_t>(cam + 0x205) != 0) {
                bool cam0_clear = ValidPtr(cam0, 0x208) && kernel::G<uint8_t>(cam0 + 0x205) == 0;
                bool cam4_clear = ValidPtr(cam4, 0x208) && kernel::G<uint8_t>(cam4 + 0x205) == 0;
                if ((s > 0 && cam0_clear) || (s == 0 && cam4_clear && GetF(cam + 0x1FC) == 0.0f)) {
                    kernel::G<uint8_t>(cam + 0x203) = 0;
                    kernel::G<uint8_t>(cam + 0x205) = 0;
                }
            }
            if (s > 0 && CoopGone(s)) {
                kernel::G<uint8_t>(cam + 0x29) = 0;  // a split screen's view of a player who left stays dark
                kernel::G<uint8_t>(cam + 0x2A) = 0;
            } else if (s > 0 && R32(kOuterActors + uint32_t(s) * 4) != 0) {
                kernel::G<uint8_t>(cam + 0x29) = cutscene ? 0 : 1;
            }
        }
        ++g_coop_frames;
        if (g_coop_frames == 120 || g_coop_frames == 1200) LogCoopCameras(g_coop_frames == 120 ? "at 2 s" : "at 20 s");
        LogLeftObjects();
    }
    d3d::SuppressDraws(false);
    __imp__sub_000DAC20(ctx, base);
    d3d::SuppressDraws(false);
}

// the split-screen divider bars drawn at the end of 0xDAC20 when [0x2ADF33] is non-zero
X86_FUNC(sub_000E7130) {
    if (!net::FullScreenView()) return __imp__sub_000E7130(ctx, base);
    bool was = d3d::SuppressDraws(true);
    __imp__sub_000E7130(ctx, base);
    d3d::SuppressDraws(was);
}

// The player's hints (0xB9580, cdecl: the player's object, every 6th frame) show "Press Action to open door" for two
// kinds of door: a type 0x1D door's (0x2000009, from 0xB9874) every 20 s ([0x279158], counted down by each call,
// shown at 0), a type 0x1C door's (0x200000B, from 0x2F5B0) the first 3 times a boot (0x279154). Either shows once a
// level (John): after it the wait is held over 1 and the 0x1C count at 3 through the call, the game's own put back
namespace {
uint32_t g_door_level = 0, g_door_hints = 0;
constexpr uint32_t kDoorHintWait = 0x279158, kDoorHintCount = 0x279154, kDoorHintMax = 1;
void DoorHintLevel() {
    if (g_door_level != R32(0x1F658C)) g_door_level = R32(0x1F658C), g_door_hints = 0;
}
}  // namespace

namespace nf {
void DoorHintFetched() {
    DoorHintLevel();
    if (++g_door_hints == kDoorHintMax) LOG_FIRST(4, "nightfire: the door hint shown: no more this level");
}
}  // namespace nf

X86_EXTERN_FUNC(__imp__sub_000B9580);
X86_FUNC(sub_000B9580) {
    DoorHintLevel();
    if (g_door_hints < kDoorHintMax) return __imp__sub_000B9580(ctx, base);
    if (R32(kDoorHintWait) < 2) W32(kDoorHintWait, 2);
    uint32_t count = R32(kDoorHintCount);
    if (count < 3) W32(kDoorHintCount, 3);
    __imp__sub_000B9580(ctx, base);
    W32(kDoorHintCount, count);
}

// The watch's hands (0xB8CB0, cdecl: the player's object, from their update 0xAE1C0): with the watch gadgets' model
// out it makes three world objects (0x2000876-78, at 0x279150, 0x27914C, 0x279148, all cameras) once and puts them
// on the model's dial, else deletes them. One set for the level (0xB6C20 clears it): in co-op the last player's update
// put them on their own wrist in the world, or one without the watch deleted them, so player 1's dial lost them. Each
// player has their own set now, player 1's in the game's globals. The view model is in no camera ([obj+0xD4] 0, its
// own pass), so in full-screen view the hands are in every camera on their player's machine and in none on the others
// (a draw-only difference, as the guided rocket's sight)
namespace {
constexpr uint32_t kWatchHands = 0x279148;
uint32_t g_watch_hands[net::kMaxPlayers][3] = {};
}  // namespace

X86_EXTERN_FUNC(__imp__sub_000B8CB0);
X86_FUNC(sub_000B8CB0) {
    uint32_t outer = R32(ctx.esp + 4);
    int s = IsCoopCampaign() ? TruePlayerFromOuter(outer) : -1;
    if (s < 0 || s >= net::kMaxPlayers) return __imp__sub_000B8CB0(ctx, base);
    uint32_t game[3];
    for (uint32_t i = 0; i < 3; i++) {
        game[i] = R32(kWatchHands + 4 * i);
        if (s > 0) W32(kWatchHands + 4 * i, g_watch_hands[s][i]);
    }
    uint32_t had = R32(kWatchHands + 8);
    __imp__sub_000B8CB0(ctx, base);
    bool remote = net::FullScreenView() && s != net::LocalPlayer();
    uint32_t hands[3];
    uint16_t made = ValidPtr(R32(kWatchHands + 8), 0xE0) ? kernel::G<uint16_t>(R32(kWatchHands + 8) + 0xD4) : 0;
    for (uint32_t i = 0; i < 3; i++) {
        hands[i] = R32(kWatchHands + 4 * i);
        // a rejoin's snapshot of the host's memory brings this machine's own hands with the host's 0
        if (net::FullScreenView() && ValidPtr(hands[i], 0xE0)) kernel::G<uint16_t>(hands[i] + 0xD4) = remote ? 0 : 0x1F;
        if (s > 0) g_watch_hands[s][i] = hands[i], W32(kWatchHands + 4 * i, game[i]);
    }
    if (hands[2] && !had)
        LOG_FIRST(8, "nightfire: player %d's watch hands: %08X %08X %08X, the game's cameras %04X%s", s + 1, hands[2],
                  hands[1], hands[0], unsigned(made), remote ? ", in none here" : "");
}

X86_EXTERN_FUNC(__imp__sub_000B6C20);
X86_FUNC(sub_000B6C20) {
    std::memset(g_watch_hands, 0, sizeof(g_watch_hands));
    __imp__sub_000B6C20(ctx, base);
}

// snapshots (src/game/hooks.cpp): the co-op and session state the players' updates go by
namespace nf {
void SaveViewState(snapshot::Writer& w) {
    w.Pod(g_slot), w.Pod(g_active_slot), w.Pod(g_hud_shown), w.Pod(g_weapon_held), w.Pod(g_coop_spawn_pos);
    w.Pod(g_coop_spawn_rot), w.Pod(g_coop_frames), w.Pod(g_coop_default_model), w.Pod(g_applied_char);
    w.Pod(g_mission_hands), w.Pod(g_mission_skin), w.Pod(g_death_held), w.Pod(g_down_ticks), w.Pod(g_was_gone);
    w.Pod(g_door_level), w.Pod(g_door_hints), w.Pod(g_watch_hands), w.Pod(g_low_bots);
}
void LoadViewState(snapshot::Reader& r) {
    r.Pod(g_slot), r.Pod(g_active_slot), r.Pod(g_hud_shown), r.Pod(g_weapon_held), r.Pod(g_coop_spawn_pos);
    r.Pod(g_coop_spawn_rot), r.Pod(g_coop_frames), r.Pod(g_coop_default_model), r.Pod(g_applied_char);
    r.Pod(g_mission_hands), r.Pod(g_mission_skin), r.Pod(g_death_held), r.Pod(g_down_ticks), r.Pod(g_was_gone);
    r.Pod(g_door_level), r.Pod(g_door_hints), r.Pod(g_watch_hands), r.Pod(g_low_bots);
}
}  // namespace nf

// 0x1B170 (a bot's spawn) after 0x1B191's add ebp,0x1D98E0: a bot below slot 4 (edx its slot) has its state in
// net::BotBlocks(), not 0x1D98E0 + (slot - 4) * 0x768 (another table's memory)
X86_HOST_API void BotStateBlock(X86Context& ctx, uint8_t*) {
    int slot = int(int16_t(ctx.edx & 0xFFFF));
    if (slot >= 0 && slot < 4 && net::BotBlocks()) ctx.ebp = net::BotBlocks() + uint32_t(slot) * kBotState;
}

// after 0x1B3CB's mov [ebp+0x750],ax (its slot; +0x752 slot - 4): the bot's index into the bots' six-entry tables
// (each state's +0x3C seen times, 0x2600B4's columns, 0x37C10's rows) is one of the six's, its slot's
X86_HOST_API void BotStateIndex(X86Context& ctx, uint8_t*) {
    uint32_t blocks = net::BotBlocks();
    if (!blocks || ctx.ebp < blocks || ctx.ebp >= blocks + 4 * kBotState) return;
    kernel::G<uint16_t>(ctx.ebp + 0x752) = uint16_t(kernel::G<int16_t>(ctx.ebp + 0x750) & 3);
}

// 0xA84D0 (a pickup's update) offers it to the players (0x1F6654) and the six bots (0x262814): after 0xA87CD's
// mov ebp,[ecx+0x262814], the bot past six (eax its index) is a bot below slot 4
X86_HOST_API void PickupBot(X86Context& ctx, uint8_t*) {
    uint32_t i = ctx.eax & 0xFFFF;
    if (i < kBotSlots) return;
    int s = LowBotSlot(int(i - kBotSlots));
    ctx.ebp = s >= 0 ? SlotActor(s) : 0;
}

// after 0xA8850's cmp dx,6 (before its jb back): the loop goes on through the bots below slot 4
X86_HOST_API void PickupBots(X86Context& ctx, uint8_t*) {
    uint32_t i = ctx.edx & 0xFFFF;
    if (i >= kBotSlots && LowBotSlot(int(i - kBotSlots)) >= 0) ctx.cf = 1;
}
