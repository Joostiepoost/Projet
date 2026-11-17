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

    std::function<void()> onPlayStop, onToStart, onZoomIn, onZoomOut, onZoomFit, onImport, onTogglePanel, onToggleAudio, onToggleOutline;
    /** The Panel button shows whether the panel on the right is shown. */
    void setPanelShown (bool shown)                 { panelButton.setToggleState (shown, juce::dontSendNotification); }
    /** The Audio button shows whether the audio engine runs (off saves CPU and battery). */
    void setAudioOn (bool on)                       { audioButton.setToggleState (on, juce::dontSendNotification); }
    /** The Outline button shows whether the outline on the left is shown (5b). */
    void setOutlineShown (bool shown)               { outlineButton.setToggleState (shown, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton playButton { "Play" }, startButton { "|<" }, importButton { "Import..." };
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, zoomFit { "Fit" }, panelButton { "Panel" }, audioButton { "Audio" }, outlineButton { "Outline" };
    juce::Label timeLabel, statusLabel, nameLabel;
    float peak[2] { 0.0f, 0.0f }, held[2] { 0.0f, 0.0f };
    bool statusIsWarning = false;
};
