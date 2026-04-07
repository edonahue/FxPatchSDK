# playground/

Reference material for Polyend's Playground — the hosted text-to-effect service for the
Endless pedal.

---

## What is Playground?

Playground is Polyend's AI-powered effect generator. You describe what you want in plain
language, and a pipeline of specialized agents parses the request, selects DSP algorithms,
generates and compiles C++ code, runs automated tests, and hands back a ready-to-use
`.endl` file — in minutes, for a per-prompt token fee.

Access requires a registered Endless owner account. See `docs/endless-reference.md §2` for
the token economy and cost breakdown.

---

## This directory

```
playground/
  README.md                    ← you are here
  prompting-best-practices.md  ← how to write effective prompts; revision patterns
  examples/
    README.md                  ← examples directory overview
    SpiralCaster_Examples/
      README.md                ← SpiralCaster collection overview
      CATALOG.md               ← per-effect reference: all 23 effects with controls
      <effect-name>/
        <effect>.endl          ← ready-to-use binary
        <effect>.pdf           ← prompt log (prompts, revisions, and final result)
```

---

## Quick start

1. Load a `.endl` from `examples/SpiralCaster_Examples/<name>/` onto your Endless (USB-C
   drag-and-drop) to hear what Playground produces
2. Read the matching `.pdf` to see the prompt that generated it
3. Read [`prompting-best-practices.md`](prompting-best-practices.md) to understand what
   patterns work well
4. Read [`examples/SpiralCaster_Examples/CATALOG.md`](examples/SpiralCaster_Examples/CATALOG.md)
   for a quick-reference table of all 23 effects with knob assignments

---

## SDK vs. Playground

| | SDK | Playground |
|---|---|---|
| Cost | Free, unlimited | ~$1–5 per effect |
| Control | Full C++ source | Black box |
| Speed | Build time + deploy | Minutes |
| Skill required | C++, DSP | Natural language |
| Revisable? | Edit source directly | Multi-round prompt sessions |

Use Playground for rapid prototyping and exploration. Use the SDK when you need full
control, want to iterate tightly, or want to understand exactly what's happening.

See [`docs/playground-to-sdk.md`](../docs/playground-to-sdk.md) for a guide to
re-implementing a Playground effect as an SDK patch.

---

## See also

- [`docs/endless-reference.md`](../docs/endless-reference.md) — full SDK reference including §2 Playground overview
- [`effects/`](../effects/) — custom patches built from source with the SDK
