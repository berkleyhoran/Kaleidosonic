// Mandelbrot Explorer — the classic set, rendered plainly with the sharp
// distance-estimate shading (black interior, crisp filament edges). No
// autopilot: mouse wheel / up-down arrows zoom, drag pans (see
// FractalNavigator's manual mode and exploreFractal in common.glsl).

void main()
{
    fragColor = vec4(exploreFractal(0, 1.0), 1.0);
}
