// Logo Hologram — the uploaded picture/logo as a cyan-tinted holographic
// projection: fine scanlines, RGB channel splitting (chromatic aberration),
// and a bright scan band that sweeps down and wraps, brightening whatever
// it currently passes over. Everything here is a handful of extra texture
// taps and cheap periodic math per pixel -- no loops, same O(1)-per-pixel
// cost class as every other image preset. Shows the shared placeholder
// ring until an image is actually loaded.

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

    float split = (0.003 + 0.007 * uDistortion) * (1.0 + uTreble * react * 0.8);
    float rC = sampleUserImage(iuv + vec2(split, 0.0)).r;
    vec4 baseC = sampleUserImage(iuv);
    float bC = sampleUserImage(iuv - vec2(split, 0.0)).b;
    vec3 imgCol = vec3(rC, baseC.g, bC);

    // Scan band sweeps down and wraps; wrap-aware distance so it loops
    // seamlessly instead of snapping at the edge.
    float scanY = fract(uTime * 0.15 - uBass * react * 0.08);
    float distToScan = abs(iuv.y - scanY);
    distToScan = min(distToScan, 1.0 - distToScan);
    float scanGlow = smoothstep(0.18, 0.0, distToScan);
    float scanLine = smoothstep(0.012, 0.0, distToScan);

    // Fine horizontal scanline texture for the projected-light look.
    float lines = 0.82 + 0.18 * sin(iuv.y * 420.0 - uTime * 2.0);

    vec3 col = vec3(0.004, 0.008, 0.016);
    float presence = baseC.a * lines * (0.4 + 0.6 * scanGlow);
    vec3 cyanTint = mix(vec3(0.55, 0.85, 1.0), vec3(1.0, 0.95, 0.9), scanGlow);

    col += imgCol * presence * cyanTint * (0.9 + uLevel * react * 0.5);
    col += vec3(0.6, 0.9, 1.0) * scanLine * 0.7;
    col += palette(uTime * 0.02, uHue) * scanGlow * baseC.a * 0.12;

    col += uOnset * react * 0.12 * vec3(0.5, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
