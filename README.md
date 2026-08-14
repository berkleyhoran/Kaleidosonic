# Kaleidosonic

An audio-reactive, fully DAW-automatable VST3 visualizer plugin. Load it on
any channel and it renders GPU-shader fractals and kaleidoscopic feedback
tunnels that pulse, zoom, and morph with the audio passing through it. Every
visual parameter is exposed as a normal VST3 parameter, so you can draw
automation curves for it in your DAW exactly like you would for a filter
cutoff or a reverb mix.

Audio passes through completely unmodified — this is a pure visualizer.

## Features

- **32 GLSL presets**, switchable and cross-fadable while the DAW automates
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
  - **Image Ripple / Image Shatter / Image Kaleidoscope** — three presets
    built around a picture you load yourself ("Load Image..." in the
    control panel; the file path is remembered across sessions, and
    **animated GIFs play back frame-accurately** -- see below). Ripple
    displaces the image in concentric sine rings like water, bass driving
    a slow swell and treble a fine ripple, with onsets spawning fresh
    expanding rings and a chromatic offset scaled by local ripple
    strength. Shatter breaks the picture into a voronoi field of glass
    shards that spring-kick apart on each onset and settle back (the
    settle is free — `uOnset` is already a decaying envelope, see
    VisualizerRenderer's onsetEnvelope) with crisp crack lines between
    them; Distortion controls shard density. Kaleidoscope folds the image
    through the usual mirror segments and tiles it (adjacent tiles
    mirrored so a non-tileable photo still joins seamlessly) into an
    endless mosaic tunnel with a bounded bass-driven breathing zoom --
    deliberately not an unbounded dive, since a plain texture sample has
    no fractal detail to reveal by going deeper. All three show a gentle
    pulsing placeholder until an image is actually loaded. Four more
    presets (**Particle Bloom**, **Starfield Warp**, **Audio Nebula**,
    **Fractal Bubbles**) also pick up an image when one's loaded --
    coloring their particles/stars/clouds/bubbles from it instead of the
    palette sweep, falling back gracefully wherever it doesn't apply.
  - **Shape Rave** — a raymarched field of rounded boxes, tori, and
    spheres extending infinitely in every direction via domain repetition
    (`mod`-folding the sample point back into one cell, like Mandelbox's
    fold but a single wrap instead of an iterated one) -- no precision
    machinery needed, the whole endless field costs the same as one cell.
    The camera flies continuously through it; every shape bobs on its own
    phase and scale-pulses on the beat. Since the flight path has no
    guarantee it avoids any given shape (placement is randomized per
    cell), the camera pushes itself away from the nearest surface along
    its gradient each frame before marching -- without that, flying
    straight into a shape is a real, if occasional, event, and
    raymarching from inside/right at a surface is what produces
    checkerboard-y noise (the step size collapses toward zero).
  - **Pipes** — a homage to the classic Windows pipes screensaver. Five
    pipes each grow one grid segment at a time, elbowing at right angles
    only, until each fills its joint budget, holds a moment so the
    finished shape actually reads, then clears and restarts with a new
    color. The joint chain is raymarched as a run of capsules; the
    leading segment interpolates smoothly toward its next joint every
    frame (not just once fully grown) so the pipes visibly extrude rather
    than popping into place in discrete unit-length jumps. A glowing
    pulse of energy travels down each pipe from its start joint, onsets
    kicking a fresh one loose. The raymarch skips a pipe's entire segment
    chain with one bounding-sphere check whenever it's nowhere near the
    ray, and stops each pipe's inner loop at however many joints are
    actually real instead of always walking to the per-pipe budget --
    the fix for this preset being reported as laggy, which traced to
    always evaluating up to 27,500 capsule distances per pixel per frame
    regardless of how much was actually visible.
  - **Infinite Maze** — a Wolfenstein-style autonomous walk through a
    procedurally-hashed pillar maze that never repeats and needs no
    storage at all: every cell's wall/open state is a pure function of
    its integer grid coordinate, evaluated identically by the GPU's
    rendering and by a lightweight CPU walker's navigation logic (an
    ordinary 32-bit integer hash, not a float one -- the two sides must
    never disagree about where a wall is, and integer ops are exact and
    portable the way float transcendentals aren't). The walker turns at
    junctions on its own (audio biases how eager it is to turn vs.
    continue straight) and automatically teleports to a fresh area if it
    ever finds itself in an isolated pocket with nowhere new to go.
    Corridors are rendered wide with only thin pillars/wall-slabs (not
    filling their whole grid cell) so the walk reads as roomy rather than
    scraping the walls.
  - **Rotating Light Logo** — the loaded picture (or GIF) redrawn as a
    disc of individually-lit points, like a light-bulb marquee sign,
    spinning in 3D while the camera drifts around it; each point's
    brightness comes from the image at its own grid cell and breathes on
    its own hashed sine-wave phase, so the grid twinkles rather than
    pulsing as one block. Deliberately an analytic ray-*plane*
    intersection rather than a raymarched heightfield -- Shape Rave's
    header comment explains why a discontinuous per-cell field isn't a
    real distance estimator and would risk the same overshoot artifacts
    that took two fixes to chase down there.
  - **Wireframe Tunnel** — a classic demoscene vector-tunnel flythrough:
    neon hoops stacked down the Z axis connected by longitudinal rails,
    both genuinely thin (radius ~0.02) rather than solid walls, so the
    image is built mostly from the glow accumulator's halo around
    near-misses instead of direct hit-shading -- the soft-neon-line look
    a wireframe needs. Both hoops and rails are torus/capsule primitives
    (true distance estimators, no discontinuities).
  - **Metaballs** — a cluster of orbiting spheres fused into one
    wobbling, breathing organic blob via the standard smooth-min CSG
    blend, camera slowly circling the whole cluster. Bass swells each
    ball's radius and the blend softness together so the fuse visibly
    breathes on the beat; picks up a loaded image's colors across the
    fused surface by world position.
  - **Crystal Cave** — a fixed cluster of faceted crystal shards
    (rotated, stretched boxes -- their flat faces and sharp edges give a
    genuinely faceted look for free), camera orbiting the formation. Each
    facet's color shifts by which way its face points, layered over a
    palette base and a strong specular glint, so it reads as cut crystal
    rather than plain colored glass; onset strobes a camera-flash across
    every facet at once.
- **Animated GIF playback**: the same "Load Image..." picker used by the
  image-reactive presets above decodes every frame of a `.gif` (via a
  vendored [stb_image](https://github.com/nothings/stb) decoder --
  JUCE's own image loader only ever gives you a GIF's first frame) and
  plays them back at their real per-frame timing, looping automatically.
  Every image-reactive preset (and the duotone-adjacent presets that
  merely *sample* an image) animates for free -- they just keep reading
  the same `uUserImage` texture as always; only the CPU-side frame
  advance is new. See
  [Source/Rendering/GifDecoder.h](Source/Rendering/GifDecoder.h).
- **Real-time audio analysis** (FFT-based): bass/mid/treble band energy,
  overall level, spectral-flux onset ("beat") detection, a 2048-sample
  rolling waveform buffer, and **auto-gain** normalization so reactivity
  tracks the *dynamics* of whatever's playing instead of absolute loudness.
- **46 automatable parameters**, including a **Palette** parameter (0–8)
  that sweeps/crossfades through curated cosine-gradient palettes
  (Spectrum, Fire, Ice, Synthwave, Sunset, Forest, Mono, Psychedelic) on
  top of the Hue knob, plus eighteen global post-FX (Trails, Blur, Noise,
  Datamosh, Bloom, Vignette, Chromatic Aberration, Color Cycle Speed,
  Pulse Depth, Posterize, Fisheye, Trail Direction, Flame, Shine, Gummy,
  Jpegify, Dot Matrix, Color Override). Flame streams bright content
  along an adjustable direction (0° = up) with turbulence and warm ember
  decay, so it reads as rising fire (or drips/streaks in any direction
  you dial in); Shine grows anisotropic star-streak specular glints out
  of hot spots with a glossy response curve; Gummy adds a soft
  audio-breathing screen-space wobble plus a milky response lift, so the
  whole image reads as translucent, lit jelly; Jpegify fakes real JPEG
  compression damage — blocky quantization, chroma-subsampling color
  bleed, and edge ringing, not a literal DCT, just the three things that
  actually read as "that's compressed"; Dot Matrix redraws the image as
  an audio-reactive halftone/particle grid, each dot sized by local
  brightness and gently twinkling; Color Override remaps the whole image
  onto a luminance gradient between two picked colors (a duotone effect)
  via the Primary/Secondary Color swatches in the panel. All six are
  0/off by default and layer on top of *any* preset.
- **Two-layer compositing**: Layer A (the main Preset dropdown) and an
  independently-chosen **Layer B** blend together via **Layer Mix**
  (0 = just Layer A) through a **Blend Mode** — Crossfade, Add, Screen,
  Multiply, Difference, or Lighten — so any two presets can be layered
  on top of each other, not just dissolved between.
- **Manual control panel** (collapsible, scrollable, opaque backdrop so
  labels stay readable over any visual) with a slider for every parameter,
  organized into collapsible sections (Audio Reactivity, Motion & Zoom,
  Fractal Detail, Color, three Post FX groups) — click a section header to
  fold it away. Whichever sliders the *current* preset's shader doesn't
  actually read are dimmed (grounded in the real per-preset uniform usage,
  see `isParamRelevantForPreset` in
  [VisualizerParameters.cpp](Source/VisualizerParameters.cpp)) so it's
  obvious at a glance what's worth touching — dimmed controls stay fully
  functional, since switching presets or the Layer Mix can make them
  matter again. Sliders deliberately ignore the mouse wheel so
  wheel-scrolling the panel never yanks values.
- **Explorer navigation**: on the explorer presets, mouse wheel / Up-Down
  arrows zoom and dragging the visual pans; over the panel the wheel just
  scrolls the panel. `[`/`]` step through presets from anywhere.
- **Fullscreen toggle** (button or `F` key, `Esc` to exit) hides the side
  panel; **`H`** additionally hides the top-left Controls/Fullscreen
  buttons themselves for a completely chrome-free view (press `H` again
  to bring them back — keyboard-only, since a button can't un-hide
  itself).
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
  Rendering/VisualizerRenderer.*  OpenGL context, FBOs, per-frame uniforms, fractal slots,
                                   and the user-image GL texture (Load Image...) -- decoded
                                   on the message thread, uploaded lazily on the GL thread
  Rendering/FractalNavigator.*    CPU boundary-bisection autopilot + manual explorer navigation
  Rendering/DoubleDouble.h        Double-double (~32 digit) CPU arithmetic for reference orbits
  Rendering/PipeNetwork.*         Pipes preset: grows/animates the joint chains, no GPU precision needed
  Rendering/MazeWalker.*          Infinite Maze preset: autonomous navigation through the hashed maze
  Rendering/GifDecoder.*          Thin wrapper around vendored stb_image for animated-GIF frame decode
ThirdParty/
  stb_image.h                 Vendored (public domain), GIF-decode-only build -- see GifDecoder.cpp
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
  image_ripple.frag           image_shatter.frag       image_kaleidoscope.frag
  shape_rave.frag              pipes.frag                infinite_maze.frag
  light_logo.frag               wireframe_tunnel.frag     metaballs.frag
  crystal_cave.frag
```

The renderer pipeline is two stages: Layer A (and Layer B too, whenever
Layer Mix is above zero, composited via the selected Blend Mode) renders
into an offscreen "raw" buffer, then a global post-process pass blends
that against last frame's fully-processed output using
Trails/Blur/Noise/Datamosh (plus, if turned up, the Color Override
duotone remap) before blitting to screen — see the comment at the top of
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

**macOS / Linux:** the CMake build itself is cross-platform (same
`FetchContent`-based JUCE setup); macOS additionally builds an **AU**
target (`Kaleidosonic_AU`) alongside VST3/Standalone, since most Mac DAWs
(Logic Pro in particular) expect it. Linux needs the usual JUCE system
dependencies (ALSA, X11, FreeType, etc.) installed first -- see the
`apt-get install` list in
[.github/workflows/release.yml](.github/workflows/release.yml) for the
exact package set used in CI, which is the most current reference for
what's actually needed.

```bash
cmake -B build -G Xcode                 # macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release  # Linux
cmake --build build --config Release --target Kaleidosonic_VST3
cmake --build build --config Release --target Kaleidosonic_Standalone
```

**Cross-platform CI + releases:** pushing a version tag (`v0.2.0`, etc.)
triggers [.github/workflows/release.yml](.github/workflows/release.yml),
which builds all three platforms and publishes a GitHub Release with
every installer attached, named:

```
Kaleidosonic-<version>-windows.exe
Kaleidosonic-<version>-macos.zip
Kaleidosonic-<version>-linux-x64.tar.gz
```

(That naming is deliberately stable -- anything reading the GitHub
Releases API to build a downloads page can rely on it.) The version
number flows from the tag into both the plugin binary and the installer
via `-DKALEIDOSONIC_VERSION=...` / Inno Setup's `/DAppVersion=...`
overrides -- no version numbers need editing by hand before tagging a
release. "Run workflow" from the Actions tab (workflow_dispatch) builds
all three platforms the same way without publishing anything, for
testing the pipeline itself. macOS/Windows builds are currently
**unsigned** (no Apple notarization or Windows code-signing cert yet),
so Gatekeeper/SmartScreen will show an "unknown developer" warning on
first launch -- expected for now, not a broken build.

## Parameters

| Parameter | Range | What it does |
|---|---|---|
| Preset | choice (32) | Selects Layer A, the main active shader preset |
| Layer B | choice (32) | The second, independently-chosen preset Layer Mix blends in |
| Blend Mode | choice (6) | How Layer B combines with Layer A: Crossfade, Add, Screen, Multiply, Difference, Lighten |
| Layer Mix | 0–1 | How much of Layer B (combined via Blend Mode) shows over Layer A |
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
| Jpegify | 0–1 | Fake JPEG damage: blocky quantization, chroma-subsampling color bleed, edge ringing |
| Dot Matrix | 0–1 | Redraws the image as an audio-reactive halftone/particle dot grid |
| Color Override | 0–1 | Duotone remap toward the Primary/Secondary Color swatches (0 = original colors) |

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
5. Add an entry to the relevance table in `conditionalRelevance()`
   ([Source/VisualizerParameters.cpp](Source/VisualizerParameters.cpp)),
   listing which of the conditionally-relevant parameters your preset's
   shader (and any common.glsl helpers it calls) actually reads — this is
   what the editor dims when it doesn't apply. Missing an entry fails open
   (nothing gets dimmed for that preset) rather than dimming incorrectly.
6. Re-run CMake configure (new shader files need to be picked up by the
   `file(GLOB ...)` in `CMakeLists.txt`) and rebuild.

## Known follow-ups

- Parameter values persist via the host's own automation/session state, as
  normal for a VST3; the picture loaded via "Load Image..." additionally
  persists its file path as a plain property on the same saved state (see
  `PluginProcessor::getImagePath`/`setImagePath`), so it reloads with the
  session -- but only the path, not the image data itself, so it won't
  travel with the project if you move it to a machine without that file.
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
