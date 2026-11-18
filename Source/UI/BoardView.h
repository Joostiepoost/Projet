// BoardView.h — the board editor (stage 9a): a board box's canvas, opened
// below the timeline where the magnifier opens (or filling the area).
//
// Tools on a strip at the left, each with a key: select (V), text card (T),
// arrow (A), shape (S; again for ellipse and line), pen (P), image (I),
// sound card (O), timeline link (L) (9b). A sound card plays from its button
// (or Space), its fragment set by dragging its ends on the waveform; marked
// out, it plays in the piece. Dragged up onto a track, it makes a sound box.
// A box Alt-dragged from the timeline onto the board makes a link to it.
// Morphogram regions (M) hold time across and register up; energy gestures
// (E) are paths drawn with an energy's glyph (9c), their factor curves in
// the panel; dragged up onto a track, a gesture makes a text box with its
// energy and curves.
// Scroll pans, Cmd-scroll or pinch zooms; items snap to a 10-unit grid
// unless Cmd is held. Double-click a card to write in it (or the empty board
// for a new card), a shape or arrow for its label, an image for its caption.
// Everything is undoable; the panel on the right shows what is selected.

#pragma once

#include <JuceHeader.h>
#include <map>
#include "../Model/Project.h"
#include "../Model/ProjectFolder.h"
#include "../Model/Board.h"
#include "BoardPainter.h"

class BoardView : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  public juce::DragAndDropTarget
{
public:
    BoardView (Project&, const Framework&, const ProjectFolder&, const projet::SoundPool&);
    ~BoardView() override;

    /** The board box shown (invalid: none). */
    void setBox (const juce::ValueTree& box);
    juce::ValueTree getBox() const                  { return box; }
    juce::ValueTree getBoard() const                { return Project::boardOf (box); }
    /** After any change to the model: repaint, and forget selected items that went. */
    void modelChanged();

    enum class Tool { select, card, arrow, shape, pen, image, sound, link, region, gesture };
    void setTool (Tool);
    Tool getTool() const noexcept                   { return tool; }

    juce::Array<juce::ValueTree> getSelectedItems() const;
    bool hasSelection() const                       { return ! getSelectedItems().isEmpty(); }
    void selectItems (const juce::Array<juce::ValueTree>&);
    void selectAll();
    void deselect();
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void paste();
    static bool canPaste();
    void duplicateSelection();
    void bringToFront();
    void sendToBack();

    void zoomToFit();
    void zoomBy (float factor);
    /** The part shown on the box: the selection's bounds, else what is in view. */
    void showOnBox();
    void clearShownPart();
    /** Asks for image files and places them in the middle of the view. */
    void chooseImages();

    void setActivePane (bool);
    /** Saves words being typed on the board. */
    void commitEdit();
    /** Writes in a card (or a shape's or arrow's label, an image's caption) in place. */
    void editText (juce::ValueTree item);

    std::function<void()> onActivate, onSelectionChanged;
    std::function<void (const juce::String&, bool warning)> onStatus;
    // 9b: hearing a sound card, going to a linked box, sounds from files.
    std::function<void (juce::ValueTree card)> onPlayCard;
    std::function<juce::String()> previewingCard;           // the card heard now ("" none)
    std::function<double()> previewSeconds;
    std::function<void (juce::ValueTree box)> onGoToBox;
    std::function<void (const juce::StringArray& files, juce::Point<float> boardPoint)> onImportSounds;
    std::function<void (juce::Point<float> boardPoint)> onChooseSoundFiles;
    /** True when the timeline is in view above (a sound card can be dragged onto it). */
    std::function<bool()> canDragOut;
    /** A sound card placed at a point (from a project sound). */
    juce::ValueTree addSoundCard (const juce::String& soundId, juce::Point<float> boardPoint, bool undoable = true);
    /** The energy new gestures are drawn with (a value of the main glyph category). */
    void setGestureEnergy (const juce::String& valueId)     { gestureEnergy = valueId; }
    const juce::String& getGestureEnergy() const            { return gestureEnergy; }
    /** Called when the framework changes: the glyph scripts again. */
    void frameworkReloaded();
    /** Points a link to another box (a menu of the piece's boxes). */
    void relink (juce::ValueTree link)              { showLinkMenu (Board::rect (link).getTopLeft(), link); }
    /** The selected item when it is the only one, else invalid. */
    juce::ValueTree singleSelected() const;

    // juce::DragAndDropTarget: a box from the timeline becomes a link (9b)
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

    static constexpr int toolStripWidth = 48;

private:
    Project& project;
    const Framework& framework;
    const ProjectFolder& folder;
    const projet::SoundPool& pool;
    juce::ValueTree box;
    juce::UndoManager* um()                         { return project.um(); }

    // The view: the board point at the canvas' top left, and the zoom.
    juce::Point<float> origin;
    float zoom = 1.0f;
    struct ViewState { juce::Point<float> origin; float zoom = 1.0f; };
    std::map<juce::String, ViewState> views;        // per board box, for this session
    void rememberView();
    juce::Rectangle<int> canvasArea() const         { return getLocalBounds().withTrimmedLeft (toolStripWidth); }
    juce::Point<float> toScreen (juce::Point<float> b) const;
    juce::Point<float> toBoard (juce::Point<float> s) const;
    juce::Rectangle<float> toScreen (juce::Rectangle<float> b) const;
    juce::Rectangle<float> visibleBoardArea() const;
    void zoomAbout (juce::Point<float> screenPoint, float newZoom);
    float snapValue (float v, bool on) const        { return on ? std::round (v / 10.0f) * 10.0f : v; }
    juce::Point<float> snapPoint (juce::Point<float> p, bool on) const { return { snapValue (p.x, on), snapValue (p.y, on) }; }

    // Tools
    Tool tool = Tool::select;
    juce::String shapeKind = "rect";
    struct ToolButton { Tool tool; juce::Rectangle<int> area; const char* name; const char* key; };
    std::vector<ToolButton> toolButtons() const;
    void paintToolStrip (juce::Graphics&);
    /** The part shown on the box, the selection, what is being made, the hints. */
    void paintOverlay (juce::Graphics&, const juce::ValueTree& board, juce::Rectangle<int> canvas);
    void showToolMenu (Tool);

    // Selection
    juce::StringArray selectedIds;
    void setSelectedIds (const juce::StringArray&);
    bool isSelected (const juce::ValueTree& item) const   { return selectedIds.contains (item[IDs::id].toString()); }
    float tolerance() const                         { return 5.0f / zoom; }

    // Dragging
    enum class Drag { none, pan, move, rubber, corner, end, bend, newCard, newShape, newArrow, pen, trimIn, trimOut, dragOut, newRegion, gesture };
    Drag drag = Drag::none;
    juce::Point<float> downScreen, downBoard, lastScreen;
    juce::Point<float> current;                     // the board point under the mouse while dragging
    int handle = -1;                                // a corner (0 top left, clockwise) or an end (0, 1)
    juce::ValueTree dragItem;                       // the item resized, or whose end or bend moves
    std::vector<std::pair<juce::ValueTree, juce::ValueTree>> moved;   // items moved, with copies of how they were
    juce::Rectangle<float> dragRect;                // the rectangle resized, as it was
    juce::String arrowFrom, hoverTarget;            // a new arrow's start, an end's item under the mouse
    juce::Array<juce::Point<float>> trail;          // the pen's points
    juce::StringArray rubberBase;                   // selected before a Shift rubber band
    /** The handle of the single selected item under a screen point (-1: none); `kind` 0 corner, 1 end, 2 bend. */
    int handleAt (juce::Point<float> screen, int& kind) const;
    std::vector<juce::Point<float>> cornerHandles (const juce::ValueTree& item) const;
    void moveSelectionTo (juce::Point<float> totalDelta);
    juce::ValueTree itemUnder (juce::Point<float> boardPoint, const juce::ValueTree& except = {}) const;
    void finishCreation (const juce::MouseEvent&);

    // Words typed in place
    std::unique_ptr<juce::TextEditor> editor;
    juce::ValueTree editing;
    juce::Identifier editingField;
    void positionEditor();

    void showItemMenu (juce::ValueTree item);
    void showSoundMenu (juce::Point<float> boardPoint);
    void showLinkMenu (juce::Point<float> boardPoint, juce::ValueTree relink = {});
    /** The trim end (0 in, 1 out) of a sound card under a screen point, or -1. */
    int trimHandleAt (const juce::ValueTree& card, juce::Point<float> screen) const;
    double soundSeconds (const juce::ValueTree& card) const;
    void startDragOut (const juce::ValueTree& card);
    bool linkDragOver = false;
    // 9c
    Glyphs::Set glyphs;
    juce::String gestureEnergy;
    std::vector<std::pair<juce::ValueTree, juce::ValueTree>> regionGestures;   // a region's gestures as they were, while it is resized
    void showEnergyMenu();
    void startGestureOut (const juce::ValueTree& gesture);
    void showBoardMenu (juce::Point<float> boardPoint);
    void addImages (const juce::Array<juce::File>&, juce::Point<float> at);
    void addCardAt (juce::Point<float> boardPoint, bool edit);
    juce::ValueTree addItem (juce::ValueTree item, const juce::String& undoName);
    void status (const juce::String& m, bool warning = false) { if (onStatus) onStatus (m, warning); }
    BoardPainter::Context context() const;

    bool activePane = false;
    int pasteCount = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardView)
};
