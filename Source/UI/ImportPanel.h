// ImportPanel.h — a small floating panel over the timeline while sounds are
// being converted: one row per file (name, step, progress bar), and Cancel.
// It disappears a moment after the last file is done.

#pragma once

#include <JuceHeader.h>
#include "../Engine/Importer.h"

class ImportPanel : public juce::Component
{
public:
    ImportPanel();

    void add (std::shared_ptr<projet::ImportProgress>);
    /** Call at the display rate; returns true while the panel should be shown. */
    bool update();

    void paint (juce::Graphics&) override;
    void resized() override;

    int preferredHeight() const;

private:
    std::vector<std::shared_ptr<projet::ImportProgress>> jobs;
    juce::TextButton cancelButton { "Cancel" };
    static constexpr int rowHeight = 38, maxRows = 6, headerHeight = 34, footerHeight = 40;
};
