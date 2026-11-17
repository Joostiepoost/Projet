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

void BoardPanel::setMode (Mode m)
{
    mode = m;
    const bool card = m == Mode::card, lines = m == Mode::shape || m == Mode::arrow || m == Mode::stroke;
    const bool board = m == Mode::board;
    labelLabel.setVisible (card);
    labelBox.setVisible (card);
    colourLabel.setVisible (! board && m != Mode::image);
    colours.setVisible (colourLabel.isVisible());
    auto* fc = BoardPainter::fillCategory (framework);
    levelLabel.setVisible (card && fc != nullptr);
    levelBox.setVisible (levelLabel.isVisible());
    textLabel.setVisible (card);
    notes.setVisible (card);
    wordsLabel.setVisible (m == Mode::shape || m == Mode::arrow || m == Mode::image);
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
                 "S shape (S again: ellipse, line),  P pen,  I image.\n"
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
        case Mode::card: case Mode::shape: case Mode::arrow: case Mode::stroke: case Mode::image:
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
    if (m == Mode::shape || m == Mode::arrow || m == Mode::image)
    {
        wordsLabel.setText (m == Mode::image ? "Caption" : "Label", juce::dontSendNotification);
        if (! words.hasKeyboardFocus (true)) words.setText (item[m == Mode::image ? IDs::caption : IDs::label].toString(), false);
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
    if (info.getText().isNotEmpty()) info.setBounds (r.removeFromTop (34)); else info.setBounds ({});
    if (labelBox.isVisible()) labelled (labelLabel, labelBox, 26);
    if (shapeBox.isVisible()) labelled (shapeLabel, shapeBox, 24);
    if (colours.isVisible()) labelled (colourLabel, colours, 24);
    if (levelBox.isVisible()) labelled (levelLabel, levelBox, 24);
    if (words.isVisible()) labelled (wordsLabel, words, 26);
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
    if (notes.isVisible())
    {
        r.removeFromTop (4);
        textLabel.setBounds (r.removeFromTop (17));
        notes.setBounds (r);
    }
}
