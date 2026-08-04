#pragma once

#include <random>

// Distance-estimator-guided autopilot for a real-time deep zoom into the
// Mandelbrot (or Burning Ship) set. Runs entirely in genuine C++ double
// precision on the CPU -- this only has to decide *where* to zoom once per
// frame, not once per pixel, so it gets far more headroom (53-bit mantissa)
// than the GPU's double-float emulation needs for that decision. It
// continuously hill-climbs the view center toward nearby boundary detail,
// so the dive stays visually interesting instead of drifting into a flat
// interior "lake" or a featureless far exterior -- which is what a blind
// sinusoidal wander could do. When it reaches the precision floor it loops
// back to its starting point and dives again.
class FractalNavigator
{
public:
    FractalNavigator(double startCx, double startCy, bool burningShipVariant);

    // dt: seconds since last call, already clamped by the caller.
    // zoomRatePerSecond: view radius multiplier per second (e.g. 0.9 means
    // it shrinks to 90% of itself every second) -- expected in (0, 1).
    void update(double dt, double zoomRatePerSecond, std::mt19937& rng);

    double centerX() const noexcept { return cx; }
    double centerY() const noexcept { return cy; }
    double radius() const noexcept { return viewRadius; }

    // 0 right after a reset, easing to 1 over ~1.5s -- multiply brightness
    // by this so a reset reads as a gentle fade rather than a visible pop.
    float fadeEnvelope() const noexcept;

private:
    double distanceEstimate(double px, double py) const;
    void reset();

    const bool isBurningShip;
    const double homeCx, homeCy;

    double cx, cy;
    double targetX, targetY;
    double viewRadius;
    double searchTimer = 0.0;
    double timeSinceReset = 0.0;
};
