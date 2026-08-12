// Image Kaleidoscope — the uploaded picture folded through the standard
// kaleidoscope() mirror and tiled into an endless mosaic tunnel (adjacent
// tiles mirrored so a non-tileable photo still joins seamlessly at every
// edge). Deliberately a bounded, breathing zoom rather than an unbounded
// dive: a plain texture sample has no fractal detail to reveal by going
// deeper, so there's nothing to gain from the precision machinery the
// fractal presets need -- just a bass-driven pulse in and out. Treble
// adds a faint chromatic shimmer at the tile seams. Shows a pulsing
// placeholder until an image is actually loaded.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0) * uCameraScale;

    if (uUserImageLoaded < 0.5)
    {
        fragColor = vec4(grade(imagePlaceholder(uv)), 1.0);
        return;
    }

    float react = uReactivity;
    uv = rotate2d(uTime * 0.05 * uRotationSpeed) * uv;
    vec2 kuv = kaleidoscope(uv, uKaleidoscopeSegments);

    float zoom = 1.4 + 0.5 * sin(uTime * 0.15) + uBass * react * 0.35;
    float tileScale = 1.2 + uZoomWander * 0.6;
    vec2 tiled = kuv * zoom * tileScale;

    vec2 cell = floor(tiled + 0.5);
    vec2 localUV = fract(tiled + 0.5);
    vec2 mirror = mod(cell, 2.0);
    vec2 sampleUV = mix(localUV, 1.0 - localUV, mirror);

    float shimmer = uTreble * react * 0.01;
    vec4 imgC = sampleUserImage(sampleUV);
    float r = sampleUserImage(sampleUV + vec2(shimmer, 0.0)).r;
    float b = sampleUserImage(sampleUV - vec2(shimmer, 0.0)).b;
    vec3 col = vec3(r, imgC.g, b);

    col *= 0.85 + uLevel * react * 0.35;
    col += uOnset * react * uCameraShake * 0.15 * palette(uTime * 0.02, uHue);

    fragColor = vec4(grade(col), 1.0);
}
