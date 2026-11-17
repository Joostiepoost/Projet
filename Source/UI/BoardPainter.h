// BoardPainter.h — draws a board's items (stage 9), in board units: the
// caller sets the transform. Used by the board editor and by the timeline,
// which shows a chosen part of a board on its box.

#pragma once

#include <JuceHeader.h>
#include "../Model/Board.h"
#include "../Model/Framework.h"

namespace BoardPainter
{
    struct Context
    {
        const Framework* framework = nullptr;
        juce::File projectFolder;               // images are stored relative to it
        bool fills = true;                      // semiotic fills on cards
        juce::String skipText;                  // an item whose words are being edited (drawn without them)
        float alpha = 1.0f;
    };

    /** Every item, back to front. */
    void paint (juce::Graphics&, const juce::ValueTree& board, const Context&);
    void paintItem (juce::Graphics&, const juce::ValueTree& board, const juce::ValueTree& item, const Context&);

    /** The card's colours: its edge (the item colour) and its paper. */
    juce::Colour cardPaper (juce::Colour edge);
    /** The height a card's words need at its width (label and text). */
    float cardTextHeight (const juce::ValueTree& card);

    /** The image file of an IMAGE item, cached. */
    juce::Image imageFor (const juce::ValueTree& item, const juce::File& projectFolder);

    /** The category whose values fill boxes (the semiotic levels), or null. */
    const Framework::Category* fillCategory (const Framework&);
}
