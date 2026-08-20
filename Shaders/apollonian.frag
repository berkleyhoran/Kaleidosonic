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

// Bug that made this render solid black at every setting: `s` was being
// used as BOTH the per-iteration fold radius (dividing r2 to get k) AND
// the running scale accumulator (s *= k), so it compounded as
// s_new = s_old * (s_old / r2) = s_old^2 / r2 -- a quadratic runaway that
// sends s to somewhere near 0 or nearly infinite within a handful of the
// 10 iterations depending on whether r2 tends above or below 1, for
// essentially every pixel. Whichever direction it blew up, the final
// length(p)/abs(s) landed either far above 1 (glow's clamp(1-d,0,1)
// floors to 0) or effectively 0 divided by a huge s (also ~0) -- either
// way, zero glow everywhere, a black screen regardless of any parameter.
// The fix (matching the standard Apollonian IFS formula this is based
// on): fixedRadius2 stays a genuinely fixed value each iteration (it can
// still vary frame-to-frame via the caller's audio/time-modulated
// argument -- that part was fine), and totalScale is a SEPARATE
// accumulator that only tracks the product of k's for the final divide.
float apollonian(vec2 p, float fixedRadius2)
{
    float totalScale = 1.0;
    for (int n = 0; n < 10; ++n)
    {
        p = -1.0 + 2.0 * fract(0.5 * p + 0.5);
        float r2 = dot(p, p);
        float k = fixedRadius2 / max(r2, 1e-4);
        p *= k;
        totalScale *= k;
    }
    return length(p) / abs(totalScale);
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
