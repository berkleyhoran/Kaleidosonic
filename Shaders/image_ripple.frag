// Image Ripple — the user's uploaded picture (see common.glsl's
// sampleUserImage/imageContainUV, and PluginEditor's "Load Image...")
// rippling like it's underwater: concentric sine displacement of the
// sampling UV, bass driving a slow deep swell, treble a fast fine ripple,
// and every onset spawning a fresh expanding ring. A tiny chromatic
// offset scaled by the local ripple strength reads as light refracting
// through the water surface. Shows a pulsing placeholder until an image
// is actually loaded.

void main()
{
    vec2 screenUV = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;

    if (uUserImageLoaded < 0.5)
    {
        fragColor = vec4(grade(imagePlaceholder(screenUV)), 1.0);
        return;
    }

    float react = uReactivity;
    vec2 imgUV = imageContainUV(screenUV, uUserImageAspect);

    float dist = length(screenUV);
    float angle = atan(screenUV.y, screenUV.x);
    vec2 dir = dist > 1.0e-4 ? screenUV / dist : vec2(0.0);

    float t = uTime * (0.5 + 0.5 * clamp(uZoomSpeed, 0.0, 1.0));
    float swell = sin(dist * 24.0 - t * 3.0) * (0.006 + uBass * react * 0.016);
    float fine = sin(dist * 64.0 - t * 7.0 + angle * 3.0) * (0.002 + uTreble * react * 0.007);
    float onsetRing = uOnset * react * 0.022 * sin(dist * 42.0 - t * 11.0);
    float ripple = swell + fine + onsetRing;

    vec2 displaced = imgUV + dir * ripple;
    float chroma = clamp(abs(ripple) * 55.0, 0.0, 1.0) * (0.002 + uDistortion * 0.004);
    vec4 imgC = sampleUserImage(displaced);
    vec4 imgR = sampleUserImage(displaced + dir * chroma);
    vec4 imgB = sampleUserImage(displaced - dir * chroma);

    vec3 col = vec3(imgR.r, imgC.g, imgB.b);
    float alpha = imgC.a;

    // Outside the image's own edge (letterboxed area): a soft palette glow
    // instead of dead void, echoing the placeholder look.
    vec3 background = palette(dist * 0.4 + uTime * 0.015, uHue) * 0.12;
    col = mix(background, col, alpha);
    col *= 0.85 + uLevel * react * 0.3;

    fragColor = vec4(grade(col), 1.0);
}
