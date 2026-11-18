// BoardPanel.cpp — see BoardPanel.h.

#include "BoardPanel.h"
#include "BoardPainter.h"
#include "Theme.h"

namespace
{
    void styleLabel (juce::Label& l, float size = 12.0f, juce::Colour c = Theme::textMuted)
    {
        l.setFont (Theme::font (size));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (juce::Justification::centredLeft);
    }
}

//== The colour row ============================================================

juce::Rectangle<float> BoardPanel::ColourRow::swatch (int i) const
{
    return { 1.0f + (float) i * 28.0f, 1.0f, 22.0f, 22.0f };
}

void BoardPanel::ColourRow::paint (juce::Graphics& g)
{
    const auto& p = Board::palette();
    for (int i = 0; i < p.size(); ++i)
    {
        const auto r = swatch (i);
        g.setColour (p[i]);
        g.fillEllipse (r.reduced (2.0f));
        if (! mixed && p[i] == current)
        {
            g.setColour (Theme::text);
            g.drawEllipse (r.reduced (0.5f), 2.0f);
        }
    }
}

void BoardPanel::ColourRow::mouseDown (const juce::MouseEvent& e)
{
    const auto& p = Board::palette();
    for (int i = 0; i < p.size(); ++i)
        if (swatch (i).contains (e.position) && onPick) { onPick (p[i]); return; }
}

//== The panel =================================================================

BoardPanel::BoardPanel (Project& p, const Framework& f)
    : project (p), framework (f), notes (p, false)
{
    title.setFont (Theme::font (12.0f, true));
    title.setColour (juce::Label::textColourId, Theme::textMuted);
    addAndMakeVisible (title);
    styleLabel (info, 12.0f, Theme::text.withAlpha (0.85f));
    info.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (info);

    for (auto* l : { &labelLabel, &colourLabel, &levelLabel, &textLabel, &wordsLabel, &widthLabel, &shapeLabel })
    {
        styleLabel (*l);
        addChildComponent (*l);
    }

    labelBox.setEditableText (true);
    labelBox.setTextWhenNothingSelected ("No label");
    labelBox.onChange = [this]
    {
        if (updating) return;
        auto text = labelBox.getText().trim();
        if (text == "No label") text = {};
        setOnAll (IDs::label, text, "Label");
    };
    addChildComponent (labelBox);

    colours.onPick = [this] (juce::Colour c) { setOnAll (IDs::colour, (int) c.getARGB(), "Colour"); };
    addChildComponent (colours);

    levelBox.onChange = [this]
    {
        if (updating) return;
        auto* c = BoardPainter::fillCategory (framework);
        if (c == nullptr) return;
        const int i = levelBox.getSelectedId() - 2;
        const auto& values = framework.valuesOf (*c);
        project.getUndoManager().beginNewTransaction (framework.text (c->name));
        for (auto& item : items)
        {
            if (! item.hasType (IDs::CARD)) continue;
            if (i < 0 || i >= values.size()) item.removeProperty (c->property(), um());
            else item.setProperty (c->property(), values[i].id, um());
        }
    };
    addChildComponent (levelBox);
    addChildComponent (notes);

    words.setFont (Theme::font (14.0f));
    words.onReturnKey = [this] { commitWords(); };
    words.onFocusLost = [this] { commitWords(); };
    addChildComponent (words);

    width.setSliderStyle (juce::Slider::LinearHorizontal);
    width.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    width.setRange (0.5, 8.0, 0.5);
    width.onDragStart = [this] { project.getUndoManager().beginNewTransaction ("Line width"); };
    width.onValueChange = [this]
    {
        if (updating) return;
        for (auto& item : items)
            if (! item.hasType (IDs::CARD) && ! item.hasType (IDs::IMAGE)) item.setProperty (IDs::lineWidth, width.getValue(), um());
    };
    addChildComponent (width);

    shapeBox.addItem ("Rectangle", 1);
    shapeBox.addItem ("Ellipse", 2);
    shapeBox.addItem ("Line", 3);
    shapeBox.onChange = [this]
    {
        if (updating || items.size() != 1 || ! items[0].hasType (IDs::SHAPE)) return;
        const auto kind = shapeBox.getSelectedId() == 2 ? "ellipse" : (shapeBox.getSelectedId() == 3 ? "line" : "rect");
        project.getUndoManager().beginNewTransaction ("Shape");
        items[0].setProperty (IDs::shape, kind, um());
    };
    addChildComponent (shapeBox);

    dashed.onClick = [this] { setOnAll (IDs::dashed, dashed.getToggleState(), "Dashed"); };
    filled.onClick = [this] { setOnAll (IDs::filled, filled.getToggleState(), "Filled"); };
    straight.onClick = [this] { setOnAll (IDs::bend, 0.0, "Straight"); };
    for (auto* b : { &dashed, &filled })
    {
        b->setColour (juce::ToggleButton::textColourId, Theme::text);
        addChildComponent (*b);
    }
    addChildComponent (straight);

    // Sound cards and links (9b)
    for (auto* l : { &fromLabel, &toLabel, &gainLabel, &fadeInLabel, &fadeOutLabel, &startLabel })
    {
        styleLabel (*l);
        addChildComponent (*l);
    }
    auto timeField = [this] (juce::TextEditor& ed, const juce::Identifier& id, const juce::String& undoName)
    {
        ed.setFont (Theme::font (13.0f));
        ed.setJustification (juce::Justification::centredLeft);
        ed.onReturnKey = [this, &ed, id, undoName] { commitTime (ed, id, undoName); };
        ed.onFocusLost = [this, &ed, id, undoName] { commitTime (ed, id, undoName); };
        addChildComponent (ed);
    };
    timeField (fromEdit, IDs::srcIn, "Fragment");
    timeField (toEdit, IDs::srcOut, "Fragment");
    timeField (fadeInEdit, IDs::fadeIn, "Fade in");
    timeField (fadeOutEdit, IDs::fadeOut, "Fade out");
    timeField (startEdit, IDs::outStart, "Start in the box");
    gain.setSliderStyle (juce::Slider::LinearHorizontal);
    gain.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
    gain.setRange (-48.0, 12.0, 0.1);
    gain.setDoubleClickReturnValue (true, 0.0);
    gain.onDragStart = [this] { project.getUndoManager().beginNewTransaction ("Gain"); };
    gain.onValueChange = [this]
    {
        if (updating || items.size() != 1) return;
        if (! gain.isMouseButtonDown()) project.getUndoManager().beginNewTransaction ("Gain");
        items[0].setProperty (IDs::gainDb, gain.getValue(), um());
    };
    addChildComponent (gain);
    outToggle.setColour (juce::ToggleButton::textColourId, Theme::text);
    outToggle.onClick = [this] { setOnAll (IDs::temporalOut, outToggle.getToggleState(), outToggle.getToggleState() ? "Plays in the piece" : "Plays on the board only"); };
    addChildComponent (outToggle);
    playButton.onClick = [this] { if (onPlayCard && items.size() == 1) onPlayCard (items[0]); };
    wholeButton.onClick = [this]
    {
        if (items.size() != 1) return;
        project.getUndoManager().beginNewTransaction ("The whole sound");
        items[0].removeProperty (IDs::srcIn, um());
        items[0].removeProperty (IDs::srcOut, um());
    };
    goButton.onClick = [this]
    {
        if (items.size() != 1) return;
        if (auto target = BoardPainter::linkedBox (items[0], &project); target.isValid() && onGoToBox) onGoToBox (target);
    };
    relinkButton.onClick = [this] { if (onRelink && items.size() == 1) onRelink (items[0]); };
    takeButton.onClick = [this]
    {
        if (items.size() != 1) return;
        auto target = BoardPainter::linkedBox (items[0], &project);
        auto take = target.getChildWithName (IDs::TAKES).getChildWithProperty (IDs::id, items[0][IDs::targetTake].toString());
        if (! target.isValid() || ! take.isValid()) return;
        project.getUndoManager().beginNewTransaction ("Switch take");
        project.setActiveTake (target, take);
    };
    for (auto* b : { &playButton, &wholeButton, &goButton, &relinkButton, &takeButton })
        addChildComponent (*b);

    // Regions and gestures (9c)
    for (auto* l : { &timeLabel, &scaleLabel, &hzLabel, &hzToLabel, &energyLabel, &energy2Label, &thicknessLabel })
    {
        styleLabel (*l);
        addChildComponent (*l);
    }
    timeBox.addItem ("The board box's (0 to its length)", 1);
    timeBox.addItem ("Free (no time of its own)", 2);
    timeBox.onChange = [this] { setOnAll (IDs::timeMode, timeBox.getSelectedId() == 2 ? "free" : "box", "Region's time"); };
    scaleBox.addItem ("Low, middle, high", 1);
    scaleBox.addItem ("Hz (logarithmic)", 2);
    scaleBox.onChange = [this] { setOnAll (IDs::registerScale, scaleBox.getSelectedId() == 2 ? "hz" : "bands", "Region's register"); };
    energyBox.onChange = [this] { setEnergy (0, energyBox.getSelectedId()); };
    energy2Box.onChange = [this] { setEnergy (1, energy2Box.getSelectedId()); };
    for (auto* c : { &timeBox, &scaleBox, &energyBox, &energy2Box })
        addChildComponent (*c);
    for (auto* ed : { &hzLowEdit, &hzHighEdit })
    {
        ed->setFont (Theme::font (13.0f));
        addChildComponent (*ed);
    }
    hzLowEdit.onReturnKey = hzLowEdit.onFocusLost = [this] { commitHz (hzLowEdit, IDs::hzLow); };
    hzHighEdit.onReturnKey = hzHighEdit.onFocusLost = [this] { commitHz (hzHighEdit, IDs::hzHigh); };
    thickness.setSliderStyle (juce::Slider::LinearHorizontal);
    thickness.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    thickness.setRange (8.0, 160.0, 1.0);
    thickness.setDoubleClickReturnValue (true, 36.0);
    thickness.onDragStart = [this] { project.getUndoManager().beginNewTransaction ("Gesture's width"); };
    thickness.onValueChange = [this]
    {
        if (updating) return;
        if (! thickness.isMouseButtonDown()) project.getUndoManager().beginNewTransaction ("Gesture's width");
        for (auto& item : items)
            if (item.hasType (IDs::GESTURE)) item.setProperty (IDs::thickness, thickness.getValue(), um());
    };
    addChildComponent (thickness);
    factorView.setViewedComponent (&factorHolder, false);
    factorView.setScrollBarsShown (true, false);
    factorView.setScrollBarThickness (8);
    addChildComponent (factorView);

    showOnBox.onClick = [this] { if (onShowOnBox) onShowOnBox(); };
    clearShown.onClick = [this] { if (onClearShown) onClearShown(); };
    fit.onClick = [this] { if (onFit) onFit(); };
    addImages.onClick = [this] { if (onAddImages) onAddImages(); };
    front.onClick = [this] { if (onFront) onFront(); };
    back.onClick = [this] { if (onBack) onBack(); };
    remove.onClick = [this] { if (onDelete) onDelete(); };
    close.onClick = [this] { if (onClose) onClose(); };
    fill.onClick = [this] { if (onFill) onFill(); };
    showOnBox.setTooltip ("The box on the timeline shows this part of the board (the selected items, or with nothing selected what is in view)");
    for (auto* b : { &showOnBox, &clearShown, &fit, &addImages, &front, &back, &remove, &close, &fill })
        addChildComponent (*b);
}

BoardPanel::~BoardPanel() = default;

void BoardPanel::setItems (const juce::ValueTree& b, const juce::Array<juce::ValueTree>& selected)
{
    if (words.hasKeyboardFocus (true)) commitWords();
    notes.commit();
    box = b;
    items = selected;
    refresh();
}

void BoardPanel::setOnAll (const juce::Identifier& id, const juce::var& v, const juce::String& undoName)
{
    if (updating || items.isEmpty()) return;
    project.getUndoManager().beginNewTransaction (undoName);
    for (auto& item : items)
    {
        if (id == IDs::label && ! item.hasType (IDs::CARD) && items.size() > 1) continue;
        if (id == IDs::colour && item.hasType (IDs::IMAGE)) continue;
        if (id == IDs::bend && ! item.hasType (IDs::ARROW)) continue;
        if (item[id] != v) item.setProperty (id, v, um());
    }
}

void BoardPanel::commitWords()
{
    if (updating || items.size() != 1) return;
    auto item = items[0];
    const auto& id = item.hasType (IDs::IMAGE) ? IDs::caption : IDs::label;
    const auto text = words.getText().trim();
    if (text == item[id].toString()) return;
    project.getUndoManager().beginNewTransaction (item.hasType (IDs::IMAGE) ? "Caption" : "Label");
    item.setProperty (id, text, um());
}

void BoardPanel::commitTime (juce::TextEditor& ed, const juce::Identifier& id, const juce::String& undoName)
{
    if (updating || items.size() != 1 || ! items[0].hasType (IDs::SOUNDCARD)) return;
    auto card = items[0];
    const double seconds = soundSeconds ? soundSeconds (card) : 0.0;
    double v = Board::parseSeconds (ed.getText());
    if (v < 0.0) { refresh(); return; }        // not a time: back to what it was
    if (id == IDs::srcIn) v = juce::jlimit (0.0, juce::jmax (0.0, Board::cardOut (card, seconds) - 0.01), v);
    if (id == IDs::srcOut) v = seconds > 0.0 ? juce::jlimit (Board::cardIn (card) + 0.01, seconds, v) : v;
    if (std::abs (v - (double) card.getProperty (id, id == IDs::srcOut ? seconds : 0.0)) < 1.0e-4) { refresh(); return; }
    project.getUndoManager().beginNewTransaction (undoName);
    if (id == IDs::srcOut && seconds > 0.0 && v > seconds - 0.0005) card.removeProperty (IDs::srcOut, um());
    else card.setProperty (id, v, um());
}

void BoardPanel::commitHz (juce::TextEditor& ed, const juce::Identifier& id)
{
    if (updating || items.size() != 1 || ! items[0].hasType (IDs::REGION)) return;
    auto region = items[0];
    auto text = ed.getText().trim().toLowerCase();
    double v = text.getDoubleValue();
    if (text.endsWith ("k") || text.endsWith ("khz")) v *= 1000.0;
    const double lo = region.getProperty (IDs::hzLow, 20.0), hi = region.getProperty (IDs::hzHigh, 20000.0);
    if (v <= 0.0 || (id == IDs::hzLow && v >= hi) || (id == IDs::hzHigh && v <= lo)) { refresh(); return; }
    if (std::abs (v - (double) region.getProperty (id, id == IDs::hzLow ? 20.0 : 20000.0)) < 1.0e-6) return;
    project.getUndoManager().beginNewTransaction ("Region's Hz scale");
    region.setProperty (id, v, um());
}

void BoardPanel::setEnergy (int which, int selectedId)
{
    if (updating) return;
    const auto cats = BoardPainter::glyphCategories (framework);
    if (which >= cats.size()) return;
    auto* c = cats[which];
    const auto& values = framework.valuesOf (*c);
    const int i = selectedId - 2;
    project.getUndoManager().beginNewTransaction (framework.text (c->name));
    for (auto& item : items)
    {
        if (! item.hasType (IDs::GESTURE)) continue;
        if (i < 0 || i >= values.size()) item.removeProperty (c->property(), um());
        else item.setProperty (c->property(), values[i].id, um());
    }
}

void BoardPanel::buildFactorEditors()
{
    // Built again only when the gesture or its energies change, so a curve being drawn keeps its editor.
    juce::StringArray energies;
    const auto cats = BoardPainter::glyphCategories (framework);
    const auto gesture = mode == Mode::gesture && items.size() == 1 ? items[0] : juce::ValueTree();
    for (auto* c : cats) energies.add (gesture[c->property()].toString());
    if (factorFor.size() == 1 && factorFor[0] == gesture && energies == factorsBuiltFor)
    {
        // The curves again (undo).
        int k = 0;
        for (auto* c : cats)
            for (auto* factor : framework.factorsOf (*c, gesture[c->property()].toString()))
                if (auto* e = factorEditors[k++]; e != nullptr && ! e->isDragging()) e->setCurve (Project::factorCurve (gesture, *c, factor->id));
        return;
    }
    factorEditors.clear();
    factorHeadings.clear();
    factorFor.clearQuick();
    factorsBuiltFor = energies;
    factorView.setVisible (false);
    if (! gesture.isValid()) return;
    factorFor.add (gesture);
    for (auto* c : cats)
    {
        const auto value = gesture[c->property()].toString();
        if (value.isEmpty() || framework.factorsOf (*c, value).isEmpty()) continue;
        auto* heading = factorHeadings.add (new juce::Label());
        juce::String name = value;
        if (auto* v = framework.findValue (*c, value)) name = framework.text (v->name);
        heading->setText (Theme::capitals (name) + "  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + framework.text (c->name).toLowerCase(),
                          juce::dontSendNotification);
        styleLabel (*heading, 11.0f);
        heading->getProperties().set ("cat", c->key());
        factorHolder.addAndMakeVisible (heading);
        const auto vc = framework.valueColours (*c, value);
        const auto colour = vc.light != 0 ? juce::Colour (vc.light) : (vc.colour != 0 ? juce::Colour (vc.colour) : Theme::accent);
        const auto key = c->key();
        for (auto* factor : framework.factorsOf (*c, value))
        {
            auto* editor = factorEditors.add (new FactorCurves::Editor (framework.text (factor->name), framework.text (factor->low),
                                                                       framework.text (factor->high), factor->neutral, colour));
            editor->setCurve (Project::factorCurve (gesture, *c, factor->id));
            editor->setTooltip (framework.text (factor->name) + (factor->about.isEmpty() ? juce::String() : ": " + framework.text (factor->about))
                                + ". Along the gesture's path. Click to add a point, drag to move one, double-click to remove it, Alt-drag to draw freely; right-click for shapes.");
            const auto factorId = factor->id;
            editor->onChange = [this, gesture, key, factorId] (const Factors::Curve& curve, bool starting)
            {
                auto* cat = framework.findCategory (key);
                if (cat == nullptr) return;
                if (starting) project.getUndoManager().beginNewTransaction ("Variation factor");
                project.setFactorCurve (gesture, *cat, factorId, curve);
            };
            editor->getProperties().set ("cat", key);
            factorHolder.addAndMakeVisible (editor);
        }
    }
    factorView.setVisible (! factorEditors.isEmpty());
}

void BoardPanel::layoutFactors (int w)
{
    int y = 0;
    for (auto* heading : factorHeadings)
    {
        if (y > 0) y += 6;
        heading->setBounds (0, y, w, 18);
        y += 20;
        for (auto* e : factorEditors)
            if (e->getProperties()["cat"] == heading->getProperties()["cat"])
            {
                e->setBounds (0, y, w, FactorCurves::Editor::preferredHeight);
                y += FactorCurves::Editor::preferredHeight + 4;
            }
    }
    factorHolder.setSize (w, y);
}

void BoardPanel::setMode (Mode m)
{
    mode = m;
    const bool card = m == Mode::card, lines = m == Mode::shape || m == Mode::arrow || m == Mode::stroke;
    const bool board = m == Mode::board;
    labelLabel.setVisible (card);
    labelBox.setVisible (card);
    const bool sound = m == Mode::soundCard, link = m == Mode::link;
    for (auto* c : std::initializer_list<juce::Component*> { &fromLabel, &toLabel, &gainLabel, &fadeInLabel, &fadeOutLabel, &startLabel,
                                                             &fromEdit, &toEdit, &fadeInEdit, &fadeOutEdit, &startEdit, &gain, &outToggle, &playButton, &wholeButton })
        c->setVisible (sound);
    goButton.setVisible (link);
    relinkButton.setVisible (link);
    const bool region = m == Mode::region, gesture = m == Mode::gesture;
    for (auto* c : std::initializer_list<juce::Component*> { &timeLabel, &timeBox, &scaleLabel, &scaleBox })
        c->setVisible (region);
    for (auto* c : std::initializer_list<juce::Component*> { &energyLabel, &energyBox, &thicknessLabel, &thickness })
        c->setVisible (gesture);
    energy2Label.setVisible (gesture && BoardPainter::glyphCategories (framework).size() > 1);
    energy2Box.setVisible (energy2Label.isVisible());
    for (auto* c : std::initializer_list<juce::Component*> { &hzLabel, &hzToLabel, &hzLowEdit, &hzHighEdit })
        c->setVisible (region);
    takeButton.setVisible (false);
    colourLabel.setVisible (! board && m != Mode::image && m != Mode::gesture);
    colours.setVisible (colourLabel.isVisible());
    auto* fc = BoardPainter::fillCategory (framework);
    levelLabel.setVisible (card && fc != nullptr);
    levelBox.setVisible (levelLabel.isVisible());
    textLabel.setVisible (card);
    notes.setVisible (card);
    wordsLabel.setVisible (m == Mode::shape || m == Mode::arrow || m == Mode::image || m == Mode::soundCard || m == Mode::region);
    words.setVisible (wordsLabel.isVisible());
    widthLabel.setVisible (lines);
    width.setVisible (lines);
    dashed.setVisible (lines);
    filled.setVisible (m == Mode::shape);
    shapeLabel.setVisible (m == Mode::shape);
    shapeBox.setVisible (m == Mode::shape);
    straight.setVisible (m == Mode::arrow);
    showOnBox.setVisible (true);
    clearShown.setVisible (board);
    fit.setVisible (board);
    addImages.setVisible (board);
    close.setVisible (board);
    fill.setVisible (board);
    front.setVisible (! board);
    back.setVisible (! board);
    remove.setVisible (! board);
}

void BoardPanel::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    const auto boardTree = Project::boardOf (box);
    // Items that went (undo, delete) drop out.
    juce::Array<juce::ValueTree> still;
    for (auto& i : items) if (i.getParent().isValid()) still.add (i);
    items = still;

    Mode m = Mode::board;
    if (items.size() > 1) m = Mode::several;
    else if (items.size() == 1)
        switch (Board::kindOf (items[0]))
        {
            case Board::Kind::card:   m = Mode::card; break;
            case Board::Kind::shape:  m = Mode::shape; break;
            case Board::Kind::arrow:  m = Mode::arrow; break;
            case Board::Kind::stroke: m = Mode::stroke; break;
            case Board::Kind::image:  m = Mode::image; break;
            case Board::Kind::soundCard: m = Mode::soundCard; break;
            case Board::Kind::link:   m = Mode::link; break;
            case Board::Kind::region: m = Mode::region; break;
            case Board::Kind::gesture: m = Mode::gesture; break;
            case Board::Kind::unknown: break;
        }
    setMode (m);

    const auto item = items.isEmpty() ? juce::ValueTree() : items[0];
    switch (m)
    {
        case Mode::board:
        {
            title.setText ("BOARD", juce::dontSendNotification);
            const auto shown = Board::shownPart (boardTree);
            juce::String t;
            t << box[IDs::name].toString() << ": " << Board::summary (boardTree) << ".\n"
              << (shown.isEmpty() ? "The box on the timeline shows its name and what the board holds; choose a part to show there instead."
                                  : "The box on the timeline shows the part outlined in blue.")
              << "\n\nTools on the left, or their keys:\n"
                 "V select and move,  T text card,  A arrow (from card to card),\n"
                 "S shape (S again: ellipse, line),  P pen,  I image,\n"
                 "O sound card,  L link to a box (or Alt-drag a box from the timeline onto the board),\n"
                 "M morphogram region,  E energy gesture (E again: another energy).\n"
                 "Double-click: write in a card (on the empty board: a new card), a label, a caption.\n"
                 "Scroll to move around, Cmd-scroll or pinch to zoom, Cmd-0 to fit. Items snap to the grid unless Cmd is held; Shift keeps proportions.\n"
                 "Right-click an item for its colour, label, dashes and order.";
            info.setText (t, juce::dontSendNotification);
            showOnBox.setButtonText ("Show what is in view on the box");
            clearShown.setEnabled (! shown.isEmpty());
            fill.setButtonText (isFilled && isFilled() ? "Share the area" : "Fill the area");
            break;
        }
        case Mode::several:
        {
            title.setText (juce::String (items.size()) + " ITEMS", juce::dontSendNotification);
            info.setText ("Move them together, give them one colour, copy (Cmd-C), duplicate (Cmd-D) or delete them.", juce::dontSendNotification);
            colours.mixed = true;
            showOnBox.setButtonText ("Show them on the box");
            break;
        }
        case Mode::card: case Mode::shape: case Mode::arrow: case Mode::stroke: case Mode::image: case Mode::soundCard: case Mode::link:
        case Mode::region: case Mode::gesture:
        {
            title.setText (Theme::capitals (Board::kindName (item)), juce::dontSendNotification);
            info.setText ({}, juce::dontSendNotification);
            colours.mixed = false;
            colours.current = Board::colourOf (item);
            showOnBox.setButtonText ("Show it on the box");
            break;
        }
    }
    colours.repaint();

    if (m == Mode::card)
    {
        labelBox.clear (juce::dontSendNotification);
        const auto labels = Board::labelsIn (project.getState());
        for (int i = 0; i < labels.size(); ++i) labelBox.addItem (labels[i], i + 1);
        labelBox.setText (item[IDs::label].toString(), juce::dontSendNotification);
        if (auto* c = BoardPainter::fillCategory (framework))
        {
            levelLabel.setText (framework.text (c->name), juce::dontSendNotification);
            levelBox.clear (juce::dontSendNotification);
            levelBox.addItem ("None", 1);
            const auto& values = framework.valuesOf (*c);
            int selected = 1;
            for (int i = 0; i < values.size(); ++i)
            {
                levelBox.addItem (framework.text (values[i].name), i + 2);
                if (values[i].id == item[c->property()].toString()) selected = i + 2;
            }
            levelBox.setSelectedId (selected, juce::dontSendNotification);
        }
        if (notes.getBox() != item)
        {
            notes.setBox (item);
            if (item[IDs::notes].toString().trim().isEmpty()) notes.setWriting (true);
        }
        else notes.refresh();
    }
    if (m == Mode::shape || m == Mode::arrow || m == Mode::image || m == Mode::soundCard || m == Mode::region)
    {
        wordsLabel.setText (m == Mode::image ? "Caption" : (m == Mode::soundCard ? "Name (empty: the sound's)" : (m == Mode::region ? "Name" : "Label")),
                            juce::dontSendNotification);
        if (! words.hasKeyboardFocus (true)) words.setText (item[m == Mode::image ? IDs::caption : IDs::label].toString(), false);
        words.setTextToShowWhenEmpty (m == Mode::soundCard ? BoardPainter::soundCardName (item, &project)
                                                           : (m == Mode::region ? juce::String ("Morphogram") : juce::String()), Theme::textMuted);
    }
    if (m == Mode::region)
    {
        timeBox.setSelectedId (Board::regionInBoxTime (item) ? 1 : 2, juce::dontSendNotification);
        const bool hz = item[IDs::registerScale].toString() == "hz";
        scaleBox.setSelectedId (hz ? 2 : 1, juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &hzLabel, &hzToLabel, &hzLowEdit, &hzHighEdit })
            c->setVisible (hz);
        auto hzText = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 1) + "k" : juce::String (juce::roundToInt (v)); };
        if (! hzLowEdit.hasKeyboardFocus (true)) hzLowEdit.setText (hzText (item.getProperty (IDs::hzLow, 20.0)), false);
        if (! hzHighEdit.hasKeyboardFocus (true)) hzHighEdit.setText (hzText (item.getProperty (IDs::hzHigh, 20000.0)), false);
        const auto gestures = Board::gesturesIn (boardTree, item);
        juce::String about;
        about << "Time runs across, register up. " << (gestures.isEmpty() ? juce::String ("No gestures in it yet: E to draw one.")
                                                                         : juce::String (gestures.size()) + (gestures.size() == 1 ? " gesture" : " gestures") + " in it.");
        if (Board::regionInBoxTime (item)) about << " Its width is the board box's length, " << Board::formatSeconds (Project::boxLength (box)) << ".";
        info.setText (about, juce::dontSendNotification);
    }
    if (m == Mode::gesture)
    {
        const auto cats = BoardPainter::glyphCategories (framework);
        auto fillEnergies = [&] (juce::ComboBox& combo, juce::Label& label, int which)
        {
            combo.clear (juce::dontSendNotification);
            if (which >= cats.size()) return;
            auto* c = cats[which];
            label.setText (framework.text (c->name), juce::dontSendNotification);
            combo.addItem ("None", 1);
            const auto& values = framework.valuesOf (*c);
            int selected = 1;
            for (int i = 0; i < values.size(); ++i)
            {
                if (values[i].glyph.isEmpty()) continue;
                combo.addItem (framework.text (values[i].name), i + 2);
                if (values[i].id == item[c->property()].toString()) selected = i + 2;
            }
            combo.setSelectedId (selected, juce::dontSendNotification);
        };
        fillEnergies (energyBox, energyLabel, 0);
        fillEnergies (energy2Box, energy2Label, 1);
        thickness.setValue ((double) item.getProperty (IDs::thickness, 36.0), juce::dontSendNotification);
        juce::String about;
        const auto region = Board::findItem (boardTree, item[IDs::regionId].toString());
        const auto span = Board::gestureSpan (boardTree, item, Project::boxLength (box));
        if (! region.isValid()) about << "Drawn outside a region: no time or register of its own.";
        else
        {
            const auto r = Board::rect (item);
            about << "In " << (region[IDs::label].toString().isEmpty() ? juce::String ("the morphogram") : region[IDs::label].toString());
            if (span.getLength() > 0.0) about << ", " << Board::formatSeconds (span.getStart()) << " to " << Board::formatSeconds (span.getEnd());
            const auto top = Board::registerAt (region, r.getY()), bottom = Board::registerAt (region, r.getBottom());
            about << (top == bottom ? ", " + top : ", " + bottom + " to " + top) << ".";
        }
        about << " Drag it up onto a track for a text box with its energy and curves.";
        info.setText (about, juce::dontSendNotification);
    }
    buildFactorEditors();
    if (m == Mode::soundCard)
    {
        const double seconds = soundSeconds ? soundSeconds (item) : 0.0;
        auto setIfFree = [] (juce::TextEditor& ed, const juce::String& t) { if (! ed.hasKeyboardFocus (true)) ed.setText (t, false); };
        setIfFree (fromEdit, Board::formatSeconds (Board::cardIn (item)));
        setIfFree (toEdit, Board::formatSeconds (Board::cardOut (item, seconds)));
        setIfFree (fadeInEdit, juce::String ((double) item.getProperty (IDs::fadeIn, 0.0), 3));
        setIfFree (fadeOutEdit, juce::String ((double) item.getProperty (IDs::fadeOut, 0.0), 3));
        setIfFree (startEdit, Board::formatSeconds ((double) item.getProperty (IDs::outStart, 0.0)));
        gain.setValue ((double) item.getProperty (IDs::gainDb, 0.0), juce::dontSendNotification);
        const bool out = Board::isTemporalOut (item);
        outToggle.setToggleState (out, juce::dontSendNotification);
        startEdit.setEnabled (out);
        startLabel.setAlpha (out ? 1.0f : 0.5f);
        const bool hearing = previewingCard && previewingCard() == item[IDs::id].toString();
        playButton.setButtonText (hearing ? "Stop" : "Play");
        wholeButton.setEnabled (Board::cardIn (item) > 0.0 || item.hasProperty (IDs::srcOut));
        juce::String about;
        about << "Sound: " << project.findSound (item[IDs::soundId].toString())[IDs::name].toString() << ", " << Board::formatSeconds (seconds)
              << ".\nDrag the ends on its waveform for a fragment. Up onto a track (with the timeline in view) it makes a sound box.";
        info.setText (about, juce::dontSendNotification);
    }
    if (m == Mode::link)
    {
        auto target = BoardPainter::linkedBox (item, &project);
        info.setText (target.isValid() ? "To " + target[IDs::name].toString() + ": " + BoardPainter::linkDetails (item, target) + "."
                                       : "Its box is gone (it was " + item[IDs::name].toString() + "). Point it elsewhere, or delete it.",
                      juce::dontSendNotification);
        goButton.setEnabled (target.isValid());
        const auto takeId = item[IDs::targetTake].toString();
        auto take = target.getChildWithName (IDs::TAKES).getChildWithProperty (IDs::id, takeId);
        if (target.isValid() && take.isValid() && target[IDs::activeTake].toString() != takeId)
        {
            takeButton.setButtonText ("Switch the box to take " + take[IDs::name].toString());
            takeButton.setVisible (true);
        }
    }
    if (m == Mode::shape || m == Mode::arrow || m == Mode::stroke)
    {
        width.setValue ((double) item.getProperty (IDs::lineWidth, 1.5), juce::dontSendNotification);
        dashed.setToggleState ((bool) item[IDs::dashed], juce::dontSendNotification);
        filled.setToggleState ((bool) item[IDs::filled], juce::dontSendNotification);
        const auto kind = item[IDs::shape].toString();
        shapeBox.setSelectedId (kind == "ellipse" ? 2 : (kind == "line" ? 3 : 1), juce::dontSendNotification);
        straight.setEnabled (std::abs ((double) item[IDs::bend]) > 0.5);
    }
    resized();
    repaint();
}

void BoardPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
}

void BoardPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    title.setBounds (r.removeFromTop (20));
    r.removeFromTop (6);
    auto labelled = [&r] (juce::Label& l, juce::Component& c, int h)
    {
        l.setBounds (r.removeFromTop (17));
        c.setBounds (r.removeFromTop (h));
        r.removeFromTop (8);
    };
    auto buttonRow = [&r] (std::initializer_list<juce::Button*> buttons)
    {
        juce::Array<juce::Button*> shown;
        for (auto* b : buttons) if (b->isVisible()) shown.add (b);
        if (shown.isEmpty()) return;
        auto row = r.removeFromTop (26);
        const int w = (row.getWidth() - 6 * (shown.size() - 1)) / shown.size();
        for (auto* b : shown) { b->setBounds (row.removeFromLeft (w)); row.removeFromLeft (6); }
        r.removeFromTop (6);
    };

    if (mode == Mode::board)
    {
        buttonRow ({ &showOnBox });
        buttonRow ({ &clearShown });
        buttonRow ({ &fit, &addImages });
        buttonRow ({ &fill, &close });
        r.removeFromTop (6);
        info.setBounds (r.removeFromTop (juce::jmin (r.getHeight(), 320)));
        return;
    }
    if (info.getText().isNotEmpty()) info.setBounds (r.removeFromTop (mode == Mode::soundCard || mode == Mode::gesture || mode == Mode::region ? 66 : 34));
    else info.setBounds ({});
    if (labelBox.isVisible()) labelled (labelLabel, labelBox, 26);
    if (shapeBox.isVisible()) labelled (shapeLabel, shapeBox, 24);
    if (colours.isVisible()) labelled (colourLabel, colours, 24);
    if (levelBox.isVisible()) labelled (levelLabel, levelBox, 24);
    if (words.isVisible()) labelled (wordsLabel, words, 26);
    if (mode == Mode::soundCard)
    {
        buttonRow ({ &playButton, &wholeButton });
        auto pair = [&r] (juce::Label& l1, juce::TextEditor& e1, juce::Label& l2, juce::TextEditor& e2)
        {
            auto labels = r.removeFromTop (17);
            auto fields = r.removeFromTop (26);
            const int w = (fields.getWidth() - 8) / 2;
            l1.setBounds (labels.removeFromLeft (w));
            labels.removeFromLeft (8);
            l2.setBounds (labels);
            e1.setBounds (fields.removeFromLeft (w));
            fields.removeFromLeft (8);
            e2.setBounds (fields);
            r.removeFromTop (8);
        };
        pair (fromLabel, fromEdit, toLabel, toEdit);
        labelled (gainLabel, gain, 22);
        pair (fadeInLabel, fadeInEdit, fadeOutLabel, fadeOutEdit);
        outToggle.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        labelled (startLabel, startEdit, 26);
    }
    if (mode == Mode::region)
    {
        labelled (timeLabel, timeBox, 24);
        labelled (scaleLabel, scaleBox, 24);
        if (hzLowEdit.isVisible())
        {
            auto labels = r.removeFromTop (17);
            auto fields = r.removeFromTop (26);
            const int w = (fields.getWidth() - 8) / 2;
            hzLabel.setBounds (labels.removeFromLeft (w));
            labels.removeFromLeft (8);
            hzToLabel.setBounds (labels);
            hzLowEdit.setBounds (fields.removeFromLeft (w));
            fields.removeFromLeft (8);
            hzHighEdit.setBounds (fields);
            r.removeFromTop (8);
        }
    }
    if (mode == Mode::gesture)
    {
        labelled (energyLabel, energyBox, 24);
        if (energy2Box.isVisible()) labelled (energy2Label, energy2Box, 24);
        labelled (thicknessLabel, thickness, 22);
    }
    if (mode == Mode::link)
    {
        buttonRow ({ &goButton, &relinkButton });
        if (takeButton.isVisible()) buttonRow ({ &takeButton });
    }
    if (width.isVisible()) labelled (widthLabel, width, 22);
    if (dashed.isVisible())
    {
        auto row = r.removeFromTop (24);
        dashed.setBounds (row.removeFromLeft (100));
        if (filled.isVisible()) filled.setBounds (row.removeFromLeft (100));
        if (straight.isVisible()) straight.setBounds (row.removeFromLeft (90));
        r.removeFromTop (8);
    }
    buttonRow ({ &showOnBox });
    buttonRow ({ &front, &back, &remove });
    if (factorView.isVisible())
    {
        r.removeFromTop (4);
        factorView.setBounds (r);
        layoutFactors (r.getWidth());
        if (factorHolder.getHeight() > r.getHeight()) layoutFactors (r.getWidth() - 10);   // room for the scroll bar
    }
    if (notes.isVisible())
    {
        r.removeFromTop (4);
        textLabel.setBounds (r.removeFromTop (17));
        notes.setBounds (r);
    }
}
