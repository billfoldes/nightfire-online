#include "core/snapshot.h"

#include <chrono>
#include <condition_variable>
#include <thread>

#include "core/log.h"
#include "kernel/kernel.h"

namespace mem {
void SaveState(snapshot::Writer& w);
void LoadState(snapshot::Reader& r);
}  // namespace mem
namespace kernel {
void SaveState(snapshot::Writer& w);
void LoadState(snapshot::Reader& r);
void CheckState(snapshot::Reader& r);
void SaveClock(snapshot::Writer& w);
void LoadClock(snapshot::Reader& r);
}  // namespace kernel
namespace d3d {
void Quiesce();  // the render thread done with every draw recorded, nothing pending for guest memory
void SaveState(snapshot::Writer& w);
void LoadState(snapshot::Reader& r);
}  // namespace d3d
namespace dsound {
void SaveState(snapshot::Writer& w);
void LoadState(snapshot::Reader& r);
}  // namespace dsound
namespace xapi {
void SaveState(snapshot::Writer& w);
void LoadState(snapshot::Reader& r);
}  // namespace xapi

namespace snapshot {

namespace {

constexpr uint32_t kMagic = 0x504E5358;  // XSNP
constexpr uint32_t kVersion = 2;

X86Context* g_ctx = nullptr;

// the title thread's registers; its thread block, handle and host data stay the machine's
void SaveCpu(Writer& w) {
    X86Context c = *g_ctx;
    c.fs_base = 0;
    c.thread_handle = 0;
    c.last_icall = 0;
    c.host = nullptr;
    w.Pod(c);
}
void CheckCpu(Reader& r) {
    X86Context c;
    r.Pod(c);
    if (c.esp != g_ctx->esp) r.Fail("cpu: another snapshot point");
}
void LoadCpu(Reader& r) {
    X86Context c;
    r.Pod(c);
    c.fs_base = g_ctx->fs_base;
    c.thread_handle = g_ctx->thread_handle;
    c.last_icall = g_ctx->last_icall;
    c.host = g_ctx->host;
    if (c.esp != g_ctx->esp) {
        LOGE("snapshot: the title's stack is at %08X here, %08X in the snapshot", g_ctx->esp, c.esp);
        return r.Fail("cpu: another snapshot point");
    }
    *g_ctx = c;
}

struct Section {
    std::string name;
    SaveFn save;
    LoadFn load;
    LoadFn check = nullptr;  // Check's dry run: fails as the load would, changes nothing
};

std::vector<Section>& Sections() {
    static std::vector<Section> s = {
        {"cpu", SaveCpu, LoadCpu, CheckCpu},
        {"memory", mem::SaveState, mem::LoadState},
        {"kernel", kernel::SaveState, kernel::LoadState, kernel::CheckState},
        {"clock", kernel::SaveClock, kernel::LoadClock},
        {"d3d", d3d::SaveState, d3d::LoadState},
        {"dsound", dsound::SaveState, dsound::LoadState},
        {"xapi", xapi::SaveState, xapi::LoadState},
    };
    return s;
}

// the sections' bodies in order, null when the bytes are not this build's sections
std::vector<std::pair<const uint8_t*, size_t>> Bodies(Reader& r) {
    std::vector<std::pair<const uint8_t*, size_t>> out;
    uint32_t magic = r.U32(), version = r.U32(), count = r.U32();
    if (magic != kMagic || version != kVersion || count != Sections().size()) {
        LOGE("snapshot: not a snapshot of this build (%08X version %u, %u sections; this build %zu)", magic, version,
             count, Sections().size());
        return {};
    }
    for (auto& s : Sections()) {
        std::string name = r.Str();
        uint32_t n = r.U32();
        if (!r.ok() || name != s.name || r.left() < n) {
            LOGE("snapshot: section %s where %s was expected", name.c_str(), s.name.c_str());
            return {};
        }
        out.push_back({r.Skip(n), n});
    }
    return out;
}

std::atomic<bool> g_busy{false};  // a save or load under way
std::mutex g_busy_lock;
std::condition_variable g_busy_cv;

// every other title thread parked in the kernel, a second at most (one held in a host call shows in the log)
void Quiesce() {
    g_busy = true;
    kernel::Thread* me = kernel::Current();
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    for (;;) {
        uint32_t running = 0;
        for (auto& t : kernel::Threads())
            if (t.get() != me && !t->parked) running = t->id;
        if (!running) return;
        if (std::chrono::steady_clock::now() > until) {
            LOGE("snapshot: thread %X is still in guest code", running);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct Quiet {
    Quiet() { Quiesce(); }
    ~Quiet() {
        {
            std::lock_guard<std::mutex> lock(g_busy_lock);
            g_busy = false;
        }
        g_busy_cv.notify_all();
    }
};

long long Since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

std::recursive_mutex& GuestLock() {
    static std::recursive_mutex m;
    return m;
}

void Unpark(std::atomic<bool>& parked) {
    for (;;) {
        parked = false;
        if (!g_busy) return;
        parked = true;
        std::unique_lock<std::mutex> lock(g_busy_lock);
        g_busy_cv.wait(lock, [] { return !g_busy.load(); });
    }
}

void Register(const char* name, SaveFn save, LoadFn load) {
    for (auto& s : Sections())
        if (s.name == name) {
            s.save = save;
            s.load = load;
            return;
        }
    Sections().push_back({name, save, load});
}

std::vector<uint8_t> Save(X86Context& ctx) {
    auto t0 = std::chrono::steady_clock::now();
    std::lock_guard<std::recursive_mutex> lock(GuestLock());
    Quiet quiet;
    d3d::Quiesce();
    g_ctx = &ctx;
    Writer w;
    w.U32(kMagic);
    w.U32(kVersion);
    w.U32(uint32_t(Sections().size()));
    for (auto& s : Sections()) {
        w.Str(s.name);
        size_t at = w.data.size();
        w.U32(0);
        s.save(w);
        uint32_t n = uint32_t(w.data.size() - at - 4);
        std::memcpy(w.data.data() + at, &n, 4);
        LOGI("snapshot: %s %u KB", s.name.c_str(), n >> 10);
    }
    g_ctx = nullptr;
    LOGR("snapshot: saved, %zu KB in %lld ms", w.data.size() >> 10, Since(t0));
    return std::move(w.data);
}

bool Load(const std::vector<uint8_t>& bytes, X86Context& ctx) {
    auto t0 = std::chrono::steady_clock::now();
    std::lock_guard<std::recursive_mutex> lock(GuestLock());
    Reader r(bytes.data(), bytes.size());
    auto bodies = Bodies(r);
    if (bodies.empty()) return false;
    Quiet quiet;
    d3d::Quiesce();
    g_ctx = &ctx;
    bool ok = true;
    for (size_t i = 0; i < bodies.size() && ok; i++) {
        auto& s = Sections()[i];
        Reader sr(bodies[i].first, bodies[i].second);
        s.load(sr);
        if (!sr.ok() || sr.left()) {
            LOGE("snapshot: section %s did not load (%s, %zu bytes left)", s.name.c_str(), sr.ok() ? "long" : sr.why(),
                 sr.left());
            ok = false;
        }
    }
    g_ctx = nullptr;
    if (ok) LOGR("snapshot: loaded, %zu KB in %lld ms", bytes.size() >> 10, Since(t0));
    return ok;
}

const char* Check(const std::vector<uint8_t>& bytes, X86Context& ctx) {
    std::lock_guard<std::recursive_mutex> lock(GuestLock());
    Reader r(bytes.data(), bytes.size());
    auto bodies = Bodies(r);
    if (bodies.empty()) return "not this build's";
    g_ctx = &ctx;
    const char* why = nullptr;
    for (size_t i = 0; i < bodies.size() && !why; i++)
        if (auto check = Sections()[i].check) {
            Reader sr(bodies[i].first, bodies[i].second);
            check(sr);
            if (!sr.ok()) why = sr.why();
        }
    g_ctx = nullptr;
    return why;
}

}  // namespace snapshot
