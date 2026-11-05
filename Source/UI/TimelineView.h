// TimelineView.h — the flat timeline of stage 1: a ruler, track headers and
// lanes with sound boxes.
//
// Mouse:
//   ruler              click or drag to move the playhead
//   box body           select; drag to move (also to another track)
//   box edges          drag to trim (the sound stays in place)
//   top corners        drag the small squares to set the fade lengths
//   track header       M / S buttons; double-click the name to rename;
//                      right-click for colour, add and delete
//   wheel              scroll (Shift: sideways); Cmd-wheel or pinch: zoom
// Moving and trimming snap to box edges and the playhead; hold Shift for
// free movement. Sounds dropped from the Finder are imported where they land.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Engine/SoundData.h"

class TimelineView : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    TimelineView (Project&, projet::SoundPool&);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override       { dropHighlight = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override                  { dropHighlight = false; repaint(); }
    void filesDropped (const juce::StringArray&, int x, int y) override;

    //== Selection and view ===================================================
    juce::ValueTree getSelectedBox();
    void selectBox (const juce::ValueTree& box);
    juce::ValueTree getSelectedTrack();

    double getViewStart() const noexcept            { return viewStart; }
    double getPixelsPerSecond() const noexcept      { return pps; }
    void setView (double start, double pixelsPerSecond);
    void zoomBy (double factor, double aroundSeconds);
    void zoomToFit();
    /** Scrolls so the playhead stays visible while playing. */
    void followPlayhead (double seconds);

    /** Called by the owner at the display rate. */
    void setPlayhead (double seconds);

    //== Callbacks ============================================================
    std::function<void (double seconds)> onLocate;
    std::function<void()> onSelectionChanged;
    std::function<void (const juce::StringArray& files, juce::ValueTree track, double seconds)> onFilesDropped;
    std::function<void (juce::ValueTree box)> onRevealOriginal;

    static constexpr int headerWidth = 172;
    static constexpr int rulerHeight = 30;
    static constexpr int addTrackHeight = 34;

private:
    enum class DragMode { none, locate, move, trimStart, trimEnd, fadeIn, fadeOut };
    enum class Zone { none, body, startEdge, endEdge, fadeInHandle, fadeOutHandle };

    struct TrackRow { juce::ValueTree track; int y = 0, h = 0; };
    std::vector<TrackRow> rows() const;
    int contentHeight() const;
    TrackRow rowAt (int y) const;

    double xToTime (float x) const noexcept         { return viewStart + (double) (x - (float) headerWidth) / pps; }
    float timeToX (double t) const noexcept         { return (float) headerWidth + (float) ((t - viewStart) * pps); }
    juce::Rectangle<float> boxBounds (const juce::ValueTree& box, const TrackRow& row) const;

    juce::ValueTree boxAt (juce::Point<float>, Zone& zone);
    double snap (double t, const juce::ValueTree& exclude, bool enabled) const;

    void paintRuler (juce::Graphics&);
    void paintTrackHeader (juce::Graphics&, const TrackRow&, int index);
    void paintBox (juce::Graphics&, const juce::ValueTree& box, const TrackRow&);
    void paintWaveform (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> r);

    juce::Rectangle<int> muteButton (const TrackRow& r) const;
    juce::Rectangle<int> soloButton (const TrackRow& r) const;
    juce::Rectangle<int> addTrackButton() const;

    void showTrackMenu (juce::ValueTree track);
    void showBoxMenu (juce::ValueTree box);
    void renameTrack (juce::ValueTree track);

    Project& project;
    projet::SoundPool& pool;

    double viewStart = 0.0, pps = 20.0;
    int scrollY = 0;
    double playhead = 0.0;
    bool dropHighlight = false;

    juce::String selectedBoxId, selectedTrackId;

    DragMode dragMode = DragMode::none;
    juce::ValueTree dragBox;
    double dragStartTime = 0.0, dragOrigStart = 0.0, dragOrigEnd = 0.0, dragOrigFadeIn = 0.0, dragOrigFadeOut = 0.0;
    float dragGrabOffset = 0.0f;
};
