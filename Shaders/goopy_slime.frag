// Goopy Slime — a single continuous mass of viscous goo: a few big lobes
// smooth-min merged into one oozing body that bulges with the bass, each
// lobe sagging slowly downward on its own cycle for the "slumping under
// its own weight" read. Three things make this genuinely different from
// Metaballs rather than a reskin of it: (1) the surface itself is
// perturbed by a rolling sine-noise bump field, so it reads as boiling/
// lumpy goo instead of Metaballs' perfectly smooth spherical fusion,
// (2) a few thin, tapering strands actually droop off the underside of
// the mass down onto a floor below it -- real dripping, not just an
// orbiting blob -- and (3) a hard, tight specular highlight for a wet/
// glossy surface instead of Metaballs' softer plastic finish.

float sminG(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float hashG(float n) { return fract(sin(n) * 43758.5453123); }

const int kNumLobes = 5;
const float kFloorY = -2.0;

vec3 lobeCenter(int i, float react)
{
    float seed = float(i) * 9.7 + 3.0;
    float speed = mix(0.12, 0.25, hashG(seed)) * (0.4 + uMid * react * 0.5);
    float radius = mix(0.5, 1.1, hashG(seed + 5.0));
    float phase = hashG(seed + 11.0) * 6.28318530718;
    // Slow vertical sag -- each lobe drifts down then eases back up,
    // reading as the mass slumping under its own weight over and over.
    // A continuous sine bob, NOT a fract()-based sawtooth (see git history
    // -- that version reset via fract(time/period) and jumped position by
    // a full 1.1 units the instant that wrapped, a real once-per-cycle
    // discontinuity that read as "glitchy"). Sine has no such jump.
    float sagPeriod = mix(6.0, 11.0, hashG(seed + 17.0));
    float sag = 0.5 + 0.5 * sin(uTime * (6.28318530718 / sagPeriod) + hashG(seed + 23.0) * 6.28318530718);

    vec3 c;
    c.x = cos(uTime * speed + phase) * radius;
    c.y = 0.6 - sag * 1.1 - 0.3 * sin(uTime * speed * 0.6 + phase);
    c.z = sin(uTime * speed + phase) * radius * 0.7;
    return c;
}

float lobeRadius(int i, float react)
{
    float bulge = 1.0 + uBass * react * 0.3;
    return mix(0.75, 1.15, hashG(float(i) * 6.0 + 1.0)) * bulge;
}

float massDE(vec3 p, float react)
{
    float blendK = 1.1 + uBass * react * 0.4;
    float d = 1.0e5;
    for (int i = 0; i < kNumLobes; ++i)
    {
        float bd = length(p - lobeCenter(i, react)) - lobeRadius(i, react);
        d = sminG(d, bd, blendK);
    }

    // Lumpy/boiling surface: a rolling sine-noise perturbation, cheap and
    // bounded (no raymarch-overshoot risk -- the amplitude is well under
    // the march step size), but it's the single biggest "this is thick
    // goo, not a clean fused sphere" tell.
    float bump = sin(p.x * 9.0 + uTime * 1.3) * sin(p.y * 7.0 - uTime * 1.7) * sin(p.z * 8.0 + uTime * 1.1);
    d += bump * 0.045 * (0.6 + uMid * react * 0.6);
    return d;
}

// A tapered capsule (cone with rounded ends) -- the standard cheap
// approximation, exact enough for a drooping drip strand at this scale.
float sdTaperedCapsuleG(vec3 p, vec3 a, vec3 b, float ra, float rb)
{
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - mix(ra, rb, h);
}

const int kNumDripStrands = 3;

float dripStrandsDE(vec3 p, float react)
{
    float d = 1.0e5;
    for (int i = 0; i < kNumDripStrands; ++i)
    {
        int lobeIdx = i * 2; // spread across alternating lobes (0, 2, 4)
        vec3 lobeC = lobeCenter(lobeIdx, react);
        float lobeR = lobeRadius(lobeIdx, react);
        vec3 top = lobeC - vec3(0.0, lobeR * 0.7, 0.0);

        float seed = float(i) * 13.0 + 5.0;
        float sway = sin(uTime * 0.4 + seed) * 0.15;
        float dripLen = mix(1.5, 2.2, hashG(seed + 3.0)) + uBass * react * 0.5;
        vec3 bottom = top + vec3(sway, -dripLen, cos(uTime * 0.3 + seed) * 0.1);
        bottom.y = max(bottom.y, kFloorY + 0.03);

        float strand = sdTaperedCapsuleG(p, top, bottom, lobeR * 0.22, 0.025);
        d = min(d, strand);
    }
    return d;
}

float sceneDE(vec3 p, float react)
{
    float d = min(massDE(p, react), dripStrandsDE(p, react));
    d = min(d, p.y - kFloorY); // floor plane, catches the drip strands' pooled ends
    return d;
}

vec3 sceneNormalG(vec3 p, float react)
{
    vec2 e = vec2(0.002, 0.0);
    return normalize(vec3(
        sceneDE(p + e.xyy, react) - sceneDE(p - e.xyy, react),
        sceneDE(p + e.yxy, react) - sceneDE(p - e.yxy, react),
        sceneDE(p + e.yyx, react) - sceneDE(p - e.yyx, react)));
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
    bool hitFloor = false;
    vec3 p = ro;
    for (int i = 0; i < 110; ++i)
    {
        p = ro + rd * t;
        float d = sceneDE(p, react);
        if (d > 0.1)
            glow += 0.0012 / (0.02 + d * d * 3.0);
        float eps = 0.0015 + t * 0.0006;
        if (d < eps)
        {
            hit = true;
            hitFloor = abs(p.y - kFloorY) < 0.01;
            break;
        }
        // Slower march step than Metaballs -- goo's surface is smoother/
        // rounder (wider smin blend radius), so a gentler step avoids
        // overshoot artifacts on the wide blends.
        t += d * 0.6;
        if (t > 32.0)
            break;
    }

    vec3 col = vec3(0.01, 0.02, 0.012);
    if (hit)
    {
        vec3 n = sceneNormalG(p, react);
        vec3 lightDir = normalize(vec3(0.4, 0.8, -0.3));
        float diff = max(dot(n, lightDir), 0.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 4.0);
        vec3 halfV = normalize(lightDir - rd);
        float spec = pow(max(dot(n, halfV), 0.0), 60.0);

        if (hitFloor)
        {
            // Dark reflective puddle where the drips land.
            vec3 puddle = vec3(0.04, 0.07, 0.05) * (0.5 + 0.5 * diff);
            col = puddle;
            col += spec * 0.6;
        }
        else
        {
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
    }

    col += glow * palette(t * 0.04 + uTime * 0.01, uHue) * (0.4 + react * 0.8) * vec3(0.6, 1.0, 0.5);
    col *= 1.0 - smoothstep(16.0, 32.0, t) * 0.8;
    col += uOnset * react * uCameraShake * 0.12 * vec3(0.5, 0.9, 0.5);

    fragColor = vec4(grade(col), 1.0);
}
