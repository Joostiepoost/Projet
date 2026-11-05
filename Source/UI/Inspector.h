// Inspector.h — the panel on the right.
//
// With a box selected: its name, gain, fades, opacity, times and sound. With
// nothing selected: the project (sample rate, placeholder format and quality,
// sounds).

#pragma once

#include <JuceHeader.h>
#include "../Model/Project.h"
#include "../Model/ProjectFolder.h"
#include "../Engine/SoundData.h"

class Inspector : public juce::Component
{
public:
    Inspector (Project&, projet::SoundPool&, const ProjectFolder&);

    void setBox (const juce::ValueTree& box);
    /** Re-reads the model (after any change). */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (const juce::File&)> onReveal;

private:
    void showBox (bool);
    void commitName();
    juce::String projectSummary();

    Project& project;
    projet::SoundPool& pool;
    const ProjectFolder& folder;
    juce::ValueTree box;
    bool updating = false;

    juce::Label title;
    juce::Label nameLabel { {}, "Name" };
    juce::TextEditor nameEditor;
    juce::Label gainLabel { {}, "Gain (dB)" };
    juce::Slider gain;
    juce::Label fadeInLabel { {}, "Fade in (ms)" }, fadeOutLabel { {}, "Fade out (ms)" };
    juce::Slider fadeIn, fadeOut;
    juce::ComboBox fadeInShape, fadeOutShape;
    juce::Label opacityLabel { {}, "Opacity (visual only)" };
    juce::Slider opacity;
    juce::Label times;
    juce::Label soundInfo;
    juce::TextButton revealPlaceholder { "Show copy in Finder" }, revealOriginal { "Show original in Finder" };

    // Project (nothing selected)
    juce::Label projectInfo;
    juce::Label formatLabel { {}, "New imports as" };
    juce::ComboBox formatBox;
    juce::Label qualityLabel { {}, "Vorbis quality" };
    juce::Slider quality;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Inspector)
};
