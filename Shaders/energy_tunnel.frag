// Energy Tunnel — a kaleidoscopically-folded, pulsing energy-veined
// fractal tunnel the camera flies continuously through: visionary-art
// (Alex Grey-style) sacred-geometry mandala detail, built from the same
// family of bounded iterated fold as Mandelbox (so, like it, genuinely
// infinite detail *by construction* -- no perturbation theory needed) but
// applied to a domain-repeated infinite corridor with radial kaleidoscope
// symmetry folded in before the fractal iteration itself, and a cylinder
// carved out of the middle (a CSG subtraction) so the camera always has a
// clear flight path down the tunnel's own axis while the fractal detail
// radiates as walls all around it -- a genuinely different experience from
// Mandelbox's single static orbited object.

// Folds p around the Z axis into `segments` radial mirror wedges before
// the fractal iteration below ever sees it -- what turns an otherwise
// arbitrary fold pattern into the repeating mandala/sacred-geometry
// symmetry the whole preset is built around. Preserves length(p.xy), so
// the tunnel-carving radius test after this call still measures correctly.
vec3 kaleidoFold3(vec3 p, float segments)
{
    float a = atan(p.y, p.x);
    float r = length(p.xy);
    float seg = 6.28318530718 / max(segments, 1.0);
    a = mod(a, seg);
    a = abs(a - seg * 0.5);
    return vec3(cos(a) * r, sin(a) * r, p.z);
}

float tunnelDE(vec3 p, float scale, float pulse, int iters)
{
    // Infinite corridor: mod-fold Z back into one repeating cell, so the
    // camera flies through an endless stack of identical mandala rings
    // (the same "single wrap, not an iterated one" trick Shape Rave uses
    // for its own infinite field -- no precision cost, the whole endless
    // tunnel costs the same as one cell).
    float cellLen = 3.2;
    p.z = mod(p.z + cellLen * 0.5, cellLen) - cellLen * 0.5;

    p = kaleidoFold3(p, 7.0);
    float axisDist = length(p.xy);

    vec3 z = p;
    float dr = 1.0;
    for (int i = 0; i < iters; ++i)
    {
        z = clamp(z, -1.0, 1.0) * 2.0 - z; // box fold
        float r2 = dot(z, z);
        float minR2 = 0.3 + 0.15 * pulse;
        if (r2 < minR2)
        {
            float t = 1.0 / minR2;
            z *= t;
            dr *= t;
        }
        else if (r2 < 1.0)
        {
            float t = 1.0 / r2;
            z *= t;
            dr *= t;
        }
        z = z * scale + p;
        dr = dr * abs(scale) + 1.0;
    }
    float structureDE = length(z) / abs(dr);

    // Carve a clear flight tube out of the structure (CSG subtraction:
    // max(shape, -tube)) so the camera, which stays near the axis, never
    // hits solid material regardless of what the fractal iteration above
    // does further out.
    float tubeRadius = 1.25;
    return max(structureDE, tubeRadius - axisDist);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Pulse: a strong, fast bass/onset-driven breathing on the fold's own
    // ball-fold radius -- what actually reads as "pulsing" rather than
    // just structurally detailed, the core visionary-art cue this whole
    // preset is built around.
    float pulse = 0.5 + 0.5 * sin(uTime * 1.6);
    pulse = clamp(pulse + uBass * react * 0.6 + uOnset * react * 0.8, 0.0, 1.6);
    float scale = 1.9 + 0.35 * sin(uTime * 0.1) + uBass * react * 0.25;

    // Camera flies continuously forward down the tunnel -- Zoom Speed sets
    // travel speed (always forward: flying backward through a mod-
    // repeated tunnel looks identical, so there's no reason to support
    // negative), Rotation Speed spins the view around the tunnel axis as
    // it travels, so the mandala feels like it's slowly turning around you.
    float travel = uTime * (0.6 + 1.4 * clamp(uZoomSpeed, 0.0, 1.0) + uBass * react * uCameraShake * 0.6);
    float spin = uTime * 0.1 * uRotationSpeed;

    vec3 ro = vec3(0.0, 0.0, travel);
    vec3 forward = vec3(0.0, 0.0, 1.0);
    vec3 right = vec3(cos(spin), sin(spin), 0.0);
    vec3 up = vec3(-sin(spin), cos(spin), 0.0);
    vec3 rd = normalize(forward * 1.5 + right * uv.x + up * uv.y);

    int iters = int(clamp(uIterations * 0.35, 6.0, 14.0));

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 100; ++i)
    {
        p = ro + rd * t;
        float d = tunnelDE(p, scale, pulse, iters);
        if (d > 0.06)
            glow += 0.002 / (0.02 + d * d * 3.0);
        float eps = 0.001 + t * 0.0004;
        if (d < eps)
        {
            hit = true;
            break;
        }
        t += d * 0.6;
        if (t > 22.0)
            break;
    }

    vec3 col = vec3(0.01, 0.005, 0.02);
    if (hit)
    {
        vec2 e = vec2(0.002, 0.0);
        vec3 n = normalize(vec3(
            tunnelDE(p + e.xyy, scale, pulse, iters) - tunnelDE(p - e.xyy, scale, pulse, iters),
            tunnelDE(p + e.yxy, scale, pulse, iters) - tunnelDE(p - e.yxy, scale, pulse, iters),
            tunnelDE(p + e.yyx, scale, pulse, iters) - tunnelDE(p - e.yyx, scale, pulse, iters)));
        vec3 lightDir = normalize(vec3(0.4, 0.6, -0.5));
        float diff = max(dot(n, lightDir), 0.0);
        float paletteT = length(p) * 0.1 + p.z * 0.05 + uTime * 0.03;
        vec3 base = palette(paletteT, uHue) * (0.5 + 0.6 * diff);
        col = base * (0.6 + uLevel * react * 1.0);
        col += pow(diff, 10.0) * 0.6;
    }

    // Energy veins: the glow accumulator, strongly saturated and pulsing,
    // is the actual "radiating energy field" look -- brightened hard on
    // the beat.
    col += min(glow, 1.4) * (0.7 + pulse * 0.6) * palette(t * 0.08 + uTime * 0.02, uHue) * (0.8 + react * 1.4);
    col *= 1.0 - smoothstep(16.0, 22.0, t) * 0.85;
    col += uOnset * react * uCameraShake * 0.3 * palette(uTime * 0.05 + 0.5, uHue);

    fragColor = vec4(grade(col), 1.0);
}
