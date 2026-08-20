// Video Feedback — an analog video-synth "camera pointed at its own
// monitor" loop: each frame re-samples the previous frame through a
// zoom/rotate/translate transform (the same optical transform a real
// camera-on-monitor rig applies physically) and blends a small amount of
// fresh seed content back in, so the recursive image spirals, blooms, and
// drifts forever instead of fading to black. Cheap by construction -- one
// uPrevFrame sample and a couple of palette() calls, no loops -- so it
// costs about the same per pixel as Plasma Feedback despite reading richer.
//
// Growth/Rotation/Direction/Blend are mapped onto existing generic sliders
// rather than adding new automatable parameters -- same "repurpose a
// generic knob for this preset's own concept" convention Infinite Maze
// already uses for Zoom Speed/Camera Shake (-> walk speed/turn eagerness):
//   Zoom Speed        -> Growth: how fast each generation magnifies (>0,
//                         the classic feedback tunnel blooming outward) or
//                         recedes inward (<0, a black-hole pull)
//   Rotation Speed     -> Rotation: spin applied each generation
//   Distortion         -> Direction: 0..1 mapped to a full turn, the angle
//                         the loop drifts toward -- what makes the spiral
//                         pull off-center instead of sitting dead-still,
//                         exactly what a real rig gets from never being
//                         re-aimed at *perfectly* dead center
//   Feedback Amount    -> Blend: previous-frame vs. fresh-seed mix
// Camera Shake/Scale keep their usual meaning (audio kick strength / manual
// framing zoom), same as every other feedback-style preset.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv /= max(uCameraScale, 0.05);

    float react = uReactivity;

    // Growth: mirrors Plasma Feedback's own fbZoom convention -- positive
    // Zoom Speed magnifies (each generation zooms in a little, so content
    // visibly grows/blooms outward over many frames), negative recedes
    // (spirals inward instead).
    float growth = 0.012 + 0.05 * uZoomSpeed + uBass * react * uCameraShake * 0.05;
    float fbZoom = 1.0 - growth;

    float fbRot = (0.15 * uRotationSpeed + uOnset * react * uCameraShake * 0.12) * 0.15;

    // Direction: a full turn from Distortion, so the drift can point
    // anywhere around the compass rather than always toward one fixed side.
    float dirAngle = uDistortion * 6.28318530718;
    vec2 drift = vec2(cos(dirAngle), sin(dirAngle)) * (0.0035 + uMid * react * uCameraShake * 0.01);

    vec2 fbUv = rotate2d(fbRot) * (uv * fbZoom) + drift;
    vec2 prevUv = fbUv / vec2(uResolution.x / uResolution.y, 1.0) + 0.5;

    // Clamp rather than hard-cut -- same reasoning as Plasma Feedback -- an
    // out-of-frame sample just repeats its edge pixel instead of ever
    // collapsing the loop to black.
    vec3 prev = texture(uPrevFrame, clamp(prevUv, vec2(0.002), vec2(0.998))).rgb;

    // Continuous per-generation hue creep -- the classic analog-feedback
    // "chasing rainbow" color cycle, gentler than Plasma Feedback's own
    // since this preset leans on structure/spiral more than raw color.
    float prevLum = dot(prev, vec3(0.299, 0.587, 0.114));
    vec3 prevHueShifted = hsv2rgb(vec3(fract(uHue + uTime * 0.015 + prevLum * 0.1), uSaturation, prevLum));
    prev = mix(prev, prevHueShifted, 0.12);

    // Mild phosphor-style scanline darkening -- cheap, but it's the single
    // biggest "this reads as an analog video signal" tell.
    float scan = 0.94 + 0.06 * sin(vUv.y * uResolution.y * 3.14159265);
    prev *= scan;

    // Fresh seed: a small self-luminous pulsing dot plus a thin ring at
    // center -- what the feedback loop actually has to amplify into
    // spirals/tunnels. Without *some* seed the loop has nothing to feed on
    // and just decays; a real rig's seed is whatever's physically in front
    // of the camera (a light, the monitor's own glow) -- this is the
    // shader equivalent.
    float r = length(uv);
    float centerDot = smoothstep(0.05, 0.0, r) * (0.6 + uLevel * react * 1.4);
    float ring = smoothstep(0.02, 0.0, abs(r - 0.22)) * (0.3 + uOnset * react * 1.2);
    vec3 seedCol = palette(uTime * 0.02 + r * 0.3, uHue);
    vec3 fresh = seedCol * (centerDot + ring) * (0.5 + uTreble * react * 0.8);

    // Blend clamped a little tighter than the parameter range, fresh
    // contribution keeps a floor -- same self-sustaining guarantee as
    // Plasma Feedback, the loop can't decay to black even at high Blend.
    float blend = clamp(uFeedback, 0.0, 0.96);
    vec3 col = prev * blend + fresh * max(1.0 - blend, 0.12);

    // Onset bloom kick: a bright flash injected each beat so the loop
    // visibly "gets tapped" on transients, not just drifting smoothly.
    col += uOnset * react * 0.35 * palette(uTime * 0.03 + 0.5, uHue);

    fragColor = vec4(grade(col), 1.0);
}
