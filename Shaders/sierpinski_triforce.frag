// Sierpinski Triforce — a real Sierpinski gasket, crisply rendered, with a
// LITERALLY infinite zoom: the gasket is *exactly* self-similar under
// scaling by 2 about any of its corner vertices, so the dive is centered
// on the top corner and the zoom exponent wraps modulo log2(2) (see
// uZoomPhase in common.glsl). The image at phase 0 and phase 1 is pixel-
// identical, so the wrap is seamless and the zoom runs forever in plain
// float precision -- no deep-precision math, no resets, no walls, ever.
//
// Membership is the classic corner-fold test: repeatedly double the point
// away from its nearest corner; points in the gasket stay inside the big
// triangle forever, points in a hole leave at some fold depth ("level").
// Coloring by (level + uZoomPhase) is continuous across the wrap (a wrap
// shifts every pixel's level by exactly 1 while the phase drops by 1), and
// a signed-distance-to-triangle term carves crisp, antialiased edges.

// iq's equilateral-triangle SDF (triangle pointing up, centroid at the
// origin, circumradius 2r/sqrt(3); r = sqrt(3)/2 puts the top vertex at
// (0,1) and the base corners at (+-sqrt(3)/2, -0.5)).
float sdEquilateralTriangle(vec2 p, float r)
{
    const float k = 1.73205080757;
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0)
        p = vec2(p.x - k * p.y, -k * p.x - p.y) * 0.5;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    uv = rotate2d(uTime * 0.05 * uRotationSpeed) * uv;

    // The dive is anchored on the top corner -- the gasket's own fixed
    // point of self-similarity -- but the *frame* is shifted down into the
    // corner's 60-degree wedge so the fractal fills the view instead of
    // leaving 5/6 of the screen as empty background. The shift is
    // proportional to the extent, so the whole configuration scales
    // exactly with the zoom and the wrap stays pixel-perfect. Extent and
    // the Camera Scale clamp keep every visible pixel inside the top
    // corner's nearest-corner basin (roughly |p - corner| < 0.85), which
    // is what makes the wrap argument hold for every pixel on screen.
    const vec2 corner = vec2(0.0, 1.0);
    float extent = 0.62 * clamp(uCameraScale, 0.3, 1.0) / exp2(uZoomPhase);
    vec2 p = corner + (uv + vec2(0.0, -0.42)) * extent;

    const float r = 0.86602540378;
    const vec2 c1 = vec2(0.0, 1.0);
    const vec2 c2 = vec2(-r, -0.5);
    const vec2 c3 = vec2(r, -0.5);

    float iterMax = clamp(uIterations, 8.0, 44.0);
    float level = -1.0; // -1 = still in the set at full fold depth
    float sd = 0.0;

    for (float k = 0.0; k < 44.0; k += 1.0)
    {
        if (k >= iterMax)
            break;

        sd = sdEquilateralTriangle(p, r);
        if (sd > 0.0)
        {
            level = k;
            break;
        }

        // Fold: double the point away from its nearest corner. The gasket
        // maps exactly onto itself under this, so members never leave.
        float d1 = dot(p - c1, p - c1);
        float d2 = dot(p - c2, p - c2);
        float d3 = dot(p - c3, p - c3);
        vec2 c = c1;
        float dm = d1;
        if (d2 < dm) { c = c2; dm = d2; }
        if (d3 < dm) { c = c3; dm = d3; }
        p = c + (p - c) * 2.0;
    }

    float pix = extent / max(uResolution.y, 1.0);
    vec3 col;

    if (level < 0.0)
    {
        // In the gasket at full depth: the golden triforce body itself,
        // breathing with the audio level.
        vec3 gold = mix(palette(0.08 + uTime * 0.01, uHue), vec3(1.0, 0.85, 0.35), 0.4);
        col = gold * (0.85 + uLevel * react * 0.8 + uBass * react * 0.3);
    }
    else if (level < 0.5)
    {
        // Outside the big triangle entirely: a soft palette glow hugging
        // the gasket's silhouette instead of dead void. Depends only on
        // the world distance to the triangle (scale-free relative to the
        // extent), so it's seamless across the wrap too.
        float dOut = max(sd, 0.0) / max(extent, 1.0e-9);
        col = palette(0.5 + uTime * 0.012, uHue) * 0.14 * exp(-dOut * 2.2)
              * (0.7 + uLevel * react * 0.8);
    }
    else
    {
        // Inside a hole created at fold depth `level`. (level + uZoomPhase)
        // is continuous across the zoom wrap, so the colors dive seamlessly
        // forever along with the geometry.
        float depth = level + uZoomPhase;
        float dWorld = max(sd, 0.0) * exp2(-level);

        col = palette(depth * 0.11 + uTime * 0.012, uHue);
        float shade = 0.55 + 0.45 * sin(depth * 1.7 - uTime * 0.3);
        shade *= 1.0 + uBass * react * 0.3 * sin(depth * 3.14159 + uTime * 2.0);

        // Crisp dark crease along every triangle edge, antialiased at
        // exactly pixel scale no matter how deep the dive is.
        float edge = smoothstep(0.0, 1.5 * pix, dWorld);
        col *= shade * mix(0.04, 1.0, edge);
    }

    col += uOnset * react * uCameraShake * vec3(1.0, 0.85, 0.3) * 0.2;

    fragColor = vec4(grade(col), 1.0);
}
