// DisplayPanel.cpp

#include "DisplayPanel.h"
#include "Dialogs.h"
#include "Theme.h"

class DisplayPanel::Content : public juce::Component
{
public:
    Content (DisplayPanel& o) : owner (o) {}

    void build (int w)
    {
        removeAllChildren();
        owned.clear();
        auto& fw = owner.framework;
        auto& state = owner.project.getState();
        const auto display = Framework::parsePairs (state[IDs::fwDisplay].toString());
        int y = 0;

        // Title
        auto* t = place (std::make_unique<juce::Label> (juce::String(), "DISPLAY"), 0, y, w - 70, 22);
        t->setFont (Theme::font (12.0f, true));
        t->setColour (juce::Label::textColourId, Theme::textMuted);
        t->setBorderSize ({ 0, 0, 0, 0 });
        auto* done = place (std::make_unique<juce::TextButton> ("Done"), w - 64, y, 64, 22);
        done->onClick = [this] { later ([] (DisplayPanel& panel) { if (panel.onDone) panel.onDone(); }); };
        y += 28;
        y = note ("How the framework shows on the boxes. Kept with the project; changing it is not a change to save.", y, w);
        y += 6;

        // Colour mode
        heading ("Colour boxes by", y, w);
        {
            auto* combo = place (std::make_unique<juce::ComboBox>(), 0, y, w, 24);
            juce::StringArray keys { "own" };
            combo->addItem ("Track colours", 1);
            for (auto& c : fw.getCategories())
            {
                if ((c.kind != Framework::Kind::choice && c.kind != Framework::Kind::tags) || ! fw.hasColours (c)) continue;
                bool ownColours = false;
                for (auto& v : fw.valuesOf (c)) ownColours = ownColours || v.colours.colour != 0;
                const auto name = fw.text (c.name) + (! ownColours && ! fw.source (c).groupsName.isEmpty() ? " (" + fw.text (fw.source (c).groupsName) + ")" : juce::String());
                keys.add (c.key());
                combo->addItem (name, keys.size());
            }
            const auto mode = state.getProperty (IDs::colourMode, "own").toString();
            combo->setSelectedId (juce::jmax (1, keys.indexOf (mode) + 1), juce::dontSendNotification);
            combo->onChange = [this, combo, keys]
            {
                const int i = combo->getSelectedId() - 1;
                if (i < 0) return;
                const auto key = keys[i];
                later ([key] (DisplayPanel& panel) { if (panel.setView) panel.setView (IDs::colourMode, key); });
            };
            y += 30;
        }

        // Marks
        if (fw.getCategories().isEmpty())
            y = note ("This project uses no framework packages with categories.", y, w);
        static const char* markNames[] = { "Not shown", "Strip", "Badge", "Label", "Glyph" };
        for (auto& p : fw.getPackages())
        {
            if (p.categories.isEmpty()) continue;
            packageHeading (fw.text (p.name), y, w);
            for (auto& key : p.categories)
            {
                auto* c = fw.findCategory (key);
                if (c == nullptr) continue;
                auto* l = place (std::make_unique<juce::Label> (juce::String(), fw.text (c->name)), 0, y, w - 124, 24);
                l->setFont (Theme::font (12.0f));
                l->setColour (juce::Label::textColourId, Theme::text);
                l->setBorderSize ({ 0, 0, 0, 0 });
                l->setMinimumHorizontalScale (0.8f);
                auto* combo = place (std::make_unique<juce::ComboBox>(), w - 120, y, 120, 24);
                const auto marks = Framework::marksFor (*c);
                juce::StringArray names;
                for (int i = 0; i < marks.size(); ++i)
                {
                    names.add (Framework::markName (marks[i]));
                    combo->addItem (juce::String (markNames[(int) marks[i]]) + (marks[i] == c->mark ? " (default)" : ""), i + 1);
                }
                combo->setSelectedId (marks.indexOf (Framework::chosenMark (*c, display)) + 1, juce::dontSendNotification);
                const auto def = Framework::markName (c->mark);
                combo->onChange = [this, combo, names, key, def]
                {
                    const int i = combo->getSelectedId() - 1;
                    if (i < 0) return;
                    const auto mark = names[i];
                    later ([key, def, mark] (DisplayPanel& panel)
                    {
                        auto pairs = Framework::parsePairs (panel.project.getState()[IDs::fwDisplay].toString());
                        if (mark == def) pairs.remove (juce::StringRef (key)); else pairs.set (key, mark);
                        if (panel.setView) panel.setView (IDs::fwDisplay, Framework::formatPairs (pairs));
                    });
                };
                y += 28;
            }
            y += 4;
        }

        // Fills and weight
        heading ("Drawing", y, w);
        auto addToggle = [&] (const juce::Identifier& id, const juce::String& text, const juce::String& tip)
        {
            auto* b = place (std::make_unique<juce::ToggleButton> (text), 0, y, w, 22);
            b->setTooltip (tip);
            b->setToggleState (state[id].toString() != "off", juce::dontSendNotification);
            b->onClick = [this, b, id]
            {
                const bool on = b->getToggleState();
                later ([id, on] (DisplayPanel& panel) { if (panel.setView) panel.setView (id, on ? juce::String() : juce::String ("off")); });
            };
            y += 26;
        };
        addToggle (IDs::fwFills, "Fill patterns",
                   "Values with a fill texture their boxes: hatched, dotted, or veiled with an outline (the semiotic levels)");
        addToggle (IDs::fwWeight, "Figure and ground weight",
                   "Weighted values set boxes forward or back: the ground fainter, the figure with a stronger edge (the roles)");
        y += 4;

        // Presets, for all projects
        y += 6;
        heading ("Presets (for all projects)", y, w);
        {
            const auto list = owner.presets();
            auto* combo = place (std::make_unique<juce::ComboBox>(), 0, y, w, 24);
            combo->setTextWhenNothingSelected (list.size() > 0 ? "Choose a preset..." : "No presets yet");
            if (auto* a = list.getArray())
                for (int i = 0; i < a->size(); ++i) combo->addItem ((*a)[i]["name"].toString(), i + 1);
            combo->onChange = [this, combo]
            {
                const int i = combo->getSelectedId() - 1;
                const auto list2 = owner.presets();
                if (i < 0 || ! list2.isArray() || i >= list2.size()) return;
                const auto preset = list2[i];
                if (! preset.isObject()) return;
                const auto presetMode = preset.getProperty ("colourMode", "own").toString(), presetMarks = preset["display"].toString();
                const auto presetFills = preset.getProperty ("fills", "").toString(), presetWeight = preset.getProperty ("weight", "").toString();
                later ([presetMode, presetMarks, presetFills, presetWeight] (DisplayPanel& panel)
                {
                    if (! panel.setView) return;
                    panel.setView (IDs::colourMode, presetMode);
                    panel.setView (IDs::fwDisplay, presetMarks);
                    panel.setView (IDs::fwFills, presetFills);
                    panel.setView (IDs::fwWeight, presetWeight);
                });
            };
            y += 30;
            const int bw = (w - 8) / 3;
            auto* save = place (std::make_unique<juce::TextButton> ("Save as..."), 0, y, bw, 24);
            save->setTooltip ("Keeps the colour mode and the marks as a preset, for all projects");
            save->onClick = [this] { askPresetName(); };
            auto* del = place (std::make_unique<juce::TextButton> ("Delete"), bw + 4, y, bw, 24);
            del->setTooltip ("Deletes the preset chosen above");
            del->onClick = [this, combo]
            {
                const int i = combo->getSelectedId() - 1;
                later ([i] (DisplayPanel& panel)
                {
                    auto list2 = panel.presets();
                    if (auto* a = list2.getArray(); a != nullptr && juce::isPositiveAndBelow (i, a->size()))
                    {
                        a->remove (i);
                        panel.savePresets (list2);
                        panel.refresh();
                    }
                });
            };
            auto* reset = place (std::make_unique<juce::TextButton> ("Defaults"), 2 * (bw + 4), y, w - 2 * (bw + 4), 24);
            reset->setTooltip ("Every category gets its package's mark again; fills and weight come back on");
            reset->onClick = [this]
            {
                later ([] (DisplayPanel& panel)
                {
                    if (! panel.setView) return;
                    panel.setView (IDs::fwDisplay, {});
                    panel.setView (IDs::fwFills, {});
                    panel.setView (IDs::fwWeight, {});
                });
            };
            y += 30;
        }
        y = note ("Strips run along the bottom of the boxes, badges sit at the bottom right, labels at the bottom left. A glyph draws its value's shape in the band above the strips, shaped by the dynamic profile.", y, w);
        setSize (w, y + 8);
    }

private:
    /** Runs once the click or choice that asked for it has finished: what it
        does rebuilds this panel, which deletes the control that asked. */
    void later (std::function<void (DisplayPanel&)> f)
    {
        juce::Component::SafePointer<DisplayPanel> safe (&owner);
        juce::MessageManager::callAsync ([safe, f] { if (safe != nullptr) f (*safe); });
    }

    void askPresetName()
    {
        auto* aw = new juce::AlertWindow ("Save a display preset", "A name for the colour mode and the marks as they are now:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", {}, "Name");
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<DisplayPanel> safe (&owner);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int r)
        {
            if (safe == nullptr || r != 1) return;
            const auto name = aw->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            auto list = safe->presets();
            if (! list.isArray()) list = juce::var (juce::Array<juce::var>());
            auto* item = new juce::DynamicObject();
            item->setProperty ("name", name);
            item->setProperty ("colourMode", safe->project.getState().getProperty (IDs::colourMode, "own").toString());
            item->setProperty ("display", safe->project.getState()[IDs::fwDisplay].toString());
            item->setProperty ("fills", safe->project.getState()[IDs::fwFills].toString());
            item->setProperty ("weight", safe->project.getState()[IDs::fwWeight].toString());
            auto* a = list.getArray();
            for (int i = a->size(); --i >= 0;)
                if ((*a)[i]["name"].toString() == name) a->remove (i);     // same name: replaced
            a->add (juce::var (item));
            safe->savePresets (list);
            safe->refresh();
        }), true);
        Dialogs::focusField (aw, "name");
    }

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

    void packageHeading (const juce::String& text, int& y, int w)
    {
        auto* l = place (std::make_unique<juce::Label> (juce::String(), text.toUpperCase()), 0, y, w, 16);
        l->setFont (Theme::font (10.5f, true));
        l->setColour (juce::Label::textColourId, Theme::accent.withAlpha (0.85f));
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

    DisplayPanel& owner;
    std::vector<std::unique_ptr<juce::Component>> owned;
};

DisplayPanel::DisplayPanel (Project& p, const Framework& f)
    : project (p), framework (f)
{
    content = std::make_unique<Content> (*this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

DisplayPanel::~DisplayPanel()
{
    viewport.setViewedComponent (nullptr, false);
}

void DisplayPanel::refresh()
{
    const auto pos = viewport.getViewPosition();
    content->build (juce::jmax (160, viewport.getWidth() - viewport.getScrollBarThickness() - 2));
    viewport.setViewPosition (pos);
}

void DisplayPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    refresh();
}

juce::var DisplayPanel::presets() const
{
    if (properties == nullptr) return juce::var (juce::Array<juce::var>());
    auto v = juce::JSON::parse (properties->getValue ("displayPresets"));
    return v.isArray() ? v : juce::var (juce::Array<juce::var>());
}

void DisplayPanel::savePresets (const juce::var& list)
{
    if (properties == nullptr) return;
    properties->setValue ("displayPresets", juce::JSON::toString (list, true));
    properties->saveIfNeeded();
}
