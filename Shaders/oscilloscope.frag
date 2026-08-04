// Oscilloscope Glow — the actual audio waveform (uWaveform) traced radially
// around the center instead of left-to-right, so it reads as an
// audio-driven mandala rather than a flat lab scope. Kaleidoscope segments
// repeat it around the circle for symmetry, and the ring pulses hard with
// bass and beats.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv = rotate2d(uTime * 0.025 * uRotationSpeed) * uv;

    float react = uReactivity;
    float segs = max(uKaleidoscopeSegments, 1.0);

    float angle = atan(uv.y, uv.x);
    float radius = length(uv);

    float segAngle = 6.28318530718 / segs;
    float foldedAngle = mod(angle, segAngle);
    float sampleX = foldedAngle / segAngle;

    float amp = texture(uWaveform, vec2(sampleX, 0.5)).r;
    float baseRadius = 0.4 + 0.12 * sin(uTime * 0.1 * (0.5 + uZoomSpeed)) + uBass * react * 0.25;
    float ampScale = 0.3 * (0.5 + uReactivity) * (1.0 + uLevel * react * 2.5);
    float targetRadius = baseRadius + amp * ampScale + uDistortion * 0.15 * sin(angle * 6.0 + uTime);
    targetRadius *= 1.0 + uOnset * react * 0.35;

    float dist = abs(radius - targetRadius);
    float lineWidth = 0.006 + 0.02 * uOnset * react;
    float glow = lineWidth / (dist + lineWidth * 0.3) - 1.0;
    glow = max(glow, 0.0);

    float hue = fract(uHue + angle / 6.28318530718 + uTime * 0.04 + uTreble * react * 0.7);
    vec3 lineCol = hsv2rgb(vec3(hue, uSaturation, 1.0));

    vec3 col = vec3(0.012, 0.018, 0.03);
    col += lineCol * glow * (0.7 + uLevel * react * 1.6);

    float under = smoothstep(targetRadius, targetRadius - 0.35, radius);
    col += lineCol * under * 0.06;
    col += hsv2rgb(vec3(fract(uHue + 0.5 + uTime * 0.02), uSaturation * 0.6, 1.0)) * 0.07 / (radius * 6.0 + 0.3);

    col += uOnset * react * 0.4 * vec3(0.6, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
