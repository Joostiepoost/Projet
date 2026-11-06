// NotesWindow.cpp

#include "NotesWindow.h"
#include "Theme.h"

namespace
{
    /** Background and margins around the editor. */
    struct Content : juce::Component
    {
        explicit Content (Project& p) : notes (p, true)  { addAndMakeVisible (notes); }
        void paint (juce::Graphics& g) override            { g.fillAll (Theme::panel); }
        void resized() override                            { notes.setBounds (getLocalBounds().reduced (16, 12)); }
        NotesEditor notes;
    };
}

NotesWindow::NotesWindow (Project& project)
    : juce::DocumentWindow ("Notes", Theme::panel, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
{
    auto* content = new Content (project);
    editor = &content->notes;
    setUsingNativeTitleBar (true);
    setContentOwned (content, false);
    setResizable (true, true);
    setResizeLimits (520, 300, 4000, 3000);
    centreWithSize (960, 640);
}

void NotesWindow::setBox (const juce::ValueTree& b)
{
    box = b;
    editor->setBox (b);
    updateTitle();
}

void NotesWindow::refresh()
{
    if (box.isValid() && ! box.getParent().isValid()) setBox ({});   // deleted
    editor->refresh();
    updateTitle();
}

void NotesWindow::updateTitle()
{
    setName (box.isValid() ? "Notes: " + box[IDs::name].toString() : juce::String ("Notes (no box selected)"));
    editor->setEnabled (box.isValid());
}

void NotesWindow::closeButtonPressed()
{
    editor->commit();
    setVisible (false);
}
