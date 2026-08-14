// Tunnel Spiral — cheap, punchy polar spiral tunnel with kaleidoscope
// mirroring and hard audio-reactive color banding. Good "always readable"
// default preset, now with a much harder bass-driven spiral surge.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    float react = uReactivity;
    float radius = length(uv) + 1e-4;
    float angle = atan(uv.y, uv.x);

    // Spiral speed is driven by Camera Shake, not Reactivity -- see
    // Plasma Feedback's comment for why.
    float spiralSpeed = uTime * (0.4 + 1.2 * uZoomSpeed) + uBass * uCameraShake * 9.0 + uOnset * uCameraShake * 4.0;
    float spiral = angle * (2.0 + uDistortion * 6.0) + 1.0 / radius - spiralSpeed;

    float bands = clamp(uIterations, 4.0, 40.0);
    float band = floor(fract(spiral / 6.28318530718) * bands);
    float bandT = band / bands;

    float hue = fract(uHue + bandT + uTreble * react * 1.4 + uTime * 0.03 * uRotationSpeed + uTime * 0.02);
    float val = 1.0 - smoothstep(0.0, 1.4, radius * (1.0 - 0.35 * uBass * react));
    val *= 0.5 + 0.5 * sin(bandT * 6.2831 + uOnset * react * 8.0);
    val += uOnset * react * 0.7;

    vec3 col = hsv2rgb(vec3(hue, uSaturation, clamp(val, 0.0, 1.0)));
    col *= 0.3 + 1.7 * uLevel * react;

    fragColor = vec4(grade(col), 1.0);
}
