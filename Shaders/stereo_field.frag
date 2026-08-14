// Stereo Field — a real goniometer/vectorscope: L/R sample pairs from
// uStereoScope (see AudioAnalyzer::stereoScopeSize) plotted the classic
// rotated way -- x = L-R (the "side"/width axis), y = L+R (the "mid"/mono
// axis) -- so pure mono content collapses to a vertical line up the middle
// and wide/decorrelated content spreads sideways into a fuller shape. Only
// a subsample of the scope (numPoints, not all 1024) is actually plotted
// per pixel each frame -- same "loop over a fixed small point count, glow
// each one" technique Particle Bloom already uses, deliberately kept far
// below Pipes' old per-pixel cost since that's exactly what made Pipes
// laggy. Cool blue/cyan by default (same fixed hue bias as the Spectrum
// presets), with a tint toward red as phase correlation goes negative --
// a real early-warning cue for mono-collapse risk, not just decoration.

float sfHash(float n) { return fract(sin(n) * 43758.5453123); }

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;
    float react = uReactivity;

    vec3 col = vec3(0.008, 0.01, 0.016);

    // Faint diamond reference grid: vertical = mono axis, horizontal = pure
    // side/anti-phase axis, plus a unity circle so scale reads at a glance.
    col += vec3(0.1, 0.16, 0.22) * smoothstep(0.0025, 0.0, abs(uv.x)) * 0.35;
    col += vec3(0.1, 0.16, 0.22) * smoothstep(0.0025, 0.0, abs(uv.y)) * 0.25;
    float ring = smoothstep(0.01, 0.0, abs(length(uv) - 0.5));
    col += vec3(0.08, 0.12, 0.18) * ring * 0.3;

    float coolHue = uHue + 0.55;
    // Red the more out-of-phase the signal is (uCorrelation -1..1) -- a
    // real mono-collapse warning, not just a palette choice.
    float phaseWarning = smoothstep(0.15, -0.6, uCorrelation);
    vec3 warnCol = vec3(1.0, 0.15, 0.1);

    const int numPoints = 128;
    const int stride = kStereoScopeSize / numPoints;
    float scale = (0.42 + 0.22 * react) * (1.0 + uLevel * react * 0.4);

    for (int i = 0; i < numPoints; ++i)
    {
        float texX = (float(i * stride) + 0.5) / float(kStereoScopeSize);
        vec2 lr = texture(uStereoScope, vec2(texX, 0.5)).rg;
        vec2 pos = vec2(lr.x - lr.y, lr.x + lr.y) * scale;

        // Phosphor-style persistence: most recent samples (near the end of
        // the ring buffer) brightest, older ones fading.
        float age = float(i) / float(numPoints);
        float bright = age * age;

        float size = mix(0.006, 0.01, age) * (1.0 + uOnset * react * 1.5);
        float d = length(uv - pos);
        float g = size / (d * d / size + size);

        vec3 pointCol = palette(age * 0.5 + uTime * 0.02, coolHue);
        pointCol = mix(pointCol, warnCol, phaseWarning * 0.6);

        col += pointCol * g * (0.5 + bright * 1.2) * (0.6 + uLevel * react * 0.8);
    }

    col += uOnset * react * 0.15 * mix(palette(uTime * 0.05, coolHue), warnCol, phaseWarning * 0.5);

    fragColor = vec4(grade(col), 1.0);
}
