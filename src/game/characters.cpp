// The multiplayer characters a co-op campaign's players chose, registered with the level as it loads.
//
// A campaign level's archive packs its own cast alone; a match's packs all 29 characters' models (type 3) and
// texture packs (type 2), the multiplayer first-person hands (050000B0) and their four skin packs. This reads the
// chosen ones out of the first multiplayer map's archive and hands them to the level loader's own dispatcher just
// before the level's last entry, its main pack (type 1), so that entry's 0x11200 gives them Bond's hardpoints as a
// match's does. With them, on every co-op level, a match's third-person player animations (see Stances), which the
// four stance functions below then play by weapon. Their data stays in the game heap until the next level resets it.
// Sessions only: with no session the level loads and plays exactly as it always has.
//
// An archive stores a texture once: a later copy is an alias ("TD" and a slot) of the first, and the slot is where
// that first one lands in the game's texture table (0x2ABE80, filled from 1 by 0xDDF90 as each pack of types 0, 1, 2
// and 11 loads, in the archive's order) when the whole archive loads. 14 of the 29 characters' packs alias another
// pack's texture, so each pack is rebuilt with its aliases replaced by the textures they name, numbered as that load
// would.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <vector>

#include "core/snapshot.h"
#include "common/net.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

X86_EXTERN_FUNC(__imp__sub_00017220);
X86_EXTERN_FUNC(__imp__sub_00017B90);
X86_EXTERN_FUNC(__imp__sub_0006B500);
X86_EXTERN_FUNC(__imp__sub_00070AE0);
X86_EXTERN_FUNC(__imp__sub_000A91E0);
X86_EXTERN_FUNC(__imp__sub_000A9350);
X86_EXTERN_FUNC(__imp__sub_000A94C0);
X86_EXTERN_FUNC(__imp__sub_000A9590);
X86_EXTERN_FUNC(__imp__sub_000BE7B0);
X86_EXTERN_FUNC(__imp__sub_000BE910);
X86_EXTERN_FUNC(__imp__sub_000D4BC0);
X86_EXTERN_FUNC(__imp__sub_000E2960);
X86_EXTERN_FUNC(__imp__sub_000E3100);
X86_EXTERN_FUNC(__imp__sub_000E31A0);
X86_EXTERN_FUNC(__imp__sub_000E31F0);
X86_EXTERN_FUNC(__imp__sub_000E3220);

namespace net = nf::net;

namespace {
using kernel::R32;
using kernel::W32;

constexpr uint32_t kTable = 0x1637C0;  // the 29 characters, 0x10 each: model, texture pack, hand skin
constexpr uint32_t kCharacters = 29;
constexpr uint32_t kHandTextures = 0x1634C0;  // the 8 hand skins' textures
constexpr uint32_t kMpHands = 0x050000B0;
// skins 4-7's textures (020009E6, 020009FF, 02000A14, 02000A13) are in these packs, one each
constexpr uint32_t kHandPacks[4] = {0x010001FD, 0x01000200, 0x01000202, 0x01000201};
constexpr char kArchive[] = "07000023.bin";

// the level loader 0xBE910 (state 4 dispatches one entry a call through 0xBE7B0) and its current entry, which
// 0xDCA30 hands to the entry type's loader while archive mode is on
constexpr uint32_t kLoaderState = 0x27927C;
constexpr uint32_t kEntryData = 0x279168, kEntrySize = 0x279174, kEntryEnd = 0x279178, kEntryType = 0x279184;
constexpr uint32_t kEntryId = 0x2791C8;
constexpr uint32_t kArchiveMode = 0x2ADF70;
constexpr uint32_t kIsMultiplayer = 0x260018, kMatchMode = 0x1F65D4;

// The third-person player animations. A player's stance is their weapon's ([0x18D024 + 0x10C * weapon], 1-6): 0xA91E0
// and 0xA9350 play a set of 9 records by it (the sets 0x17E68C-0x17E8CC, 0x24 each), 0xA94C0 and 0xA9590 one record.
// Out of a match (!0x260018) every weapon gets one held pose (the sets 0x17E68C and 0x17E6B0, all 06000A00 and
// 060009FF; stance 3's record), and only that pose is in a campaign level: the match's records (type 5, 0x20 bytes
// each, an animation's ID low half at +0xE) and their animations (type 4) come from 07000023.bin with the characters
struct Stances {
    uint32_t by_stance[6], other;
};
constexpr Stances kSetsA91E0 = {{0x17E764, 0x17E884, 0x17E6D4, 0x17E7F4, 0x17E7AC, 0x17E83C}, 0x17E71C};
constexpr Stances kSetsA9350 = {{0x17E788, 0x17E8A8, 0x17E6F8, 0x17E818, 0x17E7D0, 0x17E860}, 0x17E740};
constexpr Stances kRecordA94C0 = {{0x60009DA, 0x60009AE, 0x60001DD, 0x60001D1, 0x60001E9, 0x60009B6}, 0x60001F4};
constexpr Stances kRecordA9590 = {{0x60009DD, 0x60009B1, 0x60001DC, 0x60001D0, 0x60001E8, 0x60009B9}, 0x60001F3};
constexpr uint32_t kSetsFrom = 0x17E68C, kSetsTo = 0x17E8CC;
constexpr uint32_t kRecordSize = 0x20;

// the archive (KXC: "KXC\4", a chunk count, then EDL chunks of at most 1 MB unpacked, each with a 12-byte header:
// magic and method, stored size with the header, unpacked size); unpacked, a 0x20 header (directory size, entry
// count), the directory (size, type, name: the ID is the hex after the last separator) and the data in its order
constexpr uint32_t kChunkMax = 0x100000;
constexpr uint32_t kSector = 0x800;
constexpr uint32_t kNameAt = 0, kIoAt = 0x1000, kIoSize = kChunkMax + 0x2000, kUnpackAt = kIoAt + kIoSize;
constexpr uint32_t kScratchSize = kUnpackAt + kChunkMax;

// a pack (types 0, 1, 2, 11): a word, then chunks of a header word (tag << 24 | size with the header) and a body; its
// first is 0x0E, its second the texture table (0x10: 12 bytes a texture), its textures 0x17 or 0x18 chunks in that
// order, each body a texture: a real one ("KXT\6", width +0xC, height +0x10, format +0x14, its pixels) or an alias
// ("TD" in the high half, the slot - 1 in the low)
constexpr uint32_t kKxt = 0x0654584B, kAlias = 0x4454;
constexpr uint8_t kTexTable = 0x10;
bool TexturesPack(uint8_t type) { return type == 0 || type == 1 || type == 2 || type == 11; }
bool TextureChunk(uint8_t tag) { return tag == 0x17 || tag == 0x18; }

struct Chunk {
    uint32_t file_off, stored, size, at;  // at: its place in the unpacked archive
};
struct Entry {
    uint32_t id, size, at;
    uint8_t type;
    uint32_t first = 0, textures = 0;  // its slots in a whole load of the archive
};
struct Directory {
    bool tried = false, ok = false;
    std::vector<Chunk> chunks;
    std::vector<Entry> entries;
    size_t numbered = 0;     // entries numbered so far, in order
    uint32_t next_slot = 1;  // 0xE02E0 starts the count at 1 (slot 0 is the default texture)
} g_dir;

uint32_t g_level = 0;       // the archive 0xBE910 is loading
bool g_registered = false;  // this load's characters are in (or were not wanted)

template <class Fn>
uint32_t Call(X86Context& ctx, uint8_t* base, Fn fn, std::initializer_list<uint32_t> args, uint32_t eax = 0) {
    uint32_t saved_eax = ctx.eax, saved_ebx = ctx.ebx, saved_ecx = ctx.ecx, saved_edx = ctx.edx;
    uint32_t saved_esi = ctx.esi, saved_edi = ctx.edi, saved_ebp = ctx.ebp, saved_esp = ctx.esp;
    for (auto it = std::rbegin(args); it != std::rend(args); ++it) {
        ctx.esp -= 4;
        W32(ctx.esp, *it);
    }
    ctx.esp -= 4;
    W32(ctx.esp, 0);
    ctx.eax = eax;
    fn(ctx, base);
    uint32_t ret = ctx.eax;
    ctx.eax = saved_eax, ctx.ebx = saved_ebx, ctx.ecx = saved_ecx, ctx.edx = saved_edx;
    ctx.esi = saved_esi, ctx.edi = saved_edi, ctx.ebp = saved_ebp, ctx.esp = saved_esp;
    return ret;
}

bool Registered(X86Context& ctx, uint8_t* base, uint32_t id) { return Call(ctx, base, __imp__sub_0006B500, {id}) != 0; }

uint32_t Le32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// the archive through the game's packed file system: 0xE3100 opens a file by name, 0xE31A0 (handle, buffer, size,
// offset low, high) reads whole sectors of its stored bytes, 0xE31F0 is the read's busy flag, 0xE3220 closes;
// 0xD4BC0 (to, chunk) unpacks one EDL chunk
class Reader {
  public:
    Reader(X86Context& ctx, uint8_t* base) : ctx_(ctx), base_(base) {}
    ~Reader() {
        if (handle_ > 0) Call(ctx_, base_, __imp__sub_000E3220, {uint32_t(handle_)});
        if (scratch_) mem::FreeContiguous(scratch_);
    }

    bool Open() {
        scratch_ = mem::AllocHostContiguous(kScratchSize, 0x1000);
        if (!scratch_) {
            LOGE("nightfire: co-op characters: no memory to read %s", kArchive);
            return false;
        }
        uint32_t name = scratch_ + kNameAt;
        std::memcpy(&kernel::G<char>(name), kArchive, sizeof(kArchive));
        if (int32_t(Call(ctx_, base_, __imp__sub_000E2960, {}, name)) < 0) {  // 0xE3100 stops the game on a missing name
            LOGE("nightfire: co-op characters: %s is not on this disc", kArchive);
            return false;
        }
        handle_ = int32_t(Call(ctx_, base_, __imp__sub_000E3100, {name}));
        return handle_ > 0;
    }

    // the stored bytes [off, off + size), at the returned guest address (valid until the next read)
    uint32_t Read(uint32_t off, uint32_t size) {
        uint32_t from = off & ~(kSector - 1), to = (off + size + kSector - 1) & ~(kSector - 1);
        if (to - from > kIoSize) return 0;
        uint32_t io = scratch_ + kIoAt;
        Call(ctx_, base_, __imp__sub_000E31A0, {uint32_t(handle_), io, to - from, from, 0u});
        for (int spin = 0; spin < 10000000 && (Call(ctx_, base_, __imp__sub_000E31F0, {uint32_t(handle_)}) & 0xFF);
             spin++) {
        }
        return io + (off - from);
    }

    // chunk i unpacked, at the returned guest address
    uint32_t Unpack(size_t i) {
        uint32_t to = scratch_ + kUnpackAt;
        if (unpacked_ == int(i)) return to;
        const Chunk& c = g_dir.chunks[i];
        uint32_t from = Read(c.file_off, c.stored);
        unpacked_ = -1;
        if (!from || !Call(ctx_, base_, __imp__sub_000D4BC0, {to, from})) return 0;
        unpacked_ = int(i);
        return to;
    }

    // the unpacked archive's [at, at + size) copied to out
    bool Bytes(uint32_t at, void* out, uint32_t size) {
        auto* dst = static_cast<uint8_t*>(out);
        for (size_t i = 0; i < g_dir.chunks.size() && size; i++) {
            const Chunk& c = g_dir.chunks[i];
            if (at >= c.at + c.size || at < c.at) continue;
            uint32_t src = Unpack(i);
            if (!src) return false;
            uint32_t n = std::min(size, c.at + c.size - at);
            std::memmove(dst, &kernel::G<uint8_t>(src + (at - c.at)), n);
            dst += n, at += n, size -= n;
        }
        return size == 0;
    }

  private:
    X86Context& ctx_;
    uint8_t* base_;
    uint32_t scratch_ = 0;
    int32_t handle_ = 0;
    int unpacked_ = -1;
};

// the chunk table and the directory, read once: they never change
bool ReadDirectory(Reader& rd) {
    uint32_t p = rd.Read(0, 8);
    if (!p || R32(p) != 0x0443584B || R32(p + 4) == 0 || R32(p + 4) > 256) {
        LOGE("nightfire: co-op characters: %s is not a KXC archive", kArchive);
        return false;
    }
    uint32_t count = R32(p + 4), file_off = 8, at = 0;
    for (uint32_t i = 0; i < count; i++) {
        p = rd.Read(file_off, 12);
        uint32_t stored = p ? R32(p + 4) : 0, size = p ? R32(p + 8) : 0;
        if (!p || (R32(p) & 0xFFFFFF) != 0x4C4445 || stored <= 12 || stored > kChunkMax + 12 || size > kChunkMax) {
            LOGE("nightfire: co-op characters: %s's chunk %u at %X is not an EDL chunk", kArchive, i, file_off);
            return false;
        }
        g_dir.chunks.push_back({file_off, stored, size, at});
        file_off += stored, at += size;
    }
    uint32_t head[2];
    if (!rd.Bytes(0, head, 8) || 0x20 + head[0] > g_dir.chunks[0].size) {
        LOGE("nightfire: co-op characters: %s's directory could not be read", kArchive);
        return false;
    }
    uint32_t dir_size = head[0], entries = head[1];
    std::vector<char> dir(dir_size + 1, 0);
    if (!rd.Bytes(0x20, dir.data(), dir_size)) return false;
    uint32_t pos = 0, data = 0x20 + dir_size;
    for (uint32_t i = 0; i < entries && pos + 5 < dir_size; i++) {
        uint32_t size;
        std::memcpy(&size, &dir[pos], 4);
        uint8_t type = uint8_t(dir[pos + 4]);
        const char* name = &dir[pos + 5];
        const char* file = name;
        for (const char* c = name; *c; c++)
            if (*c == '\\' || *c == '/') file = c + 1;
        char* end = nullptr;
        uint32_t id = uint32_t(std::strtoul(file, &end, 16));
        g_dir.entries.push_back({end != file ? id : 0xFFFFFFFFu, size, data, type});
        data += size;
        pos += 5 + uint32_t(std::strlen(name)) + 1;
    }
    return true;
}

const Entry* Find(uint32_t id, uint8_t type) {
    for (const Entry& e : g_dir.entries)
        if (e.id == id && e.type == type) return &e;
    return nullptr;
}

// the entry holding slot `slot` in a whole load of the archive, and its texture there; the packs are numbered as far
// as that needs, once
const Entry* SlotOwner(Reader& rd, uint32_t slot, uint32_t& local) {
    while (g_dir.next_slot <= slot && g_dir.numbered < g_dir.entries.size()) {
        Entry& e = g_dir.entries[g_dir.numbered++];
        if (!TexturesPack(e.type)) continue;
        uint8_t head[8];
        uint32_t table = 0;
        if (e.size >= 0x20 && rd.Bytes(e.at + 4, head, 4) && rd.Bytes(e.at + 4 + (Le32(head) & 0xFFFFFF), head + 4, 4) &&
            (Le32(head + 4) >> 24) == kTexTable)
            table = Le32(head + 4) & 0xFFFFFF;
        if (table < 4) {
            LOGE("nightfire: co-op characters: %08X (entry %zu) has no texture table; slots from %X are unknown", e.id,
                 g_dir.numbered - 1, g_dir.next_slot);
            g_dir.numbered = g_dir.entries.size();
            break;
        }
        e.first = g_dir.next_slot;
        e.textures = (table - 4) / 12;
        g_dir.next_slot += e.textures;
    }
    for (size_t i = 0; i < g_dir.numbered; i++) {
        const Entry& e = g_dir.entries[i];
        if (e.textures && slot >= e.first && slot < e.first + e.textures) {
            local = slot - e.first;
            return &e;
        }
    }
    return nullptr;
}

// the pack's texture `local` (its chunk, header and body)
bool TextureOf(Reader& rd, const Entry& e, uint32_t local, std::vector<uint8_t>& out) {
    uint32_t pos = 4, k = 0;
    while (pos + 4 <= e.size) {
        uint8_t head[4];
        if (!rd.Bytes(e.at + pos, head, 4)) return false;
        uint32_t size = Le32(head) & 0xFFFFFF;
        if (size < 4 || pos + size > e.size) return false;
        if (TextureChunk(Le32(head) >> 24) && k++ == local) {
            out.resize(size);
            return rd.Bytes(e.at + pos, out.data(), size);
        }
        pos += size;
    }
    return false;
}

// the pack's bytes with each alias replaced by the texture it names (an alias whose texture is not found, or not of
// its size and format, is left with no picture: 0xDDF90 gives such a texture the default one)
bool Rebuild(Reader& rd, const Entry& e, std::vector<uint8_t>& out, uint32_t& aliases, uint32_t& blank) {
    std::vector<uint8_t> in(e.size), tex;
    if (!rd.Bytes(e.at, in.data(), e.size)) return false;
    out.assign(in.begin(), in.begin() + std::min<uint32_t>(4, e.size));
    uint32_t pos = 4;
    while (pos + 4 <= e.size) {
        uint32_t head = Le32(&in[pos]), size = head & 0xFFFFFF;
        if (size < 4 || pos + size > e.size) break;  // the stream's end
        if (TextureChunk(head >> 24) && size >= 4 + 0x18 && (Le32(&in[pos + 4]) >> 16) == kAlias) {
            const uint8_t* alias = &in[pos + 4];
            uint32_t slot = (Le32(alias) & 0xFFFF) + 1, local = 0;
            const Entry* owner = SlotOwner(rd, slot, local);
            bool same = owner && TextureOf(rd, *owner, local, tex) && tex.size() >= 4 + 0x18 && Le32(&tex[4]) == kKxt &&
                        std::memcmp(&tex[4 + 0xC], alias + 0xC, 12) == 0;
            aliases++;
            if (same) {
                out.insert(out.end(), tex.begin(), tex.end());
            } else {
                blank++;
                size_t at = out.size();
                out.insert(out.end(), in.begin() + pos, in.begin() + pos + size);
                std::memset(&out[at + 4], 0, 4);
                LOGE("nightfire: co-op characters: %08X's alias of slot %X (%s) is drawn with the default texture",
                     e.id, slot, owner ? "not the same texture" : "not found");
            }
        } else {
            out.insert(out.end(), in.begin() + pos, in.begin() + pos + size);
        }
        pos += size;
    }
    out.insert(out.end(), in.begin() + pos, in.end());
    return true;
}

// a level co-op plays (view.cpp's IsCoopCampaign) in a session of two or more: an on-foot level's archive, not the
// front end's (0x7000048), a match's, a cutscene's (0x71000xx) or the loading screen's (0x7000500)
bool Wanted(uint32_t level) {
    return net::Active() && net::Players() > 1 && !R32(kIsMultiplayer) && !R32(kMatchMode) &&
           (level & 0xFFFFFF00) == 0x07000000 && level != 0x07000000 && level != 0x07000048;
}

void RegisterCharacters(X86Context& ctx, uint8_t* base) {
    // choice 1 is Bond, who plays as the mission's own model (view.cpp's ResolveCoopModel)
    std::vector<uint32_t> chosen;
    for (int s = 0; s < std::min(net::Players(), 4); s++) {
        int ch = net::CharacterFor(s);
        if (ch >= 1 && ch < int(kCharacters) && std::find(chosen.begin(), chosen.end(), uint32_t(ch)) == chosen.end())
            chosen.push_back(uint32_t(ch));
    }
    // the stances' records this level lacks
    std::vector<uint32_t> records;
    auto record = [&](uint32_t id) {
        if (id && !Registered(ctx, base, id) && std::find(records.begin(), records.end(), id) == records.end())
            records.push_back(id);
    };
    for (uint32_t a = kSetsFrom; a < kSetsTo; a += 4) record(R32(a));
    for (const Stances* s : {&kRecordA94C0, &kRecordA9590}) {
        for (uint32_t id : s->by_stance) record(id);
        record(s->other);
    }

    struct Want {
        uint32_t id;
        uint8_t type;
    };
    std::vector<Want> want;
    std::vector<uint32_t> models;
    if (!chosen.empty()) {
        for (uint32_t i = 0; i < 4; i++)
            if (!Registered(ctx, base, R32(kHandTextures + (4 + i) * 4))) want.push_back({kHandPacks[i], 2});
        for (uint32_t ch : chosen) {
            uint32_t model = R32(kTable + ch * 0x10), pack = R32(kTable + ch * 0x10 + 4);
            if (Registered(ctx, base, model)) continue;
            want.push_back({pack, 2});
            models.push_back(model);
        }
        if (!Registered(ctx, base, kMpHands)) want.push_back({kMpHands, 3});
        for (uint32_t m : models) want.push_back({m, 3});
    }
    if (want.empty() && records.empty()) return;

    auto start = std::chrono::steady_clock::now();
    Reader rd(ctx, base);
    if (!rd.Open()) return;
    if (!g_dir.tried) {
        g_dir.tried = true;
        g_dir.ok = ReadDirectory(rd);
    }
    if (!g_dir.ok) return;

    uint32_t animations = 0;
    for (uint32_t id : records) {
        const Entry* e = Find(id, 5);
        std::vector<uint8_t> rec(e ? e->size : 0);
        if (!e || !rd.Bytes(e->at, rec.data(), e->size)) {
            LOGE("nightfire: co-op stances: record %08X is not in %s", id, kArchive);
            continue;
        }
        want.push_back({id, 5});
        for (uint32_t o = 0; o + kRecordSize <= e->size; o += kRecordSize) {
            uint16_t low;
            std::memcpy(&low, &rec[o + 0xE], 2);
            uint32_t anim = 0x04000000u | low;
            if (!low || Registered(ctx, base, anim) || !Find(anim, 4)) continue;
            bool listed = false;
            for (const Want& w : want) listed |= w.id == anim && w.type == 4;
            if (!listed) want.push_back({anim, 4}), animations++;
        }
    }
    // in the archive's order, as a match loads them: animations before their records, packs before the models
    // whose setup (0x144C0) looks their textures up
    std::stable_sort(want.begin(), want.end(), [](const Want& a, const Want& b) {
        const Entry *x = Find(a.id, a.type), *y = Find(b.id, b.type);
        return (x ? x->at : ~0u) < (y ? y->at : ~0u);
    });

    uint32_t saved[5] = {R32(kEntryData), R32(kEntrySize), R32(kEntryEnd), R32(kEntryType), R32(kEntryId)};
    uint32_t bytes = 0, done = 0, aliases = 0, blank = 0;
    std::vector<uint8_t> pack;
    for (const Want& w : want) {
        const Entry* e = Find(w.id, w.type);
        if (!e) {
            LOGE("nightfire: co-op characters: %08X is not in %s", w.id, kArchive);
            continue;
        }
        pack.resize(e->size);
        bool read = TexturesPack(w.type) ? Rebuild(rd, *e, pack, aliases, blank) : rd.Bytes(e->at, pack.data(), e->size);
        if (!read) {
            LOGE("nightfire: co-op characters: %08X could not be read from %s", w.id, kArchive);
            break;
        }
        uint32_t size = uint32_t(pack.size());
        uint32_t data = Call(ctx, base, __imp__sub_00070AE0, {size, 0x1204u, 0x80u});
        if (!data) {
            LOGE("nightfire: co-op characters: no room in the level heap for %08X (%u bytes)", w.id, size);
            break;
        }
        std::memcpy(&kernel::G<uint8_t>(data), pack.data(), size);
        W32(kEntryData, data);
        W32(kEntrySize, size);
        W32(kEntryEnd, data + size);
        W32(kEntryType, w.type);
        W32(kEntryId, w.id);
        for (int step = 0; step < 1000 && !(Call(ctx, base, __imp__sub_000BE7B0, {}) & 0xFF); step++) {
        }
        bytes += size;
        done++;
    }
    W32(kEntryData, saved[0]), W32(kEntrySize, saved[1]), W32(kEntryEnd, saved[2]);
    W32(kEntryType, saved[3]), W32(kEntryId, saved[4]);

    for (uint32_t ch : chosen) {
        uint32_t model = R32(kTable + ch * 0x10);
        LOGR("nightfire: co-op character %u (%s) for level %08X: model %08X %s", ch + 1,
             net::CharacterChoiceName(ch + 1), g_level, model, Registered(ctx, base, model) ? "registered" : "missing");
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    LOGR("nightfire: co-op characters: %u of %zu entries (%u KB, %u texture aliases, %u left blank; %zu stance records, "
         "%u animations) from %s in %lld ms, hands %08X %s",
         done, want.size(), bytes / 1024, aliases, blank, records.size(), animations, kArchive, (long long)ms, kMpHands,
         Registered(ctx, base, kMpHands) ? "registered" : "missing");
}

// a co-op player's stance: their weapon's, when this level has its record (registered above), as a match plays it
bool CoopStance(X86Context& ctx, uint8_t* base, uint32_t actor, const Stances& s, uint32_t& pick) {
    if (!net::Active() || net::Players() < 2 || R32(kIsMultiplayer)) return false;
    uint32_t b8 = R32(actor + 0xB8);
    if (!b8) return false;
    int weapon = kernel::G<int8_t>(b8 + 0x52);
    uint8_t stance = kernel::G<uint8_t>(0x18D024 + uint32_t(weapon * 0x10C));
    pick = stance >= 1 && stance <= 6 ? s.by_stance[stance - 1] : s.other;
    return Registered(ctx, base, pick < 0x06000000 ? R32(pick) : pick);
}

// 0xA91E0, 0xA9350 (actor): the movement set by the stance into [inner + 0x820] (0x17B90: actor, set, 0.37, 0.5)
template <class Fn>
void PlaySet(X86Context& ctx, uint8_t* base, Fn original, const Stances& s) {
    uint32_t actor = R32(ctx.esp + 4), set;
    if (!CoopStance(ctx, base, actor, s, set)) return original(ctx, base);
    uint32_t played = Call(ctx, base, __imp__sub_00017B90, {actor, set, 0x3EBD70A4u, 0x3F000000u});
    LOG_FIRST(12, "nightfire: co-op stance: actor %08X plays set %06X (%08X): %08X", actor, set, R32(set), played);
    W32(R32(actor + 0xBC) + 0x820, played);
    ctx.eax = played;
    ctx.esp += 4;
}

// 0xA94C0, 0xA9590 (actor): the stance's record (0x17220: actor, record), its speed +0x94 made 4 or -4 by its sign
// against [0x15D2E8] and +0x74 given 0xD000000
template <class Fn>
void PlayRecord(X86Context& ctx, uint8_t* base, Fn original, const Stances& s) {
    uint32_t actor = R32(ctx.esp + 4), id;
    if (!CoopStance(ctx, base, actor, s, id)) return original(ctx, base);
    uint32_t played = Call(ctx, base, __imp__sub_00017220, {actor, id});
    LOG_FIRST(12, "nightfire: co-op stance: actor %08X plays record %08X: %08X", actor, id, played);
    ctx.eax = 0;
    if (played) {
        float speed, against;
        std::memcpy(&speed, &kernel::G<uint8_t>(played + 0x94), 4);
        std::memcpy(&against, &kernel::G<uint8_t>(0x15D2E8), 4);
        W32(played + 0x94, speed >= against ? 0x40800000u : 0xC0800000u);
        W32(played + 0x74, R32(played + 0x74) | 0xD000000u);
        ctx.eax = R32(played + 0x74);
    }
    ctx.esp += 4;
}
}  // namespace

X86_FUNC(sub_000A91E0) { PlaySet(ctx, base, __imp__sub_000A91E0, kSetsA91E0); }
X86_FUNC(sub_000A9350) { PlaySet(ctx, base, __imp__sub_000A9350, kSetsA9350); }
X86_FUNC(sub_000A94C0) { PlayRecord(ctx, base, __imp__sub_000A94C0, kRecordA94C0); }
X86_FUNC(sub_000A9590) { PlayRecord(ctx, base, __imp__sub_000A9590, kRecordA9590); }

// [[hook]] after 0x18F4C (config/nightfire.toml): 0x18BB0 (actor ebp, layer esi, speed, top speed, ...) plays a
// movement layer's walk by speed, then only in a match ([0x260018] in eax) the side step from the speeds' angle:
// the layer's list (+0x18, record IDs) after its count (+0x31), one to the right of it for one side, three for the
// other, into +0x14. A co-op player's layer steps sideways as in a match when this level has both records
X86_HOST_API void CoopStrafe(X86Context& ctx, uint8_t* base) {
    if (ctx.eax || !net::Active() || net::Players() < 2) return;
    bool player = false;
    for (uint32_t i = 0; i < 4; i++) player |= ctx.ebp && R32(0x1F6654 + i * 4) == ctx.ebp;
    if (!player) return;
    uint32_t list = R32(ctx.esi + 0x18), count = kernel::G<uint8_t>(ctx.esi + 0x31);
    uint32_t a = R32(list + (count + 1) * 4), b = R32(list + (count + 3) * 4);
    if (!a || !b || !Registered(ctx, base, a) || !Registered(ctx, base, b)) return;
    LOG_FIRST(4, "nightfire: co-op side steps: actor %08X, records %08X %08X", ctx.ebp, a, b);
    ctx.eax = 1;
}

// the level loader (1, level, ?, part): its state 0 opens a new archive
X86_FUNC(sub_000BE910) {
    if (R32(kLoaderState) == 0 && R32(ctx.esp + 4) == 1) {
        g_level = R32(ctx.esp + 8);
        g_registered = false;
    }
    __imp__sub_000BE910(ctx, base);
}

// the level loader's entry dispatcher: the chosen characters go in just before the level's main pack (type 1, its
// last entry), whose 0x11200 then gives every human model Bond's hardpoints
X86_FUNC(sub_000BE7B0) {
    if (!g_registered && R32(kEntryType) == 1 && kernel::G<uint8_t>(kArchiveMode)) {
        g_registered = true;
        if (Wanted(g_level)) RegisterCharacters(ctx, base);
    }
    __imp__sub_000BE7B0(ctx, base);
}

// snapshots (src/game/hooks.cpp): the archive loading and whether its characters are in
namespace nf {
void SaveCharactersState(snapshot::Writer& w) { w.Pod(g_level), w.Pod(g_registered); }
void LoadCharactersState(snapshot::Reader& r) { r.Pod(g_level), r.Pod(g_registered); }
}  // namespace nf
