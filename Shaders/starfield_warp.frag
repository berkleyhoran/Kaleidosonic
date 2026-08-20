// Starfield Warp — flying through stars at warp speed, radiating outward
// from center. Cheap, punchy, and a deliberate change of pace from the
// fractal-heavy presets. Bass and onsets slam the warp speed. When an
// image (or GIF) is loaded, each star's color comes from the picture at
// that star's fixed launch direction instead of the hue sweep -- since
// the direction (not the animated depth) is what stays constant per
// star, it reads as flying through the photo itself.
//
// Particle Density/Size (uParticleDensity/uParticleSize) scale the star
// count/size -- see Particle Bloom's header comment for why the loop
// bound stays a fixed compile-time constant with an early break instead.
// Default Density=1 reproduces the original 90-star look exactly.

float hash1(float n) { return fract(sin(n) * 43758.5453123); }

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;
    float react = uReactivity;

    // Warp speed is driven by Camera Shake, not Reactivity -- see
    // Plasma Feedback's comment for why.
    float speed = (0.3 + 1.5 * max(uZoomSpeed, 0.0)) * (1.0 + uBass * uCameraShake * 2.5 + uOnset * uCameraShake * 2.0);
    float t = uTime * speed;

    vec3 col = vec3(0.0);

    const int kNumStarsMax = 180;
    int activeStars = int(float(kNumStarsMax) * clamp(uParticleDensity, 0.0, 2.0) * 0.5 + 0.5);
    for (int i = 0; i < kNumStarsMax; ++i)
    {
        if (i >= activeStars)
            break;
        float seed = float(i) * 13.7 + 1.0;
        float angle = hash1(seed) * 6.2831853 + uZoomWander * 0.3 * sin(uTime * 0.05 + seed);
        float radiusSeed = hash1(seed + 91.0);

        float z = mod(radiusSeed * 20.0 - t * (0.5 + hash1(seed + 5.0)), 20.0) + 0.05;
        vec2 dir = vec2(cos(angle), sin(angle));
        vec2 pos = dir * (1.0 / z) * 0.5;

        float size = (0.004 + 0.012 / z) * (1.0 + uOnset * react * 1.6) * uParticleSize;
        float d = length(uv - pos);
        float star = size / (d + size * 0.3) - 1.0;
        star = max(star, 0.0);

        float hue = fract(uHue + hash1(seed + 3.0) * 0.3 + uTime * 0.02);
        vec3 sc = hsv2rgb(vec3(hue, uSaturation * 0.5, 1.0));
        if (uUserImageLoaded > 0.5)
        {
            vec4 imgC = sampleUserImage(imageContainUV(dir * 0.5, uUserImageAspect));
            sc = mix(sc, imgC.rgb, imgC.a);
        }
        float streak = clamp(1.0 / (z * z), 0.0, 3.0);
        col += sc * star * (0.5 + streak * 0.4) * (0.5 + uLevel * react);
    }

    col *= 0.8 + uLevel * react * 0.7;
    col += uOnset * react * 0.05 * vec3(0.7, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
