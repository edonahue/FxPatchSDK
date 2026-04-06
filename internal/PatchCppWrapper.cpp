#include "PatchCppWrapper.h"
#include "../source/Patch.h"

#include <string.h>

extern "C"
{
    bool error = false;

    void patch_agent_set_buffer(const PatchEnv* /* env */, float* buffer)
    {
        if (buffer != nullptr)
        {
            Patch::getInstance()->setWorkingBuffer(
              std::span<float, Patch::kWorkingBufferSize>(buffer, Patch::kWorkingBufferSize));
        }
        else
        {
            error = true;
        }
    }

    int patch_agent_get_buffer_size(const PatchEnv* /* env */)
    {
        return Patch::kWorkingBufferSize;
    }

    float patch_agent_update_buffers(const PatchEnv* /* env */,
                                     float* left,
                                     float* right,
                                     int samples)
    {
        Patch::getInstance()->processAudio(std::span<float>(left, samples),
                                           std::span<float>(right, samples));
        return 0.0f;
    }

    float patch_agent_get_param_min(const PatchEnv* /* env */, int idx)
    {
        return Patch::getInstance()->getParameterMetadata(idx).minValue;
    }

    float patch_agent_get_param_max(const PatchEnv* /* env */, int idx)
    {
        return Patch::getInstance()->getParameterMetadata(idx).maxValue;
    }

    float patch_agent_get_param_default(const PatchEnv* /* env */, int idx)
    {
        return Patch::getInstance()->getParameterMetadata(idx).defaultValue;
    }

    uint8_t patch_agent_is_param_enabled(const PatchEnv* /* env */, int idx, int sourceId)
    {
        bool isKnob       = sourceId == 0;
        bool isExpression = sourceId == 1;

        // Knobs: Left (0), Mid (1), Right (2) are always enabled.
        if (idx >= 0 && idx < endless::kParams && isKnob)
            return 1;

        // Expression pedal: mapped to param 2 (Right knob position).
        // heel down = 0.0, toe down = 1.0 — same range as the knob.
        // When the expression pedal is connected the firmware will call
        // setParamValue(2, ...) from the pedal; the Right knob is ignored
        // while the pedal is plugged in.
        if (idx == 2 && isExpression)
            return 1;

        return 0;
    }

    void patch_agent_get_param_name(const PatchEnv* /* env */,
                                    int /* idx */,
                                    char* out,
                                    size_t bufferSize)
    {
        if (bufferSize > 0)
        {
            out[0] = '\0';
        }
    }

    void patch_agent_get_param_unit(const PatchEnv* /* env */,
                                    int /* idx */,
                                    char* out,
                                    size_t /* bufferSize */)
    {
        out[0] = '\0';
    }

    int patch_agent_get_state_idx(const PatchEnv* /* env */)
    {
        return error ? static_cast<int>(Patch::Color::kRed)
                     : static_cast<int>(Patch::getInstance()->getStateLedColor());
    }

    void patch_agent_set_param(const PatchEnv* /* env */, int idx, float value)
    {
        Patch::getInstance()->setParamValue(idx, value);
    }

    void patch_agent_special_action(const PatchEnv* /* env */, int idx)
    {
        Patch::getInstance()->handleAction(idx);
    }

    void patch_agent_init(const PatchEnv* /* env */)
    {
        Patch::getInstance()->init();
    }
} // extern "C"
