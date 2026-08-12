// Image Shatter — the uploaded picture broken into a voronoi field of
// glass shards (see common.glsl's sampleUserImage). At rest the shards
// sit flush and the image reads whole; every onset kicks each shard along
// its own random direction, and since uOnset is already an eased/decaying
// envelope (see VisualizerRenderer's onsetEnvelope), the shards spring
// back to resting position on their own with no extra easing needed here.
// Distortion controls shard density (coarse chunks to fine mosaic). Shows
// a pulsing placeholder until an image is actually loaded.

vec2 shatterHash2(vec2 p)
{
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453123);
}

// Standard 2nd-nearest-neighbor voronoi: f1/f2 give both the nearest
// feature point's distance (for the per-shard ID/offset) and the gap to
// the second-nearest (for crisp crack lines between shards).
void voronoi(vec2 uv, out vec2 id, out float f1, out float f2)
{
    vec2 base = floor(uv);
    f1 = 8.0;
    f2 = 8.0;
    id = base;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 neighbor = base + vec2(float(x), float(y));
            vec2 point = neighbor + shatterHash2(neighbor);
            float d = length(point - uv);
            if (d < f1)
            {
                f2 = f1;
                f1 = d;
                id = neighbor;
            }
            else if (d < f2)
            {
                f2 = d;
            }
        }
    }
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
    float cellScale = mix(5.0, 22.0, clamp(uDistortion, 0.0, 1.0));
    vec2 gridUV = screenUV * cellScale;

    vec2 id;
    float f1, f2;
    voronoi(gridUV, id, f1, f2);

    vec2 shardDir = normalize(shatterHash2(id) - 0.5 + 1.0e-4);
    float kick = uOnset * react * (0.35 + uBass * react * 0.35);
    vec2 offset = shardDir * kick * 0.12;

    vec2 imgUV = imageContainUV(screenUV + offset, uUserImageAspect);
    vec4 img = sampleUserImage(imgUV);

    // Crack line: darkens the last sliver of each cell closest to its
    // neighbor, at a width that stays constant in screen space regardless
    // of cellScale.
    float edge = smoothstep(0.0, 0.05, (f2 - f1) * cellScale * 0.15 + 0.05);

    vec3 background = palette(f1 + uTime * 0.02, uHue) * 0.1;
    vec3 col = mix(background, img.rgb, img.a) * mix(0.15, 1.0, edge);
    col *= 0.88 + uLevel * react * 0.28;
    col += uOnset * react * 0.06 * palette(uTime * 0.03, uHue);

    fragColor = vec4(grade(col), 1.0);
}
