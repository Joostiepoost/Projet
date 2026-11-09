// FrameworkPanel.h — the framework values of the selected boxes (stage 4a, 4b).
//
// Shown in the inspector's Framework tab, built from the categories of the
// project's framework packages, package by package: a choice as a menu
// (grouped, with its group pills and details), a row of buttons, or a chart
// (Schaeffer's typology); tags as toggles; a number as a slider; a text as a
// field. Categories that can change along a box get the along editor; values
// a folder gives are shown fainter. With several boxes selected, a choice
// applies to all of them; changes along a box need one box.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Model/Framework.h"

class FrameworkPanel : public juce::Component
{
public:
    FrameworkPanel (Project&, const Framework&);
    ~FrameworkPanel() override;

    void setBoxes (const juce::Array<juce::ValueTree>&);
    /** Re-reads the model (rebuilds unless a field is being typed in or a slider dragged). */
    void refresh();
    /** Rebuilds even while editing (after the framework was reloaded). */
    void rebuildNow()                               { rebuild(); }

    void resized() override;

    /** Shows a chart category's values in rows and columns; `onPick` gets a value id ("" = none). */
    static void showChart (const Framework&, const Framework::Category&, const juce::String& current, juce::Component& near,
                           std::function<void (const juce::String&)> onPick);

    /** "Frameworks..." and "Display..." at the bottom of the tab. */
    std::function<void()> onShowFrameworks, onShowDisplay;

private:
    class Content;
    Project& project;
    const Framework& framework;
    juce::Array<juce::ValueTree> boxes;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    void rebuild();
    /** Sets a category's value on every selected box ("" removes it). Undoable. */
    void setValue (const juce::String& categoryKey, const juce::var& value, const juce::String& undoName, bool newTransaction = true);
    /** Adds a tag to every selected box, or takes it from all of them. Undoable. */
    void setTag (const juce::String& categoryKey, const juce::String& tag, bool on);
    /** The changes along the (one) selected box. Undoable. */
    void setAlong (const juce::String& categoryKey, const juce::Array<Along::Change>& changes);
};
