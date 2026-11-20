#include <algorithm>
#include <chrono>
#include <map>
#include <queue>

#include "core/snapshot.h"
#include "kernel/kernel.h"

namespace kernel {

namespace {

std::mutex g_dispatch;
std::condition_variable g_cv;

std::mutex g_handles_lock;
std::unordered_map<uint32_t, ObjectRef> g_handles;
uint32_t g_next_handle = 0x1000;

std::mutex g_ptr_lock;
struct PtrEntry {
    ObjectRef obj;
    uint32_t serial;
};
std::unordered_map<uint32_t, PtrEntry> g_by_ptr;
uint32_t g_serial = 1;
constexpr uint32_t kStamp = 0x4A424F4Bu;  // written into WaitListHead.Blink of objects the host tracks

std::mutex g_names_lock;
std::unordered_map<std::string, std::weak_ptr<Object>> g_names;

}  // namespace

std::mutex& DispatchLock() { return g_dispatch; }
void SignalChanged() { g_cv.notify_all(); }

uint32_t NewHandle(ObjectRef obj) {
    std::lock_guard<std::mutex> lock(g_handles_lock);
    uint32_t h = g_next_handle;
    g_next_handle += 4;
    g_handles[h] = std::move(obj);
    return h;
}

ObjectRef FromHandle(uint32_t handle) {
    if (handle == 0xFFFFFFFE) return CurrentRef();  // NtCurrentThread pseudo handle
    std::lock_guard<std::mutex> lock(g_handles_lock);
    auto it = g_handles.find(handle);
    return it == g_handles.end() ? nullptr : it->second;
}

bool CloseHandle(uint32_t handle) {
    std::lock_guard<std::mutex> lock(g_handles_lock);
    return g_handles.erase(handle) != 0;
}

uint32_t DuplicateHandle(uint32_t handle) {
    ObjectRef o = FromHandle(handle);
    return o ? NewHandle(o) : 0;
}

// serial in WaitListHead.Flink, stamp in Blink
static void Stamp(uint32_t guest, uint32_t serial) {
    W32(guest + 8, serial);
    W32(guest + 12, kStamp);
}

void Bind(ObjectRef obj, uint32_t guest) {
    obj->guest = guest;
    std::lock_guard<std::mutex> lock(g_ptr_lock);
    uint32_t serial = g_serial++;
    g_by_ptr[guest] = {obj, serial};
    Stamp(guest, serial);
}

ObjectRef FromPointer(uint32_t guest) {
    if (!guest) return nullptr;
    std::lock_guard<std::mutex> lock(g_ptr_lock);
    auto it = g_by_ptr.find(guest);
    if (it != g_by_ptr.end() && R32(guest + 12) == kStamp && R32(guest + 8) == it->second.serial) return it->second.obj;
    // first sight, or the guest reinitialised the structure: build from the header
    uint8_t type = G<uint8_t>(guest);
    int32_t state = int32_t(R32(guest + 4));
    ObjectRef o;
    switch (type) {
        case 0:
        case 1:
            o = std::make_shared<Object>(ObjType::Event);
            o->notification = type == 0;
            o->signal = state;
            break;
        case 2:
            o = std::make_shared<Object>(ObjType::Mutant);
            o->signal = state;
            break;
        case 5:
            o = std::make_shared<Object>(ObjType::Semaphore);
            o->signal = state;
            o->limit = int32_t(R32(guest + 0x10));
            break;
        case 8:
        case 9:
            o = std::make_shared<Object>(ObjType::Timer);
            o->notification = type == 8;
            o->signal = state;
            break;
        case 6:
            if (it != g_by_ptr.end()) return it->second.obj;  // threads keep their object
            o = std::make_shared<Object>(ObjType::Other);
            break;
        default:
            LOGW("objects: unknown dispatcher type %u at %08X", type, guest);
            if (Current()) LogGuestStack(*Current()->ctx, 64);
            o = std::make_shared<Object>(ObjType::Event);
            o->notification = true;
            o->signal = state;
            break;
    }
    o->guest = guest;
    uint32_t serial = g_serial++;
    g_by_ptr[guest] = {o, serial};
    Stamp(guest, serial);
    return o;
}

ObjectRef FindNamed(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_names_lock);
    auto it = g_names.find(name);
    return it == g_names.end() ? nullptr : it->second.lock();
}

void AddNamed(const std::string& name, ObjectRef obj) {
    std::lock_guard<std::mutex> lock(g_names_lock);
    obj->name = name;
    g_names[name] = obj;
}

void SetSignal(Object& o, int32_t v) {
    o.signal = v;
    if (o.guest) W32(o.guest + 4, uint32_t(v));
}

static uint32_t Me() {
    Thread* t = Current();
    return t ? t->ethread : 1;
}

static bool Satisfied(Object& o, uint32_t me) {
    if (o.type == ObjType::Mutant) return o.signal > 0 || o.owner == me;
    return o.signal > 0;
}

static void Consume(Object& o, uint32_t me) {
    switch (o.type) {
        case ObjType::Event:
        case ObjType::Timer:
            if (!o.notification) SetSignal(o, 0);
            break;
        case ObjType::Semaphore:
            SetSignal(o, o.signal - 1);
            break;
        case ObjType::Mutant:
            if (o.owner == me) {
                o.recursion++;
            } else {
                o.owner = me;
                o.recursion = 1;
                SetSignal(o, 0);
            }
            break;
        default:
            break;
    }
}

int64_t Now100ns() {
    using namespace std::chrono;
    // 1601 to 1970 in 100 ns units
    auto t = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    return t / 100 + 116444736000000000ll;
}

int64_t ToDeadline(int64_t due) {
    using namespace std::chrono;
    int64_t now = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    if (due < 0) return now + (-due) * 100;  // negative due is relative, positive is absolute system time
    int64_t rel = due - Now100ns();
    return now + std::max<int64_t>(rel, 0) * 100;
}

static uint32_t WaitFor(const std::vector<ObjectRef>& objs, bool wait_all, int64_t timeout, bool alertable) {
    using namespace std::chrono;
    Thread* self = Current();
    uint32_t me = Me();
    bool infinite = timeout == INT64_MAX;
    int64_t deadline = infinite ? 0 : ToDeadline(timeout);
    std::unique_lock<std::mutex> lock(g_dispatch);
    for (;;) {
        if (wait_all) {
            bool all = std::all_of(objs.begin(), objs.end(), [&](const ObjectRef& o) { return o && Satisfied(*o, me); });
            if (all) {
                for (auto& o : objs) Consume(*o, me);
                return kWait0;
            }
        } else {
            for (size_t i = 0; i < objs.size(); i++)
                if (objs[i] && Satisfied(*objs[i], me)) {
                    Consume(*objs[i], me);
                    return kWait0 + uint32_t(i);
                }
        }
        if (alertable && self) {
            bool pending;
            {
                std::lock_guard<std::mutex> al(self->apc_lock);
                pending = !self->apcs.empty();
            }
            if (pending) {
                lock.unlock();
                RunApcs(*self);
                return kUserApc;
            }
        }
        if (!infinite) {
            int64_t now = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
            if (now >= deadline) return kTimeout;
            // bounded so alertable waits notice new APCs
            g_cv.wait_for(lock, nanoseconds(std::min<int64_t>(deadline - now, 50000000)));
        } else {
            g_cv.wait_for(lock, milliseconds(50));
        }
    }
}

uint32_t Wait(const std::vector<ObjectRef>& objs, bool wait_all, int64_t timeout, bool alertable) {
    Parked parked;
    SuspendPoint();
    uint32_t r = WaitFor(objs, wait_all, timeout, alertable);
    SuspendPoint();
    return r;
}

int32_t SetEvent(Object& o) {
    std::lock_guard<std::mutex> lock(g_dispatch);
    int32_t old = o.signal;
    SetSignal(o, 1);
    g_cv.notify_all();
    return old;
}

int32_t ResetEvent(Object& o) {
    std::lock_guard<std::mutex> lock(g_dispatch);
    int32_t old = o.signal;
    SetSignal(o, 0);
    return old;
}

int32_t PulseEvent(Object& o) {
    // waiters already blocked cannot be told apart here, so a pulse behaves like set for one pass
    std::lock_guard<std::mutex> lock(g_dispatch);
    int32_t old = o.signal;
    SetSignal(o, 1);
    g_cv.notify_all();
    return old;
}

int32_t ReleaseSemaphore(Object& o, int32_t count, uint32_t& status) {
    std::lock_guard<std::mutex> lock(g_dispatch);
    int32_t old = o.signal;
    if (o.limit && old + count > o.limit) {
        status = kSemaphoreLimitExceeded;
        return old;
    }
    SetSignal(o, old + count);
    status = kSuccess;
    g_cv.notify_all();
    return old;
}

int32_t ReleaseMutant(Object& o, uint32_t& status) {
    std::lock_guard<std::mutex> lock(g_dispatch);
    uint32_t me = Me();
    if (o.owner != me) {
        status = kMutantNotOwned;
        return o.signal;
    }
    int32_t old = o.signal;
    if (--o.recursion == 0) {
        o.owner = 0;
        SetSignal(o, 1);
        g_cv.notify_all();
    }
    status = kSuccess;
    return old;
}

// timers and DPCs

namespace {

struct TimerEntry {
    int64_t deadline;
    uint64_t seq;
    ObjectRef timer;
    int32_t period_ms;
    uint32_t dpc;
    std::weak_ptr<Thread> apc_thread;
    uint32_t apc_routine, apc_context;
    bool operator>(const TimerEntry& o) const { return deadline > o.deadline; }
};

std::mutex g_timer_lock;
std::condition_variable g_timer_cv;
std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<TimerEntry>> g_timers;
std::unordered_map<Object*, uint64_t> g_timer_seq;  // latest arm per timer, older entries are stale
uint64_t g_seq = 1;
bool g_timer_thread = false;

std::mutex g_dpc_lock;
std::condition_variable g_dpc_cv;
std::deque<std::array<uint32_t, 3>> g_dpcs;
bool g_dpc_thread = false;

}  // namespace

void RunDpcs(X86Context& ctx) {
    for (;;) {
        std::array<uint32_t, 3> d;
        {
            std::unique_lock<std::mutex> lock(g_dpc_lock);
            g_dpc_cv.wait(lock, [] { return !g_dpcs.empty(); });
            d = g_dpcs.front();
            g_dpcs.pop_front();
        }
        uint32_t dpc = d[0];
        std::lock_guard<std::recursive_mutex> guest(snapshot::GuestLock());
        G<uint8_t>(dpc + 2) = 0;  // Inserted
        uint32_t routine = R32(dpc + 0x0C), context = R32(dpc + 0x10);  // KDPC.DeferredRoutine and DeferredContext
        if (routine) CallGuest(ctx, routine, {dpc, context, d[1], d[2]});
    }
}

void QueueDpc(uint32_t dpc, uint32_t arg1, uint32_t arg2) {
    std::lock_guard<std::mutex> lock(g_dpc_lock);
    W32(dpc + 0x14, arg1);  // KDPC.SystemArgument1 and 2
    W32(dpc + 0x18, arg2);
    g_dpcs.push_back({dpc, arg1, arg2});
    g_dpc_cv.notify_one();
    if (!g_dpc_thread) {
        g_dpc_thread = true;
        StartHostThread("dpc", [](X86Context& ctx) { RunDpcs(ctx); });
    }
}

static void TimerLoop() {
    using namespace std::chrono;
    std::unique_lock<std::mutex> lock(g_timer_lock);
    for (;;) {
        if (g_timers.empty()) {
            g_timer_cv.wait(lock);
            continue;
        }
        int64_t now = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
        TimerEntry e = g_timers.top();
        if (e.deadline > now) {
            g_timer_cv.wait_for(lock, nanoseconds(e.deadline - now));
            continue;
        }
        g_timers.pop();
        auto it = g_timer_seq.find(e.timer.get());
        if (it == g_timer_seq.end() || it->second != e.seq) continue;
        {
            std::lock_guard<std::mutex> dl(g_dispatch);
            SetSignal(*e.timer, 1);
            g_cv.notify_all();
        }
        if (e.dpc) QueueDpc(e.dpc, uint32_t(Now100ns()), uint32_t(Now100ns() >> 32));
        if (auto t = e.apc_thread.lock(); t && e.apc_routine)
            QueueApc(*t, e.apc_routine, e.apc_context, uint32_t(Now100ns()), uint32_t(Now100ns() >> 32));
        if (e.period_ms > 0) {
            e.deadline += int64_t(e.period_ms) * 1000000;
            g_timers.push(e);
        } else {
            g_timer_seq.erase(e.timer.get());
        }
    }
}

void SetTimer(ObjectRef timer, int64_t due, int32_t period_ms, uint32_t dpc, std::shared_ptr<Thread> apc_thread,
              uint32_t apc_routine, uint32_t apc_context) {
    std::lock_guard<std::mutex> lock(g_timer_lock);
    if (!g_timer_thread) {
        g_timer_thread = true;
        std::thread(TimerLoop).detach();
    }
    {
        std::lock_guard<std::mutex> dl(g_dispatch);
        SetSignal(*timer, 0);
    }
    uint64_t seq = g_seq++;
    g_timer_seq[timer.get()] = seq;
    g_timers.push({ToDeadline(due), seq, timer, period_ms, dpc, apc_thread, apc_routine, apc_context});
    g_timer_cv.notify_all();
}

bool CancelTimer(Object& timer) {
    std::lock_guard<std::mutex> lock(g_timer_lock);
    return g_timer_seq.erase(&timer) != 0;
}

// The handles, threads and the objects by pointer are made by the title's boot, the same on every machine: a
// snapshot carries their state (signals, file positions) and refuses a machine whose set differs
void SaveState(snapshot::Writer& w) {
    {
        std::lock_guard<std::mutex> lock(g_handles_lock);
        std::lock_guard<std::mutex> dl(g_dispatch);
        std::map<uint32_t, ObjectRef> sorted(g_handles.begin(), g_handles.end());
        w.U32(g_next_handle);
        w.U32(uint32_t(sorted.size()));
        for (auto& [h, o] : sorted) {
            w.U32(h);
            w.Pod(o->type);
            w.Pod(o->signal);
            w.Pod(o->owner);
            w.Pod(o->recursion);
            w.Str(FilePath(o));
            w.Pod(FilePosition(o));
        }
    }
    auto threads = Threads();
    w.U32(uint32_t(threads.size()));
    for (auto& t : threads) {
        w.U32(t->id);
        w.U32(t->stack_top);
    }
    auto host = HostThreads();
    w.U32(uint32_t(host.size()));
    for (auto& [name, stack] : host) {
        w.Str(name);
        w.U32(stack);
    }
}

// apply false: only whether this machine's handles and threads are the snapshot's
static void LoadOrCheck(snapshot::Reader& r, bool apply) {
    {
        std::lock_guard<std::mutex> lock(g_handles_lock);
        std::lock_guard<std::mutex> dl(g_dispatch);
        uint32_t next = r.U32(), n = r.U32();
        if (n != g_handles.size()) return r.Fail("kernel: another set of handles");
        for (uint32_t i = 0; i < n && r.ok(); i++) {
            uint32_t h = r.U32();
            ObjType type;
            int32_t signal;
            uint32_t owner, recursion;
            uint64_t pos;
            r.Pod(type);
            r.Pod(signal);
            r.Pod(owner);
            r.Pod(recursion);
            std::string path = r.Str();
            r.Pod(pos);
            auto it = g_handles.find(h);
            if (it == g_handles.end() || it->second->type != type || FilePath(it->second) != path) {
                if (apply) LOGE("snapshot: handle %X (%s) is not this machine's", h, path.c_str());
                return r.Fail("kernel: another set of handles");
            }
            if (!apply) continue;
            Object& o = *it->second;
            o.signal = signal;
            o.owner = owner;
            o.recursion = recursion;
            SetFilePosition(it->second, pos);
        }
        if (apply) g_next_handle = next;
    }
    auto threads = Threads();
    if (r.U32() != threads.size()) return r.Fail("kernel: another set of threads");
    for (auto& t : threads)
        if (r.U32() != t->id || r.U32() != t->stack_top) return r.Fail("kernel: another set of threads");
    std::map<std::string, uint32_t> saved;
    for (uint32_t n = r.U32(); n && r.ok(); n--) {
        std::string name = r.Str();
        saved[name] = r.U32();
    }
    for (auto& [name, stack] : HostThreads())
        if (saved.count(name) ? saved[name] != stack : true) {
            if (apply)
                LOGE("snapshot: this machine's %s thread has its stack at %08X, the snapshot's %08X", name.c_str(),
                     stack, saved.count(name) ? saved[name] : 0);
            return r.Fail("kernel: the host's threads");
        }
    if (!apply) return;
    // guest objects this machine tracks: their state is now the loaded header's
    std::lock_guard<std::mutex> lock(g_ptr_lock);
    std::lock_guard<std::mutex> dl(g_dispatch);
    for (auto it = g_by_ptr.begin(); it != g_by_ptr.end();) {
        uint32_t guest = it->first;
        Object& o = *it->second.obj;
        uint8_t t = G<uint8_t>(guest);
        ObjType want = t <= 1 ? ObjType::Event : t == 2 ? ObjType::Mutant : t == 5 ? ObjType::Semaphore
                     : t == 8 || t == 9 ? ObjType::Timer : t == 6 ? o.type : ObjType::Other;
        if (R32(guest + 12) != kStamp || want != o.type) {
            it = g_by_ptr.erase(it);  // built again from its header when the title uses it
            continue;
        }
        Stamp(guest, it->second.serial);
        if (o.type != ObjType::Thread && o.type != ObjType::Other) o.signal = int32_t(R32(guest + 4));
        ++it;
    }
    g_cv.notify_all();
}

void LoadState(snapshot::Reader& r) { LoadOrCheck(r, true); }
void CheckState(snapshot::Reader& r) { LoadOrCheck(r, false); }

void LogInventory() {
    static const char* kTypes[] = {"event", "semaphore", "mutant", "timer", "thread", "file", "symlink", "device", "queue", "other"};
    {
        std::lock_guard<std::mutex> lock(g_handles_lock);
        LOGR("inventory: %zu handles, next %X", g_handles.size(), g_next_handle);
        std::map<uint32_t, ObjectRef> sorted(g_handles.begin(), g_handles.end());
        for (auto& [h, o] : sorted)
            LOGR("inventory:  handle %X %s guest %08X signal %d use %ld %s %s", h, kTypes[int(o->type)], o->guest,
                 o->signal, o.use_count(), o->name.c_str(), DescribeObject(o).c_str());
    }
    {
        std::lock_guard<std::mutex> lock(g_ptr_lock);
        size_t live = 0;
        for (auto& [g, e] : g_by_ptr) live += e.obj.use_count() > 1;
        LOGR("inventory: %zu guest objects by pointer (%zu held elsewhere), serial %u", g_by_ptr.size(), live, g_serial);
    }
    {
        std::lock_guard<std::mutex> lock(g_names_lock);
        for (auto& [n, w] : g_names) LOGR("inventory:  name %s %s", n.c_str(), w.expired() ? "(gone)" : "");
    }
    {
        std::lock_guard<std::mutex> lock(g_timer_lock);
        LOGR("inventory: %zu timer entries, %zu armed", g_timers.size(), g_timer_seq.size());
    }
    {
        std::lock_guard<std::mutex> lock(g_dpc_lock);
        LOGR("inventory: %zu DPCs queued", g_dpcs.size());
    }
    for (auto& t : Threads())
        LOGR("inventory: thread %X ethread %08X stack %08X suspend %d apcs %zu", t->id, t->ethread, t->stack_top,
             t->suspend.load(), t->apcs.size());
}

}  // namespace kernel
