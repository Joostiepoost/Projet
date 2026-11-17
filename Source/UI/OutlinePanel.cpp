// OutlinePanel.cpp

#include "OutlinePanel.h"
#include "Theme.h"
#include "TimeFormat.h"

OutlinePanel::OutlinePanel (Project& p) : project (p)
{
    viewport.setViewedComponent (&rows, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    hideButton.setTooltip ("Hide the outline (Cmd-Alt-O)");
    hideButton.onClick = [this] { if (onHide) onHide(); };
    addAndMakeVisible (hideButton);
}

void OutlinePanel::refresh()
{
    stale = true;
    rows.repaint();
    resized();
}

void OutlinePanel::setSelected (const juce::StringArray& ids)
{
    if (ids == selected) return;
    selected = ids;
    rows.repaint();
}

void OutlinePanel::setLevels (const juce::ValueTree& upper, const juce::ValueTree& magnifiedBelow)
{
    shown = upper;
    magnified = magnifiedBelow;
    rows.repaint();
}

void OutlinePanel::showFocus()
{
    if (stale) rebuild();
    const auto focus = shown;
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].box == focus)
        {
            const int y = (int) i * rowHeight;
            const int top = viewport.getViewPositionY(), h = viewport.getHeight();
            if (y < top || y + rowHeight > top + h) viewport.setViewPosition (0, juce::jmax (0, y - h / 3));
            break;
        }
}

void OutlinePanel::rebuild()
{
    stale = false;
    list.clear();
    list.push_back ({ {}, 0, true, Theme::text });
    addLevel (project.getRootTracks(), 1);
    rows.setSize (juce::jmax (10, viewport.getMaximumVisibleWidth()), (int) list.size() * rowHeight + 8);
}

void OutlinePanel::addLevel (const juce::ValueTree& tracks, int depth)
{
    // The boxes of a level in time order (then track order), whatever track they are on.
    struct Item { juce::ValueTree box; int track; };
    std::vector<Item> items;
    int index = 0;
    for (auto t : tracks)
    {
        if (! Project::isFolder (t))
            for (auto b : t) items.push_back ({ b, index });
        ++index;
    }
    std::stable_sort (items.begin(), items.end(), [] (const Item& x, const Item& y)
    {
        const double a = Project::boxStart (x.box), b = Project::boxStart (y.box);
        return std::abs (a - b) > 1.0e-9 ? a < b : x.track < y.track;
    });
    for (auto& it : items)
    {
        const bool container = Project::isContainer (it.box);
        list.push_back ({ it.box, depth, container, Theme::trackColour (project.effectiveColour (it.box.getParent())) });
        if (container && depth < 32 && folded.count (it.box[IDs::id].toString()) == 0)
            addLevel (Project::innerTracks (it.box), depth + 1);
    }
}

void OutlinePanel::resized()
{
    auto r = getLocalBounds();
    auto title = r.removeFromTop (titleHeight);
    hideButton.setBounds (title.removeFromRight (34).reduced (4, 6));
    r.removeFromRight (1);
    viewport.setBounds (r);
    if (stale) rebuild();
    rows.setSize (juce::jmax (10, viewport.getMaximumVisibleWidth()), (int) list.size() * rowHeight + 8);
}

void OutlinePanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::line);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
    g.fillRect (0, titleHeight - 1, getWidth(), 1);
    g.setColour (Theme::textMuted);
    g.setFont (Theme::font (12.0f, true));
    g.drawText ("OUTLINE", 14, 0, getWidth() - 50, titleHeight, juce::Justification::centredLeft);
}

void OutlinePanel::Rows::paint (juce::Graphics& g)
{
    auto& o = owner;
    if (o.stale) o.rebuild();
    const auto focus = o.shown;
    const int w = getWidth();
    for (size_t i = 0; i < o.list.size(); ++i)
    {
        const auto& row = o.list[i];
        const int y = (int) i * rowHeight;
        const bool here = row.box == focus;                 // the level shown (the piece: an invalid box)
        const bool isSelected = row.box.isValid() && o.selected.contains (row.box[IDs::id].toString());
        if (isSelected)
        {
            g.setColour (Theme::accent.withAlpha (0.16f));
            g.fillRect (0, y, w, rowHeight);
        }
        if (here)
        {
            g.setColour (Theme::accent.withAlpha (0.09f));
            g.fillRect (0, y, w, rowHeight);
            g.setColour (Theme::accent);
            g.fillRect (0, y + 2, 3, rowHeight - 4);
        }
        else if (o.magnified.isValid() && row.box == o.magnified)     // magnified below (stage 6)
        {
            g.setColour (Theme::magnifier.withAlpha (0.12f));
            g.fillRect (0, y, w, rowHeight);
            g.setColour (Theme::magnifier);
            g.fillRect (0, y + 2, 3, rowHeight - 4);
        }

        // Depth number, then the indent
        g.setColour (Theme::textMuted.withAlpha (0.8f));
        g.setFont (Theme::mono (10.0f));
        g.drawText (juce::String (row.depth), 6, y, 14, rowHeight, juce::Justification::centredLeft);
        int x = o.indentFor (row.depth);
        const float cy = (float) y + rowHeight * 0.5f;

        if (row.container && row.box.isValid())
        {
            const bool isFolded = o.folded.count (row.box[IDs::id].toString()) > 0;
            juce::Path tri;
            if (isFolded) tri.addTriangle ((float) x - 9.0f, cy - 4.0f, (float) x - 9.0f, cy + 4.0f, (float) x - 3.0f, cy);
            else          tri.addTriangle ((float) x - 10.0f, cy - 3.0f, (float) x - 2.0f, cy - 3.0f, (float) x - 6.0f, cy + 3.0f);
            g.setColour (Theme::textMuted);
            g.fillPath (tri);
        }

        // A small sign of the kind: filled (sound), outlined (text), double (container)
        juce::Rectangle<float> icon ((float) x + 1.0f, cy - 5.0f, 10.0f, 10.0f);
        if (! row.box.isValid())
        {
            g.setColour (Theme::text);
            g.drawRect (icon, 1.2f);
            g.drawRect (icon.reduced (2.5f), 1.0f);
        }
        else if (row.container)
        {
            g.setColour (row.colour);
            g.drawRect (icon, 1.2f);
            g.fillRect (icon.reduced (3.0f));
        }
        else if (Project::isSound (row.box))
        {
            g.setColour (row.colour.withAlpha (Project::gainOpacity (row.box.getProperty (IDs::gainDb, 0.0))));
            g.fillRect (icon.reduced (1.0f));
        }
        else
        {
            g.setColour (juce::Colour (0xff9a8f78));
            g.drawRect (icon.reduced (1.0f), 1.0f);
        }
        x += 16;

        juce::String name = row.box.isValid() ? row.box[IDs::name].toString() : juce::String ("Piece");
        if (name.isEmpty()) name = row.container ? "Container" : (Project::isSound (row.box) ? "Sound" : "Text");
        const juce::String time = row.box.isValid() ? TimeFormat::format (Project::boxStart (row.box), 1) : juce::String();
        g.setFont (Theme::mono (10.0f));
        int timeW = time.isEmpty() ? 0 : juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), time) + 8;
        g.setColour (Theme::textMuted);
        if (timeW > 0 && w - x > timeW + 30) g.drawText (time, w - timeW - 4, y, timeW, rowHeight, juce::Justification::centredRight);
        else timeW = 0;
        g.setColour (here || row.container ? Theme::text : Theme::text.withAlpha (0.85f));
        g.setFont (Theme::font (12.0f, here || ! row.box.isValid()));
        g.drawText (name, x, y, juce::jmax (10, w - x - timeW - 6), rowHeight, juce::Justification::centredLeft, true);
    }
}

void OutlinePanel::Rows::mouseDown (const juce::MouseEvent& e)
{
    auto& o = owner;
    const int i = e.y / rowHeight;
    if (i < 0 || i >= (int) o.list.size()) return;
    const auto row = o.list[(size_t) i];
    // The triangle folds a container.
    if (row.container && row.box.isValid() && e.x < o.indentFor (row.depth) && e.x > o.indentFor (row.depth) - 14)
    {
        const auto id = row.box[IDs::id].toString();
        if (o.folded.count (id) > 0) o.folded.erase (id); else o.folded.insert (id);
        o.refresh();
        return;
    }
    if (e.getNumberOfClicks() == 1 && o.onSelect) o.onSelect (row.box);
}

void OutlinePanel::Rows::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& o = owner;
    const int i = e.y / rowHeight;
    if (i < 0 || i >= (int) o.list.size()) return;
    const auto row = o.list[(size_t) i];
    if (row.container && o.onDiveIn) o.onDiveIn (row.box);
}
