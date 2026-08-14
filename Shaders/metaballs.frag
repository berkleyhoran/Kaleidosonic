// Metaballs — a cluster of orbiting spheres fused into one wobbling,
// breathing organic blob via the standard smooth-min CSG blend, camera
// slowly circling the whole cluster. A genuinely different "camera orbits
// a fixed structure" read from the infinite-flythrough presets (Shape
// Rave, the tunnels) -- same family as Pipes/Rotating Light Logo. Bass
// swells each ball's radius and the blend softness together (so the fuse
// visibly breathes on the beat), onset flashes a rim-light kick, and when
// an image (or GIF) is loaded its colors paint across the fused surface
// by world-space position instead of the palette sweep.

float smin(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float hash1(float n) { return fract(sin(n) * 43758.5453123); }

const int kNumBalls = 7;

vec3 ballCenter(int i, float react)
{
    float seed = float(i) * 13.1 + 1.0;
    float speed = mix(0.25, 0.55, hash1(seed)) * (0.5 + uMid * react * 0.8);
    float radius = mix(0.9, 2.0, hash1(seed + 5.0));
    float phase = hash1(seed + 11.0) * 6.28318530718;
    float tiltSpeed = mix(0.15, 0.35, hash1(seed + 17.0));

    vec3 c;
    c.x = cos(uTime * speed + phase) * radius;
    c.y = sin(uTime * tiltSpeed + phase * 1.7) * radius * 0.6;
    c.z = sin(uTime * speed * 0.8 + phase) * radius;
    return c;
}

float metaballsDE(vec3 p, float react)
{
    float blendK = 0.7 + uBass * react * 0.5;
    float d = 1.0e5;
    for (int i = 0; i < kNumBalls; ++i)
    {
        float r = mix(0.55, 0.85, hash1(float(i) * 7.0 + 2.0)) * (1.0 + uBass * react * 0.25);
        float bd = length(p - ballCenter(i, react)) - r;
        d = smin(d, bd, blendK);
    }
    return d;
}

vec3 metaballsNormal(vec3 p, float react)
{
    vec2 e = vec2(0.002, 0.0);
    return normalize(vec3(
        metaballsDE(p + e.xyy, react) - metaballsDE(p - e.xyy, react),
        metaballsDE(p + e.yxy, react) - metaballsDE(p - e.yxy, react),
        metaballsDE(p + e.yyx, react) - metaballsDE(p - e.yyx, react)));
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    float orbitAngle = uTime * 0.12 * (0.35 + abs(uRotationSpeed));
    float dist = 8.5 / max(uCameraScale, 0.05);
    vec3 ro = vec3(sin(orbitAngle), 0.35 * sin(uTime * 0.07), cos(orbitAngle)) * dist;
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.7 + right * uv.x + up * uv.y);

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 110; ++i)
    {
        p = ro + rd * t;
        float d = metaballsDE(p, react);
        if (d > 0.1)
            glow += 0.0016 / (0.02 + d * d * 3.0);
        float eps = 0.0015 + t * 0.0006;
        if (d < eps)
        {
            hit = true;
            break;
        }
        t += d * 0.8;
        if (t > 30.0)
            break;
    }

    vec3 col = vec3(0.015, 0.01, 0.03);
    if (hit)
    {
        vec3 n = metaballsNormal(p, react);
        vec3 lightDir = normalize(vec3(0.5, 0.7, -0.4));
        float diff = max(dot(n, lightDir), 0.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);

        vec3 base = palette(p.y * 0.2 + uTime * 0.02, uHue);
        if (uUserImageLoaded > 0.5)
        {
            vec4 imgC = sampleUserImage(imageContainUV(p.xy * 0.35, uUserImageAspect));
            base = mix(base, imgC.rgb, imgC.a * 0.9);
        }

        col = base * (0.35 + 0.75 * diff) * (0.7 + uLevel * react * 1.0);
        col += fresnel * vec3(0.6, 0.75, 1.0) * (0.4 + uOnset * react * uCameraShake * 1.2);
        col += pow(diff, 14.0) * 0.5;
    }

    col += glow * palette(t * 0.05 + uTime * 0.015, uHue) * (0.5 + react * 1.0);
    col *= 1.0 - smoothstep(15.0, 30.0, t) * 0.8;
    col += uOnset * react * uCameraShake * 0.15 * vec3(0.8, 0.6, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
