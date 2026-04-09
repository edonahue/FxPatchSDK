# playground/

This directory holds artifacts produced through Polyend's
[Playground](https://polyend.com/playground/) flow rather than through the local SDK.

Use it as reference material, not as editable source:

- `.endl` files are compiled patch binaries meant for deployment to Endless
- PDFs and related files are useful for naming, UX, and creator-output review
- `examples/` holds archived creator example bundles gathered from external demos
- `polyend_plates/` holds archived Polyend Plates `.endl` files plus product-page-based documentation
- none of these files replace readable C++ source in `effects/`

The practical reason to keep these artifacts in the repo is comparison: they show what
the hosted Polyend workflow produces, while the rest of this fork focuses on deliberate,
version-controlled SDK development.

See [`playground/examples/SpiralCaster_Examples/README.md`](examples/SpiralCaster_Examples/README.md)
for the creator archive and [`playground/polyend_plates/README.md`](polyend_plates/README.md)
for the documented Plates archive.
