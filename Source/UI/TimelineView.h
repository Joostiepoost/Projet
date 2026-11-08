// TimelineView.h — the timeline: a time ruler with a proportion strip below
// it, track headers (with folders) and lanes with boxes.
//
// Mouse:
//   time ruler         click or drag to move the playhead
//   proportion strip   drag to make a span (it becomes the frame); drag the
//                      frame's edges; click the strip's name for the frame,
//                      the piece length and the divisions
//   track header       drag to reorder or into a folder; click a folder's
//                      triangle to fold it
//   box body           select (Shift adds, Cmd toggles); drag to move all
//                      selected boxes (also to other tracks)
//   empty lane         drag a rectangle to select; double-click: new text box
//   double-click box   write its notes
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
#include "../Model/Proportion.h"

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
    /** The selected box when exactly one is selected. */
    juce::ValueTree getSelectedBox();
    juce::Array<juce::ValueTree> getSelectedBoxes();
    void selectBox (const juce::ValueTree& box);
    void setSelection (const juce::Array<juce::ValueTree>& boxes);
    void selectAll();
    juce::ValueTree getSelectedTrack();
    /** The selected track if it can hold boxes, else the nearest one that can. */
    juce::ValueTree getSelectedLaneTrack();

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
    std::function<void (juce::ValueTree track, double seconds)> onAddTextBox;
    std::function<void (juce::ValueTree box)> onEditNotes, onSplitAtPlayhead, onOpenNotesWindow;
    std::function<void (const juce::String& message, bool warning)> onStatus;

    static constexpr int headerWidth = 172;
    static constexpr int timeRulerHeight = 30;
    static constexpr int stripHeight = 20;
    static constexpr int rulerHeight = timeRulerHeight + stripHeight;   // where the tracks begin
    static constexpr int folderHeight = 30;
    static constexpr int addTrackHeight = 34;

private:
    enum class DragMode { none, locate, move, trimStart, trimEnd, fadeIn, fadeOut, lasso,
                          span, frameStart, frameEnd, track };
    enum class Zone { none, body, startEdge, endEdge, fadeInHandle, fadeOutHandle };

    struct TrackRow { juce::ValueTree track; int y = 0, h = 0, depth = 0; bool folder = false; };
    std::vector<TrackRow> rows() const;
    int contentHeight() const;
    TrackRow rowAt (int y) const;

    double xToTime (float x) const noexcept         { return viewStart + (double) (x - (float) headerWidth) / pps; }
    float timeToX (double t) const noexcept         { return (float) headerWidth + (float) ((t - viewStart) * pps); }
    juce::Rectangle<float> boxBounds (const juce::ValueTree& box, const TrackRow& row) const;

    juce::ValueTree boxAt (juce::Point<float>, Zone& zone);
    double snap (double t, const juce::ValueTree& exclude, bool enabled) const;

    //== Proportions ==========================================================
    struct Tick { double time = 0.0; Proportion::Division division; float height = 0.0f; bool labelled = false; };
    /** The ruler frame's divisions that are far enough apart at this zoom. */
    std::vector<Tick> visibleTicks() const;
    juce::String readoutFor (const juce::ValueTree& box, double time, const juce::String& what) const;
    void showStripMenu();
    void askPieceLength();
    void askCustomDenominator();

    //== Folders ==============================================================
    juce::Rectangle<int> foldButton (const TrackRow& r) const;
    struct TrackDrop { int index = -1; juce::String folderId; int lineY = -1, indent = 0; juce::ValueTree into; bool valid = false; };
    TrackDrop trackDropAt (int y) const;

    void paintRuler (juce::Graphics&);
    void paintStrip (juce::Graphics&);
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

    void selectionChanged();
    bool isSelected (const juce::ValueTree& box) const   { return selectedBoxIds.contains (box[IDs::id].toString()); }

    juce::StringArray selectedBoxIds;
    juce::String selectedTrackId;

    DragMode dragMode = DragMode::none;
    juce::ValueTree dragBox;
    double dragStartTime = 0.0, dragOrigStart = 0.0, dragOrigEnd = 0.0, dragOrigFadeIn = 0.0, dragOrigFadeOut = 0.0;
    float dragGrabOffset = 0.0f;
    bool clickedSelectedBox = false;                 // re-select only it on mouse-up without a drag
    struct Moving { juce::ValueTree box; double start; int track; };
    std::vector<Moving> moving;                      // all selected boxes, as they were when the drag began
    juce::Rectangle<int> lasso;
    juce::StringArray lassoBase;                     // selection before the rectangle (Shift / Cmd)

    double spanAnchor = 0.0;
    int lastTrackDelta = 0;
    juce::ValueTree draggedTrack;
    TrackDrop trackDrop;
    juce::String readout;                            // shown near the mouse while moving or trimming
    juce::Point<int> readoutPos;
};
