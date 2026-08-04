// Apollonian Gasket — repeated circle-inversion folding, the classic
// "infinite nested circles" shader fractal. Continuously dives deeper into
// the nested rings instead of just pulsing, kaleidoscope-segmented and
// heavily audio-modulated: bass pushes the inversion center, treble speeds
// the nesting, onset flashes the innermost rings.

float apollonian(vec2 p, float scale)
{
    float s = scale;
    for (int n = 0; n < 8; ++n)
    {
        p = -1.0 + 2.0 * fract(0.5 * p + 0.5);
        float r2 = dot(p, p);
        float k = s / max(r2, 1e-4);
        p *= k;
        s *= k;
    }
    return length(p) / abs(s);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv *= 1.6 * uCameraScale;

    float react = uReactivity;
    uv = rotate2d(uTime * 0.06 * uRotationSpeed) * uv;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    // Center drift and inversion scale are driven by Camera Shake, not
    // Reactivity -- see IFS Tunnel's comment for why.
    vec2 center = 0.18 * uZoomWander * vec2(sin(uTime * 0.17 + uBass * uCameraShake * 1.8), cos(uTime * 0.13));
    vec2 zc = zoomCycle(8.0, 0.6);
    float scale = 1.0 + zc.x * 0.4 + uTreble * uCameraShake * 0.4;

    float d = apollonian(uv - center, scale);
    float glow = pow(clamp(1.0 - d, 0.0, 1.0), 3.0 + uDistortion * 6.0);

    float hue = fract(uHue + d * 1.3 + uMid * react * 0.9 + uTime * 0.02);
    float val = clamp(glow * (0.7 + uLevel * react * 1.4), 0.0, 1.0);
    vec3 col = hsv2rgb(vec3(hue, uSaturation, val));

    float rings = smoothstep(0.0, 0.06, 0.06 - abs(fract(d * 6.0) - 0.5) * 0.12);
    col += rings * 0.3 * vec3(0.8, 0.6, 1.0) * (0.4 + react);
    col += uOnset * react * uCameraShake * glow * vec3(1.2, 0.6, 1.0);
    col *= mix(0.3, 1.0, zc.y);

    fragColor = vec4(grade(col), 1.0);
}
