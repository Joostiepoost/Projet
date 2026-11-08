// RulerPanel.h — the ruler layers, in the panel on the right (stage 3b).
//
// Top: the layers, top to bottom (drag ⋮⋮ to reorder; show or hide; small,
// medium, large; delete), presets (built in and your own). Below: the chosen
// layer: its frame, divisions, series and marks.

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"

class RulerPanel : public juce::Component
{
public:
    explicit RulerPanel (Project&);
    ~RulerPanel() override;

    /** Which layer the details show ("" = the active one). */
    void showLayer (const juce::String& layerId);
    /** Re-reads the model; rebuilds only when layers or marks were added or removed. */
    void refresh();

    void setProperties (juce::PropertiesFile* p)    { properties = p; refresh(); }

    std::function<double()> getPlayhead;
    std::function<void()> onDone;
    std::function<void (const juce::String& message, bool warning)> onStatus;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    class Content;
    Project& project;
    juce::PropertiesFile* properties = nullptr;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    juce::String layerId;
};
