// TakesMenu.cpp — see TakesMenu.h.

#include "TakesMenu.h"
#include "Dialogs.h"

namespace TakesMenu
{
    namespace
    {
        constexpr int switchFirst = 1, copyTake = 500, soundFile = 501, emptyText = 502, emptyContainer = 503, emptyBoard = 504,
                      renameTake = 510, deleteTake = 511, showTakes = 520, soundFirst = 1000;

        juce::Array<juce::ValueTree> soundsByName (Project& project)
        {
            juce::Array<juce::ValueTree> sounds;
            for (auto s : project.getSounds()) sounds.add (s);
            std::sort (sounds.begin(), sounds.end(), [] (const juce::ValueTree& a, const juce::ValueTree& b)
                       { return a[IDs::name].toString().compareNatural (b[IDs::name].toString()) < 0; });
            return sounds;
        }
    }

    juce::String describe (const juce::ValueTree& take)
    {
        const auto note = take[IDs::takeNote].toString();
        return take[IDs::name].toString() + (note.isEmpty() ? juce::String() : ": " + note);
    }

    juce::PopupMenu build (Project& project, const juce::ValueTree& box, int base, bool listTakes)
    {
        juce::PopupMenu m;
        const auto takes = Project::takesOf (box);
        const auto active = Project::activeTakeOf (box);
        if (listTakes && takes.size() > 1)
        {
            m.addSectionHeader ("Takes");
            for (int i = 0; i < takes.size() && i < 400; ++i)
            {
                auto kind = Project::takeKind (box, takes[i]);
                m.addItem (base + switchFirst + i, describe (takes[i]) + "   (" + (kind == "container" ? "container" : (kind == "text" ? "text" : (kind == "board" ? "board" : "sound"))) + ")",
                           true, takes[i] == active);
            }
        }
        m.addSectionHeader ("New take");
        const auto name = takes.isEmpty() ? juce::String ("this one") : "take " + active[IDs::name].toString();
        m.addItem (base + copyTake, "A copy of " + name);
        juce::PopupMenu sounds;
        const auto list = soundsByName (project);
        for (int i = 0; i < list.size() && i < 999; ++i)
            sounds.addItem (base + soundFirst + i, list[i][IDs::name].toString());
        m.addSubMenu ("A sound from the project", sounds, ! list.isEmpty());
        m.addItem (base + soundFile, "A sound file...");
        m.addItem (base + emptyText, "An empty text");
        m.addItem (base + emptyContainer, "An empty container");
        m.addItem (base + emptyBoard, "An empty board");
        if (takes.size() > 1)
        {
            m.addSeparator();
            m.addItem (base + renameTake, "Rename take " + active[IDs::name].toString() + "...");
            m.addItem (base + deleteTake, "Delete take " + active[IDs::name].toString());
        }
        m.addSeparator();
        m.addItem (base + showTakes, "Show takes in boxes (stacked)", true, (bool) project.getState().getProperty (IDs::showTakes, false));
        return m;
    }

    bool perform (Project& project, juce::ValueTree box, int result, int base, const Actions& actions, juce::Component* dialogParent)
    {
        const int r = result - base;
        if (result <= 0 || r < switchFirst || r >= 2000 || ! box.isValid()) return false;
        auto& um = project.getUndoManager();
        const auto takes = Project::takesOf (box);
        auto active = Project::activeTakeOf (box);

        if (r < copyTake)
        {
            const auto take = takes[r - switchFirst];
            if (take.isValid() && take != active)
            {
                um.beginNewTransaction ("Take " + take[IDs::name].toString());
                project.setActiveTake (box, take);
            }
        }
        else if (r == copyTake)      { um.beginNewTransaction ("New take"); project.addTake (box, Project::NewTake::copy); }
        else if (r == emptyText)     { um.beginNewTransaction ("New take"); project.addTake (box, Project::NewTake::text); }
        else if (r == emptyContainer){ um.beginNewTransaction ("New take"); project.addTake (box, Project::NewTake::container); }
        else if (r == emptyBoard)    { um.beginNewTransaction ("New take"); project.addTake (box, Project::NewTake::board); }
        else if (r == soundFile)     { if (actions.addSoundFiles) actions.addSoundFiles (box); }
        else if (r == showTakes)     { if (actions.toggleShowTakes) actions.toggleShowTakes(); }
        else if (r >= soundFirst)
        {
            const auto list = soundsByName (project);
            const auto sound = list[r - soundFirst];
            if (sound.isValid())
            {
                um.beginNewTransaction ("New take");
                project.addTake (box, Project::NewTake::sound, sound[IDs::id].toString());
            }
        }
        else if (r == deleteTake && takes.size() > 1)
        {
            um.beginNewTransaction ("Delete take " + active[IDs::name].toString());
            project.removeTake (box, active);
        }
        else if (r == renameTake && takes.size() > 1)
        {
            auto* w = new juce::AlertWindow ("Rename take " + active[IDs::name].toString(),
                                             "A short name (a letter, or a word such as dry or long)", juce::MessageBoxIconType::NoIcon, dialogParent);
            w->addTextEditor ("name", active[IDs::name].toString());
            w->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            Dialogs::focusField (w, "name");
            Project* p = &project;
            w->enterModalState (true, juce::ModalCallbackFunction::create ([p, w, active] (int ok) mutable
            {
                if (ok != 1) return;
                p->getUndoManager().beginNewTransaction ("Rename take");
                p->setTakeName (active, w->getTextEditorContents ("name"));
            }), true);
        }
        return true;
    }

    void show (Project& project, juce::ValueTree box, juce::Component* target, bool atMouse, bool listTakes,
               const Actions& actions, std::function<void()> afterwards)
    {
        constexpr int base = 10000;
        auto menu = build (project, box, base, listTakes);
        juce::Component::SafePointer<juce::Component> safe (target);
        Project* p = &project;
        menu.showMenuAsync (atMouse ? juce::PopupMenu::Options() : juce::PopupMenu::Options().withTargetComponent (target),
                            [p, box, actions, afterwards, safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            perform (*p, box, r, base, actions, safe->getTopLevelComponent());
            if (afterwards) afterwards();
        });
    }
}
