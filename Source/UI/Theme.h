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
    inline const juce::Colour nodes        { 0xffb9a8e6 };     // nodes on folders (4d.2): a soft purple
    inline const juce::Colour nodesFlagged { 0xffe8a3bd };     // a node out of balance, or a voice that does not change
    inline const juce::Colour laneEven     { 0xff16171a };
    inline const juce::Colour laneOdd      { 0xff18191d };

    /** The track colours (muted, distinct hues). */
    juce::Colour trackColour (int index);
    constexpr int numTrackColours = 8;
    const char* trackColourName (int index);
    /** Named marks' colours (on the ruler and through the tracks): 0 is the amber of the rulers. */
    juce::Colour markColour (int index);
    constexpr int numMarkColours = 9;
    const char* markColourName (int index);

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

    /** Text in capitals for headings. JUCE's toUpperCase leaves accented
        letters small on some systems ("éCRITURE"), so accents are dropped
        first, as is usual in French capitals on screen: "ECRITURE". */
    inline juce::String capitals (const juce::String& input)
    {
        static const char* const from[] = { "\xc3\xa0", "\xc3\xa2", "\xc3\xa4", "\xc3\xa1", "\xc3\xa3", "\xc3\xa9", "\xc3\xa8", "\xc3\xaa", "\xc3\xab",
                                            "\xc3\xae", "\xc3\xaf", "\xc3\xad", "\xc3\xac", "\xc3\xb4", "\xc3\xb6", "\xc3\xb3", "\xc3\xb2", "\xc3\xb5",
                                            "\xc3\xb9", "\xc3\xbb", "\xc3\xbc", "\xc3\xba", "\xc3\xa7", "\xc3\xb1", "\xc5\x93", "\xc3\xa6",
                                            "\xc3\x80", "\xc3\x82", "\xc3\x89", "\xc3\x88", "\xc3\x8a", "\xc3\x8e", "\xc3\x94", "\xc3\x99", "\xc3\x9b", "\xc3\x87" };
        static const char* const to[]   = { "a", "a", "a", "a", "a", "e", "e", "e", "e",
                                            "i", "i", "i", "i", "o", "o", "o", "o", "o",
                                            "u", "u", "u", "u", "c", "n", "oe", "ae",
                                            "A", "A", "E", "E", "E", "I", "O", "U", "U", "C" };
        auto s = input;
        for (size_t i = 0; i < sizeof (from) / sizeof (from[0]); ++i)
            s = s.replace (juce::String::fromUTF8 (from[i]), to[i]);
        return s.toUpperCase();
    }
}
