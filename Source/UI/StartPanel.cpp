// StartPanel.cpp

#include "StartPanel.h"
#include "Theme.h"

StartPanel::StartPanel()
{
    newButton.onClick = [this] { if (onNew) onNew(); };
    openButton.onClick = [this] { if (onOpen) onOpen(); };
    newButton.setTooltip ("Make a new project folder (Cmd-N)");
    openButton.setTooltip ("Open a .projet file (Cmd-O)");
    addAndMakeVisible (newButton);
    addAndMakeVisible (openButton);
}

void StartPanel::setRecent (const juce::Array<juce::File>& projects)
{
    recent.clear();
    recentButtons.clear();
    for (auto& f : projects)
    {
        if (! f.existsAsFile() || recent.size() >= 8) continue;
        recent.add (f);
        auto* b = recentButtons.add (new juce::TextButton (f.getFileNameWithoutExtension()));
        b->setTooltip (f.getParentDirectory().getFullPathName());
        b->onClick = [this, f] { if (onOpenRecent) onOpenRecent (f); };
        addAndMakeVisible (b);
    }
    resized();
    repaint();
}

juce::Rectangle<int> StartPanel::card() const
{
    const int h = 172 + (recent.isEmpty() ? 0 : 36 + recent.size() * 44);
    return getLocalBounds().withSizeKeepingCentre (520, juce::jmin (getHeight() - 40, h));
}

void StartPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
    auto c = card();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (c.toFloat(), 10.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (c.toFloat().reduced (0.5f), 10.0f, 1.0f);

    auto r = c.reduced (32, 28);
    g.setColour (Theme::text);
    g.setFont (Theme::font (26.0f, true));
    g.drawText ("Projet", r.removeFromTop (36), juce::Justification::centredLeft);
    g.setColour (Theme::textMuted);
    g.setFont (Theme::font (14.0f));
    g.drawText ("A sketch timeline for acousmatic pieces. Each piece lives in its own folder.",
                r.removeFromTop (24), juce::Justification::centredLeft);

    if (! recent.isEmpty())
    {
        g.setColour (Theme::textMuted);
        g.setFont (Theme::font (12.0f, true));
        g.drawText ("RECENT", c.getX() + 32, c.getY() + 178, 200, 20, juce::Justification::centredLeft);

        g.setFont (Theme::font (11.5f));
        for (int i = 0; i < recentButtons.size(); ++i)
        {
            auto b = recentButtons[i]->getBounds();
            g.drawText (recent[i].getParentDirectory().getParentDirectory().getFullPathName(),
                        b.getRight() + 12, b.getY(), c.getRight() - b.getRight() - 44, b.getHeight(),
                        juce::Justification::centredLeft, true);
        }
    }
}

void StartPanel::resized()
{
    auto c = card();
    auto row = juce::Rectangle<int> (c.getX() + 32, c.getY() + 120, c.getWidth() - 64, 40);
    newButton.setBounds (row.removeFromLeft ((row.getWidth() - 12) / 2));
    row.removeFromLeft (12);
    openButton.setBounds (row);

    int y = c.getY() + 204;
    for (auto* b : recentButtons)
    {
        b->setBounds (c.getX() + 32, y, 200, 34);
        y += 44;
    }
}
