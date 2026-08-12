// Shape Rave — a raymarched, domain-repeated field of floating shapes
// (rounded boxes, tori, spheres) extending infinitely in every direction.
// Domain repetition (`mod` folding the sample point back into one cell,
// like Mandelbox's box-fold but simpler -- a single wrap instead of an
// iterated one) means the whole endless field costs the same as a single
// cell: no precision machinery needed at all, the same way Mandelbox and
// Burning Ship 3D get endless detail for free from their fold instead of
// from deep zoom. The camera flies continuously through the field; each
// shape bobs on its own phase and scale-pulses on the beat.

float hash13(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

vec3 hash33(vec3 p)
{
    return vec3(hash13(p), hash13(p + 19.19), hash13(p + 71.71));
}

float sdRoundBox(vec3 p, vec3 b, float r)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdTorus(vec3 p, vec2 t)
{
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sdSphere(vec3 p, float r)
{
    return length(p) - r;
}

const float kCell = 3.0;

// Distance to the nearest shape, in a cell-local frame. cellId (out) is
// the repeated cell's integer coordinate, used to pick a stable per-cell
// hue/shape/phase.
float shapeField(vec3 p, float pulse, out vec3 cellId)
{
    vec3 cell = floor(p / kCell + 0.5);
    cellId = cell;
    vec3 local = p - cell * kCell;

    vec3 jitter = (hash33(cell) - 0.5) * (kCell * 0.5);
    local -= jitter;

    float bob = sin(uTime * 0.6 + hash13(cell) * 30.0) * 0.35;
    local.y -= bob;

    float scale = 0.55 + 0.35 * hash13(cell + 5.0) + pulse * 0.4;
    local /= scale;

    float shapeType = hash13(cell + 11.0);
    float d;
    if (shapeType < 0.34)
        d = sdRoundBox(local, vec3(0.55), 0.12);
    else if (shapeType < 0.67)
        d = sdTorus(local, vec2(0.55, 0.22));
    else
        d = sdSphere(local, 0.6);

    return d * scale;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Continuous forward flight, path gently weaving; onset gives the
    // whole camera a kick, like the field is thumping.
    float speed = 1.4 + 1.6 * clamp(uZoomSpeed, 0.0, 1.0);
    float travel = uTime * speed;
    vec3 ro = vec3(sin(travel * 0.11) * 3.0, cos(travel * 0.09) * 2.0, travel);
    vec3 lookAt = ro + vec3(sin((travel + 4.0) * 0.11) * 3.0 - sin(travel * 0.11) * 3.0,
                             cos((travel + 4.0) * 0.09) * 2.0 - cos(travel * 0.09) * 2.0, 4.0);

    vec3 forward = normalize(lookAt - ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    float roll = uTime * 0.05 * uRotationSpeed;
    right = right * cos(roll) + up * sin(roll);
    up = cross(forward, right);
    vec3 rd = normalize(forward * 1.5 + right * uv.x + up * uv.y);

    float pulse = uBass * react * 0.6 + uOnset * react * uCameraShake * 0.8;

    // The flight path weaves through the field with no guarantee it
    // avoids any given cell's shape (jitter can place a shape anywhere
    // in its cell) -- without this, the camera would periodically fly
    // straight into one, which reads as a checkerboard/black glitch
    // (raymarching from inside or right at a surface is numerically
    // unstable: the step size collapses toward zero and the glow term
    // below spikes). Push the camera away from the nearest surface along
    // its gradient before marching, so the ray always starts in clear
    // space regardless of how the path happens to weave.
    {
        vec3 dummyCell;
        for (int k = 0; k < 3; ++k)
        {
            float dHere = shapeField(ro, pulse, dummyCell);
            if (dHere > 0.9)
                break;
            vec2 e2 = vec2(0.01, 0.0);
            vec3 grad = normalize(vec3(
                shapeField(ro + e2.xyy, pulse, dummyCell) - shapeField(ro - e2.xyy, pulse, dummyCell),
                shapeField(ro + e2.yxy, pulse, dummyCell) - shapeField(ro - e2.yxy, pulse, dummyCell),
                shapeField(ro + e2.yyx, pulse, dummyCell) - shapeField(ro - e2.yyx, pulse, dummyCell)) + 1.0e-4);
            ro += grad * (0.9 - dHere);
        }
    }

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    vec3 hitCell = vec3(0.0);
    for (int i = 0; i < 110; ++i)
    {
        p = ro + rd * t;
        vec3 cellId;
        float d = shapeField(p, pulse, cellId);
        if (d < 0.0)
        {
            // Grazed inside a surface despite the avoidance above (e.g. a
            // shape bobbed/pulsed into the ray after the push) -- treat
            // as an immediate hit rather than let the marcher bounce
            // around near zero, which is what actually produced the
            // checkerboard artifacting.
            hit = true;
            hitCell = cellId;
            break;
        }
        glow += min(0.0015 / (0.02 + d * d * 3.0), 0.06);
        if (d < 0.0015)
        {
            hit = true;
            hitCell = cellId;
            break;
        }
        t += d * 0.7;
        if (t > 60.0)
            break;
    }

    vec3 col = vec3(0.01, 0.01, 0.02);
    if (hit)
    {
        vec2 e = vec2(0.002, 0.0);
        vec3 dummy;
        vec3 n = normalize(vec3(
            shapeField(p + e.xyy, pulse, dummy) - shapeField(p - e.xyy, pulse, dummy),
            shapeField(p + e.yxy, pulse, dummy) - shapeField(p - e.yxy, pulse, dummy),
            shapeField(p + e.yyx, pulse, dummy) - shapeField(p - e.yyx, pulse, dummy)));

        vec3 lightDir = normalize(vec3(0.4, 0.8, -0.3));
        float diff = max(dot(n, lightDir), 0.0);
        float paletteT = hash13(hitCell) + uTreble * react * 0.3 + uTime * 0.02;
        vec3 base = palette(paletteT, uHue) * (0.4 + 0.6 * diff);
        col = base * (0.7 + uLevel * react * 1.1);
        col += pow(diff, 10.0) * 0.5;
    }

    col += glow * palette(t * 0.03 + uTime * 0.015, uHue) * (0.6 + react * 1.1);
    col *= 1.0 - smoothstep(35.0, 60.0, t) * 0.8;
    col += uOnset * react * uCameraShake * 0.25 * vec3(0.8, 0.5, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
