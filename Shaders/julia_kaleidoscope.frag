// Julia Kaleidoscope — a drifting Julia set folded through N mirror
// segments, diving into the pattern while the "c" constant orbits, and
// the mid band widens the orbit. The dive is centered on the *repelling
// fixed point* beta = (1 + sqrt(1 - 4c)) / 2, which provably lies ON the
// Julia set for every c -- so no matter where the orbiting c has dragged
// the shape, the zoom target always sits on the boundary where the detail
// lives, instead of diving into the featureless origin (which is what
// used to fade to a flat color at depth). beta is only float-precise
// (~1e-7), so depth is capped where that precision is still comfortably
// below the view size; the cap is plenty deep for a set whose real visual
// interest is the continuously reshaping c orbit.
const vec2 kJuliaZoomFloor = vec2(2.0e-5, 0.0);

// Principal complex square root.
vec2 csqrt(vec2 w)
{
    float r = length(w);
    float re = sqrt(max((r + w.x) * 0.5, 0.0));
    float im = sqrt(max((r - w.x) * 0.5, 0.0));
    return vec2(re, w.y >= 0.0 ? im : -im);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv *= 1.4 * uCameraScale;

    float react = uReactivity;
    uv = rotate2d(uTime * 0.06 * uRotationSpeed + uOnset * uCameraShake * 0.4) * uv;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    // cAngle deliberately does NOT react to onset: c fully determines the
    // Julia set's shape, so any abrupt jump here is a full-pattern jump
    // cut (not a color change) -- that's what was reading as "flashing".
    // Onset instead only adds a brightness flash near the end of main().
    float orbit = (0.72 + 0.2 * uMid * react) * mix(1.0, 1.15, clamp(uZoomWander * 0.5, 0.0, 1.0));
    float cAngle = uTime * 0.14;
    vec2 c = vec2(cos(cAngle), sin(cAngle * 1.3)) * orbit;
    DComplex dcC = DComplex(dsSet(c.x), dsSet(c.y));

    // The repelling fixed point of z^2 + c: always on the Julia set.
    vec2 beta = (vec2(1.0, 0.0) + csqrt(vec2(1.0 - 4.0 * c.x, -4.0 * c.y))) * 0.5;

    vec2 zoomScale = uIfsZoomScale.x > kJuliaZoomFloor.x ? uIfsZoomScale : kJuliaZoomFloor;
    DComplex z = DComplex(dsAdd(dsSet(beta.x), dsMul(zoomScale, dsSet(uv.x))),
                          dsAdd(dsSet(beta.y), dsMul(zoomScale, dsSet(uv.y))));

    // Distortion magnitude scales down with zoom depth -- otherwise, at
    // deep zoom, a fixed-size offset would completely swamp the (by then
    // tiny) fractal detail instead of just adding surface texture to it.
    float distortAmt = uDistortion * 0.3 * zoomScale.x;
    z.re = dsAdd(z.re, dsSet(distortAmt * sin(uv.y * 5.0 + uTime)));
    z.im = dsAdd(z.im, dsSet(distortAmt * cos(uv.x * 5.0 + uTime)));

    // Iteration need grows as the zoom deepens (points near the boundary
    // take longer to resolve), so scale the cap with depth like the
    // perturbation presets do.
    float decades = max(0.0, -log2(max(zoomScale.x, 1.0e-9)) * 0.30103);
    float iterMax = clamp(uIterations * 3.0 + decades * 40.0, 24.0, 400.0);
    float i = 0.0;
    float mag2 = 0.0;
    for (float n = 0.0; n < 400.0; n += 1.0)
    {
        if (n >= iterMax)
            break;
        mag2 = z.re.x * z.re.x + z.im.x * z.im.x;
        if (mag2 > 16.0)
            break;
        z = dcAdd(dcSq(z), dcC);
        i += 1.0;
    }

    vec3 col;
    if (i >= iterMax - 0.5)
    {
        col = vec3(0.0); // interior: black, like a real render
    }
    else
    {
        float smoothI = max(i + 1.0 - log2(max(log2(max(mag2, 1.0001)), 1.0e-6)), 0.0);
        float t = sqrt(smoothI);
        col = palette(t * 0.16 - uTime * 0.03 + uTreble * react * 0.5, uHue);
        float shade = 0.6 + 0.4 * sin(t * 2.1 - uTime * 0.2);
        col *= shade * (0.55 + 0.8 * uLevel * react);
    }

    col += uOnset * react * uCameraShake * 0.5 * vec3(0.7, 0.4, 1.0) * smoothstep(0.0, 0.05, dot(col, col));
    col *= uIfsFade;

    fragColor = vec4(grade(col), 1.0);
}
