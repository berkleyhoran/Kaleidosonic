// Burning Ship — escape-time fractal using z = (|Re(z)| + i|Im(z)|)^2 + c,
// giving the classic spiky/flame silhouette. Same CPU-navigated, double-
// float autopilot dive as Mandelbrot Pulse -- see its comment and
// FractalNavigator.h/.cpp for how the zoom target is chosen.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    float swirl = uDistortion * (0.2 + uTreble * react * 1.0);
    float ang = uTime * 0.035 * uRotationSpeed + swirl * length(uv);
    uv = rotate2d(ang) * uv;

    vec2 offset = uv * uFractalRadius;
    DComplex c = DComplex(dsAdd(uFractalRe, dsSet(offset.x)), dsAdd(uFractalIm, dsSet(offset.y)));
    DComplex z = DComplex(dsSet(0.0), dsSet(0.0));

    float iterMax = clamp(uIterations, 8.0, 200.0);
    float i = 0.0;
    float mag2 = 0.0;
    for (float n = 0.0; n < 200.0; n += 1.0)
    {
        mag2 = z.re.x * z.re.x + z.im.x * z.im.x;
        if (n >= iterMax || mag2 > 16.0)
            break;
        // Burning Ship folds both components to their absolute value before
        // squaring, which is what gives it the spiky "flame" silhouette
        // instead of Mandelbrot's smooth cardioid.
        z.re = vec2(abs(z.re.x), 0.0);
        z.im = vec2(abs(z.im.x), 0.0);
        z = dcAdd(dcSq(z), c);
        i += 1.0;
    }

    float escaped = step(i, iterMax - 1.0);
    float smoothI = i - log2(max(log2(mag2 + 1e-6), 1e-6));
    float t = clamp(smoothI / iterMax, 0.0, 1.0);

    float hue = fract(uHue + 0.04 + t * 0.35 - uMid * react * 0.5 + uTime * 0.012);
    // Interior points AND fast-escaping far-exterior points (t near 0) both
    // get a small glow floor instead of fading to hard black -- see
    // Mandelbrot Pulse's comment for why.
    float exteriorVal = 0.15 + pow(t, 0.5) * (0.55 + uLevel * react * 1.2);
    float val = mix(0.12 + uLevel * react * 0.25, exteriorVal, escaped);
    float sat = clamp(uSaturation * (0.8 + 0.4 * t), 0.0, 1.0);
    vec3 col = hsv2rgb(vec3(hue, sat, clamp(val, 0.0, 1.0)));

    col += uOnset * react * uCameraShake * vec3(0.6, 0.28, 0.05) * (1.0 - t);
    col *= uFractalFade;

    fragColor = vec4(grade(col), 1.0);
}
