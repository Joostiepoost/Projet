// TimelineView.cpp

#include "TimelineView.h"
#include "Dialogs.h"
#include "Theme.h"
#include "TimeFormat.h"
#include "../Engine/Arrangement.h"
#include "Markdown.h"
#include <limits>

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

TimelineView::TimelineView (Project& p, projet::SoundPool& sp, const Framework& f)
    : project (p), pool (sp), framework (f)
{
    setWantsKeyboardFocus (false);
    glyphs.rebuild (framework);
    vScroll.setAutoHide (true);
    vScroll.addListener (this);
    vScroll.setColour (juce::ScrollBar::thumbColourId, Theme::textMuted.withAlpha (0.6f));
    addChildComponent (vScroll);
}

void TimelineView::frameworkReloaded()
{
    glyphs.rebuild (framework);
    glyphCache.clear();
    displayCache = "\x01";      // reparse the display settings
    repaint();
}

void TimelineView::resized() { updateScrollBar(); }

int TimelineView::maxScroll() const
{
    return rulerPinned() ? juce::jmax (0, contentHeight() - (getHeight() - rulerHeight()))
                         : juce::jmax (0, rulerHeight() + contentHeight() - getHeight());
}

void TimelineView::updateScrollBar()
{
    // After every paint (asynchronously): the scroll bar follows the content.
    const int clamped = juce::jlimit (0, maxScroll(), scrollY);
    if (clamped != scrollY) { scrollY = clamped; repaint(); }
    const bool pinned = rulerPinned();
    const int top = pinned ? rulerHeight() : 0;
    const int total = pinned ? contentHeight() : rulerHeight() + contentHeight();
    const int visible = juce::jmax (1, getHeight() - top);
    const auto bounds = juce::Rectangle<int> (getWidth() - 11, top, 10, juce::jmax (0, getHeight() - top));
    if (vScroll.getBounds() != bounds) vScroll.setBounds (bounds);
    vScroll.setRangeLimits (0.0, (double) juce::jmax (total, visible), juce::dontSendNotification);
    vScroll.setCurrentRange ((double) scrollY, (double) visible, juce::dontSendNotification);
    vScroll.setVisible (total > visible);
}

void TimelineView::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    const int s = juce::jlimit (0, maxScroll(), juce::roundToInt (newRangeStart));
    if (s == scrollY) return;
    scrollY = s;
    repaint();
}

//== Geometry ==================================================================

std::vector<TimelineView::TrackRow> TimelineView::rows() const
{
    std::vector<TrackRow> r;
    int y = rulerHeight() - scrollY;
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
    const int y = rs.empty() ? rulerHeight() - scrollY : rs.back().y + rs.back().h;
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

    snappedMark.clear();
    auto consider = [&] (double c) { const double d = std::abs (c - t); if (d < bestD) { bestD = d; best = c; snappedMark.clear(); } };
    consider (playhead);
    consider (0.0);
    for (auto tr : project.getState().getChildWithName (IDs::TRACKS))
        for (auto b : tr)
            if (b != exclude) { consider (Project::boxStart (b)); consider (Project::boxEnd (b)); }

    // Every visible proportion layer that snaps: its frame's edges, the
    // divisions shown at this zoom, and its series.
    for (auto& row : rulerRows())
    {
        if (row.time || row.lane || ! (bool) row.layer.getProperty (IDs::snap, true)) continue;
        const auto f = project.frameFor (project.layerFrame (row.layer));
        if (! f.isValid()) continue;
        consider (f.start);
        consider (f.end());
        for (auto& k : visibleTicks (row.layer)) consider (k.time);
    }

    // Marks pull harder than anything else (they count as half as far away).
    for (auto m : project.getMarks())
    {
        if (m == dragMark) continue;
        const double mt = project.markTime (m);
        if (mt < 0.0) continue;
        const double d = std::abs (mt - t) * 0.5;
        if (d <= bestD && d < range) { bestD = d; best = mt; snappedMark = m[IDs::name].toString(); }
    }
    return best;
}

//== Proportions ===============================================================

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
        if (selectedItemId.isNotEmpty() || selectedNodeId.isNotEmpty())   // one kind of selection at a time
        {
            selectedItemId = selectedNodeId = {};
            if (ids == selectedBoxIds) { selectionChanged(); return; }
        }
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
    triggerAsyncUpdate();               // the scroll bar follows (after this paint)
    g.fillAll (Theme::background);
    auto rs = rows();
    signHits.clear();

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
        g.reduceClipRegion (headerWidth, tracksTop(), getWidth() - headerWidth, getHeight() - tracksTop());

        // The proportion grid of the active layer: its frame's edges and the labelled divisions.
        const auto activeLayer = project.getActiveLayer();
        const auto frame = activeLayer.isValid() && (bool) activeLayer.getProperty (IDs::visible, true)
                               ? project.frameFor (project.layerFrame (activeLayer)) : Project::Frame();
        if (frame.isValid())
        {
            const float top = (float) tracksTop(), bottom = (float) getHeight();
            const float dashes[] = { 3.0f, 4.0f };
            g.setColour (Theme::accent.withAlpha (0.10f));
            for (auto& k : visibleTicks (activeLayer))
                if (k.labelled || k.series)
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

        paintMarksInLanes (g);

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

            // Values set on the folder only (writing type, time-feel: 4d), at the left of its lane.
            juce::StringArray labels;
            for (auto& c : framework.getCategories())
                if (c.foldersOnly && markFor (c) == Framework::Mark::label && r.track[c.property()].toString().isNotEmpty())
                    labels.add (framework.valueName (c, r.track[c.property()].toString()));
            if (! labels.isEmpty())
            {
                const auto text = labels.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 "));
                const auto font = Theme::font (11.0f);
                const int w = juce::GlyphArrangement::getStringWidthInt (font, text) + 12;
                const auto pill = juce::Rectangle<float> ((float) headerWidth + 6.0f, (float) r.y + (float) r.h * 0.5f - 8.0f, (float) w, 16.0f);
                g.setColour (Theme::background.withAlpha (0.88f));
                g.fillRoundedRectangle (pill, 8.0f);
                g.setColour (Theme::textMuted.brighter (0.2f));
                g.setFont (font);
                g.drawText (text, pill, juce::Justification::centred, true);
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
                bool isFrame = false;
                for (auto& lr : rulerRows()) if (! lr.time && ! lr.lane && project.layerFrame (lr.layer) == box[IDs::id].toString()) isFrame = true;
                if (isFrame)
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

        // Seam and end signs, over the boxes (stage 4c).
        for (auto& r : rs)
            if (! r.folder) paintSeams (g, r);

        if (project.getContentEnd() <= 0.0)
        {
            g.setColour (Theme::textMuted);
            g.setFont (Theme::font (15.0f));
            g.drawText ("Drop sound files here, or choose File > Import sounds (Cmd-I)",
                        juce::Rectangle<int> (headerWidth, tracksTop(), getWidth() - headerWidth, juce::jmin (200, getHeight() - tracksTop())),
                        juce::Justification::centred);
        }
    }

    // Headers
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (0, tracksTop(), headerWidth, getHeight() - tracksTop());
        g.setColour (Theme::header);
        g.fillRect (0, tracksTop(), headerWidth, getHeight() - tracksTop());
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
    g.fillRect (headerWidth - 1, tracksTop(), 1, getHeight() - tracksTop());

    paintRuler (g);
    paintNodes (g);                     // across folders' tracks and the lanes (4d.2)

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

    // A move dropped below the last track: the new track(s) it would make
    if (dragMode == DragMode::move && newTracksBelow > 0 && ! rs.empty())
    {
        constexpr int previewHeight = 56;
        const int y0 = rs.back().y + rs.back().h;
        const auto area = juce::Rectangle<int> (0, y0, getWidth(), previewHeight * newTracksBelow);
        g.setColour (Theme::header);
        g.fillRect (area.withWidth (headerWidth));
        g.setColour (Theme::accent.withAlpha (0.08f));
        g.fillRect (area);
        g.setColour (Theme::accent.withAlpha (0.6f));
        g.drawRect (area, 1);
        g.setFont (Theme::font (12.5f));
        g.drawText (newTracksBelow == 1 ? "New track" : juce::String (newTracksBelow) + " new tracks",
                    area.withWidth (headerWidth).reduced (12, 0).withHeight (previewHeight), juce::Justification::centredLeft);
        int lanesCount = 0;
        for (auto& r : rs) if (! r.folder) ++lanesCount;
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area.withTrimmedLeft (headerWidth));
        for (auto& m : moving)
        {
            const int k = m.track + newTrackDelta - lanesCount;
            if (m.track < 0 || k < 0) continue;
            const float x0 = timeToX (Project::boxStart (m.box)), x1 = timeToX (Project::boxEnd (m.box));
            const auto r = juce::Rectangle<float> (x0, (float) (y0 + k * previewHeight + 4), juce::jmax (2.0f, x1 - x0), (float) previewHeight - 8.0f);
            juce::Path outline, dashed;
            outline.addRoundedRectangle (r, 4.0f);
            const float dashes[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (Theme::accent.withAlpha (0.9f));
            g.fillPath (dashed);
        }
    }

    // The proportion of the edge being moved
    if (readout.isNotEmpty())
    {
        g.setFont (Theme::mono (11.5f));
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), readout) + 14;
        auto box = juce::Rectangle<int> (readoutPos.x + 14, readoutPos.y - 30, w, 20)
                       .constrainedWithin (getLocalBounds().withTrimmedTop (tracksTop()).reduced (4));
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
        for (auto& lr : rulerRows())
            if (lr.time)
            {
                const float bottom = (float) (lr.y + lr.h);
                juce::Path tri;
                tri.addTriangle (px - 5.0f, bottom - 9.0f, px + 5.0f, bottom - 9.0f, px, bottom - 1.0f);
                g.fillPath (tri);
            }
    }

    if (dropHighlight)
    {
        g.setColour (Theme::accent.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 2);
    }
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
        // The values this folder gives its boxes, as pills before M and S.
        juce::StringArray pillTexts;
        juce::Array<juce::uint32> pillColours;
        for (auto& c : framework.getCategories())
            if (c.folders && ! c.foldersOnly && track[c.property()].toString().isNotEmpty())
            {
                const auto id = track[c.property()].toString();
                pillTexts.add (c.kind == Framework::Kind::number ? framework.formatNumber (c, id.getDoubleValue()) : framework.valueLabel (c, id));
                pillColours.add (framework.valueColours (c, id).colour);
            }
        int pillsW = 0;
        juce::Array<int> widths;
        for (auto& t : pillTexts)
        {
            const int w = juce::jmin (58, juce::GlyphArrangement::getStringWidthInt (Theme::font (10.5f), t) + 10);
            if (pillsW + w > 96) break;
            widths.add (w);
            pillsW += w;
        }
        g.drawText (Theme::capitals (track[IDs::name].toString()), x, r.y, headerWidth - 56 - x - pillsW, r.h, juce::Justification::centredLeft, true);
        int px = headerWidth - 52 - pillsW;
        for (int i = 0; i < widths.size(); ++i)
        {
            const auto c = pillColours[i];
            juce::Rectangle<float> pill ((float) px, (float) r.y + (float) (r.h - 15) / 2.0f, (float) widths[i] - 4.0f, 15.0f);
            g.setColour ((c != 0 ? juce::Colour (c) : Theme::lineStrong).withAlpha (0.3f));
            g.fillRoundedRectangle (pill, 7.0f);
            g.setColour (c != 0 ? juce::Colour (c).brighter (0.5f) : Theme::text);
            g.setFont (Theme::font (10.5f));
            g.drawText (pillTexts[i], pill, juce::Justification::centred, true);
            px += widths[i];
        }
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
    if (highlight.isActive() && ! matchesHighlight (box)) alpha *= 0.22f;
    const auto wm = weightFor (box);                           // background roles recede (4b.2)
    alpha *= wm.alphaFactor;
    const auto base = boxColour (box, row);
    const bool colourMode = project.getState().getProperty (IDs::colourMode, "own").toString() != "own";
    const bool missing = sound && pool.get (box[IDs::soundId].toString()) == nullptr;

    if (! sound)
    {
        // Text box: warm paper, dashed edge, the name and the first lines of the notes.
        g.setColour (juce::Colour (0xff2f2b25).withAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
        paintFrameworkFill (g, box, r, alpha);
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
        if (colourMode)
        {
            juce::Graphics::ScopedSaveState s (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 4.0f);
            g.reduceClipRegion (clip);
            g.setColour (base.withAlpha (alpha));
            g.fillRect (r.withWidth (5.0f));
        }
        paintFrameworkMarks (g, box, r, alpha);
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
            juce::PathStrokeType (wm.emphasis ? 1.6f : 1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour ((colourMode ? base : juce::Colour (0xff9a8f78)).withAlpha (alpha));
            g.fillPath (dashed);
        }
        return;
    }

    g.setColour (base.darker (1.1f).withAlpha (alpha));
    g.fillRoundedRectangle (r, 4.0f);
    paintFrameworkFill (g, box, r, alpha);

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

    paintFrameworkMarks (g, box, r, alpha);
    g.setColour (selected ? Theme::accent : (wm.emphasis ? base.brighter (0.25f) : base).withAlpha (alpha));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, selected ? 2.0f : (wm.emphasis ? 1.8f : 1.0f));
}

//== Framework on the boxes (stage 4a, 4b) =====================================

namespace
{
    juce::Colour fromModel (juce::uint32 c, juce::Colour fallback)   { return c != 0 ? juce::Colour (c) : fallback; }
}

Framework::Mark TimelineView::markFor (const Framework::Category& c) const
{
    auto& self = const_cast<TimelineView&> (*this);
    const auto setting = project.getState()[IDs::fwDisplay].toString();
    if (setting != displayCache)
    {
        self.displayCache = setting;
        self.displayMarks = Framework::parsePairs (setting);
    }
    return Framework::chosenMark (c, displayMarks);
}

juce::Colour TimelineView::boxColour (const juce::ValueTree& box, const TrackRow& row)
{
    const auto mode = project.getState().getProperty (IDs::colourMode, "own").toString();
    if (mode == "own") return Theme::trackColour (project.effectiveColour (row.track));
    const juce::Colour none (0xff4a4c52);   // not set: neutral grey
    auto* c = framework.findCategory (mode);
    if (c == nullptr) return Theme::trackColour (project.effectiveColour (row.track));
    const auto v = project.frameworkValue (box, *c);
    const auto first = c->kind == Framework::Kind::tags ? Tags::parse (v.value)[0] : v.value;
    return fromModel (framework.valueColours (*c, first).colour, none);
}

TimelineView::Weight TimelineView::weightFor (const juce::ValueTree& box) const
{
    if (project.getState()[IDs::fwWeight].toString() == "off") return {};
    for (auto& c : framework.getCategories())
    {
        if (c.kind != Framework::Kind::choice) continue;
        const auto& values = framework.valuesOf (c);
        auto lightest = std::numeric_limits<int>::max(), heaviest = std::numeric_limits<int>::min();
        for (auto& val : values) { lightest = juce::jmin (lightest, val.weight); heaviest = juce::jmax (heaviest, val.weight); }
        if (values.isEmpty() || lightest == heaviest) continue;    // the first weighted category decides

        const auto v = project.frameworkValue (box, c);
        if (v.value.isEmpty() || v.isDefault()) return {};         // only a value the box (or its folder) states
        auto* val = framework.findValue (c, v.value);
        if (val == nullptr) return {};
        Weight w;
        if (val->weight == lightest) w.alphaFactor = 0.62f;        // the ground recedes
        w.emphasis = val->weight == heaviest;                      // the figure comes forward
        return w;
    }
    return {};
}

void TimelineView::paintFrameworkFill (juce::Graphics& g, const juce::ValueTree& box, juce::Rectangle<float> r, float alpha)
{
    if (project.getState()[IDs::fwFills].toString() == "off") return;
    for (auto& c : framework.getCategories())
    {
        if (c.kind != Framework::Kind::choice) continue;
        const auto v = project.frameworkValue (box, c);
        if (v.value.isEmpty() || v.isDefault()) continue;
        auto* val = framework.findValue (c, v.value);
        if (val == nullptr || val->fill.isEmpty()) continue;

        // The first filled value decides; "solid" is the plain box itself.
        const float strength = (v.fromFolder() ? 0.6f : 1.0f) * alpha;
        const auto colour = fromModel (framework.valueColours (c, v.value).colour, Theme::accent);
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 4.0f);
        g.reduceClipRegion (clip);
        if (val->fill == "hatched")
        {
            g.setColour (colour.withAlpha (0.12f * strength));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 7.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
        }
        else if (val->fill == "dotted")
        {
            g.setColour (colour.withAlpha (0.16f * strength));
            for (float y = r.getY() + 4.5f; y < r.getBottom() - 1.0f; y += 9.0f)
                for (float x = r.getX() + 4.5f; x < r.getRight() - 1.0f; x += 9.0f)
                    g.fillEllipse (x - 1.1f, y - 1.1f, 2.2f, 2.2f);
        }
        else if (val->fill == "outline")
        {
            g.setColour (Theme::background.withAlpha (0.32f * strength));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (colour.withAlpha (0.4f * strength));
            g.drawRoundedRectangle (r.reduced (2.0f), 3.0f, 1.0f);
        }
        return;
    }
}

void TimelineView::paintGlyphs (juce::Graphics& g, const juce::ValueTree& box, juce::Rectangle<float> r, float alpha, float bandBottom)
{
    const float bandH = juce::jmin (30.0f, (r.getHeight() - 16.0f) * 0.55f);
    if (bandH < 8.0f || r.getWidth() < 26.0f) return;

    // The profile: the first choice category whose effective value has an
    // envelope (a flat one changes nothing and is left out of the cache key).
    juce::Array<float> env;
    juce::String envKey;
    for (auto& c : framework.getCategories())
    {
        if (c.kind != Framework::Kind::choice) continue;
        const auto v = project.frameworkValue (box, c);
        if (v.value.isEmpty()) continue;
        auto* val = framework.findValue (c, v.value);
        if (val == nullptr || val->env.isEmpty()) continue;
        bool flat = true;
        for (int i = 1; i < val->env.size(); i += 2) flat = flat && std::abs (val->env[i] - 1.0f) < 1.0e-6f;
        if (! flat)
        {
            env = val->env;
            envKey = c.key() + "=" + v.value;
            // Its peak moved, its peak rounded (stage 4c).
            double peakAt = -1.0;
            bool soft = false;
            for (auto& shaper : framework.getCategories())
            {
                if (shaper.envelope.isEmpty()) continue;
                const auto sv = project.frameworkValue (box, shaper);
                if (sv.value.isEmpty()) continue;
                if (shaper.envelope == "peak" && val->peak >= 0)
                    peakAt = juce::jlimit (0.0, 1.0, (sv.value.getDoubleValue() - shaper.minimum) / (shaper.maximum - shaper.minimum));
                else if (shaper.envelope == "shape")
                    if (auto* shape = framework.findValue (shaper, sv.value)) soft = shape->curve == "soft";
            }
            if (peakAt >= 0.0 || soft)
            {
                env = Framework::shapeEnvelope (env, val->peak, peakAt, soft);
                envKey << "|" << juce::String (peakAt, 3) << (soft ? "s" : "h");
            }
        }
        break;
    }

    // Shapes are built at a bucketed size and stretched the last few pixels,
    // so dragging a box does not rebuild them at every width.
    const int seed = (box[IDs::id].toString().hashCode() & 0x7fffffff) % 7;
    const int wB = juce::jmin (2048, (((int) r.getWidth() + 11) / 12) * 12);
    const int hB = juce::jmax (3, (((int) bandH + 2) / 3) * 3);
    const juce::Rectangle<float> band (r.getX(), bandBottom - bandH, r.getWidth(), bandH);

    for (auto& c : framework.getCategories())
    {
        if (c.kind != Framework::Kind::choice || markFor (c) != Framework::Mark::glyph) continue;
        const auto v = project.frameworkValue (box, c);
        if (v.value.isEmpty()) continue;
        if (! c.showDefault && v.isDefault()) continue;
        auto* val = framework.findValue (c, v.value);
        if (val == nullptr || val->glyph.isEmpty()) continue;
        const auto found = glyphs.find (val->glyph, { val->from, c.package });
        if (found.script == nullptr) continue;

        const auto key = found.owner + "|" + val->glyph + "|" + juce::String (wB) + "x" + juce::String (hB)
                         + "|" + envKey + "|" + juce::String (seed);
        auto it = glyphCache.find (key);
        if (it == glyphCache.end())
        {
            if (glyphCache.size() > 400) glyphCache.clear();
            CachedGlyph built;
            juce::Array<Glyphs::Stroke> strokes;
            if (found.script->run (val->glyph, (float) wB, (float) hB, env, seed, strokes).wasOk())
                for (auto& stroke : strokes)
                {
                    if (stroke.points.size() >= 4)
                    {
                        juce::Path path;
                        path.startNewSubPath (stroke.points[0], stroke.points[1]);
                        for (int i = 2; i + 1 < stroke.points.size(); i += 2)
                            path.lineTo (stroke.points[i], stroke.points[i + 1]);
                        if (stroke.closed || stroke.filled) path.closeSubPath();
                        built.paths.add (std::move (path));
                        built.alphas.add (stroke.alpha);
                        built.widths.add (stroke.width);
                        built.fills.add (stroke.filled);
                    }
                    if (stroke.dots.size() >= 3)
                    {
                        juce::Path path;
                        for (int i = 0; i + 2 < stroke.dots.size(); i += 3)
                        {
                            const float radius = juce::jmax (0.3f, stroke.dots[i + 2]);
                            path.addEllipse (stroke.dots[i] - radius, stroke.dots[i + 1] - radius, radius * 2.0f, radius * 2.0f);
                        }
                        built.paths.add (std::move (path));
                        built.alphas.add (stroke.alpha);
                        built.widths.add (stroke.width);
                        built.fills.add (true);                    // dots are always filled
                    }
                }
            it = glyphCache.emplace (key, std::move (built)).first;
        }

        const auto& cached = it->second;
        if (cached.paths.isEmpty()) continue;
        const auto xform = juce::AffineTransform::scale (r.getWidth() / (float) wB, bandH / (float) hB)
                               .translated (band.getX(), band.getY());
        const auto colours = framework.valueColours (c, v.value);
        const auto colour = colours.light != 0 ? juce::Colour (colours.light)
                                               : fromModel (colours.colour, fromModel (c.colour, Theme::accent));
        // The second energy (a category sharing another's values) is drawn lighter, on top.
        const float strength = alpha * (v.fromFolder() ? 0.6f : 1.0f) * (c.valuesFrom.isEmpty() ? 0.9f : 0.5f);
        for (int i = 0; i < cached.paths.size(); ++i)
        {
            g.setColour (colour.withMultipliedAlpha (cached.alphas[i] * strength));
            if (cached.fills[i])
                g.fillPath (cached.paths[i], xform);
            else
                g.strokePath (cached.paths[i], juce::PathStrokeType (cached.widths[i] * 1.1f, juce::PathStrokeType::curved,
                                                                     juce::PathStrokeType::rounded), xform);
        }
    }
}

juce::String TimelineView::describeGap (double gap)
{
    const double a = std::abs (gap);
    const auto amount = a < 1.0 ? juce::String (juce::roundToInt (a * 1000.0)) + " ms" : juce::String (a, 2) + " s";
    return (gap < 0.0 ? "overlap " : "gap ") + amount;
}

const juce::Path& TimelineView::signPath (const juce::String& svg)
{
    auto it = signCache.find (svg);
    if (it == signCache.end())
    {
        if (signCache.size() > 200) signCache.clear();
        it = signCache.emplace (svg, juce::Drawable::parseSVGPath (svg)).first;
    }
    return it->second;
}

const TimelineView::SignHit* TimelineView::signAt (juce::Point<float> p) const
{
    for (auto it = signHits.rbegin(); it != signHits.rend(); ++it)
        if (it->area.contains (p)) return &*it;
    return nullptr;
}

juce::String TimelineView::getTooltip()
{
    if (auto* hit = signAt (getMouseXYRelative().toFloat())) return hit->tip;
    return {};
}

juce::String TimelineView::describeSource (const Project::FrameworkValue& v) const
{
    if (v.fromTiming()) return "from the timing";
    if (v.isImplied())
        if (auto* c = framework.findCategory (v.source))
            return "from the folder's " + framework.text (c->name).toLowerCase();
    if (v.fromFolder()) return "from the folder";
    return {};
}

juce::Path TimelineView::fadeGaps (float endX, float endY, float length)
{
    // The line before an end bracket turning to dashes: shorter dashes and
    // longer gaps towards the bracket, with a stub left on the corner. In
    // sign units, as fractions of the length (16 by default).
    static constexpr float gaps[][2] = { { 16.0f, 13.5f }, { 9.5f, 7.0f }, { 4.5f, 1.0f } };
    juce::Path p;
    const float k = length / 16.0f;
    for (auto& gap : gaps)
        p.addRectangle (endX - gap[0] * k, endY - 1.8f, (gap[0] - gap[1]) * k, 3.6f);   // the stroke and its smoothing
    return p;
}

void TimelineView::paintSeams (juce::Graphics& g, const TrackRow& row)
{
    juce::Array<const Framework::Category*> seamCats, endCats;
    for (auto& c : framework.getCategories())
    {
        const auto m = markFor (c);
        if (m == Framework::Mark::seam && c.between) seamCats.add (&c);
        else if (m == Framework::Mark::end && ! c.between) endCats.add (&c);
    }
    if (seamCats.isEmpty() && endCats.isEmpty()) return;

    constexpr float scale = 0.75f;                  // sign units (a 60 by 20 box) to pixels
    const float centreY = (float) row.y + 5.0f;     // across the top edge of the boxes
    juce::StringArray combined;                     // boxes whose ending went into a seam sign

    auto draw = [&] (const juce::Path& path, juce::Point<float> anchor, float anchorX, bool solid, float alpha,
                     const juce::String& boxId, const juce::String& tip, bool seam, const juce::Path& cut, const juce::Path& faint)
    {
        auto b = path.getBounds();
        if (! faint.isEmpty()) b = b.getUnion (faint.getBounds());
        const auto xform = juce::AffineTransform::translation (-anchorX, -b.getCentreY()).scaled (scale).translated (anchor);
        const auto drawn = b.transformedBy (xform);
        const auto area = juce::Rectangle<float> (drawn.getX() - 4.0f, anchor.y - 7.0f, drawn.getWidth() + 8.0f, 14.0f);
        if (area.getRight() < (float) headerWidth || area.getX() > (float) getWidth()) return;
        g.setColour (Theme::background.withAlpha (0.9f));
        g.fillRoundedRectangle (area, 3.0f);
        if (solid)
        {
            g.setColour (Theme::signs.withAlpha (0.8f * alpha));
            g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);
        }
        else
        {
            juce::Path edge, dashed;
            edge.addRoundedRectangle (area.reduced (0.5f), 3.0f);
            const float dashes[] = { 2.5f, 2.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, edge, dashes, 2);
            g.setColour (Theme::signs.withAlpha (0.45f * alpha));
            g.fillPath (dashed);
        }
        juce::Graphics::ScopedSaveState state (g);
        if (! cut.isEmpty())                        // gaps cut out of the line (a fading ending)
        {
            juce::Path keep;
            keep.setUsingNonZeroWinding (false);
            keep.addRectangle (area.expanded (4.0f));
            keep.addPath (cut, xform);
            g.reduceClipRegion (keep);
        }
        g.setColour (Theme::signs.withAlpha ((solid ? 1.0f : 0.75f) * alpha));
        g.strokePath (path, juce::PathStrokeType (1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), xform);
        if (! faint.isEmpty())                      // worked out, not chosen (a writing type's mixing relation)
        {
            g.setColour (Theme::signs.withAlpha (0.45f * alpha));
            g.strokePath (faint, juce::PathStrokeType (1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), xform);
        }
        signHits.push_back ({ area, boxId, tip, seam });
    };

    for (auto box : row.track)
    {
        float alpha = project.effectiveMute (row.track) ? 0.5f : 1.0f;
        if (highlight.isActive() && ! matchesHighlight (box)) alpha *= 0.3f;

        // == The seam with the box before
        const auto join = project.joinBefore (box);
        if (join.isValid() && ! seamCats.isEmpty())
        {
            juce::Path main, all, cut, faint;
            juce::StringArray tips;
            bool own = false;
            const Framework::Value* first = nullptr;
            float nextX = 0.0f;
            for (auto* c : seamCats)
            {
                const auto v = project.frameworkValue (box, *c);
                if (v.value.isEmpty()) continue;
                auto* val = framework.findValue (*c, v.value);
                auto tip = framework.valueName (*c, v.value);
                if (first == nullptr) tip << (v.isOwn() ? " (chosen; " : " (from the timing; ") << describeGap (join.gap) << ")";
                else if (v.isImplied()) tip << " (" << describeSource (v) << ")";
                tips.add (juce::String (framework.text (c->name)) + ": " + tip);
                if (val == nullptr || val->sign.isEmpty()) continue;
                auto path = signPath (val->sign);
                if (first == nullptr)
                {
                    first = val;
                    own = v.isOwn();
                    main = path;
                    nextX = path.getBounds().getRight() + 5.0f;
                }
                else
                {
                    const auto pb = path.getBounds();
                    path.applyTransform (juce::AffineTransform::translation (nextX - pb.getX(), 0.0f));
                    (v.isDerived() ? faint : all).addPath (path);
                    nextX += pb.getWidth() + 5.0f;
                }
            }
            for (auto* c : seamCats)
                for (auto& note : c->notes)
                    if (join.gap >= note.from && join.gap < note.to) tips.add (framework.text (note.text));

            // The ending of the box before, on its bracket (Thoresen's figure 6.4).
            if (first != nullptr && first->endX >= 0.0f && ! endCats.isEmpty())
            {
                const auto ev = project.frameworkValue (join.before, *endCats[0]);
                if (auto* ending = framework.findValue (*endCats[0], ev.value);
                    ending != nullptr && (ending->seamSign.isNotEmpty() || ending->seamFade > 0.0f))
                {
                    if (ending->seamSign.isNotEmpty())
                    {
                        auto mark = signPath (ending->seamSign);
                        mark.applyTransform (juce::AffineTransform::translation (first->endX, first->endY));
                        main.addPath (mark);
                    }
                    if (ending->seamFade > 0.0f)
                        cut = fadeGaps (first->fadeX >= 0.0f ? juce::jmin (first->fadeX, first->endX) : first->endX,
                                        first->endY, ending->seamFade);
                    combined.add (join.before[IDs::id].toString());
                    tips.add (framework.text (endCats[0]->name) + " of " + join.before[IDs::name].toString().quoted() + ": "
                              + framework.valueName (*endCats[0], ev.value));
                }
            }
            if (! main.isEmpty())
            {
                const double tEnd = juce::jmin (Project::boxEnd (join.before), Project::boxEnd (box));
                const float x = timeToX ((Project::boxStart (box) + tEnd) / 2.0);
                all.addPath (main);
                draw (all, { x, centreY }, main.getBounds().getCentreX(), own, alpha, box[IDs::id].toString(), tips.joinIntoString ("\n"), true, cut, faint);
            }
        }

    }

    // == How each box ends, on its own (when no seam sign took it; after all
    // the seams, since a box may come before the one it ends into)
    for (auto box : row.track)
    {
        float alpha = project.effectiveMute (row.track) ? 0.5f : 1.0f;
        if (highlight.isActive() && ! matchesHighlight (box)) alpha *= 0.3f;
        for (int k = 0; k < endCats.size(); ++k)
        {
            auto* c = endCats[k];
            if (k == 0 && combined.contains (box[IDs::id].toString())) continue;
            const auto v = project.frameworkValue (box, *c);
            auto* val = framework.findValue (*c, v.value);
            if (val == nullptr || val->sign.isEmpty()) continue;
            const auto& path = signPath (val->sign);
            const float x = timeToX (Project::boxEnd (box)) + 2.0f;
            draw (path, { x, centreY + (float) k * 15.0f }, path.getBounds().getRight(), true, alpha, box[IDs::id].toString(),
                  framework.text (c->name) + ": " + framework.valueName (*c, v.value), false, {}, {});
        }
    }
}

void TimelineView::showSignMenu (const SignHit& hit)
{
    auto box = project.findBox (hit.boxId);
    if (! box.isValid()) return;
    juce::PopupMenu m;
    const bool seam = hit.seam;
    addFrameworkMenus (m, { box }, [seam] (const Framework::Category& c) { return seam ? c.between : c.atEnd; });
    if (m.getNumItems() == 0) return;
    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, box] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= boxMenuBase && r < folderMenuBase) { safe->applyFrameworkMenu (r, { box }); safe->repaint(); }
    });
}

void TimelineView::paintFrameworkMarks (juce::Graphics& g, const juce::ValueTree& box, juce::Rectangle<float> r, float alpha)
{
    if (framework.getCategories().isEmpty()) return;
    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, 4.0f);
    g.reduceClipRegion (clip);

    // Strips along the bottom, one per category (first at the bottom): a
    // choice in its value's colour (a folder's value fainter) with its
    // changes along the box; a number as a bar of its share of the range.
    float bottom = r.getBottom();
    auto xAt = [&r] (double p) { return r.getX() + (float) p * r.getWidth(); };
    for (auto& c : framework.getCategories())
    {
        if (markFor (c) != Framework::Mark::strip) continue;
        const auto v = project.frameworkValue (box, c);
        const auto strip = juce::Rectangle<float> (r.getX(), bottom - 4.0f, r.getWidth(), 4.0f);
        if (c.kind == Framework::Kind::number)
        {
            if (v.value.isEmpty()) continue;
            const double n = juce::jlimit (0.0, 1.0, (v.value.getDoubleValue() - c.minimum) / (c.maximum - c.minimum));
            const auto colour = fromModel (c.colour, Theme::accent);
            g.setColour (colour.withMultipliedAlpha (0.18f * alpha));
            g.fillRect (strip);
            g.setColour (colour.withMultipliedAlpha ((v.fromFolder() ? 0.5f : 1.0f) * alpha));
            g.fillRect (strip.withWidth (strip.getWidth() * (float) n));
            bottom -= 5.0f;
            continue;
        }
        const auto changes = c.along ? Project::changesAlong (box, c) : juce::Array<Along::Change>();
        if (v.value.isEmpty() && changes.isEmpty()) continue;
        if (! c.showDefault && v.isDefault() && changes.isEmpty()) continue;
        auto colourOf = [this, &c] (const juce::String& id)
        {
            return fromModel (framework.valueColours (c, id).colour, id.isEmpty() ? juce::Colours::transparentBlack : fromModel (c.colour, Theme::accent));
        };
        double from = 0.0;
        auto current = v.value;
        auto currentAlpha = v.fromFolder() ? 0.5f : 1.0f;
        for (auto& ch : changes)
        {
            const double a = juce::jlimit (from, 1.0, ch.at - ch.width / 2.0), b = juce::jlimit (a, 1.0, ch.at + ch.width / 2.0);
            g.setColour (colourOf (current).withMultipliedAlpha (currentAlpha * alpha));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (from), strip.getY(), xAt (a), strip.getBottom()));
            if (b - a > 1.0e-6)
            {
                g.setGradientFill (juce::ColourGradient (colourOf (current).withMultipliedAlpha (currentAlpha * alpha), xAt (a), 0.0f,
                                                         colourOf (ch.to).withMultipliedAlpha (alpha), xAt (b), 0.0f, false));
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (a), strip.getY(), xAt (b), strip.getBottom()));
            }
            current = ch.to;
            currentAlpha = 1.0f;
            from = b;
        }
        g.setColour (colourOf (current).withMultipliedAlpha (currentAlpha * alpha));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (from), strip.getY(), xAt (1.0), strip.getBottom()));
        bottom -= 5.0f;
    }

    // The energy glyphs, in the band above the strips (4b.2).
    paintGlyphs (g, box, r, alpha, bottom);

    // Badges at the bottom right (the last category rightmost), then labels at
    // the bottom left in the room that is left.
    if (r.getWidth() < 40.0f || r.getHeight() < 22.0f) return;
    float x = juce::jmin (r.getRight(), (float) getWidth()) - 4.0f;     // in view, also for long boxes
    const float leftEdge = juce::jmax (r.getX(), (float) headerWidth) + 4.0f;
    const float y = bottom - 2.0f - 14.0f;
    const auto arrow = juce::String::fromUTF8 ("\xe2\x86\x92");
    float badgeAlpha = alpha;      // fainter for a value the box has from its folder
    auto badge = [&] (const juce::String& text, juce::Colour fill, juce::Colour edge, juce::Colour ink)
    {
        const auto font = Theme::font (10.5f, true);
        const float w = juce::jmin (160.0f, (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 10.0f);
        if (x - w < leftEdge + 26.0f) return;
        juce::Rectangle<float> b (x - w, y, w, 14.0f);
        g.setColour (fill.withMultipliedAlpha (badgeAlpha));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (edge.withMultipliedAlpha (badgeAlpha));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (ink.withMultipliedAlpha (juce::jmax (0.45f, badgeAlpha)));
        g.setFont (font);
        g.drawText (text, b.reduced (3.0f, 0.0f), juce::Justification::centred, true);
        x -= w + 3.0f;
    };

    juce::StringArray labels;
    const auto& cats = framework.getCategories();
    for (int i = cats.size(); --i >= 0;)
    {
        const auto& c = cats.getReference (i);
        const auto mark = markFor (c);
        if (mark != Framework::Mark::badge && mark != Framework::Mark::label) continue;
        const auto v = project.frameworkValue (box, c);
        const auto changes = c.along ? Project::changesAlong (box, c) : juce::Array<Along::Change>();
        if (v.value.isEmpty() && changes.isEmpty()) continue;
        if (! c.showDefault && v.isDefault() && changes.isEmpty()) continue;

        juce::String shortText, longText;
        Framework::Colours colours;
        switch (c.kind)
        {
            case Framework::Kind::choice:
                shortText = framework.valueLabel (c, v.value);
                longText = framework.valueName (c, v.value);
                for (auto& ch : changes) { shortText << arrow << framework.valueLabel (c, ch.to); longText << " " << arrow << " " << framework.valueName (c, ch.to); }
                colours = framework.valueColours (c, v.value);
                break;
            case Framework::Kind::tags:
            {
                juce::StringArray s, l;
                for (auto& t : Tags::parse (v.value)) { s.add (framework.valueLabel (c, t)); l.add (framework.valueName (c, t)); }
                shortText = s.joinIntoString (" ");
                longText = l.joinIntoString (", ");
                break;
            }
            case Framework::Kind::number:
                shortText = longText = framework.text (c.name) + " " + framework.formatNumber (c, v.value.getDoubleValue());
                break;
            case Framework::Kind::text:
                shortText = v.value.length() > 24 ? v.value.substring (0, 23) + juce::String::fromUTF8 ("\xe2\x80\xa6") : v.value;
                longText = v.value;
                break;
        }
        if (mark == Framework::Mark::label) { labels.insert (0, longText); continue; }
        badgeAlpha = v.fromFolder() && changes.isEmpty() ? alpha * 0.55f : alpha;
        if (colours.colour != 0)
            badge (shortText, juce::Colour (colours.dark != 0 ? colours.dark : colours.colour).withAlpha (0.92f),
                   juce::Colour (colours.colour), juce::Colour (colours.light != 0 ? colours.light : 0xffe8e4dc));
        else
            badge (shortText, Theme::background.withAlpha (0.85f), fromModel (c.colour, Theme::accent).withAlpha (0.8f), fromModel (c.colour, Theme::accent));
    }

    if (! labels.isEmpty() && r.getHeight() >= 30.0f && x - leftEdge > 30.0f)
    {
        const auto font = Theme::font (10.5f);
        const auto text = labels.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 "));
        const float w = juce::jmin (x - leftEdge - 4.0f, (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 10.0f);
        juce::Rectangle<float> b (leftEdge - 1.0f, y, w, 14.0f);
        g.setColour (Theme::background.withAlpha (0.55f * alpha));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (Theme::text.withAlpha (juce::jmax (0.6f, alpha)));
        g.setFont (font);
        g.drawText (text, b.reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);
    }
}

bool TimelineView::Highlight::isActive() const
{
    for (auto& v : ticked.getAllValues()) if (v.isNotEmpty()) return true;
    return false;
}

juce::String TimelineView::Highlight::describe (const Framework& f) const
{
    juce::StringArray parts;
    for (int i = 0; i < ticked.size(); ++i)
    {
        auto key = ticked.getAllKeys()[i];
        const bool groups = key.endsWith ("#groups");
        if (groups) key = key.dropLastCharacters (7);
        auto* c = f.findCategory (key);
        if (c == nullptr) continue;
        for (auto& id : Tags::parse (ticked.getAllValues()[i]))
        {
            if (groups) { auto* g = f.findGroup (*c, id); parts.add (g != nullptr ? f.text (g->name) : id + "?"); }
            else parts.add (f.valueLabel (*c, id));
        }
    }
    return parts.joinIntoString (", ");
}

void TimelineView::toggleHighlight (const juce::String& key, const juce::String& id)
{
    auto ids = highlight.idsFor (key);
    if (ids.contains (id)) ids.removeString (id);
    else ids.add (id);
    if (ids.isEmpty()) highlight.ticked.remove (juce::StringRef (key));
    else highlight.ticked.set (key, Tags::format (ids));
    repaint();
}

bool TimelineView::matchesHighlight (const juce::ValueTree& box)
{
    for (int i = 0; i < highlight.ticked.size(); ++i)
    {
        auto key = highlight.ticked.getAllKeys()[i];
        const auto ids = Tags::parse (highlight.ticked.getAllValues()[i]);
        if (ids.isEmpty()) continue;
        const bool groups = key.endsWith ("#groups");
        if (groups) key = key.dropLastCharacters (7);
        auto* source = framework.findCategory (key);
        if (source == nullptr) continue;            // a package that is gone
        bool any = false;
        for (auto* c : framework.sharing (*source))
            for (auto& v : project.valuesShown (box, *c))
            {
                if (! groups) any = any || ids.contains (v);
                else if (auto* value = framework.findValue (*c, v))
                    for (auto& g : value->groups) any = any || ids.contains (g);
            }
        if (! any) return false;
    }
    return true;
}

void TimelineView::addFrameworkMenus (juce::PopupMenu& m, const juce::Array<juce::ValueTree>& boxes,
                                      std::function<bool (const Framework::Category&)> only)
{
    // Ids: boxMenuBase + category index * menuStride + 1 + value index (+0 = none).
    for (auto& c : framework.getCategories())
    {
        if ((c.kind != Framework::Kind::choice && c.kind != Framework::Kind::tags) || ! c.forBoxes()) continue;
        if (only && ! only (c)) continue;
        const auto& values = framework.valuesOf (c);
        if (values.isEmpty() || values.size() >= menuStride - 1) continue;
        bool applies = false;                       // "only with": for at least one of the boxes
        for (auto b : boxes) applies = applies || project.applies (b, c);
        if (! applies) continue;
        const int base = boxMenuBase + c.index * menuStride;

        // What the boxes have now: a choice they share, the tags all of them have.
        juce::String common;
        juce::StringArray allTags;
        bool first = true, mixed = false;
        for (auto b : boxes)
        {
            const auto own = b[c.property()].toString();
            if (c.kind == Framework::Kind::choice) { if (first) common = own; else if (own != common) mixed = true; }
            else
            {
                auto tags = Tags::parse (own);
                if (first) allTags = tags;
                else for (int k = allTags.size(); --k >= 0;) if (! tags.contains (allTags[k])) allTags.remove (k);
            }
            first = false;
        }

        juce::PopupMenu sub;
        if (c.kind == Framework::Kind::choice)
        {
            auto none = c.defaultValue.isNotEmpty() ? "None (the default, " + framework.valueLabel (c, c.defaultValue) + ")"
                      : c.folders ? juce::String ("None (or the folder's)") : juce::String ("None");
            if (c.between || ! framework.implicationsFor (c).isEmpty())
            {
                // Back to what is worked out: the timing, or the folder's other value (a writing type).
                none = ! c.derive.isEmpty() ? "From the timing" : ! framework.implicationsFor (c).isEmpty() ? "From the folder" : "None";
                if (boxes.size() == 1)
                {
                    const auto derived = project.frameworkValue (boxes.getFirst(), c, true);
                    if (derived.value.isNotEmpty()) none << " (" << framework.valueName (c, derived.value) << ")";
                }
            }
            sub.addItem (base, none, true, ! mixed && common.isEmpty());
        }
        auto addValue = [&] (int i)
        {
            const auto& v = values.getReference (i);
            bool fits = false;
            for (auto b : boxes) fits = fits || project.valueApplies (b, c, v.id);
            if (! fits) return;
            auto text = framework.valueName (c, v.id);
            if (v.label.isNotEmpty() && ! text.startsWith (v.label)) text = v.label + "   " + text;
            const bool ticked = c.kind == Framework::Kind::choice ? (! mixed && common == v.id) : allTags.contains (v.id);
            sub.addItem (base + 1 + i, text, true, ticked);
        };
        juce::Array<int> listed;
        for (auto& g : framework.groupsOf (c))
        {
            bool heading = false;
            for (int i = 0; i < values.size(); ++i)
                if (values[i].groups.size() > 0 && values[i].groups[0] == g.id)
                {
                    if (! heading) { sub.addSectionHeader (framework.text (g.name)); heading = true; }
                    addValue (i);
                    listed.add (i);
                }
        }
        if (! listed.isEmpty() && listed.size() < values.size()) sub.addSeparator();
        for (int i = 0; i < values.size(); ++i)
            if (! listed.contains (i)) addValue (i);
        m.addSubMenu (framework.text (c.name), sub);
    }
}

void TimelineView::applyFrameworkMenu (int r, const juce::Array<juce::ValueTree>& boxes)
{
    const int index = (r - boxMenuBase) / menuStride, item = (r - boxMenuBase) % menuStride;
    if (! juce::isPositiveAndBelow (index, framework.getCategories().size())) return;
    const auto& c = framework.getCategories().getReference (index);
    const auto& values = framework.valuesOf (c);
    if (item > values.size()) return;
    const auto id = item == 0 ? juce::String() : values[item - 1].id;
    project.getUndoManager().beginNewTransaction (framework.text (c.name));
    if (c.kind == Framework::Kind::tags)
    {
        // Ticked on all of them: off for all; else on for all.
        bool all = true;
        for (auto b : boxes) all = all && Tags::parse (b[c.property()].toString()).contains (id);
        for (auto b : boxes)
        {
            auto tags = Tags::parse (b[c.property()].toString());
            if (all) tags.removeString (id); else tags.addIfNotAlreadyThere (id);
            project.setFrameworkValue (b, c, Tags::format (tags));
        }
        return;
    }
    for (auto b : boxes)
    {
        if (! project.applies (b, c) || (id.isNotEmpty() && ! project.valueApplies (b, c, id))) continue;
        project.setFrameworkValue (b, c, id == c.defaultValue ? juce::String() : id);
    }
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
    if (rulerMouseMove (e)) return;
    Zone zone;
    auto box = boxAt (e.position, zone);
    if (zone == Zone::startEdge || zone == Zone::endEdge) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (zone == Zone::fadeInHandle || zone == Zone::fadeOutHandle) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else setMouseCursor (juce::MouseCursor::NormalCursor);
    juce::ignoreUnused (box);
}

void TimelineView::mouseDown (const juce::MouseEvent& e)
{
    dragMode = DragMode::none;
    if (auto* parent = getParentComponent()) parent->grabKeyboardFocus();   // so Space etc. reach the commands

    // The ruler layers
    if (rulerMouseDown (e)) return;

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

    // A folder's lane: its nodes (4d.2).
    if (auto frow = rowAt (e.y); frow.folder && nodeMouseDown (frow, e)) return;

    // Lanes: a seam or end sign selects its box (the later one, for a seam).
    if (auto* hit = signAt (e.position))
    {
        const auto copy = *hit;
        auto signBox = project.findBox (copy.boxId);
        if (signBox.isValid())
        {
            selectBox (signBox);
            if (e.mods.isPopupMenu()) showSignMenu (copy);
            return;
        }
    }

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
    if (rulerMouseDrag (e)) return;
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
            const auto markAtStart = snappedMark;
            const double snappedEnd = snap (start + len, dragBox, snapping);
            const auto markAtEnd = snappedMark;
            const bool byStart = std::abs (snappedStart - start) <= std::abs (snappedEnd - (start + len));
            if (byStart) start = snappedStart;
            else start = snappedEnd - len;
            const auto pinTo = byStart ? markAtStart : markAtEnd;

            double delta = start - dragOrigStart;
            for (auto& m : moving) delta = juce::jmax (delta, -m.start);   // nothing before 0:00

            std::vector<juce::ValueTree> lanes;
            const auto rs = rows();
            for (auto& r : rs) if (! r.folder) lanes.push_back (r.track);
            auto row = rowAt (juce::jlimit (tracksTop(), getHeight() - 1, e.y));
            newTracksBelow = 0;
            if (! rs.empty() && ! lanes.empty() && e.y >= rs.back().y + rs.back().h)
            {
                // Below the last track: the grabbed box goes to a new track
                // there (the others keep their distance from it); made on release.
                int grabbedTrack = -1;
                bool folded = false;
                for (auto& m : moving) { if (m.box == dragBox) grabbedTrack = m.track; folded = folded || m.track < 0; }
                if (grabbedTrack >= 0 && ! folded)
                {
                    const int trackDelta = (int) lanes.size() - grabbedTrack;
                    for (auto& m : moving)
                        newTracksBelow = juce::jmax (newTracksBelow, m.track + trackDelta - ((int) lanes.size() - 1));
                    newTrackDelta = trackDelta;
                }
            }
            else if (row.track.isValid() && ! row.folder)
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

            // Dropped on a mark: a box with proportions is pinned to it.
            if (pinTo.isNotEmpty() && project.isBound (dragBox) && Project::isLocked (dragBox))
            {
                project.pinEdge (dragBox, byStart, pinTo);
                readout << (byStart ? "   start pinned to " : "   end pinned to ") << pinTo;
            }
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
        {
            const double v = snap (t, dragBox, snapping);
            const auto pinTo = snappedMark;
            project.trimStart (dragBox, v);
            readout = readoutFor (dragBox, Project::boxStart (dragBox), "start");
            if (pinTo.isNotEmpty() && project.isBound (dragBox) && Project::isLocked (dragBox))
            {
                project.pinEdge (dragBox, true, pinTo);
                readout << "   pinned to " << pinTo;
            }
            readoutPos = e.getPosition();
            repaint();
            break;
        }

        case DragMode::trimEnd:
        {
            const double v = snap (t, dragBox, snapping);
            const auto pinTo = snappedMark;
            project.trimEnd (dragBox, v);
            readout = readoutFor (dragBox, Project::boxEnd (dragBox), "end");
            if (project.isBound (dragBox))
                readout << "   length " << Proportion::describe (project.actualProportion (dragBox, false), project.getDenominators());
            if (pinTo.isNotEmpty() && project.isBound (dragBox) && Project::isLocked (dragBox))
            {
                project.pinEdge (dragBox, false, pinTo);
                readout << "   pinned to " << pinTo;
            }
            readoutPos = e.getPosition();
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

        case DragMode::span:
        case DragMode::frameStart:
        case DragMode::frameEnd:
        case DragMode::layerHeight:
        case DragMode::mark:        // handled by the ruler (TimelineRuler.cpp)
        case DragMode::drawSpan:
        case DragMode::itemMove:
        case DragMode::itemStart:
        case DragMode::itemEnd:
        case DragMode::itemGoal:
        case DragMode::itemGoalEnd:
        case DragMode::drawNode:
        case DragMode::nodeMove:
        case DragMode::nodeStart:
        case DragMode::nodeEnd:     // drawn lanes and nodes (TimelineDrawn.cpp)
        case DragMode::none:
            break;
    }
}

void TimelineView::mouseUp (const juce::MouseEvent& e)
{
    if (drawnMouseUp (e)) return;                   // drawn lanes and nodes (4d.2)
    if (clickedSelectedBox && ! e.mouseWasDraggedSinceMouseDown() && dragBox.isValid())
        selectBox (dragBox);
    clickedSelectedBox = false;
    if (dragMode == DragMode::lasso) { lasso = {}; repaint(); }

    if (dragMode == DragMode::move && newTracksBelow > 0)
    {
        // Still within the move's undo step.
        std::vector<juce::ValueTree> lanes;
        for (auto& r : rows()) if (! r.folder) lanes.push_back (r.track);
        for (int i = 0; i < newTracksBelow; ++i) lanes.push_back (project.addTrack());
        for (auto& m : moving)
        {
            const int to = m.track + newTrackDelta;
            if (m.track < 0 || ! juce::isPositiveAndBelow (to, (int) lanes.size())) continue;
            project.moveBox (m.box, lanes[(size_t) to], Project::boxStart (m.box));
        }
        if (dragBox.isValid()) selectedTrackId = dragBox.getParent()[IDs::id].toString();
    }
    newTracksBelow = 0;

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
    dragLayer = {};
    dragMark = {};
    readout.clear();
    lastTrackDelta = 0;

    dragMode = DragMode::none;
    dragBox = {};
    moving.clear();
    repaint();
}

void TimelineView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (rulerDoubleClick (e)) return;
    if (e.y < tracksTop()) return;

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
        scrollY = juce::jlimit (0, maxScroll(), scrollY - juce::roundToInt (dy * 200.0f));
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
    Dialogs::focusField (w, "name");
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
        // Categories a folder can give to the boxes inside (ids: folderMenuBase + index * stride + 1 + value).
        for (auto& c : framework.getCategories())
        {
            if (! c.folders || c.kind != Framework::Kind::choice) continue;
            const auto& values = framework.valuesOf (c);
            if (values.isEmpty() || values.size() >= menuStride - 1) continue;
            const int base = folderMenuBase + c.index * menuStride;
            const auto current = track[c.property()].toString();
            juce::PopupMenu sub;
            sub.addItem (base, "None", true, current.isEmpty());
            for (int i = 0; i < values.size(); ++i)
                sub.addItem (base + 1 + i, framework.valueName (c, values[i].id), true, current == values[i].id);
            m.addSubMenu (framework.text (c.name) + (c.foldersOnly ? juce::String() : juce::String (" for its boxes")), sub);
        }
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
        else if (r >= folderMenuBase)
        {
            const int catIndex = (r - folderMenuBase) / menuStride, item = (r - folderMenuBase) % menuStride;
            const auto& fw = safe->framework;
            if (juce::isPositiveAndBelow (catIndex, fw.getCategories().size()))
            {
                const auto& c = fw.getCategories().getReference (catIndex);
                if (item <= fw.valuesOf (c).size())
                {
                    p.getUndoManager().beginNewTransaction (fw.text (c.name) + " of the folder");
                    p.setFrameworkValue (track, c, item == 0 ? juce::String() : fw.valuesOf (c)[item - 1].id);
                }
            }
        }
        else if (r >= 100) { p.getUndoManager().beginNewTransaction ("Track colour"); track.setProperty (IDs::colour, r - 100, p.um()); }
        safe->repaint();
    });
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
    if (project.isBound (box) && project.getMarks().getNumChildren() > 0)
    {
        juce::PopupMenu pinStart, pinEnd;
        int i = 0;
        for (auto mk : project.getMarks())
        {
            pinStart.addItem (1000 + i, mk[IDs::name].toString());
            pinEnd.addItem (2000 + i, mk[IDs::name].toString());
            ++i;
        }
        m.addSubMenu ("Pin the start to a mark", pinStart);
        m.addSubMenu ("Pin the end to a mark", pinEnd);
    }
    m.addItem (11, "Split by a ratio (cut, or marks in it)...", ! several);
    m.addSeparator();
    {
        auto selection = getSelectedBoxes();
        if (! selection.contains (box)) selection = { box };
        addFrameworkMenus (m, selection);
    }

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, box, sound] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->project;
        auto selection = safe->getSelectedBoxes();
        if (selection.isEmpty()) selection.add (box);
        if (r >= boxMenuBase && r < folderMenuBase) { safe->applyFrameworkMenu (r, selection); safe->repaint(); return; }

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
        else if (r == 11) safe->askRatioSplit ({}, box);
        else if (r >= 1000 && r < 3000)
        {
            const bool start = r < 2000;
            auto mk = p.getMarks().getChild ((r - (start ? 1000 : 2000)));
            p.getUndoManager().beginNewTransaction ("Pin to mark");
            for (auto b : selection)
                if (p.isBound (b))
                {
                    if (! Project::isLocked (b)) p.setLock (b, true);
                    p.pinEdge (b, start, mk[IDs::name].toString());
                }
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
