// Hooks for default.xbe (the on-foot levels). A strong definition replaces the recompiled function at that address, for example:
//
// #include "xdk/hle.h"
//
// X86_EXTERN_FUNC(__imp__sub_00012345);
// X86_FUNC(sub_00012345) {
//     __imp__sub_00012345(ctx, base);  // the original
// }

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "core/snapshot.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/dsound.h"
#include "xdk/hle.h"

namespace nf {
// which exe this is (default.xbe): src/common's key layouts and settings tables follow it
extern const bool kDriving = false;

// Eurocom's menu context for each player (0x25F1D0, 0x1D8 bytes a player): +0x1B8 the page shown (0x40000034 at the
// title, 0x400000xx in the front end and the pause menu), +0x1BC its widget, none in play. In a session: a menu
// every player has, or this machine's player's own (src/game/pause.cpp)
bool MenuUp() { return MenuUpFor(net::LocalPlayer()); }
bool MenuKeys() {  // not the pause page (0xAD520)
    return MenuUp() && kernel::R32(0x25F1D0 + uint32_t(net::LocalPlayer()) * 0x1D8 + 0x1B8) != 0x4000004B;
}
int GunStyle() { return -1; }  // the mounted guns are Driving.xbe's
uint32_t Level() { return 0; }
int PlayerPort() { return 0; }
int Reticles(Mark*, int) { return 0; }
void ViewAngles(float, float) {}
bool GunWaitsForShot() { return false; }
int GuidedBy() { return 0; }
}  // namespace nf

// On a fresh install with no saved codename ([0x17D544] == 2), pressing Start on the title screen normally calls
// sub_0007CD50, which sets [0x1F6588] = 0x9000001 and forces a relaunch into Driving.xbe's Paris tutorial mission.
// Returning 0 when skip_tutorial is enabled (without popping the caller's stack frame, since sub_0007CD50 is a 0-arg
// `ret` function) tells the title screen handler to open the Main Menu (NightFire / Multiplayer / Codenames) directly.
X86_EXTERN_FUNC(__imp__sub_0007CD50);
X86_FUNC(sub_0007CD50) {
    static const bool level_test = [] {  // NF_LEVEL (sub_000BDDF0's) takes the tutorial's place
        const char* e = std::getenv("NF_LEVEL");
        return e && *e;
    }();
    if (!level_test && (nf::skip_tutorial || nf::net::Active())) {  // a session's machines all go the same way
        kernel::G<uint8_t>(0x17D580) = 0;
        ctx.eax = 0;
        LOGR("nightfire: skipped forced first-boot Paris tutorial -> opening Main Menu");
        return;
    }
    __imp__sub_0007CD50(ctx, base);
}

// sub_000BDDF0(level, flag, ...) starts a level from the menus (a driving one, 0x9000001-6, relaunches into
// Driving.xbe; an on-foot one plays its intro, 0x71000xx, first). Headless runs only: NF_LEVEL=<hex level id> starts
// that level instead of the first boot's Paris (NF_SKIP_TUTORIAL=0, a fresh disk, START at the title), once.
X86_EXTERN_FUNC(__imp__sub_000BDDF0);
X86_FUNC(sub_000BDDF0) {
    static uint32_t level = [] {
        const char* e = std::getenv("NF_LEVEL");
        return e && *e ? uint32_t(std::strtoul(e, nullptr, 16)) : 0u;
    }();
    if (level && kernel::R32(ctx.esp + 4) == 0x9000001) {
        LOGR("nightfire: NF_LEVEL starts level %08X instead of %08X", level, kernel::R32(ctx.esp + 4));
        kernel::W32(ctx.esp + 4, level);
        level = 0;
    }
    __imp__sub_000BDDF0(ctx, base);
}

// 0xDFFB0 is the game's clock in hundredths of a second since 0xDFFA0 set its base ([0x2ADF48], at boot and at
// 0xBFDB6), both read from the time stamp counter (0xE8F20). A match's time and end (0x9CC50, 0xA2B80), the frame's
// time stamp (0x6ADE7, [0x1F65CC]) and the pause's sound (0xB0570) follow it: in a session it counts the ticks the
// game has played (a 60th of a second each, [0x17C104]), so every machine ends a match on the same tick
namespace {
namespace net = nf::net;
uint32_t g_clock_base = 0;
}

X86_EXTERN_FUNC(__imp__sub_000DFFA0);
X86_FUNC(sub_000DFFA0) {
    g_clock_base = net::Tick();
    __imp__sub_000DFFA0(ctx, base);
}

// 0xE8EE0 calls 0xDD1D0, the game's frame, in a loop: between two calls the game's thread is at the same place on
// every machine, its stack the same, the snapshot point (core/snapshot.h). Headless tests: NF_SNAPSHOT_SAVE=tick,file
// writes a snapshot there at that session tick, NF_SNAPSHOT_LOAD=tick,file waits for the file then and loads it
// (fN for the Nth frame instead of a tick: a machine outside a session); a file dumptick in the run folder holding a
// tick: physical memory written to mem_tick_<tick>.bin there (uncommitted pages as zeros, as the harness's memdump),
// the same point of the game on every machine of a session
namespace {
struct SnapshotAt {
    uint32_t tick = 0;
    bool frames = false;
    std::string file;
    explicit SnapshotAt(const char* env) {
        const char* e = std::getenv(env);
        if (!e || !*e) return;
        frames = *e == 'f';
        char* end;
        tick = uint32_t(std::strtoul(e + frames, &end, 0));
        if (*end == ',') file = end + 1;
    }
    bool Due(uint32_t frame) const { return tick && (frames ? frame : net::Tick()) >= tick; }
};

// the sound's frame (0xE19C0, from 0xDD8E0 at the end of each drawn frame) asks each of its 64 voices' buffers
// (0x2AE8C0, 0x40 bytes each) GetStatus and frees the voices done; the next sound takes the first free one. By the
// host's mixer two machines free the same sound a frame apart, and their games part (the RNG and the objects made,
// 07000006 tick 8760, 2026-11-15). In a session the buffers play by the session's ticks (dsound::FrameClock)
int64_t g_sound_tick = -1;

void SoundTicks() {
    if (!net::Active()) return;
    uint32_t now = net::Tick();
    if (g_sound_tick < 0) {
        dsound::SetFrameClock(true);
        g_sound_tick = now;
        LOGR("nightfire: the sounds end by the session's ticks (tick %u), the same on every machine", now);
    }
    if (int32_t(now - uint32_t(g_sound_tick)) > 0) dsound::FrameClock(double(now - uint32_t(g_sound_tick)) / 60);
    g_sound_tick = now;
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_000DD1D0);
X86_FUNC(sub_000DD1D0) {
    static SnapshotAt save("NF_SNAPSHOT_SAVE"), load("NF_SNAPSHOT_LOAD"), dump("");
    static std::string dump_later;  // the dumptick file's later ticks
    static uint32_t at = [] {
        const char* e = std::getenv("NF_INVENTORY");
        return e ? uint32_t(std::strtoul(e, nullptr, 0)) : 0u;
    }();
    static uint32_t frames = 0;
    ++frames;
    SoundTicks();
    if (at && frames % at == 0) {
        LOGR("inventory: frame %u tick %u esp %08X", frames, net::Tick(), ctx.esp);
        kernel::LogInventory();
    }
    if (save.Due(frames)) {
        save.tick = 0;
        std::vector<uint8_t> s = snapshot::Save(ctx);
        std::string tmp = save.file + ".part";
        if (FILE* f = std::fopen(tmp.c_str(), "wb")) {
            std::fwrite(s.data(), 1, s.size(), f);
            std::fclose(f);
            std::rename(tmp.c_str(), save.file.c_str());
        }
        LOGR("nightfire: snapshot of tick %u to %s", net::Tick(), save.file.c_str());
    }
    if (frames % 30 == 0)
        if (FILE* f = std::fopen("dumptick", "rb")) {
            char t[512] = {};
            std::fread(t, 1, sizeof t - 1, f);
            std::fclose(f);
            std::remove("dumptick");
            dump_later = t;
        }
    if (!dump.tick && !dump_later.empty()) {  // "tick,tick,...": each in turn
        size_t comma = dump_later.find(',');
        dump.tick = uint32_t(std::strtoul(dump_later.c_str(), nullptr, 0));
        dump_later = comma == std::string::npos ? "" : dump_later.substr(comma + 1);
        dump.file = "mem_tick_" + std::to_string(dump.tick) + ".bin";
    }
    if (dump.Due(frames)) {
        dump.tick = 0;
        static const std::vector<uint8_t> zeros(0x1000);
        auto write = [&](const std::string& name, uint32_t lo, uint32_t hi) {
            if (FILE* f = std::fopen(name.c_str(), "wb")) {
                for (uint32_t va = lo; va < hi; va += 0x1000)
                    std::fwrite(mem::IsCommitted(va) ? kernel::HostPtr(va) : zeros.data(), 1, 0x1000, f);
                std::fclose(f);
            }
        };
        write(dump.file, 0x80000000u, 0x84000000u);
        write(dump.file.substr(0, dump.file.size() - 4) + "_image.bin", 0, 0x400000u);  // the xbe's data with it
        LOGR("nightfire: memory at tick %u to %s", net::Tick(), dump.file.c_str());
    }
    if (load.Due(frames)) {
        load.tick = 0;
        std::vector<uint8_t> s;
        for (int i = 0; i < 3000 && s.empty(); i++) {
            if (FILE* f = std::fopen(load.file.c_str(), "rb")) {
                std::fseek(f, 0, SEEK_END);
                s.resize(size_t(std::ftell(f)));
                std::fseek(f, 0, SEEK_SET);
                if (std::fread(s.data(), 1, s.size(), f) != s.size()) s.clear();
                std::fclose(f);
            }
            if (s.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        uint32_t tick = net::Tick();
        if (!snapshot::Load(s, ctx)) {
            LOGE("nightfire: the snapshot %s did not load", load.file.c_str());
            std::exit(3);
        }
        LOGR("nightfire: snapshot %s loaded at tick %u", load.file.c_str(), tick);
    }
    // the 2D sprite pool (0x29AA98: its free count at +8, those in use at +0x14; view.cpp's 0xCEB30): a HUD built
    // from a full one writes through no sprite
    static bool sprites_said = false;
    if (!sprites_said && kernel::R16(0x29AAAC) && kernel::R16(0x29AAA0) < 32) {
        sprites_said = true;
        LOGR("nightfire: the 2D sprite pool is nearly full (%u free, %u in use)", kernel::R16(0x29AAA0),
             kernel::R16(0x29AAAC));
    }
    // unlock_missions: the game's every-mission flag (its mission list 0x75994, the choice 0x8672B), outside the
    // profile's saved mask (0x17C590 + 0x18 a mission, packed by 0x76240), so a save never keeps it
    if (nf::net::UnlocksMissions()) kernel::G<uint8_t>(0x25D79E) = 1;
    nf::BotsFrame(ctx);
    net::SnapshotPoint(ctx);  // a rejoin: the host's state taken, or the rejoining machine's loaded
    __imp__sub_000DD1D0(ctx, base);
}

// unlock_multiplayer: the every-mission flag (0x25D79E) for the calls that read it in multiplayer's lists alone, the
// lock bytes beside it (+0x10 of each 0x18-byte entry) then not read: the characters (0x7F3D0, 0x17C8E0's), the
// bots' character (0x8AB70, 0x245348's), the scenarios (0x87460, 0x17C7A8's), the maps (0x87790, 0x17C6A0's) and
// 0x86410's list (0x17C760's). The missions' list (0x75950 from 0x86E80 and 0x870F0, 0x86690) and the save's copy
// (0x19A10) never see it set
#define NF_UNLOCKED_MP(fn)                                                                                             \
    X86_EXTERN_FUNC(__imp__##fn);                                                                                      \
    X86_FUNC(fn) {                                                                                                     \
        uint8_t& flag = kernel::G<uint8_t>(0x25D79E);                                                                  \
        if (!nf::net::UnlocksMultiplayer() || flag) return __imp__##fn(ctx, base);                                     \
        flag = 1;                                                                                                      \
        __imp__##fn(ctx, base);                                                                                        \
        flag = 0;                                                                                                      \
    }
NF_UNLOCKED_MP(sub_0007F3D0)
NF_UNLOCKED_MP(sub_0008AB70)
NF_UNLOCKED_MP(sub_00087460)
NF_UNLOCKED_MP(sub_00087790)
NF_UNLOCKED_MP(sub_00086410)
#undef NF_UNLOCKED_MP

// unlock_rewards: what a mission's medals give (0x17F47C's table: per mission and medal its reward, 4 a gadget's
// upgrade) is decoded by 0xB1740 from the codename's 64 reward bits ([0x25D6E8 + 8 * codename], which the codename
// cheats such as PASSPORT and Q LAB set through 0x7CB10). Decoded from all 64 set instead, every reward is had while
// the bits themselves, and so the save, stay as earned; the gadgets' upgrades as the codename loads (0x7CB10's
// 0xB9E30)
X86_EXTERN_FUNC(__imp__sub_000B1740);
X86_FUNC(sub_000B1740) {  // (bits*, mission, medal, ?, out*)
    constexpr uint32_t kRewardBits = 0x25D6E8, kCodenames = 4;
    uint32_t bits = kernel::R32(ctx.esp + 4);
    if (!nf::net::UnlocksRewards() || bits < kRewardBits || bits >= kRewardBits + 8 * kCodenames || (bits - kRewardBits) % 8)
        return __imp__sub_000B1740(ctx, base);
    uint32_t all = (ctx.esp - 0x100) & ~7u;
    kernel::W32(all, 0xFFFFFFFFu), kernel::W32(all + 4, 0xFFFFFFFFu);
    kernel::W32(ctx.esp + 4, all);
    __imp__sub_000B1740(ctx, base);
    kernel::W32(ctx.esp + 4, bits);
}

// the hooks' own state that steers the game: a rejoining machine's copy must decide as the session's do
namespace nf {
void SaveViewState(snapshot::Writer& w);
void LoadViewState(snapshot::Reader& r);
void SavePauseState(snapshot::Writer& w);
void LoadPauseState(snapshot::Reader& r);
void SaveControlsState(snapshot::Writer& w);
void LoadControlsState(snapshot::Reader& r);
void SaveSettingsItemState(snapshot::Writer& w);
void LoadSettingsItemState(snapshot::Reader& r);
void SaveMouseState(snapshot::Writer& w);
void LoadMouseState(snapshot::Reader& r);
void SaveCharactersState(snapshot::Writer& w);
void LoadCharactersState(snapshot::Reader& r);

void RegisterSnapshot() {
    snapshot::Register("net", net::SaveState, net::LoadState);
    snapshot::Register(
        "game",
        [](snapshot::Writer& w) {
            w.Pod(g_clock_base), w.Pod(g_sound_tick);
            SaveViewState(w), SavePauseState(w), SaveControlsState(w), SaveSettingsItemState(w), SaveMouseState(w);
            SaveCharactersState(w);
        },
        [](snapshot::Reader& r) {
            r.Pod(g_clock_base), r.Pod(g_sound_tick);
            LoadViewState(r), LoadPauseState(r), LoadControlsState(r), LoadSettingsItemState(r), LoadMouseState(r);
            LoadCharactersState(r);
        });
    net::EnableRejoin();
}
}  // namespace nf

X86_EXTERN_FUNC(__imp__sub_000DFFB0);
X86_FUNC(sub_000DFFB0) {
    if (!net::Active()) {
        __imp__sub_000DFFB0(ctx, base);
        return;
    }
    ctx.eax = uint32_t(uint64_t(net::Tick() - g_clock_base) * 100 / 60);
    ctx.edx = 0;
    ctx.esp += 4;
}
