// Burning Ship — kaleidoscope-folded, multi-scale dive into the Burning
// Ship set, z = (|Re z| + i|Im z|)^2 + c. Same CPU boundary-bisection lock
// and three-scale perturbation overlay as Mandelbrot Pulse (see its
// comment and FractalNavigator.h/.cpp) -- the lock sits ON the boundary to
// machine precision, so the spiky filament detail stays in frame at every
// depth of the dive.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    float swirl = uDistortion * (0.2 + uTreble * react * 1.0);
    float ang = uTime * 0.035 * uRotationSpeed + swirl * length(uv);
    uv = rotate2d(ang) * uv;

    vec2 kuv = kaleidoscope(uv, uKaleidoscopeSegments);

    vec3 colA = fractalLayer(kuv, 1, 1.0, 0.0);
    vec3 colB = fractalLayer(kuv, 1, 10.0, 0.3);
    vec3 colC = fractalLayer(kuv, 1, 80.0, 0.6);

    vec3 col = colA + colB * 0.32 + colC * 0.2;
    col *= uLevel * react * 0.5 + 0.9;

    col += uOnset * react * uCameraShake * vec3(0.6, 0.28, 0.05) * smoothstep(0.0, 0.05, dot(col, col));
    col *= uFractalFade;

    fragColor = vec4(grade(col), 1.0);
}
