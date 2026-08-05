#pragma once

#include <random>
#include <vector>

// Which escape-time iteration formula a navigator (and its matching preset
// shader) uses. Values match the integer `variant` passed to common.glsl's
// perturbEscapeTime, so CPU reference orbit and GPU per-pixel iteration can
// never disagree about which fractal they are computing.
enum class FractalFormula
{
    Mandelbrot = 0,        // z^2 + c
    BurningShip = 1,       // (|Re z| + i|Im z|)^2 + c
    PerpendicularShip = 2, // (Re z + i|Im z|)^2 + c
    Buffalo = 3,           // |Re(w^2)| + i Im(w^2) + c, w = |Re z| + i|Im z|
    Tricorn = 4,           // conj(z)^2 + c
};

// Autopilot or manual navigation for a real-time deep zoom into an
// escape-time fractal, built on perturbation theory -- the same technique
// real deep-zoom fractal tools (Kalles Fraktaler, etc.) use, see
// mathr.co.uk's "Deep zoom theory and practice" and the SuperFractalThing
// paper.
//
// Autopilot mode -- the actual fix for "eventually zooms into nothing":
// every earlier approach locked onto a point *near* the boundary (found by
// a distance-estimator search), but any such point sits some fixed real
// distance from the boundary, and a continuously-shrinking view radius
// mathematically must eventually pass that distance and show a flat,
// featureless patch. No tuning avoids that. Instead, the seek is now a
// plain bisection: take a point known to be inside the set and one known
// to be outside, and halve the bracket ~90 times. That converges to a
// point ON the boundary to the last ulp of a double (~1e-16), which is far
// below the zoom floor (~1e-11) -- so the boundary, where all the detail
// lives, is guaranteed to remain inside every view of the entire dive.
// Candidate rays are aimed at curated detail-rich regions and scored by
// actually sampling escape-count variation around the landing point at
// several scales, so the lock also *looks* good, not just mathematically
// correct. A rare, cheap sampled-detail check remains purely as a safety
// net.
//
// Manual mode (the "explorer" presets): no autopilot at all. The center
// and radius move only in response to zoomBy/panBy (mouse wheel / arrow
// keys / drag from the editor). The reference orbit is re-anchored, at a
// throttled rate, onto the highest-escape-count point found in the current
// view (references must escape late or never for perturbation to stay
// accurate), and the center-minus-reference offset is handed to the GPU so
// pixels are rendered as offsets from that anchor rather than from the
// view center.
//
// A "reference orbit" is a sequence of points computed once per (re)anchor
// using double-double (~32 decimal digit) CPU precision (see
// DoubleDouble.h). Every pixel then renders as a tiny, cheap double-float
// *offset* from that orbit (perturbation theory). Each orbit point is
// downcast to double-float (~13-14 digits) for GPU upload; that downcast is
// the real depth ceiling (~1e11x, with margin), hence the zoom floor.
class FractalNavigator
{
public:
    // Reference orbit iteration cap -- also the GPU orbit texture's fixed
    // width, so VisualizerRenderer can size that texture from one source
    // of truth. Deep views need iteration counts that grow with depth
    // (roughly 70 per decade of zoom -- see iterCapForRadius /
    // fractalIterCap in common.glsl), so this comfortably covers the
    // deepest reachable view plus headroom.
    static constexpr int maxReferenceIterations = 2500;

    enum class Mode { Autopilot, Manual };

    FractalNavigator(FractalFormula formulaToUse, Mode modeToUse, double startCx, double startCy,
                     double startViewRadius);

    // Autopilot only. dt: seconds since last call. zoomRatePerSecond: view
    // radius multiplier per second (e.g. 0.9 = shrinks to 90% every
    // second), expected in (0, 1).
    void update(double dt, double zoomRatePerSecond, std::mt19937& rng);

    // Manual only. zoomBy multiplies the view radius (values < 1 zoom in);
    // panBy moves the center by fractions of the current view radius.
    // tick() advances timers and performs the throttled reference-orbit
    // re-anchor when the view has moved/zoomed enough to need one.
    void zoomBy(double factor);
    void panBy(double dxFraction, double dyFraction);
    void tick(double dt);

    double radius() const noexcept { return viewRadius; }

    // 0 right after an autopilot (re)lock, easing to 1 over ~1.5s. Always 1
    // in manual mode -- interaction should feel immediate, never faded.
    float fadeEnvelope() const noexcept;

    // How many iterations the GPU actually needs to resolve the current
    // view, measured directly (real escape counts sampled around the view
    // every frame, smoothed) rather than guessed from a fixed
    // per-decade-of-zoom curve -- escape-time growth varies wildly by
    // location, and a too-slow guess renders everything near the lock as
    // "interior" black. Rises instantly, falls slowly.
    float recommendedIterCap() const noexcept { return (float) iterNeedSmoothed; }

    // Reference orbit data for the GPU: interleaved (re.hi, re.lo, im.hi,
    // im.lo) float32 quadruples, one per iteration, ready to upload as an
    // RGBA32F texture row.
    const std::vector<float>& referenceOrbitData() const noexcept { return refOrbitFloats; }
    int referenceOrbitLength() const noexcept { return refOrbitLen; }

    // (view center - reference point) as GPU double-float pairs, for the
    // uFractalRefOffset uniform. Zero in autopilot mode (the reference IS
    // the center there).
    void getReferenceOffset(float& xHi, float& xLo, float& yHi, float& yLo) const noexcept;

    // True exactly once after a (re)lock/re-anchor produces fresh reference
    // orbit data the caller needs to re-upload; clears itself on read.
    bool consumeOrbitDirty() noexcept
    {
        const bool d = orbitDirty;
        orbitDirty = false;
        return d;
    }

private:
    // One step of this navigator's iteration formula, in plain double.
    void stepFormula(double& zx, double& zy, double px, double py) const;

    int escapeIterationCount(double px, double py, int cap) const;
    bool isInside(double px, double py, int cap) const { return escapeIterationCount(px, py, cap) >= cap; }

    // Escape-count spread across a ring of samples around (px, py) at the
    // given radius -- a direct measure of how much visible variety a view
    // centered there would actually have.
    int detailSpreadAt(double px, double py, double sampleRadius) const;
    bool hasVisibleDetail() const;

    // The bisection itself: halves the inside/outside bracket until the
    // endpoints are one ulp apart, then returns the inside endpoint --
    // a point on the boundary to the full precision of a double.
    void bisectToBoundary(double inX, double inY, double outX, double outY, double& bx, double& by) const;

    // Autopilot seeks: a full fresh one (curated seed/target rays, scored,
    // best of several tries) and an in-view one (re-anchoring inside the
    // current view when its detail has genuinely run out mid-dive).
    void freshSeekAndLock(std::mt19937& rng, bool resetFade);
    bool inViewRelock();

    // Computes and commits the DD reference orbit at (px, py).
    void lockOnto(double px, double py, bool resetFade);

    // Manual mode: re-anchor the reference orbit onto the best (longest-
    // surviving) point in the current view.
    void refreshManualReference();

    // Re-measures iterNeedSmoothed against the current view (see
    // recommendedIterCap).
    void refreshIterNeed(double dt);

    FractalFormula formula;
    Mode mode;
    double homeCx, homeCy, homeRadius;

    bool locked = false;
    double cx, cy;   // view center
    double refX, refY; // reference orbit anchor (== center in autopilot)
    double viewRadius;
    double timeSinceLock = 0.0;
    double detailCheckTimer = 0.0;

    // Manual-mode reference bookkeeping: what the view looked like when the
    // reference was last anchored, to decide when a refresh is needed.
    double refAnchorRadius = 0.0;
    double refRefreshCooldown = 0.0;
    bool refStale = true;
    double iterNeedSmoothed = 400.0;

    std::vector<float> refOrbitFloats;
    int refOrbitLen = 0;
    bool orbitDirty = false;
};
