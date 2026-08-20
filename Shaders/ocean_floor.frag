// Ocean Floor — a sunlit sandy sea floor viewed from above: rippled sand
// ridges, swaying seaweed strands, animated caustic light nets, and
// shafts of light descending from above, all tinted a strong blue-green so
// it unmistakably reads as "looking down through water" rather than an
// abstract caustic pattern. Fully procedural (no raymarch) -- the caustic
// look comes from overlapping warped sine fields, the same cheap trick
// real-time caustic effects have used for decades, so cost stays flat
// regardless of any other setting.
//
// Deliberately NOT audio-reactive (an explicit user call after trying it
// live) -- earlier drafts drove the caustic sparkle's contrast *power*
// (not just its brightness) off treble, which changed the actual texture/
// character of the pattern moment to moment rather than reading as a
// normal pulse, and came across as "weird" rather than musical. This is
// meant to be an ambient, slowly-animating scene the audio sits over, not
// something that visibly reacts to it -- same idea as a still photograph
// preset, just in motion on its own clock.

float hashOF(float n) { return fract(sin(n) * 43758.5453123); }

float causticLayer(vec2 uv, float t, float freq, float speed)
{
    vec2 p = uv * freq;
    float v = sin(p.x + t * speed) + sin(p.y * 1.3 - t * speed * 0.8)
            + sin((p.x + p.y) * 0.7 + t * speed * 1.4)
            + sin(length(p) * 1.1 - t * speed * 1.7);
    return v * 0.25;
}

// One seaweed strand's coverage near local p (0,0 = rooted point, +y up):
// a thin ribbon that bends progressively more with height, like it's
// swaying in a current.
float weedCoverage(vec2 p, float sway, float height)
{
    float bend = sway * smoothstep(0.0, height, p.y) * (0.4 + 0.6 * sin(p.y * 6.0));
    float px = p.x - bend;
    float taper = 1.0 - clamp(p.y / max(height, 1.0e-3), 0.0, 1.0);
    float halfW = mix(0.006, 0.014, taper);
    return step(abs(px), halfW) * step(0.0, p.y) * step(p.y, height);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv /= max(uCameraScale, 0.05);

    float t = uTime;

    // Slow current drift -- Zoom Speed repurposed as drift speed, the same
    // "generic knob -> this preset's own concept" convention Wispy Ribbons
    // already uses it for.
    vec2 drift = vec2(t * (0.02 + 0.03 * uZoomSpeed), t * 0.012);
    vec2 sandUV = uv + drift;

    // Rippled sand: parallel ridges perpendicular to the current, warped
    // by a slower secondary field so the ridges snake rather than sitting
    // in perfectly straight rows -- what actually reads as "sand", versus
    // the caustic net alone which reads as pure light.
    float ridgeWarp = causticLayer(sandUV * 0.35, t, 1.0, 0.08) * 0.35;
    float ridgePhase = (sandUV.x + sandUV.y * 0.4) * 26.0 + ridgeWarp * 4.0;
    float ridges = sin(ridgePhase) * 0.5 + 0.5;
    float ridgeShade = 0.55 + 0.45 * ridges;
    // Cooled toward blue-green even at the sand layer itself (a real sea
    // floor never reads warm/sandy from directly overhead through water --
    // the water column itself does this filtering) rather than relying
    // only on the final tint pass to sell it.
    vec3 sandCol = mix(vec3(0.16, 0.30, 0.34), vec3(0.32, 0.52, 0.52), ridgeShade);
    // A little fine grain on top so it doesn't read as flat bands.
    sandCol *= 0.94 + 0.08 * causticLayer(sandUV * 3.0, t, 1.0, 0.02);

    // Caustic light net: two layers at different scale/speed multiplied
    // together (the standard caustic trick -- bright only where both
    // layers happen to peak together, giving the characteristic net/mesh
    // look). Fixed amplitude/contrast -- not audio-driven, see header.
    float c1 = causticLayer(sandUV, t, 5.0, 0.6);
    float c2 = causticLayer(sandUV * 1.7 + 3.1, t, 8.0, -0.8);
    float caustic = max(c1, 0.0) * max(c2, 0.0);
    caustic = pow(clamp(caustic * 1.6, 0.0, 1.0), 2.4);

    vec3 lightCol = palette(ridgeShade * 0.3 + t * 0.01, uHue) * 0.35 + vec3(0.45, 0.85, 0.95) * 0.65;
    vec3 col = sandCol;
    col += lightCol * caustic * 1.1;

    // God rays: a few slow diagonal light shafts descending from the top
    // of frame, brightest where they cross open sand -- the single
    // biggest "sunlight through water" tell.
    float rays = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        float ri = float(i);
        float rayX = sin(t * 0.03 + ri * 2.1) * 0.6 + ri * 0.5 - 0.75;
        float shaftUvX = uv.x - uv.y * 0.35 - rayX;
        float shaft = smoothstep(0.16, 0.0, abs(shaftUvX));
        rays += shaft * (0.5 + 0.5 * sin(uv.y * 3.0 - t * 0.4 + ri));
    }
    col += vec3(0.5, 0.8, 0.85) * rays * 0.16 * 0.85;

    // Seaweed: a sparse row of strands rooted in the sand, swaying in the
    // current.
    float weedCellW = 0.5;
    float weedX = uv.x / weedCellW;
    float weedId = floor(weedX);
    float weedLocalX = (fract(weedX) - 0.5) * weedCellW;
    float weedSeed = weedId * 9.7 + 4.1;
    if (hashOF(weedSeed + 1.0) > 0.45) // sparse -- not every cell gets a strand
    {
        float weedHeight = mix(0.25, 0.55, hashOF(weedSeed));
        float weedRootY = mix(-0.9, -0.5, hashOF(weedSeed + 6.0)) / max(uCameraScale, 0.05);
        float weedSway = sin(t * 0.7 + weedSeed * 3.0) * 0.07;
        vec2 weedLocal = vec2(weedLocalX, uv.y - weedRootY);
        float weedCov = weedCoverage(weedLocal, weedSway, weedHeight);
        vec3 weedCol = vec3(0.10, 0.30, 0.16) * 0.85;
        col = mix(col, weedCol, weedCov);
    }

    float dist = length(uv);

    // Strong, unambiguously blue depth tint and vignette toward the edges
    // -- makes the "peering down into open ocean water" read unmistakable
    // rather than a subtle abstract-pattern one.
    col = mix(col, col * vec3(0.28, 0.55, 0.82), 0.62);
    col += vec3(0.02, 0.06, 0.14) * 0.55;
    col *= 1.0 - smoothstep(0.45, 1.3, dist) * 0.65;

    fragColor = vec4(grade(col), 1.0);
}
