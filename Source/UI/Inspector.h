// Inspector.h — the panel on the right.
//
// One box selected: name, kind (sound / text; later kinds listed but not yet
// available), for sound boxes the sound, gain, fades and opacity, the times,
// and the box's notes (kept whatever the kind). Several boxes: how many.
// Nothing selected: the project. With boxes selected, a Framework tab shows
// their framework values, built from the project's framework packages
// (stage 4a, 4b). Side panels replace the rest when asked for: the ruler
// layers (3b), Display and Frameworks (4b).

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Model/ProjectFolder.h"
#include "../Engine/SoundData.h"
#include "NotesEditor.h"
#include "RulerPanel.h"
#include "FrameworkPanel.h"
#include "DisplayPanel.h"
#include "PackagesPanel.h"

class Inspector : public juce::Component
{
public:
    Inspector (Project&, projet::SoundPool&, const ProjectFolder&, const Framework&);

    void setSelection (const juce::Array<juce::ValueTree>& boxes);
    /** Shows the ruler layers (stage 3b), with this layer's details ("" = the active one). */
    void showRuler (const juce::String& layerId);
    bool isShowingRuler() const                     { return mode == Mode::ruler; }
    RulerPanel& getRulerPanel()                     { return rulerPanel; }
    /** How the framework shows on the boxes (stage 4b). */
    void showDisplay();
    DisplayPanel& getDisplayPanel()                 { return displayPanel; }
    /** The framework packages (stage 4b); MainComponent fills in the entries. */
    void showPackages();
    bool isShowingPackages() const                  { return mode == Mode::packages; }
    PackagesPanel& getPackagesPanel()               { return packagesPanel; }
    /** After the framework was reloaded: rebuild what shows it. */
    void frameworkChanged();
    /** Shown when no box is selected and this returns a title (a selected node, 4d.2). */
    std::function<std::pair<juce::String, juce::String>()> extraInfo;
    /** Re-reads the model (after any change). */
    void refresh();
    /** Saves notes being typed. */
    void commitNotes()                              { notes.commit(); }
    void focusNotes()                               { notes.setWriting (true); notes.focusEditor(); }

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (const juce::File&)> onReveal;
    std::function<void()> onExpandNotes;
    /** The Framework tab's "Frameworks..." button. */
    std::function<void()> onShowFrameworks;
    std::function<void()> onHidePanel;          // the panel's own hide button (stage 4c)

private:
    enum class Mode { project, sound, text, container, several, ruler, display, packages };
    bool isSidePanel() const                        { return mode == Mode::ruler || mode == Mode::display || mode == Mode::packages; }
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
    // Box / Framework tabs (stage 4a)
    const Framework& framework;
    FrameworkPanel frameworkPanel { project, framework };
    DisplayPanel displayPanel { project, framework };
    PackagesPanel packagesPanel;
    juce::TextButton boxTab { "Box" }, frameworkTab { "Framework" };
    juce::TextButton hideButton { juce::String::fromUTF8 ("\xc2\xbb") };   // the panel goes, the timeline widens
    bool showFramework = false;
    bool isBoxMode() const                          { return (mode == Mode::sound || mode == Mode::text || mode == Mode::container || mode == Mode::several) && selectionSize > 0; }

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
    // Containers (stage 5a)
    juce::Label insideLabel { {}, "Inside" };
    juce::ComboBox insideBox;
    juce::Label containerInfo;
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
