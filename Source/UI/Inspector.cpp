// Inspector.cpp

#include "Inspector.h"
#include "Theme.h"
#include "TimeFormat.h"

namespace
{
    void styleLabel (juce::Label& l, float size = 12.0f, juce::Colour c = Theme::textMuted)
    {
        l.setFont (Theme::font (size));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (juce::Justification::centredLeft);
    }

    void styleSlider (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    }
}

Inspector::Inspector (Project& p, projet::SoundPool& sp, const ProjectFolder& f)
    : project (p), pool (sp), folder (f)
{
    styleLabel (title, 12.0f);
    title.setFont (Theme::font (12.0f, true));
    addAndMakeVisible (title);

    for (auto* l : { &nameLabel, &gainLabel, &fadeInLabel, &fadeOutLabel, &opacityLabel, &formatLabel, &qualityLabel })
    {
        styleLabel (*l);
        addAndMakeVisible (*l);
    }

    nameEditor.setFont (Theme::font (14.0f));
    nameEditor.onReturnKey = [this] { commitName(); };
    nameEditor.onFocusLost = [this] { commitName(); };
    addAndMakeVisible (nameEditor);

    styleSlider (gain);
    gain.setRange (-60.0, 12.0, 0.1);
    gain.setDoubleClickReturnValue (true, 0.0);
    gain.setSkewFactorFromMidPoint (-12.0);
    gain.onDragStart = [this] { project.getUndoManager().beginNewTransaction ("Gain"); };
    gain.onValueChange = [this]
    {
        if (updating || ! box.isValid()) return;
        box.setProperty (IDs::gainDb, gain.getValue(), project.um());
    };
    addAndMakeVisible (gain);

    for (auto* s : { &fadeIn, &fadeOut })
    {
        styleSlider (*s);
        s->setRange (0.0, 60000.0, 1.0);
        s->setSkewFactorFromMidPoint (1000.0);
        s->onDragStart = [this] { project.getUndoManager().beginNewTransaction ("Fade"); };
        s->onValueChange = [this]
        {
            if (updating || ! box.isValid()) return;
            project.setFades (box, fadeIn.getValue() * 0.001, fadeOut.getValue() * 0.001);
        };
        addAndMakeVisible (*s);
    }

    for (auto* c : { &fadeInShape, &fadeOutShape })
    {
        for (int i = 0; i < numFadeShapes; ++i)
            c->addItem (fadeShapeName ((FadeShape) i), i + 1);
        addAndMakeVisible (*c);
    }
    fadeInShape.onChange = [this]
    {
        if (updating || ! box.isValid()) return;
        project.getUndoManager().beginNewTransaction ("Fade shape");
        box.setProperty (IDs::fadeInShape, fadeInShape.getSelectedId() - 1, project.um());
    };
    fadeOutShape.onChange = [this]
    {
        if (updating || ! box.isValid()) return;
        project.getUndoManager().beginNewTransaction ("Fade shape");
        box.setProperty (IDs::fadeOutShape, fadeOutShape.getSelectedId() - 1, project.um());
    };

    styleSlider (opacity);
    opacity.setRange (0.15, 1.0, 0.01);
    opacity.setDoubleClickReturnValue (true, 1.0);
    opacity.onDragStart = [this] { project.getUndoManager().beginNewTransaction ("Opacity"); };
    opacity.onValueChange = [this]
    {
        if (updating || ! box.isValid()) return;
        box.setProperty (IDs::opacity, opacity.getValue(), project.um());
    };
    addAndMakeVisible (opacity);

    for (auto* l : { &times, &soundInfo, &projectInfo })
    {
        styleLabel (*l, 12.0f, Theme::text);
        l->setJustificationType (juce::Justification::topLeft);
        l->setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (*l);
    }
    times.setFont (Theme::mono (12.0f));

    revealPlaceholder.onClick = [this]
    {
        auto s = project.findSound (box[IDs::soundId].toString());
        if (onReveal && s.isValid()) onReveal (folder.resolve (s[IDs::file].toString()));
    };
    revealOriginal.onClick = [this]
    {
        auto s = project.findSound (box[IDs::soundId].toString());
        if (onReveal && s.isValid()) onReveal (juce::File (s[IDs::originalPath].toString()));
    };
    addAndMakeVisible (revealPlaceholder);
    addAndMakeVisible (revealOriginal);

    formatBox.addItem ("Ogg Vorbis (small)", 1);
    formatBox.addItem ("FLAC 24-bit (lossless)", 2);
    formatBox.onChange = [this]
    {
        if (updating) return;
        project.getState().setProperty (IDs::placeholderFormat, formatBox.getSelectedId() == 2 ? "flac" : "ogg", nullptr);
        refresh();
    };
    addAndMakeVisible (formatBox);

    styleSlider (quality);
    quality.setRange (3.0, 10.0, 1.0);
    quality.textFromValueFunction = [] (double v) { return "q" + juce::String (v / 10.0, 1); };
    quality.onValueChange = [this]
    {
        if (updating) return;
        project.getState().setProperty (IDs::vorbisQuality, (int) quality.getValue(), nullptr);
    };
    addAndMakeVisible (quality);

    showBox (false);
}

void Inspector::setBox (const juce::ValueTree& b)
{
    if (nameEditor.hasKeyboardFocus (true)) commitName();
    box = b;
    refresh();
}

void Inspector::commitName()
{
    if (updating || ! box.isValid()) return;
    const auto n = nameEditor.getText().trim();
    if (n.isNotEmpty() && n != box[IDs::name].toString())
    {
        project.getUndoManager().beginNewTransaction ("Rename box");
        box.setProperty (IDs::name, n, project.um());
    }
}

void Inspector::showBox (bool b)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &nameLabel, &nameEditor, &gainLabel, &gain, &fadeInLabel, &fadeIn, &fadeInShape,
                                &fadeOutLabel, &fadeOut, &fadeOutShape, &opacityLabel, &opacity, &times, &soundInfo,
                                &revealPlaceholder, &revealOriginal })
        c->setVisible (b);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &projectInfo, &formatLabel, &formatBox, &qualityLabel, &quality })
        c->setVisible (! b);

    quality.setVisible (! b && project.getPlaceholderFormat() == "ogg");
    qualityLabel.setVisible (quality.isVisible());
}

juce::String Inspector::projectSummary()
{
    juce::int64 bytes = 0;
    int count = 0;
    for (auto s : project.getSounds())
    {
        bytes += folder.resolve (s[IDs::file].toString()).getSize();
        ++count;
    }

    int boxes = 0;
    for (auto t : project.getTracks()) boxes += t.getNumChildren();

    juce::String s;
    s << project.getName() << "\n\n"
      << "Sample rate: " << juce::String (project.getSampleRate() / 1000.0, 1) << " kHz\n"
      << "Tracks: " << project.getNumTracks() << ",  boxes: " << boxes << "\n"
      << "Sounds: " << count << " (" << juce::File::descriptionOfSizeInBytes (bytes) << ")\n"
      << "Length: " << TimeFormat::format (project.getContentEnd(), 1) << "\n\n"
      << "Folder:\n" << folder.getFolder().getFullPathName();
    return s;
}

void Inspector::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);

    if (box.isValid() && ! box.getParent().isValid()) box = {};   // deleted
    showBox (box.isValid());

    if (! box.isValid())
    {
        title.setText ("PROJECT", juce::dontSendNotification);
        projectInfo.setText (projectSummary(), juce::dontSendNotification);
        formatBox.setSelectedId (project.getPlaceholderFormat() == "flac" ? 2 : 1, juce::dontSendNotification);
        quality.setValue (project.getVorbisQuality(), juce::dontSendNotification);
        resized();
        repaint();
        return;
    }

    title.setText ("SOUND BOX", juce::dontSendNotification);
    if (! nameEditor.hasKeyboardFocus (true))
        nameEditor.setText (box[IDs::name].toString(), false);
    gain.setValue (box[IDs::gainDb], juce::dontSendNotification);

    const double lenMs = Project::boxLength (box) * 1000.0;
    for (auto* s : { &fadeIn, &fadeOut })
        s->setRange (0.0, juce::jmax (1.0, std::floor (lenMs)), 1.0);
    fadeIn.setValue ((double) box[IDs::fadeIn] * 1000.0, juce::dontSendNotification);
    fadeOut.setValue ((double) box[IDs::fadeOut] * 1000.0, juce::dontSendNotification);
    fadeInShape.setSelectedId ((int) box[IDs::fadeInShape] + 1, juce::dontSendNotification);
    fadeOutShape.setSelectedId ((int) box[IDs::fadeOutShape] + 1, juce::dontSendNotification);
    opacity.setValue (box.getProperty (IDs::opacity, 1.0), juce::dontSendNotification);

    juce::String t;
    t << "Start   " << TimeFormat::format (Project::boxStart (box)) << "\n"
      << "End     " << TimeFormat::format (Project::boxEnd (box)) << "\n"
      << "Length  " << TimeFormat::format (Project::boxLength (box)) << "\n"
      << "Offset  " << TimeFormat::format ((double) box[IDs::offset]) << "  (into the sound)";
    times.setText (t, juce::dontSendNotification);

    auto sound = project.findSound (box[IDs::soundId].toString());
    juce::String si;
    if (sound.isValid())
    {
        const auto copy = folder.resolve (sound[IDs::file].toString());
        si << "Copy: " << copy.getFileName() << " (" << sound[IDs::format].toString().toUpperCase() << ", "
           << juce::File::descriptionOfSizeInBytes (copy.getSize()) << ")\n"
           << "Length: " << TimeFormat::format (project.soundSeconds (sound[IDs::id].toString())) << "\n"
           << "Original: " << juce::File (sound[IDs::originalPath].toString()).getFileName() << "\n"
           << "   " << juce::String ((double) sound[IDs::originalRate] / 1000.0, 1) << " kHz, "
           << (int) sound[IDs::originalChannels] << ((int) sound[IDs::originalChannels] == 1 ? " channel" : " channels");
        if (! juce::File (sound[IDs::originalPath].toString()).existsAsFile()) si << "\n   (the original is no longer there)";
        if (pool.get (sound[IDs::id].toString()) == nullptr) si << "\n(loading...)";
    }
    soundInfo.setText (si, juce::dontSendNotification);
    revealOriginal.setEnabled (sound.isValid() && juce::File (sound[IDs::originalPath].toString()).existsAsFile());
    resized();
    repaint();
}

void Inspector::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::line);
    g.fillRect (0, 0, 1, getHeight());
}

void Inspector::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    title.setBounds (r.removeFromTop (20));
    r.removeFromTop (8);

    if (box.isValid())
    {
        nameLabel.setBounds (r.removeFromTop (18));
        nameEditor.setBounds (r.removeFromTop (28));
        r.removeFromTop (10);
        gainLabel.setBounds (r.removeFromTop (18));
        gain.setBounds (r.removeFromTop (24));
        r.removeFromTop (10);

        fadeInLabel.setBounds (r.removeFromTop (18));
        auto row = r.removeFromTop (24);
        fadeInShape.setBounds (row.removeFromRight (110));
        row.removeFromRight (6);
        fadeIn.setBounds (row);
        r.removeFromTop (6);
        fadeOutLabel.setBounds (r.removeFromTop (18));
        row = r.removeFromTop (24);
        fadeOutShape.setBounds (row.removeFromRight (110));
        row.removeFromRight (6);
        fadeOut.setBounds (row);
        r.removeFromTop (10);

        opacityLabel.setBounds (r.removeFromTop (18));
        opacity.setBounds (r.removeFromTop (24));
        r.removeFromTop (14);

        times.setBounds (r.removeFromTop (70));
        r.removeFromTop (8);
        soundInfo.setBounds (r.removeFromTop (110));
        revealPlaceholder.setBounds (r.removeFromTop (26));
        r.removeFromTop (6);
        revealOriginal.setBounds (r.removeFromTop (26));
    }
    else
    {
        projectInfo.setBounds (r.removeFromTop (190));
        r.removeFromTop (10);
        formatLabel.setBounds (r.removeFromTop (18));
        formatBox.setBounds (r.removeFromTop (26));
        r.removeFromTop (10);
        qualityLabel.setBounds (r.removeFromTop (18));
        quality.setBounds (r.removeFromTop (24));
    }
}
