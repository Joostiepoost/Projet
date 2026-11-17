// BoardView.cpp — see BoardView.h.

#include "BoardView.h"
#include "Theme.h"

namespace
{
    constexpr float minZoom = 0.1f, maxZoom = 4.0f;
    constexpr int handleSize = 8;

    juce::Array<juce::ValueTree>& boardClipboard()
    {
        static juce::Array<juce::ValueTree> items;
        return items;
    }

    bool isImageFile (const juce::File& f)
    {
        return f.hasFileExtension ("png;jpg;jpeg;gif");
    }

    juce::Path iconPath (const char* svg)
    {
        return juce::Drawable::parseSVGPath (svg);
    }
}

BoardView::BoardView (Project& p, const Framework& f, const ProjectFolder& pf)
    : project (p), framework (f), folder (pf)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
}

BoardView::~BoardView()
{
    editor.reset();
}

//== The box ===================================================================

void BoardView::setBox (const juce::ValueTree& b)
{
    if (b == box) return;
    commitEdit();
    rememberView();
    box = b;
    selectedIds.clear();
    drag = Drag::none;
    tool = Tool::select;
    pasteCount = 0;
    auto it = views.find (box[IDs::id].toString());
    if (it != views.end()) { origin = it->second.origin; zoom = it->second.zoom; }
    else zoomToFit();
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void BoardView::rememberView()
{
    if (box.isValid()) views[box[IDs::id].toString()] = { origin, zoom };
}

void BoardView::modelChanged()
{
    const auto board = getBoard();
    juce::StringArray still;
    for (auto& id : selectedIds)
        if (Board::findItem (board, id).isValid()) still.add (id);
    if (still != selectedIds)
    {
        selectedIds = still;
        if (onSelectionChanged) onSelectionChanged();
    }
    if (editing.isValid() && ! editing.getParent().isValid()) { editor.reset(); editing = {}; }
    repaint();
}

void BoardView::setActivePane (bool on)
{
    if (activePane == on) return;
    activePane = on;
    if (! on) commitEdit();
    repaint();
}

BoardPainter::Context BoardView::context() const
{
    BoardPainter::Context c;
    c.framework = &framework;
    c.projectFolder = folder.getFolder();
    c.fills = project.getState()[IDs::fwFills].toString() != "off";
    if (editing.isValid() && editingField == IDs::notes) c.skipText = editing[IDs::id].toString();
    return c;
}

//== The view ==================================================================

juce::Point<float> BoardView::toScreen (juce::Point<float> b) const
{
    const auto c = canvasArea();
    return { (float) c.getX() + (b.x - origin.x) * zoom, (float) c.getY() + (b.y - origin.y) * zoom };
}

juce::Point<float> BoardView::toBoard (juce::Point<float> s) const
{
    const auto c = canvasArea();
    return { origin.x + (s.x - (float) c.getX()) / zoom, origin.y + (s.y - (float) c.getY()) / zoom };
}

juce::Rectangle<float> BoardView::toScreen (juce::Rectangle<float> b) const
{
    return { toScreen (b.getTopLeft()), toScreen (b.getBottomRight()) };
}

juce::Rectangle<float> BoardView::visibleBoardArea() const
{
    const auto c = canvasArea().toFloat();
    return { toBoard (c.getTopLeft()), toBoard (c.getBottomRight()) };
}

void BoardView::zoomAbout (juce::Point<float> s, float newZoom)
{
    newZoom = juce::jlimit (minZoom, maxZoom, newZoom);
    const auto fixed = toBoard (s);
    zoom = newZoom;
    const auto c = canvasArea();
    origin = { fixed.x - (s.x - (float) c.getX()) / zoom, fixed.y - (s.y - (float) c.getY()) / zoom };
    positionEditor();
    repaint();
}

void BoardView::zoomBy (float factor)
{
    zoomAbout (canvasArea().toFloat().getCentre(), zoom * factor);
}

void BoardView::zoomToFit()
{
    const auto c = canvasArea().toFloat().reduced (24.0f);
    auto content = Board::contentBounds (getBoard());
    if (content.isEmpty() || c.isEmpty())
    {
        zoom = 1.0f;
        origin = { -24.0f, -24.0f };
    }
    else
    {
        zoom = juce::jlimit (minZoom, 1.0f, juce::jmin (c.getWidth() / content.getWidth(), c.getHeight() / content.getHeight()));
        const auto centre = content.getCentre();
        const auto area = canvasArea().toFloat();
        origin = { centre.x - area.getWidth() * 0.5f / zoom, centre.y - area.getHeight() * 0.5f / zoom };
    }
    positionEditor();
    repaint();
}

void BoardView::resized()
{
    positionEditor();
}

//== Tools =====================================================================

std::vector<BoardView::ToolButton> BoardView::toolButtons() const
{
    std::vector<ToolButton> b;
    int y = 10;
    auto add = [&] (Tool t, const char* name, const char* key)
    {
        b.push_back ({ t, { 6, y, 36, 36 }, name, key });
        y += 42;
    };
    add (Tool::select, "Select and move", "V");
    add (Tool::card, "Text card", "T");
    add (Tool::arrow, "Arrow", "A");
    add (Tool::shape, "Shape", "S");
    add (Tool::pen, "Pen", "P");
    add (Tool::image, "Image", "I");
    return b;
}

void BoardView::setTool (Tool t)
{
    if (t == Tool::image) { chooseImages(); return; }
    commitEdit();
    tool = t;
    switch (t)
    {
        case Tool::select: status ({}); break;
        case Tool::card:   status ("Text card: click for a card, or drag its size; then write."); break;
        case Tool::arrow:  status ("Arrow: drag from a card (or anywhere) to another; its ends stay attached to the cards."); break;
        case Tool::shape:  status ("Shape (" + juce::String (shapeKind == "rect" ? "rectangle" : shapeKind) + "): drag to draw it; S again for the next shape."); break;
        case Tool::pen:    status ("Pen: draw freely; Esc or V to stop."); break;
        case Tool::image:  break;
    }
    repaint();
}

void BoardView::paintToolStrip (juce::Graphics& g)
{
    g.setColour (Theme::panel);
    g.fillRect (0, 0, toolStripWidth, getHeight());
    g.setColour (Theme::line);
    g.fillRect (toolStripWidth - 1, 0, 1, getHeight());
    for (auto& b : toolButtons())
    {
        const bool on = b.tool == tool;
        const auto r = b.area.toFloat();
        if (on)
        {
            g.setColour (juce::Colour (0xff3a3220));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (Theme::accent);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        }
        juce::Path icon;
        switch (b.tool)
        {
            case Tool::select: icon = iconPath ("M4 3 L4 14 L7 11 L9.5 15.5 L11 14.7 L8.6 10.3 L12.5 10 Z"); break;
            case Tool::card:   icon = iconPath ("M3 4 H15 V14 H3 Z M6 7.5 H12 M6 10.5 H10"); break;
            case Tool::arrow:  icon = iconPath ("M3 14 L14 4 M8.5 4 H14 V9.5"); break;
            case Tool::shape:
                if (shapeKind == "ellipse") icon.addEllipse (3.0f, 4.0f, 12.0f, 10.0f);
                else if (shapeKind == "line") icon = iconPath ("M3 15 L15 3");
                else icon = iconPath ("M3 4 H15 V14 H3 Z");
                break;
            case Tool::pen:    icon = iconPath ("M3 15 C 5 11, 7 13, 9 9 C 11 5, 13 7, 15 3"); break;
            case Tool::image:  icon = iconPath ("M3 4 H15 V14 H3 Z M3 12 L7 8 L10 11 L12 9 L15 12"); break;
        }
        icon.applyTransform (juce::AffineTransform::translation (r.getX() + 9.0f, r.getY() + 9.0f));
        g.setColour (on ? juce::Colour (0xfff0cf8e) : Theme::textMuted.brighter (0.2f));
        g.strokePath (icon, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (Theme::font (8.5f));
        g.setColour (Theme::textMuted.withAlpha (0.7f));
        g.drawText (b.key, b.area.withTrimmedLeft (25).withTrimmedTop (24), juce::Justification::centred, false);
    }
}

void BoardView::showToolMenu (Tool t)
{
    if (t != Tool::shape) return;
    juce::PopupMenu m;
    m.addItem (1, "Rectangle", true, shapeKind == "rect");
    m.addItem (2, "Ellipse", true, shapeKind == "ellipse");
    m.addItem (3, "Line", true, shapeKind == "line");
    juce::Component::SafePointer<BoardView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->shapeKind = r == 1 ? "rect" : (r == 2 ? "ellipse" : "line");
        safe->setTool (Tool::shape);
    });
}

//== Selection =================================================================

juce::Array<juce::ValueTree> BoardView::getSelectedItems() const
{
    juce::Array<juce::ValueTree> out;
    const auto board = getBoard();
    for (auto item : board)
        if (isSelected (item)) out.add (item);
    return out;
}

void BoardView::setSelectedIds (const juce::StringArray& ids)
{
    if (ids == selectedIds) return;
    selectedIds = ids;
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void BoardView::selectItems (const juce::Array<juce::ValueTree>& items)
{
    juce::StringArray ids;
    for (auto& i : items) ids.add (i[IDs::id].toString());
    setSelectedIds (ids);
}

void BoardView::selectAll()
{
    juce::Array<juce::ValueTree> all;
    for (auto item : getBoard()) all.add (item);
    selectItems (all);
}

void BoardView::deselect()
{
    setSelectedIds ({});
}

void BoardView::deleteSelection()
{
    auto items = getSelectedItems();
    if (items.isEmpty()) return;
    commitEdit();
    project.getUndoManager().beginNewTransaction (items.size() == 1 ? "Delete " + Board::kindName (items[0]) : juce::String ("Delete items"));
    auto board = getBoard();
    for (auto& item : items) Board::detachArrowsFrom (board, item[IDs::id].toString(), um());
    for (auto& item : items) board.removeChild (item, um());
    setSelectedIds ({});
}

void BoardView::copySelection()
{
    auto items = getSelectedItems();
    if (items.isEmpty()) return;
    commitEdit();
    boardClipboard() = Board::copyItems (getBoard(), items);
    pasteCount = 0;
    status (items.size() == 1 ? "Copied the " + Board::kindName (items[0]) + "." : "Copied " + juce::String (items.size()) + " items.");
}

void BoardView::cutSelection()
{
    copySelection();
    deleteSelection();
}

bool BoardView::canPaste()
{
    return ! boardClipboard().isEmpty();
}

void BoardView::paste()
{
    auto board = getBoard();
    if (! board.isValid() || boardClipboard().isEmpty()) return;
    commitEdit();
    juce::ValueTree holder (IDs::BOARD);
    for (auto& c : boardClipboard()) holder.appendChild (c.createCopy(), nullptr);
    juce::Array<juce::ValueTree> source;
    for (auto c : holder) source.add (c);
    auto copies = Board::copyItems (holder, source);
    // Each paste a little further on; into the view if the copies are out of it.
    ++pasteCount;
    const auto content = Board::boundsOf (holder, source);
    juce::Point<float> shift (20.0f * (float) pasteCount, 20.0f * (float) pasteCount);
    const auto view = visibleBoardArea();
    if (! view.intersects (content.translated (shift.x, shift.y)))
        shift = snapPoint (view.getCentre() - content.getCentre(), true);
    project.getUndoManager().beginNewTransaction ("Paste");
    for (auto& c : copies)
    {
        Board::moveBy (c, shift, nullptr);
        board.appendChild (c, um());
    }
    selectItems (copies);
}

void BoardView::duplicateSelection()
{
    auto items = getSelectedItems();
    if (items.isEmpty()) return;
    commitEdit();
    auto board = getBoard();
    auto copies = Board::copyItems (board, items);
    project.getUndoManager().beginNewTransaction ("Duplicate");
    for (auto& c : copies)
    {
        Board::moveBy (c, { 20.0f, 20.0f }, nullptr);
        board.appendChild (c, um());
    }
    selectItems (copies);
}

void BoardView::bringToFront()
{
    auto items = getSelectedItems();
    if (items.isEmpty()) return;
    project.getUndoManager().beginNewTransaction ("Bring to front");
    auto board = getBoard();
    for (auto& item : items) board.moveChild (board.indexOf (item), board.getNumChildren() - 1, um());
}

void BoardView::sendToBack()
{
    auto items = getSelectedItems();
    if (items.isEmpty()) return;
    project.getUndoManager().beginNewTransaction ("Send to back");
    auto board = getBoard();
    for (int i = items.size(); --i >= 0;) board.moveChild (board.indexOf (items[i]), 0, um());
}

void BoardView::showOnBox()
{
    auto board = getBoard();
    if (! board.isValid()) return;
    const auto items = getSelectedItems();
    auto part = Board::boundsOf (board, items);
    if (items.isEmpty()) part = visibleBoardArea();
    else part = part.expanded (12.0f);
    project.getUndoManager().beginNewTransaction ("Show on the box");
    Board::setShownPart (board, part, um());
    status (items.isEmpty() ? "The box shows what is in view now." : "The box shows the selected items.");
}

void BoardView::clearShownPart()
{
    auto board = getBoard();
    if (Board::shownPart (board).isEmpty()) return;
    project.getUndoManager().beginNewTransaction ("Show nothing on the box");
    Board::setShownPart (board, {}, um());
}

//== Adding items ==============================================================

juce::ValueTree BoardView::addItem (juce::ValueTree item, const juce::String& undoName)
{
    auto board = getBoard();
    if (! board.isValid()) return {};
    project.getUndoManager().beginNewTransaction (undoName);
    board.appendChild (item, um());
    setSelectedIds ({ item[IDs::id].toString() });
    return item;
}

void BoardView::addCardAt (juce::Point<float> p, bool edit)
{
    const auto at = snapPoint (p, true);
    auto card = addItem (Board::makeCard ({ at.x, at.y, 220.0f, 100.0f }, (int) Board::palette()[0].getARGB()), "New card");
    if (edit && card.isValid()) editText (card);
}

void BoardView::chooseImages()
{
    if (! getBoard().isValid()) return;
    chooser = std::make_unique<juce::FileChooser> ("Images for the board", juce::File(), "*.png;*.jpg;*.jpeg;*.gif");
    juce::Component::SafePointer<BoardView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        safe->addImages (fc.getResults(), safe->visibleBoardArea().getCentre());
    });
}

void BoardView::addImages (const juce::Array<juce::File>& files, juce::Point<float> at)
{
    auto board = getBoard();
    if (! board.isValid()) return;
    juce::Array<juce::ValueTree> added;
    juce::StringArray failed;
    auto place = at;
    for (auto& f : files)
    {
        if (! isImageFile (f)) { failed.add (f.getFileName()); continue; }
        const auto image = juce::ImageCache::getFromFile (f);
        const auto path = folder.copyIntoBoards (f);
        if (! image.isValid() || path.isEmpty()) { failed.add (f.getFileName()); continue; }
        // At its own size, at most 480 units either way.
        const float k = juce::jmin (1.0f, 480.0f / (float) juce::jmax (image.getWidth(), image.getHeight()));
        const float w = std::round ((float) image.getWidth() * k), h = std::round ((float) image.getHeight() * k);
        // The first one centred where it goes, the others a little further on each.
        const auto topLeft = snapPoint (place - juce::Point<float> (w, h) * 0.5f, true);
        const juce::Rectangle<float> r (topLeft.x, topLeft.y, w, h);
        if (added.isEmpty()) project.getUndoManager().beginNewTransaction (files.size() == 1 ? "Add image" : "Add images");
        auto item = Board::makeImage (path, r);
        board.appendChild (item, um());
        added.add (item);
        place += juce::Point<float> (30.0f, 30.0f);
    }
    if (! added.isEmpty()) selectItems (added);
    if (! failed.isEmpty()) status ("Could not place " + failed.joinIntoString (", ") + " (PNG, JPEG and GIF images can go on a board).", true);
    else if (! added.isEmpty()) status ("Copied into the project's Boards folder.");
    if (tool == Tool::image) tool = Tool::select;
    repaint();
}

bool BoardView::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (! getBoard().isValid()) return false;
    for (auto& f : files)
        if (isImageFile (juce::File (f))) return true;
    return false;
}

void BoardView::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::Array<juce::File> list;
    for (auto& f : files) list.add (juce::File (f));
    if (onActivate) onActivate();
    addImages (list, toBoard ({ (float) x, (float) y }));
}

//== Words typed in place ======================================================

void BoardView::editText (juce::ValueTree item)
{
    commitEdit();
    if (! item.isValid() || ! item.getParent().isValid()) return;
    const auto kind = Board::kindOf (item);
    editing = item;
    editingField = kind == Board::Kind::card ? IDs::notes : (kind == Board::Kind::image ? IDs::caption : IDs::label);
    setSelectedIds ({ item[IDs::id].toString() });
    editor = std::make_unique<juce::TextEditor>();
    const bool multi = editingField == IDs::notes;
    editor->setMultiLine (multi, true);
    editor->setReturnKeyStartsNewLine (multi);
    editor->setScrollbarsShown (multi);
    editor->setText (item[editingField].toString(), false);
    editor->setColour (juce::TextEditor::backgroundColourId, multi ? BoardPainter::cardPaper (Board::colourOf (item)) : juce::Colour (0xff17181b));
    editor->setColour (juce::TextEditor::outlineColourId, Theme::accent.withAlpha (0.6f));
    editor->setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);
    editor->setColour (juce::TextEditor::textColourId, Theme::text);
    editor->setJustification (multi ? juce::Justification::topLeft : juce::Justification::centred);
    editor->onEscapeKey = [this] { commitEdit(); grabKeyboardFocus(); };
    editor->onReturnKey = [this] { if (editingField != IDs::notes) { commitEdit(); grabKeyboardFocus(); } };
    editor->onFocusLost = [this] { commitEdit(); };
    addAndMakeVisible (*editor);
    positionEditor();
    editor->grabKeyboardFocus();
    editor->moveCaretToEnd();
    status (multi ? "Writing in the card: **bold**, *italic*, - lists, # headings, links; Esc or a click outside when done." : "Esc or Return when done.");
    repaint();
}

void BoardView::positionEditor()
{
    if (editor == nullptr || ! editing.isValid()) return;
    const auto board = getBoard();
    juce::Rectangle<float> r;
    float fontSize = 14.0f;
    if (editingField == IDs::notes)
    {
        auto card = Board::rect (editing);
        const float labelH = editing[IDs::label].toString().trim().isNotEmpty() ? 27.0f : 9.0f;
        r = toScreen (juce::Rectangle<float> (card.getX() + 8.0f, card.getY() + labelH, card.getWidth() - 12.0f, juce::jmax (40.0f, card.getHeight() - labelH - 4.0f)));
    }
    else if (editingField == IDs::caption)
    {
        auto img = Board::rect (editing);
        r = toScreen (juce::Rectangle<float> (img.getX(), img.getBottom() + 4.0f, juce::jmax (160.0f, img.getWidth()), 22.0f));
        fontSize = 12.0f;
    }
    else
    {
        const auto centre = editing.hasType (IDs::ARROW) ? Board::arrowPath (board, editing).middle() : Board::bounds (board, editing).getCentre();
        r = toScreen (juce::Rectangle<float> (180.0f, 24.0f).withCentre (centre));
        fontSize = 13.0f;
    }
    editor->setFont (Theme::font (juce::jmax (9.0f, fontSize * zoom)));
    editor->setBounds (r.toNearestInt().getIntersection (canvasArea()).withSizeKeepingCentre (juce::jmax (60, r.toNearestInt().getWidth()), juce::jmax (20, r.toNearestInt().getHeight())));
}

void BoardView::commitEdit()
{
    if (editor == nullptr) return;
    auto item = editing;
    const auto field = editingField;
    const auto text = editor->getText();
    // Gone first, so focus moving out of it cannot commit twice.
    auto keep = std::move (editor);
    editing = {};
    keep->setVisible (false);
    removeChildComponent (keep.get());
    juce::MessageManager::callAsync ([doomed = keep.release()] { delete doomed; });
    if (item.isValid() && item.getParent().isValid() && text != item[field].toString())
    {
        project.getUndoManager().beginNewTransaction (field == IDs::notes ? "Write in a card" : (field == IDs::caption ? "Caption" : "Label"));
        item.setProperty (field, field == IDs::notes ? text : text.trim(), um());
        // A card grows to show what was written in it.
        if (field == IDs::notes)
        {
            const float need = BoardPainter::cardTextHeight (item);
            if (need > (float) (double) item[IDs::h]) item.setProperty (IDs::h, std::ceil (need / 10.0f) * 10.0f, um());
        }
    }
    status ({});
    repaint();
}

//== Painting ==================================================================

std::vector<juce::Point<float>> BoardView::cornerHandles (const juce::ValueTree& item) const
{
    const auto r = toScreen (Board::bounds (getBoard(), item));
    return { r.getTopLeft(), r.getTopRight(), r.getBottomRight(), r.getBottomLeft() };
}

void BoardView::paint (juce::Graphics& g)
{
    const auto canvas = canvasArea();
    g.setColour (juce::Colour (0xff17181b));
    g.fillRect (canvas);
    const auto board = getBoard();

    // The dot grid: every 20 units, every 100 when they would crowd.
    {
        const float step = 20.0f * zoom < 9.0f ? 100.0f : 20.0f;
        const auto area = visibleBoardArea();
        g.setColour (juce::Colour (0xff2c2e34));
        const float dot = juce::jlimit (1.0f, 2.0f, 1.2f * zoom);
        for (float y = std::floor (area.getY() / step) * step; y <= area.getBottom(); y += step)
            for (float x = std::floor (area.getX() / step) * step; x <= area.getRight(); x += step)
            {
                const auto s = toScreen (juce::Point<float> (x, y));
                g.fillRect (s.x, s.y, dot, dot);
            }
    }

    if (! board.isValid())
    {
        paintToolStrip (g);
        return;
    }

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (canvas);
        g.addTransform (juce::AffineTransform::translation (-origin.x, -origin.y).scaled (zoom).translated ((float) canvas.getX(), (float) canvas.getY()));
        BoardPainter::paint (g, board, context());
    }

    paintOverlay (g, board, canvas);
    paintToolStrip (g);
}

void BoardView::paintOverlay (juce::Graphics& g, const juce::ValueTree& board, juce::Rectangle<int> canvas)
{
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (canvas);

    // The part shown on the box.
    const auto shown = Board::shownPart (board);
    if (! shown.isEmpty())
    {
        const auto r = toScreen (shown);
        juce::Path p, dashed;
        p.addRectangle (r);
        const float dashes[] = { 6.0f, 5.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dashes, 2);
        g.setColour (Theme::magnifier.withAlpha (0.8f));
        g.fillPath (dashed);
        g.setFont (Theme::font (10.5f));
        g.drawText ("shown on the box", juce::Rectangle<float> (r.getX() + 4.0f, r.getY() - 16.0f, 160.0f, 14.0f), juce::Justification::bottomLeft, false);
    }

    // The selection.
    const auto selected = getSelectedItems();
    for (auto& item : selected)
    {
        g.setColour (Theme::accent);
        if (Board::hasEnds (item))
        {
            const auto [a, b] = Board::ends (board, item);
            if (selected.size() == 1)
                for (auto p : { toScreen (a), toScreen (b) })
                {
                    g.setColour (Theme::background);
                    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (p));
                    g.setColour (Theme::accent);
                    g.drawEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (p), 1.5f);
                }
            if (item.hasType (IDs::ARROW))
            {
                const auto mid = toScreen (Board::arrowPath (board, item).middle());
                if (selected.size() == 1)
                {
                    g.setColour (Theme::accent.withAlpha (0.85f));
                    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (mid));
                }
                else g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (mid), 1.2f);
            }
            continue;
        }
        const auto r = toScreen (Board::bounds (board, item)).expanded (2.0f);
        g.drawRect (r, 1.5f);
        if (selected.size() == 1 && Board::hasCorners (item))
            for (auto p : cornerHandles (item))
            {
                const auto h = juce::Rectangle<float> ((float) handleSize, (float) handleSize).withCentre (p);
                g.setColour (Theme::background);
                g.fillRect (h);
                g.setColour (Theme::accent);
                g.drawRect (h, 1.2f);
            }
    }

    // What is being made.
    g.setColour (Theme::accent.withAlpha (0.8f));
    if (drag == Drag::rubber || drag == Drag::newCard || drag == Drag::newShape)
    {
        auto r = juce::Rectangle<float> (toScreen (downBoard), toScreen (current));
        if (drag == Drag::newShape && shapeKind == "line") g.drawLine ({ toScreen (downBoard), toScreen (current) }, 1.5f);
        else if (drag == Drag::newShape && shapeKind == "ellipse") g.drawEllipse (r, 1.2f);
        else
        {
            if (drag == Drag::rubber) { g.setColour (Theme::accent.withAlpha (0.08f)); g.fillRect (r); g.setColour (Theme::accent.withAlpha (0.8f)); }
            g.drawRect (r, 1.2f);
        }
    }
    else if (drag == Drag::newArrow)
    {
        juce::Point<float> from = toScreen (downBoard);
        if (auto it = Board::findItem (board, arrowFrom); it.isValid()) from = toScreen (Board::bounds (board, it).getCentre());
        g.drawArrow ({ from, toScreen (current) }, 1.5f, 10.0f, 9.0f);
    }
    else if (drag == Drag::pen && trail.size() > 1)
    {
        juce::Path p;
        p.startNewSubPath (toScreen (trail[0]));
        for (int i = 1; i < trail.size(); ++i) p.lineTo (toScreen (trail[i]));
        g.setColour (Board::palette()[6]);
        g.strokePath (p, juce::PathStrokeType (2.0f * zoom, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // The item an arrow's end would attach to.
    if (hoverTarget.isNotEmpty())
        if (auto t = Board::findItem (board, hoverTarget); t.isValid())
        {
            g.setColour (Theme::magnifier);
            g.drawRect (toScreen (Board::bounds (board, t)).expanded (4.0f), 2.0f);
        }

    if (board.getNumChildren() == 0 && drag == Drag::none)
    {
        g.setColour (Theme::textMuted.withAlpha (0.8f));
        g.setFont (Theme::font (13.0f));
        g.drawFittedText ("An empty board. Double-click to write a card, or pick a tool on the left:\n"
                          "T card,  A arrow,  S shape (S again: ellipse, line),  P pen,  I image (or drop images here).\n"
                          "Scroll to move around, Cmd-scroll or pinch to zoom.",
                          canvas.reduced (40), juce::Justification::centred, 4);
    }

    if (activePane)
    {
        g.setColour (Theme::magnifier);
        g.fillRect (canvas.getX(), canvas.getY(), canvas.getWidth(), 2);
    }
}

//== The mouse =================================================================

int BoardView::handleAt (juce::Point<float> s, int& kind) const
{
    const auto selected = getSelectedItems();
    if (selected.size() != 1) return -1;
    const auto& item = selected.getReference (0);
    const auto board = getBoard();
    if (Board::hasEnds (item))
    {
        const auto [a, b] = Board::ends (board, item);
        if (s.getDistanceFrom (toScreen (a)) <= 7.0f) { kind = 1; return 0; }
        if (s.getDistanceFrom (toScreen (b)) <= 7.0f) { kind = 1; return 1; }
        if (item.hasType (IDs::ARROW) && s.getDistanceFrom (toScreen (Board::arrowPath (board, item).middle())) <= 7.0f) { kind = 2; return 0; }
        return -1;
    }
    if (! Board::hasCorners (item)) return -1;
    const auto corners = cornerHandles (item);
    for (int i = 0; i < 4; ++i)
        if (s.getDistanceFrom (corners[(size_t) i]) <= 7.0f) { kind = 0; return i; }
    return -1;
}

juce::ValueTree BoardView::itemUnder (juce::Point<float> p, const juce::ValueTree& except) const
{
    const auto board = getBoard();
    for (int i = board.getNumChildren(); --i >= 0;)
    {
        auto item = board.getChild (i);
        if (item == except || item.hasType (IDs::ARROW)) continue;
        if (Board::bounds (board, item).expanded (tolerance()).contains (p)) return item;
    }
    return {};
}

void BoardView::mouseDown (const juce::MouseEvent& e)
{
    if (onActivate) onActivate();
    if (editor != nullptr && ! editor->getBounds().contains (e.getPosition())) commitEdit();
    grabKeyboardFocus();
    downScreen = lastScreen = e.position;
    downBoard = current = toBoard (e.position);
    drag = Drag::none;
    hoverTarget.clear();
    auto board = getBoard();

    if (e.x < toolStripWidth)
    {
        for (auto& b : toolButtons())
            if (b.area.contains (e.getPosition()))
            {
                if (e.mods.isPopupMenu()) { showToolMenu (b.tool); return; }
                if (b.tool == Tool::shape && tool == Tool::shape)
                    shapeKind = shapeKind == "rect" ? "ellipse" : (shapeKind == "ellipse" ? "line" : "rect");
                setTool (b.tool);
                return;
            }
        return;
    }
    if (! board.isValid()) return;

    // Middle-drag, or Alt-drag on the empty board, moves around (as scrolling does).
    if (e.mods.isMiddleButtonDown() || (e.mods.isAltDown() && ! Board::itemAt (board, downBoard, tolerance()).isValid()))
    {
        drag = Drag::pan;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }

    if (e.mods.isPopupMenu())
    {
        auto item = Board::itemAt (board, downBoard, tolerance());
        if (item.isValid())
        {
            if (! isSelected (item)) setSelectedIds ({ item[IDs::id].toString() });
            showItemMenu (item);
        }
        else showBoardMenu (downBoard);
        return;
    }

    const bool snapOn = ! e.mods.isCommandDown();
    switch (tool)
    {
        case Tool::card:  drag = Drag::newCard; downBoard = current = snapPoint (downBoard, snapOn); return;
        case Tool::shape: drag = Drag::newShape; downBoard = current = snapPoint (downBoard, snapOn); return;
        case Tool::arrow:
        {
            drag = Drag::newArrow;
            auto from = itemUnder (downBoard);
            arrowFrom = from.isValid() ? from[IDs::id].toString() : juce::String();
            return;
        }
        case Tool::pen:
            drag = Drag::pen;
            trail.clearQuick();
            trail.add (downBoard);
            return;
        case Tool::image: case Tool::select:
            break;
    }

    int kind = -1;
    const int h = handleAt (e.position, kind);
    if (h >= 0)
    {
        dragItem = getSelectedItems()[0];
        handle = h;
        if (kind == 0) { drag = Drag::corner; dragRect = Board::rect (dragItem); project.getUndoManager().beginNewTransaction ("Resize " + Board::kindName (dragItem)); }
        else if (kind == 1) { drag = Drag::end; project.getUndoManager().beginNewTransaction ("Move the end"); }
        else { drag = Drag::bend; project.getUndoManager().beginNewTransaction ("Bend the arrow"); }
        return;
    }

    auto item = Board::itemAt (board, downBoard, tolerance());
    if (item.isValid())
    {
        const auto id = item[IDs::id].toString();
        if (e.mods.isShiftDown())
        {
            auto ids = selectedIds;
            if (ids.contains (id)) ids.removeString (id); else ids.add (id);
            setSelectedIds (ids);
            if (! ids.contains (id)) return;
        }
        else if (! isSelected (item)) setSelectedIds ({ id });
        drag = Drag::move;
        moved.clear();
        for (auto& s : getSelectedItems()) moved.push_back ({ s, s.createCopy() });
        project.getUndoManager().beginNewTransaction (moved.size() == 1 ? "Move " + Board::kindName (item) : juce::String ("Move items"));
        return;
    }

    drag = Drag::rubber;
    rubberBase = e.mods.isShiftDown() ? selectedIds : juce::StringArray();
    if (! e.mods.isShiftDown()) setSelectedIds ({});
}

void BoardView::moveSelectionTo (juce::Point<float> d)
{
    for (auto& [item, was] : moved)
    {
        if (! item.getParent().isValid()) continue;
        if (item.hasType (IDs::ARROW))
        {
            if (item[IDs::fromItem].toString().isEmpty())
            {
                item.setProperty (IDs::x1, (double) was[IDs::x1] + d.x, um());
                item.setProperty (IDs::y1, (double) was[IDs::y1] + d.y, um());
            }
            if (item[IDs::toItem].toString().isEmpty())
            {
                item.setProperty (IDs::x2, (double) was[IDs::x2] + d.x, um());
                item.setProperty (IDs::y2, (double) was[IDs::y2] + d.y, um());
            }
            continue;
        }
        item.setProperty (IDs::x, (double) was[IDs::x] + d.x, um());
        item.setProperty (IDs::y, (double) was[IDs::y] + d.y, um());
    }
}

void BoardView::mouseDrag (const juce::MouseEvent& e)
{
    auto board = getBoard();
    const bool snapOn = ! e.mods.isCommandDown();
    current = toBoard (e.position);
    switch (drag)
    {
        case Drag::pan:
            origin -= (e.position - lastScreen) / zoom;
            positionEditor();
            break;
        case Drag::move:
        {
            auto d = current - downBoard;
            // The first item lands on the grid (a free arrow: its start).
            if (snapOn && ! moved.empty())
            {
                const auto& was = moved.front().second;
                const juce::Point<float> p0 = was.hasType (IDs::ARROW) ? juce::Point<float> ((float) (double) was[IDs::x1], (float) (double) was[IDs::y1])
                                                                       : juce::Point<float> ((float) (double) was[IDs::x], (float) (double) was[IDs::y]);
                d = snapPoint (p0 + d, true) - p0;
            }
            moveSelectionTo (d);
            break;
        }
        case Drag::corner:
        {
            auto r = dragRect;
            auto p = snapPoint (current, snapOn);
            float left = r.getX(), top = r.getY(), right = r.getRight(), bottom = r.getBottom();
            if (handle == 0 || handle == 3) left = juce::jmin (p.x, right - 8.0f); else right = juce::jmax (p.x, left + 8.0f);
            if (handle == 0 || handle == 1) top = juce::jmin (p.y, bottom - 8.0f); else bottom = juce::jmax (p.y, top + 8.0f);
            juce::Rectangle<float> nr (left, top, right - left, bottom - top);
            // Images keep their proportions (Shift frees them); other items keep theirs with Shift.
            const bool keep = dragItem.hasType (IDs::IMAGE) != e.mods.isShiftDown();
            if (keep && r.getHeight() > 0.5f)
            {
                const float aspect = r.getWidth() / r.getHeight();
                const float w = juce::jmax (nr.getWidth(), nr.getHeight() * aspect);
                const float hh = w / aspect;
                nr = { (handle == 0 || handle == 3) ? right - w : left, (handle == 0 || handle == 1) ? bottom - hh : top, w, hh };
            }
            Board::setRect (dragItem, nr, um());
            break;
        }
        case Drag::end:
        {
            auto target = dragItem.hasType (IDs::ARROW) ? itemUnder (current, dragItem) : juce::ValueTree();
            hoverTarget = target.isValid() ? target[IDs::id].toString() : juce::String();
            Board::setEnd (dragItem, handle, snapPoint (current, snapOn && hoverTarget.isEmpty()), hoverTarget, um());
            break;
        }
        case Drag::bend:
        {
            // The bend: how far the mouse is off the straight line between the ends.
            const auto ap = Board::arrowPath (board, dragItem);
            const auto a = ap.a, b = ap.b;
            auto dir = b - a;
            const float len = juce::jmax (1.0e-3f, dir.getDistanceFromOrigin());
            const juce::Point<float> normal (-dir.y / len, dir.x / len);
            float bend = (current - (a + b) * 0.5f).getDotProduct (normal);
            if (std::abs (bend) < 6.0f) bend = 0.0f;
            dragItem.setProperty (IDs::bend, std::round (bend), um());
            break;
        }
        case Drag::rubber:
        {
            const auto r = juce::Rectangle<float> (downBoard, current);
            auto ids = rubberBase;
            for (auto item : board)
                if (r.intersects (Board::bounds (board, item))) ids.addIfNotAlreadyThere (item[IDs::id].toString());
            setSelectedIds (ids);
            break;
        }
        case Drag::newCard: case Drag::newShape:
            current = snapPoint (current, snapOn);
            break;
        case Drag::newArrow:
        {
            auto target = itemUnder (current, Board::findItem (board, arrowFrom));
            hoverTarget = target.isValid() ? target[IDs::id].toString() : juce::String();
            break;
        }
        case Drag::pen:
            if (trail.isEmpty() || trail.getLast().getDistanceFrom (current) >= 1.5f) trail.add (current);
            break;
        case Drag::none:
            break;
    }
    lastScreen = e.position;
    repaint();
}

void BoardView::finishCreation (const juce::MouseEvent& e)
{
    auto board = getBoard();
    const bool tiny = e.position.getDistanceFrom (downScreen) < 4.0f;
    const auto colourOf = [] (int index) { return (int) Board::palette()[index].getARGB(); };
    if (drag == Drag::newCard)
    {
        auto r = tiny ? juce::Rectangle<float> (downBoard.x, downBoard.y, 220.0f, 100.0f) : juce::Rectangle<float> (downBoard, current);
        r = r.withWidth (juce::jmax (60.0f, r.getWidth())).withHeight (juce::jmax (40.0f, r.getHeight()));
        auto card = addItem (Board::makeCard (r, colourOf (0)), "New card");
        tool = Tool::select;
        editText (card);
    }
    else if (drag == Drag::newShape)
    {
        juce::Rectangle<float> r;
        if (shapeKind == "line")
            r = tiny ? juce::Rectangle<float> (downBoard.x, downBoard.y, 160.0f, 0.0f) : juce::Rectangle<float> (downBoard.x, downBoard.y, current.x - downBoard.x, current.y - downBoard.y);
        else
        {
            r = tiny ? juce::Rectangle<float> (downBoard.x, downBoard.y, 160.0f, 100.0f) : juce::Rectangle<float> (downBoard, current);
            r = r.withWidth (juce::jmax (8.0f, r.getWidth())).withHeight (juce::jmax (8.0f, r.getHeight()));
        }
        addItem (Board::makeShape (shapeKind, r, colourOf (3)), "New " + juce::String (shapeKind == "rect" ? "rectangle" : shapeKind));
        tool = Tool::select;
        status ({});
    }
    else if (drag == Drag::newArrow)
    {
        auto from = Board::findItem (board, arrowFrom);
        const auto a = from.isValid() ? Board::bounds (board, from).getCentre() : snapPoint (downBoard, ! e.mods.isCommandDown());
        const auto b = hoverTarget.isNotEmpty() ? Board::bounds (board, Board::findItem (board, hoverTarget)).getCentre() : snapPoint (current, ! e.mods.isCommandDown());
        if (a.getDistanceFrom (b) < 8.0f && (hoverTarget.isEmpty() || hoverTarget == arrowFrom))
        {
            status ("Drag from one card to another to draw an arrow.");
        }
        else
        {
            addItem (Board::makeArrow (a, b, arrowFrom, hoverTarget == arrowFrom ? juce::String() : hoverTarget, colourOf (6)), "New arrow");
            tool = Tool::select;
            status ({});
        }
    }
    else if (drag == Drag::pen)
    {
        if (trail.size() == 1) trail.add (trail[0] + juce::Point<float> (0.5f, 0.5f));
        // Fewer points: one every 3 units at least.
        juce::Array<juce::Point<float>> pts;
        for (auto& p : trail)
            if (pts.isEmpty() || pts.getLast().getDistanceFrom (p) >= 3.0f) pts.add (p);
        if (pts.getLast() != trail.getLast()) pts.add (trail.getLast());
        auto stroke = Board::makeStroke (pts, colourOf (6), 2.0f);
        auto added = addItem (stroke, "Draw");
        juce::ignoreUnused (added);
        setSelectedIds ({});        // the pen keeps drawing; nothing to handle yet
        trail.clearQuick();
    }
}

void BoardView::mouseUp (const juce::MouseEvent& e)
{
    switch (drag)
    {
        case Drag::newCard: case Drag::newShape: case Drag::newArrow: case Drag::pen:
            finishCreation (e);
            break;
        case Drag::end:
            break;
        case Drag::pan:
            setMouseCursor (juce::MouseCursor::NormalCursor);
            break;
        case Drag::move: case Drag::corner: case Drag::bend: case Drag::rubber: case Drag::none:
            break;
    }
    drag = Drag::none;
    hoverTarget.clear();
    arrowFrom.clear();
    moved.clear();
    dragItem = {};
    repaint();
}

void BoardView::mouseMove (const juce::MouseEvent& e)
{
    if (e.x < toolStripWidth || ! getBoard().isValid()) { setMouseCursor (juce::MouseCursor::NormalCursor); return; }
    int kind = -1;
    const int h = handleAt (e.position, kind);
    if (h >= 0) setMouseCursor (kind == 0 ? ((h == 0 || h == 2) ? juce::MouseCursor::TopLeftCornerResizeCursor : juce::MouseCursor::TopRightCornerResizeCursor)
                                          : juce::MouseCursor::CrosshairCursor);
    else if (tool == Tool::select) setMouseCursor (Board::itemAt (getBoard(), toBoard (e.position), tolerance()).isValid() ? juce::MouseCursor::DraggingHandCursor
                                                                                                                         : juce::MouseCursor::NormalCursor);
    else setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void BoardView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.x < toolStripWidth || ! getBoard().isValid() || tool != Tool::select) return;
    const auto p = toBoard (e.position);
    auto item = Board::itemAt (getBoard(), p, tolerance());
    if (item.isValid())
    {
        if (! item.hasType (IDs::STROKE)) editText (item);
        return;
    }
    addCardAt (p, true);
}

void BoardView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! getBoard().isValid()) return;
    if (e.mods.isCommandDown())
    {
        const float dy = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX;
        zoomAbout (e.position, zoom * std::pow (2.0f, dy * 1.5f));
        return;
    }
    const float k = 400.0f / zoom;
    origin -= juce::Point<float> (w.deltaX * k * (w.isReversed ? -1.0f : 1.0f), w.deltaY * k * (w.isReversed ? -1.0f : 1.0f));
    if (e.mods.isShiftDown() && std::abs (w.deltaX) < 1.0e-4f) origin += juce::Point<float> (-w.deltaY * k, w.deltaY * k);   // Shift-scroll: sideways
    positionEditor();
    repaint();
}

void BoardView::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    if (getBoard().isValid()) zoomAbout (e.position, zoom * scaleFactor);
}

//== Keys ======================================================================

bool BoardView::keyPressed (const juce::KeyPress& key)
{
    if (! getBoard().isValid()) return false;
    const auto mods = key.getModifiers();
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (! mods.isCommandDown() && ! mods.isAltDown() && ! mods.isCtrlDown())
    {
        if (c == 'v') { setTool (Tool::select); return true; }
        if (c == 't') { setTool (Tool::card); return true; }
        if (c == 'a') { setTool (Tool::arrow); return true; }
        if (c == 'p') { setTool (Tool::pen); return true; }
        if (c == 'i') { setTool (Tool::image); return true; }
        if (c == 's')
        {
            if (tool == Tool::shape) shapeKind = shapeKind == "rect" ? "ellipse" : (shapeKind == "ellipse" ? "line" : "rect");
            setTool (Tool::shape);
            return true;
        }
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (tool != Tool::select) setTool (Tool::select);
        else deselect();
        return true;
    }
    if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey) { deleteSelection(); return true; }
    if (key == juce::KeyPress::returnKey && getSelectedItems().size() == 1) { editText (getSelectedItems()[0]); return true; }
    if (mods.isCommandDown() && ! mods.isShiftDown() && ! mods.isAltDown())
    {
        if (c == 'c') { copySelection(); return true; }
        if (c == 'x') { cutSelection(); return true; }
        if (c == 'v') { paste(); return true; }
        if (c == 'd') { duplicateSelection(); return true; }
        if (c == 'a') { selectAll(); return true; }
        if (c == '0') { zoomToFit(); return true; }
        if (c == '=' || c == '+') { zoomBy (1.25f); return true; }
        if (c == '-') { zoomBy (0.8f); return true; }
    }
    // Arrow keys nudge the selection (Shift: by 10).
    const float step = mods.isShiftDown() ? 10.0f : 1.0f;
    juce::Point<float> nudge;
    if (key.getKeyCode() == juce::KeyPress::leftKey)  nudge = { -step, 0.0f };
    if (key.getKeyCode() == juce::KeyPress::rightKey) nudge = { step, 0.0f };
    if (key.getKeyCode() == juce::KeyPress::upKey)    nudge = { 0.0f, -step };
    if (key.getKeyCode() == juce::KeyPress::downKey)  nudge = { 0.0f, step };
    if (nudge != juce::Point<float>() && ! mods.isCommandDown())
    {
        auto items = getSelectedItems();
        if (items.isEmpty()) return true;
        project.getUndoManager().beginNewTransaction ("Nudge");
        for (auto& item : items) Board::moveBy (item, nudge, um());
        return true;
    }
    return false;
}

//== Menus =====================================================================

void BoardView::showItemMenu (juce::ValueTree item)
{
    const auto selected = getSelectedItems();
    const bool one = selected.size() == 1;
    const auto kind = Board::kindOf (item);
    juce::PopupMenu m;
    if (one && kind == Board::Kind::card) m.addItem (1, "Write in the card (double-click, Return)");
    if (one && (kind == Board::Kind::shape || kind == Board::Kind::arrow)) m.addItem (1, "Label (double-click)");
    if (one && kind == Board::Kind::image) m.addItem (1, "Caption (double-click)");
    juce::PopupMenu colours;
    for (int i = 0; i < Board::palette().size(); ++i)
    {
        static const char* names[] = { "Warm", "Blue", "Teal", "Rose", "Amber", "Grey", "Light" };
        colours.addColouredItem (10 + i, names[i], Board::palette()[i], true, one && Board::colourOf (item) == Board::palette()[i]);
    }
    if (kind != Board::Kind::image) m.addSubMenu ("Colour", colours);
    if (kind == Board::Kind::card)
    {
        juce::PopupMenu labels;
        const auto all = Board::labelsIn (project.getState());
        labels.addItem (100, "No label", true, item[IDs::label].toString().isEmpty());
        labels.addSeparator();
        for (int i = 0; i < all.size() && i < 80; ++i)
            labels.addItem (101 + i, all[i], true, one && item[IDs::label].toString() == all[i]);
        m.addSubMenu ("Label", labels);
    }
    if (kind == Board::Kind::shape || kind == Board::Kind::arrow || kind == Board::Kind::stroke)
    {
        m.addItem (30, "Dashed", true, (bool) item[IDs::dashed]);
        juce::PopupMenu widths;
        const float w = (float) (double) item.getProperty (IDs::lineWidth, 1.5);
        int i = 0;
        for (auto v : { 1.0f, 1.5f, 2.5f, 4.0f })
            widths.addItem (40 + i++, juce::String (v, 1), true, std::abs (w - v) < 0.01f);
        m.addSubMenu ("Line width", widths);
    }
    if (kind == Board::Kind::shape && item[IDs::shape].toString() != "line") m.addItem (31, "Filled", true, (bool) item[IDs::filled]);
    if (kind == Board::Kind::arrow) m.addItem (32, "Straight", std::abs ((double) item[IDs::bend]) > 0.5);
    m.addSeparator();
    m.addItem (50, "Bring to front");
    m.addItem (51, "Send to back");
    m.addSeparator();
    m.addItem (52, "Duplicate (Cmd-D)");
    m.addItem (53, "Copy (Cmd-C)");
    m.addItem (54, one ? "Delete" : "Delete items");
    m.addSeparator();
    m.addItem (60, one ? "Show it on the box" : "Show them on the box");

    juce::Component::SafePointer<BoardView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, item] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& um = safe->project.getUndoManager();
        auto items = safe->getSelectedItems();
        if (items.isEmpty()) items.add (item);
        if (r == 1) safe->editText (item);
        else if (r >= 10 && r < 10 + Board::palette().size())
        {
            um.beginNewTransaction ("Colour");
            for (auto& i : items) i.setProperty (IDs::colour, (int) Board::palette()[r - 10].getARGB(), safe->um());
        }
        else if (r >= 100 && r < 200)
        {
            const auto all = Board::labelsIn (safe->project.getState());
            const auto label = r == 100 ? juce::String() : all[r - 101];
            um.beginNewTransaction ("Label");
            for (auto& i : items)
                if (i.hasType (IDs::CARD)) i.setProperty (IDs::label, label, safe->um());
        }
        else if (r == 30 || r == 31)
        {
            const auto& id = r == 30 ? IDs::dashed : IDs::filled;
            const bool on = ! (bool) item[id];
            um.beginNewTransaction (r == 30 ? "Dashed" : "Filled");
            for (auto& i : items) i.setProperty (id, on, safe->um());
        }
        else if (r >= 40 && r < 44)
        {
            const float widths[] = { 1.0f, 1.5f, 2.5f, 4.0f };
            um.beginNewTransaction ("Line width");
            for (auto& i : items) if (! i.hasType (IDs::CARD) && ! i.hasType (IDs::IMAGE)) i.setProperty (IDs::lineWidth, widths[r - 40], safe->um());
        }
        else if (r == 32)
        {
            um.beginNewTransaction ("Straight");
            for (auto& i : items) if (i.hasType (IDs::ARROW)) i.setProperty (IDs::bend, 0.0, safe->um());
        }
        else if (r == 50) safe->bringToFront();
        else if (r == 51) safe->sendToBack();
        else if (r == 52) safe->duplicateSelection();
        else if (r == 53) safe->copySelection();
        else if (r == 54) safe->deleteSelection();
        else if (r == 60) safe->showOnBox();
        safe->repaint();
    });
}

void BoardView::showBoardMenu (juce::Point<float> p)
{
    juce::PopupMenu m;
    m.addItem (1, "New card here");
    m.addItem (2, "Paste here", canPaste());
    m.addItem (3, "Add images...");
    m.addSeparator();
    m.addItem (4, "Show what is in view on the box");
    m.addItem (5, "Show nothing on the box", ! Board::shownPart (getBoard()).isEmpty());
    m.addSeparator();
    m.addItem (6, "Select all (Cmd-A)", getBoard().getNumChildren() > 0);
    m.addItem (7, "Fit (Cmd-0)");
    juce::Component::SafePointer<BoardView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, p] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) safe->addCardAt (p, true);
        else if (r == 2)
        {
            safe->paste();
            // Pasted where the menu was opened.
            auto items = safe->getSelectedItems();
            const auto all = Board::boundsOf (safe->getBoard(), items);
            if (! items.isEmpty())
            {
                const auto d = safe->snapPoint (p - all.getTopLeft(), true);
                for (auto& i : items) Board::moveBy (i, d, safe->um());
            }
        }
        else if (r == 3) safe->chooseImages();
        else if (r == 4) { safe->deselect(); safe->showOnBox(); }
        else if (r == 5) safe->clearShownPart();
        else if (r == 6) safe->selectAll();
        else if (r == 7) safe->zoomToFit();
    });
}
