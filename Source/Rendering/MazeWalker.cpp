#include "MazeWalker.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
    // 32-bit integer hash -- deliberately NOT a float/sin-based hash like
    // the palette noise elsewhere in this codebase, because this one has
    // to agree bit-for-bit with infinite_maze.frag's copy: if the CPU's
    // navigation decision about whether an edge is open ever disagreed
    // with what the GPU actually rendered there, the camera would clip
    // through a wall it just decided was open. Integer ops (multiply,
    // xor, shift) are exact and portable between C++'s uint32_t and
    // GLSL's uint the way floating-point transcendentals are not.
    // MUST be kept identical to mazeHashU in infinite_maze.frag.
    uint32_t mazeHashU(int x, int z, int dir)
    {
        uint32_t h = (uint32_t) x * 374761393u + (uint32_t) z * 668265263u + (uint32_t) dir * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h = h ^ (h >> 16);
        return h;
    }

    // dir: 0 = the edge from (nodeX, nodeZ) toward +x; 1 = toward +z.
    // Every edge has exactly one canonical owner (the node on its -x/-z
    // side), so this is the single source of truth both languages query.
    // ~42% of edges are walls; pillars (handled separately, GPU-side
    // only -- see infinite_maze.frag) are always solid.
    bool mazeIsEdgeWall(int nodeX, int nodeZ, int dir)
    {
        return (mazeHashU(nodeX, nodeZ, dir) % 1000u) < 420u;
    }

    bool neighborOpen(int nodeX, int nodeZ, int dx, int dz)
    {
        if (dx == 1)
            return ! mazeIsEdgeWall(nodeX, nodeZ, 0);
        if (dx == -1)
            return ! mazeIsEdgeWall(nodeX - 1, nodeZ, 0);
        if (dz == 1)
            return ! mazeIsEdgeWall(nodeX, nodeZ, 1);
        return ! mazeIsEdgeWall(nodeX, nodeZ - 1, 1); // dz == -1
    }

    struct Dir { int dx, dz; };
}

MazeWalker::MazeWalker()
{
    std::mt19937 seedRng { std::random_device {}() };
    chooseNextDirection(seedRng, 0.0);
}

namespace
{
    // Evaluates the open neighbors of (nodeX, nodeZ) and returns whichever
    // direction chooseNextDirection should commit to -- factored out so
    // the isolated-pocket escape below can reuse it at the fresh landing
    // spot without duplicating the selection logic.
    Dir pickDirection(int nodeX, int nodeZ, int prevDirX, int prevDirZ, double turnBias, std::mt19937& rng,
                      bool& wasForcedReversal)
    {
        static const std::array<Dir, 4> allDirs { { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } };

        std::vector<Dir> open;
        for (const auto& d : allDirs)
            if (neighborOpen(nodeX, nodeZ, d.dx, d.dz))
                open.push_back(d);

        const Dir reverse { -prevDirX, -prevDirZ };
        std::vector<Dir> nonReverse;
        for (const auto& d : open)
            if (d.dx != reverse.dx || d.dz != reverse.dz)
                nonReverse.push_back(d);
        const auto& candidates = nonReverse.empty() ? open : nonReverse;

        if (candidates.empty())
        {
            // Fully enclosed node (rare -- roughly 2.5% of nodes with all
            // 4 edges walls). Nowhere legal to go, so step back the way
            // we came rather than getting stuck; the caller tracks
            // whether this keeps happening (see forcedReversalStreak).
            wasForcedReversal = true;
            return reverse;
        }

        // A single remaining candidate that IS the reverse direction
        // means every other neighbor was a wall -- a forced reversal,
        // same as the empty case above, just with the reverse edge
        // itself open. Two adjacent nodes that are both this constrained
        // form an isolated pocket the walker can only ever bounce
        // between (see forcedReversalStreak).
        wasForcedReversal = candidates.size() == 1 && candidates[0].dx == reverse.dx && candidates[0].dz == reverse.dz;

        const Dir straight { prevDirX, prevDirZ };
        bool straightOpen = false;
        for (const auto& d : candidates)
            if (d.dx == straight.dx && d.dz == straight.dz)
                straightOpen = true;

        std::uniform_real_distribution<double> chance(0.0, 1.0);
        if (straightOpen && chance(rng) > turnBias)
            return straight;

        std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
        return candidates[pick(rng)];
    }
}

void MazeWalker::chooseNextDirection(std::mt19937& rng, double turnBias)
{
    bool forced = false;
    Dir chosen = pickDirection(nodeX, nodeZ, dirX, dirZ, turnBias, rng, forced);

    forcedReversalStreak = forced ? forcedReversalStreak + 1 : 0;

    if (forcedReversalStreak >= 3)
    {
        // Confirmed isolated pocket (three forced reversals in a row --
        // a one-off dead-end spur only ever produces one). There's no
        // local move that escapes it, so jump to a fresh, distant node
        // instead of continuing to bounce -- reads as the walk finding a
        // new part of the maze, not as it getting stuck.
        std::uniform_int_distribution<int> jumpDist(18, 40);
        std::uniform_int_distribution<int> signDist(0, 1);
        nodeX += jumpDist(rng) * (signDist(rng) != 0 ? 1 : -1);
        nodeZ += jumpDist(rng) * (signDist(rng) != 0 ? 1 : -1);
        forcedReversalStreak = 0;

        bool ignored = false;
        chosen = pickDirection(nodeX, nodeZ, dirX, dirZ, turnBias, rng, ignored);
    }

    targetNodeX = nodeX + chosen.dx;
    targetNodeZ = nodeZ + chosen.dz;
    dirX = chosen.dx;
    dirZ = chosen.dz;
}

void MazeWalker::update(double dt, double speed, double turnBias, std::mt19937& rng)
{
    dt = std::clamp(dt, 0.0, 0.1);
    speed = std::clamp(speed, 0.05, 12.0);
    turnBias = std::clamp(turnBias, 0.0, 1.0);

    progress += dt * speed; // 1 logical unit per node-to-node hop; the
                             // shader renders logical units at a wider
                             // pitch (see kPitch in infinite_maze.frag) --
                             // this class only ever deals in logical space.
    while (progress >= 1.0)
    {
        progress -= 1.0;
        nodeX = targetNodeX;
        nodeZ = targetNodeZ;
        chooseNextDirection(rng, turnBias);
    }

    currentX = (double) nodeX + (double) dirX * progress;
    currentZ = (double) nodeZ + (double) dirZ * progress;

    // Ease the camera-facing direction toward the (possibly just-changed)
    // discrete travel direction; ~0.35s time constant reads as a smooth
    // corner turn rather than an instant snap.
    const double ease = 1.0 - std::exp(-dt / 0.35);
    smoothDirX += ((double) dirX - smoothDirX) * ease;
    smoothDirZ += ((double) dirZ - smoothDirZ) * ease;
}
