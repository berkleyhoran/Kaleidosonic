// Image Feedback Zoom — the uploaded picture fed through the same
// previous-frame feedback loop Plasma Feedback/Video Feedback use, except
// the picture itself (not procedural noise) is what gets blended back in
// on top each generation -- so the recursed, receding, rotating trail
// behind it evokes the classic Droste "picture containing a picture"
// effect instead of a plasma/noise tunnel. Feedback Amount controls how
// much of the crisp source photo shows through versus how deep the
// recursion reads. Shows a pulsing placeholder until an image is loaded.

void main()
{
    vec2 screenUV = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    if (uUserImageLoaded < 0.5)
    {
        fragColor = vec4(grade(imagePlaceholder(screenUV * uCameraScale)), 1.0);
        return;
    }

    float react = uReactivity;

    // Feedback transform: same shape as Plasma/Video Feedback's own loop
    // (zoom + rotate + bass kick), Zoom Speed always biased toward
    // magnifying since a Droste-style zoom only reads correctly recursing
    // one direction (inward/growing, not shrinking away).
    float fbZoom = 1.0 - (0.02 + 0.015 * clamp(uZoomSpeed, -1.0, 1.0) + uBass * react * uCameraShake * 0.03);
    float fbRot = 0.01 * uRotationSpeed + uOnset * react * uCameraShake * 0.05;
    vec2 fbUv = rotate2d(fbRot) * (screenUV * fbZoom);
    vec2 prevUv = fbUv / vec2(uResolution.x / uResolution.y, 1.0) + 0.5;
    vec3 prev = texture(uPrevFrame, clamp(prevUv, vec2(0.002), vec2(0.998))).rgb;

    vec2 imgUV = imageContainUV(screenUV * uCameraScale, uUserImageAspect);
    vec4 img = sampleUserImage(imgUV);

    // Where the actual picture has content, it overwrites the recursed
    // feedback (mixed down a little as Feedback Amount rises, so more of
    // the recursion peeks through even under the crisp source) -- what
    // makes each generation read as nesting *inside* the picture instead
    // of the picture just dissolving into a blurred trail.
    float feedbackMix = clamp(uFeedback, 0.0, 0.95);
    vec3 col = mix(prev, img.rgb, img.a * mix(1.0, 0.65, feedbackMix));

    col *= 0.88 + uLevel * react * 0.3;
    col += uOnset * react * 0.06 * palette(uTime * 0.03, uHue);

    fragColor = vec4(grade(col), 1.0);
}
