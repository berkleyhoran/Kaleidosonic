#pragma once

#include <array>
#include <random>
#include <vector>

// CPU state for the "Pipes" preset -- a homage to the classic Windows
// pipes screensaver. A handful of pipes each grow one grid segment at a
// time, turning at right angles (never a diagonal -- a real elbow joint,
// like the original), staying inside a bounding cube; once a pipe fills
// its joint budget it holds a moment (so the finished shape actually
// reads before vanishing, matching the original's fill-then-clear cycle)
// then clears and restarts from a fresh point with a new color.
//
// Joint positions are uploaded to the GPU as a small texture -- same
// pattern as FractalNavigator's reference-orbit texture -- and raymarched
// as a chain of capsules (see pipes.frag). Unlike the fractal navigators,
// nothing here needs extended precision: the bounding cube keeps every
// coordinate small and bounded forever, so plain float is exact.
class PipeNetwork
{
public:
    static constexpr int numPipes = 5;
    static constexpr int maxJointsPerPipe = 56;
    static constexpr int textureWidth = numPipes * maxJointsPerPipe;

    PipeNetwork();

    // dt: seconds since last call, already clamped by the caller.
    // growthRate: grid steps per second the pipes should advance at,
    // audio-reactive -- see VisualizerRenderer::updateNavigators (Zoom
    // Speed and Camera Shake are repurposed here as "how fast things
    // build" the same way they're repurposed as the IFS presets' zoom
    // rate, since Pipes has no zoom of its own).
    void update(double dt, double growthRate, std::mt19937& rng);

    // Interleaved (x, y, z, radius) float quadruples, textureWidth texels,
    // ready to upload as an RGBA32F texture row. Rebuilt every update()
    // call: the texel right after a pipe's confirmed joints is the
    // currently-growing tip, interpolated toward the next grid point by
    // that pipe's own step progress, so the pipe visibly extrudes
    // smoothly rather than only ever appearing to grow in discrete,
    // instant unit-length pops. Joints beyond the tip are padded by
    // repeating it, so the shader's capsules between padding entries are
    // zero-length (and therefore invisible) rather than needing a
    // separate "how many joints are real" uniform.
    const std::vector<float>& jointData() const noexcept { return jointFloats; }

    // Per-pipe hue offset (0..1), for the shader's palette() call.
    const std::array<float, numPipes>& hues() const noexcept { return pipeHues; }

    // True exactly once after growth actually changed the joint data;
    // clears itself on read.
    bool consumeDirty() noexcept
    {
        const bool d = dirty;
        dirty = false;
        return d;
    }

private:
    struct Pipe
    {
        std::vector<std::array<float, 3>> joints;
        std::array<int, 3> dir { 1, 0, 0 };
        int turnCooldown = 2;
        double stepTimer = 0.0;
        bool holding = false;
        double holdTimer = 0.0;
        float hue = 0.0f;
    };

    void stepPipe(Pipe& pipe, std::mt19937& rng);
    void resetPipe(Pipe& pipe, std::mt19937& rng);
    void rebuildBuffer();

    std::array<Pipe, numPipes> pipes;
    std::array<float, numPipes> pipeHues {};
    std::vector<float> jointFloats;
    bool dirty = false;
};
