// TimelineView.h — the timeline: a time ruler with a proportion strip below
// it, track headers (with folders) and lanes with boxes.
//
// Mouse:
//   ruler layers       a stack (stage 3b): the time layer moves the playhead;
//                      a proportion layer: drag to make a span (it becomes its
//                      frame), drag the frame's edges, double-click for a mark,
//                      drag a mark; click a layer's name for its menu; drag a
//                      layer's lower edge (in the name column) to resize it
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
#include <map>
#include "../Model/Project.h"
#include "../Engine/SoundData.h"
#include "../Model/Proportion.h"
#include "../Model/Framework.h"
#include "../Model/Glyphs.h"

class TimelineView : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     public juce::TooltipClient
{
public:
    TimelineView (Project&, projet::SoundPool&, const Framework&);

    /** After the framework's packages change: reloads the glyph scripts,
        clears the drawing caches and repaints. */
    void frameworkReloaded();
    /** Glyph scripts that could not be loaded, one line each ("<package>: why"). */
    const juce::StringArray& glyphProblems() const      { return glyphs.getProblems(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    /** Over a seam or end sign: what it says (stage 4c). */
    juce::String getTooltip() override;

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
    /** Show the ruler layers in the panel (a layer id, or "" for the active one). */
    std::function<void (const juce::String& layerId)> onShowRulerPanel;

    static constexpr int headerWidth = 172;
    static constexpr int folderHeight = 30;

    //== Framework (stage 4a, 4b) =============================================
    /** Boxes that match are drawn as usual, the others faded. Keys: a
        category's key (its values, also for the categories sharing them) or
        key + "#groups" (its groups: families, zones). Within a key any ticked
        id matches; across keys all must. */
    struct Highlight
    {
        juce::StringPairArray ticked;     // key -> ids joined by ","
        bool isActive() const;
        juce::StringArray idsFor (const juce::String& key) const  { return Tags::parse (ticked[key]); }
        juce::String describe (const Framework&) const;
    };
    void toggleHighlight (const juce::String& key, const juce::String& id);
    void clearHighlight()                           { highlight = {}; repaint(); }
    const Highlight& getHighlight() const noexcept  { return highlight; }
    bool matchesHighlight (const juce::ValueTree& box);
    /** The mark a category gets on the boxes: the project's display setting,
        else the package's. */
    Framework::Mark markFor (const Framework::Category&) const;

    /** Asks for parts ("2:3:5:8") and splits the frame into marks, or the box into parts. */
    void askRatioSplit (const juce::String& frameRef, juce::ValueTree boxToCut);
    static constexpr int addTrackHeight = 34;

private:
    enum class DragMode { none, locate, move, trimStart, trimEnd, fadeIn, fadeOut, lasso,
                          span, frameStart, frameEnd, track, layerHeight, mark };
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

    //== Ruler layers (TimelineRuler.cpp) =====================================
    struct LayerRow { juce::ValueTree layer; int y = 0, h = 0; bool time = false; };
    std::vector<LayerRow> rulerRows() const;
    /** Where the tracks begin. */
    int rulerHeight() const;
    LayerRow layerAt (int y) const;

    struct Tick { double time = 0.0; Proportion::Division division; float height = 0.0f; bool labelled = false;
                  bool series = false; juce::String seriesLabel; };
    /** A layer's divisions that are far enough apart at this zoom, then its series. */
    std::vector<Tick> visibleTicks (const juce::ValueTree& layer) const;
    juce::ValueTree markAt (const LayerRow& row, float x) const;
    juce::String readoutFor (const juce::ValueTree& box, double time, const juce::String& what) const;

    void paintRuler (juce::Graphics&);
    void paintTimeLayer (juce::Graphics&, const LayerRow&);
    void paintProportionLayer (juce::Graphics&, const LayerRow&);
    void paintMarksInLanes (juce::Graphics&);
    bool rulerMouseDown (const juce::MouseEvent&);
    bool rulerMouseDrag (const juce::MouseEvent&);
    bool rulerMouseMove (const juce::MouseEvent&);
    bool rulerDoubleClick (const juce::MouseEvent&);
    void showLayerMenu (juce::ValueTree layer);
    void showMarkMenu (juce::ValueTree mark);
    void askPieceLength();
    void askCustomDenominator (juce::ValueTree layer);
    void askMarkName (juce::ValueTree mark);
    void askMarkExpression (juce::ValueTree mark);

    //== Folders ==============================================================
    juce::Rectangle<int> foldButton (const TrackRow& r) const;
    struct TrackDrop { int index = -1; juce::String folderId; int lineY = -1, indent = 0; juce::ValueTree into; bool valid = false; };
    TrackDrop trackDropAt (int y) const;


    void paintTrackHeader (juce::Graphics&, const TrackRow&, int index);
    void paintBox (juce::Graphics&, const juce::ValueTree& box, const TrackRow&);
    void paintWaveform (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> r);
    /** The colour of the box in the current colour mode (track colour, or a category's colours). */
    juce::Colour boxColour (const juce::ValueTree& box, const TrackRow& row);
    /** Each category's mark: strips along the bottom, badges at the bottom right, labels at the bottom left. */
    void paintFrameworkMarks (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> r, float alpha);
    /** Seam signs (junctions, with the mixing relation beside them and the
        ending of the box before on their bracket) and end signs (stage 4c). */
    static juce::Path fadeGaps (float endX, float endY, float length);
    void paintSeams (juce::Graphics&, const TrackRow&);
    /** A package's SVG path, parsed once. */
    const juce::Path& signPath (const juce::String& svg);
    std::map<juce::String, juce::Path> signCache;
    struct SignHit
    {
        juce::Rectangle<float> area;
        juce::String boxId, tip;
        bool seam = true;                       // a seam (the later box's join); else an end sign
    };
    std::vector<SignHit> signHits;
    const SignHit* signAt (juce::Point<float>) const;
    /** The menu of a sign: the joins of the later box, or the box's ending. */
    void showSignMenu (const SignHit&);
    /** "gap 1.20 s", "overlap 30 ms" */
    static juce::String describeGap (double gap);
    /** The fill pattern of the box's first filled value (hatched, dotted, outline). */
    void paintFrameworkFill (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> r, float alpha);
    /** The energy glyphs in the band above the strips, shaped by the profile. */
    void paintGlyphs (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> r, float alpha, float bandBottom);
    /** How a box's weighted value (the role) changes its drawing. */
    struct Weight { float alphaFactor = 1.0f; bool emphasis = false; };
    Weight weightFor (const juce::ValueTree& box) const;
    /** The framework submenus of the box menu (ids from boxMenuBase); `only`
        keeps some categories (the sign menus). */
    void addFrameworkMenus (juce::PopupMenu&, const juce::Array<juce::ValueTree>& boxes,
                            std::function<bool (const Framework::Category&)> only = {});
    void applyFrameworkMenu (int result, const juce::Array<juce::ValueTree>& boxes);
    static constexpr int boxMenuBase = 20000, folderMenuBase = 100000, menuStride = 500;

    juce::Rectangle<int> muteButton (const TrackRow& r) const;
    juce::Rectangle<int> soloButton (const TrackRow& r) const;
    juce::Rectangle<int> addTrackButton() const;

    void showTrackMenu (juce::ValueTree track);
    void showBoxMenu (juce::ValueTree box);
    void renameTrack (juce::ValueTree track);

    Project& project;
    projet::SoundPool& pool;
    const Framework& framework;
    Highlight highlight;
    juce::String displayCache;                  // the project's fwDisplay, as last parsed
    juce::StringPairArray displayMarks;
    Glyphs::Set glyphs;                         // the packages' drawing scripts (4b.2)
    struct CachedGlyph                          // one glyph at one size, as paths
    {
        juce::Array<juce::Path> paths;
        juce::Array<float> alphas, widths;
        juce::Array<bool> fills;
    };
    std::map<juce::String, CachedGlyph> glyphCache;

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
    juce::ValueTree dragLayer, dragMark;
    int dragLayerStartHeight = 0;
    mutable juce::String snappedMark;                // the mark the last snap landed on ("" if none)
    int lastTrackDelta = 0;
    int newTracksBelow = 0, newTrackDelta = 0;                          // a move dropped below the last track makes this many tracks
    juce::ValueTree draggedTrack;
    TrackDrop trackDrop;
    juce::String readout;                            // shown near the mouse while moving or trimming
    juce::Point<int> readoutPos;
};
