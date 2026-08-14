// Neon Logo — the uploaded picture/logo redrawn as a glowing neon-tube
// outline: a cheap 4-tap luminance gradient (not a real edge detector,
// just enough to find where the picture's brightness changes fast) traced
// as a thin bright line, colored, and left for the existing global Bloom
// Intensity post-FX to actually bloom -- no local blur/multi-pass glow of
// its own needed. Flickers like a real neon tube on a hashed per-second
// schedule, with onsets forcing an extra-hard flicker. Shows the shared
// placeholder ring until an image is actually loaded.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;
    float react = uReactivity;

    if (uUserImageLoaded < 0.5)
    {
        fragColor = vec4(grade(imagePlaceholder(uv)), 1.0);
        return;
    }

    vec2 iuv = imageContainUV(uv, uUserImageAspect);

    float eps = 0.0022 + 0.0018 * uDistortion; // Distortion thickens the traced line
    vec4 cL = sampleUserImage(iuv - vec2(eps, 0.0));
    vec4 cR = sampleUserImage(iuv + vec2(eps, 0.0));
    vec4 cD = sampleUserImage(iuv - vec2(0.0, eps));
    vec4 cU = sampleUserImage(iuv + vec2(0.0, eps));

    float lL = dot(cL.rgb, vec3(0.299, 0.587, 0.114)) * cL.a;
    float lR = dot(cR.rgb, vec3(0.299, 0.587, 0.114)) * cR.a;
    float lD = dot(cD.rgb, vec3(0.299, 0.587, 0.114)) * cD.a;
    float lU = dot(cU.rgb, vec3(0.299, 0.587, 0.114)) * cU.a;

    vec2 grad = vec2(lR - lL, lU - lD);
    float edge = length(grad) * 2.2;
    edge = smoothstep(0.08, 0.4, edge);

    // Real neon-tube flicker: mostly steady, occasional hashed dips,
    // harder flicker forced on every onset.
    float flickerSeed = fract(sin(floor(uTime * 5.0) * 12.9898) * 43758.5453123);
    float flicker = flickerSeed > 0.88 ? mix(0.35, 0.7, fract(flickerSeed * 7.0)) : 1.0;
    flicker *= 1.0 - uOnset * react * 0.25;

    float hueT = uTime * 0.03 + uTreble * react * 0.4;
    vec3 neonCol = palette(hueT, uHue);

    // Dim ambient wash of the same color so the backdrop isn't flatly dead.
    vec3 col = vec3(0.006, 0.006, 0.012) + neonCol * 0.02 * (0.5 + uBass * react);

    float lineBright = edge * flicker * (0.9 + uLevel * react * 0.6);
    col += neonCol * lineBright;
    // Hot white core on the brightest part of the line, like a real tube.
    col += vec3(1.0) * pow(edge, 4.0) * flicker * 0.5;

    col += uOnset * react * 0.15 * neonCol;

    fragColor = vec4(grade(col), 1.0);
}
