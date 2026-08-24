// source/dsp/dsp.h — meta-include for the dsp/ primitive layer.
//
// Effects that want everything can `#include "dsp/dsp.h"`. Effects that
// care about translation-unit size or want to make their dependencies
// explicit can pick individual headers.

#pragma once

#include "crossfade.h"
#include "dc_blocker.h"
#include "filter_coeff.h"
#include "fractional_delay.h"
#include "lfo.h"
#include "one_pole_filter.h"
#include "parameter_smoother.h"
#include "ring_buffer.h"
#include "clamp.h"
#include "soft_limit.h"
