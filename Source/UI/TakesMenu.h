// TakesMenu.h — the menu of a box's takes (stage 7): the takes to switch to,
// and new takes (a copy of the active one, a sound from the project, a sound
// file, an empty text or container), renaming and deleting. Shown from the
// badge on a box, its right-click menu and the panel's New take button.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"

namespace TakesMenu
{
    /** What the menu asks of the app. */
    struct Actions
    {
        std::function<void (juce::ValueTree box)> addSoundFiles;    // sound files that become new takes of the box
        std::function<void()> toggleShowTakes;                      // View > Show takes in boxes
    };

    /** Item ids from `base` up to base + 2000. */
    juce::PopupMenu build (Project&, const juce::ValueTree& box, int base, bool listTakes);

    /** Does what the item chose (each its own undo step); false when the
        result is not one of ours. */
    bool perform (Project&, juce::ValueTree box, int result, int base, const Actions&, juce::Component* dialogParent);

    /** Builds, shows (under `target`, or at the mouse) and performs. */
    void show (Project&, juce::ValueTree box, juce::Component* target, bool atMouse, bool listTakes,
               const Actions&, std::function<void()> afterwards);

    /** "B: less reverb" for the menus and tooltips. */
    juce::String describe (const juce::ValueTree& take);
}
