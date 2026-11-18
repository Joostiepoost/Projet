// ReaperExport.h — the piece as a Reaper project (stage 10a).
//
// Flattened: one .rpp. Tracks and folders become Reaper tracks and folder
// tracks; a container becomes a folder track named after it under its track,
// holding its tracks, with its gain as the folder's volume and its fades as
// the folder's volume envelope (Reaper sums the folder, so this is exactly
// how Projet plays it). Sounds sit where they sound, cut at the containers'
// edges as in playback (5 ms fades there), never stretched. Text boxes and
// board boxes become empty items with their text as item notes; a board's
// temporal outs become items on a track of their own under it. A sound
// box's sound takes become the item's takes. The project's copies of the
// sounds are referenced by paths relative to the .rpp, so the project folder
// can move.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Model/Framework.h"
#include "../Model/ProjectFolder.h"

namespace projet::ReaperExport
{
    struct Options
    {
        /** Only this span of the piece, moved to 0:00 (empty: the whole piece). */
        juce::Range<double> span;
        bool regions = true;        // the containers on the piece's own tracks, as regions
        bool markers = true;        // the named marks and the nodes, as markers
        /** Where the .rpp goes: sound paths are written relative to its folder. */
        juce::File rppFile;
        /** The tracks' colours (a track keeps an index into the app's palette); none: Reaper's own. */
        std::function<juce::Colour (int index)> trackColour;
        /** Seconds since 1970, written in the header (0: now). */
        juce::int64 timeStamp = 0;
    };

    struct Result
    {
        juce::String text;          // the .rpp
        int tracks = 0, items = 0, emptyItems = 0, regions = 0, markers = 0;
        juce::StringArray missing;  // sound files not found on disk
    };

    Result write (Project&, const Framework&, const ProjectFolder&, const Options&);

    /** Text for an .rpp line: in double quotes, or other quotes if it holds them. */
    juce::String quoted (const juce::String&);
    /** A colour (ARGB) as Reaper's custom colour on macOS. */
    int colourFor (int argb);
}
