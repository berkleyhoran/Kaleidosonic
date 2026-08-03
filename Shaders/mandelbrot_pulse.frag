// Mandelbrot Pulse — classic escape-time fractal that breathes and zooms
// with the bass, with treble adding coordinate swirl and hue racing with uTime.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

    float react = uReactivity;
    float zoomT = uTime * (0.06 + 0.25 * uZoomSpeed) + uBass * react * 1.5;
    float zoom = exp(-1.4 - zoomT * 0.35 + 0.6 * sin(uTime * 0.05));

    float swirl = uDistortion * (0.4 + uTreble * react) * 2.0;
    float ang = uTime * 0.08 * uRotationSpeed + swirl * length(uv);
    uv = rotate2d(ang) * uv;

    vec2 c = uv * zoom + vec2(-0.745, 0.11);

    vec2 z = vec2(0.0);
    float iterMax = clamp(uIterations, 8.0, 256.0);
    float i = 0.0;
    for (float n = 0.0; n < 256.0; n += 1.0)
    {
        if (n >= iterMax || dot(z, z) > 16.0) break;
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        i += 1.0;
    }

    float escaped = step(i, iterMax - 1.0);
    float smoothI = i - log2(max(log2(dot(z, z) + 1e-6), 1e-6));
    float t = smoothI / iterMax;

    float hue = fract(uHue + t * 1.5 + uTime * 0.02 + uMid * react * 0.3);
    float val = escaped * (0.35 + 0.65 * sin(t * 6.2831 + uOnset * react * 6.0));
    vec3 col = hsv2rgb(vec3(hue, uSaturation, clamp(val, 0.0, 1.0)));

    col += uOnset * react * vec3(0.25, 0.15, 0.35) * (1.0 - t);

    fragColor = vec4(grade(col), 1.0);
}
