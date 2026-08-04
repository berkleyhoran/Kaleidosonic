// Particle Bloom — a procedural field of glowing particles (no geometry,
// just per-pixel accumulated glow), orbiting, pulsing, and exploding
// outward with the audio. Folded through the kaleidoscope for a symmetric
// mandala of particles.

float hash1(float n) { return fract(sin(n) * 43758.5453123); }

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;
    uv = rotate2d(uTime * 0.04 * uRotationSpeed) * uv;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    float react = uReactivity;
    vec3 col = vec3(0.015, 0.01, 0.03);

    const int numParticles = 44;
    for (int i = 0; i < numParticles; ++i)
    {
        float seed = float(i);
        float angle0 = hash1(seed) * 6.28318530718;
        float speed = mix(0.15, 0.6, hash1(seed + 11.0)) * (0.4 + uMid * react * 2.0);
        float radius = mix(0.08, 0.85, hash1(seed + 22.0));
        radius += 0.2 * sin(uTime * 0.5 + seed) * (0.3 + uBass * react * 1.5);
        // Kick particles outward hard on every beat.
        radius *= 1.0 + uOnset * react * 0.6;

        float angle = angle0 + uTime * speed * (hash1(seed + 33.0) > 0.5 ? 1.0 : -1.0);
        vec2 pos = radius * vec2(cos(angle), sin(angle));

        float baseSize = mix(0.006, 0.02, hash1(seed + 44.0));
        float size = baseSize * (1.0 + uOnset * react * 5.0 + uLevel * react * 2.0);

        float d = length(uv - pos);
        float g = size / (d * d / size + size);

        float hue = fract(uHue + seed * 0.11 + uTime * 0.03 + uTreble * react * 0.7);
        vec3 pc = hsv2rgb(vec3(hue, uSaturation, 1.0));
        col += pc * g * (0.5 + 0.9 * uLevel * react);
    }

    col += uOnset * react * 0.18 * vec3(1.0, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
