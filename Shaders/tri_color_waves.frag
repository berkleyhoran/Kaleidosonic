// Tri-Color Waves — the same left-to-right trace idea as Waveform Scope,
// drawn three times with a small phase/vertical offset between each pass
// and a distinct hue per pass (a classic RGB-triad split, rotated by the
// Hue knob), so the single waveform reads as three colored beams weaving
// through each other -- a "misconverged CRT" look -- rather than one flat
// line. Distortion controls how far the three beams separate; at 0 they
// sit almost coincident (reading as a single bright near-white trace).

float waveGlow(vec2 uv, float phaseOffset, float ampScale, float onsetReact)
{
    float amp = texture(uWaveform, vec2(fract(uv.x + phaseOffset), 0.5)).r;
    float targetY = 0.5 + amp * ampScale;
    float dist = abs(uv.y - targetY);
    float lineWidth = 0.003 + 0.012 * onsetReact;
    return max(lineWidth / (dist + lineWidth * 0.3) - 1.0, 0.0);
}

void main()
{
    vec2 uv = vUv;
    float react = uReactivity;

    float ampScale = 0.26 * (0.5 + react) * (1.0 + uLevel * react * 2.0);
    // At Distortion = 0 the three passes nearly coincide (a single bright
    // trace); at 1 they fan out into three fully separate colored beams.
    float spread = 0.02 + uDistortion * 0.16;
    float phase = uTime * 0.02 * uZoomSpeed;
    float onsetReact = uOnset * react;

    float gR = waveGlow(vec2(uv.x, uv.y + spread * 0.6), phase - spread * 0.4, ampScale, onsetReact);
    float gG = waveGlow(uv,                              phase,                ampScale, onsetReact);
    float gB = waveGlow(vec2(uv.x, uv.y - spread * 0.6), phase + spread * 0.4, ampScale, onsetReact);

    vec3 tintR = hsv2rgb(vec3(fract(uHue + 0.0),                          uSaturation, 1.0));
    vec3 tintG = hsv2rgb(vec3(fract(uHue + 0.333 + uTreble * react * 0.1), uSaturation, 1.0));
    vec3 tintB = hsv2rgb(vec3(fract(uHue + 0.667),                        uSaturation, 1.0));

    vec3 col = vec3(0.01, 0.012, 0.02);
    float levelBoost = 0.8 + uLevel * react * 1.2;
    col += tintR * gR * levelBoost;
    col += tintG * gG * levelBoost;
    col += tintB * gB * levelBoost;

    // Hot near-white core wherever all three beams roughly coincide --
    // sells the "misconverged tri-color CRT" read instead of three
    // unrelated lines drifting past each other.
    col += vec3(1.0) * min(min(gR, gG), gB) * 0.6;

    // Faint center scope line, same restrained treatment as Waveform Scope.
    col += vec3(0.15, 0.25, 0.3) * smoothstep(0.0018, 0.0, abs(uv.y - 0.5)) * 0.25;

    col += onsetReact * 0.25 * vec3(0.9, 0.9, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
