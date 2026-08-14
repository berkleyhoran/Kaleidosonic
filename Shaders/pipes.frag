// Pipes — a homage to the classic Windows pipes screensaver. Three
// CPU-grown pipes (Source/Rendering/PipeNetwork.h) each build one grid
// segment at a time, turning at right-angle elbows, until they fill their
// bounding cube and hold a moment before clearing and starting over with a
// fresh color -- the same fill-then-reset rhythm as the original. The
// joint chain is raymarched here as a run of capsules; a capsule's own
// rounded caps are what give the elbows their joint-ball look, with no
// separate sphere primitive needed.
//
// kNumPipes/kMaxJointsPerPipe must match PipeNetwork::numPipes/
// maxJointsPerPipe -- there's no way to share the constant between C++
// and GLSL here, so if one changes, so must the other.
const int kNumPipes = 5;
const int kMaxJointsPerPipe = 56;

vec4 pipeJointAt(int idx)
{
    return texelFetch(uPipeJoints, ivec2(idx, 0), 0);
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r)
{
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1.0e-9), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

vec4 pipeBoundsAt(int pi)
{
    return pi == 0 ? uPipeBounds0
         : pi == 1 ? uPipeBounds1
         : pi == 2 ? uPipeBounds2
         : pi == 3 ? uPipeBounds3
                    : uPipeBounds4;
}

int pipeJointCountAt(int pi)
{
    return int(pi == 0 ? uPipeJointCountA.x
             : pi == 1 ? uPipeJointCountA.y
             : pi == 2 ? uPipeJointCountA.z
             : pi == 3 ? uPipeJointCountA.w
                        : uPipeJointCountE);
}

// Nearest distance to any pipe segment, plus which pipe it belongs to
// (for coloring) via pipeIndex (out). Two real optimizations over "check
// every joint of every pipe every step" (which is what made this preset
// laggy -- up to 27,500 capsule evaluations per pixel per frame):
//   1. A pipe's whole segment chain is skipped with a single
//      distance-to-bounding-sphere check whenever that sphere is already
//      farther away than the closest surface found so far -- safe
//      because the sphere is built to fully contain every real segment,
//      so distance-to-sphere is always <= the true distance to the pipe.
//   2. The inner loop stops at that pipe's real joint count instead of
//      always walking to kMaxJointsPerPipe, so a pipe early in its growth
//      (a handful of real joints) doesn't pay for dozens of zero-length
//      "padding" capsules every step.
float pipesDE(vec3 p, out int pipeIndex)
{
    float best = 1.0e5;
    pipeIndex = 0;
    for (int pi = 0; pi < kNumPipes; ++pi)
    {
        vec4 b = pipeBoundsAt(pi);
        float boundDist = length(p - b.xyz) - b.w;
        if (boundDist >= best)
            continue;

        int count = pipeJointCountAt(pi);
        vec4 prevJoint = pipeJointAt(pi * kMaxJointsPerPipe);
        for (int j = 1; j < count; ++j)
        {
            vec4 joint = pipeJointAt(pi * kMaxJointsPerPipe + j);
            float d = sdCapsule(p, prevJoint.xyz, joint.xyz, joint.w);
            if (d < best)
            {
                best = d;
                pipeIndex = pi;
            }
            prevJoint = joint;
        }
    }
    return best;
}

// exploreFractal-style plain wrapper for the normal/glow passes, which
// don't care which pipe they hit.
float pipesDEPlain(vec3 p)
{
    int dummy;
    return pipesDE(p, dummy);
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // Camera orbits the bounding cube (PipeNetwork keeps every joint
    // inside [-8.5,8.5]^3) from comfortably outside it -- see Mandelbox's
    // comment on why the camera must clear the structure's true bounding
    // radius, not just its "typical" size. A noticeably livelier orbit
    // than a typical dive-style preset's default -- Pipes has no zoom of
    // its own, so the camera actively circling is what keeps the view
    // from reading as static between growth events.
    float orbitAngle = uTime * 0.22 * (0.4 + abs(uRotationSpeed));
    float dist = (28.0 + 5.0 * sin(uTime * 0.1)) / max(uCameraScale, 0.05);
    dist = max(dist, 18.0);
    vec3 ro = vec3(sin(orbitAngle), 0.4 * sin(uTime * 0.05), cos(orbitAngle)) * dist;
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
    vec3 up = cross(forward, right);
    vec3 rd = normalize(forward * 1.7 + right * uv.x + up * uv.y);

    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    int hitPipe = 0;
    vec3 p = ro;
    for (int i = 0; i < 100; ++i)
    {
        p = ro + rd * t;
        int pipeIndex;
        float d = pipesDE(p, pipeIndex);
        glow += 0.0012 / (0.02 + d * d * 4.0);
        if (d < 0.0015)
        {
            hit = true;
            hitPipe = pipeIndex;
            break;
        }
        t += d * 0.6;
        if (t > 60.0)
            break;
    }

    vec3 col = vec3(0.01, 0.012, 0.02);
    if (hit)
    {
        vec2 e = vec2(0.002, 0.0);
        vec3 n = normalize(vec3(
            pipesDEPlain(p + e.xyy) - pipesDEPlain(p - e.xyy),
            pipesDEPlain(p + e.yxy) - pipesDEPlain(p - e.yxy),
            pipesDEPlain(p + e.yyx) - pipesDEPlain(p - e.yyx)));

        vec3 lightDir = normalize(vec3(0.4, 0.75, -0.4));
        float diff = max(dot(n, lightDir), 0.0);
        float pipeHue = hitPipe == 0 ? uPipeHuesA.x
                       : hitPipe == 1 ? uPipeHuesA.y
                       : hitPipe == 2 ? uPipeHuesA.z
                       : hitPipe == 3 ? uPipeHuesA.w
                                      : uPipeHueE;
        float paletteT = pipeHue + uMid * react * 0.2;
        vec3 base = palette(paletteT, uHue) * (0.45 + 0.65 * diff);
        col = base * (0.75 + uLevel * react * 1.0);
        col += pow(diff, 12.0) * 0.6; // metallic highlight, like the original's chrome look

        // A glowing pulse of energy flowing down each pipe from its start
        // joint -- straight-line distance rather than true along-the-pipe
        // arc length (which would need walking the joint chain again),
        // but at the speeds/frequencies here it reads as flow, not a cheat.
        // Onsets kick a fresh pulse loose; mid drives its speed.
        vec3 pipeStart = pipeJointAt(hitPipe * kMaxJointsPerPipe).xyz;
        float travelDist = length(p - pipeStart);
        float pulsePhase = travelDist * 1.4 - uTime * (2.2 + uMid * react * 3.0);
        float pulse = pow(max(sin(pulsePhase), 0.0), 4.0);
        col += pulse * palette(paletteT + 0.2, uHue) * (0.5 + uOnset * react * uCameraShake * 1.2);
    }

    col += glow * palette(t * 0.02 + uTime * 0.01, uHue) * (0.5 + react * 0.9);
    col *= 1.0 - smoothstep(30.0, 60.0, t) * 0.8;
    col += uOnset * react * uCameraShake * 0.2 * vec3(0.5, 0.7, 1.0);

    fragColor = vec4(grade(col), 1.0);
}
