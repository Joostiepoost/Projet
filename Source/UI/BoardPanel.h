// BoardPanel.h — the panel on the right while the board editor is worked in
// (stage 9a): what is selected on the board (a card's label, colour,
// semiotic level and text; a shape's, arrow's or drawing's colour, width,
// dashes and label; an image's caption), or, with nothing selected, the
// board itself: what it holds, the part shown on its box, and the keys.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Model/Board.h"
#include "NotesEditor.h"

class BoardPanel : public juce::Component
{
public:
    BoardPanel (Project&, const Framework&);
    ~BoardPanel() override;

    void setItems (const juce::ValueTree& box, const juce::Array<juce::ValueTree>& items);
    /** Re-reads the model. */
    void refresh();
    void commitNotes()                              { notes.commit(); }

    std::function<void()> onShowOnBox, onClearShown, onFit, onAddImages, onFront, onBack, onDelete, onClose, onFill;
    std::function<bool()> isFilled;
    // 9b
    std::function<void (juce::ValueTree card)> onPlayCard;
    std::function<juce::String()> previewingCard;
    std::function<void (juce::ValueTree box)> onGoToBox;
    std::function<void (juce::ValueTree link)> onRelink;
    std::function<double (const juce::ValueTree& card)> soundSeconds;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Mode { board, card, shape, arrow, stroke, image, soundCard, link, several };
    Mode mode = Mode::board;
    void setMode (Mode);
    void commitWords();
    /** A sound card's times typed in (9b). */
    void commitTime (juce::TextEditor&, const juce::Identifier&, const juce::String& undoName);
    void setOnAll (const juce::Identifier&, const juce::var&, const juce::String& undoName);
    juce::UndoManager* um()                         { return project.um(); }

    struct ColourRow : juce::Component, juce::SettableTooltipClient
    {
        juce::Colour current;
        bool mixed = false;
        std::function<void (juce::Colour)> onPick;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::Rectangle<float> swatch (int i) const;
    };

    Project& project;
    const Framework& framework;
    juce::ValueTree box;
    juce::Array<juce::ValueTree> items;
    bool updating = false;

    juce::Label title, info;
    juce::Label labelLabel { {}, "Label" }, colourLabel { {}, "Colour" }, levelLabel, textLabel { {}, "Text (double-click the card to write on the board)" };
    juce::ComboBox labelBox, levelBox;
    ColourRow colours;
    NotesEditor notes;
    juce::Label wordsLabel { {}, "Label" };
    juce::TextEditor words;
    juce::Label widthLabel { {}, "Line width" }, shapeLabel { {}, "Shape" };
    juce::Slider width;
    juce::ComboBox shapeBox;
    juce::ToggleButton dashed { "Dashed" }, filled { "Filled" };
    juce::TextButton straight { "Straight" };
    // Sound cards and links (9b)
    juce::Label fromLabel { {}, "Fragment: from" }, toLabel { {}, "to" }, gainLabel { {}, "Gain (dB)" },
                fadeInLabel { {}, "Fade in (s)" }, fadeOutLabel { {}, "Fade out (s)" }, startLabel { {}, "Starts in the box at" };
    juce::TextEditor fromEdit, toEdit, fadeInEdit, fadeOutEdit, startEdit;
    juce::Slider gain;
    juce::ToggleButton outToggle { "Plays in the piece (temporal out)" };
    juce::TextButton playButton { "Play" }, wholeButton { "Whole sound" }, goButton { "Go there" }, relinkButton { "Point it elsewhere..." }, takeButton;
    juce::TextButton showOnBox { "Show on the box" }, clearShown { "Show nothing on the box" }, fit { "Fit" }, addImages { "Add images..." },
                     front { "To front" }, back { "To back" }, remove { "Delete" }, close { "Close the board" }, fill { "Fill the area" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardPanel)
};
