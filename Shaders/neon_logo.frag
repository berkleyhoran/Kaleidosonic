// Neon Logo — the uploaded picture/logo redrawn as a glowing neon-tube
// outline, mounted on a flat card that spins around its own vertical (Y)
// axis in real 3D -- a genuine ray-*plane* intersection (see Rotating
// Light Logo's header comment for why that's the safe way to do a flat
// object in a raymarch-free preset: a closed-form intersection, nothing
// to step through, no sphere-tracing overshoot risk), not a flat 2D pass.
// The card dims toward its rim as it turns edge-on (a cheap Fresnel-style
// term from the ray/normal angle), which is what actually sells the
// "spinning in space" read.
//
// The outline itself is the same cheap 4-tap luminance-gradient edge trace
// as before, left for the existing global Bloom Intensity post-FX to
// actually bloom. Flickers like a real neon tube on a hashed per-second
// schedule, with onsets forcing an extra-hard flicker.

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    if (uUserImageLoaded < 0.5)
    {
        fragColor = vec4(grade(imagePlaceholder(uv)), 1.0);
        return;
    }

    // Simple fixed pinhole camera looking down +Z; the card itself spins,
    // not the camera -- a literal "logo on a turntable" rather than an
    // orbiting-camera read.
    vec3 ro = vec3(0.0, 0.0, -3.2 / max(uCameraScale, 0.05));
    vec3 rd = normalize(vec3(uv, 1.4));

    float spin = uTime * (0.5 + abs(uRotationSpeed) * 1.5) * (1.0 + uOnset * react * 0.6);
    float cs = cos(spin), sn = sin(spin);
    vec3 right = vec3(cs, 0.0, sn);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 planeNormal = vec3(-sn, 0.0, cs);

    vec3 col = vec3(0.006, 0.006, 0.012);

    float denom = dot(rd, planeNormal);
    if (abs(denom) > 1.0e-5)
    {
        float t = -dot(ro, planeNormal) / denom;
        if (t > 0.0)
        {
            vec3 hit = ro + rd * t;
            float localX = dot(hit, right);
            float localY = dot(hit, up);

            const float cardHalf = 1.15;
            if (abs(localX) < cardHalf && abs(localY) < cardHalf)
            {
                // Scaled so the card's own ±cardHalf extent lines up with
                // imageContainUV's own ±1-ish "fully visible" range (the
                // same convention every other image preset's screen-space
                // uv is already in) instead of showing the picture tiny in
                // the middle of a mostly-empty card.
                vec2 iuv = imageContainUV(vec2(localX, localY) / cardHalf, uUserImageAspect);

                float eps = 0.0022 + 0.0018 * uDistortion;
                vec4 cL = sampleUserImage(iuv - vec2(eps, 0.0));
                vec4 cR = sampleUserImage(iuv + vec2(eps, 0.0));
                vec4 cD = sampleUserImage(iuv - vec2(0.0, eps));
                vec4 cU = sampleUserImage(iuv + vec2(0.0, eps));

                float lL = dot(cL.rgb, vec3(0.299, 0.587, 0.114)) * cL.a;
                float lR = dot(cR.rgb, vec3(0.299, 0.587, 0.114)) * cR.a;
                float lD = dot(cD.rgb, vec3(0.299, 0.587, 0.114)) * cD.a;
                float lU = dot(cU.rgb, vec3(0.299, 0.587, 0.114)) * cU.a;

                vec2 grad = vec2(lR - lL, lU - lD);
                float edge = length(grad) * 2.2;
                edge = smoothstep(0.08, 0.4, edge);

                // Thin neon frame right at the card's own boundary, so the
                // spinning object reads as a discrete card even where the
                // image content itself has no interesting edges near the
                // rim.
                float frameDist = cardHalf - max(abs(localX), abs(localY));
                float frame = smoothstep(0.05, 0.0, frameDist);
                edge = max(edge, frame);

                // Real neon-tube flicker: mostly steady, occasional hashed
                // dips, harder flicker forced on every onset.
                float flickerSeed = fract(sin(floor(uTime * 5.0) * 12.9898) * 43758.5453123);
                float flicker = flickerSeed > 0.88 ? mix(0.35, 0.7, fract(flickerSeed * 7.0)) : 1.0;
                flicker *= 1.0 - uOnset * react * 0.25;

                // Fresnel-style rim dim: face-on (denom near -1, since rd
                // points +Z-ish into the plane) reads full brightness,
                // edge-on (denom near 0) dims toward the card's own thin
                // silhouette, exactly what sells "this is a flat card
                // turning in 3D" rather than a flat 2D overlay.
                float facing = clamp(-denom, 0.15, 1.0);

                float hueT = uTime * 0.03 + uTreble * react * 0.4;
                vec3 neonCol = palette(hueT, uHue);

                col += neonCol * 0.02 * (0.5 + uBass * react) * facing;
                float lineBright = edge * flicker * facing * (0.9 + uLevel * react * 0.6);
                col += neonCol * lineBright;
                col += vec3(1.0) * pow(edge, 4.0) * flicker * facing * 0.5;
            }
        }
    }

    col += uOnset * react * 0.15 * palette(uTime * 0.03, uHue) * 0.3;

    fragColor = vec4(grade(col), 1.0);
}
