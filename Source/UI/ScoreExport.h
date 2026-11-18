// ScoreExport.h — the graphic score (stage 10b).
//
// The timeline as it stands, laid out in systems like a score: a chosen
// number of seconds a line, as many lines a page as fit, A3 or A4 landscape.
// Each system is drawn by the timeline's own code (a TimelineView set to
// that stretch of time, the height of everything it shows), recorded as
// vectors; so the score shows what the timeline shows (hidden layers stay
// hidden, folded folders folded). On paper by default: the screen's dark
// colours turned light, their hues kept. Optionally each container opened
// out after the level it is in (its inside, in its own time, as when you go
// inside), and each board on a page of its own at the end.

#pragma once

#include <JuceHeader.h>
#include "VectorGraphics.h"
#include "../Model/Project.h"
#include "../Model/Framework.h"
#include "../Model/ProjectFolder.h"
#include "../Engine/SoundData.h"

namespace Score
{
    struct Options
    {
        juce::ValueTree level;                  // the level to print (invalid: the piece)
        juce::Range<double> span;               // in the level's time; empty: all of it
        bool a4 = false;                        // A4 landscape, else A3 landscape
        double secondsPerLine = 30.0;
        bool paper = true;                      // light, else as on screen
        bool openContainers = false;            // each container's inside after its level
        bool boards = false;                    // each board on a page of its own
        juce::String title;                     // the piece's name
    };

    struct Result
    {
        std::vector<Vector::Page> pages;
        int systems = 0, containers = 0, boards = 0;
    };

    Result render (Project&, projet::SoundPool&, const Framework&, const ProjectFolder&, const Options&);
}
