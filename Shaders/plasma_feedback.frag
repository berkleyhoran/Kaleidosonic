// Plasma Feedback — classic video-feedback tunnel: each frame samples a
// zoomed/rotated/hue-shifted copy of the previous frame and blends in fresh
// plasma, so trails spiral and bloom outward on beats.

float plasma(vec2 uv, float t)
{
    float v = 0.0;
    v += sin(uv.x * 6.0 + t);
    v += sin(uv.y * 6.0 - t * 1.3);
    v += sin((uv.x + uv.y) * 5.0 + t * 0.7);
    v += sin(length(uv) * 8.0 - t * 2.0);
    return v * 0.25;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    float react = uReactivity;

    // Sample the previous frame through a zoom/rotate transform so the
    // feedback loop spirals rather than just fading in place.
    float fbZoom = 1.0 - (0.012 + 0.02 * uZoomSpeed + uBass * react * 0.02);
    float fbRot = 0.01 * uRotationSpeed + uOnset * react * 0.03;
    vec2 fbUv = rotate2d(fbRot) * (uv * fbZoom);
    vec2 prevUv = fbUv / vec2(uResolution.x / uResolution.y, 1.0) + 0.5;

    vec3 prev = vec3(0.0);
    if (prevUv.x > 0.0 && prevUv.x < 1.0 && prevUv.y > 0.0 && prevUv.y < 1.0)
        prev = texture(uPrevFrame, prevUv).rgb;

    // Slowly rotate hue of the trailing feedback for a psychedelic drift.
    float prevLum = dot(prev, vec3(0.299, 0.587, 0.114));
    vec3 prevHsvShift = hsv2rgb(vec3(fract(uHue + uTime * 0.015), uSaturation, prevLum));
    prev = mix(prev, prevHsvShift, 0.15);

    float p = plasma(uv * (1.0 + uDistortion * 2.0), uTime * (0.3 + 0.4 * uZoomSpeed) + uMid * react * 2.0);
    float hue = fract(uHue + p * 0.5 + uTreble * react * 0.5);
    float val = 0.5 + 0.5 * sin(p * 3.14159 + uOnset * react * 5.0);
    vec3 fresh = hsv2rgb(vec3(hue, uSaturation, val)) * (0.25 + uLevel * react);

    float feedback = clamp(uFeedback, 0.0, 0.98);
    vec3 col = prev * feedback + fresh * (1.0 - feedback * 0.6);

    fragColor = vec4(grade(col), 1.0);
}
