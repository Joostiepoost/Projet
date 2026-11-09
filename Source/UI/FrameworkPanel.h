// FrameworkPanel.h — the framework fields of the selected boxes (stage 4a).
//
// Shown in the inspector's Framework tab: energy and second energy (with their
// families), semiotic level (own or from the folder) and its changes along
// the box, role (Thoresen's F / M / B, or figure and ground) and its changes,
// and Schaeffer's type, picked from his typology chart. With several boxes
// selected, a choice applies to all of them; changes along a box need one box.

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
    /** Re-reads the model (rebuilds unless a field is being typed in). */
    void refresh();

    void resized() override;

    /** Shows Schaeffer's chart; `onPick` gets a type id ("" = none). */
    static void showTypologyChart (const Framework&, const juce::String& current, juce::Component& near,
                                   std::function<void (const juce::String&)> onPick);

private:
    class Content;
    Project& project;
    const Framework& framework;
    juce::Array<juce::ValueTree> boxes;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    void rebuild();
    /** Sets a field on every selected box ("" removes it). Undoable. */
    void setField (const juce::Identifier& id, const juce::String& value, const juce::String& undoName);
    /** The changes along the (one) selected box. Undoable. */
    void setAlong (const juce::Identifier& alongId, const juce::Array<Along::Change>& changes);
};
