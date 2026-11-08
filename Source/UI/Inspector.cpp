// Inspector.cpp

#include "Inspector.h"
#include "Theme.h"
#include "TimeFormat.h"
#include "../Model/Proportion.h"

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

    // Kind menu ids
    constexpr int kindSound = 1, kindText = 2, kindContainer = 3, kindBoard = 4, kindCodebox = 5;
}

Inspector::Inspector (Project& p, projet::SoundPool& sp, const ProjectFolder& f)
    : project (p), pool (sp), folder (f), notes (p, false)
{
    title.setFont (Theme::font (12.0f, true));
    title.setColour (juce::Label::textColourId, Theme::textMuted);
    addAndMakeVisible (title);

    for (auto* l : { &nameLabel, &kindLabel, &soundLabel, &gainLabel, &fadeInLabel, &fadeOutLabel, &opacityLabel,
                     &formatLabel, &qualityLabel, &notesLabel, &propLabel, &startLabel, &lengthLabel, &pieceLengthLabel })
    {
        styleLabel (*l);
        addAndMakeVisible (*l);
    }

    nameEditor.setFont (Theme::font (14.0f));
    nameEditor.onReturnKey = [this] { commitName(); };
    nameEditor.onFocusLost = [this] { commitName(); };
    addAndMakeVisible (nameEditor);

    kindBox.addItem ("Sound", kindSound);
    kindBox.addItem ("Text", kindText);
    kindBox.addSeparator();
    kindBox.addItem ("Container (stage 5)", kindContainer);
    kindBox.addItem ("Board (stage 11)", kindBoard);
    kindBox.addItem ("Codebox (later)", kindCodebox);
    kindBox.setItemEnabled (kindContainer, false);
    kindBox.setItemEnabled (kindBoard, false);
    kindBox.setItemEnabled (kindCodebox, false);
    kindBox.onChange = [this]
    {
        if (updating || ! box.isValid()) return;
        const auto kind = kindBox.getSelectedId() == kindText ? "text" : "sound";
        project.getUndoManager().beginNewTransaction ("Change kind");
        project.setKind (box, kind);
    };
    addAndMakeVisible (kindBox);

    soundBox.setTextWhenNothingSelected ("Choose a sound...");
    soundBox.setTextWhenNoChoicesAvailable ("No sounds imported yet");
    soundBox.onChange = [this]
    {
        if (updating || ! box.isValid() || soundBox.getSelectedItemIndex() < 0) return;
        auto s = project.getSounds().getChild (soundBox.getSelectedItemIndex());
        if (! s.isValid() || s[IDs::id].toString() == box[IDs::soundId].toString()) return;
        project.getUndoManager().beginNewTransaction ("Change sound");
        project.setSound (box, s[IDs::id].toString());
    };
    addAndMakeVisible (soundBox);

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
    times.setFont (Theme::mono (11.5f));
    soundInfo.setFont (Theme::font (11.5f));
    soundInfo.setColour (juce::Label::textColourId, Theme::textMuted);

    revealPlaceholder.setTooltip ("Show the placeholder copy in the Finder");
    revealOriginal.setTooltip ("Show the original sound file in the Finder");
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

    // Proportions
    frameBox.setTextWhenNothingSelected ("No frame");
    frameBox.onChange = [this]
    {
        if (updating || ! box.isValid()) return;
        const int i = frameBox.getSelectedId() - 1;
        if (i < 0 || i >= frameRefs.size()) return;
        const auto ref = frameRefs[i];
        if (ref == Project::boundFrame (box)) return;
        project.getUndoManager().beginNewTransaction (ref.isEmpty() ? "Remove proportions" : "Give proportions");
        project.bindBox (box, ref);
    };
    addAndMakeVisible (frameBox);
    for (auto* field : { &startField, &lengthField })
    {
        field->setFont (Theme::mono (13.0f));
        field->setTooltip ("A proportion of the frame: 3/11, 0.25, 27%, 2/3 of 3/4, 1-1/phi, golden, 3:8 ...");
        addAndMakeVisible (*field);
    }
    startField.onReturnKey = [this] { commitProportion (true); };
    startField.onFocusLost = [this] { commitProportion (true); };
    lengthField.onReturnKey = [this] { commitProportion (false); };
    lengthField.onFocusLost = [this] { commitProportion (false); };
    for (auto* l : { &propInfo, &propError })
    {
        styleLabel (*l, 11.0f, Theme::text);
        l->setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (*l);
    }
    propInfo.setFont (Theme::mono (11.0f));
    propError.setColour (juce::Label::textColourId, juce::Colour (0xffe07a6a));
    lockToggle.setColour (juce::ToggleButton::textColourId, Theme::text);
    lockToggle.setColour (juce::ToggleButton::tickColourId, Theme::accent);
    lockToggle.onClick = [this]
    {
        if (updating || ! box.isValid()) return;
        project.getUndoManager().beginNewTransaction ("Keep proportions");
        project.setLock (box, lockToggle.getToggleState());
    };
    addAndMakeVisible (lockToggle);

    pieceLength.setFont (Theme::mono (13.0f));
    pieceLength.onReturnKey = [this] { commitPieceLength(); };
    pieceLength.onFocusLost = [this] { commitPieceLength(); };
    addAndMakeVisible (pieceLength);

    notes.onExpand = [this] { if (onExpandNotes) onExpandNotes(); };
    addAndMakeVisible (notes);

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

    setMode (Mode::project);
}

void Inspector::setSelection (const juce::Array<juce::ValueTree>& boxes)
{
    if (nameEditor.hasKeyboardFocus (true)) commitName();
    selectionSize = boxes.size();
    box = boxes.size() == 1 ? boxes.getFirst() : juce::ValueTree();
    notes.setBox (box);
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

void Inspector::commitProportion (bool start)
{
    if (updating || ! box.isValid() || ! project.isBound (box)) return;
    auto& field = start ? startField : lengthField;
    const auto text = field.getText().trim();
    const auto& id = start ? IDs::propStart : IDs::propLength;
    if (text == box[id].toString()) return;

    juce::String error;
    project.getUndoManager().beginNewTransaction (start ? "Proportional start" : "Proportional length");
    if (! project.setProportion (box, start, text, error))
        propError.setText (error, juce::dontSendNotification);
    else
        propError.setText ({}, juce::dontSendNotification);
}

void Inspector::commitPieceLength()
{
    if (updating) return;
    const auto text = pieceLength.getText().trim();
    double v = 0.0;
    if (text.isNotEmpty() && ! text.startsWithIgnoreCase ("auto"))
    {
        v = TimeFormat::parse (text);
        if (v < 1.0) { refresh(); return; }
    }
    if (std::abs (v - (double) project.getState().getProperty (IDs::frameLength, 0.0)) < 1.0e-9) return;
    project.getUndoManager().beginNewTransaction ("Piece length");
    project.setPieceLength (v);
}

void Inspector::refreshProportions()
{
    // The frames this box can use: none, the piece, the span, and any box that does not follow it.
    frameBox.clear (juce::dontSendNotification);
    frameRefs.clear();
    auto add = [this] (const juce::String& ref, const juce::String& text, bool enabled)
    {
        frameRefs.add (ref);
        frameBox.addItem (text, frameRefs.size());
        frameBox.setItemEnabled (frameRefs.size(), enabled);
    };
    add ({}, "None", true);
    add ("piece", "The piece (" + TimeFormat::format (project.getPieceLength(), 0) + ")", true);
    add ("span", "The span on the ruler", project.hasSpan());
    for (auto b : project.allBoxes())
        if (b != box && project.canUseAsFrame (box, b[IDs::id].toString()))
            add (b[IDs::id].toString(), "Box: " + b[IDs::name].toString() + "  (" + b.getParent()[IDs::name].toString() + ")", true);

    const auto ref = Project::boundFrame (box);
    const int index = frameRefs.indexOf (ref);
    frameBox.setSelectedId (index >= 0 ? index + 1 : 1, juce::dontSendNotification);

    const bool bound = project.isBound (box);
    startField.setEnabled (bound);
    lengthField.setEnabled (bound);
    lockToggle.setEnabled (bound);
    lockToggle.setToggleState (bound && Project::isLocked (box), juce::dontSendNotification);

    if (! bound)
    {
        if (! startField.hasKeyboardFocus (false)) startField.setText ({}, false);
        if (! lengthField.hasKeyboardFocus (false)) lengthField.setText ({}, false);
        propInfo.setText (ref.isNotEmpty() ? "Its frame is gone (undo brings it back)." : "Choose a frame to type the start and length as proportions.",
                          juce::dontSendNotification);
        return;
    }

    const auto extra = project.getDenominators();
    const auto f = project.frameFor (ref);
    const bool locked = Project::isLocked (box);
    // Locked: what was typed. Unlocked: where the box is now.
    const auto startText = locked ? box[IDs::propStart].toString() : Proportion::name (project.actualProportion (box, true), extra);
    const auto lengthText = locked ? box[IDs::propLength].toString() : Proportion::name (project.actualProportion (box, false), extra);
    if (! startField.hasKeyboardFocus (false)) startField.setText (startText, false);
    if (! lengthField.hasKeyboardFocus (false)) lengthField.setText (lengthText, false);

    const double ps = project.actualProportion (box, true), pl = project.actualProportion (box, false);
    juce::String info;
    info << "start " << Proportion::describe (ps, extra) << " = " << TimeFormat::format (Project::boxStart (box), 2) << "\n"
         << "length " << Proportion::describe (pl, extra) << " = " << TimeFormat::format (Project::boxLength (box), 2) << "\n"
         << "end " << Proportion::describe (ps + pl, extra) << " of " << project.frameName (ref)
         << " (" << TimeFormat::format (f.length, 1) << ")";
    if (ref != "piece")
    {
        const auto piece = project.frameFor ("piece");
        info << "\n= " << Proportion::describe (piece.proportionOf (Project::boxStart (box)), extra) << " to "
             << Proportion::describe (piece.proportionOf (Project::boxEnd (box)), extra) << " of the piece";
    }
    propInfo.setText (info, juce::dontSendNotification);
}

void Inspector::setMode (Mode m)
{
    mode = m;
    const bool anyBox = m == Mode::sound || m == Mode::text;
    const bool sound = m == Mode::sound;

    for (juce::Component* c : std::initializer_list<juce::Component*> { &nameLabel, &nameEditor, &kindLabel, &kindBox, &opacityLabel, &opacity,
                                                                        &times, &notesLabel, &notes, &propLabel, &frameBox, &startLabel,
                                                                        &lengthLabel, &startField, &lengthField, &propInfo, &propError, &lockToggle })
        c->setVisible (anyBox);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &soundLabel, &soundBox, &gainLabel, &gain, &fadeInLabel, &fadeIn, &fadeInShape,
                                                                        &fadeOutLabel, &fadeOut, &fadeOutShape, &soundInfo,
                                                                        &revealPlaceholder, &revealOriginal })
        c->setVisible (sound);

    const bool proj = m == Mode::project;
    projectInfo.setVisible (proj || m == Mode::several);
    formatLabel.setVisible (proj);
    formatBox.setVisible (proj);
    quality.setVisible (proj && project.getPlaceholderFormat() == "ogg");
    qualityLabel.setVisible (quality.isVisible());
    pieceLength.setVisible (proj);
    pieceLengthLabel.setVisible (proj);
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

    int sounds = 0, texts = 0;
    for (auto b : project.allBoxes()) (Project::isSound (b) ? sounds : texts)++;

    juce::String s;
    s << project.getName() << "\n\n"
      << "Sample rate: " << juce::String (project.getSampleRate() / 1000.0, 1) << " kHz\n"
      << "Tracks: " << project.getNumTracks() << "\n"
      << "Boxes: " << sounds << " sound, " << texts << " text\n"
      << "Sounds: " << count << " (" << juce::File::descriptionOfSizeInBytes (bytes) << ")\n"
      << "Length: " << TimeFormat::format (project.getContentEnd(), 1) << "\n\n"
      << "Folder:\n" << folder.getFolder().getFullPathName();
    return s;
}

void Inspector::fillSoundList()
{
    soundBox.clear (juce::dontSendNotification);
    int index = 1, selected = 0;
    for (auto s : project.getSounds())
    {
        soundBox.addItem (s[IDs::name].toString(), index);
        if (s[IDs::id].toString() == box[IDs::soundId].toString()) selected = index;
        ++index;
    }
    soundBox.setSelectedId (selected, juce::dontSendNotification);
}

void Inspector::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);

    if (box.isValid() && ! box.getParent().isValid()) { box = {}; selectionSize = 0; notes.setBox ({}); }   // deleted
    if (! box.isValid()) propError.setText ({}, juce::dontSendNotification);

    if (selectionSize > 1)
    {
        setMode (Mode::several);
        title.setText (juce::String (selectionSize) + " BOXES SELECTED", juce::dontSendNotification);
        projectInfo.setText ("Move them together by dragging one of them. Cmd-C / Cmd-X copy or cut them, "
                             "Cmd-T splits them at the playhead, Delete removes them.", juce::dontSendNotification);
        resized();
        repaint();
        return;
    }

    if (! box.isValid())
    {
        setMode (Mode::project);
        title.setText ("PROJECT", juce::dontSendNotification);
        projectInfo.setText (projectSummary(), juce::dontSendNotification);
        formatBox.setSelectedId (project.getPlaceholderFormat() == "flac" ? 2 : 1, juce::dontSendNotification);
        quality.setValue (project.getVorbisQuality(), juce::dontSendNotification);
        if (! pieceLength.hasKeyboardFocus (false))
            pieceLength.setText (project.hasSetPieceLength() ? TimeFormat::format (project.getPieceLength(), 1)
                                                             : "auto (" + TimeFormat::format (project.getPieceLength(), 1) + ")", false);
        resized();
        repaint();
        return;
    }

    const bool sound = Project::isSound (box);
    setMode (sound ? Mode::sound : Mode::text);
    title.setText (sound ? "SOUND BOX" : "TEXT BOX", juce::dontSendNotification);
    if (! nameEditor.hasKeyboardFocus (true))
        nameEditor.setText (box[IDs::name].toString(), false);
    kindBox.setSelectedId (sound ? kindSound : kindText, juce::dontSendNotification);
    opacity.setValue (box.getProperty (IDs::opacity, 1.0), juce::dontSendNotification);

    juce::String t;
    t << TimeFormat::format (Project::boxStart (box)) << " - " << TimeFormat::format (Project::boxEnd (box))
      << "   length " << TimeFormat::format (Project::boxLength (box));
    if (sound) t << "\noffset into the sound " << TimeFormat::format ((double) box.getProperty (IDs::offset, 0.0));
    times.setText (t, juce::dontSendNotification);

    if (sound)
    {
        fillSoundList();
        gain.setValue (box.getProperty (IDs::gainDb, 0.0), juce::dontSendNotification);
        const double lenMs = Project::boxLength (box) * 1000.0;
        for (auto* s : { &fadeIn, &fadeOut })
            s->setRange (0.0, juce::jmax (1.0, std::floor (lenMs)), 1.0);
        fadeIn.setValue ((double) box.getProperty (IDs::fadeIn, 0.0) * 1000.0, juce::dontSendNotification);
        fadeOut.setValue ((double) box.getProperty (IDs::fadeOut, 0.0) * 1000.0, juce::dontSendNotification);
        fadeInShape.setSelectedId ((int) box.getProperty (IDs::fadeInShape, 2) + 1, juce::dontSendNotification);
        fadeOutShape.setSelectedId ((int) box.getProperty (IDs::fadeOutShape, 2) + 1, juce::dontSendNotification);

        auto s = project.findSound (box[IDs::soundId].toString());
        juce::String si;
        if (s.isValid())
        {
            const auto copy = folder.resolve (s[IDs::file].toString());
            const juce::File original (s[IDs::originalPath].toString());
            si << copy.getFileName() << " (" << s[IDs::format].toString().toUpperCase() << ", "
               << juce::File::descriptionOfSizeInBytes (copy.getSize()) << "), "
               << TimeFormat::format (project.soundSeconds (s[IDs::id].toString()), 1) << "\n"
               << "from " << original.getFileName() << ", " << juce::String ((double) s[IDs::originalRate] / 1000.0, 1) << " kHz"
               << (original.existsAsFile() ? "" : " (no longer there)");
            if (pool.get (s[IDs::id].toString()) == nullptr) si << "\n(loading...)";
        }
        else
            si << "No sound: choose one above.";
        soundInfo.setText (si, juce::dontSendNotification);
        revealPlaceholder.setEnabled (s.isValid());
        revealOriginal.setEnabled (s.isValid() && juce::File (s[IDs::originalPath].toString()).existsAsFile());
    }

    refreshProportions();
    notes.refresh();
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
    r.removeFromTop (6);

    auto labelled = [&r] (juce::Label& l, juce::Component& c, int h)
    {
        l.setBounds (r.removeFromTop (17));
        c.setBounds (r.removeFromTop (h));
        r.removeFromTop (7);
    };

    if (mode == Mode::sound || mode == Mode::text)
    {
        labelled (nameLabel, nameEditor, 26);

        auto row = r.removeFromTop (17);
        kindLabel.setBounds (row.removeFromLeft (100));
        row.removeFromLeft (8);
        soundLabel.setBounds (row);
        row = r.removeFromTop (24);
        kindBox.setBounds (row.removeFromLeft (100));
        row.removeFromLeft (8);
        soundBox.setBounds (row);
        r.removeFromTop (7);

        if (mode == Mode::sound)
        {
            labelled (gainLabel, gain, 22);

            fadeInLabel.setBounds (r.removeFromTop (17));
            row = r.removeFromTop (22);
            fadeInShape.setBounds (row.removeFromRight (104));
            row.removeFromRight (6);
            fadeIn.setBounds (row);
            r.removeFromTop (5);
            fadeOutLabel.setBounds (r.removeFromTop (17));
            row = r.removeFromTop (22);
            fadeOutShape.setBounds (row.removeFromRight (104));
            row.removeFromRight (6);
            fadeOut.setBounds (row);
            r.removeFromTop (7);
        }

        labelled (opacityLabel, opacity, 22);
        times.setBounds (r.removeFromTop (mode == Mode::sound ? 32 : 18));
        r.removeFromTop (4);

        propLabel.setBounds (r.removeFromTop (17));
        frameBox.setBounds (r.removeFromTop (24));
        r.removeFromTop (5);
        row = r.removeFromTop (15);
        startLabel.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
        row.removeFromLeft (6);
        lengthLabel.setBounds (row);
        row = r.removeFromTop (26);
        startField.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
        row.removeFromLeft (6);
        lengthField.setBounds (row);
        r.removeFromTop (3);
        propError.setBounds (r.removeFromTop (propError.getText().isEmpty() ? 0 : 30));
        propInfo.setBounds (r.removeFromTop (Project::boundFrame (box) != "piece" && project.isBound (box) ? 58 : 44));
        lockToggle.setBounds (r.removeFromTop (22));
        r.removeFromTop (8);

        if (mode == Mode::sound)
        {
            soundInfo.setBounds (r.removeFromTop (44));
            row = r.removeFromTop (24);
            revealPlaceholder.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
            row.removeFromLeft (6);
            revealOriginal.setBounds (row);
            r.removeFromTop (8);
        }

        notesLabel.setBounds (r.removeFromTop (17));
        notes.setBounds (r);
    }
    else if (mode == Mode::several)
    {
        projectInfo.setBounds (r.removeFromTop (120));
    }
    else
    {
        projectInfo.setBounds (r.removeFromTop (200));
        r.removeFromTop (10);
        formatLabel.setBounds (r.removeFromTop (18));
        formatBox.setBounds (r.removeFromTop (26));
        r.removeFromTop (10);
        qualityLabel.setBounds (r.removeFromTop (18));
        quality.setBounds (r.removeFromTop (24));
        r.removeFromTop (10);
        pieceLengthLabel.setBounds (r.removeFromTop (18));
        pieceLength.setBounds (r.removeFromTop (26));
    }
}
