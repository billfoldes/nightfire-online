// musl's src/internal/libm.h (MIT, COPYRIGHT here), cut to what these files use and made C++: the word access
// is memcpy, not C's compound literals. Included inside sdk/runtime/cpu/xmath.cpp's namespace, after the system
// headers; x86-64 only (double_t is double, little endian, no FMA: the files' !__FP_FAST_FMA paths)
#ifndef _LIBM_H
#define _LIBM_H

#define hidden
#define WANT_ROUNDING 1
#define WANT_SNAN 0
#define issignalingf_inline(x) 0
#define issignaling_inline(x) 0
#define TOINT_INTRINSICS 0

#define predict_true(x) __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)

static inline float eval_as_float(float x) { return x; }
static inline double eval_as_double(double x) { return x; }

static inline float fp_barrierf(float x) {
    volatile float y = x;
    return y;
}
static inline double fp_barrier(double x) {
    volatile double y = x;
    return y;
}
static inline void fp_force_evalf(float x) {
    volatile float y;
    y = x;
}
static inline void fp_force_eval(double x) {
    volatile double y;
    y = x;
}
#define FORCE_EVAL(x)                                    \
    do {                                                 \
        if (sizeof(x) == sizeof(float)) fp_force_evalf(x); \
        else fp_force_eval(x);                           \
    } while (0)

static inline uint32_t asuint(float f) {
    uint32_t i;
    memcpy(&i, &f, 4);
    return i;
}
static inline float asfloat(uint32_t i) {
    float f;
    memcpy(&f, &i, 4);
    return f;
}
static inline uint64_t asuint64(double f) {
    uint64_t i;
    memcpy(&i, &f, 8);
    return i;
}
static inline double asdouble(uint64_t i) {
    double f;
    memcpy(&f, &i, 8);
    return f;
}

#define EXTRACT_WORDS(hi, lo, d)      \
    do {                              \
        uint64_t __u = asuint64(d);   \
        (hi) = __u >> 32;             \
        (lo) = (uint32_t)__u;         \
    } while (0)
#define GET_HIGH_WORD(hi, d) \
    do {                     \
        (hi) = asuint64(d) >> 32; \
    } while (0)
#define GET_LOW_WORD(lo, d)           \
    do {                              \
        (lo) = (uint32_t)asuint64(d); \
    } while (0)
#define INSERT_WORDS(d, hi, lo)                                \
    do {                                                       \
        (d) = asdouble(((uint64_t)(hi) << 32) | (uint32_t)(lo)); \
    } while (0)
#define SET_HIGH_WORD(d, hi) INSERT_WORDS(d, hi, (uint32_t)asuint64(d))
#define SET_LOW_WORD(d, lo) INSERT_WORDS(d, asuint64(d) >> 32, lo)

int __rem_pio2_large(double*, double*, int, int, int);
int __rem_pio2(double, double*);
double __sin(double, double, int);
double __cos(double, double);
double __tan(double, double, int);
double __math_xflow(uint32_t, double);
double __math_uflow(uint32_t);
double __math_oflow(uint32_t);
double __math_divzero(uint32_t);
double __math_invalid(double);

#endif
