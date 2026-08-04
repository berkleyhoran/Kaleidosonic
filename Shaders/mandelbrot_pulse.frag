// Mandelbrot Pulse — classic escape-time Mandelbrot. Where to zoom is
// decided on the CPU (see FractalNavigator.h/.cpp) using real double
// precision and a distance-estimator-guided autopilot that hill-climbs
// toward nearby boundary detail, so the dive keeps finding "points of
// interest" instead of drifting into a flat black interior lake or a
// featureless white-ish far exterior. The per-pixel iteration here then
// runs in double-float (~45-bit) precision for a genuinely deep, smooth
// zoom with no per-frame camera jolts -- speed changes are continuous, so
// it never snaps or jitters even at high Camera Shake.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Gentle continuous rotation/swirl for texture. Deliberately mild and
    // NOT coupled to a hard onset snap -- see the comment above.
    float swirl = uDistortion * (0.25 + uTreble * react * 1.0);
    float ang = uTime * 0.04 * uRotationSpeed + swirl * length(uv);
    uv = rotate2d(ang) * uv;

    vec2 offset = uv * uFractalRadius;
    DComplex c = DComplex(dsAdd(uFractalRe, dsSet(offset.x)), dsAdd(uFractalIm, dsSet(offset.y)));
    DComplex z = DComplex(dsSet(0.0), dsSet(0.0));

    float iterMax = clamp(uIterations, 8.0, 256.0);
    float i = 0.0;
    float mag2 = 0.0;
    for (float n = 0.0; n < 256.0; n += 1.0)
    {
        mag2 = z.re.x * z.re.x + z.im.x * z.im.x;
        if (n >= iterMax || mag2 > 16.0)
            break;
        z = dcAdd(dcSq(z), c);
        i += 1.0;
    }

    float escaped = step(i, iterMax - 1.0);
    float smoothI = i - log2(max(log2(mag2 + 1e-6), 1e-6));
    float t = smoothI / iterMax;

    float hue = fract(uHue + t * 1.5 + uTime * 0.03 + uMid * react * 1.0);
    // Interior (non-escaping) points get a small nonzero glow floor instead
    // of hard black, and the exterior oscillation is kept always-positive
    // (0.55 +/- 0.45, not 0.3 +/- 0.55) -- otherwise either side could dip
    // to zero/negative and the whole screen could still read as solid
    // black or blown-out white depending on exactly where the navigator's
    // hill-climb currently sits.
    float exteriorVal = 0.55 + 0.45 * sin(t * 6.2831 + uOnset * react * 4.0) + uLevel * react * 0.8;
    float val = mix(0.14 + uLevel * react * 0.3, exteriorVal, escaped);
    vec3 col = hsv2rgb(vec3(hue, uSaturation, clamp(val, 0.0, 1.0)));

    col += uOnset * react * uCameraShake * vec3(0.35, 0.2, 0.5) * (1.0 - t);
    col *= (0.85 + uLevel * react * 1.0) * uFractalFade;

    fragColor = vec4(grade(col), 1.0);
}
