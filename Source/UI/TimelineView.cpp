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
    auto tracks = project.getState().getChildWithName (IDs::TRACKS);
    for (auto t : tracks)
    {
        const int h = juce::jlimit (40, 240, (int) t.getProperty (IDs::height, 72));
        r.push_back ({ t, y, h });
        y += h;
    }
    return r;
}

int TimelineView::contentHeight() const
{
    int h = 0;
    for (auto t : project.getState().getChildWithName (IDs::TRACKS))
        h += juce::jlimit (40, 240, (int) t.getProperty (IDs::height, 72));
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

juce::Rectangle<int> TimelineView::muteButton (const TrackRow& r) const   { return { headerWidth - 52, r.y + 8, 20, 18 }; }
juce::Rectangle<int> TimelineView::soloButton (const TrackRow& r) const   { return { headerWidth - 28, r.y + 8, 20, 18 }; }

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
    if (! row.track.isValid() || p.x < (float) headerWidth) return {};

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
    return best;
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
        g.setColour (i % 2 == 0 ? Theme::laneEven : Theme::laneOdd);
        g.fillRect (headerWidth, r.y, getWidth() - headerWidth, r.h);
        g.setColour (Theme::line);
        g.fillRect (headerWidth, r.y + r.h - 1, getWidth() - headerWidth, 1);
    }

    // Boxes, clipped to the lane area
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (headerWidth, rulerHeight, getWidth() - headerWidth, getHeight() - rulerHeight);
        for (auto& r : rs)
            for (auto box : r.track)
            {
                auto b = boxBounds (box, r);
                if (b.getRight() < (float) headerWidth || b.getX() > (float) getWidth()) continue;
                paintBox (g, box, r);
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
        tri.addTriangle (px - 5.0f, (float) rulerHeight - 9.0f, px + 5.0f, (float) rulerHeight - 9.0f, px, (float) rulerHeight - 1.0f);
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
    g.fillRect (0, 0, getWidth(), rulerHeight);
    g.setColour (Theme::line);
    g.fillRect (0, rulerHeight - 1, getWidth(), 1);

    g.setColour (Theme::textMuted);
    g.setFont (Theme::font (11.0f));
    g.drawText ("Seconds", 12, 0, headerWidth - 16, rulerHeight, juce::Justification::centredLeft);

    // A step that leaves at least 70 px between labels.
    static const double steps[] = { 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1200 };
    double step = steps[std::size (steps) - 1];
    for (double s : steps) if (s * pps >= 70.0) { step = s; break; }
    const int decimals = step < 0.1 ? 2 : (step < 1.0 ? 1 : 0);

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (headerWidth, 0, getWidth() - headerWidth, rulerHeight);

    const double end = xToTime ((float) getWidth());
    const double first = std::floor (viewStart / step) * step;
    for (double t = first; t <= end; t += step)
    {
        const float x = timeToX (t);
        g.setColour (Theme::lineStrong);
        g.fillRect (x, 12.0f, 1.0f, (float) rulerHeight - 13.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::mono (11.0f));
        g.drawText (TimeFormat::format (t, decimals), (int) x + 4, 2, 80, 16, juce::Justification::centredLeft);

        // Minor ticks
        g.setColour (Theme::line);
        for (int k = 1; k < 5; ++k)
        {
            const float mx = timeToX (t + step * k / 5.0);
            g.fillRect (mx, (float) rulerHeight - 6.0f, 1.0f, 5.0f);
        }
    }
}

void TimelineView::paintTrackHeader (juce::Graphics& g, const TrackRow& r, int index)
{
    juce::ignoreUnused (index);
    auto track = r.track;
    const bool selected = track[IDs::id].toString() == selectedTrackId;

    g.setColour (selected ? juce::Colour (0xff202227) : Theme::header);
    g.fillRect (0, r.y, headerWidth, r.h);
    g.setColour (Theme::trackColour ((int) track[IDs::colour]));
    g.fillRect (0, r.y, 4, r.h - 1);
    g.setColour (Theme::line);
    g.fillRect (0, r.y + r.h - 1, headerWidth, 1);

    g.setColour (Theme::text);
    g.setFont (Theme::font (13.5f, selected));
    g.drawText (track[IDs::name].toString(), 14, r.y + 6, headerWidth - 72, 22, juce::Justification::centredLeft, true);

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

    if (r.h >= 56)
    {
        g.setColour (Theme::textMuted);
        g.setFont (Theme::font (11.0f));
        g.drawText (juce::String (track.getNumChildren()) + (track.getNumChildren() == 1 ? " box" : " boxes"),
                    14, r.y + 30, headerWidth - 24, 16, juce::Justification::centredLeft);
    }
}

void TimelineView::paintBox (juce::Graphics& g, const juce::ValueTree& box, const TrackRow& row)
{
    auto r = boxBounds (box, row);
    const bool selected = isSelected (box);
    const bool sound = Project::isSound (box);
    float alpha = juce::jlimit (0.15f, 1.0f, (float) (double) box.getProperty (IDs::opacity, 1.0));
    if ((bool) row.track[IDs::mute]) alpha *= 0.45f;     // muted track: dimmed
    const auto base = Theme::trackColour ((int) row.track[IDs::colour]);
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
    if (e.y < rulerHeight && e.x >= headerWidth)       setMouseCursor (juce::MouseCursor::IBeamCursor);
    else if (zone == Zone::startEdge || zone == Zone::endEdge) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (zone == Zone::fadeInHandle || zone == Zone::fadeOutHandle) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else setMouseCursor (juce::MouseCursor::NormalCursor);
    juce::ignoreUnused (box);
}

void TimelineView::mouseDown (const juce::MouseEvent& e)
{
    dragMode = DragMode::none;
    if (auto* parent = getParentComponent()) parent->grabKeyboardFocus();   // so Space etc. reach the commands

    // Ruler: locate
    if (e.y < rulerHeight)
    {
        if (e.x >= headerWidth)
        {
            dragMode = DragMode::locate;
            const double t = juce::jmax (0.0, xToTime (e.position.x));
            if (onLocate) onLocate (t);
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

    // Everything selected moves together.
    moving.clear();
    auto tracks = project.getTracks();
    for (auto b : getSelectedBoxes())
        moving.push_back ({ b, Project::boxStart (b), tracks.indexOf (b.getParent()) });

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

            auto tracks = project.getTracks();
            auto row = rowAt (juce::jlimit (rulerHeight, getHeight() - 1, e.y));
            int trackDelta = 0;
            if (row.track.isValid())
            {
                int grabbedTrack = 0;
                for (auto& m : moving) if (m.box == dragBox) grabbedTrack = m.track;
                trackDelta = tracks.indexOf (row.track) - grabbedTrack;
                for (auto& m : moving)
                    trackDelta = juce::jlimit (-m.track, tracks.getNumChildren() - 1 - m.track, trackDelta);
            }

            for (auto& m : moving)
                project.moveBox (m.box, tracks.getChild (m.track + trackDelta), m.start + delta);
            selectedTrackId = dragBox.getParent()[IDs::id].toString();
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

        case DragMode::trimStart:  project.trimStart (dragBox, snap (t, dragBox, snapping)); break;
        case DragMode::trimEnd:    project.trimEnd (dragBox, snap (t, dragBox, snapping)); break;

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
    dragMode = DragMode::none;
    dragBox = {};
    moving.clear();
}

void TimelineView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.y < rulerHeight) return;

    if (e.x < headerWidth)
    {
        auto row = rowAt (e.y);
        if (row.track.isValid() && ! muteButton (row).contains (e.getPosition()) && ! soloButton (row).contains (e.getPosition()))
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
    if (row.track.isValid() && onAddTextBox)
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
    juce::PopupMenu colours;
    for (int i = 0; i < Theme::numTrackColours; ++i)
        colours.addItem (100 + i, Theme::trackColourName (i), true, (int) track[IDs::colour] == i);

    juce::PopupMenu m;
    m.addItem (1, "Rename...");
    m.addSubMenu ("Colour", colours);
    m.addSeparator();
    m.addItem (2, "Add track above");
    m.addItem (3, "Add track below");
    m.addSeparator();
    m.addItem (4, "Delete track" + juce::String (track.getNumChildren() > 0 ? " and its boxes" : ""), project.getNumTracks() > 1);

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, track] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->project;
        const int index = p.getTracks().indexOf (track);
        if (r == 1) safe->renameTrack (track);
        else if (r == 2) { p.getUndoManager().beginNewTransaction ("Add track"); p.addTrack ({}, index); }
        else if (r == 3) { p.getUndoManager().beginNewTransaction ("Add track"); p.addTrack ({}, index + 1); }
        else if (r == 4) { p.getUndoManager().beginNewTransaction ("Delete track"); p.removeTrack (track); }
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
    if (! track.isValid())
    {
        // Below the last track: a new track.
        project.getUndoManager().beginNewTransaction ("Add track");
        track = project.addTrack();
    }
    onFilesDropped (audio, track, juce::jmax (0.0, x >= headerWidth ? xToTime ((float) x) : playhead));
}
