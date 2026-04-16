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
- The repo keeps the stock SDK shape, but the default SDK target remains `source/PatchImpl.cpp` while helper scripts can build every top-level custom effect without mutating it.
- This fork adds hand-written effects in `effects/`, validation in `tests/`,
  design/reference notes in `docs/`, compiled Playground artifacts in `playground/`,
  and a documented local sync workflow for the official Polyend Plates archive.

For the audit that produced this rewrite, see
[`docs/repository-review.md`](docs/repository-review.md).

## Repository Map

```text
source/        public Patch API and the default single-patch build target
internal/      C ABI wrapper, image header, linker script, patch entrypoint
effects/       custom stock-SDK-compatible patches and reference examples
effects/builds/ local-only generated .endl outputs for hand-written effects
tests/         host-side syntax and lint validation
docs/          SDK notes, patch design walkthroughs, branch/repo review
scripts/       helper utilities such as local Polyend Plates sync and effect builds
playground/    compiled Playground examples and supporting artifacts
```

## Included Effect Work

This fork currently includes ten stock-SDK-compatible custom effects:

- `effects/back_talk_reverse_delay.cpp`: Back Talk-inspired reverse delay with a texture mode and expression-as-mix
- `effects/bbe_sonic_stomp.cpp`: guitar-oriented sonic enhancer inspired by BBE Sonic Stomp / Aion Lumin
- `effects/big_muff.cpp`: Ram's Head-inspired Big Muff fuzz with a Tone Bypass alternate voice and expression-as-blend
- `effects/chorus.cpp`: stereo modulated-delay chorus
- `effects/harmonica.cpp`: blues bullet-mic harmonica voicing of a guitar, with an Open/Cupped voicing toggle and expression-driven hand-cup sweep
- `effects/klon_centaur.cpp`: Klon-inspired transparent overdrive with a Tone Mod alternate voice and expression-as-output
- `effects/mxr_distortion_plus.cpp`: MXR Distortion+ inspired distortion, retuned for smoother Endless control
- `effects/phase_90.cpp`: one-knob Phase 90 phaser with a script-mod vintage alternate voice
- `effects/tube_screamer.cpp`: TS808-inspired overdrive with a TS9 alternate voice and expression-as-tone
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

To build every top-level hand-written effect into local deployment artifacts under
`effects/builds/`:

```bash
bash scripts/build_effects.sh
```

For the official Polyend Plates catalog specifically, see
[`playground/polyend_plates/README.md`](playground/polyend_plates/README.md). That
archive documents the full current Plates lineup, keeps only a small sample of
official binaries tracked in git, and uses
[`scripts/sync_polyend_plates.sh`](scripts/sync_polyend_plates.sh) for local-only
sync. For fast day-to-day use, it also now includes a Plate-by-Plate control cheat
sheet. The hand-written SDK patches have a matching quick reference in
[`effects/README.md`](effects/README.md), and the community reverb example is
summarized in [`effects/examples/README.md`](effects/examples/README.md).
The host-side analyzer now also tracks output-control unity position, midband level
movement, and early limiter pressure so drive retunes are judged against pedal-like
behavior instead of raw RMS span alone.

[`source/PatchImpl.cpp`](source/PatchImpl.cpp) remains the default SDK build target for
ad hoc single-patch work. For repeatable local builds across every hand-written effect,
use [`scripts/build_effects.sh`](scripts/build_effects.sh) or
[`tests/build_effects.sh`](tests/build_effects.sh) instead of manually copying files
into `source/PatchImpl.cpp`.

## Validation

Run the host-side syntax and lint checks before hardware testing:

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
```

This keeps three levels of verification:

- `tests/check_patches.sh`: fast host-side syntax/lint checks
- `tests/build_effects.sh`: real ARM `.endl` builds for all top-level `effects/*.cpp`
- `tests/analyze_effects.sh`: repeatable probe sweeps that check nonlinear growth,
  unity position, and limiter/headroom behavior for the current control laws

## Recommended Reading Order

If you are preparing to add or change code in this fork, start here:

1. [`docs/repository-review.md`](docs/repository-review.md)
2. [`docs/endless-reference.md`](docs/endless-reference.md)
3. [`effects/README.md`](effects/README.md)
4. [`docs/circuit-to-patch-conversion.md`](docs/circuit-to-patch-conversion.md)

Then read the specific walkthroughs for any effect you plan to extend:

- [`docs/back-talk-reverse-delay-build-walkthrough.md`](docs/back-talk-reverse-delay-build-walkthrough.md)
- [`docs/bbe-sonic-stomp-research.md`](docs/bbe-sonic-stomp-research.md)
- [`docs/bbe-sonic-stomp-build-walkthrough.md`](docs/bbe-sonic-stomp-build-walkthrough.md)
- [`docs/big-muff-research.md`](docs/big-muff-research.md)
- [`docs/big-muff-build-walkthrough.md`](docs/big-muff-build-walkthrough.md)
- [`docs/harmonica-build-walkthrough.md`](docs/harmonica-build-walkthrough.md)
- [`docs/klon-centaur-research.md`](docs/klon-centaur-research.md)
- [`docs/klon-centaur-build-walkthrough.md`](docs/klon-centaur-build-walkthrough.md)
- [`docs/mxr-distortion-plus-circuit-analysis.md`](docs/mxr-distortion-plus-circuit-analysis.md)
- [`docs/phase-90-research.md`](docs/phase-90-research.md)
- [`docs/phase-90-build-walkthrough.md`](docs/phase-90-build-walkthrough.md)
- [`docs/tube-screamer-research.md`](docs/tube-screamer-research.md)
- [`docs/tube-screamer-build-walkthrough.md`](docs/tube-screamer-build-walkthrough.md)
- [`docs/wah-build-walkthrough.md`](docs/wah-build-walkthrough.md)

## Official References

- Product page: <https://polyend.com/endless/>
- Playground: <https://polyend.com/playground/>
- Plates: <https://polyend.com/plates/>
- Downloads/manuals: <https://polyend.com/downloads/endless-downloads/>
- Official upstream SDK: <https://github.com/polyend/FxPatchSDK>
- Official forum/community: <https://backstage.polyend.com/>

## License

This project is licensed under the MIT License. See [`LICENSE.TXT`](LICENSE.TXT).
