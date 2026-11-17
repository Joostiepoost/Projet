// MagnifierBar.cpp — see MagnifierBar.h.

#include "MagnifierBar.h"
#include "Theme.h"

MagnifierBar::MagnifierBar()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible (goInsideButton);
    addAndMakeVisible (closeButton);
    goInsideButton.setTooltip ("Go inside this container (double-click in it does the same); the magnifier closes");
    closeButton.setTooltip ("Close the magnifier (Shift-Cmd-Down toggles it)");
    goInsideButton.setMouseCursor (juce::MouseCursor::NormalCursor);
    closeButton.setMouseCursor (juce::MouseCursor::NormalCursor);
    goInsideButton.onClick = [this] { if (onGoInside) onGoInside(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    setTooltip ("Drag to give either timeline more room");
}

void MagnifierBar::setInfo (const juce::String& n, const juce::String& a)
{
    if (n == name && a == about) return;
    name = n;
    about = a;
    repaint();
}

void MagnifierBar::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    closeButton.setBounds (r.removeFromRight (24));
    r.removeFromRight (6);
    goInsideButton.setBounds (r.removeFromRight (82));
}

void MagnifierBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1d2129));
    g.setColour (Theme::magnifier);
    g.fillRect (0, 0, getWidth(), 2);
    g.setColour (Theme::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // A small grip at the top, in the middle, tells it can be dragged.
    g.setColour (Theme::textMuted.withAlpha (0.45f));
    const int gx = getWidth() / 2 - 10;
    g.fillRect (gx, 4, 20, 1);
    g.fillRect (gx, 6, 20, 1);

    int x = 12;
    g.setColour (Theme::magnifier);
    g.setFont (Theme::font (10.5f, true));
    g.drawText ("MAGNIFIED", x, 0, 80, getHeight(), juce::Justification::centredLeft, false);
    x += 76;
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.5f, true));
    const int nameW = juce::jmin (260, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 6);
    g.drawText (name, x, 0, nameW, getHeight(), juce::Justification::centredLeft, true);
    x += nameW + 8;
    g.setColour (Theme::textMuted);
    g.setFont (Theme::font (11.5f));
    g.drawText (about, x, 0, juce::jmax (0, goInsideButton.getX() - x - 8), getHeight(), juce::Justification::centredLeft, true);
}

void MagnifierBar::mouseDown (const juce::MouseEvent&)    { if (onDragStart) onDragStart(); }
void MagnifierBar::mouseDrag (const juce::MouseEvent& e)  { if (onDrag) onDrag (e.getDistanceFromDragStartY()); }
void MagnifierBar::mouseUp (const juce::MouseEvent&)      { if (onDragEnd) onDragEnd(); }
