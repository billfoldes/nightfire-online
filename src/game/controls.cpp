// The keyboard and mouse play as the NightFire controller style whatever style the player chose.
//
// default.xbe reads the pads into each player's controls in 0xDE720 (cdecl: the players, their count; each player
// 0x158 bytes from 0x1FE6D0): a switch on the player's controller style (the u16 at +0xE: NightFire 0, Moonraker 1,
// Octopussy 2, Goldfinger 3, Dr. No 4, Thunderball 5, GoldenEye 6, Classic Bond 7) picks which stick axis is moving,
// strafing, turning and looking and which button does what. The keys are laid out for style 0's controls page. While
// controller 1 is played on the keyboard and mouse its player's style is 0, so the game's prompts and controls page
// name the buttons the keys play (the pictures show the keys: src/common/buttons.cpp); a controller puts the chosen
// style back, and the profile is written with it (0x6ED30 packs a player's controls for the save and the launch data).
//   With a session (src/common/net.cpp) every player is a controller port, and the keyboard on their machine plays
// theirs: each player whose own machine's input says keyboard reads style 0, every machine runs this for every
// player, and the styles the game reads are the same everywhere.
#include "core/snapshot.h"
#include "common/net.h"
#include "common/nightfire.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace net = nf::net;  // the session (src/common/net.cpp)

namespace {
constexpr uint32_t kControls = 0x1FE6D0, kPlayerSize = 0x158, kStyle = 0xE, kPort = 0x156;
int g_player = -1;     // controller 1's player while its style is held at 0
uint16_t g_chosen = 0;  // its own style meanwhile
// a session: each player's own style while the keyboard on their machine plays them, 0 not forced
uint16_t g_session_chosen[net::kMaxPlayers] = {};

void Restore() {
    if (g_player < 0) return;
    kernel::G<uint16_t>(kControls + uint32_t(g_player) * kPlayerSize + kStyle) = g_chosen;
    g_player = -1;
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_000DE720);
X86_FUNC(sub_000DE720) {
    uint32_t players = kernel::R32(ctx.esp + 4), count = kernel::R32(ctx.esp + 8) & 0xFFFF;
    if (net::Active()) {
        if (players == kControls)
            for (uint32_t i = 0; i < count && i < 4; i++) {
                uint32_t p = kControls + i * kPlayerSize;
                uint8_t port = kernel::G<uint8_t>(p + kPort);
                if (port >= 4) continue;  // not one of the four controller ports
                int pl = net::PortPlayer(int(port));
                if (pl < 0 || pl >= net::kMaxPlayers) continue;
                uint16_t& style = kernel::G<uint16_t>(p + kStyle);
                uint16_t& chosen = g_session_chosen[pl];
                if (net::KeyboardFor(pl)) {
                    if (style) {
                        chosen = style;  // chosen anew in the options meanwhile: that one is the player's
                        style = 0;
                        LOGR("nightfire: player %d's style %u; their keyboard plays as NightFire's (style 0)",
                             pl + 1, chosen);
                    }
                } else if (chosen && !style) {
                    style = chosen;  // their machine plays a controller: their own style is back
                    chosen = 0;
                }
            }
    } else if (!input::UsingKeyboard() || players != kControls || count > 4) {
        Restore();
    } else if (g_player >= 0) {
        uint16_t& style = kernel::G<uint16_t>(kControls + uint32_t(g_player) * kPlayerSize + kStyle);
        if (style) g_chosen = style, style = 0;  // chosen anew in the options meanwhile: that one is the player's
    } else {
        for (uint32_t i = 0; i < count; i++) {
            uint32_t p = kControls + i * kPlayerSize;
            if (kernel::G<uint8_t>(p + kPort) != 0) continue;  // controller 1's player
            g_chosen = kernel::G<uint16_t>(p + kStyle);
            g_player = int(i);
            kernel::G<uint16_t>(p + kStyle) = 0;
            if (g_chosen)
                LOGR("nightfire: controller style %u chosen; the keyboard and mouse play as NightFire's (style 0)",
                     g_chosen);
            break;
        }
    }
    __imp__sub_000DE720(ctx, base);
    nf::MouseAsStick();
}

// a player's controls packed for the profile and the launch data (cdecl: a tag, the player, ...): with its own style
X86_EXTERN_FUNC(__imp__sub_0006ED30);
X86_FUNC(sub_0006ED30) {
    int player = int(kernel::R32(ctx.esp + 8) & 0xFFFF);
    if (net::Active()) {
        if (player < 0 || player >= 4) return __imp__sub_0006ED30(ctx, base);
        uint32_t row = kControls + uint32_t(player) * kPlayerSize;
        uint8_t port = kernel::G<uint8_t>(row + kPort);
        int pl = (port < 4) ? net::PortPlayer(int(port)) : -1;
        if (pl < 0 || pl >= net::kMaxPlayers || !g_session_chosen[pl]) return __imp__sub_0006ED30(ctx, base);
        uint16_t& style = kernel::G<uint16_t>(row + kStyle);
        style = g_session_chosen[pl];  // packed as their own style, as the profile keeps it
        __imp__sub_0006ED30(ctx, base);
        style = 0;
        return;
    }
    if (player != g_player) return __imp__sub_0006ED30(ctx, base);
    uint16_t& style = kernel::G<uint16_t>(kControls + uint32_t(player) * kPlayerSize + kStyle);
    style = g_chosen;
    __imp__sub_0006ED30(ctx, base);
    style = 0;
}

// snapshots (src/game/hooks.cpp): the controller styles held
namespace nf {
void SaveControlsState(snapshot::Writer& w) { w.Pod(g_player), w.Pod(g_chosen), w.Pod(g_session_chosen); }
void LoadControlsState(snapshot::Reader& r) { r.Pod(g_player), r.Pod(g_chosen), r.Pod(g_session_chosen); }
}  // namespace nf
