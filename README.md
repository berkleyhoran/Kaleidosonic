# Kaleidosonic

An audio-reactive, fully DAW-automatable VST3 visualizer plugin. Load it on
any channel and it renders GPU-shader fractals and kaleidoscopic feedback
tunnels that pulse, zoom, and morph with the audio passing through it. Every
visual parameter is exposed as a normal VST3 parameter, so you can draw
automation curves for it in your DAW exactly like you would for a filter
cutoff or a reverb mix.

Audio passes through completely unmodified — this is a pure visualizer.

## Features

- **14 GLSL presets**, switchable and cross-fadable while the DAW automates
  them:
  - **Mandelbrot Pulse** — classic escape-time Mandelbrot. Where to zoom is
    decided on the CPU by a real double-precision, distance-estimator-
    guided autopilot (`Source/Rendering/FractalNavigator.h/.cpp`) that
    stays committed to one point of interest — it only goes looking for a
    new target once the current spot has actually drifted out of "good
    detail" range, and picks whichever viable candidate is closest to
    where the camera already is, so retargeting reads as a small,
    gradual course-correction rather than jumping between unrelated
    points of interest. The per-pixel iteration then runs in double-float
    (~45-bit) precision instead of plain float32, so it dives genuinely
    deep (roughly 1e11x) before cycling back with a soft fade instead of
    a visible pop — the loop-back point sits with a safety margin before
    the GPU's own precision wall so it resets while detail is still
    crisp, not after it's already turned to blocky mush. Accelerates hard
    with the bass.
  - **Burning Ship** — the spiky/flame-shaped escape-time cousin of
    Mandelbrot, same double-float continuous-dive treatment.
  - **Julia Kaleidoscope** — a drifting Julia set folded through N mirror
    segments, orbit driven by the mid band.
  - **Plasma Feedback** — video-feedback style trails: each frame samples a
    zoomed/rotated/hue-shifted copy of itself, blooming outward on beats.
  - **IFS Tunnel** — a folded, domain-repeated tunnel built from an iterated
    function system; bass drives forward speed.
  - **Tunnel Spiral** — a cheap, punchy polar spiral tunnel with hard
    audio-reactive color banding.
  - **Apollonian Gasket** — repeated circle-inversion folding, the classic
    "infinite nested circles" fractal.
  - **Raymarch Tunnel 3D** — a genuine sphere-traced 3D scene: the camera
    flies through a twisting, lit tunnel, not a flat 2D effect.
  - **Particle Bloom** — a procedural field of glowing particles orbiting
    and pulsing with the audio, folded through the kaleidoscope.
  - **Oscilloscope Glow** — the actual audio waveform, traced radially
    around the center as a glowing mandala rather than a flat lab scope.
  - **Waveform Scope** — the classic left-to-right lab oscilloscope trace,
    with a soft grid and glowing fill.
  - **Sierpinski Triforce** — the three-triangle Sierpinski gasket via an
    iterated "fold toward nearest corner" contraction, diving continuously
    into the nested triangles with a golden flash on beats.
  - **Fractal Bubbles** — glowing, parallax-depth bubbles positioned by a
    Sierpinski chaos-game point set (the same corner-converging process
    real fractal generators use), so the bubble cloud is itself a fractal
    point distribution — "dots in 3D space" tracing a fractal.
  - **Starfield Warp** — flying through stars at warp speed, radiating
    outward from center; bass and beats slam the warp speed.
- **Real-time audio analysis** (FFT-based): bass/mid/treble band energy,
  overall level, spectral-flux onset ("beat") detection, a 2048-sample
  rolling waveform buffer (for the two oscilloscope-style presets), and
  **auto-gain**: each band/level is also exposed normalized against a
  slowly-decaying tracker of its own recent peak, so reactivity tracks the
  *dynamics* of whatever's playing instead of its absolute loudness —
  quiet material still reads as punchy, loud material doesn't just pin at
  the ceiling.
- **30 automatable parameters** — preset choice, preset morph/crossfade,
  reactivity, per-band gains, zoom/rotation speed, hue/saturation/
  brightness/contrast, kaleidoscope segment count, feedback amount,
  fractal iteration depth, distortion, zoom wander, camera shake, camera
  scale, and eleven **global post-FX** parameters that layer on top of
  *any* preset:
  - **Trails** — phosphor-persistence motion trails
  - **Blur** — screen-space box blur
  - **Noise** — animated film-grain, bursts brighter on beats
  - **Datamosh** — real P-frame-style corruption: blocks smear along a
    per-block "motion vector" that only changes every fraction of a
    second (so it drags instead of flickering), some blocks fully freeze
    onto stale history and visibly melt across following frames, and
    onsets add chromatic-aberration channel splitting for a beat-synced
    glitch burst
  - **Bloom Intensity** — scales the always-on glow (see below)
  - **Vignette** — darkens the frame edges
  - **Chromatic Aberration** — a standalone lens-like RGB split that grows
    toward the edges, independent of Datamosh's own beat-burst split
  - **Color Cycle Speed** — scales the automatic hue-drift rate (see below)
  - **Pulse Depth** — scales the automatic audio-tied brightness pulse
  - **Posterize** — quantizes color into hard bands for a stylized look
  - **Fisheye** — a central lens-warp (power-curve radial distortion)
    applied to every preset's output in the shared post-process pass
- **Camera Scale**: a manual zoom-out/in multiplier applied consistently
  across the spatial presets (Mandelbrot/Burning Ship's view radius, and
  the base scale of Julia, IFS Tunnel, Tunnel Spiral, Apollonian,
  Raymarch Tunnel, Particle Bloom, Sierpinski Triforce, Fractal Bubbles,
  Starfield Warp).
- **Camera Shake is decoupled from Reactivity**: zoom/rotation/tunnel-
  speed motion in the fractal and tunnel presets responds to Camera Shake
  alone, not Reactivity — Reactivity now only scales color/brightness
  response. Coupling actual camera motion to the same knob as color
  reactivity made high-Reactivity settings feel like motion sickness;
  crank Reactivity as high as you like now without the picture lurching.
- **Always-on bloom/glow**: bright edges genuinely bloom, intensity
  breathing with the audio level and punching harder on beats — not a flat
  constant glow.
- **Continuous auto-evolution**: every preset's output has a slow global
  hue crawl (with a little jump on each beat) and a gentle audio-tied
  brightness pulse baked into the shared post-process pass, so the visual
  keeps changing even with every parameter held perfectly still — both are
  deliberately subtle by default (tune via Color Cycle Speed / Pulse
  Depth) so they read as "alive," not as strobing.
- **Manual control panel** in the plugin editor (collapsible) with a slider
  for every parameter, in addition to DAW automation.
- **Fullscreen toggle** (button or `F` key, `Esc` to exit) — best-effort via
  JUCE's native kiosk mode; works reliably in the Standalone app, host-
  dependent in a VST3 DAW window.
- Builds as **VST3** and **Standalone** (JUCE multi-format target).

## Project layout

```
CMakeLists.txt              Build config; fetches JUCE via FetchContent
Source/
  PluginProcessor.*         Audio passthrough + wiring
  PluginEditor.*            GUI: OpenGL visual background + control panel
  AudioAnalyzer.*           FFT band energy + onset detection (audio thread)
  VisualizerParameters.*    APVTS parameter layout (the automation surface)
  Presets/PresetManager.*   Maps preset index -> GLSL source (BinaryData)
  Rendering/VisualizerRenderer.*  OpenGL context, FBOs, per-frame uniforms
  Rendering/FractalNavigator.*    CPU double-precision autopilot for Mandelbrot/Burning Ship zoom
Shaders/
  common.glsl                Shared uniform block + helpers, prepended to
                              every preset's fragment source at compile time
  fullscreen.vert             Fullscreen-triangle vertex shader (shared)
  mandelbrot_pulse.frag
  julia_kaleidoscope.frag
  plasma_feedback.frag
  ifs_tunnel.frag
  tunnel_spiral.frag
  burning_ship.frag
  apollonian.frag
  raymarch_tunnel.frag
  particle_bloom.frag
  oscilloscope.frag
  waveform_scope.frag
  sierpinski_triforce.frag
  fractal_bubbles.frag
  starfield_warp.frag
```

`common.glsl` also carries a small double-float ("double-single") arithmetic
library (`dsAdd`/`dsMul`/`DComplex`/`dcSq`, Dekker/Knuth-style compensated
summation) — Mandelbrot Pulse and Burning Ship iterate in this instead of
plain float32 for their deep zoom; any new preset is free to use it too.

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
  (provides the MSVC toolchain)
- Git
- Internet access on first configure — CMake fetches JUCE 8.0.15 from GitHub
  (a few hundred MB) via `FetchContent`

> Verified: this builds clean with MSVC (Visual Studio Build Tools 2022,
> v14.44 toolset) against JUCE 8.0.15, all 14 presets compile with zero
> GLSL errors (including the double-float Mandelbrot/Burning Ship
> iteration), and the Standalone build launches and renders correctly.
> Given the volume of shader changes in the latest pass, the newest 4
> presets (Waveform Scope, Sierpinski Triforce, Fractal Bubbles, Starfield
> Warp) are confirmed to compile and the app runs stable with them
> selected, but weren't each individually eyeballed for visual polish —
> flag it if one looks off and it'll get a pass.
>
> The CPU-side fractal navigator (`FractalNavigator.h/.cpp`) has been
> watched running live for tens of seconds at a time and reliably lands on
> genuine boundary detail rather than flat regions. It was tuned twice
> based on live feedback: once so it actually finds rich detail instead of
> settling for "good enough," and again so it commits to one point of
> interest (closest viable candidate, hysteresis on retargeting) instead
> of restlessly hopping between several — and `radiusFloor` in
> `FractalNavigator.cpp` was pulled back from the GPU's double-float
> precision wall so the reset happens before the render visibly turns to
> mush, not after. Still a heuristic, not perfectly tuned — if Mandelbrot
> Pulse or Burning Ship ever spend a long stretch looking flat, `idealDE`
> in `update()` controls how close to the boundary it aims to stay.
>
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

`COPY_PLUGIN_AFTER_BUILD` is off, since copying into
`C:\Program Files\Common Files\VST3\` needs admin rights that a normal build
shouldn't require. To make your DAW see the plugin, either:
- Copy the `.vst3` folder into `C:\Program Files\Common Files\VST3\`
  yourself (one-time, needs an elevated copy — e.g. an admin PowerShell
  running `Copy-Item -Recurse`), or
- Point your DAW's plugin scan at
  `build/Kaleidosonic_artefacts/Release/VST3/` directly as a custom VST3
  search path (most DAWs support extra scan folders in preferences) — no
  admin needed, and it's the faster loop while iterating.

For a faster iterate loop, build `Debug` instead and open
`build/Kaleidosonic.sln` in Visual Studio.

## Parameters

| Parameter | Range | What it does |
|---|---|---|
| Preset | choice (14) | Selects the active shader preset |
| Preset Morph | 0–1 | Cross-fades toward the *next* preset in the list |
| Reactivity | 0–2 | Global multiplier on all audio-driven motion |
| Bass / Mid / Treble Gain | 0–2 each | Per-band weighting before it hits the shaders |
| Zoom Speed | -1–1 | Fractal zoom/travel rate |
| Rotation Speed | -1–1 | Rotation rate |
| Hue | 0–1 | Base color hue |
| Saturation | 0–1 | Color saturation |
| Brightness | 0–2 | Output brightness |
| Contrast | 0–2 | Output contrast |
| Kaleidoscope Segments | 1–16 | Radial mirror-fold count |
| Feedback | 0–0.98 | Internal trail persistence (Plasma Feedback preset only) |
| Iterations | 4–64 | Fractal/tunnel detail depth |
| Distortion | 0–1 | Extra coordinate warping |
| Zoom Wander | 0–2 | How far fractal zoom targets wander while diving (Julia/Apollonian/etc; Mandelbrot/Burning Ship use the CPU navigator instead) |
| Camera Shake | 0–2 | How hard bass/onsets drive camera/zoom/tunnel motion — independent of Reactivity |
| Camera Scale | 0.2–6 | Manual zoom-out/in multiplier, applies across the spatial presets |
| Trails | 0–0.97 | Global motion-trail persistence, applies to any preset |
| Blur | 0–1 | Global screen-space blur |
| Noise | 0–1 | Global animated grain, brighter on beats |
| Datamosh | 0–1 | Global glitch/pixel-displacement effect, intensifies on beats |
| Bloom Intensity | 0–2 | Scales the always-on glow |
| Vignette | 0–1 | Darkens the frame edges |
| Chromatic Aberration | 0–1 | Standalone edge-growing RGB split |
| Color Cycle Speed | 0–2 | Scales the automatic hue-drift rate (0 = off) |
| Fisheye | 0–1 | Central lens-warp radial distortion, applies to any preset |
| Pulse Depth | 0–2 | Scales the automatic audio-tied brightness pulse (0 = off) |
| Posterize | 0–1 | Quantizes color into hard bands |

All of the above are `AudioProcessorValueTreeState` parameters, so they show
up in your DAW's automation lane list exactly like any other plugin
parameter.

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
4. Re-run CMake configure (new shader files need to be picked up by the
   `file(GLOB ...)` in `CMakeLists.txt`) and rebuild.

## Known follow-ups

- No preset persistence beyond the host's own automation/session state yet
  (no save/load of custom parameter "snapshots" as named user presets).
- Raymarch Tunnel 3D reads a bit dim at default Brightness with no audio
  driving it — bump Brightness or Reactivity if it looks too dark on your
  monitor.
- Audio-reactivity coefficients (in the shaders and in `AudioAnalyzer`'s
  `squash()`/RMS scaling, on top of the auto-gain normalization) were
  tuned by inspection, not against real program material — if everything
  under-/over-reacts to your mix, Reactivity and the per-band Gain
  parameters are the first place to adjust.
- Mandelbrot Pulse and Burning Ship's zoom target is chosen by the CPU
  navigator (see above), not by Zoom Wander — that parameter only affects
  the other fractal presets (Julia/Apollonian/etc). The navigator can
  still occasionally settle briefly in a less busy stretch of boundary
  before its next search finds something richer; both presets have an
  interior/far-field glow floor so this reads as dim rather than ever
  going solid black. Double-float arithmetic pushes the usable zoom depth
  from plain float32's ~1e6x wall out to roughly 1e12-1e13x before the
  render visibly breaks down — the navigator deliberately loops back
  around 1e11x, well before that wall, so the reset always happens while
  detail is still crisp. This is still a real limit of GPU real-time
  fractal rendering without full arbitrary-precision math, not a bug.
- If you're iterating in a DAW: VST3 hosts keep the plugin's `.dll`
  memory-mapped while it's loaded, so a rebuild can fail with `LNK1104
  cannot open file ...Kaleidosonic.vst3` until you remove/reload the
  device (or restart the host) to release the lock. The Standalone target
  is unaffected and is the faster loop while iterating.
