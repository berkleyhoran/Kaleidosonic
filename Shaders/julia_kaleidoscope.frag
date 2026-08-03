// Julia Kaleidoscope — a drifting Julia set folded through N mirror segments,
// with the "c" constant orbiting in time and the mid band widening the orbit.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv *= 1.4;

    float react = uReactivity;
    uv = rotate2d(uTime * 0.05 * uRotationSpeed) * uv;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    float zoom = 1.0 - 0.35 * sin(uTime * 0.04 * (0.5 + uZoomSpeed)) - uBass * react * 0.25;
    uv *= zoom;

    float orbit = 0.72 + 0.06 * uMid * react;
    float cAngle = uTime * 0.12 + uOnset * react * 1.5;
    vec2 c = vec2(cos(cAngle), sin(cAngle * 1.3)) * orbit;

    vec2 z = uv + uDistortion * 0.3 * vec2(sin(uv.y * 5.0 + uTime), cos(uv.x * 5.0 + uTime));

    float iterMax = clamp(uIterations, 8.0, 200.0);
    float i = 0.0;
    for (float n = 0.0; n < 200.0; n += 1.0)
    {
        if (n >= iterMax || dot(z, z) > 4.0) break;
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        i += 1.0;
    }

    float t = i / iterMax;
    float hue = fract(uHue + t * 2.0 - uTime * 0.03 + uTreble * react * 0.4);
    float val = smoothstep(0.0, 1.0, t) * (0.6 + 0.4 * uLevel * react);
    vec3 col = hsv2rgb(vec3(hue, uSaturation, val));

    fragColor = vec4(grade(col), 1.0);
}
