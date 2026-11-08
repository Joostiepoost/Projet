// TimelineRuler.cpp — the ruler layers of the timeline (stage 3b): a stack of
// one time layer and any number of proportion layers, each with its own
// frame, divisions, series and marks.

#include "TimelineView.h"
#include "Theme.h"
#include "TimeFormat.h"

namespace
{
    const juce::Colour seriesColour (0xff8fd6c9);
    const juce::Colour layerBackground (0xff1b1a17);
    constexpr int minLayerHeight = 12, maxLayerHeight = 90;
    constexpr int sizeSmall = 16, sizeMedium = 24, sizeLarge = 36;

    juce::String sizeName (int h)  { return h <= 18 ? "S" : (h <= 30 ? "M" : "L"); }
}

//== Geometry ==================================================================

std::vector<TimelineView::LayerRow> TimelineView::rulerRows() const
{
    std::vector<LayerRow> out;
    int y = 0;
    for (auto l : project.getRulers())
    {
        if (! (bool) l.getProperty (IDs::visible, true)) continue;
        const int h = juce::jlimit (minLayerHeight, maxLayerHeight, (int) l.getProperty (IDs::height, 24));
        out.push_back ({ l, y, h, Project::isTimeLayer (l) });
        y += h;
    }
    if (out.empty()) out.push_back ({ {}, 0, 16, false });    // every layer hidden: a thin strip to bring them back
    return out;
}

int TimelineView::rulerHeight() const
{
    int h = 0;
    for (auto& r : rulerRows()) h += r.h;
    return h;
}

TimelineView::LayerRow TimelineView::layerAt (int y) const
{
    for (auto& r : rulerRows())
        if (y >= r.y && y < r.y + r.h) return r;
    return {};
}

std::vector<TimelineView::Tick> TimelineView::visibleTicks (const juce::ValueTree& layer) const
{
    std::vector<Tick> out;
    if (! layer.isValid() || Project::isTimeLayer (layer)) return out;
    const auto f = project.frameFor (project.layerFrame (layer));
    if (! f.isValid()) return out;

    const double widthPx = f.length * pps;
    auto dens = project.layerDenominators (layer);
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
    const bool autoDivisions = (bool) layer.getProperty (IDs::autoDivisions, true);
    const auto shown = autoDivisions ? pick (7.0) : dens;
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

    // Series (golden section, Fibonacci, harmonic, your own): always shown.
    for (auto& p : Proportion::seriesPoints (layer[IDs::series].toString()))
    {
        Tick k;
        k.time = f.at (p.position);
        k.series = true;
        k.seriesLabel = p.label;
        k.height = 1.0f;
        k.labelled = true;
        out.push_back (k);
    }
    return out;
}

juce::ValueTree TimelineView::markAt (const LayerRow& row, float x) const
{
    if (! row.layer.isValid() || row.time) return {};
    const auto ref = project.layerFrame (row.layer);
    for (auto m : project.getMarks())
    {
        if (m[IDs::frame].toString() != ref) continue;
        const double t = project.markTime (m);
        if (t >= 0.0 && std::abs (timeToX (t) - x) <= 5.0f) return m;
    }
    return {};
}

//== Painting ==================================================================

void TimelineView::paintRuler (juce::Graphics& g)
{
    for (auto& r : rulerRows())
    {
        if (! r.layer.isValid())
        {
            g.setColour (layerBackground);
            g.fillRect (0, r.y, getWidth(), r.h);
            g.setColour (Theme::textMuted);
            g.setFont (Theme::font (10.5f));
            g.drawText ("Every ruler layer is hidden: click here to show the ruler panel", 12, r.y, getWidth() - 24, r.h,
                        juce::Justification::centredLeft, true);
        }
        else if (r.time) paintTimeLayer (g, r);
        else paintProportionLayer (g, r);
    }
    g.setColour (Theme::lineStrong);
    g.fillRect (0, rulerHeight() - 1, getWidth(), 1);
}

void TimelineView::paintTimeLayer (juce::Graphics& g, const LayerRow& row)
{
    const int y0 = row.y, h = row.h;
    g.setColour (juce::Colour (0xff18191c));
    g.fillRect (0, y0, getWidth(), h);
    g.setColour (Theme::line);
    g.fillRect (0, y0 + h - 1, getWidth(), 1);

    g.setColour (Theme::textMuted);
    g.setFont (Theme::font (11.0f));
    g.drawText (row.layer[IDs::name].toString(), 12, y0, headerWidth - 16, h, juce::Justification::centredLeft);

    // A step that leaves at least 70 px between labels.
    static const double steps[] = { 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1200 };
    double step = steps[std::size (steps) - 1];
    for (double s : steps) if (s * pps >= 70.0) { step = s; break; }
    const int decimals = step < 0.1 ? 2 : (step < 1.0 ? 1 : 0);

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (headerWidth, y0, getWidth() - headerWidth, h);

    const bool roomForLabels = h >= 20;
    const double end = xToTime ((float) getWidth());
    const double first = std::floor (viewStart / step) * step;
    for (double t = first; t <= end; t += step)
    {
        const float x = timeToX (t);
        g.setColour (Theme::lineStrong);
        const float top = roomForLabels ? (float) y0 + (float) h * 0.4f : (float) y0 + 2.0f;
        g.fillRect (x, top, 1.0f, (float) (y0 + h - 1) - top);
        if (roomForLabels)
        {
            g.setColour (Theme::text);
            g.setFont (Theme::mono (11.0f));
            g.drawText (TimeFormat::format (t, decimals), (int) x + 4, y0 + 2, 80, juce::jmin (16, h - 6), juce::Justification::centredLeft);
        }

        // Minor ticks
        g.setColour (Theme::line);
        for (int k = 1; k < 5; ++k)
        {
            const float mx = timeToX (t + step * k / 5.0);
            g.fillRect (mx, (float) (y0 + h) - 6.0f, 1.0f, 5.0f);
        }
    }
}

void TimelineView::paintProportionLayer (juce::Graphics& g, const LayerRow& row)
{
    const int y0 = row.y, h = row.h;
    const bool active = row.layer == project.getActiveLayer();
    g.setColour (layerBackground);
    g.fillRect (0, y0, getWidth(), h);
    g.setColour (Theme::line);
    g.fillRect (0, y0 + h - 1, getWidth(), 1);

    const auto ref = project.layerFrame (row.layer);
    const auto frame = project.frameFor (ref);
    const auto layerName = row.layer[IDs::name].toString();
    juce::String label = layerName.equalsIgnoreCase (project.frameName (ref)) ? layerName : layerName + ": " + project.frameName (ref);
    if (ref == "piece") label << " " << TimeFormat::format (frame.length, 0) << (project.hasSetPieceLength() ? "" : " (auto)");
    if (active)
    {
        g.setColour (Theme::accent);
        g.fillRect (5, y0 + h / 2 - 2, 4, 4);
    }
    g.setColour (active ? Theme::accent : Theme::accent.withAlpha (0.65f));
    g.setFont (Theme::font (h < 16 ? 9.5f : 10.5f));
    g.drawText (label + juce::String::fromUTF8 (" \xe2\x96\xbe"), 12, y0, headerWidth - 16, h, juce::Justification::centredLeft, true);

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (headerWidth, y0, getWidth() - headerWidth, h);

    // A span that is not this layer's frame: a faint bar.
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

    // Ticks: simpler fractions taller and brighter; series in their own colour.
    auto ticks = visibleTicks (row.layer);
    const float usable = (float) h - 3.0f;
    for (auto& k : ticks)
    {
        const float x = timeToX (k.time);
        if (k.series)
        {
            g.setColour (seriesColour.withAlpha (0.9f));
            g.fillRect (x, (float) y0 + 1.0f, 1.0f, (float) h - 2.0f);
            juce::Path diamond;
            diamond.addQuadrilateral (x + 0.5f, (float) (y0 + h) - 8.0f, x + 3.5f, (float) (y0 + h) - 5.0f,
                                      x + 0.5f, (float) (y0 + h) - 2.0f, x - 2.5f, (float) (y0 + h) - 5.0f);
            g.fillPath (diamond);
            continue;
        }
        const float th = usable * k.height;
        g.setColour (Theme::accent.withAlpha (k.division.rank <= 3.0 ? 0.95f : 0.55f));
        g.fillRect (x, (float) (y0 + h - 1) - th, 1.0f, th);
    }

    // Labels, never overlapping: halves and thirds, then series, then the rest.
    std::vector<juce::Range<int>> taken;
    auto tryLabel = [&] (double time, const juce::String& text, juce::Colour colour)
    {
        const int x = (int) timeToX (time) + 3;
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 2;
        const juce::Range<int> range (x - 4, x + w + 4);
        for (auto& t : taken) if (t.intersects (range)) return;
        taken.push_back (range);
        g.setColour (colour);
        g.drawText (text, x, y0 + 1, w, 10, juce::Justification::centredLeft, false);
    };

    // Marks first (they are the most important labels).
    const bool roomForLabels = h >= 15;
    for (auto m : project.getMarks())
    {
        if (m[IDs::frame].toString() != ref) continue;
        const double t = project.markTime (m);
        if (t < 0.0) continue;
        const float x = timeToX (t);
        g.setColour (Theme::accent);
        g.fillRect (x - 1.0f, (float) y0, 2.0f, (float) h - 1.0f);
        if (! roomForLabels) continue;
        g.setFont (Theme::font (10.0f, true));
        const auto name = m[IDs::name].toString();
        const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 8;
        juce::Rectangle<float> pill ((float) x + 2.0f, (float) y0 + 1.0f, (float) w, juce::jmin (13.0f, (float) h - 3.0f));
        g.fillRoundedRectangle (pill, 3.0f);
        g.setColour (Theme::background);
        g.drawText (name, pill, juce::Justification::centred, false);
        taken.push_back ({ (int) x - 2, (int) x + w + 6 });
    }

    if (roomForLabels)
    {
        g.setFont (Theme::mono (9.5f));
        std::vector<size_t> order;
        for (size_t i = 0; i < ticks.size(); ++i) if (ticks[i].labelled) order.push_back (i);
        auto key = [&] (size_t i) { return ticks[i].series ? 3.5 : ticks[i].division.rank; };
        std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b) { return key (a) < key (b); });
        for (auto i : order)
        {
            auto& k = ticks[i];
            if (k.series) tryLabel (k.time, k.seriesLabel, seriesColour);
            else tryLabel (k.time, k.division.label(), k.division.rank <= 3.0 ? Theme::accent : Theme::accent.withAlpha (0.7f));
        }
    }

    // The frame's edges (draggable for the piece and the span).
    g.setColour (Theme::accent);
    g.fillRect (fx0 - 1.0f, (float) y0, 2.0f, (float) h - 1.0f);
    g.fillRect (fx1 - 1.0f, (float) y0, 2.0f, (float) h - 1.0f);
}

void TimelineView::paintMarksInLanes (juce::Graphics& g)
{
    // Every mark whose frame is shown: a dashed line through the tracks.
    std::set<juce::String> shownFrames;
    for (auto& r : rulerRows()) if (r.layer.isValid() && ! r.time) shownFrames.insert (project.layerFrame (r.layer));

    const float top = (float) rulerHeight(), bottom = (float) getHeight();
    const float dashes[] = { 6.0f, 4.0f };
    for (auto m : project.getMarks())
    {
        if (shownFrames.count (m[IDs::frame].toString()) == 0) continue;
        const double t = project.markTime (m);
        if (t < 0.0) continue;
        const float x = timeToX (t);
        g.setColour (Theme::accent.withAlpha (m == dragMark ? 0.9f : 0.45f));
        g.drawDashedLine ({ x, top, x, bottom }, dashes, 2, 1.2f);
    }
}

//== Mouse =====================================================================

bool TimelineView::rulerMouseMove (const juce::MouseEvent& e)
{
    if (e.y >= rulerHeight()) return false;
    const auto row = layerAt (e.y);
    if (e.x < headerWidth)
    {
        const bool onEdge = row.layer.isValid() && std::abs (e.y - (row.y + row.h)) <= 3;
        setMouseCursor (onEdge ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::PointingHandCursor);
        return true;
    }
    if (row.layer.isValid() && ! row.time)
    {
        const auto f = project.frameFor (project.layerFrame (row.layer));
        const bool onEdge = f.isValid() && (std::abs (e.position.x - timeToX (f.start)) <= 5.0f || std::abs (e.position.x - timeToX (f.end())) <= 5.0f);
        if (markAt (row, e.position.x).isValid() || onEdge) { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); return true; }
    }
    setMouseCursor (juce::MouseCursor::IBeamCursor);
    return true;
}

bool TimelineView::rulerMouseDown (const juce::MouseEvent& e)
{
    if (e.y >= rulerHeight()) return false;
    const auto row = layerAt (e.y);

    if (! row.layer.isValid())
    {
        if (onShowRulerPanel) onShowRulerPanel ({});
        return true;
    }

    // The name column: resize from the lower edge, else the layer's menu.
    if (e.x < headerWidth)
    {
        if (std::abs (e.y - (row.y + row.h)) <= 3 && ! e.mods.isPopupMenu())
        {
            dragMode = DragMode::layerHeight;
            dragLayer = row.layer;
            dragLayerStartHeight = row.h;
            return true;
        }
        if (! row.time) project.setActiveLayer (row.layer);
        repaint();
        showLayerMenu (row.layer);
        return true;
    }

    if (row.time)
    {
        if (e.mods.isPopupMenu()) { showLayerMenu (row.layer); return true; }
        dragMode = DragMode::locate;
        if (onLocate) onLocate (juce::jmax (0.0, xToTime (e.position.x)));
        return true;
    }

    // A proportion layer: it becomes the active one.
    project.setActiveLayer (row.layer);
    repaint();
    auto mark = markAt (row, e.position.x);
    if (e.mods.isPopupMenu())
    {
        if (mark.isValid()) showMarkMenu (mark); else showLayerMenu (row.layer);
        return true;
    }

    if (mark.isValid())
    {
        dragMode = DragMode::mark;
        dragMark = mark;
        project.getUndoManager().beginNewTransaction ("Move mark");
        return true;
    }

    const auto ref = project.layerFrame (row.layer);
    const auto frame = project.frameFor (ref);
    const bool nearStart = frame.isValid() && std::abs (e.position.x - timeToX (frame.start)) <= 5.0f;
    const bool nearEnd = frame.isValid() && std::abs (e.position.x - timeToX (frame.end())) <= 5.0f;
    dragLayer = row.layer;

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
    return true;
}

bool TimelineView::rulerMouseDrag (const juce::MouseEvent& e)
{
    const bool snapping = ! e.mods.isShiftDown();
    const double t = juce::jmax (0.0, xToTime (e.position.x));

    if (dragMode == DragMode::layerHeight)
    {
        {
            const int h = juce::jlimit (minLayerHeight, maxLayerHeight, dragLayerStartHeight + e.getDistanceFromDragStartY());
            dragLayer.setProperty (IDs::height, h, nullptr);
            repaint();
            return true;
        }

    }
    if (dragMode == DragMode::span)
    {
        {
            if (e.getDistanceFromDragStart() < 3) return true;
            project.setSpan (spanAnchor, snap (t, {}, snapping));
            if (project.hasSpan() && dragLayer.isValid()) project.setLayerFrame (dragLayer, "span");
            repaint();
            return true;
        }

    }
    if (dragMode == DragMode::frameStart || dragMode == DragMode::frameEnd)
    {
        {
            const auto ref = project.layerFrame (dragLayer);
            const double v = snap (t, {}, snapping);
            if (ref == "piece") project.setPieceLength (juce::jmax (1.0, v));
            else if (ref == "span")
            {
                const auto f = project.frameFor ("span");
                if (dragMode == DragMode::frameStart) project.setSpan (juce::jmin (v, f.end() - 0.01), f.end());
                else project.setSpan (f.start, juce::jmax (v, f.start + 0.01));
            }
            repaint();
            return true;
        }

    }
    if (dragMode == DragMode::mark)
    {
        {
            const auto ref = dragMark[IDs::frame].toString();
            const auto f = project.frameFor (ref);
            if (! f.isValid()) return true;
            const double v = snap (t, {}, snapping);
            const double p = f.proportionOf (v);
            const auto old = project.evaluate (dragMark[IDs::expr].toString(), ref);
            if (! old.ok || std::abs (old.value - p) > 1.0e-9)
                dragMark.setProperty (IDs::expr, Proportion::expressionFor (p, project.getDenominators()), project.um());
            readout = dragMark[IDs::name].toString() + " " + Proportion::describe (p, project.getDenominators())
                      + " of " + project.frameName (ref) + "  " + TimeFormat::format (v);
            readoutPos = e.getPosition().withY (juce::jmax (e.y, rulerHeight() + 34));
            repaint();
            return true;
        }
    }
    return false;
}

bool TimelineView::rulerDoubleClick (const juce::MouseEvent& e)
{
    if (e.y >= rulerHeight()) return false;
    const auto row = layerAt (e.y);
    if (! row.layer.isValid() || row.time) return true;

    if (e.x < headerWidth)
    {
        if (onShowRulerPanel) onShowRulerPanel (row.layer[IDs::id].toString());
        return true;
    }

    // A new mark where you double-clicked (snapped).
    const auto ref = project.layerFrame (row.layer);
    const auto f = project.frameFor (ref);
    if (! f.isValid()) return true;
    const double v = snap (juce::jmax (0.0, xToTime (e.position.x)), {}, ! e.mods.isShiftDown());
    project.getUndoManager().beginNewTransaction ("New mark");
    auto mark = project.addMark (ref, Proportion::expressionFor (f.proportionOf (v), project.getDenominators()));
    repaint();
    askMarkName (mark);
    return true;
}

//== Menus =====================================================================

void TimelineView::showLayerMenu (juce::ValueTree layer)
{
    const bool time = Project::isTimeLayer (layer);
    const int h = (int) layer.getProperty (IDs::height, 24);
    auto selected = getSelectedBox();

    juce::PopupMenu size;
    size.addItem (1, "Small", true, sizeName (h) == "S");
    size.addItem (2, "Medium", true, sizeName (h) == "M");
    size.addItem (3, "Large", true, sizeName (h) == "L");

    juce::PopupMenu m;
    if (time)
    {
        m.addSectionHeader ("Time layer");
        m.addSubMenu ("Size", size);
        m.addItem (4, "Hide");
        m.addItem (5, "Add a proportion layer below");
        m.addSeparator();
        m.addItem (6, "Ruler layers panel...");
    }
    else
    {
        const auto ref = project.layerFrame (layer);
        const auto dens = project.layerDenominators (layer);
        const auto series = layer[IDs::series].toString();

        juce::PopupMenu frame;
        frame.addItem (10, "The piece (" + TimeFormat::format (project.getPieceLength(), 0) + (project.hasSetPieceLength() ? ")" : ", its current length)"), true, ref == "piece");
        frame.addItem (11, "The span dragged on a proportion layer", project.hasSpan(), ref == "span");
        if (selected.isValid())
            frame.addItem (12, "The selected box: " + selected[IDs::name].toString(), true, ref == selected[IDs::id].toString());
        else if (ref != "piece" && ref != "span")
            frame.addItem (12, "Box: " + project.frameName (ref), true, true);
        else
            frame.addItem (12, "A box (select one first)", false);

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

        const int golden = Proportion::seriesPart (series, "golden").getIntValue();
        const int fib = Proportion::seriesPart (series, "fibonacci").getIntValue();
        const int harmonic = Proportion::seriesPart (series, "harmonic").getIntValue();
        juce::PopupMenu seriesMenu;
        seriesMenu.addSectionHeader ("Golden section");
        seriesMenu.addItem (500, "Off", true, golden == 0);
        for (int lv = 1; lv <= 3; ++lv) seriesMenu.addItem (500 + lv, juce::String (lv) + (lv == 1 ? " level (0.382, 0.618)" : " levels"), true, golden == lv);
        seriesMenu.addSectionHeader ("Fibonacci");
        seriesMenu.addItem (510, "Off", true, fib == 0);
        for (int n : { 5, 8, 13, 21, 34, 55 }) seriesMenu.addItem (510 + n, "Parts of " + juce::String (n), true, fib == n);
        seriesMenu.addSectionHeader ("Harmonic (1/2, 1/3 ...)");
        seriesMenu.addItem (600, "Off", true, harmonic == 0);
        for (int n : { 4, 6, 8, 12, 16 }) seriesMenu.addItem (600 + n, "Up to 1/" + juce::String (n), true, harmonic == n);
        seriesMenu.addSeparator();
        seriesMenu.addItem (7, "Your own series: in the ruler panel...");

        m.addSectionHeader (layer[IDs::name].toString());
        m.addSubMenu ("Frame: " + project.frameName (ref), frame);
        m.addItem (13, "Set the piece length...");
        m.addItem (14, "Follow the piece's current length", project.hasSetPieceLength());
        m.addItem (15, "Remove the span", project.hasSpan());
        m.addSeparator();
        m.addSubMenu ("Divisions", divisions);
        m.addItem (16, "Finer divisions appear as you zoom in", true, (bool) layer.getProperty (IDs::autoDivisions, true));
        m.addSubMenu ("Series", seriesMenu);
        m.addItem (17, "Boxes and marks snap to this layer", true, (bool) layer.getProperty (IDs::snap, true));
        m.addSeparator();
        m.addItem (18, "New mark at the playhead");
        m.addItem (19, "Split the frame by a ratio into marks...");
        m.addSeparator();
        m.addSubMenu ("Size", size);
        m.addItem (4, "Hide");
        m.addItem (5, "Add a proportion layer below");
        m.addItem (8, "Delete this layer");
        m.addItem (6, "Ruler layers panel...");
    }

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, layer, selected] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->project;
        auto list = p.layerDenominators (layer);
        auto setList = [&] (juce::Array<double> l) { std::sort (l.begin(), l.end()); p.setLayerDenominators (layer, l); };
        auto setSeries = [&] (const juce::String& key, int v)
        {
            layer.setProperty (IDs::series, Proportion::withSeriesPart (layer[IDs::series].toString(), key, v > 0 ? juce::String (v) : juce::String()), nullptr);
        };

        if (r >= 1 && r <= 3) layer.setProperty (IDs::height, r == 1 ? sizeSmall : (r == 2 ? sizeMedium : sizeLarge), nullptr);
        else if (r == 4) layer.setProperty (IDs::visible, false, nullptr);
        else if (r == 5)
        {
            auto l = p.addLayer ("proportion", p.getRulers().indexOf (layer) + 1);
            p.setActiveLayer (l);
        }
        else if (r == 6 || r == 7) { if (safe->onShowRulerPanel) safe->onShowRulerPanel (layer[IDs::id].toString()); }
        else if (r == 8) p.removeLayer (layer);
        else if (r == 10) p.setLayerFrame (layer, "piece");
        else if (r == 11) p.setLayerFrame (layer, "span");
        else if (r == 12 && selected.isValid()) p.setLayerFrame (layer, selected[IDs::id].toString());
        else if (r == 13) safe->askPieceLength();
        else if (r == 14) { p.getUndoManager().beginNewTransaction ("Piece length"); p.setPieceLength (0.0); }
        else if (r == 15)
        {
            p.getUndoManager().beginNewTransaction ("Remove span");
            p.setSpan (0.0, 0.0);
        }
        else if (r == 16) layer.setProperty (IDs::autoDivisions, ! (bool) layer.getProperty (IDs::autoDivisions, true), nullptr);
        else if (r == 17) layer.setProperty (IDs::snap, ! (bool) layer.getProperty (IDs::snap, true), nullptr);
        else if (r == 18)
        {
            const auto ref = p.layerFrame (layer);
            const auto f = p.frameFor (ref);
            if (f.isValid())
            {
                p.getUndoManager().beginNewTransaction ("New mark");
                auto mark = p.addMark (ref, Proportion::expressionFor (f.proportionOf (safe->playhead), list));
                safe->askMarkName (mark);
            }
        }
        else if (r == 19) safe->askRatioSplit (p.layerFrame (layer), {});
        else if (r == 20) safe->askCustomDenominator (layer);
        else if (r == 21) setList ({ 2, 3, 4, 5, 6, 8 });
        else if (r == 22) setList ({ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 });
        else if (r == 23) setList (Project::defaultDenominators());
        else if (r == 24) setList ({ 2, 3, 5, 7, 11, 13, 17, 19 });
        else if (r == 25) setList ({});
        else if (r >= 200 && r <= 220)
        {
            const int i = Proportion::indexOf (list, r - 200.0);
            if (i >= 0) list.remove (i); else list.add (r - 200.0);
            setList (list);
        }
        else if (r >= 400 && r < 500) { list.remove (r - 400); setList (list); }
        else if (r >= 500 && r < 510) setSeries ("golden", r - 500);
        else if (r >= 510 && r < 600) setSeries ("fibonacci", r - 510);
        else if (r >= 600 && r < 700) setSeries ("harmonic", r - 600);
        safe->repaint();
    });
}

void TimelineView::showMarkMenu (juce::ValueTree mark)
{
    auto boxes = getSelectedBoxes();
    bool anyBound = false;
    for (auto b : boxes) anyBound = anyBound || project.isBound (b);
    const auto name = mark[IDs::name].toString();

    juce::PopupMenu m;
    m.addSectionHeader ("Mark " + name + " (" + mark[IDs::expr].toString() + " of " + project.frameName (mark[IDs::frame].toString()) + ")");
    m.addItem (1, "Rename...");
    m.addItem (2, "Set its proportion...");
    m.addItem (3, "Move it to the playhead");
    m.addSeparator();
    m.addItem (4, "Pin the selected boxes' start here", anyBound);
    m.addItem (5, "Pin the selected boxes' end here", anyBound);
    if (! anyBound) m.addItem (99, "(select boxes with proportions to pin them)", false);
    m.addSeparator();
    m.addItem (6, "Delete the mark");

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, mark, boxes, name] (int r) mutable
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->project;
        if (r == 1) safe->askMarkName (mark);
        else if (r == 2) safe->askMarkExpression (mark);
        else if (r == 3)
        {
            const auto ref = mark[IDs::frame].toString();
            const auto f = p.frameFor (ref);
            if (f.isValid())
            {
                p.getUndoManager().beginNewTransaction ("Move mark");
                mark.setProperty (IDs::expr, Proportion::expressionFor (f.proportionOf (safe->playhead), p.getDenominators()), p.um());
            }
        }
        else if (r == 4 || r == 5)
        {
            p.getUndoManager().beginNewTransaction ("Pin to mark");
            for (auto b : boxes)
                if (p.isBound (b))
                {
                    if (! Project::isLocked (b)) p.setLock (b, true);
                    p.pinEdge (b, r == 4, name);
                }
        }
        else if (r == 6) { p.getUndoManager().beginNewTransaction ("Delete mark"); p.removeMark (mark); }
        safe->repaint();
    });
}

//== Dialogs ===================================================================

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

void TimelineView::askCustomDenominator (juce::ValueTree layer)
{
    auto* w = new juce::AlertWindow ("Your own denominator",
                                     "Divides the frame into this many parts. A decimal such as 7.5 gives 7 full parts and a half part at the end.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("d", "7.5");
    w->addButton ("Add", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TimelineView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, layer] (int result)
    {
        if (safe == nullptr || result != 1) return;
        const auto r = Proportion::parse (w->getTextEditorContents ("d"));
        if (! r.ok || r.value <= 1.0 || r.value > 1000.0)
        {
            if (safe->onStatus) safe->onStatus ("A denominator is a number above 1, for example 7.5.", true);
            return;
        }
        auto list = safe->project.layerDenominators (layer);
        if (Proportion::indexOf (list, r.value) < 0) list.add (r.value);
        std::sort (list.begin(), list.end());
        safe->project.setLayerDenominators (layer, list);
        safe->repaint();
    }), true);
}

void TimelineView::askMarkName (juce::ValueTree mark)
{
    auto* w = new juce::AlertWindow ("Mark name",
                                     "Box proportions can use it: start \"a1\", length \"a2 - a1\". Letters, digits and _.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", mark[IDs::name].toString());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    if (auto* ed = w->getTextEditor ("name")) ed->selectAll();
    juce::Component::SafePointer<TimelineView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, mark] (int result) mutable
    {
        if (safe == nullptr || result != 1) return;
        juce::String error;
        safe->project.getUndoManager().beginNewTransaction ("Rename mark");
        if (! safe->project.renameMark (mark, w->getTextEditorContents ("name").trim(), error) && safe->onStatus)
            safe->onStatus (error, true);
        safe->repaint();
    }), true);
}

void TimelineView::askMarkExpression (juce::ValueTree mark)
{
    auto* w = new juce::AlertWindow ("Mark " + mark[IDs::name].toString(),
                                     "Its place as a proportion of " + project.frameName (mark[IDs::frame].toString())
                                         + ": 3/11, golden, 2/3 of 3/4, another mark's name + 1/12 ...",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("expr", mark[IDs::expr].toString());
    w->addButton ("Set", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TimelineView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, mark] (int result) mutable
    {
        if (safe == nullptr || result != 1) return;
        juce::String error;
        safe->project.getUndoManager().beginNewTransaction ("Mark proportion");
        if (! safe->project.setMarkExpression (mark, w->getTextEditorContents ("expr"), error) && safe->onStatus)
            safe->onStatus (error, true);
        safe->repaint();
    }), true);
}

void TimelineView::askRatioSplit (const juce::String& frameRef, juce::ValueTree boxToCut)
{
    const bool box = boxToCut.isValid();
    auto* w = new juce::AlertWindow (box ? "Split \"" + boxToCut[IDs::name].toString() + "\" by a ratio" : "Split the frame by a ratio",
                                     juce::String (box ? "Cut the box into parts, or put marks in it, " : "Put marks in " + project.frameName (frameRef) + " ")
                                         + "with these ratios. For example 3:8, 1:1:1, 2:3:5:8 (decimals allowed).",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("parts", "2:3:5:8");
    w->addTextEditor ("prefix", box ? boxToCut[IDs::name].toString().retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_").substring (0, 8) : juce::String ("r"),
                      "Names of the marks start with");
    if (box) w->addButton ("Cut the box", 2);
    w->addButton (box ? "Marks in the box" : "Make marks", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TimelineView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, frameRef, boxToCut] (int result) mutable
    {
        if (safe == nullptr || result == 0) return;
        auto& p = safe->project;
        const auto parts = Proportion::parseParts (w->getTextEditorContents ("parts"));
        if (parts.isEmpty())
        {
            if (safe->onStatus) safe->onStatus ("Write the parts with colons, for example 2:3:5:8.", true);
            return;
        }
        if (result == 2)
        {
            p.getUndoManager().beginNewTransaction ("Cut by ratio");
            safe->setSelection (p.cutBoxByRatio (boxToCut, parts));
        }
        else
        {
            const auto ref = boxToCut.isValid() ? boxToCut[IDs::id].toString() : frameRef;
            auto prefix = w->getTextEditorContents ("prefix").trim();
            if (! Proportion::isValidName (prefix + "1")) prefix = "r";
            p.getUndoManager().beginNewTransaction ("Split by ratio");
            auto marks = p.splitFrameByRatio (ref, parts, prefix);
            if (safe->onStatus) safe->onStatus ("Made " + juce::String (marks.size()) + " marks in " + p.frameName (ref) + ".", false);
        }
        safe->repaint();
    }), true);
}
