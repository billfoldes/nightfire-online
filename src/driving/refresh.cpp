// Driving at 60 frames a second on the PAL disc: Driving.xbe's rate is 0x117DA0, the refresh of the video standard
// [0x243540] from the table 0x1A204C (NTSC-M 60, NTSC-J 60, PAL-I 50, PAL-M 60). The game's frame timer takes it
// (0x10AE50 from 0x59920) and its steps are 1 / it (0x95380, 0xB49F0, 0x5CEB0), so on PAL the whole of the driving
// half ran at 50 frames a second while the screen shows 60. The USA disc runs the same code at 60. For the call
// alone the standard reads as NTSC-M, and NTSC-M's rate is nf::GameRate()'s (60 unless the game frame rate is set
// higher, as Driving.xbe boots: its frame timer is made once then, so a rate changed later plays at the next mission;
// 60 in a session); everything else (the PAL movies and files) still sees PAL.
#include <bit>
#include <cstdint>

#include "app/app.h"
#include "common/net.h"
#include "common/nightfire.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

X86_EXTERN_FUNC(__imp__sub_00117DA0);
X86_FUNC(sub_00117DA0) {
    constexpr uint32_t kStandard = 0x243540, kRates = 0x1A204C, kPal = 2, kNtsc = 0;
    static const uint32_t booted = nf::GameRate();  // the frame timer's, as Driving.xbe boots
    uint32_t standard = kernel::R32(kStandard), rate = nf::net::Active() ? 60 : booted;
    static bool said = false;
    if (!said && standard == kPal) {
        said = true;
        LOGR("nightfire: the driving levels at %u frames a second (the PAL disc's 50 Hz rate read as NTSC's)", rate);
    }
    app::g_pace_hz = rate;
    uint32_t ntsc = kernel::R32(kRates);
    kernel::W32(kStandard, kNtsc);
    kernel::W32(kRates, std::bit_cast<uint32_t>(float(rate)));
    __imp__sub_00117DA0(ctx, base);
    kernel::W32(kRates, ntsc);
    kernel::W32(kStandard, standard);
}

// The frame timer (0x10AE50) counts every 1000 / rate whole milliseconds (0x10EED3, the timers' table [0x24B21C], 0x44
// bytes an entry, its period at +0x3C in 100 ns units, negative): 16 ms at 60 (62.5 a second, as the Xbox counted),
// 6 ms at 144 (167 a second, the game 16% fast). Above 60 the period is 1 / rate exactly
X86_EXTERN_FUNC(__imp__sub_0010AE50);
X86_FUNC(sub_0010AE50) {
    constexpr uint32_t kTimerId = 0x2422D4, kTimers = 0x24B21C;
    uint32_t rate = kernel::R32(ctx.esp + 4);
    __imp__sub_0010AE50(ctx, base);
    uint32_t id = kernel::R32(kTimerId), table = kernel::R32(kTimers);
    if (rate <= 60 || !table || (id & 0xFFFF) == 0) return;
    uint32_t entry = table + ((id & 0xFFFF) - 1) * 0x44 + 0x3C;
    int64_t period = -int64_t(10000000 / rate);
    kernel::W32(entry, uint32_t(uint64_t(period))), kernel::W32(entry + 4, uint32_t(uint64_t(period) >> 32));
    LOGR("nightfire: the driving frame timer every %.3f ms (%u a second)", 1000.0 / rate, rate);
}
