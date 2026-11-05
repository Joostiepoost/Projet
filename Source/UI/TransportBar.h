// TransportBar.h — the strip at the top: transport, time, zoom, meters and a
// status line (imports, warnings).

#pragma once

#include <JuceHeader.h>

class TransportBar : public juce::Component
{
public:
    TransportBar();

    void setPlaying (bool);
    void setTime (double seconds);
    void setPeaks (float left, float right);
    void setStatus (const juce::String& text, bool warning = false);
    void setProjectName (const juce::String&, bool dirty);

    std::function<void()> onPlayStop, onToStart, onZoomIn, onZoomOut, onZoomFit, onImport;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton playButton { "Play" }, startButton { "|<" }, importButton { "Import..." };
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, zoomFit { "Fit" };
    juce::Label timeLabel, statusLabel, nameLabel;
    float peak[2] { 0.0f, 0.0f }, held[2] { 0.0f, 0.0f };
    bool statusIsWarning = false;
};
