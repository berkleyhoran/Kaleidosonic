#include "PipeNetwork.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float bound = 8.5f;   // pipes stay inside [-bound, bound]^3
    constexpr float step = 1.0f;    // grid unit per growth step
    constexpr float radius = 0.16f; // capsule radius
    constexpr double holdSeconds = 1.6; // how long a finished pipe lingers before resetting
}

PipeNetwork::PipeNetwork()
{
    std::mt19937 seedRng { std::random_device {}() };
    for (auto& pipe : pipes)
        resetPipe(pipe, seedRng);
    rebuildBuffer();
}

void PipeNetwork::resetPipe(Pipe& pipe, std::mt19937& rng)
{
    std::uniform_real_distribution<float> posDist(-bound * 0.6f, bound * 0.6f);
    std::uniform_int_distribution<int> axisDist(0, 2);
    std::uniform_int_distribution<int> signDist(0, 1);
    std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);

    pipe.joints.clear();
    pipe.joints.push_back({ posDist(rng), posDist(rng), posDist(rng) });

    pipe.dir = { 0, 0, 0 };
    pipe.dir[(size_t) axisDist(rng)] = signDist(rng) != 0 ? 1 : -1;
    pipe.turnCooldown = 2;
    pipe.holding = false;
    pipe.holdTimer = 0.0;
    pipe.stepTimer = 0.0;
    pipe.hue = hueDist(rng);
}

void PipeNetwork::stepPipe(Pipe& pipe, std::mt19937& rng)
{
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    const auto cur = pipe.joints.back();

    const std::array<float, 3> straightNext = { cur[0] + (float) pipe.dir[0] * step,
                                                 cur[1] + (float) pipe.dir[1] * step,
                                                 cur[2] + (float) pipe.dir[2] * step };
    const bool outOfBounds = std::abs(straightNext[0]) > bound || std::abs(straightNext[1]) > bound
                           || std::abs(straightNext[2]) > bound;

    if (outOfBounds || (pipe.turnCooldown <= 0 && chance(rng) < 0.28))
    {
        // A real elbow: pick one of the two axes perpendicular to the
        // current direction, never the same axis (that would just be
        // "straight" or "reverse", not a turn), preferring options that
        // stay inside the bounding cube.
        const int curAxis = pipe.dir[0] != 0 ? 0 : (pipe.dir[1] != 0 ? 1 : 2);
        const std::array<int, 2> otherAxes { (curAxis + 1) % 3, (curAxis + 2) % 3 };

        std::vector<std::array<int, 3>> candidates;
        for (int axis : otherAxes)
        {
            for (int s : { -1, 1 })
            {
                std::array<int, 3> d { 0, 0, 0 };
                d[(size_t) axis] = s;
                const float next = cur[(size_t) axis] + (float) s * step;
                if (std::abs(next) <= bound)
                    candidates.push_back(d);
            }
        }

        if (! candidates.empty())
        {
            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            pipe.dir = candidates[pick(rng)];
        }
        // else: every perpendicular option would leave the cube (rare,
        // deep in a corner) -- keep the current direction; outOfBounds
        // will keep re-triggering this check every step until a turn
        // does open up, since the cube is large relative to one step.

        pipe.turnCooldown = 2 + (int) (chance(rng) * 3.0);
    }
    else
    {
        --pipe.turnCooldown;
    }

    pipe.joints.push_back({ cur[0] + (float) pipe.dir[0] * step, cur[1] + (float) pipe.dir[1] * step,
                            cur[2] + (float) pipe.dir[2] * step });
}

void PipeNetwork::update(double dt, double growthRate, std::mt19937& rng)
{
    dt = std::clamp(dt, 0.0, 0.1);
    growthRate = std::clamp(growthRate, 0.1, 30.0);

    for (auto& pipe : pipes)
    {
        if (pipe.holding)
        {
            pipe.holdTimer -= dt;
            if (pipe.holdTimer <= 0.0)
                resetPipe(pipe, rng);
            continue;
        }

        pipe.stepTimer += dt * growthRate;
        while (pipe.stepTimer >= 1.0)
        {
            pipe.stepTimer -= 1.0;
            stepPipe(pipe, rng);
            if ((int) pipe.joints.size() >= maxJointsPerPipe)
            {
                pipe.holding = true;
                pipe.holdTimer = holdSeconds;
                break;
            }
        }
    }

    // Rebuilt every frame (not just when a joint actually commits) so the
    // leading segment's tip -- see rebuildBuffer -- visibly extrudes
    // continuously instead of the pipe only ever appearing to grow in
    // discrete, instant unit-length pops.
    rebuildBuffer();
}

void PipeNetwork::rebuildBuffer()
{
    jointFloats.assign((size_t) textureWidth * 4, 0.0f);

    for (int p = 0; p < numPipes; ++p)
    {
        const auto& pipe = pipes[(size_t) p];
        const int confirmed = (int) pipe.joints.size();
        const std::array<float, 3> lastConfirmed =
            pipe.joints.empty() ? std::array<float, 3> { 0.0f, 0.0f, 0.0f } : pipe.joints.back();

        // While actively growing, the segment past the last confirmed
        // joint is drawn part-way to the next one -- a real extruding
        // tip, not a segment that only appears once fully grown.
        std::array<float, 3> tip = lastConfirmed;
        bool hasTip = false;
        if (! pipe.holding && confirmed > 0 && confirmed < maxJointsPerPipe)
        {
            const float frac = (float) std::clamp(pipe.stepTimer, 0.0, 1.0);
            tip = { lastConfirmed[0] + (float) pipe.dir[0] * frac, lastConfirmed[1] + (float) pipe.dir[1] * frac,
                    lastConfirmed[2] + (float) pipe.dir[2] * frac };
            hasTip = true;
        }
        const std::array<float, 3>& pad = hasTip ? tip : lastConfirmed;

        // Real joints (including the growing tip, excluding padding) --
        // the shader stops scanning here instead of always walking to
        // maxJointsPerPipe. At least 2 so there's always at least one
        // (possibly zero-length, i.e. a single ball) capsule to draw --
        // a fresh pipe's single start point shouldn't just vanish.
        const int activeCount = std::clamp(confirmed + (hasTip ? 1 : 0), 2, maxJointsPerPipe);

        float minX = 1.0e9f, minY = 1.0e9f, minZ = 1.0e9f;
        float maxX = -1.0e9f, maxY = -1.0e9f, maxZ = -1.0e9f;

        for (int j = 0; j < maxJointsPerPipe; ++j)
        {
            std::array<float, 3> pos;
            if (j < confirmed)
                pos = pipe.joints[(size_t) j];
            else if (j == confirmed && hasTip)
                pos = tip;
            else
                pos = pad;

            if (j < activeCount)
            {
                minX = std::min(minX, pos[0]); maxX = std::max(maxX, pos[0]);
                minY = std::min(minY, pos[1]); maxY = std::max(maxY, pos[1]);
                minZ = std::min(minZ, pos[2]); maxZ = std::max(maxZ, pos[2]);
            }

            const size_t idx = (size_t) (p * maxJointsPerPipe + j) * 4;
            jointFloats[idx + 0] = pos[0];
            jointFloats[idx + 1] = pos[1];
            jointFloats[idx + 2] = pos[2];
            jointFloats[idx + 3] = radius;
        }
        pipeHues[(size_t) p] = pipe.hue;
        pipeJointCounts[(size_t) p] = (float) activeCount;

        const float cx = (minX + maxX) * 0.5f, cy = (minY + maxY) * 0.5f, cz = (minZ + maxZ) * 0.5f;
        const float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
        // Half-diagonal of the active joints' bounding box, plus the
        // capsule radius and a small margin -- a sphere that's guaranteed
        // to fully contain every real segment, so distance-to-this-sphere
        // is always a safe (never too large) lower bound on distance to
        // the pipe's actual geometry.
        const float boundRadius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz) + radius + 0.05f;
        pipeBounds[(size_t) p] = { cx, cy, cz, boundRadius };
    }

    dirty = true;
}
