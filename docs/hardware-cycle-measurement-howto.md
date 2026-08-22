# Hardware cycle-measurement how-to (walkthrough, not verified)

## Status: unconfirmed against real Endless hardware

This document describes the standard Cortex-M7 debug-probe methodology for
measuring real per-sample cycle costs. It is **not** exercised or verified
against a physical Polyend Endless in this repo or by this session's
research. Specifically unconfirmed:

- Whether the Endless PCB exposes SWD/JTAG debug pins at all (no teardown
  exists in this session's research to confirm this).
- Whether the firmware loader that runs a patch leaves the debug interface
  enabled, or locks it down for production units.
- Whether anything below actually works on this specific pedal.

Treat this as "when you have a debug probe and a teardown, here's where to
start" — not a pre-verified recipe. If you get real numbers this way,
record them in [`cycle-budget.md`](cycle-budget.md)'s per-patch table with
the measurement source noted, and update this doc's status section with
what you learned (probe used, whether debug access worked, any deviations
from the steps below).

## Why this, and not something automatable from this environment

[`cycle-budget.md`](cycle-budget.md) documents two paths to real cycle
numbers: host-cycle-counted runs of `effect_probe.cpp` (useful only as a
between-patch *ordering* signal, since host CPU cycles aren't Cortex-M7
cycles), and real hardware measurement. This doc is the latter. It requires
physical access to a debug probe (an ST-Link, J-Link, or similar SWD/JTAG
adapter) connected to the pedal's PCB, which is why it can't be scripted or
verified from this sandboxed environment — there is no such hardware here.

## The DWT cycle counter

The Cortex-M7's Data Watchpoint and Trace (DWT) unit includes a free-running
cycle counter, `DWT->CYCCNT`, that increments once per core clock cycle
(720 MHz on Endless per [`endless-reference.md`](endless-reference.md)).
This is the standard, low-overhead way to measure wall-clock cycles for a
span of code on any Cortex-M with a DWT unit — no external timer or
profiler needed, just two register reads bracketing the code of interest.

### 1. Enable the counter

The DWT unit is disabled by default and must be enabled through the Core
Debug block first:

```cpp
#include <cstdint>

// CoreDebug and DWT registers, standard CMSIS layout for any Cortex-M7.
// If a CMSIS device header is available for this target, prefer its
// CoreDebug_Type/DWT_Type definitions over hand-rolled addresses.
#define DEMCR_ADDR   0xE000EDFCu  // CoreDebug->DEMCR
#define DWT_CTRL_ADDR 0xE0001000u // DWT->CTRL
#define DWT_CYCCNT_ADDR 0xE0001004u // DWT->CYCCNT

inline void enableCycleCounter()
{
    volatile uint32_t& demcr = *reinterpret_cast<volatile uint32_t*>(DEMCR_ADDR);
    volatile uint32_t& dwtCtrl = *reinterpret_cast<volatile uint32_t*>(DWT_CTRL_ADDR);
    volatile uint32_t& cycCnt = *reinterpret_cast<volatile uint32_t*>(DWT_CYCCNT_ADDR);

    demcr |= (1u << 24);   // TRCENA: enable trace/debug subsystem
    cycCnt = 0;
    dwtCtrl |= (1u << 0);  // CYCCNTENA: enable the free-running cycle counter
}
```

This has to run once, early — `init()` is the natural place if you're
instrumenting a patch build. It is **not** something to leave in a shipped
patch; it's a temporary measurement build.

### 2. Bracket `processAudio`

Wherever the harness or firmware calls `processAudio`, read the counter
immediately before and after:

```cpp
volatile uint32_t& cycCnt = *reinterpret_cast<volatile uint32_t*>(DWT_CYCCNT_ADDR);

const uint32_t start = cycCnt;
patch.processAudio(left, right);
const uint32_t elapsed = cycCnt - start;  // wraps correctly at UINT32_MAX
```

`elapsed` is total cycles for that block; divide by the block's sample
count for cycles/sample. Cortex-M7 has an instruction/data cache and branch
predictor, so expect variance between the first few calls (cold cache) and
steady-state — record several blocks and report a range, not a single
number, the same way [`cycle-budget.md`](cycle-budget.md) asks for
"the resulting cycles/sample (or a tight range), and the spread/uncertainty."

### 3. Getting the numbers off the device

This is the part that's genuinely uncertain for Endless specifically: patch
code has no documented host-communication channel (no UART/USB logging API
in the SDK surface). Two realistic options, in order of how much debug-probe
sophistication they need:

**Breakpoint + register/memory read (simplest, most likely to work).**
Stop execution at a breakpoint placed after the `elapsed` computation (or
after N iterations, if accumulating a running sum/histogram in a static
variable) and read the value directly through the debug probe's memory/
register inspection — every SWD probe's host tooling (OpenOCD, J-Link
Commander, pyOCD, etc.) supports this. No firmware changes needed beyond
the instrumentation itself; this is the first thing to try.

**Write to a known, probe-pollable memory address.** If breaking execution
isn't acceptable (e.g. you need cycle counts across many blocks without
halting the audio pipeline), write each block's `elapsed` value into a
fixed offset of the working buffer (`setWorkingBuffer`'s 9.6 MB region) —
it's the one large, patch-writable memory region with a known runtime
address, and using a few floats of it for a temporary measurement build
doesn't compete with any patch's actual delay-line/state usage if you pick
an offset near the end. A debug-probe session can then poll that address
directly (`mem2array` in OpenOCD's Tcl console, or the equivalent in
whichever tool you're using) without halting the core. Revert this before
shipping — it's a measurement-build-only hack, not something that belongs
in `effects/*.cpp`.

Either way, this is inherently a **manual, probe-session activity** — there
is no automated path to extract these numbers without a person at a debug
probe. That's why [`cycle-budget.md`](cycle-budget.md) treats it as
external data to record, not something `tests/`/`scripts/` can produce on
their own.

## What this doc deliberately does not attempt

- No claim about whether Endless's firmware leaves SWD/JTAG accessible on
  a production unit, or whether opening the case voids anything. That's a
  hardware/legal question outside this repo's scope — establish it before
  attempting any of this.
- No script or harness change in this repo. Nothing here can be exercised
  in this environment (no probe, no physical pedal), so there is nothing
  to validate by running tests.
- No claim that the DWT register addresses above are correct for whatever
  specific Cortex-M7 part variant Endless uses. They are the standard
  CMSIS-defined addresses for the architecture, not implementation-guessed
  — but "standard for the architecture" and "confirmed against this part"
  are different claims, and only the first is being made here.
