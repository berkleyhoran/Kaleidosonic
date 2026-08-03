#version 330 core

// Shared uniform contract for every preset fragment shader. PresetManager
// prepends this file's source to each preset's .frag body before compiling,
// so individual presets only need to implement main() using these.

in vec2 vUv;
out vec4 fragColor;

uniform vec2  uResolution;
uniform float uTime;             // seconds, wraps every ~1000s to keep float precision sane

// --- audio-reactive inputs, all driven from AudioAnalyzer, 0..1-ish ---
uniform float uBass;
uniform float uMid;
uniform float uTreble;
uniform float uLevel;            // overall RMS level
uniform float uOnset;            // transient/beat pulse, decays after each hit

// --- automatable visual parameters (APVTS) ---
uniform float uReactivity;       // 0..2 global multiplier on all audio inputs above
uniform float uZoomSpeed;        // -1..1
uniform float uRotationSpeed;    // -1..1
uniform float uHue;              // 0..1
uniform float uSaturation;       // 0..1
uniform float uBrightness;       // 0..2
uniform float uContrast;         // 0..2
uniform float uKaleidoscopeSegments; // 1..16
uniform float uFeedback;         // 0..1, only used by feedback-style presets
uniform float uIterations;       // 4..64, fractal detail
uniform float uDistortion;       // 0..1

uniform sampler2D uPrevFrame;    // previous rendered frame, for feedback presets

vec3 hsv2rgb(vec3 c)
{
    vec4 k = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + k.xyz) * 6.0 - k.www);
    return c.z * mix(k.xxx, clamp(p - k.xxx, 0.0, 1.0), c.y);
}

// Folds an angle into N radial kaleidoscope segments.
vec2 kaleidoscope(vec2 uv, float segments)
{
    float angle = atan(uv.y, uv.x);
    float radius = length(uv);
    float segAngle = 6.28318530718 / max(segments, 1.0);
    angle = mod(angle, segAngle);
    angle = abs(angle - segAngle * 0.5);
    return vec2(cos(angle), sin(angle)) * radius;
}

mat2 rotate2d(float a)
{
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

// Applies contrast/brightness/saturation grading driven by the automatable params.
vec3 grade(vec3 col)
{
    col = (col - 0.5) * uContrast + 0.5;
    col *= uBrightness;
    float g = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(g), col, uSaturation);
    return clamp(col, 0.0, 1.0);
}
