// Wireframe Tunnel — a classic demoscene vector-tunnel flythrough: neon
// hoops stacked down the Z axis, connected by longitudinal rails, both
// genuinely thin (radius ~0.02) rather than solid walls -- so between the
// wires there's void, not a lit surface. Almost every ray *grazes* a wire
// instead of hitting one dead-on, so the image is built mostly from the
// glow accumulator's halo rather than direct hit-shading -- exactly the
// soft neon-line look a wireframe tunnel needs.
// Both hoops and rails are torus/capsule primitives, genuine distance
// estimators with no discontinuities, so this is safe to sphere-trace at
// any step size without the overshoot risk a discontinuous field would
// carry (see Shape Rave's header comment for why that distinction matters
// here).

mat2 wfRot(float a) { float s = sin(a), c = cos(a); return mat2(c, -s, s, c); }

float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xy) - t.x, p.z); return length(q) - t.y; }

float sdCapsule(vec3 p, vec3 a, vec3 b, float r)
{
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1.0e-9), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

const float kPeriod = 3.2;
const float kRadius = 1.4;
const int kNumRails = 8;

float tunnelDE(vec3 p, float react)
{
    float twist = (0.3 + uDistortion * 1.5 + uTreble * react * 2.0) * p.z * 0.1;
    p.xy = wfRot(twist) * p.xy;

    float cellZ = mod(p.z, kPeriod) - kPeriod * 0.5;
    vec3 q = vec3(p.xy, cellZ);

    float hoop = sdTorus(q, vec2(kRadius, 0.02));

    float rails = 1.0e5;
    for (int i = 0; i < kNumRails; ++i)
    {
        float ang = (float(i) / float(kNumRails)) * 6.28318530718;
        vec2 dir = vec2(cos(ang), sin(ang)) * kRadius;
        float d = sdCapsule(q, vec3(dir, -kPeriod * 0.5), vec3(dir, kPeriod * 0.5), 0.015);
        rails = min(rails, d);
    }

    return min(hoop, rails);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Smooth, constant travel speed -- Zoom Speed is the only thing that
    // sets the pace. Earlier versions added raw uBass/uOnset kicks
    // directly into camZ and an onset-driven FOV punch, both of which
    // (being fast-fluctuating per-frame values, not integrated motion)
    // read as the flight jittering/surging unevenly rather than a smooth
    // fly-through, per explicit feedback ("lets just do a smooth zoom
    // since its too much").
    float camZ = uTime * (1.4 + 5.0 * max(uZoomSpeed, 0.0));
    vec3 ro = vec3(0.0, 0.0, camZ);
    vec3 rd = normalize(vec3(uv, 1.5 / uCameraScale));
    rd.xy = wfRot(sin(uTime * 0.08) * 0.12 * uRotationSpeed) * rd.xy;

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 90; ++i)
    {
        p = ro + rd * t;
        float d = tunnelDE(p, react);
        glow += 0.0022 / (0.015 + d * d * 6.0);
        if (d < 0.0015)
        {
            hit = true;
            break;
        }
        t += d * 0.75;
        if (t > 45.0)
            break;
    }

    vec3 col = vec3(0.01, 0.008, 0.02);
    vec3 neon = hsv2rgb(vec3(fract(uHue + p.z * 0.015 + uTime * 0.03), uSaturation, 1.0));

    if (hit)
    {
        // Wires are thin enough that a direct hit is close to grazing
        // anyway -- treat it as a bright core rather than doing full
        // normal-based shading, which would need a much finer epsilon to
        // look right on something this thin.
        col += neon * 1.4;
    }

    col += glow * neon * (0.6 + react * 1.3);
    col *= 1.0 - smoothstep(18.0, 45.0, t) * 0.85;
    col += uOnset * react * uCameraShake * 0.3 * vec3(0.5, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
