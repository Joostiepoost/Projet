// Board.h — the items on a board box's canvas (stage 9): what they are,
// where they lie and how they are hit, made, moved and resized. Shared by
// the board editor, the painter and the timeline (which shows a part of a
// board on its box).
//
// Coordinates are board units (pixels at 100%), y downwards. Every item has
// an id; cards, shapes, strokes and images have x, y, w, h; an arrow has two
// ends, each attached to an item (fromItem, toItem: it then starts at that
// item's edge) or free (x1, y1 / x2, y2), and a bend.

#pragma once

#include <JuceHeader.h>
#include "Identifiers.h"

namespace Board
{
    enum class Kind { card, arrow, shape, stroke, image, unknown };
    Kind kindOf (const juce::ValueTree& item);
    /** "card", "arrow" ... for the panel and undo names. */
    juce::String kindName (const juce::ValueTree& item);

    juce::ValueTree findItem (const juce::ValueTree& board, const juce::String& id);

    /** An item's rectangle (an arrow's or a line's: around its ends). */
    juce::Rectangle<float> bounds (const juce::ValueTree& board, const juce::ValueTree& item);
    /** The rectangle of a card, shape or image as stored (may be negative for lines). */
    juce::Rectangle<float> rect (const juce::ValueTree& item);
    /** Around every item (empty for an empty board). */
    juce::Rectangle<float> contentBounds (const juce::ValueTree& board);
    /** Around these items. */
    juce::Rectangle<float> boundsOf (const juce::ValueTree& board, const juce::Array<juce::ValueTree>& items);
    /** Around both, even when one has no width or height (juce's getUnion drops an empty rectangle). */
    juce::Rectangle<float> unite (juce::Rectangle<float>, juce::Rectangle<float>);

    /** Where an arrow runs: from a to b through the curve's control point. */
    struct ArrowPath
    {
        juce::Point<float> a, b, control;
        juce::Point<float> middle() const;      // on the curve, halfway
        juce::Path path() const;
    };
    ArrowPath arrowPath (const juce::ValueTree& board, const juce::ValueTree& arrow);
    /** The stroke's points on the board. */
    juce::Array<juce::Point<float>> strokePoints (const juce::ValueTree& stroke);
    juce::String formatPoints (const juce::Array<juce::Point<float>>& relative);

    /** True if `p` hits the item (`tolerance` in board units). Unfilled
        rectangles and ellipses are hit near their outline only, so what lies
        inside them stays easy to pick. */
    bool hits (const juce::ValueTree& board, const juce::ValueTree& item, juce::Point<float> p, float tolerance);
    /** The topmost item hit, or invalid. */
    juce::ValueTree itemAt (const juce::ValueTree& board, juce::Point<float> p, float tolerance);

    /** New items (not yet on a board). */
    juce::ValueTree makeCard (juce::Rectangle<float>, int colour);
    juce::ValueTree makeShape (const juce::String& shape, juce::Rectangle<float>, int colour);
    juce::ValueTree makeArrow (juce::Point<float> a, juce::Point<float> b, const juce::String& fromId, const juce::String& toId, int colour);
    juce::ValueTree makeStroke (const juce::Array<juce::Point<float>>& points, int colour, float width);
    juce::ValueTree makeImage (const juce::String& file, juce::Rectangle<float>);

    /** Moves an item (an arrow: its free ends). */
    void moveBy (juce::ValueTree item, juce::Point<float> delta, juce::UndoManager*);
    /** Gives a card, shape, image or stroke a new rectangle (a stroke's points scale). */
    void setRect (juce::ValueTree item, juce::Rectangle<float>, juce::UndoManager*);
    /** Can be resized by its corners (cards, rectangles, ellipses, images, strokes). */
    bool hasCorners (const juce::ValueTree& item);
    /** Has two ends to drag (lines, arrows). */
    bool hasEnds (const juce::ValueTree& item);
    /** A line's or arrow's ends. */
    std::pair<juce::Point<float>, juce::Point<float>> ends (const juce::ValueTree& board, const juce::ValueTree& item);
    /** Moves one end (0 or 1) of a line or arrow; an arrow's end attaches to
        `attachTo` (an item id) or comes free at `p`. */
    void setEnd (juce::ValueTree item, int end, juce::Point<float> p, const juce::String& attachTo, juce::UndoManager*);

    /** Arrows attached to an item that goes lose their attachment, keeping
        their present ends (before the item is removed). */
    void detachArrowsFrom (juce::ValueTree board, const juce::String& itemId, juce::UndoManager*);

    /** The part of the board shown on its box (empty: none chosen). */
    juce::Rectangle<float> shownPart (const juce::ValueTree& board);
    void setShownPart (juce::ValueTree board, juce::Rectangle<float>, juce::UndoManager*);
    /** "3 cards, 2 arrows, 1 image" ("empty"). */
    juce::String summary (const juce::ValueTree& board);

    /** Copies of items with new ids; arrows between copied items follow
        them, arrows to items left behind keep their present ends. */
    juce::Array<juce::ValueTree> copyItems (const juce::ValueTree& board, const juce::Array<juce::ValueTree>& items);

    /** The colours offered for items (ARGB), the first the default. */
    const juce::Array<juce::Colour>& palette();
    juce::Colour colourOf (const juce::ValueTree& item);

    /** Labels offered for cards: those used on every board of a project, then a few common ones. */
    juce::StringArray labelsIn (const juce::ValueTree& projectState);
}
