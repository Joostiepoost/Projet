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

    bool isSoundFile (const juce::File& f)
    {
        return f.hasFileExtension ("wav;wave;aif;aiff;flac;ogg;mp3;m4a;caf;aac");
    }

    juce::Path iconPath (const char* svg)
    {
        return juce::Drawable::parseSVGPath (svg);
    }
}

BoardView::BoardView (Project& p, const Framework& f, const ProjectFolder& pf, const projet::SoundPool& sp)
    : project (p), framework (f), folder (pf), pool (sp)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    glyphs.rebuild (framework);
}

void BoardView::frameworkReloaded()
{
    glyphs.rebuild (framework);
    repaint();
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
    c.project = &project;
    c.pool = &pool;
    c.glyphs = &glyphs;
    c.boxLength = box.isValid() ? Project::boxLength (box) : 0.0;
    if (previewingCard) c.previewCard = previewingCard();
    if (c.previewCard.isNotEmpty() && previewSeconds) c.previewSeconds = previewSeconds();
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
    // Closer together when the board is low.
    const int step = juce::jlimit (28, 42, (getHeight() - 14) / 10);
    const int size = juce::jmin (36, step - 4);
    auto add = [&] (Tool t, const char* name, const char* key)
    {
        b.push_back ({ t, { 6 + (36 - size) / 2, y, size, size }, name, key });
        y += step;
    };
    add (Tool::select, "Select and move", "V");
    add (Tool::card, "Text card", "T");
    add (Tool::arrow, "Arrow", "A");
    add (Tool::shape, "Shape", "S");
    add (Tool::pen, "Pen", "P");
    add (Tool::image, "Image", "I");
    add (Tool::sound, "Sound card", "O");
    add (Tool::link, "Timeline link", "L");
    add (Tool::region, "Morphogram region", "M");
    add (Tool::gesture, "Energy gesture", "E");
    return b;
}

void BoardView::setTool (Tool t)
{
    if (t == Tool::image) { chooseImages(); return; }
    if (t == Tool::gesture && gestureEnergy.isEmpty()) { showEnergyMenu(); return; }
    commitEdit();
    tool = t;
    switch (t)
    {
        case Tool::select: status ({}); break;
        case Tool::card:   status ("Text card: click for a card, or drag its size; then write."); break;
        case Tool::arrow:  status ("Arrow: drag from a card (or anywhere) to another; its ends stay attached to the cards."); break;
        case Tool::shape:  status ("Shape (" + juce::String (shapeKind == "rect" ? "rectangle" : shapeKind) + "): drag to draw it; S again for the next shape."); break;
        case Tool::pen:    status ("Pen: draw freely; Esc or V to stop."); break;
        case Tool::sound:  status ("Sound card: click where it goes, then choose a sound of the project (or a file). Or drop sound files on the board."); break;
        case Tool::link:   status ("Timeline link: click where it goes, then choose the box. Or Alt-drag a box from the timeline onto the board."); break;
        case Tool::region: status ("Morphogram region: drag its frame (time across, register up); then draw gestures in it with E."); break;
        case Tool::gesture:
        {
            juce::String name = gestureEnergy;
            for (auto* c : BoardPainter::glyphCategories (framework))
                if (auto* v = framework.findValue (*c, gestureEnergy)) { name = framework.text (v->name); break; }
            status ("Energy gesture (" + name + "): draw its path, in a region for its time and register; E again or right-click the tool for another energy; Esc or V to stop.");
            break;
        }
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
            case Tool::sound:  icon = iconPath ("M4 7 V11 M7 4 V14 M10 6 V12 M13 8 V10"); break;
            case Tool::link:   icon = iconPath ("M3 9 H10 M7 5.5 L10.5 9 L7 12.5 M12 4 H15 V14 H12"); break;
            case Tool::region: icon = iconPath ("M3 4 H15 V14 H3 Z M3 7.5 H15 M3 11 H15"); break;
            case Tool::gesture: icon = iconPath ("M3 12 C 5 6, 7 6, 9 10 C 11 14, 13 13, 15 6 M4 14 L5 12 M8 13 L9 11 M12 15 L13 13"); break;
        }
        icon.applyTransform (juce::AffineTransform::translation (r.getCentreX() - 9.0f, r.getCentreY() - 9.0f));
        g.setColour (on ? juce::Colour (0xfff0cf8e) : Theme::textMuted.brighter (0.2f));
        g.strokePath (icon, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (Theme::font (8.5f));
        g.setColour (Theme::textMuted.withAlpha (0.7f));
        g.drawText (b.key, b.area.withTrimmedLeft (b.area.getWidth() - 11).withTrimmedTop (b.area.getHeight() - 12), juce::Justification::centred, false);
    }
}

void BoardView::showToolMenu (Tool t)
{
    if (t == Tool::gesture) { showEnergyMenu(); return; }
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
    // A region goes with the gestures drawn in it (9c).
    for (int i = 0; i < items.size(); ++i)
        if (items[i].hasType (IDs::REGION))
            for (auto& gst : Board::gesturesIn (board, items[i])) items.addIfNotAlreadyThere (gst);
    for (auto& item : items) Board::detachArrowsFrom (board, item[IDs::id].toString(), um());
    for (auto& item : items) board.removeChild (item, um());
    setSelectedIds ({});
}

void BoardView::copySelection()
{
    auto items = getSelectedItems();
    if (items.isEmpty()) return;
    for (int i = 0; i < items.size(); ++i)
        if (items[i].hasType (IDs::REGION))
            for (auto& gst : Board::gesturesIn (getBoard(), items[i])) items.addIfNotAlreadyThere (gst);
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
    for (int i = 0; i < items.size(); ++i)
        if (items[i].hasType (IDs::REGION))
            for (auto& gst : Board::gesturesIn (getBoard(), items[i])) items.addIfNotAlreadyThere (gst);
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
    if (items.size() == 1 && items[0].hasType (IDs::REGION))
    {
        // A region: the box shows its gestures in the box's own time (9c).
        project.getUndoManager().beginNewTransaction ("Show the region on the box");
        Board::setShownRegion (board, items[0], um());
        status (Board::regionInBoxTime (items[0]) ? "The box shows the morphogram in its own time: each gesture where it falls."
                                                  : "The box shows the region (its time is free, so as a picture).");
        return;
    }
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
        if (isImageFile (juce::File (f)) || isSoundFile (juce::File (f))) return true;
    return false;
}

void BoardView::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::Array<juce::File> images;
    juce::StringArray sounds;
    for (auto& f : files)
    {
        const juce::File file (f);
        if (isSoundFile (file)) sounds.add (f);
        else images.add (file);
    }
    if (onActivate) onActivate();
    const auto at = toBoard ({ (float) x, (float) y });
    if (! images.isEmpty()) addImages (images, at);
    if (! sounds.isEmpty() && onImportSounds) onImportSounds (sounds, snapPoint (at, true));     // sound cards when imported (9b)
}

//== Words typed in place ======================================================

void BoardView::editText (juce::ValueTree item)
{
    commitEdit();
    if (! item.isValid() || ! item.getParent().isValid()) return;
    const auto kind = Board::kindOf (item);
    if (kind == Board::Kind::link || kind == Board::Kind::stroke || kind == Board::Kind::gesture) return;
    editing = item;
    editingField = kind == Board::Kind::card ? IDs::notes : (kind == Board::Kind::image ? IDs::caption : IDs::label);
    setSelectedIds ({ item[IDs::id].toString() });
    editor = std::make_unique<juce::TextEditor>();
    const bool multi = editingField == IDs::notes;
    editor->setMultiLine (multi, true);
    editor->setReturnKeyStartsNewLine (multi);
    editor->setScrollbarsShown (multi);
    editor->setText (kind == Board::Kind::soundCard && item[IDs::label].toString().isEmpty() ? BoardPainter::soundCardName (item, &project)
                                                                                                : item[editingField].toString(), false);
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
    else if (editing.hasType (IDs::REGION))
    {
        const auto reg = Board::rect (editing);
        r = toScreen (juce::Rectangle<float> (reg.getX() + 4.0f, reg.getY() + 1.0f, juce::jmin (260.0f, reg.getWidth() - 8.0f), 19.0f));
        fontSize = 11.0f;
    }
    else if (editing.hasType (IDs::SOUNDCARD))
    {
        const auto play = BoardPainter::playButton (editing);
        const auto card = Board::rect (editing);
        r = toScreen (juce::Rectangle<float> (play.getRight() + 6.0f, card.getY() + 6.0f, card.getRight() - play.getRight() - 16.0f, 26.0f));
        fontSize = 13.0f;
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
    // A sound card named as its sound needs no label of its own.
    auto newText = text;
    if (item.hasType (IDs::SOUNDCARD) && field == IDs::label && text.trim() == project.findSound (item[IDs::soundId].toString())[IDs::name].toString())
        newText = {};
    if (item.isValid() && item.getParent().isValid() && newText != item[field].toString())
    {
        project.getUndoManager().beginNewTransaction (field == IDs::notes ? "Write in a card" : (field == IDs::caption ? "Caption" : "Label"));
        item.setProperty (field, field == IDs::notes ? newText : newText.trim(), um());
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
    const auto shownRegion = BoardPainter::shownRegion (board);
    const auto shown = shownRegion.isValid() ? Board::rect (shownRegion) : Board::shownPart (board);
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
        g.drawText (shownRegion.isValid() ? "shown on the box, in its time" : "shown on the box",
                    juce::Rectangle<float> (r.getX() + 4.0f, r.getY() - 16.0f, 200.0f, 14.0f), juce::Justification::bottomLeft, false);
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
    if (drag == Drag::rubber || drag == Drag::newCard || drag == Drag::newShape || drag == Drag::newRegion)
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
    else if ((drag == Drag::pen || drag == Drag::gesture) && trail.size() > 1)
    {
        juce::Path p;
        p.startNewSubPath (toScreen (trail[0]));
        for (int i = 1; i < trail.size(); ++i) p.lineTo (toScreen (trail[i]));
        if (drag == Drag::gesture)
        {
            // The band the glyph will take.
            g.setColour (Theme::accent.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (36.0f * zoom, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
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
                          "T card,  A arrow,  S shape (S again: ellipse, line),  P pen,  I image,  O sound card,  L link to a box,\n"
                          "M morphogram region,  E energy gesture.\n"
                          "Drop images or sounds here; Alt-drag a box from the timeline for a link. Scroll to move around, Cmd-scroll or pinch to zoom.",
                          canvas.reduced (40), juce::Justification::centred, 5);
    }

    if (activePane)
    {
        g.setColour (Theme::magnifier);
        g.fillRect (canvas.getX(), canvas.getY(), canvas.getWidth(), 2);
    }
    if (linkDragOver)
    {
        g.setColour (Theme::magnifier);
        g.drawRect (canvas, 3);
        g.setFont (Theme::font (13.0f, true));
        g.drawText ("Drop to link to this box", canvas.reduced (12).removeFromTop (22), juce::Justification::centredRight, false);
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
                if (b.tool == Tool::gesture && tool == Tool::gesture) { showEnergyMenu(); return; }
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
        case Tool::sound:
            showSoundMenu (snapPoint (downBoard, snapOn));
            return;
        case Tool::link:
            showLinkMenu (snapPoint (downBoard, snapOn));
            return;
        case Tool::region:
            drag = Drag::newRegion;
            downBoard = current = snapPoint (downBoard, snapOn);
            return;
        case Tool::gesture:
            drag = Drag::gesture;
            trail.clearQuick();
            trail.add (downBoard);
            return;
        case Tool::image: case Tool::select:
            break;
    }

    // A sound card's play button and fragment ends; a link's button (9b).
    if (auto hit = Board::itemAt (board, downBoard, tolerance()); hit.isValid())
    {
        if (hit.hasType (IDs::SOUNDCARD))
        {
            if (BoardPainter::playButton (hit).expanded (2.0f).contains (downBoard))
            {
                setSelectedIds ({ hit[IDs::id].toString() });
                if (onPlayCard) onPlayCard (hit);
                return;
            }
            if (const int end = trimHandleAt (hit, e.position); end >= 0)
            {
                setSelectedIds ({ hit[IDs::id].toString() });
                dragItem = hit;
                drag = end == 0 ? Drag::trimIn : Drag::trimOut;
                project.getUndoManager().beginNewTransaction ("Trim the sound card");
                return;
            }
        }
        if (hit.hasType (IDs::LINK) && BoardPainter::goButton (hit).expanded (2.0f).contains (downBoard))
        {
            setSelectedIds ({ hit[IDs::id].toString() });
            if (auto target = BoardPainter::linkedBox (hit, &project); target.isValid() && onGoToBox) onGoToBox (target);
            return;
        }
    }

    int kind = -1;
    const int h = handleAt (e.position, kind);
    if (h >= 0)
    {
        dragItem = getSelectedItems()[0];
        handle = h;
        if (kind == 0)
        {
            drag = Drag::corner;
            dragRect = Board::bounds (getBoard(), dragItem);
            regionGestures.clear();
            if (dragItem.hasType (IDs::REGION))
                for (auto& gst : Board::gesturesIn (getBoard(), dragItem)) regionGestures.push_back ({ gst, gst.createCopy() });
            project.getUndoManager().beginNewTransaction ("Resize " + Board::kindName (dragItem));
        }
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
        const auto selectedNow = getSelectedItems();
        for (auto& s : selectedNow) moved.push_back ({ s, s.createCopy() });
        // A region takes the gestures drawn in it along (9c).
        for (auto& s : selectedNow)
            if (s.hasType (IDs::REGION))
                for (auto& gst : Board::gesturesIn (board, s))
                    if (! selectedNow.contains (gst)) moved.push_back ({ gst, gst.createCopy() });
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
        case Drag::trimIn: case Drag::trimOut:
        {
            const double seconds = soundSeconds (dragItem);
            if (seconds <= 0.0) break;
            const auto wave = BoardPainter::waveArea (dragItem);
            double t = seconds * juce::jlimit (0.0, 1.0, (double) ((current.x - wave.getX()) / wave.getWidth()));
            const double in = Board::cardIn (dragItem), out = Board::cardOut (dragItem, seconds);
            if (drag == Drag::trimIn)
            {
                t = juce::jlimit (0.0, juce::jmax (0.0, out - 0.01), t);
                dragItem.setProperty (IDs::srcIn, t < 0.0005 ? 0.0 : t, um());
            }
            else
            {
                t = juce::jlimit (in + 0.01, seconds, t);
                if (t > seconds - 0.0005) dragItem.removeProperty (IDs::srcOut, um());
                else dragItem.setProperty (IDs::srcOut, t, um());
            }
            status ("The fragment: " + Board::formatSeconds (Board::cardIn (dragItem)) + juce::String::fromUTF8 (" \xe2\x80\x93 ")
                    + Board::formatSeconds (Board::cardOut (dragItem, seconds)) + " of " + Board::formatSeconds (seconds) + ".");
            break;
        }
        case Drag::dragOut:
            break;
        case Drag::move:
        {
            // A sound card pulled up out of the board, onto a track (9b).
            if (e.y < 0 && moved.size() == 1 && (moved.front().first.hasType (IDs::SOUNDCARD) || moved.front().first.hasType (IDs::GESTURE))
                && canDragOut && canDragOut())
            {
                auto item = moved.front().first;
                project.getUndoManager().undoCurrentTransactionOnly();     // it stays where it was on the board
                moved.clear();
                drag = Drag::dragOut;
                if (item.hasType (IDs::GESTURE)) startGestureOut (item); else startDragOut (item);
                break;
            }
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
            const float pad = dragItem.hasType (IDs::GESTURE) ? (float) (double) dragItem.getProperty (IDs::thickness, 36.0) * 0.5f : 0.0f;
            Board::setRect (dragItem, nr.reduced (pad), um());
            // A region's gestures keep their place in its time and register.
            for (auto& [gst, was] : regionGestures)
            {
                if (! gst.getParent().isValid()) continue;
                const auto old = Board::rect (was);
                const auto a = Board::regionInside (r), b = Board::regionInside (nr);
                auto mapX = [&] (float x) { return b.getX() + (x - a.getX()) * b.getWidth() / juce::jmax (1.0f, a.getWidth()); };
                auto mapY = [&] (float y) { return b.getY() + (y - a.getY()) * b.getHeight() / juce::jmax (1.0f, a.getHeight()); };
                gst.copyPropertiesFrom (was, um());
                Board::setRect (gst, juce::Rectangle<float> (juce::Point<float> (mapX (old.getX()), mapY (old.getY())),
                                                             juce::Point<float> (mapX (old.getRight()), mapY (old.getBottom()))), um());
            }
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
        case Drag::newCard: case Drag::newShape: case Drag::newRegion:
            current = snapPoint (current, snapOn);
            break;
        case Drag::gesture:
            if (trail.isEmpty() || trail.getLast().getDistanceFrom (current) >= 1.5f) trail.add (current);
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
    else if (drag == Drag::newRegion)
    {
        auto r = tiny ? juce::Rectangle<float> (downBoard.x, downBoard.y, 480.0f, 240.0f) : juce::Rectangle<float> (downBoard, current);
        r = r.withWidth (juce::jmax (120.0f, r.getWidth())).withHeight (juce::jmax (90.0f, r.getHeight()));
        auto region = Board::makeRegion (r, colourOf (3));
        project.getUndoManager().beginNewTransaction ("New morphogram region");
        board.addChild (region, 0, um());          // behind everything: gestures go on it
        setSelectedIds ({ region[IDs::id].toString() });
        tool = Tool::select;
        status ("A morphogram region: E to draw energy gestures in it. Its time and register are in the panel.");
    }
    else if (drag == Drag::gesture)
    {
        juce::Array<juce::Point<float>> pts;
        for (auto& p : trail)
            if (pts.isEmpty() || pts.getLast().getDistanceFrom (p) >= 3.0f) pts.add (p);
        if (pts.size() < 2 || trail.getFirst().getDistanceFrom (trail.getLast()) < 6.0f)
            status ("Draw a gesture's path with the mouse held down.");
        else
        {
            if (pts.getLast() != trail.getLast()) pts.add (trail.getLast());
            auto region = Board::regionAt (board, trail.getFirst());
            auto gesture = Board::makeGesture (pts, region, 36.0f);
            if (auto cats = BoardPainter::glyphCategories (framework); ! cats.isEmpty() && gestureEnergy.isNotEmpty())
                gesture.setProperty (cats[0]->property(), gestureEnergy, nullptr);
            addItem (gesture, "Draw a gesture");
        }
        trail.clearQuick();
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
        case Drag::newCard: case Drag::newShape: case Drag::newArrow: case Drag::pen: case Drag::newRegion: case Drag::gesture:
            finishCreation (e);
            break;
        case Drag::end:
            break;
        case Drag::pan:
            setMouseCursor (juce::MouseCursor::NormalCursor);
            break;
        case Drag::move:
        {
            // A gesture moved on its own belongs to the region it now starts in (or none) (9c).
            auto board = getBoard();
            juce::StringArray movedRegions;
            for (auto& [item, was] : moved)
                if (item.hasType (IDs::REGION)) movedRegions.add (item[IDs::id].toString());
            for (auto& [item, was] : moved)
            {
                if (! item.hasType (IDs::GESTURE) || ! item.getParent().isValid() || movedRegions.contains (item[IDs::regionId].toString())) continue;
                const auto pts = Board::strokePoints (item);
                if (pts.isEmpty()) continue;
                const auto region = Board::regionAt (board, pts[0]);
                const auto id = region[IDs::id].toString();
                if (id == item[IDs::regionId].toString()) continue;
                if (id.isEmpty()) item.removeProperty (IDs::regionId, um());
                else item.setProperty (IDs::regionId, id, um());
            }
            break;
        }
        case Drag::corner: case Drag::bend: case Drag::rubber: case Drag::none:
        case Drag::trimIn: case Drag::trimOut: case Drag::dragOut:
            break;
    }
    regionGestures.clear();
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
    if (tool == Tool::select)
        if (auto hit = Board::itemAt (getBoard(), toBoard (e.position), tolerance()); hit.isValid())
        {
            const auto p = toBoard (e.position);
            if ((hit.hasType (IDs::SOUNDCARD) && BoardPainter::playButton (hit).expanded (2.0f).contains (p))
                || (hit.hasType (IDs::LINK) && BoardPainter::goButton (hit).expanded (2.0f).contains (p)))
            { setMouseCursor (juce::MouseCursor::PointingHandCursor); return; }
            if (hit.hasType (IDs::SOUNDCARD) && trimHandleAt (hit, e.position) >= 0) { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); return; }
        }
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
        if (item.hasType (IDs::LINK))
        {
            if (auto target = BoardPainter::linkedBox (item, &project); target.isValid() && onGoToBox) onGoToBox (target);
            else showLinkMenu (Board::rect (item).getTopLeft(), item);
        }
        else if (! item.hasType (IDs::STROKE)) editText (item);
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
        if (c == 'o') { setTool (Tool::sound); return true; }
        if (c == 'l') { setTool (Tool::link); return true; }
        if (c == 'm') { setTool (Tool::region); return true; }
        if (c == 'e')
        {
            if (tool == Tool::gesture) showEnergyMenu(); else setTool (Tool::gesture);
            return true;
        }
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
    if (one && kind == Board::Kind::soundCard)
    {
        const bool hearing = previewingCard && previewingCard() == item[IDs::id].toString();
        m.addItem (70, hearing ? "Stop" : "Play (its button, Space)");
        m.addItem (71, "Plays in the piece (temporal out)", true, Board::isTemporalOut (item));
        m.addItem (72, "The whole sound", Board::cardIn (item) > 0.0 || item.hasProperty (IDs::srcOut));
        m.addItem (73, "Name (double-click)");
    }
    juce::ValueTree linked;
    if (one && kind == Board::Kind::link)
    {
        linked = BoardPainter::linkedBox (item, &project);
        m.addItem (80, "Go there (its button, double-click)", linked.isValid());
        m.addItem (81, "Point it elsewhere...");
        const auto takeId = item[IDs::targetTake].toString();
        if (linked.isValid() && takeId.isNotEmpty() && linked[IDs::activeTake].toString() != takeId)
            if (auto take = linked.getChildWithName (IDs::TAKES).getChildWithProperty (IDs::id, takeId); take.isValid())
                m.addItem (82, "Switch the box to take " + take[IDs::name].toString());
    }
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
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, item, linked] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& um = safe->project.getUndoManager();
        auto items = safe->getSelectedItems();
        if (items.isEmpty()) items.add (item);
        if (r == 1 || r == 73) safe->editText (item);
        else if (r == 70) { if (safe->onPlayCard) safe->onPlayCard (item); }
        else if (r == 71)
        {
            um.beginNewTransaction (Board::isTemporalOut (item) ? "Plays on the board only" : "Plays in the piece");
            item.setProperty (IDs::temporalOut, ! Board::isTemporalOut (item), safe->um());
        }
        else if (r == 72)
        {
            um.beginNewTransaction ("The whole sound");
            item.removeProperty (IDs::srcIn, safe->um());
            item.removeProperty (IDs::srcOut, safe->um());
        }
        else if (r == 80) { if (safe->onGoToBox && linked.isValid()) safe->onGoToBox (linked); }
        else if (r == 81) safe->showLinkMenu (Board::rect (item).getTopLeft(), item);
        else if (r == 82 && linked.isValid())
        {
            um.beginNewTransaction ("Switch take");
            safe->project.setActiveTake (linked, linked.getChildWithName (IDs::TAKES).getChildWithProperty (IDs::id, item[IDs::targetTake].toString()));
        }
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
    m.addItem (8, "Sound card here...");
    m.addItem (9, "Link to a box here...");
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
        else if (r == 8) safe->showSoundMenu (safe->snapPoint (p, true));
        else if (r == 9) safe->showLinkMenu (safe->snapPoint (p, true));
        else if (r == 4) { safe->deselect(); safe->showOnBox(); }
        else if (r == 5) safe->clearShownPart();
        else if (r == 6) safe->selectAll();
        else if (r == 7) safe->zoomToFit();
    });
}

//== Sound cards and links (9b) ================================================

juce::ValueTree BoardView::singleSelected() const
{
    const auto items = getSelectedItems();
    return items.size() == 1 ? items[0] : juce::ValueTree();
}

double BoardView::soundSeconds (const juce::ValueTree& card) const
{
    const auto id = card[IDs::soundId].toString();
    if (auto s = pool.get (id)) return s->seconds();
    return project.soundSeconds (id);
}

int BoardView::trimHandleAt (const juce::ValueTree& card, juce::Point<float> screen) const
{
    const double seconds = soundSeconds (card);
    if (seconds <= 0.0) return -1;
    const auto wave = toScreen (BoardPainter::waveArea (card));
    if (! wave.expanded (0.0f, 2.0f).contains (screen)) return -1;
    const float xIn = wave.getX() + wave.getWidth() * (float) (Board::cardIn (card) / seconds);
    const float xOut = wave.getX() + wave.getWidth() * (float) (Board::cardOut (card, seconds) / seconds);
    const float dIn = std::abs (screen.x - xIn), dOut = std::abs (screen.x - xOut);
    if (juce::jmin (dIn, dOut) > 6.0f) return -1;
    return dIn < dOut ? 0 : 1;
}

juce::ValueTree BoardView::addSoundCard (const juce::String& soundId, juce::Point<float> p, bool undoable)
{
    auto board = getBoard();
    if (! board.isValid() || ! project.findSound (soundId).isValid()) return {};
    auto card = Board::makeSoundCard (soundId, { p.x, p.y, 240.0f, 92.0f }, (int) Board::palette()[2].getARGB());
    if (undoable) project.getUndoManager().beginNewTransaction ("New sound card");
    board.appendChild (card, um());
    setSelectedIds ({ card[IDs::id].toString() });
    tool = Tool::select;
    status ("Click its button (or Space) to hear it; drag the ends of the waveform for a fragment; mark it out in the panel to hear it in the piece.");
    return card;
}

void BoardView::showSoundMenu (juce::Point<float> p)
{
    juce::PopupMenu m;
    juce::Array<juce::ValueTree> sounds;
    for (auto s : project.getSounds()) sounds.add (s);
    std::sort (sounds.begin(), sounds.end(), [] (const juce::ValueTree& a, const juce::ValueTree& b)
               { return a[IDs::name].toString().compareNatural (b[IDs::name].toString()) < 0; });
    m.addSectionHeader ("A sound card here");
    m.addItem (1, "From a file...");
    if (! sounds.isEmpty()) m.addSeparator();
    for (int i = 0; i < sounds.size() && i < 500; ++i)
        m.addItem (100 + i, sounds[i][IDs::name].toString() + "   " + Board::formatSeconds (project.soundSeconds (sounds[i][IDs::id].toString())));
    juce::Component::SafePointer<BoardView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, p, sounds] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) { if (safe->onChooseSoundFiles) safe->onChooseSoundFiles (p); safe->tool = Tool::select; safe->repaint(); return; }
        if (r >= 100 && r - 100 < sounds.size()) safe->addSoundCard (sounds[r - 100][IDs::id].toString(), p);
    });
}

void BoardView::showLinkMenu (juce::Point<float> p, juce::ValueTree relink)
{
    // The piece's boxes as its outline: a container's inside as a submenu.
    auto boxes = std::make_shared<juce::Array<juce::ValueTree>>();
    const auto thisBox = box;
    std::function<void (juce::PopupMenu&, const juce::ValueTree&)> fill = [&] (juce::PopupMenu& menu, const juce::ValueTree& tracks)
    {
        for (auto track : tracks)
        {
            if (Project::isFolder (track)) continue;
            for (auto b : track)
            {
                if (! b.hasType (IDs::BOX) || boxes->size() > 600) continue;
                auto name = b[IDs::name].toString();
                if (name.isEmpty()) name = Project::isContainer (b) ? "Container" : (Project::isSound (b) ? "Sound" : (Project::isBoard (b) ? "Board" : "Text"));
                if (Project::isContainer (b))
                {
                    juce::PopupMenu inside;
                    boxes->add (b);
                    inside.addItem (1000 + boxes->size() - 1, "This container");
                    inside.addSeparator();
                    fill (inside, Project::innerTracks (b));
                    menu.addSubMenu (name, inside);
                }
                else
                {
                    boxes->add (b);
                    menu.addItem (1000 + boxes->size() - 1, name, b != thisBox);
                }
            }
        }
    };
    juce::PopupMenu m;
    m.addSectionHeader (relink.isValid() ? "Point the link to" : "A link here, to");
    fill (m, project.getRootTracks());
    if (boxes->isEmpty()) m.addItem (1, "(no boxes yet)", false);
    juce::Component::SafePointer<BoardView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, p, boxes, relink] (int r) mutable
    {
        if (safe == nullptr || r < 1000 || r - 1000 >= boxes->size()) { if (safe != nullptr) safe->tool = Tool::select; return; }
        auto target = (*boxes)[r - 1000];
        safe->tool = Tool::select;
        if (relink.isValid() && relink.getParent().isValid())
        {
            safe->project.getUndoManager().beginNewTransaction ("Point the link elsewhere");
            relink.setProperty (IDs::targetBox, target[IDs::id].toString(), safe->um());
            if (target.getChildWithName (IDs::TAKES).isValid()) relink.setProperty (IDs::targetTake, target[IDs::activeTake].toString(), safe->um());
            else relink.removeProperty (IDs::targetTake, safe->um());
            relink.setProperty (IDs::name, target[IDs::name].toString(), safe->um());
            safe->repaint();
            return;
        }
        auto link = Board::makeLink (target, { p.x, p.y, 240.0f, 56.0f });
        link.setProperty (IDs::name, target[IDs::name].toString(), nullptr);
        safe->addItem (link, "New link");
        safe->status ("Its button (or a double-click) goes to the box.");
        safe->repaint();
    });
}

void BoardView::startDragOut (const juce::ValueTree& card)
{
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr) return;
    const double seconds = soundSeconds (card);
    const double length = juce::jmax (0.05, Board::cardOut (card, seconds) - Board::cardIn (card));
    const auto name = BoardPainter::soundCardName (card, &project);
    juce::Image image (juce::Image::ARGB, 180, 26, true);
    {
        juce::Graphics g (image);
        g.setColour (Board::colourOf (card).withAlpha (0.85f));
        g.fillRoundedRectangle (image.getBounds().toFloat().reduced (1.0f), 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (name, image.getBounds().reduced (8, 0), juce::Justification::centredLeft, true);
    }
    status ("Drop it on a track for a sound box with this fragment.");
    container->startDragging ("projet:soundcard:" + box[IDs::id].toString() + ":" + card[IDs::id].toString() + ":" + juce::String (length, 6),
                              this, juce::ScaledImage (image), true);
}

bool BoardView::isInterestedInDragSource (const SourceDetails& d)
{
    return getBoard().isValid() && d.description.toString().startsWith ("projet:box:");
}

void BoardView::itemDragEnter (const SourceDetails&)
{
    linkDragOver = true;
    repaint();
}

void BoardView::itemDragExit (const SourceDetails&)
{
    linkDragOver = false;
    repaint();
}

void BoardView::itemDropped (const SourceDetails& d)
{
    linkDragOver = false;
    const auto id = d.description.toString().fromFirstOccurrenceOf ("projet:box:", false, false);
    auto target = project.findBoxInPiece (id);
    if (! target.isValid() || ! getBoard().isValid()) { repaint(); return; }
    if (onActivate) onActivate();
    const auto at = snapPoint (toBoard (d.localPosition.toFloat()), true);
    auto link = Board::makeLink (target, { at.x, at.y, 240.0f, 56.0f });
    link.setProperty (IDs::name, target[IDs::name].toString(), nullptr);
    addItem (link, "New link");
    status ("A link to " + target[IDs::name].toString() + ": its button (or a double-click) goes there.");
    repaint();
}

//== Regions and gestures (9c) =================================================

void BoardView::showEnergyMenu()
{
    auto cats = BoardPainter::glyphCategories (framework);
    if (cats.isEmpty()) { status ("The framework has no energies with glyphs to draw gestures with.", true); return; }
    const auto* c = cats[0];
    juce::PopupMenu m;
    m.addSectionHeader ("Draw gestures with");
    const auto& values = framework.valuesOf (*c);
    for (int i = 0; i < values.size() && i < 200; ++i)
    {
        if (values[i].glyph.isEmpty()) continue;
        const auto colours = framework.valueColours (*c, values[i].id);
        const auto colour = colours.light != 0 ? juce::Colour (colours.light) : (colours.colour != 0 ? juce::Colour (colours.colour) : Theme::text);
        m.addColouredItem (1 + i, framework.text (values[i].name), colour, true, values[i].id == gestureEnergy);
    }
    juce::Component::SafePointer<BoardView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, values] (int r)
    {
        if (safe == nullptr || r <= 0 || r - 1 >= values.size()) return;
        safe->gestureEnergy = values[r - 1].id;
        safe->setTool (Tool::gesture);
    });
}

void BoardView::startGestureOut (const juce::ValueTree& gesture)
{
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr) return;
    const auto span = Board::gestureSpan (getBoard(), gesture, Project::boxLength (box));
    const double length = span.getLength() > 0.01 ? span.getLength() : 4.0;
    juce::String name = "Gesture";
    if (auto cats = BoardPainter::glyphCategories (framework); ! cats.isEmpty())
        if (auto* v = framework.findValue (*cats[0], gesture[cats[0]->property()].toString())) name = framework.text (v->name);
    juce::Image image (juce::Image::ARGB, 180, 26, true);
    {
        juce::Graphics g (image);
        g.setColour (BoardPainter::gestureColour (gesture, framework).withAlpha (0.85f));
        g.fillRoundedRectangle (image.getBounds().toFloat().reduced (1.0f), 4.0f);
        g.setColour (juce::Colours::black);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (name, image.getBounds().reduced (8, 0), juce::Justification::centredLeft, true);
    }
    status ("Drop it on a track: a text box " + Board::formatSeconds (length) + " long with its energy and curves, waiting for its sound.");
    container->startDragging ("projet:gesture:" + box[IDs::id].toString() + ":" + gesture[IDs::id].toString() + ":" + juce::String (length, 6),
                              this, juce::ScaledImage (image), true);
}
