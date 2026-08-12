// Shape Rave — a raymarched, domain-repeated field of floating shapes
// (rounded boxes, tori, spheres) extending infinitely in every direction.
// Domain repetition (`mod` folding the sample point back into one cell,
// like Mandelbox's box-fold but simpler -- a single wrap instead of an
// iterated one) means the whole endless field costs the same as a single
// cell: no precision machinery needed at all, the same way Mandelbox and
// Burning Ship 3D get endless detail for free from their fold instead of
// from deep zoom. The camera flies continuously through the field; each
// shape bobs on its own phase and scale-pulses on the beat.
//
// The flight path has no inherent guarantee it avoids any given cell's
// shape (jitter can place one anywhere in its cell). Two things were
// tried and discarded before this one: steering the camera away from
// whatever it was about to hit (reads as the camera visibly, mechanically
// dodging -- not a natural flight path), and fading a nearby shape's
// distance value toward "very far away" as the camera approached (looks
// right in principle, but *inflating* the value a raymarcher uses for its
// own step size is a real raymarching mistake -- a step near the camera
// suddenly sized for a "shape" 25 units away massively overshoots
// whatever real geometry is actually close by, which is exactly what
// produced the checkerboard-y noise). The fix that's actually correct for
// a raymarcher: carve a literal tube-shaped void out of the whole field
// along the flight path, via ordinary CSG subtraction (`max(shape, -void)`,
// the standard, safe way to remove one shape from another in a distance
// field -- it never touches step-size validity). The path is
// simply always clear, by construction, everywhere along its length --
// nothing to fade, nothing to dodge, and nothing shape-specific to break.

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
const float kTubeRadius = 1.7; // comfortably past the largest shape's ~0.8 radius plus flight margin

// The flight path's own (x, y) at a given z/travel -- kept as a function
// so shapeField can carve clearance around it without needing the
// camera's instantaneous position at all. Must match main()'s `ro`.
vec2 flightPathXY(float z)
{
    return vec2(sin(z * 0.11) * 3.0, cos(z * 0.09) * 2.0);
}

// Distance to the nearest shape, in a cell-local frame, with the flight
// path's tube subtracted out. cellId (out) is the repeated cell's integer
// coordinate, used to pick a stable per-cell hue/shape/phase.
float shapeField(vec3 p, float pulse, out vec3 cellId)
{
    vec3 cell = floor(p / kCell + 0.5);
    cellId = cell;

    vec3 jitter = (hash33(cell) - 0.5) * (kCell * 0.5);
    vec3 shapeCenter = cell * kCell + jitter;
    vec3 local = p - shapeCenter;

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

    d *= scale;

    float tubeSdf = length(p.xy - flightPathXY(p.z)) - kTubeRadius; // negative inside the tube
    return max(d, -tubeSdf);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Continuous forward flight, path gently weaving; onset gives the
    // whole camera a kick, like the field is thumping. uTime itself
    // already wraps every ~1000s (see common.glsl), which bounds travel
    // to a range comfortably inside float32's precise-integer span
    // regardless of session length.
    float speed = 1.4 + 1.6 * clamp(uZoomSpeed, 0.0, 1.0);
    float travel = uTime * speed;
    vec3 ro = vec3(flightPathXY(travel), travel);
    vec3 lookAt = vec3(flightPathXY(travel + 4.0), travel + 4.0);

    vec3 forward = normalize(lookAt - ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    float roll = uTime * 0.05 * uRotationSpeed;
    right = right * cos(roll) + up * sin(roll);
    up = cross(forward, right);
    vec3 rd = normalize(forward * 1.5 + right * uv.x + up * uv.y);

    float pulse = uBass * react * 0.6 + uOnset * react * uCameraShake * 0.8;

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    vec3 hitCell = vec3(0.0);
    float lastD = 1.0e9;
    for (int i = 0; i < 140; ++i)
    {
        p = ro + rd * t;
        vec3 cellId;
        float d = shapeField(p, pulse, cellId);
        hitCell = cellId;
        lastD = d;

        // Glow only accumulates while comfortably clear of any surface.
        // Ungated, the many tiny near-surface steps a converging ray takes
        // right before landing were adding a per-pixel-varying halo ON the
        // surface itself -- the ripply "fingerprint" contour texture on
        // close shapes, since neighboring pixels' rays take different
        // near-surface step counts.
        if (d > 0.15)
            glow += min(0.0012 / (0.02 + d * d * 3.0), 0.05);

        // Hit tolerance relaxes with distance along the ray (standard
        // pixel-footprint scaling): silhouette-grazing rays converge and
        // shade instead of running out of iterations and speckling.
        float eps = 0.0012 + t * 0.0007;
        if (d < eps)
        {
            hit = true;
            break;
        }
        t += d * 0.8;
        if (t > 60.0)
            break;
    }
    // Ran out of iterations while nearly touching a surface (grazing
    // silhouettes do this): shade it as a hit rather than leaving a
    // background-colored speckle punched into the edge.
    if (! hit && lastD < 0.06)
        hit = true;

    vec3 col = vec3(0.01, 0.01, 0.02);
    if (hit)
    {
        vec2 e = vec2(0.0025, 0.0);
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
