// Burning Ship 3D — the Burning Ship's abs() fold lifted into true 3D:
// a power-2 triplex ("Mandelbulb-style") iteration with every component
// folded through abs() before each squaring, raymarched with a running
// derivative distance estimate. Like Mandelbox (deliberately untouched --
// this is its own preset), the endless fold detail is bounded by
// construction, so ordinary float32 is plenty and no perturbation theory
// is needed. An orbit trap drives the coloring, and bass subtly morphs
// the c-offset so the whole structure breathes with the music.

float ship3dDE(vec3 pos, vec3 c, int iters, out float trap)
{
    vec3 z = pos;
    float dr = 1.0;
    float r = length(z);
    trap = 1.0e9;

    for (int i = 0; i < iters; ++i)
    {
        z = abs(z); // the burning-ship fold, in all three axes
        r = length(z);
        trap = min(trap, r);
        if (r > 2.0)
            break;

        // Power-2 triplex square (spherical-coordinate angle doubling).
        float theta = acos(clamp(z.z / max(r, 1.0e-9), -1.0, 1.0)) * 2.0;
        float phi = atan(z.y, z.x) * 2.0;
        dr = 2.0 * r * dr + 1.0;
        z = (r * r) * vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)) + c;
    }

    return 0.25 * log(max(r, 1.0e-9)) * r / dr;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Bass gently morphs the constant offset -- structural variety with
    // zero precision cost, same philosophy as Mandelbox's animated fold
    // scale.
    vec3 cOffset = vec3(0.0, 0.0, 0.12 * sin(uTime * 0.07)) * (1.0 + uBass * react * 0.5);

    // Slow orbiting camera; Camera Shake (not Reactivity) drives how hard
    // onsets kick it, same reasoning as every other dive-style preset.
    float orbitAngle = uTime * 0.06 * uRotationSpeed;
    float dist = (2.9 + 0.5 * sin(uTime * 0.08)) / max(uCameraScale, 0.05);
    vec3 ro = vec3(sin(orbitAngle) * 0.9, 0.55 + 0.25 * sin(uTime * 0.05), cos(orbitAngle) * 0.9);
    ro = normalize(ro) * dist;
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.7 + right * uv.x + up * uv.y);

    int iters = int(clamp(uIterations * 0.35, 6.0, 14.0));

    float t = 0.0;
    float glow = 0.0;
    float trap = 1.0e9;
    bool hit = false;
    vec3 p = ro;
    for (int n = 0; n < 100; ++n)
    {
        p = ro + rd * t;
        float d = ship3dDE(p, p + cOffset, iters, trap);
        glow += 0.0007 / (0.03 + d * d * 6.0);
        if (d < 0.0012)
        {
            hit = true;
            break;
        }
        t += d * 0.6;
        if (t > 10.0)
            break;
    }

    vec3 col = vec3(0.015, 0.008, 0.025);
    if (hit)
    {
        vec2 e = vec2(0.002, 0.0);
        float trapDummy;
        vec3 n = normalize(vec3(
            ship3dDE(p + e.xyy, p + e.xyy + cOffset, iters, trapDummy)
                - ship3dDE(p - e.xyy, p - e.xyy + cOffset, iters, trapDummy),
            ship3dDE(p + e.yxy, p + e.yxy + cOffset, iters, trapDummy)
                - ship3dDE(p - e.yxy, p - e.yxy + cOffset, iters, trapDummy),
            ship3dDE(p + e.yyx, p + e.yyx + cOffset, iters, trapDummy)
                - ship3dDE(p - e.yyx, p - e.yyx + cOffset, iters, trapDummy)));

        vec3 lightDir = normalize(vec3(0.45, 0.8, -0.35));
        float diff = max(dot(n, lightDir), 0.0);

        // Orbit-trap coloring: how close the orbit swung past the origin
        // maps beautifully onto the palette, giving flame-like gradients
        // that follow the folds.
        float paletteT = trap * 0.8 + uMid * react * 0.5 + uTime * 0.012;
        vec3 base = palette(paletteT, uHue) * (0.45 + 0.55 * diff);
        col = base * (0.7 + uLevel * react * 1.2);
        col += pow(diff, 10.0) * 0.5 * vec3(1.0, 0.85, 0.6);
    }

    // Clamped: near-surface rays can accumulate a lot of steps, and
    // unbounded glow was blowing the whole frame out to white.
    col += min(glow, 1.6) * 0.4 * palette(t * 0.06 + uTime * 0.015, uHue) * (0.5 + react * 0.8);
    col *= 1.0 - smoothstep(5.0, 10.0, t) * 0.75;
    col += uOnset * react * uCameraShake * 0.25 * vec3(0.9, 0.4, 0.1);

    fragColor = vec4(grade(col), 1.0);
}
