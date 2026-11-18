// BoardPainter.h — draws a board's items (stage 9), in board units: the
// caller sets the transform. Used by the board editor and by the timeline,
// which shows a chosen part of a board on its box.

#pragma once

#include <JuceHeader.h>
#include "../Model/Board.h"
#include "../Model/Framework.h"
#include "../Model/Project.h"
#include "../Engine/SoundData.h"
#include "../Model/Glyphs.h"

namespace BoardPainter
{
    struct Context
    {
        const Framework* framework = nullptr;
        juce::File projectFolder;               // images are stored relative to it
        bool fills = true;                      // semiotic fills on cards
        juce::String skipText;                  // an item whose words are being edited (drawn without them)
        float alpha = 1.0f;
        Project* project = nullptr;             // sound names, linked boxes (9b)
        const projet::SoundPool* pool = nullptr;    // waveforms (9b)
        juce::String previewCard;               // the sound card heard now, and where (seconds into its sound)
        double previewSeconds = 0.0;
        const Glyphs::Set* glyphs = nullptr;    // the energies' shapes, for gestures (9c)
        double boxLength = 0.0;                 // the board box's length: a region's time
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

    //== Sound cards and links (9b): their parts, in board units ===============
    juce::Rectangle<float> playButton (const juce::ValueTree& soundCard);
    juce::Rectangle<float> waveArea (const juce::ValueTree& soundCard);
    juce::Rectangle<float> goButton (const juce::ValueTree& link);
    /** A sound card's name: its label, else its sound's name. */
    juce::String soundCardName (const juce::ValueTree& card, const Project*);
    /** The box a link points to, if it is in the piece. */
    juce::ValueTree linkedBox (const juce::ValueTree& link, Project*);
    /** "take B · level 2" for a linked box. */
    juce::String linkDetails (const juce::ValueTree& link, const juce::ValueTree& box);

    //== Regions and gestures (9c) =============================================
    /** The categories whose values draw glyphs (the energy, the second energy). */
    juce::Array<const Framework::Category*> glyphCategories (const Framework&);
    /** The colour a gesture is drawn in (its energy's). */
    juce::Colour gestureColour (const juce::ValueTree& gesture, const Framework&);
    /** The region the board shows on its box, when the part shown is one
        region running in the box's time; otherwise invalid. */
    juce::ValueTree shownRegion (const juce::ValueTree& board);
    /** A region's gestures across `area` (the box on the timeline): its time
        across, so each gesture sits at its own time in the box, its register up. */
    void paintRegionAcross (juce::Graphics&, const juce::ValueTree& board, const juce::ValueTree& region,
                            juce::Rectangle<float> area, const Context&);

    /** The category whose values fill boxes (the semiotic levels), or null. */
    const Framework::Category* fillCategory (const Framework&);
}
