// RulerPanel.cpp

#include "RulerPanel.h"
#include "Dialogs.h"
#include "Theme.h"
#include "TimeFormat.h"
#include "../Model/Lanes.h"

namespace
{
    constexpr int rowHeight = 30;
    constexpr int sizeSmall = 16, sizeMedium = 24, sizeLarge = 36;

    juce::var layerVar (const juce::String& kind, const juce::String& name, int height, const juce::String& frame = "piece",
                        const juce::String& series = {}, const juce::String& dens = {})
    {
        juce::ValueTree l (IDs::LAYER);
        l.setProperty (IDs::kind, kind, nullptr);
        l.setProperty (IDs::name, name, nullptr);
        l.setProperty (IDs::height, height, nullptr);
        l.setProperty (IDs::visible, true, nullptr);
        if (kind != "time")
        {
            l.setProperty (IDs::frame, frame, nullptr);
            l.setProperty (IDs::snap, true, nullptr);
            l.setProperty (IDs::autoDivisions, true, nullptr);
            l.setProperty (IDs::denominators, dens.isNotEmpty() ? dens : Proportion::formatList (Project::defaultDenominators()), nullptr);
            if (series.isNotEmpty()) l.setProperty (IDs::series, series, nullptr);
        }
        return Project::toVar (l);
    }

    juce::var preset (const juce::String& name, juce::Array<juce::var> layers)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", name);
        o->setProperty ("layers", layers);
        return juce::var (o);
    }

    juce::Array<juce::var> builtInPresets()
    {
        return {
            preset ("Time and the piece (A)", { layerVar ("time", "Time", 30), layerVar ("proportion", "Proportion", 20) }),
            preset ("Time only", { layerVar ("time", "Time", 30) }),
            preset ("Form work (B)", { layerVar ("proportion", "Piece", sizeSmall, "piece", "golden:2", "2,3,4"),
                                       layerVar ("time", "Time", 28),
                                       layerVar ("proportion", "Span", sizeMedium, "span") })
        };
    }

    void styleLabel (juce::Label& l, float size = 11.5f, juce::Colour c = Theme::textMuted)
    {
        l.setFont (Theme::font (size));
        l.setColour (juce::Label::textColourId, c);
        l.setInterceptsMouseClicks (false, false);
    }
}

//==============================================================================
// One layer in the list.
class LayerRowComponent : public juce::Component
{
public:
    LayerRowComponent (Project& p, juce::ValueTree l, bool sel, const Framework* fw = nullptr) : project (p), framework (fw), layer (l), selected (sel) {}

    std::function<void()> onSelect;
    std::function<void (int contentY)> onDragTo;
    std::function<void()> onDragEnd;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (0, 2);
        g.setColour (selected ? juce::Colour (0xff2a2c32) : Theme::panel2);
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        if (selected)
        {
            g.setColour (Theme::accent);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
        }

        // ⋮⋮ handle
        g.setColour (Theme::textMuted);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 2; ++j)
                g.fillEllipse (7.0f + (float) j * 4.0f, (float) r.getCentreY() - 6.0f + (float) i * 5.0f, 2.0f, 2.0f);

        // Eye
        const bool shown = (bool) layer.getProperty (IDs::visible, true);
        auto eye = eyeArea().toFloat();
        juce::Path p;
        p.startNewSubPath (eye.getX(), eye.getCentreY());
        p.quadraticTo (eye.getCentreX(), eye.getY() - 2.0f, eye.getRight(), eye.getCentreY());
        p.quadraticTo (eye.getCentreX(), eye.getBottom() + 2.0f, eye.getX(), eye.getCentreY());
        g.setColour (shown ? Theme::text : Theme::textMuted.withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
        if (shown) g.fillEllipse (eye.getCentreX() - 2.0f, eye.getCentreY() - 2.0f, 4.0f, 4.0f);
        else g.drawLine (eye.getX(), eye.getBottom(), eye.getRight(), eye.getY(), 1.2f);

        // Name and kind
        const bool time = Project::isTimeLayer (layer);
        g.setColour (shown ? Theme::text : Theme::textMuted);
        g.setFont (Theme::font (12.5f, selected));
        auto text = layer[IDs::name].toString();
        const auto frameName = project.frameName (project.layerFrame (layer));
        if (Project::isProportionLayer (layer) && ! text.equalsIgnoreCase (frameName)) text << "  (" << frameName << ")";
        if (Project::isLaneLayer (layer)) text << "  (lane)";
        const auto from = layer.getProperty (IDs::frame, "piece").toString();
        if (time && from != "piece" && project.frameFor (from).isValid()) text << "  (from " << project.frameName (from) << ")";
        g.drawText (text, nameArea(), juce::Justification::centredLeft, true);

        // S M L
        const int h = (int) layer.getProperty (IDs::height, 24);
        juce::String cur = h <= 18 ? "S" : (h <= 30 ? "M" : "L");
        if (auto* lane = laneOf())
        {
            const int small = Lanes::heightFor (*framework, *lane, 0), medium = Lanes::heightFor (*framework, *lane, 1);
            cur = h < (small + medium) / 2 ? "S" : (h <= medium + 4 ? "M" : "L");
        }
        const char* names[] = { "S", "M", "L" };
        for (int i = 0; i < 3; ++i)
        {
            auto b = sizeArea (i);
            const bool on = cur == names[i];
            g.setColour (on ? Theme::accent : Theme::background);
            g.fillRoundedRectangle (b.toFloat(), 3.0f);
            g.setColour (on ? Theme::background : Theme::textMuted);
            g.setFont (Theme::font (10.5f, true));
            g.drawText (names[i], b, juce::Justification::centred);
        }

        // Delete (not the last time layer)
        if (canDelete())
        {
            auto d = deleteArea().toFloat().reduced (3.0f);
            g.setColour (Theme::textMuted);
            g.drawLine (d.getX(), d.getY(), d.getRight(), d.getBottom(), 1.3f);
            g.drawLine (d.getRight(), d.getY(), d.getX(), d.getBottom(), 1.3f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        draggingHandle = e.x < 20;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingHandle && onDragTo) onDragTo (getY() + e.y);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (draggingHandle)
        {
            draggingHandle = false;
            if (e.mouseWasDraggedSinceMouseDown()) { if (onDragEnd) onDragEnd(); return; }
        }
        const auto pos = e.getPosition();
        if (eyeArea().expanded (4).contains (pos)) { layer.setProperty (IDs::visible, ! (bool) layer.getProperty (IDs::visible, true), nullptr); return; }
        for (int i = 0; i < 3; ++i)
            if (sizeArea (i).contains (pos))
            {
                if (auto* lane = laneOf())  layer.setProperty (IDs::height, Lanes::heightFor (*framework, *lane, i), nullptr);
                else                        layer.setProperty (IDs::height, i == 0 ? sizeSmall : (i == 1 ? sizeMedium : sizeLarge), nullptr);
                return;
            }
        if (canDelete() && deleteArea().contains (pos)) { project.removeLayer (layer); return; }
        if (onSelect) onSelect();
    }

private:
    Project& project;
    const Framework* framework = nullptr;
    juce::ValueTree layer;
    bool selected = false, draggingHandle = false;

    const Framework::Lane* laneOf() const
    {
        return framework != nullptr && Project::isLaneLayer (layer) ? framework->findLane (layer[IDs::lane].toString()) : nullptr;
    }

    bool canDelete() const
    {
        if (! Project::isTimeLayer (layer)) return true;
        int timeLayers = 0;
        for (auto l : project.getRulers()) if (Project::isTimeLayer (l)) ++timeLayers;
        return timeLayers > 1;
    }

    juce::Rectangle<int> eyeArea() const    { return { 22, getHeight() / 2 - 5, 16, 10 }; }
    juce::Rectangle<int> deleteArea() const { return { getWidth() - 22, getHeight() / 2 - 8, 16, 16 }; }
    juce::Rectangle<int> sizeArea (int i) const { return { getWidth() - 96 + i * 22, getHeight() / 2 - 9, 20, 18 }; }
    juce::Rectangle<int> nameArea() const   { return { 46, 0, getWidth() - 46 - 100, getHeight() }; }
};

//==============================================================================
class RulerPanel::Content : public juce::Component
{
public:
    Content (RulerPanel& o) : owner (o), project (o.project) {}

    void rebuild (int width)
    {
        parts.clear();
        y = 0;
        w = width;
        auto& p = project;

        auto layer = p.findLayer (owner.layerId);
        if (! layer.isValid()) layer = p.getActiveLayer();
        if (! layer.isValid() && p.getRulers().getNumChildren() > 0) layer = p.getRulers().getChild (0);
        shown = layer;

        // Title
        {
            auto* t = add (std::make_unique<juce::Label> (juce::String(), "RULER LAYERS"), 22, 0, w - 70);
            t->setFont (Theme::font (12.0f, true));
            t->setColour (juce::Label::textColourId, Theme::textMuted);
            auto* done = place (std::make_unique<juce::TextButton> ("Done"), w - 64, y - 22, 64, 22);
            done->onClick = [this] { if (owner.onDone) owner.onDone(); };
            gap (4);
            note ("Click a layer to show it here. Drag the dots to reorder; the eye shows or hides it; S M L sizes it.");
        }

        // The layers
        rowsTop = y;
        int index = 0;
        for (auto l : p.getRulers())
        {
            auto* row = add (std::make_unique<LayerRowComponent> (p, l, l == layer, owner.framework), rowHeight, 0);
            const auto id = l[IDs::id].toString();
            row->onSelect = [this, id, l]
            {
                owner.layerId = id;
                if (Project::isProportionLayer (l)) project.setActiveLayer (l);
                // Later: this row is still handling its click.
                juce::Component::SafePointer<RulerPanel> safe (&owner);
                juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refresh(); });
            };
            const int from = index;
            row->onDragTo = [this, from] (int contentY) { dragTarget = juce::jlimit (0, project.getRulers().getNumChildren() - 1, (contentY - rowsTop) / rowHeight); juce::ignoreUnused (from); };
            row->onDragEnd = [this, from]
            {
                if (dragTarget >= 0 && dragTarget != from) project.getRulers().moveChild (from, dragTarget, nullptr);
                dragTarget = -1;
            };
            ++index;
        }
        gap (4);
        {
            auto* addProp = add (std::make_unique<juce::TextButton> ("+ Proportion layer"), 24, 0, (w - 6) / 2);
            addProp->onClick = [this]
            {
                auto l = project.addLayer ("proportion");
                project.setActiveLayer (l);
                owner.layerId = l[IDs::id].toString();
            };
            auto* addTime = place (std::make_unique<juce::TextButton> ("+ Time layer"), (w + 6) / 2, y - 24, (w - 6) / 2, 24);
            addTime->onClick = [this]
            {
                auto l = project.addLayer ("time");
                owner.layerId = l[IDs::id].toString();
            };
            // Overview lanes (stage 4d), as the packages declare them.
            gap (4);
            auto* addLane = add (std::make_unique<juce::TextButton> ("+ Lane..."), 24, 0, (w - 6) / 2);
            const bool any = owner.framework != nullptr && ! owner.framework->getLanes().isEmpty();
            addLane->setEnabled (any && owner.onAddLane != nullptr);
            addLane->setTooltip (any ? "An overview lane: energies, semiotic profile, typology..." : "No package in this project declares a lane");
            addLane->onClick = [this, addLane]
            {
                if (owner.framework == nullptr) return;
                juce::PopupMenu m;
                const auto& lanes = owner.framework->getLanes();
                for (int i = 0; i < lanes.size() && i < 99; ++i)
                {
                    const auto& lane = lanes.getReference (i);
                    bool present = false;
                    for (auto l : project.getRulers()) present = present || l[IDs::lane].toString() == lane.key();
                    m.addItem (1 + i, owner.framework->text (lane.name), true, present);
                }
                juce::Component::SafePointer<RulerPanel> safe (&owner);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addLane), [safe] (int r)
                {
                    if (safe == nullptr || r <= 0 || safe->onAddLane == nullptr) return;
                    auto l = safe->onAddLane (r - 1);
                    if (l.isValid()) safe->layerId = l[IDs::id].toString();
                    safe->refresh();
                });
            };
        }

        // Presets
        gap (10);
        heading ("Presets");
        {
            auto all = builtInPresets();
            const int builtIns = all.size();
            for (auto& v : userPresets()) all.add (v);

            auto* box = add (std::make_unique<juce::ComboBox>(), 24, 0, w - 132);
            box->setTextWhenNothingSelected ("Apply a preset...");
            for (int i = 0; i < all.size(); ++i)
            {
                if (i == builtIns) box->addSeparator();
                box->addItem (all[i]["name"].toString(), i + 1);
            }
            box->onChange = [this, box, all]
            {
                const int i = box->getSelectedId() - 1;
                if (i < 0 || i >= all.size()) return;
                project.layersFromVar (all[i]["layers"]);
                owner.layerId = {};
                project.setActiveLayer (project.getActiveLayer());
            };
            auto* save = place (std::make_unique<juce::TextButton> ("Save..."), w - 128, y - 24, 62, 24);
            save->onClick = [this] { savePreset(); };
            auto* del = place (std::make_unique<juce::TextButton> ("Delete..."), w - 62, y - 24, 62, 24);
            del->setEnabled (userPresets().size() > 0);
            del->onClick = [this, del] { deletePreset (del); };
        }

        // The chosen layer
        gap (14);
        if (! layer.isValid()) { note ("No layers."); finish(); return; }

        heading (Project::isTimeLayer (layer) ? "Time layer" : Project::isLaneLayer (layer) ? "Lane" : "Proportion layer");
        {
            label ("Name");
            auto* name = add (std::make_unique<juce::TextEditor>(), 24, 6);
            name->setText (layer[IDs::name].toString(), false);
            auto commit = [layer, name] () mutable { if (name->getText().trim().isNotEmpty()) layer.setProperty (IDs::name, name->getText().trim(), nullptr); };
            name->onReturnKey = commit;
            name->onFocusLost = commit;
        }

        if (Project::isLaneLayer (layer))
        {
            const auto key = layer[IDs::lane].toString();
            auto* lane = owner.framework != nullptr ? owner.framework->findLane (key) : nullptr;
            if (lane == nullptr)
                note ("This lane (" + key + ") comes from a package the project does not use. Framework > Frameworks... brings it in.");
            else
            {
                auto* pk = owner.framework->findPackage (lane->package);
                note (owner.framework->text (lane->about) + (pk != nullptr ? " (" + owner.framework->text (pk->name) + ")" : juce::String()) + ".");
                note ("Worked out from the boxes; muted tracks are left out. Click a row to highlight the boxes behind it, "
                      "again to stop; the lane's menu clears the highlight.");
            }
            finish();
            return;
        }

        if (Project::isTimeLayer (layer))
        {
            label ("Count from");
            auto* from = add (std::make_unique<juce::ComboBox>(), 24, 6);
            juce::StringArray refs { "piece", "span" };
            from->addItem ("The start of the piece (0:00)", 1);
            from->addItem ("The start of the span", 2);
            from->setItemEnabled (2, p.hasSpan());
            for (auto b : p.allBoxes())
            {
                refs.add (b[IDs::id].toString());
                from->addItem ("The start of the box " + b[IDs::name].toString() + "  (" + b.getParent()[IDs::name].toString() + ")", refs.size());
            }
            const auto cur = layer.getProperty (IDs::frame, "piece").toString();
            from->setSelectedId (p.frameFor (cur).isValid() ? refs.indexOf (cur) + 1 : 1, juce::dontSendNotification);
            from->onChange = [from, refs, layer] () mutable
            {
                const int i = from->getSelectedId() - 1;
                if (i >= 0 && i < refs.size()) layer.setProperty (IDs::frame, refs[i], nullptr);
            };
            note ("Click or drag on any time layer to move the playhead. A layer counting from a box reads the time inside that box "
                  "(negative before it), and follows the box when it moves.");
            finish();
            return;
        }

        // Frame
        {
            label ("Frame (what the layer divides)");
            auto* frame = add (std::make_unique<juce::ComboBox>(), 24, 6);
            juce::StringArray refs { "piece", "span" };
            frame->addItem ("The piece (" + TimeFormat::format (p.getPieceLength(), 0) + ")", 1);
            frame->addItem ("The span", 2);
            frame->setItemEnabled (2, p.hasSpan());
            for (auto b : p.allBoxes())
            {
                refs.add (b[IDs::id].toString());
                frame->addItem ("Box: " + b[IDs::name].toString() + "  (" + b.getParent()[IDs::name].toString() + ")", refs.size());
            }
            frame->setSelectedId (refs.indexOf (p.layerFrame (layer)) + 1, juce::dontSendNotification);
            frame->onChange = [this, frame, refs, layer]
            {
                const int i = frame->getSelectedId() - 1;
                if (i >= 0 && i < refs.size()) project.setLayerFrame (layer, refs[i]);
            };
        }

        toggle ("Boxes and marks snap to this layer", (bool) layer.getProperty (IDs::snap, true),
                [layer] (bool on) mutable { layer.setProperty (IDs::snap, on, nullptr); });
        toggle ("Finer divisions appear as you zoom in", (bool) layer.getProperty (IDs::autoDivisions, true),
                [layer] (bool on) mutable { layer.setProperty (IDs::autoDivisions, on, nullptr); });

        // Divisions
        gap (6);
        label ("Divisions");
        {
            const auto dens = p.layerDenominators (layer);
            const int perRow = 5, bw = (w - (perRow - 1) * 4) / perRow;
            for (int d = 2; d <= 20; ++d)
            {
                const int i = d - 2, col = i % perRow;
                if (col == 0 && i > 0) y += 26;
                auto* b = place (std::make_unique<juce::TextButton> ("1/" + juce::String (d)), col * (bw + 4), y, bw, 22);
                const bool on = Proportion::indexOf (dens, (double) d) >= 0;
                b->setColour (juce::TextButton::buttonColourId, on ? Theme::accent.withAlpha (0.25f) : Theme::background);
                b->setColour (juce::TextButton::textColourOffId, on ? Theme::accent : Theme::textMuted);
                b->onClick = [this, layer, d]
                {
                    auto list = project.layerDenominators (layer);
                    const int idx = Proportion::indexOf (list, (double) d);
                    if (idx >= 0) list.remove (idx); else list.add (d);
                    std::sort (list.begin(), list.end());
                    project.setLayerDenominators (layer, list);
                };
            }
            y += 26;

            juce::Array<double> own;
            for (auto d : dens) if (std::abs (d - std::round (d)) > 1.0e-9 || d > 20.0) own.add (d);
            label ("Your own denominators (decimals allowed: 7.5 = 7 parts and a half)");
            auto* custom = add (std::make_unique<juce::TextEditor>(), 24, 6);
            custom->setText (Proportion::formatList (own).replace (",", ", "), false);
            auto commit = [this, layer, custom] () mutable
            {
                juce::Array<double> list;
                for (auto d : project.layerDenominators (layer)) if (std::abs (d - std::round (d)) < 1.0e-9 && d <= 20.0) list.add (d);
                for (auto d : Proportion::parseList (custom->getText())) if (Proportion::indexOf (list, d) < 0) list.add (d);
                std::sort (list.begin(), list.end());
                project.setLayerDenominators (layer, list);
            };
            custom->onReturnKey = commit;
            custom->onFocusLost = commit;
        }

        // Series
        gap (6);
        label ("Series (drawn in their own colour, snapped to)");
        {
            const auto spec = layer[IDs::series].toString();
            const int cw = (w - 8) / 3;
            struct Choice { const char* key; const char* title; std::vector<int> values; };
            const Choice choices[] = { { "golden", "Golden", { 1, 2, 3 } },
                                       { "fibonacci", "Fibonacci", { 5, 8, 13, 21, 34, 55 } },
                                       { "harmonic", "Harmonic", { 4, 6, 8, 12, 16, 24, 32, 64 } } };
            for (int c = 0; c < 3; ++c)
            {
                auto* l = place (std::make_unique<juce::Label> (juce::String(), choices[c].title), c * (cw + 4), y, cw, 16);
                styleLabel (*l, 10.5f);
            }
            y += 16;
            for (int c = 0; c < 3; ++c)
            {
                auto* box = place (std::make_unique<juce::ComboBox>(), c * (cw + 4), y, cw, 24);
                box->addItem ("Off", 1);
                const auto& ch = choices[c];
                const int current = Proportion::seriesPart (spec, ch.key).getIntValue();
                for (size_t i = 0; i < ch.values.size(); ++i)
                {
                    const int v = ch.values[i];
                    juce::String text = c == 0 ? juce::String (v) + (v == 1 ? " level" : " levels") : (c == 1 ? "of " + juce::String (v) : "to 1/" + juce::String (v));
                    box->addItem (text, (int) i + 2);
                    if (v == current) box->setSelectedId ((int) i + 2, juce::dontSendNotification);
                }
                if (box->getSelectedId() == 0) box->setSelectedId (1, juce::dontSendNotification);
                const juce::String key = ch.key;
                const auto values = ch.values;
                box->onChange = [layer, box, key, values] () mutable
                {
                    const int i = box->getSelectedId() - 2;
                    const auto v = i >= 0 && i < (int) values.size() ? juce::String (values[(size_t) i]) : juce::String();
                    layer.setProperty (IDs::series, Proportion::withSeriesPart (layer[IDs::series].toString(), key, v), nullptr);
                };
            }
            y += 28;

            // Direction: from the start, from the end (reversed), or both
            auto directionBox = [this, layer, spec] (const juce::String& seriesKey, int left, int boxTop, int boxWidth) mutable
            {
                const auto key = seriesKey;
                auto* d = place (std::make_unique<juce::ComboBox>(), left, boxTop, boxWidth, 24);
                d->addItem ("From the start", 1);
                d->addItem ("Reversed", 2);
                d->addItem ("Both", 3);
                const auto cur = Proportion::seriesPart (spec, key + ".dir");
                d->setSelectedId (cur == "reversed" ? 2 : (cur == "both" ? 3 : 1), juce::dontSendNotification);
                d->setTooltip ("Reversed measures the series from the end of the frame (1/3 becomes 2/3)");
                d->onChange = [d, layer, key] () mutable
                {
                    const int id = d->getSelectedId();
                    layer.setProperty (IDs::series, Proportion::withSeriesPart (layer[IDs::series].toString(), key + ".dir",
                                                                                id == 2 ? "reversed" : (id == 3 ? "both" : "")), nullptr);
                };
                return d;
            };
            {
                auto* sym = place (std::make_unique<juce::Label> (juce::String(), "symmetric"), 0, y, cw, 24);
                styleLabel (*sym, 10.5f);
                directionBox ("fibonacci", cw + 4, y, cw);
                directionBox ("harmonic", 2 * (cw + 4), y, cw);
                y += 30;
            }

            label ("Your own series: proportions separated by commas (1/7, 3/7, 0.3)");
            auto* custom = place (std::make_unique<juce::TextEditor>(), 0, y, w - cw - 4, 24);
            directionBox ("custom", w - cw, y, cw);
            y += 30;
            custom->setText (Proportion::seriesPart (spec, "custom"), false);
            auto commit = [layer, custom] () mutable
            {
                layer.setProperty (IDs::series, Proportion::withSeriesPart (layer[IDs::series].toString(), "custom", custom->getText()), nullptr);
            };
            custom->onReturnKey = commit;
            custom->onFocusLost = commit;
        }

        // Marks
        gap (8);
        const auto ref = p.layerFrame (layer);
        heading ("Marks in " + p.frameName (ref));
        note ("Double-click the layer for a new mark; drag a mark to move it. Box proportions can name them: start \"a1\", length \"a2 - a1\".");
        for (auto m : p.getMarks())
        {
            if (m[IDs::frame].toString() != ref) continue;
            auto* name = place (std::make_unique<juce::TextEditor>(), 0, y, 64, 24);
            name->setText (m[IDs::name].toString(), false);
            auto* expr = place (std::make_unique<juce::TextEditor>(), 68, y, w - 68 - 92, 24);
            expr->setText (m[IDs::expr].toString(), false);
            expr->setFont (Theme::mono (12.0f));
            auto* when = place (std::make_unique<juce::Label> (juce::String(), TimeFormat::format (juce::jmax (0.0, p.markTime (m)), 1)), w - 88, y, 64, 24);
            styleLabel (*when, 11.0f, Theme::text);
            when->setFont (Theme::mono (11.0f));
            auto* del = place (std::make_unique<juce::TextButton> ("x"), w - 22, y, 22, 24);
            y += 28;

            auto commitName = [this, m, name] () mutable
            {
                juce::String error;
                if (name->getText().trim() == m[IDs::name].toString()) return;
                project.getUndoManager().beginNewTransaction ("Rename mark");
                if (! project.renameMark (m, name->getText().trim(), error))
                {
                    name->setText (m[IDs::name].toString(), false);
                    if (owner.onStatus) owner.onStatus (error, true);
                }
            };
            name->onReturnKey = commitName;
            name->onFocusLost = commitName;
            auto commitExpr = [this, m, expr] () mutable
            {
                juce::String error;
                if (expr->getText().trim() == m[IDs::expr].toString()) return;
                project.getUndoManager().beginNewTransaction ("Mark proportion");
                if (! project.setMarkExpression (m, expr->getText(), error))
                {
                    expr->setText (m[IDs::expr].toString(), false);
                    if (owner.onStatus) owner.onStatus (error, true);
                }
            };
            expr->onReturnKey = commitExpr;
            expr->onFocusLost = commitExpr;
            del->onClick = [this, m] () mutable { project.getUndoManager().beginNewTransaction ("Delete mark"); project.removeMark (m); };
        }
        {
            auto* addMark = add (std::make_unique<juce::TextButton> ("+ Mark at the playhead"), 24, 8);
            addMark->onClick = [this, ref]
            {
                const auto f = project.frameFor (ref);
                if (! f.isValid() || ! owner.getPlayhead) return;
                project.getUndoManager().beginNewTransaction ("New mark");
                project.addMark (ref, Proportion::expressionFor (f.proportionOf (owner.getPlayhead()), project.getDenominators()));
            };
        }

        // Ratio split
        label ("Split the frame by a ratio into marks");
        {
            auto* ratio = place (std::make_unique<juce::TextEditor>(), 0, y, w - 160, 24);
            ratio->setText ("2:3:5:8", false);
            ratio->setFont (Theme::mono (12.0f));
            auto* prefix = place (std::make_unique<juce::TextEditor>(), w - 156, y, 52, 24);
            prefix->setText ("r", false);
            prefix->setTooltip ("The marks' names start with this");
            auto* go = place (std::make_unique<juce::TextButton> ("Make marks"), w - 100, y, 100, 24);
            y += 30;
            go->onClick = [this, ref, ratio, prefix]
            {
                const auto list = Proportion::parseParts (ratio->getText());
                if (list.isEmpty()) { if (owner.onStatus) owner.onStatus ("Write the parts with colons, for example 2:3:5:8.", true); return; }
                auto pre = prefix->getText().trim();
                if (! Proportion::isValidName (pre + "1")) pre = "r";
                project.getUndoManager().beginNewTransaction ("Split by ratio");
                project.splitFrameByRatio (ref, list, pre);
            };
        }
        finish();
    }

    bool isEditing() const
    {
        for (auto* c : parts)
            if (auto* t = dynamic_cast<juce::TextEditor*> (c))
                if (t->hasKeyboardFocus (true)) return true;
        return dragTarget >= 0 || juce::Desktop::getInstance().getMainMouseSource().isDragging();
    }

private:
    RulerPanel& owner;
    Project& project;
    juce::OwnedArray<juce::Component> parts;
    juce::ValueTree shown;
    int y = 0, w = 280, rowsTop = 0, dragTarget = -1;

    template <class C> C* place (std::unique_ptr<C> c, int x, int top, int width, int height)
    {
        auto* raw = c.get();
        raw->setBounds (x, top, width, height);
        addAndMakeVisible (raw);
        parts.add (c.release());
        return raw;
    }

    template <class C> C* add (std::unique_ptr<C> c, int height, int gapAfter, int width = -1)
    {
        auto* raw = place (std::move (c), 0, y, width < 0 ? w : width, height);
        y += height + gapAfter;
        return raw;
    }

    void gap (int g) { y += g; }

    void heading (const juce::String& text)
    {
        auto* l = add (std::make_unique<juce::Label> (juce::String(), text), 20, 2);
        l->setFont (Theme::font (12.5f, true));
        l->setColour (juce::Label::textColourId, Theme::text);
    }

    void label (const juce::String& text)
    {
        auto* l = add (std::make_unique<juce::Label> (juce::String(), text), 17, 1);
        styleLabel (*l);
    }

    void note (const juce::String& text)
    {
        auto* l = std::make_unique<juce::Label> (juce::String(), text).release();
        styleLabel (*l, 10.5f);
        l->setJustificationType (juce::Justification::topLeft);
        const int lines = juce::jmax (1, (int) std::ceil (juce::GlyphArrangement::getStringWidth (l->getFont(), text) / (float) juce::jmax (40, w - 8)));
        add (std::unique_ptr<juce::Label> (l), lines * 14 + 4, 4);
    }

    void toggle (const juce::String& text, bool on, std::function<void (bool)> set)
    {
        auto* t = add (std::make_unique<juce::ToggleButton> (text), 22, 0);
        t->setToggleState (on, juce::dontSendNotification);
        t->setColour (juce::ToggleButton::textColourId, Theme::text);
        t->setColour (juce::ToggleButton::tickColourId, Theme::accent);
        t->onClick = [t, set] { set (t->getToggleState()); };
    }

    void finish()
    {
        setSize (w, y + 12);
    }

    juce::Array<juce::var> userPresets() const
    {
        if (owner.properties == nullptr) return {};
        const auto v = juce::JSON::parse (owner.properties->getValue ("rulerPresets"));
        juce::Array<juce::var> out;
        if (auto* a = v.getArray()) for (auto& x : *a) out.add (x);
        return out;
    }

    void storeUserPresets (const juce::Array<juce::var>& list)
    {
        if (owner.properties == nullptr) return;
        owner.properties->setValue ("rulerPresets", juce::JSON::toString (juce::var (list), true));
        owner.properties->saveIfNeeded();
    }

    void savePreset()
    {
        auto* win = new juce::AlertWindow ("Save the ruler layers as a preset",
                                           "Presets are kept for all your projects. A box frame becomes the piece in other projects.",
                                           juce::MessageBoxIconType::NoIcon, this);
        win->addTextEditor ("name", "My ruler");
        win->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        win->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<Content> safe (this);
        Dialogs::focusField (win, "name");
        win->enterModalState (true, juce::ModalCallbackFunction::create ([safe, win] (int result)
        {
            if (safe == nullptr || result != 1) return;
            const auto name = win->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            auto list = safe->userPresets();
            for (int i = list.size(); --i >= 0;) if (list[i]["name"].toString() == name) list.remove (i);
            juce::Array<juce::var> layers;
            if (auto* a = safe->project.layersToVar().getArray()) for (auto& x : *a) layers.add (x);
            list.add (preset (name, layers));
            safe->storeUserPresets (list);
            safe->owner.refresh();
        }), true);
    }

    void deletePreset (juce::Component* target)
    {
        juce::PopupMenu m;
        auto list = userPresets();
        for (int i = 0; i < list.size(); ++i) m.addItem (i + 1, "Delete \"" + list[i]["name"].toString() + "\"");
        juce::Component::SafePointer<Content> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [safe, list] (int r) mutable
        {
            if (safe == nullptr || r == 0) return;
            list.remove (r - 1);
            safe->storeUserPresets (list);
            safe->owner.refresh();
        });
    }
};

//==============================================================================

RulerPanel::RulerPanel (Project& p) : project (p)
{
    content = std::make_unique<Content> (*this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

RulerPanel::~RulerPanel() = default;

void RulerPanel::showLayer (const juce::String& id)
{
    layerId = id;
    refresh();
}

void RulerPanel::refresh()
{
    if (! isVisible() || getWidth() <= 0) return;
    if (content->isEditing()) return;     // never pull a field away while it is being typed in
    const auto pos = viewport.getViewPosition();
    content->rebuild (viewport.getMaximumVisibleWidth() - 2);
    viewport.setViewPosition (pos);
}

void RulerPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    content->rebuild (viewport.getMaximumVisibleWidth() - 2);
}

void RulerPanel::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}
