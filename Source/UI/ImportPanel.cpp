// ImportPanel.cpp

#include "ImportPanel.h"
#include "Theme.h"

ImportPanel::ImportPanel()
{
    cancelButton.setTooltip ("Stop the imports that are still running");
    cancelButton.onClick = [this]
    {
        for (auto& j : jobs)
            if (! j->isFinished()) j->cancel.store (true);
    };
    addAndMakeVisible (cancelButton);
    setInterceptsMouseClicks (true, true);
}

void ImportPanel::add (std::shared_ptr<projet::ImportProgress> job)
{
    jobs.push_back (std::move (job));
}

bool ImportPanel::update()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    bool anyRunning = false;

    for (auto& j : jobs)
    {
        if (j->isFinished()) { if (j->finishedAt <= 0.0) j->finishedAt = now; }
        else anyRunning = true;
    }

    // Forget finished rows once everything has been done for 1.5 s.
    if (! anyRunning)
    {
        bool allOld = true;
        for (auto& j : jobs) allOld = allOld && now - j->finishedAt > 1500.0;
        if (allOld) jobs.clear();
    }

    cancelButton.setEnabled (anyRunning);
    repaint();
    return ! jobs.empty();
}

int ImportPanel::preferredHeight() const
{
    const int rows = juce::jmin ((int) jobs.size(), maxRows) + ((int) jobs.size() > maxRows ? 1 : 0);
    return headerHeight + rows * rowHeight + footerHeight;
}

void ImportPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 8.0f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (r.reduced (0.5f), 8.0f);
    g.setColour (Theme::lineStrong);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    int running = 0, finished = 0;
    for (auto& j : jobs) (j->isFinished() ? finished : running)++;

    g.setColour (Theme::text);
    g.setFont (Theme::font (13.0f, true));
    const juce::String title = running > 0
        ? "Importing " + juce::String (running) + (running == 1 ? " sound" : " sounds") + (finished > 0 ? "  (" + juce::String (finished) + " done)" : "")
        : "Import finished";
    g.drawText (title, 14, 0, getWidth() - 28, headerHeight, juce::Justification::centredLeft);

    int y = headerHeight;
    for (int i = 0; i < juce::jmin ((int) jobs.size(), maxRows); ++i)
    {
        auto& j = *jobs[(size_t) i];
        const int phase = j.phase.load();
        const float f = juce::jlimit (0.0f, 1.0f, j.fraction.load());

        g.setColour (Theme::text);
        g.setFont (Theme::font (12.5f));
        g.drawText (j.name, 14, y, getWidth() - 150, 18, juce::Justification::centredLeft, true);

        juce::String step = projet::ImportProgress::phaseName (phase);
        if (phase == projet::ImportProgress::reading || phase == projet::ImportProgress::converting || phase == projet::ImportProgress::encoding)
            step << "  " << juce::roundToInt (f * 100.0f) << "%";
        g.setColour (phase == projet::ImportProgress::failed ? juce::Colour (0xffd85a30)
                     : phase == projet::ImportProgress::done ? juce::Colour (0xff4ea597) : Theme::textMuted);
        g.setFont (Theme::font (11.5f));
        g.drawText (step, getWidth() - 160, y, 146, 18, juce::Justification::centredRight);

        // Overall bar: reading 0-15 %, converting 15-60 %, encoding 60-100 % (a rough guide).
        float overall = 0.0f;
        switch (phase)
        {
            case projet::ImportProgress::reading:    overall = 0.15f * f; break;
            case projet::ImportProgress::converting: overall = 0.15f + 0.45f * f; break;
            case projet::ImportProgress::encoding:   overall = 0.60f + 0.40f * f; break;
            case projet::ImportProgress::done:       overall = 1.0f; break;
            default: break;
        }
        auto bar = juce::Rectangle<float> (14.0f, (float) y + 22.0f, (float) getWidth() - 28.0f, 5.0f);
        g.setColour (Theme::panel2);
        g.fillRoundedRectangle (bar, 2.5f);
        g.setColour (phase == projet::ImportProgress::done ? juce::Colour (0xff4ea597) : Theme::accent);
        if (phase != projet::ImportProgress::failed && phase != projet::ImportProgress::cancelled)
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * overall), 2.5f);

        y += rowHeight;
    }

    if ((int) jobs.size() > maxRows)
    {
        g.setColour (Theme::textMuted);
        g.setFont (Theme::font (12.0f));
        g.drawText ("and " + juce::String ((int) jobs.size() - maxRows) + " more", 14, y, getWidth() - 28, rowHeight, juce::Justification::centredLeft);
    }
}

void ImportPanel::resized()
{
    cancelButton.setBounds (getWidth() - 14 - 80, getHeight() - footerHeight + 6, 80, 26);
}
