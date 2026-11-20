// Co-op campaign: more enemies, and tougher, with more players. A level's characters (drones: 0x978-byte state
// blocks, class 2) come three ways, each through 0x2FC50 (a spawn record: +4 the place, +0x10 the turn, +0x20 the
// level's 0x21 settings words, copied to the state's +0x894): placed by the level's data as it loads (0xA4BC0's type
// 0x0F, 0x2FCF0), spawned by its script (0xC2990's event 8, 0x2FD80) and by its spawners (0x323E0: a list of spawn
// points at the spawner's +0x38, 0xAC bytes each, the record at +8). Each is set up by 0x42A90 (the placed ones at
// the level's start, 0x31FD0, the others as they spawn), which sets the health ([state+0x90], its most +0x94: 12
// for most guards) from zero. The team ([state+0x44]) is 1 for the enemies (2 and 3 the others: allies, civilians).
//   In a co-op session every enemy comes with one more per extra player, beside it, and every enemy's health is
// 1 + 0.5 per extra player of the game's (the host's net_coop_more_enemies and net_coop_tougher_enemies, each on by
// default). Every machine runs the same calls on the same frames: lockstep holds.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

X86_EXTERN_FUNC(__imp__sub_0002FC50);
X86_EXTERN_FUNC(__imp__sub_0002FCF0);
X86_EXTERN_FUNC(__imp__sub_0002FD80);
X86_EXTERN_FUNC(__imp__sub_00031FD0);
X86_EXTERN_FUNC(__imp__sub_000323E0);
X86_EXTERN_FUNC(__imp__sub_00042A90);

namespace net = nf::net;

namespace {
using kernel::R32;
using kernel::W32;

constexpr uint32_t kPlacedRet = 0xA56B9;  // 0xA4BC0's call of 0x2FCF0: a level's placed character
constexpr uint32_t kPointSize = 0xAC, kRecordSize = kPointSize - 8;
constexpr uint8_t kEnemyTeam = 1;
constexpr float kOffsets[3][2] = {{0.8f, 0.0f}, {-0.8f, 0.0f}, {0.0f, -0.8f}};  // x, z beside the original

float GetF(uint32_t va) {
    uint32_t v = R32(va);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
void SetF(uint32_t va, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    W32(va, v);
}

int Extra() { return std::max(0, nf::CoopCampaignPlayers() - 1); }
int Copies() { return net::CoopMoreEnemies() ? Extra() : 0; }         // net_coop_more_enemies
int Toughness() { return net::CoopTougherEnemies() ? Extra() : 0; }   // net_coop_tougher_enemies

bool Enemy(uint32_t obj) {
    uint32_t in = obj ? R32(obj + 0xBC) : 0;
    return in && kernel::G<uint8_t>(obj + 0xDB) == 0x02 && kernel::G<uint8_t>(in + 0x44) == kEnemyTeam;
}

// a cdecl call of the game's with its arguments, every register as it was after (eax the call's answer returned)
template <class Fn>
uint32_t Call(X86Context& ctx, uint8_t* base, Fn fn, std::initializer_list<uint32_t> args) {
    uint32_t eax = ctx.eax, ebx = ctx.ebx, ecx = ctx.ecx, edx = ctx.edx;
    uint32_t esi = ctx.esi, edi = ctx.edi, ebp = ctx.ebp, esp = ctx.esp;
    for (auto it = std::rbegin(args); it != std::rend(args); ++it) X86_PUSH32(*it);
    X86_PUSH32(0u);
    fn(ctx, base);
    uint32_t ret = ctx.eax;
    ctx.eax = eax, ctx.ebx = ebx, ctx.ecx = ecx, ctx.edx = edx;
    ctx.esi = esi, ctx.edi = edi, ctx.ebp = ebp, ctx.esp = esp;
    return ret;
}

// guest memory below the stack for a call's copies: the stack moved below it for the calls
struct Scratch {
    X86Context& ctx;
    uint32_t esp, va;
    Scratch(X86Context& c, uint32_t size) : ctx(c), esp(c.esp) {
        va = (esp - size - 0x40) & ~15u;
        ctx.esp = va - 0x10;
    }
    ~Scratch() { ctx.esp = esp; }
};

constexpr uint32_t kDataSize = 0x2C + 0x21 * 4;  // what 0x2FCF0 reads of a character's data: its 0x21 words at +0x2C
struct PlacedOne {
    uint32_t obj, pos[3], rot[3];
    uint8_t data[kDataSize];  // the loader's, gone by the level's start
};
std::vector<PlacedOne> g_placed;  // the level's load to its start

void Placed(uint32_t pos, uint32_t to, int k) {
    SetF(to, GetF(pos) + kOffsets[k][0]);
    SetF(to + 4, GetF(pos + 4));
    SetF(to + 8, GetF(pos + 8) + kOffsets[k][1]);
}
}  // namespace

// a character's setup (cdecl: the character): an enemy's health with the players
X86_FUNC(sub_00042A90) {
    uint32_t obj = R32(ctx.esp + 4);
    __imp__sub_00042A90(ctx, base);
    int extra = Toughness();
    if (!extra || !Enemy(obj)) return;
    uint32_t in = R32(obj + 0xBC);
    float scale = 1.0f + 0.5f * float(extra);
    SetF(in + 0x90, GetF(in + 0x90) * scale);
    SetF(in + 0x94, GetF(in + 0x94) * scale);
    LOG_FIRST(3, "co-op: an enemy's health %.1f of %.1f (x%.1f)", GetF(in + 0x90), GetF(in + 0x94), scale);
}

// a level's placed character (cdecl: &place, &turn, the level's character data): its team is set with the others
// at the level's start (0x31FD0), its copies made then
X86_FUNC(sub_0002FCF0) {
    uint32_t ret = R32(ctx.esp), pos = R32(ctx.esp + 4), rot = R32(ctx.esp + 8), data = R32(ctx.esp + 12);
    __imp__sub_0002FCF0(ctx, base);
    if (ret != kPlacedRet || !ctx.eax || !Copies()) return;
    PlacedOne p{ctx.eax, {}, {}, {}};
    for (int i = 0; i < 3; i++) p.pos[i] = R32(pos + 4 * uint32_t(i)), p.rot[i] = R32(rot + 4 * uint32_t(i));
    std::memcpy(p.data, &kernel::G<uint8_t>(data), kDataSize);
    g_placed.push_back(p);
}

// the level's start (the placed characters set up): the placed enemies' copies, set up as a spawner's are
X86_FUNC(sub_00031FD0) {
    __imp__sub_00031FD0(ctx, base);
    std::vector<PlacedOne> placed;
    placed.swap(g_placed);
    int extra = Copies(), made = 0;
    if (!extra) return nf::ReportHeap();
    Scratch s(ctx, 0x20 + kDataSize);
    for (const PlacedOne& p : placed) {
        if (!Enemy(p.obj)) continue;
        for (int i = 0; i < 3; i++) W32(s.va + 0x10 + 4 * uint32_t(i), p.rot[i]);
        std::memcpy(&kernel::G<uint8_t>(s.va + 0x20), p.data, kDataSize);
        for (int k = 0; k < extra && k < 3; k++) {
            for (int i = 0; i < 3; i++) W32(s.va + 4 * uint32_t(i), p.pos[i]);
            Placed(s.va, s.va, k);
            if (uint32_t more = Call(ctx, base, __imp__sub_0002FCF0, {s.va, s.va + 0x10, s.va + 0x20})) {
                Call(ctx, base, sub_00042A90, {more});
                made++;
            }
        }
    }
    LOGR("co-op: %d more enemies beside the level's placed ones (%d more players)", made, extra);
    nf::ReportHeap();
}

// a character the level's script spawns (cdecl: &place, &turn, its four words, a word for [state+0x49E])
X86_FUNC(sub_0002FD80) {
    uint32_t a[7];
    for (int i = 0; i < 7; i++) a[i] = R32(ctx.esp + 4 + 4 * uint32_t(i));
    __imp__sub_0002FD80(ctx, base);
    uint32_t obj = ctx.eax;
    int extra = Copies();
    if (!extra || !Enemy(obj)) return;
    Scratch s(ctx, 0x10);
    for (int k = 0; k < extra && k < 3; k++) {
        Placed(a[0], s.va, k);
        Call(ctx, base, __imp__sub_0002FD80, {s.va, a[1], a[2], a[3], a[4], a[5], a[6]});
    }
    LOG_FIRST(3, "co-op: the script's enemy (%08X) with %d more beside it", obj, extra);
    ctx.eax = obj;
}

// a spawner's next character (cdecl: the spawner): the others from a copy of the same spawn point's record
X86_FUNC(sub_000323E0) {
    uint32_t spawner = R32(ctx.esp + 4);
    __imp__sub_000323E0(ctx, base);
    uint32_t obj = ctx.eax;
    int extra = Copies();
    if (!extra || !Enemy(obj)) return;
    uint32_t in = R32(spawner + 0xBC), points = in ? R32(in + 0x38) : 0, rec = 0;
    for (uint32_t i = 0, n = in ? kernel::G<uint16_t>(in + 8) : 0; points && i < n; i++)
        if (R32(points + i * kPointSize + 8) == obj) rec = points + i * kPointSize + 8;
    if (!rec) return;
    Scratch s(ctx, kRecordSize);
    for (int k = 0; k < extra && k < 3; k++) {
        std::memcpy(&kernel::G<uint8_t>(s.va), &kernel::G<uint8_t>(rec), kRecordSize);
        Placed(rec + 4, s.va + 4, k);
        if (uint32_t more = Call(ctx, base, __imp__sub_0002FC50, {s.va})) Call(ctx, base, sub_00042A90, {more});  // the health too
    }
    LOG_FIRST(3, "co-op: a spawner's enemy (%08X) with %d more beside it", obj, extra);
    ctx.eax = obj;
}
