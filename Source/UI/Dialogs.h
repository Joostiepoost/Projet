// Dialogs.h — small helpers for the app's dialogs.

#pragma once

#include <JuceHeader.h>

namespace Dialogs
{
    /** Puts the cursor in a dialog's text field, its text selected, once the
        window is on screen (macOS does not do this by itself for these windows). */
    inline void focusField (juce::AlertWindow* window, const juce::String& fieldName)
    {
        juce::Component::SafePointer<juce::AlertWindow> safe (window);
        auto focus = [safe, fieldName]
        {
            if (safe == nullptr) return;
            safe->toFront (true);
            if (auto* editor = safe->getTextEditor (fieldName))
            {
                editor->grabKeyboardFocus();
                editor->selectAll();
            }
        };
        juce::MessageManager::callAsync (focus);
        juce::Timer::callAfterDelay (120, focus);    // again, once the window is really there
    }
}
