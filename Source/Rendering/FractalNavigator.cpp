#include "FractalNavigator.h"
#include "DoubleDouble.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    constexpr double refBailout = 1.0e8;

    // Depth at which the reference orbit stops being trustworthy on the
    // GPU, with a safety margin. The CPU computes it at double-double
    // (~32 decimal digit) precision, but each point is downcast to
    // double-float (~13-14 decimal digits) for GPU upload (see
    // ddToFloatPair) -- *that* downcast is the real ceiling: past roughly
    // 1e13x zoom, per-pixel offsets are too small to survive being added
    // to a reference value without the bits to represent anything finer.
    constexpr double radiusFloor = 1.0e-11;

    // Manual mode can always zoom back out to see the whole set.
    constexpr double maxManualRadius = 4.0;

    // Iteration depth needed to resolve boundary detail grows with zoom
    // depth -- roughly linearly per decade. This is deliberately the same
    // curve as fractalIterCap in common.glsl, so the CPU's notion of
    // "inside" / "has detail" matches what the GPU will actually render.
    int iterCapForRadius(double radius)
    {
        const double decades = std::max(0.0, -std::log10(std::max(radius, 1.0e-13)));
        return (int) std::clamp(200.0 + decades * 70.0, 200.0, 2400.0);
    }

    // Deep cap used by the boundary bisection itself: the level set of
    // "survives this many iterations" converges to the true boundary as the
    // cap grows, so bisecting against a cap well beyond anything the
    // renderer will ever need guarantees boundary detail at every reachable
    // depth.
    constexpr int bisectionCap = 2000;

    struct Ray { double sx, sy, tx, ty; };
}

FractalNavigator::FractalNavigator(FractalFormula formulaToUse, Mode modeToUse, double startCx, double startCy,
                                   double startViewRadius)
    : formula(formulaToUse), mode(modeToUse), homeCx(startCx), homeCy(startCy), homeRadius(startViewRadius),
      cx(startCx), cy(startCy), refX(startCx), refY(startCy), viewRadius(startViewRadius)
{
}

float FractalNavigator::fadeEnvelope() const noexcept
{
    if (mode == Mode::Manual)
        return 1.0f; // interaction should feel immediate, never faded
    const double t = std::clamp(timeSinceLock / 1.5, 0.0, 1.0);
    return (float) (t * t * (3.0 - 2.0 * t)); // smoothstep
}

void FractalNavigator::stepFormula(double& zx, double& zy, double px, double py) const
{
    double x = zx, y = zy;
    switch (formula)
    {
        case FractalFormula::Mandelbrot:
            zx = x * x - y * y + px;
            zy = 2.0 * x * y + py;
            break;
        case FractalFormula::BurningShip:
            x = std::abs(x);
            y = std::abs(y);
            zx = x * x - y * y + px;
            zy = 2.0 * x * y + py;
            break;
        case FractalFormula::PerpendicularShip:
            y = std::abs(y);
            zx = x * x - y * y + px;
            zy = 2.0 * x * y + py;
            break;
        case FractalFormula::Buffalo:
            x = std::abs(x);
            y = std::abs(y);
            zx = std::abs(x * x - y * y) + px;
            zy = 2.0 * x * y + py;
            break;
        case FractalFormula::Tricorn:
            zx = x * x - y * y + px;
            zy = -2.0 * x * y + py;
            break;
    }
}

int FractalNavigator::escapeIterationCount(double px, double py, int cap) const
{
    double zx = 0.0, zy = 0.0;
    int i = 0;
    for (; i < cap; ++i)
    {
        stepFormula(zx, zy, px, py);
        if (zx * zx + zy * zy > 4.0)
            break;
    }
    return i;
}

int FractalNavigator::detailSpreadAt(double px, double py, double sampleRadius) const
{
    const int cap = iterCapForRadius(sampleRadius);
    int minCount = escapeIterationCount(px, py, cap);
    int maxCount = minCount;
    for (int k = 0; k < 8; ++k)
    {
        const double angle = k * (6.28318530718 / 8.0);
        const int count = escapeIterationCount(px + std::cos(angle) * sampleRadius,
                                               py + std::sin(angle) * sampleRadius, cap);
        minCount = std::min(minCount, count);
        maxCount = std::max(maxCount, count);
    }
    return maxCount - minCount;
}

bool FractalNavigator::hasVisibleDetail() const
{
    // With the center locked ON the boundary by bisection this should
    // essentially never fail -- it remains purely as a safety net.
    return std::max(detailSpreadAt(cx, cy, viewRadius * 0.4),
                    detailSpreadAt(cx, cy, viewRadius * 0.9)) >= 12;
}

void FractalNavigator::bisectToBoundary(double inX, double inY, double outX, double outY, double& bx,
                                        double& by) const
{
    // ~90 halvings shrinks the bracket to one ulp of a double: the inside
    // endpoint ends up on the boundary to ~1e-16 -- five orders of
    // magnitude below the zoom floor, so the boundary can never leave the
    // view during a dive.
    for (int k = 0; k < 90; ++k)
    {
        const double mx = 0.5 * (inX + outX);
        const double my = 0.5 * (inY + outY);
        if (mx == inX && my == inY) // bracket collapsed to a single ulp
            break;
        if (isInside(mx, my, bisectionCap))
        {
            inX = mx;
            inY = my;
        }
        else
        {
            outX = mx;
            outY = my;
        }
    }
    bx = inX;
    by = inY;
}

void FractalNavigator::lockOnto(double px, double py, bool resetFade)
{
    const DD cxDD = ddFromDouble(px);
    const DD cyDD = ddFromDouble(py);

    refOrbitFloats.clear();
    refOrbitFloats.reserve((size_t) maxReferenceIterations * 4);

    DD zx = ddFromDouble(0.0);
    DD zy = ddFromDouble(0.0);
    int storedCount = 0;

    for (int iter = 0; iter < maxReferenceIterations; ++iter)
    {
        float reHi, reLo, imHi, imLo;
        ddToFloatPair(zx, reHi, reLo);
        ddToFloatPair(zy, imHi, imLo);
        refOrbitFloats.push_back(reHi);
        refOrbitFloats.push_back(reLo);
        refOrbitFloats.push_back(imHi);
        refOrbitFloats.push_back(imLo);
        ++storedCount;

        // Same formula as stepFormula, in double-double.
        DD x = zx, y = zy;
        switch (formula)
        {
            case FractalFormula::Mandelbrot:
                zx = ddAdd(ddSub(ddMul(x, x), ddMul(y, y)), cxDD);
                zy = ddAdd(ddAdd(ddMul(x, y), ddMul(x, y)), cyDD);
                break;
            case FractalFormula::BurningShip:
                x = ddAbs(x);
                y = ddAbs(y);
                zx = ddAdd(ddSub(ddMul(x, x), ddMul(y, y)), cxDD);
                zy = ddAdd(ddAdd(ddMul(x, y), ddMul(x, y)), cyDD);
                break;
            case FractalFormula::PerpendicularShip:
                y = ddAbs(y);
                zx = ddAdd(ddSub(ddMul(x, x), ddMul(y, y)), cxDD);
                zy = ddAdd(ddAdd(ddMul(x, y), ddMul(x, y)), cyDD);
                break;
            case FractalFormula::Buffalo:
                x = ddAbs(x);
                y = ddAbs(y);
                zx = ddAdd(ddAbs(ddSub(ddMul(x, x), ddMul(y, y))), cxDD);
                zy = ddAdd(ddAdd(ddMul(x, y), ddMul(x, y)), cyDD);
                break;
            case FractalFormula::Tricorn:
                zx = ddAdd(ddSub(ddMul(x, x), ddMul(y, y)), cxDD);
                zy = ddAdd(ddNeg(ddAdd(ddMul(x, y), ddMul(x, y))), cyDD);
                break;
        }

        if (zx.hi * zx.hi + zy.hi * zy.hi > refBailout)
            break; // orbit escaped -- nothing meaningful beyond this to store
    }

    refX = px;
    refY = py;
    refOrbitLen = storedCount;
    orbitDirty = true;
    locked = true;
    refAnchorRadius = viewRadius;
    refStale = false;
    if (resetFade)
        timeSinceLock = 0.0;
}

void FractalNavigator::freshSeekAndLock(std::mt19937& rng, bool resetFade)
{
    // Curated inside seeds and detail-rich target regions per formula.
    // Seeds are verified inside at runtime (with (0,0) -- inside for every
    // formula here, since the orbit just stays at c -- as the guaranteed
    // fallback); targets only aim the ray, the bisection finds the actual
    // boundary along it.
    std::array<std::array<double, 2>, 3> seeds {};
    std::array<std::array<double, 2>, 4> targets {};
    int numSeeds = 1, numTargets = 0;
    seeds[0] = { 0.0, 0.0 };

    switch (formula)
    {
        case FractalFormula::Mandelbrot:
            seeds = { { { -0.5, 0.0 }, { -1.0, 0.0 }, { -0.1226, 0.7449 } } };
            numSeeds = 3;
            targets = { { { -0.7454, 0.1130 },   // seahorse valley
                          { 0.2820, 0.0110 },    // elephant valley
                          { -1.4012, 0.0004 },   // antenna / Feigenbaum tip
                          { -0.1592, 1.0317 } } }; // top-bulb filaments
            numTargets = 4;
            break;
        case FractalFormula::BurningShip:
            seeds[0] = { 0.0, 0.0 };
            numSeeds = 1;
            targets = { { { -1.7550, -0.0300 },  // the armada
                          { -1.6250, -0.0350 },
                          { -1.8610, -0.0060 },
                          { -0.5880, -0.5660 } } }; // main ship's prow
            numTargets = 4;
            break;
        default:
            // Variants: random rays from the origin; the detail scoring
            // below is what selects a good one.
            numSeeds = 1;
            numTargets = 0;
            break;
    }

    std::uniform_real_distribution<double> jitter(-0.03, 0.03);
    std::uniform_real_distribution<double> angleDist(0.0, 6.28318530718);
    std::uniform_int_distribution<int> seedPick(0, numSeeds - 1);

    double bestX = homeCx, bestY = homeCy;
    int bestScore = -1;

    for (int attempt = 0; attempt < 6; ++attempt)
    {
        double sx = seeds[(size_t) seedPick(rng)][0];
        double sy = seeds[(size_t) seedPick(rng)][1];
        if (! isInside(sx, sy, bisectionCap))
        {
            sx = 0.0;
            sy = 0.0;
        }

        double tx, ty;
        if (numTargets > 0)
        {
            std::uniform_int_distribution<int> targetPick(0, numTargets - 1);
            const auto& t = targets[(size_t) targetPick(rng)];
            tx = t[0] + jitter(rng);
            ty = t[1] + jitter(rng);
        }
        else
        {
            const double a = angleDist(rng);
            tx = sx + 2.2 * std::cos(a);
            ty = sy + 2.2 * std::sin(a);
        }

        // Make sure the far end really is outside; every formula here
        // escapes for |c| > 3, so a few extensions along the ray always
        // gets there.
        double dx = tx - sx, dy = ty - sy;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0e-9)
            continue;
        dx /= len;
        dy /= len;
        double reach = len;
        for (int ext = 0; ext < 10 && isInside(sx + dx * reach, sy + dy * reach, bisectionCap); ++ext)
            reach *= 1.4;
        const double ox = sx + dx * reach;
        const double oy = sy + dy * reach;
        if (isInside(ox, oy, bisectionCap))
            continue;

        double bx, by;
        bisectToBoundary(sx, sy, ox, oy, bx, by);

        // Score by real escape-count variety around the landing point at
        // two very different scales -- filament-rich regions show a big
        // spread at both, smooth cardioid-style edges don't.
        const int score = std::min(detailSpreadAt(bx, by, 3.0e-3), detailSpreadAt(bx, by, 1.0e-5));
        if (score > bestScore)
        {
            bestScore = score;
            bestX = bx;
            bestY = by;
        }
        if (bestScore >= 60)
            break; // plenty rich -- no need to keep looking
    }

    cx = bestX;
    cy = bestY;
    lockOnto(bestX, bestY, resetFade);
    detailCheckTimer = 0.0;
}

bool FractalNavigator::inViewRelock()
{
    // Detail ran out mid-dive (safety net -- see hasVisibleDetail):
    // bisect between an inside and an outside sample of the *current*
    // view, so the new lock stays visually nearby instead of jumping to a
    // fresh region.
    const int cap = std::min(iterCapForRadius(viewRadius) + 200, bisectionCap);
    double inX = 0.0, inY = 0.0, outX = 0.0, outY = 0.0;
    int bestInside = -1, bestOutside = cap + 1;

    for (int gy = -3; gy <= 3; ++gy)
    {
        for (int gx = -3; gx <= 3; ++gx)
        {
            const double px = cx + (gx / 3.0) * viewRadius * 0.8;
            const double py = cy + (gy / 3.0) * viewRadius * 0.8;
            const int count = escapeIterationCount(px, py, cap);
            if (count >= cap && count > bestInside)
            {
                bestInside = count;
                inX = px;
                inY = py;
            }
            else if (count < bestOutside)
            {
                bestOutside = count;
                outX = px;
                outY = py;
            }
        }
    }

    if (bestInside < 0 || bestOutside > cap)
        return false; // view is entirely one-sided -- caller does a full reset

    double bx, by;
    bisectToBoundary(inX, inY, outX, outY, bx, by);
    cx = bx;
    cy = by;
    lockOnto(bx, by, true);
    return true;
}

void FractalNavigator::refreshIterNeed(double dt)
{
    // Sample real escape counts on two rings across the view; the largest
    // finite one (plus margin) is what the GPU must run to render this
    // view without falsely blacking out near-boundary pixels as
    // "interior". A fixed per-decade growth guess was tried first and
    // failed -- escape-time growth varies wildly by location. Rises
    // instantly (a rising need means detail is about to vanish), falls
    // slowly to avoid flicker.
    constexpr int cap = maxReferenceIterations - 60;
    int need = 0;
    bool anyEscaped = false;
    const double radii[2] = { 0.45, 0.95 };
    for (double radiusFrac : radii)
    {
        for (int k = 0; k < 6; ++k)
        {
            const double angle = k * (6.28318530718 / 6.0);
            const int count = escapeIterationCount(cx + std::cos(angle) * viewRadius * radiusFrac,
                                                   cy + std::sin(angle) * viewRadius * radiusFrac, cap);
            if (count < cap)
            {
                anyEscaped = true;
                need = std::max(need, count);
            }
        }
    }
    if (! anyEscaped)
        need = cap; // whole ring is interior at this cap: budget exhausted

    const double target = std::clamp(need * 1.3 + 80.0, 250.0, (double) (maxReferenceIterations - 50));
    if (target > iterNeedSmoothed)
        iterNeedSmoothed = target;
    else
        iterNeedSmoothed += (target - iterNeedSmoothed) * std::min(1.0, dt * 0.5);
}

void FractalNavigator::update(double dt, double zoomRatePerSecond, std::mt19937& rng)
{
    if (mode == Mode::Manual)
    {
        tick(dt);
        return;
    }

    dt = std::clamp(dt, 0.0, 0.1);
    timeSinceLock += dt;

    if (! locked)
    {
        freshSeekAndLock(rng, true);
        return;
    }

    viewRadius *= std::pow(std::clamp(zoomRatePerSecond, 0.01, 0.999), dt);
    refreshIterNeed(dt);

    // Two honest walls, whichever comes first: the GPU orbit's double-
    // float precision (radiusFloor), and the iteration budget -- once the
    // view needs more iterations than the reference orbit can carry, its
    // detail is mathematically about to vanish, so reset gracefully to a
    // fresh boundary point instead of letting it fade to black.
    if (viewRadius <= radiusFloor || iterNeedSmoothed >= (double) (maxReferenceIterations - 70))
    {
        viewRadius = homeRadius;
        freshSeekAndLock(rng, true);
        iterNeedSmoothed = 400.0;
        return;
    }

    detailCheckTimer += dt;
    if (detailCheckTimer > 4.0)
    {
        detailCheckTimer = 0.0;
        if (! hasVisibleDetail())
        {
            if (! inViewRelock())
            {
                viewRadius = homeRadius;
                freshSeekAndLock(rng, true);
                iterNeedSmoothed = 400.0;
            }
        }
    }
}

void FractalNavigator::zoomBy(double factor)
{
    viewRadius = std::clamp(viewRadius * factor, radiusFloor, maxManualRadius);
    if (viewRadius < refAnchorRadius / 3.0 || viewRadius > refAnchorRadius * 3.0)
        refStale = true;
}

void FractalNavigator::panBy(double dxFraction, double dyFraction)
{
    cx += dxFraction * viewRadius;
    cy += dyFraction * viewRadius;
    const double dx = cx - refX, dy = cy - refY;
    if (std::sqrt(dx * dx + dy * dy) > viewRadius * 0.5)
        refStale = true;
}

void FractalNavigator::tick(double dt)
{
    dt = std::clamp(dt, 0.0, 0.1);
    timeSinceLock += dt;
    refreshIterNeed(dt);
    refRefreshCooldown -= dt;
    if (refStale && refRefreshCooldown <= 0.0)
    {
        refreshManualReference();
        refRefreshCooldown = 0.15; // throttle: at most ~7 re-anchors/second while dragging
    }
}

void FractalNavigator::refreshManualReference()
{
    // Perturbation needs a reference whose orbit escapes as late as
    // possible (ideally never); anchor onto the longest-surviving point in
    // the current view. Ties go to the sample nearest the center.
    const int cap = std::min(iterCapForRadius(viewRadius) + 300, maxReferenceIterations - 100);
    double bestX = cx, bestY = cy;
    int bestCount = -1;
    double bestDist = 1.0e30;

    for (int gy = -6; gy <= 6; ++gy)
    {
        for (int gx = -6; gx <= 6; ++gx)
        {
            const double px = cx + (gx / 6.0) * viewRadius * 0.75;
            const double py = cy + (gy / 6.0) * viewRadius * 0.75;
            const int count = escapeIterationCount(px, py, cap);
            const double dist = (double) (gx * gx + gy * gy);
            if (count > bestCount || (count == bestCount && dist < bestDist))
            {
                bestCount = count;
                bestDist = dist;
                bestX = px;
                bestY = py;
            }
        }
    }

    lockOnto(bestX, bestY, false); // note: lockOnto sets refX/refY, not cx/cy -- center stays put
}

void FractalNavigator::getReferenceOffset(float& xHi, float& xLo, float& yHi, float& yLo) const noexcept
{
    // TwoSum makes the double subtraction exact as a hi+lo pair before the
    // float downcast, so the anchor offset carries double-float precision.
    double errX = 0.0, errY = 0.0;
    const double sx = ddTwoSum(cx, -refX, errX);
    const double sy = ddTwoSum(cy, -refY, errY);
    ddToFloatPair(DD { sx, errX }, xHi, xLo);
    ddToFloatPair(DD { sy, errY }, yHi, yLo);
}
