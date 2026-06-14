// source/dsp/ring_buffer.h — bitmask-based circular buffer for delay lines.
//
// Harvested wrap logic from effects/back_talk_reverse_delay.cpp:58-61. The
// bitmask form requires the buffer length to be a power of two; the
// constraint is documented at init() and assumed elsewhere. This is by
// design — the alternative (modulo or branched wrap) costs more per sample
// at Cortex-M7 speeds, and pedal-style delay lines size naturally to
// powers of two from `Patch::kWorkingBufferSize` anyway.
//
// State is held entirely in members. `init()` accepts a pointer into the
// patch's working buffer plus a length. Multiple instances coexist by
// pointing into different slices of the working buffer.
//
// CPU cost: write is two stores and an AND. Read with integer delay is
// one load and an AND. Read with fractional delay is two loads, an AND,
// and the lerp arithmetic.

#pragma once

namespace dsp
{

class RingBuffer
{
public:
    // `length` must be a power of two. `data` must point at storage of at
    // least `length` floats.
    void init(float* data, int length)
    {
        data_  = data;
        mask_  = length - 1;
        write_ = 0;
    }

    void reset()
    {
        if (data_ != nullptr)
        {
            for (int i = 0; i <= mask_; ++i) data_[i] = 0.0f;
        }
        write_ = 0;
    }

    void write(float x)
    {
        data_[write_ & mask_] = x;
        write_ = (write_ + 1) & mask_;
    }

    // Read `samplesBack` integer samples behind the write head (1 = most
    // recently written sample, 2 = the one before that, ...).
    float readDelayed(int samplesBack) const
    {
        const int idx = (write_ - samplesBack) & mask_;
        return data_[idx];
    }

    // Read with fractional delay. samplesBack may have a fractional part;
    // the read is lerp-interpolated between the two surrounding samples.
    float readDelayedFrac(float samplesBack) const
    {
        const int   base = static_cast<int>(samplesBack);
        const float frac = samplesBack - static_cast<float>(base);
        const int   idx0 = (write_ - base) & mask_;
        const int   idx1 = (write_ - base - 1) & mask_;
        return data_[idx0] * (1.0f - frac) + data_[idx1] * frac;
    }

    int mask() const { return mask_; }
    int length() const { return mask_ + 1; }

private:
    float* data_  = nullptr;
    int    mask_  = 0;
    int    write_ = 0;
};

}  // namespace dsp
