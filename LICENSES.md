# Licensing

This repository contains material under three different terms. The MIT licence
in [`LICENSE`](LICENSE) does **not** cover all of it.

## MIT — this project's own work

Everything written for this port:

- `src/` — the independent reimplementation
- `tests/`, `tools/`, `packaging/`
- `CMakeLists.txt`, build scripts, `README.md`

Copyright (c) 2026 David Homoney. See [`LICENSE`](LICENSE).

## BSD-3-Clause — the OpenFX SDK

`vendor/openfx` is a submodule pointing at
[AcademySoftwareFoundation/openfx](https://github.com/AcademySoftwareFoundation/openfx),
© The Open Effects Association. See that repository's `LICENSE.md`.

The built plugin links against its support library, so distributed binaries
carry the BSD-3-Clause notice.

## Not licensed by this project — recovered analysis material

`notes/` holds material recovered from DJI's shipped binary and retained as
analysis evidence:

| File | What it is |
|---|---|
| `recovered_PanoKernels.cl` | OpenCL kernel source extracted from DJI's plugin |
| `annotated_describe.txt` | Annotated decompiler output |
| `presets.csv` | The preset table, re-extracted as data |

This material is **not** covered by the MIT licence and is not this project's
to relicense. It remains the copyright of SZ DJI Technology Co., Ltd. It is
included for the interoperability analysis described in README §5 and §13, is
never compiled, and is not part of any distributed binary.

"DJI" is a trademark of SZ DJI Technology Co., Ltd. This project is not
affiliated with, authorised by, or endorsed by DJI.
