// OutlinePanel.h — the panel on the left (stage 5b): the piece's containers
// and boxes as a tree in time order, indented by depth, with the depth
// number. The level shown and the selected boxes are highlighted. Click a
// row to select the box (going to its level), double-click a container to
// go inside it; the triangles fold containers.

#pragma once

#include <JuceHeader.h>
#include <set>
#include "../Model/Project.h"

class OutlinePanel : public juce::Component
{
public:
    explicit OutlinePanel (Project&);

    /** The project changed: the rows are worked out again when next painted. */
    void refresh();
    /** The timeline's selection (box ids). */
    void setSelected (const juce::StringArray& ids);
    /** Scrolls so the level shown is in view. */
    void showFocus();

    std::function<void (juce::ValueTree box)> onSelect;          // invalid: the piece
    std::function<void (juce::ValueTree container)> onDiveIn;    // invalid: the piece
    std::function<void()> onHide;

    static constexpr int preferredWidth = 230;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        juce::ValueTree box;          // invalid: the piece
        int depth = 0;
        bool container = false;
        juce::Colour colour;
    };

    class Rows : public juce::Component
    {
    public:
        explicit Rows (OutlinePanel& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
    private:
        OutlinePanel& owner;
    };

    Project& project;
    Rows rows { *this };
    juce::Viewport viewport;
    juce::TextButton hideButton { juce::String::fromUTF8 ("\xc2\xab") };
    std::vector<Row> list;
    std::set<juce::String> folded;
    juce::StringArray selected;
    bool stale = true;

    static constexpr int rowHeight = 21, titleHeight = 34;
    void rebuild();
    void addLevel (const juce::ValueTree& tracks, int depth);
    int indentFor (int depth) const       { return 26 + depth * 13; }
};
