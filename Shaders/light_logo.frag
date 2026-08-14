// Rotating Light Logo — the uploaded picture (or GIF) redrawn as a disc of
// individually-lit points, like a light-bulb marquee sign, spinning in 3D
// while the camera drifts around it. Each point's brightness comes from
// the picture at its own grid cell, and every point breathes on its own
// sine-wave phase (per-cell hashed, so they don't all pulse in lockstep) --
// bass/onset kick the whole grid's brightness and size. Shows a procedural
// pinwheel placeholder until an image is actually loaded, same as the
// other image-reactive presets.
//
// Deliberately an analytic ray-*plane* intersection, not a raymarched
// heightfield: an earlier preset this project shipped (Shape Rave) learned
// the hard way that anything a raymarcher steps through has to stay a
// genuine distance estimator, and a grid of discontinuous per-cell bumps
// is not one -- sphere-tracing through it would risk the exact overshoot/
// checkerboard artifacts that took two real fixes to chase down there. A
// flat plane has no such risk (the intersection is a closed-form formula,
// nothing to step through), so the "points floating in 3D, camera orbiting
// around them" read comes from the camera motion, and the "alive" read
// comes from each point's own brightness/size breathing -- not from
// physically displacing the surface.

float llHash(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453123); }

// What the disc shows before an image is loaded: a slow pinwheel of spokes
// fading toward the rim, so there's always something to look at.
float placeholderLum(vec2 uv)
{
    float r = length(uv);
    float ang = atan(uv.y, uv.x);
    float spokes = 0.5 + 0.5 * sin(ang * 7.0 - uTime * 0.6 + r * 3.0);
    float fade = smoothstep(1.0, 0.05, r);
    return spokes * fade;
}

const float kDiskRadius = 6.0;
const float kGridN = 42.0; // points across the disc's diameter

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Camera orbits at a fairly steep angle so the disc reads as a tilted
    // sign rather than a flat coin, with a slow bob for extra life.
    float orbitAngle = uTime * 0.16 * (0.35 + abs(uRotationSpeed));
    float camHeight = 4.0 + 1.2 * sin(uTime * 0.11);
    float dist = 11.0 / max(uCameraScale, 0.05);
    vec3 ro = vec3(sin(orbitAngle), 0.0, cos(orbitAngle)) * dist + vec3(0.0, camHeight, 0.0);
    vec3 lookAt = vec3(0.0);
    vec3 forward = normalize(lookAt - ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.6 + right * uv.x + up * uv.y);

    // Void background: a faint palette-tinted ambient glow, bass-breathing,
    // so the backdrop isn't flatly dead even where no point is lit.
    vec3 col = vec3(0.01, 0.01, 0.025) + palette(uTime * 0.01, uHue) * 0.015 * (0.5 + uBass * react);

    if (abs(rd.y) > 1.0e-5)
    {
        float t = -ro.y / rd.y;
        if (t > 0.0)
        {
            vec3 p = ro + rd * t;
            float r = length(p.xz) / kDiskRadius;
            if (r < 1.0)
            {
                // The disc itself spins around its own normal -- the
                // "rotating" in Rotating Light Logo -- independent of the
                // camera's own slower orbit.
                float spin = uTime * 0.35 * (0.4 + abs(uRotationSpeed));
                vec2 spun = rotate2d(spin) * p.xz;

                vec2 diskUV = spun / kDiskRadius;
                vec2 cellUv = diskUV * kGridN * 0.5;
                vec2 cellId = floor(cellUv);
                vec2 cellFrac = fract(cellUv) - 0.5;
                vec2 cellCenterUV = (cellId + 0.5) / (kGridN * 0.5);

                float lum;
                vec3 pointColor;
                if (uUserImageLoaded > 0.5)
                {
                    vec4 imgC = sampleUserImage(imageContainUV(cellCenterUV, uUserImageAspect));
                    lum = dot(imgC.rgb, vec3(0.299, 0.587, 0.114)) * imgC.a;
                    pointColor = imgC.rgb;
                }
                else
                {
                    lum = placeholderLum(cellCenterUV);
                    pointColor = palette(llHash(cellId) + uTime * 0.03, uHue);
                }

                // Per-point sine-wave breathing, each cell on its own
                // hashed phase/speed so the whole grid twinkles rather
                // than pulsing as one block. Bass/onset scale it up.
                float phase = llHash(cellId + 5.0) * 6.2831853;
                float speed = mix(0.7, 2.0, llHash(cellId + 13.0));
                float breathe = 0.55 + 0.45 * sin(uTime * speed + phase);
                float pulse = 1.0 + uBass * react * 0.7 + uOnset * react * uCameraShake * 1.3;

                float pointRadius = clamp(lum * 0.5 * breathe * pulse, 0.0, 0.48);
                float pointMask = 1.0 - smoothstep(pointRadius - 0.08, pointRadius, length(cellFrac));

                // Fade the whole disc softly at its rim instead of a hard
                // circular cutoff.
                float rimFade = smoothstep(1.0, 0.85, r);

                col += pointColor * pointMask * (0.8 + uLevel * react * 0.6) * rimFade;
            }
        }
    }

    col += uOnset * react * uCameraShake * 0.12 * vec3(0.8, 0.7, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
