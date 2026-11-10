// TimelineDrawn.cpp — stage 4d.2: what you draw by hand.
//
// A drawn lane (Thoresen's dynamic form) holds spans (form-building
// functions, with an emphasis and, for forward spans and deltas, a goal) and
// points (accents, with one or more functions). Drag in the lane to draw a
// span, click for an accent; drag an item to move it, its edges to resize
// it, a delta's handle to move its goal; right-click it for its kinds.
//
// Nodes (Vande Gorne) sit on folders: drag on a folder's lane to place one.
// They are drawn as narrow bands across the folder's tracks and the lanes,
// and flagged when their length does not balance the stretch before them.

#include "TimelineView.h"
#include "Theme.h"
#include "TimeFormat.h"
#include "../Model/Lanes.h"

namespace
{
    constexpr float edgeGrab = 5.0f;
    constexpr double defaultNodeLength = 0.5;

    juce::String setTag (const juce::String& tags, const juce::String& tag, bool on)
    {
        auto list = juce::StringArray::fromTokens (tags, ",", {});
        list.trim();
        list.removeEmptyStrings();
        if (on) list.addIfNotAlreadyThere (tag); else list.removeString (tag);
        return list.joinIntoString (",");
    }
}

//== Selection =================================================================

juce::ValueTree TimelineView::getSelectedItem()     { return project.findItem (selectedItemId); }
juce::ValueTree TimelineView::getSelectedNode()     { return project.findNode (selectedNodeId); }

void TimelineView::selectDrawn (const juce::String& itemId, const juce::String& nodeId)
{
    selectBox ({});
    if (itemId == selectedItemId && nodeId == selectedNodeId) { repaint(); return; }
    selectedItemId = itemId;
    selectedNodeId = nodeId;
    selectionChanged();
    repaint();
}

void TimelineView::deleteDrawnSelection()
{
    auto item = getSelectedItem();
    auto node = getSelectedNode();
    if (! item.isValid() && ! node.isValid()) return;
    project.getUndoManager().beginNewTransaction (item.isValid() ? "Delete from the lane" : "Delete node");
    if (item.isValid()) project.removeItem (item);
    if (node.isValid()) project.removeNode (node);
    selectDrawn ({}, {});
}

//== Drawn lanes: geometry and painting ========================================

TimelineView::DrawnGeometry TimelineView::drawnGeometry (const LayerRow& row, int rows) const
{
    DrawnGeometry g;
    g.pointsTop = (float) row.y + 1.0f;
    g.pointsH = juce::jlimit (9.0f, 16.0f, (float) row.h * 0.32f);
    g.rowsTop = g.pointsTop + g.pointsH + 1.0f;
    g.rows = juce::jmax (1, rows);
    g.rowH = juce::jmax (4.0f, ((float) (row.y + row.h) - 2.0f - g.rowsTop) / (float) g.rows);
    return g;
}

juce::Rectangle<float> TimelineView::spanRect (const DrawnGeometry& g, const juce::ValueTree& span, int row) const
{
    const float x0 = timeToX ((double) span[IDs::start]), x1 = timeToX ((double) span[IDs::end]);
    return { x0, g.rowsTop + (float) row * g.rowH + 1.5f, juce::jmax (2.0f, x1 - x0), juce::jmax (3.0f, g.rowH - 3.0f) };
}

void TimelineView::paintSpan (juce::Graphics& g, const Framework::Lane& lane, const juce::ValueTree& span, juce::Rectangle<float> r, bool selected)
{
    // After Thoresen's figures 8.1-8.5: ramps on a baseline, boxes, crossed
    // boxes, a triangle for a delta; the emphasis on the line carrying the
    // tendency only (the slope, the top of a box); goal marks above the
    // high corner (figure 8.3).
    const auto* fnValue = [&]() -> const Framework::Value*
    {
        auto* fn = framework.findCategory (lane.spans);
        return fn != nullptr ? framework.findValue (*fn, span[IDs::value].toString()) : nullptr;
    }();
    const auto shape = fnValue != nullptr ? fnValue->shape : juce::String ("presence");
    juce::String stroke = "solid";
    if (auto* em = framework.findCategory (lane.emphasis))
    {
        auto e = span[IDs::emphasis].toString();
        if (e.isEmpty()) e = em->defaultValue;
        if (auto* v = framework.findValue (*em, e)) stroke = v->stroke;
    }
    float size = 1.0f;
    if (auto* hc = framework.findCategory (lane.height))
    {
        auto h = span[IDs::size].toString();
        if (h.isEmpty()) h = hc->defaultValue;
        if (auto* v = framework.findValue (*hc, h); v != nullptr && v->size > 0.0f) size = v->size;
    }
    const Framework::Value* goalValue = nullptr;
    if (auto* goal = framework.findCategory (lane.goal); goal != nullptr && lane.goalOn.contains (span[IDs::value].toString()))
        goalValue = framework.findValue (*goal, span[IDs::goal].toString());

    const float markH = lane.goal.isNotEmpty() ? juce::jlimit (5.0f, 10.0f, r.getHeight() * 0.32f) : 0.0f;
    auto area = r.withTrimmedTop (markH);
    area = area.withTop (area.getBottom() - area.getHeight() * size);
    const float l = area.getX(), rt = area.getRight(), top = area.getY(), bottom = area.getBottom(), w = area.getWidth();
    const float gx = l + (float) juce::jlimit (0.0, 1.0, (double) span.getProperty (IDs::goalAt, 0.5)) * w;
    const float ge = juce::jmax (gx, l + (float) juce::jlimit (0.0, 1.0, (double) span.getProperty (IDs::goalEnd, juce::jmin (1.0, (double) span.getProperty (IDs::goalAt, 0.5) + 0.3))) * w);

    juce::Path frame, tendency, dashedFrame;
    juce::Array<float> goalsAt;                 // where goal marks go (on the top line)
    const float early = goalValue != nullptr ? goalValue->early : -1.0f;
    auto rise = [&] (float peak)
    {
        // The rising slope up to `peak`; a blunted goal arrives early, with a
        // second, dashed slope arriving at the peak.
        if (early > 0.0f)
        {
            const float xe = l + early * (peak - l);
            tendency.startNewSubPath (l, bottom); tendency.lineTo (xe, top); tendency.lineTo (peak, top);
            dashedFrame.startNewSubPath (l, bottom); dashedFrame.lineTo (peak, top);
            goalsAt.add (xe);
        }
        else { tendency.startNewSubPath (l, bottom); tendency.lineTo (peak, top); }
        goalsAt.add (peak);
    };

    if (shape == "forward")
    {
        rise (rt);
        frame.startNewSubPath (rt, top); frame.lineTo (rt, bottom); frame.lineTo (l, bottom);
    }
    else if (shape == "backward")
    {
        tendency.startNewSubPath (l, top); tendency.lineTo (rt, bottom);
        frame.startNewSubPath (l, top); frame.lineTo (l, bottom); frame.lineTo (rt, bottom);
    }
    else if (shape == "delta" || shape == "plateau")
    {
        const float peakEnd = shape == "plateau" ? ge : gx;
        rise (gx);
        tendency.startNewSubPath (gx, top); tendency.lineTo (peakEnd, top); tendency.lineTo (rt, bottom);
        frame.startNewSubPath (l, bottom); frame.lineTo (rt, bottom);
    }
    else if (shape == "neutral")
    {
        dashedFrame.addRectangle (area);
        dashedFrame.startNewSubPath (l, top); dashedFrame.lineTo (rt, bottom);
        dashedFrame.startNewSubPath (l, bottom); dashedFrame.lineTo (rt, top);
    }
    else
    {
        tendency.startNewSubPath (l, top); tendency.lineTo (rt, top);
        frame.startNewSubPath (l, top); frame.lineTo (l, bottom); frame.lineTo (rt, bottom); frame.lineTo (rt, top);
        if (shape == "wavering")
        {
            frame.startNewSubPath (l, top); frame.lineTo (rt, bottom);
            frame.startNewSubPath (l, bottom); frame.lineTo (rt, top);
        }
    }

    const auto colour = selected ? Theme::accent : Theme::signs;
    const float dashes[] = { 4.0f, 3.0f };
    auto dashed = [&] (const juce::Path& path, float width)
    {
        juce::Path out;
        juce::PathStrokeType (width).createDashedStroke (out, path, dashes, 2);
        g.fillPath (out);
    };
    g.setColour (colour.withAlpha (0.95f));
    g.strokePath (frame, juce::PathStrokeType (1.1f));
    dashed (dashedFrame, 1.1f);
    if (stroke == "dashed")     dashed (tendency, 1.2f);
    else                        g.strokePath (tendency, juce::PathStrokeType (stroke == "bold" ? 2.8f : 1.3f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    if (goalValue != nullptr && (goalValue->sign.isNotEmpty() || goalValue->signFill.isNotEmpty()))
    {
        const float k = juce::jmax (markH, 6.0f) / 10.0f;
        for (auto x : goalsAt)
        {
            const auto t = juce::AffineTransform::scale (k).translated (x, top);
            if (goalValue->sign.isNotEmpty())
            {
                auto sign = juce::Drawable::parseSVGPath (goalValue->sign);
                g.strokePath (sign, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            }
            if (goalValue->signFill.isNotEmpty()) g.fillPath (juce::Drawable::parseSVGPath (goalValue->signFill), t);
        }
    }

    if (selected && (shape == "delta" || shape == "plateau"))
    {
        g.setColour (Theme::accent);
        for (auto x : { gx, shape == "plateau" ? ge : -1.0f })
        {
            if (x < 0.0f) continue;
            juce::Path handle;
            handle.addTriangle (x - 4.0f, r.getBottom() + 1.5f, x + 4.0f, r.getBottom() + 1.5f, x, r.getBottom() - 3.5f);
            g.fillPath (handle);
        }
    }
}

void TimelineView::paintDrawnLane (juce::Graphics& g, const LayerRow& row, const Framework::Lane& lane)
{
    int rows = 1;
    const auto placed = Lanes::stack (project, lane.key(), rows);
    const auto geo = drawnGeometry (row, rows);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (headerWidth, row.y, getWidth() - headerWidth, row.h);

    // A thin line between the accents and the spans.
    g.setColour (Theme::line.withAlpha (0.5f));
    g.fillRect ((float) headerWidth, geo.rowsTop - 0.5f, (float) (getWidth() - headerWidth), 1.0f);

    for (auto& p : placed)
    {
        const auto r = spanRect (geo, p.item, p.row);
        if (r.getRight() < (float) headerWidth || r.getX() > (float) getWidth()) continue;
        paintSpan (g, lane, p.item, r, p.item[IDs::id].toString() == selectedItemId);
    }

    // The span being drawn.
    if (dragMode == DragMode::drawSpan && drawLayer == row.layer)
    {
        const float x0 = timeToX (juce::jmin (drawStart, drawEnd)), x1 = timeToX (juce::jmax (drawStart, drawEnd));
        g.setColour (Theme::accent.withAlpha (0.25f));
        g.fillRect (x0, geo.rowsTop, juce::jmax (1.0f, x1 - x0), (float) (row.y + row.h - 2) - geo.rowsTop);
    }

    // Accents: the weighted point (a filled triangle) with each function's mark (figure 8.4).
    auto* accents = framework.findCategory (lane.points);
    const auto base = juce::Drawable::parseSVGPath (lane.pointBase.isNotEmpty() ? lane.pointBase : juce::String ("M5.5 6 L14.5 6 L10 14 Z"));
    for (auto item : project.drawnItems (lane.key()))
    {
        if (Project::isSpan (item)) continue;
        const float x = timeToX ((double) item[IDs::start]);
        if (x < (float) headerWidth - 12.0f || x > (float) getWidth() + 12.0f) continue;
        const bool selected = item[IDs::id].toString() == selectedItemId;
        const auto colour = selected ? Theme::accent : Theme::signs;
        juce::Path strokes, fills = base;
        if (accents != nullptr)
            for (auto& tag : juce::StringArray::fromTokens (item[IDs::value].toString(), ",", {}))
                if (auto* v = framework.findValue (*accents, tag.trim()))
                {
                    if (v->sign.isNotEmpty()) strokes.addPath (juce::Drawable::parseSVGPath (v->sign));
                    if (v->signFill.isNotEmpty()) fills.addPath (juce::Drawable::parseSVGPath (v->signFill));
                }
        const float k = geo.pointsH / 20.0f;
        const auto t = juce::AffineTransform::scale (k).translated (x - 10.0f * k, geo.pointsTop);
        g.setColour (colour.withAlpha (0.15f));
        g.fillRect (x - 0.5f, geo.rowsTop, 1.0f, (float) (row.y + row.h - 2) - geo.rowsTop);
        g.setColour (colour);
        g.fillPath (fills, t);
        g.strokePath (strokes, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
    }

    if (project.drawnItems (lane.key()).isEmpty())
    {
        g.setColour (Theme::textMuted.withAlpha (0.7f));
        g.setFont (Theme::font (10.5f));
        g.drawText ("Drag to draw a span (a form-building function); click for an accent.",
                    headerWidth + 8, row.y, getWidth() - headerWidth - 16, row.h, juce::Justification::centredLeft, true);
    }
}

//== Drawn lanes: mouse =========================================================

bool TimelineView::drawnMouseDown (const LayerRow& row, const Framework::Lane& lane, const juce::MouseEvent& e)
{
    if (e.x < headerWidth) return false;
    int rows = 1;
    const auto placed = Lanes::stack (project, lane.key(), rows);
    const auto geo = drawnGeometry (row, rows);
    const auto pos = e.position;

    // An accent?
    if (pos.y < geo.rowsTop)
        for (auto item : project.drawnItems (lane.key()))
            if (! Project::isSpan (item) && std::abs (timeToX ((double) item[IDs::start]) - pos.x) <= 6.0f)
            {
                selectDrawn (item[IDs::id].toString(), {});
                if (e.mods.isPopupMenu()) { showItemMenu (item, lane); return true; }
                project.getUndoManager().beginNewTransaction ("Move accent");
                dragItem = item;
                dragFrom = (double) item[IDs::start];
                dragMode = DragMode::itemMove;
                return true;
            }

    // A span?
    for (auto& p : placed)
    {
        const auto r = spanRect (geo, p.item, p.row);
        if (! r.expanded (edgeGrab, 0.0f).contains (pos)) continue;
        selectDrawn (p.item[IDs::id].toString(), {});
        if (e.mods.isPopupMenu()) { showItemMenu (p.item, lane); return true; }
        project.getUndoManager().beginNewTransaction ("Change span");
        dragItem = p.item;
        dragFrom = (double) p.item[IDs::start];
        dragTo = (double) p.item[IDs::end];
        const double goalAt = juce::jlimit (0.0, 1.0, (double) p.item.getProperty (IDs::goalAt, 0.5));
        const float gx = r.getX() + (float) goalAt * r.getWidth();
        const float ge = r.getX() + (float) juce::jlimit (goalAt, 1.0, (double) p.item.getProperty (IDs::goalEnd, juce::jmin (1.0, goalAt + 0.3))) * r.getWidth();
        juce::String shape;
        if (auto* fn = framework.findCategory (lane.spans))
            if (auto* v = framework.findValue (*fn, p.item[IDs::value].toString())) shape = v->shape;
        const bool peaked = shape == "delta" || shape == "plateau";
        const bool low = pos.y > r.getCentreY();
        if (shape == "plateau" && low && std::abs (pos.x - ge) <= edgeGrab && std::abs (pos.x - ge) < std::abs (pos.x - gx)) dragMode = DragMode::itemGoalEnd;
        else if (peaked && low && std::abs (pos.x - gx) <= edgeGrab) dragMode = DragMode::itemGoal;
        else if (std::abs (pos.x - r.getX()) <= edgeGrab) dragMode = DragMode::itemStart;
        else if (std::abs (pos.x - r.getRight()) <= edgeGrab) dragMode = DragMode::itemEnd;
        else dragMode = DragMode::itemMove;
        return true;
    }

    if (e.mods.isPopupMenu()) return false;     // the lane's own menu
    selectDrawn ({}, {});
    const double t = snap (juce::jmax (0.0, xToTime (pos.x)), {}, ! e.mods.isShiftDown());
    drawLayer = row.layer;
    drawStart = drawEnd = t;
    dragMode = DragMode::drawSpan;
    return true;
}

bool TimelineView::drawnMouseDrag (const juce::MouseEvent& e)
{
    const bool snapping = ! e.mods.isShiftDown();
    const double t = juce::jmax (0.0, xToTime (e.position.x));
    const double dt = t - juce::jmax (0.0, xToTime ((float) e.getMouseDownX()));
    if (dragMode == DragMode::drawSpan || dragMode == DragMode::drawNode)
    {
        drawEnd = snap (t, {}, snapping);
        repaint();
        return true;
    }
    if (dragMode == DragMode::itemMove)
    {
        if (! dragItem.isValid()) return true;
        double s = snap (juce::jmax (0.0, dragFrom + dt), {}, snapping);
        if (Project::isSpan (dragItem))
        {
            const double len = dragTo - dragFrom;
            // The end may snap instead of the start.
            const double se = snap (dragFrom + dt + len, {}, snapping);
            if (std::abs (se - (dragFrom + dt + len)) > 1.0e-9 && std::abs (s - (dragFrom + dt)) < 1.0e-9) s = se - len;
            s = juce::jmax (0.0, s);
            dragItem.setProperty (IDs::start, s, project.um());
            dragItem.setProperty (IDs::end, s + len, project.um());
        }
        else
            dragItem.setProperty (IDs::start, s, project.um());
        repaint();
        return true;
    }
    if (dragMode == DragMode::itemStart)
    {
        dragItem.setProperty (IDs::start, juce::jmin (snap (juce::jmax (0.0, dragFrom + dt), {}, snapping), dragTo - 0.01), project.um());
        repaint();
        return true;
    }
    if (dragMode == DragMode::itemEnd)
    {
        dragItem.setProperty (IDs::end, juce::jmax (snap (dragTo + dt, {}, snapping), dragFrom + 0.01), project.um());
        repaint();
        return true;
    }
    if (dragMode == DragMode::itemGoal || dragMode == DragMode::itemGoalEnd)
    {
        // A delta's peak; a plateau's start and end (kept in order).
        const double s = dragItem[IDs::start], en = dragItem[IDs::end];
        if (en <= s) return true;
        const double at = juce::jlimit (0.0, 1.0, (snap (t, {}, snapping) - s) / (en - s));
        const double from = juce::jlimit (0.0, 1.0, (double) dragItem.getProperty (IDs::goalAt, 0.5));
        const double to = juce::jlimit (from, 1.0, (double) dragItem.getProperty (IDs::goalEnd, juce::jmin (1.0, from + 0.3)));
        if (dragMode == DragMode::itemGoal)
        {
            dragItem.setProperty (IDs::goalAt, juce::jmin (at, dragItem.hasProperty (IDs::goalEnd) ? to : 1.0), project.um());
            if (onStatus) onStatus ("Peak at " + juce::String (juce::roundToInt (100.0 * (double) dragItem[IDs::goalAt])) + "% of the span", false);
        }
        else
        {
            if (! dragItem.hasProperty (IDs::goalEnd)) dragItem.setProperty (IDs::goalEnd, to, project.um());
            dragItem.setProperty (IDs::goalEnd, juce::jmax (at, from), project.um());
            if (onStatus) onStatus ("The plateau ends at " + juce::String (juce::roundToInt (100.0 * (double) dragItem[IDs::goalEnd])) + "% of the span", false);
        }
        repaint();
        return true;
    }
    if (dragMode == DragMode::nodeMove || dragMode == DragMode::nodeStart || dragMode == DragMode::nodeEnd)
    {
        if (! dragNode.isValid()) return true;
        const double s0 = dragFrom, len0 = dragTo;
        if (dragMode == DragMode::nodeMove)
            dragNode.setProperty (IDs::start, snap (juce::jmax (0.0, s0 + dt), {}, snapping), project.um());
        else if (dragMode == DragMode::nodeStart)
        {
            const double s = juce::jmin (snap (juce::jmax (0.0, s0 + dt), {}, snapping), s0 + len0 - 0.005);
            dragNode.setProperty (IDs::start, s, project.um());
            dragNode.setProperty (IDs::length, s0 + len0 - s, project.um());
        }
        else
            dragNode.setProperty (IDs::length, juce::jmax (0.005, snap (s0 + len0 + dt, {}, snapping) - s0), project.um());
        repaint();
        return true;
    }
    return false;
}

bool TimelineView::drawnMouseUp (const juce::MouseEvent& e)
{
    const auto mode = dragMode;
    if (mode == DragMode::drawSpan)
    {
        dragMode = DragMode::none;
        auto* lane = framework.findLane (drawLayer[IDs::lane].toString());
        const double a = juce::jmin (drawStart, drawEnd), b = juce::jmax (drawStart, drawEnd);
        repaint();
        if (lane == nullptr) return true;
        if (std::abs (timeToX (b) - timeToX (a)) >= 4.0f && lane->spans.isNotEmpty()) showNewSpanMenu (*lane, a, b);
        else if (lane->points.isNotEmpty()) showNewPointMenu (*lane, snap (juce::jmax (0.0, xToTime (e.position.x)), {}, ! e.mods.isShiftDown()));
        return true;
    }
    if (mode == DragMode::drawNode)
    {
        dragMode = DragMode::none;
        const double a = juce::jmin (drawStart, drawEnd), b = juce::jmax (drawStart, drawEnd);
        if (drawFolder.isValid() && std::abs (timeToX (b) - timeToX (a)) >= 3.0f)
        {
            project.getUndoManager().beginNewTransaction ("Add node");
            auto n = project.addNode (drawFolder, a, b - a);
            selectDrawn ({}, n[IDs::id].toString());
        }
        repaint();
        return true;
    }
    if (mode == DragMode::itemMove || mode == DragMode::itemStart || mode == DragMode::itemEnd || mode == DragMode::itemGoal || mode == DragMode::itemGoalEnd
        || mode == DragMode::nodeMove || mode == DragMode::nodeStart || mode == DragMode::nodeEnd)
    {
        dragMode = DragMode::none;
        dragItem = {};
        dragNode = {};
        selectionChanged();         // the panel follows a node's new place
        repaint();
        return true;
    }
    return false;
}

//== Drawn lanes: menus =========================================================

void TimelineView::showNewSpanMenu (const Framework::Lane& lane, double a, double b)
{
    auto* fn = framework.findCategory (lane.spans);
    if (fn == nullptr) return;
    juce::PopupMenu m;
    m.addSectionHeader (framework.text (fn->name) + ", " + TimeFormat::format (a) + " - " + TimeFormat::format (b));
    const auto& values = framework.valuesOf (*fn);
    for (int i = 0; i < values.size(); ++i) m.addItem (100 + i, framework.valueName (*fn, values.getReference (i).id));
    juce::Component::SafePointer<TimelineView> safe (this);
    const auto key = lane.key();
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, key, a, b] (int r)
    {
        if (safe == nullptr || r < 100) return;
        auto* l = safe->framework.findLane (key);
        auto* c = l != nullptr ? safe->framework.findCategory (l->spans) : nullptr;
        if (c == nullptr || ! juce::isPositiveAndBelow (r - 100, safe->framework.valuesOf (*c).size())) return;
        safe->project.getUndoManager().beginNewTransaction ("Draw span");
        auto span = safe->project.addSpan (key, a, b, safe->framework.valuesOf (*c).getReference (r - 100).id);
        safe->selectDrawn (span[IDs::id].toString(), {});
    });
}

void TimelineView::showNewPointMenu (const Framework::Lane& lane, double t)
{
    auto* acc = framework.findCategory (lane.points);
    if (acc == nullptr) return;
    juce::PopupMenu m;
    m.addSectionHeader (framework.text (acc->name) + ", " + TimeFormat::format (t));
    const auto& values = framework.valuesOf (*acc);
    for (int i = 0; i < values.size(); ++i) m.addItem (100 + i, framework.valueName (*acc, values.getReference (i).id));
    juce::Component::SafePointer<TimelineView> safe (this);
    const auto key = lane.key();
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, key, t] (int r)
    {
        if (safe == nullptr || r < 100) return;
        auto* l = safe->framework.findLane (key);
        auto* c = l != nullptr ? safe->framework.findCategory (l->points) : nullptr;
        if (c == nullptr || ! juce::isPositiveAndBelow (r - 100, safe->framework.valuesOf (*c).size())) return;
        safe->project.getUndoManager().beginNewTransaction ("Add accent");
        auto point = safe->project.addPoint (key, t, safe->framework.valuesOf (*c).getReference (r - 100).id);
        safe->selectDrawn (point[IDs::id].toString(), {});
    });
}

void TimelineView::showItemMenu (juce::ValueTree item, const Framework::Lane& lane)
{
    juce::PopupMenu m;
    const bool span = Project::isSpan (item);
    if (span)
    {
        auto* fn = framework.findCategory (lane.spans);
        auto* em = framework.findCategory (lane.emphasis);
        auto* goal = framework.findCategory (lane.goal);
        const auto kind = item[IDs::value].toString();
        m.addSectionHeader ((fn != nullptr ? framework.valueName (*fn, kind) : kind) + ", " + TimeFormat::format ((double) item[IDs::start])
                            + " - " + TimeFormat::format ((double) item[IDs::end]));
        if (fn != nullptr)
        {
            juce::PopupMenu sub;
            const auto& values = framework.valuesOf (*fn);
            for (int i = 0; i < values.size(); ++i) sub.addItem (100 + i, framework.valueName (*fn, values.getReference (i).id), true, values.getReference (i).id == kind);
            m.addSubMenu (framework.text (fn->name), sub);
        }
        if (em != nullptr)
        {
            juce::PopupMenu sub;
            auto cur = item[IDs::emphasis].toString();
            if (cur.isEmpty()) cur = em->defaultValue;
            const auto& values = framework.valuesOf (*em);
            for (int i = 0; i < values.size(); ++i) sub.addItem (200 + i, framework.valueName (*em, values.getReference (i).id), true, values.getReference (i).id == cur);
            m.addSubMenu (framework.text (em->name), sub);
        }
        if (auto* hc = framework.findCategory (lane.height))
        {
            juce::PopupMenu sub;
            auto cur = item[IDs::size].toString();
            if (cur.isEmpty()) cur = hc->defaultValue;
            const auto& values = framework.valuesOf (*hc);
            for (int i = 0; i < values.size(); ++i) sub.addItem (400 + i, framework.valueName (*hc, values.getReference (i).id), true, values.getReference (i).id == cur);
            m.addSubMenu (framework.text (hc->name), sub);
        }
        if (goal != nullptr && lane.goalOn.contains (kind))
        {
            juce::PopupMenu sub;
            const auto cur = item[IDs::goal].toString();
            sub.addItem (399, "None", true, cur.isEmpty());
            const auto& values = framework.valuesOf (*goal);
            for (int i = 0; i < values.size(); ++i) sub.addItem (300 + i, framework.valueName (*goal, values.getReference (i).id), true, values.getReference (i).id == cur);
            m.addSubMenu (framework.text (goal->name), sub);
        }
    }
    else if (auto* acc = framework.findCategory (lane.points))
    {
        const auto tags = juce::StringArray::fromTokens (item[IDs::value].toString(), ",", {});
        m.addSectionHeader (framework.text (acc->name) + ", " + TimeFormat::format ((double) item[IDs::start]) + " (several can combine)");
        const auto& values = framework.valuesOf (*acc);
        for (int i = 0; i < values.size(); ++i)
            m.addItem (100 + i, framework.valueName (*acc, values.getReference (i).id), true, tags.contains (values.getReference (i).id));
    }
    m.addSeparator();
    m.addItem (1, "Delete");

    juce::Component::SafePointer<TimelineView> safe (this);
    const auto key = lane.key();
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, item, key, span] (int r) mutable
    {
        if (safe == nullptr || r <= 0 || ! item.getParent().isValid()) return;
        auto* l = safe->framework.findLane (key);
        if (l == nullptr) return;
        auto& fw = safe->framework;
        auto& um = safe->project.getUndoManager();
        if (r == 1) { safe->selectDrawn (item[IDs::id].toString(), {}); safe->deleteDrawnSelection(); return; }
        auto pick = [&] (const juce::String& categoryKey, int index) -> juce::String
        {
            auto* c = fw.findCategory (categoryKey);
            return c != nullptr && juce::isPositiveAndBelow (index, fw.valuesOf (*c).size()) ? fw.valuesOf (*c).getReference (index).id : juce::String();
        };
        if (span)
        {
            if (r >= 100 && r < 200) { um.beginNewTransaction ("Function"); item.setProperty (IDs::value, pick (l->spans, r - 100), &um); }
            else if (r >= 200 && r < 300) { um.beginNewTransaction ("Emphasis"); item.setProperty (IDs::emphasis, pick (l->emphasis, r - 200), &um); }
            else if (r == 399) { um.beginNewTransaction ("Goal"); item.removeProperty (IDs::goal, &um); }
            else if (r >= 300 && r < 399) { um.beginNewTransaction ("Goal"); item.setProperty (IDs::goal, pick (l->goal, r - 300), &um); }
            else if (r >= 400 && r < 500) { um.beginNewTransaction ("Height"); item.setProperty (IDs::size, pick (l->height, r - 400), &um); }
        }
        else if (r >= 100)
        {
            const auto tag = pick (l->points, r - 100);
            const bool on = ! juce::StringArray::fromTokens (item[IDs::value].toString(), ",", {}).contains (tag);
            um.beginNewTransaction ("Accent");
            item.setProperty (IDs::value, setTag (item[IDs::value].toString(), tag, on), &um);
        }
        safe->repaint();
    });
}

//== Nodes =====================================================================

bool TimelineView::isInside (const juce::ValueTree& track, const juce::ValueTree& folder)
{
    for (auto f = project.folderOf (track); f.isValid(); f = project.folderOf (f))
        if (f == folder) return true;
    return false;
}

void TimelineView::paintNodes (juce::Graphics& g)
{
    const auto& rules = framework.getNodeRules();
    if (project.getNodes().getNumChildren() == 0) return;
    const auto rs = rows();
    const auto layers = rulerRows();
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (headerWidth, 0, getWidth() - headerWidth, getHeight());

    for (auto& fr : rs)
    {
        if (! fr.folder) continue;
        for (auto node : project.nodesOf (fr.track))
        {
            const float x0 = timeToX ((double) node[IDs::start]);
            const float x1 = juce::jmax (x0 + 2.0f, timeToX ((double) node[IDs::start] + (double) node[IDs::length]));
            if (x1 < (float) headerWidth || x0 > (float) getWidth()) continue;
            const bool selected = node[IDs::id].toString() == selectedNodeId;
            const auto check = rules.present ? Lanes::checkNode (project, rules, node) : Lanes::NodeCheck();
            const bool flagged = check.tooShort || check.tooLong || ! check.still.isEmpty();
            const auto edge = (flagged ? Theme::accent : Theme::signs).withAlpha (selected ? 0.95f : 0.6f);
            auto band = [&] (float top, float bottom)
            {
                g.setColour (Theme::signs.withAlpha (selected ? 0.2f : 0.1f));
                g.fillRect (x0, top, x1 - x0, bottom - top);
                g.setColour (edge);
                g.fillRect (x0, top, 1.0f, bottom - top);
                g.fillRect (x1 - 1.0f, top, 1.0f, bottom - top);
            };
            // Across the folder and its tracks
            float bottom = (float) (fr.y + fr.h);
            for (auto& r : rs) if (r.y > fr.y && isInside (r.track, fr.track)) bottom = juce::jmax (bottom, (float) (r.y + r.h));
            if (bottom > (float) rulerHeight()) band (juce::jmax ((float) fr.y, (float) rulerHeight()), bottom);   // under the ruler when scrolled
            // and across the lanes
            for (auto& l : layers) if (l.lane) band ((float) l.y, (float) (l.y + l.h - 1));
            // A mark on the folder's row
            const auto mark = juce::Rectangle<float> (x0 - 4.0f, (float) fr.y + 2.0f, juce::jmax (8.0f, x1 - x0 + 8.0f), 9.0f);
            juce::Path diamond;
            const float cx = (x0 + x1) / 2.0f, cy = (float) fr.y + 6.0f;
            diamond.addQuadrilateral (cx, cy - 4.0f, cx + 4.0f, cy, cx, cy + 4.0f, cx - 4.0f, cy);
            if (cy < (float) rulerHeight()) continue;
            g.setColour (flagged ? Theme::accent : Theme::signs);
            g.fillPath (diamond);
            if (flagged)
            {
                g.setFont (Theme::font (10.0f, true));
                g.drawText ("!", mark.translated (6.0f, 0.0f).toNearestInt(), juce::Justification::centredRight, false);
            }
        }
    }

    // A node being drawn
    if (dragMode == DragMode::drawNode && drawFolder.isValid())
        for (auto& fr : rs)
            if (fr.track == drawFolder)
            {
                const float x0 = timeToX (juce::jmin (drawStart, drawEnd)), x1 = timeToX (juce::jmax (drawStart, drawEnd));
                g.setColour (Theme::accent.withAlpha (0.3f));
                g.fillRect (x0, (float) fr.y, juce::jmax (1.0f, x1 - x0), (float) fr.h);
            }
}

bool TimelineView::nodeMouseDown (const TrackRow& row, const juce::MouseEvent& e)
{
    if (! row.folder || e.x < headerWidth || ! framework.getNodeRules().present) return false;
    const auto pos = e.position;
    for (auto node : project.nodesOf (row.track))
    {
        const float x0 = timeToX ((double) node[IDs::start]);
        const float x1 = juce::jmax (x0 + 2.0f, timeToX ((double) node[IDs::start] + (double) node[IDs::length]));
        if (pos.x < x0 - edgeGrab || pos.x > x1 + edgeGrab) continue;
        selectDrawn ({}, node[IDs::id].toString());
        if (e.mods.isPopupMenu()) { showNodeMenu (node, row.track, xToTime (pos.x)); return true; }
        project.getUndoManager().beginNewTransaction ("Move node");
        dragNode = node;
        dragFrom = node[IDs::start];
        dragTo = node[IDs::length];
        const bool wide = x1 - x0 > 3.0f * edgeGrab;
        dragMode = wide && std::abs (pos.x - x0) <= edgeGrab ? DragMode::nodeStart
                 : wide && std::abs (pos.x - x1) <= edgeGrab ? DragMode::nodeEnd : DragMode::nodeMove;
        return true;
    }
    if (e.mods.isPopupMenu()) { showNodeMenu ({}, row.track, juce::jmax (0.0, xToTime (pos.x))); return true; }
    selectDrawn ({}, {});
    drawFolder = row.track;
    drawStart = drawEnd = snap (juce::jmax (0.0, xToTime (pos.x)), {}, ! e.mods.isShiftDown());
    dragMode = DragMode::drawNode;
    return true;
}

void TimelineView::showNodeMenu (juce::ValueTree node, const juce::ValueTree& folder, double t)
{
    juce::PopupMenu m;
    const auto& rules = framework.getNodeRules();
    if (node.isValid())
    {
        const auto [title, text] = describeNode (node);
        m.addSectionHeader (title);
        for (auto& line : juce::StringArray::fromLines (text)) if (line.isNotEmpty()) m.addItem (-1, line, false);
        m.addSeparator();
        m.addItem (1, "Delete node");
    }
    else
    {
        m.addSectionHeader (framework.text (rules.name) + juce::String (": ") + framework.text (rules.about));
        m.addItem (2, "Add a node here (" + TimeFormat::format (defaultNodeLength) + ")");
        m.addItem (3, "Or drag on the folder's lane to draw one", false);
    }
    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, node, folder, t] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1 && node.isValid()) { safe->selectDrawn ({}, node[IDs::id].toString()); safe->deleteDrawnSelection(); }
        if (r == 2 && folder.isValid())
        {
            safe->project.getUndoManager().beginNewTransaction ("Add node");
            auto n = safe->project.addNode (folder, juce::jmax (0.0, t - defaultNodeLength / 2.0), defaultNodeLength);
            safe->selectDrawn ({}, n[IDs::id].toString());
        }
    });
}

std::pair<juce::String, juce::String> TimelineView::describeNode (const juce::ValueTree& node)
{
    const auto& rules = framework.getNodeRules();
    auto folder = project.findTrack (node[IDs::folderId].toString());
    const double s = node[IDs::start], len = node[IDs::length];
    juce::String title = Theme::capitals (framework.text (rules.name)) + juce::String::fromUTF8 (" \xc2\xb7 ") + Theme::capitals (folder[IDs::name].toString());
    juce::String text;
    text << TimeFormat::format (s) << " - " << TimeFormat::format (s + len) << "   length " << TimeFormat::format (len) << "\n";
    if (! rules.present) return { title, text + "\nNo package in this project says what a node is (Vande Gorne's does)." };
    const auto c = Lanes::checkNode (project, rules, node);
    if (c.stretch > 0.0)
    {
        text << "\n" << juce::String (100.0 * c.ratio, c.ratio < 0.1 ? 1 : 0) << "% of the stretch before it (" << TimeFormat::format (c.stretch) << ")";
        if (c.tooShort) text << ": too short, it may go unheard (under " << juce::String (100.0 * rules.shortRatio, 0) << "%)";
        else if (c.tooLong) text << ": too long, it becomes a line (over " << juce::String (100.0 * rules.longRatio, 0) << "%)";
        else text << ": in balance";
        text << ".\n";
    }
    else
        text << "\nNothing comes before it in the folder.\n";
    if (c.tracks == 0) text << "\nThe folder has no tracks with boxes.";
    else if (c.still.isEmpty()) text << "\nEvery voice changes within it (" << c.tracks << (c.tracks == 1 ? " track" : " tracks") << ").";
    else text << "\nNot changing within it: " << c.still.joinIntoString (", ") << " (of " << c.tracks << ").";
    text << "\n\n" << framework.text (rules.about) << ".\n\nDrag it, or its edges, on the folder's lane; right-click it to delete it; Delete removes it too.";
    return { title, text };
}
