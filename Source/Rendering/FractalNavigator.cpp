#include "FractalNavigator.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr int deIterations = 260;
    constexpr double bailout = 1.0e8;
    // The GPU renders in double-float (~45-bit, see common.glsl), which
    // starts visibly breaking down into blocky/pixelated mush somewhere
    // around 1e12-1e13x zoom -- well before this CPU navigator's own
    // float64 math runs out of room. The floor here is what actually
    // triggers the loop-back reset, so it needs a healthy safety margin
    // *before* that GPU wall, not right up against it: if it's set too
    // deep, the render is already visibly breaking for several seconds
    // before the CPU notices it's time to reset. 1e-11 keeps peak zoom
    // around ~1e11x, comfortably inside the GPU's precision budget.
    constexpr double radiusFloor = 1.0e-11;
    constexpr double startRadius = 1.4;
}

FractalNavigator::FractalNavigator(double startCx, double startCy, bool burningShipVariant)
    : isBurningShip(burningShipVariant), homeCx(startCx), homeCy(startCy),
      cx(startCx), cy(startCy), targetX(startCx), targetY(startCy), viewRadius(startRadius)
{
}

void FractalNavigator::reset()
{
    cx = targetX = homeCx;
    cy = targetY = homeCy;
    viewRadius = startRadius;
    searchTimer = 0.0;
    timeSinceReset = 0.0;
}

float FractalNavigator::fadeEnvelope() const noexcept
{
    const double t = std::clamp(timeSinceReset / 1.5, 0.0, 1.0);
    return (float) (t * t * (3.0 - 2.0 * t)); // smoothstep
}

double FractalNavigator::distanceEstimate(double px, double py) const
{
    double zx = 0.0, zy = 0.0;
    double dzx = 1.0, dzy = 0.0;

    for (int i = 0; i < deIterations; ++i)
    {
        // dz' = 2*z*dz + 1 (derivative of z -> z^2+c wrt c, accumulated)
        const double newDzx = 2.0 * (zx * dzx - zy * dzy) + 1.0;
        const double newDzy = 2.0 * (zx * dzy + zy * dzx);
        dzx = newDzx;
        dzy = newDzy;

        double zxIn = zx, zyIn = zy;
        if (isBurningShip)
        {
            zxIn = std::abs(zx);
            zyIn = std::abs(zy);
        }

        const double newZx = zxIn * zxIn - zyIn * zyIn + px;
        const double newZy = 2.0 * zxIn * zyIn + py;
        zx = newZx;
        zy = newZy;

        if (zx * zx + zy * zy > bailout)
            break;
    }

    const double zMag = std::sqrt(zx * zx + zy * zy);
    const double dzMag = std::sqrt(dzx * dzx + dzy * dzy);
    if (dzMag < 1.0e-300 || zMag < 1.0e-300)
        return 1.0e6; // deep interior / degenerate -- treat as "far from any boundary"
    return zMag * std::log(zMag) / dzMag;
}

void FractalNavigator::update(double dt, double zoomRatePerSecond, std::mt19937& rng)
{
    dt = std::clamp(dt, 0.0, 0.1);
    timeSinceReset += dt;

    viewRadius *= std::pow(std::clamp(zoomRatePerSecond, 0.01, 0.999), dt);
    if (viewRadius <= radiusFloor)
    {
        reset();
        return;
    }

    // Stays committed to a single point of interest instead of restlessly
    // hopping between candidates: only goes looking for a new target once
    // the current spot has actually drifted out of "good detail" range,
    // and when it does search, it picks whichever viable candidate is
    // *closest* to where the camera already is (not whichever scores best
    // fractal-wide), so retargeting reads as a small course-correction to
    // the nearest interesting detail rather than a jump to some other POI
    // across the view.
    searchTimer += dt;
    if (searchTimer > 0.15)
    {
        searchTimer = 0.0;

        const double idealDE = viewRadius * 0.06;
        const double currentDE = distanceEstimate(cx, cy);
        const bool currentIsGood = std::isfinite(currentDE)
                                    && currentDE > idealDE * 0.25
                                    && currentDE < idealDE * 4.0;

        if (! currentIsGood)
        {
            std::uniform_real_distribution<double> angleDist(0.0, 6.28318530718);
            std::uniform_real_distribution<double> stepDist(0.15, 1.1);

            double bestDist = std::numeric_limits<double>::max();
            double bestX = cx, bestY = cy;
            bool found = false;

            for (int k = 0; k < 20; ++k)
            {
                const double angle = angleDist(rng);
                const double step = viewRadius * stepDist(rng);
                const double candX = cx + std::cos(angle) * step;
                const double candY = cy + std::sin(angle) * step;
                const double candDE = distanceEstimate(candX, candY);
                if (! std::isfinite(candDE))
                    continue;
                if (candDE < idealDE * 0.25 || candDE > idealDE * 4.0)
                    continue; // not viable detail, skip regardless of distance

                if (step < bestDist)
                {
                    bestDist = step;
                    bestX = candX;
                    bestY = candY;
                    found = true;
                }
            }

            // If nothing viable turned up this tick, keep heading toward the
            // existing target and just try again next tick rather than
            // giving up and snapping back to the current position.
            if (found)
            {
                targetX = bestX;
                targetY = bestY;
            }
        }
    }

    // Smoothly drift toward the target -- slow enough that even a genuine
    // retarget reads as a gradual pan, not a snap.
    const double followRate = 1.0 - std::pow(0.5, dt);
    cx += (targetX - cx) * followRate;
    cy += (targetY - cy) * followRate;
}
