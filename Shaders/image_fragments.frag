// Image Fragments — the uploaded picture broken into a regular grid of
// square fragments, each one continuously drifting, spinning, and
// breathing in/out of the picture plane on its own independent orbit
// (seeded per-cell) rather than sitting flush until struck like Image
// Shatter's onset-triggered spring-back shards -- a "shattered photo
// floating in zero gravity" read instead of a "cracked glass" one.
// Distortion controls fragment density (coarse chunks to fine mosaic).
// Shows a pulsing placeholder until an image is actually loaded.

vec2 fragHash2(vec2 p)
{
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453123);
}

void main()
{
    vec2 screenUV = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;

    if (uUserImageLoaded < 0.5)
    {
        fragColor = vec4(grade(imagePlaceholder(screenUV)), 1.0);
        return;
    }

    float react = uReactivity;
    vec2 imgUV = imageContainUV(screenUV, uUserImageAspect);

    float cellScale = mix(4.0, 14.0, clamp(uDistortion, 0.0, 1.0));
    vec2 gridUV = imgUV * cellScale;
    vec2 cellId = floor(gridUV);
    vec2 cellCenter = (cellId + 0.5) / cellScale;
    vec2 cellLocal = imgUV - cellCenter;

    vec2 seed = fragHash2(cellId);
    float phase = seed.x * 6.28318530718;
    float speed = mix(0.3, 0.9, seed.y) * (0.4 + uMid * react * 0.6);
    float orbitR = mix(0.004, 0.02, seed.x) * (0.5 + uBass * react * 1.5);

    // Each fragment drifts around its own home cell center in a small
    // continuous orbit, spins slowly, and breathes in scale with bass --
    // all time-driven rather than onset-gated, so it reads as floating
    // debris rather than a struck-and-settling break.
    vec2 orbitOffset = vec2(cos(uTime * speed + phase), sin(uTime * speed * 1.3 + phase)) * orbitR;
    float spin = uTime * (0.15 + seed.y * 0.4) * (0.4 + abs(uRotationSpeed)) + phase;
    float scale = 1.0 - (0.06 + uBass * react * 0.1) * (0.5 + 0.5 * sin(uTime * speed * 0.7 + phase));

    vec2 rotatedLocal = rotate2d(spin) * (cellLocal / max(scale, 0.2));
    vec2 sampleUV = cellCenter + rotatedLocal - orbitOffset;

    vec4 img = sampleUserImage(sampleUV);

    // Gap between fragments: widens toward each cell's own edge -- a cheap
    // fake "floating apart" darkening (real screen-space gaps would need
    // actually translating each fragment's on-screen position, which isn't
    // feasible per-pixel without inverse-mapping every neighbor cell).
    vec2 edgeDist = 0.5 / cellScale - abs(cellLocal);
    float gap = smoothstep(0.0, 0.06 / cellScale, min(edgeDist.x, edgeDist.y));

    vec3 background = palette(length(cellId) * 0.1 + uTime * 0.02, uHue) * 0.08;
    vec3 col = mix(background, img.rgb, img.a * gap);
    col *= 0.86 + uLevel * react * 0.3;
    col += uOnset * react * 0.05 * palette(uTime * 0.03 + seed.x, uHue);

    fragColor = vec4(grade(col), 1.0);
}
