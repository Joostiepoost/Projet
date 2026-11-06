// Markdown.cpp

#include "Markdown.h"
#include "Theme.h"

namespace Markdown
{

juce::String normaliseUrl (const juce::String& u)
{
    return u.startsWithIgnoreCase ("www.") ? "https://" + u : u;
}

namespace
{
    struct Span { juce::String text; bool bold = false, italic = false; juce::String url; };

    bool isUrlStart (const juce::String& s, int i)
    {
        auto rest = s.substring (i);
        return rest.startsWithIgnoreCase ("https://") || rest.startsWithIgnoreCase ("http://") || rest.startsWithIgnoreCase ("www.");
    }

    /** Inline marks of one line: **bold**, *italic*, [text](url), bare addresses. */
    std::vector<Span> inlineSpans (const juce::String& line)
    {
        std::vector<Span> spans;
        juce::String plain;
        bool bold = false, italic = false;

        auto flush = [&]
        {
            if (plain.isNotEmpty()) { spans.push_back ({ plain, bold, italic, {} }); plain.clear(); }
        };

        const int n = line.length();
        for (int i = 0; i < n;)
        {
            const auto c = line[i];

            if (c == '*' && i + 1 < n && line[i + 1] == '*')        { flush(); bold = ! bold; i += 2; continue; }
            if (c == '*' && i + 1 < n && line[i + 1] != ' ')        { flush(); italic = ! italic; i += 1; continue; }
            if (c == '*' && italic)                                  { flush(); italic = false; i += 1; continue; }

            if (c == '[')
            {
                const int close = line.indexOf (i, "](");
                const int end = close > 0 ? line.indexOfChar (close + 2, ')') : -1;
                if (close > i && end > close)
                {
                    flush();
                    spans.push_back ({ line.substring (i + 1, close), bold, italic, normaliseUrl (line.substring (close + 2, end).trim()) });
                    i = end + 1;
                    continue;
                }
            }

            if ((i == 0 || ! juce::CharacterFunctions::isLetterOrDigit (line[i - 1])) && isUrlStart (line, i))
            {
                int end = i;
                while (end < n && ! juce::CharacterFunctions::isWhitespace (line[end])) ++end;
                while (end > i && juce::String (".,;:)!?").containsChar (line[end - 1])) --end;   // trailing punctuation
                flush();
                const auto url = line.substring (i, end);
                spans.push_back ({ url, bold, italic, normaliseUrl (url) });
                i = end;
                continue;
            }

            plain << juce::String::charToString (c);
            ++i;
        }
        flush();
        return spans;
    }
}

Styled style (const juce::String& markdown, float bodySize, juce::Colour textColour, juce::Colour linkColour)
{
    Styled out;
    out.text.setWordWrap (juce::AttributedString::byWord);
    out.text.setLineSpacing (bodySize * 0.25f);
    int position = 0;

    auto lines = juce::StringArray::fromLines (markdown);
    for (int li = 0; li < lines.size(); ++li)
    {
        auto line = lines[li];
        float size = bodySize;
        bool headingBold = false;
        juce::String prefix;

        if (line.startsWith ("## "))      { size = bodySize * 1.12f; headingBold = true; line = line.substring (3); }
        else if (line.startsWith ("# "))  { size = bodySize * 1.3f;  headingBold = true; line = line.substring (2); }
        else if (line.startsWith ("- ") || line.startsWith ("* ")) { prefix = juce::String::fromUTF8 ("\xe2\x80\xa2  "); line = line.substring (2); }

        if (prefix.isNotEmpty())
        {
            out.text.append (prefix, Theme::font (size), textColour);
            position += prefix.length();
        }

        for (auto& s : inlineSpans (line))
        {
            const bool bold = s.bold || headingBold;
            auto f = Theme::font (size, bold);
            if (s.italic) f = f.italicised();
            if (s.url.isNotEmpty())
            {
                f.setUnderline (true);
                out.links.push_back ({ { position, position + s.text.length() }, s.url });
            }
            out.text.append (s.text, f, s.url.isNotEmpty() ? linkColour : textColour);
            position += s.text.length();
        }

        if (li < lines.size() - 1)
        {
            out.text.append ("\n", Theme::font (size), textColour);
            position += 1;
        }
    }
    return out;
}

void drawPreview (juce::Graphics& g, const juce::String& markdown, juce::Rectangle<float> area, float bodySize, juce::Colour textColour)
{
    if (markdown.trim().isEmpty() || area.getHeight() < bodySize) return;
    auto styled = style (markdown, bodySize, textColour, textColour);
    juce::TextLayout layout;
    layout.createLayout (styled.text, area.getWidth());

    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (area.toNearestInt());
    layout.draw (g, area.withHeight (juce::jmax (area.getHeight(), layout.getHeight())));
}

} // namespace Markdown

//==============================================================================

MarkdownView::MarkdownView()
{
    setRepaintsOnMouseActivity (false);
}

void MarkdownView::setMarkdown (const juce::String& md)
{
    if (md == source && ! styled.text.getText().isEmpty()) return;
    source = md;
    styled = Markdown::style (md, 13.5f, Theme::text, juce::Colour (0xff9db7e0));
    relayout();
}

void MarkdownView::relayout()
{
    layout.createLayout (styled.text, (float) juce::jmax (10, getWidth() - 4));
    repaint();
}

int MarkdownView::getIdealHeight() const
{
    return (int) std::ceil (layout.getHeight()) + 8;
}

void MarkdownView::resized()
{
    relayout();
}

void MarkdownView::paint (juce::Graphics& g)
{
    if (source.trim().isEmpty())
    {
        g.setColour (Theme::textMuted);
        g.setFont (Theme::font (12.5f).italicised());
        g.drawFittedText (placeholder, getLocalBounds().reduced (2), juce::Justification::topLeft, 4);
        return;
    }
    layout.draw (g, getLocalBounds().toFloat().reduced (2.0f, 0.0f).withHeight (juce::jmax ((float) getHeight(), layout.getHeight())));
}

int MarkdownView::linkAt (juce::Point<float> p) const
{
    p -= { 2.0f, 0.0f };
    for (int li = 0; li < layout.getNumLines(); ++li)
    {
        const auto& line = layout.getLine (li);
        if (! line.getLineBoundsY().contains (p.y)) continue;

        for (auto* run : line.runs)
            for (int gi = 0; gi < run->glyphs.size(); ++gi)
            {
                const auto& glyph = run->glyphs.getReference (gi);
                const float x0 = line.lineOrigin.x + glyph.anchor.x;
                if (p.x < x0 || p.x >= x0 + juce::jmax (1.0f, glyph.width)) continue;

                const int charIndex = run->stringRange.getStart() + juce::jmin (gi, run->stringRange.getLength() - 1);
                for (int k = 0; k < (int) styled.links.size(); ++k)
                    if (styled.links[(size_t) k].range.contains (charIndex))
                        return k;
                return -1;
            }
    }
    return -1;
}

void MarkdownView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (linkAt (e.position) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void MarkdownView::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown()) return;
    const int k = linkAt (e.position);
    if (k >= 0)
        juce::URL (styled.links[(size_t) k].url).launchInDefaultBrowser();
}

void MarkdownView::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick) onDoubleClick();
}
