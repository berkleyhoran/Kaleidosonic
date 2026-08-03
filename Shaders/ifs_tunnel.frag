// IFS Tunnel — domain-repeated, folded tunnel built from an iterated
// function system. Bass drives forward speed, treble adds extra folding.

vec2 fold(vec2 p, float a)
{
    vec2 n = vec2(cos(a), sin(a));
    float d = dot(p, n);
    return p - 2.0 * min(d, 0.0) * n;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    float react = uReactivity;
    float segs = max(uKaleidoscopeSegments, 1.0);

    float angle = atan(uv.y, uv.x) + uTime * 0.05 * uRotationSpeed;
    float radius = length(uv) + 1e-4;

    float depth = uTime * (0.35 + 0.9 * uZoomSpeed) + uBass * react * 2.0;
    float tunnel = 1.0 / radius + depth;

    vec2 p = vec2(angle * segs / 3.14159, tunnel);

    float iterMax = clamp(uIterations, 3.0, 12.0);
    float scale = 1.0;
    for (float n = 0.0; n < 12.0; n += 1.0)
    {
        if (n >= iterMax) break;
        p = abs(p) - (0.6 + 0.15 * uDistortion + 0.05 * uTreble * react);
        p = fold(p, 0.9 + n * 0.15 + uOnset * react * 0.4);
        p *= 1.25;
        scale *= 1.25;
    }

    float d = length(p) / scale;
    float glow = 1.0 - smoothstep(0.0, 0.9, d);

    float hue = fract(uHue + tunnel * 0.05 + uMid * react * 0.3);
    vec3 col = hsv2rgb(vec3(hue, uSaturation, glow));
    col *= 0.4 + 0.9 * uLevel * react;

    float rings = 0.5 + 0.5 * sin(tunnel * 8.0 - uOnset * react * 4.0);
    col += rings * glow * 0.15 * vec3(0.6, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
