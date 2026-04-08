FxPatchSDK Fork for Polyend Endless
===================================

This repository is a development-focused fork of the official
[`polyend/FxPatchSDK`](https://github.com/polyend/FxPatchSDK) for the
[Polyend Endless](https://polyend.com/endless/) multi-effects pedal. The official
SDK is intentionally small: one `Patch` base class, a thin C ABI, and a build
pipeline that emits `.endl` binaries for drag-and-drop deployment. This fork keeps
that stock SDK shape, then layers on custom effects, validation scripts, deeper
notes, and a working knowledge base for future Endless development.

Polyend positions Endless as both a pedal and a platform: you can build effects by
writing C++ against the SDK, or use the hosted
[Playground](https://polyend.com/playground/) text-to-effect workflow to generate
compiled patches. This fork is for the first path: controlled, inspectable, versioned
development.

## Current Repository Status

- `master` is the current baseline for future work in this fork.
- The local `source/` tree still matches the stock SDK layout.
- This fork adds hand-written effects in `effects/`, validation in `tests/`,
  design/reference notes in `docs/`, and compiled Playground artifacts in `playground/`.

For the audit that produced this rewrite, see
[`docs/repository-review.md`](docs/repository-review.md).

## Repository Map

```text
source/        public Patch API and the default upstream bitcrusher example
internal/      C ABI wrapper, image header, linker script, patch entrypoint
effects/       custom stock-SDK-compatible patches and reference examples
tests/         host-side syntax and lint validation
docs/          SDK notes, patch design walkthroughs, branch/repo review
playground/    compiled Playground examples and supporting artifacts
```

## Included Effect Work

This fork currently includes three stock-SDK-compatible custom effects:

- `effects/chorus.cpp`: stereo modulated-delay chorus
- `effects/mxr_distortion_plus.cpp`: circuit-informed MXR Distortion+ model
- `effects/wah.cpp`: dual-mode Crybaby/Vox-inspired wah using expression control

There is also one external reference example in `effects/examples/reverb.cpp` that
compiles against the stock SDK.

## Build and Deploy

Install the GNU Arm Embedded toolchain for your host, then build from the repo root:

```bash
make TOOLCHAIN=/usr/bin/arm-none-eabi-
```

To build a named output:

```bash
make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=my_effect
```

The build emits `build/<PATCH_NAME>_<timestamp>.endl`. Deploy by connecting the
Endless over USB-C and copying the `.endl` file onto the mounted Endless drive.

The default source file at [`source/PatchImpl.cpp`](source/PatchImpl.cpp) is still the
simple upstream bitcrusher example. To build one of the custom effects in `effects/`,
copy it into `source/PatchImpl.cpp`, change the include from
`#include "../source/Patch.h"` to `#include "Patch.h"`, then run `make`.

## Validation

Run the host-side syntax and lint checks before hardware testing:

```bash
bash tests/check_patches.sh
```

This confirms that each `effects/*.cpp` file parses cleanly with SDK-like compile
flags and checks for common mistakes such as heap use, bare double literals, and
missing `getInstance()` definitions.

## Recommended Reading Order

If you are preparing to add or change code in this fork, start here:

1. [`docs/repository-review.md`](docs/repository-review.md)
2. [`docs/endless-reference.md`](docs/endless-reference.md)
3. [`effects/README.md`](effects/README.md)
4. [`docs/circuit-to-patch-conversion.md`](docs/circuit-to-patch-conversion.md)

Then read the specific walkthroughs for any effect you plan to extend:

- [`docs/mxr-distortion-plus-circuit-analysis.md`](docs/mxr-distortion-plus-circuit-analysis.md)
- [`docs/wah-build-walkthrough.md`](docs/wah-build-walkthrough.md)

## Official References

- Product page: <https://polyend.com/endless/>
- Playground: <https://polyend.com/playground/>
- Downloads/manuals: <https://polyend.com/downloads/endless-downloads/>
- Official upstream SDK: <https://github.com/polyend/FxPatchSDK>
- Official forum/community: <https://backstage.polyend.com/>

## License

This project is licensed under the MIT License. See [`LICENSE.TXT`](LICENSE.TXT).
