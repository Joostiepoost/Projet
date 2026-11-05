// Theme.cpp

#include "Theme.h"

namespace Theme
{

juce::Colour trackColour (int index)
{
    static const juce::uint32 palette[numTrackColours] = { 0xff5d82bd, 0xff4ea597, 0xffc49a4a, 0xffb9707e,
                                                           0xff7f77dd, 0xff7fae6c, 0xffd08a5a, 0xff7d7f86 };
    return juce::Colour (palette[((index % numTrackColours) + numTrackColours) % numTrackColours]);
}

const char* trackColourName (int index)
{
    static const char* names[numTrackColours] = { "Blue", "Teal", "Ochre", "Rose", "Violet", "Green", "Copper", "Grey" };
    return names[((index % numTrackColours) + numTrackColours) % numTrackColours];
}

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font mono (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

LookAndFeel::LookAndFeel()
{
    auto scheme = juce::LookAndFeel_V4::getDarkColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, background);
    scheme.setUIColour (ColourScheme::widgetBackground, panel2);
    scheme.setUIColour (ColourScheme::menuBackground, panel);
    scheme.setUIColour (ColourScheme::outline, line);
    scheme.setUIColour (ColourScheme::defaultText, text);
    scheme.setUIColour (ColourScheme::defaultFill, lineStrong);
    scheme.setUIColour (ColourScheme::highlightedText, background);
    scheme.setUIColour (ColourScheme::highlightedFill, accent);
    scheme.setUIColour (ColourScheme::menuText, text);
    setColourScheme (scheme);

    setColour (juce::TextButton::buttonColourId, panel2);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::TextEditor::backgroundColourId, panel2);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::Slider::backgroundColourId, panel2);
    setColour (juce::Slider::trackColourId, juce::Colour (0xff5d82bd));
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, line);
    setColour (juce::Slider::textBoxBackgroundColourId, panel2);
    setColour (juce::Label::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, background);
    setColour (juce::ScrollBar::thumbColourId, lineStrong);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (14.0f, (float) buttonHeight * 0.55f));
}

juce::Font LookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (13.0f);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return font (14.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                                        bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? findColour (juce::TextButton::buttonOnColourId) : backgroundColour;
    if (down) c = c.brighter (0.15f);
    else if (highlighted) c = c.brighter (0.07f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (b.getToggleState() ? c : line);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

} // namespace Theme
