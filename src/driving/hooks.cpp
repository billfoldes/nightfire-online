// Hooks for Driving.xbe (the driving levels). A strong definition replaces the recompiled function at that address,
// as in src/game/hooks.cpp.

#include "common/net.h"
#include "common/nightfire.h"
#include "core/snapshot.h"
#include "kernel/kernel.h"

namespace nf {
// which exe this is (Driving.xbe): src/common's key layouts and settings tables follow it
extern const bool kDriving = true;

// EA's in-game menus (Paused and its tabs, Mission Failed, the controller-removed menu): the HUD's byte +0x2B0, set
// by 0xDCAE0 (0xDCBBB) when one opens (START 0xE0278, a pad removed 0xE00F0, the mission's end 0xDF2E0) and cleared
// at 0xD9091 (.local/driving_input_re.md)
bool MenuUp() {
    uint32_t hud = kernel::R32(0x23F44C);
    return hud && kernel::G<uint8_t>(hud + 0x2B0) != 0;
}

// the menus show Enter and Backspace for A and B, but for the pause's CONTROLS tab (the HUD's page +0x470: MISSION 0,
// OBJECTIVES 1, CONTROLS 2, SCORE 3), whose lines name the keys played
bool MenuKeys() {
    uint32_t hud = kernel::R32(0x23F44C);
    return MenuUp() && kernel::R32(hud + 0x470) != 2;
}

// the control map (0x1E455C): +4 its mode, 1 a mounted gun (data\control\povcfgN.def), 0 the car (drivecfg.def), 2 none
// yet; +0xC the gun's style (the player's controller style)
bool PlayerFree() { return true; }  // default.xbe's questions (src/game/mouse.cpp)
bool Zoomed() { return false; }
void ScopeZoom(int) {}

// the mission's level id: the launch data default.xbe wrote is copied to 0x243B90, its level at +0x14C
uint32_t Level() { return kernel::R32(0x243CDC); }

// the player's controller port, [0x244520] (0x50EE0's pad-removed check, the input objects' +0x18 through 0x108480),
// from the launch data default.xbe wrote
int PlayerPort() {
    uint32_t port = kernel::R32(0x244520);
    return port < 4 ? int(port) : 0;
}

int GunStyle() {
    if (kernel::R32(0x1E455C + 4) != 1 || MenuUp()) return -1;
    return int(kernel::R32(0x1E455C + 0xC) & 7);
}

// the snapshot point is the main loop's frame (clock.cpp)
void SaveClockState(snapshot::Writer& w);
void LoadClockState(snapshot::Reader& r);
void SaveCoopState(snapshot::Writer& w);
void LoadCoopState(snapshot::Reader& r);

void RegisterSnapshot() {
    snapshot::Register("net", net::SaveState, net::LoadState);
    snapshot::Register(
        "driving", [](snapshot::Writer& w) { SaveClockState(w), SaveCoopState(w); },
        [](snapshot::Reader& r) { LoadClockState(r), LoadCoopState(r); });
    net::EnableRejoin();
}
}  // namespace nf

// the HUD's menu events (thiscall, the event: 0x41 up, 0x42 down, 0x43 left, 0x44 right, 0x45 A, 0x46 B). In the
// pause's MISSION tab (+0x470 0) with QUIT chosen (+0x474 2) and its "are you sure" on YES (+0x484 1), A ends the
// mission (0xDCDEF). The pause is every player's (game_info.cpp's SharedMenus), so in a session that YES is the
// players' whose A is down this tick: their machines leave the session and quit alone; on the others the same press
// is CONTINUE (+0x474 0), the pause closes and the mission goes on without them. The host's, with net_quit_together
// (the host's, by the tick), is the game's own on every machine: the mission ends for all, the session goes on
X86_EXTERN_FUNC(__imp__sub_000DCC10);
X86_FUNC(sub_000DCC10) {
    constexpr uint32_t kA = 0x45, kTab = 0x470, kChoice = 0x474, kConfirm = 0x484, kQuit = 2, kYes = 1;
    uint32_t hud = ctx.ecx;
    if (nf::net::Active() && kernel::R32(ctx.esp + 4) == kA && kernel::R32(hud + kTab) == 0 &&
        kernel::R32(hud + kChoice) == kQuit && kernel::R32(hud + kConfirm) == kYes) {
        bool any = false, mine = false, host = false;
        for (int player = 0; player < nf::net::Players(); ++player) {
            input::Pad p;
            if (!nf::net::PadFor(player, p) || !p.connected || p.analog[0] <= 64) continue;
            any = true;
            mine |= player == nf::net::LocalPlayer();
            host |= player == 0 && !nf::net::PlayerLeft(0);
            LOGR("driving: player %d quit from the pause screen at tick %u", player + 1, nf::net::Tick());
        }
        if (host && nf::net::QuitTogether()) {
            LOGR("driving: the host quit from the pause screen: everyone goes to the menus with them (net_quit_together)");
        } else if (mine) {
            LOGR("driving: QUIT on the pause screen: this player leaves the session for the menu");
            nf::net::LeaveToMenu();
        } else if (any) {
            kernel::W32(hud + kChoice, 0);
            kernel::W32(hud + kConfirm, 0);
        }
    }
    __imp__sub_000DCC10(ctx, base);
}
