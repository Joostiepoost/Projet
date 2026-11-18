// Board.cpp — see Board.h.

#include "Board.h"

namespace Board
{
    namespace
    {
        float num (const juce::ValueTree& t, const juce::Identifier& id, float fallback = 0.0f)
        {
            return t.hasProperty (id) ? (float) (double) t[id] : fallback;
        }

        juce::String newId()                                    { return juce::Uuid().toString(); }


        float distanceToSegment (juce::Point<float> p, juce::Point<float> a, juce::Point<float> b)
        {
            const auto ab = b - a;
            const float len2 = ab.x * ab.x + ab.y * ab.y;
            if (len2 < 1.0e-6f) return p.getDistanceFrom (a);
            const float t = juce::jlimit (0.0f, 1.0f, ((p - a).x * ab.x + (p - a).y * ab.y) / len2);
            return p.getDistanceFrom (a + ab * t);
        }

        bool isEllipse (const juce::ValueTree& item)            { return item.hasType (IDs::SHAPE) && item[IDs::shape].toString() == "ellipse"; }
        bool isLine (const juce::ValueTree& item)               { return item.hasType (IDs::SHAPE) && item[IDs::shape].toString() == "line"; }

        /** Where a ray from the item's centre towards `towards` leaves it (a little outside). */
        juce::Point<float> edgePoint (const juce::ValueTree& board, const juce::ValueTree& item, juce::Point<float> towards)
        {
            const auto r = bounds (board, item);
            const auto c = r.getCentre();
            auto d = towards - c;
            const float len = std::sqrt (d.x * d.x + d.y * d.y);
            if (len < 1.0e-3f) return c;
            d /= len;
            const float hw = r.getWidth() * 0.5f, hh = r.getHeight() * 0.5f;
            float t;
            if (isEllipse (item))
                t = 1.0f / std::sqrt (juce::square (d.x / juce::jmax (1.0f, hw)) + juce::square (d.y / juce::jmax (1.0f, hh)));
            else
                t = juce::jmin (std::abs (d.x) > 1.0e-6f ? hw / std::abs (d.x) : 1.0e9f,
                                std::abs (d.y) > 1.0e-6f ? hh / std::abs (d.y) : 1.0e9f);
            return c + d * juce::jmin (t + 5.0f, len);
        }

        juce::ValueTree attachedItem (const juce::ValueTree& board, const juce::ValueTree& arrow, const juce::Identifier& end)
        {
            const auto id = arrow[end].toString();
            if (id.isEmpty()) return {};
            auto item = findItem (board, id);
            return item.isValid() && ! item.hasType (IDs::ARROW) ? item : juce::ValueTree();
        }
    }

    juce::Rectangle<float> unite (juce::Rectangle<float> a, juce::Rectangle<float> b)
    {
        const float x0 = juce::jmin (a.getX(), b.getX()), y0 = juce::jmin (a.getY(), b.getY());
        const float x1 = juce::jmax (a.getRight(), b.getRight()), y1 = juce::jmax (a.getBottom(), b.getBottom());
        return { x0, y0, x1 - x0, y1 - y0 };
    }

    juce::Rectangle<float> boundsOf (const juce::ValueTree& board, const juce::Array<juce::ValueTree>& items)
    {
        juce::Rectangle<float> all;
        for (int i = 0; i < items.size(); ++i)
            all = i == 0 ? bounds (board, items[i]) : unite (all, bounds (board, items[i]));
        return all;
    }

    Kind kindOf (const juce::ValueTree& item)
    {
        if (item.hasType (IDs::CARD))   return Kind::card;
        if (item.hasType (IDs::ARROW))  return Kind::arrow;
        if (item.hasType (IDs::SHAPE))  return Kind::shape;
        if (item.hasType (IDs::STROKE)) return Kind::stroke;
        if (item.hasType (IDs::IMAGE))  return Kind::image;
        if (item.hasType (IDs::SOUNDCARD)) return Kind::soundCard;
        if (item.hasType (IDs::LINK))   return Kind::link;
        return Kind::unknown;
    }

    juce::String kindName (const juce::ValueTree& item)
    {
        switch (kindOf (item))
        {
            case Kind::card:    return "card";
            case Kind::arrow:   return "arrow";
            case Kind::shape:   return isLine (item) ? "line" : (isEllipse (item) ? "ellipse" : "rectangle");
            case Kind::stroke:  return "drawing";
            case Kind::image:   return "image";
            case Kind::soundCard: return "sound card";
            case Kind::link:    return "link";
            case Kind::unknown: break;
        }
        return "item";
    }

    juce::ValueTree findItem (const juce::ValueTree& board, const juce::String& id)
    {
        return id.isEmpty() ? juce::ValueTree() : board.getChildWithProperty (IDs::id, id);
    }

    juce::Rectangle<float> rect (const juce::ValueTree& item)
    {
        return { num (item, IDs::x), num (item, IDs::y), num (item, IDs::w), num (item, IDs::h) };
    }

    juce::Rectangle<float> bounds (const juce::ValueTree& board, const juce::ValueTree& item)
    {
        switch (kindOf (item))
        {
            case Kind::arrow:
            {
                const auto p = arrowPath (board, item);
                return unite (p.path().getBounds(), juce::Rectangle<float> (p.a, p.b)).expanded (2.0f);
            }
            case Kind::shape:
                if (isLine (item))
                {
                    const float x = num (item, IDs::x), y = num (item, IDs::y);
                    return juce::Rectangle<float> (juce::Point<float> (x, y), juce::Point<float> (x + num (item, IDs::w), y + num (item, IDs::h)));
                }
                return rect (item);
            case Kind::card: case Kind::stroke: case Kind::image: case Kind::soundCard: case Kind::link:
                return rect (item);
            case Kind::unknown:
                break;
        }
        return {};
    }

    juce::Rectangle<float> contentBounds (const juce::ValueTree& board)
    {
        juce::Rectangle<float> all;
        bool first = true;
        for (auto item : board)
        {
            const auto b = bounds (board, item);
            all = first ? b : unite (all, b);
            first = false;
        }
        return all;
    }

    juce::Point<float> ArrowPath::middle() const
    {
        return a * 0.25f + control * 0.5f + b * 0.25f;
    }

    juce::Path ArrowPath::path() const
    {
        juce::Path p;
        p.startNewSubPath (a);
        p.quadraticTo (control, b);
        return p;
    }

    ArrowPath arrowPath (const juce::ValueTree& board, const juce::ValueTree& arrow)
    {
        const auto fromItem = attachedItem (board, arrow, IDs::fromItem), toItem = attachedItem (board, arrow, IDs::toItem);
        const juce::Point<float> ca = fromItem.isValid() ? bounds (board, fromItem).getCentre() : juce::Point<float> (num (arrow, IDs::x1), num (arrow, IDs::y1));
        const juce::Point<float> cb = toItem.isValid() ? bounds (board, toItem).getCentre() : juce::Point<float> (num (arrow, IDs::x2), num (arrow, IDs::y2));
        const float bend = num (arrow, IDs::bend);
        auto d = cb - ca;
        const float len = juce::jmax (1.0e-3f, std::sqrt (d.x * d.x + d.y * d.y));
        const juce::Point<float> normal (-d.y / len, d.x / len);
        // The control point twice as far out as the bend, so the curve's middle lies `bend` off the line.
        ArrowPath p;
        p.control = (ca + cb) * 0.5f + normal * (bend * 2.0f);
        p.a = fromItem.isValid() ? edgePoint (board, fromItem, std::abs (bend) > 0.5f ? p.control : cb) : ca;
        p.b = toItem.isValid() ? edgePoint (board, toItem, std::abs (bend) > 0.5f ? p.control : ca) : cb;
        if (std::abs (bend) <= 0.5f) p.control = (p.a + p.b) * 0.5f;
        return p;
    }

    juce::Array<juce::Point<float>> strokePoints (const juce::ValueTree& stroke)
    {
        juce::Array<juce::Point<float>> out;
        const float x = num (stroke, IDs::x), y = num (stroke, IDs::y);
        for (auto& pair : juce::StringArray::fromTokens (stroke[IDs::points].toString(), " ", {}))
        {
            const auto dx = pair.upToFirstOccurrenceOf (",", false, false), dy = pair.fromFirstOccurrenceOf (",", false, false);
            if (dx.isNotEmpty() && dy.isNotEmpty()) out.add ({ x + dx.getFloatValue(), y + dy.getFloatValue() });
        }
        return out;
    }

    juce::String formatPoints (const juce::Array<juce::Point<float>>& relative)
    {
        juce::StringArray parts;
        for (auto& p : relative) parts.add (juce::String (p.x, 1) + "," + juce::String (p.y, 1));
        return parts.joinIntoString (" ");
    }

    bool hits (const juce::ValueTree& board, const juce::ValueTree& item, juce::Point<float> p, float tol)
    {
        switch (kindOf (item))
        {
            case Kind::card:
            case Kind::image:
            case Kind::soundCard:
            case Kind::link:
                return rect (item).expanded (tol).contains (p);
            case Kind::shape:
            {
                const float w = num (item, IDs::lineWidth, 2.0f) * 0.5f;
                if (isLine (item))
                {
                    const auto [a, b] = ends (board, item);
                    return distanceToSegment (p, a, b) <= tol + w;
                }
                const auto r = rect (item);
                const bool filled = (bool) item[IDs::filled];
                if (item[IDs::label].toString().isNotEmpty() && juce::Rectangle<float> (r.getWidth() * 0.6f, 24.0f).withCentre (r.getCentre()).contains (p))
                    return true;
                if (isEllipse (item))
                {
                    const float a = juce::jmax (1.0f, r.getWidth() * 0.5f), b = juce::jmax (1.0f, r.getHeight() * 0.5f);
                    const auto c = r.getCentre();
                    const float k = std::sqrt (juce::square ((p.x - c.x) / a) + juce::square ((p.y - c.y) / b));
                    if (filled) return k <= 1.0f + tol / juce::jmin (a, b);
                    return std::abs (k - 1.0f) * juce::jmin (a, b) <= tol + w;
                }
                if (filled) return r.expanded (tol).contains (p);
                return r.expanded (tol + w).contains (p) && ! r.reduced (tol + w).contains (p);
            }
            case Kind::stroke:
            {
                const auto pts = strokePoints (item);
                const float w = num (item, IDs::lineWidth, 2.0f) * 0.5f;
                if (pts.size() == 1) return p.getDistanceFrom (pts[0]) <= tol + w;
                for (int i = 1; i < pts.size(); ++i)
                    if (distanceToSegment (p, pts[i - 1], pts[i]) <= tol + w) return true;
                return false;
            }
            case Kind::arrow:
            {
                const auto ap = arrowPath (board, item);
                if (item[IDs::label].toString().isNotEmpty() && juce::Rectangle<float> (80.0f, 22.0f).withCentre (ap.middle()).contains (p)) return true;
                auto prev = ap.a;
                for (int i = 1; i <= 24; ++i)
                {
                    const float t = (float) i / 24.0f, u = 1.0f - t;
                    const auto q = ap.a * (u * u) + ap.control * (2.0f * u * t) + ap.b * (t * t);
                    if (distanceToSegment (p, prev, q) <= tol + 2.0f) return true;
                    prev = q;
                }
                return false;
            }
            case Kind::unknown:
                break;
        }
        return false;
    }

    juce::ValueTree itemAt (const juce::ValueTree& board, juce::Point<float> p, float tolerance)
    {
        for (int i = board.getNumChildren(); --i >= 0;)
            if (hits (board, board.getChild (i), p, tolerance)) return board.getChild (i);
        return {};
    }

    namespace
    {
        void setRectProps (juce::ValueTree& t, juce::Rectangle<float> r)
        {
            t.setProperty (IDs::x, r.getX(), nullptr);
            t.setProperty (IDs::y, r.getY(), nullptr);
            t.setProperty (IDs::w, r.getWidth(), nullptr);
            t.setProperty (IDs::h, r.getHeight(), nullptr);
        }
    }

    juce::ValueTree makeCard (juce::Rectangle<float> r, int colour)
    {
        juce::ValueTree t (IDs::CARD);
        t.setProperty (IDs::id, newId(), nullptr);
        setRectProps (t, r);
        t.setProperty (IDs::colour, colour, nullptr);
        t.setProperty (IDs::notes, juce::String(), nullptr);
        return t;
    }

    juce::ValueTree makeShape (const juce::String& shape, juce::Rectangle<float> r, int colour)
    {
        juce::ValueTree t (IDs::SHAPE);
        t.setProperty (IDs::id, newId(), nullptr);
        t.setProperty (IDs::shape, shape, nullptr);
        setRectProps (t, r);
        t.setProperty (IDs::colour, colour, nullptr);
        t.setProperty (IDs::lineWidth, 1.5, nullptr);
        return t;
    }

    juce::ValueTree makeArrow (juce::Point<float> a, juce::Point<float> b, const juce::String& fromId, const juce::String& toId, int colour)
    {
        juce::ValueTree t (IDs::ARROW);
        t.setProperty (IDs::id, newId(), nullptr);
        t.setProperty (IDs::x1, a.x, nullptr);
        t.setProperty (IDs::y1, a.y, nullptr);
        t.setProperty (IDs::x2, b.x, nullptr);
        t.setProperty (IDs::y2, b.y, nullptr);
        if (fromId.isNotEmpty()) t.setProperty (IDs::fromItem, fromId, nullptr);
        if (toId.isNotEmpty()) t.setProperty (IDs::toItem, toId, nullptr);
        t.setProperty (IDs::colour, colour, nullptr);
        t.setProperty (IDs::lineWidth, 1.5, nullptr);
        return t;
    }

    juce::ValueTree makeStroke (const juce::Array<juce::Point<float>>& points, int colour, float width)
    {
        juce::ValueTree t (IDs::STROKE);
        t.setProperty (IDs::id, newId(), nullptr);
        juce::Rectangle<float> r;
        for (int i = 0; i < points.size(); ++i)
            r = i == 0 ? juce::Rectangle<float> (points[i], points[i]) : unite (r, juce::Rectangle<float> (points[i], points[i]));
        juce::Array<juce::Point<float>> rel;
        for (auto& p : points) rel.add (p - r.getTopLeft());
        setRectProps (t, r);
        t.setProperty (IDs::points, formatPoints (rel), nullptr);
        t.setProperty (IDs::colour, colour, nullptr);
        t.setProperty (IDs::lineWidth, width, nullptr);
        return t;
    }

    juce::ValueTree makeImage (const juce::String& file, juce::Rectangle<float> r)
    {
        juce::ValueTree t (IDs::IMAGE);
        t.setProperty (IDs::id, newId(), nullptr);
        t.setProperty (IDs::file, file, nullptr);
        setRectProps (t, r);
        return t;
    }

    juce::ValueTree makeSoundCard (const juce::String& soundId, juce::Rectangle<float> r, int colour)
    {
        juce::ValueTree t (IDs::SOUNDCARD);
        t.setProperty (IDs::id, newId(), nullptr);
        t.setProperty (IDs::soundId, soundId, nullptr);
        setRectProps (t, r);
        t.setProperty (IDs::colour, colour, nullptr);
        return t;
    }

    juce::ValueTree makeLink (const juce::ValueTree& box, juce::Rectangle<float> r)
    {
        juce::ValueTree t (IDs::LINK);
        t.setProperty (IDs::id, newId(), nullptr);
        t.setProperty (IDs::targetBox, box[IDs::id].toString(), nullptr);
        if (box.getChildWithName (IDs::TAKES).isValid())
            t.setProperty (IDs::targetTake, box[IDs::activeTake].toString(), nullptr);
        setRectProps (t, r);
        t.setProperty (IDs::colour, (int) palette()[1].getARGB(), nullptr);
        return t;
    }

    double cardIn (const juce::ValueTree& card)
    {
        return juce::jmax (0.0, (double) card.getProperty (IDs::srcIn, 0.0));
    }

    double cardOut (const juce::ValueTree& card, double soundSeconds)
    {
        const double out = (double) card.getProperty (IDs::srcOut, 0.0);
        const double end = soundSeconds > 0.0 ? soundSeconds : 1.0e9;
        return out > 0.0 ? juce::jlimit (cardIn (card), end, out) : end;
    }

    bool isTemporalOut (const juce::ValueTree& card)
    {
        return card.hasType (IDs::SOUNDCARD) && (bool) card[IDs::temporalOut];
    }

    juce::Array<juce::ValueTree> temporalOuts (const juce::ValueTree& board)
    {
        juce::Array<juce::ValueTree> out;
        for (auto item : board)
            if (isTemporalOut (item)) out.add (item);
        return out;
    }

    juce::String formatSeconds (double seconds)
    {
        seconds = juce::jmax (0.0, seconds);
        const int minutes = (int) (seconds / 60.0);
        const double rest = seconds - minutes * 60.0;
        return juce::String (minutes) + ":" + (rest < 10.0 ? "0" : "") + juce::String (rest, 3);
    }

    double parseSeconds (const juce::String& text)
    {
        const auto t = text.trim();
        if (t.isEmpty()) return -1.0;
        if (t.containsChar (':'))
        {
            const auto m = t.upToFirstOccurrenceOf (":", false, false).trim(), s = t.fromFirstOccurrenceOf (":", false, false).trim();
            if (! m.containsOnly ("0123456789") || ! s.containsOnly ("0123456789.")) return -1.0;
            return m.getIntValue() * 60.0 + s.getDoubleValue();
        }
        return t.containsOnly ("0123456789.") ? t.getDoubleValue() : -1.0;
    }

    void moveBy (juce::ValueTree item, juce::Point<float> d, juce::UndoManager* um)
    {
        if (item.hasType (IDs::ARROW))
        {
            if (item[IDs::fromItem].toString().isEmpty())
            {
                item.setProperty (IDs::x1, num (item, IDs::x1) + d.x, um);
                item.setProperty (IDs::y1, num (item, IDs::y1) + d.y, um);
            }
            if (item[IDs::toItem].toString().isEmpty())
            {
                item.setProperty (IDs::x2, num (item, IDs::x2) + d.x, um);
                item.setProperty (IDs::y2, num (item, IDs::y2) + d.y, um);
            }
            return;
        }
        item.setProperty (IDs::x, num (item, IDs::x) + d.x, um);
        item.setProperty (IDs::y, num (item, IDs::y) + d.y, um);
    }

    void setRect (juce::ValueTree item, juce::Rectangle<float> r, juce::UndoManager* um)
    {
        if (item.hasType (IDs::STROKE))
        {
            const float ow = num (item, IDs::w), oh = num (item, IDs::h);
            const float kx = ow > 0.01f ? r.getWidth() / ow : 1.0f, ky = oh > 0.01f ? r.getHeight() / oh : 1.0f;
            const float x0 = num (item, IDs::x), y0 = num (item, IDs::y);
            juce::Array<juce::Point<float>> rel;
            for (auto& p : strokePoints (item)) rel.add ({ (p.x - x0) * kx, (p.y - y0) * ky });
            item.setProperty (IDs::points, formatPoints (rel), um);
        }
        if (! isLine (item)) r = r.withWidth (juce::jmax (4.0f, r.getWidth())).withHeight (juce::jmax (4.0f, r.getHeight()));
        item.setProperty (IDs::x, r.getX(), um);
        item.setProperty (IDs::y, r.getY(), um);
        item.setProperty (IDs::w, r.getWidth(), um);
        item.setProperty (IDs::h, r.getHeight(), um);
    }

    bool hasCorners (const juce::ValueTree& item)
    {
        const auto k = kindOf (item);
        return k == Kind::card || k == Kind::image || k == Kind::stroke || k == Kind::soundCard || k == Kind::link
               || (k == Kind::shape && ! isLine (item));
    }

    bool hasEnds (const juce::ValueTree& item)
    {
        return item.hasType (IDs::ARROW) || isLine (item);
    }

    std::pair<juce::Point<float>, juce::Point<float>> ends (const juce::ValueTree& board, const juce::ValueTree& item)
    {
        if (item.hasType (IDs::ARROW))
        {
            const auto p = arrowPath (board, item);
            return { p.a, p.b };
        }
        const float x = num (item, IDs::x), y = num (item, IDs::y);
        return { { x, y }, { x + num (item, IDs::w), y + num (item, IDs::h) } };
    }

    void setEnd (juce::ValueTree item, int end, juce::Point<float> p, const juce::String& attachTo, juce::UndoManager* um)
    {
        if (item.hasType (IDs::ARROW))
        {
            const auto& attach = end == 0 ? IDs::fromItem : IDs::toItem;
            if (attachTo.isNotEmpty() && attachTo != item[end == 0 ? IDs::toItem : IDs::fromItem].toString())
            {
                if (item[attach].toString() != attachTo) item.setProperty (attach, attachTo, um);
            }
            else if (item.hasProperty (attach)) item.removeProperty (attach, um);
            item.setProperty (end == 0 ? IDs::x1 : IDs::x2, p.x, um);
            item.setProperty (end == 0 ? IDs::y1 : IDs::y2, p.y, um);
            return;
        }
        const float x = num (item, IDs::x), y = num (item, IDs::y), x2 = x + num (item, IDs::w), y2 = y + num (item, IDs::h);
        if (end == 0)
        {
            item.setProperty (IDs::x, p.x, um);
            item.setProperty (IDs::y, p.y, um);
            item.setProperty (IDs::w, x2 - p.x, um);
            item.setProperty (IDs::h, y2 - p.y, um);
        }
        else
        {
            item.setProperty (IDs::w, p.x - x, um);
            item.setProperty (IDs::h, p.y - y, um);
        }
    }

    void detachArrowsFrom (juce::ValueTree board, const juce::String& itemId, juce::UndoManager* um)
    {
        if (itemId.isEmpty()) return;
        for (auto item : board)
        {
            if (! item.hasType (IDs::ARROW)) continue;
            const bool from = item[IDs::fromItem].toString() == itemId, to = item[IDs::toItem].toString() == itemId;
            if (! from && ! to) continue;
            const auto p = arrowPath (board, item);
            if (from)
            {
                item.setProperty (IDs::x1, p.a.x, um);
                item.setProperty (IDs::y1, p.a.y, um);
                item.removeProperty (IDs::fromItem, um);
            }
            if (to)
            {
                item.setProperty (IDs::x2, p.b.x, um);
                item.setProperty (IDs::y2, p.b.y, um);
                item.removeProperty (IDs::toItem, um);
            }
        }
    }

    juce::Rectangle<float> shownPart (const juce::ValueTree& board)
    {
        if (num (board, IDs::showW) <= 0.5f || num (board, IDs::showH) <= 0.5f) return {};
        return { num (board, IDs::showX), num (board, IDs::showY), num (board, IDs::showW), num (board, IDs::showH) };
    }

    void setShownPart (juce::ValueTree board, juce::Rectangle<float> r, juce::UndoManager* um)
    {
        if (r.isEmpty())
        {
            for (auto* id : { &IDs::showX, &IDs::showY, &IDs::showW, &IDs::showH })
                if (board.hasProperty (*id)) board.removeProperty (*id, um);
            return;
        }
        board.setProperty (IDs::showX, r.getX(), um);
        board.setProperty (IDs::showY, r.getY(), um);
        board.setProperty (IDs::showW, r.getWidth(), um);
        board.setProperty (IDs::showH, r.getHeight(), um);
    }

    juce::String summary (const juce::ValueTree& board)
    {
        int cards = 0, arrows = 0, shapes = 0, strokes = 0, images = 0, sounds = 0, links = 0, outs = 0;
        for (auto item : board)
            switch (kindOf (item))
            {
                case Kind::card:   ++cards; break;
                case Kind::arrow:  ++arrows; break;
                case Kind::shape:  ++shapes; break;
                case Kind::stroke: ++strokes; break;
                case Kind::image:  ++images; break;
                case Kind::soundCard: ++sounds; if (isTemporalOut (item)) ++outs; break;
                case Kind::link:   ++links; break;
                case Kind::unknown: break;
            }
        juce::StringArray parts;
        auto add = [&parts] (int n, const char* one, const char* many) { if (n > 0) parts.add (juce::String (n) + " " + (n == 1 ? one : many)); };
        add (cards, "card", "cards");
        add (arrows, "arrow", "arrows");
        add (shapes, "shape", "shapes");
        add (strokes, "drawing", "drawings");
        add (images, "image", "images");
        add (sounds, "sound card", "sound cards");
        add (links, "link", "links");
        if (outs > 0) parts.add (juce::String (outs) + " playing in the piece");
        return parts.isEmpty() ? juce::String ("empty") : parts.joinIntoString (", ");
    }

    juce::Array<juce::ValueTree> copyItems (const juce::ValueTree& board, const juce::Array<juce::ValueTree>& items)
    {
        std::map<juce::String, juce::String> ids;
        juce::Array<juce::ValueTree> copies;
        for (auto& item : items)
        {
            auto c = item.createCopy();
            const auto fresh = newId();
            ids[item[IDs::id].toString()] = fresh;
            c.setProperty (IDs::id, fresh, nullptr);
            copies.add (c);
        }
        for (int i = 0; i < copies.size(); ++i)
        {
            auto& c = copies.getReference (i);
            if (! c.hasType (IDs::ARROW)) continue;
            const auto path = board.isValid() ? arrowPath (board, items[i]) : ArrowPath();
            for (int end = 0; end < 2; ++end)
            {
                const auto& attach = end == 0 ? IDs::fromItem : IDs::toItem;
                const auto target = c[attach].toString();
                if (target.isEmpty()) continue;
                auto it = ids.find (target);
                if (it != ids.end()) { c.setProperty (attach, it->second, nullptr); continue; }
                // Its item stays behind: the end comes free where it is now.
                const auto p = end == 0 ? path.a : path.b;
                c.setProperty (end == 0 ? IDs::x1 : IDs::x2, p.x, nullptr);
                c.setProperty (end == 0 ? IDs::y1 : IDs::y2, p.y, nullptr);
                c.removeProperty (attach, nullptr);
            }
        }
        return copies;
    }

    const juce::Array<juce::Colour>& palette()
    {
        static const juce::Array<juce::Colour> colours {
            juce::Colour (0xff9a8f78),      // warm (cards)
            juce::Colour (0xff5d82bd),      // blue
            juce::Colour (0xff4ea597),      // teal
            juce::Colour (0xffc98394),      // rose
            juce::Colour (0xffe8b04b),      // amber
            juce::Colour (0xff7d7f86),      // grey
            juce::Colour (0xffcfd3dc)       // light (arrows, drawings)
        };
        return colours;
    }

    juce::Colour colourOf (const juce::ValueTree& item)
    {
        if (item.hasProperty (IDs::colour)) return juce::Colour ((juce::uint32) (juce::int64) item[IDs::colour]);
        return item.hasType (IDs::CARD) ? palette()[0] : palette()[6];
    }

    juce::StringArray labelsIn (const juce::ValueTree& projectState)
    {
        juce::StringArray labels;
        std::function<void (const juce::ValueTree&)> scan = [&] (const juce::ValueTree& t)
        {
            for (auto c : t)
            {
                if (c.hasType (IDs::CARD))
                {
                    const auto l = c[IDs::label].toString().trim();
                    if (l.isNotEmpty()) labels.addIfNotAlreadyThere (l, true);
                }
                else scan (c);
            }
        };
        scan (projectState);
        labels.sortNatural();
        for (auto* l : { "Goal", "Means", "Open", "Question", "Idea", "Material" })
            labels.addIfNotAlreadyThere (l, true);
        return labels;
    }
}
