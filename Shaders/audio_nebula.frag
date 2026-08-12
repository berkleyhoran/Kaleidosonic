// Audio Nebula — domain-warped fractal (FBM) noise clouds, the classic
// "iq warping" technique: the noise field's own value warps its sampling
// coordinates, twice, which turns flat noise into billowing smoke.
// Audio-reactive by construction: bass drives the warp amplitude (the
// clouds physically churn on low end), treble adds a fine shimmer octave,
// level breathes the density, and onsets flash lightning through the
// cloud body along ridged-noise filaments.

float nHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = nHash(i);
    float b = nHash(i + vec2(1.0, 0.0));
    float c = nHash(i + vec2(0.0, 1.0));
    float d = nHash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p)
{
    float amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < 6; ++i)
    {
        sum += amp * vnoise(p);
        p = p * 2.03 + vec2(17.1, 9.2);
        amp *= 0.5;
    }
    return sum;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    uv = rotate2d(uTime * 0.02 * uRotationSpeed) * uv;
    vec2 p = uv * 2.4 * uCameraScale + vec2(uTime * 0.035, uTime * 0.012);

    // Double domain warp: q warps r, r warps the final density read. Bass
    // cranks the warp amplitude so the clouds visibly churn with the low
    // end; Distortion adds a manual baseline churn.
    float warpAmt = 1.1 + uDistortion * 1.6 + uBass * react * 1.4;
    vec2 q = vec2(fbm(p + vec2(0.0, 0.0)), fbm(p + vec2(5.2, 1.3)));
    vec2 r = vec2(fbm(p + warpAmt * q + vec2(1.7, 9.2) + uTime * 0.06),
                  fbm(p + warpAmt * q + vec2(8.3, 2.8) - uTime * 0.045));
    float density = fbm(p + 2.8 * r);

    // Treble shimmer: a fast, fine-grained sparkle octave on top.
    density += uTreble * react * 0.12 * vnoise(p * 9.0 + uTime * 2.5);

    // Color: palette through the density, tinted by where the warp folded
    // (q/r make great free color variety), breathing with the level.
    float t = density * 1.25 + r.x * 0.35 + uTime * 0.012;
    vec3 col = palette(t, uHue);
    float body = smoothstep(0.25, 0.95, density);
    col *= body * (0.55 + uLevel * react * 1.1) + 0.03;

    // Depth: darken where the first warp field is thin, so the clouds get
    // shadowed undersides instead of reading flat.
    col *= 0.55 + 0.45 * smoothstep(0.2, 0.8, q.y);

    // Lightning on onsets: thin ridged-noise filaments flashed white-blue,
    // only inside the cloud body so bolts read as inside the storm.
    float ridge = 1.0 - abs(2.0 * fbm(p * 3.1 + vec2(uTime * 0.4, 0.0)) - 1.0);
    float bolt = pow(smoothstep(0.86, 1.0, ridge), 3.0) * uOnset * react;
    col += bolt * body * vec3(0.75, 0.85, 1.2) * (1.0 + uCameraShake);

    fragColor = vec4(grade(col), 1.0);
}
