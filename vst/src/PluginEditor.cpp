#include "PluginEditor.h"
#include "EffectConfig.h"

namespace
{
const char* const kKnobNames[endless::kParams] = {"Left", "Mid", "Right"};

// Approximate the 16 Patch::Color enum values as on-screen colours.
juce::Colour ledColour(int index)
{
    switch (static_cast<Patch::Color>(index))
    {
        case Patch::Color::kDimWhite:       return juce::Colour(0xff9a9a9a);
        case Patch::Color::kDarkRed:        return juce::Colour(0xff5a0000);
        case Patch::Color::kDarkLime:       return juce::Colour(0xff2f5a00);
        case Patch::Color::kDarkCobalt:     return juce::Colour(0xff00305a);
        case Patch::Color::kLightYellow:    return juce::Colour(0xfffff27a);
        case Patch::Color::kDimBlue:        return juce::Colour(0xff20407a);
        case Patch::Color::kBeige:          return juce::Colour(0xffe8d8b0);
        case Patch::Color::kDimCyan:        return juce::Colour(0xff2f8a8a);
        case Patch::Color::kMagenta:        return juce::Colour(0xffd000d0);
        case Patch::Color::kLightBlueColor: return juce::Colour(0xff7ab8ff);
        case Patch::Color::kPastelGreen:    return juce::Colour(0xff9ad8a0);
        case Patch::Color::kDimYellow:      return juce::Colour(0xffb0a040);
        case Patch::Color::kBlue:           return juce::Colour(0xff1040ff);
        case Patch::Color::kLightGreen:     return juce::Colour(0xff60e060);
        case Patch::Color::kRed:            return juce::Colour(0xffe00000);
        case Patch::Color::kDimGreen:       return juce::Colour(0xff2f7a3a);
    }
    return juce::Colours::black;
}
}  // namespace

FxPatchAudioProcessorEditor::FxPatchAudioProcessorEditor(FxPatchAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor_(p)
{
    for (int i = 0; i < endless::kParams; ++i)
    {
        auto& knob = knobs_[(size_t) i];

        knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible(knob.slider);

        knob.label.setText(kKnobNames[i], juce::dontSendNotification);
        knob.label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knob.label);

        knob.attachment = std::make_unique<juce::SliderParameterAttachment>(
            *processor_.knobParameter(i), knob.slider);
    }

    pressButton_.onClick = [this] { processor_.requestFootswitchPress(); };
    holdButton_.onClick = [this] { processor_.requestFootswitchHold(); };
    addAndMakeVisible(pressButton_);
    addAndMakeVisible(holdButton_);

    setSize(440, 300);
    startTimerHz(20);
}

FxPatchAudioProcessorEditor::~FxPatchAudioProcessorEditor()
{
    stopTimer();
}

void FxPatchAudioProcessorEditor::timerCallback()
{
    const int latest = processor_.ledColorIndex();
    if (latest != ledColor_)
    {
        ledColor_ = latest;
        repaint();
    }
}

void FxPatchAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText(juce::String("FxPatch \xe2\x80\x94 ") + FX_EFFECT_NAME,
               12, 8, getWidth() - 56, 24, juce::Justification::left);

    // State LED.
    const auto led = juce::Rectangle<int>(getWidth() - 34, 10, 20, 20).toFloat();
    g.setColour(ledColour(ledColor_));
    g.fillEllipse(led);
    g.setColour(juce::Colours::black);
    g.drawEllipse(led, 1.0f);

    if (! processor_.sampleRateOk())
    {
        g.setColour(juce::Colour(0xffff5050));
        g.setFont(13.0f);
        g.drawText("Host is not at 48 kHz \xe2\x80\x94 audio passes through dry. "
                   "Set the host sample rate to 48000 Hz.",
                   12, getHeight() - 26, getWidth() - 24, 18,
                   juce::Justification::left);
    }
}

void FxPatchAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(28);  // title row

    auto knobRow = area.removeFromTop(150);
    const int knobWidth = knobRow.getWidth() / endless::kParams;
    for (int i = 0; i < endless::kParams; ++i)
    {
        auto cell = knobRow.removeFromLeft(knobWidth);
        knobs_[(size_t) i].label.setBounds(cell.removeFromTop(20));
        knobs_[(size_t) i].slider.setBounds(cell.reduced(6));
    }

    area.removeFromTop(12);
    auto buttonRow = area.removeFromTop(34);
    pressButton_.setBounds(
        buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(4, 0));
    holdButton_.setBounds(buttonRow.reduced(4, 0));
}
