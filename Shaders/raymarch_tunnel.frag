// Raymarch Tunnel 3D — a genuinely 3D sphere-traced scene: camera flies
// forward through an infinitely repeated, twisting boxed tunnel with simple
// normal-based lighting. Bass drives forward speed hard, treble drives
// twist, onset gives the camera a kick and a light burst.

mat2 rot2(float a)
{
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

float sdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float mapScene(vec3 p, float react)
{
    float twist = (0.5 + uDistortion * 2.5 + uTreble * react * 3.0) * p.z * 0.15;
    p.xy = rot2(twist) * p.xy;

    float period = 2.6;
    vec3 q = p;
    q.z = mod(p.z, period) - period * 0.5;

    float tunnel = -sdBox(q, vec3(1.0, 1.0, period * 0.5));

    vec3 pillarPos = q;
    pillarPos.xy = rot2(uTime * 0.4 * uRotationSpeed) * pillarPos.xy;
    float pillars = sdBox(pillarPos, vec3(0.12, 0.12, period * 0.5)) - 0.02;

    return min(tunnel, pillars);
}

vec3 sceneNormal(vec3 p, float react)
{
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        mapScene(p + e.xyy, react) - mapScene(p - e.xyy, react),
        mapScene(p + e.yxy, react) - mapScene(p - e.yxy, react),
        mapScene(p + e.yyx, react) - mapScene(p - e.yyx, react)));
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Camera speed/FOV kick is driven by Camera Shake, not Reactivity --
    // coupling actual camera motion to the same knob as color/brightness
    // reactivity made high-Reactivity settings feel like motion sickness.
    float camZ = uTime * (1.2 + 4.0 * max(uZoomSpeed, 0.0)) + uBass * uCameraShake * 10.0 + uOnset * uCameraShake * 4.0;
    vec3 ro = vec3(0.0, 0.0, camZ);
    vec3 rd = normalize(vec3(uv * (1.0 + uOnset * uCameraShake * 0.2), 1.4 / uCameraScale));
    rd.xy = rot2(sin(uTime * 0.1) * 0.15 * uRotationSpeed) * rd.xy;

    float t = 0.0;
    float glow = 0.0;
    vec3 col = vec3(0.03, 0.02, 0.06);
    bool hit = false;

    for (int n = 0; n < 80; ++n)
    {
        vec3 p = ro + rd * t;
        float d = mapScene(p, react);
        glow += 0.004 / (0.02 + d * d * 4.0);
        if (d < 0.002)
        {
            hit = true;
            break;
        }
        t += d * 0.7;
        if (t > 40.0)
            break;
    }

    if (hit)
    {
        vec3 p = ro + rd * t;
        vec3 n = sceneNormal(p, react);
        vec3 lightDir = normalize(vec3(0.4, 0.6, -0.6));
        float diff = max(dot(n, lightDir), 0.0);
        float hue = fract(uHue + p.z * 0.02 + uMid * react * 0.7 + uTime * 0.02);
        vec3 base = hsv2rgb(vec3(hue, uSaturation, 0.6 + 0.4 * diff));
        col = base * (0.5 + diff * 1.2) * (0.8 + uLevel * react * 1.6);
        col += pow(diff, 6.0) * 0.8;
    }

    col += glow * hsv2rgb(vec3(fract(uHue + t * 0.01 + uTime * 0.02), uSaturation, 1.0)) * (0.8 + react * 1.5);
    col *= 1.0 - smoothstep(15.0, 40.0, t) * 0.8;
    col += uOnset * react * 0.4 * vec3(0.6, 0.4, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
