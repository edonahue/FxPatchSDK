# Playground → SDK: Re-implementing Effects from Source

A guide for taking a Playground-generated effect and building an equivalent (or improved)
version using the FxPatchSDK. Useful when you want to:

- Understand what's inside a `.endl` you like
- Iterate tightly on algorithm details without spending tokens per change
- Modify behavior Playground can't expose (e.g., change which knob the expression pedal
  targets, add a feature that requires code-level control)
- Build something you own and can evolve freely

---

## Reading a Playground effect

Each SpiralCaster example includes a `.pdf` prompt log. The final `(result)` block is the
most useful section — it describes the implemented algorithm in one paragraph with parameter
ranges and control behavior. This is the spec you're implementing.

Example (BELLOWS):
```
Stereo plate reverb with linked input-detected dynamics: gated tails by default, ducking
swells when toggled. Params: SizeDec 0–1 (size/decay macro), Thresh 0–1 (gate/duck
threshold), Tone 0–1 (tail brightness). Short press toggles Gated↔Ducking (LED cyan for
ducking); long press resets to Gated and flushes the reverb state.
```

From this you can extract:
- **Algorithm:** Freeverb-style plate reverb + envelope follower + gain shaping
- **Knob 0 (Left):** SizeDec — feeds `roomSize` and `damp` of the reverb
- **Knob 1 (Mid):** Thresh — threshold for envelope comparator
- **Knob 2 (Right):** Tone — post-reverb lowpass or shelving EQ
- **State machine:** `bool ducking` toggled by short press
- **LED:** `kDimCyan` when ducking, something else when gating
- **Long press:** resets `ducking = false`, zeros reverb delay lines

---

## The translation process

### 1. Identify the DSP building blocks

Most Playground effects combine a small set of algorithms:

| Playground description | SDK building block |
|---|---|
| "Plate reverb" | Freeverb (4 comb + 2 all-pass); see `effects/examples/reverb.cpp` |
| "Feedback delay" | Ring buffer + feedback coefficient |
| "Ladder LPF" | 4-pole Moog ladder (Huovilainen model or simpler bilinear) |
| "8-stage phaser" | Chain of 8 first-order all-pass sections with LFO |
| "Envelope follower" | `env = max(abs(in), env * decay)` |
| "Granular" | Ring buffer → overlapping windowed grains |
| "Bitcrush" | `round(x * N) / N`; see `source/PatchImpl.cpp` |
| "ZOH sample-rate reduction" | Counter-driven hold; see BOTTO-WAH in CATALOG.md |
| "Soft limiting" | `tanh(x * gain) / gain` or `x / (1 + abs(x))` |
| "HPF on wet signal" | 1-pole IIR: `y = a * (y_prev + x - x_prev)` |
| "Sine oscillator" | Phase accumulator: `phase += freq / 48000.0f; out = sinf(phase * 2π)` |
| "Tap tempo" | Store timestamp on press, compute period between presses |

### 2. Map knobs to parameters

From the result description, extract each knob's range and map it in `getParameterMetadata`:

```cpp
ParameterMetadata getParameterMetadata(int paramIdx) override {
    if (paramIdx == 0) return {0.0f, 1.0f, 0.5f};  // SizeDec
    if (paramIdx == 1) return {0.0f, 1.0f, 0.3f};  // Thresh
    return                      {0.0f, 1.0f, 0.5f};  // Tone
}
```

For non-0–1 ranges, store the raw 0–1 knob value and map in `setParamValue` or in
`processAudio` before use:

```cpp
// Knob gives 0–1; map to 30–900 ms
float delayMs = 30.0f + knobDelay_ * 870.0f;
int delaySamples = static_cast<int>(delayMs * 0.001f * kSampleRate);
```

For logarithmic ranges (frequency controls, Jacob's Ladder):
```cpp
// Map 0–1 to 20–20000 Hz log
float freq = 20.0f * std::pow(1000.0f, knob_);  // 20 Hz at 0, 20 kHz at 1
```

### 3. Implement footswitch state machines

Short press (idx 0) and long press (idx 1) both fire `handleAction`. Pattern for
cycle-through modes:

```cpp
void handleAction(int idx) override {
    if (idx == 0) {
        mode_ = (mode_ + 1) % NUM_MODES;
    }
    if (idx == 1) {
        mode_ = 0;  // reset
        // clear state...
    }
}
```

Pattern for toggle:
```cpp
void handleAction(int idx) override {
    if (idx == 0) active_ = !active_;
}
```

Pattern for tap tempo:
```cpp
void handleAction(int idx) override {
    if (idx == 0) {
        // approximate tap tempo — no real-time clock in SDK
        // maintain a sample counter and compute period between presses
        if (lastTapSample_ > 0) {
            int period = tapSampleCounter_ - lastTapSample_;
            delayTarget_ = std::clamp(period, minDelay_, maxDelay_);
        }
        lastTapSample_ = tapSampleCounter_;
    }
}
// In processAudio, increment tapSampleCounter_ each sample (or each buffer)
```

Note: the SDK has no real-time clock. You have to count samples yourself. See §3a of
`docs/endless-reference.md` for the full SDK interface.

### 4. Wire the LED

```cpp
Color getStateLedColor() override {
    if (mode_ == 0) return Color::kDimBlue;
    if (mode_ == 1) return Color::kDimCyan;
    if (mode_ == 2) return Color::kLightBlueColor;
    return Color::kBlue;
}
```

See `docs/endless-reference.md §3a` for the full list of 16 color values.

### 5. Allocate large buffers from working buffer

Delay lines, reverb comb buffers, granular ring buffers — anything large must come from the
working buffer provided via `setWorkingBuffer`, not from member arrays. The working buffer
is 2.4M floats (~9.6 MB) in external RAM.

```cpp
void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override {
    // Sub-allocate from buf
    size_t offset = 0;
    delayL_ = buf.subspan(offset, MAX_DELAY_SAMPLES);  offset += MAX_DELAY_SAMPLES;
    delayR_ = buf.subspan(offset, MAX_DELAY_SAMPLES);  offset += MAX_DELAY_SAMPLES;
    // etc.
}
```

External RAM access has higher latency than internal SRAM — avoid tight inner-loop random
access. Sequential reads (ring buffer playback) are fine. Sparse random reads (granular
grain scheduling to arbitrary offsets) add latency per access.

---

## Example translation: BELLOWS (gated plate reverb)

### What Playground built

From the result description:
```
Stereo plate reverb with linked input-detected dynamics: gated tails by default, ducking
swells when toggled. Params: SizeDec 0–1 (size/decay macro), Thresh 0–1 (gate/duck
threshold), Tone 0–1 (tail brightness).
```

### SDK skeleton

```cpp
#include "Patch.h"
#include <cmath>
#include <algorithm>

// Freeverb constants (from andybalham/FxPatchSDK — see effects/examples/reverb.cpp)
static constexpr int kCombLengths[8] = {1116,1188,1277,1356,1422,1491,1557,1617};
static constexpr int kApLengths[2]   = {556, 441};
static constexpr int kMaxDelay = 2048;

class PatchImpl : public Patch {
public:
    void init() override {
        sizeDec_ = 0.5f;
        thresh_  = 0.3f;
        tone_    = 0.5f;
        ducking_ = false;
        env_     = 0.0f;
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override {
        // Sub-allocate comb and all-pass buffers here
        // (see effects/examples/reverb.cpp for full Freeverb layout)
        workBuf_ = buf;
    }

    void processAudio(std::span<float> left, std::span<float> right) override {
        for (size_t i = 0; i < left.size(); ++i) {
            float in = (left[i] + right[i]) * 0.5f;

            // Envelope follower
            float absIn = std::abs(in);
            env_ = std::max(absIn, env_ * 0.9995f);

            // Gate/duck gain
            float gain = 1.0f;
            if (!ducking_) {
                // Gated: silence reverb when input is below threshold
                gain = (env_ > thresh_) ? 1.0f : 0.0f;
            } else {
                // Ducking: reverb swells when signal drops
                gain = (env_ < thresh_) ? 1.0f : 0.0f;
            }

            // Run reverb (placeholder — use full Freeverb from reverb.cpp)
            float wetL = in, wetR = in;  // replace with actual reverb output
            wetL *= gain;
            wetR *= gain;

            // Tone: simple 1-pole LPF
            float coeff = tone_;  // 1.0 = bright, 0.0 = dark
            toneL_ = toneL_ + coeff * (wetL - toneL_);
            toneR_ = toneR_ + coeff * (wetR - toneR_);

            left[i]  = toneL_;
            right[i] = toneR_;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override {
        if (paramIdx == 0) return {0.0f, 1.0f, 0.5f};  // SizeDec
        if (paramIdx == 1) return {0.0f, 1.0f, 0.3f};  // Thresh
        return                      {0.0f, 1.0f, 0.5f};  // Tone
    }

    void setParamValue(int idx, float value) override {
        if (idx == 0) sizeDec_ = value;
        if (idx == 1) thresh_  = value;
        if (idx == 2) tone_    = value;
    }

    void handleAction(int idx) override {
        if (idx == 0) ducking_ = !ducking_;
        if (idx == 1) {
            ducking_ = false;
            // TODO: flush reverb state (zero delay lines)
        }
    }

    Color getStateLedColor() override {
        return ducking_ ? Color::kDimCyan : Color::kDimWhite;
    }

private:
    float sizeDec_, thresh_, tone_;
    bool  ducking_;
    float env_;
    float toneL_ = 0.0f, toneR_ = 0.0f;
    std::span<float, kWorkingBufferSize> workBuf_;
};

static PatchImpl patch;
Patch* Patch::getInstance() { return &patch; }
```

This is a skeleton, not a complete implementation — replace the reverb placeholder with the
Freeverb algorithm from `effects/examples/reverb.cpp`. The pattern for everything else
(envelope follower, gate/duck logic, parameter mapping, footswitch, LED) is complete and
correct.

---

## Which effects are approachable vs. complex

### Approachable without specialized DSP knowledge

These effects are buildable with standard techniques covered in the SDK and community examples:

| Effect | Key components |
|---|---|
| BELLOWS | Freeverb + envelope follower + gain shaping |
| EVENT HORIZON | Freeverb + high-feedback mode (feedback → 1.0) |
| DROPGATE | Envelope follower + smoothed VCA |
| TRIPLE SLAPPER | Three ring-buffer delay lines |
| SILICON SUPER VILLAIN | Hard/soft clipping stages + biquad notch filter |
| WALL PUSHER | Freeverb + tanh saturation |
| OUROBOROS | Chain of all-pass sections + LFO |
| ORBITAL WASHER | Freeverb + all-pass phaser in series |
| SHOEGAZI | Ring buffer + clipping + 1-pole filter |

### Moderate complexity

These require more careful buffer management or intermediate DSP:

| Effect | Key challenge |
|---|---|
| GRAVITY MELT | Pitch shifting per delay repeat (granular or linear interpolation pitch shift) |
| OILY BOY | Three independent delay lines + nonlinear feedback model |
| RIPTIDE | Windowed reverse playback with crossfading + linked timing |
| BOTTO-WAH | State-variable bandpass filter swept by LFO |
| JACOB'S LADDER | 4-pole ladder filter (nonlinear Moog model) |
| CLOUDSTRETCH | Granular engine (ring buffer + multiple simultaneous grains) |
| STARDUST | Granular + pitch shift + reverb + tremolo in series |

### Complex

These involve algorithms that are difficult to implement correctly without DSP background:

| Effect | Key challenge |
|---|---|
| AUTOSYNTH PSYCHONAUT | Polyphonic chord synthesis + pitch generation without MIDI |
| MOODY HARP | Internal arpeggiator sequencer + synth voice |
| GHOST THEREMIN | Sine oscillator + dual delay with psychedelic feedback ramp |
| CASCADE FLANGE | Pitched feedback (frequency-domain pitch shift in delay feedback path) |
| STUTTERFRAG | Precise crossfaded forward/reverse direction switching |
| FAULT LINE | Randomized buffer scheduling with hard boundary cuts |
| REVERSE MICRO | Continuous reverse playback with pre-delay and speed control |

---

## Working from community examples

`effects/examples/reverb.cpp` (from andybalham/FxPatchSDK) compiles against the stock SDK
and is the best starting point for any reverb-based effect. It implements Freeverb correctly
with proper working buffer sub-allocation — study the `setWorkingBuffer` pattern there
before writing any effect that needs a large delay buffer.

For effects from `sthompsonjr/Endless-FxPatchSDK` (Optical Compressor, Tubescreamer, Wah,
Big Muff, ProCo Rat, etc.) — those use the WDF library and won't compile against the stock
SDK. Clone that repo directly; it's self-contained and well-structured.

See `docs/endless-reference.md §9` for a full breakdown of all community forks.

---

## See also

- [`effects/README.md`](../effects/README.md) — patch workflow and template
- [`effects/examples/reverb.cpp`](../effects/examples/reverb.cpp) — Freeverb reference implementation
- [`docs/endless-reference.md`](endless-reference.md) — full SDK reference
- [`playground/examples/SpiralCaster_Examples/CATALOG.md`](../playground/examples/SpiralCaster_Examples/CATALOG.md) — all 23 effects with specs
