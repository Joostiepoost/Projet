// NotesEditor.h — the notes of one box: read (styled, links clickable) or
// write (the Markdown text, with buttons that insert the marks).
//
// Used in the panel on the right and, side by side (write | read), in the
// large notes window. Typing is saved into the model after a short pause,
// as one undo step ("Edit notes").

#pragma once

#include <JuceHeader.h>
#include "Markdown.h"
#include "../Model/Project.h"

class NotesEditor : public juce::Component,
                    private juce::Timer
{
public:
    /** splitView: write and read side by side (the large window). */
    explicit NotesEditor (Project&, bool splitView = false);
    ~NotesEditor() override;

    void setBox (const juce::ValueTree&);
    juce::ValueTree getBox() const              { return box; }
    /** Re-reads the notes from the model (unless they are being typed). */
    void refresh();
    /** Saves what was typed now (before the box changes, the window closes, ...). */
    void commit();

    void setWriting (bool);
    bool isWriting() const noexcept             { return writing; }
    void focusEditor();

    std::function<void()> onExpand;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override               { stopTimer(); commit(); }
    void wrapSelection (const juce::String& before, const juce::String& after, const juce::String& placeholder);
    void prefixLines (const juce::String& prefix);
    void updateModeButtons();

    Project& project;
    const bool split;
    juce::ValueTree box;
    bool writing = false;

    juce::TextButton boldButton { "B" }, italicButton { "I" }, headingButton { "H" }, bulletButton { "List" }, linkButton { "Link" };
    juce::TextButton modeButton { "Preview" }, expandButton { "Expand" };
    juce::TextEditor editor;
    juce::Viewport viewport;
    MarkdownView view;
};
