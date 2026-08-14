// Crystal Cave — a fixed cluster of faceted crystal shards (rotated,
// stretched boxes -- their flat faces and sharp edges give a genuinely
// faceted look for free, no extra geometry trick needed), camera orbiting
// slowly around the whole formation. Each facet's color shifts by which
// way its flat face points (the classic "gem" cheat: tint by abs(normal)),
// layered over a palette base and a strong specular glint, so the cluster
// reads as cut crystal rather than plain colored glass. Onset strobes a
// bright flash across every facet at once, like a camera flash catching
// the whole cluster.

mat2 ccRot(float a) { float s = sin(a), c = cos(a); return mat2(c, -s, s, c); }

float hash1(float n) { return fract(sin(n) * 43758.5453123); }

float sdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

const int kNumShards = 11;

// Per-shard transform, fixed (hashed) but for the slow shared spin below --
// a stable cluster, not a flythrough field.
float shardDE(vec3 p, int i, float react, out vec3 hue)
{
    float seed = float(i) * 9.7 + 3.0;
    vec3 center = vec3(hash1(seed) - 0.5, hash1(seed + 1.0) - 0.5, hash1(seed + 2.0) - 0.5) * 2.6;
    vec3 q = p - center;

    float spin = uTime * 0.15 * (0.3 + abs(uRotationSpeed)) * (hash1(seed + 3.0) > 0.5 ? 1.0 : -1.0);
    q.xz = ccRot(spin + hash1(seed + 4.0) * 6.2831853) * q.xz;
    q.xy = ccRot(hash1(seed + 5.0) * 3.14159) * q.xy;

    float pulse = 1.0 + uBass * react * 0.25 + uOnset * react * uCameraShake * 0.4;
    vec3 halfExtent = vec3(mix(0.08, 0.16, hash1(seed + 6.0)), mix(0.08, 0.16, hash1(seed + 7.0)),
                            mix(0.35, 0.75, hash1(seed + 8.0)))
                    * pulse;

    hue = palette(hash1(seed + 9.0) + uTime * 0.02, uHue);
    return sdBox(q, halfExtent);
}

float sceneDE(vec3 p, float react)
{
    float d = 1.0e5;
    for (int i = 0; i < kNumShards; ++i)
    {
        vec3 hue;
        d = min(d, shardDE(p, i, react, hue));
    }
    return d;
}

vec3 sceneNormal(vec3 p, float react)
{
    vec2 e = vec2(0.0015, 0.0);
    return normalize(vec3(
        sceneDE(p + e.xyy, react) - sceneDE(p - e.xyy, react),
        sceneDE(p + e.yxy, react) - sceneDE(p - e.yxy, react),
        sceneDE(p + e.yyx, react) - sceneDE(p - e.yyx, react)));
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    float orbitAngle = uTime * 0.1 * (0.3 + abs(uRotationSpeed));
    float dist = 6.5 / max(uCameraScale, 0.05);
    vec3 ro = vec3(sin(orbitAngle), 0.3 * sin(uTime * 0.06), cos(orbitAngle)) * dist;
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.8 + right * uv.x + up * uv.y);

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    int hitShard = 0;
    vec3 p = ro;
    for (int i = 0; i < 100; ++i)
    {
        p = ro + rd * t;
        float d = sceneDE(p, react);
        if (d > 0.08)
            glow += 0.0014 / (0.02 + d * d * 4.0);
        float eps = 0.0012 + t * 0.0006;
        if (d < eps)
        {
            hit = true;
            break;
        }
        t += d * 0.85;
        if (t > 22.0)
            break;
    }

    vec3 col = vec3(0.012, 0.008, 0.02);
    if (hit)
    {
        vec3 n = sceneNormal(p, react);
        vec3 lightDir = normalize(vec3(0.5, 0.8, -0.3));
        float diff = max(dot(n, lightDir), 0.0);
        float spec = pow(max(dot(reflect(-lightDir, n), -rd), 0.0), 32.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 2.5);

        // Find whichever shard actually owns this surface point, for its
        // hue -- a second, cheap pass over the same small shard count.
        float best = 1.0e5;
        vec3 hue = vec3(1.0);
        for (int i = 0; i < kNumShards; ++i)
        {
            vec3 h;
            float d = shardDE(p, i, react, h);
            if (d < best)
            {
                best = d;
                hue = h;
            }
        }

        vec3 facetTint = mix(vec3(1.0), abs(n), 0.6); // "which face you're looking at" gem cheat
        vec3 base = hue * facetTint;
        col = base * (0.3 + 0.8 * diff) * (0.7 + uLevel * react * 0.9);
        col += spec * 0.9 * (0.5 + uOnset * react * uCameraShake * 1.0);
        col += fresnel * vec3(0.7, 0.8, 1.0) * 0.5;
        col += uOnset * react * uCameraShake * 0.5; // camera-flash strobe across every facet at once
    }

    col += glow * palette(t * 0.06 + uTime * 0.02, uHue) * (0.4 + react * 0.8);
    col *= 1.0 - smoothstep(11.0, 22.0, t) * 0.85;

    fragColor = vec4(grade(col), 1.0);
}
