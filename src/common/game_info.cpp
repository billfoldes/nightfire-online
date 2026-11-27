#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <toml++/toml.hpp>
#endif
#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include "app/app.h"
#include "common/net.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "nightfire.h"

namespace net = nf::net;  // the session (src/common/net.cpp)

namespace nf {
bool skip_intro = true;
bool skip_tutorial = true;
bool hide_hud = false;
bool others_reticles = false;
bool own_view = true;
bool unlock_missions = false;
bool unlock_multiplayer = false;
bool unlock_rewards = false;
uint32_t fov_percent = 100;
extern bool modern_buttons;  // src/common/buttons.cpp

float Ultrawide() {
    static const float forced = [] {  // NF_ULTRAWIDE=k headless (no settings or screen): the 3D k times wider
        const char* e = std::getenv("NF_ULTRAWIDE");
        return e && *e ? float(std::atof(e)) : 0.0f;
    }();
    if (!app::Widescreen()) return 1.0f;
    float k = forced > 0 ? forced : std::max(app::FrameAspect(), 16.0f / 9.0f) / (16.0f / 9.0f);
    static bool said = false;
    if (k > 1.0f + 1e-3f && !said) {
        said = true;
        LOGR("nightfire: 3D views widened %.3f times for the %.3f:1 frame", k, app::FrameAspect());
    }
    return k;
}

float FovScale() {
    uint32_t pct = std::clamp<uint32_t>(fov_percent, 50, 150);
    static uint32_t said = 100;
    if (said != pct) {
        said = pct;
        LOGR("nightfire: field of view %u%% of the game's", pct);
    }
    return float(pct) / 100;
}

uint32_t crosshair_color = 0, crosshair_size = 100;

namespace {
struct CrosshairColor {
    const char* name;
    uint32_t rgb;
};
constexpr CrosshairColor kColors[kCrosshairColors] = {
    {"The game's own (red)", 0xFF0000}, {"White", 0xFFFFFF}, {"Green", 0x00FF00}, {"Cyan", 0x00FFFF},
    {"Yellow", 0xFFFF00}, {"Magenta", 0xFF00FF}, {"Orange", 0xFF8000}, {"Blue", 0x2060FF},
    {"Pink", 0xFF80C0}, {"Black", 0x000000}};
}  // namespace

const char* CrosshairColorName(uint32_t index) { return kColors[std::min(index, kCrosshairColors - 1)].name; }
uint32_t CrosshairRgb(uint32_t index) { return kColors[std::min(index, kCrosshairColors - 1)].rgb; }
}  // namespace nf

namespace {
// after the settings load: the harness's environment overrides them; each setting in effect is logged
void Init() {
    nf::RegisterSnapshot();
    if (const char* e = std::getenv("NF_SKIP_INTRO"); e && *e) nf::skip_intro = *e == '1';
    if (const char* e = std::getenv("NF_SKIP_TUTORIAL"); e && *e) nf::skip_tutorial = *e == '1';
    if (const char* e = std::getenv("NF_FOV"); e && *e) nf::fov_percent = uint32_t(std::atoi(e));
    if (const char* e = std::getenv("NF_CROSSHAIR_COLOR"); e && *e) nf::crosshair_color = uint32_t(std::atoi(e));
    if (const char* e = std::getenv("NF_CROSSHAIR_SIZE"); e && *e) nf::crosshair_size = uint32_t(std::atoi(e));
    if (const char* e = std::getenv("NF_HIDE_HUD"); e && *e) nf::hide_hud = *e == '1';
    if (const char* e = std::getenv("NF_UNLOCK_MISSIONS"); e && *e) nf::unlock_missions = *e == '1';
    if (const char* e = std::getenv("NF_UNLOCK_MULTIPLAYER"); e && *e) nf::unlock_multiplayer = *e == '1';
    if (const char* e = std::getenv("NF_UNLOCK_REWARDS"); e && *e) nf::unlock_rewards = *e == '1';
    if (const char* e = std::getenv("NF_OWN_VIEW"); e && *e) nf::own_view = *e == '1';
    if (const char* e = std::getenv("NF_MODERN_BUTTONS"); e && *e) nf::modern_buttons = *e == '1';
    // every achievement's condition parses (a typo would leave it never met)
    for (const achievements::Def& d : app::kGame.achievements)
        if (std::string e = achievements::Check(d.condition); !e.empty())
            LOGE("nightfire: achievement %s: %s", d.id, e.c_str());
    if (nf::skip_intro) LOGR("nightfire: the intro logos are skipped (skip_intro)");
    if (nf::hide_hud) LOGR("nightfire: the HUD is hidden (hide_hud)");
    if (nf::unlock_missions) LOGR("nightfire: every mission can be played (unlock_missions)");
    if (nf::unlock_multiplayer) LOGR("nightfire: all of multiplayer can be picked (unlock_multiplayer)");
    if (nf::unlock_rewards) LOGR("nightfire: every medal's reward is unlocked (unlock_rewards)");
    if (!nf::modern_buttons) LOGR("nightfire: the console's button pictures (modern_buttons off)");
}

// GameInfo::window: the game window is up (after --prepare-code on Windows), before kernel::Boot
// the window closing (Quit Game, the close button, Alt+F4): a session's other machines play on without this one
bool NetMessage(void*, uint32_t msg, uintptr_t, intptr_t, intptr_t*) {
    constexpr uint32_t kClose = 0x0010;  // WM_CLOSE
    if (msg == kClose) net::Leave();
    return false;
}

void NetWindow(void* hwnd) {
    net::Init(hwnd);  // online play: the machines meet before boot, or the game boots as it does without a session
}

// skip_intro: default.xbe's EA (07370048) and MGM (07380048) logo movies reported missing, which the game passes
// over; the language select's background (07350048) and the story's movies stay. A session skips them on every
// machine whatever its setting: one still in them is behind the others at the title
bool HideFile(const std::string& path) {
    if (!nf::skip_intro && !net::Active()) return false;
    std::string n;
    for (char c : path) n += char(std::tolower(static_cast<unsigned char>(c)));
    for (const char* logo : {"\\25_fps\\07370048.xmv", "\\25_fps\\07380048.xmv",   // PAL
                             "\\30_fps\\07370048.xmv", "\\30_fps\\07380048.xmv"})  // USA
        if (n.size() >= std::char_traits<char>::length(logo) &&
            n.compare(n.size() - std::char_traits<char>::length(logo), std::string::npos, logo) == 0)
            return true;
    return false;
}

// keyboard and mouse (Windows virtual key codes: this file builds for the headless harness too)
constexpr uint16_t kMouseLeft = 0x01, kMouseRight = 0x02, kMouseMiddle = 0x04, kTab = 0x09, kEnter = 0x0D,
                   kEscape = 0x1B, kSpace = 0x20, kArrowLeft = 0x25, kArrowUp = 0x26, kArrowRight = 0x27, kArrowDown = 0x28,
                   kLeftShift = 0xA0, kLeftCtrl = 0xA2, kBackspace = 0x08;
// on foot (default.xbe): a PC shooter's layout for the game's NightFire controller style (the keyboard plays as it:
// src/game/controls.cpp), as its controls page lists it: A crouch, X reload/action, Y jump, B next weapon, White alt.
// fire, Black next gadget, L manual aim, R fire, Back vision mode, the d-pad's left and right cycle the weapon (the
// mouse wheel too: src/common/menu_mouse.cpp), up and down the gadget and the zoom; the mouse looks and aims
std::array<uint16_t, 2> ShooterKeys(int c) {
    using namespace input;
    switch (c) {
        case kLSUp: return {'W', 0};
        case kLSDown: return {'S', 0};
        case kLSLeft: return {'A', 0};
        case kLSRight: return {'D', 0};
        case kRT: return {kMouseLeft, 0};    // fire
        case kLT: return {kMouseRight, 0};   // manual aim
        case kA: return {'C', kLeftCtrl};    // crouch (Enter selects in the menus: menu_mouse.cpp)
        case kX: return {'E', 'R'};          // action, reload
        case kY: return {kSpace, 0};         // jump
        case kB: return {'Q', kBackspace};   // next weapon; back in the menus
        case kWhite: return {kMouseMiddle, 'F'};  // alt. fire
        case kBlack: return {'G', '2'};      // next gadget
        case kBack: return {'V', kTab};      // vision mode
        case kUp: return {kArrowUp, 0};      // the d-pad: gadget and zoom up and down, the weapon left and right
        case kDown: return {kArrowDown, 0};
        case kLeft: return {kArrowLeft, '3'};
        case kRight: return {kArrowRight, '1'};  // 1 the next weapon, 2 the next gadget (Black)
        case kStart: return {kEscape, 'P'};
        default: return {0, 0};
    }
}
// driving (Driving.xbe), the car's controls page (Driving Config): R gas, L brake/reverse, the left stick steers, A
// fire, B handbrake, Y camera, X gadget, the d-pad's left and right and Black/White cycle the weapon (the mouse wheel
// too), the right stick's click looks back. On a mounted gun (Paris, Alpine Escape) the mouse aims, its buttons fire
// and zoom and R reloads instead (src/common/menu_mouse.cpp, src/driving/mouse.cpp)
std::array<uint16_t, 2> DrivingKeys(int c) {
    using namespace input;
    switch (c) {
        case kRT: return {'W', 0};                   // gas
        case kLT: return {'S', 0};                   // brake, reverse
        case kLSLeft: return {'A', kArrowLeft};      // steer
        case kLSRight: return {'D', kArrowRight};
        case kLSUp: return {kArrowUp, kLeftShift};   // pitch (Deep Descent's dive) and the menus
        case kLSDown: return {kArrowDown, kLeftCtrl};
        case kA: return {kMouseLeft, kEnter};        // fire; select in the menus
        case kB: return {kSpace, kBackspace};        // handbrake; back in the menus
        case kX: return {kMouseRight, 'E'};          // the gadget
        case kY: return {'C', 0};                    // the camera
        case kBlack: return {'Q', 0};                // the next weapon
        case kWhite: return {'Z', 0};                // the weapon before
        case kRThumb: return {'V', kMouseMiddle};    // look back
        case kLeft: return {'1', 0};                 // the d-pad: the weapon
        case kRight: return {'2', 0};
        case kUp: return {'3', 0};
        case kDown: return {'4', 0};
        case kStart: return {kEscape, 'P'};
        case kBack: return {kTab, 0};
        default: return {0, 0};
    }
}
std::array<uint16_t, 2> Keys(int c) { return nf::kDriving ? DrivingKeys(c) : ShooterKeys(c); }

#ifdef _WIN32  // the settings file (the headless harness has none)
// One settings file for both exes, each with its own keys: the runtime reads and writes [keys] as this exe's (only
// what differs from its defaults); Driving.xbe's are kept as [driving_keys] instead, and each exe writes the other's
// table back as it read it (EoN's way)
constexpr const char* kOtherKeys[2] = {"driving_keys", "keys"};  // by kDriving: the table this exe keeps unread
toml::table g_other_keys;

void LoadKeys(const void* table) {
    const toml::table& t = *static_cast<const toml::table*>(table);
    const toml::table* other = t[kOtherKeys[nf::kDriving]].as_table();
    g_other_keys = other ? *other : toml::table{};
    // the online port's default was 26555 until 2026-11-12: a file saved before then holds it, the player's or not
    if (!t["net_port_26565"].value_or(false) && net::port == 26555) net::port = 26565;
    // net_public (List my game on the internet) became net_who's Anyone on 2026-11-16
    if (t["net_public"].value_or(false) && !t.contains("net_who")) net::who = net::kWhoAnyone;
    // net_host_relay (Host through the relay) became net_host_by's join code on 2026-11-18
    if (t["net_host_relay"].value_or(false) && !t.contains("net_host_by")) net::host_by = net::kHostByRelay;
    if (!nf::kDriving) return;
    // the runtime took [keys], default.xbe's, as these: the driving defaults and [driving_keys] instead
    app::Settings& s = app::Get();
    for (int c = 0; c < input::kControls && c < int(s.keys.size()); c++) s.keys[c] = DrivingKeys(c);
    if (const toml::table* mine = t["driving_keys"].as_table())
        for (auto& [name, v] : *mine) {
            int c = input::ControlByName(std::string(name.str()).c_str());
            const toml::array* a = v.as_array();
            if (c < 0 || c >= int(s.keys.size()) || !a) continue;
            s.keys[c] = {0, 0};
            for (size_t i = 0; i < 2 && i < a->size(); i++) s.keys[c][i] = app::KeyFromName((*a)[i].value_or(std::string()));
        }
}

void SaveKeys(void* table) {
    toml::table& t = *static_cast<toml::table*>(table);
    t.insert_or_assign("net_port_26565", true);
    if (nf::kDriving) {
        // the runtime wrote this exe's as [keys]
        if (toml::table* mine = t["keys"].as_table()) {
            toml::table copy = *mine;
            t.erase("keys");
            t.insert("driving_keys", copy);
        }
    }
    if (!g_other_keys.empty()) t.insert_or_assign(kOtherKeys[nf::kDriving], g_other_keys);
}
#else
constexpr void (*LoadKeys)(const void*) = nullptr;
constexpr void (*SaveKeys)(void*) = nullptr;
#endif
}  // namespace

// GameInfo::texture_bound: the fonts (src/common/fonts.cpp), the button pictures (src/common/buttons.cpp)
bool TextureBound(uint32_t header, uint32_t palette) {
    nf::FontPicture(header);
    return nf::ButtonPictures(header, palette);
}

// Driving.xbe reads its pause (START, event 0x47 at 0xE02B0) and its menus from player 1's controller alone. One
// game runs for every player there, so its pause is everyone's: in a session any player's START is player 1's, and
// while a menu is up every player's d-pad, Back, A, B and left stick play it (the session's pads at this tick, the
// same on every machine)
static void SharedMenus(input::Pad& p) {
    constexpr uint16_t kStartBit = 0x10, kMenuBits = 0x0F | 0x20;  // the d-pad, Back
    bool menu = nf::MenuUp();
    for (int player = 1; player < net::Players(); ++player) {
        input::Pad o;
        if (!net::PadFor(player, o) || !o.connected) continue;
        p.buttons |= o.buttons & kStartBit;
        if (!menu) continue;
        p.buttons |= o.buttons & kMenuBits;
        p.analog[0] = std::max(p.analog[0], o.analog[0]);
        p.analog[1] = std::max(p.analog[1], o.analog[1]);
        if (std::abs(o.lx) > std::abs(p.lx)) p.lx = o.lx;
        if (std::abs(o.ly) > std::abs(p.ly)) p.ly = o.ly;
    }
}

// the other players' shots on a mounted gun copy player 1's last one (src/driving/coop.cpp): till player 1 has fired
// one, another player's trigger is player 1's too, so the first shot comes from either
static void FirstShot(input::Pad& p) {
    if (!nf::GunWaitsForShot()) return;
    for (int player = 1; player < net::Players(); ++player) {
        input::Pad o;
        if (!net::PadFor(player, o) || !o.connected) continue;
        p.analog[7] = std::max(p.analog[7], o.analog[7]);
    }
}

static void DrivingSeat(int port, input::Pad& p);

static std::atomic<bool> g_skip_held{false};
bool nf::SkipHeld() { return g_skip_held.load(); }

// GameInfo::pad: the local player's mouse in the menus (nf::MenuMouse), then a session's pads
// (src/common/net.cpp). With no session this is nf::MenuMouse as it was
void NetPad(int port, input::Pad& p) {
    if (!mem::Base()) return;
    // a session's player is this machine's controller 1: any of its controllers (2026-11-14: a Flydigi pad SDL lists
    // twice played from its second, and player 1 was locked out)
    if (port == 0) input::MergeControllers(net::Active() || net::LeftSeat() >= 0);
    if (port == 0)
        g_skip_held = p.analog[0] >= 128 || p.analog[1] >= 128 || (p.buttons & (1u << input::kStart));  // before a
                                                                                                    // session's pad
    if (!net::Active()) {
        // a machine that left its session for the menus (QUIT on the pause screen) is still in the level: its
        // controller stays on its seat and the others' are connected and idle, or the game stops to ask for them
        if (int seat = net::LeftSeat(); seat >= 0 && !net::OnGameThread()) {
            if (port < net::LeftSeats()) p.connected = true;  // the runtime's device thread: the ports plugged in
            return;
        }
        if (int seat = net::LeftSeat(); seat >= 0) {
            static input::Pad mine;
            // default.xbe's first menu page after the level (the mission list, a match's debriefing): the
            // controller is player 1's again (not the pause, 0x4000004B, nor the reconnect page, 0x4000002F)
            uint32_t page = nf::kDriving ? 0 : kernel::R32(0x25F1D0 + 0x1B8);
            if (port == 0 && page && page != 0x4000004B && page != 0x4000002F) {
                LOGR("net: back in the menus (page %08X): this machine's controller is player 1's", page);
                net::EndLeftSeats();
                nf::MenuMouse(port, p);
                return;
            }
            if (port == 0) {
                nf::MenuMouse(port, p);
                mine = p;
            }
            if (port < net::LeftSeats()) {
                if (port == seat) p = mine;
                else p = input::Pad{}, p.connected = true;
            }
            return;
        }
        nf::MenuMouse(port, p);
        return;
    }
    if (net::OnGameThread()) {
        if (port == 0) {
#ifdef _WIN32
            // when testing two instances on one PC (--instance N), an unfocused window sends an idle pad
            if (HWND w = static_cast<HWND>(app::Window()); app::Instance() > 0 && w && GetForegroundWindow() != w) {
                bool wheel = p.wheel;
                p = input::Pad{};
                p.connected = true;
                p.wheel = wheel;
            } else
#endif
            {
                nf::MenuMouse(port, p);  // this machine's player's pad, live; net::Pad captures what it made
            }
        }
        net::Pad(port, p);
        if (nf::kDriving) DrivingSeat(port, p);
    } else {
        net::BackgroundPad(port, p);  // the runtime's device thread only reads what the game is being given
    }
}

// Driving.xbe plays one controller, on the port its launch data names (nf::PlayerPort: the one that started the
// mission in default.xbe, any player's in a session). Seat 1's input goes there, the other ports idle: the others play
// through the co-op code (net::PadFor), and a port of theirs the game read would be its player alone (2026-11-14:
// player 2's port named, player 1 could not fire or pause)
static std::atomic<int64_t> g_took_car{0};  // when this machine's player took the car (steady clock ms), for Ui

static int64_t SteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static void DrivingSeat(int port, input::Pad& p) {
    static input::Pad seat;
    int game_port = nf::PlayerPort();
    static int said_port = 0;
    if (game_port != said_port) {
        said_port = game_port;
        LOGR("driving: the game plays controller port %d (its launch data's): seat 1's input goes there", game_port + 1);
    }
    if (port == 0) {
        if (int driver = net::Driver()) {
            static bool said = false;
            if (!said) {
                said = true;
                LOGR("driving: player 1 is gone: player %d takes the car, the pause and the gun", driver + 1);
                if (driver == net::LocalPlayer()) g_took_car = SteadyMs();
            }
            net::PadFor(driver, p);
            p.connected = true;
        }
        // a guided torpedo another player fired: theirs to steer and set off, START and BACK still seat 1's
        if (int guide = nf::GuidedBy(); guide > 0 && guide != net::Driver()) {
            input::Pad g;
            if (net::PadFor(guide, g)) {
                constexpr uint16_t kSeatOwn = 0x10 | 0x20;
                g.buttons = uint16_t((g.buttons & ~kSeatOwn) | (p.buttons & kSeatOwn));
                g.connected = true;
                p = g;
            }
        }
        SharedMenus(p), FirstShot(p);
        // each START the game is given, whose it was (a player whose pause does not open shows)
        static bool was = false;
        static int said = 0;
        bool start = p.buttons & 0x10;
        if (start && !was && said < 12) {
            said++;
            std::string who;
            for (int player = 0; player < net::Players(); ++player) {
                input::Pad o;
                if (net::PadFor(player, o) && (o.buttons & 0x10)) who += (who.empty() ? "" : " ") + std::to_string(player + 1);
            }
            LOGR("driving: START at tick %u from player %s, %s", net::Tick(), who.empty() ? "?" : who.c_str(),
                 nf::MenuUp() ? "a menu up" : "in play");
        }
        was = start;
        seat = p;
    }
    if (port == game_port) {
        p = seat;
    } else {
        p = input::Pad{};
        p.connected = true;
    }
}

// GameInfo::rumble: a session player's events play on their own machine's device (the game sends each player's to
// their controller port, and this machine's device plays controller 1's)
void NetRumble(int port, uint16_t& left, uint16_t& right) {
    if (!net::Active()) return;
    if (nf::kDriving) {  // the game's one port is seat 1's (DrivingSeat): its driver's device
        if (port == nf::PlayerPort() && net::Driver() == net::LocalPlayer()) port = 0;
        else left = right = 0;
        return;
    }
    if (port == net::LocalPlayer()) port = 0;
    else left = right = 0;  // the other players' devices are on their machines
}

// GameInfo::ui: the co-op reticles on a mounted gun (nf::Reticles), over the frame where the renderers
// place it (centred, app::FrameAspect's shape); the harness draws no interface
#if __has_include(<imgui.h>)
// a driving mission's notice to the player whose machine takes the car from player 1, for a few seconds
static void TookCarNotice(ImDrawList* dl, ImVec2 size, float vh) {
    constexpr int64_t kShowMs = 6000;
    int64_t at = g_took_car.load(), age = at ? SteadyMs() - at : kShowMs;
    if (age >= kShowMs) return;
    const char* line1 = "Player 1 has left";
    const char* line2 = "You are driving now";
    float fade = std::min(1.0f, float(kShowMs - age) / 600.0f);
    ImFont* font = ImGui::GetFont();
    float big = vh * 0.05f, small = vh * 0.035f;
    ImVec2 s1 = font->CalcTextSizeA(big, FLT_MAX, 0, line1), s2 = font->CalcTextSizeA(small, FLT_MAX, 0, line2);
    float w = std::max(s1.x, s2.x) + big * 1.6f, h = s1.y + s2.y + big * 0.9f, top = size.y * 0.22f;
    ImVec2 a{size.x / 2 - w / 2, top}, b{size.x / 2 + w / 2, top + h};
    dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, int(170 * fade)), big * 0.25f);
    float y = top + big * 0.45f;
    dl->AddText(font, big, {size.x / 2 - s1.x / 2, y}, IM_COL32(230, 205, 90, int(255 * fade)), line1);
    dl->AddText(font, small, {size.x / 2 - s2.x / 2, y + s1.y}, IM_COL32(235, 235, 235, int(255 * fade)), line2);
}
#endif

void Ui(double) {
#if __has_include(<imgui.h>)
    ImVec2 size = ImGui::GetIO().DisplaySize;
    float aspect = app::FrameAspect(), vw = std::min(size.x, size.y * aspect), vh = vw / aspect;
    if (nf::kDriving) TookCarNotice(ImGui::GetForegroundDrawList(), size, vh);
    nf::Mark marks[16];
    int n = nf::Reticles(marks, 16);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    // the others' smaller and fainter, under this machine's own
    for (int mine = 0; mine < 2; mine++)
        for (int i = 0; i < n; i++) {
            const nf::Mark& m = marks[i];
            if (m.mine != bool(mine)) continue;
            ImVec2 c{size.x / 2 + m.x * vw / 2, size.y / 2 - m.y * vh / 2};
            float r = vh * (mine ? 0.022f : 0.016f), t = std::max(1.5f, vh / 360), gap = r * 0.45f, arm = r * 1.6f;
            uint32_t shadow = IM_COL32(0, 0, 0, mine ? 140 : 90);
            uint32_t colour = mine ? m.colour : (m.colour & 0x00FFFFFFu) | 0xB0000000u;
            for (int pass = 0; pass < 2; pass++) {
                uint32_t col = pass ? colour : shadow;
                float w = pass ? t : t + 2;
                dl->AddCircle(c, r, col, 32, w);
                dl->AddLine({c.x - arm, c.y}, {c.x - gap, c.y}, col, w);
                dl->AddLine({c.x + gap, c.y}, {c.x + arm, c.y}, col, w);
                dl->AddLine({c.x, c.y - arm}, {c.x, c.y - gap}, col, w);
                dl->AddLine({c.x, c.y + gap}, {c.x, c.y + arm}, col, w);
            }
        }
#endif
}

// the bot settings' rows (net::bot_setting): each greyed while another decides it
app::Option BotRow(const char* key, uint32_t* value, const char* label, const char* help, uint32_t count,
                   const char* (*name)(uint32_t), bool (*enabled)(), const char* section = nullptr, bool folded = false) {
    app::Option o{key, nullptr, value, label, help, count, name, section};
    o.folded = folded;
    o.enabled = enabled;
    return o;
}
bool NoBotCharacters() {
    for (int i = 0; i < 8; i++)
        if (net::bot_setting[net::kBotChar + i]) return false;
    return true;
}
bool NoEveryBotCharacter() { return net::bot_setting[net::kBotEvery] == 0; }
bool NoBotFields() {
    for (int i = net::kBotField; i < net::kBotSettings; i++)
        if (net::bot_setting[i]) return false;
    return true;
}
// Bot 7's or 8's fields: not while every bot has Bot 1's skill, nor for a character whose statistics are fixed (15 on)
template <int Bot>
bool BotFieldsFree() {
    if (net::bot_setting[net::kBotSkill]) return false;
    uint32_t c = net::bot_setting[net::kBotEvery] ? net::bot_setting[net::kBotEvery] : net::bot_setting[net::kBotChar + Bot];
    return c == 0 || c == net::kBotRandom || c <= 15;
}
template <int Field>
const char* BotFieldName(uint32_t choice) { return net::BotFieldChoiceName(Field, choice); }

#define BOT_CHAR(n)                                                                                                    \
    BotRow("net_bot_char_" #n, &net::bot_setting[net::kBotChar + n - 1], "Bot " #n " character",                       \
           "The game's bot menu's Bot " #n " in an online match: the menu shows this character, and one picked there comes back here. Default: the menu's own. Random: picked again each match. Greyed while Every bot's character is set.", \
           net::kBotCharChoices, net::BotCharChoiceName, NoEveryBotCharacter)
// Bots 7 and 8 (past the menu's six: Bot like, 1 or 2, again by default)
#define BOT_LOW(n, like)                                                                                               \
    BotRow("net_bot_char_" #n, &net::bot_setting[net::kBotChar + n - 1], "Bot " #n " character",                       \
           "Bot " #n " is past the game's bot menu's six. Default: it plays Bot " #like " again, character and fields as the menu has them. Random: picked again each match. Greyed while Every bot's character is set.", \
           net::kBotCharChoices, net::BotCharChoiceName, NoEveryBotCharacter, "Bot " #n, true),                       \
        BOT_FIELD(n, like, 0, "speed"), BOT_FIELD(n, like, 1, "personality"), BOT_FIELD(n, like, 2, "aggression"),     \
        BOT_FIELD(n, like, 3, "accuracy"), BOT_FIELD(n, like, 4, "health"), BOT_FIELD(n, like, 5, "reaction"),         \
        BOT_FIELD(n, like, 6, "recovery")
#define BOT_FIELD(n, like, f, key)                                                                                     \
    BotRow("net_bot" #n "_" key, &net::bot_setting[net::kBotField + (n - 7) * int(net::kBotFieldCount) + f],          \
           net::kBotFields[f].label,                                                                                   \
           "As the game's bot menu has it for its six. Default: the character's own when Bot " #n " has one here, else Bot " #like "'s. Greyed while Every bot's skill is Bot 1's, or for a character whose statistics are fixed.", \
           net::kBotFields[f].count + 1, BotFieldName<f>, BotFieldsFree<n - 1>)

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"007: NightFire",          // window and splash
    .short_title = L"NightFire",          // message boxes
    .name = L"Nightfire",                 // window classes, crash dumps, handover event
    .log = "nightfire",                   // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe and Driving.xbe with their folders beside them.",
    .images = {{"default.xbe", "Nightfire"}, {"driving.xbe", "NightfireDriving"}},
                                          // each recompiled xbe and its exe, for title launches
    .options = {{"skip_intro", &nf::skip_intro, nullptr, "Skip intro movies (needs a restart)",
                 "On by default: starts without the EA and MGM logo movies. Online play always skips them, so every PC reaches the title together."},
                {"skip_tutorial", &nf::skip_tutorial, nullptr, "Skip first-boot Paris tutorial",
                 "On by default: pressing Start on a fresh install goes straight to the Main Menu instead of forcing the Paris tutorial mission. Online play always skips it."},
                {"hide_hud", &nf::hide_hud, nullptr, "Hide the HUD", "For screenshots; at once, on foot and driving. The menus and pause screens stay."},
                {"unlock_missions", &nf::unlock_missions, nullptr, "Unlock every mission (greyed ones can be picked)",
                 "At once: every mission can be picked, whatever the codename has finished. The ones not reached yet stay greyed out in the mission list, but select them and they play. The save is not changed (off again, it is as it was). Online, the host's setting as the session started is everyone's."},
                {"unlock_multiplayer", &nf::unlock_multiplayer, nullptr, "Unlock all of multiplayer (greyed ones can be picked)",
                 "At once: every character (the bots' too), map and scenario of multiplayer can be picked, whatever the codename has won. The locked ones may stay greyed out in the lists, but select them and they play. The missions are left as they are (Unlock every mission does both), and the save is not changed. Online, the host's setting as the session started is everyone's."},
                {"unlock_rewards", &nf::unlock_rewards, nullptr, "Unlock every reward (what the cheat codes give)",
                 "Every reward the missions' medals give, as the codename cheats (PASSPORT, Q LAB and the others) unlock them: the gadgets' upgrades, the bonus characters, maps and modes. The gadgets are upgraded as a codename loads (the next level or menu after turning it on). The save is not changed (off again, it is as earned). Online, the host's setting as the session started is everyone's."},
                {"modern_buttons", &nf::modern_buttons, nullptr, "Modern button pictures",
                 "A controller's prompts as today's Xbox pad marks them: LB and RB for White and Black, Menu and View for Start and Back."},
                {"fov_percent", nullptr, &nf::fov_percent, "Field of view (%)",
                 "50 to 150; 100 is the game's. On foot, driving and in the engine's cutscenes; the scope zooms in proportion."},
                {"crosshair_color", nullptr, &nf::crosshair_color, "Crosshair color",
                 "The on-foot crosshair's color, in every mission, co-op and match; applies at once. Online, each player's own.",
                 nf::kCrosshairColors, nf::CrosshairColorName},
                {"crosshair_size", nullptr, &nf::crosshair_size, "Crosshair size (%)",
                 "25 to 400; 100 is the game's. The on-foot crosshair, in every mission, co-op and match; applies at once. Online, each player's own."},
                {"net_host", &net::host, nullptr, "Host online play (needs a restart)",
                 "Before boot: waits for joiners on the UDP port, then every machine boots into a shared match. The rows under it are the host's, whichever way players reach it.",
                 0, nullptr, "Hosting"},
                {"net_host_by", nullptr, &net::host_by, "How players reach me (needs a restart)",
                 "My IP address: players connect straight to this PC, the quickest way; a listed game shows everyone your IP address, and players on the internet need the UDP port forwarded on your router. A join code through a relay: the connection screen shows a code, NF- and six letters, instead of this PC's address, and players anywhere join with it; no player sees another's IP address and no port needs forwarding, but every packet goes through the relay server (under Servers), which adds its distance to the game's delay. Players on this network can join by the address either way. Players, Who can join and the password below work the same with both.",
                 net::kHostByChoices, net::HostByChoiceName},
                {"net_players", nullptr, &net::players, "Players, 2 to 10 (needs a restart)",
                 "When hosting: the game starts as soon as this many players are in (2 by default, up to 10 using the 6 bot slots); press Enter or Start in the host window to start earlier with fewer. A match has the players who are in, and the bots the game's bot menu (or Bots in a match below) asks for, up to 10 in all."},
                {"net_who", nullptr, &net::who, "Who can join (needs a restart)",
                 "When hosting. Not listed: no Server Browser's internet tab has it; this network's Server Browsers see it, and anyone you give the address or join code joins. Anyone: it is in every player's Server Browser too (the game lists under Servers carry it), and anyone can join it from there. Only people I tell: it is listed with a lock; by a join code, the code is withheld and players type the one you give them (the connection screen copies it), by your IP address they type the password below. A listed game waits for its players with no time limit.",
                 net::kWhoChoices, net::WhoChoiceName},
                {"net_password", nullptr, nullptr, "Password for my game",
                 "Only people I tell, reached by my IP address: players must give this password to join (the Server Browser asks for it, and so does their connection screen). It keeps strangers out; it travels unencrypted, so use one of its own, not a real one. A change applies to the next player who asks.",
                 0, nullptr, nullptr, &net::password, "none", nullptr, nullptr, false, true, net::AsksPassword},
                {"net_quit_together", &net::quit_together, nullptr, "Everyone quits with the host",
                 "On by default: QUIT on the host's pause screen ends the mission or match for every player, and everyone goes to the menus together, still in the session, for the host to pick what is next. Off: the host leaves alone, as any other player's QUIT does, and the others play on. Applies at once."},
                {"net_restart_everyone", nullptr, nullptr, "Restart the session for everyone",
                 "Host only, during a session: for one that is stuck. Every PC in it restarts online play from power-on and meets this host again (the same join code), and the game starts once all of them are back. Progress since the last save is lost.",
                 0, nullptr, nullptr, nullptr, nullptr, net::RestartEveryone, nullptr, false, false, nullptr,
                 net::CanRestartEveryone, "Restart"},
                {"net_join", &net::join_setting, nullptr, "Join online play (needs a restart)",
                 "Before boot: connects to the host at the address or join code below and boots with its session. It goes off by itself once that session is over (or did not come), so the next start plays alone; the Server Browser's Connect turns it on again.",
                 0, nullptr, "Joining"},
                {"net_master", nullptr, nullptr, "Server Browser's game list",
                 "Where every player's Server Browser gets its internet games, and where a host whose Who can join lists it puts its game: everyone on the same lists sees the same games. It lists games only; the match itself goes straight between the players, or through a relay. Empty: the game's own lists, and those they name (each list says which others there are, and the game keeps that in servers.txt for its next start, so one going away leaves the others). An address here (tools/master in the project: anyone can run one, on a VPS or Cloudflare) is used as well; several are separated by commas; none: this network's games alone, and no game of yours is listed.",
                 0, nullptr, "Servers (hosting and joining both use them)", &net::master_url, "the game's own"},
                {"net_relay", nullptr, nullptr, "Relay server (hides IP addresses)",
                 "Used by a host players reach by a join code, and by a player joining one; it carries the match's packets and lists no games (the game list above does). Empty: the game's own relays, and those the game lists name. A host through one gets a join code, NF- and six letters, and players type the code as the host's address: no player sees another's IP address, and no port needs forwarding. Every packet goes through the server, so it adds its distance to the game's delay. A host:port here (tools/master in the project: master.py --relay on a VPS; the port is 26581 unless it says otherwise) is tried first; several are separated by commas; none: no relay at all. Greyed while hosting by my IP address.",
                 0, nullptr, nullptr, &net::relay_server, "the game's own", nullptr, nullptr, false, false, nullptr, net::RelayUsed},
                {"net_direct", &net::direct, nullptr, "Connect directly when possible (needs a restart)",
                 "On by default. A game by join code starts through the relay, then the host and each joiner try to reach each other directly; when both have this on and their routers let it through, the match's packets go straight between them (no relay distance added), and back through the relay if that stops. Each PC then knows the other's IP address, though it is never shown. Off: this PC's packets always go through the relay, and no other player learns its address."},
                {"net_port", nullptr, &net::port, "UDP port (needs a restart)",
                 "The UDP port a host listens on and a joiner asks on; 26565 by default. Forward it on the host's router when players on the internet reach it by its IP address.",
                 0, nullptr, "Advanced", nullptr, nullptr, nullptr, nullptr, true},
                {"net_upnp", &net::upnp, nullptr, "Open my port on the router (UPnP) (needs a restart)",
                 "On by default: a host players reach by its IP address asks the router (UPnP) to forward the UDP port above to this PC while it hosts, so players on the internet can reach it. The log says whether the router did. Routers with UPnP switched off need the port forwarded by hand. Not used through a relay."},
                {"net_wait", nullptr, &net::wait, "Wait in seconds (needs a restart)",
                 "How long a host waits for joiners (and a joiner tries to connect) before the game boots without a session; 0: no limit (ESC boots offline). A listed host waits with no limit whatever this says."},
                {"net_input_delay", nullptr, &net::delay, "Input delay in ticks (needs a restart)",
                 "1..12; how many ticks every player's input is held back to absorb the network's round trip. The host's setting is everyone's."},
                {"net_coop_friendly_fire", &net::coop_friendly_fire, nullptr, "Friendly fire",
                 "On by default, as the game has it: in campaign co-op the players' bullets and blows hurt each other. Off: they do nothing to a teammate. Applies at once.",
                 0, nullptr, "Co-op (the host's settings are everyone's)"},
                {"net_coop_more_enemies", &net::coop_more_enemies, nullptr, "More enemies",
                 "On by default: in campaign co-op every enemy comes with one more beside it for each extra player (up to 3). Applies from the next level or restart."},
                {"net_coop_tougher_enemies", &net::coop_tougher_enemies, nullptr, "Tougher enemies",
                 "On by default: in campaign co-op the enemies' health is the game's times 1 + 0.5 per extra player. Applies to enemies set up from then on (the next level or restart for those already there)."},
                {"net_coop_fail", nullptr, &net::host_setting[net::kHostCoopFail], "The mission fails when",
                 "In campaign co-op. The last player down (the default): a player who goes down comes back beside a teammate a few seconds later, and the mission fails only when nobody is left standing. Player 1 down: the others still come back, but the mission fails when player 1 (the host) goes down. Anyone down: the first player down fails the mission for everyone. Applies at once.",
                 net::kCoopFailChoices, net::CoopFailChoiceName},
                {"net_coop_pickups", nullptr, &net::host_setting[net::kHostCoopPickups], "Pickups",
                 "In campaign co-op. Shared (the default): a pickup one player takes (ammunition, armor, a weapon) is gone for the others, as the game has it. Separate: each player can take every pickup once for themselves, so nobody has to leave them to a teammate. Applies at once.",
                 net::kCoopPickupsChoices, net::CoopPickupsChoiceName},
                {"net_name", nullptr, nullptr, "My name",
                 "Your name to the other players online, in co-op and matches (15 letters at most); empty shows Player 1, Player 2 and so on. A change reaches the others at once.",
                 0, nullptr, "You (this PC)", &net::my_name, "Player N", net::NameChanged},
                {"net_character", nullptr, &net::my_character, "My character",
                 "Your character model online, in multiplayer matches and in campaign co-op (Players 5 to 10 have no character box on the join screen). Applies at once, in matches and in campaign co-op.",
                 net::kCharacterChoices, net::CharacterChoiceName},
                {"net_handicap", nullptr, &net::my_handicap, "My health handicap",
                 "Your health at the start of an online match: 100 plus this. Join screen's uses your join box's Health Handicap (Players 5 to 10 have none: 100). Not in co-op. Applies at the next match.",
                 net::kHandicapChoices, net::HandicapChoiceName},
                {"net_default_names", &net::default_names, nullptr, "Streamer mode: hide typed names and IP addresses",
                 "On this PC, at once: the other players' typed names are drawn as Player 1, Player 2 and so on; the Server Browser lists each game as Game and a number of its own instead of its host's name (saved servers too); and no IP address shows, in the Server Browser, its boxes, the host's address row or the connection screen (typed ones as dots). A locked game's join code is hidden on its host's connection screen too; C still copies it."},
                {"net_others_reticles", &nf::others_reticles, nullptr, "Show the other players' reticles",
                 "On this PC, at once: on the driving missions' mounted guns in co-op (Paris's van, Alpine Escape's turret), each other player's reticle is drawn too, smaller and fainter than yours, in their color. Player 1 aims with the game's own crosshair."},
                {"net_own_view", &nf::own_view, nullptr, "My own view on the mounted guns",
                 "On by default: on the driving missions' mounted guns in co-op (Paris's van, Alpine Escape's turret), your view turns with your own aim instead of following player 1's, and your shots go where you look. The gun itself still turns with player 1. Off: you see player 1's view and aim with a reticle across it. On this PC, at once."},
                {"net_fullscreen", &net::fullscreen, nullptr, "Full-screen view per player",
                 "On by default: in an online match each PC draws only its own player's camera and HUD at full screen instead of split-screen. Applies fully at the next match or level."},
                BotRow("net_bots", &net::bots, "Bots in a match (needs a restart)",
                       "How many bots join the players in an online match, up to 10 in all (2 players and 8 bots, 3 and 7, ...). The bot menu's: as many as the game's own bot menu says, six at most. Fill to the Players count: bots take the places of the players who did not come, so a game started early with fewer players than Players above still has that many.",
                       net::kBotChoices, net::BotChoiceName, nullptr, "Bots (the host's settings are everyone's)"),
                BotRow("net_bot_character", &net::bot_setting[net::kBotEvery], "Every bot's character",
                       "Every bot in an online match plays this character (Random: each one its own, picked again each match), and the game's bot menu shows it. Default: each as Bot 1 to Bot 8 below say. Greyed while one of those has a character.",
                       net::kBotCharChoices, net::BotCharChoiceName, NoBotCharacters),
                BotRow("net_bot_skill", &net::bot_setting[net::kBotSkill], "Every bot's skill",
                       "Bot 1's: every bot plays with Bot 1's statistics from the game's bot menu (speed, aggression, accuracy, health, reaction and recovery; its personality too where it is on Bot 1's side), the bots whose statistics are fixed apart. Applies at the next match. Greyed while Bot 7 or Bot 8 has its own.",
                       2, net::BotSkillChoiceName, NoBotFields),
                BOT_CHAR(1), BOT_CHAR(2), BOT_CHAR(3), BOT_CHAR(4), BOT_CHAR(5), BOT_CHAR(6),
                BOT_LOW(7, 1), BOT_LOW(8, 2)},
    .load = LoadKeys,                     // Driving.xbe's keys apart (DrivingKeys)
    .save = SaveKeys,
    .init = Init,                         // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = NetWindow,                  // the game window is up: online sessions meet here before boot
    .message = NetMessage,                // window messages first; true takes one (a session's leaving)
    .ui = Ui,                             // ImGui drawing each frame: a co-op player's reticle
    .pad = NetPad,                        // the mouse in the menus, then a session's pads (src/common/net.cpp)
    .rumble = NetRumble,                 // a session player's events on their own machine's device
    .achievements = nf::AchievementSet(), // conditions on default.xbe's Results page (src/common/achievement_set.cpp)
    // the USA release (title 0x45410026, region North America): its default.xbe and Driving.xbe differ from the PAL
    // ones in the signature and certificate alone; its certificate makes the console NTSC (pal_console), where it
    // finds its own 30 fps movies and NTSC files. The Europe (En,Fr,De,Es,It,Nl,Sv) disc's are the PAL ones.
    .same_code = {"7e4b8f8a54bacc61a5897ba5245fdd65bddc0752", "8d46a023dfbd5c89d4ee04fe28b24a9620de546a"},
    .squeeze_screen_programs = true,      // ultrawide: Driving.xbe draws its HUD and menus through vertex programs
    .edge_bars = true,                    // ultrawide: the sniper scope's black side bars reach the screen's sides
    .default_volume = 50,                 // the game is loud at full level (2026-11-26, John)
    .sharpen_2d = true,                   // text and HUD pictures enlarged sharp (Settings::sharp_2d turns it off)
    .pal_console = true,                  // a PAL disc (region 4): the driving levels ask for NTSC\ files otherwise;
                                          // the USA disc's certificate (region 1) keeps an NTSC console
    .hide_file = HideFile,                // skip_intro
    .mouse_look = true,                   // the mouse looks and aims
    .mouse_direct = true,                 // the view and the mounted guns turn by it directly (src/game/mouse.cpp,
                                          // src/driving/mouse.cpp), not as the right stick
    .default_keys = Keys,                 // a shooter's keys on foot, a driving layout in Driving.xbe
    .texture_bound = TextureBound,        // the button pictures as the keys, the fonts with smooth edges
    .relaunching = net::Close,            // closes the session's UDP socket before the handover to the next exe
    .restarting = net::Restarting,        // F1's Restart online play: leaves the session, the new boot meets again
    .find_games = net::FindGames,         // F1's Server Browser: the games on this LAN and the internet list's
    .ask_games = net::AskGames,           // and at its favourites' and history's addresses
    .refresh_games = net::RefreshGames,
    .online_restart_after = "net_join",  // Restart online play closes Hosting and Joining
    .streaming = [] { return net::default_names; },  // net_default_names: no names or addresses in F1
    .display_mode = app::DisplayMode::Fullscreen,  // a new install: the cursor stays on the game's screen
    // optional, after these: .wheel (what a steering wheel's force feedback follows)
};
