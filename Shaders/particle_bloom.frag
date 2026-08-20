// Particle Bloom — a procedural field of glowing particles (no geometry,
// just per-pixel accumulated glow), orbiting, pulsing, and exploding
// outward with the audio. Folded through the kaleidoscope for a symmetric
// mandala of particles. When an image (or GIF) is loaded, each particle
// picks up its color from whatever the picture shows at that particle's
// own orbit position instead of the procedural hue sweep -- particles
// outside the picture's bounds (or with no image loaded at all) still
// fall back to the hue sweep, so it degrades gracefully either way.
//
// Particle Density/Size (uParticleDensity/uParticleSize) let this scale
// beyond the original fixed count/size. The loop bound itself has to stay
// a fixed compile-time constant (GLSL requirement) -- kNumParticlesMax is
// the real ceiling, and Density (0..2, default 1) picks how many of those
// are actually drawn each frame via an early break, the same pattern
// Water Ripples' drop density already established. Default Density=1
// reproduces the original 44-particle look exactly (kNumParticlesMax is
// double that specifically so Density has headroom to go both below and
// above the original count).

float hash1(float n) { return fract(sin(n) * 43758.5453123); }

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;
    uv = rotate2d(uTime * 0.04 * uRotationSpeed) * uv;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    float react = uReactivity;
    vec3 col = vec3(0.015, 0.01, 0.03);

    const int kNumParticlesMax = 88;
    int activeParticles = int(float(kNumParticlesMax) * clamp(uParticleDensity, 0.0, 2.0) * 0.5 + 0.5);
    for (int i = 0; i < kNumParticlesMax; ++i)
    {
        if (i >= activeParticles)
            break;
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
        float size = baseSize * (1.0 + uOnset * react * 5.0 + uLevel * react * 2.0) * uParticleSize;

        float d = length(uv - pos);
        float g = size / (d * d / size + size);

        float hue = fract(uHue + seed * 0.11 + uTime * 0.03 + uTreble * react * 0.7);
        vec3 pc = hsv2rgb(vec3(hue, uSaturation, 1.0));
        if (uUserImageLoaded > 0.5)
        {
            vec4 imgC = sampleUserImage(imageContainUV(pos, uUserImageAspect));
            pc = mix(pc, imgC.rgb, imgC.a);
        }
        col += pc * g * (0.5 + 0.9 * uLevel * react);
    }

    col += uOnset * react * 0.18 * vec3(1.0, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
