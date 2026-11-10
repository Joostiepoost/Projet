// Lanes.cpp — what the overview lanes show (stage 4d).

#include "Lanes.h"
#include "Identifiers.h"

namespace Lanes
{
    namespace
    {
        /** Boxes on tracks that sound: not folders, not muted. */
        template <typename Fn>
        void forEachSoundingBox (Project& project, Fn&& fn)
        {
            for (auto t : project.getTracks())
            {
                if (Project::isFolder (t) || project.effectiveMute (t)) continue;
                for (auto b : t)
                    if (b.hasType (IDs::BOX)) fn (b);
            }
        }

        /** Intervals to stretches with how many overlap (touching ones join). */
        std::vector<Segment> sweep (std::vector<std::pair<double, double>> spans)
        {
            std::vector<std::pair<double, int>> events;
            for (auto& s : spans)
                if (s.second > s.first) { events.push_back ({ s.first, +1 }); events.push_back ({ s.second, -1 }); }
            // At the same time a box starting counts before one ending, so touching boxes leave no gap.
            std::sort (events.begin(), events.end(), [] (const auto& a, const auto& b)
                       { return std::abs (a.first - b.first) > 1.0e-12 ? a.first < b.first : a.second > b.second; });
            std::vector<Segment> out;
            int count = 0;
            for (size_t i = 0; i < events.size(); ++i)
            {
                count += events[i].second;
                if (i + 1 >= events.size()) break;
                const double a = events[i].first, b = events[i + 1].first;
                if (count <= 0 || b - a <= 1.0e-9) continue;
                if (! out.empty() && out.back().count == count && std::abs (out.back().end - a) < 1.0e-9) out.back().end = b;
                else out.push_back ({ a, b, count });
            }
            return out;
        }
    }

    int rowCount (const Framework& fw, const Framework::Lane& lane)
    {
        auto* c = fw.findCategory (lane.category);
        if (c == nullptr) return 1;
        const bool groups = lane.kind == Framework::Lane::Kind::rows && lane.byGroups && ! fw.groupsOf (*c).isEmpty();
        return juce::jmax (1, groups ? fw.groupsOf (*c).size() : fw.valuesOf (*c).size());
    }

    int heightFor (const Framework& fw, const Framework::Lane& lane, int size)
    {
        const int perRow = size <= 0 ? 5 : (size == 1 ? (lane.kind == Framework::Lane::Kind::scale ? 11 : 10) : 15);
        return juce::jlimit (12, 90, rowCount (fw, lane) * perRow + 4);
    }

    std::vector<Piece> piecesOf (Project& project, const juce::ValueTree& box, const Framework::Category& c)
    {
        std::vector<Piece> out;
        const double s = Project::boxStart (box), e = Project::boxEnd (box), len = e - s;
        if (len <= 0.0) return out;
        auto current = project.frameworkValue (box, c).value;
        auto changes = c.along ? Project::changesAlong (box, c) : juce::Array<Along::Change>();
        std::sort (changes.begin(), changes.end(), [] (const Along::Change& a, const Along::Change& b) { return a.at < b.at; });
        double t0 = s;
        for (auto& ch : changes)
        {
            const double t = s + juce::jlimit (0.0, 1.0, ch.at) * len;
            if (t > t0 && current.isNotEmpty()) out.push_back ({ t0, t, current });
            current = ch.to;
            t0 = juce::jmax (t0, t);
        }
        if (e > t0 && current.isNotEmpty()) out.push_back ({ t0, e, current });
        return out;
    }

    std::vector<Row> rows (Project& project, const Framework& fw, const Framework::Lane& lane)
    {
        std::vector<Row> out;
        auto* cat = fw.findCategory (lane.category);
        if (cat == nullptr) return out;
        auto* second = lane.second.isNotEmpty() ? fw.findCategory (lane.second) : nullptr;

        auto values = fw.valuesOf (*cat);
        std::stable_sort (values.begin(), values.end(), [] (const Framework::Value& a, const Framework::Value& b) { return a.order < b.order; });
        if (lane.byGroups && ! fw.groupsOf (*cat).isEmpty())
            for (auto& g : fw.groupsOf (*cat)) out.push_back ({ g.id, g.name, g.colours.colour, {}, {}, {} });
        else
            for (auto& v : values) out.push_back ({ v.id, v.name, fw.valueColours (*cat, v.id).colour, {}, {}, {} });
        const bool groups = lane.byGroups && ! fw.groupsOf (*cat).isEmpty();

        auto rowsOf = [&] (const juce::String& valueId)
        {
            juce::Array<int> idx;
            if (! groups)
            {
                for (size_t i = 0; i < out.size(); ++i) if (out[i].id == valueId) idx.add ((int) i);
                return idx;
            }
            if (auto* v = fw.findValue (*cat, valueId))
                for (auto& g : v->groups)                       // a value in two families counts in both
                    for (size_t i = 0; i < out.size(); ++i) if (out[i].id == g) idx.addIfNotAlreadyThere ((int) i);
            return idx;
        };

        std::vector<std::vector<std::pair<double, double>>> mainSpans (out.size()), secondSpans (out.size());
        forEachSoundingBox (project, [&] (const juce::ValueTree& box)
        {
            const auto id = box[IDs::id].toString();
            for (int pass = 0; pass < 2; ++pass)
            {
                auto* c = pass == 0 ? cat : second;
                if (c == nullptr) continue;
                for (auto& piece : piecesOf (project, box, *c))
                    for (int r : rowsOf (piece.value))
                    {
                        (pass == 0 ? mainSpans : secondSpans)[(size_t) r].push_back ({ piece.start, piece.end });
                        out[(size_t) r].boxIds.addIfNotAlreadyThere (id);
                    }
            }
        });
        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i].main = sweep (mainSpans[i]);
            out[i].second = sweep (secondSpans[i]);
        }
        return out;
    }

    Scale scale (Project& project, const Framework& fw, const Framework::Lane& lane)
    {
        Scale out;
        auto* cat = fw.findCategory (lane.category);
        if (cat == nullptr) return out;
        auto* weights = lane.weightCategory.isNotEmpty() ? fw.findCategory (lane.weightCategory) : nullptr;

        auto values = fw.valuesOf (*cat);
        std::stable_sort (values.begin(), values.end(), [] (const Framework::Value& a, const Framework::Value& b) { return a.order < b.order; });
        for (auto& v : values) out.ids.add (v.id);
        if (out.ids.isEmpty()) return out;

        struct Box { std::vector<Piece> levels, weights; };
        std::vector<Box> boxes;
        std::vector<double> cuts;
        forEachSoundingBox (project, [&] (const juce::ValueTree& b)
        {
            Box box;
            box.levels = piecesOf (project, b, *cat);
            if (box.levels.empty()) return;
            if (weights != nullptr) box.weights = piecesOf (project, b, *weights);
            for (auto& p : box.levels) { cuts.push_back (p.start); cuts.push_back (p.end); }
            for (auto& p : box.weights) { cuts.push_back (p.start); cuts.push_back (p.end); }
            boxes.push_back (std::move (box));
        });
        std::sort (cuts.begin(), cuts.end());
        cuts.erase (std::unique (cuts.begin(), cuts.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-9; }), cuts.end());

        auto at = [] (const std::vector<Piece>& pieces, double t) -> const Piece*
        {
            for (auto& p : pieces) if (t >= p.start && t < p.end) return &p;
            return nullptr;
        };

        for (size_t i = 0; i + 1 < cuts.size(); ++i)
        {
            const double a = cuts[i], b = cuts[i + 1], m = (a + b) / 2.0;
            double low = 1.0e9, high = -1.0e9, sum = 0.0, total = 0.0;
            for (auto& box : boxes)
            {
                auto* level = at (box.levels, m);
                if (level == nullptr) continue;
                const int pos = out.ids.indexOf (level->value);
                if (pos < 0) continue;                  // an unknown value: left out
                auto* w = at (box.weights, m);
                const double weight = weights == nullptr ? 1.0 : (w != nullptr ? lane.weightFor (w->value) : lane.defaultWeight);
                low = juce::jmin (low, (double) pos);
                high = juce::jmax (high, (double) pos);
                sum += weight * pos;
                total += weight;
            }
            if (high < low) continue;
            const double mean = total > 0.0 ? sum / total : (low + high) / 2.0;
            auto& segs = out.segments;
            if (! segs.empty() && std::abs (segs.back().end - a) < 1.0e-9 && std::abs (segs.back().low - low) < 1.0e-9
                && std::abs (segs.back().high - high) < 1.0e-9 && std::abs (segs.back().mean - mean) < 1.0e-9)
                segs.back().end = b;
            else
                segs.push_back ({ a, b, low, high, mean });
        }
        return out;
    }
}
