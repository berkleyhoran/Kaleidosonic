#version 330 core

// Shared uniform contract for every preset fragment shader. PresetManager
// prepends this file's source to each preset's .frag body before compiling,
// so individual presets only need to implement main() using these.

in vec2 vUv;
out vec4 fragColor;

uniform vec2  uResolution;
uniform float uTime;             // seconds, wraps every ~1000s to keep float precision sane

// --- audio-reactive inputs, all driven from AudioAnalyzer, 0..1-ish ---
uniform float uBass;
uniform float uMid;
uniform float uTreble;
uniform float uLevel;            // overall RMS level
uniform float uOnset;            // transient/beat pulse, decays after each hit

// --- automatable visual parameters (APVTS) ---
uniform float uReactivity;       // 0..2 global multiplier on all audio inputs above
uniform float uZoomSpeed;        // -1..1
uniform float uRotationSpeed;    // -1..1
uniform float uHue;              // 0..1
uniform float uSaturation;       // 0..1
uniform float uBrightness;       // 0..2
uniform float uContrast;         // 0..2
uniform float uKaleidoscopeSegments; // 1..16
uniform float uFeedback;         // 0..1, only used by feedback-style presets
uniform float uIterations;       // 4..64, fractal detail
uniform float uDistortion;       // 0..1
uniform float uZoomWander;       // 0..2, how far fractal zoom targets wander while diving
uniform float uCameraShake;      // 0..2, how hard onsets kick the camera/zoom
uniform float uCameraScale;      // 0.2..6, manual zoom-out multiplier on top of everything else

// CPU-computed (real double precision) autopilot target for Mandelbrot
// Pulse / Burning Ship -- see FractalNavigator.h. re/im are double-float
// (hi, lo) pairs; radius is safe as plain float (it's just a scale, never
// added to something much larger). fade eases 0->1 after the navigator
// loops back to its start, so the reset reads as a gentle dip instead of
// a jump cut.
uniform vec2  uFractalRe;
uniform vec2  uFractalIm;
uniform float uFractalRadius;
uniform float uFractalFade;

uniform sampler2D uPrevFrame;    // previous rendered frame, for feedback presets
uniform sampler2D uWaveform;     // 2048x1 R32F texture of recent mono samples, oldest at x=0

vec3 hsv2rgb(vec3 c)
{
    vec4 k = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + k.xyz) * 6.0 - k.www);
    return c.z * mix(k.xxx, clamp(p - k.xxx, 0.0, 1.0), c.y);
}

// Folds an angle into N radial kaleidoscope segments.
vec2 kaleidoscope(vec2 uv, float segments)
{
    float angle = atan(uv.y, uv.x);
    float radius = length(uv);
    float segAngle = 6.28318530718 / max(segments, 1.0);
    angle = mod(angle, segAngle);
    angle = abs(angle - segAngle * 0.5);
    return vec2(cos(angle), sin(angle)) * radius;
}

mat2 rotate2d(float a)
{
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

// Applies contrast/brightness/saturation grading driven by the automatable params.
vec3 grade(vec3 col)
{
    col = (col - 0.5) * uContrast + 0.5;
    col *= uBrightness;
    float g = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(g), col, uSaturation);
    return clamp(col, 0.0, 1.0);
}

// Continuous, monotonically-deepening zoom cycle for fractal presets, so
// zooming reads as diving forever instead of breathing in and out. GPU
// float32 precision limits how deep a fractal zoom can go before it turns
// to blocky mush, so this loops every `cycleLength` e-folds rather than
// truly zooming to infinity -- speed accelerates with bass/onset so the
// dive visibly surges with the music. Returns (depth, brightnessMask); the
// mask dips to a brief dark flash right at the reset cut so it reads as a
// deliberate beat-like pulse instead of a visible pop.
vec2 zoomCycle(float cycleLength, float speedBase)
{
    float speed = speedBase * (0.35 + 1.4 * max(uZoomSpeed, 0.0))
                * (1.0 + uBass * uReactivity * 1.8 + uOnset * uReactivity * 1.4);
    float cyclePos = fract(uTime * speed / cycleLength);
    float depth = cyclePos * cycleLength;
    float mask = smoothstep(0.0, 0.05, cyclePos) * (1.0 - smoothstep(0.95, 1.0, cyclePos));
    return vec2(depth, mask);
}

// ---------------------------------------------------------------------
// Double-float ("double-single") arithmetic: represents a number as a
// (hi, lo) pair of float32s so hi+lo carries ~45 bits of precision
// instead of float32's ~23. This is what lets Mandelbrot Pulse and
// Burning Ship zoom genuinely deep before the classic GPU-fractal "turns
// to blocky mush" precision wall shows up (that wall sits around
// 1e12-1e13x here), instead of the ~1e6x ceiling plain float32 hits. The
// CPU-side navigator (FractalNavigator.cpp) deliberately loops back well
// before reaching that wall -- around 1e11x -- so the reset always
// happens while detail is still crisp instead of after it's already
// turned to mush. Standard Dekker/Knuth-style compensated arithmetic, as
// used in most real-time deep-zoom fractal shaders.
vec2 dsSet(float a) { return vec2(a, 0.0); }

vec2 dsAdd(vec2 dsa, vec2 dsb)
{
    float t1 = dsa.x + dsb.x;
    float e = t1 - dsa.x;
    float t2 = ((dsb.x - e) + (dsa.x - (t1 - e))) + dsa.y + dsb.y;
    float hi = t1 + t2;
    float lo = t2 - (hi - t1);
    return vec2(hi, lo);
}

vec2 dsNeg(vec2 a) { return vec2(-a.x, -a.y); }
vec2 dsSub(vec2 a, vec2 b) { return dsAdd(a, dsNeg(b)); }

vec2 dsMul(vec2 dsa, vec2 dsb)
{
    const float split = 4097.0; // 2^12 + 1
    float cona = dsa.x * split;
    float conb = dsb.x * split;
    float a1 = cona - (cona - dsa.x);
    float b1 = conb - (conb - dsb.x);
    float a2 = dsa.x - a1;
    float b2 = dsb.x - b1;

    float c11 = dsa.x * dsb.x;
    float c21 = a2 * b2 - (((c11 - a1 * b1) - a2 * b1) - a1 * b2);

    float c2 = dsa.x * dsb.y + dsa.y * dsb.x;

    float t1 = c11 + c2;
    float e = t1 - c11;
    float t2 = dsa.y * dsb.y + ((c2 - e) + (c11 - (t1 - e))) + c21;

    float hi = t1 + t2;
    float lo = t2 - (hi - t1);
    return vec2(hi, lo);
}

// A complex number as a pair of double-floats.
struct DComplex { vec2 re; vec2 im; };

DComplex dcAdd(DComplex a, DComplex b) { return DComplex(dsAdd(a.re, b.re), dsAdd(a.im, b.im)); }

DComplex dcSq(DComplex a)
{
    // (re + i*im)^2 = (re^2 - im^2) + i*(2*re*im)
    vec2 re2 = dsMul(a.re, a.re);
    vec2 im2 = dsMul(a.im, a.im);
    vec2 reim = dsMul(a.re, a.im);
    return DComplex(dsSub(re2, im2), dsAdd(reim, reim));
}
