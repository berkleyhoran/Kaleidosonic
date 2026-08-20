// Water Ripples — rain-drop-style ripples landing on the surface of still
// water: each drop is a fresh expanding ring wave (like Image Ripple's
// concentric rings, but many independent drops instead of one centered
// swell), summed together so overlapping rings genuinely interfere, then
// used to perturb a fake surface normal for a real specular glint rather
// than a flat colored ring. Fully procedural (no image needed, no
// raymarch) -- a small fixed loop of drops, cheap regardless of anything
// else. Distortion controls drop density.

float hashWR(float n) { return fract(sin(n) * 43758.5453123); }

const int kNumDrops = 10;

// Height contribution of a single expanding ring wave at distance `dist`
// from its drop's own landing point, `age` seconds after it landed: a
// damped sine ring that grows outward and fades.
float rippleHeight(float dist, float age, float speed, float decay)
{
    if (age < 0.0 || age > 3.5)
        return 0.0;
    float r = age * speed;
    float wave = sin((dist - r) * 26.0) * exp(-abs(dist - r) * 6.0);
    float envelope = exp(-age * decay) * smoothstep(0.0, 0.15, age);
    return wave * envelope;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv /= max(uCameraScale, 0.05);
    float react = uReactivity;

    float dropDensity = mix(0.6, 1.0, clamp(uDistortion, 0.0, 1.0));
    int numDrops = int(float(kNumDrops) * dropDensity + 0.5);

    float height = 0.0;
    vec2 gradient = vec2(0.0);
    const float eps = 0.01;

    for (int i = 0; i < kNumDrops; ++i)
    {
        if (i >= numDrops)
            break;

        float seed = float(i) * 6.7 + 2.0;
        float period = mix(0.9, 2.4, hashWR(seed)) / (0.6 + uBass * react * 1.0);
        float phaseOffset = hashWR(seed + 3.0);
        float cycle = fract(uTime / period + phaseOffset);
        float age = cycle * period;

        vec2 dropPos = vec2(mix(-1.4, 1.4, hashWR(seed + 5.0)), mix(-1.0, 1.0, hashWR(seed + 7.0)));
        float speed = mix(0.5, 0.9, hashWR(seed + 9.0));

        float dist = length(uv - dropPos);
        height += rippleHeight(dist, age, speed, 1.1);

        // Cheap two-tap gradient (per axis) for the fake surface normal
        // below -- avoids a full screen-space derivative pass.
        gradient.x += rippleHeight(length(uv + vec2(eps, 0.0) - dropPos), age, speed, 1.1)
                    - rippleHeight(length(uv - vec2(eps, 0.0) - dropPos), age, speed, 1.1);
        gradient.y += rippleHeight(length(uv + vec2(0.0, eps) - dropPos), age, speed, 1.1)
                    - rippleHeight(length(uv - vec2(0.0, eps) - dropPos), age, speed, 1.1);
    }

    // Onset: an extra strong ripple radiating from center right on the
    // beat, on top of the ambient drops.
    float onsetDist = length(uv);
    height += uOnset * react * 0.6 * rippleHeight(onsetDist, 0.05, 0.9, 0.6);

    vec3 normal = normalize(vec3(-gradient * 3.0, 1.0));
    vec3 lightDir = normalize(vec3(0.4, 0.6, 0.7));
    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(reflect(-lightDir, normal), vec3(0.0, 0.0, 1.0)), 0.0), 40.0);

    vec3 deepWater = palette(0.55, uHue) * vec3(0.35, 0.6, 0.85);
    vec3 col = deepWater * (0.4 + 0.5 * diff) + height * vec3(0.15, 0.3, 0.4);
    col += spec * vec3(0.9, 0.95, 1.0) * (0.6 + uTreble * react * 1.0);

    col *= 0.85 + uLevel * react * 0.3;
    col += uOnset * react * 0.1 * palette(uTime * 0.02, uHue);

    fragColor = vec4(grade(col), 1.0);
}
