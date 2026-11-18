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
#include "FactorCurves.h"
#include <map>
#include "../Model/Project.h"
#include "../Engine/SoundData.h"
#include "../Model/Proportion.h"
#include "../Model/Framework.h"
#include "../Model/Glyphs.h"
#include "../Model/Lanes.h"

class TimelineView : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     public juce::DragAndDropTarget,          // a sound card from a board (9b)
                     public juce::TooltipClient,
                     private juce::ScrollBar::Listener,
                     private juce::AsyncUpdater
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
    double getViewEnd() const noexcept              { return xToTime ((float) getWidth()); }
    void setView (double start, double pixelsPerSecond);
    void zoomBy (double factor, double aroundSeconds);
    void zoomToFit();
    /** Scrolls so the playhead stays visible while playing. */
    void followPlayhead (double seconds);

    /** Called by the owner at the display rate (piece seconds; drawn where
        it falls in the level shown). */
    void setPlayhead (double seconds);

    //== Levels (stage 5b, 6) =================================================
    /** Shows a level (invalid: the piece; or a container). Each level keeps
        its own view; the first visit to a container fits its window. Going
        up selects the container you came from. Everything the view draws or
        does happens in its own level, whichever level the project is
        working on (stage 6: two views). */
    void setLevel (juce::ValueTree level);
    juce::ValueTree getLevel() const                { return level; }
    /** The container magnified in the other view, highlighted here. */
    void setMagnified (const juce::ValueTree& container)    { magnified = container; repaint(); }
    /** With two views: the one worked in has a mark along its top. */
    void setActivePane (bool isActive)              { if (isActive != activePane) { activePane = isActive; repaint(); } }
    /** The view is clicked: its level becomes the one worked on. */
    std::function<void()> onActivate;
    /** Magnify a container below (stage 6). */
    std::function<void (juce::ValueTree container)> onMagnify;
    /** The piece's own view, wherever the focus is (for saving). */
    double getPieceViewStart() const;
    double getPiecePixelsPerSecond() const;
    /** The graphic score (10b): drawn for paper, without the playhead,
        hints and highlights of the screen. */
    void setPrinting (bool on)                      { printing = on; }
    /** Everything the view draws from top to bottom: the ruler and every track. */
    int printHeight() const;
    /** Forget every level's view (another project). */
    void resetLevels()                              { levelViews.clear(); levelId = {}; level = {}; magnified = {}; }

    //== Callbacks ============================================================
    std::function<void (double seconds)> onLocate;
    /** The loop (10a): 0 turn it on or off, 1 loop the selected boxes, 2 remove it. */
    std::function<void (int action)> onLoopAction;
    std::function<void()> onSelectionChanged;
    std::function<void (const juce::StringArray& files, juce::ValueTree track, double seconds)> onFilesDropped;
    std::function<void (juce::ValueTree box)> onRevealOriginal;
    /** Takes (stage 7): sound files to choose as new takes of the box; files dropped with Alt on a box. */
    std::function<void (juce::ValueTree box)> onAddSoundTakes;
    std::function<void (const juce::StringArray& files, juce::ValueTree box)> onFilesDroppedAsTakes;
    std::function<void()> onToggleShowTakes;
    /** Stage 8: the factor whose curve is drawn over the selected box ("" "": none). */
    void setFactorDrawing (const juce::String& categoryKey, const juce::String& factorId)
                                                    { factorKey = categoryKey; factorId_ = factorId; repaint(); }
    std::function<void (juce::ValueTree track, double seconds)> onAddTextBox;
    std::function<void (juce::ValueTree box)> onEditNotes, onSplitAtPlayhead, onOpenNotesWindow;
    /** Go inside a container (stage 5b). */
    std::function<void (juce::ValueTree container)> onDiveIn;
    /** Open a board box's board below (stage 9). */
    std::function<void (juce::ValueTree box)> onOpenBoard;
    /** The project folder, for the images on boards shown on their boxes. */
    void setProjectFolder (const juce::File& f)     { projectFolderRoot = f; }
    /** A sound card dropped on a track (9b): the board box, the card, the track, the start here. */
    std::function<void (const juce::String& boardBoxId, const juce::String& cardId, juce::ValueTree track, double start)> onDropSoundCard;
    /** A gesture dropped from a board (9c): a text box with its energy and curves. */
    std::function<void (const juce::String& boardBoxId, const juce::String& gestureId, juce::ValueTree track, double start, double length)> onDropGesture;
    /** Brings a box into view (its start, its track). */
    void revealBox (const juce::ValueTree& box);
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    /** The board box whose board is open below, marked like the magnified container. */
    void setOpenBoard (const juce::ValueTree& b)    { if (b != openBoard) { openBoard = b; repaint(); } }
    std::function<void (const juce::String& message, bool warning)> onStatus;
    /** Show the ruler layers in the panel (a layer id, or "" for the active one). */
    std::function<void (const juce::String& layerId)> onShowRulerPanel;
    /** The highlight changed from the timeline itself (a click in a lane). */
    std::function<void()> onHighlightChanged;

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
    void setHighlight (const Highlight& h)          { highlight = h; repaint(); }
    bool matchesHighlight (const juce::ValueTree& box);
    /** The mark a category gets on the boxes: the project's display setting,
        else the package's. */
    Framework::Mark markFor (const Framework::Category&) const;

    /** Asks for parts ("2:3:5:8") and splits the frame into marks, or the box into parts. */
    void askRatioSplit (const juce::String& frameRef, juce::ValueTree boxToCut);
    /** Makes sure a proportion layer framed by `ref` is shown (adding one
        below the others if needed); true if one was added. */
    bool showFrameLayer (const juce::String& ref);
    static constexpr int addTrackHeight = 34;

private:
    enum class DragMode { none, locate, move, trimStart, trimEnd, fadeIn, fadeOut, lasso,
                          span, frameStart, frameEnd, track, layerHeight, mark,
                          drawSpan, itemMove, itemStart, itemEnd, itemGoal, itemGoalEnd,   // drawn lanes (4d.2)
                          drawNode, nodeMove, nodeStart, nodeEnd,
                          trackHeight,                                                       // a track's lower edge (after stage 7)
                          factorCurve,                                                       // a factor's curve over a box (stage 8)
                          linkOut,                                                           // a box Alt-dragged onto a board (9b)
                          loopDraw, loopStart, loopEnd, loopMove };                          // the loop region (10a)
    enum class Zone { none, body, startEdge, endEdge, fadeInHandle, fadeOutHandle };

    struct TrackRow { juce::ValueTree track; int y = 0, h = 0, depth = 0; bool folder = false; };
    /** The track (not a folder) whose lower edge is at y, if any: drag it to change the height. */
    juce::ValueTree trackEdgeAt (int y) const;
    static constexpr int minTrackHeight = 40, maxTrackHeight = 480, defaultTrackHeight = 72;
    std::vector<TrackRow> rows() const;
    int contentHeight() const;
    TrackRow rowAt (int y) const;

    double xToTime (float x) const noexcept         { return viewStart + (double) (x - (float) headerWidth) / pps; }
    float timeToX (double t) const noexcept         { return (float) headerWidth + (float) ((t - viewStart) * pps); }
    juce::Rectangle<float> boxBounds (const juce::ValueTree& box, const TrackRow& row) const;

    juce::ValueTree boxAt (juce::Point<float>, Zone& zone);
    double snap (double t, const juce::ValueTree& exclude, bool enabled) const;

    //== Ruler layers (TimelineRuler.cpp) =====================================
    struct LayerRow { juce::ValueTree layer; int y = 0, h = 0; bool time = false, lane = false, section = false; };
    /** Inside a container (5c): a row at the bottom of the ruler for the
        container as a section: its writing type and time-feel, its nodes. */
    static constexpr int sectionHeight = 24;
    void paintSectionRow (juce::Graphics&, const LayerRow&);
    bool sectionMouseDown (const LayerRow&, const juce::MouseEvent&);
    void showSectionMenu (double t);
    std::vector<LayerRow> rulerRows() const;
    /** Where the tracks begin. */
    int rulerHeight() const;
    /** Vertical scrolling. The ruler stays at the top while it takes up to
        half the view, and the tracks scroll under it; taller (big lanes), the
        ruler and the tracks scroll together as one page. */
    bool rulerPinned() const                        { return rulerHeight() <= getHeight() / 2; }
    int rulerTop() const                            { return rulerPinned() ? 0 : -scrollY; }
    int rulerBottom() const                         { return rulerTop() + rulerHeight(); }
    int tracksTop() const                           { return juce::jmax (0, rulerBottom()); }
    int maxScroll() const;
    juce::ScrollBar vScroll { true };
    void updateScrollBar();
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void handleAsyncUpdate() override;
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
    void paintBoxIn (juce::Graphics&, const juce::ValueTree& box, const TrackRow&, juce::Rectangle<float> r);
    //== Takes (stage 7) ======================================================
    bool showingTakes() const                       { return (bool) project.getState().getProperty (IDs::showTakes, false); }
    /** With View > Show takes in boxes: a lane per take, in order (a take
        put aside as long as it is); empty when not stacked. */
    std::vector<juce::Rectangle<float>> takeLanes (const juce::ValueTree& box, juce::Rectangle<float> r) const;
    /** The badge (B/3) that opens the takes' list; stacked, the active lane's tag. */
    juce::Rectangle<float> takeBadgeArea (const juce::ValueTree& box, juce::Rectangle<float> r) const;
    void paintTakeTag (juce::Graphics&, juce::Rectangle<float> area, const juce::String& text, bool active);
    /** A take put aside, as a box to draw (cached until the project changes). */
    juce::ValueTree takeLaneView (const juce::ValueTree& box, const juce::ValueTree& take);
    std::map<juce::String, juce::ValueTree> takeViews;
    bool drawingTakeLanes = false;                  // no fade handles in the lanes
    int takeViewsCount = -1;
    struct TakeHit { juce::ValueTree box, take; bool badge = false; };
    TakeHit takeAt (juce::Point<float>);
    void showTakesMenu (juce::ValueTree box);
    static constexpr int takesMenuBase = 200000;
    //== Factor curves over a box (stage 8) ====================================
    juce::String factorKey, factorId_;
    FactorCurves::Edit factorEdit;
    juce::ValueTree factorBox;
    bool factorStarted = false;
    /** The area over the selected box where the armed factor's curve is drawn (empty: none); the box in `box`. */
    juce::Rectangle<float> factorArea (juce::ValueTree& box);
    void paintFactorOverlay (juce::Graphics&);
    void setFactorFromEdit();
    void paintFades (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float>);
    void paintFadeHandles (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float>);
    /** A board box (stage 9): its name, and the part of its board chosen to show, or what it holds. */
    void paintBoardBox (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float>, float alpha,
                        juce::Colour base, bool selected, bool emphasis);
    juce::File projectFolderRoot;
    juce::ValueTree openBoard;
    // A sound card dragged over a track (9b)
    juce::String dropTrackId;
    double dropStart = 0.0, dropLength = 0.0;
    bool dropAt (const SourceDetails&, juce::ValueTree& track, double& start, double& length) const;
    /** The temporal outs of a board box, as a strip along its bottom (9b). */
    void paintOutsStrip (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> strip);
    /** A container (stage 5a): header, miniature of the inside, fades, double edge. */
    void paintContainer (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float>, float alpha,
                         juce::Colour base, bool selected, bool emphasis);
    /** The container's inner tracks as thin rows and their boxes where they sound. */
    void paintMiniature (juce::Graphics&, const juce::ValueTree& container, juce::Rectangle<float>, float alpha, int depth);
    void paintWaveform (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> r);
    /** The colour of the box in the current colour mode (track colour, or a category's colours). */
    juce::Colour boxColour (const juce::ValueTree& box, const TrackRow& row);
    /** Each category's mark: strips along the bottom, badges at the bottom right, labels at the bottom left. */
    void paintFrameworkMarks (juce::Graphics&, const juce::ValueTree& box, juce::Rectangle<float> r, float alpha);
    /** Seam signs (junctions, with the mixing relation beside them and the
        ending of the box before on their bracket) and end signs (stage 4c). */
    static juce::Path fadeGaps (float endX, float endY, float length);
    /** "from the timing", "from the folder's writing type" ... */
    juce::String describeSource (const Project::FrameworkValue&) const;
    void paintSeams (juce::Graphics&, const TrackRow&);

    //== Lanes (stage 4d) =====================================================
public:
    /** The lanes the loaded packages declare, as a submenu (ids base + index). */
    juce::PopupMenu lanesMenu (int base) const;
    /** Adds a lane layer for the package lane at `laneIndex` (of getLanes()). */
    juce::ValueTree addLane (int laneIndex, int layerIndex = -1);
    /** A lane's height when first added: enough for its rows. */
    static int laneHeight (const Framework&, const Framework::Lane&);
private:
    struct LaneCache { int changes = -1, generation = -1; std::vector<Lanes::Row> rows; Lanes::Scale scale; };
    std::map<juce::String, LaneCache> laneCache;            // by lane key
    const LaneCache& laneData (const Framework::Lane&);
    void paintLaneLayer (juce::Graphics&, const LayerRow&);
    void paintFactorLane (juce::Graphics&, const LayerRow&, const Framework::Lane&, const juce::String& factorId, int nameW);   // stage 8
    /** Clicking a lane highlights what it shows there (a family, a zone, a level). */
    void laneClicked (const LayerRow&, const juce::MouseEvent&);

    //== Drawn lanes and nodes (stage 4d.2: TimelineDrawn.cpp) =================
public:
    /** The selected drawn span or point, or node (one at a time; selecting a box clears it). */
    juce::ValueTree getSelectedItem();
    juce::ValueTree getSelectedNode();
    bool hasDrawnSelection()                        { return getSelectedItem().isValid() || getSelectedNode().isValid(); }
    /** Deletes the selected span, point or node (undoable). */
    void deleteDrawnSelection();
    /** What a node says: its title and the panel's text. */
    std::pair<juce::String, juce::String> describeNode (const juce::ValueTree& node);
private:
    juce::String selectedItemId, selectedNodeId;
    juce::ValueTree dragItem, dragNode, drawLayer, drawFolder;
    double dragFrom = 0.0, dragTo = 0.0, drawStart = 0.0, drawEnd = 0.0;
    struct DrawnGeometry { float pointsTop = 0, pointsH = 0, rowsTop = 0, rowH = 0, bracketTop = 0; int rows = 1; bool brackets = false; };
    DrawnGeometry drawnGeometry (const LayerRow&, int rows) const;
    juce::Rectangle<float> spanRect (const DrawnGeometry&, const juce::ValueTree& span, int row) const;
    void paintDrawnLane (juce::Graphics&, const LayerRow&, const Framework::Lane&);
    void paintSpan (juce::Graphics&, const Framework::Lane&, const juce::ValueTree& span, juce::Rectangle<float> r, bool selected);
    bool drawnMouseDown (const LayerRow&, const Framework::Lane&, const juce::MouseEvent&);
    bool drawnMouseDrag (const juce::MouseEvent&);
    bool drawnMouseUp (const juce::MouseEvent&);
    void showItemMenu (juce::ValueTree item, const Framework::Lane&);
    void showNewSpanMenu (const Framework::Lane&, double a, double b);
    void showNewPointMenu (const Framework::Lane&, double t);
    void selectDrawn (const juce::String& itemId, const juce::String& nodeId);
    // Nodes
    bool isInside (const juce::ValueTree& track, const juce::ValueTree& folder);
    void paintNodes (juce::Graphics&);
    bool nodeMouseDown (const TrackRow&, const juce::MouseEvent&);
    void showNodeMenu (juce::ValueTree node, const juce::ValueTree& folder, double t);
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
    double playhead = 0.0;              // in the level shown
    double playheadPiece = 0.0;         // in the piece
    // Each level's view (stage 5b): container id ("" for the piece) -> start, pixels per second.
    std::map<juce::String, std::pair<double, double>> levelViews;
    juce::String levelId;
    juce::ValueTree level, magnified;
    bool activePane = false;
    bool printing = false;
    void paintWindowEdges (juce::Graphics&);
    bool dropHighlight = false;

    void selectionChanged();
    bool isSelected (const juce::ValueTree& box) const   { return selectedBoxIds.contains (box[IDs::id].toString()); }

    juce::StringArray selectedBoxIds;
    juce::String selectedTrackId;

    DragMode dragMode = DragMode::none;

    // The loop region (10a), in the piece's time

    juce::Range<double> loopWas;

    double loopAnchor = 0.0;

    /** The loop's ends on screen (this view's level), or empty. */

    juce::Range<float> loopOnScreen() const;

    /** -1 near the loop's start, 1 near its end, 0 elsewhere. */

    int loopEdgeAt (float x) const;

    void paintLoop (juce::Graphics&);
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
