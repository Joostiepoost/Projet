// Theme.h — Projet's colours and look (dark, as in the mockups).

#pragma once

#include <JuceHeader.h>

namespace Theme
{
    inline const juce::Colour background   { 0xff141518 };
    inline const juce::Colour panel        { 0xff1c1d21 };
    inline const juce::Colour panel2       { 0xff23252a };
    inline const juce::Colour header       { 0xff1a1b1f };
    inline const juce::Colour line         { 0xff2e3036 };
    inline const juce::Colour lineStrong   { 0xff4a4d55 };
    inline const juce::Colour text         { 0xffe8e6e1 };
    inline const juce::Colour textMuted    { 0xffa19f97 };
    inline const juce::Colour accent       { 0xffe8b04b };     // selection, playhead
    inline const juce::Colour signs        { 0xffa3e49c };     // junction, ending and mixing signs (stage 4c)
    inline const juce::Colour laneEven     { 0xff16171a };
    inline const juce::Colour laneOdd      { 0xff18191d };

    /** The track colours (muted, distinct hues). */
    juce::Colour trackColour (int index);
    constexpr int numTrackColours = 8;
    const char* trackColourName (int index);

    juce::Font font (float height, bool bold = false);
    juce::Font mono (float height);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool highlighted, bool down) override;
    };
}
