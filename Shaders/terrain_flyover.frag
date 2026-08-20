// Terrain Flyover — an infinite hilly landscape flown over from a
// bird's-eye vantage, built from layered value noise (a cheap Perlin-style
// FBM height field) and raymarched as a heightfield: the classic "distance
// = vertical gap to the surface, times a slope-safety factor" heightfield
// approximation, not a general SDF -- exact distance estimation isn't
// needed for a stylized flyover, just something that won't overshoot on
// slopes. Since the terrain is a pure function of world (x, z), flying
// "infinitely" costs nothing extra -- there's no domain-repeat wall to
// hit the way a repeated-cell tunnel would need, the camera just keeps
// sampling noise further and further along its path. Bass swells hill
// height, treble adds fine ridge detail, and onsets flash light breaking
// through the haze. Camera path gently curves and banks into its own
// turns, following the terrain's own contour for altitude.

float hashTF(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

float vnoiseTF(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hashTF(i);
    float b = hashTF(i + vec2(1.0, 0.0));
    float c = hashTF(i + vec2(0.0, 1.0));
    float d = hashTF(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbmTF(vec2 p)
{
    float amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < 5; ++i)
    {
        sum += amp * vnoiseTF(p);
        p = p * 2.02 + vec2(9.1, 3.7);
        amp *= 0.5;
    }
    return sum;
}

float terrainHeight(vec2 xz, float react)
{
    float hillHeight = 1.2 + uBass * react * 0.9;
    float base = fbmTF(xz * 0.15);
    float ridge = fbmTF(xz * 0.5 + 5.0);
    float fine = vnoiseTF(xz * 2.0) * (0.06 + uTreble * react * 0.12);
    return (base * 1.6 + ridge * 0.5 - 0.9) * hillHeight + fine;
}

float terrainDE(vec3 p, float react)
{
    // The 0.45 fudge factor is the "slope safety" -- a heightfield's real
    // distance to the nearest surface point is less than the vertical gap
    // wherever the terrain is sloped, so under-estimating by this much
    // keeps the march from ever stepping through a steep hillside.
    return (p.y - terrainHeight(p.xz, react)) * 0.45;
}

// A much lower-frequency, single-octave version of the same height field
// -- the broad landscape trend only, none of the ridge/fine detail --
// used purely as the camera's own altitude reference (see main()). Flying
// at a fixed clearance above the *detailed* terrain made the camera bob
// up and down over every individual hillock, reported as "the camera
// shifts down and up" instead of a smooth fly; cruising a fixed clearance
// above this coarse trend instead still rises over mountains and dips
// into valleys at a large scale, but doesn't react to small bumps.
float terrainHeightCoarse(vec2 xz, float react)
{
    float hillHeight = 1.2 + uBass * react * 0.9;
    float base = fbmTF(xz * 0.035);
    return (base * 1.6 - 0.8) * hillHeight;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Forward travel speed -- Zoom Speed sets the pace, bass/onset give a
    // kinetic kick (Camera Shake gated, the usual convention every other
    // flythrough preset here follows).
    float speed = 3.0 + 6.0 * clamp(uZoomSpeed, 0.0, 1.0) + uBass * react * uCameraShake * 2.5;
    float travel = uTime * speed;

    // A gentle S-curving path (not a straight line) -- wander is a slow
    // sine in X as a function of travel distance, so the flight visibly
    // banks left and right over time instead of drilling dead straight.
    float pathX = sin(travel * 0.025) * 6.0;
    vec3 ro = vec3(pathX, 0.0, travel);
    ro.y = terrainHeightCoarse(ro.xz, react) + (4.0 / max(uCameraScale, 0.05));

    // Look a little ahead along the same path for the forward direction,
    // banking the camera's roll into the turn based on the path's own
    // curvature -- Rotation Speed scales how hard it banks. Same coarse
    // height reference as the camera itself, so pitch stays level too.
    float aheadTravel = travel + 6.0;
    float aheadX = sin(aheadTravel * 0.025) * 6.0;
    vec3 lookAt = vec3(aheadX, 0.0, aheadTravel);
    lookAt.y = terrainHeightCoarse(lookAt.xz, react) + (3.4 / max(uCameraScale, 0.05));

    vec3 forward = normalize(lookAt - ro);
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(worldUp, forward));
    vec3 up = cross(forward, right);

    float bank = (pathX - sin((travel - 2.0) * 0.025) * 6.0) * 0.05 * (0.5 + abs(uRotationSpeed));
    right = normalize(right + up * bank);
    up = normalize(cross(forward, right));

    vec3 rd = normalize(forward * 1.7 + right * uv.x + up * uv.y);

    float t = 0.1;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 110; ++i)
    {
        p = ro + rd * t;
        float d = terrainDE(p, react);
        if (d < 0.01 + t * 0.0006)
        {
            hit = true;
            break;
        }
        t += max(d, 0.03);
        if (t > 260.0)
            break;
    }

    // Sky: a palette-driven gradient with a bright band near the horizon,
    // plus an onset light-flash breaking through.
    float skyT = clamp(uv.y * 0.7 + 0.35 - rd.y * 0.5, 0.0, 1.0);
    vec3 sky = mix(palette(0.55 + uTime * 0.004, uHue) * 0.55, palette(0.05 + uTime * 0.004, uHue) * 1.0, skyT);
    sky += uOnset * react * 0.4 * vec3(0.9, 0.9, 1.0);

    vec3 col = sky;
    if (hit)
    {
        vec2 e = vec2(0.05, 0.0);
        vec3 n = normalize(vec3(
            terrainDE(p + e.xyy, react) - terrainDE(p - e.xyy, react),
            terrainDE(p + e.yxy, react) - terrainDE(p - e.yxy, react),
            terrainDE(p + e.yyx, react) - terrainDE(p - e.yyx, react)));

        vec3 lightDir = normalize(vec3(0.4, 0.7, -0.4));
        float diff = max(dot(n, lightDir), 0.0);

        // Height-banded terrain color: low = deep green, mid = lighter
        // green/olive, high = rocky grey, tinted a little by the palette
        // for audio-reactive color variety without losing the "hills"
        // read entirely.
        float heightT = clamp((p.y - ro.y + 3.0) / 6.0, 0.0, 1.0);
        vec3 low = vec3(0.08, 0.22, 0.08);
        vec3 mid = vec3(0.20, 0.34, 0.12);
        vec3 high = vec3(0.42, 0.4, 0.38);
        vec3 base = mix(low, mid, smoothstep(0.0, 0.5, heightT));
        base = mix(base, high, smoothstep(0.55, 0.9, heightT));
        base = mix(base, palette(heightT * 0.4 + uTime * 0.01, uHue), 0.18);

        col = base * (0.35 + 0.75 * diff);

        // Distance fog toward the sky color -- the actual "flying through
        // haze" depth cue.
        float fog = smoothstep(30.0, 220.0, t);
        col = mix(col, sky, fog);
    }
    else
    {
        // Never hit terrain (looking up into open sky) -- fade slightly
        // toward the horizon band for a touch more atmosphere.
        col = sky;
    }

    col *= 0.9 + uLevel * react * 0.25;
    col += uOnset * react * uCameraShake * 0.08 * vec3(0.8, 0.85, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
