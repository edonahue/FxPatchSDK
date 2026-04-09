# Polyend Plates Archive

This directory archives compiled `.endl` effects patches sourced from the
[Polyend Plates](https://polyend.com/plates/) collection for Endless.

These files are deployment artifacts, not editable SDK source:

- `.endl` files are compiled binaries meant to be copied to Endless
- local inspection can recover some embedded control labels with `strings`, but not a full spec
- authoritative descriptions, categories, and control behavior come from the matching Polyend product pages

The first four files in this folder were added manually before this documentation pass.
`Wax.endl` was downloaded here as an automation proof-of-concept on 2026-04-08 by
following the product page's `Download free effect` link.

## Plates Summary

| Local file | Size | Plate | Categories | Author | Description | Embedded labels recovered locally | Source |
| --- | ---: | --- | --- | --- | --- | --- | --- |
| `Dual-Lines.endl` | 9064 bytes | [Dual Lines](https://polyend.com/product/dual-lines/) | Delay, Drive | Polyend | Dual delay that couples a phasered ping-pong line with a gritty tape echo so the repeats interact through cross-feedback and can leave a short ghost texture between notes. | `Ratio`, `Cross` | Pre-existing local copy matched to the official product page and download link. |
| `Malleus_Fuzz.endl` | 3148 bytes | [Malleus](https://polyend.com/product/malleus/) | Drive | Polyend | Thick, sustaining fuzz with two-stage clipping, a broad tone sweep, and enough level control to stay heavy without disappearing in a mix. | `Fuzz`, `Tone`, `Level` | Pre-existing local copy matched to the official product page and download link. |
| `Slapper_Morton.endl` | 3172 bytes | [Slapper Morton](https://polyend.com/product/slapper-morton/) | Drive | Polyend | Aggressive drive plus a tight slapback delay for width and pseudo-double-tracking without smearing the pick attack. | `Drive`, `DelayMs`, `Level` | Pre-existing local copy matched to the official product page and download link. |
| `Yield_reverse_delay.endl` | 4084 bytes | [Yield](https://polyend.com/product/yield/) | Delay | Polyend | Lo-fi reverse delay built around short reverse windows for tape-like smears, back-spun phrases, and ambient beds under the dry signal. | `Speed`, `Tone` | Pre-existing local copy matched to the official product page and download link. |
| `Wax.endl` | 13120 bytes | [Wax](https://polyend.com/product/wax/) | Modulation, Sim | Polyend | Vinyl-character modulation that combines smooth warble, irregular drift, and record-wear behavior for unstable pitch motion and worn-media texture. | `DRIFT`, `SPEED` | Downloaded automatically on 2026-04-08 from the product page's `Download free effect` link. |

## Control Reference

### Dual Lines

Product page: <https://polyend.com/product/dual-lines/>

| Control | Behavior |
| --- | --- |
| `Ratio` | Relationship between the two delay times, from tighter lock to more polyrhythmic tension. |
| `Mix` | Cross-feedback depth between the two delay paths; does not change the output blend. |
| `Grit` | Tape drive plus wow/flutter depth, from clean to warmer and less stable. |
| Short footswitch press | Tap tempo and clear buffers. |
| Long footswitch press | Toggle bright/dark drift, a slow tonal shift that can be reversed with another press. |

### Malleus

Product page: <https://polyend.com/product/malleus/>

| Control | Behavior |
| --- | --- |
| `Fuzz` | Amount of fuzz and sustain. |
| `Tone` | Tone sweep from 200 Hz to 4000 Hz. |
| `Level` | Output volume. |
| Short footswitch press | Toggle hi/lo gain. |
| Long footswitch press | Cycle clipping mode through Germanium, Silicon, and Bypass. |

### Slapper Morton

Product page: <https://polyend.com/product/slapper-morton/>

| Control | Behavior |
| --- | --- |
| `Drive` | Gain and saturation amount. |
| `Delay Ms` | Slapback time from 5 ms to 60 ms. |
| `Level` | Output volume from -12 dB to +12 dB. |
| Short footswitch press | Toggle delay on/off. |
| Long footswitch press | No action. |

### Yield

Product page: <https://polyend.com/product/yield/>

| Control | Behavior |
| --- | --- |
| `Mix` | Dry/wet blend. |
| `Speed` | Reverse window length from 0.12 s to 1.45 s. |
| `Tone` | Brightness from 200 Hz to 4000 Hz. |
| Short footswitch press | Toggle Wide Window for smoother crossfades and softer edges. |
| Long footswitch press | Enable random stereo toss so each new window pans left or right. |

### Wax

Product page: <https://polyend.com/product/wax/>

| Control | Behavior |
| --- | --- |
| `Drift` | Depth and unruliness of the pitch modulation. |
| `Speed` | Rate of the modulation motion. |
| `Age` | Record-wear macro that darkens and thickens the sound while adding micro-dropouts at higher settings. |
| Short footswitch press | Cycle Dust state: Silent, Faint hiss, Worn hiss + crackle, Heavy crackle. |
| Long footswitch press | Cycle Vinyl mode: Classic, Locked Groove, Dubplate, Heat Warp. |

## Local Inspection Notes

The binaries in this directory contain enough plain-text data to confirm that the
files are Endless patches and to recover some control labels:

```bash
strings playground/polyend_plates/Malleus_Fuzz.endl
strings playground/polyend_plates/Wax.endl
```

That inspection is useful for sanity-checking filenames and parameter labels, but it
does not fully describe the patch behavior. For example, `Dual-Lines.endl` exposes
`Ratio` and `Cross` locally while the full control spec on the product page is
`Ratio`, `Mix`, and `Grit`.

## Automation Notes

Polyend's Plates product pages expose a direct `Download free effect` link to a
hosted `.endl` asset. `Wax.endl` was added with:

```bash
curl -fsSL -A Mozilla/5.0 \
  https://polyend-website.fra1.digitaloceanspaces.com/wp-content/uploads/2026/03/23175324/Wax.endl \
  -o playground/polyend_plates/Wax.endl
```

Practical workflow for future additions:

1. Open the matching product page from <https://polyend.com/plates/>.
2. Resolve the `Download free effect` target URL.
3. Save the `.endl` into this directory using the shipped filename unless there is a repo-specific reason to normalize it.
4. Update this README with the product URL, categories, description, and controls from the product page.

This repo keeps the binaries and the human-readable notes together so the archive is
useful even though the patch internals are not editable here.
