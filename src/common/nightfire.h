#pragma once
// NightFire's own settings and the hooks the two executables share (default.xbe: title, menus and the on-foot
// levels; Driving.xbe: EA's front end and the driving levels)
#include <cstdint>

#include <vector>

#include "core/achievements.h"
#include "core/input.h"

struct X86Context;

namespace nf {
extern const bool kDriving;  // Driving.xbe's exe (src/driving/hooks.cpp), default.xbe's false (src/game/hooks.cpp)
extern bool skip_intro;
extern bool unlock_missions;  // default.xbe's every-mission flag (src/game/hooks.cpp)
extern bool unlock_multiplayer;  // the same flag for multiplayer's lists alone (src/game/hooks.cpp)
extern bool skip_tutorial;
extern bool hide_hud;  // no HUD, for screenshots (src/game/hud.cpp, src/driving/hud.cpp)
extern uint32_t fov_percent;  // the 3D views' field of view, percent of the game's (src/game/view.cpp, src/driving/view.cpp)
float Ultrawide();            // how much wider than 16:9 the frame is shown, 1 on a 16:9 or 4:3 screen
float FovScale();             // fov_percent as a factor, 0.5 to 1.5
// the on-foot crosshair (src/game/hud.cpp): a colour of kCrosshairColors (0 the game's own) and a size, percent
extern uint32_t crosshair_color, crosshair_size;
constexpr uint32_t kCrosshairColors = 10;
const char* CrosshairColorName(uint32_t index);
uint32_t CrosshairRgb(uint32_t index);  // 0xRRGGBB
// a menu has the controller (each exe's hooks.cpp): the mouse then plays A, B and the d-pad (MenuMouse)
void RegisterSnapshot();  // GameInfo::init: the title's snapshot sections (src/game/hooks.cpp; Driving.xbe none yet)
bool MenuUp();
// default.xbe in a session (src/game/pause.cpp): a level's pause is its player's own; SharedMenuUp a menu every
// player has (the reconnect-controller page, a level's end), MenuUpFor that or the player's own menu
bool OwnPause(int player);
int CoopCampaignPlayers();  // default.xbe: a co-op campaign session's players (src/game/view.cpp), else 0
void ReportHeap();          // default.xbe: a level's start, the game's heap free (src/game/heap.cpp)
void DoorHintFetched();
// default.xbe's bots in an online match as the host's bot settings have them (src/game/bots.cpp)
void BotsFrame(X86Context& ctx);                       // each frame: the host's changes in, its menu edits out
void BotsMatchStart(X86Context& ctx);                  // a match spawns its bots: the bot menu gets its characters
bool BotsName(X86Context& ctx, int bot, uint32_t dst);  // bot 0..7's character's name at dst, if its settings give one
void BotsEntry(int bot, uint32_t entry);               // bot 0..7's bot menu entry (a copy at entry) as the match plays it     // default.xbe: 0x6D460 fetched a door hint's text (src/game/view.cpp)
bool SharedMenuUp();
bool MenuUpFor(int player);
void ClosePauseLater(int player);
struct PausedUpdate {  // around a player's update (0xAD520)
    explicit PausedUpdate(uint32_t outer);
    ~PausedUpdate();
    int port = -1;
    uint8_t controls[0x158];
};
std::vector<achievements::Def> AchievementSet();  // src/common/achievement_set.cpp
void MenuMouse(int port, input::Pad& p);
void MouseAsStick();
bool PlayerFree();  // default.xbe: Bond under the player's own control (not a keypad, a missile, a cutscene)
bool Zoomed();
void ScopeZoom(int notches);  // default.xbe: the wheel's notches through a scope, up nearer (src/game/mouse.cpp)  // default.xbe: player 1 looking through a scope or sight, zoomed in (src/game/mouse.cpp)
int GunStyle();
uint32_t Level();  // Driving.xbe: the mission's level id (its launch data's, Deep Descent 0x09000002), 0 in default.xbe
int PlayerPort();  // Driving.xbe: the controller port the game plays (its launch data's), 0 in default.xbe
bool SkipHeld();   // this machine's controller 1 (any of its controllers, the keyboard, the mouse) holds A, B or START
// Driving.xbe's mounted guns in a session (src/driving/coop.cpp): the reticles to draw, x and y -1 to 1 across the
// frame (up positive), the colour ImGui's; this machine's player's (none for player 1) and, with others_reticles,
// the other players' but player 1's (the game draws that one). None in the car or default.xbe
struct Mark {
    float x, y;
    uint32_t colour;
    bool mine;
};
int Reticles(Mark* out, int max);
extern bool others_reticles;  // F1: the other players' reticles too
void ViewAngles(float tan_x, float tan_y);  // Driving.xbe: the view just built, its half angles' tangents
int GuidedBy();  // Driving.xbe in a session: the player steering the guided torpedo in flight (0: player 1, or none)
bool GunWaitsForShot();  // Driving.xbe: player 1 on a mounted gun, no shot of theirs yet for the others to copy
bool ButtonPictures(uint32_t header, uint32_t palette);  // GameInfo::texture_bound (src/common/buttons.cpp)
void RefreshButtons();  // each poll
void FontPicture(uint32_t header);  // GameInfo::texture_bound (src/common/fonts.cpp)
bool MenuKeys();  // the menu up shows Enter and Backspace for A and B (not a level's pause, whose controls page
                  // names the keys played)       // Driving.xbe on a mounted gun: its controller style (0-7); -1 in the car, on foot, in a menu  // default.xbe: after its pad reader (src/game/mouse.cpp)  // GameInfo::pad  // no EA and MGM logo movies at boot (HideFile)
}  // namespace nf
