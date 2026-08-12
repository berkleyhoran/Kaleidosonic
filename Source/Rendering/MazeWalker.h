#pragma once

#include <random>

// CPU state for the "Infinite Maze" preset — a Wolfenstein-style
// autonomous walk through a procedurally-hashed pillar maze that never
// repeats and never needs to store anything: every cell's wall/open state
// is a pure function of its integer coordinate (see mazeIsEdgeWall
// below), so there is nothing to upload to the GPU at all -- the shader
// (infinite_maze.frag) evaluates the exact same function independently to
// raymarch the walls, and this class only has to track where the camera
// currently is and which way it's facing.
//
// The maze lives on a lattice of "nodes" one logical unit apart in this
// class's own coordinate space (the shader renders logical units at a
// wider physical pitch -- see kPitch in infinite_maze.frag -- so
// corridors read as comfortably wide despite the maze itself being a
// tight, one-cell-per-step grid); the walker always sits at a node or is
// smoothly interpolating to a neighboring one, turning (never diagonally)
// only at nodes. Because both sides only
// need to agree on which edges are open, not on anything numeric, the
// hash only has to be an ordinary 32-bit integer hash -- see the note on
// mazeIsEdgeWall for why that (not a float one) is what keeps the CPU's
// navigation decisions and the GPU's rendered walls from ever disagreeing
// about where a given wall actually is.
class MazeWalker
{
public:
    MazeWalker();

    // dt: seconds since last call, already clamped by the caller.
    // speed: world units per second the walker should move at,
    // audio-reactive the same way FractalNavigator's zoom rate and
    // PipeNetwork's growth rate are.
    // turnBias: 0..1, how likely a junction is to turn rather than
    // continue straight when both are open -- driven up on strong beats
    // so the walk visibly reacts instead of wandering at a flat rate.
    void update(double dt, double speed, double turnBias, std::mt19937& rng);

    // World-space position (x, z), for the shader's camera.
    float posX() const noexcept { return (float) currentX; }
    float posZ() const noexcept { return (float) currentZ; }

    // Facing direction (x, z; normalized) -- an eased version of the
    // discrete travel direction, not the raw axis-aligned one, so the
    // camera visibly turns over a fraction of a second at each corner
    // instead of snapping 90 degrees instantly. A passive viewer isn't
    // the one steering, so a hard snap every corner would read as a
    // glitch rather than a turn.
    float headingX() const noexcept { return (float) smoothDirX; }
    float headingZ() const noexcept { return (float) smoothDirZ; }

private:
    void chooseNextDirection(std::mt19937& rng, double turnBias);

    // Current node (the walker's last stable position) and the node it's
    // travelling toward, in node-index space (world position = node * 2).
    int nodeX = 0, nodeZ = 0;
    int targetNodeX = 1, targetNodeZ = 0;
    int dirX = 1, dirZ = 0; // current travel direction (drives navigation, not the camera directly)
    double progress = 0.0;  // 0..1 across the current hop
    double smoothDirX = 1.0, smoothDirZ = 0.0; // eased toward (dirX, dirZ) -- see headingX/headingZ

    // Two adjacent nodes whose only other edges all happen to hash as
    // walls form an isolated pocket -- rare per node (~0.5%) but likely
    // to turn up eventually over a long walk through hundreds of nodes.
    // Without this, the walker would ping-pong between them forever
    // (every hop a forced reversal, since reversing is the only legal
    // move). Counts consecutive forced reversals; chooseNextDirection
    // teleports to a fresh, almost certainly-open area once it's clear
    // that's what's happening, rather than actually solving the trap
    // (there's no local move that does).
    int forcedReversalStreak = 0;

    // Interpolated world position, recomputed each update() from
    // progress -- kept as members purely so posX()/posZ() can be cheap
    // accessors without needing update()'s locals.
    double currentX = 0.0, currentZ = 0.0;
};
