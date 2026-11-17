// BoardPainter.cpp — see BoardPainter.h.

#include "BoardPainter.h"
#include "Markdown.h"
#include "Theme.h"

namespace BoardPainter
{
    namespace
    {
        constexpr float cardPad = 12.0f, labelSize = 11.0f, bodySize = 14.0f;

        float num (const juce::ValueTree& t, const juce::Identifier& id, float fallback)
        {
            return t.hasProperty (id) ? (float) (double) t[id] : fallback;
        }

        void strokeMaybeDashed (juce::Graphics& g, const juce::Path& path, float width, bool dashed, bool rounded = false)
        {
            juce::PathStrokeType stroke (width, rounded ? juce::PathStrokeType::curved : juce::PathStrokeType::mitered,
                                         rounded ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt);
            if (! dashed) { g.strokePath (path, stroke); return; }
            juce::Path out;
            const float dashes[] = { 6.0f + width * 2.0f, 5.0f + width };
            stroke.createDashedStroke (out, path, dashes, 2);
            g.fillPath (out);
        }

        juce::String labelText (const juce::ValueTree& card)
        {
            return Theme::capitals (card[IDs::label].toString().trim());
        }

        void paintFill (juce::Graphics& g, const juce::ValueTree& card, juce::Rectangle<float> r, const Context& ctx)
        {
            if (! ctx.fills || ctx.framework == nullptr) return;
            auto* c = fillCategory (*ctx.framework);
            if (c == nullptr) return;
            const auto value = card[c->property()].toString();
            auto* v = ctx.framework->findValue (*c, value);
            if (v == nullptr || v->fill.isEmpty()) return;
            const auto cc = ctx.framework->valueColours (*c, value).colour;
            const auto colour = cc != 0 ? juce::Colour (cc) : Theme::accent;
            juce::Graphics::ScopedSaveState s (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 6.0f);
            g.reduceClipRegion (clip);
            if (v->fill == "hatched")
            {
                g.setColour (colour.withAlpha (0.14f * ctx.alpha));
                for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 9.0f)
                    g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
            }
            else if (v->fill == "dotted")
            {
                g.setColour (colour.withAlpha (0.18f * ctx.alpha));
                for (float y = r.getY() + 5.5f; y < r.getBottom() - 1.0f; y += 11.0f)
                    for (float x = r.getX() + 5.5f; x < r.getRight() - 1.0f; x += 11.0f)
                        g.fillEllipse (x - 1.3f, y - 1.3f, 2.6f, 2.6f);
            }
            else if (v->fill == "outline")
            {
                g.setColour (colour.withAlpha (0.45f * ctx.alpha));
                g.drawRoundedRectangle (r.reduced (3.0f), 4.0f, 1.0f);
            }
        }

        void paintCard (juce::Graphics& g, const juce::ValueTree& card, const Context& ctx)
        {
            const auto r = Board::rect (card);
            const auto edge = Board::colourOf (card);
            g.setColour (cardPaper (edge).withMultipliedAlpha (ctx.alpha));
            g.fillRoundedRectangle (r, 6.0f);
            paintFill (g, card, r, ctx);
            g.setColour (edge.withMultipliedAlpha (ctx.alpha));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.reduced (2.0f).toNearestInt());
            auto inner = r.reduced (cardPad, 9.0f);
            const auto label = labelText (card);
            if (label.isNotEmpty())
            {
                g.setColour (edge.interpolatedWith (Theme::text, 0.55f).withMultipliedAlpha (ctx.alpha));
                g.setFont (Theme::font (labelSize, true).withExtraKerningFactor (0.06f));
                g.drawText (label, inner.removeFromTop (16.0f), juce::Justification::topLeft, true);
                inner.removeFromTop (2.0f);
            }
            if (ctx.skipText == card[IDs::id].toString()) return;
            const auto md = card[IDs::notes].toString();
            if (md.trim().isEmpty()) return;
            auto styled = Markdown::style (md, bodySize, Theme::text.withMultipliedAlpha (ctx.alpha), juce::Colour (0xff9db7e0).withMultipliedAlpha (ctx.alpha));
            juce::TextLayout layout;
            layout.createLayout (styled.text, juce::jmax (10.0f, inner.getWidth()));
            layout.draw (g, inner.withHeight (juce::jmax (inner.getHeight(), layout.getHeight())));
        }

        void paintShape (juce::Graphics& g, const juce::ValueTree& shape, const Context& ctx)
        {
            const auto colour = Board::colourOf (shape).withMultipliedAlpha (ctx.alpha);
            const float width = num (shape, IDs::lineWidth, 1.5f);
            const bool dashed = (bool) shape[IDs::dashed];
            const auto kind = shape[IDs::shape].toString();
            juce::Path p;
            const auto r = Board::rect (shape);
            if (kind == "line")
            {
                p.startNewSubPath (r.getX(), r.getY());
                p.lineTo (r.getRight(), r.getBottom());
            }
            else if (kind == "ellipse") p.addEllipse (r);
            else p.addRoundedRectangle (r, 4.0f);
            if ((bool) shape[IDs::filled] && kind != "line")
            {
                g.setColour (colour.withMultipliedAlpha (0.2f));
                g.fillPath (p);
            }
            g.setColour (colour);
            strokeMaybeDashed (g, p, width, dashed, kind == "line");
            const auto label = shape[IDs::label].toString();
            if (label.isNotEmpty())
            {
                const auto b = Board::bounds ({}, shape);
                g.setColour (colour.interpolatedWith (Theme::text, 0.5f));
                g.setFont (Theme::font (13.0f).italicised());
                if (kind == "line")
                    g.drawText (label, juce::Rectangle<float> (200.0f, 18.0f).withCentre (b.getCentre().translated (0.0f, -12.0f)), juce::Justification::centred, false);
                else
                    g.drawFittedText (label, b.reduced (6.0f).toNearestInt(), juce::Justification::centred, 3);
            }
        }

        void paintStroke (juce::Graphics& g, const juce::ValueTree& stroke, const Context& ctx)
        {
            const auto pts = Board::strokePoints (stroke);
            if (pts.isEmpty()) return;
            const float width = num (stroke, IDs::lineWidth, 2.0f);
            g.setColour (Board::colourOf (stroke).withMultipliedAlpha (ctx.alpha));
            if (pts.size() == 1) { g.fillEllipse (juce::Rectangle<float> (width, width).withCentre (pts[0])); return; }
            juce::Path p;
            p.startNewSubPath (pts[0]);
            // Smoothed through the midpoints, so the drawing is not angular.
            for (int i = 1; i < pts.size() - 1; ++i)
                p.quadraticTo (pts[i], (pts[i] + pts[i + 1]) * 0.5f);
            p.lineTo (pts.getLast());
            strokeMaybeDashed (g, p, width, (bool) stroke[IDs::dashed], true);
        }

        void paintArrow (juce::Graphics& g, const juce::ValueTree& board, const juce::ValueTree& arrow, const Context& ctx)
        {
            const auto ap = Board::arrowPath (board, arrow);
            const auto colour = Board::colourOf (arrow).withMultipliedAlpha (ctx.alpha);
            const float width = num (arrow, IDs::lineWidth, 1.5f);
            g.setColour (colour);
            // The line stops short of the head, which sits on the end.
            const float headLen = 9.0f + width * 2.0f;
            auto dir = ap.b - ap.control;
            if (dir.getDistanceFromOrigin() < 0.5f) dir = ap.b - ap.a;
            const float len = juce::jmax (1.0e-3f, dir.getDistanceFromOrigin());
            dir /= len;
            juce::Path line;
            line.startNewSubPath (ap.a);
            line.quadraticTo (ap.control, ap.b - dir * (headLen * 0.6f));
            strokeMaybeDashed (g, line, width, (bool) arrow[IDs::dashed], true);
            const juce::Point<float> normal (-dir.y, dir.x);
            juce::Path head;
            head.addTriangle (ap.b, ap.b - dir * headLen + normal * (headLen * 0.45f), ap.b - dir * headLen - normal * (headLen * 0.45f));
            g.fillPath (head);
            const auto label = arrow[IDs::label].toString();
            if (label.isNotEmpty())
            {
                const auto font = Theme::font (11.0f);
                const float w = (float) juce::GlyphArrangement::getStringWidthInt (font, label) + 14.0f;
                const auto pill = juce::Rectangle<float> (w, 18.0f).withCentre (ap.middle());
                g.setColour (juce::Colour (0xff17181b).withMultipliedAlpha (ctx.alpha));
                g.fillRoundedRectangle (pill, 9.0f);
                g.setColour (Theme::lineStrong.withMultipliedAlpha (ctx.alpha));
                g.drawRoundedRectangle (pill.reduced (0.5f), 9.0f, 1.0f);
                g.setColour (Theme::text.withMultipliedAlpha (ctx.alpha));
                g.setFont (font);
                g.drawText (label, pill, juce::Justification::centred, false);
            }
        }

        void paintImage (juce::Graphics& g, const juce::ValueTree& item, const Context& ctx)
        {
            const auto r = Board::rect (item);
            const auto image = imageFor (item, ctx.projectFolder);
            if (image.isValid())
            {
                g.setOpacity (ctx.alpha);
                g.drawImage (image, r, juce::RectanglePlacement::centred);
                g.setOpacity (1.0f);
            }
            else
            {
                g.setColour (juce::Colour (0xff1a1b1f).withMultipliedAlpha (ctx.alpha));
                g.fillRoundedRectangle (r, 4.0f);
                juce::Path p, dashed;
                p.addRoundedRectangle (r.reduced (0.5f), 4.0f);
                const float dashes[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dashes, 2);
                g.setColour (Theme::lineStrong.withMultipliedAlpha (ctx.alpha));
                g.fillPath (dashed);
                g.setColour (Theme::textMuted.withMultipliedAlpha (ctx.alpha));
                g.setFont (Theme::font (12.0f).italicised());
                g.drawFittedText ("Image not found:\n" + item[IDs::file].toString(), r.reduced (8.0f).toNearestInt(), juce::Justification::centred, 4);
            }
            const auto caption = item[IDs::caption].toString();
            if (caption.isNotEmpty())
            {
                g.setColour (Theme::textMuted.withMultipliedAlpha (ctx.alpha));
                g.setFont (Theme::font (12.0f));
                g.drawFittedText (caption, juce::Rectangle<float> (r.getX(), r.getBottom() + 4.0f, juce::jmax (120.0f, r.getWidth()), 32.0f).toNearestInt(),
                                  juce::Justification::topLeft, 2);
            }
        }
    }

    const Framework::Category* fillCategory (const Framework& framework)
    {
        for (auto& c : framework.getCategories())
        {
            if (c.kind != Framework::Kind::choice) continue;
            for (auto& v : framework.valuesOf (c))
                if (v.fill.isNotEmpty()) return &c;
        }
        return nullptr;
    }

    juce::Colour cardPaper (juce::Colour edge)
    {
        return juce::Colour (0xff17181b).interpolatedWith (edge, 0.2f);
    }

    float cardTextHeight (const juce::ValueTree& card)
    {
        const auto r = Board::rect (card);
        float h = 18.0f;
        if (card[IDs::label].toString().trim().isNotEmpty()) h += 18.0f;
        const auto md = card[IDs::notes].toString();
        if (md.trim().isNotEmpty())
        {
            auto styled = Markdown::style (md, bodySize, Theme::text, Theme::text);
            juce::TextLayout layout;
            layout.createLayout (styled.text, juce::jmax (10.0f, r.getWidth() - 2.0f * cardPad));
            h += layout.getHeight();
        }
        return h + 4.0f;
    }

    juce::Image imageFor (const juce::ValueTree& item, const juce::File& projectFolder)
    {
        const auto path = item[IDs::file].toString();
        if (path.isEmpty() || projectFolder == juce::File()) return {};
        const auto f = projectFolder.getChildFile (path);
        if (! f.existsAsFile()) return {};
        return juce::ImageCache::getFromFile (f);
    }

    void paintItem (juce::Graphics& g, const juce::ValueTree& board, const juce::ValueTree& item, const Context& ctx)
    {
        switch (Board::kindOf (item))
        {
            case Board::Kind::card:   paintCard (g, item, ctx); break;
            case Board::Kind::shape:  paintShape (g, item, ctx); break;
            case Board::Kind::stroke: paintStroke (g, item, ctx); break;
            case Board::Kind::arrow:  paintArrow (g, board, item, ctx); break;
            case Board::Kind::image:  paintImage (g, item, ctx); break;
            case Board::Kind::unknown: break;
        }
    }

    void paint (juce::Graphics& g, const juce::ValueTree& board, const Context& ctx)
    {
        for (auto item : board)
            paintItem (g, board, item, ctx);
    }
}
