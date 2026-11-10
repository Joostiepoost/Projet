// Lanes.h — what the overview lanes show (stage 4d), worked out from the
// boxes: for a "rows" lane, where boxes have a value of each group (or value)
// of a category, and how many at once; for a "scale" lane, the range of an
// ordered category present at each moment and its weighted average.
//
// Only boxes on tracks that sound (not muted) count. Changes along a box are
// taken at their middle. No graphics here: TimelineRuler.cpp draws the result.

#pragma once

#include "Project.h"

namespace Lanes
{
    /** A stretch of time where `count` boxes have the row's value (count > 0). */
    struct Segment
    {
        double start = 0.0, end = 0.0;
        int count = 0;
    };

    struct Row
    {
        juce::String id;                        // a group's id, or a value's
        Framework::Name name;
        juce::uint32 colour = 0;                // 0: none (drawn in a neutral colour)
        std::vector<Segment> main, second;      // second: the lane's second category (drawn lighter)
        juce::StringArray boxIds;               // every box behind the row (for highlighting)
    };

    /** One row per group (or value) of the lane's category, in the
        category's order; rows nothing fills are kept, so the lane keeps its
        shape as the piece grows. */
    std::vector<Row> rows (Project&, const Framework&, const Framework::Lane&);

    /** A stretch of time where some box has a value of the scale's category:
        the lowest and highest positions present (0 = the first value of the
        scale) and their average weighted by the lane's weight category. */
    struct ScaleSegment
    {
        double start = 0.0, end = 0.0;
        double low = 0.0, high = 0.0, mean = 0.0;
    };

    struct Scale
    {
        juce::StringArray ids;                  // the values, in order (by their "order")
        std::vector<ScaleSegment> segments;
    };

    Scale scale (Project&, const Framework&, const Framework::Lane&);

    /** How many rows a lane draws (groups, values, or the scale's steps). */
    int rowCount (const Framework&, const Framework::Lane&);
    /** A lane layer's height for its size: 0 small, 1 medium (enough for its
        row names), 2 large. */
    int heightFor (const Framework&, const Framework::Lane&, int size);

    //== Drawn lanes and nodes (stage 4d.2) ====================================

    /** A drawn lane's spans on rows: overlapping spans stack (each takes the
        first row where it fits, in order of start). */
    struct Placed { juce::ValueTree item; int row = 0; };
    std::vector<Placed> stack (Project&, const juce::String& laneKey, int& rows);

    /** What a node on a folder says: the stretch before it (since the
        previous node, or the folder's first box) and its share of it, flagged
        when too short (it goes unheard) or too long (it becomes a line); and
        the folder's tracks that do not change within it (a box starting or
        ending there, up to the rules' window outside it). Muted and empty
        tracks are left out. */
    struct NodeCheck
    {
        double stretch = 0.0, ratio = 0.0;
        bool tooShort = false, tooLong = false;
        int tracks = 0;                         // the voices it should gather
        juce::StringArray still;                // names of those that do not change within it
    };
    NodeCheck checkNode (Project&, const Framework::NodeRules&, const juce::ValueTree& node);

    /** A box's values of a category over time: (start, end, value) pieces, the
        changes along the box taken at their middle. */
    struct Piece { double start = 0.0, end = 0.0; juce::String value; };
    std::vector<Piece> piecesOf (Project&, const juce::ValueTree& box, const Framework::Category&);
}
