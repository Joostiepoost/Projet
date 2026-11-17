// MagnifierBar.h — the bar above the magnified container (stage 6): its
// name, depth, place in the piece and magnification, Go inside and Close.
// The bar is also the divider between the two timelines: drag it up or
// down to give either more room.

#pragma once

#include <JuceHeader.h>

class MagnifierBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    MagnifierBar();

    /** The container's name, and the rest of the line (depth, place, scale). */
    void setInfo (const juce::String& name, const juce::String& about);
    /** The bar of an open board (stage 9): its heading, and the button's words and tips. */
    void setHeading (const juce::String& heading)  { tag = heading; repaint(); }
    void setButton (const juce::String& text, const juce::String& tip)
                                                    { goInsideButton.setButtonText (text); goInsideButton.setTooltip (tip); }
    void setCloseTip (const juce::String& tip)      { closeButton.setTooltip (tip); }

    std::function<void()> onGoInside, onClose;
    /** Dragging the bar: the distance from where the drag began (pixels, down positive). */
    std::function<void (int dy)> onDrag;
    std::function<void()> onDragStart, onDragEnd;

    static constexpr int preferredHeight = 28;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String name, about, tag { "MAGNIFIED" };
    juce::TextButton goInsideButton { "Go inside" };
    juce::TextButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
};
