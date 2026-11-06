// NotesWindow.h — the notes of the selected box in a large, separate window:
// the Markdown text on the left, the styled result on the right. Follows the
// selection; closing it saves.

#pragma once

#include <JuceHeader.h>
#include "NotesEditor.h"

class NotesWindow : public juce::DocumentWindow
{
public:
    explicit NotesWindow (Project&);

    void setBox (const juce::ValueTree&);
    void refresh();
    void closeButtonPressed() override;

private:
    void updateTitle();

    NotesEditor* editor = nullptr;
    juce::ValueTree box;
};
