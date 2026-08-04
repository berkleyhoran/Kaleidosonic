// Fractal Bubbles — glowing, parallax-depth bubbles positioned by a
// Sierpinski chaos-game point set (the same "converge toward a random
// corner" process real fractal generators use), so the cloud of bubbles is
// itself a fractal point distribution, drifting and popping with the audio.

float hash1(float n) { return fract(sin(n) * 43758.5453123); }

vec2 sierpinskiPoint(float seed)
{
    vec2 c1 = vec2(0.0, 1.0);
    vec2 c2 = vec2(-0.866, -0.5);
    vec2 c3 = vec2(0.866, -0.5);
    vec2 p = vec2(0.0);
    for (int k = 0; k < 9; ++k)
    {
        float h = hash1(seed + float(k) * 17.13);
        vec2 corner = h < 0.333 ? c1 : (h < 0.667 ? c2 : c3);
        p = mix(p, corner, 0.5);
    }
    return p;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;
    float react = uReactivity;
    uv = rotate2d(uTime * 0.03 * uRotationSpeed) * uv;

    vec3 col = vec3(0.02, 0.02, 0.05);

    const int numBubbles = 40;
    for (int i = 0; i < numBubbles; ++i)
    {
        float seed = float(i) * 7.0 + 1.0;
        vec2 base = sierpinskiPoint(seed) * 0.85;

        // Fake depth (z) gives cheap parallax: closer bubbles are bigger,
        // brighter, and drift/pulse more with the beat.
        float depth = hash1(seed + 91.0);
        float speed = mix(0.05, 0.25, hash1(seed + 5.0)) * (0.4 + uMid * react * 1.5);
        float drift = sin(uTime * speed + seed) * 0.18 * (1.0 + uBass * react);
        vec2 pos = base * (0.75 + depth * 0.5)
                 + vec2(drift, cos(uTime * speed * 0.7 + seed) * 0.12);
        pos *= 1.0 + uZoomWander * 0.15 * sin(uTime * 0.11 + seed);

        float size = mix(0.018, 0.075, depth) * (1.0 + uOnset * react * uCameraShake * 1.0);
        float d = length(uv - pos);

        float bubble = smoothstep(size, size * 0.35, d);
        float rim = smoothstep(size, size * 0.85, d) - smoothstep(size * 0.85, size * 0.55, d);

        float hue = fract(uHue + seed * 0.05 + depth * 0.3 + uTime * 0.02 + uTreble * react * 0.5);
        vec3 bc = hsv2rgb(vec3(hue, uSaturation * 0.7, 1.0));

        col += bc * bubble * 0.18 * (0.4 + depth) * (0.6 + uLevel * react * 1.3);
        col += vec3(1.0) * rim * 0.35 * (0.5 + depth);
    }

    col += uOnset * react * 0.08 * vec3(0.7, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
