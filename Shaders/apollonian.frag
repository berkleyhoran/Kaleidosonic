// Apollonian Gasket — repeated circle-inversion folding, the classic
// "infinite nested circles" shader fractal. Its endless nesting comes from
// the fold itself (a domain-repetition step, then a "ball fold" that
// aggressively renormalizes anything that strays too close to the origin
// back out to a normal scale) rather than from tracking one absolute deep
// coordinate the way Mandelbrot/Sierpinski do -- each frame's fold starts
// fresh from screen space, so it doesn't need double-float or perturbation
// precision to keep deepening; ordinary float32 stays accurate throughout.
// uIfsZoomScale (unbounded, never wraps) drives real continuous zoom into
// that fold instead of the old fixed-length pulse cycle, kaleidoscope-
// segmented and heavily audio-modulated: bass pushes the inversion
// center, treble speeds the nesting, onset flashes the innermost rings.

// First bug (made this render solid black at every setting): `s` was used
// as BOTH the per-iteration fold radius (dividing r2 to get k) AND the
// running scale accumulator (s *= k), so it compounded as
// s_new = s_old^2 / r2 -- a quadratic runaway sending it near 0 or near-
// infinite within a handful of iterations, every pixel, every time --
// zero glow everywhere regardless of any parameter.
//
// Second bug (fixed the black screen, but then rendered as flat, mostly
// uniform color instead of the actual nested-circle detail): splitting
// the accumulator out was necessary but not sufficient -- a *linear*
// product of 10 k's still drifts strongly toward 0 or infinity whenever
// fixedRadius2 isn't kept close to the fold's own natural r2 range
// (~0..2, since p is always re-folded into [-1,1) each iteration), and
// the caller's audio-modulated fixedRadius2 argument ranges up to ~3.2 --
// nowhere near 1. With totalScale trending the same direction at nearly
// every pixel, length(p)/totalScale ends up in a narrow band almost
// everywhere, so glow's clamp(1-d,0,1) saturates near-uniformly instead
// of tracing actual fractal boundaries.
//
// Real fix: an orbit trap instead of an accumulated-scale distance. p
// is *always* bounded (the fract() fold keeps every component in
// [-1,1), so r2 is always in [0,2]) regardless of fixedRadius2's exact
// value -- tracking the minimum r2 reached across all 10 iterations is
// therefore inherently stable and bounded no matter what, with no
// accumulator to destabilize at all. This is the standard technique 2D
// Apollonian/Mandelbox-family "fractal glow" shaders actually use --
// color by *where* the fold's orbit passes closest to the origin, not by
// a running scale product meant for literal 3D raymarch distance
// estimation (which this preset was never doing in the first place; see
// the file header -- there's no raymarch loop here at all).
float apollonian(vec2 p, float fixedRadius2)
{
    float trap = 1.0e5;
    for (int n = 0; n < 10; ++n)
    {
        p = -1.0 + 2.0 * fract(0.5 * p + 0.5);
        float r2 = dot(p, p);
        trap = min(trap, r2);
        float k = fixedRadius2 / max(r2, 1e-4);
        p *= k;
    }
    return sqrt(trap);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv *= 1.6 * uCameraScale;

    float react = uReactivity;
    uv = rotate2d(uTime * 0.06 * uRotationSpeed) * uv;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    // Real continuous zoom -- the fold's ball-fold renormalization means
    // ordinary float precision stays accurate no matter how small this
    // gets, so just the "hi" component of the double-float scale is
    // plenty here (still representable down to a tiny exponent, which is
    // all that's needed).
    uv *= uIfsZoomScale.x;

    // Center drift and inversion scale are driven by Camera Shake, not
    // Reactivity -- see Plasma Feedback's comment for why. The scale animation
    // combines two incommensurate frequencies so it never visibly repeats.
    vec2 center = 0.18 * uZoomWander * vec2(sin(uTime * 0.17 + uBass * uCameraShake * 1.8), cos(uTime * 0.13));
    float scale = 1.6 + 0.5 * sin(uTime * 0.045) + 0.3 * sin(uTime * 0.071 + 1.7) + uTreble * uCameraShake * 0.4;

    float d = apollonian(uv - center, scale);
    float glow = pow(clamp(1.0 - d, 0.0, 1.0), 3.0 + uDistortion * 6.0);

    float paletteT = d * 1.3 + uMid * react * 0.9 + uTime * 0.02;
    float val = clamp(glow * (0.7 + uLevel * react * 1.4), 0.0, 1.0);
    vec3 col = palette(paletteT, uHue) * val;

    float rings = smoothstep(0.0, 0.06, 0.06 - abs(fract(d * 6.0) - 0.5) * 0.12);
    col += rings * 0.3 * vec3(0.8, 0.6, 1.0) * (0.4 + react);
    col += uOnset * react * uCameraShake * glow * vec3(1.2, 0.6, 1.0);
    col *= uIfsFade;

    fragColor = vec4(grade(col), 1.0);
}
