# playground/

This directory holds artifacts produced through Polyend's
[Playground](https://polyend.com/playground/) flow rather than through the local SDK.

Use it as reference material, not as editable source:

- `.endl` files are compiled patch binaries meant for deployment to Endless
- PDFs and related files are useful for naming, UX, and creator-output review
- `examples/` holds archived creator example bundles gathered from external demos
- `polyend_plates/` documents the official Polyend Plates catalog and supports a local-only full archive sync
- none of these files replace readable C++ source in `effects/`

The practical reason to keep these artifacts in the repo is comparison: they show what
the hosted Polyend workflow produces, while the rest of this fork focuses on deliberate,
version-controlled SDK development.

`playground/polyend_plates/` now has two layers:

- five tracked official `.endl` sample files kept in git as reference artifacts
- the broader official Plates archive, which can be synced locally with `bash scripts/sync_polyend_plates.sh` and stays out of git by default

That separation is intentional. The docs attribute the official catalog to Polyend and
keep the bulk synced downloads local-only for personal archival/reference use.

See [`playground/examples/SpiralCaster_Examples/README.md`](examples/SpiralCaster_Examples/README.md)
for the creator archive and [`playground/polyend_plates/README.md`](polyend_plates/README.md)
for the documented Plates archive.
