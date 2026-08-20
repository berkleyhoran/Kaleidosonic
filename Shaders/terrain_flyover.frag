// Terrain Flyover — an infinite hilly landscape flown over from a
// bird's-eye vantage, built from layered value noise (a cheap Perlin-style
// FBM height field) and raymarched as a heightfield: the classic "distance
// = vertical gap to the surface, times a slope-safety factor" heightfield
// approximation, not a general SDF -- exact distance estimation isn't
// needed for a stylized flyover, just something that won't overshoot on
// slopes. Since the terrain is a pure function of world (x, z), flying
// "infinitely" costs nothing extra -- there's no domain-repeat wall to
// hit the way a repeated-cell tunnel would need, the camera just keeps
// sampling noise further and further along its path.
//
// Deliberately calm and NOT audio-reactive (an explicit user call, same
// treatment as Ocean Floor/Water Ripples): straight-line, level flight at
// a fixed altitude, constant hill/ridge shaping, no bass/treble/onset
// terms anywhere. Earlier versions curved the path, banked into turns,
// and tracked the ground's own contour for altitude -- all of that read
// as "wiggling" and "annoying auto camera adjustments" rather than a calm
// flyover, so this version only moves in one straight direction at one
// constant height. Zoom Speed (a manual control, not reactivity) still
// sets how fast it travels.

float hashTF(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

float vnoiseTF(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hashTF(i);
    float b = hashTF(i + vec2(1.0, 0.0));
    float c = hashTF(i + vec2(0.0, 1.0));
    float d = hashTF(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbmTF(vec2 p)
{
    float amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < 5; ++i)
    {
        sum += amp * vnoiseTF(p);
        p = p * 2.02 + vec2(9.1, 3.7);
        amp *= 0.5;
    }
    return sum;
}

float terrainHeight(vec2 xz)
{
    float hillHeight = 1.6;
    float base = fbmTF(xz * 0.15);
    float ridge = fbmTF(xz * 0.5 + 5.0);
    float fine = vnoiseTF(xz * 2.0) * 0.08;
    return (base * 1.6 + ridge * 0.5 - 0.9) * hillHeight + fine;
}

float terrainDE(vec3 p)
{
    // The 0.45 fudge factor is the "slope safety" -- a heightfield's real
    // distance to the nearest surface point is less than the vertical gap
    // wherever the terrain is sloped, so under-estimating by this much
    // keeps the march from ever stepping through a steep hillside.
    return (p.y - terrainHeight(p.xz)) * 0.45;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    // Straight-line, level flight -- Zoom Speed (manual) sets the pace,
    // nothing audio-driven touches the path or the camera at all. Fixed
    // altitude (not terrain-following) so there is genuinely zero
    // vertical motion beyond what the pilot's own forward view naturally
    // shows -- Camera Scale still lets it fly closer/further from the
    // ground overall.
    float speed = 3.0 + 6.0 * clamp(uZoomSpeed, 0.0, 1.0);
    float travel = uTime * speed;

    vec3 ro = vec3(0.0, 5.0 / max(uCameraScale, 0.05), travel);
    vec3 forward = vec3(0.0, 0.0, 1.0);
    vec3 right = vec3(1.0, 0.0, 0.0);
    vec3 up = vec3(0.0, 1.0, 0.0);

    vec3 rd = normalize(forward * 1.7 + right * uv.x + up * uv.y);

    float t = 0.1;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 110; ++i)
    {
        p = ro + rd * t;
        float d = terrainDE(p);
        if (d < 0.01 + t * 0.0006)
        {
            hit = true;
            break;
        }
        t += max(d, 0.03);
        if (t > 260.0)
            break;
    }

    // Sky: a palette-driven gradient with a bright band near the horizon.
    float skyT = clamp(uv.y * 0.7 + 0.35 - rd.y * 0.5, 0.0, 1.0);
    vec3 sky = mix(palette(0.55 + uTime * 0.004, uHue) * 0.55, palette(0.05 + uTime * 0.004, uHue) * 1.0, skyT);

    vec3 col = sky;
    if (hit)
    {
        vec2 e = vec2(0.05, 0.0);
        vec3 n = normalize(vec3(
            terrainDE(p + e.xyy) - terrainDE(p - e.xyy),
            terrainDE(p + e.yxy) - terrainDE(p - e.yxy),
            terrainDE(p + e.yyx) - terrainDE(p - e.yyx)));

        vec3 lightDir = normalize(vec3(0.4, 0.7, -0.4));
        float diff = max(dot(n, lightDir), 0.0);

        // Height-banded terrain color: low = deep green, mid = lighter
        // green/olive, high = rocky grey, tinted a little by the palette
        // for color variety without losing the "hills" read entirely.
        float heightT = clamp((p.y - ro.y + 3.0) / 6.0, 0.0, 1.0);
        vec3 low = vec3(0.08, 0.22, 0.08);
        vec3 mid = vec3(0.20, 0.34, 0.12);
        vec3 high = vec3(0.42, 0.4, 0.38);
        vec3 base = mix(low, mid, smoothstep(0.0, 0.5, heightT));
        base = mix(base, high, smoothstep(0.55, 0.9, heightT));
        base = mix(base, palette(heightT * 0.4 + uTime * 0.01, uHue), 0.18);

        col = base * (0.35 + 0.75 * diff);

        // Distance fog toward the sky color -- the actual "flying through
        // haze" depth cue.
        float fog = smoothstep(30.0, 220.0, t);
        col = mix(col, sky, fog);
    }

    fragColor = vec4(grade(col), 1.0);
}
