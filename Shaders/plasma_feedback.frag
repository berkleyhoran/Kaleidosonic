// Plasma Feedback — classic video-feedback tunnel: each frame samples a
// zoomed/rotated/hue-shifted copy of the previous frame and blends in fresh
// plasma, so trails spiral and bloom outward hard on beats.

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
    // feedback loop spirals rather than just fading in place. Driven by
    // Camera Shake, not Reactivity -- coupling actual motion speed to the
    // same knob as color/brightness reactivity made high-Reactivity
    // settings feel like motion sickness. (Other presets with the same
    // Camera-Shake-not-Reactivity motion split point back here.)
    float fbZoom = 1.0 - (0.012 + 0.02 * uZoomSpeed + uBass * uCameraShake * 0.06);
    float fbRot = 0.01 * uRotationSpeed + uOnset * uCameraShake * 0.06;
    vec2 fbUv = rotate2d(fbRot) * (uv * fbZoom);
    vec2 prevUv = fbUv / vec2(uResolution.x / uResolution.y, 1.0) + 0.5;

    // Clamp instead of hard-cutting to black: a zoomed-out or rotated
    // sample that lands outside the texture just repeats its edge pixel,
    // so the feedback loop can never "eat itself" into a black screen.
    vec3 prev = texture(uPrevFrame, clamp(prevUv, vec2(0.002), vec2(0.998))).rgb;

    // Continuous hue drift on the trailing feedback for a psychedelic crawl.
    float prevLum = dot(prev, vec3(0.299, 0.587, 0.114));
    vec3 prevHsvShift = hsv2rgb(vec3(fract(uHue + uTime * 0.025), uSaturation, prevLum));
    prev = mix(prev, prevHsvShift, 0.18);

    float p = plasma(uv * (1.0 + uDistortion * 2.5), uTime * (0.3 + 0.4 * uZoomSpeed) + uMid * react * 6.0);
    float hue = fract(uHue + p * 0.5 + uTreble * react * 1.1 + uTime * 0.01);
    float val = 0.5 + 0.5 * sin(p * 3.14159 + uOnset * react * 10.0);
    vec3 fresh = hsv2rgb(vec3(hue, uSaturation, val)) * (0.3 + uLevel * react * 2.0);

    // Feedback amount is clamped a little tighter than the parameter range
    // and fresh contribution always keeps a floor, so the loop is
    // self-sustaining and can't decay to black even at high Feedback.
    float feedback = clamp(uFeedback, 0.0, 0.95);
    vec3 col = prev * feedback + fresh * max(1.0 - feedback, 0.18);

    col += uOnset * react * 0.5 * vec3(1.0, 0.6, 0.9);

    fragColor = vec4(grade(col), 1.0);
}
