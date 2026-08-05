// Mandelbox — Tom Lowe's box-fold / ball-fold IFS fractal, raymarched in
// true 3D. Unlike the escape-time presets, its endless nested-box detail
// comes from the iterated fold itself: each iteration reflects z back
// inside a fixed box, then reprojects it based on distance from the
// origin, which keeps every value bounded no matter how deep you look.
// That makes it genuinely infinite-detail *by construction* -- ordinary
// float32 is plenty, no perturbation theory or extended precision needed
// at all, unlike zooming into one fixed point of an escape-time set. The
// fold "scale" is the real complexity knob here (animated continuously,
// pushed by bass/onset) rather than a camera zooming toward a point.

float mandelboxDE(vec3 pos, float scale, float minRadius2, float fixedRadius2, int iters)
{
    vec3 z = pos;
    float dr = 1.0;
    for (int i = 0; i < iters; ++i)
    {
        // Box fold: reflects any component outside [-1,1] back inside.
        z = clamp(z, -1.0, 1.0) * 2.0 - z;

        // Ball fold: reprojects z based on distance from the origin --
        // this is what keeps the whole iteration bounded (and precision-
        // safe) regardless of scale or iteration count.
        float r2 = dot(z, z);
        if (r2 < minRadius2)
        {
            float t = fixedRadius2 / minRadius2;
            z *= t;
            dr *= t;
        }
        else if (r2 < fixedRadius2)
        {
            float t = fixedRadius2 / r2;
            z *= t;
            dr *= t;
        }

        z = z * scale + pos;
        dr = dr * abs(scale) + 1.0;
    }
    return length(z) / abs(dr);
}

mat2 rot2(float a)
{
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Fold scale animates continuously -- this is what gives endless-
    // feeling structural variety with zero precision cost, since it's not
    // a camera position that needs to stay pinned on one point. Bass
    // pushes it harder; onset gives it a kick.
    float scale = 2.2 + 0.7 * sin(uTime * 0.08 * (0.4 + max(uZoomSpeed, 0.0)))
                + uBass * uCameraShake * 0.5 + uOnset * uCameraShake * 0.3;

    // Camera slowly orbits and breathes in/out. Driven by Camera Shake for
    // motion, kept off Reactivity -- see any other dive-style preset's
    // comment here for why (fast Reactivity + camera motion together reads
    // as nauseating rather than exciting).
    float orbitAngle = uTime * 0.05 * uRotationSpeed;

    // The mandelbox structure extends much further out than intuition
    // suggests (roughly +-6 at fold scale 2, more at lower scales), and
    // the original base distance of ~3.2 put the camera *inside* it at
    // default Camera Scale -- rendering a solid color, since every ray
    // "hits" at its first step. The view everyone actually liked was the
    // whole box from well outside (~16 units, which the old code only
    // reached with Camera Scale cranked down to 0.2) -- make that the
    // default, and floor the distance so no Camera Scale setting can
    // shove the camera back inside.
    float dist = (16.0 + 3.0 * sin(uTime * 0.09)) / max(uCameraScale, 0.05);
    dist = max(dist, 10.0);
    vec3 ro = vec3(sin(orbitAngle), 0.35 * sin(uTime * 0.06), cos(orbitAngle)) * dist;
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.6 + right * uv.x + up * uv.y);

    float minRadius2 = 0.25;
    float fixedRadius2 = 1.0;
    int iters = int(clamp(uIterations, 4.0, 16.0));

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    for (int n = 0; n < 90; ++n)
    {
        p = ro + rd * t;
        float d = mandelboxDE(p, scale, minRadius2, fixedRadius2, iters);
        glow += 0.0025 / (0.02 + d * d * 3.0);
        if (d < 0.0015)
        {
            hit = true;
            break;
        }
        t += d * 0.55;
        if (t > 34.0) // far enough to cover the whole structure from the outside camera
            break;
    }

    vec3 col = vec3(0.02, 0.015, 0.04);
    if (hit)
    {
        vec2 e = vec2(0.0025, 0.0);
        vec3 n = normalize(vec3(
            mandelboxDE(p + e.xyy, scale, minRadius2, fixedRadius2, iters)
                - mandelboxDE(p - e.xyy, scale, minRadius2, fixedRadius2, iters),
            mandelboxDE(p + e.yxy, scale, minRadius2, fixedRadius2, iters)
                - mandelboxDE(p - e.yxy, scale, minRadius2, fixedRadius2, iters),
            mandelboxDE(p + e.yyx, scale, minRadius2, fixedRadius2, iters)
                - mandelboxDE(p - e.yyx, scale, minRadius2, fixedRadius2, iters)));
        vec3 lightDir = normalize(vec3(0.5, 0.7, -0.4));
        float diff = max(dot(n, lightDir), 0.0);
        float paletteT = length(p) * 0.15 + uMid * react * 0.6 + uTime * 0.015;
        vec3 base = palette(paletteT, uHue) * (0.5 + 0.5 * diff);
        col = base * (0.7 + uLevel * react * 1.2);
        col += pow(diff, 8.0) * 0.6;
    }

    // Clamped: from the outside camera, rays grazing the structure crawl
    // through many small-DE steps and the unbounded sum was blowing the
    // whole frame out to white.
    col += min(glow, 1.2) * 0.5 * palette(t * 0.05 + uTime * 0.02, uHue) * (0.7 + react * 1.3);
    col *= 1.0 - smoothstep(14.0, 34.0, t) * 0.75; // depth fog, matched to the farther camera
    col += uOnset * react * uCameraShake * 0.3 * vec3(0.6, 0.4, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
