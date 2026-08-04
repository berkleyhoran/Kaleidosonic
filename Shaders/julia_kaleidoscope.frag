// Julia Kaleidoscope — a drifting Julia set folded through N mirror
// segments, diving continuously into the pattern while the "c" constant
// orbits, and the mid band widens the orbit.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    uv *= 1.4 * uCameraScale;

    float react = uReactivity;
    // Rotation/zoom motion is driven by Camera Shake, not Reactivity -- see
    // IFS Tunnel's comment for why.
    uv = rotate2d(uTime * 0.06 * uRotationSpeed + uOnset * uCameraShake * 0.4) * uv;
    uv = kaleidoscope(uv, uKaleidoscopeSegments);

    vec2 zc = zoomCycle(9.0, 0.7);
    float zoom = exp(-zc.x * 0.55) - uBass * uCameraShake * 0.4;
    uv *= zoom;

    float orbit = (0.72 + 0.2 * uMid * react) * mix(1.0, 1.15, clamp(uZoomWander * 0.5, 0.0, 1.0));
    float cAngle = uTime * 0.14 + uOnset * react * uCameraShake * 3.5;
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
    float hue = fract(uHue + t * 2.0 - uTime * 0.04 + uTreble * react * 1.3);
    float val = smoothstep(0.0, 1.0, t) * (0.5 + 0.8 * uLevel * react);
    vec3 col = hsv2rgb(vec3(hue, uSaturation, clamp(val, 0.0, 1.0)));

    col += uOnset * react * uCameraShake * 0.5 * vec3(0.7, 0.4, 1.0);
    col *= mix(0.25, 1.0, zc.y);

    fragColor = vec4(grade(col), 1.0);
}
