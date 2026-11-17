// FactorCurves.cpp — see FactorCurves.h.

#include "FactorCurves.h"
#include "Theme.h"

namespace FactorCurves
{

void paint (juce::Graphics& g, juce::Rectangle<float> area, const Factors::Curve& curve, double neutral,
            juce::Colour colour, bool withPoints, int hotPoint)
{
    // The neutral value: a dotted line.
    const float yn = area.getBottom() - (float) neutral * area.getHeight();
    g.setColour (Theme::textMuted.withAlpha (0.35f));
    for (float x = area.getX(); x < area.getRight(); x += 6.0f)
        g.fillRect (x, yn, 3.0f, 1.0f);
    if (curve.isEmpty()) return;

    const int n = juce::jmax (8, (int) area.getWidth() / 3);
    const auto values = Factors::sample (curve, n, neutral);
    juce::Path p;
    for (int i = 0; i <= n; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) n;
        const float y = area.getBottom() - values[i] * area.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (! withPoints) return;
    for (int i = 0; i < curve.size(); ++i)
    {
        const juce::Point<float> s (area.getX() + (float) curve[i].t * area.getWidth(), area.getBottom() - (float) curve[i].v * area.getHeight());
        const float r = i == hotPoint ? 4.5f : 3.5f;
        g.setColour (Theme::background);
        if (curve[i].straight) g.fillRect (s.x - r, s.y - r, r * 2.0f, r * 2.0f);
        else g.fillEllipse (s.x - r, s.y - r, r * 2.0f, r * 2.0f);
        g.setColour (colour);
        if (curve[i].straight) g.drawRect (s.x - r, s.y - r, r * 2.0f, r * 2.0f, 1.4f);
        else g.drawEllipse (s.x - r, s.y - r, r * 2.0f, r * 2.0f, 1.4f);
    }
}

//== Edit ======================================================================

juce::Point<float> Edit::toScreen (const Factors::Point& p) const
{
    return { area.getX() + (float) p.t * area.getWidth(), area.getBottom() - (float) p.v * area.getHeight() };
}

Factors::Point Edit::fromScreen (juce::Point<float> s) const
{
    Factors::Point p;
    p.t = juce::jlimit (0.0, 1.0, (double) ((s.x - area.getX()) / juce::jmax (1.0f, area.getWidth())));
    p.v = juce::jlimit (0.0, 1.0, (double) ((area.getBottom() - s.y) / juce::jmax (1.0f, area.getHeight())));
    return p;
}

int Edit::pointAt (juce::Point<float> s) const
{
    for (int i = 0; i < curve.size(); ++i)
        if (toScreen (curve[i]).getDistanceFrom (s) <= 6.0f) return i;
    return -1;
}

bool Edit::down (juce::Point<float> s, bool alt)
{
    freehand = false;
    trail.clear();
    dragging = pointAt (s);
    if (dragging >= 0) return false;
    if (alt)
    {
        freehand = true;
        trail.add (s);
        return false;
    }
    // A new point: the curve's first two points are its ends when it had none.
    auto p = fromScreen (s);
    if (curve.isEmpty())
    {
        curve.add (p);
        dragging = 0;
        return true;
    }
    curve.add (p);
    curve = Factors::tidy (curve);
    for (int i = 0; i < curve.size(); ++i)
        if (std::abs (curve[i].t - p.t) < 1.0e-6) dragging = i;
    return true;
}

bool Edit::drag (juce::Point<float> s)
{
    if (freehand)
    {
        if (trail.isEmpty() || trail.getLast().getDistanceFrom (s) >= 4.0f) trail.add (s);
        return false;
    }
    if (! juce::isPositiveAndBelow (dragging, curve.size())) return false;
    auto p = fromScreen (s);
    // A point stays between its neighbours.
    const double lo = dragging > 0 ? curve[dragging - 1].t + 0.002 : 0.0;
    const double hi = dragging + 1 < curve.size() ? curve[dragging + 1].t - 0.002 : 1.0;
    p.t = juce::jlimit (lo, juce::jmax (lo, hi), p.t);
    p.straight = curve[dragging].straight;
    if (p == curve[dragging]) return false;
    curve.set (dragging, p);
    return true;
}

bool Edit::up()
{
    const bool wasFreehand = freehand;
    freehand = false;
    dragging = -1;
    if (! wasFreehand || trail.size() < 2) { trail.clear(); return false; }

    // The trail replaces the curve where it went: every 12 pixels a point.
    auto a = fromScreen (trail.getFirst()), b = fromScreen (trail.getLast());
    if (a.t > b.t) { std::swap (a, b); std::reverse (trail.begin(), trail.end()); }
    Factors::Curve out;
    for (auto& p : curve) if (p.t < a.t - 1.0e-6 || p.t > b.t + 1.0e-6) out.add (p);
    float lastX = -1.0e9f;
    for (auto& s : trail)
        if (std::abs (s.x - lastX) >= 12.0f || &s == &trail.getReference (trail.size() - 1))
        {
            out.add (fromScreen (s));
            lastX = s.x;
        }
    curve = Factors::tidy (out);
    trail.clear();
    return true;
}

bool Edit::doubleClick (juce::Point<float> s)
{
    const int i = pointAt (s);
    if (i < 0) return false;
    curve.remove (i);
    return true;
}

void showMenu (const Factors::Curve& curve, int pointIndex, double neutral, juce::Component* target,
               std::function<void (const Factors::Curve&)> done)
{
    juce::PopupMenu m;
    if (juce::isPositiveAndBelow (pointIndex, curve.size()))
    {
        const bool straight = curve[pointIndex].straight;
        m.addItem (1, straight ? "A smooth curve from this point" : "A straight line from this point", pointIndex + 1 < curve.size());
        m.addItem (2, "Remove this point (double-click)");
        m.addSeparator();
    }
    juce::PopupMenu shapes;
    const auto names = Factors::presetNames();
    const juce::StringArray labels { "Flat (at the neutral value)", "Rising", "Falling", "Swell", "Dip", "Steps" };
    for (int i = 0; i < names.size(); ++i) shapes.addItem (10 + i, labels[i]);
    m.addSubMenu ("Start from a shape", shapes);
    m.addItem (3, "All straight lines", ! curve.isEmpty());
    m.addItem (4, "All smooth curves", ! curve.isEmpty());
    m.addSeparator();
    m.addItem (5, "Reset (no curve: the neutral value)", ! curve.isEmpty());
    juce::Component::SafePointer<juce::Component> safe (target);
    m.showMenuAsync (juce::PopupMenu::Options(), [curve, pointIndex, neutral, done, names, safe] (int r)
    {
        if (r == 0 || safe == nullptr) return;
        auto c = curve;
        if (r == 1) c.getReference (pointIndex).straight = ! c[pointIndex].straight;
        else if (r == 2) c.remove (pointIndex);
        else if (r == 3) for (auto& p : c) p.straight = true;
        else if (r == 4) for (auto& p : c) p.straight = false;
        else if (r == 5) c.clear();
        else if (r >= 10 && r < 10 + names.size()) c = Factors::preset (names[r - 10], neutral);
        done (c);
    });
}

//== Editor ====================================================================

Editor::Editor (const juce::String& n, const juce::String& lo, const juce::String& hi, double neutralValue, juce::Colour c)
    : name (n), low (lo), high (hi), neutral (neutralValue), colour (c)
{
    setTooltip (name + ": click to add a point, drag to move one, double-click to remove it, Alt-drag to draw freely; right-click for shapes and straight lines");
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> Editor::curveArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (15.0f).reduced (4.0f, 4.0f);
}

void Editor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::text.withAlpha (0.85f));
    g.setFont (Theme::font (11.5f, true));
    g.drawText (name, r.removeFromTop (15.0f).withTrimmedLeft (2.0f), juce::Justification::centredLeft, true);
    g.setColour (Theme::background);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Theme::line.brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    // Its two ends, faint in the corners.
    g.setColour (Theme::textMuted.withAlpha (0.55f));
    g.setFont (Theme::font (9.5f));
    const auto inner = r.reduced (5.0f, 2.0f);
    g.drawText (high, inner, juce::Justification::topRight, false);
    g.drawText (low, inner, juce::Justification::bottomRight, false);

    edit.area = curveArea();
    int hot = -1;
    if (isMouseOver()) hot = edit.pointAt (getMouseXYRelative().toFloat());
    FactorCurves::paint (g, edit.area, edit.curve, neutral, colour, true, hot);
    if (edit.freehand && edit.trail.size() > 1)
    {
        juce::Path p;
        p.startNewSubPath (edit.trail[0]);
        for (int i = 1; i < edit.trail.size(); ++i) p.lineTo (edit.trail[i]);
        g.setColour (colour.withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    if (edit.curve.isEmpty() && ! isMouseButtonDown())
    {
        g.setColour (Theme::textMuted.withAlpha (0.6f));
        g.setFont (Theme::font (10.5f).italicised());
        g.drawText ("click to draw", edit.area, juce::Justification::centred, false);
    }
}

void Editor::changed()
{
    if (onChange) onChange (edit.curve, ! started);
    started = true;
    repaint();
}

void Editor::mouseDown (const juce::MouseEvent& e)
{
    edit.area = curveArea();
    started = false;
    if (e.mods.isPopupMenu())
    {
        juce::Component::SafePointer<Editor> safe (this);
        showMenu (edit.curve, edit.pointAt (e.position), neutral, this, [safe] (const Factors::Curve& c)
        {
            if (safe == nullptr) return;
            safe->edit.curve = c;
            safe->started = false;
            safe->changed();
        });
        return;
    }
    if (edit.down (e.position, e.mods.isAltDown())) changed();
    repaint();
}

void Editor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (edit.drag (e.position)) changed();
    repaint();
}

void Editor::mouseUp (const juce::MouseEvent&)
{
    if (edit.up()) changed();
    repaint();
}

void Editor::mouseDoubleClick (const juce::MouseEvent& e)
{
    started = false;
    if (edit.doubleClick (e.position)) changed();
}

}
