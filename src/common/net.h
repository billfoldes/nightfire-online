#pragma once
// Online multiplayer over the split-screen game: lockstep of the pads and the mouse. Every machine runs the same
// match from a common boot, so no game state is ever sent: each tick the machines exchange only what their player
// did (pad, mouse movement, scope zoom), and each machine's own copy of the game applies every player's input at
// the same tick. The session starts before boot (GameInfo::window): the machines boot together from power-on, the
// one state they all share - and re-meet at each relaunch between the two exes, the multiplayer's path running
// through Driving.xbe's front end. Both exes build this file and start sessions.
#include <cstdint>
#include <string>
#include <vector>

#include "core/input.h"
#include "core/snapshot.h"

namespace app {
struct FoundGame;
}

namespace nf::net {
constexpr int kMaxPlayers = 10;  // 4 native controller slots (0..3) + 6 bot slots (4..9) turned into human players
constexpr uint32_t kCharacterChoices = 30;  // 0 = Default (Menu), 1..29 = characters 0..28 in 0x1637C0

extern bool host;            // net_host: wait for joiners before boot, play as controller 1
extern bool join;            // this machine joins a session: connects to join_ip before boot, plays the controller it is given
extern bool join_setting;    // net_join, which Init makes join; it goes off once the session it joined is over
extern bool fullscreen;      // net_fullscreen: each machine draws only its own player's view at full screen
extern uint32_t players;     // net_players: how many players a host waits for before starting (2..10, default 2)
extern uint32_t port;        // net_port: the UDP port a host listens on
extern uint32_t wait;        // net_wait: seconds a host waits for joiners (and a joiner tries to connect)
extern uint32_t delay;       // net_input_delay: ticks input is held before the game gets it, 1..12, the host's for all
extern std::string my_name;                   // net_name: the name over this player's head in co-op ("" = Player N)
extern std::string master_url;                // net_master: internet game lists added to the built-in ones ("none" = LAN alone)
// net_who, who can join a host's game: not listed (this network's browsers see it, anyone with the address or code
// joins); anyone (on the internet list too); only people I tell (listed locked: through the relay its join code is
// withheld and asked for, else net_password is)
constexpr uint32_t kWhoNotListed = 0, kWhoAnyone = 1, kWhoInvited = 2, kWhoChoices = 3;
extern uint32_t who;
const char* WhoChoiceName(uint32_t choice);
bool AsksPassword();                          // net_who and the relay settings leave net_password the key
extern std::string relay_server;              // net_relay: relays' host:port tried before the built-in ones ("none" = none)
// net_host_by, how players reach a host: straight at its IP address, or by a join code through a relay
constexpr uint32_t kHostByAddress = 0, kHostByRelay = 1, kHostByChoices = 2;
extern uint32_t host_by;
const char* HostByChoiceName(uint32_t choice);
bool RelayUsed();                             // the relay settings matter: not while hosting by the IP address
extern bool direct;                           // net_direct: through a relay, the host and a joiner that both have it
                                              // on send their packets straight to each other once their routers let them
extern bool upnp;                             // net_upnp: a host by its IP address asks the router to forward its port
extern std::string password;                  // net_password: Only people I tell's key when not through the relay
void NameChanged();                           // net_name changed: the session's players get it within kNameLen ticks
const char* PlayerName(int player);           // that player's net_name in the session now, "" for none
extern bool default_names;                    // net_default_names: this PC draws Player N for the typed names
extern uint32_t my_character;                 // net_character: this player's chosen character (0 = Default, 1..29)
constexpr uint32_t kHandicapChoices = 9;      // 0 = the join screen's, 1..8 = the game's Health Handicap list
extern uint32_t my_handicap;                  // net_handicap: this player's health handicap in matches
const char* HandicapChoiceName(uint32_t choice);
bool HandicapFor(int player, int32_t& handicap);  // that player's net_handicap in the session now; false for none
extern bool coop_friendly_fire;                // net_coop_friendly_fire: the host's is everyone's
bool CoopFriendlyFire();                      // the host's as of the tick the game has now (the same on every machine)
extern bool coop_more_enemies;                 // net_coop_more_enemies: an enemy more per extra player, the host's
extern bool coop_tougher_enemies;              // net_coop_tougher_enemies: enemies' health with the players, the host's
bool CoopMoreEnemies();                       // the host's as of the tick the game has now, as CoopFriendlyFire
bool CoopTougherEnemies();
// the host's co-op choices after the bot settings in its bundles' rotation (kStart carries them too), every
// machine's at the same tick: kHostCoopFail (net_coop_fail), kHostCoopPickups (net_coop_pickups)
constexpr int kHostCoopFail = 0, kHostCoopPickups = 1, kHostSettings = 2;
constexpr uint32_t kCoopFailLast = 0, kCoopFailPlayer1 = 1, kCoopFailAnyone = 2, kCoopFailChoices = 3;
constexpr uint32_t kCoopPickupsShared = 0, kCoopPickupsSeparate = 1, kCoopPickupsChoices = 2;
extern uint32_t host_setting[kHostSettings];  // this PC's (a host's are the session's)
const char* CoopFailChoiceName(uint32_t choice);
const char* CoopPickupsChoiceName(uint32_t choice);
uint32_t HostSetting(int setting);  // the session's as of the tick the game has now; this PC's with no session
extern bool quit_together;                     // net_quit_together: the host's QUIT takes everyone to the menus
bool QuitTogether();                          // the host's as of the tick the game has now, as CoopFriendlyFire
bool UnlocksMissions();  // unlock_missions as the session has it: the host's (kStart), else this machine's
bool UnlocksMultiplayer();  // unlock_multiplayer, the same way
bool UnlocksRewards();      // unlock_rewards, the same way

const char* CharacterChoiceName(uint32_t choice);          // 0 = "Default (Menu)", 1..29 = character name
void SetCharacterName(uint32_t model_idx, const char* s);  // populated from default.xbe's string table (0x7EB90)
int CharacterFor(int slot);                                // the player's chosen model index (0..28) for slot 0..9, or -1
constexpr uint32_t kBotChoices = 11;  // net_bots: 0 = the bot menu's count, 1..9 = 0..8 bots, kBotsFill
constexpr uint32_t kBotsFill = 10;    // as many as the host's net_players has places the players left empty
extern uint32_t bots;
const char* BotChoiceName(uint32_t choice);
int Bots();                                                // the session's bot count (the host's net_bots), -1 the menu's
uint32_t BotBlocks();                                      // the bots' state below slot 4 (0 none yet), in the snapshot
void SetBotBlocks(uint32_t va);
// the host's bot settings (net_bot_*), every machine's in a session: kStart carries them, then the host's bundles a
// value a tick, so a change in its settings window reaches every machine at the same tick. Bots 1 to 6 are the bot
// menu's entries (src/game/bots.cpp writes them in at the spawn); Bots 7 and 8 are below slot 4
constexpr uint32_t kBotRandom = kCharacterChoices;         // a character setting's Random
constexpr uint32_t kBotCharChoices = kCharacterChoices + 1;  // 0 = Default, 1..29 a character, then Random
constexpr uint32_t kBotFieldCount = 7;
constexpr int kBotEvery = 0, kBotSkill = 1;  // every bot's character; every bot's skill (0 its own, 1 Bot 1's)
constexpr int kBotChar = 2;                  // + bot 0..7: that bot's character
constexpr int kBotField = kBotChar + 8;      // + bot 0..7 * kBotFieldCount + field: that bot's fields
constexpr int kBotTeam = kBotField + 8 * int(kBotFieldCount);  // + bot 0..7: its team in the team scenarios
constexpr int kBotSettings = kBotTeam + 8;
constexpr uint32_t kBotTeamChoices = 3;  // its character's side (the menu's), MI6 (the entry's team 1), Phoenix (0)
struct BotField {        // a field of the bot menu's entry (0x12 bytes at 0x245282 a bot)
    const char* label;
    uint8_t offset, size, count;
    const char* names[9];
    uint16_t values[9];  // what the entry holds for each choice
};
extern const BotField kBotFields[kBotFieldCount];
extern uint32_t bot_setting[kBotSettings];  // this PC's (a host's are the session's)
uint32_t BotChoices(int setting);
const char* BotCharChoiceName(uint32_t choice);
const char* BotSkillChoiceName(uint32_t choice);
const char* BotFieldChoiceName(int field, uint32_t choice);
const char* BotTeamChoiceName(uint32_t choice);
void SetBotSettings(const char* list);  // "setting=value,...", NF_NET_BOT
int BotSetting(int setting);            // the session's as of the tick the game has now; 0 (Default) with no session
uint32_t BotChanges();                  // how often the session's changed (the same on every machine)
bool BotSettingsWhole();                // a round of the rotation's ticks since the last change: all the host's came
uint32_t BotSeed();                     // the session's seed for their Random
uint32_t& BotMenuChanges();             // src/game/bots.cpp's, in the snapshot: the BotChanges the bot menu has,
uint32_t& BotRound();                   // and the rounds of Random picks
void SetHostBotSetting(int setting, uint32_t value);  // a host's bot menu edit: its own setting, saved

void Init(void* hwnd = nullptr);  // GameInfo::window: NF_NET=host|join[,address] (and NF_NET_PORT, _WAIT, _DELAY,
                                  // _PLAYERS, _IP) for the harness, else the settings; holds until the machines are
                                  // together, or the game boots as it does without a session
void Close();                     // GameInfo::relaunching: closes the UDP socket before the handover to the next exe
void Leave(bool over = true);     // this machine quits: the others play on, its player's input idle from this tick
                                  // (over: the session is done with here, and net_join goes off)
void LeaveToMenu();               // Leave, then this machine plays on alone (and its relaunches boot without a session)
bool CanRestartEveryone();         // a host in a session under way: F1's Restart session for everyone is there
void RestartEveryone();            // that button: every machine restarts online play and meets this host again
void RestartForHost();             // a joiner hearing it: restarts online play, Join kept on
void Restarting();                // GameInfo::restarting (F1's Restart online play): Leave, and the new boot meets again
// GameInfo::find_games (F1's Online page): the hosts on this LAN and this PC, asked while it is called (NF_NET_FIND=1:
// from boot, every change logged, for the harness)
void FindGames(std::vector<app::FoundGame>& out);
void AskGames(const std::vector<std::string>& addresses);  // GameInfo::ask_games: the browser's saved addresses too
void RefreshGames();                                        // GameInfo::refresh_games
int LeftSeat();                   // after LeaveToMenu, until EndLeftSeats: this machine's controller port in the session
int LeftSeats();                  // and the session's ports (at most 4), kept connected and idle until the menus are back
void EndLeftSeats();

bool Active();          // a session is on: input travels between the machines
bool FullScreenView();  // Active() and fullscreen: each machine draws only its own player's view full-screen
bool OnGameThread();    // this poll is the game's own thread (input::GameThreadPoll, set by XInputGetState)
int Players();          // total players in the active session (2..10), or 1 when offline
int LocalPlayer();      // this machine's player index (a host 0, a joiner its session slot 1..9), 0 with no session
int Driver();           // the player whose input plays controller 1's seat: 0, or this machine's own once the host has
                        // gone (it plays on alone): in Driving.xbe they take the car, the pause and the gun
bool PlayerLeft(int player);  // that player has left the session by the tick the game has now (the same on every
                              // machine): their controller is idle from it
uint32_t Tick();              // the tick the game has now, the same on every machine (0 before the first)

// While decoding inputs for extra players (slots 4..9) through hardware controller port 3, remaps port 3 -> slot
// and suppresses advancing the network lockstep tick on port 0
void SetExtraInputPass(bool active, int port3_player = 3);
int PortPlayer(int port);

// GameInfo::pad with a session, after nf::MenuMouse made the local player's pad: captures that pad (and the mouse
// and the zoom the game-repo code took since the last tick), waits for the other machines' ticks, and hands the
// game the session's players' pads. Ports without a player read disconnected
void Pad(int port, input::Pad& p);
void BackgroundPad(int port, input::Pad& p);  // a poll from the runtime's device thread: the pads as they are

// this tick's input of the session's player at that index (the controller port their pad plays), the same on
// every machine: what the view control, the auto-aim, the styles and the scope zoom of every player read
bool PadFor(int player, input::Pad& p);         // their pad this tick (idle once they left); false for no such player
bool MouseFor(int player, float& x, float& y);  // counts this tick; 0, 0 while their mouse did not turn their view
bool KeyboardFor(int player);                  // their player is played on the keyboard (style 0, no auto-aim)
int ZoomFor(int player);                       // their wheel's notches through a scope this tick

void AddZoom(int notches);    // nf::ScopeZoom with a session: the local player's notches, taken each tick

// rejoining (net.cpp's kRejoin): a machine whose player left the session under way boots alone and is given the
// session's game, a snapshot (core/snapshot.h) the host takes at its snapshot point, then plays their seat again
void Gather();  // the next tick waits until every player has sent it: the machines go on together (Driving.xbe's levels)
void EnableRejoin();                     // the exe has a snapshot point (default.xbe; Driving.xbe has none yet)
void SnapshotPoint(X86Context& ctx);     // there, each frame, on the game's thread
// on the game's thread, each frame at the same point: every 60 ticks these words of the game (its time, the camera)
// go to the others, and the first tick a machine's differ from this one's is logged with both (net.cpp's kCheck)
void CheckState(const uint32_t* words, size_t count);
// Driving.xbe's movies (a mission's opening, before its frames, where the pads are not read): this machine's player
// skipped the movie with that number since boot (the same on every machine), and every machine is told (kSkip);
// MovieSkipped: anyone's skip of it has come
void SkipMovie(uint32_t movie);
bool MovieSkipped(uint32_t movie);
void SaveState(snapshot::Writer& w);     // the session's "net" section: its ticks, inputs and players, not this
void LoadState(snapshot::Reader& r);     // machine's own seat, socket or addresses

}  // namespace nf::net
