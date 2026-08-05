// Mandelbrot Pulse — kaleidoscope-folded, multi-scale Mandelbrot dive.
// Where to zoom is decided on the CPU (see FractalNavigator.h/.cpp): a
// bisection between a known-inside and known-outside point lands the lock
// ON the set boundary to the last ulp of a double, so the boundary -- where
// all the detail lives -- stays in frame at every zoom level of the entire
// dive, by construction. Rendered as three simultaneous scales of that
// same locked point (common.glsl's fractalLayer), kaleidoscope-folded and
// blended -- the "3D overlaying colors" look. Perturbation theory
// (perturbEscapeTime) renders each pixel as a tiny double-float offset
// from a CPU-computed reference orbit, which is what makes the dive
// genuinely deep (~1e11x) with no pixelation.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    float swirl = uDistortion * (0.25 + uTreble * react * 1.0);
    float ang = uTime * 0.04 * uRotationSpeed + swirl * length(uv);
    uv = rotate2d(ang) * uv;

    vec2 kuv = kaleidoscope(uv, uKaleidoscopeSegments);

    // Three simultaneous scales of the same locked boundary point, each
    // with its own hue offset so they read as distinct overlaid layers.
    // The wide layers are weighted down so they add supporting color and
    // texture without washing out the deep layer's fine detail.
    vec3 colA = fractalLayer(kuv, 0, 1.0, 0.0);
    vec3 colB = fractalLayer(kuv, 0, 9.0, 0.33);
    vec3 colC = fractalLayer(kuv, 0, 70.0, 0.67);

    vec3 col = colA + colB * 0.32 + colC * 0.2;
    col *= uLevel * react * 0.6 + 0.85;

    col += uOnset * react * uCameraShake * vec3(0.35, 0.2, 0.5) * smoothstep(0.0, 0.05, dot(col, col));
    col *= uFractalFade;

    fragColor = vec4(grade(col), 1.0);
}
