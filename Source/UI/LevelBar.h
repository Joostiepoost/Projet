// LevelBar.h — above the timeline while inside a container (stage 5b): the
// breadcrumbs (Piece > Section A > Phrase 2), the depth, and a context strip
// showing the level around, with the container you are in highlighted.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"

class LevelBar : public juce::Component
{
public:
    explicit LevelBar (Project&);

    /** The main timeline's view (in the focus' time), shown as a bracket. */
    void setView (double start, double end);
    /** Piece seconds. */
    void setPlayhead (double pieceSeconds);

    /** Go to a level (invalid: the piece). */
    std::function<void (juce::ValueTree container)> onGoTo;
    /** A box of the level around was clicked: go up and select it. */
    std::function<void (juce::ValueTree box)> onSelectAround;

    static constexpr int crumbsHeight = 24, stripHeight = 38;
    static constexpr int preferredHeight = crumbsHeight + stripHeight;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    Project& project;
    struct Crumb { juce::Rectangle<int> area; juce::ValueTree target; };
    std::vector<Crumb> crumbs;
    struct Hit { juce::Rectangle<float> area; juce::ValueTree box; };
    std::vector<Hit> hits;
    double viewStart = 0.0, viewEnd = 0.0, playheadPiece = 0.0;

    /** The level around the focus: its tracks, and the time range drawn. */
    juce::ValueTree aroundTracks() const;
    juce::Range<double> aroundRange();
    float toX (double t, juce::Range<double> range) const;
};
