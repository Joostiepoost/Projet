// LevelBar.cpp

#include "LevelBar.h"
#include "Theme.h"
#include "TimelineView.h"

LevelBar::LevelBar (Project& p) : project (p)
{
    setInterceptsMouseClicks (true, false);
}

void LevelBar::setView (double start, double end)
{
    if (std::abs (start - viewStart) < 1.0e-9 && std::abs (end - viewEnd) < 1.0e-9) return;
    viewStart = start;
    viewEnd = end;
    repaint();
}

void LevelBar::setPlayhead (double pieceSeconds)
{
    if (std::abs (pieceSeconds - playheadPiece) < 1.0e-4) return;
    playheadPiece = pieceSeconds;
    repaint (0, crumbsHeight, getWidth(), stripHeight);
}

juce::ValueTree LevelBar::aroundTracks() const
{
    auto around = Project::containerOf (project.getFocus());
    return around.isValid() ? Project::innerTracks (around) : project.getRootTracks();
}

juce::Range<double> LevelBar::aroundRange()
{
    // The piece from 0:00, or the window of the container around, widened
    // to whatever lies there.
    auto focus = project.getFocus();
    auto around = Project::containerOf (focus);
    double first = 0.0, last = Project::contentEnd (aroundTracks());
    if (around.isValid())
    {
        first = Project::innerOffset (around);
        last = juce::jmax (last, first + Project::boxLength (around) / Project::containerScale (around));
        for (auto t : aroundTracks())
            for (auto b : t) first = juce::jmin (first, Project::boxStart (b));
    }
    else last = juce::jmax (last, project.getPieceLength());
    if (focus.isValid()) last = juce::jmax (last, Project::boxEnd (focus));
    const double pad = juce::jmax (0.05, (last - first) * 0.02);
    return { first - pad, last + pad };
}

float LevelBar::toX (double t, juce::Range<double> range) const
{
    const float x0 = (float) TimelineView::headerWidth, x1 = (float) getWidth() - 10.0f;
    return x0 + (float) ((t - range.getStart()) / juce::jmax (1.0e-9, range.getLength())) * (x1 - x0);
}

void LevelBar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    auto focus = project.getFocus();
    if (! focus.isValid()) return;

    // == Breadcrumbs: Piece > Section A > Phrase, then the depth and scale
    crumbs.clear();
    const auto path = project.focusPath();
    int x = 12;
    const int y = 3, h = crumbsHeight - 6;
    auto crumb = [&] (const juce::String& text, juce::ValueTree target, bool last)
    {
        g.setFont (Theme::font (12.5f, last));
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 4;
        juce::Rectangle<int> area (x, y, w, h);
        g.setColour (last ? Theme::text : Theme::accent);
        g.drawText (text, area, juce::Justification::centredLeft, false);
        if (! last) crumbs.push_back ({ area, target });
        x += w;
    };
    auto separator = [&]
    {
        g.setFont (Theme::font (12.5f));
        g.setColour (Theme::textMuted);
        g.drawText (juce::String::fromUTF8 ("\xe2\x80\xba"), x + 4, y, 10, h, juce::Justification::centred, false);
        x += 18;
    };
    crumb ("Piece", {}, false);
    for (int i = 0; i < path.size(); ++i)
    {
        separator();
        auto name = path[i][IDs::name].toString();
        crumb (name.isEmpty() ? juce::String ("Container") : name, path[i], i == path.size() - 1);
    }
    const double k = Project::containerScale (focus);
    juce::String about;
    about << "   depth " << path.size() << "   " << juce::String::fromUTF8 ("\xc2\xb7") << "   "
          << (Project::isStretch (focus) ? "stretch " + juce::String::fromUTF8 ("\xc3\x97") + juce::String (k, 2) : juce::String ("window"));
    g.setFont (Theme::font (11.5f));
    g.setColour (Theme::textMuted);
    g.drawText (about, x, y, 260, h, juce::Justification::centredLeft, false);
    g.drawText ("Cmd-Up: one level up", getWidth() - 190, y, 178, h, juce::Justification::centredRight, false);

    // == Context strip: the level around, the container you are in highlighted
    hits.clear();
    const auto range = aroundRange();
    const auto strip = juce::Rectangle<int> (0, crumbsHeight, getWidth(), stripHeight - 1);
    g.setColour (juce::Colour (0xff18191c));
    g.fillRect (strip);
    g.setColour (Theme::textMuted);
    g.setFont (Theme::font (11.0f));
    auto around = Project::containerOf (focus);
    g.drawText ("Around: " + (around.isValid() ? around[IDs::name].toString() : juce::String ("the piece")),
                strip.withWidth (TimelineView::headerWidth).reduced (12, 0), juce::Justification::centredLeft, true);

    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (strip.withTrimmedLeft (TimelineView::headerWidth));
    juce::Array<juce::ValueTree> lanes;
    for (auto t : aroundTracks()) if (! Project::isFolder (t)) lanes.add (t);
    const float area0 = (float) strip.getY() + 3.0f, areaH = (float) strip.getHeight() - 8.0f;
    const float rowH = lanes.isEmpty() ? areaH : juce::jlimit (2.0f, 10.0f, areaH / (float) lanes.size());
    for (int i = 0; i < lanes.size(); ++i)
    {
        auto lane = lanes[i];
        const auto colour = Theme::trackColour (project.effectiveColour (lane));
        const float ry = area0 + rowH * (float) i;
        for (auto b : lane)
        {
            const float bx0 = toX (Project::boxStart (b), range), bx1 = juce::jmax (bx0 + 2.0f, toX (Project::boxEnd (b), range));
            juce::Rectangle<float> r (bx0, ry, bx1 - bx0, juce::jmax (1.5f, rowH - 1.0f));
            const bool here = b == focus;
            if (here)
            {
                g.setColour (Theme::accent.withAlpha (0.45f));
                g.fillRect (r);
                g.setColour (Theme::accent);
                g.drawRect (r.expanded (1.0f), 1.5f);
            }
            else
            {
                g.setColour (colour.withAlpha (Project::isContainer (b) ? 0.55f : 0.35f));
                g.fillRect (r);
                if (Project::isContainer (b))
                {
                    g.setColour (colour.withAlpha (0.8f));
                    g.drawRect (r, 1.0f);
                }
            }
            hits.push_back ({ r.expanded (0.0f, 1.0f), b });
        }
    }

    // What the timeline below shows, and the playhead.
    if (viewEnd > viewStart)
    {
        const float v0 = toX (Project::toOuter (focus, viewStart), range), v1 = toX (Project::toOuter (focus, viewEnd), range);
        const float by = (float) strip.getBottom() - 3.0f;
        g.setColour (Theme::text.withAlpha (0.55f));
        g.fillRect (v0, by, juce::jmax (1.0f, v1 - v0), 1.5f);
        g.fillRect (v0, by - 3.0f, 1.0f, 4.5f);
        g.fillRect (v1 - 1.0f, by - 3.0f, 1.0f, 4.5f);
    }
    double t = playheadPiece;
    for (int i = 0; i + 1 < path.size(); ++i) t = Project::toInner (path[i], t);
    const float px = toX (t, range);
    g.setColour (Theme::accent);
    g.fillRect (px - 0.5f, (float) strip.getY(), 1.5f, (float) strip.getHeight());
}

void LevelBar::mouseDown (const juce::MouseEvent& e)
{
    for (auto& c : crumbs)
        if (c.area.contains (e.getPosition()))
        {
            if (onGoTo) onGoTo (c.target);
            return;
        }
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            if (Project::isContainer (it->box)) { if (onGoTo) onGoTo (it->box); }
            else if (onSelectAround) onSelectAround (it->box);
            return;
        }
}

void LevelBar::mouseMove (const juce::MouseEvent& e)
{
    bool over = false;
    for (auto& c : crumbs) over = over || c.area.contains (e.getPosition());
    for (auto& h : hits) over = over || h.area.contains (e.position);
    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
