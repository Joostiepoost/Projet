// PackagesPanel.cpp

#include "PackagesPanel.h"
#include "Markdown.h"
#include "Theme.h"

namespace
{
    /** A package's help (help.md), styled, in a call-out. */
    class HelpView : public juce::Component
    {
    public:
        explicit HelpView (const juce::String& markdown)
        {
            view.setMarkdown (markdown);
            addAndMakeVisible (view);
            view.setSize (440, 100);
            setSize (460, juce::jlimit (80, 520, view.getIdealHeight() + 20));
        }
        void paint (juce::Graphics& g) override  { g.fillAll (Theme::panel); }
        void resized() override                  { view.setBounds (getLocalBounds().reduced (10)); }
    private:
        MarkdownView view;
    };
}

class PackagesPanel::Content : public juce::Component
{
public:
    Content (PackagesPanel& o) : owner (o) {}

    void build (int w)
    {
        removeAllChildren();
        owned.clear();
        int y = 0;

        auto* t = place (std::make_unique<juce::Label> (juce::String(), "FRAMEWORKS"), 0, y, w - 70, 22);
        t->setFont (Theme::font (12.0f, true));
        t->setColour (juce::Label::textColourId, Theme::textMuted);
        t->setBorderSize ({ 0, 0, 0, 0 });
        auto* done = place (std::make_unique<juce::TextButton> ("Done"), w - 64, y, 64, 22);
        done->onClick = [this] { if (owner.onDone) owner.onDone(); };
        y += 28;
        y = note ("Packages of categories (energies, levels, types ...). This project keeps its own copies; the library holds them "
                  "for new projects; Projet ships some.", y, w, Theme::textMuted);

        // Language of the names
        {
            auto* l = place (std::make_unique<juce::Label> (juce::String(), "Names in"), 0, y + 2, 70, 20);
            l->setFont (Theme::font (12.0f));
            l->setColour (juce::Label::textColourId, Theme::textMuted);
            l->setBorderSize ({ 0, 0, 0, 0 });
            auto* combo = place (std::make_unique<juce::ComboBox>(), 74, y, juce::jmin (140, w - 74), 24);
            combo->addItem (juce::String::fromUTF8 ("Fran\xc3\xa7" "ais"), 1);
            combo->addItem ("English", 2);
            combo->setSelectedId (owner.language == "en" ? 2 : 1, juce::dontSendNotification);
            combo->onChange = [this, combo] { act ("language", combo->getSelectedId() == 2 ? "en" : "fr"); };
            y += 32;
        }

        for (auto& e : owner.entries) y = card (e, y, w) + 10;
        if (owner.entries.isEmpty()) y = note ("No packages anywhere yet.", y, w, Theme::textMuted);

        // Folders
        y += 4;
        const int bw = (w - 4) / 2;
        auto* add = place (std::make_unique<juce::TextButton> ("Add a package folder..."), 0, y, w, 24);
        add->setTooltip ("A folder with a framework.json (such as Examples/Frameworks/example): copied into the library and this project");
        add->onClick = [this] { act ("add", {}); };
        y += 28;
        auto* reload = place (std::make_unique<juce::TextButton> ("Reload"), 0, y, bw, 24);
        reload->setTooltip ("Reads the project's packages again (after editing them)");
        reload->onClick = [this] { act ("reload", {}); };
        auto* showProject = place (std::make_unique<juce::TextButton> ("Project's folder"), bw + 4, y, w - bw - 4, 24);
        showProject->setTooltip ("Shows this project's Frameworks folder");
        showProject->onClick = [this] { act ("showProject", {}); };
        y += 28;
        auto* showLibrary = place (std::make_unique<juce::TextButton> ("Library folder"), 0, y, bw, 24);
        showLibrary->setTooltip ("Shows the library (Application Support/Projet/Frameworks)");
        showLibrary->onClick = [this] { act ("showLibrary", {}); };
        y += 32;
        y = note ("Edit a package's framework.json to rename, translate or add values (see its help and the README); then Reload.", y, w, Theme::textMuted);
        setSize (w, y + 8);
    }

private:
    int card (const Entry& e, int y, int w)
    {
        const int top = y;
        auto* name = place (std::make_unique<juce::Label> (juce::String(), e.name.isNotEmpty() ? e.name : e.id), 8, y + 6, w - 16, 18);
        name->setFont (Theme::font (13.0f, true));
        name->setColour (juce::Label::textColourId, e.inProject ? Theme::text : Theme::textMuted);
        name->setBorderSize ({ 0, 0, 0, 0 });
        y += 26;

        auto where = [] (const juce::String& place, int version) { return place + " v" + juce::String (version); };
        juce::StringArray places;
        if (e.inProject) places.add (where ("in this project", e.projectVersion));
        else places.add ("not in this project");
        if (e.inLibrary) places.add (where ("library", e.libraryVersion));
        if (e.builtin) places.add (where ("Projet's", e.builtinVersion));
        y = note (e.id + juce::String::fromUTF8 (" \xc2\xb7 ") + places.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")), y, w - 16, Theme::textMuted, 8);
        if (e.about.isNotEmpty()) y = note (e.about, y, w - 16, Theme::text.withAlpha (0.85f), 8);
        if (! e.categories.isEmpty()) y = note ("Categories: " + e.categories.joinIntoString (", "), y, w - 16, Theme::textMuted, 8);
        else if (! e.unreadable) y = note ("No categories (it carries other things, for later stages).", y, w - 16, Theme::textMuted, 8);
        if (e.hasScript) y = note ("Draws glyphs with JavaScript (sandboxed: no files or network, time-limited).", y, w - 16, Theme::textMuted, 8);
        for (auto& p : e.problems) y = note (p, y, w - 16, juce::Colour (0xffe0a060), 8);

        // Actions
        struct Action { juce::String label, action, tip; };
        juce::Array<Action> actions;
        if (e.inProject)
        {
            if (e.inLibrary && e.libraryVersion != e.projectVersion)
                actions.add ({ "Take the library's (v" + juce::String (e.libraryVersion) + ")", "update",
                               "Replaces this project's copy with the library's" });
            actions.add ({ e.inLibrary ? "Copy to the library" : "Put in the library", "toLibrary",
                           "The library keeps this project's version, for new projects and other projects" });
            actions.add ({ "Remove from project", "remove", "This project stops using it (its folder goes to the trash; the values on the boxes stay)" });
        }
        else if (e.inLibrary)
            actions.add ({ "Use in this project", "use", "Copies the library's package into this project" });
        else if (e.builtin)
            actions.add ({ "Use in this project", "useBuiltin", "Puts Projet's package in the library and this project" });
        if (e.builtin && e.inLibrary && e.builtinVersion > e.libraryVersion)
            actions.add ({ "Library: take Projet's v" + juce::String (e.builtinVersion), "install", "Replaces the library's copy with Projet's newer one" });
        if (e.builtin && ! e.inLibrary && e.inProject)
            actions.add ({ "Library: add Projet's", "install", "Puts Projet's package in the library" });
        if (e.inProject || e.inLibrary)
            actions.add ({ "Show folder", "show", "Shows the package's folder (this project's copy, else the library's)" });
        if (e.help.isNotEmpty())
            actions.add ({ "Help", "help", "What the package holds" });

        int x = 8;
        y += 2;
        for (auto& a : actions)
        {
            const int bw = juce::jmin (w - 16, juce::GlyphArrangement::getStringWidthInt (Theme::font (12.0f), a.label) + 22);
            if (x > 8 && x + bw > w - 8) { x = 8; y += 26; }
            auto* b = place (std::make_unique<juce::TextButton> (a.label), x, y, bw, 22);
            b->setTooltip (a.tip);
            const auto action = a.action, id = e.id, help = e.help;
            if (action == "help")
                b->onClick = [b, help]
                {
                    juce::CallOutBox::launchAsynchronously (std::make_unique<HelpView> (help), b->getScreenBounds(), nullptr);
                };
            else
                b->onClick = [this, action, id] { act (action, id); };
            x += bw + 4;
        }
        y += 30;

        auto* frame = place (std::make_unique<Frame> (e.inProject), 0, top, w, y - top);
        frame->toBack();
        return y;
    }

    struct Frame : public juce::Component
    {
        explicit Frame (bool used) : inProject (used)   { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (inProject ? Theme::panel2 : Theme::background.withAlpha (0.6f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (inProject ? Theme::accent.withAlpha (0.5f) : Theme::line);
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }
        bool inProject;
    };

    void act (const juce::String& action, const juce::String& id)
    {
        // The panel is rebuilt by the answer: call after this click has finished.
        juce::Component::SafePointer<PackagesPanel> safe (&owner);
        juce::MessageManager::callAsync ([safe, action, id]
        {
            if (safe != nullptr && safe->onAction) safe->onAction (action, id);
        });
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

    int note (const juce::String& text, int y, int w, juce::Colour colour, int x = 0)
    {
        auto f = Theme::font (11.0f);
        juce::GlyphArrangement ga;
        ga.addJustifiedText (f, text, 0.0f, 0.0f, (float) w, juce::Justification::topLeft);
        const int h = juce::jmax (15, (int) std::ceil (ga.getBoundingBox (0, -1, true).getHeight()) + 3);
        auto* l = place (std::make_unique<juce::Label> (juce::String(), text), x, y, w, h);
        l->setFont (f);
        l->setColour (juce::Label::textColourId, colour);
        l->setJustificationType (juce::Justification::topLeft);
        l->setBorderSize ({ 0, 0, 0, 0 });
        l->setMinimumHorizontalScale (1.0f);
        return y + h + 3;
    }

    PackagesPanel& owner;
    std::vector<std::unique_ptr<juce::Component>> owned;
};

PackagesPanel::PackagesPanel()
{
    content = std::make_unique<Content> (*this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

PackagesPanel::~PackagesPanel()
{
    viewport.setViewedComponent (nullptr, false);
}

void PackagesPanel::setEntries (const juce::Array<Entry>& e, const juce::String& lang)
{
    entries = e;
    language = lang;
    rebuild();
}

void PackagesPanel::rebuild()
{
    const auto pos = viewport.getViewPosition();
    content->build (juce::jmax (160, viewport.getWidth() - viewport.getScrollBarThickness() - 2));
    viewport.setViewPosition (pos);
}

void PackagesPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    rebuild();
}
