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

    //== A strip showing a field along the box (0 .. 1) =========================
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
                // A folder's level (not the box's own) is drawn fainter.
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

    //== Schaeffer's typology chart =============================================
    class TypologyChart : public juce::Component
    {
    public:
        TypologyChart (const Framework& f, const juce::String& current, std::function<void (const juce::String&)> pick)
            : framework (f), onPick (std::move (pick))
        {
            for (auto& t : framework.getTypes())
            {
                auto* b = buttons.add (new juce::TextButton (t.label));
                b->setTooltip (framework.text (t.name));
                auto* z = framework.findZone (t.zone);
                const auto fill = col (z != nullptr ? z->dark : 0u, Theme::panel2);
                b->setColour (juce::TextButton::buttonColourId, fill);
                b->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
                b->setColour (juce::TextButton::textColourOffId, col (z != nullptr ? z->light : 0u, Theme::text));
                b->setColour (juce::TextButton::textColourOnId, Theme::background);
                b->setToggleState (t.id == current, juce::dontSendNotification);
                const auto id = t.id;
                b->onClick = [this, id] { choose (id); };
                addAndMakeVisible (b);
                ids.add (t.id);
            }
            none.setButtonText ("No type");
            none.onClick = [this] { choose ({}); };
            addAndMakeVisible (none);
            setSize (headerW + cols * cellW + 8, headerH + rows * cellH + 40);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel);
            g.setFont (Theme::font (10.5f));
            const auto& sus = framework.getSustainments();
            for (int c = 0; c < cols && c < sus.size(); ++c)
            {
                g.setColour (Theme::textMuted);
                g.drawFittedText (framework.text (sus[c].name), headerW + c * cellW + 2, 4, cellW - 4, headerH - 6,
                                  juce::Justification::centredBottom, 3, 0.9f);
            }
            const auto& masses = framework.getMasses();
            for (int r = 0; r < rows && r < masses.size(); ++r)
            {
                g.setColour (Theme::text);
                g.drawFittedText (framework.text (masses[r].name), 6, headerH + r * cellH, headerW - 10, cellH,
                                  juce::Justification::centredLeft, 2, 0.9f);
            }
            g.setColour (Theme::textMuted);
            g.drawText ("Mass down, sustainment across (Schaeffer, Treatise fig. 34)", 6, getHeight() - 32, getWidth() - 110, 28,
                        juce::Justification::centredLeft, true);
        }

        void resized() override
        {
            // Cells; two types in one cell (the wefts Tn, Tx) share it.
            const auto& types = framework.getTypes();
            for (int i = 0; i < types.size(); ++i)
            {
                int same = 0, index = 0;
                for (int j = 0; j < types.size(); ++j)
                    if (types[j].row == types[i].row && types[j].col == types[i].col)
                    {
                        if (j < i) ++index;
                        ++same;
                    }
                const int x = headerW + types[i].col * cellW, y = headerH + types[i].row * cellH;
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

        static constexpr int cols = 7, rows = 4, cellW = 66, cellH = 34, headerW = 104, headerH = 46;
        const Framework& framework;
        std::function<void (const juce::String&)> onPick;
        juce::OwnedArray<juce::TextButton> buttons;
        juce::StringArray ids;
        juce::TextButton none;
    };
}

void FrameworkPanel::showTypologyChart (const Framework& f, const juce::String& current, juce::Component& near,
                                        std::function<void (const juce::String&)> onPick)
{
    auto chart = std::make_unique<TypologyChart> (f, current, std::move (onPick));
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
            if (auto* t = dynamic_cast<juce::TextEditor*> (c))
                if (t->hasKeyboardFocus (false)) return true;
        return false;
    }

    void build (int w)
    {
        removeAllChildren();
        owned.clear();
        auto& sel = owner.boxes;
        auto& fw = owner.framework;
        int y = 0;
        if (sel.isEmpty()) { setSize (w, 10); return; }
        const bool single = sel.size() == 1;
        auto first = sel.getFirst();

        auto common = [&sel] (const juce::Identifier& id, bool& mixed) -> juce::String
        {
            mixed = false;
            const auto v = sel.getFirst()[id].toString();
            for (auto b : sel) if (b[id].toString() != v) mixed = true;
            return mixed ? juce::String() : v;
        };

        // == Energies
        for (int which = 0; which < 2; ++which)
        {
            const juce::Identifier id = which == 0 ? IDs::energy : IDs::energy2;
            heading (which == 0 ? "Energy" : "Second energy (drawn lighter)", y, w);
            bool mixed = false;
            const auto current = common (id, mixed);
            auto* combo = place (std::make_unique<juce::ComboBox>(), 0, y, w, 24);
            combo->addItem (which == 0 ? "None" : "None", 1);
            juce::StringArray comboIds { {} };
            for (auto& family : fw.getFamilies())
            {
                bool any = false;
                for (auto& e : fw.getEnergies())
                    if (e.families.size() > 0 && e.families[0] == family.id)
                    {
                        if (! any) combo->addSectionHeading (fw.text (family.name));
                        any = true;
                        comboIds.add (e.id);
                        combo->addItem (fw.text (e.name), comboIds.size());
                    }
            }
            for (auto& e : fw.getEnergies())          // energies whose family the file does not list
                if (! comboIds.contains (e.id))
                {
                    comboIds.add (e.id);
                    combo->addItem (fw.text (e.name), comboIds.size());
                }
            if (current.isNotEmpty() && ! comboIds.contains (current))
            {
                comboIds.add (current);
                combo->addItem (current + " (not in the framework file)", comboIds.size());
            }
            if (mixed) combo->setText ("(different energies)", juce::dontSendNotification);
            else combo->setSelectedId (comboIds.indexOf (current) + 1, juce::dontSendNotification);
            combo->onChange = [this, combo, comboIds, id]
            {
                const int i = combo->getSelectedId() - 1;
                if (i < 0) return;
                owner.setField (id, comboIds[i], "Energy");
            };
            y += 28;

            // Families of the chosen energy
            if (auto* e = fw.findEnergy (current))
            {
                int x = 0;
                for (auto& famId : e->families)
                {
                    auto* f = fw.findFamily (famId);
                    const auto name = fw.familyName (famId);
                    const int pw = Pill::widthFor (name);
                    place (std::make_unique<Pill> (name, col (f != nullptr ? f->dark : 0u, Theme::panel2),
                                                   col (f != nullptr ? f->colour : 0u), col (f != nullptr ? f->light : 0u, Theme::text)),
                           x, y, pw, 18);
                    x += pw + 4;
                }
                y += 22;
                if (which == 0)
                {
                    const auto factors = fw.getLanguage() == "en" ? e->factorsEn : e->factorsFr;
                    if (factors.size() > 0)
                        y = note ("Variation factors: " + factors.joinIntoString (", "), y, w);
                }
            }
            y += 6;
        }

        // == Semiotic level
        heading ("Semiotic level", y, w);
        {
            bool mixed = false;
            const auto own = common (IDs::level, mixed);
            bool inherited = false;
            const auto effective = single ? owner.project.effectiveLevel (first, &inherited) : own;
            const auto& levels = fw.getLevels();
            const int n = juce::jmax (1, levels.size());
            const int bw = (w - (n - 1) * 4) / n;
            for (int i = 0; i < levels.size(); ++i)
            {
                const auto& l = levels.getReference (i);
                auto* b = place (std::make_unique<juce::TextButton> (fw.text (l.name)), i * (bw + 4), y, bw, 24);
                const bool on = ! mixed && effective == l.id;
                b->setToggleState (on && ! inherited, juce::dontSendNotification);
                if (on && inherited)
                {
                    b->setColour (juce::TextButton::buttonColourId, juce::Colour (l.colour).withAlpha (0.25f));
                    b->setColour (juce::TextButton::textColourOffId, juce::Colour (l.colour).brighter (0.4f));
                }
                b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (l.colour));
                b->setTooltip (fw.text (l.about) + ". Click again to remove the box's own level.");
                const auto levelId = l.id;
                b->onClick = [this, levelId, own, mixed] { owner.setField (IDs::level, (! mixed && own == levelId) ? juce::String() : levelId, "Semiotic level"); };
            }
            y += 28;
            if (mixed) y = note ("The selected boxes have different levels; a click gives them all one.", y, w);
            else if (inherited)
                y = note ("From the folder " + owner.project.folderOf (first.getParent()).getProperty (IDs::name).toString().quoted()
                          + " (or one around it). Click a level to give the box its own.", y, w);
            else if (effective.isEmpty()) y = note ("None yet.", y, w);
            if (single) y = alongEditor (IDs::level, IDs::levelAlong, y, w);
            else y = note ("Changes along a box: select one box.", y, w);
            y += 6;
        }

        // == Role
        heading ("Role", y, w);
        {
            bool mixed = false;
            const auto own = common (IDs::role, mixed);
            const auto current = own.isEmpty() ? fw.defaultRole() : own;
            const auto& roles = fw.getRoles();
            const int n = juce::jmax (1, roles.size());
            const int bw = (w - (n - 1) * 4) / n;
            for (int i = 0; i < roles.size(); ++i)
            {
                const auto& r = roles.getReference (i);
                const auto label = fw.getRoleNames() == "vandegorne" ? fw.text (r.vandeGorne) : r.letter;
                auto* b = place (std::make_unique<juce::TextButton> (label), i * (bw + 4), y, bw, 24);
                b->setToggleState (! mixed && current == r.id, juce::dontSendNotification);
                b->setTooltip (fw.roleName (r.id));
                const auto roleId = r.id;
                const auto def = fw.defaultRole();
                b->onClick = [this, roleId, def] { owner.setField (IDs::role, roleId == def ? juce::String() : roleId, "Role"); };
            }
            y += 28;
            y = note (mixed ? juce::String ("The selected boxes have different roles.") : fw.roleName (current)
                      + (own.isEmpty() ? " (the default)" : ""), y, w);
            if (single) y = alongEditor (IDs::role, IDs::roleAlong, y, w);
            y += 6;
        }

        // == Type
        heading ("Type (Schaeffer's typology)", y, w);
        {
            bool mixed = false;
            const auto current = common (IDs::typology, mixed);
            auto text = mixed ? juce::String ("(different types)")
                              : current.isEmpty() ? juce::String ("Choose a type...")
                                                  : fw.typeLabel (current) + "   " + fw.typeName (current);
            auto* b = place (std::make_unique<juce::TextButton> (text), 0, y, w - 30, 26);
            if (auto* t = fw.findType (current))
                if (auto* z = fw.findZone (t->zone))
                {
                    b->setColour (juce::TextButton::buttonColourId, juce::Colour (z->dark));
                    b->setColour (juce::TextButton::textColourOffId, juce::Colour (z->light));
                }
            b->onClick = [this, b, current]
            {
                juce::Component::SafePointer<FrameworkPanel> safe (&owner);
                FrameworkPanel::showTypologyChart (owner.framework, current, *b, [safe] (const juce::String& id)
                {
                    if (safe != nullptr) safe->setField (IDs::typology, id, "Type");
                });
            };
            auto* clear = place (std::make_unique<juce::TextButton> ("x"), w - 26, y, 26, 26);
            clear->setTooltip ("No type");
            clear->setEnabled (mixed || current.isNotEmpty());
            clear->onClick = [this] { owner.setField (IDs::typology, {}, "Type"); };
            y += 30;
            if (auto* t = fw.findType (current))
            {
                // Where it sits in the chart: zone, mass, sustainment.
                juce::StringArray parts;
                if (auto* z = fw.findZone (t->zone)) parts.add (fw.text (z->name));
                if (juce::isPositiveAndBelow (t->row, fw.getMasses().size())) parts.add (fw.text (fw.getMasses()[t->row].name));
                if (juce::isPositiveAndBelow (t->col, fw.getSustainments().size())) parts.add (fw.text (fw.getSustainments()[t->col].name));
                y = note (parts.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")), y, w);
            }
        }

        y += 10;
        y = note ("The names, colours and lists come from the project's framework file (Framework menu).", y, w);
        setSize (w, y + 8);
    }

private:
    template <typename C>
    C* place (std::unique_ptr<C> c, int x, int y, int w, int h)
    {
        c->setBounds (x, y, w, h);
        addAndMakeVisible (*c);
        auto* raw = c.get();
        owned.push_back (std::unique_ptr<juce::Component> (c.release()));
        return raw;
    }

    void heading (const juce::String& text, int& y, int w)
    {
        auto* l = place (std::make_unique<juce::Label> (juce::String(), text), 0, y, w, 18);
        l->setFont (Theme::font (12.0f));
        l->setColour (juce::Label::textColourId, Theme::textMuted);
        l->setBorderSize ({ 0, 0, 0, 0 });
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

    /** The changes of a field along the (one) selected box. */
    int alongEditor (const juce::Identifier& baseId, const juce::Identifier& alongId, int y, int w)
    {
        auto& fw = owner.framework;
        auto box = owner.boxes.getFirst();
        const bool isLevel = baseId == IDs::level;
        auto changes = Project::changesAlong (box, alongId);

        juce::StringArray choiceIds, choiceNames;
        if (isLevel) for (auto& l : fw.getLevels()) { choiceIds.add (l.id); choiceNames.add (fw.text (l.name)); }
        else for (auto& r : fw.getRoles()) { choiceIds.add (r.id); choiceNames.add (fw.roleName (r.id, true)); }

        bool inherited = false;
        auto base = isLevel ? owner.project.effectiveLevel (box, &inherited) : box[IDs::role].toString();
        if (! isLevel && base.isEmpty()) base = fw.defaultRole();

        if (changes.isEmpty())
        {
            auto* add = place (std::make_unique<juce::TextButton> ("+ Change along the box"), 0, y, w, 22);
            add->setColour (juce::TextButton::buttonColourId, Theme::background);
            add->setColour (juce::TextButton::textColourOffId, Theme::textMuted);
            add->setTooltip ("From a point in the box on, another " + juce::String (isLevel ? "level" : "role")
                             + " (suddenly, or gradually over a width)");
            add->onClick = [this, alongId, base, choiceIds]
            {
                auto next = choiceIds.isEmpty() ? juce::String() : choiceIds[(choiceIds.indexOf (base) + 1) % choiceIds.size()];
                owner.setAlong (alongId, { { 0.5, next, 0.0 } });
            };
            return y + 26;
        }

        auto* preview = place (std::make_unique<AlongPreview>(), 0, y, w, isLevel ? 12 : 16);
        preview->base = base;
        preview->baseInherited = inherited;
        preview->changes = changes;
        if (isLevel)
            preview->colourFor = [&fw] (const juce::String& id) { return col (fw.levelColour (id)); };
        else
        {
            preview->colourFor = [] (const juce::String&) { return Theme::accent.withAlpha (0.8f); };
            preview->heightFor = [&fw] (const juce::String& id)
            {
                auto* r = fw.findRole (id);
                int maxW = 1;
                for (auto& x : fw.getRoles()) maxW = juce::jmax (maxW, x.weight);
                return r == nullptr ? 0.3f : 0.25f + 0.75f * (float) r->weight / (float) maxW;
            };
        }
        y += isLevel ? 16 : 20;

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

            auto commit = [this, alongId, changes, i, at, width, value, choiceIds]
            {
                auto c = changes;
                c.getReference (i).at = juce::jlimit (0.0, 100.0, at->getText().retainCharacters ("0123456789.").getDoubleValue()) / 100.0;
                c.getReference (i).width = juce::jlimit (0.0, 100.0, width->getText().retainCharacters ("0123456789.").getDoubleValue()) / 100.0;
                const int k = value->getSelectedId() - 1;
                if (k >= 0) c.getReference (i).to = choiceIds[k];
                owner.setAlong (alongId, c);
            };
            at->onReturnKey = commit;
            at->onFocusLost = commit;
            width->onReturnKey = commit;
            width->onFocusLost = commit;
            value->onChange = commit;
            del->onClick = [this, alongId, changes, i]
            {
                auto c = changes;
                c.remove (i);
                owner.setAlong (alongId, c);
            };
            y += 28;
        }

        auto* add = place (std::make_unique<juce::TextButton> ("+ Change"), 0, y, 90, 22);
        add->setColour (juce::TextButton::buttonColourId, Theme::background);
        add->setColour (juce::TextButton::textColourOffId, Theme::textMuted);
        add->onClick = [this, alongId, changes, base, choiceIds]
        {
            auto c = changes;
            const double last = c.isEmpty() ? 0.0 : c.getLast().at;
            const auto from = Along::valueAt (base, c, last);
            c.add ({ (last + 1.0) / 2.0, choiceIds.isEmpty() ? juce::String() : choiceIds[(choiceIds.indexOf (from) + 1) % choiceIds.size()], 0.0 });
            owner.setAlong (alongId, c);
        };
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

void FrameworkPanel::setField (const juce::Identifier& id, const juce::String& value, const juce::String& undoName)
{
    project.getUndoManager().beginNewTransaction (undoName);
    for (auto b : boxes)
    {
        if (value.isEmpty()) b.removeProperty (id, project.um());
        else b.setProperty (id, value, project.um());
    }
}

void FrameworkPanel::setAlong (const juce::Identifier& alongId, const juce::Array<Along::Change>& changes)
{
    if (boxes.size() != 1) return;
    project.getUndoManager().beginNewTransaction ("Change along the box");
    project.setChangesAlong (boxes.getFirst(), alongId, changes);
}
