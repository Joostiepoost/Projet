// FrameworkPanel.cpp

#include "FrameworkPanel.h"
#include "Theme.h"

namespace
{
    juce::Colour col (juce::uint32 c, juce::Colour fallback = Theme::lineStrong)
    {
        return c == 0 ? fallback : juce::Colour (c);
    }

    juce::String percent (double v)
    {
        auto s = juce::String (v * 100.0, 1);
        if (s.endsWith (".0")) s = s.dropLastCharacters (2);
        return s;
    }

    //== A small coloured label ================================================
    class Pill : public juce::Component
    {
    public:
        Pill (const juce::String& t, juce::Colour fill, juce::Colour edge, juce::Colour textColour)
            : text (t), fillColour (fill), edgeColour (edge), ink (textColour) {}

        static int widthFor (const juce::String& t)
        {
            return juce::GlyphArrangement::getStringWidthInt (Theme::font (11.0f), t) + 16;
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (fillColour);
            g.fillRoundedRectangle (r, r.getHeight() / 2.0f);
            g.setColour (edgeColour);
            g.drawRoundedRectangle (r, r.getHeight() / 2.0f, 1.0f);
            g.setColour (ink);
            g.setFont (Theme::font (11.0f));
            g.drawText (text, getLocalBounds(), juce::Justification::centred, true);
        }

    private:
        juce::String text;
        juce::Colour fillColour, edgeColour, ink;
    };

    //== A strip showing a value along the box (0 .. 1) =========================
    class AlongPreview : public juce::Component
    {
    public:
        std::function<juce::Colour (const juce::String&)> colourFor;
        std::function<float (const juce::String&)> heightFor;   // 0..1; null = full height
        juce::String base;
        bool baseInherited = false;
        juce::Array<Along::Change> changes;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (Theme::background);
            g.fillRoundedRectangle (r, 3.0f);
            const int n = juce::jmax (2, (int) r.getWidth());
            for (int i = 0; i < n; ++i)
            {
                const double p = (i + 0.5) / n;
                // Gradual changes: blend across their width.
                juce::String a = Along::valueAt (base, changes, p), b = a;
                float mixAmount = 0.0f;
                for (auto& c : changes)
                    if (c.width > 0.0 && std::abs (p - c.at) < c.width / 2.0)
                    {
                        a = Along::valueAt (base, changes, c.at - c.width / 2.0 - 1.0e-6);
                        b = c.to;
                        mixAmount = (float) ((p - (c.at - c.width / 2.0)) / c.width);
                    }
                auto colour = colourFor ? colourFor (a).interpolatedWith (colourFor (b), mixAmount) : Theme::lineStrong;
                // A folder's value (not the box's own) is drawn fainter.
                const float alphaA = baseInherited && a == base ? 0.45f : 0.95f, alphaB = baseInherited && b == base ? 0.45f : 0.95f;
                float h = r.getHeight();
                if (heightFor) h = r.getHeight() * juce::jmap (mixAmount, heightFor (a), heightFor (b));
                g.setColour (colour.withAlpha (juce::jmap (mixAmount, alphaA, alphaB)));
                g.fillRect (juce::Rectangle<float> (r.getX() + (float) i, r.getBottom() - h, 1.0f, h));
            }
            g.setColour (Theme::text.withAlpha (0.8f));
            for (auto& c : changes)
            {
                const float x = r.getX() + (float) c.at * r.getWidth();
                g.fillRect (juce::Rectangle<float> (x - 0.5f, r.getY(), 1.0f, r.getHeight()));
            }
            g.setColour (Theme::line);
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        }
    };

    //== Values in rows and columns (Schaeffer's typology) ======================
    class ChartPicker : public juce::Component
    {
    public:
        ChartPicker (const Framework& fw, const Framework::Category& c, const juce::String& current,
                     std::function<void (const juce::String&)> pick)
            : onPick (std::move (pick))
        {
            // Everything is copied: the framework may be reloaded while this is open.
            for (auto& r : c.rows) rowNames.add (fw.text (r.name));
            for (auto& x : c.columns) colNames.add (fw.text (x.name));
            caption = fw.text (c.chartCaption);
            for (auto& v : fw.valuesOf (c))
            {
                if (! juce::isPositiveAndBelow (v.row, rowNames.size()) || ! juce::isPositiveAndBelow (v.col, colNames.size())) continue;
                const auto colours = fw.valueColours (c, v.id);
                auto* b = buttons.add (new juce::TextButton (v.label.isNotEmpty() ? v.label : fw.text (v.name)));
                b->setTooltip (fw.text (v.name));
                b->setColour (juce::TextButton::buttonColourId, col (colours.dark, Theme::panel2));
                b->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
                b->setColour (juce::TextButton::textColourOffId, col (colours.light, Theme::text));
                b->setColour (juce::TextButton::textColourOnId, Theme::background);
                b->setToggleState (v.id == current, juce::dontSendNotification);
                const auto id = v.id;
                b->onClick = [this, id] { choose (id); };
                addAndMakeVisible (b);
                rows.add (v.row);
                cols.add (v.col);
            }
            none.setButtonText ("None");
            none.onClick = [this] { choose ({}); };
            addAndMakeVisible (none);
            setSize (headerW + juce::jmax (1, colNames.size()) * cellW + 8, headerH + juce::jmax (1, rowNames.size()) * cellH + 40);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel);
            g.setFont (Theme::font (10.5f));
            for (int c = 0; c < colNames.size(); ++c)
            {
                g.setColour (Theme::textMuted);
                g.drawFittedText (colNames[c], headerW + c * cellW + 2, 4, cellW - 4, headerH - 6, juce::Justification::centredBottom, 3, 0.9f);
            }
            for (int r = 0; r < rowNames.size(); ++r)
            {
                g.setColour (Theme::text);
                g.drawFittedText (rowNames[r], 6, headerH + r * cellH, headerW - 10, cellH, juce::Justification::centredLeft, 2, 0.9f);
            }
            g.setColour (Theme::textMuted);
            g.drawFittedText (caption, 6, getHeight() - 34, getWidth() - 110, 30, juce::Justification::centredLeft, 2, 0.9f);
        }

        void resized() override
        {
            // Cells; values in the same cell (Schaeffer's wefts Tn, Tx) share it.
            for (int i = 0; i < buttons.size(); ++i)
            {
                int same = 0, index = 0;
                for (int j = 0; j < buttons.size(); ++j)
                    if (rows[j] == rows[i] && cols[j] == cols[i])
                    {
                        if (j < i) ++index;
                        ++same;
                    }
                const int x = headerW + cols[i] * cellW, y = headerH + rows[i] * cellH;
                const int w = (cellW - 4) / juce::jmax (1, same);
                buttons[i]->setBounds (x + 2 + index * w, y + 2, w - (same > 1 ? 2 : 0), cellH - 4);
            }
            none.setBounds (getWidth() - 96, getHeight() - 32, 88, 26);
        }

    private:
        void choose (const juce::String& id)
        {
            auto pick = onPick;
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
            if (pick) pick (id);
        }

        static constexpr int cellW = 66, cellH = 34, headerW = 104, headerH = 46;
        std::function<void (const juce::String&)> onPick;
        juce::StringArray rowNames, colNames;
        juce::String caption;
        juce::OwnedArray<juce::TextButton> buttons;
        juce::Array<int> rows, cols;
        juce::TextButton none;
    };
}

void FrameworkPanel::showChart (const Framework& f, const Framework::Category& c, const juce::String& current, juce::Component& near,
                                std::function<void (const juce::String&)> onPick)
{
    auto chart = std::make_unique<ChartPicker> (f, c, current, std::move (onPick));
    juce::CallOutBox::launchAsynchronously (std::move (chart), near.getScreenBounds(), nullptr);
}

//== Content ===================================================================

class FrameworkPanel::Content : public juce::Component
{
public:
    Content (FrameworkPanel& o) : owner (o) {}

    bool isEditing() const
    {
        for (auto* c : getChildren())
        {
            if (auto* t = dynamic_cast<juce::TextEditor*> (c))
                if (t->hasKeyboardFocus (false)) return true;
            if (auto* s = dynamic_cast<juce::Slider*> (c))
                if (s->isMouseButtonDown()) return true;
        }
        return false;
    }

    void build (int w)
    {
        removeAllChildren();
        owned.clear();
        auto& fw = owner.framework;
        int y = 0;
        if (owner.boxes.isEmpty()) { setSize (w, 10); return; }

        if (fw.getCategories().isEmpty())
        {
            y = note ("This project uses no framework packages, or none with categories.", y, w);
            y = frameworksButton (y + 4, w);
            setSize (w, y + 8);
            return;
        }

        for (auto& p : fw.getPackages())
        {
            if (p.categories.isEmpty()) continue;
            packageHeading (fw.text (p.name), y, w);
            for (auto& key : p.categories)
                if (auto* c = fw.findCategory (key))
                    if (appliesToAny (*c))
                        y = category (*c, y, w) + 6;
            y += 4;
        }

        y += 6;
        y = note ("The names, colours and lists come from the project's framework packages.", y, w);
        y = frameworksButton (y + 2, w);
        setSize (w, y + 8);
    }

private:
    //== One category ===========================================================
    int category (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        heading (fw.text (c.name), fw.text (c.about), y, w);
        switch (c.kind)
        {
            case Framework::Kind::choice:
                if (c.control == "chart") return chart (c, y, w);
                if (c.control == "buttons") return buttons (c, y, w);
                return menu (c, y, w);
            case Framework::Kind::tags:   return tags (c, y, w);
            case Framework::Kind::number: return number (c, y, w);
            case Framework::Kind::text:   return text (c, y, w);
        }
        return y;
    }

    /** "Only with": the category applies to at least one selected box. */
    bool appliesToAny (const Framework::Category& c) const
    {
        if (! c.forBoxes()) return false;           // set on the folder (its right-click menu), or drawn in a lane
        for (auto b : owner.boxes) if (owner.project.applies (b, c)) return true;
        return false;
    }

    /** The join with the box before (stage 4c): which box, the gap, remarks. */
    int joinNote (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        if (owner.boxes.size() > 1) return note ("Each selected box: its join with the box before it on its track.", y, w);
        const auto join = owner.project.joinBefore (owner.boxes.getFirst());
        if (! join.isValid()) return note ("No box before it on this track.", y, w);
        auto text = "After " + join.before[IDs::name].toString().quoted() + " (" + describeGap (join.gap) + (join.covered ? ", spanned by another track" : "") + ")";
        for (auto& n : c.notes)
            if (join.gap >= n.from && join.gap < n.to) text << ": " << fw.text (n.text);
        return note (text + ".", y, w);
    }

    static juce::String describeGap (double gap)
    {
        const double a = std::abs (gap);
        const auto amount = a < 1.0 ? juce::String (juce::roundToInt (a * 1000.0)) + " ms" : juce::String (a, 2) + " s";
        return (gap < 0.0 ? "overlap " : "gap ") + amount;
    }

    /** The own value all selected boxes share ("" and `mixed` when they differ). */
    juce::String commonOwn (const Framework::Category& c, bool& mixed) const
    {
        mixed = false;
        const auto v = owner.boxes.getFirst()[c.property()].toString();
        for (auto b : owner.boxes) if (b[c.property()].toString() != v) mixed = true;
        return mixed ? juce::String() : v;
    }

    /** What the first box has (own, folder's, default), and whether the others differ. */
    Project::FrameworkValue commonEffective (const Framework::Category& c, bool& mixed) const
    {
        auto v = owner.project.frameworkValue (owner.boxes.getFirst(), c);
        mixed = false;
        for (auto b : owner.boxes) if (owner.project.frameworkValue (b, c).value != v.value) mixed = true;
        return v;
    }

    juce::String folderName() const
    {
        return owner.project.folderOf (owner.boxes.getFirst().getParent()).getProperty (IDs::name).toString().quoted();
    }

    /** The groups (families, zones), details and description of the value shown. */
    int describe (const Framework::Category& c, const Project::FrameworkValue& v, bool mixed, int y, int w, bool aboutToo)
    {
        auto& fw = owner.framework;
        if (mixed) return note ("The selected boxes have different values; a choice gives them all one.", y, w);
        if (v.fromFolder())
            y = note ("From the folder " + folderName() + " (or one around it). Choosing gives the box its own.", y, w);
        auto* value = fw.findValue (c, v.value);
        if (value == nullptr)
        {
            if (v.value.isNotEmpty()) return note (v.value + " is not in the framework packages (kept as it is).", y, w);
            return y;
        }
        if (value->groups.size() > 0)
        {
            int x = 0;
            for (auto& gid : value->groups)
            {
                auto* g = fw.findGroup (c, gid);
                const auto name = g != nullptr ? fw.text (g->name) : gid + "?";
                const int pw = Pill::widthFor (name);
                if (x > 0 && x + pw > w) { x = 0; y += 22; }
                place (std::make_unique<Pill> (name, col (g != nullptr ? g->colours.dark : 0u, Theme::panel2),
                                               col (g != nullptr ? g->colours.colour : 0u), col (g != nullptr ? g->colours.light : 0u, Theme::text)),
                       x, y, pw, 18);
                x += pw + 4;
            }
            y += 22;
        }
        juce::String line;
        if (c.hasTwoNames) line = fw.valueName (c, v.value);
        else if (aboutToo) line = fw.text (value->about);
        if (v.isDefault()) line = (line.isEmpty() ? fw.valueName (c, v.value) : line) + " (the default)";
        if (v.fromTiming()) line = fw.valueName (c, v.value) + " (from the timing): " + fw.text (value->about);
        else if (v.isImplied())
        {
            auto* source = fw.findCategory (v.source);
            line = fw.valueName (c, v.value) + " (from the folder's " + (source != nullptr ? fw.text (source->name).toLowerCase() : juce::String ("values"))
                 + "): " + fw.text (value->about);
        }
        if (line.isNotEmpty()) y = note (line, y, w);
        const auto details = fw.valueDetails (*value);
        if (! details.isEmpty())
            y = note ((c.detailsLabel.isEmpty() ? juce::String ("Details") : fw.text (c.detailsLabel)) + ": " + details.joinIntoString (", "), y, w);
        return y;
    }

    //== A choice as a menu (grouped) ============================================
    int menu (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        const auto& values = fw.valuesOf (c);
        bool mixed = false, effMixed = false;
        const auto own = commonOwn (c, mixed);
        const auto eff = commonEffective (c, effMixed);

        auto* combo = place (std::make_unique<juce::ComboBox>(), 0, y, w, 24);
        juce::StringArray ids { juce::String() };
        combo->addItem (c.folders ? "None (or the folder's)" : c.defaultValue.isNotEmpty() ? "None (the default)" : "None", 1);
        juce::Array<int> listed;
        for (auto& g : fw.groupsOf (c))
        {
            bool heading = false;
            for (int i = 0; i < values.size(); ++i)
                if (values[i].groups.size() > 0 && values[i].groups[0] == g.id)
                {
                    if (! heading) { combo->addSectionHeading (fw.text (g.name)); heading = true; }
                    ids.add (values[i].id);
                    combo->addItem (fw.valueName (c, values[i].id), ids.size());
                    listed.add (i);
                }
        }
        for (int i = 0; i < values.size(); ++i)
            if (! listed.contains (i))
            {
                ids.add (values[i].id);
                combo->addItem (fw.valueName (c, values[i].id), ids.size());
            }
        if (own.isNotEmpty() && ! ids.contains (own))
        {
            ids.add (own);
            combo->addItem (own + " (not in the framework)", ids.size());
        }
        if (mixed) combo->setText ("(different)", juce::dontSendNotification);
        else combo->setSelectedId (ids.indexOf (own) + 1, juce::dontSendNotification);
        const auto key = c.key();
        const auto undo = fw.text (c.name);
        combo->onChange = [this, combo, ids, key, undo]
        {
            const int i = combo->getSelectedId() - 1;
            if (i >= 0) owner.setValue (key, ids[i], undo);
        };
        y += 28;
        y = describe (c, eff, effMixed, y, w, true);
        return alongOrHint (c, y, w);
    }

    //== A choice as buttons ======================================================
    int buttons (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        const auto& values = fw.valuesOf (c);
        bool mixed = false, effMixed = false;
        const auto own = commonOwn (c, mixed);
        const auto eff = commonEffective (c, effMixed);
        const bool second = c.hasTwoNames && fw.getNaming (fw.source (c).key()) == "second";
        if (c.between)
        {
            y = joinNote (c, y, w);
            if (owner.boxes.size() == 1 && ! owner.project.joinBefore (owner.boxes.getFirst()).isValid()) return y;   // nothing to join
        }

        // The values that apply ("only with"), as wide as their names need.
        juce::Array<int> shown;
        int widest = 0;
        auto labelOf = [&] (const Framework::Value& v)
        {
            return second && ! v.name2.isEmpty() ? fw.text (v.name2) : v.label.isNotEmpty() ? v.label : fw.text (v.name);
        };
        for (int i = 0; i < values.size(); ++i)
        {
            bool fits = false;
            for (auto b : owner.boxes) fits = fits || owner.project.valueApplies (b, c, values.getReference (i).id);
            if (! fits) continue;
            shown.add (i);
            // (buttons squeeze their text a little, so 80% of its width is enough)
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidthInt (Theme::font (12.0f), labelOf (values.getReference (i))) * 8 / 10 + 10);
        }
        const int n = juce::jmax (1, shown.size());
        const int perRow = juce::jlimit (1, n, (w + 4) / juce::jmax (64, widest + 4));
        const int rowsNeeded = (n + perRow - 1) / perRow;
        const int bw = (w - (perRow - 1) * 4) / perRow;
        const auto key = c.key();
        const auto undo = fw.text (c.name);
        for (int k = 0; k < shown.size(); ++k)
        {
            const int i = k;
            const auto& v = values.getReference (shown[k]);
            const auto label = labelOf (v);
            auto* b = place (std::make_unique<juce::TextButton> (label), (i % perRow) * (bw + 4), y + (i / perRow) * 28, bw, 24);
            const bool on = ! effMixed && eff.value == v.id;
            const auto colour = col (fw.valueColours (c, v.id).colour, Theme::accent);
            b->setToggleState (on && eff.isOwn(), juce::dontSendNotification);
            b->setColour (juce::TextButton::buttonOnColourId, colour);
            b->setColour (juce::TextButton::textColourOnId, Theme::background);
            if (on && ! eff.isOwn())
            {
                // The folder's value, or the default: shown, but not the box's own.
                b->setColour (juce::TextButton::buttonColourId, colour.withAlpha (0.25f));
                b->setColour (juce::TextButton::textColourOffId, colour.brighter (0.4f));
            }
            auto tip = fw.valueName (c, v.id);
            if (! v.about.isEmpty()) tip << ": " << fw.text (v.about);
            b->setTooltip (tip + (c.defaultValue.isEmpty() ? ". Click again to remove the box's own." : juce::String()));
            const auto id = v.id;
            const auto def = c.defaultValue;
            b->onClick = [this, key, undo, id, own, mixed, def]
            {
                juce::String value = id;
                if (def.isNotEmpty()) { if (id == def) value = {}; }
                else if (! mixed && own == id) value = {};
                owner.setValue (key, value, undo);
            };
        }
        y += rowsNeeded * 28;
        const bool implied = ! fw.implicationsFor (c).isEmpty();
        if (c.between || implied)
        {
            // A chosen value stays until it goes back to what is worked out:
            // the timing, or the folder's other value (a writing type, 4d).
            bool anyOwn = false;
            for (auto b : owner.boxes) anyOwn = anyOwn || b[c.property()].toString().isNotEmpty();
            const bool timing = ! c.derive.isEmpty();
            if (anyOwn && (timing || implied))
            {
                auto* back = place (std::make_unique<juce::TextButton> (timing ? "From the timing" : "From the folder"), 0, y, juce::jmin (w, 130), 22);
                back->setTooltip (timing ? "Forget the chosen value: follow what the timing says again"
                                         : "Forget the chosen value: follow what the folder gives");
                back->onClick = [this, key, undo] { owner.setValue (key, {}, undo); };
                y += 26;
                if (owner.boxes.size() == 1)
                {
                    const auto derived = owner.project.frameworkValue (owner.boxes.getFirst(), c, true);
                    if (derived.value.isNotEmpty() && derived.value != own)
                        y = note ("Chosen; " + juce::String (derived.fromTiming() ? "the timing" : "the folder") + " says " + fw.valueName (c, derived.value) + ".", y, w);
                }
            }
        }
        if (! effMixed && eff.value.isEmpty() && ! mixed && ! c.between) y = note ("None yet.", y, w);
        y = describe (c, eff, effMixed || mixed, y, w, true);
        return alongOrHint (c, y, w);
    }

    //== A choice from a chart ======================================================
    int chart (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        bool mixed = false;
        const auto current = commonOwn (c, mixed);
        auto* value = fw.findValue (c, current);
        const auto label = mixed ? juce::String ("(different)")
                         : current.isEmpty() ? juce::String ("Choose...")
                         : value != nullptr ? (value->label.isNotEmpty() ? value->label + "   " : juce::String()) + fw.text (value->name)
                         : current + "?";
        auto* b = place (std::make_unique<juce::TextButton> (label), 0, y, w - 30, 26);
        const auto colours = fw.valueColours (c, current);
        if (colours.colour != 0)
        {
            b->setColour (juce::TextButton::buttonColourId, col (colours.dark));
            b->setColour (juce::TextButton::textColourOffId, col (colours.light, Theme::text));
        }
        const auto key = c.key();
        const auto undo = fw.text (c.name);
        b->onClick = [this, b, current, key, undo]
        {
            auto* cat = owner.framework.findCategory (key);
            if (cat == nullptr) return;
            juce::Component::SafePointer<FrameworkPanel> safe (&owner);
            FrameworkPanel::showChart (owner.framework, *cat, current, *b, [safe, key, undo] (const juce::String& id)
            {
                if (safe != nullptr) safe->setValue (key, id, undo);
            });
        };
        auto* clear = place (std::make_unique<juce::TextButton> ("x"), w - 26, y, 26, 26);
        clear->setTooltip ("None");
        clear->setEnabled (mixed || current.isNotEmpty());
        clear->onClick = [this, key, undo] { owner.setValue (key, {}, undo); };
        y += 30;
        if (value != nullptr)
        {
            // Where it sits: its group, its row, its column.
            juce::StringArray parts;
            for (auto& gid : value->groups) if (auto* g = fw.findGroup (c, gid)) parts.add (fw.text (g->name));
            if (juce::isPositiveAndBelow (value->row, c.rows.size())) parts.add (fw.text (c.rows[value->row].name));
            if (juce::isPositiveAndBelow (value->col, c.columns.size())) parts.add (fw.text (c.columns[value->col].name));
            y = note (parts.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")), y, w);
        }
        else if (current.isNotEmpty())
            y = note (current + " is not in the framework packages (kept as it is).", y, w);
        return alongOrHint (c, y, w);
    }

    //== Tags ======================================================================
    int tags (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        const auto key = c.key();
        bool some = false;
        int x = 0;
        for (auto& v : fw.valuesOf (c))
        {
            int count = 0;
            for (auto b : owner.boxes) if (Tags::parse (b[c.property()].toString()).contains (v.id)) ++count;
            const bool all = count == owner.boxes.size();
            some = some || (count > 0 && ! all);
            const auto label = v.label.isNotEmpty() ? v.label : fw.text (v.name);
            const int bw = juce::jmin (w, juce::GlyphArrangement::getStringWidthInt (Theme::font (12.0f), label) + 20);
            if (x > 0 && x + bw > w) { x = 0; y += 26; }
            auto* b = place (std::make_unique<juce::TextButton> (label), x, y, bw, 22);
            const auto colour = col (fw.valueColours (c, v.id).colour, col (c.colour, Theme::accent));
            b->setToggleState (all, juce::dontSendNotification);
            b->setColour (juce::TextButton::buttonOnColourId, colour);
            b->setColour (juce::TextButton::textColourOnId, Theme::background);
            if (count > 0 && ! all) b->setColour (juce::TextButton::buttonColourId, colour.withAlpha (0.3f));
            b->setTooltip (fw.text (v.name) + (v.about.isEmpty() ? juce::String() : ": " + fw.text (v.about)));
            const auto id = v.id;
            b->onClick = [this, key, id, all] { owner.setTag (key, id, ! all); };
            x += bw + 4;
        }
        y += 26;
        if (fw.valuesOf (c).isEmpty()) y = note ("No values in the package.", y, w);
        if (some) y = note ("Lighter: some of the selected boxes have it. A click gives it to all of them.", y, w);
        return y;
    }

    //== A number ===================================================================
    int number (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        bool mixed = false, effMixed = false;
        const auto own = commonOwn (c, mixed);
        const auto eff = commonEffective (c, effMixed);
        const auto key = c.key();
        const auto undo = fw.text (c.name);
        auto* s = place (std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight), 0, y, w - 30, 24);
        s->setRange (c.minimum, c.maximum, c.step > 0.0 ? c.step : 0.0);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        const auto fwRef = &fw;
        s->textFromValueFunction = [fwRef, key] (double v)
        {
            auto* cat = fwRef->findCategory (key);
            return cat != nullptr ? fwRef->formatNumber (*cat, v) : juce::String (v);
        };
        s->valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getDoubleValue(); };
        s->setValue (eff.value.isNotEmpty() ? eff.value.getDoubleValue() : c.minimum, juce::dontSendNotification);
        s->updateText();
        if (eff.value.isEmpty() || ! eff.isOwn()) s->setAlpha (0.6f);
        s->onDragStart = [this, undo] { owner.project.getUndoManager().beginNewTransaction (undo); };
        s->onValueChange = [this, s, key, undo] { owner.setValue (key, s->getValue(), undo, ! s->isMouseButtonDown()); };
        s->onDragEnd = [this]
        {
            // The panel waited while dragging: show the new state now.
            juce::Component::SafePointer<FrameworkPanel> safe (&owner);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->rebuildNow(); });
        };
        auto* clear = place (std::make_unique<juce::TextButton> ("x"), w - 26, y, 26, 24);
        clear->setTooltip ("Not set");
        clear->setEnabled (mixed || own.isNotEmpty());
        clear->onClick = [this, key, undo] { owner.setValue (key, {}, undo); };
        y += 28;
        if (mixed || effMixed) y = note ("The selected boxes have different values; moving the slider gives them all one.", y, w);
        else if (eff.fromFolder()) y = note ("From the folder " + folderName() + ".", y, w);
        else if (eff.value.isEmpty()) y = note ("Not set (drag to set).", y, w);
        return y;
    }

    //== A text =====================================================================
    int text (const Framework::Category& c, int y, int w)
    {
        bool mixed = false;
        const auto own = commonOwn (c, mixed);
        const auto key = c.key();
        const auto undo = owner.framework.text (c.name);
        auto* e = place (std::make_unique<juce::TextEditor>(), 0, y, w, 24);
        e->setFont (Theme::font (12.5f));
        e->setText (own, false);
        if (mixed) e->setTextToShowWhenEmpty ("(different)", Theme::textMuted);
        auto commit = [this, e, key, undo, own]
        {
            const auto t = e->getText().trim();
            if (t != own) owner.setValue (key, t, undo);
        };
        e->onReturnKey = commit;
        e->onFocusLost = commit;
        return y + 28;
    }

    //== Changes along a box ==========================================================
    int alongOrHint (const Framework::Category& c, int y, int w)
    {
        if (! c.along) return y;
        if (owner.boxes.size() == 1) return alongEditor (c, y, w);
        return note ("Changes along a box: select one box.", y, w);
    }

    int alongEditor (const Framework::Category& c, int y, int w)
    {
        auto& fw = owner.framework;
        auto box = owner.boxes.getFirst();
        const auto changes = Project::changesAlong (box, c);
        const auto key = c.key();
        const auto& values = fw.valuesOf (c);

        juce::StringArray choiceIds, choiceNames;
        for (auto& v : values) { choiceIds.add (v.id); choiceNames.add (fw.valueLabel (c, v.id)); }

        const auto eff = owner.project.frameworkValue (box, c);
        const auto base = eff.value;

        if (changes.isEmpty())
        {
            auto* add = place (std::make_unique<juce::TextButton> ("+ Change along the box"), 0, y, w, 22);
            add->setColour (juce::TextButton::buttonColourId, Theme::background);
            add->setColour (juce::TextButton::textColourOffId, Theme::textMuted);
            add->setTooltip ("From a point in the box on, another value (suddenly, or gradually over a width)");
            add->onClick = [this, key, base, choiceIds]
            {
                auto next = choiceIds.isEmpty() ? juce::String() : choiceIds[(choiceIds.indexOf (base) + 1) % choiceIds.size()];
                owner.setAlong (key, { { 0.5, next, 0.0 } });
            };
            return y + 26;
        }

        // Weights (roles) draw as heights; colours as colours.
        int maxWeight = 0;
        for (auto& v : values) maxWeight = juce::jmax (maxWeight, v.weight);
        auto* preview = place (std::make_unique<AlongPreview>(), 0, y, w, maxWeight > 0 ? 16 : 12);
        preview->base = base;
        preview->baseInherited = eff.fromFolder();
        preview->changes = changes;
        const auto fallback = col (c.colour, Theme::accent).withAlpha (0.8f);
        preview->colourFor = [&fw, key, fallback] (const juce::String& id)
        {
            auto* cat = fw.findCategory (key);
            return cat != nullptr ? col (fw.valueColours (*cat, id).colour, fallback) : fallback;
        };
        if (maxWeight > 0)
            preview->heightFor = [&fw, key, maxWeight] (const juce::String& id)
            {
                auto* cat = fw.findCategory (key);
                auto* v = cat != nullptr ? fw.findValue (*cat, id) : nullptr;
                return v == nullptr ? 0.3f : 0.25f + 0.75f * (float) v->weight / (float) maxWeight;
            };
        y += maxWeight > 0 ? 20 : 16;

        auto* header = place (std::make_unique<juce::Label> (juce::String(), "at %          becomes                 over %"), 0, y, w, 15);
        header->setFont (Theme::font (10.5f));
        header->setColour (juce::Label::textColourId, Theme::textMuted);
        header->setBorderSize ({ 0, 0, 0, 0 });
        y += 16;

        for (int i = 0; i < changes.size(); ++i)
        {
            auto* at = place (std::make_unique<juce::TextEditor>(), 0, y, 52, 24);
            at->setFont (Theme::mono (12.0f));
            at->setText (percent (changes[i].at), false);
            at->setTooltip ("Where the change happens, in % of the box");
            auto* value = place (std::make_unique<juce::ComboBox>(), 58, y, w - 58 - 56 - 28, 24);
            for (int k = 0; k < choiceIds.size(); ++k) value->addItem (choiceNames[k], k + 1);
            value->setSelectedId (choiceIds.indexOf (changes[i].to) + 1, juce::dontSendNotification);
            if (value->getSelectedId() == 0) value->setText (changes[i].to + "?", juce::dontSendNotification);
            auto* width = place (std::make_unique<juce::TextEditor>(), w - 56 - 24, y, 50, 24);
            width->setFont (Theme::mono (12.0f));
            width->setText (percent (changes[i].width), false);
            width->setTooltip ("0 = sudden; otherwise the % of the box over which it changes gradually");
            auto* del = place (std::make_unique<juce::TextButton> ("x"), w - 24, y, 24, 24);
            del->setTooltip ("Remove this change");

            auto commit = [this, key, changes, i, at, width, value, choiceIds]
            {
                auto c2 = changes;
                c2.getReference (i).at = juce::jlimit (0.0, 100.0, at->getText().retainCharacters ("0123456789.").getDoubleValue()) / 100.0;
                c2.getReference (i).width = juce::jlimit (0.0, 100.0, width->getText().retainCharacters ("0123456789.").getDoubleValue()) / 100.0;
                const int k = value->getSelectedId() - 1;
                if (k >= 0) c2.getReference (i).to = choiceIds[k];
                owner.setAlong (key, c2);
            };
            at->onReturnKey = commit;
            at->onFocusLost = commit;
            width->onReturnKey = commit;
            width->onFocusLost = commit;
            value->onChange = commit;
            del->onClick = [this, key, changes, i]
            {
                auto c2 = changes;
                c2.remove (i);
                owner.setAlong (key, c2);
            };
            y += 28;
        }

        auto* add = place (std::make_unique<juce::TextButton> ("+ Change"), 0, y, 90, 22);
        add->setColour (juce::TextButton::buttonColourId, Theme::background);
        add->setColour (juce::TextButton::textColourOffId, Theme::textMuted);
        add->onClick = [this, key, changes, base, choiceIds]
        {
            auto c2 = changes;
            const double last = c2.isEmpty() ? 0.0 : c2.getLast().at;
            const auto from = Along::valueAt (base, c2, last);
            c2.add ({ (last + 1.0) / 2.0, choiceIds.isEmpty() ? juce::String() : choiceIds[(choiceIds.indexOf (from) + 1) % choiceIds.size()], 0.0 });
            owner.setAlong (key, c2);
        };
        return y + 26;
    }

    //== Layout helpers ===============================================================
    template <typename C>
    C* place (std::unique_ptr<C> c, int x, int y, int w, int h)
    {
        c->setBounds (x, y, w, h);
        addAndMakeVisible (*c);
        auto* raw = c.get();
        owned.push_back (std::unique_ptr<juce::Component> (c.release()));
        return raw;
    }

    void packageHeading (const juce::String& text, int& y, int w)
    {
        auto* l = place (std::make_unique<juce::Label> (juce::String(), Theme::capitals (text)), 0, y, w, 16);
        l->setFont (Theme::font (10.5f, true));
        l->setColour (juce::Label::textColourId, Theme::accent.withAlpha (0.85f));
        l->setBorderSize ({ 0, 0, 0, 0 });
        y += 20;
    }

    void heading (const juce::String& text, const juce::String& tip, int& y, int w)
    {
        auto* l = place (std::make_unique<juce::Label> (juce::String(), text), 0, y, w, 18);
        l->setFont (Theme::font (12.0f));
        l->setColour (juce::Label::textColourId, Theme::textMuted);
        l->setBorderSize ({ 0, 0, 0, 0 });
        if (tip.isNotEmpty()) l->setTooltip (tip);
        y += 20;
    }

    int note (const juce::String& text, int y, int w)
    {
        auto f = Theme::font (11.0f);
        juce::GlyphArrangement ga;
        ga.addJustifiedText (f, text, 0.0f, 0.0f, (float) w, juce::Justification::topLeft);
        const int h = juce::jmax (15, (int) std::ceil (ga.getBoundingBox (0, -1, true).getHeight()) + 3);
        auto* l = place (std::make_unique<juce::Label> (juce::String(), text), 0, y, w, h);
        l->setFont (f);
        l->setColour (juce::Label::textColourId, Theme::textMuted);
        l->setJustificationType (juce::Justification::topLeft);
        l->setBorderSize ({ 0, 0, 0, 0 });
        l->setMinimumHorizontalScale (1.0f);
        return y + h + 3;
    }

    int frameworksButton (int y, int w)
    {
        const int bw = juce::jmin (120, (w - 4) / 2);
        auto* b = place (std::make_unique<juce::TextButton> ("Frameworks..."), 0, y, bw, 22);
        b->setTooltip ("The framework packages of this project (also in the Framework menu)");
        b->onClick = [this] { if (owner.onShowFrameworks) owner.onShowFrameworks(); };
        auto* d = place (std::make_unique<juce::TextButton> ("Display..."), bw + 4, y, bw, 22);
        d->setTooltip ("How each category shows on the boxes, and the colour mode (also View > Display...)");
        d->onClick = [this] { if (owner.onShowDisplay) owner.onShowDisplay(); };
        return y + 26;
    }

    FrameworkPanel& owner;
    std::vector<std::unique_ptr<juce::Component>> owned;
};

//== FrameworkPanel ============================================================

FrameworkPanel::FrameworkPanel (Project& p, const Framework& f)
    : project (p), framework (f)
{
    content = std::make_unique<Content> (*this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

FrameworkPanel::~FrameworkPanel()
{
    viewport.setViewedComponent (nullptr, false);
}

void FrameworkPanel::setBoxes (const juce::Array<juce::ValueTree>& b)
{
    boxes = b;
    rebuild();
}

void FrameworkPanel::refresh()
{
    // Boxes that were deleted drop out.
    juce::Array<juce::ValueTree> alive;
    for (auto b : boxes) if (b.getParent().isValid()) alive.add (b);
    const bool changed = alive.size() != boxes.size();
    boxes = alive;
    if (! changed && content->isEditing()) return;
    rebuild();
}

void FrameworkPanel::rebuild()
{
    const auto pos = viewport.getViewPosition();
    content->build (juce::jmax (120, viewport.getWidth() - viewport.getScrollBarThickness() - 2));
    viewport.setViewPosition (pos);
}

void FrameworkPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    rebuild();
}

void FrameworkPanel::setValue (const juce::String& key, const juce::var& value, const juce::String& undoName, bool newTransaction)
{
    auto* c = framework.findCategory (key);
    if (c == nullptr) return;
    if (newTransaction) project.getUndoManager().beginNewTransaction (undoName);
    const auto id = value.toString();
    for (auto b : boxes)
    {
        // Only where it applies ("only with": a mixing relation on overlaps).
        if (! project.applies (b, *c) || (c->kind == Framework::Kind::choice && id.isNotEmpty() && ! project.valueApplies (b, *c, id))) continue;
        project.setFrameworkValue (b, *c, value);
    }
}

void FrameworkPanel::setTag (const juce::String& key, const juce::String& tag, bool on)
{
    auto* c = framework.findCategory (key);
    if (c == nullptr) return;
    project.getUndoManager().beginNewTransaction (framework.text (c->name));
    for (auto b : boxes)
    {
        auto tags = Tags::parse (b[c->property()].toString());
        if (on) tags.addIfNotAlreadyThere (tag); else tags.removeString (tag);
        project.setFrameworkValue (b, *c, Tags::format (tags));
    }
}

void FrameworkPanel::setAlong (const juce::String& key, const juce::Array<Along::Change>& changes)
{
    auto* c = framework.findCategory (key);
    if (c == nullptr || boxes.size() != 1) return;
    project.getUndoManager().beginNewTransaction ("Change along the box");
    project.setChangesAlong (boxes.getFirst(), *c, changes);
}
