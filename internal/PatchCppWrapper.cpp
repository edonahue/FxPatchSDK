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
        bool isKnob = sourceId == 0;
        if (idx < endless::kParams && idx >= 0 && isKnob)
        {
            return 1;
        }

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
