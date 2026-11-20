#ifdef XBE_GAME_DLL
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

#ifdef _WIN32
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#ifdef XBE_GAME_DLL
// the C runtime functions and runtime services the game code DLL calls, exported cleanly even under cross-compilation
#pragma comment(linker, "/EXPORT:memcpy")
#pragma comment(linker, "/EXPORT:memmove")
#pragma comment(linker, "/EXPORT:memset")
#pragma comment(linker, "/EXPORT:__CxxFrameHandler3")
#pragma comment(linker, "/EXPORT:_CxxThrowException")
#pragma comment(linker, "/EXPORT:?x86_icall@@YAXAEAUX86Context@@PEAEI@Z")
#pragma comment(linker, "/EXPORT:?x86_resume@@YAXAEAUX86Context@@PEAEAEBUX86Resume@@@Z")
#pragma comment(linker, "/EXPORT:?x86_kernel@@YAXAEAUX86Context@@PEAEI@Z")
#pragma comment(linker, "/EXPORT:?x86_trap@@YAXAEAUX86Context@@IPEBD@Z")
#pragma comment(linker, "/EXPORT:?x86_priv@@YAXAEAUX86Context@@IPEBD@Z")
#pragma comment(linker, "/EXPORT:?x86_trace@@YAIAEAUX86Context@@II@Z")
#pragma comment(linker, "/EXPORT:?x86_rdtsc@@YA_KXZ")
#pragma comment(linker, "/EXPORT:?x86_cpuid@@YAXAEAUX86Context@@@Z")
#pragma comment(linker, "/EXPORT:sin")
#pragma comment(linker, "/EXPORT:cos")
#pragma comment(linker, "/EXPORT:tan")
#pragma comment(linker, "/EXPORT:atan2")
#pragma comment(linker, "/EXPORT:fmod")
#pragma comment(linker, "/EXPORT:remainder")
#pragma comment(linker, "/EXPORT:sqrt")
#pragma comment(linker, "/EXPORT:sqrtf")
#pragma comment(linker, "/EXPORT:log2")
#pragma comment(linker, "/EXPORT:exp2")
#pragma comment(linker, "/EXPORT:ldexp")
#pragma comment(linker, "/EXPORT:frexp")
#pragma comment(linker, "/EXPORT:x86_sin")
#pragma comment(linker, "/EXPORT:x86_cos")
#pragma comment(linker, "/EXPORT:x86_tan")
#pragma comment(linker, "/EXPORT:x86_atan2")
#pragma comment(linker, "/EXPORT:x86_exp2")
#pragma comment(linker, "/EXPORT:x86_log2")
#endif

namespace {

#ifdef XBE_GAME_DLL
uint8_t* AllocateNearModule(HMODULE mod, size_t size) {
    uintptr_t base = reinterpret_cast<uintptr_t>(mod);
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
    uintptr_t lo = base > 0x70000000ull ? (base - 0x70000000ull) & ~(gran - 1) : gran;
    uintptr_t hi = base + 0x70000000ull;
    for (uintptr_t addr = (base & ~(gran - 1)) - gran; addr >= lo; addr -= gran) {
        if (void* p = VirtualAlloc(reinterpret_cast<void*>(addr), size, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE))
            return static_cast<uint8_t*>(p);
        if (addr < gran) break;
    }
    for (uintptr_t addr = (base & ~(gran - 1)) + gran; addr < hi; addr += gran) {
        if (void* p = VirtualAlloc(reinterpret_cast<void*>(addr), size, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE))
            return static_cast<uint8_t*>(p);
    }
    return nullptr;
}

// Redirect direct calls (`FF 15`/`FF 25` via IAT and `E8`/`E9` relative) inside `Nightfire.dll` to any
// `X86_FUNC(sub_XXXXXXXX)` hook exported by `Nightfire.exe` that wasn't in `Nightfire.lib` when `Nightfire.dll` was linked.
size_t PatchUnlinkedDllHooks(HMODULE dll, HMODULE exe, const X86FuncMapping* mappings) {
    if (!dll || !exe || !mappings) return 0;
    auto* base = reinterpret_cast<uint8_t*>(dll);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    std::unordered_map<uintptr_t, uintptr_t> imp_to_hook;
    std::unordered_map<uintptr_t, uintptr_t> imp_to_stub;
    uint8_t* stub_page = nullptr;
    size_t stub_used = 0;

    for (const X86FuncMapping* m = mappings; m->host; ++m) {
        char name[32];
        std::snprintf(name, sizeof(name), "sub_%08X", m->guest);
        FARPROC hook = GetProcAddress(exe, name);
        if (!hook) continue;
        char imp_name[40];
        std::snprintf(imp_name, sizeof(imp_name), "__imp__sub_%08X", m->guest);
        FARPROC imp = GetProcAddress(dll, imp_name);
        uintptr_t hook_va = reinterpret_cast<uintptr_t>(hook);
        uintptr_t imp_va = imp ? reinterpret_cast<uintptr_t>(imp) : reinterpret_cast<uintptr_t>(m->host);
        if (!imp_va || imp_va == hook_va) continue;
        imp_to_hook[imp_va] = hook_va;
        if (reinterpret_cast<uintptr_t>(m->host) != hook_va)
            imp_to_hook[reinterpret_cast<uintptr_t>(m->host)] = hook_va;
        if (!stub_page) stub_page = AllocateNearModule(dll, 0x10000);
        if (stub_page && stub_used + 16 <= 0x10000) {
            uint8_t* stub = stub_page + stub_used;
            stub_used += 16;
            // mov rax, imm64; jmp rax
            stub[0] = 0x48;
            stub[1] = 0xB8;
            std::memcpy(stub + 2, &hook_va, 8);
            stub[10] = 0xFF;
            stub[11] = 0xE0;
            uintptr_t stub_va = reinterpret_cast<uintptr_t>(stub);
            imp_to_stub[imp_va] = stub_va;
            if (reinterpret_cast<uintptr_t>(m->host) != hook_va)
                imp_to_stub[reinterpret_cast<uintptr_t>(m->host)] = stub_va;
        }
    }
    if (imp_to_hook.empty()) return 0;

    size_t patched_iat = 0;
    size_t patched_calls = 0;
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec) {
        uint8_t* sec_base = base + sec->VirtualAddress;
        size_t len = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
        if (len < 8) continue;

        // 1. Patch any 8-byte function pointer slots (such as __imp____imp__sub_XXXXXXXX in .rdata / .data / IAT)
        // that X86_EXTERN_FUNC's dllimport calls (`FF 15 [rip+disp32]`) dereference at runtime.
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
            DWORD old_prot = 0;
            if (VirtualProtect(sec_base, len, PAGE_READWRITE, &old_prot)) {
                for (size_t off = 0; off + 8 <= len; off += 8) {
                    uintptr_t val = 0;
                    std::memcpy(&val, sec_base + off, 8);
                    auto it = imp_to_hook.find(val);
                    if (it != imp_to_hook.end()) {
                        uintptr_t hook_va = it->second;
                        std::memcpy(sec_base + off, &hook_va, 8);
                        ++patched_iat;
                    }
                }
                VirtualProtect(sec_base, len, old_prot, &old_prot);
            }
            continue;
        }

        // 2. Also patch any executable section instructions (`FF 15`/`FF 25` RIP-relative indirect calls/jumps
        // and `E8`/`E9` direct relative calls/jumps).
        DWORD old_prot = 0;
        if (!VirtualProtect(sec_base, len, PAGE_EXECUTE_READWRITE, &old_prot)) continue;
        uintptr_t dll_end = reinterpret_cast<uintptr_t>(base) + nt->OptionalHeader.SizeOfImage;
        for (size_t i = 0; i + 6 <= len; ++i) {
            if (sec_base[i] == 0xFF && (sec_base[i + 1] == 0x15 || sec_base[i + 1] == 0x25)) {
                int32_t disp = 0;
                std::memcpy(&disp, sec_base + i + 2, 4);
                uintptr_t slot_va = reinterpret_cast<uintptr_t>(sec_base + i + 6) + intptr_t(disp);
                if (slot_va >= reinterpret_cast<uintptr_t>(base) && slot_va + 8 <= dll_end) {
                    uintptr_t target_fn = 0;
                    std::memcpy(&target_fn, reinterpret_cast<const void*>(slot_va), 8);
                    auto it = imp_to_hook.find(target_fn);
                    if (it != imp_to_hook.end()) {
                        DWORD slot_prot = 0;
                        if (VirtualProtect(reinterpret_cast<void*>(slot_va), 8, PAGE_READWRITE, &slot_prot)) {
                            uintptr_t hook_va = it->second;
                            std::memcpy(reinterpret_cast<void*>(slot_va), &hook_va, 8);
                            VirtualProtect(reinterpret_cast<void*>(slot_va), 8, slot_prot, &slot_prot);
                            ++patched_iat;
                        }
                        ++patched_calls;
                        i += 5;
                        continue;
                    }
                }
            }
            if ((sec_base[i] == 0xE8 || sec_base[i] == 0xE9) && !imp_to_stub.empty()) {
                int32_t rel = 0;
                std::memcpy(&rel, sec_base + i + 1, 4);
                uintptr_t target = reinterpret_cast<uintptr_t>(sec_base + i + 5) + intptr_t(rel);
                auto it = imp_to_stub.find(target);
                if (it != imp_to_stub.end()) {
                    intptr_t new_rel = intptr_t(it->second) - reinterpret_cast<intptr_t>(sec_base + i + 5);
                    if (new_rel >= INT32_MIN && new_rel <= INT32_MAX) {
                        int32_t r32 = int32_t(new_rel);
                        std::memcpy(sec_base + i + 1, &r32, 4);
                        ++patched_calls;
                        i += 4;
                    }
                }
            }
        }
        VirtualProtect(sec_base, len, old_prot, &old_prot);
        FlushInstructionCache(GetCurrentProcess(), sec_base, len);
    }
    if (stub_page && stub_used) FlushInstructionCache(GetCurrentProcess(), stub_page, stub_used);
    LOGR("dispatch: patched %zu IAT slots and %zu direct call sites in DLL across %zu hooks",
         patched_iat, patched_calls, imp_to_hook.size());
    return patched_iat + patched_calls;
}
#endif

std::vector<X86Func*> g_table;  // one slot per code byte
std::vector<bool> g_continuation;
uint32_t g_lo = 0, g_hi = 0;
#ifdef XBE_GAME_DLL
HMODULE g_code_dll = nullptr;
// the recompiled code's tables, exported by the game code DLL
const X86FuncMapping* g_mappings = nullptr;
const uint32_t* g_continuations = nullptr;
uint32_t g_entry = 0;
#else
// the headless harness links them in
const X86FuncMapping* g_mappings = X86FuncMappings;
const uint32_t* g_continuations = X86Continuations;
uint32_t g_entry = X86EntryPoint;
#endif

struct Export {
    const char* name = nullptr;
    kernel::KernelFn fn = nullptr;
    uint32_t data = 0;
};
Export g_exports[400];

}  // namespace

// synthetic guest addresses for kernel functions reached through a pointer
constexpr uint32_t kKernelThunkBase = 0xFE000000;

namespace dispatch {

#ifdef XBE_GAME_DLL
// the game code DLL and its tables; data comes by name, as the delay loaded imports carry only functions
bool Load(const std::filesystem::path& dll) {
    HMODULE m = LoadLibraryW(dll.c_str());
    if (!m) return false;
    g_code_dll = m;
    g_mappings = reinterpret_cast<const X86FuncMapping*>(GetProcAddress(m, "X86FuncMappings"));
    g_continuations = reinterpret_cast<const uint32_t*>(GetProcAddress(m, "X86Continuations"));
    auto* lo = reinterpret_cast<const uint32_t*>(GetProcAddress(m, "X86CodeBase"));
    auto* hi = reinterpret_cast<const uint32_t*>(GetProcAddress(m, "X86CodeEnd"));
    auto* entry = reinterpret_cast<const uint32_t*>(GetProcAddress(m, "X86EntryPoint"));
    if (!g_mappings || !g_continuations || !lo || !hi || !entry) return false;
    g_lo = *lo;
    g_hi = *hi;
    g_entry = *entry;
    return true;
}
#endif

uint32_t EntryPoint() { return g_entry; }
bool InCode(uint32_t va) { return va >= g_lo && va < g_hi; }

void Init() {
#ifndef XBE_GAME_DLL
    g_lo = X86CodeBase;
    g_hi = X86CodeEnd;
#endif
    g_table.assign(g_hi - g_lo, nullptr);
    size_t n = 0;
#ifdef XBE_GAME_DLL
    HMODULE exe_mod = GetModuleHandleW(nullptr);
    PatchUnlinkedDllHooks(g_code_dll, exe_mod, g_mappings);
#endif
    for (const X86FuncMapping* m = g_mappings; m->host; m++, n++) {
        X86Func* fn = m->host;
#ifdef XBE_GAME_DLL
        if (exe_mod) {
            char name[32];
            std::snprintf(name, sizeof(name), "sub_%08X", m->guest);
            if (auto* hook = reinterpret_cast<X86Func*>(GetProcAddress(exe_mod, name)); hook && hook != fn) {
                fn = hook;
            }
        }
#endif
        if (m->guest >= g_lo && m->guest < g_hi) g_table[m->guest - g_lo] = fn;
    }
    g_continuation.assign(g_hi - g_lo, false);
    for (const uint32_t* c = g_continuations; *c; c++)
        if (*c >= g_lo && *c < g_hi) g_continuation[*c - g_lo] = true;
    LOGI("dispatch: %zu functions over %08X..%08X", n, g_lo, g_hi);
}

X86Func* Find(uint32_t guest) {
    if (guest >= g_lo && guest < g_hi) return g_table[guest - g_lo];
    return nullptr;
}

// host code the guest may call through a pointer gets an address in this range
constexpr uint32_t kHostThunkBase = 0xFD000000;
static std::vector<X86Func*> g_host_thunks;
static std::mutex g_host_lock;

uint32_t HostThunk(X86Func* fn) {
    std::lock_guard<std::mutex> lock(g_host_lock);
    for (size_t i = 0; i < g_host_thunks.size(); i++)
        if (g_host_thunks[i] == fn) return kHostThunkBase + uint32_t(i) * 4;
    g_host_thunks.push_back(fn);
    return kHostThunkBase + uint32_t(g_host_thunks.size() - 1) * 4;
}

uint32_t KernelThunkAddress(uint32_t ordinal) { return kKernelThunkBase + ordinal * 4; }

}  // namespace dispatch

void x86_icall(X86Context& ctx, uint8_t* base, uint32_t target) {
    ctx.last_icall = target;
    if (target >= g_lo && target < g_hi && g_continuation[target - g_lo]) throw X86Resume{target, ctx.esp};
    if (X86Func* f = dispatch::Find(target)) {
        f(ctx, base);
        return;
    }
    if (target >= kKernelThunkBase && target < kKernelThunkBase + 400 * 4) {
        x86_kernel(ctx, base, (target - kKernelThunkBase) / 4);
        return;
    }
    if (target >= dispatch::kHostThunkBase) {
        size_t i = (target - dispatch::kHostThunkBase) / 4;
        X86Func* f = nullptr;
        {
            std::lock_guard<std::mutex> lock(dispatch::g_host_lock);
            if (i < dispatch::g_host_thunks.size()) f = dispatch::g_host_thunks[i];
        }
        if (f) {
            f(ctx, base);
            return;
        }
    }
    uint32_t ret = kernel::R32(ctx.esp);
    LOGE("indirect call to %08X has no recompiled function (return address %08X, esp %08X)", target, ret, ctx.esp);
    LOGE("add 0x%X to [functions] in the image's config/*.toml and rebuild", target);
    x86_trap(ctx, ret, "indirect call to unknown code");
}

void x86_resume(X86Context& ctx, uint8_t* base, const X86Resume& r) {
    ctx.esp = r.esp;
    dispatch::Find(r.target)(ctx, base);
}

void x86_kernel(X86Context& ctx, uint8_t* base, uint32_t ordinal) {
    if (ordinal < 400 && g_exports[ordinal].fn) {
        g_exports[ordinal].fn(ctx, base);
        return;
    }
    LOGE("kernel export %u (%s) is not implemented, return address %08X", ordinal, kernel::ExportName(ordinal),
         kernel::R32(ctx.esp));
    x86_trap(ctx, kernel::R32(ctx.esp), "unimplemented kernel export");
}

namespace kernel {

void RegisterExport(uint32_t ordinal, const char* name, KernelFn fn) {
    g_exports[ordinal].name = name;
    g_exports[ordinal].fn = fn;
}

static uint32_t g_kdata_next = mem::kKernelDataBase + 0x1000;

uint32_t RegisterData(uint32_t ordinal, const char* name, uint32_t size) {
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    if (g_exports[ordinal].data) return g_exports[ordinal].data;
    uint32_t a = (g_kdata_next + 15) & ~15u;
    g_kdata_next = a + ((size + 15) & ~15u);
    g_exports[ordinal].name = name;
    g_exports[ordinal].data = a;
    return a;
}

uint32_t DataAddress(uint32_t ordinal) { return ordinal < 400 ? g_exports[ordinal].data : 0; }
const char* ExportName(uint32_t ordinal) { return ordinal < 400 && g_exports[ordinal].name ? g_exports[ordinal].name : OrdinalName(ordinal); }
bool HasExport(uint32_t ordinal) { return ordinal < 400 && (g_exports[ordinal].fn || g_exports[ordinal].data); }

uint32_t CallGuest(X86Context& ctx, uint32_t fn, const uint32_t* args, size_t count) {
    // the D3D and DirectSound hooks call this several times a draw: no heap, only the registers put back are kept
    struct {
        uint32_t eax, ecx, edx, ebx, esi, edi, ebp;
    } saved{ctx.eax, ctx.ecx, ctx.edx, ctx.ebx, ctx.esi, ctx.edi, ctx.ebp};
    uint32_t esp = ctx.esp;
    for (const uint32_t* p = args + count; p != args;) {
        ctx.esp -= 4;
        W32(ctx.esp, *--p);
    }
    ctx.esp -= 4;
    W32(ctx.esp, 0xFFFFFFF0u);  // never used as a target, calls return through the host stack
    x86_icall(ctx, g_base, fn);
    uint32_t result = ctx.eax;
    ctx.eax = saved.eax;
    ctx.ecx = saved.ecx;
    ctx.edx = saved.edx;
    ctx.ebx = saved.ebx;
    ctx.esi = saved.esi;
    ctx.edi = saved.edi;
    ctx.ebp = saved.ebp;
    ctx.esp = esp;
    return result;
}

}  // namespace kernel

namespace kernel {
// return addresses found on the guest stack, the cheapest way to place a guest event
void LogGuestStack(X86Context& ctx, int words) {
    LOGE("  eax %08X ecx %08X edx %08X ebx %08X esp %08X ebp %08X esi %08X edi %08X", ctx.eax, ctx.ecx, ctx.edx, ctx.ebx,
         ctx.esp, ctx.ebp, ctx.esi, ctx.edi);
    for (int i = 0; i < words; i++) {
        uint32_t a = ctx.esp + uint32_t(i) * 4;
        if (!mem::IsCommitted(a)) break;
        uint32_t v = R32(a);
        if (dispatch::InCode(v)) LOGE("  [esp+%03X] %08X", i * 4, v);
    }
}
}  // namespace kernel

void x86_trap(X86Context& ctx, uint32_t va, const char* what) {
    LOGE("guest trap at %08X: %s", va, what);
    kernel::LogGuestStack(ctx, 64);
    logx::Flush();
#ifdef _WIN32
    __debugbreak();
#endif
    std::abort();
}

uint32_t x86_trace(X86Context& ctx, uint32_t va, uint32_t ret) {
    using kernel::R32;
    if (!ret) {
        ret = R32(ctx.esp);
        LOGI("trace %08X from %08X (%08X, %08X, %08X, %08X) ecx %08X", va, ret, R32(ctx.esp + 4), R32(ctx.esp + 8),
             R32(ctx.esp + 12), R32(ctx.esp + 16), ctx.ecx);
    } else {
        LOGI("trace %08X returned %08X to %08X", va, ctx.eax, ret);
    }
    return ret;
}

void x86_priv(X86Context& ctx, uint32_t va, const char* what) {
    static std::mutex m;
    static std::unordered_set<uint32_t> seen;
    std::lock_guard<std::mutex> lock(m);
    if (seen.insert(va).second) LOGW("privileged instruction %s at %08X ignored", what, va);
    (void)ctx;
}

// the guest measured time against a 733.33 MHz time stamp counter
uint64_t x86_rdtsc() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    auto ns = duration_cast<nanoseconds>(steady_clock::now() - start).count();
    uint64_t sec = uint64_t(ns) / 1000000000ull, rem = uint64_t(ns) % 1000000000ull;
    return sec * 733333333ull + rem * 733333333ull / 1000000000ull;
}

void x86_cpuid(X86Context& ctx) {
    switch (ctx.eax) {
        case 0:
            ctx.eax = 2;
            ctx.ebx = 0x756E6547;  // GenuineIntel
            ctx.edx = 0x49656E69;
            ctx.ecx = 0x6C65746E;
            break;
        case 1:
            ctx.eax = 0x0000068A;  // family 6 model 8, Pentium III Coppermine
            ctx.ebx = 0;
            ctx.ecx = 0;
            ctx.edx = 0x0383F9FF;  // fpu tsc cx8 cmov mmx fxsr sse
            break;
        default:
            ctx.eax = ctx.ebx = ctx.ecx = ctx.edx = 0;
            break;
    }
}
