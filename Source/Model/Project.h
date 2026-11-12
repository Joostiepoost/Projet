// Project.h — Projet's piece: sounds, tracks and boxes (a juce::ValueTree with
// undo). Saved as readable JSON in the project folder (see ProjectFolder.h).
//
// Boxes may overlap on a track (sounds blend into each other). Sound boxes
// and containers are drawn fainter the quieter they are (gainOpacity, 5a);
// a text box's opacity is visual only.

#pragma once

#include <JuceHeader.h>
#include <map>
#include "Identifiers.h"
#include "Proportion.h"
#include "Framework.h"

enum class FadeShape { linear = 0, equalPower, sCurve };
constexpr int numFadeShapes = 3;
const char* fadeShapeName (FadeShape);

class Project : private juce::ValueTree::Listener
{
public:
    static constexpr int currentFormatVersion = 1;
    static constexpr double defaultSampleRate = 48000.0;
    static constexpr double minBoxLength = 0.005;

    Project();
    ~Project() override;

    juce::ValueTree& getState() noexcept            { return state; }
    juce::UndoManager& getUndoManager() noexcept    { return undoManager; }
    juce::UndoManager* um() noexcept                { return &undoManager; }

    /** A fresh project with one empty track. */
    void resetToDefault (const juce::String& name, double sampleRate = defaultSampleRate);

    juce::String getName() const                    { return state[IDs::name].toString(); }
    double getSampleRate() const                    { return (double) state.getProperty (IDs::sampleRate, defaultSampleRate); }
    juce::String getPlaceholderFormat() const       { return state.getProperty (IDs::placeholderFormat, "ogg").toString(); }
    int getVorbisQuality() const                    { return (int) state.getProperty (IDs::vorbisQuality, 7); }

    //== Sounds ===============================================================
    juce::ValueTree getSounds()                     { return state.getChildWithName (IDs::SOUNDS); }
    juce::ValueTree findSound (const juce::String& soundId) const;
    /** An earlier import of the same file (same contents) in the same placeholder format. */
    juce::ValueTree findSoundByHash (const juce::String& hash, const juce::String& format) const;
    /** Adds a SOUND record (not undoable: the file exists on disk either way). */
    juce::ValueTree addSound (const juce::ValueTree& soundRecord);
    double soundSeconds (const juce::String& soundId) const;

    //== Tracks ===============================================================
    juce::ValueTree getTracks()                     { return state.getChildWithName (IDs::TRACKS); }
    int getNumTracks()                              { return getTracks().getNumChildren(); }
    juce::ValueTree getTrack (int index)            { return getTracks().getChild (index); }
    juce::ValueTree addTrack (const juce::String& name = {}, int index = -1, const juce::String& folderId = {});
    /** Removes a track; a folder goes with all its tracks. */
    void removeTrack (juce::ValueTree track);
    juce::ValueTree findTrack (const juce::String& trackId);

    //== Folders (stage 3a) ===================================================
    // The track list stays flat: a folder's tracks follow it directly and name
    // it in folderId. A folder has no boxes of its own.
    static bool isFolder (const juce::ValueTree& t)                { return (bool) t[IDs::isFolder]; }
    juce::ValueTree folderOf (const juce::ValueTree& track);
    int depthOf (const juce::ValueTree& track);
    /** True if `track` is inside `folder` (at any depth). */
    bool isInside (const juce::ValueTree& track, const juce::ValueTree& folder);
    /** False when a folder around the track is folded. */
    bool isVisible (const juce::ValueTree& track);
    /** The colour shown: the outermost folder's colour, else the track's own. */
    int effectiveColour (const juce::ValueTree& track);
    bool effectiveMute (const juce::ValueTree& track);
    bool effectiveSolo (const juce::ValueTree& track);
    bool anySolo()                                  { return anySolo (getTracks()); }
    /** True if a track or folder in this list (the piece's, or a container's) is soloed. */
    static bool anySolo (const juce::ValueTree& tracks);
    /** The tracks that can hold boxes (not folders), in order. */
    juce::Array<juce::ValueTree> laneTracks();
    /** Index just after the last track inside the folder (or after the track). */
    int blockEnd (const juce::ValueTree& track);

    juce::ValueTree addFolder (const juce::String& name, int index, const juce::String& folderId = {});
    /** One folder per value of a category folders can give (in the values'
        order), each holding one track and giving its value to the boxes on it:
        the semiotic track template (stage 4d). Undoable; needs the framework. */
    juce::Array<juce::ValueTree> addFoldersFor (const Framework::Category&);
    /** A new folder in the track's place, holding the track. */
    juce::ValueTree putInNewFolder (juce::ValueTree track);
    /** Moves the track (with everything inside it, if a folder) so it lands
        before the track now at `toIndex` (or at the end), inside `folderId`. */
    void moveTrack (juce::ValueTree track, int toIndex, const juce::String& folderId);
    /** The folder's tracks move up one level; the folder goes. */
    void ungroupFolder (juce::ValueTree folder);

    //== Boxes ================================================================
    /** A sound box for the whole sound, at `start` seconds. */
    juce::ValueTree addSoundBox (juce::ValueTree track, double start, const juce::String& soundId);
    /** A text box (notes only, silent). */
    juce::ValueTree addTextBox (juce::ValueTree track, double start, double length, const juce::String& name = "Text");

    static juce::String kindOf (const juce::ValueTree& b)          { return b.getProperty (IDs::kind, "sound").toString(); }
    static bool isSound (const juce::ValueTree& b)                 { return kindOf (b) == "sound"; }
    /** Changes the kind; everything else (notes, sound, fades) is kept for switching back. */
    void setKind (juce::ValueTree box, const juce::String& kind);
    /** Gives a sound box another sound (the whole sound, keeping start and fades). */
    void setSound (juce::ValueTree box, const juce::String& soundId);

    /** Cuts the box at `t` (seconds on the timeline); returns the right-hand part,
        or an invalid tree if `t` is not inside the box. */
    juce::ValueTree splitBox (juce::ValueTree box, double t);
    void removeBox (juce::ValueTree box);
    juce::ValueTree duplicateBox (juce::ValueTree box);
    void moveBox (juce::ValueTree box, juce::ValueTree toTrack, double newStart);
    juce::ValueTree findBox (const juce::String& boxId);
    /** Every box, track by track. */
    juce::Array<juce::ValueTree> allBoxes();
    juce::ValueTree trackOf (const juce::ValueTree& box) const     { return box.getParent(); }

    static double boxStart (const juce::ValueTree& b)              { return (double) b[IDs::start]; }
    static double boxLength (const juce::ValueTree& b)             { return (double) b[IDs::length]; }
    static double boxEnd (const juce::ValueTree& b)                { return boxStart (b) + boxLength (b); }

    /** Moves the box's left edge to `newStart` (the sound stays in place under it). */
    void trimStart (juce::ValueTree box, double newStart);
    /** Moves the box's right edge to `newEnd`. */
    void trimEnd (juce::ValueTree box, double newEnd);
    /** Fades are clamped so fade in + fade out never exceed the box. */
    void setFades (juce::ValueTree box, double fadeIn, double fadeOut);

    /** End of the last box (0 if there are none). */
    double getContentEnd();

    /** How opaque a sound box or container is drawn: -72 dB or less the
        faintest (0.15), 0 dB and more fully opaque, linear in dB between. */
    static float gainOpacity (double gainDb) noexcept;

    //== Containers (stage 5a) ================================================
    // A container is a box whose content is a timeline of its own: a TIMELINE
    // child holding TRACKS (tracks, folders and boxes; containers again). A
    // point t of the inner timeline lies at
    //     start + (t - innerOffset) * scale
    // outside. Stretch (the default): scale = length / innerLength, so the
    // inside follows the box when it changes length. Window: scale 1; the box
    // shows inner time from innerOffset on, as a sound box shows its sound.
    // Sounds keep their speed and length at any scale (no varispeed): only
    // their starts move, and what runs past the container's end is cut.
    // Text boxes and containers inside scale their lengths.
    static bool isContainer (const juce::ValueTree& b)             { return kindOf (b) == "container"; }
    /** Sound boxes and containers have a gain and fades. */
    static bool isAudible (const juce::ValueTree& b)               { return isSound (b) || isContainer (b); }
    /** The container's inner tracks (invalid for other boxes). */
    static juce::ValueTree innerTracks (const juce::ValueTree& box) { return box.getChildWithName (IDs::TIMELINE).getChildWithName (IDs::TRACKS); }
    static bool isStretch (const juce::ValueTree& b)               { return (bool) b.getProperty (IDs::stretch, true); }
    static double innerOffset (const juce::ValueTree& b)           { return (double) b.getProperty (IDs::innerOffset, 0.0); }
    static double innerLength (const juce::ValueTree& b);
    /** Outer seconds per inner second. */
    static double containerScale (const juce::ValueTree& b);
    static double toOuter (const juce::ValueTree& container, double innerTime)  { return boxStart (container) + (innerTime - innerOffset (container)) * containerScale (container); }
    static double toInner (const juce::ValueTree& container, double outerTime)  { return innerOffset (container) + (outerTime - boxStart (container)) / containerScale (container); }
    /** How long a box inside the container lasts outside it. */
    static double outerLength (const juce::ValueTree& container, const juce::ValueTree& inner);
    /** The inner tracks that can hold boxes, in order. */
    static juce::Array<juce::ValueTree> innerLanes (const juce::ValueTree& container);

    /** An empty container (one inner track), stretched. Undoable. */
    juce::ValueTree addContainer (juce::ValueTree track, double start, double length, const juce::String& name = "Container");
    /** Puts the boxes (all on tracks of the same timeline) into a new
        container that spans them, on the first of their tracks. Inside,
        tracks mirror the ones they came from (with their folders), each box
        in the same place. Undoable; invalid if there is nothing to group. */
    juce::ValueTree groupIntoContainer (const juce::Array<juce::ValueTree>& boxes);
    /** Puts the container's boxes back on the tracks of the same names
        (new tracks below the container's where there are none), where they
        sounded; the container goes. Undoable. */
    juce::Array<juce::ValueTree> ungroupContainer (juce::ValueTree container);
    /** Stretch to window bakes the present scale in (nothing moves); window
        to stretch makes the present length the inner length. Undoable. */
    void setStretch (juce::ValueTree container, bool stretch);
    /** Two boxes change places, each keeping its length: the later one starts
        where the earlier one started, the earlier one ends where the later
        one ended (on each other's tracks). Undoable. */
    bool swapBoxes (juce::ValueTree a, juce::ValueTree b);
    /** New ids for everything inside a box (a container's tracks and boxes),
        for copies; references inside (folders, frames) follow. */
    static void renewInnerIds (juce::ValueTree box);

    //== Proportion frames (stage 3a) =========================================
    struct Frame
    {
        double start = 0.0, length = 0.0;
        bool isValid() const noexcept                { return length > 1.0e-9; }
        double at (double proportion) const noexcept { return start + proportion * length; }
        double proportionOf (double t) const noexcept { return (t - start) / length; }
        double end() const noexcept                  { return start + length; }
    };

    /** The piece as a frame: its set length, or its current length. */
    double getPieceLength();
    bool hasSetPieceLength() const                  { return (double) state.getProperty (IDs::frameLength, 0.0) > 0.0; }
    /** 0 follows the piece's current length. Undoable. */
    void setPieceLength (double seconds);

    bool hasSpan() const;
    /** The span dragged on the proportion ruler; a == b removes it. Undoable. */
    void setSpan (double a, double b);

    /** "piece", "span" or a box id; invalid when there is no such frame. */
    Frame frameFor (const juce::String& ref);
    juce::String frameName (const juce::String& ref);

    //== Ruler layers (stage 3b) ==============================================
    // The ruler is a stack of layers: one time layer and any number of
    // proportion layers, each with its own frame, divisions and series.
    // Their layout is view state (not undoable); marks are content (undoable).
    juce::ValueTree getRulers();
    juce::ValueTree findLayer (const juce::String& layerId);
    static bool isTimeLayer (const juce::ValueTree& l)              { return l[IDs::kind].toString() == "time"; }
    /** An overview lane (stage 4d): draws what a package's lane works out from the boxes. */
    static bool isLaneLayer (const juce::ValueTree& l)              { return l[IDs::kind].toString() == "lane"; }
    static bool isProportionLayer (const juce::ValueTree& l)        { return l.isValid() && ! isTimeLayer (l) && ! isLaneLayer (l); }
    juce::ValueTree addLayer (const juce::String& kind, int index = -1);
    /** A lane layer for a package's lane ("composition.energies"). */
    juce::ValueTree addLaneLayer (const juce::String& laneKey, const juce::String& name, int height, int index = -1);
    void removeLayer (juce::ValueTree layer);
    /** The proportion layer the menus and "the ruler's frame" refer to. */
    juce::ValueTree getActiveLayer();
    void setActiveLayer (const juce::ValueTree& layer)              { state.setProperty (IDs::activeLayer, layer[IDs::id].toString(), nullptr); }
    /** A layer's frame (a removed box falls back to the piece). */
    juce::String layerFrame (const juce::ValueTree& layer);
    void setLayerFrame (juce::ValueTree layer, const juce::String& ref) { layer.setProperty (IDs::frame, ref, nullptr); }
    juce::Array<double> layerDenominators (const juce::ValueTree& layer) const;
    void setLayerDenominators (juce::ValueTree layer, const juce::Array<double>&);
    static juce::Array<double> defaultDenominators();
    /** The layers as plain data (for presets), and back. */
    juce::var layersToVar();
    void layersFromVar (const juce::var&);

    /** The active layer's frame, divisions, and zoom setting (stage 3a names). */
    juce::String getRulerFrame();
    void setRulerFrame (const juce::String& ref);
    juce::Array<double> getDenominators();
    void setDenominators (const juce::Array<double>&);
    bool getAutoDivisions();
    void setAutoDivisions (bool on);

    //== Named marks (stage 3b) ===============================================
    // A mark is a named proportion of a frame (it follows the frame). Box
    // proportions can name marks: start "m1", length "m2 - m1".
    juce::ValueTree getMarks();

    //== Drawn lanes and nodes (stage 4d.2) ====================================
    /** Spans and points drawn in drawn lanes, by lane key; kept apart from
        the ruler layers, so presets never touch them. */
    juce::ValueTree getDrawn();
    juce::Array<juce::ValueTree> drawnItems (const juce::String& laneKey);
    static bool isSpan (const juce::ValueTree& item)        { return item[IDs::kind].toString() != "point"; }
    juce::ValueTree addSpan (const juce::String& laneKey, double start, double end, const juce::String& kind);
    juce::ValueTree addPoint (const juce::String& laneKey, double time, const juce::String& functions);
    juce::ValueTree findItem (const juce::String& itemId);
    void removeItem (juce::ValueTree item);
    /** Nodes on folders: short spans where every voice of the folder changes at once. */
    juce::ValueTree getNodes();
    juce::Array<juce::ValueTree> nodesOf (const juce::ValueTree& folder);
    juce::ValueTree addNode (const juce::ValueTree& folder, double start, double length);
    juce::ValueTree findNode (const juce::String& nodeId);
    void removeNode (juce::ValueTree node);
    /** Every track (not folder) inside a folder, at any depth. */
    juce::Array<juce::ValueTree> tracksIn (const juce::ValueTree& folder);
    juce::ValueTree findMark (const juce::String& markId);
    juce::ValueTree findMarkByName (const juce::String& name);
    juce::String uniqueMarkName (const juce::String& prefix);
    juce::ValueTree addMark (const juce::String& frameRef, const juce::String& expression, const juce::String& name = {});
    /** Renames it everywhere (also in box proportions); false if the name is
        not valid or taken. Undoable. */
    bool renameMark (juce::ValueTree mark, const juce::String& newName, juce::String& error);
    /** Removes it; proportions naming it keep their present value. Undoable. */
    void removeMark (juce::ValueTree mark);
    bool setMarkExpression (juce::ValueTree mark, const juce::String& text, juce::String& error);
    /** Where the mark is in seconds; negative if its frame is gone. */
    double markTime (const juce::ValueTree& mark);
    /** An expression's value as a proportion of `frameRef`, mark names included. */
    Proportion::Result evaluate (const juce::String& expression, const juce::String& frameRef);
    /** Pins a box's start (or end) to a mark. Undoable. */
    void pinEdge (juce::ValueTree box, bool start, const juce::String& markName);
    /** Marks at the boundaries of a ratio split of a frame ("2:3:5:8"). */
    juce::Array<juce::ValueTree> splitFrameByRatio (const juce::String& frameRef, const juce::Array<double>& parts, const juce::String& prefix);
    /** Cuts a box into parts with these ratios. */
    juce::Array<juce::ValueTree> cutBoxByRatio (juce::ValueTree box, const juce::Array<double>& parts);

    //== Proportional boxes ===================================================
    // A bound box has a frame and a start and length typed as proportions of
    // it. While locked (the default) it follows the frame: move or stretch the
    // frame and the box keeps its proportions. Moving or trimming the box
    // itself rewrites its proportions.
    static juce::String boundFrame (const juce::ValueTree& b)      { return b[IDs::propFrame].toString(); }
    static bool isLocked (const juce::ValueTree& b)                { return (bool) b.getProperty (IDs::propLock, true); }
    bool isBound (const juce::ValueTree& b)                        { return boundFrame (b).isNotEmpty() && frameFor (boundFrame (b)).isValid(); }
    /** True if `box` follows `frameBox`, directly or through other boxes. */
    bool dependsOn (const juce::ValueTree& box, const juce::ValueTree& frameBox);
    /** A box cannot be the frame of itself or of a box it follows. */
    bool canUseAsFrame (const juce::ValueTree& box, const juce::String& ref);

    /** Binds the box to a frame, its present times becoming its proportions;
        "" unbinds it. Undoable. */
    void bindBox (juce::ValueTree box, const juce::String& frameRef);
    /** Sets the start or length from typed text; false (and an error) if the
        text is not a proportion. Undoable. */
    bool setProportion (juce::ValueTree box, bool start, const juce::String& text, juce::String& error);
    void setLock (juce::ValueTree box, bool locked);
    /** The box's present start or length as a proportion of its frame. */
    double actualProportion (const juce::ValueTree& box, bool start);
    /** A copy put somewhere else (paste): its proportions follow its new times.
        Call before adding it to a track. */
    void refitProportions (juce::ValueTree copy)    { rewriteProportions (copy, boxStart (copy), boxLength (copy), nullptr); }
    /** Brings every locked box in line with its frame (not undoable: the
        times follow from the proportions). Runs after every change. */
    void applyProportions();

    //== Framework values (stage 4b) ===========================================
    /** What a box has for a category: its own value, else its folder's (for
        categories folders can give), else the category's default. */
    struct FrameworkValue
    {
        enum class From { none, own, folder, defaultValue, timing, implied };
        juce::String value;
        From from = From::none;
        juce::String source;                        // implied: the key of the folder's category that gave it
        bool isOwn() const noexcept                 { return from == From::own; }
        bool fromFolder() const noexcept            { return from == From::folder; }
        bool isDefault() const noexcept             { return from == From::defaultValue; }
        bool fromTiming() const noexcept            { return from == From::timing; }
        bool isImplied() const noexcept             { return from == From::implied; }
        /** Worked out rather than set: from the timing, or given by a folder's other value. */
        bool isDerived() const noexcept             { return fromTiming() || isImplied(); }
    };
    /** Also: a join's value found from the timing (stage 4c), a value a
        folder's other category gives (a writing type's mixing relation, 4d),
        and nothing while the category (or its value) does not apply ("only
        with"), or for a category set on folders only. `ignoreOwn`: what the
        box would have without a value of its own. */
    FrameworkValue frameworkValue (const juce::ValueTree& box, const Framework::Category&, bool ignoreOwn = false);

    /** The framework the project's values are read with (for "only with"
        conditions, and for what a split keeps on each side). May be null. */
    void setFramework (const Framework* f)          { framework = f; joinsValid = false; }
    const Framework* getFramework() const noexcept  { return framework; }
    /** What the box's folders give it for `c` through another category ("" if nothing). */
    FrameworkValue impliedValue (const juce::ValueTree& box, const Framework::Category&);
    /** True if the category applies to the box ("only with"). */
    bool applies (const juce::ValueTree& box, const Framework::Category&);
    /** True if this value of the category applies to the box. */
    bool valueApplies (const juce::ValueTree& box, const Framework::Category&, const juce::String& valueId);

    //== Joins (stage 4c) =======================================================
    /** A box and the one before it on the same track: the box that starts
        most recently before it. The gap is negative for an overlap; covered
        means a box on another track (not muted) spans the whole gap. */
    struct Join
    {
        juce::ValueTree before;
        double gap = 0.0;
        bool covered = false;
        bool isValid() const noexcept               { return before.isValid(); }
    };
    Join joinBefore (const juce::ValueTree& box);
    /** The box whose join-before is this one (invalid if none). */
    juce::ValueTree boxAfter (const juce::ValueTree& box);
    /** What the category's timing rules make of a join ("" if none fits). */
    static juce::String derivedValue (const Framework::Category&, const Join&);
    /** The value of the nearest folder around `track` (or `track` itself, if a
        folder) that has one; "" if none. */
    juce::String folderValue (const juce::ValueTree& track, const juce::Identifier& property);
    /** Sets a category's value on a box or folder ("" or void removes it). Undoable. */
    void setFrameworkValue (juce::ValueTree target, const Framework::Category&, const juce::var& value);
    /** Changes of a category along a box. */
    static juce::Array<Along::Change> changesAlong (const juce::ValueTree& box, const Framework::Category& c)
                                                    { return Along::parse (box[c.alongProperty()].toString()); }
    void setChangesAlong (juce::ValueTree box, const Framework::Category&, const juce::Array<Along::Change>&);
    /** Every value a box shows for a category: its value (own, folder's or
        default), those it changes to along the box, or each of its tags. */
    juce::StringArray valuesShown (const juce::ValueTree& box, const Framework::Category&);
    /** Renames the 4a framework fields (energy, level, typology ...) of a
        project tree to their stage 4b keys; true if anything changed. */
    static bool migrateFrameworkFields (juce::ValueTree projectTree);

    /** Called after any change to the model (also undo/redo). */
    std::function<void()> onChanged;
    /** Goes up with every change (for caches of what is worked out from the boxes). */
    int getChangeCount() const noexcept             { return changeCount; }

    //== JSON =================================================================
    static juce::var toVar (const juce::ValueTree&);
    static juce::ValueTree fromVar (const juce::var&);
    juce::String toJson() const;
    /** Replaces the state; returns false (state unchanged) if the text is not a Projet file. */
    bool fromJson (const juce::String&, juce::String& error);

    static juce::String newId()                     { return juce::Uuid().toString(); }

private:
    static juce::ValueTree createState (const juce::String& name, double rate);
    void changed();
    void ensureRulers();
    int markDepth = 0;
    /** For a locked, bound box: its proportions become these times. */
    void rewriteProportions (juce::ValueTree box, double newStart, double newLength, juce::UndoManager* undo);
    void freezePieceLengthFor (const juce::String& ref);
    static juce::ValueTree makeTrack (const juce::String& name, int colour);
    static juce::ValueTree makeTimeline (bool withTrack);
    static juce::ValueTree makeContainer (double start, double length, const juce::String& name);
    /** Scales what a box shows by k, its start aside (see scaleTimeline). */
    void scaleBox (juce::ValueTree box, double k);
    /** Inner times times k: starts, text box and container lengths (sounds
        keep theirs); window containers inside scale their contents too. */
    void scaleTimeline (juce::ValueTree tracks, double k);

    bool applying = false, loading = false;
    int changeCount = 0;

    const Framework* framework = nullptr;
    struct JoinInfo { juce::String before, after; double gap = 0.0; bool covered = false; };
    std::map<juce::String, JoinInfo> joins;      // by box id; rebuilt after a change
    bool joinsValid = false;
    int conditionDepth = 0;
    void buildJoins();
    bool holds (const juce::ValueTree& box, const Framework::Condition&);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { changed(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { changed(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { changed(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { changed(); }
    void valueTreeRedirected (juce::ValueTree&) override                                 { changed(); }

    juce::ValueTree state;
    juce::UndoManager undoManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Project)
};
