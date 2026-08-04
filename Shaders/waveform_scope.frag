// Waveform Scope — the classic left-to-right lab oscilloscope trace (as
// opposed to Oscilloscope Glow's radial mandala version), with a soft grid
// and a glowing fill under the trace.

void main()
{
    vec2 uv = vUv;
    float react = uReactivity;

    float amp = texture(uWaveform, vec2(uv.x, 0.5)).r;
    float ampScale = 0.32 * (0.5 + react) * (1.0 + uLevel * react * 2.0);
    float targetY = 0.5 + amp * ampScale;

    float dist = abs(uv.y - targetY);
    float lineWidth = 0.0035 + 0.014 * uOnset * react;
    float glow = lineWidth / (dist + lineWidth * 0.3) - 1.0;
    glow = max(glow, 0.0);

    float hue = fract(uHue + uv.x * 0.25 + uTime * 0.04 + uTreble * react * 0.6);
    vec3 lineCol = hsv2rgb(vec3(hue, uSaturation, 1.0));

    vec3 col = vec3(0.008, 0.012, 0.02);

    // Faint scope grid: a center line plus a few horizontal divisions.
    float grid = smoothstep(0.0015, 0.0, abs(uv.y - 0.5));
    for (int d = 1; d <= 3; ++d)
    {
        float off = float(d) * 0.15;
        grid += smoothstep(0.0012, 0.0, abs(uv.y - 0.5 - off));
        grid += smoothstep(0.0012, 0.0, abs(uv.y - 0.5 + off));
    }
    col += vec3(0.15, 0.25, 0.3) * grid * 0.3;

    col += lineCol * glow * (0.7 + uLevel * react * 1.6);

    float under = smoothstep(targetY, targetY - 0.4, uv.y);
    col += lineCol * under * 0.05;

    col += uOnset * react * 0.3 * vec3(0.6, 0.8, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
