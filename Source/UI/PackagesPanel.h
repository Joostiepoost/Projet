// PackagesPanel.h — the framework packages (stage 4b): Framework > Frameworks...
//
// In the panel on the right: every package this project uses, the library
// has, or Projet ships, with its versions in each place, its categories and
// any problems reading it. Its buttons ask the main window to use a package
// in the project, remove it, take the library's version, copy the project's
// to the library, install Projet's, show its folder or its help; and to add a
// package folder, reload, or show the folders.

#pragma once

#include <JuceHeader.h>

class PackagesPanel : public juce::Component
{
public:
    struct Entry
    {
        juce::String id, name, about, help;
        juce::StringArray categories;           // "Énergie (choice)", ...
        juce::StringArray problems;             // could not be read, warnings
        bool inProject = false, inLibrary = false, builtin = false;
        int projectVersion = 0, libraryVersion = 0, builtinVersion = 0;
        int order = 50;                         // the package's place among the others
        bool unreadable = false;                // its framework.json could not be read
    };

    PackagesPanel();
    ~PackagesPanel() override;

    void setEntries (const juce::Array<Entry>&, const juce::String& language);
    void resized() override;

    std::function<void()> onDone;
    /** use, remove, update, toLibrary, install, useBuiltin, show, add, reload,
        showProject, showLibrary, language (id = "fr" / "en"). */
    std::function<void (const juce::String& action, const juce::String& id)> onAction;

private:
    class Content;
    juce::Array<Entry> entries;
    juce::String language { "fr" };
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    void rebuild();
};
