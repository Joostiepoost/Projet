// TransportBar.cpp

#include "TransportBar.h"
#include "Theme.h"
#include "TimeFormat.h"

TransportBar::TransportBar()
{
    playButton.setClickingTogglesState (false);
    playButton.onClick = [this] { if (onPlayStop) onPlayStop(); };
    startButton.onClick = [this] { if (onToStart) onToStart(); };
    zoomIn.onClick = [this] { if (onZoomIn) onZoomIn(); };
    zoomOut.onClick = [this] { if (onZoomOut) onZoomOut(); };
    zoomFit.onClick = [this] { if (onZoomFit) onZoomFit(); };
    importButton.onClick = [this] { if (onImport) onImport(); };

    playButton.setTooltip ("Play / stop (Space)");
    startButton.setTooltip ("Back to the start (Return)");
    zoomIn.setTooltip ("Zoom in (Cmd-=)");
    zoomOut.setTooltip ("Zoom out (Cmd--)");
    zoomFit.setTooltip ("Show the whole piece (Cmd-0)");
    importButton.setTooltip ("Import sounds (Cmd-I)");

    for (auto* b : { &playButton, &startButton, &zoomIn, &zoomOut, &zoomFit, &importButton })
        addAndMakeVisible (*b);

    nameLabel.setFont (Theme::font (15.0f, true));
    timeLabel.setFont (Theme::mono (17.0f));
    timeLabel.setText ("0:00.000", juce::dontSendNotification);
    statusLabel.setFont (Theme::font (12.5f));
    statusLabel.setColour (juce::Label::textColourId, Theme::textMuted);
    for (auto* l : { &nameLabel, &timeLabel, &statusLabel })
        addAndMakeVisible (*l);
}

void TransportBar::setPlaying (bool p)
{
    playButton.setToggleState (p, juce::dontSendNotification);
    playButton.setButtonText (p ? "Stop" : "Play");
}

void TransportBar::setTime (double seconds)
{
    timeLabel.setText (TimeFormat::format (seconds), juce::dontSendNotification);
}

void TransportBar::setPeaks (float l, float r)
{
    const float in[2] = { l, r };
    bool changed = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float v = juce::jmax (in[ch], peak[ch] * 0.85f);
        if (std::abs (v - peak[ch]) > 0.001f) { peak[ch] = v; changed = true; }
        held[ch] = juce::jmax (in[ch], held[ch] * 0.995f);
    }
    if (changed) repaint (getWidth() - 140, 0, 140, getHeight());
}

void TransportBar::setStatus (const juce::String& text, bool warning)
{
    statusIsWarning = warning;
    statusLabel.setColour (juce::Label::textColourId, warning ? Theme::accent : Theme::textMuted);
    statusLabel.setText (text, juce::dontSendNotification);
}

void TransportBar::setProjectName (const juce::String& name, bool dirty)
{
    nameLabel.setText (name + (dirty ? " *" : ""), juce::dontSendNotification);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // Meters (peak, dBFS, -60..0)
    auto m = juce::Rectangle<int> (getWidth() - 132, 14, 116, 6);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = m.translated (0, ch * 10).toFloat();
        g.setColour (Theme::panel2);
        g.fillRect (bar);
        auto toX = [&] (float v)
        {
            const float db = juce::Decibels::gainToDecibels (v, -60.0f);
            return bar.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        };
        g.setColour (held[ch] >= 1.0f ? juce::Colour (0xffd85a30) : juce::Colour (0xff4ea597));
        g.fillRect (bar.withWidth (toX (peak[ch])));
        g.setColour (Theme::text.withAlpha (0.7f));
        g.fillRect (bar.getX() + toX (held[ch]), bar.getY(), 1.5f, bar.getHeight());
    }
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (12, 9);
    nameLabel.setBounds (r.removeFromLeft (220));
    r.removeFromLeft (8);
    startButton.setBounds (r.removeFromLeft (36));
    r.removeFromLeft (4);
    playButton.setBounds (r.removeFromLeft (64));
    r.removeFromLeft (12);
    timeLabel.setBounds (r.removeFromLeft (110));
    r.removeFromLeft (12);
    importButton.setBounds (r.removeFromLeft (86));
    r.removeFromLeft (16);
    r.removeFromRight (140);
    zoomFit.setBounds (r.removeFromRight (44));
    r.removeFromRight (4);
    zoomIn.setBounds (r.removeFromRight (30));
    r.removeFromRight (4);
    zoomOut.setBounds (r.removeFromRight (30));
    r.removeFromRight (12);
    statusLabel.setBounds (r);
}
