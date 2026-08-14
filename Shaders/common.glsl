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
uniform float uPalette;          // 0..8, continuous -- selects/crossfades curated cosine-gradient palettes

// CPU-computed (double-double, ~32 decimal digit precision) perturbation
// reference orbit for Mandelbrot Pulse / Burning Ship -- see
// FractalNavigator.h/.cpp. Every pixel renders as a cheap double-float
// *offset* from this orbit (perturbation theory) instead of needing that
// same extreme precision itself; radius/fade are safe as plain floats
// (radius is just a scale, fade a 0..1 envelope). uFractalOrbitLength is
// how many valid iterations are actually stored in uFractalOrbit.
uniform sampler2D uFractalOrbit;
uniform float uFractalOrbitLength;
uniform float uFractalRadius;
uniform float uFractalFade;
// (view center - reference anchor) as two double-float pairs (x hi/lo,
// y hi/lo). Zero for the autopilot presets (their reference IS the view
// center); nonzero for the manual "explorer" presets, whose reference is
// re-anchored onto the best surviving point in view (see
// FractalNavigator's manual mode).
uniform vec4  uFractalRefOffset;
// How many iterations this view actually needs, measured on the CPU by
// sampling real escape counts around the current view (FractalNavigator's
// refreshIterNeed) -- a fixed per-decade-of-zoom growth curve was tried
// first and failed, because escape-time growth varies wildly by location.
uniform float uFractalIterNeed;

// Unbounded, continuously-shrinking zoom scale (a double-float hi/lo pair,
// updated the same iterative way as FractalNavigator's viewRadius -- never
// recomputed from a growing exponent, which is what would underflow) used
// by the exactly-self-similar IFS/DE presets (Sierpinski, Apollonian,
// Julia) for genuinely continuous deep zoom. Unlike escape-time fractals,
// self-similar folds don't accumulate chaotic error, so running the fold
// loop itself in double-float (not just the initial multiply -- the whole
// loop needs that precision to "unfold" back out of a deeply-zoomed
// starting point) gets these out to roughly 1e13x depth with no
// perturbation theory needed. uIfsFade eases 0->1 after an (extremely
// rare, only once this depth is actually exhausted) reset, same idea as
// uFractalFade.
uniform vec2  uIfsZoomScale;
uniform float uIfsFade;

// Sawtooth zoom phase in [0,1): the fractional part of the accumulated
// log2 zoom depth. For an *exactly* self-similar fractal zooming into one
// of its own fixed points (Sierpinski's corner vertex, self-similar under
// scaling by 2), the image at phase 0 and phase 1 is pixel-identical, so
// wrapping this phase gives a genuinely seamless, literally infinite dive
// with plain float precision -- no walls, ever.
uniform float uZoomPhase;

uniform sampler2D uPrevFrame;    // previous rendered frame, for feedback presets
uniform sampler2D uWaveform;     // 2048x1 R32F texture of recent mono samples, oldest at x=0

// User-uploaded picture (Load Image... in the editor), for the
// image-reactive presets. uUserImageLoaded is 0 until something is
// actually loaded, so presets can show a placeholder instead of sampling
// garbage; uUserImageAspect is width/height, used by imageContainUV below
// to letterbox the picture so the whole thing is always visible regardless
// of the plugin window's aspect ratio.
uniform sampler2D uUserImage;
uniform float uUserImageAspect;
uniform float uUserImageLoaded;

// "Pipes" preset joint positions (see Source/Rendering/PipeNetwork.h): one
// texel per joint, (x, y, z, radius), PipeNetwork::textureWidth texels
// wide. uPipeHuesA/uPipeHueE give each of the 5 pipes its own palette
// phase (4 packed in the vec4, the 5th its own float -- there's no vec5).
uniform sampler2D uPipeJoints;
uniform vec4 uPipeHuesA;
uniform float uPipeHueE;

// "Infinite Maze" preset camera state (see Source/Rendering/MazeWalker.h):
// world-space (x, z) position and normalized (x, z) facing direction. No
// texture needed -- the maze itself is a pure function of grid
// coordinates, evaluated identically (and independently) by the CPU
// navigation logic and infinite_maze.frag's rendering.
uniform vec2 uMazePos;
uniform vec2 uMazeHeading;

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


// ---------------------------------------------------------------------
// Double-float ("double-single") arithmetic: represents a number as a
// (hi, lo) pair of float32s so hi+lo carries ~45 bits of precision
// instead of float32's ~23, well past the classic GPU-fractal "turns to
// blocky mush" precision wall of plain float32 (~1e6x). Used two ways
// here: directly, for the IFS/DE presets' (Sierpinski/Apollonian/Julia)
// continuous zoom, which reaches roughly 1e12-1e13x since their fold/
// iteration doesn't need anything deeper; and as the per-pixel *delta*
// storage in Mandelbrot Pulse/Burning Ship's perturbation theory (below),
// where it's actually the reference orbit's double-float storage (not
// this arithmetic itself) that caps those two at a shallower ~1e11x --
// see FractalNavigator.h. Standard Dekker/Knuth-style compensated
// arithmetic, as used in most real-time deep-zoom fractal shaders.
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
vec2 dsAbs(vec2 a) { return a.x < 0.0 ? vec2(-a.x, -a.y) : a; } // negation is lossless

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
DComplex dcSub(DComplex a, DComplex b) { return DComplex(dsSub(a.re, b.re), dsSub(a.im, b.im)); }

// General complex multiply, needed for perturbation theory's "2*Z_n*eps"
// term (Z_n isn't always a clean real scalar like the dcSq case).
DComplex dcMul(DComplex a, DComplex b)
{
    return DComplex(dsSub(dsMul(a.re, b.re), dsMul(a.im, b.im)),
                     dsAdd(dsMul(a.re, b.im), dsMul(a.im, b.re)));
}

DComplex dcSq(DComplex a)
{
    // (re + i*im)^2 = (re^2 - im^2) + i*(2*re*im)
    vec2 re2 = dsMul(a.re, a.re);
    vec2 im2 = dsMul(a.im, a.im);
    vec2 reim = dsMul(a.re, a.im);
    return DComplex(dsSub(re2, im2), dsAdd(reim, reim));
}

// ---------------------------------------------------------------------
// Perturbation theory: reads a CPU-computed (double-double, ~32 decimal
// digit) reference orbit and iterates only the tiny per-pixel *offset*
// from it, in double-float. This is the actual mechanism behind genuinely
// deep zoom -- see FractalNavigator.h for the full explanation and
// mathr.co.uk's "Deep zoom theory and practice" for the technique this is
// based on. Shared by Mandelbrot Pulse and Burning Ship so the tricky
// numerics only have to be gotten right in one place.

DComplex referenceOrbitAt(int n)
{
    vec4 t = texelFetch(uFractalOrbit, ivec2(n, 0), 0);
    return DComplex(vec2(t.r, t.g), vec2(t.b, t.a));
}

// The pre-square fold of each formula variant, applied to a value with its
// OWN signs (dsAbs is a lossless negation). Variants matching
// FractalFormula on the CPU: 0 Mandelbrot, 1 Burning Ship,
// 2 Perpendicular Ship, 3 Buffalo, 4 Tricorn.
DComplex foldFor(int variant, DComplex z)
{
    if (variant == 1 || variant == 3) // Burning Ship / Buffalo: fold both
        return DComplex(dsAbs(z.re), dsAbs(z.im));
    if (variant == 2) // Perpendicular Ship: fold imaginary only
        return DComplex(z.re, dsAbs(z.im));
    return z;
}

// dc: this pixel's offset from the reference anchor, as a double-float
// complex delta (always small in magnitude -- bounded by the current view
// radius -- regardless of how deep the zoom has gone, which is exactly why
// double-float is enough precision for it).
//
// The step is computed as an exact difference of squares of the own-sign-
// folded values: q = (fold(full) - fold(Z)) * (fold(full) + fold(Z))
// = fold(full)^2 - fold(Z)^2. When eps is tiny and signs agree, the
// subtraction collapses (exactly, by compensated arithmetic) to the
// classic 2*Z*eps + eps^2 perturbation; when eps is large -- e.g. right
// after a rebase against Z_0 = 0, where eps *is* the whole orbit -- it
// stays exact and, crucially, the abs() folds are applied with the full
// value's own signs. (The earlier fold-by-reference-signs shortcut
// (Pauldelbrot) silently dropped the fold whenever eps dominated Z, which
// made every ship-family fractal quietly render as a plain Mandelbrot in
// rebased/wide regions.)
//
// iterMax: already clamped by the caller to uFractalOrbitLength.
// mag2 (out): final |z|^2, for smooth-iteration-count coloring.
// de (out): exterior distance estimate |z|*ln|z|/|dz/dc| -- how far this
//   pixel is from the set, which is what makes crisp, filament-hugging
//   shading possible (the derivative only matters logarithmically, so
//   tracking it in plain float alongside the double-float eps is fine).
// Returns the escape iteration count (== iterMax if it never escaped).
float perturbEscapeTime(DComplex dc, int variant, float iterMax, out float mag2, out float de)
{
    DComplex eps = DComplex(vec2(0.0), vec2(0.0));
    vec2 der = vec2(0.0);
    int refIdx = 0;
    int orbitLen = int(uFractalOrbitLength);
    float i = 0.0;
    mag2 = 0.0;
    de = 0.0;
    bool escaped = false;
    const float glitchG = 1.0e-6; // Pauldelbrot's heuristic threshold

    for (float n = 0.0; n < 2600.0; n += 1.0)
    {
        if (n >= iterMax)
            break;

        DComplex Zn = referenceOrbitAt(refIdx);
        DComplex full = dcAdd(Zn, eps);
        float fullMag2 = full.re.x * full.re.x + full.im.x * full.im.x;
        mag2 = fullMag2;

        if (fullMag2 > 16.0)
        {
            escaped = true;
            break;
        }

        float znMag2 = Zn.re.x * Zn.re.x + Zn.im.x * Zn.im.x;
        if (fullMag2 < glitchG * znMag2 || refIdx + 1 >= orbitLen)
        {
            // Glitch (this pixel's true orbit drifted too far from the
            // reference for the difference to stay precise), or the
            // reference orbit ran out (it escaped earlier than this pixel
            // does). Rebase against the start of the orbit -- Z_0 is
            // always 0 for these formulas, so the full current value
            // becomes the new eps exactly -- and still perform this
            // iteration's step from the rebased state (skipping it was a
            // deadlock when the reference escaped immediately).
            eps = full;
            refIdx = 0;
            Zn = DComplex(vec2(0.0), vec2(0.0));
        }

        DComplex V = foldFor(variant, full);
        DComplex W = foldFor(variant, Zn);
        DComplex q = dcMul(dcSub(V, W), dcAdd(V, W)); // V^2 - W^2, exact at any eps size

        DComplex step;
        if (variant == 3)
        {
            // Buffalo: post-square abs on the real part. |Re V^2|-|Re W^2|
            // needs the actual values; Re W^2 is recoverable from the
            // stored Z_n (the pre-fold doesn't change squares), and
            // Re V^2 = Re W^2 + Re q.
            vec2 reW2 = dsSub(dsMul(W.re, W.re), dsMul(W.im, W.im));
            vec2 reV2 = dsAdd(reW2, q.re);
            step = DComplex(dsSub(dsAbs(reV2), dsAbs(reW2)), q.im);
        }
        else if (variant == 4)
        {
            // Tricorn: conj(z)^2 = conj(z^2), so conjugate the difference.
            step = DComplex(q.re, dsNeg(q.im));
        }
        else
        {
            step = q;
        }

        eps = dcAdd(step, dc);
        refIdx += 1;

        // Derivative wrt c in plain float: der' = 2*z*der + 1. The sign
        // folds only flip component signs, which doesn't change |der| in
        // any way that matters for a distance *estimate*, so they're
        // skipped here. Frozen once huge to avoid float overflow.
        if (dot(der, der) < 1.0e30)
        {
            vec2 zf = vec2(full.re.x, full.im.x);
            der = 2.0 * vec2(zf.x * der.x - zf.y * der.y, zf.x * der.y + zf.y * der.x) + vec2(1.0, 0.0);
        }

        i += 1.0;
    }

    if (escaped)
    {
        float mag = sqrt(max(mag2, 1.0e-12));
        de = mag * log(mag) / max(length(der), 1.0e-12);
    }
    return i;
}

// ---------------------------------------------------------------------
// Procedural color palettes (Inigo Quilez's cosine-gradient technique --
// see https://iquilezles.org/articles/palettes/): color(t) = a + b*cos(2pi*(c*t+d)).
// Four vec3 constants per palette give a lot more character than a raw
// HSV hue sweep. uPalette (0..8, continuous) selects one, crossfading
// smoothly to the next via its fractional part, so a single automatable
// parameter sweeps through the whole set instead of exposing 12 raw
// numbers per preset.

vec3 cosPalette(float t, vec3 a, vec3 b, vec3 c, vec3 d)
{
    return a + b * cos(6.28318530718 * (c * t + d));
}

const vec3 kPaletteA[8] = vec3[8](
    vec3(0.55, 0.55, 0.55), vec3(0.55, 0.35, 0.25), vec3(0.20, 0.40, 0.55), vec3(0.55, 0.30, 0.45),
    vec3(0.65, 0.42, 0.35), vec3(0.30, 0.45, 0.30), vec3(0.50, 0.50, 0.50), vec3(0.50, 0.45, 0.60));
const vec3 kPaletteB[8] = vec3[8](
    vec3(0.55, 0.55, 0.55), vec3(0.45, 0.35, 0.25), vec3(0.20, 0.35, 0.40), vec3(0.40, 0.25, 0.45),
    vec3(0.35, 0.30, 0.30), vec3(0.25, 0.35, 0.20), vec3(0.50, 0.50, 0.50), vec3(0.50, 0.50, 0.50));
const vec3 kPaletteC[8] = vec3[8](
    vec3(1.00, 1.00, 1.00), vec3(1.00, 1.00, 0.70), vec3(0.60, 0.80, 1.00), vec3(1.20, 0.90, 1.10),
    vec3(0.90, 0.80, 0.60), vec3(0.90, 1.10, 0.70), vec3(0.30, 0.30, 0.30), vec3(2.00, 1.50, 3.00));
const vec3 kPaletteD[8] = vec3[8](
    vec3(0.00, 0.33, 0.67), vec3(0.00, 0.10, 0.20), vec3(0.35, 0.40, 0.50), vec3(0.00, 0.20, 0.60),
    vec3(0.00, 0.15, 0.30), vec3(0.10, 0.35, 0.05), vec3(0.00, 0.00, 0.00), vec3(0.30, 0.20, 0.80));
// Index: 0 Spectrum, 1 Fire, 2 Ice, 3 Synthwave, 4 Sunset, 5 Forest, 6 Mono, 7 Psychedelic

// t: the preset's own 0..1 "how deep / how escaped" value. hue: the
// existing Hue knob, applied as an extra phase rotation so it keeps doing
// something meaningful no matter which palette is selected.
vec3 palette(float t, float hue)
{
    t = fract(t + hue);
    float p = mod(uPalette, 8.0);
    int i0 = int(floor(p));
    int i1 = (i0 + 1) % 8;
    float f = fract(p);
    vec3 colA = cosPalette(t, kPaletteA[i0], kPaletteB[i0], kPaletteC[i0], kPaletteD[i0]);
    vec3 colB = cosPalette(t, kPaletteA[i1], kPaletteB[i1], kPaletteC[i1], kPaletteD[i1]);
    return mix(colA, colB, f);
}

// ---------------------------------------------------------------------
// Renders one layer of an escape-time perturbation fractal, the way real
// fractal renders shade it: pure black interior, a fast-cycling cosine-
// palette exterior driven by the smooth iteration count, and a thin dark
// crease hugging the set boundary carved by the per-pixel distance
// estimate -- which is what makes filaments read as *sharp fractal edges*
// rather than soft blobs. radiusMultiplier > 1 renders a wider view of the
// same anchor (the multi-scale overlay the kaleidoscope presets use);
// plain presets pass 1.0.
vec3 fractalLayer(vec2 uv, int variant, float radiusMultiplier, float hueShift)
{
    float layerRadius = uFractalRadius * radiusMultiplier;
    vec2 offset = uv * layerRadius;
    DComplex dc = DComplex(dsAdd(dsSet(offset.x), uFractalRefOffset.xy),
                           dsAdd(dsSet(offset.y), uFractalRefOffset.zw));

    // Iteration budget: the CPU-measured need for the deep view, scaled by
    // the Iterations knob (default 24 = 1x). Wider overlay layers resolve
    // with far fewer iterations, so they get a reduced budget.
    float iterScale = clamp(uIterations / 24.0, 0.35, 2.0);
    float base = uFractalIterNeed * iterScale;
    if (radiusMultiplier > 1.5)
        base = max(base * 0.55, 300.0);
    float iterMax = clamp(base, 60.0, min(uFractalOrbitLength - 1.0, 2400.0));
    float mag2 = 0.0;
    float de = 0.0;
    float i = perturbEscapeTime(dc, variant, iterMax, mag2, de);

    if (i >= iterMax - 0.5)
        return vec3(0.0); // interior: pure black, like a real fractal render

    // Smooth (fractional) iteration count, compressed by sqrt so band
    // density stays pleasing across the whole 0..2400 range.
    float smoothI = max(i + 1.0 - log2(max(log2(max(mag2, 1.0001)), 1.0e-6)), 0.0);
    float t = sqrt(smoothI);

    vec3 col = palette(t * 0.13 + hueShift + uTime * 0.015, uHue);
    float shade = 0.62 + 0.38 * sin(t * 2.3 - uTime * 0.25);

    // Distance-estimate edge emphasis: how many *pixels* this point sits
    // from the set. Darkening the last ~3 pixels before the boundary draws
    // a crisp outline around every filament at every zoom level.
    float pixSize = layerRadius / max(uResolution.y, 1.0);
    float edge = clamp(de / max(pixSize, 1.0e-30), 0.0, 60.0);
    float crease = smoothstep(0.0, 3.0, edge);

    return col * shade * mix(0.08, 1.0, crease);
}

// ---------------------------------------------------------------------
// User-image helpers, shared by the image-reactive presets.

// Maps aspect-corrected, centered screen coordinates (the standard
// `(vUv-0.5)*vec2(uResolution.x/uResolution.y,1.0)` pattern every other
// preset already uses) onto the uploaded image's own 0..1 UV space, fit
// so the whole picture is always visible ("contain"/letterbox) no matter
// how the plugin window's aspect ratio compares to the image's.
vec2 imageContainUV(vec2 screenUV, float imageAspect)
{
    // Not named `half` -- that's a GLSL *reserved* word (kept aside for a
    // future extension, alongside `fixed`/`hvec2`/etc.), not merely a
    // used one. Most compiler front-ends silently tolerate it as an
    // ordinary identifier, but this is exactly the kind of latent bug
    // that stays invisible for a long time and then hard-fails the
    // moment a stricter driver/GL-context path compiles it -- which is
    // precisely what happened here: every single preset failing to
    // compile, identically, at this one shared line, the moment a
    // different context-negotiation path (a plugin host embedding the
    // GL surface differently than a plain top-level window, in this
    // case) exercised a compiler that actually enforces the reservation.
    vec2 halfSize = imageAspect > 1.0 ? vec2(imageAspect, 1.0) : vec2(1.0, 1.0 / imageAspect);
    return screenUV / halfSize + 0.5;
}

// Samples the user image at 0..1 UV, returning transparent black outside
// [0,1] (the image's own edge) or before anything has been loaded, so
// callers can composite against a background without a branch of their
// own. Flips V: JUCE decodes images top-row-first, uploaded as-is, while
// GL texture V=0 is conventionally the bottom.
vec4 sampleUserImage(vec2 uv01)
{
    if (uUserImageLoaded < 0.5 || uv01.x < 0.0 || uv01.x > 1.0 || uv01.y < 0.0 || uv01.y > 1.0)
        return vec4(0.0);
    return texture(uUserImage, vec2(uv01.x, 1.0 - uv01.y));
}

// What every image preset shows before an image is actually loaded: a
// gentle pulsing palette glow with an implicit "drop an image in" read,
// so the preset never looks broken/black while idle.
vec3 imagePlaceholder(vec2 uv)
{
    float r = length(uv);
    float pulse = 0.5 + 0.5 * sin(uTime * 1.4);
    float ring = smoothstep(0.02, 0.0, abs(r - (0.28 + 0.03 * pulse)));
    vec3 col = palette(r * 0.6 + uTime * 0.02, uHue) * (0.10 + ring * 0.9);
    return col;
}

// Shared body for the plain "explorer" presets: the fractal rendered
// straight, single-scale, no kaleidoscope -- the classic textbook view.
// The view is driven manually (mouse wheel / up-down arrows zoom, drag
// pans -- see FractalNavigator's manual mode); audio only breathes the
// brightness and flashes escaped regions on onsets, deliberately never
// moving the camera out from under the user. ySign = -1.0 flips the
// imaginary axis for the ship-family formulas, whose classic orientation
// is drawn with imaginary-down (the "ship" reads right side up).
vec3 exploreFractal(int variant, float ySign)
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv.y *= ySign;

    vec3 col = fractalLayer(uv, variant, 1.0, 0.0);
    col *= 0.85 + uLevel * uReactivity * 0.45;
    col += uOnset * uReactivity * uCameraShake * 0.2 * palette(uTime * 0.01, uHue)
           * smoothstep(0.0, 0.05, dot(col, col));
    col *= uFractalFade;
    return grade(col);
}
