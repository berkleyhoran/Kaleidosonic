// Wave Sphere — the audio waveform (uWaveform) wrapped around a raymarched
// sphere's own surface as radial displacement, sampled by azimuthal angle
// around the body, so the whole ball visibly ripples and spikes with the
// waveform's actual shape instead of the flat left-to-right or radial-2D
// reads the other waveform presets give. Genuinely 3D -- lit from an SDF-
// gradient normal, same raymarch/lighting shape as Metaballs, camera slowly
// orbiting a fixed structure. Iteration cap kept modest (90, same ballpark
// as Metaballs' 110) since it's a single smooth body, not a multi-blob CSG.

float sphereDE(vec3 p, float react)
{
    float radiusBase = 1.0 + uBass * react * 0.15;
    vec3 dir = normalize(p + 1.0e-6);
    // Azimuthal angle around the Y axis, 0..1 -- what uWaveform's x-axis
    // gets wrapped onto.
    float azimuth = atan(dir.x, dir.z) / 6.28318530718 + 0.5;
    float amp = texture(uWaveform, vec2(fract(azimuth + uTime * 0.01 * uZoomSpeed), 0.5)).r;
    // Latitude fade: displacement strongest at the equator, tapering
    // toward the poles, so the ripple reads as wrapped around the body's
    // waist rather than smeared uniformly over the whole surface.
    float latFade = 1.0 - abs(dir.y);
    float displaced = radiusBase + amp * (0.16 + uDistortion * 0.22) * latFade;
    return length(p) - displaced;
}

vec3 sphereNormal(vec3 p, float react)
{
    vec2 e = vec2(0.0015, 0.0);
    return normalize(vec3(
        sphereDE(p + e.xyy, react) - sphereDE(p - e.xyy, react),
        sphereDE(p + e.yxy, react) - sphereDE(p - e.yxy, react),
        sphereDE(p + e.yyx, react) - sphereDE(p - e.yyx, react)));
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    float orbitAngle = uTime * 0.14 * (0.35 + abs(uRotationSpeed));
    float dist = 4.2 / max(uCameraScale, 0.05);
    vec3 ro = vec3(sin(orbitAngle), 0.3 * sin(uTime * 0.09), cos(orbitAngle)) * dist;
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.7 + right * uv.x + up * uv.y);

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 90; ++i)
    {
        p = ro + rd * t;
        float d = sphereDE(p, react);
        if (d > 0.08)
            glow += 0.0014 / (0.02 + d * d * 3.0);
        float eps = 0.0012 + t * 0.0005;
        if (d < eps)
        {
            hit = true;
            break;
        }
        t += d * 0.85;
        if (t > 16.0)
            break;
    }

    vec3 col = vec3(0.01, 0.012, 0.025);
    if (hit)
    {
        vec3 n = sphereNormal(p, react);
        vec3 lightDir = normalize(vec3(0.5, 0.7, -0.4));
        float diff = max(dot(n, lightDir), 0.0);
        float fresnel = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);

        vec3 dirp = normalize(p + 1.0e-6);
        float azimuth = atan(dirp.x, dirp.z) / 6.28318530718 + 0.5;
        vec3 base = palette(azimuth + dirp.y * 0.3 + uTime * 0.02, uHue);
        if (uUserImageLoaded > 0.5)
        {
            vec4 imgC = sampleUserImage(imageContainUV(vec2(azimuth * 2.0 - 1.0, dirp.y), uUserImageAspect));
            base = mix(base, imgC.rgb, imgC.a * 0.85);
        }

        col = base * (0.3 + 0.8 * diff) * (0.7 + uLevel * react * 1.0);
        col += fresnel * vec3(0.7, 0.8, 1.0) * (0.4 + uOnset * react * uCameraShake * 1.2);
        col += pow(diff, 16.0) * 0.6 * (1.0 + uTreble * react * 0.5);
    }

    col += glow * palette(t * 0.08 + uTime * 0.015, uHue) * (0.4 + react * 0.9);
    col *= 1.0 - smoothstep(9.0, 16.0, t) * 0.85;
    col += uOnset * react * uCameraShake * 0.15 * vec3(0.7, 0.85, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
