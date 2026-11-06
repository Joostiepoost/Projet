// StartPanel.h — shown when no project is open (first start, or the last
// project has gone): New project, Open project, and the recent projects.
// Projet never makes a project folder the user did not choose.

#pragma once

#include <JuceHeader.h>

class StartPanel : public juce::Component
{
public:
    StartPanel();

    void setRecent (const juce::Array<juce::File>& projects);

    std::function<void()> onNew, onOpen;
    std::function<void (const juce::File&)> onOpenRecent;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> card() const;

    juce::TextButton newButton { "New project..." }, openButton { "Open project..." };
    juce::OwnedArray<juce::TextButton> recentButtons;
    juce::Array<juce::File> recent;
};
