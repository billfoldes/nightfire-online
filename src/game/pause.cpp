// A level's pause screen in an online session is the pauser's alone. Offline, START (0xAD520, a player's update)
// opens the pause page for that player's menu context (0x25F1D0, 0x1D8 a player) and calls 0x6AF70(1), which sets
// the global pause flag (0x1F65D0) and every player's (0x262738, 0x30 apart) and pauses the sound (0xB0570); the
// world's update (0x6AA90) runs while 0x6AFB0(-1) is false, and each player's update skips while 0x6AFB0(player),
// their own flag, is set. Closing a menu START opened (0x942B0, the player in esi / 0x1D8) calls 0x6AF70(0).
//   In a session that START sets the pauser's flag alone: the world and the other players play on, the pauser's
// character stands still, and their menu (every page of it: the tabs, CONTINUE, RESTART, QUIT and its YES/NO, PC
// SETTINGS) runs on every machine from their input, as everything does, but is drawn on theirs alone (the menu is
// their view's 2D list, 0xD9410, src/game/hud.cpp). Every other 0x6AF70 call (the reconnect-controller page, a
// level's end) is the game's. When every player is paused at once ([0x260018] set) 0x6AFB0(-1) pauses the world.
//   Every menu's sprites are camera 7's 2D list (0x72550 sets them): items of the sprite pool (0x29AA98, 0x44 bytes
// each; 0x6E6F0 takes one, 0x6E740 gives it back). Those taken while a player's menu is built or updated (0x93960
// opening their pause, 0x95A80 updating their menu) are tagged with that player here, outside guest memory, and
// 0xD9410 draws the list without another player's own pause: the list's array is compacted for the call and put
// back, so every machine's memory stays the same.
//   A paused player who is alive still has their update (0xAD520), with their controls (0x1FE6D0, 0x158 a player)
// idle through it: enemies see, shoot and kill them. Killed, their update waits for them to close the pause, and
// they die then. The pause's fire picture (0xE0020, which the game runs only while the world is paused) runs while
// any player's own pause is up. Player N+1's menu is not held by player N's own pause (the menus' updates, 0x94370
// and 0x94720, skip a player's menu while the one before has one up: OwnMenuNotHeld, config/nightfire.toml).
// Another 0x6AF70 (a cutscene, a level's end) leaves the paused players' flags set. Building a page clears every
// player's controls: a player's own pause leaves the others' as they were (their START in the same frame counts).
//   A door's keypad (0xD1550, a keypad object's update, at its state 3) opens its page (0x40000021) for the player
// who used it ([0x1F656C]+0x8DE, set by 0xD0230), in menu 1: in a session in that player's menu, their own as the
// pause is, drawn on
// their machine alone, and the menus after it are not held by it (2026-11-26, John: Nightshift's keypad showed on
// every player's screen).
#include <map>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "core/snapshot.h"
#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace net = nf::net;
using kernel::R32;
using kernel::W32;

namespace {
constexpr uint32_t kMenus = 0x25F1D0, kMenuSize = 0x1D8;  // +0x1BC the menu's root widget, none in play
constexpr uint32_t kPauseFlags = 0x262738, kPauseFlagSize = 0x30;
constexpr uint32_t kFromStart = 0xAD58C;      // 0x6AF70(1) after START opened the pause page
constexpr uint32_t kFromClose = 0x94317;      // 0x6AF70(0) as 0x942B0 closes a menu START opened
constexpr uint32_t kStartPressed = 0xAD541;   // 0xAD520's START query, 0x6C9A0(port, 0x1E, 4)
constexpr uint32_t kCloseMenu = 0x942B0;      // cdecl (player)
// a cutscene's skip, 0x6C9A0(-1, 0x19, 1): any controller's press. 0xC4730 (a cutscene object's update, playing) calls
// 0xC2F60(its scene, 1) on it; the game's state 6 (0x6ACA0) ends on it
constexpr uint32_t kCutsceneSkip = 0xC498D, kStateSkip = 0x6AE5D;
constexpr uint32_t kFirePicture = 0xE0020;
constexpr uint32_t kGlobalPause = 0x1F65D0;
constexpr uint32_t kControls = 0x1FE6D0, kControlSize = 0x158;

bool g_own[4];          // the player's own pause is up
bool g_start_held[4];   // their START just closed it: the same press does not open it again
bool g_close[4];        // their pause closes at the next menu update (they chose QUIT's YES on another machine)

constexpr uint32_t kSpritePool = 0x29AA98;
constexpr uint32_t kOpenPause = 0xAD57E;      // 0x93960 returning to 0xAD520: the pause page for its player
constexpr uint32_t kOpenKeypad = 0xD1AF4;     // 0x93960 returning to 0xD1550: a keypad's page for its player
bool g_keypad[4];                             // the player's menu is a keypad they use
bool OwnMenu(int p) { return g_own[p] || g_keypad[p]; }
int g_menu_player = -1;                       // the player whose menu is being built or updated
int g_opening = -1;                           // the player whose own pause 0x93960 is opening
uint8_t g_others[4][kControlSize];            // the other players' controls while it does (it clears them all)
std::unordered_map<uint32_t, int> g_sprite_player;
}  // namespace

namespace nf {
// around 0xAD520 (src/game/view.cpp): an alive paused player's update runs, their controls idle through it
PausedUpdate::PausedUpdate(uint32_t outer) {
    if (!net::Active() || !outer) return;
    uint32_t inner = R32(outer + 0xBC);
    if (!inner) return;
    int p = int8_t(kernel::G<uint8_t>(inner + 0x8DE));
    if (p < 0 || p >= 4 || !g_own[p] || kernel::G<int16_t>(outer + 0xD0) != 1) return;
    if (kernel::G<float>(inner + 0x824) <= 0.0f) return;
    port = p;
    uint8_t* c = &kernel::G<uint8_t>(kControls + uint32_t(p) * kControlSize);
    std::memcpy(controls, c, kControlSize);
    std::memset(c + 0x14, 0, kControlSize - 0x18);
    W32(kPauseFlags + uint32_t(p) * kPauseFlagSize, 0);
}

PausedUpdate::~PausedUpdate() {
    if (port < 0) return;
    std::memcpy(&kernel::G<uint8_t>(kControls + uint32_t(port) * kControlSize), controls, kControlSize);
    W32(kPauseFlags + uint32_t(port) * kPauseFlagSize, g_own[port] ? 1 : 0);
}

// 0x93960 (src/game/pc_settings_item.cpp): the page being built is the pause of its player (its third argument;
// menu_arg the address of its fourth, the menu it goes in). A keypad's page goes in menu 1 whoever used it: in a
// session it goes in its player's menu as the pause does (2026-11-27, John: player 1's keypad showed on every screen,
// in player 2's menu)
int MenuBuilt(uint32_t from, uint32_t player, uint32_t menu_arg) {
    int was = g_menu_player;
    if ((from == kOpenPause || from == kOpenKeypad) && player < 4) {
        if (from == kOpenKeypad && net::Active()) {
            W32(menu_arg, (R32(menu_arg) & ~0xFFu) | player);
            if (!g_keypad[player]) {
                g_keypad[player] = true;
                LOGR("nightfire: player %d is at a keypad: their own menu", int(player) + 1);
            }
        }
        g_menu_player = g_opening = int(player);
        if (net::Active())
            for (uint32_t p = 0; p < 4; p++)
                std::memcpy(g_others[p], &kernel::G<uint8_t>(kControls + p * kControlSize), kControlSize);
    }
    return was;
}
void MenuBuiltDone(int was) {
    if (g_opening >= 0 && net::Active())
        for (uint32_t p = 0; p < 4; p++)
            if (int(p) != g_opening)
                std::memcpy(&kernel::G<uint8_t>(kControls + p * kControlSize), g_others[p], kControlSize);
    g_menu_player = was;
    g_opening = -1;
}

bool OwnPause(int player) { return net::Active() && player >= 0 && player < 4 && g_own[player]; }

void ClosePauseLater(int player) {
    if (player >= 0 && player < 4) g_close[player] = true;
}

bool SharedMenuUp() {
    for (uint32_t i = 0; i < 4; i++)
        if (kernel::R32(kMenus + i * kMenuSize + 0x1BC) != 0 && !(net::Active() && OwnMenu(int(i)))) return true;
    return false;
}

bool MenuUpFor(int player) {
    if (!net::Active()) return kernel::R32(kMenus + 0x1BC) != 0;
    return SharedMenuUp() || (player >= 0 && player < 4 && kernel::R32(kMenus + uint32_t(player) * kMenuSize + 0x1BC) != 0);
}
}  // namespace nf

X86_EXTERN_FUNC(__imp__sub_0006AF70);
X86_FUNC(sub_0006AF70) {
    if (net::Active()) {
        uint32_t from = R32(ctx.esp);
        bool on = (R32(ctx.esp + 4) & 0xFF) != 0;
        int player = -1;
        if (from == kFromStart && on) player = kernel::G<uint8_t>(ctx.ebp + 0x8DE);
        if (from == kFromClose && !on) player = int(ctx.esi / kMenuSize);
        if (player >= 0 && player < 4 && (on || g_own[player])) {
            g_own[player] = on;
            if (!on) g_start_held[player] = true;
            W32(kPauseFlags + uint32_t(player) * kPauseFlagSize, on ? 1 : 0);
            LOGR("nightfire: player %d %s their own pause; the game goes on", player + 1, on ? "opened" : "closed");
            ctx.esp += 4;  // cdecl: the return address only
            return;
        }
    }
    __imp__sub_0006AF70(ctx, base);
    for (uint32_t p = 0; p < 4; p++)
        if (net::Active() && g_own[p]) W32(kPauseFlags + p * kPauseFlagSize, 1);
}

// 0x6C9A0 (cdecl: port, button, how) answers 0xAD520's START query: none while that player's pause is up, nor in the
// frame their START closed it
X86_EXTERN_FUNC(__imp__sub_0006C9A0);
X86_FUNC(sub_0006C9A0) {
    // in a session the host skips a cutscene, the others watch it (2026-11-14, John); with the host gone, the first
    // player still in
    if (uint32_t ret = R32(ctx.esp); net::Active() && (ret == kCutsceneSkip || ret == kStateSkip) &&
                                     int16_t(R32(ctx.esp + 4) & 0xFFFF) == -1) {
        int skipper = 0;
        while (skipper < std::min(net::Players(), 4) - 1 && net::PlayerLeft(skipper)) skipper++;
        W32(ctx.esp + 4, uint32_t(skipper));
        __imp__sub_0006C9A0(ctx, base);
        static int said = 0;
        if ((ctx.eax & 0xFFFF) && said < 8) {
            said++;
            LOGR("nightfire: player %d skips the %s", skipper + 1, ret == kCutsceneSkip ? "cutscene" : "level's intro");
        }
        return;
    }
    if (net::Active() && R32(ctx.esp) == kStartPressed) {
        int port = int16_t(R32(ctx.esp + 4) & 0xFFFF);
        if (port >= 0 && port < 4 && (g_own[port] || g_start_held[port])) {
            g_start_held[port] = false;
            ctx.eax = 0;
            ctx.esp += 4;
            return;
        }
    }
    __imp__sub_0006C9A0(ctx, base);
}

// the menus' update (0x6AA90 calls it first each frame): a pause another machine's player quit from closes here
X86_EXTERN_FUNC(__imp__sub_00094370);
X86_FUNC(sub_00094370) {
    bool fire = false;
    for (uint32_t p = 0; p < 4; p++) {
        if (g_own[p] && kernel::R32(kMenus + p * kMenuSize + 0x1BC) == 0) {  // its menu went with a level's end
            g_own[p] = false;
            W32(kPauseFlags + p * kPauseFlagSize, 0);
        }
        if (g_keypad[p] && kernel::R32(kMenus + p * kMenuSize + 0x1BC) == 0) g_keypad[p] = false;
        fire |= net::Active() && g_own[p];
        if (!g_close[p]) continue;
        g_close[p] = false;
        if (kernel::R32(kMenus + p * kMenuSize + 0x1BC) == 0) continue;
        uint32_t eax = ctx.eax;
        kernel::CallGuest(ctx, kCloseMenu, {p});
        ctx.eax = eax;
    }
    __imp__sub_00094370(ctx, base);
    if (fire && (R32(kGlobalPause) & 0xFF) == 0) {
        uint32_t eax = ctx.eax;
        kernel::CallGuest(ctx, kFirePicture, {});
        ctx.eax = eax;
    }
}

// config/nightfire.toml's [[hook]]s after 0x94412 and 0x9473E (mov al, [esi + 0x25F1C9]: the menu before this
// one, esi / 0x1D8, is up): not when that one is its player's own pause
X86_HOST_API void OwnMenuNotHeld(X86Context& ctx, uint8_t* base) {
    (void)base;
    uint32_t i = ctx.esi / kMenuSize;
    if (net::Active() && i > 0 && i < 4 && OwnMenu(int(i) - 1)) ctx.eax &= ~0xFFu;
}

// NF_KEYPAD_TEST=player,tick (the harness): that player uses the level's first idle keypad at that session tick
// (0xD0230, a player's use of a keypad: its state 3, the user at 0x1F6568)
X86_EXTERN_FUNC(__imp__sub_000D1550);
X86_FUNC(sub_000D1550) {
    static int test_player = -1;
    static uint32_t test_tick = 0;
    static bool parsed = false, done = false;
    if (!parsed) {
        parsed = true;
        if (const char* e = std::getenv("NF_KEYPAD_TEST")) std::sscanf(e, "%d,%u", &test_player, &test_tick);
    }
    uint32_t obj = R32(ctx.esp + 4);
    if (test_player >= 0 && !done && net::Active() && net::Tick() >= test_tick && obj &&
        kernel::G<int16_t>(obj + 0xD0) == 0) {
        done = true;
        uint32_t player = R32(0x1F6654 + uint32_t(test_player) * 4);
        LOGR("keypad test: player %d uses keypad %08X at session tick %u", test_player + 1, obj, net::Tick());
        uint32_t eax = ctx.eax;
        kernel::CallGuest(ctx, 0xD0230, {obj, player});
        ctx.eax = eax;
    }
    __imp__sub_000D1550(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_00095A80);
X86_FUNC(sub_00095A80) {
    int was = g_menu_player;
    uint32_t menu = R32(ctx.esp + 4) - kMenus;
    if (menu < 4 * kMenuSize) g_menu_player = int(menu / kMenuSize);
    __imp__sub_00095A80(ctx, base);
    g_menu_player = was;
}

X86_EXTERN_FUNC(__imp__sub_0006E6F0);
X86_FUNC(sub_0006E6F0) {
    uint32_t pool = R32(ctx.esp + 4);
    __imp__sub_0006E6F0(ctx, base);
    if (pool == kSpritePool && ctx.eax) {
        if (g_menu_player >= 0) g_sprite_player[ctx.eax] = g_menu_player;
        else g_sprite_player.erase(ctx.eax);
    }
}

X86_EXTERN_FUNC(__imp__sub_0006E740);
X86_FUNC(sub_0006E740) {
    if (R32(ctx.esp + 4) == kSpritePool) g_sprite_player.erase(R32(ctx.esp + 8));
    __imp__sub_0006E740(ctx, base);
}

namespace nf {
// 0xD9410 (cdecl: the view, the sprites' array, first, end; src/game/hud.cpp): another player's own pause is not
// drawn here. Returns false when the list has none
bool DrawWithoutOthersPauses(X86Context& ctx, uint8_t* base, void (*draw)(X86Context&, uint8_t*)) {
    if (!net::Active() || g_sprite_player.empty() || SharedMenuUp()) return false;
    int local = net::LocalPlayer();
    bool any = false;
    for (int p = 0; p < 4; p++) any |= p != local && OwnMenu(p);
    if (!any) return false;
    uint32_t array = R32(ctx.esp + 8), first = R32(ctx.esp + 0xC), end = R32(ctx.esp + 0x10);
    if (!array || end <= first || end - first > 0x200) return false;
    std::vector<uint32_t> was(end - first);
    uint32_t kept = first;
    for (uint32_t i = first; i < end; i++) {
        uint32_t sprite = R32(array + i * 4);
        was[i - first] = sprite;
        auto it = g_sprite_player.find(sprite);
        if (it != g_sprite_player.end() && it->second != local && OwnMenu(it->second)) continue;
        W32(array + kept++ * 4, sprite);
    }
    if (kept == end) return false;
    W32(ctx.esp + 0x10, kept);
    draw(ctx, base);
    for (uint32_t i = first; i < end; i++) W32(array + i * 4, was[i - first]);
    return true;
}
}  // namespace nf

// snapshots (src/game/hooks.cpp): whose pause is up
namespace nf {
void SavePauseState(snapshot::Writer& w) {
    w.Pod(g_own), w.Pod(g_start_held), w.Pod(g_close), w.Pod(g_keypad);
    std::map<uint32_t, int> sprites(g_sprite_player.begin(), g_sprite_player.end());
    w.Map(sprites);
}
void LoadPauseState(snapshot::Reader& r) {
    r.Pod(g_own), r.Pod(g_start_held), r.Pod(g_close), r.Pod(g_keypad);
    std::map<uint32_t, int> sprites;
    r.Map(sprites);
    g_sprite_player.clear();
    g_sprite_player.insert(sprites.begin(), sprites.end());
}
}  // namespace nf
