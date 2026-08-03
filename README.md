# Kaleidosonic

An audio-reactive, fully DAW-automatable VST3 visualizer plugin. Load it on
any channel and it renders GPU-shader fractals and kaleidoscopic feedback
tunnels that pulse, zoom, and morph with the audio passing through it. Every
visual parameter is exposed as a normal VST3 parameter, so you can draw
automation curves for it in your DAW exactly like you would for a filter
cutoff or a reverb mix.

Audio passes through completely unmodified — this is a pure visualizer.

## Features

- **5 GLSL fractal/trippy presets**, switchable and cross-fadable while the
  DAW automates them:
  - **Mandelbrot Pulse** — classic escape-time Mandelbrot, zoom pulses with
    the bass, hue races over time.
  - **Julia Kaleidoscope** — a drifting Julia set folded through N mirror
    segments, orbit driven by the mid band.
  - **Plasma Feedback** — video-feedback style trails: each frame samples a
    zoomed/rotated/hue-shifted copy of itself, blooming outward on beats.
  - **IFS Tunnel** — a folded, domain-repeated tunnel built from an iterated
    function system; bass drives forward speed.
  - **Tunnel Spiral** — a cheap, punchy polar spiral tunnel with hard
    audio-reactive color banding.
- **Real-time audio analysis** (FFT-based): bass/mid/treble band energy,
  overall level, and spectral-flux onset ("beat") detection, all smoothed
  and fed into every shader as uniforms.
- **16 automatable parameters** — preset choice, preset morph/crossfade,
  reactivity, per-band gains, zoom/rotation speed, hue/saturation/
  brightness/contrast, kaleidoscope segment count, feedback amount,
  fractal iteration depth, and distortion.
- **Manual control panel** in the plugin editor (collapsible) with a slider
  for every parameter, in addition to DAW automation.
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
Shaders/
  common.glsl                Shared uniform block + helpers, prepended to
                              every preset's fragment source at compile time
  fullscreen.vert             Fullscreen-triangle vertex shader (shared)
  mandelbrot_pulse.frag
  julia_kaleidoscope.frag
  plasma_feedback.frag
  ifs_tunnel.frag
  tunnel_spiral.frag
```

## Building

**Prerequisites (Windows):**
- [CMake](https://cmake.org/download/) 3.22+
- Visual Studio 2022 with the **"Desktop development with C++"** workload
  (provides the MSVC toolchain)
- Git
- Internet access on first configure — CMake fetches JUCE 8.0.15 from GitHub
  (a few hundred MB) via `FetchContent`

> This repo was scaffolded on a machine with neither CMake nor a Visual
> Studio C++ toolchain installed, so the build below has **not** been
> compiled/verified yet. The code follows current JUCE 8 CMake API patterns
> throughout, but treat the first build as the real test — check back here
> if something doesn't line up.

**Configure & build (Release):**

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

`COPY_PLUGIN_AFTER_BUILD` is enabled, so the VST3 is copied automatically to
your system's VST3 folder (typically
`C:\Program Files\Common Files\VST3\Kaleidosonic.vst3`) after a successful
build — rescan plugins in your DAW and it should show up. The standalone
app is built to
`build/Kaleidosonic_artefacts/Release/Standalone/Kaleidosonic.exe`.

For a faster iterate loop, build `Debug` instead and open
`build/Kaleidosonic.sln` in Visual Studio.

## Parameters

| Parameter | Range | What it does |
|---|---|---|
| Preset | choice (5) | Selects the active shader preset |
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
| Feedback | 0–0.98 | Trail persistence (Plasma Feedback preset) |
| Iterations | 4–64 | Fractal/tunnel detail depth |
| Distortion | 0–1 | Extra coordinate warping |

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

- First build is unverified locally — see the note above. If MSVC reports
  a shader-adjacent API mismatch, it's most likely a JUCE 8 OpenGL API
  signature to double check against the exact fetched JUCE tag.
- No preset persistence beyond the host's own automation/session state yet
  (no save/load of custom parameter "snapshots" as named user presets).
