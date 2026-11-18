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
    inline const juce::Colour magnifier     { 0xff5d82bd };      // the container magnified below (stage 6)
    inline const juce::Colour loop          { 0xff68c29f };      // the loop region (stage 10a)
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
        first, as is usual in French capitals on screen: "ECRITURE". Done
        letter by letter: String::replace matched one accented letter for
        another ("Mé-son" came out as "MA-SON"). */
    inline juce::String capitals (const juce::String& input)
    {
        juce::String s;
        for (auto p = input.getCharPointer(); ! p.isEmpty();)
        {
            const auto c = p.getAndAdvance();
            const char* plain = nullptr;
            switch (c)
            {
                case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: plain = "a"; break;
                case 0xe8: case 0xe9: case 0xea: case 0xeb:                       plain = "e"; break;
                case 0xec: case 0xed: case 0xee: case 0xef:                       plain = "i"; break;
                case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6:            plain = "o"; break;
                case 0xf9: case 0xfa: case 0xfb: case 0xfc:                       plain = "u"; break;
                case 0xe7: plain = "c"; break;
                case 0xf1: plain = "n"; break;
                case 0xe6: plain = "ae"; break;
                case 0x153: plain = "oe"; break;
                case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: plain = "A"; break;
                case 0xc8: case 0xc9: case 0xca: case 0xcb:                       plain = "E"; break;
                case 0xcc: case 0xcd: case 0xce: case 0xcf:                       plain = "I"; break;
                case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6:            plain = "O"; break;
                case 0xd9: case 0xda: case 0xdb: case 0xdc:                       plain = "U"; break;
                case 0xc7: plain = "C"; break;
                case 0x152: plain = "OE"; break;
                default: break;
            }
            if (plain != nullptr) s << plain;
            else                  s << juce::String::charToString (c);
        }
        return s.toUpperCase();
    }
}
