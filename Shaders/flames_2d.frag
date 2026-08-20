// 2D Flames — a classic upward-scrolling domain-warped-noise fire, shaped
// by a vertical/horizontal falloff so it reads as flame silhouettes rising
// off the bottom of the screen rather than a generic noise field. Bass
// makes the flames roar taller, treble adds fine turbulent flicker, and
// onsets launch a shower of rising embers off the flame body. Reuses the
// same domain-warped FBM technique as Audio Nebula (this codebase's
// established "iq warping" approach) with a dedicated warm fire color
// ramp instead of the general cosine palette, since a flame's color
// progression (dark ember -> orange -> yellow -> white-hot) is specific
// enough that the curated palettes don't read as fire.

float nHashF(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }
float hash1F(float n) { return fract(sin(n) * 43758.5453123); }

float vnoiseF(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = nHashF(i);
    float b = nHashF(i + vec2(1.0, 0.0));
    float c = nHashF(i + vec2(0.0, 1.0));
    float d = nHashF(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbmF(vec2 p)
{
    float amp = 0.5;
    float sum = 0.0;
    for (int i = 0; i < 5; ++i)
    {
        sum += amp * vnoiseF(p);
        p = p * 2.05 + vec2(11.3, 7.1);
        amp *= 0.5;
    }
    return sum;
}

// Fire color ramp built directly in HSV (no rgb2hsv available to presets,
// only hsv2rgb -- see common.glsl): hue sweeps red toward orange/yellow as
// density rises, then desaturates toward white at the hottest core. The
// Hue knob offsets the whole sweep, so it still does something meaningful
// here instead of being ignored.
vec3 fireRamp(float t, float hue)
{
    t = clamp(t, 0.0, 1.0);
    float h = fract(hue + mix(0.0, 0.14, t));
    float s = mix(1.0, 0.15, smoothstep(0.75, 1.0, t));
    float v = smoothstep(0.0, 0.15, t) * mix(0.3, 1.0, t);
    return hsv2rgb(vec3(h, s, v));
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Bass makes the flame body roar taller; Zoom Speed repurposed as the
    // flame's own upward rise/flicker pace -- same "generic knob -> this
    // preset's own concept" convention other presets already use.
    float flameHeight = 0.85 + uBass * react * 0.6;
    float scrollSpeed = 0.35 + 0.5 * abs(uZoomSpeed);
    vec2 p = vec2(uv.x * 2.2, uv.y * 1.6 - uTime * scrollSpeed);

    float warpAmt = 0.9 + uDistortion * 1.2 + uTreble * react * 0.8;
    vec2 q = vec2(fbmF(p), fbmF(p + vec2(5.1, 1.7)));
    vec2 r = vec2(fbmF(p + warpAmt * q + vec2(1.3, 4.2)), fbmF(p + warpAmt * q + vec2(6.8, 2.1)));
    float density = fbmF(p + 2.0 * r);

    // Vertical falloff: dense/bright at the bottom, tapering to nothing
    // near flameHeight -- what shapes this into flame silhouettes rather
    // than a flat noise field. Also narrows horizontally with height,
    // like a real flame licking to a point. Ground reference is -0.5, the
    // *actual* bottom of the aspect-corrected uv space (only uv.x scales
    // by aspect ratio -- uv.y always spans exactly -0.5..0.5, never -0.9)
    // -- an earlier version measured from -0.9, which put the entire
    // visible frame already most of the way up the taper curve, so the
    // flame barely showed at all regardless of any other setting.
    float heightUv = (uv.y + 0.5) / max(flameHeight, 0.05);
    float taper = 1.0 - smoothstep(0.0, 1.0, heightUv);
    float widthNarrow = mix(1.0, 2.6, clamp(heightUv, 0.0, 1.0));
    float horizFalloff = smoothstep(0.9, 0.1, abs(uv.x) * widthNarrow);

    float body = density * taper * horizFalloff;
    body = smoothstep(0.08, 0.6, body);

    vec3 col = fireRamp(body * (0.75 + uLevel * react * 0.5), uHue);
    col *= body * 1.3;

    // Embers: a handful of small bright points launching off the flame
    // body and rising/drifting, each on its own independent cycle -- a
    // deterministic function of uTime + a per-index seed, no simulation
    // state needed (same approach Metaballs' ballCenter() established).
    for (int i = 0; i < 8; ++i)
    {
        float seed = float(i) * 7.9 + 3.0;
        float period = mix(1.1, 2.4, hash1F(seed));
        float cycle = fract(uTime / period + hash1F(seed + 1.7));
        float driftX = (hash1F(seed + 3.1) - 0.5) * 0.9;
        float emberX = driftX * cycle + sin(uTime * 2.0 + seed) * 0.02;
        float emberY = -0.5 + cycle * (flameHeight + 0.35);
        float d = length(uv - vec2(emberX, emberY));
        float glow = smoothstep(0.018, 0.0, d) * (1.0 - cycle) * (0.5 + uOnset * react * 1.2);
        col += fireRamp(0.95, uHue) * glow;
    }

    // Heat-shimmer glow around the body, and a warm floor glow at the
    // very base so the flame doesn't look like it's floating.
    col += fireRamp(0.7, uHue) * smoothstep(0.5, 0.0, length(uv - vec2(0.0, -0.5))) * 0.2;

    fragColor = vec4(grade(col), 1.0);
}
