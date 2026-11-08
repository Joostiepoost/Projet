// Inspector.h — the panel on the right.
//
// One box selected: name, kind (sound / text; later kinds listed but not yet
// available), for sound boxes the sound, gain, fades and opacity, the times,
// and the box's notes (kept whatever the kind). Several boxes: how many.
// Nothing selected: the project.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Model/ProjectFolder.h"
#include "../Engine/SoundData.h"
#include "NotesEditor.h"
#include "RulerPanel.h"

class Inspector : public juce::Component
{
public:
    Inspector (Project&, projet::SoundPool&, const ProjectFolder&);

    void setSelection (const juce::Array<juce::ValueTree>& boxes);
    /** Shows the ruler layers (stage 3b), with this layer's details ("" = the active one). */
    void showRuler (const juce::String& layerId);
    bool isShowingRuler() const                     { return mode == Mode::ruler; }
    RulerPanel& getRulerPanel()                     { return rulerPanel; }
    /** Re-reads the model (after any change). */
    void refresh();
    /** Saves notes being typed. */
    void commitNotes()                              { notes.commit(); }
    void focusNotes()                               { notes.setWriting (true); notes.focusEditor(); }

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (const juce::File&)> onReveal;
    std::function<void()> onExpandNotes;

private:
    enum class Mode { project, sound, text, several, ruler };
    juce::String ratioReport();
    juce::Array<juce::ValueTree> selection;
    void setMode (Mode);
    void commitName();
    void commitProportion (bool start);
    void commitPieceLength();
    void refreshProportions();
    void fillSoundList();
    juce::String projectSummary();

    Project& project;
    projet::SoundPool& pool;
    const ProjectFolder& folder;
    juce::ValueTree box;
    int selectionSize = 0;
    Mode mode = Mode::project;
    RulerPanel rulerPanel { project };
    bool updating = false;

    juce::Label title;
    juce::Label nameLabel { {}, "Name" }, kindLabel { {}, "Kind" }, soundLabel { {}, "Sound" };
    juce::TextEditor nameEditor;
    juce::ComboBox kindBox, soundBox;
    juce::Label gainLabel { {}, "Gain (dB)" };
    juce::Slider gain;
    juce::Label fadeInLabel { {}, "Fade in (ms)" }, fadeOutLabel { {}, "Fade out (ms)" };
    juce::Slider fadeIn, fadeOut;
    juce::ComboBox fadeInShape, fadeOutShape;
    juce::Label opacityLabel { {}, "Opacity (visual only)" };
    juce::Slider opacity;
    juce::Label times, soundInfo;
    juce::TextButton revealPlaceholder { "Show copy" }, revealOriginal { "Show original" };
    // Proportions (stage 3a)
    juce::Label propLabel { {}, "Proportions of a frame" };
    juce::ComboBox frameBox;
    juce::StringArray frameRefs;                    // per combo item id - 1
    juce::Label startLabel { {}, "Start" }, lengthLabel { {}, "Length" };
    juce::TextEditor startField, lengthField;
    juce::Label propInfo, propError;
    juce::ToggleButton lockToggle { "Keep the proportions when the frame changes" };

    juce::Label notesLabel { {}, "Notes (kept whatever the kind)" };
    NotesEditor notes;

    // Project (nothing selected)
    juce::Label projectInfo;
    juce::Label formatLabel { {}, "New imports as" };
    juce::ComboBox formatBox;
    juce::Label qualityLabel { {}, "Vorbis quality" };
    juce::Slider quality;
    juce::Label pieceLengthLabel { {}, "Piece length, as a proportion frame (m:ss, or auto)" };
    juce::TextEditor pieceLength;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Inspector)
};
