// Markdown.h — the notes of a box are plain text with a few marks, so they
// stay readable anywhere (also in Reaper item notes later):
//
//   # Heading            ## Smaller heading
//   **bold**   *italic*
//   - bullet  (or * bullet)
//   [text](https://...)  or a bare https://... / www... address
//
// Markdown::layout turns that into styled text with link ranges; MarkdownView
// shows it and opens links when clicked.

#pragma once

#include <JuceHeader.h>

namespace Markdown
{
    struct Link
    {
        juce::Range<int> range;     // characters in the laid-out text
        juce::String url;
    };

    struct Styled
    {
        juce::AttributedString text;
        std::vector<Link> links;
    };

    /** Styles `markdown`. `bodySize` is the normal font height. */
    Styled style (const juce::String& markdown, float bodySize, juce::Colour textColour, juce::Colour linkColour);

    /** The first lines of the notes, styled, for drawing inside a box. */
    void drawPreview (juce::Graphics&, const juce::String& markdown, juce::Rectangle<float> area,
                      float bodySize, juce::Colour textColour);

    /** "https://..." for "www..." addresses. */
    juce::String normaliseUrl (const juce::String&);
}

/** Read-only, styled view of Markdown notes; links open in the browser. */
class MarkdownView : public juce::Component
{
public:
    MarkdownView();

    void setMarkdown (const juce::String&);
    /** Height the text needs at the current width. */
    int getIdealHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void()> onDoubleClick;
    juce::String placeholder = "No notes yet. Double-click or press Write to add some.";

private:
    int linkAt (juce::Point<float>) const;
    void relayout();

    juce::String source;
    Markdown::Styled styled;
    juce::TextLayout layout;
};
