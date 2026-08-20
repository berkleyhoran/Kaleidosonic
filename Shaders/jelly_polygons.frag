// Jelly Polygons — the whole screen filled with soft, squishy N-gon
// "jelly" shapes drifting past and squashing into each other via a
// smooth-min blend, each one wobbling at its own edge like real jelly
// under its own weight, and the whole field squashing vertically on bass.
// Fully 2D (screen-space SDFs, no raymarch) so cost stays flat regardless
// of Iterations -- one composited field, evaluated once per pixel across
// every shape.

float hashJ(float n) { return fract(sin(n) * 43758.5453123); }

float sdPolygonJ(vec2 p, float radius, float sides, float rot, float wobble, float wobbleFreq)
{
    float a = atan(p.y, p.x) - rot;
    float seg = 6.28318530718 / sides;
    float edgeR = radius * cos(seg * 0.5) / max(cos(mod(a, seg) - seg * 0.5), 0.2);
    edgeR += wobble * sin(a * wobbleFreq + uTime * 2.4);
    return length(p) - edgeR;
}

float sminJ(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

const int kNumJellies = 8;

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv /= max(uCameraScale, 0.05);

    float react = uReactivity;
    // Kept gentle and applied once -- an earlier version divided BOTH each
    // jelly's radius AND the sampling coordinate by this same bass-driven
    // squash (a compounding effect) at a large enough multiplier that
    // fast bass envelope moves read as the whole field jittering rather
    // than breathing. Scale explicitly tuned down further per feedback --
    // "the range between 0-0.03" is where this reads as a subtle breathe
    // rather than jitter.
    float squash = 1.0 + uBass * react * 0.03;

    float field = 1.0e5;
    float nearestIdx = 0.0;
    for (int i = 0; i < kNumJellies; ++i)
    {
        float seed = float(i) * 11.3 + 2.0;
        float speed = mix(0.06, 0.18, hashJ(seed)) * (0.5 + uMid * react * 0.6);
        vec2 center = vec2(sin(uTime * speed + seed), cos(uTime * speed * 0.8 + seed * 1.7))
                    * mix(0.5, 1.3, hashJ(seed + 5.0));
        float radius = mix(0.28, 0.55, hashJ(seed + 9.0));
        float sides = floor(mix(3.0, 7.0, hashJ(seed + 13.0)));
        float rot = uTime * (0.1 + hashJ(seed + 17.0) * 0.3) * (0.4 + abs(uRotationSpeed)) + seed;
        float wobble = (0.015 + uOnset * react * 0.03) * radius;

        vec2 p = uv - center;
        p.y /= squash; // bass squashes every jelly vertically, like it's bouncing under its own weight
        float d = sdPolygonJ(p, radius, sides, rot, wobble, 5.0 + hashJ(seed + 2.0) * 3.0);

        if (d < field)
            nearestIdx = float(i);
        field = sminJ(field, d, 0.12);
    }

    vec3 col = vec3(0.02, 0.015, 0.03);
    if (field < 0.0)
    {
        float depth = clamp(-field * 3.0, 0.0, 1.0);
        vec3 base = palette(nearestIdx / float(kNumJellies) + uTime * 0.01, uHue);
        col = base * mix(0.5, 1.0, depth);
        // Translucent rim highlight where jellies overlap/edge, additive
        // for a glassy look.
        float rim = smoothstep(0.06, 0.0, abs(field));
        col += vec3(1.0) * rim * 0.35;
    }
    col *= 0.85 + uLevel * react * 0.4;
    col += uOnset * react * 0.15 * palette(uTime * 0.02 + 0.5, uHue);

    fragColor = vec4(grade(col), 1.0);
}
