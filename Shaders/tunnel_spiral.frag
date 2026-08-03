// Tunnel Spiral — cheap, punchy polar spiral tunnel with kaleidoscope
// mirroring and hard audio-reactive color banding. Good "always readable"
// default preset.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    float react = uReactivity;
    float radius = length(uv) + 1e-4;
    float angle = atan(uv.y, uv.x);

    float spiralSpeed = uTime * (0.4 + 1.2 * uZoomSpeed) + uBass * react * 2.5;
    float spiral = angle * (2.0 + uDistortion * 6.0) + 1.0 / radius - spiralSpeed;

    float bands = clamp(uIterations, 4.0, 40.0);
    float band = floor(fract(spiral / 6.28318530718) * bands);
    float bandT = band / bands;

    float hue = fract(uHue + bandT + uTreble * react * 0.5 + uTime * 0.02 * uRotationSpeed);
    float val = 1.0 - smoothstep(0.0, 1.4, radius);
    val *= 0.6 + 0.4 * sin(bandT * 6.2831 + uOnset * react * 6.0);
    val += uOnset * react * 0.3;

    vec3 col = hsv2rgb(vec3(hue, uSaturation, clamp(val, 0.0, 1.0)));
    col *= 0.5 + 0.7 * uLevel * react;

    fragColor = vec4(grade(col), 1.0);
}
