#pragma once

#include <cmath>

// "Double-double" (DD) arithmetic: represents a real number as an
// unevaluated sum of two ordinary hardware `double`s (hi + lo), carrying
// roughly 106 bits (~32 decimal digits) of precision -- about twice the
// mantissa of a plain double (53 bits, ~15-16 digits). This is the CPU-side
// counterpart to the GPU's float-pair "double-float" used elsewhere in this
// codebase (see dsAdd/dsMul in common.glsl), just built from `double`
// instead of `float`, and using std::fma for an exact two-product instead
// of manual splitting (fma is exact per IEEE 754 and available on both
// MSVC and Clang/GCC).
//
// This exists to compute a single, per-lock "reference orbit" for the
// Mandelbrot/Burning Ship fractal navigator to genuinely deep precision on
// the CPU -- see FractalNavigator.cpp -- which is what lets the GPU render
// tiny per-pixel offsets from that orbit (perturbation theory) instead of
// needing that same extreme precision per pixel.
struct DD
{
    double hi = 0.0;
    double lo = 0.0;
};

inline DD ddFromDouble(double v) noexcept
{
    return { v, 0.0 };
}

// Knuth's TwoSum: exact for any a, b (no magnitude ordering assumption).
inline double ddTwoSum(double a, double b, double& err) noexcept
{
    const double s = a + b;
    const double bb = s - a;
    err = (a - (s - bb)) + (b - bb);
    return s;
}

inline DD ddAdd(DD a, DD b) noexcept
{
    double e = 0.0;
    double s = ddTwoSum(a.hi, b.hi, e);
    e += a.lo + b.lo;
    const double hi = s + e;
    const double lo = e - (hi - s);
    return { hi, lo };
}

inline DD ddNeg(DD a) noexcept
{
    return { -a.hi, -a.lo };
}

inline DD ddSub(DD a, DD b) noexcept
{
    return ddAdd(a, ddNeg(b));
}

// Exact two-product via fma: p = a*b rounded, e = the rounding error, so
// p + e == a*b exactly (in infinite precision).
inline DD ddMul(DD a, DD b) noexcept
{
    const double p = a.hi * b.hi;
    double e = std::fma(a.hi, b.hi, -p);
    e += a.hi * b.lo + a.lo * b.hi;
    const double hi = p + e;
    const double lo = e - (hi - p);
    return { hi, lo };
}

inline DD ddAbs(DD a) noexcept
{
    return a.hi < 0.0 ? ddNeg(a) : a;
}

// Splits a DD down to a (hi, lo) float32 pair -- the GPU double-float
// representation used by dsAdd/dsMul in common.glsl. Reference orbit values
// themselves are always ordinary magnitude (bounded by the escape bailout),
// so this loses nothing that matters: only the *reference coordinate* that
// generated the orbit needs the full ~32-digit DD precision, carried
// through the whole CPU-side iteration -- each resulting orbit point is
// fine to hand off at double-float (~13-14 digit) precision for rendering.
inline void ddToFloatPair(DD v, float& hi, float& lo) noexcept
{
    hi = (float) v.hi;
    const double remainder = (v.hi - (double) hi) + v.lo;
    lo = (float) remainder;
}
