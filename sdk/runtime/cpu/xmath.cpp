// The x87's and the CRT's transcendental functions, the same bits on every PC: musl's (third_party/musl_math, MIT),
// built here. A C library's sin or pow is not exact, and glibc's and Windows' differ in the last bit for a few
// percent of arguments, so a game in lockstep between a Linux and a Windows PC drifted apart. These are plain
// double arithmetic (x86-64-v2: no FMA, as every build compiles), so they round alike on every OS and compiler.
// XBE_HOST_MATH=1 uses the C library's instead (a test: two machines that differ as two OSes do)
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <cstdlib>

#include "core/log.h"
#include "cpu/x86_ops.h"

#if defined(__FP_FAST_FMA) && __FP_FAST_FMA
#error xmath.cpp must build without FMA: musl's other paths round differently
#endif

namespace xmath_musl {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../../third_party/musl_math/libm.h"
// one translation unit: a file's static names another file has too are renamed, and its macros undefined after it
#include "../../third_party/musl_math/__math_divzero.c"
#include "../../third_party/musl_math/__math_invalid.c"
#include "../../third_party/musl_math/__math_oflow.c"
#include "../../third_party/musl_math/__math_uflow.c"
#include "../../third_party/musl_math/__math_xflow.c"
#include "../../third_party/musl_math/__rem_pio2_large.c"
#include "../../third_party/musl_math/__rem_pio2.c"
#undef EPS
#include "../../third_party/musl_math/__sin.c"
#include "../../third_party/musl_math/__cos.c"
#define pio4 pio4_tan
#include "../../third_party/musl_math/__tan.c"
#undef pio4
#include "../../third_party/musl_math/sin.c"
#include "../../third_party/musl_math/cos.c"
#include "../../third_party/musl_math/tan.c"
#include "../../third_party/musl_math/asin.c"
#define pio2_hi pio2_hi_acos
#define pio2_lo pio2_lo_acos
#define pS0 pS0_acos
#define pS1 pS1_acos
#define pS2 pS2_acos
#define pS3 pS3_acos
#define pS4 pS4_acos
#define pS5 pS5_acos
#define qS1 qS1_acos
#define qS2 qS2_acos
#define qS3 qS3_acos
#define qS4 qS4_acos
#define R R_acos
#include "../../third_party/musl_math/acos.c"
#undef pio2_hi
#undef pio2_lo
#undef pS0
#undef pS1
#undef pS2
#undef pS3
#undef pS4
#undef pS5
#undef qS1
#undef qS2
#undef qS3
#undef qS4
#undef R
#include "../../third_party/musl_math/atan.c"
#include "../../third_party/musl_math/atan2.c"
#include "../../third_party/musl_math/exp_data.c"
#undef N
#include "../../third_party/musl_math/exp2.c"
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef N
#undef Shift
#undef T
#include "../../third_party/musl_math/log2_data.c"
#undef N
#include "../../third_party/musl_math/log2.c"
#undef A
#undef B
#undef HI
#undef InvLn2hi
#undef InvLn2lo
#undef LO
#undef N
#undef OFF
#undef T
#undef T2
#include "../../third_party/musl_math/log_data.c"
#undef N
#define top16 top16_log
#include "../../third_party/musl_math/log.c"
#undef top16
#undef A
#undef B
#undef HI
#undef Ln2hi
#undef Ln2lo
#undef LO
#undef N
#undef OFF
#undef T
#undef T2
#include "../../third_party/musl_math/log10.c"
#include "../../third_party/musl_math/pow_data.c"
#undef A
#undef N
#define specialcase specialcase_pow
#define top12 top12_pow
#include "../../third_party/musl_math/pow.c"
#undef specialcase
#undef top12
#undef A
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef InvLn2N
#undef Ln2hi
#undef Ln2lo
#undef N
#undef NegLn2hiN
#undef NegLn2loN
#undef OFF
#undef Shift
#undef SIGN_BIAS
#undef T
#pragma clang diagnostic pop
}  // namespace xmath_musl

namespace {
bool HostMath() {
    static const bool host = [] {
        const char* e = std::getenv("XBE_HOST_MATH");
        bool on = e && *e == '1';
        if (on) LOGR("cpu: XBE_HOST_MATH: the C library's sin, atan2, pow and the rest (a test; another OS's differ)");
        return on;
    }();
    return host;
}
}  // namespace

#define X86_MATH1(name)                                                    \
    extern "C" X86_HOST_EXPORT double x86_##name(double v) {               \
        return HostMath() ? ::name(v) : xmath_musl::name(v);               \
    }
#define X86_MATH2(name)                                                    \
    extern "C" X86_HOST_EXPORT double x86_##name(double a, double b) {     \
        return HostMath() ? ::name(a, b) : xmath_musl::name(a, b);         \
    }
X86_MATH1(sin)
X86_MATH1(cos)
X86_MATH1(tan)
X86_MATH1(asin)
X86_MATH1(acos)
X86_MATH1(atan)
X86_MATH1(exp2)
X86_MATH1(log2)
X86_MATH1(log)
X86_MATH1(log10)
X86_MATH2(atan2)
X86_MATH2(pow)
#undef X86_MATH1
#undef X86_MATH2
