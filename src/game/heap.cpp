// The game's level heap: 0x70A40 makes it (0x3100000 bytes of contiguous memory from 0xDC870, [0x223A80] its start,
// [0x223A88] its size) and 0x70AE0 (cdecl: size, tag, alignment) allocates from it, the fit [0x223A7C] picks (0: the
// next fit from [0x2237D8]). A block's 12-byte header: +0 the block before, +4 its size, +0xB 1 when free.
//   A next fit that finds no block stores 0 as its next place and returns 0, which the callers take (0x2DC10's
// objects, 0xCD140's effects); the next allocation then reads through the 0 (0x70BC9). A co-op Phoenix Fire filled
// the heap as the helicopter fell (John's 2026-11-15, both machines). Here the next fit starts over at the heap's
// start instead, and a full heap's failures are reported.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

X86_EXTERN_FUNC(__imp__sub_00070AE0);

namespace {
using kernel::R32;
namespace net = nf::net;

constexpr uint32_t kRover = 0x2237D8, kFit = 0x223A7C, kStart = 0x223A80, kSize = 0x223A88;

struct HeapFree {
    uint32_t free = 0, largest = 0, blocks = 0;
};
// the heap's used blocks by their tag ([+8], 0x70AE0's second argument: 0x404 0x2DC10's objects, 0x1204 the
// level's data), the most bytes first
void ReportTags() {
    std::map<uint16_t, std::pair<uint32_t, uint32_t>> tags;  // bytes, blocks
    uint32_t start = R32(kStart), end = start + R32(kSize) - 0xC;
    for (uint32_t b = start; start && b < end;) {
        uint32_t n = R32(b + 4);
        if (!n) break;
        if (!kernel::G<uint8_t>(b + 0xB)) {
            auto& t = tags[kernel::R16(b + 8)];
            t.first += n, t.second++;
        }
        b += n;
    }
    std::vector<std::pair<uint16_t, std::pair<uint32_t, uint32_t>>> top(tags.begin(), tags.end());
    std::sort(top.begin(), top.end(), [](auto& a, auto& b) { return a.second.first > b.second.first; });
    std::string text;
    char one[64];
    for (size_t i = 0; i < top.size() && i < 8; i++) {
        std::snprintf(one, sizeof one, "%s%04X %u KB in %u", i ? ", " : "", top[i].first, top[i].second.first >> 10,
                      top[i].second.second);
        text += one;
    }
    LOGR("nightfire: the full heap's blocks by tag: %s", text.c_str());
}

HeapFree Walk() {
    HeapFree h;
    uint32_t start = R32(kStart), size = R32(kSize);
    if (!start) return h;
    uint32_t end = start + size - 0xC;
    for (uint32_t b = start; b < end;) {
        uint32_t n = R32(b + 4);
        if (!n) break;
        if (kernel::G<uint8_t>(b + 0xB)) {
            h.free += n;
            if (n > h.largest) h.largest = n;
        }
        h.blocks++;
        b += n;
    }
    return h;
}

uint32_t g_failed = 0;  // the allocations the full heap refused since the last level's start
// headless tests only: NF_HEAP_FILL=<session tick> fills the heap at the first next fit from that tick on (blocks
// tagged 0xFF04 till none is left), as Phoenix Fire's helicopter did, so the game plays on with a full heap

uint32_t Allocate(X86Context& ctx, uint8_t* base, uint32_t size) {
    if (R32(kFit) == 0 && R32(kRover) == 0) kernel::W32(kRover, R32(kStart));
    if (R32(kFit) == 0 && R32(kRover) == 0) kernel::W32(kRover, R32(kStart));
    X86Context saved = ctx;
    X86_PUSH32(0u);
    X86_PUSH32(0xFF04u);
    X86_PUSH32(size);
    X86_PUSH32(0u);
    __imp__sub_00070AE0(ctx, base);
    uint32_t got = ctx.eax;
    ctx = saved;
    return got;
}
}  // namespace

namespace nf {
void ReportHeap() {
    if (g_failed) LOGR("nightfire: the last level's full heap refused %u allocations", g_failed);
    g_failed = 0;
    HeapFree h = Walk();
    LOGR("nightfire: level %08X starts with %u KB of the game's heap free (the largest block %u KB, %u blocks; "
         "%u KB of the console's memory free)",
         R32(0x1F658C), h.free >> 10, h.largest >> 10, h.blocks, mem::Query().available_pages * 4);
}
}  // namespace nf

X86_FUNC(sub_00070AE0) {
    static uint32_t fill_tick = [] {
        const char* e = std::getenv("NF_HEAP_FILL");
        return e && *e ? uint32_t(std::strtoul(e, nullptr, 0)) : 0u;
    }();
    if (fill_tick && R32(kFit) == 0 && net::Tick() >= fill_tick) {
        fill_tick = 0;
        uint32_t bytes = 0;
        for (uint32_t size = 1u << 20; size >= 16; size >>= 1)
            while (uint32_t got = Allocate(ctx, base, size)) bytes += size, (void)got;
        LOGR("nightfire: NF_HEAP_FILL took %u KB of the heap at tick %u", bytes >> 10, net::Tick());
    }
    if (R32(kFit) == 0 && R32(kRover) == 0) kernel::W32(kRover, R32(kStart));
    uint32_t size = R32(ctx.esp + 4);
    __imp__sub_00070AE0(ctx, base);
    if (ctx.eax) return;
    ++g_failed;
    if ((g_failed & (g_failed - 1)) == 0) {
        HeapFree h = Walk();
        LOGR("nightfire: the game's heap is full: %u bytes asked, refused (%u such so far; %u KB free, the largest "
             "block %u bytes)", size, g_failed, h.free >> 10, h.largest);
        if (g_failed == 1) ReportTags();
    }
}
