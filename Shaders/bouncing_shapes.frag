// Bouncing Shapes — simple 2D shapes (circles, rounded squares, triangles)
// that genuinely bounce off the screen's actual edges AND off each other,
// via a real CPU-side elastic-collision simulation (see
// VisualizerRenderer::updateBounceShapes and common.glsl's
// uBounceState/kNumBounceShapes) -- pairwise collision can't be a pure
// function of uTime the way every other preset's motion is, since where
// two shapes actually collide depends on their whole prior history. This
// shader is purely a renderer for that state: it texelFetch()es each
// shape's position/size/type and draws them smooth-min blended together
// (with a bright flash right at the moment of contact), same visual
// treatment the earlier time-only version used. Fully 2D (no raymarch).

float hashS(float n) { return fract(sin(n) * 43758.5453123); }

float sdBoxS(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float sdTriS(vec2 p, float r)
{
    const float k = 1.7320508; // sqrt(3)
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0)
        p = vec2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

float sminS(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

void fetchShape(int i, out vec2 pos, out float size, out int shapeType)
{
    vec4 s = texelFetch(uBounceState, ivec2(i, 0), 0);
    pos = s.xy;
    size = s.z;
    shapeType = int(s.w + 0.5);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    vec3 col = vec3(0.012, 0.014, 0.024);

    vec2 positions[kNumBounceShapes];
    float sizes[kNumBounceShapes];

    float field = 1.0e5;
    float nearestIdx = -1.0;
    for (int i = 0; i < kNumBounceShapes; ++i)
    {
        vec2 pos;
        float size;
        int shapeType;
        fetchShape(i, pos, size, shapeType);
        positions[i] = pos;
        sizes[i] = size;

        float spin = uTime * (0.3 + 0.5 * hashS(float(i))) * (0.4 + abs(uRotationSpeed));
        vec2 p = rotate2d(spin) * (uv - pos);

        float d;
        if (shapeType == 0)
            d = length(p) - size;
        else if (shapeType == 1)
            d = sdBoxS(p, vec2(size * 0.8), size * 0.25);
        else
            d = sdTriS(p, size);

        if (d < field)
            nearestIdx = float(i);
        field = sminS(field, d, 0.05);
    }

    if (field < 0.03)
    {
        float depth = clamp(-field * 6.0, 0.0, 1.0);
        vec3 base = nearestIdx >= 0.0 ? palette(nearestIdx / float(kNumBounceShapes) + uTime * 0.015, uHue) : vec3(1.0);
        col = mix(col, base * mix(0.4, 1.0, depth), smoothstep(0.02, -0.01, field));

        float rim = smoothstep(0.05, 0.0, abs(field));
        col += vec3(1.0) * rim * 0.4 * (0.6 + uTreble * react * 0.6);
    }

    // Contact flash: real proximity from the actual collision simulation
    // above (not a guess), so this genuinely lights up right at each
    // impact.
    for (int i = 0; i < kNumBounceShapes; ++i)
    {
        for (int j = i + 1; j < kNumBounceShapes; ++j)
        {
            float gap = length(positions[i] - positions[j]) - (sizes[i] + sizes[j]);
            float contact = smoothstep(0.05, -0.02, gap);
            if (contact > 0.01)
            {
                vec2 mid = (positions[i] + positions[j]) * 0.5;
                float glow = contact * smoothstep(0.25, 0.0, length(uv - mid));
                col += glow * vec3(1.0, 0.95, 0.8) * (0.5 + uOnset * react * 1.0);
            }
        }
    }

    col *= 0.85 + uLevel * react * 0.35;
    col += uOnset * react * uCameraShake * 0.1 * vec3(0.8, 0.7, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
