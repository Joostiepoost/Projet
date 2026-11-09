// DisplayPanel.h — how the framework shows on the boxes (stage 4b).
//
// In the panel on the right (View > Display...): the colour mode, and for each
// category of the project's packages its mark (not shown, a strip, a badge, a
// label), the package's choice marked "default". Kept with the project as view
// settings (they do not count as a change to save). Presets keep combinations
// for all projects.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Model/Framework.h"

class DisplayPanel : public juce::Component
{
public:
    DisplayPanel (Project&, const Framework&);
    ~DisplayPanel() override;

    void refresh();
    void resized() override;
    /** Where the presets are kept (the app's settings). */
    void setProperties (juce::PropertiesFile* p)    { properties = p; refresh(); }

    std::function<void()> onDone;
    /** Sets a view setting of the project (colourMode, fwDisplay) without
        marking it as changed. */
    std::function<void (const juce::Identifier&, const juce::String&)> setView;

private:
    class Content;
    Project& project;
    const Framework& framework;
    juce::PropertiesFile* properties = nullptr;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;

    juce::var presets() const;
    void savePresets (const juce::var&);
};
