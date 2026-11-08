// TimelineView.cpp

#include "TimelineView.h"
#include "Theme.h"
#include "TimeFormat.h"
#include "../Engine/Arrangement.h"
#include "Markdown.h"

namespace
{
    constexpr float edgeGrab = 6.0f;        // pixels around a box edge that trim
    constexpr float handleSize = 8.0f;      // fade handles
    constexpr float snapPixels = 8.0f;

    bool isAudioFile (const juce::String& path)
    {
        static const juce::StringArray exts { ".wav", ".wave", ".aif", ".aiff", ".flac", ".ogg", ".mp3", ".m4a", ".caf", ".aac" };
        return exts.contains (juce::File (path).getFileExtension().toLowerCase());
    }
}

TimelineView::TimelineView (Project& p, projet::SoundPool& sp)
    : project (p), pool (sp)
{
    setWantsKeyboardFocus (false);
}

void TimelineView::resized() {}

//== Geometry ==================================================================

std::vector<TimelineView::TrackRow> TimelineView::rows() const
{
    std::vector<TrackRow> r;
    int y = rulerHeight - scrollY;
    for (auto t : project.getTracks())
    {
        if (! project.isVisible (t)) continue;    // inside a folded folder
        const bool folder = Project::isFolder (t);
        const int h = folder ? folderHeight : juce::jlimit (40, 240, (int) t.getProperty (IDs::height, 72));
        r.push_back ({ t, y, h, project.depthOf (t), folder });
        y += h;
    }
    return r;
}

int TimelineView::contentHeight() const
{
    int h = 0;
    for (auto& r : rows()) h += r.h;
    return h + addTrackHeight;
}

TimelineView::TrackRow TimelineView::rowAt (int y) const
{
    for (auto& r : rows())
        if (y >= r.y && y < r.y + r.h) return r;
    return {};
}

juce::Rectangle<float> TimelineView::boxBounds (const juce::ValueTree& box, const TrackRow& row) const
{
    const float x0 = timeToX (Project::boxStart (box));
    const float x1 = timeToX (Project::boxEnd (box));
    return { x0, (float) row.y + 4.0f, juce::jmax (2.0f, x1 - x0), (float) row.h - 8.0f };
}

juce::Rectangle<int> TimelineView::muteButton (const TrackRow& r) const   { return { headerWidth - 52, r.folder ? r.y + (r.h - 18) / 2 : r.y + 8, 20, 18 }; }
juce::Rectangle<int> TimelineView::soloButton (const TrackRow& r) const   { return { headerWidth - 28, r.folder ? r.y + (r.h - 18) / 2 : r.y + 8, 20, 18 }; }

juce::Rectangle<int> TimelineView::addTrackButton() const
{
    auto rs = rows();
    const int y = rs.empty() ? rulerHeight - scrollY : rs.back().y + rs.back().h;
    return { 10, y + 6, 90, 22 };
}

juce::ValueTree TimelineView::boxAt (juce::Point<float> p, Zone& zone)
{
    zone = Zone::none;
    auto row = rowAt ((int) p.y);
    if (! row.track.isValid() || row.folder || p.x < (float) headerWidth) return {};

    // Last drawn is on top.
    for (int i = row.track.getNumChildren(); --i >= 0;)
    {
        auto box = row.track.getChild (i);
        auto r = boxBounds (box, row);
        if (! r.expanded (edgeGrab, 0.0f).contains (p)) continue;

        const float fiX = timeToX (Project::boxStart (box) + (double) box[IDs::fadeIn]);
        const float foX = timeToX (Project::boxEnd (box) - (double) box[IDs::fadeOut]);
        const float top = r.getY();

        if (Project::isSound (box) && r.getWidth() > 24.0f && p.y < top + handleSize + 4.0f)
        {
            if (std::abs (p.x - fiX) <= handleSize) { zone = Zone::fadeInHandle; return box; }
            if (std::abs (p.x - foX) <= handleSize) { zone = Zone::fadeOutHandle; return box; }
        }

        if (std::abs (p.x - r.getX()) <= edgeGrab)     zone = Zone::startEdge;
        else if (std::abs (p.x - r.getRight()) <= edgeGrab) zone = Zone::endEdge;
        else zone = Zone::body;
        return box;
    }
    return {};
}

double TimelineView::snap (double t, const juce::ValueTree& exclude, bool enabled) const
{
    if (! enabled) return t;
    const double range = snapPixels / pps;
    double best = t, bestD = range;

    auto consider = [&] (double c) { const double d = std::abs (c - t); if (d < bestD) { bestD = d; best = c; } };
    consider (playhead);
    consider (0.0);
    for (auto tr : project.getState().getChildWithName (IDs::TRACKS))
        for (auto b : tr)
            if (b != exclude) { consider (Project::boxStart (b)); consider (Project::boxEnd (b)); }

    // The proportion ruler: its frame's edges and the divisions shown at this zoom.
    const auto f = project.frameFor (project.getRulerFrame());
    if (f.isValid())
    {
        consider (f.start);
        consider (f.end());
        for (auto& k : visibleTicks()) consider (k.time);
    }
    return best;
}

//== Proportions ===============================================================

std::vector<TimelineView::Tick> TimelineView::visibleTicks() const
{
    std::vector<Tick> out;
    const auto f = project.frameFor (project.getRulerFrame());
    if (! f.isValid()) return out;

    const double widthPx = f.length * pps;
    auto dens = project.getDenominators();
    std::sort (dens.begin(), dens.end());

    // The closest two ticks would be (in pixels) with this set of denominators.
    auto closestGap = [widthPx] (const juce::Array<double>& set)
    {
        double prev = 0.0, gap = 1.0;
        for (auto& d : Proportion::divisions (set)) { gap = juce::jmin (gap, d.position - prev); prev = d.position; }
        return juce::jmin (gap, 1.0 - prev) * widthPx;
    };

    // Simplest denominators first, each only while the ticks stay apart:
    // zoom in and the finer ones join.
    auto pick = [&] (double minGap)
    {
        juce::Array<double> chosen;
        for (auto d : dens)
        {
            auto trial = chosen;
            trial.add (d);
            if (closestGap (trial) >= minGap) chosen = trial;
        }
        return chosen;
    };
    const auto shown = project.getAutoDivisions() ? pick (7.0) : dens;
    const auto labelled = pick (34.0);

    double maxRank = 2.0;
    for (auto d : shown) maxRank = juce::jmax (maxRank, std::ceil (d));
    for (auto& d : Proportion::divisions (shown))
    {
        Tick k;
        k.time = f.at (d.position);
        k.division = d;
        // The simplest fraction full height, the finest shown a third.
        k.height = maxRank <= 2.0 ? 1.0f : (float) juce::jlimit (0.33, 1.0, 1.0 - 0.67 * (d.rank - 2.0) / (maxRank - 2.0));
        k.labelled = Proportion::indexOf (labelled, d.denominator) >= 0;
        out.push_back (k);
    }
    return out;
}

juce::String TimelineView::readoutFor (const juce::ValueTree& box, double time, const juce::String& what) const
{
    const auto ref = project.isBound (box) ? Project::boundFrame (box) : project.getRulerFrame();
    const auto f = project.frameFor (ref);
    juce::String s = what + " ";
    if (f.isValid())
        s << Proportion::describe (f.proportionOf (time), project.getDenominators()) << " of " << project.frameName (ref) << "  ";
    s << TimeFormat::format (time);
    return s;
}

juce::Rectangle<int> TimelineView::foldButton (const TrackRow& r) const
{
    return { 6 + r.depth * 14, r.y + (r.h - 14) / 2, 14, 14 };
}

TimelineView::TrackDrop TimelineView::trackDropAt (int y) const
{
    TrackDrop d;
    if (! draggedTrack.isValid()) return d;
    auto tracks = project.getTracks();
    auto rs = rows();
    if (rs.empty()) return d;

    auto refuses = [&] (const juce::ValueTree& t) { return t == draggedTrack || project.isInside (t, draggedTrack); };

    if (y >= rs.back().y + rs.back().h)
    {
        // Below the last track: the end, at the top level.
        d.index = tracks.getNumChildren();
        d.lineY = rs.back().y + rs.back().h;
        d.valid = true;
        return d;
    }

    for (auto& r : rs)
    {
        if (y < r.y || y >= r.y + r.h) continue;
        if (refuses (r.track)) return d;
        const int rel = y - r.y;
        const auto index = tracks.indexOf (r.track);

        if (r.folder && rel >= r.h / 4 && rel < r.h * 3 / 4)
        {
            // Into the folder, at its end.
            d.index = project.blockEnd (r.track);
            d.folderId = r.track[IDs::id].toString();
            d.into = r.track;
        }
        else if (rel < r.h / 2)
        {
            d.index = index;
            d.folderId = r.track[IDs::folderId].toString();
            d.lineY = r.y;
            d.indent = r.depth * 14;
        }
        else if (r.folder && ! (bool) r.track[IDs::folded])
        {
            // Just below an open folder: its first track.
            d.index = index + 1;
            d.folderId = r.track[IDs::id].toString();
            d.lineY = r.y + r.h;
            d.indent = (r.depth + 1) * 14;
        }
        else
        {
            d.index = project.blockEnd (r.track);
            d.folderId = r.track[IDs::folderId].toString();
            d.lineY = r.y + r.h;
            d.indent = r.depth * 14;
        }
        d.valid = true;
        return d;
    }
    return d;
}

//== Selection and view ========================================================

juce::ValueTree TimelineView::getSelectedBox()
{
    return selectedBoxIds.size() == 1 ? project.findBox (selectedBoxIds[0]) : juce::ValueTree();
}

juce::Array<juce::ValueTree> TimelineView::getSelectedBoxes()
{
    juce::Array<juce::ValueTree> boxes;
    for (auto b : project.allBoxes())
        if (isSelected (b)) boxes.add (b);
    return boxes;
}

void TimelineView::selectionChanged()
{
    // Forget boxes that no longer exist.
    for (int i = selectedBoxIds.size(); --i >= 0;)
        if (! project.findBox (selectedBoxIds[i]).isValid()) selectedBoxIds.remove (i);
    repaint();
    if (onSelectionChanged) onSelectionChanged();
}

void TimelineView::selectBox (const juce::ValueTree& box)
{
    juce::StringArray ids;
    if (box.isValid())
    {
        ids.add (box[IDs::id].toString());
        selectedTrackId = box.getParent()[IDs::id].toString();
    }
    if (ids == selectedBoxIds) return;
    selectedBoxIds = ids;
    selectionChanged();
}

void TimelineView::setSelection (const juce::Array<juce::ValueTree>& boxes)
{
    juce::StringArray ids;
    for (auto& b : boxes) ids.add (b[IDs::id].toString());
    if (! boxes.isEmpty()) selectedTrackId = boxes.getLast().getParent()[IDs::id].toString();
    if (ids == selectedBoxIds) return;
    selectedBoxIds = ids;
    selectionChanged();
}

void TimelineView::selectAll()
{
    setSelection (project.allBoxes());
}

juce::ValueTree TimelineView::getSelectedLaneTrack()
{
    auto t = project.findTrack (selectedTrackId);
    if (t.isValid() && ! Project::isFolder (t)) return t;
    auto lanes = project.laneTracks();
    if (t.isValid())
        for (auto l : lanes)
            if (project.isInside (l, t)) return l;
    return lanes.getFirst();
}

juce::ValueTree TimelineView::getSelectedTrack()
{
    auto tracks = project.getTracks();
    auto t = tracks.getChildWithProperty (IDs::id, selectedTrackId);
    return t.isValid() ? t : tracks.getChild (0);
}

void TimelineView::setView (double start, double pixelsPerSecond)
{
    pps = juce::jlimit (0.5, 4000.0, pixelsPerSecond);
    viewStart = juce::jmax (0.0, start);
    repaint();
}

void TimelineView::zoomBy (double factor, double around)
{
    const float x = timeToX (around);
    pps = juce::jlimit (0.5, 4000.0, pps * factor);
    viewStart = juce::jmax (0.0, around - (double) (x - (float) headerWidth) / pps);
    repaint();
}

void TimelineView::zoomToFit()
{
    const double end = juce::jmax (10.0, project.getContentEnd() * 1.05);
    const double width = juce::jmax (100, getWidth() - headerWidth - 20);
    setView (0.0, width / end);
}

void TimelineView::followPlayhead (double seconds)
{
    const double visible = (double) (getWidth() - headerWidth) / pps;
    if (seconds > viewStart + visible * 0.92 || seconds < viewStart)
    {
        viewStart = juce::jmax (0.0, seconds - visible * 0.1);
        repaint();
    }
}

void TimelineView::setPlayhead (double seconds)
{
    if (std::abs (seconds - playhead) * pps < 0.25) return;
    const float oldX = timeToX (playhead), newX = timeToX (seconds);
    playhead = seconds;
    // Wide enough for the triangle on top of the playhead (10 px) on any display scale.
    repaint ((int) std::floor (juce::jmin (oldX, newX)) - 8, 0, (int) std::ceil (std::abs (newX - oldX)) + 17, getHeight());
}

//== Painting ==================================================================

void TimelineView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
    auto rs = rows();

    // Lanes
    for (size_t i = 0; i < rs.size(); ++i)
    {
        auto& r = rs[i];
        g.setColour (r.folder ? juce::Colour (0xff1a1b1f) : (i % 2 == 0 ? Theme::laneEven : Theme::laneOdd));
        g.fillRect (headerWidth, r.y, getWidth() - headerWidth, r.h);
        g.setColour (Theme::line);
        g.fillRect (headerWidth, r.y + r.h - 1, getWidth() - headerWidth, 1);
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (headerWidth, rulerHeight, getWidth() - headerWidth, getHeight() - rulerHeight);

        // The proportion grid: the frame's edges and the labelled divisions.
        const auto frame = project.frameFor (project.getRulerFrame());
        if (frame.isValid())
        {
            const float top = (float) rulerHeight, bottom = (float) getHeight();
            const float dashes[] = { 3.0f, 4.0f };
            g.setColour (Theme::accent.withAlpha (0.10f));
            for (auto& k : visibleTicks())
                if (k.labelled)
                {
                    const float x = timeToX (k.time);
                    g.drawDashedLine ({ x, top, x, bottom }, dashes, 2, 1.0f);
                }
            g.setColour (Theme::accent.withAlpha (0.28f));
            for (double t : { frame.start, frame.end() })
            {
                const float x = timeToX (t);
                g.drawDashedLine ({ x, top, x, bottom }, dashes, 2, 1.0f);
            }
        }

        // Folder lanes: a thin summary of every box inside.
        for (auto& r : rs)
        {
            if (! r.folder) continue;
            for (auto t : project.getTracks())
            {
                if (Project::isFolder (t) || ! project.isInside (t, r.track)) continue;
                const auto colour = Theme::trackColour (project.effectiveColour (t)).withAlpha (project.effectiveMute (t) ? 0.3f : 0.75f);
                g.setColour (colour);
                for (auto box : t)
                {
                    const float x0 = timeToX (Project::boxStart (box)), x1 = timeToX (Project::boxEnd (box));
                    if (x1 < (float) headerWidth || x0 > (float) getWidth()) continue;
                    g.fillRoundedRectangle (x0, (float) r.y + (float) r.h * 0.5f - 3.0f, juce::jmax (2.0f, x1 - x0), 6.0f, 2.0f);
                }
            }
        }

        // Boxes
        for (auto& r : rs)
            for (auto box : r.track)
            {
                auto b = boxBounds (box, r);
                if (b.getRight() < (float) headerWidth || b.getX() > (float) getWidth()) continue;
                paintBox (g, box, r);

                // The ruler's frame: a dashed outline. A box with proportions: a corner mark
                // (filled while it follows its frame).
                if (box[IDs::id].toString() == project.getRulerFrame())
                {
                    juce::Path outline, dashed;
                    outline.addRoundedRectangle (b.expanded (2.5f), 5.0f);
                    const float dashes[] = { 5.0f, 3.0f };
                    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
                    g.setColour (Theme::accent);
                    g.fillPath (dashed);
                }
                if (project.isBound (box) && b.getWidth() > 14.0f)
                {
                    juce::Path corner;
                    corner.addTriangle (b.getRight() - 10.0f, b.getY() + 1.0f, b.getRight() - 1.0f, b.getY() + 1.0f, b.getRight() - 1.0f, b.getY() + 10.0f);
                    g.setColour (Theme::accent);
                    if (Project::isLocked (box)) g.fillPath (corner);
                    else g.strokePath (corner, juce::PathStrokeType (1.0f));
                }
            }

        if (project.getContentEnd() <= 0.0)
        {
            g.setColour (Theme::textMuted);
            g.setFont (Theme::font (15.0f));
            g.drawText ("Drop sound files here, or choose File > Import sounds (Cmd-I)",
                        juce::Rectangle<int> (headerWidth, rulerHeight, getWidth() - headerWidth, juce::jmin (200, getHeight() - rulerHeight)),
                        juce::Justification::centred);
        }
    }

    // Headers
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (0, rulerHeight, headerWidth, getHeight() - rulerHeight);
        g.setColour (Theme::header);
        g.fillRect (0, rulerHeight, headerWidth, getHeight() - rulerHeight);
        for (size_t i = 0; i < rs.size(); ++i)
            paintTrackHeader (g, rs[i], (int) i);

        auto add = addTrackButton();
        g.setColour (Theme::lineStrong);
        g.drawRoundedRectangle (add.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (Theme::textMuted);
        g.setFont (Theme::font (12.5f));
        g.drawText ("+ Track", add, juce::Justification::centred);
    }

    g.setColour (Theme::line);
    g.fillRect (headerWidth - 1, rulerHeight, 1, getHeight() - rulerHeight);

    paintRuler (g);
    paintStrip (g);

    // Where a dragged track would land
    if (dragMode == DragMode::track && trackDrop.valid)
    {
        g.setColour (Theme::accent);
        if (trackDrop.into.isValid())
        {
            for (auto& r : rs)
                if (r.track == trackDrop.into)
                    g.drawRect (0, r.y, getWidth(), r.h, 2);
        }
        else
            g.fillRect (trackDrop.indent + 4, trackDrop.lineY - 1, getWidth() - trackDrop.indent - 4, 3);
    }

    // The proportion of the edge being moved
    if (readout.isNotEmpty())
    {
        g.setFont (Theme::mono (11.5f));
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), readout) + 14;
        auto box = juce::Rectangle<int> (readoutPos.x + 14, readoutPos.y - 30, w, 20)
                       .constrainedWithin (getLocalBounds().withTrimmedTop (rulerHeight).reduced (4));
        g.setColour (juce::Colour (0xf0101114));
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (Theme::accent);
        g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Theme::text);
        g.drawText (readout, box, juce::Justification::centred);
    }

    if (! lasso.isEmpty())
    {
        g.setColour (Theme::accent.withAlpha (0.12f));
        g.fillRect (lasso);
        g.setColour (Theme::accent.withAlpha (0.7f));
        g.drawRect (lasso, 1);
    }

    // Playhead
    const float px = timeToX (playhead);
    if (px >= (float) headerWidth)
    {
        g.setColour (Theme::accent);
        g.fillRect (px - 0.5f, 0.0f, 1.5f, (float) getHeight());
        juce::Path tri;
        tri.addTriangle (px - 5.0f, (float) timeRulerHeight - 9.0f, px + 5.0f, (float) timeRulerHeight - 9.0f, px, (float) timeRulerHeight - 1.0f);
        g.fillPath (tri);
    }

    if (dropHighlight)
    {
        g.setColour (Theme::accent.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 2);
    }
}

void TimelineView::paintRuler (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff18191c));
    g.fillRect (0, 0, getWidth(), timeRulerHeight);
    g.setColour (Theme::line);
    g.fillRect (0, timeRulerHeight - 1, getWidth(), 1);

    g.setColour (Theme::textMuted);
    g.setFont (Theme::font (11.0f));
    g.drawText ("Seconds", 12, 0, headerWidth - 16, timeRulerHeight, juce::Justification::centredLeft);

    // A step that leaves at least 70 px between labels.
    static const double steps[] = { 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1200 };
    double step = steps[std::size (steps) - 1];
    for (double s : steps) if (s * pps >= 70.0) { step = s; break; }
    const int decimals = step < 0.1 ? 2 : (step < 1.0 ? 1 : 0);

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (headerWidth, 0, getWidth() - headerWidth, timeRulerHeight);

    const double end = xToTime ((float) getWidth());
    const double first = std::floor (viewStart / step) * step;
    for (double t = first; t <= end; t += step)
    {
        const float x = timeToX (t);
        g.setColour (Theme::lineStrong);
        g.fillRect (x, 12.0f, 1.0f, (float) timeRulerHeight - 13.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::mono (11.0f));
        g.drawText (TimeFormat::format (t, decimals), (int) x + 4, 2, 80, 16, juce::Justification::centredLeft);

        // Minor ticks
        g.setColour (Theme::line);
        for (int k = 1; k < 5; ++k)
        {
            const float mx = timeToX (t + step * k / 5.0);
            g.fillRect (mx, (float) timeRulerHeight - 6.0f, 1.0f, 5.0f);
        }
    }
}

void TimelineView::paintStrip (juce::Graphics& g)
{
    const int y0 = timeRulerHeight, h = stripHeight;
    g.setColour (juce::Colour (0xff1b1a17));
    g.fillRect (0, y0, getWidth(), h);
    g.setColour (Theme::line);
    g.fillRect (0, y0 + h - 1, getWidth(), 1);

    const auto ref = project.getRulerFrame();
    const auto frame = project.frameFor (ref);
    juce::String label = "Proportion: " + project.frameName (ref);
    if (ref == "piece") label << " " << TimeFormat::format (frame.length, 0) << (project.hasSetPieceLength() ? "" : " (auto)");
    g.setColour (Theme::accent);
    g.setFont (Theme::font (10.5f));
    g.drawText (label + juce::String::fromUTF8 (" \xe2\x96\xbe"), 12, y0, headerWidth - 16, h, juce::Justification::centredLeft, true);

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (headerWidth, y0, getWidth() - headerWidth, h);

    // A span that is not the frame: a faint bar.
    if (project.hasSpan() && ref != "span")
    {
        const auto sp = project.frameFor ("span");
        g.setColour (Theme::accent.withAlpha (0.3f));
        g.fillRect (timeToX (sp.start), (float) (y0 + h - 4), timeToX (sp.end()) - timeToX (sp.start), 3.0f);
    }

    if (! frame.isValid()) return;
    const float fx0 = timeToX (frame.start), fx1 = timeToX (frame.end());
    g.setColour (Theme::accent.withAlpha (0.08f));
    g.fillRect (fx0, (float) y0, fx1 - fx0, (float) h - 1.0f);

    // Ticks: simpler fractions taller and brighter.
    auto ticks = visibleTicks();
    for (auto& k : ticks)
    {
        const float x = timeToX (k.time);
        const float th = ((float) h - 3.0f) * k.height;
        g.setColour (Theme::accent.withAlpha (k.division.rank <= 3.0 ? 0.95f : 0.55f));
        g.fillRect (x, (float) (y0 + h - 1) - th, 1.0f, th);
    }

    // Labels, simplest first, never overlapping.
    std::vector<size_t> order (ticks.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b) { return ticks[a].division.rank < ticks[b].division.rank; });
    std::vector<juce::Range<int>> taken;
    g.setFont (Theme::mono (9.5f));
    for (auto i : order)
    {
        if (! ticks[i].labelled) continue;
        const auto text = ticks[i].division.label();
        const int x = (int) timeToX (ticks[i].time) + 3;
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 2;
        const juce::Range<int> range (x - 4, x + w + 4);
        bool free = true;
        for (auto& t : taken) if (t.intersects (range)) { free = false; break; }
        if (! free) continue;
        taken.push_back (range);
        g.setColour (ticks[i].division.rank <= 3.0 ? Theme::accent : Theme::accent.withAlpha (0.7f));
        g.drawText (text, x, y0 + 1, w, 10, juce::Justification::centredLeft, false);
    }

    // The frame's edges (draggable).
    g.setColour (Theme::accent);
    g.fillRect (fx0 - 1.0f, (float) y0, 2.0f, (float) h - 1.0f);
    g.fillRect (fx1 - 1.0f, (float) y0, 2.0f, (float) h - 1.0f);
}

void TimelineView::paintTrackHeader (juce::Graphics& g, const TrackRow& r, int index)
{
    juce::ignoreUnused (index);
    auto track = r.track;
    const bool selected = track[IDs::id].toString() == selectedTrackId;
    const int indent = r.depth * 14;

    g.setColour (selected ? juce::Colour (0xff25272c) : (r.folder ? juce::Colour (0xff202227) : Theme::header));
    g.fillRect (0, r.y, headerWidth, r.h);
    g.setColour (Theme::trackColour (project.effectiveColour (track)));
    g.fillRect (0, r.y, 4, r.h - 1);
    g.setColour (Theme::line);
    g.fillRect (0, r.y + r.h - 1, headerWidth, 1);
    // Guides for the levels of folders
    g.setColour (Theme::lineStrong.withAlpha (0.5f));
    for (int d = 0; d < r.depth; ++d)
        g.fillRect (12 + d * 14, r.y, 1, r.h - 1);

    if (r.folder)
    {
        auto b = foldButton (r).toFloat();
        juce::Path tri;
        if ((bool) track[IDs::folded]) tri.addTriangle (b.getX() + 4.0f, b.getY() + 2.0f, b.getRight() - 2.0f, b.getCentreY(), b.getX() + 4.0f, b.getBottom() - 2.0f);
        else tri.addTriangle (b.getX() + 2.0f, b.getY() + 4.0f, b.getRight() - 2.0f, b.getY() + 4.0f, b.getCentreX(), b.getBottom() - 2.0f);
        g.setColour (Theme::textMuted);
        g.fillPath (tri);

        g.setColour (juce::Colour (0xffc9c6bd));
        g.setFont (Theme::font (11.5f, true));
        const int x = foldButton (r).getRight() + 4;
        g.drawText (track[IDs::name].toString().toUpperCase(), x, r.y, headerWidth - 56 - x, r.h, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (Theme::text);
        g.setFont (Theme::font (13.5f, selected));
        g.drawText (track[IDs::name].toString(), 14 + indent, r.y + 6, headerWidth - 72 - indent, 22, juce::Justification::centredLeft, true);
    }

    auto drawToggle = [&] (juce::Rectangle<int> b, const juce::String& label, bool on, juce::Colour onColour)
    {
        g.setColour (on ? onColour : Theme::panel2);
        g.fillRoundedRectangle (b.toFloat(), 3.0f);
        g.setColour (on ? onColour : Theme::line);
        g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 3.0f, 1.0f);
        g.setColour (on ? Theme::background : Theme::textMuted);
        g.setFont (Theme::font (11.0f, true));
        g.drawText (label, b, juce::Justification::centred);
    };
    drawToggle (muteButton (r), "M", (bool) track[IDs::mute], juce::Colour (0xffd08a5a));
    drawToggle (soloButton (r), "S", (bool) track[IDs::solo], Theme::accent);

    if (! r.folder && r.h >= 56)
    {
        g.setColour (Theme::textMuted);
        g.setFont (Theme::font (11.0f));
        g.drawText (juce::String (track.getNumChildren()) + (track.getNumChildren() == 1 ? " box" : " boxes"),
                    14 + indent, r.y + 30, headerWidth - 24 - indent, 16, juce::Justification::centredLeft);
    }
}

void TimelineView::paintBox (juce::Graphics& g, const juce::ValueTree& box, const TrackRow& row)
{
    auto r = boxBounds (box, row);
    const bool selected = isSelected (box);
    const bool sound = Project::isSound (box);
    float alpha = juce::jlimit (0.15f, 1.0f, (float) (double) box.getProperty (IDs::opacity, 1.0));
    if (project.effectiveMute (row.track)) alpha *= 0.45f;     // muted track (or folder): dimmed
    const auto base = Theme::trackColour (project.effectiveColour (row.track));
    const bool missing = sound && pool.get (box[IDs::soundId].toString()) == nullptr;

    if (! sound)
    {
        // Text box: warm paper, dashed edge, the name and the first lines of the notes.
        g.setColour (juce::Colour (0xff2f2b25).withAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.toNearestInt());
            auto inner = r.reduced (7.0f, 4.0f);
            g.setColour (Theme::text.withAlpha (juce::jmax (0.6f, alpha)));
            g.setFont (Theme::font (12.0f, true));
            g.drawText (box[IDs::name].toString(), inner.removeFromTop (16.0f), juce::Justification::topLeft, true);
            const auto md = box[IDs::notes].toString();
            if (md.trim().isNotEmpty())
                Markdown::drawPreview (g, md, inner, 11.0f, juce::Colour (0xffd6cbb2).withAlpha (juce::jmax (0.6f, alpha)));
            else if (inner.getHeight() > 12.0f)
            {
                g.setColour (Theme::textMuted.withAlpha (0.8f));
                g.setFont (Theme::font (11.0f).italicised());
                g.drawText ("double-click to write", inner, juce::Justification::topLeft, true);
            }
        }
        juce::Path outline;
        outline.addRoundedRectangle (r.reduced (0.5f), 4.0f);
        if (selected)
        {
            g.setColour (Theme::accent);
            g.strokePath (outline, juce::PathStrokeType (2.0f));
        }
        else
        {
            juce::Path dashed;
            const float dashes[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (juce::Colour (0xff9a8f78).withAlpha (alpha));
            g.fillPath (dashed);
        }
        return;
    }

    g.setColour (base.darker (1.1f).withAlpha (alpha));
    g.fillRoundedRectangle (r, 4.0f);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        paintWaveform (g, box, r.reduced (0.0f, 3.0f).withTrimmedTop (14.0f));

        // Fades: shade the faded part and draw the curve.
        const float fiX = timeToX (Project::boxStart (box) + (double) box[IDs::fadeIn]);
        const float foX = timeToX (Project::boxEnd (box) - (double) box[IDs::fadeOut]);
        auto drawFade = [&] (float x0, float x1, FadeShape shape, bool rising)
        {
            if (x1 - x0 < 1.0f) return;
            juce::Path curve, shade;
            const int steps = juce::jlimit (2, 64, (int) (x1 - x0) / 3);
            for (int i = 0; i <= steps; ++i)
            {
                const double u = (double) i / steps;
                const float gain = projet::fadeCurve (shape, rising ? u : 1.0 - u);
                const float x = x0 + (x1 - x0) * (float) u;
                const float y = r.getBottom() - gain * r.getHeight();
                if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
            }
            shade = curve;
            shade.lineTo (x1, r.getY());
            shade.lineTo (x0, r.getY());
            shade.closeSubPath();
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillPath (shade);
            g.setColour (Theme::text.withAlpha (0.75f));
            g.strokePath (curve, juce::PathStrokeType (1.0f));
        };
        drawFade (r.getX(), fiX, (FadeShape) (int) box[IDs::fadeInShape], true);
        drawFade (foX, r.getRight(), (FadeShape) (int) box[IDs::fadeOutShape], false);

        // Name and gain
        g.setColour (Theme::text.withAlpha (juce::jmax (0.6f, alpha)));
        g.setFont (Theme::font (12.0f, true));
        juce::String label = box[IDs::name].toString();
        const double gainDb = box[IDs::gainDb];
        if (std::abs (gainDb) > 0.05) label << "  " << (gainDb > 0 ? "+" : "") << juce::String (gainDb, 1) << " dB";
        if (missing) label << "  (loading...)";
        g.drawText (label, r.toNearestInt().reduced (6, 2).withHeight (16), juce::Justification::topLeft, true);

        // Fade handles
        if (r.getWidth() > 24.0f)
        {
            g.setColour (Theme::text);
            g.fillRect (juce::Rectangle<float> (handleSize, handleSize).withCentre ({ juce::jlimit (r.getX() + 4.0f, r.getRight() - 4.0f, fiX), r.getY() + 4.0f }));
            g.fillRect (juce::Rectangle<float> (handleSize, handleSize).withCentre ({ juce::jlimit (r.getX() + 4.0f, r.getRight() - 4.0f, foX), r.getY() + 4.0f }));
        }
    }

    g.setColour (selected ? Theme::accent : base.withAlpha (alpha));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, selected ? 2.0f : 1.0f);
}

void TimelineView::paintWaveform (juce::Graphics& g, const juce::ValueTree& box, juce::Rectangle<float> r)
{
    auto sound = pool.get (box[IDs::soundId].toString());
    if (sound == nullptr || sound->peakMin.empty()) return;

    const double rate = sound->sampleRate;
    const double start = Project::boxStart (box), offset = box[IDs::offset];
    const float gain = juce::Decibels::decibelsToGain ((float) (double) box[IDs::gainDb]);
    const float mid = r.getCentreY(), half = r.getHeight() * 0.5f;
    const int x0 = juce::jmax ((int) r.getX(), headerWidth), x1 = juce::jmin ((int) r.getRight(), getWidth());
    const auto blocks = (int64_t) sound->peakMin.size();

    g.setColour (Theme::text.withAlpha (0.55f));
    for (int x = x0; x < x1; ++x)
    {
        const double t0 = xToTime ((float) x) - start + offset, t1 = xToTime ((float) x + 1.0f) - start + offset;
        auto b0 = (int64_t) (t0 * rate) / projet::SoundData::peakBlock;
        auto b1 = (int64_t) (t1 * rate) / projet::SoundData::peakBlock;
        b0 = juce::jlimit ((int64_t) 0, blocks - 1, b0);
        b1 = juce::jlimit (b0 + 1, blocks, b1 + 1);
        float lo = 0.0f, hi = 0.0f;
        for (auto b = b0; b < b1; ++b) { lo = juce::jmin (lo, sound->peakMin[(size_t) b]); hi = juce::jmax (hi, sound->peakMax[(size_t) b]); }
        lo = juce::jlimit (-1.0f, 1.0f, lo * gain);
        hi = juce::jlimit (-1.0f, 1.0f, hi * gain);
        g.fillRect ((float) x, mid - hi * half, 1.0f, juce::jmax (1.0f, (hi - lo) * half));
    }
}

//== Mouse =====================================================================

void TimelineView::mouseMove (const juce::MouseEvent& e)
{
    Zone zone;
    auto box = boxAt (e.position, zone);
    const auto frame = project.frameFor (project.getRulerFrame());
    const bool onFrameEdge = e.y >= timeRulerHeight && e.y < rulerHeight && frame.isValid() && e.x >= headerWidth
                             && (std::abs (e.position.x - timeToX (frame.start)) <= 5.0f || std::abs (e.position.x - timeToX (frame.end())) <= 5.0f);
    if (onFrameEdge)                                    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (e.y < rulerHeight && e.x >= headerWidth)   setMouseCursor (juce::MouseCursor::IBeamCursor);
    else if (zone == Zone::startEdge || zone == Zone::endEdge) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (zone == Zone::fadeInHandle || zone == Zone::fadeOutHandle) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else setMouseCursor (juce::MouseCursor::NormalCursor);
    juce::ignoreUnused (box);
}

void TimelineView::mouseDown (const juce::MouseEvent& e)
{
    dragMode = DragMode::none;
    if (auto* parent = getParentComponent()) parent->grabKeyboardFocus();   // so Space etc. reach the commands

    // Time ruler: locate
    if (e.y < timeRulerHeight)
    {
        if (e.x >= headerWidth)
        {
            dragMode = DragMode::locate;
            const double t = juce::jmax (0.0, xToTime (e.position.x));
            if (onLocate) onLocate (t);
        }
        return;
    }

    // Proportion strip: its name opens the menu; drag the frame's edges or a new span.
    if (e.y < rulerHeight)
    {
        if (e.x < headerWidth) { showStripMenu(); return; }

        const auto ref = project.getRulerFrame();
        const auto frame = project.frameFor (ref);
        const bool nearStart = frame.isValid() && std::abs (e.position.x - timeToX (frame.start)) <= 5.0f;
        const bool nearEnd = frame.isValid() && std::abs (e.position.x - timeToX (frame.end())) <= 5.0f;

        if (nearEnd && (ref == "span" || ref == "piece"))
        {
            dragMode = DragMode::frameEnd;
            project.getUndoManager().beginNewTransaction (ref == "piece" ? "Piece length" : "Span");
        }
        else if (nearStart && ref == "span")
        {
            dragMode = DragMode::frameStart;
            project.getUndoManager().beginNewTransaction ("Span");
        }
        else if ((nearStart || nearEnd) && ref != "piece")
        {
            if (onStatus) onStatus ("This frame is the box \"" + project.frameName (ref) + "\": move or trim the box to change it.", false);
        }
        else
        {
            dragMode = DragMode::span;
            spanAnchor = snap (juce::jmax (0.0, xToTime (e.position.x)), {}, ! e.mods.isShiftDown());
            project.getUndoManager().beginNewTransaction ("Span");
        }
        return;
    }

    // Header
    if (e.x < headerWidth)
    {
        if (addTrackButton().contains (e.getPosition()))
        {
            project.getUndoManager().beginNewTransaction ("Add track");
            auto t = project.addTrack();
            selectedTrackId = t[IDs::id].toString();
            repaint();
            return;
        }

        auto row = rowAt (e.y);
        if (! row.track.isValid()) return;
        selectedTrackId = row.track[IDs::id].toString();

        if (e.mods.isPopupMenu()) { showTrackMenu (row.track); repaint(); return; }

        if (row.folder && foldButton (row).expanded (4).contains (e.getPosition()))
        {
            row.track.setProperty (IDs::folded, ! (bool) row.track[IDs::folded], nullptr);
            repaint();
            return;
        }

        if (! muteButton (row).contains (e.getPosition()) && ! soloButton (row).contains (e.getPosition()))
        {
            // Maybe the start of dragging the track somewhere else.
            dragMode = DragMode::track;
            draggedTrack = row.track;
            trackDrop = {};
            repaint();
            return;
        }

        if (muteButton (row).contains (e.getPosition()))
        {
            project.getUndoManager().beginNewTransaction ("Mute");
            row.track.setProperty (IDs::mute, ! (bool) row.track[IDs::mute], project.um());
        }
        else if (soloButton (row).contains (e.getPosition()))
        {
            project.getUndoManager().beginNewTransaction ("Solo");
            row.track.setProperty (IDs::solo, ! (bool) row.track[IDs::solo], project.um());
        }
        repaint();
        return;
    }

    // Lanes
    Zone zone;
    auto box = boxAt (e.position, zone);
    auto row = rowAt (e.y);
    if (row.track.isValid()) selectedTrackId = row.track[IDs::id].toString();

    const bool adding = e.mods.isShiftDown() || e.mods.isCommandDown();

    if (! box.isValid())
    {
        // Empty lane: a selection rectangle (Shift / Cmd keep the current selection).
        if (! adding) selectBox ({});
        if (e.mods.isPopupMenu()) return;
        dragMode = DragMode::lasso;
        lassoBase = adding ? selectedBoxIds : juce::StringArray();
        lasso = { e.getPosition(), e.getPosition() };
        repaint();
        return;
    }

    clickedSelectedBox = false;
    if (e.mods.isCommandDown() && ! e.mods.isPopupMenu())
    {
        // Cmd-click toggles the box in the selection.
        const auto id = box[IDs::id].toString();
        if (isSelected (box)) selectedBoxIds.removeString (id); else selectedBoxIds.add (id);
        selectionChanged();
        if (! isSelected (box)) return;
    }
    else if (e.mods.isShiftDown())
    {
        if (! isSelected (box)) { selectedBoxIds.add (box[IDs::id].toString()); selectionChanged(); }
    }
    else if (isSelected (box) && selectedBoxIds.size() > 1)
        clickedSelectedBox = true;               // keep the group for a drag; mouse-up selects only this one
    else
        selectBox (box);

    if (e.mods.isPopupMenu()) { showBoxMenu (box); return; }

    // Everything selected moves together. Track positions count the visible
    // tracks that hold boxes (folders are skipped).
    moving.clear();
    std::vector<juce::ValueTree> lanes;
    for (auto& r : rows()) if (! r.folder) lanes.push_back (r.track);
    for (auto b : getSelectedBoxes())
    {
        const auto it = std::find (lanes.begin(), lanes.end(), b.getParent());
        moving.push_back ({ b, Project::boxStart (b), it == lanes.end() ? -1 : (int) (it - lanes.begin()) });
    }

    dragBox = box;
    dragStartTime = xToTime (e.position.x);
    dragOrigStart = Project::boxStart (box);
    dragOrigEnd = Project::boxEnd (box);
    dragOrigFadeIn = box[IDs::fadeIn];
    dragOrigFadeOut = box[IDs::fadeOut];
    dragGrabOffset = e.position.x - timeToX (dragOrigStart);

    switch (zone)
    {
        case Zone::startEdge:     dragMode = DragMode::trimStart; project.getUndoManager().beginNewTransaction ("Trim box"); break;
        case Zone::endEdge:       dragMode = DragMode::trimEnd;   project.getUndoManager().beginNewTransaction ("Trim box"); break;
        case Zone::fadeInHandle:  dragMode = DragMode::fadeIn;    project.getUndoManager().beginNewTransaction ("Fade in"); break;
        case Zone::fadeOutHandle: dragMode = DragMode::fadeOut;   project.getUndoManager().beginNewTransaction ("Fade out"); break;
        case Zone::body:          dragMode = DragMode::move;      project.getUndoManager().beginNewTransaction ("Move box"); break;
        case Zone::none:          break;
    }
}

void TimelineView::mouseDrag (const juce::MouseEvent& e)
{
    const bool snapping = ! e.mods.isShiftDown();
    const double t = xToTime (e.position.x);

    switch (dragMode)
    {
        case DragMode::locate:
            if (onLocate) onLocate (juce::jmax (0.0, t));
            break;

        case DragMode::move:
        {
            if (e.getDistanceFromDragStart() < 3) break;
            clickedSelectedBox = false;

            // Snap the grabbed box, then shift every selected box by the same amount.
            double start = xToTime (e.position.x - dragGrabOffset);
            const double len = dragOrigEnd - dragOrigStart;
            const double snappedStart = snap (start, dragBox, snapping);
            const double snappedEnd = snap (start + len, dragBox, snapping);
            if (std::abs (snappedStart - start) <= std::abs (snappedEnd - (start + len))) start = snappedStart;
            else start = snappedEnd - len;

            double delta = start - dragOrigStart;
            for (auto& m : moving) delta = juce::jmax (delta, -m.start);   // nothing before 0:00

            std::vector<juce::ValueTree> lanes;
            for (auto& r : rows()) if (! r.folder) lanes.push_back (r.track);
            auto row = rowAt (juce::jlimit (rulerHeight, getHeight() - 1, e.y));
            if (row.track.isValid() && ! row.folder)
            {
                int grabbedTrack = -1;
                for (auto& m : moving) if (m.box == dragBox) grabbedTrack = m.track;
                const auto it = std::find (lanes.begin(), lanes.end(), row.track);
                int trackDelta = (grabbedTrack < 0 || it == lanes.end()) ? 0 : (int) (it - lanes.begin()) - grabbedTrack;
                for (auto& m : moving)
                {
                    if (m.track < 0) { trackDelta = 0; break; }      // a box on a folded track stays where it is
                    trackDelta = juce::jlimit (-m.track, (int) lanes.size() - 1 - m.track, trackDelta);
                }
                lastTrackDelta = trackDelta;
            }

            for (auto& m : moving)
            {
                auto to = m.track >= 0 ? lanes[(size_t) (m.track + lastTrackDelta)] : m.box.getParent();
                project.moveBox (m.box, to, m.start + delta);
            }
            selectedTrackId = dragBox.getParent()[IDs::id].toString();
            readout = readoutFor (dragBox, Project::boxStart (dragBox), "start");
            readoutPos = e.getPosition();
            repaint();
            break;
        }

        case DragMode::lasso:
        {
            lasso = juce::Rectangle<int> (e.getMouseDownPosition(), e.getPosition());
            auto ids = lassoBase;
            for (auto& r : rows())
                for (auto b : r.track)
                    if (boxBounds (b, r).toNearestInt().intersects (lasso))
                        ids.addIfNotAlreadyThere (b[IDs::id].toString());
            if (ids != selectedBoxIds) { selectedBoxIds = ids; selectionChanged(); }
            repaint();
            break;
        }

        case DragMode::trimStart:
            project.trimStart (dragBox, snap (t, dragBox, snapping));
            readout = readoutFor (dragBox, Project::boxStart (dragBox), "start");
            readoutPos = e.getPosition();
            repaint();
            break;

        case DragMode::trimEnd:
        {
            project.trimEnd (dragBox, snap (t, dragBox, snapping));
            readout = readoutFor (dragBox, Project::boxEnd (dragBox), "end");
            if (project.isBound (dragBox))
                readout << "   length " << Proportion::describe (project.actualProportion (dragBox, false), project.getDenominators());
            readoutPos = e.getPosition();
            repaint();
            break;
        }

        case DragMode::span:
        {
            if (e.getDistanceFromDragStart() < 3) break;
            const double b = snap (juce::jmax (0.0, t), {}, snapping);
            project.setSpan (spanAnchor, b);
            if (project.hasSpan()) project.setRulerFrame ("span");
            repaint();
            break;
        }

        case DragMode::frameStart:
        case DragMode::frameEnd:
        {
            const auto ref = project.getRulerFrame();
            const double v = snap (juce::jmax (0.0, t), {}, snapping);
            if (ref == "piece") project.setPieceLength (juce::jmax (1.0, v));
            else if (ref == "span")
            {
                const auto f = project.frameFor ("span");
                if (dragMode == DragMode::frameStart) project.setSpan (juce::jmin (v, f.end() - 0.01), f.end());
                else project.setSpan (f.start, juce::jmax (v, f.start + 0.01));
            }
            repaint();
            break;
        }

        case DragMode::track:
            if (e.getDistanceFromDragStart() < 4) break;
            trackDrop = trackDropAt (e.y);
            repaint();
            break;

        case DragMode::fadeIn:
            project.setFades (dragBox, juce::jmax (0.0, t - Project::boxStart (dragBox)), dragBox[IDs::fadeOut]);
            break;

        case DragMode::fadeOut:
            project.setFades (dragBox, dragBox[IDs::fadeIn], juce::jmax (0.0, Project::boxEnd (dragBox) - t));
            break;

        case DragMode::none:
            break;
    }
}

void TimelineView::mouseUp (const juce::MouseEvent& e)
{
    if (clickedSelectedBox && ! e.mouseWasDraggedSinceMouseDown() && dragBox.isValid())
        selectBox (dragBox);
    clickedSelectedBox = false;
    if (dragMode == DragMode::lasso) { lasso = {}; repaint(); }

    if (dragMode == DragMode::track && draggedTrack.isValid() && e.mouseWasDraggedSinceMouseDown())
    {
        const auto drop = trackDropAt (e.y);
        if (drop.valid)
        {
            project.getUndoManager().beginNewTransaction (drop.into.isValid() ? "Move into folder" : "Move track");
            project.moveTrack (draggedTrack, drop.index, drop.folderId);
            auto into = drop.into;
            if (into.isValid()) into.setProperty (IDs::folded, false, nullptr);
        }
    }
    draggedTrack = {};
    trackDrop = {};
    readout.clear();
    lastTrackDelta = 0;

    dragMode = DragMode::none;
    dragBox = {};
    moving.clear();
    repaint();
}

void TimelineView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.y < rulerHeight) return;

    if (e.x < headerWidth)
    {
        auto row = rowAt (e.y);
        if (row.track.isValid() && ! muteButton (row).contains (e.getPosition()) && ! soloButton (row).contains (e.getPosition())
            && ! (row.folder && foldButton (row).expanded (4).contains (e.getPosition())))
            renameTrack (row.track);
        return;
    }

    Zone zone;
    auto box = boxAt (e.position, zone);
    if (box.isValid())
    {
        if (onEditNotes) onEditNotes (box);
        return;
    }

    auto row = rowAt (e.y);
    if (row.track.isValid() && ! row.folder && onAddTextBox)
        onAddTextBox (row.track, juce::jmax (0.0, snap (xToTime (e.position.x), {}, ! e.mods.isShiftDown())));
}

void TimelineView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        zoomBy (std::pow (1.0025, (double) w.deltaY * 400.0), xToTime (e.position.x));
        return;
    }

    float dx = w.deltaX, dy = w.deltaY;
    if (e.mods.isShiftDown() && std::abs (dx) < 1.0e-6f) { dx = dy; dy = 0.0f; }

    if (std::abs (dx) > 1.0e-6f)
        viewStart = juce::jmax (0.0, viewStart - (double) dx * 300.0 / pps);

    if (std::abs (dy) > 1.0e-6f)
    {
        const int maxScroll = juce::jmax (0, contentHeight() - (getHeight() - rulerHeight));
        scrollY = juce::jlimit (0, maxScroll, scrollY - juce::roundToInt (dy * 200.0f));
    }
    repaint();
}

void TimelineView::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomBy ((double) scaleFactor, xToTime (e.position.x));
}

//== Menus =====================================================================

void TimelineView::renameTrack (juce::ValueTree track)
{
    auto* w = new juce::AlertWindow ("Rename track", {}, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", track[IDs::name].toString());
    w->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TimelineView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, track] (int result) mutable
    {
        if (safe == nullptr || result != 1) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        safe->project.getUndoManager().beginNewTransaction ("Rename track");
        track.setProperty (IDs::name, name, safe->project.um());
    }), true);
}

void TimelineView::showTrackMenu (juce::ValueTree track)
{
    const bool folder = Project::isFolder (track);
    const bool inFolder = project.folderOf (track).isValid();

    juce::PopupMenu colours;
    for (int i = 0; i < Theme::numTrackColours; ++i)
        colours.addItem (100 + i, Theme::trackColourName (i), true, (int) track[IDs::colour] == i);

    juce::PopupMenu m;
    m.addItem (1, "Rename...");
    if (inFolder) m.addItem (99, "Colour: set by the folder", false);
    else m.addSubMenu ("Colour", colours);
    m.addSeparator();

    if (folder)
    {
        m.addItem (10, (bool) track[IDs::folded] ? "Unfold" : "Fold");
        m.addItem (11, "Add track inside");
        m.addItem (12, "Ungroup (keep the tracks)");
        m.addSeparator();
        m.addItem (4, "Delete folder and its tracks", project.laneTracks().size() > 1 || project.blockEnd (track) - project.getTracks().indexOf (track) <= 1);
    }
    else
    {
        m.addItem (2, "Add track above");
        m.addItem (3, "Add track below");
        m.addSeparator();
        m.addItem (13, "Put in a new folder");
        if (inFolder) m.addItem (14, "Move out of the folder");
        m.addSeparator();
        m.addItem (4, "Delete track" + juce::String (track.getNumChildren() > 0 ? " and its boxes" : ""), project.laneTracks().size() > 1);
    }

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, track] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->project;
        const int index = p.getTracks().indexOf (track);
        const auto folderId = track[IDs::folderId].toString();
        if (r == 1) safe->renameTrack (track);
        else if (r == 2) { p.getUndoManager().beginNewTransaction ("Add track"); p.addTrack ({}, index, folderId); }
        else if (r == 3) { p.getUndoManager().beginNewTransaction ("Add track"); p.addTrack ({}, index + 1, folderId); }
        else if (r == 4) { p.getUndoManager().beginNewTransaction ("Delete track"); p.removeTrack (track); }
        else if (r == 10) track.setProperty (IDs::folded, ! (bool) track[IDs::folded], nullptr);
        else if (r == 11)
        {
            p.getUndoManager().beginNewTransaction ("Add track");
            track.setProperty (IDs::folded, false, nullptr);
            p.addTrack ({}, p.blockEnd (track), track[IDs::id].toString());
        }
        else if (r == 12) { p.getUndoManager().beginNewTransaction ("Ungroup folder"); p.ungroupFolder (track); }
        else if (r == 13)
        {
            p.getUndoManager().beginNewTransaction ("Put in a new folder");
            auto f = p.putInNewFolder (track);
            safe->selectedTrackId = f[IDs::id].toString();
            safe->renameTrack (f);
        }
        else if (r == 14)
        {
            // Just after the folder it was in, one level up.
            auto f = p.folderOf (track);
            p.getUndoManager().beginNewTransaction ("Move out of folder");
            p.moveTrack (track, p.blockEnd (f), f[IDs::folderId].toString());
        }
        else if (r >= 100) { p.getUndoManager().beginNewTransaction ("Track colour"); track.setProperty (IDs::colour, r - 100, p.um()); }
        safe->repaint();
    });
}

void TimelineView::showStripMenu()
{
    const auto ref = project.getRulerFrame();
    const auto dens = project.getDenominators();
    auto selected = getSelectedBox();

    juce::PopupMenu frame;
    frame.addItem (1, "The piece (" + TimeFormat::format (project.getPieceLength(), 0) + (project.hasSetPieceLength() ? ")" : ", its current length)"), true, ref == "piece");
    frame.addItem (2, "The span dragged on this strip", project.hasSpan(), ref == "span");
    if (selected.isValid())
        frame.addItem (3, "The selected box: " + selected[IDs::name].toString(), true, ref == selected[IDs::id].toString());
    else if (ref != "piece" && ref != "span")
        frame.addItem (3, "Box: " + project.frameName (ref), true, true);
    else
        frame.addItem (3, "A box (select one first)", false);

    juce::PopupMenu divisions;
    for (int d = 2; d <= 20; ++d)
        divisions.addItem (200 + d, "1/" + juce::String (d), true, Proportion::indexOf (dens, (double) d) >= 0);
    divisions.addSeparator();
    bool anyCustom = false;
    for (auto d : dens)
        if (std::abs (d - std::round (d)) > 1.0e-9 || d > 20.0)
        {
            if (! anyCustom) divisions.addSectionHeader ("Your own (click to remove)");
            anyCustom = true;
            divisions.addItem (400 + Proportion::indexOf (dens, d), "1/" + Proportion::formatNumber (d), true, true);
        }
    divisions.addItem (20, "Add your own denominator (for example 7.5)...");
    divisions.addSeparator();
    divisions.addItem (21, "Halves to sixths, and eighths");
    divisions.addItem (22, "Up to twelfths");
    divisions.addItem (23, "Every denominator up to 20");
    divisions.addItem (24, "Prime denominators up to 19");
    divisions.addItem (25, "None");

    juce::PopupMenu m;
    m.addSectionHeader ("The proportion ruler divides");
    m.addSubMenu ("Frame: " + project.frameName (ref), frame);
    m.addItem (5, "Set the piece length...");
    m.addItem (6, "Follow the piece's current length", project.hasSetPieceLength());
    m.addItem (7, "Remove the span", project.hasSpan());
    m.addSeparator();
    m.addSubMenu ("Divisions", divisions);
    m.addItem (8, "Finer divisions appear as you zoom in", true, project.getAutoDivisions());

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, selected, dens] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->project;
        auto list = dens;
        auto setList = [&] (juce::Array<double> l) { std::sort (l.begin(), l.end()); p.setDenominators (l); };

        if (r == 1) p.setRulerFrame ("piece");
        else if (r == 2) p.setRulerFrame ("span");
        else if (r == 3 && selected.isValid()) p.setRulerFrame (selected[IDs::id].toString());
        else if (r == 5) safe->askPieceLength();
        else if (r == 6) { p.getUndoManager().beginNewTransaction ("Piece length"); p.setPieceLength (0.0); }
        else if (r == 7)
        {
            p.getUndoManager().beginNewTransaction ("Remove span");
            p.setSpan (0.0, 0.0);
            if (p.getRulerFrame() == "span") p.setRulerFrame ("piece");
        }
        else if (r == 8) p.setAutoDivisions (! p.getAutoDivisions());
        else if (r == 20) safe->askCustomDenominator();
        else if (r == 21) setList ({ 2, 3, 4, 5, 6, 8 });
        else if (r == 22) setList ({ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 });
        else if (r == 23) { juce::Array<double> l; for (int d = 2; d <= 20; ++d) l.add (d); setList (l); }
        else if (r == 24) setList ({ 2, 3, 5, 7, 11, 13, 17, 19 });
        else if (r == 25) setList ({});
        else if (r >= 200 && r <= 220)
        {
            const double d = r - 200;
            const int i = Proportion::indexOf (list, d);
            if (i >= 0) list.remove (i); else list.add (d);
            setList (list);
        }
        else if (r >= 400) { list.remove (r - 400); setList (list); }
        safe->repaint();
    });
}

void TimelineView::askPieceLength()
{
    auto* w = new juce::AlertWindow ("Piece length",
                                     "The piece as a proportion frame: how long it is meant to be (m:ss, or seconds).",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("length", TimeFormat::format (project.getPieceLength(), 1));
    w->addButton ("Set", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TimelineView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        const double v = TimeFormat::parse (w->getTextEditorContents ("length"));
        if (v < 1.0)
        {
            if (safe->onStatus) safe->onStatus ("Not a length: write it as 12:00, 1:05.5 or 90.", true);
            return;
        }
        safe->project.getUndoManager().beginNewTransaction ("Piece length");
        safe->project.setPieceLength (v);
        safe->repaint();
    }), true);
}

void TimelineView::askCustomDenominator()
{
    auto* w = new juce::AlertWindow ("Your own denominator",
                                     "Divides the frame into this many parts. A decimal such as 7.5 gives 7 full parts and a half part at the end.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("d", "7.5");
    w->addButton ("Add", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TimelineView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        const auto r = Proportion::parse (w->getTextEditorContents ("d"));
        if (! r.ok || r.value <= 1.0 || r.value > 1000.0)
        {
            if (safe->onStatus) safe->onStatus ("A denominator is a number above 1, for example 7.5.", true);
            return;
        }
        auto list = safe->project.getDenominators();
        if (Proportion::indexOf (list, r.value) < 0) list.add (r.value);
        std::sort (list.begin(), list.end());
        safe->project.setDenominators (list);
        safe->repaint();
    }), true);
}

void TimelineView::showBoxMenu (juce::ValueTree box)
{
    const bool sound = Project::isSound (box);
    const bool several = selectedBoxIds.size() > 1;
    juce::PopupMenu m;
    m.addItem (5, "Write notes");
    m.addItem (6, "Notes in a large window");
    m.addSeparator();
    m.addItem (7, "Split at the playhead", Project::boxStart (box) < playhead && playhead < Project::boxEnd (box));
    m.addItem (1, several ? "Duplicate boxes" : "Duplicate");
    m.addItem (2, several ? "Delete boxes" : "Delete");
    m.addSeparator();
    m.addItem (4, sound ? "Change to a text box" : "Change to a sound box", ! several);
    if (sound) m.addItem (3, "Show original sound in Finder");
    m.addSeparator();
    const auto rulerFrame = project.getRulerFrame();
    const bool isFrame = rulerFrame == box[IDs::id].toString();
    m.addItem (8, isFrame ? "Stop using as the ruler's frame" : "Use as the ruler's frame");
    if (! isFrame && project.canUseAsFrame (box, rulerFrame))
        m.addItem (9, (several ? "Give the boxes proportions of " : "Give it proportions of ") + project.frameName (rulerFrame));
    if (project.isBound (box)) m.addItem (10, several ? "Remove the proportions" : "Remove its proportions");

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, box, sound] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->project;
        auto selection = safe->getSelectedBoxes();
        if (selection.isEmpty()) selection.add (box);

        if (r == 1)
        {
            p.getUndoManager().beginNewTransaction ("Duplicate");
            juce::Array<juce::ValueTree> copies;
            for (auto b : selection) copies.add (p.duplicateBox (b));
            safe->setSelection (copies);
        }
        else if (r == 2)
        {
            p.getUndoManager().beginNewTransaction ("Delete");
            for (auto b : selection) p.removeBox (b);
            safe->selectBox ({});
        }
        else if (r == 3 && safe->onRevealOriginal) safe->onRevealOriginal (box);
        else if (r == 4)
        {
            p.getUndoManager().beginNewTransaction ("Change kind");
            p.setKind (box, sound ? "text" : "sound");
        }
        else if (r == 5 && safe->onEditNotes) safe->onEditNotes (box);
        else if (r == 6 && safe->onOpenNotesWindow) safe->onOpenNotesWindow (box);
        else if (r == 7 && safe->onSplitAtPlayhead) safe->onSplitAtPlayhead (box);
        else if (r == 8) p.setRulerFrame (p.getRulerFrame() == box[IDs::id].toString() ? juce::String ("piece") : box[IDs::id].toString());
        else if (r == 9)
        {
            const auto ref = p.getRulerFrame();
            p.getUndoManager().beginNewTransaction ("Give proportions");
            for (auto b : selection)
                if (p.canUseAsFrame (b, ref)) p.bindBox (b, ref);
        }
        else if (r == 10)
        {
            p.getUndoManager().beginNewTransaction ("Remove proportions");
            for (auto b : selection) p.bindBox (b, {});
        }
        safe->repaint();
    });
}

//== Files =====================================================================

bool TimelineView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f)) return true;
    return false;
}

void TimelineView::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropHighlight = false;
    repaint();

    juce::StringArray audio;
    for (auto& f : files) if (isAudioFile (f)) audio.add (f);
    if (audio.isEmpty() || ! onFilesDropped) return;

    auto row = rowAt (y);
    juce::ValueTree track = row.track;
    if (track.isValid() && row.folder)
    {
        // On a folder: a new track at the end of it.
        project.getUndoManager().beginNewTransaction ("Add track");
        track = project.addTrack ({}, project.blockEnd (track), track[IDs::id].toString());
    }
    else if (! track.isValid())
    {
        // Below the last track: a new track.
        project.getUndoManager().beginNewTransaction ("Add track");
        track = project.addTrack();
    }
    onFilesDropped (audio, track, juce::jmax (0.0, x >= headerWidth ? xToTime ((float) x) : playhead));
}
