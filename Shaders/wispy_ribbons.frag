// Wispy Ribbons — flowing silk-like bands drifting across the screen. Each
// ribbon's centerline is a closed-form function of x (layered sine drift,
// no domain warping/raymarching), so the whole preset costs one small fixed
// loop of cheap sine evaluations per pixel -- deliberately built this way
// (same "closed-form curve, not a per-pixel search" trick Waveform Scope
// uses) so it can't end up costing what Pipes used to.

float ribbonCurve(float x, float t, float seed, float bass, float mid, float react)
{
    float y = sin(x * (1.6 + seed * 0.4) + t * (0.5 + seed * 0.15) + seed * 3.1) * 0.22;
    y += sin(x * (0.7 + seed * 0.2) - t * (0.3 + seed * 0.1) + seed * 7.7) * 0.12;
    y += sin(x * (3.1 + seed * 0.6) + t * (0.9 + seed * 0.2)) * 0.035 * (0.4 + mid * react);
    y += sin(t * (0.35 + seed * 0.1) + seed * 5.3) * 0.18 * (0.3 + bass * react * 0.7);
    return y;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;

    float react = uReactivity;
    float t = uTime * (0.3 + 0.5 * uZoomSpeed);
    vec3 col = vec3(0.008, 0.008, 0.014);

    const int numRibbons = 6;
    for (int i = 0; i < numRibbons; ++i)
    {
        float seed = float(i);
        float laneOffset = (seed / float(numRibbons - 1) - 0.5) * 0.9;

        float centerY = laneOffset + ribbonCurve(uv.x, t, seed, uBass, uMid, react);
        float halfWidth = (0.02 + 0.018 * sin(uv.x * 1.3 + seed * 2.0 + t * 0.6))
                         * (1.0 + uBass * react * 0.9 + uOnset * react * 0.6);
        halfWidth = max(halfWidth, 0.006);

        float dist = abs(uv.y - centerY);
        float band = smoothstep(halfWidth, halfWidth * 0.15, dist);

        // Silk sheen: a brighter streak riding along the ribbon, offset from
        // center, that drifts independently for a subtle shimmer.
        float sheen = smoothstep(halfWidth * 0.5, 0.0, abs(dist - halfWidth * 0.35 * sin(uv.x * 2.0 + t + seed)));

        float hueT = seed / float(numRibbons) + t * 0.02 + uTreble * react * 0.3;
        vec3 ribbonCol = palette(hueT, uHue);

        col += ribbonCol * band * (0.55 + uLevel * react * 0.5);
        col += ribbonCol * sheen * band * 0.5;
    }

    col += uOnset * react * 0.12 * palette(t * 0.05, uHue);

    fragColor = vec4(grade(col), 1.0);
}
