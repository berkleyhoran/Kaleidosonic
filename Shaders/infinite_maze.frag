// Infinite Maze — a Wolfenstein-style autonomous walk through a
// procedurally-hashed pillar maze that never repeats and is never stored:
// every cell's wall/open state is a pure function of its integer grid
// coordinate (mazeIsEdgeWall below), evaluated completely independently
// here and by the CPU navigation logic in Source/Rendering/MazeWalker.cpp
// -- there is nothing to generate ahead of time or upload, so the "maze"
// is exactly as infinite as the raymarch is willing to look, in every
// direction, for free. The camera is the walker: MazeWalker picks a path
// through the open corridors on its own, turning at junctions (audio
// biases how eager it is to turn vs. go straight), and this shader just
// renders whatever's around wherever it currently is.
//
// mazeHashU/mazeIsEdgeWall MUST stay identical to MazeWalker.cpp's copy --
// see that file's comment for why an integer hash (not a float one) is
// what keeps the CPU's navigation decisions and the GPU's rendered walls
// from ever disagreeing about where a given wall actually is.

uint mazeHashU(int x, int z, int dir)
{
    uint h = uint(x) * 374761393u + uint(z) * 668265263u + uint(dir) * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h = h ^ (h >> 16u);
    return h;
}

bool mazeIsEdgeWall(int nodeX, int nodeZ, int dir)
{
    return (mazeHashU(nodeX, nodeZ, dir) % 1000u) < 420u;
}

int mazeFloorDiv(int a, int b)
{
    int q = a / b;
    int r = a - q * b;
    if (r != 0 && ((r < 0) != (b < 0)))
        q -= 1;
    return q;
}

int mazeFloorMod(int a, int b)
{
    return a - mazeFloorDiv(a, b) * b;
}

// World-cell (integer) wall test: pillars at (odd, odd) are always solid;
// nodes at (even, even) are always open; everything else is an edge
// between two nodes, owned canonically by the node on its -x/-z side.
bool mazeIsWallCell(int cx, int cz)
{
    int px = mazeFloorMod(cx, 2);
    int pz = mazeFloorMod(cz, 2);
    if (px == 1 && pz == 1)
        return true;
    if (px == 0 && pz == 0)
        return false;
    if (px == 1)
        return mazeIsEdgeWall(mazeFloorDiv(cx - 1, 2), mazeFloorDiv(cz, 2), 0);
    return mazeIsEdgeWall(mazeFloorDiv(cx, 2), mazeFloorDiv(cz - 1, 2), 1);
}

float sdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

const float kWallHeight = 2.6;

// Logical grid cells are 1 unit apart (see mazeIsWallCell/MazeWalker),
// but rendered at a much wider physical spacing, with walls/pillars kept
// thin rather than filling their whole cell -- the earlier version made
// each cell exactly one uniform 1x1 unit, wall boxes filling it
// completely, so a corridor was exactly as wide as the walls bounding it
// were thick, reading as cramped ("running into the walls" is really
// "the walls are as wide as the path"). Decoupling render pitch from
// logical grid size, and shrinking the wall geometry within it, is what
// actually fixes that -- widening only the corridors, not just scaling
// the whole scene uniformly (which would change nothing relative).
const float kPitch = 2.6;
const float kWallHalfThickness = 0.32;

// Distance to the nearest wall/floor/ceiling surface. Checks the 3x3
// neighborhood of logical grid cells around p (converted from render
// space via kPitch), not just the cell p is in -- a wall in an adjacent
// cell can still be the closest surface, especially right at a corner.
float mazeDE(vec3 p)
{
    vec2 logical = p.xz / kPitch;
    int cx0 = int(floor(logical.x));
    int cz0 = int(floor(logical.y));
    // An edge-wall's whole job is blocking a corridor -- unlike a pillar
    // (just a corner post), it has to span nearly the full corridor
    // width or the camera could simply walk around it, which would make
    // the maze meaningless. Only pillars stay a small post; edges get a
    // thin-in-one-axis, corridor-width-in-the-other slab.
    float halfSpan = kPitch * 0.5 - 0.03;
    float d = 1.0e5;
    for (int dz = -1; dz <= 1; ++dz)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            int cx = cx0 + dx;
            int cz = cz0 + dz;
            if (! mazeIsWallCell(cx, cz))
                continue;

            int px = mazeFloorMod(cx, 2);
            int pz = mazeFloorMod(cz, 2);
            vec3 wallHalf; // not `half` -- reserved GLSL word, see common.glsl's imageContainUV comment
            if (px == 1 && pz == 1)
                wallHalf = vec3(kWallHalfThickness, kWallHeight * 0.5, kWallHalfThickness); // corner pillar
            else if (px == 1)
                wallHalf = vec3(kWallHalfThickness, kWallHeight * 0.5, halfSpan); // blocks +/-x travel
            else
                wallHalf = vec3(halfSpan, kWallHeight * 0.5, kWallHalfThickness); // blocks +/-z travel

            vec3 center = vec3((float(cx) + 0.5) * kPitch, kWallHeight * 0.5, (float(cz) + 0.5) * kPitch);
            d = min(d, sdBox(p - center, wallHalf));
        }
    }
    d = min(d, p.y);               // floor plane, y = 0
    d = min(d, kWallHeight - p.y); // ceiling plane
    return d;
}

void main()
{
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float react = uReactivity;

    // uMazeHeading is an eased blend between the walker's previous and
    // current travel direction (see MazeWalker::update); on a reversal
    // that ease necessarily passes through (0,0) for an instant, and
    // normalize() of a zero vector is undefined (NaN on some drivers,
    // which would otherwise propagate through the whole ray and render
    // solid black). Fall back to a fixed direction on that one frame.
    vec3 headingRaw = vec3(uMazeHeading.x, 0.0, uMazeHeading.y);
    vec3 heading = length(headingRaw) > 0.001 ? normalize(headingRaw) : vec3(1.0, 0.0, 0.0);
    vec3 upDir = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(upDir, heading));
    // A little handheld sway (audio-driven) and gentle continuous bob,
    // so the walk reads as alive rather than a rigid rail camera.
    float bob = sin(uTime * 3.2) * 0.03 + uLevel * react * 0.02;
    float sway = sin(uTime * 0.7) * 0.05 * uDistortion;
    vec3 ro = vec3(uMazePos.x * kPitch, 1.3 + bob, uMazePos.y * kPitch);
    vec3 forward = normalize(heading + right * sway);
    vec3 up2 = cross(right, forward);
    vec3 rd = normalize(forward * 1.4 + right * uv.x + up2 * uv.y);

    // A long straight run of open corridor is a real, if infrequent,
    // possibility (nothing caps how many consecutive edges can happen to
    // hash open) -- generous enough range/iterations that even those
    // still resolve a real wall instead of exhausting the march.
    float t = 0.0;
    float glow = 0.0;
    bool hit = false;
    vec3 p = ro;
    for (int i = 0; i < 160; ++i)
    {
        p = ro + rd * t;
        float d = mazeDE(p);
        glow += 0.0009 / (0.015 + d * d * 5.0);
        if (d < 0.008)
        {
            hit = true;
            break;
        }
        t += d * 0.75;
        if (t > 90.0)
            break;
    }

    // Backdrop for the (now rare, but still possible) total miss: a
    // gentle vertical gradient rather than relying on the glow term
    // alone, which the distance fog below would otherwise crush to
    // imperceptible black -- the corridor should read as receding into a
    // dim, moody distance, not vanish.
    vec3 col = mix(vec3(0.015, 0.01, 0.03), vec3(0.05, 0.03, 0.07), clamp(rd.y * 0.5 + 0.5, 0.0, 1.0));
    if (hit)
    {
        vec2 e = vec2(0.01, 0.0);
        vec3 n = normalize(vec3(
            mazeDE(p + e.xyy) - mazeDE(p - e.xyy),
            mazeDE(p + e.yxy) - mazeDE(p - e.yxy),
            mazeDE(p + e.yyx) - mazeDE(p - e.yyx)));

        // Neon strip lighting driven by the cell the surface belongs to,
        // pulsing with bass -- a nod to the original's flat-shaded but
        // colorful chrome, without needing real light sources.
        vec2 cellId = floor(p.xz / kPitch);
        float cellHue = fract(sin(dot(cellId, vec2(41.3, 289.1))) * 43758.5453);
        float paletteT = cellHue + uTreble * react * 0.15 + uTime * 0.01;
        vec3 base = palette(paletteT, uHue);

        float facing = max(dot(n, -rd), 0.0);
        float pulse = 0.6 + 0.4 * sin(uTime * 2.0 + cellHue * 20.0) * (0.4 + uBass * react * 0.6);
        col = base * (0.35 + 0.65 * facing) * pulse;
        col *= 0.8 + uLevel * react * 0.6;
    }

    col += glow * palette(t * 0.05 + uTime * 0.015, uHue) * (0.5 + react * 0.8);

    // Distance fog so corridors recede into darkness instead of the
    // raymarch cutoff reading as a hard, flat wall in the distance --
    // tuned to only fully darken well past the new, longer march range,
    // so a genuinely long sightline still reads as a corridor fading
    // into the distance rather than a black wall.
    col *= 1.0 - smoothstep(30.0, 90.0, t) * 0.85;
    col += uOnset * react * uCameraShake * 0.18 * vec3(1.0, 0.5, 0.7);

    fragColor = vec4(grade(col), 1.0);
}
