// Goopy Slime — a single continuous mass of viscous goo: a few big lobes
// smooth-min merged into one oozing body that bulges with the bass, each
// lobe sagging slowly downward on its own cycle for the "slumping under
// its own weight" read, finished with a hard, tight specular highlight for
// a wet/glossy surface instead of Metaballs' softer plastic finish.

float sminG(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float hashG(float n) { return fract(sin(n) * 43758.5453123); }

const int kNumLobes = 5;

vec3 lobeCenter(int i, float react)
{
    float seed = float(i) * 9.7 + 3.0;
    float speed = mix(0.12, 0.25, hashG(seed)) * (0.4 + uMid * react * 0.5);
    float radius = mix(0.5, 1.1, hashG(seed + 5.0));
    float phase = hashG(seed + 11.0) * 6.28318530718;
    // Slow vertical sag -- each lobe drifts down then eases back up,
    // reading as the mass slumping under its own weight over and over.
    // A continuous sine bob, NOT a fract()-based sawtooth: the earlier
    // version reset via fract(time/period) and jumped position by a full
    // 1.1 units the instant that wrapped from ~1.0 back to 0.0 -- a real
    // once-per-cycle discontinuity, which is exactly what read as
    // "glitchy" (a visible snap in the merged surface every 6-11s, once
    // per lobe, staggered but frequent with 5 lobes). Sine has no such
    // jump at any point in its cycle.
    float sagPeriod = mix(6.0, 11.0, hashG(seed + 17.0));
    float sag = 0.5 + 0.5 * sin(uTime * (6.28318530718 / sagPeriod) + hashG(seed + 23.0) * 6.28318530718);

    vec3 c;
    c.x = cos(uTime * speed + phase) * radius;
    c.y = 0.6 - sag * 1.1 - 0.3 * sin(uTime * speed * 0.6 + phase);
    c.z = sin(uTime * speed + phase) * radius * 0.7;
    return c;
}

float slimeDE(vec3 p, float react)
{
    float bulge = 1.0 + uBass * react * 0.3;
    float blendK = 1.1 + uBass * react * 0.4;
    float d = 1.0e5;
    for (int i = 0; i < kNumLobes; ++i)
    {
        float r = mix(0.75, 1.15, hashG(float(i) * 6.0 + 1.0)) * bulge;
        float bd = length(p - lobeCenter(i, react)) - r;
        d = sminG(d, bd, blendK);
    }
    return d;
}

vec3 slimeNormal(vec3 p, float react)
{
    vec2 e = vec2(0.002, 0.0);
    return normalize(vec3(
        slimeDE(p + e.xyy, react) - slimeDE(p - e.xyy, react),
        slimeDE(p + e.yxy, react) - slimeDE(p - e.yxy, react),
        slimeDE(p + e.yyx, react) - slimeDE(p - e.yyx, react)));
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    float orbitAngle = uTime * 0.08 * (0.3 + abs(uRotationSpeed));
    float dist = 8.5 / max(uCameraScale, 0.05);
    vec3 ro = vec3(sin(orbitAngle), 0.15, cos(orbitAngle)) * dist;
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.8 + right * uv.x + up * uv.y);

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 100; ++i)
    {
        p = ro + rd * t;
        float d = slimeDE(p, react);
        if (d > 0.1)
            glow += 0.0012 / (0.02 + d * d * 3.0);
        float eps = 0.0015 + t * 0.0006;
        if (d < eps)
        {
            hit = true;
            break;
        }
        // Slower march step than Metaballs -- goo's surface is smoother/
        // rounder (wider smin blend radius), so a gentler step avoids
        // overshoot artifacts on the wide blends.
        t += d * 0.7;
        if (t > 30.0)
            break;
    }

    vec3 col = vec3(0.01, 0.02, 0.012);
    if (hit)
    {
        vec3 n = slimeNormal(p, react);
        vec3 lightDir = normalize(vec3(0.4, 0.8, -0.3));
        float diff = max(dot(n, lightDir), 0.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 4.0);
        vec3 halfV = normalize(lightDir - rd);
        float spec = pow(max(dot(n, halfV), 0.0), 60.0);

        vec3 base = palette(p.y * 0.15 + uTime * 0.015, uHue) * vec3(0.7, 1.0, 0.55);
        if (uUserImageLoaded > 0.5)
        {
            vec4 imgC = sampleUserImage(imageContainUV(p.xy * 0.3, uUserImageAspect));
            base = mix(base, imgC.rgb, imgC.a * 0.8);
        }

        col = base * (0.3 + 0.7 * diff) * (0.7 + uLevel * react * 0.9);
        col += fresnel * vec3(0.5, 0.8, 0.6) * (0.3 + uOnset * react * uCameraShake * 0.8);
        col += spec * (0.8 + uTreble * react * 1.2);
    }

    col += glow * palette(t * 0.04 + uTime * 0.01, uHue) * (0.4 + react * 0.8) * vec3(0.6, 1.0, 0.5);
    col *= 1.0 - smoothstep(15.0, 30.0, t) * 0.8;
    col += uOnset * react * uCameraShake * 0.12 * vec3(0.5, 0.9, 0.5);

    fragColor = vec4(grade(col), 1.0);
}
