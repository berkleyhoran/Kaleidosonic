// Drippy Liquid — realistic 2D liquid drip physics: each nozzle forms a
// bulb that thins into a neck as it grows (a smooth-min blend whose radius
// shrinks with growth, so the neck visibly pinches the way real surface
// tension does), detaches once the neck fully pinches off, falls as a
// single droplet under gravity-like ease, and splashes into an expanding
// ring plus a few spray droplets where it lands on the water line below.
// Fully 2D (screen-space SDFs, no raymarch) -- cheaper than a 3D scene,
// not just a better fit for "liquid physics" than free-floating capsules.

float hashDL(float n) { return fract(sin(n) * 43758.5453123); }

float sminDL(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

const int kNumDrips = 11;
const float kNozzleY = 0.85;
const float kFloorY = -0.55;

void dripState2D(int i, float react, out float dripX, out float cycle, out float period)
{
    float seed = float(i) * 5.3 + 1.0;
    period = mix(1.8, 3.6, hashDL(seed)) / (0.6 + uBass * react * 1.1);
    float phaseOffset = hashDL(seed + 3.0);
    cycle = fract(uTime / period + phaseOffset);
    dripX = mix(-1.5, 1.5, hashDL(seed + 7.0));
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv /= max(uCameraScale, 0.05);
    float react = uReactivity;

    // Background: a soft vertical gradient, darker toward the floor.
    vec3 col = mix(vec3(0.02, 0.03, 0.05), vec3(0.01, 0.015, 0.03),
                    clamp((kNozzleY - uv.y) / (kNozzleY - kFloorY), 0.0, 1.0));

    // Water surface band below the floor line, with a gentle shimmer.
    if (uv.y < kFloorY)
    {
        float shimmer = 0.5 + 0.5 * sin(uv.x * 12.0 + uTime * 1.4);
        vec3 water = mix(vec3(0.04, 0.09, 0.14), vec3(0.08, 0.16, 0.22), shimmer * 0.5 + 0.25);
        col = mix(water, col, smoothstep(kFloorY - 0.02, kFloorY, uv.y));
    }

    float field = 1.0e5;
    vec3 dripCol = palette(0.55, uHue) * vec3(0.7, 0.9, 1.1);

    for (int i = 0; i < kNumDrips; ++i)
    {
        float dripX, cycle, period;
        dripState2D(i, react, dripX, cycle, period);
        float seedR = mix(0.05, 0.09, hashDL(float(i) * 5.3 + 1.0 + 17.0));

        // Growth phase (0..0.5): bulb grows below the nozzle, the neck
        // between the nozzle anchor and the bulb thins (shrinking blend
        // radius) as it does -- the real "surface tension pinching a
        // forming droplet" shape.
        float growT = smoothstep(0.0, 0.5, cycle) * (1.0 - step(0.5, cycle));
        if (growT > 0.0)
        {
            float bulbY = kNozzleY - growT * 0.35;
            float bulbR = seedR * (0.4 + growT * 0.9);
            float neckK = mix(0.05, 0.002, growT); // thins toward a sharp pinch as it grows

            vec2 anchor = vec2(dripX, kNozzleY + 0.03);
            vec2 bulb = vec2(dripX, bulbY);
            float dAnchor = length(uv - anchor) - seedR * 0.55;
            float dBulb = length(uv - bulb) - bulbR;
            field = min(field, sminDL(dAnchor, dBulb, neckK));
        }

        // Fall phase (0.5..0.92): a single droplet, mildly elongated
        // vertically (non-uniform distance scale), easing downward under
        // gravity-like acceleration.
        float fallT = clamp((cycle - 0.5) / 0.42, 0.0, 1.0);
        if (fallT > 0.0 && fallT < 1.0)
        {
            float eased = fallT * fallT;
            float dropY = mix(kNozzleY - 0.35, kFloorY, eased);
            vec2 dp = uv - vec2(dripX, dropY);
            dp.y /= mix(1.0, 1.4, smoothstep(0.0, 0.3, fallT)); // stretches as it accelerates
            float dDrop = length(dp) - seedR * 0.85;
            field = min(field, dDrop);
        }

        // Splash: an expanding ring plus a few spray droplets, timed to
        // the moment this drip's fall reaches the floor and lingering a
        // little into the next cycle so it doesn't cut off abruptly.
        float sinceSplash = (cycle >= 0.92 ? (cycle - 0.92) : (cycle + 1.0 - 0.92)) * period;
        if (sinceSplash < 0.6)
        {
            float splashT = sinceSplash / 0.6;
            float ringR = splashT * 0.35;
            float ringDist = abs(length(uv - vec2(dripX, kFloorY)) - ringR) - 0.006;
            float ringGlow = smoothstep(0.03, 0.0, ringDist) * (1.0 - splashT);
            col += dripCol * ringGlow * (0.5 + uOnset * react * 0.6);

            // A few spray droplets arcing up and out right after impact.
            for (int s = 0; s < 3; ++s)
            {
                float sSeed = float(i) * 3.0 + float(s) * 7.0 + 11.0;
                float sAngle = mix(-0.6, 0.6, hashDL(sSeed)) + (float(s) - 1.0) * 0.5;
                float sSpeed = mix(0.5, 0.9, hashDL(sSeed + 2.0));
                float sx = dripX + sin(sAngle) * splashT * sSpeed;
                float sy = kFloorY + max((splashT * sSpeed * cos(sAngle) - splashT * splashT * 1.4) * 0.6, 0.0);
                float dSpray = length(uv - vec2(sx, sy)) - 0.012 * (1.0 - splashT);
                float sprayGlow = smoothstep(0.02, 0.0, dSpray) * (1.0 - splashT);
                col += dripCol * sprayGlow;
            }
        }
    }

    // The drip body itself: soft-edged, lit like wet glass with a bright
    // highlight offset toward the "light source" corner.
    if (field < 0.03)
    {
        vec3 wet = dripCol * 0.5;
        float highlight = smoothstep(0.012, -0.006, field - 0.006);
        wet += vec3(1.0) * highlight * 0.6;
        col = mix(col, wet, smoothstep(0.01, -0.01, field));
    }

    col *= 0.85 + uLevel * react * 0.3;
    col += uOnset * react * 0.08 * palette(uTime * 0.02, uHue);

    fragColor = vec4(grade(col), 1.0);
}
