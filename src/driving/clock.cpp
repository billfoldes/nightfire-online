// Driving.xbe's clock in a session: frames, not the wall clock. The game's frame timer (0x10AE50, refresh.cpp's
// rate) is a periodic timer whose callback 0x10AE10 counts [0x242428] (0x10A6C0 returns it: the game's clock) and
// [0x24242C] (the scheduler's), then runs the timer's own calls (0x242404, 8, set by 0x10ACB0): one, 0x5B9F0, counts
// the frames due [0x1E5204], and the frame 0x5BA80 runs one step for each since its last ([[0x1E520C]+0x14], each
// step's number handed to its tasks). The scheduler 0x10AC40, on the game thread before each frame of the main loop
// (0x5AE98: the loop 0x5AE40 drains the queue 0x1E4870, then runs a frame) and in the movies', loading screens' and
// streaming waits, runs the calls registered with 0x10AA50 (slots 0x2421C8, 0x10 bytes: the call, its period, when
// it is due, busy) once [0x24242C] reaches them: the pads' 0x108490 (XInputGetState; the mission's 0x516D0 every 2
// counts), the loading screen's 0xE28A0, a library's services 0x13F980.
// From the timer, the frames between two polls followed each machine's own pace (2 most ticks, 3 at different ticks
// on each), and the polls in the movies and loading screens set the tick a mission began at. In a session the main
// loop's frames are the clock: each one is one count of the timer, run on the game thread, its counts the session's
// frame count (the same on every machine at the same frame, whatever came before). Anywhere else both count on time
// (the intro movie never ended with the game's clock held), and the pads are not read: no tick passes there, and no
// machine's press alone skips a movie. A count stamped outside the frames can be ahead of the next frame's clock.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/net.h"
#include "common/nightfire.h"
#include "core/input.h"
#include "core/log.h"
#include "core/snapshot.h"
#include "kernel/kernel.h"
#include "xdk/dsound.h"
#include "xdk/hle.h"

namespace net = nf::net;

namespace {
constexpr uint32_t kClock = 0x242428, kDue = 0x24242C, kTimerCalls = 0x242404, kTimerCallsEnd = 0x242424;
constexpr uint32_t kSlots = 0x2421C8, kSlotsEnd = 0x2422C8;
constexpr uint32_t kFrames = 0x1E5204, kFrame = 0x1E520C;  // +4 every second one (the FPS meter's)
constexpr uint32_t kMainLoop = 0x5AE9D;                     // 0x10AC40's return in the main loop
// the movie player 0x1308A0's poll each picture (ESI the movie): A, B or START held on the movie's port sets its +0x10
// and the movie ends (a mission's opening, before the main loop's frames: in a session the pads are not read there)
constexpr uint32_t kMoviePoll = 0x130A89, kMovieSkip = 0x10;
constexpr uint32_t kBase = 1u << 24;  // ahead of any count the timer reached before the session's frames
std::atomic<bool> g_frames{false};    // the last scheduler run was the main loop's
std::mutex g_switch;  // a timer callback counting time runs out before the frames begin (one more count, one more step)
uint32_t g_count = 0;                 // the main loop's frames in the session
std::atomic<uint32_t> g_ticks{0};     // the frame timer's callbacks, counted or not
uint32_t g_movies = 0;                // movies played since boot: the same on every machine of a session

// EA's sound library: its thread (0x13D980) makes a pass every 10 ms of the wall clock, under its lock (0x13E7B0,
// 0x13E7C0): the voices' service 0x13B7B0 (their fades; 0x13E530 asks each buffer's GetStatus and frees the voices
// done, the game's sound handles with them), the software mix's ring 0x13D820 and the sound events' callbacks
// 0x13EFD0. Two machines freed the same sound a tick apart, and the game's next went to another emitter: Alpine
// Escape apart from tick 1622 with one machine's disc slower (2026-11-14). In a session's frames those frees are the
// game thread's, 100 passes to 60 frames, the buffers' status by the frames (dsound::FrameClock), and from the
// session's first frame on they stay so through its loads, none freed there (the clock went back to the mixer at each
// and on again with the mixer's places: a Replay's load put the sounds apart, Alpine Escape apart a thousand ticks
// on, John 2026-11-18). The rest stays on the thread: the streams' refills (0x142150) and the events' queue (0x13EE00
// adds to it without a bound) must go on while the game thread waits (a rejoin's transfer, a late peer), and the events' callbacks (0x244FB8) are null
constexpr uint32_t kSoundLock = 0x13E7B0, kSoundUnlock = 0x13E7C0, kVoiceScan = 0x13E530;
constexpr uint32_t kVoiceScanRet = 0x13B7C4, kVoiceFreeRet = 0x13E5BB;
uint32_t g_passes = 0;     // the game thread's passes
bool g_sound_frames = false;          // the session's sound by its frames (from its first frame to its end)
std::atomic<bool> g_sound_held{false};  // g_sound_frames, for the sound thread
enum class Scan { All, Refills, Frees };
thread_local Scan t_scan = Scan::All;

bool Frames() { return !net::Active() || g_frames.load(); }
bool SessionSound() { return net::Active() && (g_frames.load() || g_sound_held.load()); }

void CallGuest(X86Context& ctx, uint8_t* base, uint32_t fn) {
    X86_PUSH32(kMainLoop);
    x86_icall(ctx, base, fn);
}

// In a session's load the disc's waits run the scheduler (0x10C860 for a read, 0x10BEA0 for them all, each 1 ms), and
// the timer's counts there followed each machine's disc: a mission's Replay (a load of 20 ms) counted once on one
// machine, twice on the other, and the game's time went a frame ahead there (Alpine Escape and Enemies Vanquished,
// 2026-11-16: John's internet sessions, then headless). The sound library's services (0x13D3E0, the streams') ran
// once more, and what the load stamped with the clock was a count on. From a disc's wait to the frames (or to another
// wait) the clock stands: no count there, on any machine. A movie (a mission's opening, on another thread while the
// load goes on) counts as before: it needs the clock to end
constexpr uint32_t kReadWait = 0x10C92F, kReadsWait = 0x10BF19;
std::atomic<bool> g_disc_wait{false};  // the last scheduler run outside the frames was a disc's wait
std::atomic<int> g_movies_playing{0};

// each frame of a session's: the sound library's passes due by then
void SoundPasses(X86Context& ctx, uint8_t* base) {
    uint32_t due = uint32_t(uint64_t(g_count) * 10 / 6);
    if (!g_sound_frames) {
        g_sound_frames = true;
        g_sound_held = true;
        dsound::SetFrameClock(true);
        g_passes = due;
    }
    dsound::FrameClock(1.0 / 60);
    for (; int32_t(due - g_passes) > 0; g_passes++) {
        CallGuest(ctx, base, kSoundLock);
        t_scan = Scan::Frees;
        CallGuest(ctx, base, kVoiceScan);
        t_scan = Scan::All;
        CallGuest(ctx, base, kSoundUnlock);
    }
}

// the counts as the session's frames have them: the scheduler's dues move with them, no frames left due
void Set(uint32_t to) {
    kernel::W32(kClock, to);
    if (uint32_t delta = to - kernel::R32(kDue)) {
        for (uint32_t slot = kSlots; slot < kSlotsEnd; slot += 0x10)
            if (kernel::R32(slot)) kernel::W32(slot + 8, kernel::R32(slot + 8) + delta);
        kernel::W32(kDue, to);
    }
    if (kernel::R32(kFrames) != to) {
        if (uint32_t frame = kernel::R32(kFrame)) kernel::W32(frame + 0x14, to);
        kernel::W32(kFrames, to);
        kernel::W32(kFrames + 4, to / 2);
    }
}

// the timer's own calls, as its callback runs them
void TimerCalls(X86Context& ctx, uint8_t* base) {
    uint32_t esi = ctx.esi;
    for (uint32_t slot = kTimerCalls; slot < kTimerCallsEnd; slot += 4)
        if (uint32_t call = kernel::R32(slot)) {
            ctx.esi = slot;
            X86_PUSH32(0x10AE38u);
            x86_icall(ctx, base, call);
        }
    ctx.esi = esi;
}

// every 10 s of frames: the frames against the timer's 60 a second (a main loop faster than it runs the game, and
// the sound thread the timer's calls wake, that much faster)
void ReportRate() {
    using clock = std::chrono::steady_clock;
    static clock::time_point from = clock::now();
    static uint32_t frames = g_count, ticks = g_ticks.load();
    auto now = clock::now();
    double secs = std::chrono::duration<double>(now - from).count();
    if (secs < 10) return;
    uint32_t t = g_ticks.load();
    LOGR("driving: %u frames in %.1f s, the timer %u", g_count - frames, secs, t - ticks);
    from = now, frames = g_count, ticks = t;
}

// a frame no sooner than the timer's next count: where a frame shows nothing (the cutscenes' frames, the mission's
// first ones) no present paces the main loop, and each machine ran those as fast as it could (1100 frames in 10 s
// through Paris's first cutscene, its sound thread woken as often), one PC's game well ahead of the other's. Alone
// the game steps at the timer's rate there; a few counts' lead is kept, no more
void Pace() {
    static uint32_t used = g_ticks.load();
    uint32_t now = g_ticks.load();
    if (now - used > 3 && now - used < 0x80000000u) used = now - 3;
    // a timer that stopped counting (its thread waits on a clock) holds no frame longer than a few counts
    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    while (int32_t(g_ticks.load() - used) <= 0 && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    used++;
}

std::vector<uint8_t> ReadFile(const std::string& path) {
    std::vector<uint8_t> s;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        std::fseek(f, 0, SEEK_END);
        s.resize(size_t(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(s.data(), 1, s.size(), f) != s.size()) s.clear();
        std::fclose(f);
    }
    return s;
}

// The snapshot point: the main loop's frame before its timer calls (the sound thread they wake, 0x4FE10, ran its
// frame a frame ago). NF_SNAPSHOT_SAVE=frame,file writes a snapshot at that session frame, NF_SNAPSHOT_LOAD=frame,file
// waits for the file then and loads it
void FileSnapshots(X86Context& ctx) {
    struct At {
        uint32_t frame = 0;
        std::string file;
        explicit At(const char* env) {
            const char* e = std::getenv(env);
            if (!e || !*e) return;
            char* end;
            frame = uint32_t(std::strtoul(e, &end, 0));
            if (*end == ',') file = end + 1;
        }
    };
    static At save("NF_SNAPSHOT_SAVE"), load("NF_SNAPSHOT_LOAD");
    if (save.frame && g_count >= save.frame) {
        save.frame = 0;
        std::vector<uint8_t> s = snapshot::Save(ctx);
        std::string tmp = save.file + ".part";
        if (FILE* f = std::fopen(tmp.c_str(), "wb")) {
            std::fwrite(s.data(), 1, s.size(), f);
            std::fclose(f);
            std::rename(tmp.c_str(), save.file.c_str());
        }
        LOGR("driving: snapshot of frame %u (tick %u) to %s", g_count, net::Tick(), save.file.c_str());
    }
    if (load.frame && g_count >= load.frame) {
        load.frame = 0;
        std::vector<uint8_t> s;
        for (int i = 0; i < 3000 && s.empty(); i++) {
            s = ReadFile(load.file);
            if (s.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        uint32_t at = g_count;
        if (const char* why = snapshot::Check(s, ctx)) {
            LOGE("driving: the snapshot %s cannot load here: %s", load.file.c_str(), why);
            std::exit(3);
        }
        if (!snapshot::Load(s, ctx)) {
            LOGE("driving: the snapshot %s did not load", load.file.c_str());
            std::exit(3);
        }
        LOGR("driving: snapshot %s loaded at frame %u", load.file.c_str(), at);
    }
}
}  // namespace

namespace nf {
void SaveClockState(snapshot::Writer& w) { w.Pod(g_count), w.Pod(g_movies), w.Pod(g_passes), w.Pod(g_sound_frames); }
// a snapshot is a session's frame (the snapshot point): its clock counts frames from the load on, or the timer's
// callback counts time on the loaded clock until the scheduler's next call (the rejoin's last messages, before
// net::Active), and the tasks' dues move with it (the pads read a frame early, every tick after a frame apart:
// 2026-11-14)
void LoadClockState(snapshot::Reader& r) {
    r.Pod(g_count), r.Pod(g_movies), r.Pod(g_passes), r.Pod(g_sound_frames);
    g_sound_held = g_sound_frames;
    g_frames = true;
}
}  // namespace nf

X86_EXTERN_FUNC(__imp__sub_0010AE10);
X86_FUNC(sub_0010AE10) {  // the frame timer's callback
    if (!net::Active() && !g_frames.load()) return __imp__sub_0010AE10(ctx, base);  // a load's frames: before Active
    g_ticks++;
    std::lock_guard lock(g_switch);
    if (!g_frames.load() && (!g_disc_wait.load() || g_movies_playing.load() > 0)) {
        kernel::W32(kClock, kernel::R32(kClock) + 1);
        kernel::W32(kDue, kernel::R32(kDue) + 1);
        TimerCalls(ctx, base);
    }
    ctx.esp += 4 + 0x14;
}

X86_EXTERN_FUNC(__imp__sub_0010AC40);
X86_FUNC(sub_0010AC40) {  // the scheduler
    bool main_loop = kernel::R32(ctx.esp) == kMainLoop;
    if (main_loop) {  // NF_INVENTORY=N: the kernel's state every N frames of the main loop, for a snapshot point
        static const uint32_t every = [] {
            const char* e = std::getenv("NF_INVENTORY");
            return e ? uint32_t(std::strtoul(e, nullptr, 0)) : 0u;
        }();
        static uint32_t frames = 0;
        if (every && ++frames % every == 0) {
            LOGR("inventory: frame %u tick %u esp %08X", frames, net::Tick(), ctx.esp);
            kernel::LogInventory();
        }
        net::SnapshotPoint(ctx);  // a rejoin: the host's state taken, or the rejoining machine's loaded
    }
    bool frame = net::Active() && main_loop;
    uint32_t ret = kernel::R32(ctx.esp);
    bool disc_wait = !frame && net::Active() && (ret == kReadWait || ret == kReadsWait);
    if (frame != g_frames.load() || disc_wait != g_disc_wait.load()) {
        bool switched = frame != g_frames.load();
        {
            std::lock_guard lock(g_switch);  // the first wait's: no count between the frames and it
            g_frames = frame;
            g_disc_wait = disc_wait;
        }
        if (switched) {
            if (frame) net::Gather();  // the level's first frame on every machine together, the loading screen till then
            static int said = 0;
            if (said++ < 4) LOGR("driving: the session's clock counts %s", frame ? "frames" : "time, the pads unread");
        }
    }
    if (frame) {
        Pace();
        FileSnapshots(ctx);
        Set(kBase + g_count++);
        kernel::W32(kClock, kernel::R32(kClock) + 1);
        kernel::W32(kDue, kernel::R32(kDue) + 1);
        TimerCalls(ctx, base);
        SoundPasses(ctx, base);
        ReportRate();
    } else if (g_sound_frames && !net::Active()) {
        g_sound_frames = false;
        g_sound_held = false;
        dsound::SetFrameClock(false);
    }
    __imp__sub_0010AC40(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_001308A0);
X86_FUNC(sub_001308A0) {  // a movie (thiscall, the movie object; +0x14 its stream, none plays without one)
    if (kernel::R32(ctx.ecx + 0x14)) g_movies++;
    g_movies_playing++;
    __imp__sub_001308A0(ctx, base);
    g_movies_playing--;
}

X86_EXTERN_FUNC(__imp__sub_00108490);
X86_FUNC(sub_00108490) {  // the pads' poll
    if (Frames()) return __imp__sub_00108490(ctx, base);
    // a movie in a session: the driver's press (the host's, or theirs who took the car) skips it on every machine
    // (2026-11-14, John: Enter and the left button skipped nothing), the others' do nothing
    if (kernel::R32(ctx.esp) == kMoviePoll) {
        input::Poll(0);  // this machine's own controls (nf::SkipHeld)
        if (net::LocalPlayer() == net::Driver() && nf::SkipHeld()) net::SkipMovie(g_movies);
        if (net::MovieSkipped(g_movies) && !kernel::G<uint8_t>(ctx.esi + kMovieSkip)) {
            kernel::G<uint8_t>(ctx.esi + kMovieSkip) = 1;
            LOGR("driving: movie %u skipped by player %d", g_movies, net::Driver() + 1);
        }
    }
    ctx.esp += 4;
}


X86_EXTERN_FUNC(__imp__sub_0013E530);
X86_FUNC(sub_0013E530) {  // the voices' scan: the sound thread's refills only in a session's sound (SoundPasses)
    if (t_scan == Scan::All && SessionSound() && kernel::R32(ctx.esp) == kVoiceScanRet) {
        t_scan = Scan::Refills;
        __imp__sub_0013E530(ctx, base);
        t_scan = Scan::All;
        return;
    }
    __imp__sub_0013E530(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_00142150);
X86_FUNC(sub_00142150) {  // a stream's refill: the sound thread's
    if (t_scan == Scan::Frees) {
        ctx.esp += 4;
        return;
    }
    __imp__sub_00142150(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_0013DE50);
X86_FUNC(sub_0013DE50) {  // a voice's free: the scan's are the game thread's
    if (t_scan == Scan::Refills && kernel::R32(ctx.esp) == kVoiceFreeRet) {
        ctx.esp += 4;
        return;
    }
    __imp__sub_0013DE50(ctx, base);
}
