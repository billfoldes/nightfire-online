// hide_hud on foot: the HUD is each player view's 2D sprite list (the cameras at 0x1F661C): the ammo and weapon, the
// health bar, the timer, scores, name tags and hints. The frame's 2D pass (0xDAC20) draws each view's list through
// 0xD9410 (cdecl: the view, the list, first and end index), sprites itself and text through 0x6A340. The menus and
// the pause screen are widgets drawn apart; while one is up (MenuUp) the lists are drawn as the game has them.
//   In an online session with full-screen view on (net::FullScreenView), remote players' HUD passes run with host GPU
// draws suppressed so only the local player's HUD is drawn.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "common/net.h"
#include "common/nightfire.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace d3d {
bool SuppressDraws(bool on);  // sdk/runtime/xdk/d3d8.cpp
}

namespace net = nf::net;

namespace nf {
int CameraOwner(uint32_t cam, int* active_count = nullptr);  // src/game/view.cpp
bool DrawWithoutOthersPauses(X86Context& ctx, uint8_t* base, void (*draw)(X86Context&, uint8_t*));  // pause.cpp
}

// crosshair_color / crosshair_size: the crosshair is the list's centred picture that is not a radar blip (a sprite of
// 0x44 bytes: +0x10 its colour, alpha blue green red; +0x18 a text's, else 0; +0x22 flags, 0x800 centred on
// +0x24/+0x26, 0xD9630; +0x2C/+0x2E its width and height, 11 by 11). Only the draw sees them, so every machine's game
// state stays the game's
namespace {
constexpr uint16_t kCentred = 0x800;
constexpr uint16_t kRadarBlip = 0x100;  // the radar's blips are centred too (4 by 4, flags 0x900)
constexpr int kCrosshairMin = 6, kCrosshairMax = 32;  // other sizes of centred picture are not a crosshair

struct CrosshairLook {
    uint32_t sprite = 0, color = 0, size = 0;  // the crosshair's colour and size words as the game had them

    explicit CrosshairLook(X86Context& ctx) {
        uint32_t pct = std::clamp<uint32_t>(nf::crosshair_size, 25, 400);
        if (nf::crosshair_color == 0 && pct == 100) return;
        uint32_t list = kernel::R32(ctx.esp + 8), first = kernel::R32(ctx.esp + 12), end = kernel::R32(ctx.esp + 16);
        for (uint32_t i = first; i < end; i++) {
            uint32_t s = kernel::R32(list + i * 4);
            uint16_t flags = s ? kernel::G<uint16_t>(s + 0x22) : 0;
            if ((flags & (kCentred | kRadarBlip)) != kCentred || kernel::R32(s + 0x18)) continue;
            int16_t w = kernel::G<int16_t>(s + 0x2C), h = kernel::G<int16_t>(s + 0x2E);
            if (w < kCrosshairMin || h < kCrosshairMin || w > kCrosshairMax || h > kCrosshairMax) continue;
            sprite = s;
            color = kernel::R32(s + 0x10);
            size = kernel::R32(s + 0x2C);
            if (nf::crosshair_color) {
                uint32_t rgb = nf::CrosshairRgb(nf::crosshair_color);
                kernel::W32(s + 0x10, (color & 0xFF) | (rgb & 0xFF) << 8 | (rgb >> 8 & 0xFF) << 16 | (rgb >> 16) << 24);
            }
            if (pct != 100) {
                kernel::G<int16_t>(s + 0x2C) = int16_t(std::max<uint32_t>(1, (uint32_t(w) * pct + 50) / 100));
                kernel::G<int16_t>(s + 0x2E) = int16_t(std::max<uint32_t>(1, (uint32_t(h) * pct + 50) / 100));
            }
            static uint32_t said_color = 0, said_size = 100;
            if (said_color != nf::crosshair_color || said_size != pct) {
                said_color = nf::crosshair_color;
                said_size = pct;
                LOGR("nightfire: the crosshair is %s at %u%% (crosshair_color, crosshair_size)",
                     nf::CrosshairColorName(said_color), pct);
            }
            return;
        }
    }
    ~CrosshairLook() {
        if (!sprite) return;
        kernel::W32(sprite + 0x10, color);
        kernel::W32(sprite + 0x2C, size);
    }
};
}  // namespace

X86_EXTERN_FUNC(__imp__sub_000D9410);
X86_FUNC(sub_000D9410) {
    if (nf::hide_hud && !nf::MenuUp()) {
        ctx.esp += 4;  // cdecl: the return address only
        return;
    }
    if (net::FullScreenView() && !nf::SharedMenuUp()) {  // a player's own pause is their view's list
        int active = 0;
        int player = nf::CameraOwner(kernel::R32(ctx.esp + 4), &active);
        if (active > 1 && player >= 0 && player != net::LocalPlayer()) {
            bool was = d3d::SuppressDraws(true);
            __imp__sub_000D9410(ctx, base);
            d3d::SuppressDraws(was);
            return;
        }
    }
    CrosshairLook look(ctx);
    if (nf::DrawWithoutOthersPauses(ctx, base, __imp__sub_000D9410)) return;
    __imp__sub_000D9410(ctx, base);
}

// net_default_names: a text sprite showing a player's name points at it ([sprite+0x18], 0xCEBB0, its length at +0x20),
// the combatants' names at 0x25FE38 (0x30 apart). 0x6A340 (from 0xD950D, the sprite in ebp; the string at
// [esp+0x18], its length at [esp+0x28]) draws it with Player N in its place, and the name and lengths are put back: the game's names stay every machine's. Only the frame's
// queued glyphs (drawn later, from 0x29E810) and the text's own state (0x1F64B0..0x1F6564) are this machine's
namespace {
constexpr uint32_t kFromSpriteList = 0xD9512;
constexpr uint32_t kNames = 0x25FE38, kNameSize = 0x30;
}

X86_EXTERN_FUNC(__imp__sub_0006A340);
X86_FUNC(sub_0006A340) {
    uint32_t sprite = ctx.ebp, off = 0;
    int s = -1;
    if (net::default_names && net::Active() && kernel::R32(ctx.esp) == kFromSpriteList) {
        off = kernel::R32(sprite + 0x18) - kNames;
        if (off < uint32_t(net::kMaxPlayers) * kNameSize && off % kNameSize == 0) s = int(off / kNameSize);
    }
    if (s < 0 || !net::PlayerName(s)[0]) {
        __imp__sub_0006A340(ctx, base);
        return;
    }
    char* name = &kernel::G<char>(kNames + off);
    uint16_t& len = kernel::G<uint16_t>(sprite + 0x20);
    uint32_t len_arg = ctx.esp + 0x28;  // 0xD9410 passes the length too
    char name_was[32];
    std::memcpy(name_was, name, sizeof name_was);
    uint16_t len_was = len;
    uint32_t len_arg_was = kernel::R32(len_arg);
    std::snprintf(name, 31, "Player %d", s + 1);
    len = uint16_t(std::strlen(name));
    kernel::W32(len_arg, len);
    __imp__sub_0006A340(ctx, base);
    std::memcpy(name, name_was, sizeof name_was);
    len = len_was;
    kernel::W32(len_arg, len_arg_was);
}
