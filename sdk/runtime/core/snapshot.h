#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "cpu/x86_context.h"

// A running title's whole state as bytes, for a machine that joins a session under way (or a save state): guest
// memory and its allocators, the kernel's files, Direct3D's and DirectSound's host state, then the title's own
// sections. Taken and loaded on the title's thread at a point the title's code is at the same frame of its loop on
// every machine (the title's hook decides where); the host's other threads that run guest code are held meanwhile.
namespace snapshot {

class Writer {
public:
    void Bytes(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        data.insert(data.end(), b, b + n);
    }
    template <typename T>
    void Pod(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        Bytes(&v, sizeof v);
    }
    void U32(uint32_t v) { Pod(v); }
    void Str(const std::string& s) {
        U32(uint32_t(s.size()));
        Bytes(s.data(), s.size());
    }
    template <typename T>
    void Vec(const std::vector<T>& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        U32(uint32_t(v.size()));
        Bytes(v.data(), v.size() * sizeof(T));
    }
    template <typename K, typename V>
    void Map(const std::map<K, V>& m) {
        U32(uint32_t(m.size()));
        for (auto& [k, v] : m) {
            Pod(k);
            Pod(v);
        }
    }
    std::vector<uint8_t> data;
};

class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), end_(p + n) {}
    bool Bytes(void* out, size_t n) {
        if (size_t(end_ - p_) < n) {
            ok_ = false;
            std::memset(out, 0, n);
            return false;
        }
        std::memcpy(out, p_, n);
        p_ += n;
        return true;
    }
    template <typename T>
    void Pod(T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        Bytes(&v, sizeof v);
    }
    uint32_t U32() {
        uint32_t v = 0;
        Pod(v);
        return v;
    }
    std::string Str() {
        uint32_t n = U32();
        if (size_t(end_ - p_) < n) return ok_ = false, std::string();
        std::string s(reinterpret_cast<const char*>(p_), n);
        p_ += n;
        return s;
    }
    template <typename T>
    void Vec(std::vector<T>& v) {
        uint32_t n = U32();
        if (size_t(end_ - p_) / sizeof(T) < n) return void(ok_ = false);
        v.resize(n);
        Bytes(v.data(), n * sizeof(T));
    }
    template <typename K, typename V>
    void Map(std::map<K, V>& m) {
        m.clear();
        uint32_t n = U32();
        for (uint32_t i = 0; i < n && ok_; i++) {
            K k{};
            V v{};
            Pod(k);
            Pod(v);
            m.emplace(k, v);
        }
    }
    // the section's own check failed: the whole load is refused
    void Fail(const char* why) {
        if (ok_) why_ = why;
        ok_ = false;
    }
    bool ok() const { return ok_; }
    const char* why() const { return why_; }
    size_t left() const { return size_t(end_ - p_); }
    // the next n bytes in place (null when short)
    const uint8_t* Skip(size_t n) {
        if (left() < n) return ok_ = false, nullptr;
        const uint8_t* at = p_;
        p_ += n;
        return at;
    }

private:
    const uint8_t* p_;
    const uint8_t* end_;
    bool ok_ = true;
    const char* why_ = "short";
};

using SaveFn = void (*)(Writer&);
using LoadFn = void (*)(Reader&);
// a title's section, saved after the runtime's: from GameInfo::init or later, never a static initializer
void Register(const char* name, SaveFn save, LoadFn load);

// on the title's thread at its snapshot point, ctx its context there (its registers are saved too)
std::vector<uint8_t> Save(X86Context& ctx);
// false (and the reason logged) when the bytes are not a snapshot of this build's sections; the state is then
// partly loaded and the title cannot go on
bool Load(const std::vector<uint8_t>& bytes, X86Context& ctx);
// whether Load would take them here and now (the same handles, threads and snapshot point): null, or why not
const char* Check(const std::vector<uint8_t>& bytes, X86Context& ctx);

// LZ4, the raw size first: Compress's bytes, false when they are not
std::vector<uint8_t> Compress(const std::vector<uint8_t>& raw);
bool Decompress(const uint8_t* p, size_t n, std::vector<uint8_t>& raw);

// held by the host's threads around their calls into guest code (vertical blank, DPCs, DirectSound), and by a
// snapshot: none runs guest code while one is taken or loaded
std::recursive_mutex& GuestLock();
// the title's own threads instead (kernel::Thread::parked): a save or load waits for every other one to be parked in
// a kernel wait, delay or yield, and one leaving the kernel meanwhile holds in Unpark till it is done (a thread that
// woke into guest memory a load had cleared read none of its own: the game's service thread left its loop and
// returned through a stack of zeros, 2026-11-14)
void Unpark(std::atomic<bool>& parked);

}  // namespace snapshot
