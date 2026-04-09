# effects/builds/

This directory holds generated `.endl` artifacts built from the hand-written
patches in `effects/*.cpp`.

These files are local build products, not source:

- `bash scripts/build_effects.sh` writes one `.endl` per top-level effect here
- `bash tests/build_effects.sh` verifies the full ARM build flow and checks the outputs
- generated `.endl` files in this directory are ignored by git on purpose

Treat this directory as a local convenience cache for deployment and validation, not
as the canonical home of version-controlled artifacts.
