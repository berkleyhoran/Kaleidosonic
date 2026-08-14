// Radial Spectrum — the same real per-band FFT data as Spectrum Bars, bent
// into a ring of spokes radiating out from center and slowly spinning
// (Rotation Speed), so it reads as one of the kaleidoscope-flavored presets
// rather than a lab instrument. Cool blue/cyan/violet by default like
// Spectrum Bars, same fixed hue bias independent of the Hue knob.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;

    float react = uReactivity;
    float radius = length(uv) + 1e-4;
    float angle = atan(uv.y, uv.x) + uTime * 0.15 * uRotationSpeed;

    float barsF = float(kNumSpectrumBars);
    float angleNorm = fract(angle / 6.28318530718 + 0.5);
    float barIndexF = floor(angleNorm * barsF);
    float barU = (barIndexF + 0.5) / barsF;
    float cellA = fract(angleNorm * barsF);

    float barMask = smoothstep(0.5, 0.4, abs(cellA - 0.5));

    float amp = texture(uSpectrum, vec2(barU, 0.5)).r;
    amp = clamp(amp, 0.0, 1.6);

    float innerRadius = 0.16 + 0.05 * uBass * react;
    float spokeLength = amp * (0.55 + 0.35 * react) * (1.0 + uOnset * react * 0.6);
    float outerRadius = innerRadius + spokeLength;

    float spoke = step(innerRadius, radius) * smoothstep(outerRadius, outerRadius - 0.02, radius) * barMask;
    float glow = (0.03 / (radius - outerRadius + 0.04)) * barMask;
    glow = clamp(glow, 0.0, 1.2) * step(outerRadius, radius) * step(innerRadius, radius);

    float t = barIndexF / barsF;
    float coolHue = uHue + 0.55;
    vec3 spokeCol = palette(t * 0.8 + uTime * 0.03, coolHue);

    vec3 col = vec3(0.008, 0.01, 0.018);

    // Faint inner ring so the hub reads even when the spectrum is quiet.
    float hub = smoothstep(0.006, 0.0, abs(radius - innerRadius));
    col += vec3(0.12, 0.18, 0.26) * hub * 0.5;

    col += spokeCol * spoke * (0.9 + uLevel * react * 0.6);
    col += spokeCol * glow * 0.45;

    col += uOnset * react * 0.25 * palette(uTime * 0.05, coolHue) * smoothstep(0.5, 0.0, radius);

    fragColor = vec4(grade(col), 1.0);
}
