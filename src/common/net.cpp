// Online multiplayer, lockstep over the split-screen game. The consoles' game has no networking: its multiplayer
// is one machine simulating every player from the pads plugged into it. A session is those pads over the wire: every
// machine runs its own complete copy of the match, each tick each one sends what its player did (the pad the runtime
// made from their devices, the mouse's movement, their scope zoom) and applies what every player did at that tick,
// so the copies stay identical without any game state being sent. Ticks are counted per port-0 poll on the game's
// thread (the game's own call pattern: the same code reads the same pads at the same points on every machine), and
// each machine's input is held net_input_delay ticks before the game gets it, so a tick can absorb the network's
// round trip. A session starts before boot (GameInfo::window): the host waits for joiners, then every machine boots
// from power-on together, the one state they share, and no machine can join a match already going. The session
// forms in BOTH exes: the path from the title to the multiplayer menus runs through Driving.xbe (the front end),
// and each relaunch between the two closes the old socket (net::Close) and the next exe's machines meet again at
// their own boot - the launch data the relaunch leaves is written identically by both machines' front ends, so they
// resume the same game on the other side.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef CreateSymbolicLink  // windows.h's, which would rename the kernel's
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "app/app.h"
#include "common/http.h"
#include "common/net.h"
#include "common/nightfire.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace nf::net {
bool host = false, join = false, join_setting = false, fullscreen = true;
uint32_t players = 2, port = 26565, wait = 120, delay = 3;
uint32_t my_character = 0;
uint32_t bot_setting[kBotSettings] = {};
uint32_t bots = 0;
std::string my_name;
bool default_names = false;
uint32_t my_handicap = 0;
bool coop_friendly_fire = true;
bool coop_more_enemies = true;
bool coop_tougher_enemies = true;
bool quit_together = true;
std::string master_url;
uint32_t who = kWhoNotListed;
std::string relay_server;
uint32_t host_by = kHostByAddress;
std::string password;
}

namespace nf::net {
namespace {

std::string g_char_names[kCharacterChoices];

using clock = std::chrono::steady_clock;
int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
}

constexpr uint32_t kMagic = 0x4E464E31;  // "NFN1"
constexpr uint16_t kVersion = 25;  // 1 (and 2, its Driving.xbe) sent one tick a bundle; 3 (4) no name or handicap;
                                   // 5 (6) no co-op friendly fire; 7 (8) no rejoining; 9 (10) no host's saves;
                                   // 11 (12) the on-foot sound by the mixer; 13 (14) a driving load's clock counted,
                                   // the car's copied shots paid from player 1's weapon; 15 (16) a seat no one took
                                   // played, no bots' character; 17 (18) no bots below slot 4; 19 (20) the
                                   // host's character overrides per slot, no bot settings (kBotSettings); 21
                                   // (22) no unlock_multiplayer; 23 (24) no net_quit_together nor kRestart
constexpr uint8_t kHello = 1,   // a joiner asking to play, before boot
    kWelcome = 2,               // the host's answer: the controller it is given, the input delay
    kStart = 3,                 // the host: boot, with the session's player count in the sequence
    kBundle = 4,                // one player's input for the tick in the sequence and up to kRedundant - 1 before it
    kRefused = 5,               // the host: the session is full or already going
    kLeave = 6,                 // a player is gone from the tick in the sequence on: their input is idle from it
    kAlive = 7,                 // the network thread's heartbeat, whatever the game is doing (loading, compiling)
    kResend = 8,                // a machine missing the player's tick in the sequence asks for it again
    // rejoining a session under way: the machine whose player left asks again (kHello), the host offers it the seat
    // (kRejoin, the player, the input delay in the sequence); it boots alone and says when it is at its snapshot
    // point (kReady); the host holds its game at its own, takes the game's state there (core/snapshot.h) and sends
    // it in kChunk packets (the chunk's number in the sequence) the machine acknowledges (kChunkAck: the first one
    // missing in the sequence, then a bitmap of those after it); the machine loads it and says so (kLoaded). The
    // other joiners learn the seat plays again from a tick (kBack, acknowledged with kBackAck) before the host's
    // game goes on: no machine has played that tick yet, all of them wait for the host's input of it
    kRejoin = 9, kReady = 10, kChunk = 11, kChunkAck = 12, kLoaded = 13, kBack = 14, kBackAck = 15,
    // a Driving.xbe host's answer to a default.xbe machine's kHello while a seat is free (the player of a machine
    // that left boots default.xbe again): the launch data Driving.xbe booted with, in pieces of kLaunchPiece (the
    // offset in the sequence), sent in default.xbe's wire version. The machine relaunches into Driving.xbe with it,
    // which asks again and rejoins
    kLaunch = 16,
    // a machine's view of the game at the tick in the sequence (the words its exe gives CheckState: the game's
    // time, the camera), relayed by the host. The first tick two machines' differ is logged with both
    kCheck = 17,
    // the driver skipped Driving.xbe's movie with the number in the sequence (SkipMovie), sent for a second, relayed
    // by the host
    kSkip = 18,
    // the host's saves before boot (or a rejoin): the offset in the sequence, then their id and size, then a piece
    // of kSavesPiece. A joiner's hello says which it has whole
    kSaves = 19,
    // the host restarts the session (F1's Restart session for everyone): every machine restarts online play as
    // F1's Restart online play does and meets the host again. Sent a few times outside the lockstep, so a stalled
    // session hears it too
    kRestart = 20;
constexpr uint32_t kSavesPiece = 1024, kSavesMax = 8u << 20;
constexpr int64_t kSavesWaitMs = 15000;  // the host waits this long past its start for every joiner to have them
constexpr uint32_t kCheckEvery = 60, kCheckWords = 32, kCheckKept = 16;
constexpr uint32_t kLaunchPage = 0x1000, kLaunchPiece = 1024;
constexpr int kRedundant = 8;       // ticks a bundle carries: a lost packet's are in the next ones
constexpr uint32_t kKeep = 600;     // ticks of every player's input a machine keeps to send again
constexpr int64_t kAskMs = 50;      // a missing tick is asked for this often
constexpr int64_t kStallMs = 30000;  // a tick's wait before it says so, then every 2 s
constexpr int64_t kAliveMs = 500;    // a heartbeat this often
constexpr int64_t kGoneMs = 6000;    // a player silent this long (no heartbeat either) is gone
constexpr uint32_t kStaying = UINT32_MAX;
constexpr size_t kChunkData = 1200;     // a snapshot's bytes a kChunk carries
constexpr uint32_t kAckBits = 8192;     // the chunks after the first missing one a kChunkAck covers
constexpr int64_t kRejoinWaitMs = 120000;  // the host holds its game this long for a rejoining machine at most
constexpr int64_t kChunkAgainMs = 250;     // a chunk not acknowledged this long after it went is sent again
constexpr int kReadyFrames = 600;          // a rejoining machine's frames before it asks for the state (a fresh
                                           // boot's handles and threads are all there by then)

// one player's input for one tick, as it travels
struct Bundle {
    input::Pad pad;
    float mouse_x = 0, mouse_y = 0;  // counts (the rate the runtime gave, over the tick, sensitivity applied)
    bool mouse = false;              // their mouse turned their view that tick
    bool keyboard = false;           // their player is played on the keyboard: style 0, no auto-aim
    int16_t zoom = 0;                // the wheel's notches through a scope that tick
    uint8_t my_char = 0;             // this player's chosen character (0 = default, 1..29 = model 0..28)
    uint8_t host_bot_at = 0;         // the host's: which of its bot settings this tick carries (the tick modulo
    uint8_t host_bot = 0;            // kBotSettings), and its value
    uint8_t handicap = 0;            // this player's net_handicap (0 = the join screen's, 1..8)
    uint8_t name_char = 0;           // one letter of this player's net_name: the tick's count modulo kNameLen
    bool friendly_fire = true;       // the host's net_coop_friendly_fire (a joiner's is not read)
    bool more_enemies = true;        // the host's net_coop_more_enemies (the same)
    bool tougher_enemies = true;     // the host's net_coop_tougher_enemies (the same)
    bool quit_together = true;       // the host's net_quit_together (the same)
};

// the wire packet, packed by hand: a header, then for a bundle its tick count and that many ticks' input, the
// sequence's first and each one the tick before. Little endian, as both sides are x86. The version carries
// kDriving so a dying Driving.xbe and a newly launched default.xbe ignore each other's packets
constexpr size_t kHeader = 12;
constexpr size_t kBody = 4 + 2 + 8 + 8 + 8 + 2;
constexpr size_t kBundleMax = kHeader + 1 + kBody * kRedundant;
// a kHello carries its player's net_name after the header, a kStart every player's character, then every player's
// name. An older build's shorter packets read as no name
constexpr size_t kNameLen = 16;
constexpr size_t kNamesStart = kHeader + kMaxPlayers + kMaxPlayers * kNameLen;  // a build without handicaps
constexpr size_t kStartSize = kNamesStart + kMaxPlayers;
constexpr size_t kStartFull = kStartSize + 1;  // then the host's flags: 1 its unlock_missions, 2 unlock_multiplayer
constexpr size_t kStartSaves = kStartFull + 4;  // then the id of the host's saves (kSaves)
constexpr size_t kStartBots = kStartSaves + 1 + kBotSettings + 4;  // then the host's net_bots, its bot settings,
                                                                    // the session's seed for their Random
constexpr size_t kHelloHandicap = kHeader + kNameLen + 1;
constexpr size_t kHelloSize = kHelloHandicap + 4;  // then the id of the host's saves this machine has whole, 0 none
// then the password the joiner was given for the host's game: printable ASCII, zero padded. A host whose net_who is
// Only people I tell (HostKey) refuses a hello without it (an older build's) or with another, kRefused with
// kRefusedPassword in the sequence (kRefusedCode: the key is its join code); the password goes as it is typed, so it
// keeps strangers out and is no secret from the network
constexpr size_t kPasswordLen = 32;
constexpr size_t kHelloPassword = kHelloSize + kPasswordLen;
constexpr uint32_t kRefusedPassword = 1, kRefusedCode = 2;
// the join box's list (0x88720: the strings at 0x161870..0x161890), in the order it shows them
constexpr int32_t kHandicaps[kHandicapChoices - 1] = {-75, -50, -25, 0, 25, 50, 75, 100};

// net_name as it travels: printable ASCII, at most kNameLen - 1 characters, the rest zero
void PackName(uint8_t* b, const char* s, size_t n) {
    std::memset(b, 0, kNameLen);
    for (size_t i = 0, k = 0; i < n && s[i] && k < kNameLen - 1; i++)
        if (s[i] >= 0x20 && s[i] < 0x7F) b[k++] = uint8_t(s[i]);
}
// a password as it travels (kHelloPassword): printable ASCII, at most kPasswordLen - 1 characters, the rest zero
std::string PasswordText(const std::string& s) {
    std::string out;
    for (char c : s)
        if (c >= 0x20 && c < 0x7F && out.size() < kPasswordLen - 1) out += c;
    out.erase(0, out.find_first_not_of(' '));
    out.erase(out.find_last_not_of(' ') + 1);
    return out;
}
// a join code as a joiner may type it (nf-abc123, ABC123, NF-ABC123@relay): its six letters, upper case
std::string CodeText(std::string s) {
    s.erase(0, s.find_first_not_of(" \t"));
    if (s.size() >= 3 && (s[0] | 0x20) == 'n' && (s[1] | 0x20) == 'f' && s[2] == '-') s.erase(0, 3);
    if (size_t at = s.find('@'); at != std::string::npos) s.resize(at);
    s.erase(s.find_last_not_of(" \t") + 1);
    for (char& c : s) c = char(std::toupper(uint8_t(c)));
    return s;
}
// net_who (NF_NET_WHO for the harness; NF_NET_PUBLIC=1 lists the game as net_public did)
uint32_t Who() {
    if (const char* e = std::getenv("NF_NET_WHO"); e && *e) return std::min<uint32_t>(uint32_t(std::atoi(e)), kWhoChoices - 1);
    uint32_t w;
    {
        std::lock_guard<std::recursive_mutex> ui(app::UiLock());
        w = std::min(who, kWhoChoices - 1);
    }
    if (const char* e = std::getenv("NF_NET_PUBLIC"); e && *e == '1' && w == kWhoNotListed) w = kWhoAnyone;
    return w;
}
// a host's net_password (NF_NET_PASSWORD for the harness)
std::string HostPassword() {
    if (const char* e = std::getenv("NF_NET_PASSWORD")) return PasswordText(e);
    std::lock_guard<std::recursive_mutex> ui(app::UiLock());
    return PasswordText(password);
}
// the one a joiner gives (the settings' join_password, NF_NET_JOIN_PASSWORD)
std::string JoinPassword() {
    if (const char* e = std::getenv("NF_NET_JOIN_PASSWORD")) return PasswordText(e);
    std::lock_guard<std::recursive_mutex> ui(app::UiLock());
    return PasswordText(app::Get().join_password);
}
uint16_t WireVersion() { return uint16_t(kVersion + (kDriving ? 1 : 0)); }
size_t Pack(uint8_t* b, uint8_t type, uint8_t player, uint32_t seq) {
    uint16_t ver = WireVersion();
    std::memcpy(b, &kMagic, 4);
    std::memcpy(b + 4, &ver, 2);
    b[6] = type, b[7] = player;
    std::memcpy(b + 8, &seq, 4);
    return kHeader;
}
void PackBody(uint8_t* b, const Bundle* in) {
    size_t n = 0;
    b[n++] = uint8_t((in->pad.connected ? 1 : 0) | ((in->host_bot_at & 0x10) >> 3) | ((in->host_bot_at & 0x0F) << 2) |
                     (in->friendly_fire ? 0x40 : 0) | (in->more_enemies ? 0x80 : 0));
    b[n++] = uint8_t((in->keyboard ? 1 : 0) | ((in->my_char & 0x3F) << 1) | (in->tougher_enemies ? 0x80 : 0));
    b[n++] = uint8_t((in->mouse ? 1 : 0) | ((in->host_bot & 0x3F) << 1) | (in->quit_together ? 0x80 : 0));
    b[n++] = uint8_t(std::clamp<int>(in->zoom, -8, 8));
    std::memcpy(b + n, &in->pad.buttons, 2), n += 2;
    std::memcpy(b + n, in->pad.analog, 8), n += 8;
    std::memcpy(b + n, &in->pad.lx, 2), n += 2;
    std::memcpy(b + n, &in->pad.ly, 2), n += 2;
    std::memcpy(b + n, &in->pad.rx, 2), n += 2;
    std::memcpy(b + n, &in->pad.ry, 2), n += 2;
    std::memcpy(b + n, &in->mouse_x, 4), n += 4;
    std::memcpy(b + n, &in->mouse_y, 4), n += 4;
    b[n++] = in->handicap;
    b[n++] = in->name_char;
}
bool UnpackHeader(const uint8_t* b, size_t n, uint8_t& type, uint8_t& player, uint32_t& seq) {
    if (n < kHeader) return false;
    uint32_t magic;
    uint16_t version;
    std::memcpy(&magic, b, 4), std::memcpy(&version, b + 4, 2);
    if (magic != kMagic || version != WireVersion()) return false;
    type = b[6], player = b[7];
    std::memcpy(&seq, b + 8, 4);
    return true;
}
void UnpackBody(const uint8_t* b, Bundle& out) {
    size_t at = 0;
    out.pad.connected = (b[at] & 1) != 0;
    out.host_bot_at = uint8_t(((b[at] >> 2) & 0x0F) | ((b[at] & 2) << 3));
    out.friendly_fire = (b[at] & 0x40) != 0;
    out.more_enemies = (b[at] & 0x80) != 0;
    out.tougher_enemies = (b[at + 1] & 0x80) != 0;
    out.keyboard = (b[at + 1] & 1) != 0;
    out.my_char = (b[at + 1] >> 1) & 0x3F;
    out.mouse = (b[at + 2] & 1) != 0;
    out.host_bot = (b[at + 2] >> 1) & 0x3F;
    out.quit_together = (b[at + 2] & 0x80) != 0;
    out.zoom = int16_t(std::clamp<int>(int8_t(b[at + 3]), -8, 8));
    at += 4;
    std::memcpy(&out.pad.buttons, b + at, 2), at += 2;
    std::memcpy(out.pad.analog, b + at, 8), at += 8;
    std::memcpy(&out.pad.lx, b + at, 2), at += 2;
    std::memcpy(&out.pad.ly, b + at, 2), at += 2;
    std::memcpy(&out.pad.rx, b + at, 2), at += 2;
    std::memcpy(&out.pad.ry, b + at, 2), at += 2;
    std::memcpy(&out.mouse_x, b + at, 4), at += 4;
    std::memcpy(&out.mouse_y, b + at, 4), at += 4;
    out.handicap = uint8_t(std::min<uint32_t>(b[at], kHandicapChoices - 1));
    out.name_char = b[at + 1] >= 0x20 && b[at + 1] < 0x7F ? b[at + 1] : 0;
}

// one player's stream, as this machine has it: their input by tick, as it arrived (a machine's own, at capture),
// kept kKeep ticks past its use for any machine that asks again. The tick the game applies next is the same count
// on every machine, so the key finds its input
struct Stream {
    std::map<uint32_t, Bundle> known;
    uint32_t next = 0;  // the tick the game applies next
};

// a bundle of the player's input from the tick seq back, as many ticks as the stream has in a row (g.m held)
size_t PackBundle(uint8_t* b, uint8_t player, uint32_t seq, const Stream& s) {
    size_t n = Pack(b, kBundle, player, seq);
    uint8_t& count = b[n++];
    count = 0;
    for (int k = 0; k < kRedundant && uint32_t(k) <= seq; k++) {
        auto it = s.known.find(seq - uint32_t(k));
        if (it == s.known.end()) break;
        PackBody(b + n, &it->second), n += kBody;
        count++;
    }
    return count ? n : 0;
}

struct Session {
    bool active = false;               // the game transports input
    int self = 0;                      // this machine's player: the host 0, a joiner its controller port
    int players = 1;                   // the session's players (the host tells everyone at boot)
    uint32_t mine = 0;                 // this machine's bundles sent (its game ticks)
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in host_addr{};            // a joiner: where the host is. A host: unset
    sockaddr_in peer[kMaxPlayers]{};   // a host: its joiners, by player (0, itself, unused)
    bool has_peer[kMaxPlayers] = {};
    std::mutex m;
    std::condition_variable cv;
    Stream stream[kMaxPlayers];
    Bundle applied[kMaxPlayers];       // what the game reads this tick, by player
    uint8_t peer_char[kMaxPlayers] = {};
    uint8_t peer_name[kMaxPlayers][kNameLen] = {};  // each player's net_name, zero-padded (the host's is everyone's)
    uint8_t peer_handicap[kMaxPlayers] = {};        // each player's net_handicap
    // a name changed in a session travels a letter a tick (Bundle::name_char) into next_name, and becomes the
    // player's at the end of a round of kNameLen ticks: on every machine at the same tick, never half changed
    uint8_t next_name[kMaxPlayers][kNameLen] = {};
    uint8_t own_name[kNameLen] = {};
    bool more_enemies = true, tougher_enemies = true;  // the host's net_coop_more_enemies, net_coop_tougher_enemies
    bool quit_together = true;  // the host's net_quit_together as of the tick the game has now
    bool friendly_fire = true;  // the host's net_coop_friendly_fire as of the tick the game has now  // net_name as this machine sends it (NameChanged)
    std::atomic<int> zoom{0};          // nf::ScopeZoom's notches, taken into the next capture
    // leaving: a player's input is idle from left_at on, on every machine alike (the tick travels in kLeave). The
    // host decides for a joiner that goes silent; a joiner whose host goes silent plays on alone (host_gone)
    uint32_t left_at[kMaxPlayers];
    uint32_t back_at[kMaxPlayers];  // a player who left plays again from this tick (rejoined), kStaying none
    std::atomic<int64_t> heard[kMaxPlayers];  // a host: each joiner's last packet; a joiner: [0], the host's
    bool leaving = false;                     // this machine is leaving: no more bundles
    bool host_gone = false;
    bool unlocks = false;  // the host's unlock_missions: every machine's mission list is the host's
    bool unlocks_mp = false;  // the host's unlock_multiplayer, the same for multiplayer's lists
    uint8_t bots = 0;      // the host's net_bots: 0 the bot menu's count, else 1 + the count
    uint8_t bot[kBotSettings] = {};  // the host's bot settings as of the tick the game has now
    uint32_t bot_changes = 0;        // how often they changed in the session (kStart's is the first)
    uint32_t bot_changed_at = 0;     // the tick of the last change
    uint32_t bot_seed = 0;           // the host's, for their Random (kStart)
    uint32_t bot_menu = 0;           // src/game/bots.cpp's: the bot_changes the bot menu has, and how many rounds
    uint32_t bot_round = 0;          // of Random picks there were
    uint32_t bot_blocks = 0;  // the bots' state below slot 4, in the title's memory (src/game/view.cpp's)
    int64_t tick_now = -1;                    // the tick the game has now (the same on every machine)
} g;

// a rejoin under way (kRejoin .. kBackAck): the host's side or the rejoining machine's. Taken after g.m when both are
struct Transfer {
    std::mutex m;
    std::condition_variable cv;
    // the host
    int seat = -1;               // the seat offered, -1 none
    sockaddr_in to{};            // the machine it was offered to
    bool ready = false;          // it is at its snapshot point
    uint32_t id = 0;             // the tick it plays from, which names the transfer (0 before it starts)
    std::vector<uint8_t> acked;  // by chunk
    uint32_t acked_count = 0;
    uint32_t window = 0;         // the first chunk the machine is missing: the sender keeps within kAckBits of it
    bool loaded = false, failed = false;
    bool back_acked[kMaxPlayers] = {};
    // the rejoining machine
    bool rejoining = false;      // offered a seat: it boots alone, then loads the session's state
    std::vector<uint8_t> data;   // the compressed state as it arrives
    std::vector<uint8_t> have;   // by chunk
    uint32_t got = 0, first = 0;  // chunks here, the first one missing
    bool complete = false;
    int64_t last_ack = 0;
} g_tx;
bool g_rejoinable = false;  // the exe has a snapshot point (EnableRejoin)
// Driving.xbe's launch data (kLaunch): a host's own, read before its boot (the title frees the kernel's page); a
// default.xbe machine's from the host, the pieces as they come, then the relaunch from Meet
struct LaunchTo {
    std::mutex m;
    uint8_t page[kLaunchPage] = {};
    bool own = false;
    bool have[kLaunchPage / kLaunchPiece] = {};
    std::atomic<bool> complete{false};
} g_launch;
std::atomic<bool> g_gather{false};  // the next tick waits for every player (Gather)

std::atomic<bool> g_alive{false};   // the receive thread runs while the socket does
std::atomic<bool> g_booting{false};  // the session is going: boot proceeds, no one joins it
std::atomic<int64_t> g_password_refused{0};  // a joiner: when the host refused this machine's password, 0 not
std::atomic<bool> g_refused_code{false};     // and the host's key is its join code
constexpr int64_t kPasswordShownMs = 5000;   // with no window to type it in, the boot alone comes this long after
std::string g_env_address;  // NF_NET=join,host's address, kept for Init's address setup
int g_target = 2;           // the player count a host starts at once reached
bool g_extra_input_pass = false;
int g_port3_player = 3;

int MapPortToPlayer(int port) { return (g_extra_input_pass && port == 3) ? g_port3_player : port; }

void Send(const sockaddr_in& to, const uint8_t* b, size_t n);

// this instance's disk, as the kernel boots with it
std::filesystem::path HddDir() {
    int instance = app::Instance();
    return app::ExeDir() / (instance > 1 ? "hdd" + std::to_string(instance) : std::string("hdd"));
}

// the saves (kSaves): every machine of a session boots from the host's, the title's folder of its disk (every
// profile both exes read and write). A save steers the game - the profiles the menus list, a profile's options and
// missions - so a joiner booting from its own played another game from the menus on. A joiner's copy goes to
// E:\NFSESSION\<title> on its disk and the title's save folder is linked there: its own saves stay as they were
constexpr char kTitleFolder[] = "45410026";
struct Saves {
    std::mutex m;
    std::vector<uint8_t> data;  // a host: its own (PackSaves); a joiner: the host's as they arrive
    std::vector<uint8_t> have;  // a joiner: by piece
    uint32_t id = 0;            // a hash of the packed saves (never 0), which names them
    uint32_t got = 0;
    std::atomic<uint32_t> complete{0};  // a joiner: the id of the saves here whole, 0 none
    uint32_t host_id = 0;               // a joiner: the id the host's kStart names
    bool ok[kMaxPlayers] = {};          // a host: that joiner has them whole
} g_saves;

// the host: its saves packed, a count of files then each one's name (relative, '/'), size, write time and bytes
// (g_saves.m held)
void PackSaves() {
    namespace fs = std::filesystem;
    std::vector<uint8_t> out(4, 0);
    auto put = [&](const void* p, size_t n) {
        auto b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    };
    fs::path root = HddDir() / "E" / "UDATA" / kTitleFolder;
    std::error_code ec;
    std::vector<fs::path> paths;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) paths.push_back(it->path());
    std::sort(paths.begin(), paths.end());
    uint32_t files = 0;
    for (const fs::path& p : paths) {
        std::string name = p.lexically_relative(root).generic_string();
        uint64_t size = fs::file_size(p, ec);
        if (ec || name.size() > 255 || out.size() + size > kSavesMax) {
            LOGE("net: the save %s was not sent to the joiners", name.c_str());
            continue;
        }
        std::vector<uint8_t> bytes(size_t(size), 0);
        FILE* f = std::fopen(p.string().c_str(), "rb");
        bool read = f && std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size();
        if (f) std::fclose(f);
        if (!read) {
            LOGE("net: the save %s could not be read; it was not sent to the joiners", name.c_str());
            continue;
        }
        uint16_t len = uint16_t(name.size());
        uint32_t size32 = uint32_t(size);
        int64_t time = int64_t(fs::last_write_time(p, ec).time_since_epoch().count());
        put(&len, 2), put(name.data(), len), put(&size32, 4), put(&time, 8), put(bytes.data(), bytes.size());
        files++;
    }
    std::memcpy(out.data(), &files, 4);
    uint32_t h = 2166136261u;
    for (uint8_t b : out) h = (h ^ b) * 16777619u;
    g_saves.data = std::move(out);
    g_saves.id = h ? h : 1;
    LOGR("net: this machine's saves go to the joiners (%u files, %zu bytes)", files, g_saves.data.size());
}

// the host: its saves to a joiner, every piece (they are small; the joiner's next hello says whether they came)
void SendSaves(const sockaddr_in& to) {
    std::lock_guard l(g_saves.m);
    uint32_t total = uint32_t(g_saves.data.size());
    uint8_t pkt[kHeader + 8 + kSavesPiece];
    for (uint32_t at = 0; at < total; at += kSavesPiece) {
        uint32_t len = std::min(kSavesPiece, total - at);
        Pack(pkt, kSaves, 0, at);
        std::memcpy(pkt + kHeader, &g_saves.id, 4), std::memcpy(pkt + kHeader + 4, &total, 4);
        std::memcpy(pkt + kHeader + 8, g_saves.data.data() + at, len);
        Send(to, pkt, kHeader + 8 + len);
    }
}

// a joiner: a piece of the host's saves
void TakeSaves(const uint8_t* buf, size_t n, uint32_t at) {
    if (n < kHeader + 8) return;
    uint32_t id, total;
    std::memcpy(&id, buf + kHeader, 4), std::memcpy(&total, buf + kHeader + 4, 4);
    if (!id || !total || total > kSavesMax || at >= total || at % kSavesPiece) return;
    uint32_t len = std::min(kSavesPiece, total - at), pieces = (total + kSavesPiece - 1) / kSavesPiece;
    if (n != kHeader + 8 + len) return;
    std::lock_guard l(g_saves.m);
    if (id != g_saves.id || g_saves.data.size() != total) {
        g_saves.id = id, g_saves.data.assign(total, 0), g_saves.have.assign(pieces, 0), g_saves.got = 0;
        g_saves.complete = 0;
    }
    if (g_saves.have[at / kSavesPiece]) return;
    g_saves.have[at / kSavesPiece] = 1;
    std::memcpy(g_saves.data.data() + at, buf + kHeader + 8, len);
    if (++g_saves.got == pieces) {
        g_saves.complete = id;
        LOGR("net: the host's saves are here (%u bytes)", total);
    }
}

// a joiner, before boot: the host's saves (complete) written to its session folder, and the title's save folder
// linked to it
bool UseHostSaves() {
    namespace fs = std::filesystem;
    std::lock_guard l(g_saves.m);
    const std::vector<uint8_t>& d = g_saves.data;
    fs::path root = HddDir() / "E" / "NFSESSION" / kTitleFolder;
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    size_t at = 0;
    auto take = [&](void* p, size_t n) {
        if (at + n > d.size()) return false;
        std::memcpy(p, d.data() + at, n);
        at += n;
        return true;
    };
    uint32_t files = 0;
    if (!take(&files, 4)) return false;
    for (uint32_t i = 0; i < files; i++) {
        uint16_t len = 0;
        uint32_t size = 0;
        int64_t time = 0;
        if (!take(&len, 2) || at + len > d.size()) return false;
        std::string name(reinterpret_cast<const char*>(d.data() + at), len);
        at += len;
        if (!take(&size, 4) || !take(&time, 8) || at + size > d.size()) return false;
        if (name.empty() || name[0] == '/' || name.find("..") != std::string::npos ||
            name.find_first_of(":\\") != std::string::npos)
            return false;
        fs::path p = root / fs::path(name);
        fs::create_directories(p.parent_path(), ec);
        FILE* f = std::fopen(p.string().c_str(), "wb");
        bool wrote = f && std::fwrite(d.data() + at, 1, size, f) == size;
        if (f) std::fclose(f);
        if (!wrote) return false;
        fs::last_write_time(p, fs::file_time_type(fs::file_time_type::duration(time)), ec);
        at += size;
    }
    return kernel::CreateSymbolicLink(std::string("\\Device\\Harddisk0\\Partition1\\UDATA\\") + kTitleFolder,
                                      std::string("\\Device\\Harddisk0\\Partition1\\NFSESSION\\") + kTitleFolder);
}

// the host: every joiner has its saves (g.m held)
bool JoinersHaveSaves() {
    std::lock_guard l(g_saves.m);
    for (int p = 1; p < kMaxPlayers; p++)
        if (g.has_peer[p] && !g_saves.ok[p]) return false;
    return true;
}

// the host's kStart: the player count and slots in the sequence, then the characters and names (g.m held)
size_t PackStart(uint8_t* b) {
    uint32_t seq = uint32_t(g.players & 0xFF) | (uint32_t(std::max(g.players, g_target) & 0xFF) << 8);
    size_t n = Pack(b, kStart, 0, seq);
    std::memcpy(b + n, g.peer_char, kMaxPlayers), n += kMaxPlayers;
    std::memcpy(b + n, g.peer_name, sizeof(g.peer_name)), n += sizeof(g.peer_name);
    std::memcpy(b + n, g.peer_handicap, kMaxPlayers), n += kMaxPlayers;
    b[n++] = uint8_t((g.unlocks ? 1 : 0) | (g.unlocks_mp ? 2 : 0));
    std::lock_guard l(g_saves.m);
    std::memcpy(b + n, &g_saves.id, 4), n += 4;
    b[n++] = g.bots;
    std::memcpy(b + n, g.bot, kBotSettings), n += kBotSettings;
    std::memcpy(b + n, &g.bot_seed, 4), n += 4;
    return n;
}

// the pad the game faces at a session port: its player's last applied input, an idle controller until the first
// tick's bundle arrives - connected either way. The game asks which controllers are plugged in as it boots and
// opens its pads once (xapi's UpdatePorts and the title's own open); a pad that reads disconnected then never
// becomes one, and with no open pad the game never polls, so no tick would ever advance. A session's player is
// always plugged in: a machine that stops answering freezes the session rather than unplugging its player
input::Pad Serve(int port) {
    input::Pad p;
    int pl = MapPortToPlayer(port);
    if (!g.active || pl < 0 || pl >= g.players) return p;
    std::lock_guard lock(g.m);
    p = g.applied[pl].pad;
    p.connected = true;
    return p;
}

bool Tracing() {  // NF_NET_TRACE=1: every packet sent and received, its type, player and sequence
    static const bool t = [] {
        const char* e = std::getenv("NF_NET_TRACE");
        return e && *e == '1';
    }();
    return t;
}

// NF_NET_DROP=percent (the harness): that share of the packets this machine sends once the session is going is
// dropped, as a bad connection would
bool Dropped() {
    static const int pct = [] {
        const char* e = std::getenv("NF_NET_DROP");
        return e ? std::clamp(std::atoi(e), 0, 90) : 0;
    }();
    static std::atomic<uint32_t> state{0x2545F491u};
    if (!pct || !g_booting.load()) return false;
    uint32_t x = state.load();
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    state.store(x);
    return int(x % 100) < pct;
}

// the server lists and relays (net_master, net_relay): a setting's own (commas or spaces between several), then the
// exe's built-in ones, then those the lists named when last asked (GET /servers, kept in servers.txt beside the exe),
// so a list or relay that goes away is replaced without a new download. A setting of "none": none of that kind
constexpr size_t kMaxServers = 8, kServerTextMax = 100;
constexpr int kDirectoryWaitMs = 4000;

std::vector<std::string> SplitWords(const std::string& text) {
    std::vector<std::string> out;
    std::string w;
    for (char c : text + ",") {
        if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!w.empty()) out.push_back(w), w.clear();
        } else {
            w += c;
        }
    }
    return out;
}

// "list <url>" and "relay <host:port>" entries; the harness's Linux runs have none but NF_NET_BUILTIN's
std::vector<std::string> BuiltInServers() {
    std::vector<std::string> out;
    std::string text;
    if (const char* e = std::getenv("NF_NET_BUILTIN")) text = e;
#ifdef _WIN32
    else text = "list https://nightfire-master.nfsb.workers.dev";  // its /servers names the other lists and the relays
#endif
    std::vector<std::string> w = SplitWords(text);
    for (size_t i = 0; i + 1 < w.size(); i += 2) out.push_back(w[i] + " " + w[i + 1]);
    return out;
}

// a list's address as it is asked (http:// or https://, no trailing slash), a relay's host[:port]; "" if neither
std::string ServerText(bool list, std::string v) {
    v.erase(v.find_last_not_of('/') + 1);
    if (v.empty() || v.size() > kServerTextMax) return {};
    for (char c : v)
        if (uint8_t(c) <= ' ' || uint8_t(c) >= 0x7F) return {};
    if (list) {
        if (v.find("://") == std::string::npos) v = "https://" + v;
        return v.rfind("http://", 0) == 0 || v.rfind("https://", 0) == 0 ? v : std::string();
    }
    for (char c : v)
        if (!std::isalnum(uint8_t(c)) && c != '.' && c != '-' && c != ':') return {};
    return v;
}

struct Directory {
    std::mutex m;
    bool loaded = false;
    std::vector<std::string> named;  // the lists' last answer: "list <url>" and "relay <host:port>"
    std::atomic<bool> asked{false};
    std::atomic<bool> done{false};  // this run's asking is over, answered or not
} g_dir;

std::filesystem::path DirectoryFile() { return app::ExeDir() / "servers.txt"; }

std::vector<std::string> ServersOf(const std::string& setting, bool list) {
    std::vector<std::string> words = SplitWords(setting), out;
    if (words.size() == 1 && (words[0] == "none" || words[0] == "None" || words[0] == "NONE")) return out;
    auto add = [&](const std::string& v) {
        std::string s = ServerText(list, v);
        if (!s.empty() && std::find(out.begin(), out.end(), s) == out.end() && out.size() < kMaxServers) out.push_back(s);
    };
    for (const std::string& w : words) add(w);
    const std::string kind = list ? "list " : "relay ";
    for (const std::string& e : BuiltInServers())
        if (e.rfind(kind, 0) == 0) add(e.substr(kind.size()));
    std::lock_guard l(g_dir.m);
    if (!g_dir.loaded) {
        g_dir.loaded = true;
        if (FILE* f = std::fopen(DirectoryFile().string().c_str(), "rb")) {
            char line[256];
            while (std::fgets(line, sizeof line, f)) {
                std::vector<std::string> w = SplitWords(line);
                if (w.size() == 2 && (w[0] == "list" || w[0] == "relay")) g_dir.named.push_back(w[0] + " " + w[1]);
            }
            std::fclose(f);
        }
    }
    for (const std::string& e : g_dir.named)
        if (e.rfind(kind, 0) == 0) add(e.substr(kind.size()));
    return out;
}

std::string MasterSetting() {
    if (const char* e = std::getenv("NF_NET_MASTER")) return e;
    std::lock_guard<std::recursive_mutex> ui(app::UiLock());
    return master_url;
}
std::vector<std::string> ServerLists() { return ServersOf(MasterSetting(), true); }

// once a run, in the background: every list asked which lists and relays there are now, for the next start
void AskDirectory() {
    std::vector<std::string> lists = ServerLists();
    if (lists.empty()) g_dir.done = true;
    if (lists.empty() || g_dir.asked.exchange(true)) return;
    std::thread([lists] {
        struct Done {
            ~Done() { g_dir.done = true; }
        } done;
        std::vector<std::string> named;
        std::string from;
        for (const std::string& url : lists) {
            std::string reply;
            if (http::Request("GET", url + "/servers", "", reply, kDirectoryWaitMs) != 200 || reply.rfind("NFS1", 0) != 0)
                continue;
            if (from.empty()) from = url;
            if (std::find(named.begin(), named.end(), "list " + url) == named.end()) named.push_back("list " + url);
            std::vector<std::string> w = SplitWords(reply.substr(4));
            for (size_t i = 0; i + 1 < w.size() && named.size() < 2 * kMaxServers; i += 2) {
                bool list = w[i] == "list";
                if (!list && w[i] != "relay") continue;
                std::string e = w[i] + " " + ServerText(list, w[i + 1]);
                if (e.size() > w[i].size() + 1 && std::find(named.begin(), named.end(), e) == named.end())
                    named.push_back(e);
            }
        }
        if (from.empty()) return;  // no list answered: what they said last time stands
        std::string text;
        for (const std::string& e : named) text += e + "\n";
        {
            std::lock_guard l(g_dir.m);
            g_dir.named = named;
            g_dir.loaded = true;
        }
        if (FILE* f = std::fopen(DirectoryFile().string().c_str(), "wb")) {
            std::fwrite(text.data(), 1, text.size(), f);
            std::fclose(f);
        }
        LOGI("net: the server lists (%s first) answered: %zu lists and relays kept in servers.txt", from.c_str(), named.size());
    }).detach();
}

// a relay wanted and none known (a first start knows the built-in lists alone, which name the relays): the lists'
// answer first, a few seconds at most
void AwaitRelays(const std::string& relay_text) {
    std::vector<std::string> w = SplitWords(relay_text);
    if ((w.size() == 1 && (w[0] == "none" || w[0] == "None" || w[0] == "NONE")) || !ServersOf(relay_text, false).empty())
        return;
    AskDirectory();
    for (int64_t until = NowMs() + 2 * kDirectoryWaitMs; !g_dir.done.load() && NowMs() < until;)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    LOGR("net: no relay was known yet; the server lists %s", ServersOf(relay_text, false).empty() ? "named none" : "named some");
}

// the relay (net_relay; tools/master/master.py --relay): a host that shows no address and needs no forwarded port
// registers with a relay server, which gives it a join code, and a joiner asks the relay for the code's game. Their
// session's packets then go to the relay wrapped (kRelayData and a connection number, the joiner's), and it sends
// each on to the other end as it came. To the session a relayed machine has a stand-in address in 240.0.0.0/8, which
// no real machine has: on a joiner its host is 240.0.0.0 port 1, on the host joiner connection c is 240.0.c port c.
// Register: a token (the host's proof, kept across its relaunches) and the code it had. Join: the code and the
// connection it had. Code: the code. Joined: the connection and the code. Refused: why and the code
constexpr char kRelayMagic[4] = {'N', 'F', 'R', '1'};
constexpr uint8_t kRelayRegister = 1, kRelayCode = 2, kRelayJoin = 3, kRelayJoined = 4, kRelayRefused = 5,
                  kRelayData = 6;
constexpr uint8_t kRefusedNoGame = 1, kRefusedFull = 2, kRefusedBusy = 3;
constexpr size_t kCodeLen = 6, kRelayHeader = 4 + 1 + 2, kRelayMax = 1500;
constexpr uint16_t kRelayPort = 26581;
constexpr int64_t kRelayEveryMs = 5000, kRelayAskMs = 500, kRelayTryMs = 3000;
static_assert(kHeader + 8 + kChunkData <= kRelayMax && kBundleMax <= kRelayMax);

// with several relays (ServersOf), a host takes the first that gives it a code and a joiner by a bare code asks each
// in turn till one has its game: one that refuses, or says nothing for kRelayTryMs, gives way to the next
struct Relay {
    std::atomic<bool> on{false};
    sockaddr_in server{};
    std::string server_text;  // as the settings (or the join code) say it
    std::vector<std::string> candidates;  // server_text's, in the order tried
    size_t at = 0;
    int64_t since = 0;  // when server became candidates[at]
    int rounds = 0;     // times every candidate was tried
    std::mutex m;
    char code[kCodeLen + 1] = {};      // a host's from the relay (empty till it answers), a joiner's to join
    uint32_t token = 0;                // a host's
    std::atomic<uint16_t> conn{0};     // a joiner's at the relay
    std::atomic<bool> have_code{false};
    int64_t asked = 0;
    uint8_t refused = 0;
} g_relay;

bool Relayed(const sockaddr_in& a) { return (::ntohl(a.sin_addr.s_addr) >> 24) == 240; }
sockaddr_in RelayStandIn(uint16_t conn) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = ::htons(conn ? conn : 1);
    a.sin_addr.s_addr = ::htonl(240u << 24 | conn);
    return a;
}

std::string RelayCode() {
    std::lock_guard l(g_relay.m);
    return g_relay.have_code.load() || join ? std::string("NF-") + g_relay.code : std::string();
}

// what a host's joiners must give (net_who Only people I tell): through the relay its join code, which a joiner
// through the relay gave it there already, else net_password; empty for an open game
struct Key {
    std::string text;
    bool code = false;
};
Key HostKey() {
    if (!host || Who() != kWhoInvited) return {};
    if (g_relay.on.load()) {
        std::lock_guard l(g_relay.m);
        return {g_relay.have_code.load() ? std::string(g_relay.code) : std::string("?"), true};
    }
    return {HostPassword(), false};
}

void Send(const sockaddr_in& to, const uint8_t* b, size_t n) {
    if (g.sock == INVALID_SOCKET || to.sin_family != AF_INET) return;
    if (Dropped()) return;
    const sockaddr_in* dest = &to;
    const uint8_t* out = b;
    size_t out_n = n;
    uint8_t wrapped[kRelayHeader + kRelayMax];
    if (Relayed(to)) {
        uint16_t conn = join ? g_relay.conn.load() : uint16_t(::ntohs(to.sin_port));
        if (!g_relay.on.load() || !conn || n > kRelayMax) return;
        std::memcpy(wrapped, kRelayMagic, 4);
        wrapped[4] = kRelayData;
        std::memcpy(wrapped + 5, &conn, 2);
        std::memcpy(wrapped + kRelayHeader, b, n);
        dest = &g_relay.server, out = wrapped, out_n = kRelayHeader + n;
    }
    if (int r = ::sendto(g.sock, reinterpret_cast<const char*>(out), int(out_n), 0,
                         reinterpret_cast<const sockaddr*>(dest), sizeof(*dest));
        r < 0) {
        int err = ::WSAGetLastError();
        if (err != 10065 && err != 10051) LOG_FIRST(20, "net: send failed (%d)", err);
    } else if (Tracing()) {
        uint8_t type, player;
        uint32_t seq;
        if (UnpackHeader(b, n, type, player, seq))
            LOGR("net: sent %u bytes, type %u player %u seq %u", r, type, player, seq);
    }
}

// to whoever this machine talks to: a joiner its host (which relays its bundles to its other joiners), the host
// every joiner
void SendOut(const uint8_t* buf, size_t n) {
    if (join) {
        Send(g.host_addr, buf, n);
        return;
    }
    for (int p = 1; p < kMaxPlayers; p++)
        if (g.has_peer[p]) Send(g.peer[p], buf, n);
}

void SendPlain(uint8_t type, uint8_t player, uint32_t seq) {
    uint8_t buf[kHeader];
    SendOut(buf, Pack(buf, type, player, seq));
}

// CheckState's ticks: this machine's words, the others' until this one's are there
struct Checks {
    std::mutex m;
    std::map<uint32_t, std::vector<uint32_t>> own;
    std::map<uint32_t, std::vector<std::pair<int, std::vector<uint32_t>>>> theirs;
    bool said = false;
} g_checks;

std::string Words(const std::vector<uint32_t>& w) {
    std::string s;
    for (uint32_t v : w) {
        char b[12];
        std::snprintf(b, sizeof b, " %08X", v);
        s += b;
    }
    return s;
}

// a player's words against this machine's at the tick (g_checks.m held)
void CompareCheck(uint32_t tick, int player, const std::vector<uint32_t>& theirs) {
    auto own = g_checks.own.find(tick);
    if (own == g_checks.own.end() || g_checks.said || own->second == theirs) return;
    g_checks.said = true;
    LOGR("net: the games differ from player %d's machine at tick %u: this one's%s, theirs%s", player + 1, tick,
         Words(own->second).c_str(), Words(theirs).c_str());
}

void TakeCheck(const uint8_t* b, size_t n, int player, uint32_t tick) {
    std::vector<uint32_t> words(std::min<size_t>((n - kHeader) / 4, kCheckWords));
    std::memcpy(words.data(), b + kHeader, words.size() * 4);
    std::lock_guard lock(g_checks.m);
    if (g_checks.own.count(tick)) return CompareCheck(tick, player, words);
    if (g_checks.theirs.size() < 64) g_checks.theirs[tick].push_back({player, std::move(words)});
}

// the player's tick is missing here: a joiner asks its host, which has every player's input; the host asks the
// joiner it is from (its answer, a bundle, is relayed to every joiner as any is)
void AskAgain(int player, uint32_t tick) {
    uint8_t buf[kHeader];
    size_t n = Pack(buf, kResend, uint8_t(player), tick);
    if (join) Send(g.host_addr, buf, n);
    else if (player >= 1 && player < kMaxPlayers && g.has_peer[player]) Send(g.peer[player], buf, n);
}

// the player plays no input at the tick: they left before it and had not rejoined by it (g.m held)
bool GoneAt(int player, uint32_t tick) { return g.left_at[player] <= tick && tick < g.back_at[player]; }

void MarkLeft(int player, uint32_t tick) {
    std::lock_guard lock(g.m);
    if (player == g.self) return;
    bool again = g.back_at[player] != kStaying && tick >= g.back_at[player];  // they rejoined, then left again
    if (!again && g.left_at[player] <= tick) return;
    bool first = again || g.left_at[player] == kStaying;
    g.left_at[player] = tick;
    if (again) g.back_at[player] = kStaying;
    if (first) LOGR("net: player %d left the session at tick %u: their controller is idle from it", player + 1, tick);
    g.cv.notify_all();
}

uint32_t Address(const std::string& text) {
    if (text.empty()) return 0;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (::getaddrinfo(text.c_str(), nullptr, &hints, &res) == 0 && res) {
        uint32_t a = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr.s_addr;
        ::freeaddrinfo(res);
        return a;
    }
    return 0;
}
sockaddr_in To(uint32_t address, uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = ::htons(port);
    a.sin_addr.s_addr = address;
    return a;
}

uint32_t PrimaryLanAddress() {
    uint32_t addr = htonl(INADDR_LOOPBACK);
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = ::htons(53);
    to.sin_addr.s_addr = htonl(0x08080808);  // route check only, no packet sent
    sockaddr_in me{};
    int len = sizeof(me);
    if (s != INVALID_SOCKET && ::connect(s, reinterpret_cast<sockaddr*>(&to), sizeof(to)) == 0 &&
        ::getsockname(s, reinterpret_cast<sockaddr*>(&me), &len) == 0 && me.sin_addr.s_addr)
        addr = me.sin_addr.s_addr;
    if (s != INVALID_SOCKET) ::closesocket(s);
    return addr;
}

std::string IpString(uint32_t addr_net) {
    uint32_t h = ::ntohl(addr_net);
    char b[32];
    if (h >> 24 == 240) {  // a relayed machine's stand-in: it has no address to show
        if (h & 0xFFFF) std::snprintf(b, sizeof(b), "relay connection %u", h & 0xFFFF);
        else std::snprintf(b, sizeof(b), "the relay");
        return b;
    }
    std::snprintf(b, sizeof(b), "%u.%u.%u.%u", (h >> 24) & 0xFF, (h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF);
    return b;
}

uint32_t g_lan_ip = 0;
uint16_t g_join_port = 0;  // a joiner's: join_ip's port, else net_port

// a joiner on this PC asks through loopback and the LAN broadcast from one socket: its hellos come from 127.0.0.1 and
// from this PC's LAN address, one joiner (the port tells programs on one PC apart)
bool ThisPc(uint32_t a) { return (::ntohl(a) >> 24) == 127 || a == g_lan_ip; }
bool SameEndpoint(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_port == b.sin_port &&
           (a.sin_addr.s_addr == b.sin_addr.s_addr || (ThisPc(a.sin_addr.s_addr) && ThisPc(b.sin_addr.s_addr)));
}

// a relay's host:port; false if it has no address
bool RelayAddress(const std::string& text, sockaddr_in& out) {
    std::string at = text;
    uint16_t p = kRelayPort;
    if (size_t colon = at.rfind(':'); colon != std::string::npos) {
        int v = std::atoi(at.c_str() + colon + 1);
        if (v <= 0 || v >= 65536) return false;
        p = uint16_t(v), at.resize(colon);
    }
    uint32_t a = Address(at);
    if (!a) return false;
    out = To(a, p);
    return true;
}

// join_ip as a join code: NF- and the code, then @ and its relay if the code is that relay's alone (else every
// relay known is asked for it); "" if it is no code
std::string JoinCode(const std::string& text, std::string& relay) {
    if (text.size() < 3 + kCodeLen || (text[0] | 0x20) != 'n' || (text[1] | 0x20) != 'f' || text[2] != '-') return {};
    std::string code;
    for (size_t i = 3; i < 3 + kCodeLen; i++) {
        if (!std::isalnum(uint8_t(text[i]))) return {};
        code += char(std::toupper(uint8_t(text[i])));
    }
    if (text.size() > 3 + kCodeLen) {
        if (text[3 + kCodeLen] != '@') return {};
        relay = text.substr(4 + kCodeLen);
    }
    return code;
}

// the host's code and token in the environment: the exe it relaunches into (and F1's restart) asks for the same code
void KeepRelayCode() {
    char v[32 + kServerTextMax];
    std::snprintf(v, sizeof v, "%s:%u@%s", g_relay.code, g_relay.token, g_relay.server_text.c_str());
#ifdef _WIN32
    _putenv_s("NF_NET_RELAY_CODE", v);
#else
    setenv("NF_NET_RELAY_CODE", v, 1);
#endif
}

// the receive thread's: a host's registration and a joiner's ask, again every kRelayAskMs till the relay answers,
// then every kRelayEveryMs (the relay forgets what it has not heard of for a minute)
// g_relay.m held: candidates[at] as the relay, from candidates[from] on; false if none of them has an address
bool UseRelay(size_t from, int64_t now) {
    for (size_t i = 0; i < g_relay.candidates.size(); i++) {
        size_t at = (from + i) % g_relay.candidates.size();
        if (at == 0 && (from + i) > 0) g_relay.rounds++;
        if (!RelayAddress(g_relay.candidates[at], g_relay.server)) {
            if (!g_relay.rounds) LOGE("net: the relay %s has no address", g_relay.candidates[at].c_str());
            continue;
        }
        g_relay.at = at, g_relay.since = now, g_relay.server_text = g_relay.candidates[at];
        return true;
    }
    return false;
}

void RelayKeep() {
    if (!g_relay.on.load()) return;
    int64_t now = NowMs();
    std::lock_guard l(g_relay.m);
    bool through = host ? g_relay.have_code.load() : g_relay.conn.load() != 0;
    if (!through && g_relay.candidates.size() > 1 && now - g_relay.asked >= kRelayAskMs &&
        (g_relay.refused || now - g_relay.since >= kRelayTryMs)) {
        int rounds = g_relay.rounds;
        UseRelay(g_relay.at + 1, now);
        g_relay.refused = 0;
        // every relay asked: the next round 5 s on, as one relay is asked again
        g_relay.asked = g_relay.rounds != rounds ? now + kRelayEveryMs - kRelayAskMs : 0;
        if (g_relay.rounds == 1 && rounds == 0)
            LOGE("net: none of the %zu relays %s yet; asking them again every few seconds", g_relay.candidates.size(),
                 host ? "gave this host a code" : ("has the game NF-" + std::string(g_relay.code)).c_str());
        else if (g_relay.rounds == 0)
            LOGR("net: asking the relay at %s instead%s", g_relay.server_text.c_str(),
                 host ? "" : (" for the game NF-" + std::string(g_relay.code)).c_str());
    }
    bool answered = through || g_relay.refused;
    if (now - g_relay.asked < (answered ? kRelayEveryMs : kRelayAskMs)) return;
    g_relay.asked = now;
    uint8_t p[5 + 4 + kCodeLen] = {};
    std::memcpy(p, kRelayMagic, 4);
    size_t n;
    if (host) {
        p[4] = kRelayRegister;
        std::memcpy(p + 5, &g_relay.token, 4);
        std::memcpy(p + 9, g_relay.code, kCodeLen);
        n = 5 + 4 + kCodeLen;
    } else {
        p[4] = kRelayJoin;
        std::memcpy(p + 5, g_relay.code, kCodeLen);
        uint16_t conn = g_relay.conn.load();
        std::memcpy(p + 5 + kCodeLen, &conn, 2);
        n = 5 + kCodeLen + 2;
    }
    ::sendto(g.sock, reinterpret_cast<const char*>(p), int(n), 0, reinterpret_cast<const sockaddr*>(&g_relay.server),
             sizeof(g_relay.server));
}

// a packet from the relay: true for a session's packet, unwrapped in place with its sender's stand-in address
bool FromRelay(uint8_t* buf, int& n, sockaddr_in& from) {
    uint8_t type = buf[4];
    if (type == kRelayData) {
        if (n < int(kRelayHeader)) return false;
        uint16_t conn;
        std::memcpy(&conn, buf + 5, 2);
        if (!conn || (join && conn != g_relay.conn.load())) return false;
        n -= int(kRelayHeader);
        std::memmove(buf, buf + kRelayHeader, size_t(n));
        from = RelayStandIn(join ? 0 : conn);
        return true;
    }
    std::lock_guard l(g_relay.m);
    if (type == kRelayCode && host && n >= int(5 + kCodeLen)) {
        if (!g_relay.have_code.load() || std::memcmp(g_relay.code, buf + 5, kCodeLen) != 0) {
            std::memcpy(g_relay.code, buf + 5, kCodeLen);
            g_relay.have_code = true;
            KeepRelayCode();
            LOGR("net: hosting through the relay at %s: join code NF-%s (players type it as the host's address; "
                 "none of them sees this PC's, and no port needs forwarding)", g_relay.server_text.c_str(), g_relay.code);
        }
        g_relay.refused = 0;
    } else if (type == kRelayJoined && join && n >= int(7 + kCodeLen) &&
               std::memcmp(buf + 7, g_relay.code, kCodeLen) == 0) {
        uint16_t conn;
        std::memcpy(&conn, buf + 5, 2);
        if (conn && conn != g_relay.conn.load())
            LOGR("net: the relay at %s has the game NF-%s; asking its host to play", g_relay.server_text.c_str(),
                 g_relay.code);
        if (conn) g_relay.conn = conn;
        g_relay.refused = 0;
    } else if (type == kRelayRefused && n >= 6) {
        if (g_relay.refused != buf[5] && g_relay.candidates.size() <= 1)
            LOGE("net: the relay at %s refused %s: %s", g_relay.server_text.c_str(),
                 host ? "this host" : ("the code NF-" + std::string(g_relay.code)).c_str(),
                 buf[5] == kRefusedNoGame ? "no game has that code (it may have ended, or be on another relay)"
                 : buf[5] == kRefusedFull ? "that game has all the players the relay takes"
                                          : "the relay is full");
        g_relay.refused = buf[5];
        if (join && buf[5] == kRefusedNoGame) g_relay.conn = 0;
    }
    return false;
}

// a session's packets come from its own machines alone: a joiner hears its host, the host each joiner at the address
// it joined from. Another session on this PC or the LAN, on the same port, must not play in this one
bool FromSession(const sockaddr_in& from, uint8_t player) {
    std::lock_guard lock(g.m);
    if (join) return SameEndpoint(from, g.host_addr);
    return player >= 1 && player < kMaxPlayers && g.has_peer[player] && SameEndpoint(from, g.peer[player]);
}
// a request to send again names the player whose tick it wants, not the asker: the host hears it from any joiner
bool FromAnyOfSession(const sockaddr_in& from) {
    std::lock_guard lock(g.m);
    if (join) return SameEndpoint(from, g.host_addr);
    for (int p = 1; p < kMaxPlayers; p++)
        if (g.has_peer[p] && SameEndpoint(from, g.peer[p])) return true;
    return false;
}

// a bundle's ticks into the player's stream: those not yet applied and not already in it (g.m held)
void TakeBundle(const uint8_t* buf, size_t n, uint8_t player, uint32_t seq) {
    if (n < kHeader + 1) return;
    int count = buf[kHeader];
    if (count < 1 || count > kRedundant || uint32_t(count - 1) > seq || n != kHeader + 1 + kBody * size_t(count)) return;
    Stream& s = g.stream[player];
    for (int k = 0; k < count; k++) {
        uint32_t tick = seq - uint32_t(k);
        if (tick < s.next || s.known.count(tick)) continue;
        UnpackBody(buf + kHeader + 1 + kBody * size_t(k), s.known[tick]);
    }
}

// a machine asks to play in the session under way: a seat whose player left is offered to it, the one their name
// was first (one rejoin at a time)
void OfferSeat(const sockaddr_in& from, const uint8_t* buf, size_t n) {
    if (!g_rejoinable || !g.active || g.leaving) return;
    uint8_t name[kNameLen] = {};
    if (n >= kHeader + kNameLen) PackName(name, reinterpret_cast<const char*>(buf + kHeader), kNameLen);
    int seat = -1;
    {
        std::lock_guard lock(g.m);
        std::lock_guard tx(g_tx.m);
        if (g_tx.seat >= 0) {
            if (!SameEndpoint(from, g_tx.to)) return;
            seat = g_tx.seat;
        } else {
            uint32_t now = g.tick_now > 0 ? uint32_t(g.tick_now) : 0;
            for (int pass = 0; pass < 2 && seat < 0; pass++)
                for (int p = 1; p < g.players && seat < 0; p++)
                    if (g.has_peer[p] && GoneAt(p, now) &&
                        (pass || (name[0] && std::memcmp(g.peer_name[p], name, kNameLen) == 0)))
                        seat = p;
            if (seat < 0) return;
            g_tx.seat = seat, g_tx.to = from, g_tx.ready = false, g_tx.id = 0;
            std::lock_guard l(g_saves.m);
            PackSaves();  // as they are now: the session's machines have written theirs alike since boot
            LOGR("net: a machine at %s asks to play: it is offered controller %d, whose player left",
                 IpString(from.sin_addr.s_addr).c_str(), seat + 1);
        }
    }
    uint8_t r[kHeader];
    Send(from, r, Pack(r, kRejoin, uint8_t(seat), delay));
}

// kRejoin .. kBackAck as they arrive
void Rejoining(const uint8_t* buf, size_t n, uint8_t type, uint8_t player, uint32_t seq, const sockaddr_in& from) {
    if (host) {
        if (type == kBackAck) {
            if (!FromSession(from, player)) return;
            std::lock_guard tx(g_tx.m);
            if (g_tx.id && seq == g_tx.id) g_tx.back_acked[player] = true;
            g_tx.cv.notify_all();
            return;
        }
        std::lock_guard tx(g_tx.m);
        if (g_tx.seat < 0 || !SameEndpoint(from, g_tx.to)) return;
        if (type == kReady) {
            if (!g_tx.ready) LOGR("net: controller %d's machine is ready for the session's game", g_tx.seat + 1);
            g_tx.ready = true;
        } else if (type == kLoaded && g_tx.id && seq == g_tx.id) {
            g_tx.loaded = true;
        } else if (type == kChunkAck && g_tx.id && n >= kHeader + 4 + kAckBits / 8) {
            uint32_t id;
            std::memcpy(&id, buf + kHeader, 4);
            if (id != g_tx.id) return;
            const uint8_t* bits = buf + kHeader + 4;
            uint32_t total = uint32_t(g_tx.acked.size());
            auto mark = [&](uint32_t i) {
                if (i < total && !g_tx.acked[i]) g_tx.acked[i] = 1, g_tx.acked_count++;
            };
            for (uint32_t i = 0; i < std::min(seq, total); i++) mark(i);
            g_tx.window = std::max(g_tx.window, seq);
            for (uint32_t k = 0; k < kAckBits; k++)
                if ((bits[k / 8] >> (k % 8)) & 1) mark(seq + k);
        }
        g_tx.cv.notify_all();
        return;
    }
    if (type == kRejoin) {  // asking before boot, as any joiner: the host's session is under way
        if (g_booting.load() || g.active || player < 1 || player >= kMaxPlayers) return;
        std::lock_guard lock(g.m);
        std::lock_guard tx(g_tx.m);
        if (!g_tx.rejoining)
            LOGR("net: the host at %s has a session under way: this machine takes controller %d again, booting "
                 "alone until the session's game is here",
                 IpString(from.sin_addr.s_addr).c_str(), player + 1);
        g.host_addr = from, g.self = player;
        delay = std::clamp<uint32_t>(seq, 1, 12);
        g_tx.rejoining = true;
        g.cv.notify_all();
        return;
    }
    if (!SameEndpoint(from, g.host_addr)) return;
    if (type == kBack) {  // another player rejoins: the others learn the tick before the host's game goes on
        if (!g_booting.load() || !g.active || player >= kMaxPlayers) return;
        {
            std::lock_guard lock(g.m);
            if (g.back_at[player] != seq && seq > g.left_at[player]) {
                g.back_at[player] = seq;
                LOGR("net: player %d rejoins the session at tick %u", player + 1, seq);
            }
        }
        uint8_t a[kHeader];
        Send(from, a, Pack(a, kBackAck, uint8_t(g.self), seq));
        return;
    }
    if (type != kChunk || n < kHeader + 8) return;
    uint32_t id, total;
    std::memcpy(&id, buf + kHeader, 4), std::memcpy(&total, buf + kHeader + 4, 4);
    std::lock_guard tx(g_tx.m);
    if (!g_tx.rejoining || !total || total > (1u << 30)) return;
    uint32_t chunks = uint32_t((total + kChunkData - 1) / kChunkData);
    if (id != g_tx.id || g_tx.data.size() != total) {
        g_tx.id = id, g_tx.data.assign(total, 0), g_tx.have.assign(chunks, 0);
        g_tx.got = g_tx.first = 0, g_tx.complete = false;
    }
    if (seq < chunks && !g_tx.have[seq]) {
        size_t at = size_t(seq) * kChunkData, len = std::min(kChunkData, size_t(total) - at);
        if (n != kHeader + 8 + len) return;
        std::memcpy(&g_tx.data[at], buf + kHeader + 8, len);
        g_tx.have[seq] = 1;
        while (g_tx.first < chunks && g_tx.have[g_tx.first]) g_tx.first++;
        if (++g_tx.got == chunks) {
            g_tx.complete = true;
            LOGR("net: the session's game is here (%u KB)", total / 1024);
        }
        g_tx.cv.notify_all();
    }
    if (int64_t now = NowMs(); g_tx.complete || now - g_tx.last_ack >= 10) {
        g_tx.last_ack = now;
        uint8_t a[kHeader + 4 + kAckBits / 8] = {};
        Pack(a, kChunkAck, uint8_t(g.self), g_tx.first);
        std::memcpy(a + kHeader, &id, 4);
        for (uint32_t k = 0; k < kAckBits && g_tx.first + k < chunks; k++)
            if (g_tx.have[g_tx.first + k]) a[kHeader + 4 + k / 8] |= uint8_t(1u << (k % 8));
        Send(from, a, sizeof a);
    }
}

// kLaunch, between the two exes: a Driving.xbe host hears a default.xbe machine's kHello (that exe's version) while
// a seat is free and sends it the launch data in its version; that machine keeps it for Meet's relaunch. Anything
// else in the other exe's version is ignored, as before
bool OtherExe(const uint8_t* buf, size_t n, const sockaddr_in& from) {
    uint32_t magic, seq;
    uint16_t version;
    if (n < kHeader) return false;
    std::memcpy(&magic, buf, 4), std::memcpy(&version, buf + 4, 2), std::memcpy(&seq, buf + 8, 4);
    uint16_t other = uint16_t(kVersion + (kDriving ? 0 : 1));
    uint8_t type = buf[6];
    if (magic != kMagic) return false;
    if (version == WireVersion()) {  // kLaunch comes in this exe's version, from a host in the other
        if (type != kLaunch) return false;
        if (kDriving || !join || n != kHeader + kLaunchPiece || seq >= kLaunchPage || seq % kLaunchPiece ||
            g_booting.load() || g_launch.complete.load())
            return true;
        std::lock_guard l(g_launch.m);
        std::memcpy(g_launch.page + seq, buf + kHeader, kLaunchPiece);
        g_launch.have[seq / kLaunchPiece] = true;
        bool all = true;
        for (bool h : g_launch.have) all &= h;
        if (all) g_launch.complete.store(true);
        return true;
    }
    if (version != other) return false;
    if (kDriving && host && type == kHello && g_rejoinable && g.active && g_booting.load() && g_launch.own) {
        bool free_seat = false;
        {
            std::lock_guard lock(g.m);
            uint32_t now = g.tick_now > 0 ? uint32_t(g.tick_now) : 0;
            for (int p = 1; p < g.players; p++) free_seat |= g.has_peer[p] && GoneAt(p, now);
        }
        if (!free_seat) return true;
        static int said = 0;
        if (said++ < 3)
            LOGR("net: a machine at %s in default.xbe asks to play: it is sent this mission's launch data",
                 IpString(from.sin_addr.s_addr).c_str());
        uint8_t pkt[kHeader + kLaunchPiece];
        for (uint32_t at = 0; at < kLaunchPage; at += kLaunchPiece) {
            Pack(pkt, kLaunch, 0, at);
            std::memcpy(pkt + 4, &other, 2);
            std::memcpy(pkt + kHeader, g_launch.page + at, kLaunchPiece);
            Send(from, pkt, sizeof pkt);
        }
    }
    return true;
}

// the LAN browser (FindGames): a query any PC sends to the LAN's broadcast and this PC's loopback on the game's
// port, and each host's answer. Outside the session's packets and their version, so a browser hears hosts of
// other builds too (and greys them). A query: kFindMagic, kFindQuery, a token the answer echoes (the ping). An
// answer: kFindMagic, kFindInfo, the token, the wire version (kVersion, either exe), flags, the players in, the
// players wanted, the host's net_name, the host's id (one host heard at two addresses, the LAN's and the internet's)
constexpr uint32_t kFindMagic = 0x4E464631;  // "NFF1"
constexpr uint8_t kFindQuery = 1, kFindInfo = 2;
// kFindPassword: it asks for a key (HostKey), kFindCode: the key is its join code
constexpr uint8_t kFindUnderWay = 1, kFindSeatFree = 2, kFindDriving = 4, kFindPassword = 8, kFindCode = 16;
constexpr size_t kFindQuerySize = 4 + 1 + 4, kFindInfoSize = kFindQuerySize + 2 + 1 + 1 + 1 + kNameLen + 4;

uint32_t g_host_id = 0;  // a host's, random at Init

struct HostNow {
    uint8_t flags, in, wanted;
    char name[kNameLen + 1];
};
HostNow HostState() {
    Key key = HostKey();
    HostNow h{uint8_t((kDriving ? kFindDriving : 0) | (key.text.empty() ? 0 : kFindPassword) | (key.code ? kFindCode : 0)),
              1, 0, {}};
    std::lock_guard lock(g.m);
    if (g_booting.load() && g.active) {
        h.flags |= kFindUnderWay;
        h.in = uint8_t(g.players);
        uint32_t now = g.tick_now > 0 ? uint32_t(g.tick_now) : 0;
        for (int p = 1; p < g.players; p++)
            if (g.has_peer[p] && GoneAt(p, now)) h.in--, h.flags |= g_rejoinable && !g.leaving ? kFindSeatFree : 0;
    } else {
        for (int p = 1; p < kMaxPlayers; p++) h.in += g.has_peer[p];
    }
    h.wanted = uint8_t(std::max(g.players, g_target));
    std::memcpy(h.name, g.own_name, kNameLen);
    return h;
}

// a host: its answer to a query
void AnswerFind(const uint8_t* buf, const sockaddr_in& from) {
    uint8_t a[kFindInfoSize] = {};
    std::memcpy(a, &kFindMagic, 4);
    a[4] = kFindInfo;
    std::memcpy(a + 5, buf + 5, 4);
    uint16_t version = kVersion;
    std::memcpy(a + 9, &version, 2);
    HostNow h = HostState();
    a[11] = h.flags, a[12] = h.in, a[13] = h.wanted;
    std::memcpy(a + 14, h.name, kNameLen);
    std::memcpy(a + 14 + kNameLen, &g_host_id, 4);
    Send(from, a, sizeof a);
}

// the internet game lists (ServerLists): servers anyone runs (tools/master: master.py on a VPS, worker.js on
// Cloudflare). A host whose net_who lists it announces its game there over HTTP, every kAnnounceEveryMs and when its
// players change; the list's address for it is the one its announcement came from. A browser fetches the list and
// asks each game on it as it asks the LAN's (a query to its address and port), for its state and its ping
constexpr int64_t kAnnounceEveryMs = 60000, kAnnounceSoonestMs = 5000, kAnnounceRetryMs = 10000;
constexpr int64_t kListEveryMs = 15000, kListRetryMs = 3000;

struct Lister {
    std::mutex post;  // one request at a time: an unlist is never followed by an announce in flight
    std::atomic<bool> on{false};
    std::string url;
    // the list server asked the game from outside (master.py; worker.js cannot): 1 it answered, 0 it did not (its
    // port is not forwarded), -1 not known (through a relay, or a list that does not ask)
    std::atomic<int> reach{-1};
};
std::vector<std::unique_ptr<Lister>> g_lists;  // one a server list (ServerLists), each announced to on its own

void AnnounceLoop(Lister* list) {
    Lister& self = *list;
    std::string last;
    int64_t at = -kAnnounceEveryMs, failed_at = -kAnnounceRetryMs;
    bool said = false, failing = false;  // a list down is said once, till it takes the game
    while (g_alive.load() && self.on.load()) {
        int64_t now = NowMs();
        HostNow h = HostState();
        char form[128];
        std::snprintf(form, sizeof form, "port=%u&version=%u&flags=%u&in=%u&wanted=%u&id=%u&name=", port, kVersion,
                      h.flags, h.in, h.wanted, g_host_id);
        std::string body = form + http::Encode(h.name);
        // the list shows its code and its relay, not the address it announced from; Only people I tell: its relay
        // alone, the code "-"
        if (g_relay.on.load() && (h.flags & kFindCode)) {
            body += "&code=-&relay=" + http::Encode(g_relay.server_text);
        } else if (g_relay.on.load()) {
            std::string code = RelayCode();
            if (code.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }
            body += "&code=" + code + "&relay=" + http::Encode(g_relay.server_text);
        }
        bool due = now - at >= kAnnounceEveryMs || (body != last && now - at >= kAnnounceSoonestMs);
        if (due && now - failed_at >= kAnnounceRetryMs) {
            std::lock_guard l(self.post);
            if (!self.on.load()) break;
            std::string reply;
            int status = http::Request("POST", self.url + "/announce", body, reply);
            at = now;
            if (status == 200) {
                last = body;
                failing = false;
                if (!said) LOGR("net: this game is on the internet game list at %s", self.url.c_str());
                said = true;
                // "ok", the address it saw, then reachable or unreachable when it asked the game
                std::vector<std::string> f;
                for (size_t p = 0, tab; p <= reply.size(); p = tab + 1) {
                    tab = reply.find_first_of("\t\r\n", p);
                    if (tab == std::string::npos) tab = reply.size();
                    f.push_back(reply.substr(p, tab - p));
                }
                int reach = f.size() >= 3 && f[2] == "reachable" ? 1 : f.size() >= 3 && f[2] == "unreachable" ? 0 : -1;
                if (reach >= 0 && reach != self.reach.exchange(reach)) {
                    if (reach)
                        LOGR("net: the server list reached this game at %s UDP %u from outside: players anywhere can join",
                             f[1].c_str(), port);
                    else
                        LOGE("net: the server list could not reach this game at %s UDP %u from outside: forward that "
                             "UDP port on the router to this PC (or host through the relay), or only this network's "
                             "players can join", f[1].c_str(), port);
                }
            } else {
                failed_at = now;
                reply.erase(reply.find_last_not_of("\r\n") + 1);
                if (!std::exchange(failing, true)) LOGE("net: the internet game list at %s did not take this game (%d%s%s)", self.url.c_str(), status,
                     reply.empty() ? "" : ": ", reply.substr(0, 60).c_str());
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void StartListing() {
    if (!host) return;
    uint32_t w = Who();
    if (w == kWhoInvited && !g_relay.on.load() && HostPassword().empty())
        LOGE("net: Who can join is Only people I tell, but this game has no password (and no relay): anyone who "
             "finds it can join");
    if (w == kWhoNotListed) return;
    std::vector<std::string> urls = ServerLists();
    if (urls.empty()) {
        LOGE("net: Who can join lists this game, but the Server Browser's game list (net_master) is none: it is on "
             "this network's list alone");
        return;
    }
    if (!g_lists.empty()) return;
    for (const std::string& url : urls) {
        g_lists.push_back(std::make_unique<Lister>());
        g_lists.back()->url = url;
        g_lists.back()->on = true;
        std::thread(AnnounceLoop, g_lists.back().get()).detach();
    }
}

bool Listed() {
    return std::any_of(g_lists.begin(), g_lists.end(), [](const auto& l) { return l->on.load(); });
}
// whether the internet reaches this host: 1 a list reached it, 0 one could not and none did, -1 not known
int Reach() {
    int r = -1;
    for (const auto& l : g_lists) r = std::max(r, l->reach.load());
    return r;
}

// off every list at once, each given a moment
void Unlist() {
    std::vector<std::thread> off;
    for (auto& l : g_lists)
        if (l->on.exchange(false))
            off.emplace_back([list = l.get()] {
                std::lock_guard lock(list->post);
                std::string reply;
                http::Request("POST", list->url + "/remove", "port=" + std::to_string(port), reply, 1500);
            });
    for (std::thread& t : off) t.join();
}

bool PrivateAddress(uint32_t a) {
    uint32_t h = ::ntohl(a);
    uint8_t b0 = h >> 24, b1 = (h >> 16) & 0xFF;
    return !a || b0 == 10 || b0 == 127 || (b0 == 172 && (b1 & 0xF0) == 16) || (b0 == 192 && b1 == 168) ||
           (b0 == 169 && b1 == 254);
}

// the browser's side: its own socket, while FindGames is called
struct Found {
    std::string name;
    uint32_t addr = 0;
    uint16_t port = 0;  // host order
    uint16_t version = 0;
    uint8_t flags = 0, in = 0, wanted = 0;
    uint32_t id = 0;
    int ping = -1;
    uint32_t token = 0;  // the query it answered
    int64_t heard = 0;
    std::string code, relay;  // a game through a relay (addr and port 0); code "-": its host gives it to its players
};
uint64_t FoundKey(uint32_t addr, uint16_t port) { return uint64_t(addr) << 16 | port; }
struct Finder {
    std::mutex m;
    std::map<uint64_t, Found> games;  // what answered, by the host's address and port
    std::vector<Found> listed;        // the internet lists', as they say (heard: when fetched), one entry a game
    std::map<std::string, std::pair<std::vector<Found>, std::string>> lists;  // each list's games, or its error
    std::string list_error;           // every list's error alike: none answered
    std::vector<std::string> ask;      // AskGames': the browser's favourites and history, asked where they are
    bool ask_changed = false;
    std::set<uint64_t> asked;          // those resolved, by address and port
    std::atomic<int64_t> until{0};     // the last FindGames call's time, plus kFindKeepMs
    std::atomic<bool> running{false};
    std::atomic<bool> refresh{false}, list_refresh{false};  // RefreshGames: forget and ask again, fetch the list now
} g_find;
constexpr int64_t kFindKeepMs = 4000, kFindEveryMs = 1000, kFindGoneMs = 3500;

// g_find.m held: listed from every list's games, a game on two lists once
void MergeListed() {
    g_find.listed.clear();
    std::set<std::string> seen;
    size_t failed = 0;
    std::string error;
    for (const auto& [url, list] : g_find.lists) {
        if (!list.second.empty()) failed++, error = list.second + " (" + url + ")";
        for (const Found& f : list.first)
            if (seen.insert(std::to_string(f.id) + " " + std::to_string(FoundKey(f.addr, f.port)) + " " + f.code).second)
                g_find.listed.push_back(f);
    }
    g_find.list_error = failed < g_find.lists.size() || !failed ? std::string()
                        : failed == 1 ? error
                                      : "no answer from any of the " + std::to_string(failed) + " server lists";
}

// the browser's fetch of an internet list, every kListEveryMs while the finder runs
void ListLoop(std::string url) {
    while (g_find.running.load() && NowMs() < g_find.until.load()) {
        std::string reply, error;
        int status = http::Request("GET", url + "/list", "", reply);
        std::vector<Found> listed;
        if (status != 200 || reply.rfind("NFM1", 0) != 0) {
            error = status ? "the list answered " + std::to_string(status) : "no answer from the list";
        } else {
            size_t at = reply.find('\n');
            while (at != std::string::npos && at + 1 < reply.size()) {
                size_t end = reply.find('\n', at + 1);
                std::string line = reply.substr(at + 1, end == std::string::npos ? std::string::npos : end - at - 1);
                at = end;
                std::vector<std::string> f;
                for (size_t p = 0;;) {
                    size_t tab = line.find('\t', p);
                    f.push_back(line.substr(p, tab == std::string::npos ? std::string::npos : tab - p));
                    if (tab == std::string::npos) break;
                    p = tab + 1;
                }
                if (f.size() < 8) continue;
                Found g;
                g.addr = f[0] == "-" ? 0 : Address(f[0]);
                g.port = uint16_t(std::atoi(f[1].c_str()));
                g.version = uint16_t(std::atoi(f[2].c_str()));
                g.flags = uint8_t(std::atoi(f[3].c_str())), g.in = uint8_t(std::atoi(f[4].c_str()));
                g.wanted = uint8_t(std::atoi(f[5].c_str()));
                g.id = uint32_t(std::strtoul(f[6].c_str(), nullptr, 10));
                g.name = f[7].substr(0, kNameLen);
                g.heard = NowMs();
                if (f.size() >= 11 && !f[9].empty()) {  // a game through a relay: its code ("-" withheld), no address
                    std::string relay;
                    if ((f[9] != "-" && JoinCode(f[9], relay).empty()) || f[10].empty()) continue;
                    g.addr = 0, g.port = 0;
                    g.code = f[9], g.relay = f[10];
                    listed.push_back(std::move(g));
                } else if (g.addr && g.port) {
                    listed.push_back(std::move(g));
                }
            }
        }
        {
            std::lock_guard l(g_find.m);
            auto& mine = g_find.lists[url];
            mine.second = error;
            if (error.empty()) mine.first = std::move(listed);
            MergeListed();
        }
        int64_t again = error.empty() ? kListEveryMs : kListRetryMs;
        for (int64_t t = NowMs(); g_find.running.load() && NowMs() - t < again && NowMs() < g_find.until.load() &&
                                  !g_find.list_refresh.exchange(false);)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void FindLoop() {
    WSADATA wsa;
    ::WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        LOG_FIRST(3, "net: the game list has no socket (%d)", ::WSAGetLastError());
        g_find.running = false;
        return;
    }
    int bcast = 1;
    ::setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bcast), sizeof(bcast));
    u_long one = 1;
    ::ioctlsocket(s, FIONBIO, &one);
    sockaddr_in any = To(INADDR_ANY, 0);
    ::bind(s, reinterpret_cast<sockaddr*>(&any), sizeof(any));
    uint32_t to_port = port;
    if (const char* e = std::getenv("NF_NET_PORT"); e && *e) to_port = uint32_t(std::atoi(e));
    uint32_t lan = PrimaryLanAddress();
    sockaddr_in targets[] = {To(INADDR_BROADCAST, uint16_t(to_port)),
                             To((lan & htonl(0xFFFFFF00u)) | htonl(0x000000FFu), uint16_t(to_port)),
                             To(htonl(INADDR_LOOPBACK), uint16_t(to_port))};
    std::vector<std::string> urls = ServerLists();
    AskDirectory();
    {
        std::lock_guard l(g_find.m);
        g_find.lists.clear();
        for (const std::string& url : urls) g_find.lists[url];
        MergeListed();
    }
    std::vector<std::thread> lists;
    for (const std::string& url : urls) lists.emplace_back(ListLoop, url);
    std::map<uint32_t, int64_t> sent;  // token -> when
    uint32_t token = 0;
    int64_t last = 0;
    std::vector<sockaddr_in> asked;  // the favourites' and history's addresses (join codes are the list's to show)
    while (NowMs() < g_find.until.load()) {
        int64_t now = NowMs();
        std::vector<std::string> ask;
        bool changed = false;
        {
            std::lock_guard l(g_find.m);
            if ((changed = g_find.ask_changed)) ask = g_find.ask, g_find.ask_changed = false;
            if (g_find.refresh.exchange(false)) g_find.games.clear(), last = 0;
        }
        if (changed) {  // a name to look up takes its time: outside the lock
            asked.clear();
            std::set<uint64_t> keys;
            for (std::string text : ask) {
                std::string relay;
                if (!JoinCode(text, relay).empty()) continue;
                uint16_t p = uint16_t(to_port);
                if (size_t colon = text.rfind(':'); colon != std::string::npos && text.find(':') == colon) {
                    if (int q = std::atoi(text.c_str() + colon + 1); q > 0 && q < 65536) p = uint16_t(q);
                    text.resize(colon);
                }
                if (uint32_t a = Address(text)) asked.push_back(To(a, p)), keys.insert(FoundKey(a, p));
            }
            std::lock_guard l(g_find.m);
            g_find.asked = std::move(keys);
        }
        if (now - last >= kFindEveryMs) {
            last = now;
            sent[++token] = now;
            if (sent.size() > 8) sent.erase(sent.begin());
            uint8_t q[kFindQuerySize];
            std::memcpy(q, &kFindMagic, 4);
            q[4] = kFindQuery;
            std::memcpy(q + 5, &token, 4);
            std::vector<sockaddr_in> to(std::begin(targets), std::end(targets));
            std::lock_guard l(g_find.m);
            for (const Found& f : g_find.listed)
                if (f.addr) to.push_back(To(f.addr, f.port));
            to.insert(to.end(), asked.begin(), asked.end());
            for (const sockaddr_in& t : to)
                ::sendto(s, reinterpret_cast<const char*>(q), int(sizeof q), 0, reinterpret_cast<const sockaddr*>(&t),
                         sizeof(t));
            for (auto it = g_find.games.begin(); it != g_find.games.end();)
                it = now - it->second.heard > kFindGoneMs ? g_find.games.erase(it) : std::next(it);
        }
        uint8_t b[kFindInfoSize + 16];
        sockaddr_in from{};
        int len = sizeof(from);
        int n = ::recvfrom(s, reinterpret_cast<char*>(b), int(sizeof b), 0, reinterpret_cast<sockaddr*>(&from), &len);
        uint32_t magic = 0;
        if (n >= int(kFindInfoSize)) std::memcpy(&magic, b, 4);
        if (n < int(kFindInfoSize) || magic != kFindMagic || b[4] != kFindInfo) {
            if (n <= 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        uint32_t t;
        std::memcpy(&t, b + 5, 4);
        // a host on this PC answers the loopback and the broadcast both: one entry, at 127.0.0.1
        uint32_t addr = ThisPc(from.sin_addr.s_addr) || from.sin_addr.s_addr == lan ? htonl(INADDR_LOOPBACK)
                                                                                   : from.sin_addr.s_addr;
        Found f;
        char name[kNameLen + 1] = {};
        std::memcpy(name, b + 14, kNameLen);
        f.name = name;
        f.addr = addr;
        f.port = ntohs(from.sin_port);
        std::memcpy(&f.version, b + 9, 2);
        f.flags = b[11], f.in = b[12], f.wanted = b[13];
        std::memcpy(&f.id, b + 14 + kNameLen, 4);
        f.heard = NowMs();
        f.token = t;
        auto at = sent.find(t);
        std::lock_guard l(g_find.m);
        Found& slot = g_find.games[FoundKey(addr, f.port)];
        f.ping = at != sent.end() ? int(f.heard - at->second) : slot.ping;
        if (slot.token == t && slot.ping >= 0) f.ping = std::min(f.ping, slot.ping);  // one query's two answers
        slot = f;
    }
    ::closesocket(s);
    for (std::thread& t : lists) t.join();
    std::lock_guard l(g_find.m);
    g_find.games.clear();
    g_find.listed.clear();
    g_find.lists.clear();
    g_find.asked.clear();
    g_find.ask_changed = true;  // the next run looks them up again
    g_find.running = false;
}

std::atomic<uint32_t> g_skipped{0};     // the last movie a kSkip (or this machine's SkipMovie) named
std::atomic<uint32_t> g_skip_sent{0};   // this machine's, sent every kSkipEvery ms till g_skip_until
std::atomic<int64_t> g_skip_until{0};
constexpr int64_t kSkipEvery = 100, kSkipFor = 1000;

void ResendSkip() {
    static int64_t last = 0;
    int64_t now = NowMs();
    if (now >= g_skip_until.load() || now - last < kSkipEvery) return;
    last = now;
    SendPlain(kSkip, uint8_t(g.self), g_skip_sent.load());
}

// the receive thread: bundles into their streams, the host relaying each to its other joiners, and the host's
// answers to a joiner's hello before boot. Nothing here touches the game: only the session's streams and tables
void Receive() {
    uint8_t buf[kRelayHeader + kRelayMax];
    int64_t last_alive = 0;
    while (g_alive.load()) {
        ResendSkip();
        RelayKeep();
        sockaddr_in from{};
        int len = sizeof(from);
        int n = ::recvfrom(g.sock, reinterpret_cast<char*>(buf), sizeof buf, 0,
                           reinterpret_cast<sockaddr*>(&from), &len);
        if (n >= 5 && std::memcmp(buf, kRelayMagic, 4) == 0) {
            if (!g_relay.on.load() || !SameEndpoint(from, g_relay.server) || !FromRelay(buf, n, from)) continue;
        }
        if (n <= 0) {
            int err = n ? ::WSAGetLastError() : 0;
            if (n < 0 && err != WSAEWOULDBLOCK && err != WSAECONNRESET && err != WSAEINVAL)
                LOG_FIRST(20, "net: recv failed (%d)", err);
            if (int64_t now = NowMs(); g_booting.load() && g.active && now - last_alive >= kAliveMs) {
                last_alive = now;
                SendPlain(kAlive, uint8_t(g.self), 0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        uint8_t type, player;
        uint32_t seq;
        if (uint32_t magic = 0; n >= int(kFindQuerySize) && (std::memcpy(&magic, buf, 4), magic == kFindMagic)) {
            if (host && buf[4] == kFindQuery) AnswerFind(buf, from);
            continue;
        }
        if (OtherExe(buf, size_t(n), from)) continue;
        if (!UnpackHeader(buf, size_t(n), type, player, seq)) continue;
        if (Tracing()) LOGR("net: got %d bytes, type %u player %u seq %u", n, type, player, seq);
        if (g_booting.load() && (type == kBundle || type == kAlive || type == kLeave) && !FromSession(from, player)) {
            LOG_FIRST(5, "net: a packet from %s:%u is not this session's; it is ignored",
                      IpString(from.sin_addr.s_addr).c_str(), unsigned(::ntohs(from.sin_port)));
            continue;
        }
        if (type == kResend) {
            if (!g_booting.load() || player >= kMaxPlayers || !FromAnyOfSession(from)) continue;
            uint8_t out[kBundleMax];
            size_t m;
            {
                std::lock_guard lock(g.m);
                m = PackBundle(out, player, seq, g.stream[player]);
            }
            if (m) Send(from, out, m);
            else if (host) AskAgain(player, seq);  // the host lacks it too: from the joiner it is from
            continue;
        }
        if (type >= kRejoin && type <= kBackAck) {
            Rejoining(buf, size_t(n), type, player, seq, from);
            continue;
        }
        if (type == kSaves) {
            bool from_host;
            {
                std::lock_guard lock(g.m);
                from_host = g.host_addr.sin_family == AF_INET && SameEndpoint(from, g.host_addr);
            }
            if (join && !g_booting.load() && from_host) TakeSaves(buf, size_t(n), seq);
            continue;
        }
        if (type == kRefused && host) {  // a rejoining machine cannot load the session's game
            std::lock_guard tx(g_tx.m);
            if (g_tx.seat >= 0 && SameEndpoint(from, g_tx.to)) g_tx.failed = true;
            g_tx.cv.notify_all();
            continue;
        }
        if (g_booting.load()) {
            if (join) g.heard[0].store(NowMs());
            else if (player < kMaxPlayers && (type == kBundle || type == kAlive || type == kLeave))
                g.heard[player].store(NowMs());
            if (int64_t now = NowMs(); g.active && now - last_alive >= kAliveMs) {
                last_alive = now;
                SendPlain(kAlive, uint8_t(g.self), 0);
            }
        }
        if (type == kLeave && player < kMaxPlayers && g_booting.load()) {
            if (host)
                for (int p = 1; p < kMaxPlayers; p++)
                    if (p != player && g.has_peer[p]) Send(g.peer[p], buf, size_t(n));
            MarkLeft(player, seq);
            continue;
        }
        if (type == kRestart && join && g_booting.load() && g.active) {
            bool from_host;
            {
                std::lock_guard lock(g.m);
                from_host = g.host_addr.sin_family == AF_INET && SameEndpoint(from, g.host_addr);
            }
            if (from_host) RestartForHost();
            continue;
        }
        if (type == kSkip && player < kMaxPlayers && g_booting.load()) {
            if (seq <= g_skipped.load()) continue;
            g_skipped = seq;
            if (host)
                for (int p = 1; p < kMaxPlayers; p++)
                    if (p != player && g.has_peer[p]) Send(g.peer[p], buf, size_t(n));
            continue;
        }
        if (type == kCheck && player < kMaxPlayers && g_booting.load() && size_t(n) > kHeader) {
            TakeCheck(buf, size_t(n), player, seq);
            if (host)
                for (int p = 1; p < kMaxPlayers; p++)
                    if (p != player && g.has_peer[p]) Send(g.peer[p], buf, size_t(n));
            continue;
        }
        if (type == kBundle && player < kMaxPlayers) {
            {
                std::lock_guard lock(g.m);
                TakeBundle(buf, size_t(n), player, seq);
            }
            g.cv.notify_all();
            if (host)  // the host is the relay: the joiners hear each other through it
                for (int p = 1; p < kMaxPlayers; p++)
                    if (p != player && g.has_peer[p]) Send(g.peer[p], buf, size_t(n));
        } else if (type == kHello && host) {
            if (Key key = HostKey(); !key.text.empty() && !(key.code && Relayed(from))) {
                char given[kPasswordLen + 1] = {};
                if (size_t(n) >= kHelloPassword) std::memcpy(given, buf + kHelloSize, kPasswordLen);
                std::string text = key.code ? CodeText(PasswordText(given)) : PasswordText(given);
                if (text != key.text) {
                    uint8_t r[kHeader];
                    Send(from, r, Pack(r, kRefused, 0, key.code ? kRefusedCode : kRefusedPassword));
                    static int64_t said = 0;
                    if (int64_t now = NowMs(); now - said > 10000) {
                        said = now;
                        const char* what = key.code ? "join code" : "password";
                        LOGR("net: %s asked to join with %s %s; refused", IpString(from.sin_addr.s_addr).c_str(),
                             given[0] ? "the wrong" : "no", what);
                    }
                    continue;
                }
            }
            int give = 0;
            bool known = false;
            {
                std::lock_guard lock(g.m);
                for (int p = 1; p < kMaxPlayers && !give; p++)
                    if (g.has_peer[p] && SameEndpoint(g.peer[p], from)) give = p, known = true;
                // a player who left, asking again from where they played: a new boot, which rejoins
                if (known && g_booting.load() && GoneAt(give, g.tick_now > 0 ? uint32_t(g.tick_now) : 0))
                    give = 0, known = false;
                if (!give && !g_booting.load())
                    for (int p = 1; p < kMaxPlayers && !give; p++)
                        if (!g.has_peer[p]) give = p;
                if (give && !known) {
                    g.has_peer[give] = true;
                    g.peer[give] = from;
                }
                if (give > 0 && give < kMaxPlayers) {
                    g.peer_char[give] = uint8_t(std::min<uint32_t>(seq & 0xFFu, kCharacterChoices - 1));
                    if (size_t(n) >= kHeader + kNameLen)
                        PackName(g.peer_name[give], reinterpret_cast<const char*>(buf + kHeader), kNameLen);
                    if (size_t(n) >= kHelloHandicap)
                        g.peer_handicap[give] = uint8_t(std::min<uint32_t>(buf[kHeader + kNameLen], kHandicapChoices - 1));
                }
            }
            uint32_t has_saves = 0;
            if (size_t(n) >= kHelloSize) std::memcpy(&has_saves, buf + kHelloHandicap, 4);
            if (!give) {
                if (!g_booting.load()) {  // only refuse when all 4 slots are full before boot
                    uint8_t r[kHeader];
                    Send(from, r, Pack(r, kRefused, 0, 0));
                } else {
                    OfferSeat(from, buf, size_t(n));
                    bool offered;
                    {
                        std::lock_guard tx(g_tx.m);
                        offered = g_tx.seat >= 0 && SameEndpoint(from, g_tx.to);
                    }
                    bool has;
                    {
                        std::lock_guard l(g_saves.m);
                        has = has_saves == g_saves.id;
                    }
                    if (offered && !has) SendSaves(from);
                }
                continue;
            }
            bool has;
            {
                std::lock_guard l(g_saves.m);
                has = g_saves.ok[give] = has_saves == g_saves.id;
            }
            if (!has) SendSaves(from);
            uint8_t w[kHeader + 8];
            Pack(w, kWelcome, uint8_t(give), 0);
            uint32_t d = delay;
            std::memcpy(w + kHeader, &give, 4), std::memcpy(w + kHeader + 4, &d, 4);
            Send(from, w, kHeader + 8);
            if (g_booting.load()) {  // a registered joiner missed kStart: re-send it
                uint8_t start_packet[kStartBots];
                std::lock_guard lock(g.m);
                Send(from, start_packet, PackStart(start_packet));
            } else if (!known) {
                LOGR("net: a joiner is controller %d", give + 1);
            }
            g.cv.notify_all();
        } else if (type == kRefused && join && !g_booting.load() && (seq == kRefusedPassword || seq == kRefusedCode)) {
            // the host's game asks for a password (or its join code): only the host this machine asks says so
            // (another on the LAN's broadcast may), and the connection screen asks for it
            std::lock_guard lock(g.m);
            bool asked = g.host_addr.sin_addr.s_addr == 0 || SameEndpoint(from, g.host_addr);
            if (asked && !g.active && !g_password_refused.load()) g_refused_code = seq == kRefusedCode, g_password_refused = NowMs();
        } else if (type == kRefused && join && !g_booting.load()) {
            LOGR("net: the host's session is full or already going; booting without it");
            g.active = false;
            g_booting.store(true);
            g.cv.notify_all();
        } else if (type == kWelcome && join && !g_booting.load() && size_t(n) >= kHeader + 8) {
            if (player >= 1 && player < kMaxPlayers) {
                uint32_t d;
                std::memcpy(&d, buf + kHeader + 4, 4);
                std::lock_guard lock(g.m);
                bool first = !g.active;
                g.host_addr = from;  // lock onto the answering host (whether reached by IP or LAN broadcast)
                g.self = player;
                delay = d;  // the host's input delay is everyone's
                g.active = true;
                if (first)
                    LOGR("net: the host at %s gives this one controller %d, %u ticks of input delay",
                         IpString(from.sin_addr.s_addr).c_str(), player + 1, d);
            }
            g.cv.notify_all();
        } else if (type == kStart && join && !g_booting.load()) {
            std::lock_guard lock(g.m);
            g.host_addr = from;
            g.players = std::clamp<int>(int(seq & 0xFF), 1, kMaxPlayers);
            int target = int((seq >> 8) & 0xFF);
            g_target = std::clamp<int>(target ? target : g.players, g.players, kMaxPlayers);
            if (size_t(n) >= kHeader + kMaxPlayers) std::memcpy(g.peer_char, buf + kHeader, kMaxPlayers);
            if (size_t(n) >= kNamesStart)
                for (int p = 0; p < kMaxPlayers; p++)
                    PackName(g.peer_name[p], reinterpret_cast<const char*>(buf + kHeader + kMaxPlayers + p * kNameLen),
                             kNameLen);
            if (size_t(n) >= kStartSize)
                for (int p = 0; p < kMaxPlayers; p++)
                    g.peer_handicap[p] = uint8_t(std::min<uint32_t>(buf[kNamesStart + p], kHandicapChoices - 1));
            g.unlocks = size_t(n) >= kStartFull && (buf[kStartSize] & 1);
            g.unlocks_mp = size_t(n) >= kStartFull && (buf[kStartSize] & 2);
            g.bots = size_t(n) >= kStartBots ? uint8_t(std::min<uint32_t>(buf[kStartSaves], kBotChoices - 1)) : 0;
            for (int i = 0; i < kBotSettings; i++)
                g.bot[i] = size_t(n) >= kStartBots ? uint8_t(std::min(buf[kStartSaves + 1 + i] + 0u, BotChoices(i) - 1)) : 0;
            if (size_t(n) >= kStartBots) std::memcpy(&g.bot_seed, buf + kStartSaves + 1 + kBotSettings, 4);
            g.bot_changes = 1;
            if (size_t(n) >= kStartSaves) {
                std::lock_guard l(g_saves.m);
                std::memcpy(&g_saves.host_id, buf + kStartFull, 4);
            }
            if (g.unlocks != unlock_missions)
                LOGR("net: the host has every mission %s (unlock_missions): this machine's list is the host's",
                     g.unlocks ? "unlocked" : "as the save has them");
            if (g.unlocks_mp != unlock_multiplayer)
                LOGR("net: the host has multiplayer %s (unlock_multiplayer): this machine's lists are the host's",
                     g.unlocks_mp ? "unlocked" : "as the save has it");
            g.active = g.players > 1;
            g_booting.store(true);
            LOGR("net: booting with %d connected (%d player slots); controller %d is this one's",
                 g.players, g_target, g.self + 1);
            g.cv.notify_all();
        }
    }
}

// F1 on the connection screen: the wait ends without a session and the settings window opens on Online once the game
// draws (the window is not up before boot); joining stays on, for Restart online play
bool g_meet_settings = false;

#ifdef _WIN32
// draws a centered waiting banner inside hwnd so even Borderless / Fullscreen windows (which have no title bar)
// show live connection status, and checks for Enter/Space/Start (start early) or Escape/Back (cancel online wait)
int BannerHeight(const RECT& rc) { return std::max(18, int((rc.bottom - rc.top) / 28)); }

// the connection screen's controls: a joiner's box (the host's address or join code, or the password or join code
// its host asked for; Enter starts again with it) and a host's Copy code button, drawn by Windows over the banner
struct MeetControls {
    HWND edit = nullptr, button = nullptr;
    HFONT font = nullptr;
    LONG_PTR style = 0;       // the window's own, WS_CLIPCHILDREN added while the controls are up
    std::wstring label;       // over the box
    bool entered = false, copy = false;
    std::wstring seen;        // the box's text as last seen
    int64_t typed_at = 0;     // when it last changed
} g_meet;

void MeetControlsBegin(HWND hwnd) {
    if (g_meet.font) return;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    g_meet.font = CreateFontW(-BannerHeight(rc), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              L"Tahoma");
    g_meet.style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE, g_meet.style | WS_CLIPCHILDREN);
}

void MeetControlsEnd(HWND hwnd) {
    if (!g_meet.font) return;
    if (g_meet.edit) DestroyWindow(g_meet.edit);
    if (g_meet.button) DestroyWindow(g_meet.button);
    DeleteObject(g_meet.font);
    SetWindowLongPtrW(hwnd, GWL_STYLE, g_meet.style);
    g_meet = {};
}

// the box, with this text and label (secret: drawn as dots), focused
void MeetBox(HWND hwnd, const std::string& text, const wchar_t* label, bool secret) {
    MeetControlsBegin(hwnd);
    if (!g_meet.edit) {
        g_meet.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER,
                                      0, 0, 10, 10, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_meet.edit, WM_SETFONT, WPARAM(g_meet.font), TRUE);
        SendMessageW(g_meet.edit, EM_LIMITTEXT, 120, 0);
    }
    std::wstring w(text.begin(), text.end());
    SetWindowTextW(g_meet.edit, w.c_str());
    SendMessageW(g_meet.edit, EM_SETPASSWORDCHAR, secret ? 0x25CF : 0, 0);
    SendMessageW(g_meet.edit, EM_SETSEL, 0, -1);
    ShowWindow(g_meet.edit, SW_SHOW);
    SetFocus(g_meet.edit);
    InvalidateRect(g_meet.edit, nullptr, TRUE);
    g_meet.label = label;
    g_meet.seen = w;
    g_meet.entered = false;
}

void MeetButton(HWND hwnd, const wchar_t* text) {
    MeetControlsBegin(hwnd);
    if (g_meet.button) return;
    g_meet.button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, hwnd,
                                    nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(g_meet.button, WM_SETFONT, WPARAM(g_meet.font), TRUE);
}

// the box's text as typed (the addresses and codes are ASCII)
std::string MeetBoxText() {
    if (!g_meet.edit) return {};
    wchar_t w[128] = {};
    GetWindowTextW(g_meet.edit, w, int(std::size(w)));
    std::string s;
    for (const wchar_t* p = w; *p; p++)
        if (*p >= 0x20 && *p < 0x7F) s += char(*p);
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);
    return s;
}

void CopyText(HWND hwnd, const std::string& text) {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
        wchar_t* p = static_cast<wchar_t*>(GlobalLock(mem));
        for (size_t i = 0; i < text.size(); i++) p[i] = wchar_t(uint8_t(text[i]));
        p[text.size()] = 0;
        GlobalUnlock(mem);
        if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
    }
    CloseClipboard();
}

void DrawMeetBanner(HWND hwnd, const wchar_t* line1, const wchar_t* line2) {
    RECT rc{};
    if (!GetClientRect(hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    HDC dc = GetDC(hwnd);
    if (!dc) return;
    HBRUSH bg = CreateSolidBrush(RGB(28, 32, 24));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    int h = BannerHeight(rc);
    HFONT font = CreateFontW(-h, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Tahoma");
    HGDIOBJ old = SelectObject(dc, font);
    RECT r0 = rc, r1 = rc, r2 = rc;
    int mid = (rc.top + rc.bottom) / 2;
    r0.top = mid - h * 3;
    r0.bottom = mid - h;
    r1.top = mid - h / 2;
    r1.bottom = mid + h * 2;
    r2.top = mid + h * 2;
    r2.bottom = mid + h * 4;
    SetTextColor(dc, RGB(195, 178, 70));
    DrawTextW(dc, L"007: NIGHTFIRE \u2014 ONLINE MULTIPLAYER", -1, &r0, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(dc, RGB(225, 230, 220));
    DrawTextW(dc, line1, -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(dc, RGB(155, 165, 145));
    DrawTextW(dc, line2, -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    int cx = (rc.left + rc.right) / 2, top = mid + h * 9 / 2;
    if (g_meet.edit && IsWindowVisible(g_meet.edit)) {
        RECT r3 = rc;
        r3.top = top, r3.bottom = top + h * 3 / 2;
        SetTextColor(dc, RGB(195, 178, 70));
        DrawTextW(dc, g_meet.label.c_str(), -1, &r3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        int w = std::min(int(rc.right - rc.left) * 3 / 5, h * 26);
        MoveWindow(g_meet.edit, cx - w / 2, top + h * 3 / 2, w, h * 8 / 5, TRUE);
    }
    if (g_meet.button) {
        int w = h * 10;
        MoveWindow(g_meet.button, cx - w / 2, top + h / 2, w, h * 8 / 5, TRUE);
    }
    SelectObject(dc, old);
    DeleteObject(font);
    ReleaseDC(hwnd, dc);
}

bool PumpMeetWindow(HWND hwnd, const wchar_t* status, const wchar_t* hint, bool can_start_early, bool* cancelled) {
    if (!hwnd) return false;
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) std::_Exit(0);
        if (g_meet.edit && msg.hwnd == g_meet.edit && (msg.message == WM_KEYDOWN || msg.message == WM_CHAR) &&
            (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {  // the box's Enter is the screen's (no beep)
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) g_meet.entered = true;
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1) {  // not the window's own F1 (it toggles the overlay)
            g_meet_settings = true;
            continue;
        }
        if (g_meet.button && msg.hwnd == g_meet.button && msg.message == WM_LBUTTONUP) g_meet.copy = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (!IsWindow(hwnd)) std::_Exit(0);
    if (g_meet.edit) {
        wchar_t w[128] = {};
        GetWindowTextW(g_meet.edit, w, int(std::size(w)));
        if (g_meet.seen != w) g_meet.seen = w, g_meet.typed_at = NowMs();
    }
    if (status) {
        SetWindowTextW(hwnd, status);
        DrawMeetBanner(hwnd, status, hint ? hint : L"");
    }
    bool focused = GetForegroundWindow() == hwnd;
    if (focused && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        if (cancelled) *cancelled = true;
        return false;
    }
    if (g_meet_settings) {
        if (cancelled) *cancelled = true;
        return false;
    }
    if (!can_start_early) return false;
    return focused && ((GetAsyncKeyState(VK_RETURN) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000));
}
#endif

// the kernel's launch.bin (boot.cpp) on this instance's disk, which its boot reads and deletes: Init runs before
std::filesystem::path LaunchFile() { return HddDir() / "launch.bin"; }

// the host plays Driving.xbe (kLaunch): its launch data as this machine's, and into that exe, which joins again
void RelaunchToHost() {
    std::string path;
    {
        std::lock_guard l(g_launch.m);
        const char* p = reinterpret_cast<const char*>(g_launch.page + 8);
        path.assign(p, strnlen(p, 520));
        std::error_code ec;
        std::filesystem::create_directories(LaunchFile().parent_path(), ec);
        if (FILE* f = std::fopen(LaunchFile().string().c_str(), "wb")) {
            std::fwrite(g_launch.page, 1, kLaunchPage, f);
            std::fclose(f);
        }
    }
    const char* exe = kernel::ImageExe(path);
    if (!exe) {
        LOGE("net: the host's launch data names %s, no exe here; booting without the session", path.c_str());
        g_launch.complete.store(false);
        return;
    }
    LOGR("net: the host plays %s: relaunching into it to take this machine's seat again", path.c_str());
    kernel::Relaunch(exe);
}

// the connection screen's box entered: the host's address (or join code), first in the server browser's history,
// or the key the host asked for; saved, and the game starts again with it
[[maybe_unused]] [[noreturn]] void JoinAgain(const std::string& text, bool key) {
    {
        std::lock_guard<std::recursive_mutex> ui(app::UiLock());
        app::Settings& s = app::Get();
        if (key) {
            s.join_password = text;
        } else {
            if (text != s.join_ip) s.join_password.clear();
            s.join_ip = text;
            if (!text.empty()) {
                std::string name;
                for (const app::Settings::Server& e : s.history)
                    if (e.address == text) name = e.name;
                std::erase_if(s.history, [&](const app::Settings::Server& e) { return e.address == text; });
                s.history.insert(s.history.begin(), app::Settings::Server{text, name, int64_t(std::time(nullptr))});
                if (s.history.size() > 30) s.history.resize(30);
            }
        }
    }
    app::Save();
    if (key) LOGR("net: the connection screen joins again with the %s typed", g_refused_code.load() ? "join code" : "password");
    else LOGR("net: the connection screen joins again, the host at %s", text.empty() ? "the LAN's broadcast" : text.c_str());
    Close();
    kernel::Relaunch(app::kGame.images.empty() ? "Nightfire" : app::kGame.images[0].second);
}

// net_join as the settings have it goes off once its session is over or did not come (the next start plays alone
// unless the player joins again); a join the harness asked for (NF_NET) leaves the settings as they are
bool g_join_from_settings = false;
void JoinOff(const char* why) {
    if (!g_join_from_settings) return;
    g_join_from_settings = false;
    {
        std::lock_guard<std::recursive_mutex> ui(app::UiLock());
        join_setting = false;
    }
    app::Save();
    LOGR("net: Join online play is off again (%s): F1's Server Browser or Online page joins again", why);
}

// the wait before boot. A host: joiners hello and are given controllers, and it starts when it has all it asked
// for (net_players / NF_NET_PLAYERS), when the host presses Enter/Space/Start with at least one joiner in, or
// when its wait is over; with none it boots as the game does without a session. A joiner: it asks (by direct IP
// when join_ip is set, plus LAN broadcast on UDP net_port) until the host answers, then holds until the host
// starts the session (or refuses it)
void Meet(const std::string& join_address, uint32_t lan_ip, void* window_handle) {
    // net_wait 0, and a host on the server list (whose players come from it while it waits): no limit, the wait
    // ends with the players it asked for, Enter / START, or ESC
    bool endless = wait == 0 || (host && Listed());
    constexpr int64_t kEndless = int64_t(1) << 50;
    int64_t start = NowMs(), limit = endless ? kEndless : int64_t(wait) * 1000, target = g_target;
    if (host && Listed() && wait) LOGR("net: this game is on the server list: it waits for its players with no time limit");
    uint8_t hello[kHelloPassword] = {};
    size_t n = Pack(hello, kHello, 0, 0);
    PackName(hello + kHeader, my_name.c_str(), my_name.size());
    hello[kHeader + kNameLen] = uint8_t(std::min<uint32_t>(my_handicap, kHandicapChoices - 1));
    const std::string join_password = join ? JoinPassword() : std::string();
    std::memcpy(hello + kHelloSize, join_password.data(), join_password.size());
    int64_t last_hello = 0, last_title = 0;
    int64_t due = 0;          // a host: when it would have started, then waits for its saves to be everywhere
    int64_t rejoin_at = 0;    // a joiner: when the host offered it a seat under way
    bool start_asked = false;  // a host: Enter / START pressed
    [[maybe_unused]] int64_t copied_at = 0;    // a host: when its join code went to the clipboard
    [[maybe_unused]] bool copy_held = false;   // C down
    [[maybe_unused]] bool asking_key = false;  // a joiner: its box asks for the password (or code) its host refused
    bool can_type = false;                     // a joiner: the connection screen has its box
    if (host) {
        std::lock_guard l(g_saves.m);
        PackSaves();
    }
    std::string my_ip = IpString(lan_ip);
    sockaddr_in bcast_all = To(INADDR_BROADCAST, uint16_t(port));
    sockaddr_in bcast_sub = To((lan_ip & htonl(0xFFFFFF00u)) | htonl(0x000000FFu), uint16_t(port));
    sockaddr_in loopback = To(htonl(INADDR_LOOPBACK), uint16_t(port));
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(window_handle);
    struct EndControls {
        HWND hwnd;
        ~EndControls() {
            if (hwnd) MeetControlsEnd(hwnd);
        }
    } end_controls{hwnd};
    if (hwnd) {
        if (app::HandingOver()) app::FinishHandover();
        else if (!IsWindowVisible(hwnd)) {
            app::CloseSplash();
            ShowWindow(hwnd, SW_SHOW);
        }
    }
#else
    (void)window_handle;
#endif
    for (;;) {
        int64_t now = NowMs();
        int left = endless ? 0 : std::max(0, int((limit - (now - start) + 999) / 1000));
        [[maybe_unused]] wchar_t left_text[32] = {};  // " (12s left)", "" with no limit
        if (!endless) std::swprintf(left_text, std::size(left_text), L" (%ds left)", left);
        if (host) {
            int joined = 0;
            {
                std::lock_guard lock(g.m);
                for (int p = 1; p < kMaxPlayers; p++) joined += g.has_peer[p];
            }
            bool start_early = false, cancelled = false;
#ifdef _WIN32
            wchar_t title[192] = {}, hint[192] = {};
            const wchar_t* title_ptr = nullptr;
            if (now - last_title >= 200) {
                last_title = now;
                std::string code = g_relay.on.load() ? RelayCode() : std::string();
                // streaming: no IP address on the screen, and no withheld code (C still copies it)
                const bool hide = default_names, hide_code = hide && who == kWhoInvited;
                const std::string shown_code = hide_code ? "(hidden)" : code;
                if (g_relay.on.load() && code.empty())
                    std::swprintf(title, std::size(title),
                                  L"Hosting  \u2014  asking the relay for a join code  \u2014  %d/%d players joined%ls",
                                  joined + 1, int(target), left_text);
                else if (g_relay.on.load())
                    std::swprintf(title, std::size(title),
                                  L"Hosting  \u2014  join code %hs  \u2014  %d/%d players joined%ls",
                                  shown_code.c_str(), joined + 1, int(target), left_text);
                else if (hide)
                    std::swprintf(title, std::size(title), L"Hosting  \u2014  %d/%d players joined%ls", joined + 1,
                                  int(target), left_text);
                else
                    std::swprintf(title, std::size(title),
                                  L"Hosting on %hs:%u  \u2014  %d/%d players joined%ls",
                                  my_ip.c_str(), port, joined + 1, int(target), left_text);
                if (!code.empty() && hwnd) MeetButton(hwnd, L"Copy join code");
                bool copy_key = GetForegroundWindow() == hwnd && (GetAsyncKeyState('C') & 0x8000);
                if (!code.empty() && ((copy_key && !copy_held) || g_meet.copy)) {
                    std::string text = code + "@" + g_relay.server_text;
                    CopyText(hwnd, text);
                    copied_at = now;
                    LOGI("net: the join code %s is on the clipboard", text.c_str());
                }
                copy_held = copy_key, g_meet.copy = false;
                if (copied_at && now - copied_at < 3000)
                    std::swprintf(hint, std::size(hint),
                                  L"Copied %hs: players paste it in the Server Browser's Connect box or their connection screen",
                                  hide_code ? "the join code" : code.c_str());
                else if (joined > 0)
                    std::swprintf(hint, std::size(hint), L"Press ENTER or Controller START to begin now   |   %lsESC cancels   |   F1 settings",
                                  code.empty() ? L"" : L"C copies the join code   |   ");
                else if (!code.empty())
                    std::swprintf(hint, std::size(hint),
                                  L"Players join with the code %hs   |   C copies it   |   ESC boots offline   |   F1 settings",
                                  shown_code.c_str());
                else if (Listed() && Reach() == 1)
                    std::swprintf(hint, std::size(hint),
                                  L"On the server list, and the internet reaches this PC: players anywhere can join   |   ESC boots offline   |   F1 settings");
                else if (Listed() && Reach() == 0)
                    std::swprintf(hint, std::size(hint),
                                  L"On the server list, but the internet cannot reach UDP %u here: forward it on your router (or host through the relay)   |   ESC boots offline   |   F1 settings",
                                  port);
                else if (hide)
                    std::swprintf(hint, std::size(hint),
                                  L"Waiting for joiners on LAN (or this PC's address)   |   ESC boots offline   |   F1 settings");
                else
                    std::swprintf(hint, std::size(hint),
                                  L"Waiting for joiners on LAN (or join_ip = %hs)   |   ESC boots offline   |   F1 settings",
                                  my_ip.c_str());
                title_ptr = title;
            }
            start_early = PumpMeetWindow(hwnd, title_ptr, hint, joined > 0, &cancelled);
#endif
            if (cancelled) {
                LOGR("net: host wait %s; booting without a session",
                     g_meet_settings ? "left for the settings window (F1)" : "cancelled by player");
                if (g_meet_settings) app::OpenOverlayAt("Online");
                g.active = false;
                return;
            }
            start_asked |= start_early;
            bool enough = (target && joined + 1 >= target) || start_asked, late = now - start >= limit;
            if ((enough || late) && !due) due = now;
            bool saves_everywhere;
            {
                std::lock_guard lock(g.m);
                saves_everywhere = !joined || JoinersHaveSaves();
            }
            if ((enough || late) && (saves_everywhere || now - due >= kSavesWaitMs)) {
                if (!saves_everywhere)
                    LOGE("net: a joiner did not get this machine's saves in %d s; its game may differ from this one's",
                         int(kSavesWaitMs / 1000));
                std::lock_guard lock(g.m);
                g.players = joined + 1;
                g.active = joined > 0;
                if (!joined) LOGI("net: no joiner came; booting without a session");
                break;
            }
        } else {
            if (g_booting.load()) break;
            {
                std::lock_guard tx(g_tx.m);
                if (g_tx.rejoining && !rejoin_at) rejoin_at = now;
                // its hellos go on until the host's saves are here: they say which it has
                if (g_tx.rejoining && (g_saves.complete.load() || now - rejoin_at >= 5000)) break;
            }
            if (g_launch.complete.load()) RelaunchToHost();
            bool cancelled = false;
#ifdef _WIN32
            wchar_t title[192] = {}, hint[192] = {};
            const wchar_t* title_ptr = nullptr;
            if (hwnd && !g.active) {
                can_type = true;
                if (!g_meet.edit) {
                    std::string text;
                    {
                        std::lock_guard<std::recursive_mutex> ui(app::UiLock());
                        text = app::Get().join_ip;
                    }
                    MeetBox(hwnd, text, L"The host's address or join code (Enter connects to it)", default_names);
                }
                if (g_password_refused.load() && !asking_key) {
                    asking_key = true;
                    start = now;  // the wait begins again, for the key to be typed
                    MeetBox(hwnd, "", g_refused_code.load() ? L"Its join code (Enter joins with it)"
                                                            : L"Its password (Enter joins with it)",
                            !g_refused_code.load() || default_names);
                }
                if (g_meet.entered) JoinAgain(MeetBoxText(), asking_key);
                if (g_meet.typed_at > start) start = g_meet.typed_at;  // the wait is from the last key typed
            } else if (g_meet.edit && IsWindowVisible(g_meet.edit)) {
                ShowWindow(g_meet.edit, SW_HIDE);
            }
            if (now - last_title >= 200) {
                last_title = now;
                if (g_password_refused.load()) {
                    std::swprintf(title, std::size(title), L"The host's game asks for its %ls, and this one %ls",
                                  g_refused_code.load() ? L"join code" : L"password",
                                  join_password.empty() ? L"gave none" : L"is not it");
                } else if (g.active && default_names) {
                    std::swprintf(title, std::size(title),
                                  L"Connected to the host as Controller %d  \u2014  waiting for host to start...", g.self + 1);
                } else if (g.active) {
                    std::swprintf(title, std::size(title),
                                  L"Connected to %hs as Controller %d  \u2014  waiting for host to start...",
                                  IpString(g.host_addr.sin_addr.s_addr).c_str(), g.self + 1);
                } else if (g_relay.on.load()) {
                    std::swprintf(title, std::size(title), L"Joining game %hs through the relay%ls...",
                                  default_names ? "(hidden)" : RelayCode().c_str(), left_text);
                } else if (!join_address.empty() && default_names) {
                    std::swprintf(title, std::size(title), L"Joining the host%ls...", left_text);
                } else if (!join_address.empty()) {
                    std::swprintf(title, std::size(title), L"Joining host %hs:%u%ls%ls...",
                                  join_address.c_str(), g_join_port,
                                  PrivateAddress(g.host_addr.sin_addr.s_addr) ? L" + LAN broadcast" : L"", left_text);
                } else {
                    std::swprintf(title, std::size(title),
                                  L"Searching LAN for host on UDP %u%ls...", port, left_text);
                }
                std::swprintf(hint, std::size(hint), L"%lsESC or Controller BACK boots offline   |   F1 settings",
                              asking_key                                        ? L"Type it below and press Enter   |   "
                              : g_meet.edit && IsWindowVisible(g_meet.edit) ? L"Another host? Type it below   |   "
                                                                                : L"");
                title_ptr = title;
            }
            PumpMeetWindow(hwnd, title_ptr, hint, false, &cancelled);
#endif
            if (cancelled) {
                LOGR("net: join wait %s; booting without a session",
                     g_meet_settings ? "left for the settings window (F1)" : "cancelled by player");
                if (g_meet_settings) app::OpenOverlayAt("Online");
                g.active = false;
                return;
            }
            if (now - last_hello >= 250 && !g_password_refused.load()) {
                last_hello = now;
                uint32_t init_char = std::min<uint32_t>(my_character, kCharacterChoices - 1);
                n = Pack(hello, kHello, 0, init_char) + kNameLen + 1 + 4 + kPasswordLen;
                uint32_t has_saves = g_saves.complete.load();
                std::memcpy(hello + kHelloHandicap, &has_saves, 4);
                if (g.host_addr.sin_addr.s_addr != 0) Send(g.host_addr, hello, n);
                // a host on the internet: this network's hosts are not asked, so none of them takes this machine
                if (!g.active && PrivateAddress(g.host_addr.sin_addr.s_addr)) {
                    Send(bcast_all, hello, n);
                    if (bcast_sub.sin_addr.s_addr != bcast_all.sin_addr.s_addr) Send(bcast_sub, hello, n);
                    if (g.host_addr.sin_addr.s_addr == 0) Send(loopback, hello, n);
                }
            }
            if (int64_t refused = g_password_refused.load();
                refused && now - std::max(refused, start) >= (can_type ? limit : kPasswordShownMs)) {
                LOGR("net: the host's game %s, and this machine %s; booting without a session",
                     g_refused_code.load() ? "asks for its join code" : "has a password",
                     join_password.empty() ? "gave none" : "gave another");
                g.active = false;
                break;
            }
            if (!g.active && now - start >= limit) {
                LOGR("net: no host answered (%s); booting without a session",
                     join_address.empty() ? "LAN broadcast" : join_address.c_str());
                break;
            }
            if (g.active && now - start >= limit + 30000) {
                LOGR("net: the host never started the session; booting without it");
                g.active = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    bool rejoining;
    {
        std::lock_guard tx(g_tx.m);
        rejoining = g_tx.rejoining;
    }
    if (join && ((g_booting.load() && g.active) || rejoining)) {
        uint32_t want;
        {
            std::lock_guard l(g_saves.m);
            want = rejoining ? g_saves.id : g_saves.host_id;
        }
        if (want && g_saves.complete.load() == want && UseHostSaves())
            LOGR("net: this machine plays from the host's saves (its own are left as they were)");
        else
            LOGE("net: the host's saves did not arrive; this machine plays from its own, and the games may differ");
    }
    if (host && g.active) {  // boot, together: send twice so a single dropped UDP packet does not strand a joiner
        std::lock_guard lock(g.m);
        g.peer_char[0] = uint8_t(std::min<uint32_t>(my_character, kCharacterChoices - 1));
        PackName(g.peer_name[0], my_name.c_str(), my_name.size());
        g.peer_handicap[0] = uint8_t(std::min<uint32_t>(my_handicap, kHandicapChoices - 1));
        g.unlocks = unlock_missions;
        g.unlocks_mp = unlock_multiplayer;
        g.bots = uint8_t(std::min<uint32_t>(bots, kBotChoices - 1));
        {
            std::lock_guard<std::recursive_mutex> ui(app::UiLock());
            for (int i = 0; i < kBotSettings; i++) g.bot[i] = uint8_t(std::min(bot_setting[i], BotChoices(i) - 1));
        }
        g.bot_seed = uint32_t(std::random_device{}());
        g.bot_changes = 1;
        uint8_t start_packet[kStartBots];
        size_t start_n = PackStart(start_packet);
        for (int pass = 0; pass < 2; pass++)
            for (int p = 1; p < kMaxPlayers; p++)
                if (g.has_peer[p]) Send(g.peer[p], start_packet, start_n);
        g_booting.store(true);
        LOGR("net: booting with %d connected (%d player slots); controller 1 is this one's, %u ticks of input delay",
             g.players, std::max(g.players, g_target), delay);
    }
#ifdef _WIN32
    if (hwnd) {
        wchar_t title[128];
        if (g.active)
            std::swprintf(title, std::size(title), L"007: NightFire \u2014 Player %d (%dP Online)", g.self + 1, g.players);
        else if (app::Instance())
            std::swprintf(title, std::size(title), L"%ls [%d]", app::kGame.title, app::Instance());
        else
            std::swprintf(title, std::size(title), L"%ls", app::kGame.title);
        SetWindowTextW(hwnd, title);
    }
#endif
}

}  // namespace

void Close() {
    if (g.sock == INVALID_SOCKET) return;
    g_alive.store(false);
    SOCKET s = g.sock;
    g.sock = INVALID_SOCKET;
    ::closesocket(s);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void Init(void* hwnd) {
    // this PC's display, with or without a session (a browser alone too)
    if (const char* e = std::getenv("NF_NET_DEFAULT_NAMES")) default_names = *e == '1';
    AskDirectory();  // every start, a session or not: F1 and the browser know this run's lists and relays
    if (const char* e = std::getenv("NF_NET_FIND"); e && *e == '1')
        std::thread([] {
            std::map<std::string, std::string> said;
            std::vector<std::string> ask;  // NF_NET_FIND_ASK=a,b: asked at those addresses, as the favourites are
            if (const char* e = std::getenv("NF_NET_FIND_ASK"))
                for (std::string v = e; !v.empty();) {
                    size_t comma = v.find(',');
                    ask.push_back(v.substr(0, comma));
                    v = comma == std::string::npos ? "" : v.substr(comma + 1);
                }
            static const char* kVia[] = {"LAN", "internet", "relay", "direct"};
            for (;;) {
                if (!ask.empty()) AskGames(ask);
                std::vector<app::FoundGame> found;
                FindGames(found);
                for (const app::FoundGame& f : found)
                    if (std::string line = f.name + ": " + f.status + (f.joinable ? "" : " (not joinable)") +
                                           (f.asks_code ? " (locked by its code)" : f.password ? " (locked)" : "") +
                                           " (" + kVia[int(f.via)] + ")";
                        said[f.address] != line) {
                        said[f.address] = line;
                        LOGR("net: found %s at %s, %s, %d ms", f.name.c_str(), f.address.c_str(),
                             line.substr(f.name.size() + 2).c_str(), f.ping_ms);
                    }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }).detach();
    join = join_setting;  // the session's role; the setting itself may change under it (F1)
    bool env_set = false;
    if (const char* e = std::getenv("NF_NET")) {  // the harness: NF_NET=host | join | join,192.168.1.2
        std::string v = e;
        bool is_host = v == "host" || v.rfind("host,", 0) == 0, is_join = v == "join" || v.rfind("join,", 0) == 0;
        std::string env_address;
        if (size_t comma = v.find(','); comma != std::string::npos) env_address = v.substr(comma + 1);
        if (is_host || is_join) host = is_host, join = is_join, env_set = true;
        if (!env_address.empty()) g_env_address = env_address;  // kept for the join address below
    }
    // a second instance on the same PC (--instance 2) with net_host in settings.toml joins 127.0.0.1 automatically
    if (!env_set && app::Instance() > 1 && host && !join) {
        host = false;
        join = true;
        if (g_env_address.empty()) g_env_address = "127.0.0.1";
        LOGR("net: instance %d joins the host on 127.0.0.1", app::Instance());
    }
    if (!host && !join) return;  // the settings' net_host/net_join, or NF_NET: no session asked for
    g_join_from_settings = join && !env_set && join_setting;
    if (kDriving && host)
        if (FILE* f = std::fopen(LaunchFile().string().c_str(), "rb")) {
            g_launch.own = std::fread(g_launch.page, 1, kLaunchPage, f) == kLaunchPage;
            std::fclose(f);
        }
    if (const char* e = std::getenv("NF_NET_OFFLINE"); e && *e == '1') {
        LOGR("net: this machine left its session (QUIT): booting without one");
        return;
    }
    if (host && join) {
        LOGE("nightfire: net_host and net_join are both set; booting without a session");
        return;
    }
    players = std::clamp<uint32_t>(players, 2, kMaxPlayers);
    g_target = int(players);
    delay = std::clamp<uint32_t>(delay, 1, 12);
    std::string address = g_env_address;
    if (const char* e = std::getenv("NF_NET_PORT"); e && *e) port = uint32_t(std::atoi(e));
    if (const char* e = std::getenv("NF_NET_DELAY"); e && *e) delay = std::clamp<uint32_t>(std::atoi(e), 1, 12);
    if (const char* e = std::getenv("NF_NET_WAIT"); e && *e) wait = uint32_t(std::max(1, std::atoi(e)));
    if (const char* e = std::getenv("NF_NET_PLAYERS"); e && *e) g_target = std::clamp(std::atoi(e), 2, kMaxPlayers);
    // RestartEveryone's, for this boot alone: a later restart waits for net_players again
    if (const char* e = std::getenv("NF_NET_RESTART_PLAYERS"); e && *e) {
        g_target = std::clamp(std::atoi(e), 2, kMaxPlayers);
#ifdef _WIN32
        _putenv_s("NF_NET_RESTART_PLAYERS", "");
#else
        unsetenv("NF_NET_RESTART_PLAYERS");
#endif
    }
    if (const char* e = std::getenv("NF_NET_NAME")) my_name = e;  // net_name
    if (const char* e = std::getenv("NF_NET_COOP_FRIENDLY_FIRE"); e && *e) coop_friendly_fire = *e == '1';
    if (const char* e = std::getenv("NF_NET_COOP_MORE_ENEMIES"); e && *e) coop_more_enemies = *e == '1';
    if (const char* e = std::getenv("NF_NET_COOP_TOUGHER_ENEMIES"); e && *e) coop_tougher_enemies = *e == '1';
    if (const char* e = std::getenv("NF_NET_QUIT_TOGETHER"); e && *e) quit_together = *e == '1';
    if (const char* e = std::getenv("NF_NET_HANDICAP"); e && *e)  // net_handicap: 0 the join screen's, 1..8
        my_handicap = std::min<uint32_t>(uint32_t(std::max(0, std::atoi(e))), kHandicapChoices - 1);
    if (const char* e = std::getenv("NF_NET_BOT_CHARACTER"); e && *e)  // net_bot_character: 0 the menu's, 1..29, 30 Random
        bot_setting[kBotEvery] = std::min<uint32_t>(uint32_t(std::max(0, std::atoi(e))), BotChoices(kBotEvery) - 1);
    if (const char* e = std::getenv("NF_NET_BOT"))  // setting=value,...: the bot settings by number (kBotSettings)
        SetBotSettings(e);
    if (const char* e = std::getenv("NF_NET_BOTS"); e && *e)  // net_bots: 0 the menu's, else 1 + the count
        bots = std::min<uint32_t>(uint32_t(std::max(0, std::atoi(e))), kBotChoices - 1);
    if (const char* e = std::getenv("NF_NET_CHARACTER"); e && *e)  // net_character: 0 the menu's, 1..29 a character
        my_character = std::min<uint32_t>(uint32_t(std::max(0, std::atoi(e))), kCharacterChoices - 1);
    if (address.empty())
        if (const char* e = std::getenv("NF_NET_IP")) address = e;
    if (address.empty()) address = app::Get().join_ip;
    if (!address.empty()) {
        size_t first = address.find_first_not_of(" \t");
        if (first == std::string::npos) address.clear();
        else address = address.substr(first, address.find_last_not_of(" \t") - first + 1);
    }
    g_join_port = uint16_t(port);
    std::string relay_text = relay_server;
    if (const char* e = std::getenv("NF_NET_RELAY")) relay_text = e;
    if (const char* e = std::getenv("NF_NET_HOST_RELAY"); e && *e) host_by = *e == '1' ? kHostByRelay : kHostByAddress;
    std::string code_relay;  // a join code's own (NF-CODE@relay): that relay alone
    std::string code = join ? JoinCode(address, code_relay) : std::string();
    if (size_t colon = address.rfind(':');
        code.empty() && colon != std::string::npos && address.find(':') == colon) {  // ip:port
        if (int p = std::atoi(address.c_str() + colon + 1); p > 0 && p < 65536) g_join_port = uint16_t(p);
        address.resize(colon);
    }
    WSADATA wsa;
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LOGE("net: WSAStartup failed; booting without a session");
        return;
    }
    uint32_t lan_ip = PrimaryLanAddress();
    g_lan_ip = lan_ip;
    g.sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g.sock == INVALID_SOCKET) {
        LOGE("net: no socket (%d); booting without a session", ::WSAGetLastError());
        return;
    }
#ifndef _WIN32
    ::fcntl(g.sock, F_SETFD, FD_CLOEXEC);  // a relaunch (execl) closes it: the next exe binds the same port
#endif
    int reuse = 1, bcast = 1;
    ::setsockopt(g.sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    ::setsockopt(g.sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bcast), sizeof(bcast));
    int rcvbuf = 4 << 20;  // a rejoin's state arrives in bursts of chunks
    ::setsockopt(g.sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf), sizeof(rcvbuf));
    u_long one = 1;
    ::ioctlsocket(g.sock, FIONBIO, &one);
    if (host) {
        sockaddr_in at = To(INADDR_ANY, uint16_t(port));
        if (::bind(g.sock, reinterpret_cast<sockaddr*>(&at), sizeof(at)) != 0) {
            LOGE("net: cannot listen on UDP %u (%d); booting without a session", port, ::WSAGetLastError());
            ::closesocket(g.sock), g.sock = INVALID_SOCKET;
            return;
        }
        LOGR("net: hosting on %s:%u, waiting up to %u s for %d players", IpString(lan_ip).c_str(), port, wait, g_target);
        if (host_by == kHostByRelay) {
            AwaitRelays(relay_text);
            std::lock_guard l(g_relay.m);
            unsigned kept = 0;
            std::string kept_relay;  // the relay the kept code is on: asked first
            if (const char* e = std::getenv("NF_NET_RELAY_CODE"); e && std::strlen(e) > kCodeLen + 1 && e[kCodeLen] == ':') {
                std::memcpy(g_relay.code, e, kCodeLen);
                kept = unsigned(std::strtoul(e + kCodeLen + 1, nullptr, 10));
                if (const char* at = std::strchr(e, '@')) kept_relay = ServerText(false, at + 1);
            }
            g_relay.candidates = ServersOf(relay_text, false);
            if (!kept_relay.empty()) {
                std::erase(g_relay.candidates, kept_relay);
                g_relay.candidates.insert(g_relay.candidates.begin(), kept_relay);
            }
            if (g_relay.candidates.empty()) {
                LOGE("net: players are to reach this host by a join code but no relay is known (net_relay is none): players join by "
                     "this PC's address");
            } else if (!UseRelay(0, NowMs())) {
                LOGE("net: no relay has an address; players join by this PC's address");
            } else {
                g_relay.rounds = 0;
                g_relay.token = kept ? kept : uint32_t(std::random_device{}()) | 1;
                g_relay.on = true;
                LOGR("net: asking the relay at %s for a join code%s (%zu relays known)", g_relay.server_text.c_str(),
                     kept ? " (this game's again)" : "", g_relay.candidates.size());
            }
        }
    } else if (!code.empty()) {
        sockaddr_in any = To(INADDR_ANY, 0);
        ::bind(g.sock, reinterpret_cast<sockaddr*>(&any), sizeof(any));
        if (code_relay.empty()) AwaitRelays(relay_text);
        std::lock_guard l(g_relay.m);
        if (code_relay.empty()) g_relay.candidates = ServersOf(relay_text, false);
        else if (std::string r = ServerText(false, code_relay); !r.empty()) g_relay.candidates = {r};
        if (g_relay.candidates.empty() || !UseRelay(0, NowMs())) {
            LOGE("net: join_ip is the join code NF-%s but %s; booting without a session", code.c_str(),
                 g_relay.candidates.empty() ? "no relay is known (net_relay is none)" : "no relay has an address");
            ::closesocket(g.sock), g.sock = INVALID_SOCKET;
            return;
        }
        g_relay.rounds = 0;
        std::memcpy(g_relay.code, code.data(), kCodeLen);
        g_relay.on = true;
        g.host_addr = RelayStandIn(0);
        LOGR("net: asking the relay at %s for the game NF-%s (%zu relays known)", g_relay.server_text.c_str(),
             code.c_str(), g_relay.candidates.size());
    } else {
        sockaddr_in any = To(INADDR_ANY, 0);
        ::bind(g.sock, reinterpret_cast<sockaddr*>(&any), sizeof(any));
        uint32_t a = Address(address);
        if (a) {
            g.host_addr = To(a, g_join_port);
            LOGR("net: asking the host at %s (UDP %u%s) to play", address.c_str(), g_join_port,
                 PrivateAddress(a) ? ", plus LAN broadcast" : "");
        } else {
            g.host_addr = {};
            LOGR("net: searching LAN broadcast on UDP %u for a host", port);
        }
    }
    if (const char* e = std::getenv("NF_NET_FULLSCREEN"); e && *e) fullscreen = *e == '1';
    NameChanged();
    g_host_id = uint32_t(std::random_device{}()) | 1;
    g_alive.store(true);
    std::thread(Receive).detach();
    StartListing();
    Meet(address, lan_ip, hwnd);
    if (host && g.players < 2) g.active = false;  // a host alone is no session
    if (!g.active) Unlist();
    for (int p = 0; p < kMaxPlayers; p++) g.left_at[p] = g.back_at[p] = kStaying, g.heard[p].store(NowMs());
    if (!g.active) {
        {
            std::lock_guard tx(g_tx.m);
            if (g_tx.rejoining) return;  // the socket stays: the session's game comes at the snapshot point
        }
        Close();  // no session: the game plays as it does alone
        if (join && !g_meet_settings) JoinOff("no session came of it");
        return;
    }
    // an unfocused window in a lockstep session must never pause its simulation
    app::Get().pause_unfocused = false;
    std::lock_guard lock(g.m);
    std::memcpy(g.next_name, g.peer_name, sizeof(g.next_name));  // the ticks before the first applied one
}

void NameChanged() {
    std::lock_guard lock(g.m);
    PackName(g.own_name, my_name.c_str(), my_name.size());
}

void Leave(bool over) {
    if (!g.active || !g_booting.load()) return;
    uint32_t tick;
    {
        std::lock_guard lock(g.m);
        if (g.leaving) return;
        g.leaving = true;
        tick = g.mine;
    }
    if (over && join) JoinOff("this machine left its session");
    // a packet can be lost and no machine answers it: five, a few milliseconds apart
    for (int k = 0; k < 5; k++) {
        SendPlain(kLeave, uint8_t(g.self), tick);
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    LOGR("net: leaving the session at tick %u", tick);
    Unlist();
}

static int g_left_seat = -1, g_left_seats = 0;

void LeaveToMenu() {
    if (!g.active) return;
    Leave();
    {
        std::lock_guard lock(g.m);
        g_left_seat = g.self;
        g_left_seats = std::min(std::max(g.players, g_target), 4);
        g.active = false;
    }
    Close();
    // the exes this one relaunches into boot without the session too (they inherit the environment)
#ifdef _WIN32
    _putenv_s("NF_NET_OFFLINE", "1");
#else
    setenv("NF_NET_OFFLINE", "1", 1);
#endif
    LOGR("net: this machine left the session; it plays on alone");
}

void Restarting() {
    Leave(false);
#ifdef _WIN32
    _putenv_s("NF_NET_OFFLINE", "");
#else
    unsetenv("NF_NET_OFFLINE");
#endif
    LOGR("net: restarting online play: the next boot meets the session again");
}

bool CanRestartEveryone() { return host && g.active && g_booting.load(); }

void RestartEveryone() {
    if (!CanRestartEveryone()) return;
    int in = 1;
    {
        std::lock_guard lock(g.m);
        for (int p = 1; p < g.players; p++)
            if (g.has_peer[p] && g.left_at[p] == kStaying) in++;
    }
    for (int k = 0; k < 5; k++) {
        SendPlain(kRestart, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    // the new boot waits for every machine that was in (and asks the relay for the same join code, KeepRelayCode)
    std::string players = std::to_string(std::max(in, 2));
#ifdef _WIN32
    _putenv_s("NF_NET_RESTART_PLAYERS", players.c_str());
#else
    setenv("NF_NET_RESTART_PLAYERS", players.c_str(), 1);
#endif
    LOGR("net: restarting the session for everyone: the new boot waits for its %d players", in);
    Unlist();
    app::Save();
    kernel::Relaunch(app::kGame.images.empty() ? "Nightfire" : app::kGame.images[0].second);
}

// a joiner's, on the host's kRestart (the receive thread's): once, on a thread of its own
void RestartForHost() {
    static std::atomic<bool> once{false};
    if (once.exchange(true)) return;
    std::thread([] {
        LOGR("net: the host restarts the session for everyone: this machine restarts online play to meet it again");
        Restarting();
        kernel::Relaunch(app::kGame.images.empty() ? "Nightfire" : app::kGame.images[0].second);
    }).detach();
}

void FindGames(std::vector<app::FoundGame>& out) {
    g_find.until = NowMs() + kFindKeepMs;
    if (!g_find.running.exchange(true)) std::thread(FindLoop).detach();
    uint32_t my_port = port;
    if (const char* e = std::getenv("NF_NET_PORT"); e && *e) my_port = uint32_t(std::atoi(e));
    auto shown = [&](const Found& f, bool answered) {
        app::FoundGame g;
        uint64_t key = FoundKey(f.addr, f.port);
        // streaming: a number of its own instead of the host's typed name, the same each time it is heard
        if (default_names) g.name = "Game " + std::to_string(1000 + (f.id ? f.id : uint32_t(key ^ (key >> 32))) % 9000);
        else g.name = f.name.empty() ? "NightFire host" : f.name;
        g.address = IpString(f.addr) + (f.port != my_port ? ":" + std::to_string(f.port) : "");
        g.ping_ms = answered ? f.ping : -1;
        bool under_way = f.flags & kFindUnderWay, seat = f.flags & kFindSeatFree;
        char s[128];
        if (f.version != kVersion)
            std::snprintf(s, sizeof s, "another build of the game");
        else if (!under_way)
            std::snprintf(s, sizeof s, "waiting for players, %u of %u", f.in, f.wanted);
        else
            std::snprintf(s, sizeof s, "playing%s, %u players, %s", f.flags & kFindDriving ? " (driving)" : "", f.in,
                          seat ? "a seat free" : "full");
        g.status = s;
        bool relayed = !f.code.empty(), withheld = f.code == "-";
        if (withheld) {  // no address to show: the browser joins with the code its player types and this relay
            g.address = "#" + std::to_string(f.id) + "@" + f.relay;
            g.relay = f.relay;
            g.status += " - through a relay, by its code";
        } else if (relayed) {
            g.address = f.code + "@" + f.relay;
            g.status += " - through a relay";
        } else if (!answered) {
            g.status += NowMs() - f.heard < 3000 ? " - asking it..." : " - not answering (its port may not be forwarded)";
        }
        g.joinable = (answered || relayed) && f.version == kVersion && (!under_way || seat) && f.in < kMaxPlayers;
        g.playing = under_way;
        g.password = (f.flags & kFindPassword) || withheld;
        g.asks_code = (f.flags & kFindCode) || withheld;
        g.other_build = f.version != kVersion;
        g.full = under_way ? !seat : f.in >= kMaxPlayers;
        g.answering = answered || relayed;
        g.players = f.in;
        g.max_players = f.wanted;
        g.mode = f.flags & kFindDriving ? "Driving" : "On foot";
        g.via = relayed                                                                            ? app::FoundGame::Via::Relay
                : PrivateAddress(f.addr)                                                           ? app::FoundGame::Via::Lan
                : std::any_of(g_find.listed.begin(), g_find.listed.end(),
                              [&](const Found& l) { return FoundKey(l.addr, l.port) == key; }) ? app::FoundGame::Via::Internet
                : g_find.asked.count(key)                                                          ? app::FoundGame::Via::Direct
                                                                                                   : app::FoundGame::Via::Internet;
        return g;
    };
    std::lock_guard l(g_find.m);
    // one host heard at two addresses (this network's and the internet's): its nearest
    std::map<uint32_t, const Found*> by_id;
    for (const auto& [key, f] : g_find.games) {
        const Found*& best = by_id[f.id];
        if (!f.id || !best || (PrivateAddress(f.addr) && !PrivateAddress(best->addr))) best = &f;
    }
    for (const auto& [key, f] : g_find.games)
        if (!f.id || by_id[f.id] == &f) out.push_back(shown(f, true));
    for (const Found& f : g_find.listed)
        if (!g_find.games.count(FoundKey(f.addr, f.port)) && !(f.id && by_id.count(f.id))) out.push_back(shown(f, false));
    if (!g_find.list_error.empty()) {
        app::FoundGame g;
        g.name = "Internet list";
        g.status = g_find.list_error;
        g.joinable = false;
        g.error = true;
        out.push_back(std::move(g));
    }
}

void AskGames(const std::vector<std::string>& addresses) {
    std::lock_guard l(g_find.m);
    if (addresses == g_find.ask) return;
    g_find.ask = addresses;
    g_find.ask_changed = true;
}

void RefreshGames() {
    g_find.refresh = true;
    g_find.list_refresh = true;
    LOGI("net: the server browser asks again");
}

int LeftSeat() { return g_left_seat; }
int LeftSeats() { return g_left_seat >= 0 ? g_left_seats : 0; }
void EndLeftSeats() { g_left_seat = -1; }

bool PlayerLeft(int player) {
    if (!g.active || player < 0 || player >= kMaxPlayers) return false;
    std::lock_guard lock(g.m);
    // a rejoining machine's own player too, until the tick it plays from: as on the session's other machines
    return (player != g.self && g.host_gone) || (g.tick_now >= 0 && GoneAt(player, uint32_t(g.tick_now)));
}

const char* PlayerName(int player) {
    if (!g.active || player < 0 || player >= kMaxPlayers) return "";
    return reinterpret_cast<const char*>(g.peer_name[player]);
}

const char* WhoChoiceName(uint32_t choice) {
    static const char* const names[kWhoChoices] = {"Not listed (this network sees it)",
                                                   "Anyone (in every Server Browser)",
                                                   "Only people I tell (in the Server Browsers, locked)"};
    // the listed ones without a game list to go on (net_master none) say so
    static const char* const unlisted[kWhoChoices] = {names[0], "Anyone - but the Server Browser's game list is none",
                                                      "Only people I tell - but the Server Browser's game list is none"};
    choice = std::min(choice, kWhoChoices - 1);
    return ServerLists().empty() ? unlisted[choice] : names[choice];
}

const char* HostByChoiceName(uint32_t choice) {
    static const char* const names[kHostByChoices] = {"My IP address (direct)", "A join code through a relay"};
    choice = std::min(choice, kHostByChoices - 1);
    if (choice == kHostByRelay && ServersOf(relay_server, false).empty())
        return "A join code - but the Relay server is none";
    return names[choice];
}

bool RelayUsed() { return !(host && host_by == kHostByAddress); }

bool AsksPassword() {
    return who == kWhoInvited && !(host_by == kHostByRelay && !ServersOf(relay_server, false).empty());
}

const char* HandicapChoiceName(uint32_t choice) {
    static const char* const names[kHandicapChoices] = {"Join screen's", "-75", "-50", "-25", "0",
                                                        "+25",           "+50", "+75", "+100"};
    return names[std::min(choice, kHandicapChoices - 1)];
}

bool CoopFriendlyFire() { return !g.active || g.friendly_fire; }
bool CoopMoreEnemies() { return g.active ? g.more_enemies : coop_more_enemies; }
bool CoopTougherEnemies() { return g.active ? g.tougher_enemies : coop_tougher_enemies; }
bool QuitTogether() { return g.active ? g.quit_together : quit_together; }
bool UnlocksMissions() { return g.active ? g.unlocks : unlock_missions; }
bool UnlocksMultiplayer() { return g.active ? g.unlocks_mp : unlock_multiplayer; }

bool HandicapFor(int player, int32_t& handicap) {
    if (!g.active || player < 0 || player >= kMaxPlayers) return false;
    uint8_t c = g.peer_handicap[player];
    if (c == 0 || c >= kHandicapChoices) return false;
    handicap = kHandicaps[c - 1];
    return true;
}

uint32_t Tick() {
    std::lock_guard lock(g.m);
    return g.tick_now > 0 ? uint32_t(g.tick_now) : 0;
}

// the players connected as the session booted: a seat no one took plays no one (a seat is taken again only by a
// machine whose player left it)
int Players() { return g.active ? g.players : 1; }
int LocalPlayer() { return g.active ? g.self : 0; }
int Driver() {
    if (!g.active || !join) return 0;
    std::lock_guard lock(g.m);
    return g.host_gone ? g.self : 0;
}
bool Active() { return g.active; }
bool FullScreenView() { return g.active && fullscreen; }
bool OnGameThread() { return input::GameThreadPoll(); }

void SetExtraInputPass(bool active, int port3_player) {
    g_extra_input_pass = active;
    g_port3_player = active ? port3_player : 3;
}
int PortPlayer(int port) { return MapPortToPlayer(port); }

void AddZoom(int notches) { g.zoom.fetch_add(notches); }

void BackgroundPad(int port, input::Pad& p) { p = Serve(port); }

void Pad(int port, input::Pad& p) {
    if (!OnGameThread()) {
        BackgroundPad(port, p);
        return;
    }
    if (port == 0 && !g_extra_input_pass) {
        // NF_NET_LEAVE=seconds (the harness, which has no window to close): this machine leaves the session that
        // long after its first tick and ends, as closing the window does
        static const int64_t leave_ms = [] {
            const char* e = std::getenv("NF_NET_LEAVE");
            return e && *e ? int64_t(std::atoi(e)) * 1000 : int64_t(0);
        }();
        static const int64_t first_tick = NowMs();
        if (leave_ms && NowMs() - first_tick >= leave_ms) {
            Leave();
            logx::Flush();
            std::_Exit(0);
        }
        // NF_NET_CHANGE=tick,name,handicap (the harness, which has no settings window): at this machine's tick, its
        // net_name and net_handicap change as the window would change them
        static const std::string change = [] {
            const char* e = std::getenv("NF_NET_CHANGE");
            return std::string(e ? e : "");
        }();
        if (!change.empty() && g.mine == uint32_t(std::atoi(change.c_str()))) {
            size_t a = change.find(','), b = change.find(',', a == std::string::npos ? a : a + 1);
            if (a != std::string::npos && b != std::string::npos) {
                my_name = change.substr(a + 1, b - a - 1);
                my_handicap = std::min<uint32_t>(uint32_t(std::max(0, std::atoi(change.c_str() + b + 1))), kHandicapChoices - 1);
                NameChanged();
                LOGR("net: tick %u: net_name %s, net_handicap %u (NF_NET_CHANGE)", g.mine, my_name.c_str(), my_handicap);
            }
        }
        // NF_NET_BOT_CHANGE=tick:setting=value,... (the harness): at that tick the host's bot settings change
        static const std::string bot_change = [] {
            const char* e = std::getenv("NF_NET_BOT_CHANGE");
            return std::string(e ? e : "");
        }();
        if (host && !bot_change.empty() && bot_change.find(':') != std::string::npos &&
            g.mine == uint32_t(std::atoi(bot_change.c_str()))) {
            std::lock_guard<std::recursive_mutex> ui(app::UiLock());
            SetBotSettings(bot_change.c_str() + bot_change.find(':') + 1);
            LOGR("net: tick %u: the bot settings %s (NF_NET_BOT_CHANGE)", g.mine, bot_change.c_str());
        }
        // the pad nf::MenuMouse just made of this machine's player's devices is their input for this tick; the
        // mouse and the zoom the game-repo code took since the last tick go with it
        Bundle b;
        b.pad = p;
        b.keyboard = input::UsingKeyboard();
        b.zoom = int16_t(std::clamp<int>(g.zoom.exchange(0), -8, 8));
        b.my_char = uint8_t(std::min<uint32_t>(my_character, kCharacterChoices - 1));
        if (host) {
            int at = int(g.mine % uint32_t(kBotSettings));
            b.host_bot_at = uint8_t(at);
            std::lock_guard<std::recursive_mutex> ui(app::UiLock());
            b.host_bot = uint8_t(std::min(bot_setting[at], BotChoices(at) - 1));
        }
        b.handicap = uint8_t(std::min<uint32_t>(my_handicap, kHandicapChoices - 1));
        b.friendly_fire = coop_friendly_fire;
        b.more_enemies = coop_more_enemies;
        b.tougher_enemies = coop_tougher_enemies;
        b.quit_together = quit_together;
        static int64_t last = 0;
        int64_t now = NowMs();
        float dt = std::clamp((now - last) / 1000.0f, 0.001f, 0.1f);
        last = now;
        float rate_x, rate_y;
        if (input::TakeMouse(rate_x, rate_y)) {
            b.mouse = true;
            b.mouse_x = rate_x * 2400 * dt;  // the runtime's rate back to counts, as src/game/mouse.cpp's Counts
            b.mouse_y = rate_y * 2400 * dt;
        }
        bool leaving;
        uint8_t out[kBundleMax];
        size_t out_n;
        {
            std::lock_guard lock(g.m);
            b.name_char = g.own_name[g.mine % kNameLen];
            g.stream[g.self].known[g.mine] = b;
            out_n = PackBundle(out, uint8_t(g.self), g.mine, g.stream[g.self]);
            leaving = g.leaving;
        }
        if (!leaving) SendOut(out, out_n);
        std::unique_lock lock(g.m);
        if (g_gather.exchange(false)) {
            // Gather: every player in the session has sent this tick too (or gone silent, heartbeat too), so the
            // machines go on together however long the others took to get here
            int64_t since = 0;
            uint32_t tick = g.mine;
            for (;;) {
                bool all = true;
                for (int pl = 0; pl < g.players; pl++)
                    if (pl != g.self && !GoneAt(pl, tick) && !(pl != 0 && g.host_gone) &&
                        g.stream[pl].known.lower_bound(tick) == g.stream[pl].known.end() &&
                        NowMs() - g.heard[host ? pl : 0].load() < kGoneMs)
                        all = false;
                if (all) break;
                if (since % 2000 == 1000) LOGR("net: waiting for the other players at tick %u", tick);
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                since += 1;
                lock.lock();
            }
            LOGR("net: every player at tick %u (%lld ms waited)", tick, (long long)since);
        }
        // the tick this one applies: every player's input from net_input_delay ticks back, waiting for the others
        int64_t need = int64_t(g.mine) - int64_t(delay);
        for (int pl = 0; pl < g.players; pl++) {
            if (need < 0) {  // the first ticks of a session: no input is held back that far yet
                g.applied[pl] = Bundle{};
                continue;
            }
            Stream& s = g.stream[pl];
            uint32_t want = uint32_t(need);
            int64_t since = 0, last_said = 0, last_asked = 0;
            for (;;) {
                // the host's input is played to the tick it left at (a QUIT pressed just before it is in it), then
                // the relay is gone: the other joiners are not heard from again
                if (pl == 0 && join && want >= g.left_at[0]) g.host_gone = true;
                if ((pl != g.self && g.host_gone) || GoneAt(pl, want)) {
                    // a rejoined player's ticks from their return may be here already
                    g.applied[pl] = Bundle{};
                    s.known.erase(s.known.begin(), s.known.upper_bound(want));
                    s.next = want + 1;
                    break;
                }
                auto it = s.known.find(want);
                if (it != s.known.end()) {
                    g.applied[pl] = it->second;
                    g.peer_char[pl] = it->second.my_char;
                    g.peer_handicap[pl] = it->second.handicap;
                    g.next_name[pl][want % kNameLen] = it->second.name_char;
                    if (want % kNameLen == kNameLen - 1) {
                        g.next_name[pl][kNameLen - 1] = 0;
                        std::memcpy(g.peer_name[pl], g.next_name[pl], kNameLen);
                    }
                    if (pl == 0) {
                        g.friendly_fire = it->second.friendly_fire;
                        g.more_enemies = it->second.more_enemies;
                        g.tougher_enemies = it->second.tougher_enemies;
                        g.quit_together = it->second.quit_together;
                    }
                    if (pl == 0 && it->second.host_bot_at < kBotSettings &&
                        g.bot[it->second.host_bot_at] != it->second.host_bot) {
                        g.bot[it->second.host_bot_at] = it->second.host_bot;
                        g.bot_changes++;
                        g.bot_changed_at = want;
                    }
                    s.next = want + 1;
                    if (want >= kKeep) s.known.erase(s.known.begin(), s.known.lower_bound(want - kKeep));
                    break;
                }
                // not here yet: a later tick already came (its packet and the kRedundant - 1 after it were lost),
                // or none for a while. Asked for again until it comes; the game waits, as every machine must play
                // the same input
                bool later = s.known.upper_bound(want) != s.known.end();
                if ((later || since >= 100) && since - last_asked >= kAskMs) {
                    last_asked = since;
                    LOG_FIRST(20, "net: player %d's tick %u is missing; asking for it again", pl + 1, want);
                    lock.unlock();
                    AskAgain(pl, want);
                    lock.lock();
                    continue;
                }
                if (since >= kStallMs && since - last_said >= 2000) {
                    last_said = since;
                    LOGR("net: waiting for player %d's tick %u", pl + 1, want);
                }
                // gone silent, heartbeat too: the host gives the player up at this tick, which no machine has their
                // input for (it all comes through the host), and tells the others; a joiner whose host is silent
                // plays on with every other player idle
                int64_t quiet = NowMs() - g.heard[host ? pl : 0].load();
                if (quiet >= kGoneMs && (host || pl == 0 || g.host_gone)) {
                    lock.unlock();
                    if (host) {
                        for (int k = 0; k < 3; k++) SendPlain(kLeave, uint8_t(pl), want);
                        MarkLeft(pl, want);
                    } else if (!g.host_gone) {
                        {
                            std::lock_guard l2(g.m);
                            g.host_gone = true;
                            LOGR("net: the host is gone (silent %lld ms): playing on, the other players idle",
                                 (long long)quiet);
                        }
                        JoinOff("the host is gone");
                    }
                    lock.lock();
                    continue;
                }
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                since += 1;
                lock.lock();
            }
        }
        g.tick_now = need;
        g.mine++;
        lock.unlock();
    }
    p = Serve(port);
}

namespace {
// the host's sending of the state: every chunk until each is acknowledged, at a rate that grows while the
// acknowledgements keep up with it and falls when they do not; the other joiners told the tick the seat plays from
void SendChunks(const std::vector<uint8_t>& data, sockaddr_in to, int seat, uint32_t id,
                std::array<bool, kMaxPlayers> others, const std::atomic<bool>& done) {
    uint32_t total = uint32_t(data.size()), chunks = uint32_t((total + kChunkData - 1) / kChunkData);
    std::vector<int64_t> sent_at(chunks, INT64_MIN / 2);
    std::vector<uint8_t> acked;
    bool backs[kMaxPlayers];
    uint8_t pkt[kHeader + 8 + kChunkData];
    int burst = 8;  // chunks a millisecond
    uint32_t cursor = 0, sent_now = 0, sent_before = 0, acked_then = 0;
    int64_t window = NowMs(), last_back = 0;
    while (!done.load() && g_alive.load()) {
        int64_t now = NowMs();
        uint32_t acked_count, first;
        {
            std::lock_guard tx(g_tx.m);
            acked = g_tx.acked, acked_count = g_tx.acked_count, first = g_tx.window;
            std::memcpy(backs, g_tx.back_acked, sizeof backs);
        }
        if (now - last_back >= 100) {
            last_back = now;
            uint8_t b[kHeader];
            Pack(b, kBack, uint8_t(seat), id);
            for (int q = 1; q < kMaxPlayers; q++)
                if (others[q] && !backs[q]) Send(g.peer[q], b, kHeader);
        }
        if (now - window >= 100) {  // the chunks acknowledged now against those sent the window before
            uint32_t newly = acked_count - acked_then;
            if (sent_before)
                burst = newly * 10 < sent_before * 7 ? std::max(1, burst * 2 / 3) : std::min(64, burst + std::max(2, burst / 4));
            sent_before = sent_now, sent_now = 0, acked_then = acked_count, window = now;
        }
        for (uint32_t k = 0, looked = 0; int(k) < burst && looked < chunks && acked_count < chunks; looked++) {
            uint32_t i = cursor;
            cursor = cursor + 1 < std::min(chunks, first + kAckBits) ? cursor + 1 : first;
            if (acked[i] || now - sent_at[i] < kChunkAgainMs) continue;
            size_t at = size_t(i) * kChunkData, len = std::min(kChunkData, size_t(total) - at);
            size_t m = Pack(pkt, kChunk, uint8_t(seat), i);
            std::memcpy(pkt + m, &id, 4), std::memcpy(pkt + m + 4, &total, 4);
            std::memcpy(pkt + m + 8, data.data() + at, len);
            Send(to, pkt, m + 8 + len);
            sent_at[i] = now, k++, sent_now++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// the host at its snapshot point with a rejoining machine ready: its game held there while the state goes, until
// the machine has loaded it and every other joiner knows the tick the seat plays from. That tick is the next this
// machine captures: no machine has its input, so none has played it
void HostRejoin(X86Context& ctx) {
    int seat;
    sockaddr_in to;
    {
        std::lock_guard tx(g_tx.m);
        if (g_tx.seat < 0 || !g_tx.ready) return;
        seat = g_tx.seat, to = g_tx.to;
    }
    int64_t start = NowMs();
    uint32_t id;
    std::array<bool, kMaxPlayers> others{};
    {
        std::lock_guard lock(g.m);
        id = g.mine;
        g.back_at[seat] = id;
        g.has_peer[seat] = true, g.peer[seat] = to, g.heard[seat].store(start);
        for (int q = 1; q < g.players; q++) others[q] = q != seat && g.has_peer[q] && !GoneAt(q, id);
    }
    std::vector<uint8_t> data = snapshot::Compress(snapshot::Save(ctx));
    uint32_t chunks = uint32_t((data.size() + kChunkData - 1) / kChunkData);
    {
        std::lock_guard tx(g_tx.m);
        g_tx.id = id, g_tx.acked.assign(chunks, 0), g_tx.acked_count = g_tx.window = 0;
        g_tx.loaded = g_tx.failed = false;
        std::memset(g_tx.back_acked, 0, sizeof g_tx.back_acked);
    }
    LOGR("net: player %d rejoins at tick %u: the game's state (%zu KB) goes to them, the session waits", seat + 1, id,
         data.size() / 1024);
    std::atomic<bool> done{false};
    std::thread sender(SendChunks, std::cref(data), to, seat, id, others, std::cref(done));
    bool loaded, told = true;
    {
        std::unique_lock tx(g_tx.m);
        auto all_told = [&] {
            for (int q = 1; q < kMaxPlayers; q++)
                if (others[q] && !g_tx.back_acked[q]) return false;
            return true;
        };
        g_tx.cv.wait_until(tx, clock::now() + std::chrono::milliseconds(kRejoinWaitMs),
                           [&] { return g_tx.failed || (g_tx.loaded && all_told()); });
        loaded = g_tx.loaded && !g_tx.failed, told = all_told();
        g_tx.seat = -1, g_tx.ready = false, g_tx.acked.clear();
    }
    done.store(true);
    sender.join();
    // a machine that never loads it goes silent: the seat is given up again from this tick (Pad's silence)
    if (loaded && told)
        LOGR("net: player %d is back in the session (%lld ms)", seat + 1, (long long)(NowMs() - start));
    else if (loaded)
        LOGE("net: player %d is back, but a joiner never acknowledged it: that machine may part from the others", seat + 1);
    else
        LOGR("net: player %d's machine did not load the session's game; their seat stays empty", seat + 1);
}

// a rejoining machine at its snapshot point: it asks for the state once it is far enough into its own boot, holds
// its game there until the state is all here (playing on, the title would open its attract movie, a handle the
// host's game has not), then loads it if it can (the same handles and threads as the host's)
void JoinerRejoin(X86Context& ctx) {
    static int frames = 0;
    static int64_t last_ready = 0, first_check = 0, last_said = 0;
    static std::vector<uint8_t> raw;
    uint32_t id;
    {
        std::unique_lock tx(g_tx.m);
        if (!g_tx.rejoining || ++frames < kReadyFrames) return;
        // asked until chunks come, then until they stop coming for a while (the host gone, or holding no longer)
        int64_t asked = NowMs(), heard = asked;
        uint32_t got = g_tx.got;
        while (!g_tx.complete && NowMs() - heard < kGoneMs * 2) {
            if (int64_t now = NowMs(); !g_tx.got && now - last_ready >= 250) {
                if (!last_ready) LOGR("net: asking the host for the session's game");
                last_ready = now;
                uint8_t r[kHeader];
                Send(g.host_addr, r, Pack(r, kReady, uint8_t(g.self), 0));
            }
            g_tx.cv.wait_for(tx, std::chrono::milliseconds(50));
            if (g_tx.got != got) got = g_tx.got, heard = NowMs();
        }
        if (!g_tx.complete) {
            LOGR("net: the session's game did not come (%u of its chunks in %lld s); asking again", g_tx.got,
                 (long long)((NowMs() - asked) / 1000));
            return;
        }
        id = g_tx.id;
        if (raw.empty() && !snapshot::Decompress(g_tx.data.data(), g_tx.data.size(), raw)) raw.clear();
    }
    const char* why = raw.empty() ? "not a snapshot" : snapshot::Check(raw, ctx);
    if (why) {
        int64_t now = NowMs();
        if (!first_check) first_check = now;
        if (now - first_check < 60000 && !raw.empty()) {
            if (now - last_said >= 2000) last_said = now, LOGR("net: the session's game waits here: %s", why);
            return;
        }
        LOGR("net: the session's game cannot load here (%s): this machine plays on alone", why);
        uint8_t r[kHeader];
        for (int k = 0; k < 3; k++) Send(g.host_addr, r, Pack(r, kRefused, uint8_t(g.self), id));
        {
            std::lock_guard tx(g_tx.m);
            g_tx.rejoining = false, g_tx.data.clear();
        }
        raw.clear();
        Close();
        return;
    }
    if (!snapshot::Load(raw, ctx)) {
        LOGE("net: the session's game did not load; this machine cannot go on");
        logx::Flush();
        std::_Exit(1);
    }
    raw.clear(), raw.shrink_to_fit();
    {
        std::lock_guard tx(g_tx.m);
        g_tx.rejoining = false, g_tx.data.clear(), g_tx.data.shrink_to_fit(), g_tx.have.clear();
    }
    app::Get().pause_unfocused = false;
    g.active = true;
    g_booting.store(true);
    uint8_t l[kHeader];
    for (int k = 0; k < 5; k++) Send(g.host_addr, l, Pack(l, kLoaded, uint8_t(g.self), id));
    LOGR("net: back in the session as controller %d, playing from tick %u", g.self + 1, id);
}
}  // namespace

void EnableRejoin() { g_rejoinable = true; }
void Gather() { g_gather = true; }

void SnapshotPoint(X86Context& ctx) {
    if (!g_rejoinable) return;
    if (host && g.active && g_booting.load()) HostRejoin(ctx);
    else if (join && !g.active) JoinerRejoin(ctx);
}

void CheckState(const uint32_t* words, size_t count) {
    static uint32_t last = ~0u;
    uint32_t tick = Tick();
    if (!g.active || tick == last || (tick >= kCheckEvery && tick % kCheckEvery)) return;  // each of the first ticks too
    last = tick;
    std::vector<uint32_t> own(words, words + std::min<size_t>(count, kCheckWords));
    uint8_t out[kHeader + kCheckWords * 4];
    size_t n = Pack(out, kCheck, uint8_t(g.self), tick);
    std::memcpy(out + n, own.data(), own.size() * 4);
    SendOut(out, n + own.size() * 4);
    std::lock_guard lock(g_checks.m);
    g_checks.own[tick] = std::move(own);
    while (g_checks.own.size() > kCheckKept) g_checks.own.erase(g_checks.own.begin());
    if (auto t = g_checks.theirs.find(tick); t != g_checks.theirs.end()) {
        for (auto& [player, theirs] : t->second) CompareCheck(tick, player, theirs);
        g_checks.theirs.erase(t);
    }
    while (!g_checks.theirs.empty() && g_checks.theirs.begin()->first < tick) g_checks.theirs.erase(g_checks.theirs.begin());
}

void SkipMovie(uint32_t movie) {
    if (!g.active || g_skip_sent.load() == movie) return;
    g_skip_sent = movie;
    g_skipped = std::max(movie, g_skipped.load());
    g_skip_until = NowMs() + kSkipFor;
}

bool MovieSkipped(uint32_t movie) { return movie && g_skipped.load() == movie; }

void SaveState(snapshot::Writer& w) {
    std::lock_guard lock(g.m);
    w.Pod(g.players), w.Pod(g_target), w.Pod(delay), w.Pod(g.tick_now), w.Pod(g.mine), w.Pod(g.applied);
    w.Pod(g.peer_char), w.Pod(g.peer_name), w.Pod(g.peer_handicap), w.Pod(g.next_name);
    w.Pod(g.friendly_fire), w.Pod(g.left_at), w.Pod(g.back_at), w.Pod(g.host_gone), w.Pod(g.unlocks), w.Pod(g.unlocks_mp);
    w.Pod(g.more_enemies), w.Pod(g.tougher_enemies), w.Pod(g.bots), w.Pod(g.bot_blocks), w.Pod(g.quit_together);
    w.Pod(g.bot), w.Pod(g.bot_changes), w.Pod(g.bot_changed_at), w.Pod(g.bot_seed), w.Pod(g.bot_menu), w.Pod(g.bot_round);
    for (const Stream& st : g.stream) w.U32(st.next), w.Map(st.known);
}

void LoadState(snapshot::Reader& r) {
    std::lock_guard lock(g.m);
    r.Pod(g.players), r.Pod(g_target), r.Pod(delay), r.Pod(g.tick_now), r.Pod(g.mine), r.Pod(g.applied);
    r.Pod(g.peer_char), r.Pod(g.peer_name), r.Pod(g.peer_handicap), r.Pod(g.next_name);
    r.Pod(g.friendly_fire), r.Pod(g.left_at), r.Pod(g.back_at), r.Pod(g.host_gone), r.Pod(g.unlocks), r.Pod(g.unlocks_mp);
    r.Pod(g.more_enemies), r.Pod(g.tougher_enemies), r.Pod(g.bots), r.Pod(g.bot_blocks), r.Pod(g.quit_together);
    r.Pod(g.bot), r.Pod(g.bot_changes), r.Pod(g.bot_changed_at), r.Pod(g.bot_seed), r.Pod(g.bot_menu), r.Pod(g.bot_round);
    for (Stream& st : g.stream) st.next = r.U32(), r.Map(st.known);
    for (auto& h : g.heard) h.store(NowMs());
    g.zoom.store(0);
    g.cv.notify_all();
}

bool PadFor(int player, input::Pad& p) {
    if (!g.active || player < 0 || player >= g.players) return false;
    std::lock_guard lock(g.m);
    p = g.applied[player].pad;
    return true;
}

bool MouseFor(int player, float& x, float& y) {
    if (!g.active || player < 0 || player >= g.players) return false;
    std::lock_guard lock(g.m);
    const Bundle& b = g.applied[player];
    x = b.mouse ? b.mouse_x : 0;
    y = b.mouse ? b.mouse_y : 0;
    return true;
}

bool KeyboardFor(int player) {
    if (!g.active || player < 0 || player >= g.players) return false;
    std::lock_guard lock(g.m);
    return g.applied[player].keyboard;
}

int ZoomFor(int player) {
    if (!g.active || player < 0 || player >= g.players) return 0;
    std::lock_guard lock(g.m);
    return g.applied[player].zoom;
}

const char* CharacterChoiceName(uint32_t choice) {
    if (choice == 0 || choice >= kCharacterChoices) return "Default (In-game / Auto)";
    if (!g_char_names[choice].empty()) return g_char_names[choice].c_str();
    static char fallback[kCharacterChoices][40] = {};
    if (!fallback[choice][0])
        std::snprintf(fallback[choice], sizeof(fallback[choice]), "%u: (name not read yet)", choice);
    return fallback[choice];
}

void SetCharacterName(uint32_t model_idx, const char* s) {
    if (model_idx + 1 >= kCharacterChoices || !s || !*s) return;
    g_char_names[model_idx + 1] = std::to_string(model_idx + 1) + ": " + s;
}

int Bots() {
    std::lock_guard lock(g.m);
    return g.active && g.bots >= 1 && g.bots < kBotChoices ? int(g.bots) - 1 : -1;
}

const char* BotChoiceName(uint32_t choice) {
    static const char* const kNames[kBotChoices] = {"The bot menu's", "None", "1", "2", "3", "4", "5", "6", "7", "8"};
    return choice < kBotChoices ? kNames[choice] : "";
}

uint32_t BotBlocks() {
    std::lock_guard lock(g.m);
    return g.bot_blocks;
}

void SetBotBlocks(uint32_t va) {
    std::lock_guard lock(g.m);
    g.bot_blocks = va;
}

int CharacterFor(int slot) {
    if (slot < 0 || slot >= kMaxPlayers) return -1;
    std::lock_guard lock(g.m);
    if (!g.active) return -1;
    uint8_t peer_ov = g.peer_char[slot];
    if (peer_ov >= 1 && peer_ov < kCharacterChoices) return int(peer_ov) - 1;
    return -1;
}

// the bot menu's fields as Bots 7 and 8's settings have them (0x8ADE0's rows on page 0x4000002C, the values it
// stores in the entry): choice 0 is Default, choice k the field's k - 1th
const BotField kBotFields[kBotFieldCount] = {
    {"Move Speed", 0x06, 1, 3, {"Slow", "Normal", "Fast"}, {0, 1, 2}},
    {"Personality", 0x0B, 1, 9,
     {"None", "Judge", "Collector", "Guardian", "Team Player", "Berserker", "Greedy", "Vengeful", "Assassin"},
     {0, 4, 1, 2, 3, 5, 6, 7, 8}},
    {"Aggression", 0x02, 2, 3, {"Normal", "High", "Very High"}, {2, 3, 4}},
    {"Accuracy Rating", 0x00, 1, 4, {"Poor", "Average", "Good", "Very Good"}, {8, 5, 3, 1}},
    {"Health", 0x04, 2, 9, {"50", "75", "100", "125", "150", "175", "200", "250", "300"},
     {50, 75, 100, 125, 150, 175, 200, 250, 300}},
    {"Reaction Time", 0x07, 1, 7, {"50", "75", "100", "125", "150", "175", "200"}, {50, 75, 100, 125, 150, 175, 200}},
    {"Recovery Rate", 0x08, 1, 7, {"50", "75", "100", "125", "150", "175", "200"}, {50, 75, 100, 125, 150, 175, 200}},
};

uint32_t BotChoices(int setting) {
    if (setting == kBotSkill) return 2;
    if (setting >= kBotField && setting < kBotSettings) return kBotFields[(setting - kBotField) % kBotFieldCount].count + 1;
    return kBotCharChoices;
}

const char* BotCharChoiceName(uint32_t choice) {
    return choice == kBotRandom ? "Random" : choice == 0 ? "Default" : CharacterChoiceName(choice);
}

const char* BotSkillChoiceName(uint32_t choice) { return choice ? "Bot 1's" : "Each bot's own"; }

const char* BotFieldChoiceName(int field, uint32_t choice) {
    if (field < 0 || field >= int(kBotFieldCount) || choice > kBotFields[field].count) return "";
    return choice ? kBotFields[field].names[choice - 1] : "Default";
}

void SetBotSettings(const char* list) {
    for (const char* at = list; at && *at;) {
        int i = std::atoi(at);
        const char* eq = std::strchr(at, '=');
        if (!eq) break;
        if (i >= 0 && i < kBotSettings) bot_setting[i] = std::min(uint32_t(std::max(0, std::atoi(eq + 1))), BotChoices(i) - 1);
        at = std::strchr(eq, ',');
        if (at) at++;
    }
}

int BotSetting(int setting) {
    if (setting < 0 || setting >= kBotSettings) return 0;
    std::lock_guard lock(g.m);
    return g.active ? g.bot[setting] : 0;
}

uint32_t BotChanges() {
    std::lock_guard lock(g.m);
    return g.bot_changes;
}

bool BotSettingsWhole() {
    std::lock_guard lock(g.m);
    return g.bot_changes <= 1 || g.tick_now < 0 || uint32_t(g.tick_now) - g.bot_changed_at >= uint32_t(kBotSettings);
}

uint32_t BotSeed() {
    std::lock_guard lock(g.m);
    return g.bot_seed;
}

uint32_t& BotMenuChanges() { return g.bot_menu; }
uint32_t& BotRound() { return g.bot_round; }

void SetHostBotSetting(int setting, uint32_t value) {
    if (!host || setting < 0 || setting >= kBotSettings) return;
    {
        std::lock_guard<std::recursive_mutex> ui(app::UiLock());
        if (bot_setting[setting] == value) return;
        bot_setting[setting] = std::min(value, BotChoices(setting) - 1);
    }
    app::Save();
}

}  // namespace nf::net
