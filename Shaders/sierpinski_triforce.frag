// Sierpinski Triforce — the classic three-triangle Sierpinski gasket via
// an iterated "fold toward nearest corner" contraction, continuously
// diving into the nested triangles, with a golden flash on beats.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * 2.4 * uCameraScale;
    float react = uReactivity;

    uv = rotate2d(uTime * 0.04 * uRotationSpeed + uOnset * uCameraShake * 0.3) * uv;

    vec2 zc = zoomCycle(10.0, 0.55);
    float zoom = exp(-zc.x * 0.32);
    uv *= zoom;
    uv.y += 0.55 * zoom;

    vec2 wander = zoom * 0.5 * uZoomWander * vec2(sin(uTime * 0.07), cos(uTime * 0.05));
    uv += wander;

    vec2 corner1 = vec2(0.0, 1.0);
    vec2 corner2 = vec2(-0.866, -0.5);
    vec2 corner3 = vec2(0.866, -0.5);

    float scale = 2.0 + uBass * uCameraShake * 0.5;
    float iterMax = clamp(uIterations, 4.0, 24.0);
    vec2 p = uv;
    float n = 0.0;
    for (float k = 0.0; k < 24.0; k += 1.0)
    {
        if (k >= iterMax)
            break;
        vec2 nearest = corner1;
        float dist = length(p - corner1);
        float d2 = length(p - corner2);
        if (d2 < dist) { nearest = corner2; dist = d2; }
        float d3 = length(p - corner3);
        if (d3 < dist) { nearest = corner3; dist = d3; }
        p = nearest + (p - nearest) * scale;
        n += 1.0;
    }

    float d = length(p) * pow(scale, -n);
    float glow = pow(clamp(1.0 - d * 3.0, 0.0, 1.0), 2.0 + uDistortion * 4.0);

    float hue = fract(uHue + d * 2.0 + uMid * react * 0.8 + uTime * 0.018);
    float val = clamp(glow * (0.7 + uLevel * react * 1.4), 0.0, 1.0);
    vec3 col = hsv2rgb(vec3(hue, uSaturation, val));

    // Golden triforce flash on beats.
    col += uOnset * react * uCameraShake * glow * vec3(1.0, 0.85, 0.3);
    col *= mix(0.35, 1.0, zc.y);

    fragColor = vec4(grade(col), 1.0);
}
