# Kaleidosonic

An audio-reactive, fully DAW-automatable VST3 visualizer plugin. Load it on
any channel and it renders GPU-shader fractals and kaleidoscopic feedback
tunnels that pulse, zoom, and morph with the audio passing through it. Every
visual parameter is exposed as a normal VST3 parameter, so you can draw
automation curves for it in your DAW exactly like you would for a filter
cutoff or a reverb mix.

Audio passes through completely unmodified — this is a pure visualizer.

## Features

- **22 GLSL presets**, switchable and cross-fadable while the DAW automates
  them:
  - **Mandelbrot Pulse** — kaleidoscope-folded, multi-scale deep dive into
    the Mandelbrot set. Where to zoom is decided on the CPU
    (`Source/Rendering/FractalNavigator.h/.cpp`) by **boundary bisection**:
    a point known to be inside the set and one known to be outside are
    bisected ~90 times, landing the lock ON the set boundary to the last
    ulp of a double (~1e-16) — five orders of magnitude below the zoom
    floor, so the boundary (where all the detail lives) mathematically
    cannot leave the frame at any depth of the dive. Rendered by
    **perturbation theory**: a double-double (~32-digit) CPU reference
    orbit, with each pixel iterated as a tiny double-float offset from it,
    plus a per-pixel **distance estimate** (derivative tracking) that
    carves a crisp dark crease along every filament — sharp fractal edges,
    black interior, fast-cycling cosine-palette bands, no blobs.
  - **Burning Ship** — same treatment for z = (|Re z| + i|Im z|)² + c.
  - **Mandelbrot Explorer / Burning Ship Explorer** — the classic sets
    rendered plainly (no kaleidoscope), manually navigated: **mouse wheel
    or Up/Down arrows zoom, dragging pans**. The reference orbit re-anchors
    itself onto the best point in view as you move, so detail stays crisp
    to ~1e11x zoom.
  - **Perpendicular Ship / Buffalo Fractal / Tricorn** — three classic
    variations of the Burning Ship / Mandelbrot family
    ((Re z + i|Im z|)², the post-square-abs "buffalo", and conj(z)²),
    each with the same manual explorer navigation.
  - **Burning Ship 3D** — the Burning Ship's abs() fold lifted into true
    3D: a raymarched power-2 triplex ("Mandelbulb-style") iteration with
    every component folded through abs(), orbit-trap coloring, bass
    morphing the structure.
  - **Mandelbox** — Tom Lowe's box-fold/ball-fold IFS fractal, raymarched
    in true 3D; infinite nested detail by construction.
  - **Julia Kaleidoscope** — a drifting Julia set folded through N mirror
    segments, orbit driven by the mid band.
  - **Sierpinski Triforce** — a real Sierpinski gasket with a **literally
    infinite zoom**: the dive is centered on a corner vertex, where the
    gasket is exactly self-similar under scaling by 2, and the zoom
    exponent wraps modulo log2(2) — the image at phase 0 and 1 is
    pixel-identical, so the dive is seamless forever in plain float
    precision. Crisp antialiased triangle edges, level-continuous colors.
  - **Apollonian Gasket** — repeated circle-inversion folding, the classic
    "infinite nested circles" fractal.
  - **Plasma Feedback**, **IFS Tunnel**, **Tunnel Spiral**,
    **Raymarch Tunnel 3D**, **Particle Bloom**, **Oscilloscope Glow**,
    **Waveform Scope**, **Fractal Bubbles**, **Starfield Warp** — feedback,
    tunnel, particle, and scope-style presets.
  - **Audio Nebula** — domain-warped FBM noise clouds (the "iq warping"
    technique: the noise field warps its own sampling coordinates, twice),
    which turns flat noise into real billowing smoke. Bass drives the warp
    amplitude so the clouds physically churn on the low end, treble adds a
    fine shimmer octave, and onsets flash lightning through the cloud body
    along ridged-noise filaments.
- **Real-time audio analysis** (FFT-based): bass/mid/treble band energy,
  overall level, spectral-flux onset ("beat") detection, a 2048-sample
  rolling waveform buffer, and **auto-gain** normalization so reactivity
  tracks the *dynamics* of whatever's playing instead of absolute loudness.
- **35 automatable parameters**, including a **Palette** parameter (0–8)
  that sweeps/crossfades through curated cosine-gradient palettes
  (Spectrum, Fire, Ice, Synthwave, Sunset, Forest, Mono, Psychedelic) on
  top of the Hue knob, plus fifteen global post-FX (Trails, Blur, Noise,
  Datamosh, Bloom, Vignette, Chromatic Aberration, Color Cycle Speed,
  Pulse Depth, Posterize, Fisheye, Trail Direction, Flame, Shine, Gummy).
  Flame streams bright content along an adjustable direction (0° = up)
  with turbulence and warm ember decay, so it reads as rising fire (or
  drips/streaks in any direction you dial in); Shine grows anisotropic
  star-streak specular glints out of hot spots with a glossy response
  curve; Gummy adds a soft audio-breathing screen-space wobble plus a
  milky response lift, so the whole image reads as translucent, lit
  jelly. All three are 0 by default and layer on top of *any* preset.
- **Manual control panel** (collapsible, scrollable, opaque backdrop so
  labels stay readable over any visual) with a slider for every parameter.
  Sliders deliberately ignore the mouse wheel so wheel-scrolling the panel
  never yanks values.
- **Explorer navigation**: on the explorer presets, mouse wheel / Up-Down
  arrows zoom and dragging the visual pans; over the panel the wheel just
  scrolls the panel.
- **Fullscreen toggle** (button or `F` key, `Esc` to exit).
- Builds as **VST3** and **Standalone** (JUCE multi-format target).

## How the deep zoom actually works

Three different mechanisms, matched to the math of each fractal family:

1. **Escape-time sets (Mandelbrot/Burning Ship + variants)** — perturbation
   theory (as in Kalles Fraktaler; see mathr.co.uk's "Deep zoom theory and
   practice"): one reference orbit per lock computed on the CPU in
   double-double (~32 digits, `Source/Rendering/DoubleDouble.h`), every
   pixel iterated as a double-float offset from it, with Pauldelbrot's
   glitch-detection rebase. Each orbit point is downcast to double-float
   (~13-14 digits) for GPU upload — that downcast is the honest depth
   ceiling (~1e11x with margin), enforced as the zoom floor. The autopilot
   presets lock ON the boundary via bisection, so detail is guaranteed at
   every depth until that floor, where the dive gracefully resets to a
   fresh boundary point. The GPU iteration budget is *measured*, not
   guessed: the navigator samples real escape counts around the current
   view every frame and hands the GPU exactly what it needs, and the dive
   resets gracefully if the need ever outgrows the reference orbit — so
   deep views can never wash out to a flat color by running out of
   iterations.
2. **Exactly self-similar IFS fractals (Sierpinski)** — zoom into a fixed
   point of the self-similarity and wrap the zoom *exponent*; literally
   infinite, seamless, plain float. (Apollonian/Julia use unbounded
   double-float zoom to ~1e13x.)
3. **Bounded-fold raymarched 3D fractals (Mandelbox, Burning Ship 3D)** —
   the iterated fold keeps every value bounded by construction; detail
   comes from the fold itself, so ordinary float32 is plenty forever.

## Project layout

```
CMakeLists.txt              Build config; fetches JUCE via FetchContent
Source/
  PluginProcessor.*         Audio passthrough + wiring
  PluginEditor.*            GUI: OpenGL visual background + control panel + explorer input
  AudioAnalyzer.*           FFT band energy + onset detection (audio thread)
  VisualizerParameters.*    APVTS parameter layout (the automation surface)
  Presets/PresetManager.*   Maps preset index -> GLSL source (BinaryData)
  Rendering/VisualizerRenderer.*  OpenGL context, FBOs, per-frame uniforms, fractal slots
  Rendering/FractalNavigator.*    CPU boundary-bisection autopilot + manual explorer navigation
  Rendering/DoubleDouble.h        Double-double (~32 digit) CPU arithmetic for reference orbits
Shaders/
  common.glsl                Shared uniforms/helpers (double-float math,
                              perturbation core, palettes, fractal shading),
                              prepended to every preset at compile time
  fullscreen.vert             Fullscreen-triangle vertex shader (shared)
  mandelbrot_pulse.frag       burning_ship.frag        julia_kaleidoscope.frag
  mandelbrot_explorer.frag    burning_ship_explorer.frag
  perpendicular_ship.frag     buffalo.frag             tricorn.frag
  burning_ship_3d.frag        mandelbox.frag           sierpinski_triforce.frag
  apollonian.frag             plasma_feedback.frag     ifs_tunnel.frag
  tunnel_spiral.frag          raymarch_tunnel.frag     particle_bloom.frag
  oscilloscope.frag           waveform_scope.frag      fractal_bubbles.frag
  starfield_warp.frag         audio_nebula.frag
```

The renderer pipeline is two stages: the selected preset (or two, cross-
fading, while Preset Morph is in motion) renders into an offscreen "raw"
buffer, then a global post-process pass blends that against last frame's
fully-processed output using Trails/Blur/Noise/Datamosh before blitting to
screen — see the comment at the top of
[VisualizerRenderer.h](Source/Rendering/VisualizerRenderer.h) for the exact
data flow.

## Building

**Prerequisites (Windows):**
- [CMake](https://cmake.org/download/) 3.22+
- Visual Studio 2022 with the **"Desktop development with C++"** workload
- Git
- Internet access on first configure — CMake fetches JUCE 8.0.15 from GitHub
  via `FetchContent`

> If a preset or the post-process pass ever fails to compile at runtime,
> it's logged (with which preset and the compiler's error text) to
> `%APPDATA%\Kaleidosonic\Kaleidosonic.log` even in Release builds — check
> there first if something renders black.

**Configure & build (Release):**

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The VST3 lands at
`build/Kaleidosonic_artefacts/Release/VST3/Kaleidosonic.vst3` and the
standalone app at
`build/Kaleidosonic_artefacts/Release/Standalone/Kaleidosonic.exe`.

`COPY_PLUGIN_AFTER_BUILD` is off (copying into
`C:\Program Files\Common Files\VST3\` needs admin rights). Either copy the
`.vst3` there yourself once, or point your DAW's plugin scan at
`build/Kaleidosonic_artefacts/Release/VST3/` as a custom search path.

## Parameters

| Parameter | Range | What it does |
|---|---|---|
| Preset | choice (22) | Selects the active shader preset |
| Preset Morph | 0–1 | Cross-fades toward the *next* preset in the list |
| Reactivity | 0–2 | Global multiplier on audio-driven color/brightness response |
| Bass / Mid / Treble Gain | 0–2 each | Per-band weighting before it hits the shaders |
| Zoom Speed | -1–1 | Fractal zoom/travel rate (autopilot presets) |
| Rotation Speed | -1–1 | Rotation rate |
| Hue | 0–1 | Base color hue (phase offset within the palette) |
| Saturation | 0–1 | Color saturation |
| Brightness | 0–2 | Output brightness |
| Contrast | 0–2 | Output contrast |
| Kaleidoscope Segments | 1–16 | Radial mirror-fold count |
| Feedback | 0–0.98 | Internal trail persistence (Plasma Feedback preset only) |
| Iterations | 4–64 | Fractal/tunnel detail depth (scales the escape-time base iteration budget) |
| Distortion | 0–1 | Extra coordinate warping |
| Zoom Wander | 0–2 | How far zoom targets wander (Julia/Apollonian-style presets) |
| Camera Shake | 0–2 | How hard bass/onsets drive camera/zoom motion — independent of Reactivity |
| Camera Scale | 0.2–6 | Manual zoom multiplier (autopilot/tunnel presets; explorers use wheel/arrows instead) |
| Palette | 0–8 | Sweeps/crossfades through the curated cosine-gradient palettes |
| Trails | 0–0.97 | Global motion-trail persistence |
| Blur | 0–1 | Global screen-space blur |
| Noise | 0–1 | Global animated grain, brighter on beats |
| Datamosh | 0–1 | Global P-frame-style glitch effect, intensifies on beats |
| Bloom Intensity | 0–2 | Scales the always-on glow |
| Vignette | 0–1 | Darkens the frame edges |
| Chromatic Aberration | 0–1 | Standalone edge-growing RGB split |
| Color Cycle Speed | 0–2 | Scales the automatic hue-drift rate (0 = off) |
| Pulse Depth | 0–2 | Scales the automatic audio-tied brightness pulse (0 = off) |
| Posterize | 0–1 | Quantizes color into hard bands |
| Fisheye | 0–1 | Central lens-warp radial distortion |
| Trail Direction | -180–180° | Direction Flame's trails stream (0 = up) |
| Flame | 0–1 | Bright content streams/licks along Trail Direction with warm ember decay |
| Shine | 0–1 | Anisotropic star-streak specular on hot spots + glossy response curve |
| Gummy | 0–1 | Soft audio-breathing screen-space jelly wobble + milky response lift |

## Adding a new preset

1. Drop a new `.frag` file in `Shaders/` that implements `void main()` using
   the uniforms/helpers declared in `Shaders/common.glsl` (it's prepended to
   your file automatically at compile time — don't redeclare `#version` or
   the uniforms).
2. Add its display name to `PresetNames::all` in
   [Source/VisualizerParameters.h](Source/VisualizerParameters.h) — order
   matters, it drives the `Preset` choice parameter.
3. Add the matching `BinaryData::your_file_frag` entry to `presetResources`
   in [Source/Presets/PresetManager.cpp](Source/Presets/PresetManager.cpp),
   in the same order.
4. If it's an escape-time fractal needing a navigator, add a `FractalSlot`
   in `VisualizerRenderer`'s constructor with its preset index and formula.
5. Re-run CMake configure (new shader files need to be picked up by the
   `file(GLOB ...)` in `CMakeLists.txt`) and rebuild.

## Known follow-ups

- No preset persistence beyond the host's own automation/session state yet.
- Audio-reactivity coefficients were tuned by inspection, not against real
  program material — Reactivity and the per-band Gains are the first knobs
  to adjust if everything under-/over-reacts to your mix.
- The escape-time deep zoom's ~1e11x depth is a real limit of GPU
  double-float perturbation rendering, not a bug; the autopilot dives reset
  gracefully (fade to a fresh boundary point) at the floor, and the
  explorers simply stop zooming deeper there.
- If you're iterating in a DAW: VST3 hosts keep the plugin's `.dll`
  memory-mapped while it's loaded, so a rebuild can fail with `LNK1104`
  until you remove/reload the device (or restart the host). The Standalone
  target is the faster loop while iterating.
