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
    juce::String name, about;
    juce::TextButton goInsideButton { "Go inside" };
    juce::TextButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
};
