#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <utility>

// Mapping of IDs used in the API to Endless hardware properties.
namespace endless
{
constexpr int kParams = 3;

enum class ParamId
{
    kParamLeft,
    kParamMid,
    kParamRight,
};

enum class ActionId
{
    kLeftFootSwitchPress,
    kLeftFootSwitchHold,
};
}

/**
 *  Abstract base class to be implemented by the SDK user.
 *
 *  The class methods are called by the host application to initialize and process audio.
 */
class Patch
{
  public:
    static constexpr int kWorkingBufferSize = 2400000;
    static constexpr int kSampleRate = 48000;

    struct ParameterMetadata
    {
        float minValue;
        float maxValue;
        float defaultValue;
    };

    enum class Color
    {
        kDimWhite,
        kDarkRed,
        kDarkLime,
        kDarkCobalt,
        kLightYellow,
        kDimBlue,
        kBeige,
        kDimCyan,
        kMagenta,
        kLightBlueColor,
        kPastelGreen,
        kDimYellow,
        kBlue,
        kLightGreen,
        kRed,
        kDimGreen,
    };

    /**
     * Returns singleton instance of the patch implementation.
     */
    static Patch* getInstance();

    /**
     * Called once at patch load time.
     */
    virtual void init() = 0;

    /**
     * Called once to provide a working buffer for audio processing.
     * Note: the buffer is allocated in external memory that has higher latency when accessed by
     * the CPU.
     */
    virtual void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) = 0;

    /**
     * Called repeatedly to process audio.
     * Left and right buffers are guaranteed to have the same size.
     */
    virtual void processAudio(std::span<float> audioBufferLeft,
                              std::span<float> audioBufferRight) = 0;

    /**
     * Gets metadata for a parameter.
     */
    virtual ParameterMetadata getParameterMetadata(int paramIdx) = 0;

    /**
     * Called repeatedly to set a parameter value when changed by the user.
     * The function is called from the audio thread, no need to synchronize access to internal
     * structures.
     */
    virtual void setParamValue(int paramIdx, float value) = 0;
    /**
     * Called when an action is triggered by the user.
     */
    virtual void handleAction(int actionIdx) = 0;

    /**
     * Called by the host periodically to get the color of the state LED.
     */
    virtual Color getStateLedColor() = 0;
};
