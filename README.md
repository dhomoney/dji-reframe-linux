# DJI Reframe for DaVinci Resolve — Linux port

DJI ships its **Reframe** OFX plugin for Windows and macOS only. This project
reconstructs it for Linux from the shipped Windows binary, so 360 / ultra-wide
DJI footage can be reframed in DaVinci Resolve on Linux.

**Status:** working. CPU rendering, all five projections, both render paths, all
18 geometry checks passing. Prebuilt bundles are portable across distros
(glibc 2.17 baseline, static C++ runtime).

- [1. Requirements](#1-requirements)
- [2. Build](#2-build)
- [3. Install](#3-install)
- [4. Using the plugin](#4-using-the-plugin)
- [5. How it was reverse engineered](#5-how-it-was-reverse-engineered)
- [6. What the binary contained](#6-what-the-binary-contained)
- [7. The recovered math](#7-the-recovered-math)
- [8. The port](#8-the-port)
- [9. Testing and verification](#9-testing-and-verification)
- [10. Differences from the Windows plugin](#10-differences-from-the-windows-plugin)
- [11. Troubleshooting](#11-troubleshooting)
- [12. Reproducing the analysis](#12-reproducing-the-analysis)
- [13. Legal](#13-legal)
- [14. Licence](#14-licence)

---

## 1. Requirements

| | |
|---|---|
| **DaVinci Resolve Studio** | Required. The free edition does not load third-party OFX plugins. Developed against Studio 21.1. |
| C++17 compiler | GCC 10+ or Clang 15+ (developed with GCC 16.2.1, release builds use GCC 10.2.1) |
| CMake | 3.16+ |
| OpenFX SDK | Git submodule at `vendor/openfx` (BSD-3-Clause), API v1.5.1 |
| Docker | Only for building the portable release artifact ([§2.3](#23-portable-build)) |

No GPU runtime is required — rendering is CPU-side. See
[§10](#10-differences-from-the-windows-plugin) for what that costs you.

If you just want the plugin and not the source, grab the prebuilt bundle from
the [latest release](https://github.com/dhomoney/dji-reframe-linux/releases/latest)
and skip to [§3](#3-install). It is built against glibc 2.17 with a static C++
runtime, so it loads on any distro Resolve itself supports.

## 2. Build

### 2.1 Clone

The OpenFX SDK is a submodule, so clone recursively:

```sh
git clone --recurse-submodules https://github.com/dhomoney/dji-reframe-linux.git
cd dji-reframe-linux
```

Already cloned without it?

```sh
git submodule update --init --depth 1
```

### 2.2 Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Produces `build/DJIReframe.ofx.bundle/`, laid out the way OFX expects:

```
DJIReframe.ofx.bundle/
  Contents/
    Info.plist
    Linux-x86-64/
      DJIReframe.ofx          # the plugin (an .so with OFX entry points)
```

This bundle is fine for your own machine. It is **not** portable to other
distros — see below.

### 2.3 Portable build

A plugin is `dlopen`'d straight into Resolve's process, which makes it fussier
than an ordinary binary. Two things break portability:

| Problem | Why it bites | Fix |
|---|---|---|
| **glibc version** | Building on a rolling-release distro records symbol versions like `GLIBC_2.43`. glibc is backward compatible but not forward compatible, so an older host cannot load the plugin at all. | Build against an old glibc — the release uses the `manylinux2014` image (glibc 2.17). |
| **libstdc++** | Resolve ships its own C++ runtime. A plugin that links `libstdc++` dynamically and exports 1000+ C++ symbols can bind against the wrong one. | `-static-libstdc++ -static-libgcc`, hidden visibility, and a linker version script exporting only the two OFX entry points. |

The second fix is the `OFX_PORTABLE` CMake option, **on by default**:

```sh
cmake -S . -B build -DOFX_PORTABLE=OFF    # if you want the small, local-only build
```

| | `OFX_PORTABLE=ON` | `OFF` |
|---|---|---|
| Exported symbols | 2 | 1072 |
| Links `libstdc++` | no (static) | yes |
| Size | ~1.9 MB | ~530 KB |

For the full portable artifact, including the old-glibc container build:

```sh
tools/build-portable.sh
```

That builds in Docker, runs the geometry checks with the same toolchain, then
verifies the result before packaging it:

```
    glibc required:   2.15 (baseline 2.17) OK
    libstdc++:        static OK
    shared deps:      libpthread.so.0 libm.so.6 libc.so.6 ld-linux-x86-64.so.2
    exported symbols: OfxGetNumberOfPlugins OfxGetPlugin  OK
    bundle layout:    OK
```

Output lands in `dist/`, as both an unpacked bundle and a tarball. To re-check
an existing build without rebuilding: `tools/build-portable.sh --verify`.

## 3. Install

### From a release tarball

```sh
tar xzf DJIReframe-linux-x86_64.ofx.bundle.tar.gz
mkdir -p ~/OFX/Plugins
cp -r DJIReframe.ofx.bundle ~/OFX/Plugins/
```

### From source

System-wide (Resolve scans `/usr/OFX/Plugins` — confirmed by strings in
`/opt/resolve/bin/resolve`):

```sh
sudo cmake --install build
```

Or without root, using the `OFX_PLUGIN_PATH` variable Resolve also honours:

```sh
mkdir -p ~/OFX/Plugins
cp -r build/DJIReframe.ofx.bundle ~/OFX/Plugins/
OFX_PLUGIN_PATH=~/OFX/Plugins /opt/resolve/bin/resolve
```

Restart Resolve. The effect appears in the **OpenFX** library under the **DJI**
group as **DJI Reframe**.

## 4. Using the plugin

Drop it on an equirectangular clip in the Color or Edit page.

### Parameters

Internal names match the Windows plugin exactly, so `.drp` projects that
reference the original remain compatible.

| Control | OFX name | Range | Default |
|---|---|---|---|
| Resolution | `resolution` | 10 sizes, `Source Image`, `Resolution Custom` | `1080p 16:9(1920*1080)` |
| Custom Width / Height | `custom_width` / `custom_height` | 0 … 16384 | 1920 / 1080 |
| Projection | `camera_projection_type` | Wide, Crystal Ball, Tiny Planet, Ultra Wide, Dewarp | Wide |
| Pan | `camera_pan` | −360 … 360° | 0 |
| Tilt | `camera_tilt` | −360 … 360° | 0 |
| Roll | `camera_rotate` | −360 … 360° | 0 |
| FOV | `camera_fov_control` | 1 … 178° | 60 |
| Distortion | `camera_distortion_control` | 0 … 1.8 | 0.6 |
| Reset | `reset_button` | — | — |
| Source File Yaw / Pitch / Roll | `source_file_yaw` / `_pitch` / `_roll` | −360 … 360° | 0 |
| Border Mode | `border_mode` | Transparent, Color, Mirror / Extend | Transparent |
| Border Color | `border_color` | RGB | black |

Ranges shown are the *display* ranges. As in the original, the hard range on
every double is ±FLT_MAX, so you can type values outside the slider.

### Things worth knowing

**Distortion is the real control.** It sets the camera eye's distance from the
centre of the unit sphere. Near 0 you get a rectilinear view; at 1.0 the eye sits
on the sphere surface; past 1.0 it leaves the sphere and you see it from outside
as a ball. It is the single scalar that separates the five projections.

**Projection is only a preset button.** Selecting one writes a
(tilt, FOV, distortion) triple into the three controls and nothing reads it
afterwards — the render path never looks at it. The values depend on the chosen
*resolution* as well, because portrait output needs a wider vertical field:

| Projection | tilt | distortion | FOV landscape & square | FOV 9:16 | FOV 3:4 |
|---|---|---|---|---|---|
| Wide | 0 | 0.6 | 60 | 90 | 72 |
| Crystal Ball | 0 | 1.8 | 75 | 110 | 87 |
| Tiny Planet | −90 | 1.0 | 138 | 147 | 147 |
| Ultra Wide | 0 | 0.5 | 78 | 110 | 95 |
| Dewarp | 0 | 0.2 | 80 | 112 | 97 |

All 300 recovered values are in [`notes/presets.csv`](notes/presets.csv).

**Resolution → `Source Image` switches render paths.** It keeps the
equirectangular format and applies only the Source File orientation correction.
Pan/Tilt/Roll/FOV/Distortion are ignored and grey out. Use it to re-level a
360 clip while keeping it spherical.

**Input must be equirectangular.** A 2:1 source is treated as a full sphere.
Anything else — ordinary 16:9 DJI footage — is treated as the vertical middle
band of a partial sphere, which is what makes real clips map correctly.

---

## 5. How it was reverse engineered

### Tools

| Tool | Use |
|---|---|
| `7z` | Unpack the NSIS installer |
| `objdump` | Disassembly, section layout, string cross-referencing |
| [REA](https://github.com/morluto/rea) 6.4.0 + Ghidra 12.1.2 | Decompilation |
| Python | Constant-pool decoding, preset-table extraction, symbolic argument tracking |

On Arch/Manjaro the decompiler needs:

```sh
sudo pacman -S --needed ghidra jdk-openjdk
```

and REA needs `GHIDRA_INSTALL_DIR` in its environment. For the Claude Code MCP
server that means adding it to the server entry in `~/.claude.json`:

```json
"rea": {
  "command": "npx",
  "args": ["-y", "rea-agents@6.4.0", "mcp"],
  "env": { "GHIDRA_INSTALL_DIR": "/opt/ghidra" }
}
```

The server reads this at startup, so restart the session after changing it. To
work without restarting, drive the CLI directly:

```sh
GHIDRA_INSTALL_DIR=/opt/ghidra npx -y rea-agents@6.4.0 \
    decompile ./DJIReframe.dll 0x180009080 --snapshot ./snap.rea --format md
```

`--snapshot` caches Ghidra's analysis; the first call takes minutes, later ones
are fast.

### Method

1. **Unpack.** `7z x` on the NSIS installer yields the OFX bundle.
2. **Identify.** RTTI strings (`.?AVImageEffect@OFX@@`, `PluginFactoryHelper`)
   show it is built on the public OFX C++ Support library.
3. **Harvest strings.** `strings -t x` found the embedded OpenCL kernel source
   and the whole parameter vocabulary.
4. **Cross-reference.** A Python pass over `objdump -d` output resolved every
   RIP-relative `lea` to its target string, producing a map of 509 strings to the
   code that references them. That located `describeInContext`, the constructor
   and `changedParam` without a decompiler.
5. **Track arguments symbolically.** A small interpreter followed `movss`/`movsd`
   constant loads into XMM registers and their stores into stack argument slots,
   recovering each parameter's exact `(default, min, max)` triple from the
   call sites.
6. **Decompile the geometry.** Ghidra via REA for `Reframe::render`
   (`0x180009080`), `changedParam` (`0x1800054f0`) and the resolution helper
   (`0x180008760`).
7. **Decode data.** The preset table was read straight out of `.rdata` once the
   indexing arithmetic was understood from the addressing code.
8. **Verify.** Reimplement, then check invariants numerically *and* render test
   frames and look at them. See [§9](#9-testing-and-verification) — this step
   caught two real bugs the numeric checks had passed.

---

## 6. What the binary contained

| | |
|---|---|
| Installer | `DJI_Reframe_for_DaVinci_Resolve.exe`, NSIS-3, 186 KB |
| SHA-256 | `3e50e82c0a0bbcf801da8b636f3ea598ff6bbcc7f732bbd3efad197b83842474` |
| Payload | `DJIReframe.ofx.bundle/Contents/Win64/DJIReframe.ofx` |
| Payload type | PE32+ x86-64 DLL, 414 KB |
| Payload SHA-256 | `a770ee64806d42c5152547431ee6369ce6eb2fc7bd6d786c8dd1b2d40eff546f` |
| Plugin ID | `com.pano360.reframe`, "DJI Reframe v1.0.0" |

### Why the port was tractable

**It is an OpenFX plugin.** OFX is an open, documented, cross-platform standard,
so the entire host interface — parameter definition, clip fetching, render
dispatch — did not need reverse engineering at all. Only the image-processing
behaviour did. The original links the same public Support library this port
builds against.

**The OpenCL kernels are embedded as plaintext.** 228 lines of readable C,
sitting in `.rdata` as a string to hand to `clBuildProgram`. That is the reframe
math in source form, recovered verbatim to
[`notes/recovered_PanoKernels.cl`](notes/recovered_PanoKernels.cl).

**There is also a CUDA fatbinary** (`.nv_fatb`, 123 KB). Its mangled symbols —
`PanoSphereKernel(float const*, float*, SphereParams)` and
`PanoCylinderKernel(float const*, float*, CylinderParams)` — confirm the
parameter-struct layouts that the OpenCL variant passes as flattened scalars.

**The host side is glm.** `perspective`, `frustum`, `lookAt`, `rotate`,
`inverse`, all recognisable in the decompiler output, which made the camera
construction unambiguous.

### Instance layout

Member offsets into the `Reframe` instance, recovered from the constructor
(`0x180003200`–`0x180003d80`) and confirmed against `render`:

| Offset | Parameter | Offset | Parameter |
|---|---|---|---|
| `0x88` | `resolution` | `0xd0` | `camera_distortion_control` |
| `0x90` | `custom_width` | `0xd8` | `reset_button` |
| `0x98` | `custom_height` | `0xe0` | `source_file_yaw` |
| `0xa0` | *cached resolution int* | `0xe8` | `source_file_pitch` |
| `0xa8` | `camera_projection_type` | `0xf0` | `source_file_roll` |
| `0xb0` | `camera_pan` | `0xf8` | `border_mode` |
| `0xb8` | `camera_tilt` | `0x100` | `border_color` |
| `0xc0` | `camera_rotate` | | |
| `0xc8` | `camera_fov_control` | | |

Parameter groups: `Group_Resolution`, `Group_Camera`, `Group_SourceFile`,
`Group_Advanced`, `Group_Background`, `Group_OverlayColor`.

There is also a `version_button` that opens `https://www.dji.com/hk` through
`ShellExecuteA`. Not ported.

### Output resolution table (`0x180008760`)

The dropdown is **0-based** and the selector passed to this function is
**index + 1**, so case 0 is unreachable. Labels in `appendOption` order:

| Index | Case | Label | Size |
|---|---|---|---|
| 0 | 1 | `1080p 16:9(1920*1080)` | 1920×1080 |
| 1 | 2 | `1080p 9:16(1080*1920)` | 1080×1920 |
| 2 | 3 | `1080p 1:1 (1080*1080)` | 1080×1080 |
| 3 | 4 | `1080p 2.35:1(2538*1080)` | 2538×1080 |
| 4 | 5 | `1080p 4:3 (1440*1080)` | 1440×1080 |
| 5 | 6 | `1080p 3:4(1080*1440)` | 1080×1440 |
| 6 | 7 | `4k 16:9(3840*2160)` | 3840×2160 |
| 7 | 8 | `4k 9:16(2160*3840)` | 2160×3840 |
| 8 | 9 | `4k 1:1(2160*2160)` | 2160×2160 |
| 9 | 10 | `4k 2.35:1(3840*1634)` | 3840×1634 |
| 10 | 11 | `Source Image` | source W × H |
| 11 | 12 | `Resolution Custom` | custom W × H |

### Projection presets

`changedParam` — for **both** `camera_projection_type` and `resolution` — reads
a static table at `0x180022580` and writes into pan/tilt/roll/FOV/distortion:

```c
double presets[5 /*projection*/][12 /*resolution*/][5 /*values*/];
index  = 5 * (resolutionIndex + 12 * projectionIndex);
values = { pan, tilt, roll, fov, distortion };
```

`projection` is clamped to 0..4 and `resolutionIndex` to 0..11, both falling back
to 0. Pan and roll are 0 in all 300 entries.

The dropdown order is the `appendOption` call order — **not alphabetical**:
Wide, Crystal Ball, Tiny Planet, Ultra Wide, Dewarp.

### Border modes

| Value | Mode | Behaviour |
|---|---|---|
| 0 | Transparent | Outside-viewport and ray-miss pixels get RGBA 0 |
| 1 | Color | Those pixels get `border_color` with alpha 1; enables the swatch |
| 2 | Mirror / Extend | Viewport becomes the whole render window and the projection switches from `perspective` to an off-centre `frustum`, expanding the image to fill the frame instead of letterboxing |

---

## 7. The recovered math

Constants, all verified in `.rdata`: `deg2rad = 0.017453292`, `pi = 3.1415927`,
`zNear = 0.0005`, `zFar = 10.0`.

### Equirectangular sampling window

```
vScale  = 0.5 * srcW / max(srcH, 1.0)
vOffset = 0.5 * (1.0 - vScale)
v       = vOffset + v * vScale          // applied to every sample
```

A full 2:1 equirect gives `vScale = 1, vOffset = 0`. A 16:9 source gives
`vScale = 8/9, vOffset = 1/18` — the frame is the vertical middle of a partial
sphere. This is the detail that makes ordinary DJI footage map correctly.

### Viewport

The output rect is centred in the render window:

```
vpX = (renderW - outW) / 2      vpW = outW
vpY = (renderH - outH) / 2      vpH = outH
```

Border mode 2 overrides this to `(0, 0, renderW, renderH)`.

### Perspective path (resolution ≠ `Source Image`) → `PanoSphereKernel`

```
proj  = perspective(fov * deg2rad, outW / max(outH, 1.0), 0.0005, 10.0)
view  = lookAt(eye    = (0, 0, distortion),
               center = (0, 0, -1),
               up     = (0, -1, 0))

model = identity
model = rotate(model,  roll     * deg2rad, (0,0,1))
model = rotate(model, -tilt     * deg2rad, (1,0,0))   // note the negation
model = rotate(model,  pan      * deg2rad, (0,1,0))
model = rotate(model,  srcRoll  * deg2rad, (0,0,1))
model = rotate(model,  srcPitch * deg2rad, (1,0,0))
model = rotate(model,  srcYaw   * deg2rad, (0,1,0))

invMVP = inverse(proj * view * model)
```

Per pixel the kernel unprojects NDC (with Y flipped) at z = −1 and z = +1 through
`invMVP`, normalises the ray, intersects the unit sphere taking the near root and
falling back to the far root, then converts the hit point to equirect UV:

```
theta = acos(clamp(hit.y, -1, 1))
phi   = atan2(hit.x, hit.z);  if (phi < 0) phi += 2*pi
u     = phi / (2*pi)
v     = 1 - theta / pi
```

For border mode 2, `perspective` is replaced by an off-centre frustum:

```
halfH = zNear * tan(fov * deg2rad / 2)
halfW = halfH * (outW / outH)
sx    = (renderW/2 - (outW/2 + vpX)) / (outW/2)     // 0 when centred
sy    = (renderH/2 - (outH/2 + vpY)) / (outH/2)
ex    = halfW * (renderW / outW)
ey    = halfH * (renderH / outH)
proj  = frustum(sx*halfW - ex, sx*halfW + ex,
                sy*halfH - ey, sy*halfH + ey, 0.0005, 10.0)
```

### Equirect path (resolution = `Source Image`) → `PanoCylinderKernel`

Pan, tilt, roll, FOV and distortion are **never read** on this path. It is a pure
source-orientation correction that preserves the equirect format, using 3×3
matrices:

```
R = Rz(srcYaw * deg2rad) * Ry(pi - srcPitch * deg2rad) * Rx(srcRoll * deg2rad)
```

Per pixel it builds a z-up direction from the output UV, applies `R`, and
converts back:

```
thetaRaw = v * pi                       // v = 1 - (y - vpY)/vpH
phiRaw   = 2*pi - u * 2*pi
dir      = (sin(thetaRaw)*cos(phiRaw), sin(thetaRaw)*sin(phiRaw), cos(thetaRaw))
rot      = R * dir
theta    = acos(clamp(rot.z, -1, 1))
phi      = wrap(atan2(rot.y, rot.x) + pi, 0, 2*pi)
su       = phi / (2*pi)
sv       = theta / pi
```

The `pi -` on pitch is not a typo — with it, zero angles collapse to an exact
identity, which is how the term was confirmed.

---

## 8. The port

### Source layout

```
src/
  Mat4.h           Column-major matrix math matching glm's conventions
                   (perspective, frustum, lookAt, rotate, inverse) plus the
                   3x3 helpers for the equirect path.
  Presets.h        Generated: the 300-value preset table, projection labels and
                   resolution labels, in the order recovered from the binary.
  ReframeCore.h    Single source of truth for the geometry -- buildGeometry()
                   resolves parameters into a Geometry struct -- plus the CPU
                   sampler and per-pixel renderers.
  DJIReframe.cpp   The OFX plugin: describe, describeInContext, parameter
                   handling, changedParam presets, and the multithreaded
                   ImageProcessor.

tests/
  test_reframe.cpp Standalone harness; no host required.

tools/
  extract_presets.py   Re-extract the preset table from the Windows binary.

notes/
  recovered_PanoKernels.cl  The original's OpenCL source, verbatim, as evidence.
  presets.csv               All 300 preset values.
  annotated_describe.txt     Annotated disassembly of describeInContext.

packaging/
  Info.plist       Bundle metadata.
```

### Design decisions

**Independent implementation, not a transcription.** `src/` is written from the
recovered specification rather than pasting DJI's kernel text. The recovered
source is kept under `notes/` as analysis evidence and is not compiled. The math
involved — equirectangular projection, ray-sphere intersection, bilinear
sampling — is standard.

**One geometry function.** `buildGeometry()` resolves every parameter into a
`Geometry` struct. The CPU renderers consume it, and a future GPU backend would
consume the same struct, so the two can never drift.

**glm conventions reproduced exactly.** `Mat4.h` mirrors glm's right-handed,
depth −1..1 formulations, because the original's output depends on them.
Verified against reference values before anything was built on top.

**Row strides respected.** The sampler takes a stride so OFX images are read in
place. Repacking a 4K equirect would cost 130 MB of copying per frame.

**Parameter names preserved.** Identical OFX names mean existing projects keep
working.

---

## 9. Testing and verification

```sh
g++ -std=c++17 -O2 -o build/test_reframe tests/test_reframe.cpp
./build/test_reframe build/testout
```

The harness renders a synthetic equirectangular grid — coloured quadrants,
15° graticule, distinct pole markers — through every path and checks:

```
  [PASS] 2:1 source -> vScale 1, vOffset 0
  [PASS] 16:9 source -> vScale 8/9, vOffset 1/18
  [PASS] resolution 7 fills a 3840x2160 render window
  [PASS] output rect is centred in a larger render window
  [PASS] resolution 11 -> equirect path
  [PASS] equirect path ignores pan/tilt/FOV/distortion
  [PASS] equirect path at zero angles reproduces the source
  [PASS] pan 0 == pan 360
  [PASS] border mode Color fills outside the output rect
  [PASS] border mode Transparent leaves alpha 0 outside the rect
  [PASS] border mode Mirror/Extend expands the viewport to the whole frame
  [PASS] border mode Mirror/Extend fills the corner
  [PASS] wide / crystalball / tinyplanet / ultrawide / dewarp
  [PASS] the five projection presets all render differently
```

PPM renders land in `build/testout` for visual inspection.

The strongest check is the equirect round-trip: at zero source angles the
rotation chain must collapse to identity and reproduce the source. Interior error
is **0.003**. The only large error is at row 0 — the pole, where longitude is
mathematically undefined — so the assertion ignores a thin margin at each pole
and reports both figures.

Note that "identity" is only exact to within a fraction of a pixel: the forward
map divides by the viewport height while the sampler multiplies by `srcH - 1`.
The original has the same off-by-a-fraction behaviour, so it is reproduced rather
than corrected. It is invisible on real footage but lands hard on 1-pixel test
graticules, which is why that assertion runs against a smooth gradient instead.

### Two bugs that only visual inspection caught

Both passed every numeric check and would have shipped silently:

1. **Projection order.** Assumed alphabetical; the real `appendOption` order is
   Wide, Crystal Ball, Tiny Planet, Ultra Wide, Dewarp. Every preset was
   mislabelled — "Tiny Planet" rendered a flat rectilinear view. Visible
   immediately in a contact sheet.
2. **Resolution indexing.** The dropdown is 0-based with internal value
   `index + 1`; an assumed placeholder at index 0 shifted every size by one. The
   preset table settled it: columns 1, 5 and 7 carry the widened FOVs (90, 72,
   90), and those are exactly the three portrait sizes.

The lesson worth keeping: for geometry work, render the frames and look at them.

---

## 10. Differences from the Windows plugin

**CPU rendering.** The original ships OpenCL and CUDA kernels; this build uses a
multithreaded CPU path through the OFX `ImageProcessor`. It is correct at any
resolution but 4K is not real-time — work at proxy or a lower timeline resolution
while framing, then switch up to render. Adding a GPU backend is a contained
piece of work: the recovered OpenCL source is in
`notes/recovered_PanoKernels.cl`, the kernel argument layout is documented in
[§6](#6-what-the-binary-contained), and `Geometry` already carries everything a
kernel needs.

**No on-screen overlay.** The original has a drag-to-pan viewer widget
(`ReframeOverlay` at `0x18000a9a0`, 0.1°/pixel on pan/tilt/roll/FOV). Not
ported; use the sliders.

**No version button.** It opened `dji.com` in a browser.

**Advanced / overlay-colour controls not implemented.** `Group_Advanced`,
`Group_OverlayColor`, `overlay_color`, `Guide Lines` and `Guide Line Color` exist
as strings in the original but were never traced to any effect on rendered
output.

### Unresolved

- The i18n label table at `FUN_180004800` maps `Param_*` keys to English labels;
  only English strings are present in the binary.
- The constant `0.855` at `0x180023ef8`, used near `0x18000846b`, was not tied to
  any rendered result.

---

## 11. Troubleshooting

**The effect does not appear in Resolve.**
Check you are running Resolve **Studio** — the free edition silently ignores
third-party OFX plugins. Then confirm the bundle is at
`/usr/OFX/Plugins/DJIReframe.ofx.bundle/Contents/Linux-x86-64/DJIReframe.ofx`
and is readable, and restart Resolve.

**Verify the plugin loads at all:**

```sh
nm -D --defined-only build/DJIReframe.ofx.bundle/Contents/Linux-x86-64/DJIReframe.ofx \
    | grep Ofx
# expect OfxGetNumberOfPlugins and OfxGetPlugin
ldd build/DJIReframe.ofx.bundle/Contents/Linux-x86-64/DJIReframe.ofx
# expect no "not found"
```

**Built it myself and Resolve still will not load it.** Most likely a glibc
mismatch: a plugin built on a newer distro than Resolve's own runtime cannot be
loaded. Check what the plugin demands against what you have:

```sh
objdump -T build/DJIReframe.ofx.bundle/Contents/Linux-x86-64/DJIReframe.ofx \
    | grep -oE 'GLIBC_[0-9.]+' | sort -uV | tail -1
ldd --version | head -1
```

If the first number is higher than the second, build the portable artifact with
`tools/build-portable.sh` ([§2.3](#23-portable-build)) or use the release
tarball. Resolve's bundled libraries in `/opt/resolve/libs` can also shadow
system ones; launching Resolve after moving that directory aside is a quick way
to confirm whether a conflict there is the cause.

**Output is black or transparent.** Check Border Mode, and confirm the source is
actually equirectangular. A non-360 clip will map to something unrecognisable
rather than failing outright.

**Playback is slow.** Expected — see [§10](#10-differences-from-the-windows-plugin).
Lower the timeline resolution while framing.

**Resolve logs.** `~/.local/share/DaVinciResolve/logs/` — OFX load failures are
reported there.

## 12. Reproducing the analysis

```sh
# Unpack the installer
7z x -oextracted DJI_Reframe_for_DaVinci_Resolve.exe

# Recover the embedded OpenCL kernels
python3 - <<'EOF'
d = open('extracted/DJIReframe.ofx.bundle/Contents/Win64/DJIReframe.ofx','rb').read()
s = d.find(b'#ifndef M_PI'); e = d.find(b'map/set too long')
open('PanoKernels.cl','wb').write(d[s:e].replace(b'\x00', b''))
EOF

# Re-extract the preset table
tools/extract_presets.py extracted/DJIReframe.ofx.bundle/Contents/Win64/DJIReframe.ofx

# Decompile the render function
GHIDRA_INSTALL_DIR=/opt/ghidra npx -y rea-agents@6.4.0 \
    decompile extracted/DJIReframe.ofx.bundle/Contents/Win64/DJIReframe.ofx \
    0x180009080 --snapshot ./snap.rea --format md
```

Key addresses:

| Address | What |
|---|---|
| `0x180009080` | `Reframe::render` |
| `0x1800054f0` | `changedParam` |
| `0x180008760` | Output resolution helper |
| `0x180004800` | i18n label lookup |
| `0x18000a9a0` | `ReframeOverlay` constructor |
| `0x180022580` | Preset table (300 doubles) |
| `0x180006500`–`0x1800084ff` | `describeInContext` |

## 13. Legal

This port exists for **interoperability**: running software you already have on a
platform its vendor chose not to support. Reverse engineering for that purpose is
explicitly permitted in many jurisdictions — EU Software Directive Art. 6, and
17 U.S.C. §1201(f) in the US.

No DJI code is copied into the build. Everything under `src/` is an independent
implementation written from the recovered specification; the recovered kernel
source under `notes/` is retained as analysis evidence and is never compiled or
distributed as part of the plugin.

The OpenFX SDK in `vendor/openfx` is BSD-3-Clause, © The Open Effects
Association.

Note that `notes/` contains material extracted verbatim from DJI's binary and
is therefore **not** covered by this project's MIT licence. See
[§14](#14-licence) and [LICENSES.md](LICENSES.md) for the breakdown.

"DJI" is a trademark of SZ DJI Technology Co., Ltd. This project is not
affiliated with, authorised by, or endorsed by DJI. You need a lawfully obtained
copy of the original plugin for the preset-extraction tooling to be meaningful.

## 14. Licence

This project's own work — `src/`, `tests/`, `tools/`, `packaging/`, the build
files and this README — is **MIT licensed**. See [LICENSE](LICENSE).

Three different sets of terms apply across the repository, so
[LICENSES.md](LICENSES.md) spells out which is which. In short:

| Path | Terms |
|---|---|
| `src/`, `tests/`, `tools/`, `packaging/`, `CMakeLists.txt`, `README.md` | MIT, © 2026 David Homoney |
| `vendor/openfx` (submodule) | BSD-3-Clause, © The Open Effects Association |
| `notes/` | Recovered from DJI's binary. Not this project's to license — retained as interoperability analysis evidence, never compiled. |

Distributed binaries link the OpenFX support library, so they carry the
BSD-3-Clause notice.
