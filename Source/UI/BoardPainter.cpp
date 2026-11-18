// BoardPainter.cpp — see BoardPainter.h.

#include "BoardPainter.h"
#include "Markdown.h"
#include "Theme.h"
#include "../Model/Factors.h"
#include <map>

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

    namespace
    {
        void paintSoundCard (juce::Graphics& g, const juce::ValueTree& card, const Context& ctx)
        {
            const auto r = Board::rect (card);
            const auto edge = Board::colourOf (card);
            const bool out = Board::isTemporalOut (card);
            g.setColour (cardPaper (edge).withMultipliedAlpha (ctx.alpha));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (edge.withMultipliedAlpha (ctx.alpha));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, out ? 2.0f : 1.0f);

            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.reduced (1.0f).toNearestInt());
            const auto soundId = card[IDs::soundId].toString();
            auto sound = ctx.pool != nullptr ? ctx.pool->get (soundId) : nullptr;
            const double seconds = sound != nullptr ? sound->seconds() : (ctx.project != nullptr ? ctx.project->soundSeconds (soundId) : 0.0);
            const double from = Board::cardIn (card), to = Board::cardOut (card, seconds);
            const bool hearing = ctx.previewCard.isNotEmpty() && ctx.previewCard == card[IDs::id].toString();

            // The play button, the name, the length.
            const auto play = playButton (card);
            g.setColour (edge.darker (0.6f).withMultipliedAlpha (ctx.alpha));
            g.fillEllipse (play);
            g.setColour (edge.brighter (0.5f).withMultipliedAlpha (ctx.alpha));
            g.drawEllipse (play.reduced (0.5f), 1.0f);
            if (hearing) g.fillRect (play.withSizeKeepingCentre (8.0f, 8.0f));
            else
            {
                juce::Path tri;
                const auto c = play.getCentre();
                tri.addTriangle (c.x - 3.5f, c.y - 5.0f, c.x - 3.5f, c.y + 5.0f, c.x + 5.5f, c.y);
                g.fillPath (tri);
            }
            auto head = juce::Rectangle<float> (play.getRight() + 8.0f, r.getY() + 8.0f, r.getRight() - play.getRight() - 18.0f, 24.0f);
            g.setFont (Theme::font (11.0f).withExtraKerningFactor (0.0f));
            g.setColour (Theme::text.withMultipliedAlpha (0.75f * ctx.alpha));
            const auto length = Board::formatSeconds (juce::jmax (0.0, to - from));
            const float lw = (float) juce::GlyphArrangement::getStringWidthInt (Theme::font (11.0f), length) + 4.0f;
            g.drawText (length, head.removeFromRight (lw), juce::Justification::centredRight, false);
            g.setColour (Theme::text.withMultipliedAlpha (ctx.alpha));
            g.setFont (Theme::font (13.0f, true));
            g.drawText (soundCardName (card, ctx.project), head, juce::Justification::centredLeft, true);

            // The waveform of the whole sound, its fragment bright.
            const auto wave = waveArea (card);
            g.setColour (juce::Colour (0xff101113).withMultipliedAlpha (0.6f * ctx.alpha));
            g.fillRoundedRectangle (wave, 3.0f);
            if (sound != nullptr && ! sound->peakMin.empty() && seconds > 0.0)
            {
                const auto blocks = (int64_t) sound->peakMin.size();
                const float mid = wave.getCentreY(), half = wave.getHeight() * 0.45f;
                const int columns = juce::jmax (1, (int) wave.getWidth());
                for (int x = 0; x < columns; ++x)
                {
                    const double t0 = seconds * x / columns, t1 = seconds * (x + 1) / columns;
                    auto b0 = (int64_t) (t0 * sound->sampleRate) / projet::SoundData::peakBlock;
                    auto b1 = (int64_t) (t1 * sound->sampleRate) / projet::SoundData::peakBlock;
                    b0 = juce::jlimit ((int64_t) 0, blocks - 1, b0);
                    b1 = juce::jlimit (b0 + 1, blocks, b1 + 1);
                    float lo = 0.0f, hi = 0.0f;
                    for (auto b = b0; b < b1; ++b) { lo = juce::jmin (lo, sound->peakMin[(size_t) b]); hi = juce::jmax (hi, sound->peakMax[(size_t) b]); }
                    const bool inside = t1 > from && t0 < to;
                    g.setColour ((inside ? edge.brighter (0.6f) : Theme::textMuted.withAlpha (0.35f)).withMultipliedAlpha (ctx.alpha));
                    g.fillRect (wave.getX() + (float) x, mid - hi * half, 1.0f, juce::jmax (0.6f, (hi - lo) * half));
                }
                // The fragment's ends.
                const float xIn = wave.getX() + wave.getWidth() * (float) (from / seconds);
                const float xOut = wave.getX() + wave.getWidth() * (float) (to / seconds);
                g.setColour (Theme::accent.withMultipliedAlpha (ctx.alpha));
                g.fillRect (xIn - 0.75f, wave.getY(), 1.5f, wave.getHeight());
                g.fillRect (xOut - 0.75f, wave.getY(), 1.5f, wave.getHeight());
                juce::Path flags;
                flags.addTriangle (xIn, wave.getY(), xIn + 5.0f, wave.getY(), xIn, wave.getY() + 5.0f);
                flags.addTriangle (xOut, wave.getY(), xOut - 5.0f, wave.getY(), xOut, wave.getY() + 5.0f);
                g.fillPath (flags);
                if (hearing)
                {
                    const float xp = wave.getX() + wave.getWidth() * (float) juce::jlimit (0.0, 1.0, ctx.previewSeconds / seconds);
                    g.setColour (Theme::text.withMultipliedAlpha (ctx.alpha));
                    g.fillRect (xp - 0.5f, wave.getY(), 1.0f, wave.getHeight());
                }
            }
            else
            {
                g.setColour (Theme::textMuted.withMultipliedAlpha (ctx.alpha));
                g.setFont (Theme::font (11.0f).italicised());
                g.drawText (ctx.project != nullptr && ! ctx.project->findSound (soundId).isValid() ? "sound not in the project" : "loading...",
                            wave, juce::Justification::centred, false);
            }

            // What it does: here only, or in the piece.
            auto foot = juce::Rectangle<float> (r.getX() + 10.0f, r.getBottom() - 20.0f, r.getWidth() - 20.0f, 16.0f);
            g.setFont (Theme::font (10.5f));
            if (out)
            {
                const auto tagArea = foot.removeFromRight (32.0f);
                g.setColour (Theme::accent.withMultipliedAlpha (ctx.alpha));
                g.fillRoundedRectangle (tagArea.reduced (0.0f, 1.0f), 3.0f);
                g.setColour (juce::Colours::black.withMultipliedAlpha (ctx.alpha));
                g.setFont (Theme::font (9.5f, true));
                g.drawText ("OUT", tagArea, juce::Justification::centred, false);
                g.setFont (Theme::font (10.5f));
                const double gain = (double) card.getProperty (IDs::gainDb, 0.0);
                juce::String t = "plays in the piece at " + Board::formatSeconds ((double) card.getProperty (IDs::outStart, 0.0)) + " in the box";
                if (std::abs (gain) > 0.05) t << ", " << juce::String (gain, 1) << " dB";
                g.setColour (Theme::text.withMultipliedAlpha (0.8f * ctx.alpha));
                g.drawText (t, foot, juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour (Theme::textMuted.withMultipliedAlpha (ctx.alpha));
                g.drawText ("Sound card: plays here only", foot, juce::Justification::centredLeft, true);
            }
        }

        void paintLink (juce::Graphics& g, const juce::ValueTree& link, const Context& ctx)
        {
            const auto r = Board::rect (link);
            const auto edge = Board::colourOf (link);
            auto box = linkedBox (link, ctx.project);
            const bool gone = ! box.isValid();
            g.setColour (cardPaper (edge).withMultipliedAlpha (ctx.alpha));
            g.fillRoundedRectangle (r, 6.0f);
            if (gone)
            {
                juce::Path p, dashed;
                p.addRoundedRectangle (r.reduced (0.5f), 6.0f);
                const float dashes[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dashes, 2);
                g.setColour (edge.withMultipliedAlpha (ctx.alpha));
                g.fillPath (dashed);
            }
            else
            {
                g.setColour (edge.withMultipliedAlpha (ctx.alpha));
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            }
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.reduced (1.0f).toNearestInt());
            // A box sign, the name; a button to go there.
            auto inner = r.reduced (10.0f, 8.0f);
            auto line1 = inner.removeFromTop (20.0f);
            const auto go = goButton (link);
            line1.setRight (go.getX() - 4.0f);
            const auto sign = line1.removeFromLeft (14.0f).withSizeKeepingCentre (12.0f, 12.0f);
            g.setColour (edge.brighter (0.5f).withMultipliedAlpha (ctx.alpha));
            g.drawRoundedRectangle (sign, 2.0f, 1.0f);
            g.fillRoundedRectangle (sign.reduced (3.0f), 1.0f);
            line1.removeFromLeft (6.0f);
            g.setColour (Theme::text.withMultipliedAlpha (ctx.alpha));
            g.setFont (Theme::font (13.0f, true));
            const auto name = gone ? link[IDs::name].toString() : box[IDs::name].toString();
            g.drawText (name.isNotEmpty() ? name : juce::String ("(no name)"), line1, juce::Justification::centredLeft, true);
            if (! gone)
            {
                g.setColour (edge.darker (0.5f).withMultipliedAlpha (ctx.alpha));
                g.fillRoundedRectangle (go, 4.0f);
                g.setColour (edge.brighter (0.7f).withMultipliedAlpha (ctx.alpha));
                juce::Path arrow;
                const auto c = go.reduced (6.0f);
                arrow.startNewSubPath (c.getX(), c.getBottom());
                arrow.lineTo (c.getRight(), c.getY());
                arrow.startNewSubPath (c.getX() + 3.0f, c.getY());
                arrow.lineTo (c.getRight(), c.getY());
                arrow.lineTo (c.getRight(), c.getBottom() - 3.0f);
                g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setFont (Theme::font (11.0f));
            g.setColour ((gone ? Theme::nodesFlagged : edge.interpolatedWith (Theme::text, 0.6f)).withMultipliedAlpha (ctx.alpha));
            g.drawText (gone ? juce::String ("Timeline link: the box is gone (right-click to point it elsewhere)")
                             : "Timeline link " + juce::String::fromUTF8 ("\xc2\xb7 ") + linkDetails (link, box),
                        inner.removeFromTop (16.0f), juce::Justification::centredLeft, true);
        }
    }

    namespace
    {
        void paintRegion (juce::Graphics& g, const juce::ValueTree& region, const Context& ctx)
        {
            const auto r = Board::rect (region);
            const auto edge = Board::colourOf (region);
            g.setColour (juce::Colour (0xff141518).withMultipliedAlpha (0.55f * ctx.alpha));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (edge.withMultipliedAlpha (0.8f * ctx.alpha));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);

            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.toNearestInt());
            // The title strip: its name, its time and register.
            auto title = r.withHeight (20.0f).reduced (8.0f, 0.0f);
            g.setColour (edge.interpolatedWith (Theme::text, 0.5f).withMultipliedAlpha (ctx.alpha));
            g.setFont (Theme::font (11.0f, true));
            auto name = region[IDs::label].toString().trim();
            if (name.isEmpty()) name = "Morphogram";
            g.drawText (Theme::capitals (name), title, juce::Justification::centredLeft, true);
            const bool boxTime = Board::regionInBoxTime (region);
            const bool hz = region[IDs::registerScale].toString() == "hz";
            g.setFont (Theme::font (10.0f));
            g.setColour (Theme::textMuted.withMultipliedAlpha (ctx.alpha));
            g.drawText (juce::String (boxTime ? "the box's time" : "free time") + juce::String::fromUTF8 (" \xc2\xb7 ") + (hz ? "Hz" : "low, middle, high"),
                        title, juce::Justification::centredRight, true);

            const auto inside = Board::regionInside (region);
            const float dashes[] = { 3.0f, 4.0f };
            auto dashedLine = [&g, &dashes] (float x0, float y0, float x1, float y1)
            {
                juce::Path p, d;
                p.startNewSubPath (x0, y0);
                p.lineTo (x1, y1);
                juce::PathStrokeType (0.8f).createDashedStroke (d, p, dashes, 2);
                g.fillPath (d);
            };
            g.setFont (Theme::font (9.5f));
            if (hz)
            {
                const double lo = juce::jmax (1.0, (double) region.getProperty (IDs::hzLow, 20.0));
                const double hi = juce::jmax (lo * 1.01, (double) region.getProperty (IDs::hzHigh, 20000.0));
                for (double f : { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 })
                {
                    if (f <= lo || f >= hi) continue;
                    const float y = inside.getBottom() - inside.getHeight() * (float) (std::log (f / lo) / std::log (hi / lo));
                    g.setColour (Theme::line.withMultipliedAlpha (ctx.alpha));
                    dashedLine (inside.getX(), y, inside.getRight(), y);
                    g.setColour (Theme::textMuted.withMultipliedAlpha (0.8f * ctx.alpha));
                    g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, f < 10000.0 ? 0 : 0) + "k" : juce::String (juce::roundToInt (f)),
                                juce::Rectangle<float> (inside.getX() + 3.0f, y - 11.0f, 40.0f, 11.0f), juce::Justification::bottomLeft, false);
                }
            }
            else
            {
                const char* bands[] = { "high", "middle", "low" };
                for (int i = 0; i < 3; ++i)
                {
                    const float y0 = inside.getY() + inside.getHeight() * (float) i / 3.0f;
                    if (i > 0)
                    {
                        g.setColour (Theme::line.withMultipliedAlpha (ctx.alpha));
                        dashedLine (inside.getX(), y0, inside.getRight(), y0);
                    }
                    g.setColour (Theme::textMuted.withMultipliedAlpha (0.7f * ctx.alpha));
                    g.drawText (bands[i], juce::Rectangle<float> (inside.getX() + 4.0f, y0 + 2.0f, 60.0f, 12.0f), juce::Justification::topLeft, false);
                }
            }
            // The time along the bottom.
            auto foot = juce::Rectangle<float> (inside.getX(), inside.getBottom(), inside.getWidth(), 18.0f);
            g.setColour (Theme::line.withMultipliedAlpha (ctx.alpha));
            g.fillRect (inside.getX(), inside.getBottom(), inside.getWidth(), 1.0f);
            if (boxTime && ctx.boxLength > 0.0)
            {
                double step = 0.1;
                for (double candidate : { 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0 })
                {
                    step = candidate;
                    if (inside.getWidth() * candidate / ctx.boxLength >= 56.0) break;
                }
                g.setColour (Theme::textMuted.withMultipliedAlpha (0.85f * ctx.alpha));
                for (double t = 0.0; t <= ctx.boxLength + 1.0e-6; t += step)
                {
                    const float x = inside.getX() + inside.getWidth() * (float) (t / ctx.boxLength);
                    g.fillRect (x, inside.getBottom(), 1.0f, 4.0f);
                    const int minutes = (int) (t / 60.0);
                    const double rest = t - minutes * 60.0;
                    const auto label = juce::String (minutes) + ":" + (rest < 10.0 ? "0" : "") + juce::String (rest, step < 1.0 ? 1 : 0);
                    g.drawText (label, juce::Rectangle<float> (x + 2.0f, foot.getY() + 3.0f, 50.0f, 13.0f), juce::Justification::topLeft, false);
                }
            }
            else
            {
                g.setColour (Theme::textMuted.withMultipliedAlpha (0.6f * ctx.alpha));
                g.drawText (boxTime ? "time" : "free time", foot.withTrimmedTop (3.0f), juce::Justification::centredLeft, false);
            }
        }

        struct BuiltGlyph { juce::Array<Glyphs::Stroke> strokes; };
        std::map<juce::String, BuiltGlyph>& glyphCache()
        {
            static std::map<juce::String, BuiltGlyph> cache;
            return cache;
        }

        /** A value's glyph in a w by h band, shaped by the factor curves (as on boxes, stage 8). */
        const juce::Array<Glyphs::Stroke>* glyphStrokes (const Framework& framework, const Glyphs::Set& glyphs, const Framework::Category& c,
                                                          const juce::String& value, const juce::String& curvesText, int w, int h, int seed)
        {
            auto* val = framework.findValue (c, value);
            if (val == nullptr || val->glyph.isEmpty()) return nullptr;
            const auto found = glyphs.find (val->glyph, { val->from, c.package });
            if (found.script == nullptr) return nullptr;
            const auto key = found.owner + "|" + val->glyph + "|" + juce::String (w) + "x" + juce::String (h) + "|" + juce::String (seed)
                             + "|" + juce::String::toHexString (curvesText.hashCode64());
            auto& cache = glyphCache();
            auto it = cache.find (key);
            if (it == cache.end())
            {
                if (cache.size() > 300) cache.clear();
                Glyphs::FactorValues factorValues;
                // Its dynamics: the energy's own profile, unless a drawn envelope factor takes its place.
                juce::Array<float> env;
                bool flat = true;
                for (int i = 1; i < val->env.size(); i += 2) flat = flat && std::abs (val->env[i] - 1.0f) < 1.0e-6f;
                if (! flat) env = val->env;
                const auto curves = Factors::parse (curvesText);
                for (auto* factor : framework.factorsOf (c, value))
                {
                    auto ci = curves.find (factor->id);
                    const auto curve = ci != curves.end() ? ci->second : Factors::Curve();
                    if (factor->effect == "envelope")
                    {
                        if (curve.isEmpty()) continue;
                        const auto samples = Factors::sample (curve, 32, factor->neutral);
                        env.clearQuick();
                        for (int i = 0; i < samples.size(); ++i) env.addArray ({ (float) i / 32.0f, juce::jmax (0.04f, samples[i]) });
                    }
                    else if (factor->effect == "shift") { if (! curve.isEmpty()) factorValues.shift = Factors::sample (curve, 32, factor->neutral); }
                    else if (factor->effect == "depth") { if (! curve.isEmpty()) factorValues.depth = Factors::sample (curve, 32, factor->neutral); }
                    else factorValues.curves.push_back ({ factor->id, Factors::sample (curve, 32, factor->neutral) });
                }
                BuiltGlyph built;
                if (! found.script->run (val->glyph, (float) w, (float) h, env, seed, built.strokes, factorValues).wasOk()) built.strokes.clear();
                it = cache.emplace (key, std::move (built)).first;
            }
            return &it->second.strokes;
        }

        /** A path's points, smoothed, with their distance along it. */
        struct Bent
        {
            juce::Array<juce::Point<float>> pts;
            juce::Array<float> along;
            float length = 0.0f;

            explicit Bent (const juce::Array<juce::Point<float>>& raw)
            {
                // A light smoothing, so the bent glyph does not kink at every point.
                for (int i = 0; i < raw.size(); ++i)
                {
                    if (i == 0 || i == raw.size() - 1) { pts.add (raw[i]); continue; }
                    pts.add ((raw[i - 1] + raw[i] * 2.0f + raw[i + 1]) * 0.25f);
                }
                along.add (0.0f);
                for (int i = 1; i < pts.size(); ++i) along.add (along.getLast() + pts[i].getDistanceFrom (pts[i - 1]));
                length = along.isEmpty() ? 0.0f : along.getLast();
            }

            /** The point `s` along the path, moved `offset` off it (to the left of its direction). */
            juce::Point<float> at (float s, float offset) const
            {
                if (pts.size() < 2) return pts.isEmpty() ? juce::Point<float>() : pts[0];
                s = juce::jlimit (0.0f, length, s);
                int i = 1;
                while (i < pts.size() - 1 && along[i] < s) ++i;
                const float seg = juce::jmax (1.0e-4f, along[i] - along[i - 1]);
                const float k = (s - along[i - 1]) / seg;
                const auto p = pts[i - 1] + (pts[i] - pts[i - 1]) * k;
                auto d = pts[i] - pts[i - 1];
                const float len = juce::jmax (1.0e-4f, d.getDistanceFromOrigin());
                return p + juce::Point<float> (d.y / len, -d.x / len) * offset;
            }
        };

        void paintGesture (juce::Graphics& g, const juce::ValueTree& gesture, const Context& ctx)
        {
            const auto raw = Board::strokePoints (gesture);
            if (raw.size() < 2) return;
            const Bent bent (raw);
            const float thickness = juce::jmax (6.0f, (float) (double) gesture.getProperty (IDs::thickness, 36.0));
            const auto colour = ctx.framework != nullptr ? gestureColour (gesture, *ctx.framework) : Board::palette()[6];

            // The path itself, faint (dashed while no energy draws on it).
            juce::Path line;
            line.startNewSubPath (bent.pts[0]);
            for (int i = 1; i < bent.pts.size(); ++i) line.lineTo (bent.pts[i]);
            bool drawn = false;
            if (ctx.framework != nullptr && ctx.glyphs != nullptr && bent.length > 4.0f)
            {
                const int w = juce::jlimit (12, 2048, ((int) bent.length + 5) / 6 * 6);
                const int h = juce::jlimit (6, 400, ((int) thickness + 2) / 3 * 3);
                const int seed = (gesture[IDs::id].toString().hashCode() & 0x7fffffff) % 7;
                for (auto* c : glyphCategories (*ctx.framework))
                {
                    const auto value = gesture[c->property()].toString();
                    if (value.isEmpty()) continue;
                    auto* strokes = glyphStrokes (*ctx.framework, *ctx.glyphs, *c, value, gesture[c->factorsProperty()].toString(), w, h, seed);
                    if (strokes == nullptr) continue;
                    const auto colours = ctx.framework->valueColours (*c, value);
                    const auto cc = colours.light != 0 ? juce::Colour (colours.light) : (colours.colour != 0 ? juce::Colour (colours.colour) : colour);
                    const float strength = ctx.alpha * (c->valuesFrom.isEmpty() ? 0.95f : 0.55f);
                    const float kx = bent.length / (float) w, ky = thickness / (float) h;
                    auto map = [&] (float x, float y) { return bent.at (x * kx, ((float) h * 0.5f - y) * ky); };
                    for (auto& stroke : *strokes)
                    {
                        g.setColour (cc.withMultipliedAlpha (stroke.alpha * strength));
                        if (stroke.points.size() >= 4)
                        {
                            // Each piece in short steps, so it bends with the path.
                            juce::Path p;
                            p.startNewSubPath (map (stroke.points[0], stroke.points[1]));
                            for (int i = 2; i + 1 < stroke.points.size(); i += 2)
                            {
                                const float x0 = stroke.points[i - 2], y0 = stroke.points[i - 1], x1 = stroke.points[i], y1 = stroke.points[i + 1];
                                const int steps = juce::jlimit (1, 64, (int) (std::abs (x1 - x0) * kx / 4.0f));
                                for (int k = 1; k <= steps; ++k)
                                {
                                    const float t = (float) k / (float) steps;
                                    p.lineTo (map (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t));
                                }
                            }
                            if (stroke.closed || stroke.filled) p.closeSubPath();
                            if (stroke.filled) g.fillPath (p);
                            else g.strokePath (p, juce::PathStrokeType (stroke.width * 1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                        }
                        if (stroke.dots.size() >= 3)
                        {
                            juce::Path p;
                            for (int i = 0; i + 2 < stroke.dots.size(); i += 3)
                            {
                                const auto c2 = map (stroke.dots[i], stroke.dots[i + 1]);
                                const float radius = juce::jmax (0.3f, stroke.dots[i + 2]);
                                p.addEllipse (c2.x - radius, c2.y - radius, radius * 2.0f, radius * 2.0f);
                            }
                            g.fillPath (p);
                        }
                    }
                    drawn = true;
                }
            }
            g.setColour (colour.withMultipliedAlpha ((drawn ? 0.22f : 0.7f) * ctx.alpha));
            if (drawn) g.strokePath (line, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            else
            {
                juce::Path d;
                const float dashes[] = { 6.0f, 5.0f };
                juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createDashedStroke (d, line, dashes, 2);
                g.fillPath (d);
            }
        }
    }

    juce::Array<const Framework::Category*> glyphCategories (const Framework& framework)
    {
        // The categories the package draws as glyphs (the energies); failing
        // those, any choice whose values have glyphs.
        juce::Array<const Framework::Category*> out, withGlyphs;
        for (auto& c : framework.getCategories())
        {
            if (c.kind != Framework::Kind::choice) continue;
            for (auto& v : framework.valuesOf (c))
                if (v.glyph.isNotEmpty())
                {
                    withGlyphs.add (&c);
                    if (c.mark == Framework::Mark::glyph) out.add (&c);
                    break;
                }
        }
        if (out.isEmpty()) out = withGlyphs;
        // The main one first (a category of its own values), then those that borrow them.
        std::stable_sort (out.begin(), out.end(), [] (const Framework::Category* a, const Framework::Category* b)
                          { return a->valuesFrom.isEmpty() && ! b->valuesFrom.isEmpty(); });
        return out;
    }

    juce::Colour gestureColour (const juce::ValueTree& gesture, const Framework& framework)
    {
        for (auto* c : glyphCategories (framework))
        {
            const auto value = gesture[c->property()].toString();
            if (value.isEmpty()) continue;
            const auto colours = framework.valueColours (*c, value);
            if (colours.light != 0) return juce::Colour (colours.light);
            if (colours.colour != 0) return juce::Colour (colours.colour);
        }
        return Board::palette()[6];
    }

    juce::ValueTree shownRegion (const juce::ValueTree& board)
    {
        auto region = Board::findItem (board, board[IDs::regionId].toString());
        return region.hasType (IDs::REGION) && Board::regionInBoxTime (region) ? region : juce::ValueTree();
    }

    void paintRegionAcross (juce::Graphics& g, const juce::ValueTree& board, const juce::ValueTree& region,
                            juce::Rectangle<float> area, const Context& ctx)
    {
        const auto inside = Board::regionInside (region);
        if (inside.isEmpty() || area.isEmpty()) return;
        const float kx = area.getWidth() / inside.getWidth(), ky = area.getHeight() / inside.getHeight();
        // The register's thirds, faintly.
        g.setColour (Theme::line.withMultipliedAlpha (0.6f * ctx.alpha));
        if (region[IDs::registerScale].toString() != "hz")
            for (int i = 1; i < 3; ++i) g.fillRect (area.getX(), area.getY() + area.getHeight() * (float) i / 3.0f, area.getWidth(), 0.6f);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area.toNearestInt());
        for (auto& gesture : Board::gesturesIn (board, region))
        {
            auto copy = gesture.createCopy();
            const auto r = Board::rect (gesture);
            Board::setRect (copy, { area.getX() + (r.getX() - inside.getX()) * kx, area.getY() + (r.getY() - inside.getY()) * ky,
                                    juce::jmax (1.0f, r.getWidth() * kx), juce::jmax (1.0f, r.getHeight() * ky) }, nullptr);
            copy.setProperty (IDs::thickness, juce::jmax (6.0, (double) gesture.getProperty (IDs::thickness, 36.0) * ky), nullptr);
            paintItem (g, board, copy, ctx);
        }
    }

    juce::Rectangle<float> playButton (const juce::ValueTree& card)
    {
        const auto r = Board::rect (card);
        return { r.getX() + 10.0f, r.getY() + 8.0f, 24.0f, 24.0f };
    }

    juce::Rectangle<float> waveArea (const juce::ValueTree& card)
    {
        const auto r = Board::rect (card);
        return { r.getX() + 10.0f, r.getY() + 38.0f, juce::jmax (10.0f, r.getWidth() - 20.0f), juce::jmax (12.0f, r.getHeight() - 38.0f - 24.0f) };
    }

    juce::Rectangle<float> goButton (const juce::ValueTree& link)
    {
        const auto r = Board::rect (link);
        return { r.getRight() - 32.0f, r.getY() + 7.0f, 24.0f, 22.0f };
    }

    juce::String soundCardName (const juce::ValueTree& card, const Project* project)
    {
        const auto label = card[IDs::label].toString().trim();
        if (label.isNotEmpty()) return label;
        if (project != nullptr)
            if (auto s = project->findSound (card[IDs::soundId].toString()); s.isValid()) return s[IDs::name].toString();
        return "Sound";
    }

    juce::ValueTree linkedBox (const juce::ValueTree& link, Project* project)
    {
        if (project == nullptr) return {};
        auto box = project->findBoxInPiece (link[IDs::targetBox].toString());
        return box.isValid() && project->isInPiece (box) ? box : juce::ValueTree();
    }

    juce::String linkDetails (const juce::ValueTree& link, const juce::ValueTree& box)
    {
        juce::StringArray parts;
        const auto takeId = link[IDs::targetTake].toString();
        if (takeId.isNotEmpty() && Project::numTakes (box) > 1)
        {
            const auto take = box.getChildWithName (IDs::TAKES).getChildWithProperty (IDs::id, takeId);
            const auto active = Project::activeTakeOf (box);
            if (take.isValid())
                parts.add ("take " + take[IDs::name].toString() + (take == active ? juce::String() : " (" + active[IDs::name].toString() + " playing)"));
        }
        int depth = 1;
        for (auto c = Project::containerOf (box); c.isValid(); c = Project::containerOf (c)) ++depth;
        parts.add ("level " + juce::String (depth));
        return parts.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 "));
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
            case Board::Kind::soundCard: paintSoundCard (g, item, ctx); break;
            case Board::Kind::link:   paintLink (g, item, ctx); break;
            case Board::Kind::region: paintRegion (g, item, ctx); break;
            case Board::Kind::gesture: paintGesture (g, item, ctx); break;
            case Board::Kind::unknown: break;
        }
    }

    void paint (juce::Graphics& g, const juce::ValueTree& board, const Context& ctx)
    {
        for (auto item : board)
            paintItem (g, board, item, ctx);
    }
}
