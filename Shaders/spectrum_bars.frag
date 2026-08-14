// Spectrum Bars — the classic analyzer look: real per-band FFT magnitude
// (uSpectrum, see AudioAnalyzer::numSpectrumBars) as vertical bars, low
// frequency on the left, mirrored symmetrically above and below a center
// line so each bar reads as a single glowing column growing outward from
// the middle rather than a bottom-anchored bar chart. Cool blue/cyan/violet
// by default (a fixed hue bias on top of the palette system, independent of
// the Hue knob) per an explicit "cool colors" request; still fully explorable
// via Hue/Palette like every other preset.

void main()
{
    vec2 uv = vUv;
    float react = uReactivity;

    float barsF = float(kNumSpectrumBars);
    float barIndexF = floor(uv.x * barsF);
    float barU = (barIndexF + 0.5) / barsF;
    float cellX = fract(uv.x * barsF);

    // Small gap between bars -- solid column in the middle of each cell,
    // fading out (not hard-edged) toward the gap so it doesn't alias.
    float barMask = smoothstep(0.5, 0.42, abs(cellX - 0.5));

    float amp = texture(uSpectrum, vec2(barU, 0.5)).r;
    amp = clamp(amp, 0.0, 1.6);

    // Punchier bass/onset kick, same reactive shape as every other preset.
    float halfHeight = amp * (0.34 + 0.18 * react) * (1.0 + uOnset * react * 0.5);

    float distFromCenter = abs(uv.y - 0.5);
    float barShape = smoothstep(halfHeight, halfHeight - 0.01, distFromCenter) * barMask;

    // Soft glow beyond the bar's hard edge, brighter near the center line.
    float glow = (0.02 / (distFromCenter - halfHeight + 0.03)) * barMask;
    glow = clamp(glow, 0.0, 1.5) * step(halfHeight, distFromCenter);

    float t = barIndexF / barsF;
    float coolHue = uHue + 0.55; // bias toward blue/cyan/violet by default
    vec3 barCol = palette(t * 0.8 + uTime * 0.02, coolHue);

    vec3 col = vec3(0.01, 0.012, 0.02);

    // Faint center line so the mirror axis reads even when bars are quiet.
    col += vec3(0.1, 0.16, 0.24) * smoothstep(0.0015, 0.0, distFromCenter) * 0.4;

    col += barCol * barShape * (0.9 + uLevel * react * 0.6);
    col += barCol * glow * 0.5;

    col += uOnset * react * 0.25 * palette(uTime * 0.05, coolHue);

    fragColor = vec4(grade(col), 1.0);
}
