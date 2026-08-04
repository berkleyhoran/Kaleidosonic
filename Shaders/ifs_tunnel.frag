// IFS Tunnel — domain-repeated, folded tunnel built from an iterated
// function system. Bass drives forward speed hard, onset gives a lurching
// forward kick, treble adds extra folding detail.

vec2 fold(vec2 p, float a)
{
    vec2 n = vec2(cos(a), sin(a));
    float d = dot(p, n);
    return p - 2.0 * min(d, 0.0) * n;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;

    float react = uReactivity;
    float segs = max(uKaleidoscopeSegments, 1.0);

    float angle = atan(uv.y, uv.x) + uTime * 0.06 * uRotationSpeed;
    float radius = length(uv) + 1e-4;

    // Forward motion is driven by Camera Shake, not Reactivity -- coupling
    // actual camera speed to the same knob as color/brightness reactivity
    // made high-Reactivity settings feel like motion sickness.
    float depth = uTime * (0.4 + 1.6 * uZoomSpeed) + uBass * uCameraShake * 8.0 + uOnset * uCameraShake * 3.0;
    float tunnel = 1.0 / radius + depth;

    vec2 p = vec2(angle * segs / 3.14159, tunnel);

    float iterMax = clamp(uIterations, 3.0, 12.0);
    float scale = 1.0;
    for (float n = 0.0; n < 12.0; n += 1.0)
    {
        if (n >= iterMax) break;
        p = abs(p) - (0.6 + 0.3 * uDistortion + 0.2 * uTreble * react);
        p = fold(p, 0.9 + n * 0.15 + uOnset * uCameraShake * 0.8);
        p *= 1.25;
        scale *= 1.25;
    }

    float d = length(p) / scale;
    float glow = 1.0 - smoothstep(0.0, 0.9, d);

    float hue = fract(uHue + tunnel * 0.05 + uMid * react * 1.1 + uTime * 0.025);
    vec3 col = hsv2rgb(vec3(hue, uSaturation, glow));
    col *= 0.3 + 1.8 * uLevel * react;

    float rings = 0.5 + 0.5 * sin(tunnel * 8.0 - uOnset * react * 10.0);
    col += rings * glow * 0.4 * vec3(0.6, 0.8, 1.0) * (0.4 + react);
    col += uOnset * react * glow * 0.7 * vec3(1.0, 0.7, 0.9);

    fragColor = vec4(grade(col), 1.0);
}
