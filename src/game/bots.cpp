// The bots of an online match as the host's bot settings have them (net::BotSetting, the same on every machine).
//
// The game's bot menu (page 0x4000002C, Multiplayer Bot Setup: handler 0x8ADE0, its changes 0x8AB70) keeps six
// entries of 0x12 bytes at 0x245282: the statistics +0x00..+0x0D (accuracy +0x00, aggression +0x02, health +0x04,
// move speed +0x06, reaction +0x07, recovery +0x08, the character's side +0x09, personality +0x0B), playing +0x0E,
// team +0x0F, character +0x10, statistics edited +0x11. A character picked there (0x8AB70's 0x4B) sets the team to its
// side (0x7EAE0: its record's +0x09 is 0) and the bot's name (slot 4 + the bot's in 0x25FE38) to the character's
// (0x17C8E0's record for it, the string 0x6D460 gives its handle), and the page's end gives an entry whose
// statistics were not edited its character's own (the 14 bytes at 0x163628 + character * 14, 0x1A250).
//   The session's characters go into those entries as the menu would put them, at points every machine reaches at
// the same tick: each match's start (a new round of Random picks), and the frame (0xDD1D0) once the settings
// changed (a round of the host's bundles later, so they arrive whole, net::BotSettingsWhole) or the menu got its
// default characters (0x88EA0's 0x51, once: 0x25E4D7). The host's own edits there
// (the character list, 0x8AB70's) come back to its settings (BotsMenuEdited, each frame), so the next ones it sends
// say the same. Bots 7 and 8 (below slot 4, src/game/view.cpp) are past the menu: their entry is built at the spawn
// (BotsEntry) from the first two entries, the character and the fields their settings give. An entry the menu never
// set up (the bots past its Playing ones, net_bots) has no statistics: its character's are played.
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace nf {
namespace {
using kernel::R32;

constexpr uint32_t kMenuBots = 0x245282, kEntrySize = 0x12, kMenuSlots = 6;
constexpr uint32_t kCharacterStats = 0x163628, kStatsSize = 14;  // 0x1A250: a character's statistics
constexpr uint32_t kCharacterNames = 0x17C8E0, kCharacterRecords = 0x1D;  // 0x18 bytes: name's handle +4, id +0xC
constexpr uint32_t kCombatantNames = 0x25FE38, kCombatantSize = 0x30;
constexpr uint32_t kCharacters = 29, kFixedStats = 15;  // the characters from 15 on have their statistics fixed

uint8_t& B(uint32_t va) { return kernel::G<uint8_t>(va); }

// the session's character for bot 0..7, -1 Default (the menu's; for Bots 7 and 8 the first ones' again)
int Character(int bot) {
    uint32_t c = uint32_t(net::BotSetting(net::kBotEvery));
    if (!c) c = uint32_t(net::BotSetting(net::kBotChar + bot));
    if (c == net::kBotRandom) {
        uint64_t x = (uint64_t(net::BotSeed()) << 32 | net::BotRound()) * 0x9E3779B97F4A7C15ull + uint64_t(bot) * 0xBF58476D1CE4E5B9ull;
        x ^= x >> 31, x *= 0x94D049BB133111EBull, x ^= x >> 29;
        return int(x % kCharacters);
    }
    return c >= 1 && c <= kCharacters ? int(c) - 1 : -1;
}

void Name(X86Context& ctx, uint32_t c, uint32_t dst) {
    for (uint32_t i = 0; i < kCharacterRecords; i++) {
        uint32_t rec = kCharacterNames + i * 0x18;
        if (R32(rec + 0xC) != c) continue;
        uint32_t s = kernel::CallGuest(ctx, 0x6D460, {R32(rec + 4), 0u});
        if (s) {
            std::strncpy(reinterpret_cast<char*>(&B(dst)), reinterpret_cast<const char*>(&B(s)), 31);
            B(dst + 31) = 0;
        }
        return;
    }
}

// entry gets character c as the menu's character box gives it (0x8AB70), and its statistics unless they were edited
void SetCharacter(uint32_t entry, uint32_t c, bool stats) {
    B(entry + 0x10) = uint8_t(c);
    B(entry + 0x0F) = B(kCharacterStats + c * kStatsSize + 9) == 0;
    if (stats) std::memmove(&B(entry), &B(kCharacterStats + c * kStatsSize), kStatsSize);
}

uint8_t g_seen[kMenuSlots];  // the host's: the menu's characters as last written or looked at
bool g_seen_valid = false;

void Look() {
    for (uint32_t i = 0; i < kMenuSlots; i++) g_seen[i] = B(kMenuBots + i * kEntrySize + 0x10);
    g_seen_valid = true;
}

void ToMenu(X86Context& ctx, const char* when) {
    int written = 0;
    for (uint32_t i = 0; i < kMenuSlots; i++) {
        int c = Character(int(i));
        uint32_t entry = kMenuBots + i * kEntrySize;
        if (c < 0 || B(entry + 0x10) == c) continue;
        SetCharacter(entry, uint32_t(c), B(entry + 0x11) == 0);
        Name(ctx, uint32_t(c), kCombatantNames + (4 + i) * kCombatantSize);
        written++;
    }
    net::BotMenuChanges() = net::BotChanges();
    Look();
    if (written) LOGI("nightfire: %s: %d of the bot menu's characters from the host's bot settings", when, written);
}

// the host: an edit on the bot page made the menu's characters other than its settings say
void BotsMenuEdited() {
    if (!net::Active() || net::LocalPlayer() != 0) return;
    if (!g_seen_valid) return Look();
    uint8_t now[kMenuSlots];
    bool changed = false;
    for (uint32_t i = 0; i < kMenuSlots; i++) {
        now[i] = B(kMenuBots + i * kEntrySize + 0x10);
        changed |= now[i] != g_seen[i];
    }
    if (!changed) return;
    uint32_t every = net::bot_setting[net::kBotEvery];
    bool same = every && every != net::kBotRandom;
    for (uint32_t i = 0; i < kMenuSlots && same; i++) same = now[i] == every - 1;
    if (every && !same) {  // the bots differ now: each its own, the ones past the menu as they were
        for (int b = 6; b < 8; b++) {
            int c = every == net::kBotRandom ? Character(b) : int(every) - 1;
            net::SetHostBotSetting(net::kBotChar + b, uint32_t(c) + 1);
        }
        for (uint32_t i = 0; i < kMenuSlots; i++) net::SetHostBotSetting(net::kBotChar + int(i), now[i] + 1u);
        net::SetHostBotSetting(net::kBotEvery, 0);
        LOGR("nightfire: the bot menu's characters differ now: Every bot's character is Default, each bot has its own");
    } else if (!every) {
        for (uint32_t i = 0; i < kMenuSlots; i++)
            if (now[i] != g_seen[i] && now[i] < kCharacters) {
                net::SetHostBotSetting(net::kBotChar + int(i), now[i] + 1u);
                LOGR("nightfire: the bot menu's Bot %u is character %u now (Bot %u character)", i + 1, now[i] + 1u, i + 1);
            }
    }
    std::memcpy(g_seen, now, kMenuSlots);
}

}  // namespace

void BotsFrame(X86Context& ctx) {
    if (!net::Active()) return;
    static uint8_t defaults = 0;  // 0x88EA0's 0x51 gives the menu its default characters once (0x25E4D7)
    if ((net::BotMenuChanges() != net::BotChanges() && net::BotSettingsWhole()) || B(0x25E4D7) != defaults) {
        defaults = B(0x25E4D7);
        ToMenu(ctx, "the host's bot settings");
    }
    BotsMenuEdited();
}

void BotsMatchStart(X86Context& ctx) {
    net::BotRound()++;
    ToMenu(ctx, "a match");
}

bool BotsName(X86Context& ctx, int bot, uint32_t dst) {
    int c = Character(bot);
    if (c < 0) return false;
    Name(ctx, uint32_t(c), dst);
    return true;
}

void BotsEntry(int bot, uint32_t entry) {
    if (kernel::G<uint16_t>(entry + 4) == 0 && B(entry + 0x10) < kCharacters)  // never set up in the menu (no health)
        SetCharacter(entry, B(entry + 0x10), true);
    if (bot >= int(kMenuSlots)) {
        if (int c = Character(bot); c >= 0) SetCharacter(entry, uint32_t(c), true);
        uint32_t c = B(entry + 0x10);
        int k = bot - int(kMenuSlots);
        for (uint32_t f = 0; f < net::kBotFieldCount && c < kFixedStats; f++) {
            uint32_t v = uint32_t(net::BotSetting(net::kBotField + k * int(net::kBotFieldCount) + int(f)));
            const net::BotField& field = net::kBotFields[f];
            if (!v || v > field.count) continue;
            if (field.size == 2) kernel::G<uint16_t>(entry + field.offset) = field.values[v - 1];
            else B(entry + field.offset) = uint8_t(field.values[v - 1]);
        }
    } else if (int c = Character(bot); c >= 0 && B(entry + 0x10) != c) {
        SetCharacter(entry, uint32_t(c), true);
    }
    if (net::BotSetting(net::kBotSkill) && bot > 0 && B(entry + 0x10) < kFixedStats) {  // Bot 1's statistics
        for (uint32_t f = 0; f < net::kBotFieldCount; f++) {
            const net::BotField& field = net::kBotFields[f];
            if (field.offset == 0x0B && B(entry + 9) != B(kMenuBots + 9)) continue;  // another side's personalities
            std::memmove(&B(entry + field.offset), &B(kMenuBots + field.offset), field.size);
        }
    }
}

}  // namespace nf
