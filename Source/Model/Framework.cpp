// Framework.cpp

#include "Framework.h"

namespace
{
    Framework::Name readName (const juce::var& v)
    {
        if (v.isString()) return { v.toString(), v.toString() };
        return { v["fr"].toString(), v["en"].toString() };
    }

    juce::uint32 readColour (const juce::var& v, juce::uint32 fallback)
    {
        auto s = v.toString().trim().trimCharactersAtStart ("#");
        if (s.length() == 6 && s.containsOnly ("0123456789abcdefABCDEF")) return 0xff000000u | (juce::uint32) s.getHexValue32();
        if (s.length() == 8 && s.containsOnly ("0123456789abcdefABCDEF")) return (juce::uint32) s.getHexValue32();
        return fallback;
    }

    /** Darker or lighter by mixing with black or white (no graphics module here). */
    juce::uint32 mix (juce::uint32 c, juce::uint32 with, double amount)
    {
        auto channel = [&] (int shift)
        {
            const double a = (double) ((c >> shift) & 0xffu), b = (double) ((with >> shift) & 0xffu);
            return (juce::uint32) juce::roundToInt (a + (b - a) * amount) << shift;
        };
        return 0xff000000u | channel (16) | channel (8) | channel (0);
    }

    Framework::Colours readColours (const juce::var& v)
    {
        Framework::Colours c;
        c.colour = readColour (v["colour"], 0);
        c.dark = readColour (v["dark"], c.colour != 0 ? mix (c.colour, 0xff000000u, 0.6) : 0u);
        c.light = readColour (v["light"], c.colour != 0 ? mix (c.colour, 0xffffffffu, 0.4) : 0u);
        return c;
    }

    juce::StringArray readStrings (const juce::var& v)
    {
        juce::StringArray out;
        if (auto* a = v.getArray())
            for (auto& item : *a) out.add (item.toString());
        else if (v.isString() && v.toString().isNotEmpty())
            out.add (v.toString());
        return out;
    }

    /** Package and category ids: lower case letters, digits, - and _. */
    juce::String cleanId (const juce::String& s)
    {
        return s.trim().toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789-_");
    }

    /** Value ids may hold most characters (N'', X'), but not the separators
        of tags and of changes along a box. */
    juce::String cleanValueId (const juce::String& s)
    {
        return s.trim().removeCharacters (",;:");
    }

    juce::String trimNumber (double v)
    {
        auto s = juce::String (v, 4);
        if (s.containsChar ('.')) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s.isEmpty() || s == "-0" ? juce::String ("0") : s;
    }

    /** {"category": "junction", "values": [...]}; the key is completed on rebuild. */
    Framework::Condition readCondition (const juce::var& v)
    {
        Framework::Condition c;
        if (v.getDynamicObject() == nullptr) return c;
        c.category = v["category"].toString().trim();
        for (auto& id : readStrings (v["values"])) c.values.add (cleanValueId (id));
        if (c.values.isEmpty()) c.category = {};
        return c;
    }

    Framework::Value readValue (const juce::var& v, int index, const juce::String& from)
    {
        Framework::Value x;
        x.id = cleanValueId (v["id"].toString());
        x.from = from;
        x.label = v["label"].toString();
        x.glyph = v.hasProperty ("glyph") ? v["glyph"].toString() : x.id;
        x.fill = v["fill"].toString();
        x.name = readName (v["name"]);
        if (x.name.isEmpty()) x.name = { x.id, x.id };
        x.name2 = readName (v["name2"]);
        x.about = readName (v["about"]);
        x.groups = readStrings (v["groups"]);
        x.colours = readColours (v);
        x.order = (int) v.getProperty ("order", index + 1);
        x.weight = (int) v.getProperty ("weight", 0);
        x.row = (int) v.getProperty ("row", -1);
        x.col = (int) v.getProperty ("col", -1);
        const auto& d = v["details"];
        if (d.isArray()) x.detailsFr = x.detailsEn = readStrings (d);
        else { x.detailsFr = readStrings (d["fr"]); x.detailsEn = readStrings (d["en"]); }

        if (auto* env = v["env"].getArray())                // [[t, a], ...]
        {
            juce::Array<float> pairs;
            for (auto& p : *env)
                if (auto* pa = p.getArray(); pa != nullptr && pa->size() >= 2)
                    pairs.addArray ({ juce::jlimit (0.0f, 1.0f, (float) (double) pa->getUnchecked (0)),
                                      juce::jlimit (0.0f, 1.0f, (float) (double) pa->getUnchecked (1)) });
            for (int i = 2; i < pairs.size(); i += 2)       // keep t ascending
                if (pairs[i] < pairs[i - 2])
                    pairs.set (i, pairs[i - 2]);
            if (pairs.size() >= 4) x.env = pairs;
        }
        x.peak = (int) v.getProperty ("peak", -1);
        if (! juce::isPositiveAndBelow (x.peak, x.env.size() / 2)) x.peak = -1;
        x.curve = v["curve"].toString().trim().toLowerCase();
        x.sign = v["sign"].toString().trim();
        x.seamSign = v["seamSign"].toString().trim();
        x.seamFade = juce::jlimit (0.0f, 40.0f, (float) (double) v.getProperty ("seamFade", 0.0));
        if (auto* at = v["endAt"].getArray(); at != nullptr && at->size() >= 2)
        {
            x.endX = (float) (double) at->getUnchecked (0);
            x.endY = (float) (double) at->getUnchecked (1);
        }
        if (v.hasProperty ("fadeAt")) x.fadeX = (float) (double) v["fadeAt"];
        x.onlyWith = readCondition (v["onlyWith"]);
        x.impliesCategory = v["implies"]["category"].toString().trim();
        x.impliesValue = cleanValueId (v["implies"]["value"].toString());
        if (x.impliesValue.isEmpty()) x.impliesCategory = {};
        return x;
    }

    Framework::Group readGroup (const juce::var& v)
    {
        Framework::Group g;
        g.id = cleanValueId (v["id"].toString());
        g.name = readName (v["name"]);
        if (g.name.isEmpty()) g.name = { g.id, g.id };
        g.about = readName (v["about"]);
        g.colours = readColours (v);
        return g;
    }

    juce::Array<Framework::Axis> readAxes (const juce::var& v)
    {
        juce::Array<Framework::Axis> out;
        if (auto* a = v.getArray())
            for (auto& item : *a)
                out.add ({ item["id"].toString(), readName (item["name"]) });
        return out;
    }

    bool readCategory (const juce::var& v, const juce::String& package, Framework::Category& c, juce::StringArray& warnings)
    {
        c.id = cleanId (v["id"].toString());
        if (c.id.isEmpty())
        {
            warnings.add ("A category without an id was left out.");
            return false;
        }
        c.package = package;
        c.name = readName (v["name"]);
        if (c.name.isEmpty()) c.name = { c.id, c.id };
        c.about = readName (v["about"]);

        const auto kind = v.getProperty ("kind", "choice").toString().trim().toLowerCase();
        if (kind == "choice") c.kind = Framework::Kind::choice;
        else if (kind == "tags") c.kind = Framework::Kind::tags;
        else if (kind == "number") c.kind = Framework::Kind::number;
        else if (kind == "text") c.kind = Framework::Kind::text;
        else
        {
            warnings.add ("The category " + c.id + " has an unknown kind (" + kind + "): left out. Kinds: choice, tags, number, text.");
            return false;
        }

        c.control = v["control"].toString().trim().toLowerCase();
        c.along = (bool) v.getProperty ("along", false);
        const auto folders = v.getProperty ("folders", false);
        c.foldersOnly = folders.toString().trim().toLowerCase() == "only";
        c.folders = c.foldersOnly || (folders.isBool() ? (bool) folders : folders.toString().trim().toLowerCase() == "true");
        c.showDefault = (bool) v.getProperty ("showDefault", true);
        c.defaultValue = cleanValueId (v["default"].toString());
        c.valuesFrom = v["valuesFrom"].toString().trim();
        if (c.valuesFrom.isNotEmpty() && ! c.valuesFrom.containsChar ('.')) c.valuesFrom = package + "." + cleanId (c.valuesFrom);
        c.mark = Framework::markFromName (v["mark"].toString());
        c.colour = readColour (v["colour"], 0);
        c.minimum = (double) v.getProperty ("min", 0.0);
        c.maximum = (double) v.getProperty ("max", 1.0);
        if (c.maximum <= c.minimum) c.maximum = c.minimum + 1.0;
        c.step = juce::jmax (0.0, (double) v.getProperty ("step", 0.0));
        c.unit = v["unit"].toString();
        c.groupsName = readName (v["groups"]["name"]);
        if (auto* a = v["groups"]["list"].getArray())
            for (auto& g : *a)
            {
                auto group = readGroup (g);
                if (group.id.isNotEmpty()) c.groups.add (group);
            }
        c.detailsLabel = readName (v["details"]);
        c.firstNames = readName (v["names"]["first"]);
        c.secondNames = readName (v["names"]["second"]);
        c.rows = readAxes (v["chart"]["rows"]);
        c.columns = readAxes (v["chart"]["columns"]);
        c.chartCaption = readName (v["chart"]["caption"]);

        // Stage 4c: joins, ends, conditions, profile shaping.
        c.between = v["between"].toString().trim().toLowerCase() == "previous";
        c.atEnd = (bool) v.getProperty ("atEnd", false);
        if (auto* rules = v["derive"].getArray())
            for (auto& r : *rules)
            {
                Framework::Rule rule;
                rule.value = cleanValueId (r["value"].toString());
                if (r.hasProperty ("from") && ! r["from"].isVoid()) rule.from = (double) r["from"];
                if (r.hasProperty ("to") && ! r["to"].isVoid()) rule.to = (double) r["to"];
                if (r.hasProperty ("covered")) rule.covered = (bool) r["covered"] ? 1 : 0;
                if (rule.value.isNotEmpty()) c.derive.add (rule);
            }
        if (auto* notes = v["notes"].getArray())
            for (auto& n : *notes)
                c.notes.add ({ (double) n.getProperty ("from", 0.0), (double) n.getProperty ("to", 0.0), readName (n["text"]) });
        c.onlyWith = readCondition (v["onlyWith"]);
        c.envelope = v["envelope"].toString().trim().toLowerCase();

        if (auto* a = v["values"].getArray())
        {
            juce::StringArray seen;
            for (int i = 0; i < a->size(); ++i)
            {
                auto value = readValue (a->getReference (i), i, package);
                if (value.id.isEmpty()) { warnings.add ("A value without an id in " + c.id + " was left out."); continue; }
                if (seen.contains (value.id)) { warnings.add ("The value " + value.id + " appears twice in " + c.id + "; the second was left out."); continue; }
                seen.add (value.id);
                c.values.add (value);
            }
        }
        if (c.kind != Framework::Kind::choice && c.along)
        {
            warnings.add ("Only a choice can change along a box (" + c.id + ").");
            c.along = false;
        }
        if (c.kind == Framework::Kind::number && c.defaultValue.isNotEmpty())
            c.defaultValue = juce::String (juce::jlimit (c.minimum, c.maximum, c.defaultValue.getDoubleValue()));
        else if (c.kind != Framework::Kind::choice) c.defaultValue = {};
        if (c.between && c.kind != Framework::Kind::choice)
        {
            warnings.add ("Only a choice can belong to the join with the box before (" + c.id + ").");
            c.between = false;
        }
        if (c.between && (c.along || c.folders))
        {
            warnings.add ("A join (" + c.id + ") cannot change along a box or come from a folder.");
            c.along = c.folders = false;
        }
        if (! c.between) c.derive.clear();
        if (c.foldersOnly && (c.along || c.between || c.atEnd || c.envelope.isNotEmpty()))
        {
            warnings.add ("A category set on folders only (" + c.id + ") cannot change along a box, belong to a join or describe an ending.");
            c.along = c.between = c.atEnd = false;
            c.envelope = {};
        }
        if (c.envelope.isNotEmpty() && c.envelope != "peak" && c.envelope != "shape") c.envelope = {};
        if (c.envelope == "peak" && c.kind != Framework::Kind::number) c.envelope = {};
        if (c.envelope == "shape" && c.kind != Framework::Kind::choice) c.envelope = {};
        return true;
    }
}

//== Packages ==================================================================

bool Framework::addPackage (const juce::String& json, const juce::String& folderName, juce::String& error,
                            const juce::String& script)
{
    juce::var root;
    const auto parsed = juce::JSON::parse (json, root);
    if (parsed.failed()) { error = "not valid JSON: " + parsed.getErrorMessage(); return false; }
    if (root.getDynamicObject() == nullptr) { error = "the file does not hold a framework package."; return false; }

    Raw raw;
    auto& p = raw.package;
    p.id = cleanId (root.hasProperty ("id") ? root["id"].toString() : folderName);
    if (p.id.isEmpty()) p.id = cleanId (folderName);
    if (p.id.isEmpty()) { error = "the package has no id."; return false; }
    p.version = (int) root.getProperty ("version", 1);
    p.order = (int) root.getProperty ("order", 50);
    p.name = readName (root["name"]);
    if (p.name.isEmpty()) p.name = { p.id, p.id };
    p.about = readName (root["about"]);
    p.status = root["status"].toString();
    p.script = script;
    p.json = root;
    raw.additions = root["additions"];

    if (root.hasProperty ("categories") && ! root["categories"].isArray())
    {
        error = "\"categories\" should be a list.";
        return false;
    }
    if (auto* a = root["categories"].getArray())
        for (auto& v : *a)
        {
            Category c;
            if (! readCategory (v, p.id, c, p.warnings)) continue;
            bool duplicate = false;
            for (auto& other : raw.categories) duplicate = duplicate || other.id == c.id;
            if (duplicate) { p.warnings.add ("The category " + c.id + " appears twice; the second was left out."); continue; }
            raw.categories.add (c);
        }

    if (auto* a = root["lanes"].getArray())
        for (auto& v : *a)
        {
            Lane lane;
            lane.id = cleanId (v["id"].toString());
            lane.package = p.id;
            if (lane.id.isEmpty()) { p.warnings.add ("A lane without an id was left out."); continue; }
            lane.name = readName (v["name"]);
            if (lane.name.isEmpty()) lane.name = { lane.id, lane.id };
            lane.about = readName (v["about"]);
            const auto kind = v.getProperty ("kind", "rows").toString().trim().toLowerCase();
            if (kind == "rows") lane.kind = Lane::Kind::rows;
            else if (kind == "scale") lane.kind = Lane::Kind::scale;
            else { p.warnings.add ("The lane " + lane.id + " has an unknown kind (" + kind + "): left out. Kinds: rows, scale."); continue; }
            auto keyOf = [&p] (const juce::var& x)
            {
                auto k = x.toString().trim();
                return k.isEmpty() || k.containsChar ('.') ? k : p.id + "." + cleanId (k);
            };
            lane.category = keyOf (v["category"]);
            lane.second = keyOf (v["second"]);
            lane.byGroups = v.getProperty ("rows", "groups").toString().trim().toLowerCase() != "values";
            lane.weightCategory = keyOf (v["weightBy"]["category"]);
            lane.defaultWeight = (double) v["weightBy"].getProperty ("default", 1.0);
            if (auto* w = v["weightBy"]["weights"].getDynamicObject())
                for (auto& nv : w->getProperties())
                {
                    lane.weightIds.add (nv.name.toString());
                    lane.weightValues.add ((double) nv.value);
                }
            bool duplicate = false;
            for (auto& other : raw.lanes) duplicate = duplicate || other.id == lane.id;
            if (duplicate) { p.warnings.add ("The lane " + lane.id + " appears twice; the second was left out."); continue; }
            raw.lanes.add (lane);
        }

    for (int i = raws.size(); --i >= 0;)
        if (raws.getReference (i).package.id == p.id) raws.remove (i);
    raws.add (raw);
    rebuild();
    error = {};
    return true;
}

void Framework::removePackage (const juce::String& id)
{
    for (int i = raws.size(); --i >= 0;)
        if (raws.getReference (i).package.id == id) raws.remove (i);
    rebuild();
}

void Framework::clear()
{
    raws.clear();
    rebuild();
}

void Framework::rebuild()
{
    packages.clear();
    categories.clear();
    lanes.clear();
    implications.clear();
    ++generation;
    std::stable_sort (raws.begin(), raws.end(), [] (const Raw& a, const Raw& b)
    {
        return a.package.order != b.package.order ? a.package.order < b.package.order : a.package.id < b.package.id;
    });

    for (auto& raw : raws)
    {
        auto p = raw.package;
        p.categories.clear();
        for (auto c : raw.categories)
        {
            p.categories.add (c.key());
            categories.add (c);
        }
        packages.add (p);
    }

    auto packageIndex = [this] (const juce::String& id)
    {
        for (int i = 0; i < packages.size(); ++i) if (packages.getReference (i).id == id) return i;
        return -1;
    };
    auto categoryIndex = [this] (const juce::String& key)
    {
        for (int i = 0; i < categories.size(); ++i) if (categories.getReference (i).key() == key) return i;
        return -1;
    };

    // Additions: values (and groups) one package adds to another's category.
    for (auto& raw : raws)
        if (auto* list = raw.additions.getArray())
            for (auto& add : *list)
            {
                auto& warnings = packages.getReference (packageIndex (raw.package.id)).warnings;
                const auto target = add["category"].toString().trim();
                const int ci = categoryIndex (target);
                if (ci < 0) { warnings.add ("Adds to " + target + ", which no loaded package has."); continue; }
                auto& c = categories.getReference (ci);
                if (auto* gs = add["groups"].getArray())
                    for (auto& gv : *gs)
                    {
                        auto g = readGroup (gv);
                        bool exists = g.id.isEmpty();
                        for (auto& other : c.groups) exists = exists || other.id == g.id;
                        if (! exists) c.groups.add (g);
                    }
                if (auto* vs = add["values"].getArray())
                    for (int i = 0; i < vs->size(); ++i)
                    {
                        auto v = readValue (vs->getReference (i), c.values.size() + i, raw.package.id);
                        if (v.id.isEmpty()) continue;
                        bool exists = false;
                        for (auto& other : c.values) exists = exists || other.id == v.id;
                        if (exists) { warnings.add (target + " already has a value " + v.id + "; this addition was left out."); continue; }
                        c.values.add (v);
                    }
            }

    // Shared values, defaults, controls.
    for (int i = 0; i < categories.size(); ++i)
    {
        auto& c = categories.getReference (i);
        c.index = i;
        auto& warnings = packages.getReference (packageIndex (c.package)).warnings;
        if (c.valuesFrom.isNotEmpty())
        {
            const int si = categoryIndex (c.valuesFrom);
            if (si < 0 || si == i || categories.getReference (si).valuesFrom.isNotEmpty()
                || categories.getReference (si).kind != c.kind)
            {
                warnings.add (c.key() + " takes its values from " + c.valuesFrom + ", which is missing or does not fit; it has no values.");
                c.valuesFrom = {};
            }
        }
    }
    for (auto& c : categories)
    {
        auto& warnings = packages.getReference (packageIndex (c.package)).warnings;
        const auto& src = source (c);
        c.hasTwoNames = false;
        for (auto& v : src.values) c.hasTwoNames = c.hasTwoNames || ! v.name2.isEmpty();
        if (c.kind == Kind::choice && c.defaultValue.isNotEmpty() && findValue (c, c.defaultValue) == nullptr)
        {
            warnings.add ("The default of " + c.key() + " (" + c.defaultValue + ") is not one of its values.");
            c.defaultValue = {};
        }
        if (c.kind == Kind::choice)
        {
            const bool chartable = ! src.rows.isEmpty() && ! src.columns.isEmpty();
            if (c.control == "chart" && ! chartable) c.control = {};
            if (c.control != "menu" && c.control != "buttons" && c.control != "chart")
                c.control = chartable ? "chart" : (src.values.size() <= 5 ? "buttons" : "menu");
            if (c.rows.isEmpty()) { c.rows = src.rows; c.columns = src.columns; c.chartCaption = src.chartCaption; }
        }
        if (! marksFor (c).contains (c.mark)) c.mark = Mark::none;

        // Rules naming no value of the category are left out.
        for (int r = c.derive.size(); --r >= 0;)
            if (findValue (c, c.derive.getReference (r).value) == nullptr)
            {
                warnings.add ("A timing rule of " + c.key() + " names " + c.derive.getReference (r).value + ", which is not one of its values.");
                c.derive.remove (r);
            }
    }

    // "Only with": complete the keys; a condition on a missing category is dropped.
    auto resolve = [categoryIndex] (Condition& cond, const juce::String& package, juce::StringArray& warnings, const juce::String& what)
    {
        if (! cond.isSet()) return;
        if (! cond.category.containsChar ('.')) cond.category = package + "." + cleanId (cond.category);
        if (categoryIndex (cond.category) < 0)
        {
            warnings.add (what + " applies only with " + cond.category + ", which no loaded package has; it applies always.");
            cond = {};
        }
    };
    for (auto& c : categories)
    {
        auto& warnings = packages.getReference (packageIndex (c.package)).warnings;
        resolve (c.onlyWith, c.package, warnings, c.key());
        if (c.onlyWith.category == c.key()) c.onlyWith = {};
        for (auto& v : c.values)
        {
            resolve (v.onlyWith, v.from.isNotEmpty() ? v.from : c.package, warnings, c.key() + " " + v.id);
            if (v.onlyWith.category == c.key()) v.onlyWith = {};
        }
    }

    // Implications (4d): a folder value giving the boxes inside a value of another category.
    implications.resize (categories.size());
    for (int i = 0; i < categories.size(); ++i)
    {
        auto& c = categories.getReference (i);
        auto& warnings = packages.getReference (packageIndex (c.package)).warnings;
        for (auto& v : c.values)
        {
            if (v.impliesCategory.isEmpty()) continue;
            auto key = v.impliesCategory;
            if (! key.containsChar ('.')) key = (v.from.isNotEmpty() ? v.from : c.package) + "." + cleanId (key);
            const int ti = categoryIndex (key);
            if (! c.folders || ti < 0 || ti == i || findValue (categories.getReference (ti), v.impliesValue) == nullptr)
            {
                if (ti >= 0 || packageIndex (key.upToFirstOccurrenceOf (".", false, false)) >= 0)
                    warnings.add (c.key() + " " + v.id + " implies " + key + " " + v.impliesValue
                                  + (c.folders ? ", which is not there; left out." : ", but only folder values can imply; left out."));
                continue;
            }
            v.impliesCategory = key;
            implications.getReference (ti).add ({ i, v.id, v.impliesValue });
        }
    }

    // Lanes: their categories must be there.
    for (auto& raw : raws)
        for (auto lane : raw.lanes)
        {
            auto& warnings = packages.getReference (packageIndex (raw.package.id)).warnings;
            const int ci = categoryIndex (lane.category);
            if (ci < 0) { warnings.add ("The lane " + lane.key() + " draws " + lane.category + ", which no loaded package has; left out."); continue; }
            if (categories.getReference (ci).kind != Kind::choice) { warnings.add ("The lane " + lane.key() + " needs a choice to draw; left out."); continue; }
            if (lane.second.isNotEmpty() && categoryIndex (lane.second) < 0) lane.second = {};
            if (lane.weightCategory.isNotEmpty() && categoryIndex (lane.weightCategory) < 0) lane.weightCategory = {};
            lanes.add (lane);
        }
}

const juce::Array<Framework::Implication>& Framework::implicationsFor (const Category& c) const
{
    static const juce::Array<Implication> none;
    return juce::isPositiveAndBelow (c.index, implications.size()) ? implications.getReference (c.index) : none;
}

const Framework::Lane* Framework::findLane (const juce::String& key) const
{
    for (auto& l : lanes) if (l.key() == key) return &l;
    return nullptr;
}

const Framework::Package* Framework::findPackage (const juce::String& id) const
{
    for (auto& p : packages) if (p.id == id) return &p;
    return nullptr;
}

const Framework::Category* Framework::findCategory (const juce::String& key) const
{
    for (auto& c : categories) if (c.key() == key) return &c;
    return nullptr;
}

const Framework::Category& Framework::source (const Category& c) const
{
    if (c.valuesFrom.isNotEmpty())
        if (auto* s = findCategory (c.valuesFrom)) return *s;
    return c;
}

const Framework::Value* Framework::findValue (const Category& c, const juce::String& id) const
{
    if (id.isEmpty()) return nullptr;
    for (auto& v : valuesOf (c)) if (v.id == id) return &v;
    return nullptr;
}

const Framework::Group* Framework::findGroup (const Category& c, const juce::String& id) const
{
    for (auto& g : groupsOf (c)) if (g.id == id) return &g;
    return nullptr;
}

juce::Array<const Framework::Category*> Framework::sharing (const Category& c) const
{
    juce::Array<const Category*> out;
    const auto key = source (c).key();
    for (auto& other : categories)
        if (source (other).key() == key) out.add (&other);
    return out;
}

//== Names =====================================================================

juce::String Framework::text (const Name& n) const
{
    if (language == "en") return n.en.isNotEmpty() ? n.en : n.fr;
    return n.fr.isNotEmpty() ? n.fr : n.en;
}

void Framework::setNaming (const juce::String& key, const juce::String& naming)
{
    if (naming == "first" || naming == "second") namings.set (key, naming);
    else namings.remove (juce::StringRef (key));
}

juce::String Framework::getNaming (const juce::String& key) const
{
    const auto n = namings[key];
    return n.isEmpty() ? juce::String ("both") : n;
}

juce::String Framework::namingsToString() const
{
    return formatPairs (namings);
}

void Framework::namingsFromString (const juce::String& s)
{
    namings.clear();
    const auto pairs = parsePairs (s);
    for (int i = 0; i < pairs.size(); ++i)
        setNaming (pairs.getAllKeys()[i], pairs.getAllValues()[i]);
}

juce::StringPairArray Framework::parsePairs (const juce::String& s)
{
    juce::StringPairArray out;
    for (auto item : juce::StringArray::fromTokens (s, ";", {}))
        if (item.containsChar ('='))
            out.set (item.upToFirstOccurrenceOf ("=", false, false).trim(), item.fromFirstOccurrenceOf ("=", false, false).trim());
    return out;
}

juce::String Framework::formatPairs (const juce::StringPairArray& pairs)
{
    juce::StringArray items;
    for (int i = 0; i < pairs.size(); ++i)
        if (pairs.getAllValues()[i].isNotEmpty())
            items.add (pairs.getAllKeys()[i] + "=" + pairs.getAllValues()[i]);
    return items.joinIntoString (";");
}

juce::String Framework::valueName (const Category& c, const juce::String& id) const
{
    if (id.isEmpty()) return {};
    auto* v = findValue (c, id);
    if (v == nullptr) return id + "?";
    if (! c.hasTwoNames || v->name2.isEmpty()) return text (v->name);
    const auto naming = getNaming (source (c).key());
    const auto label = v->label.isNotEmpty() && v->label != text (v->name) ? v->label + " " : juce::String();
    if (naming == "second") return text (v->name2);
    if (naming == "first") return label + text (v->name);
    return label + text (v->name) + " / " + text (v->name2);
}

juce::String Framework::valueLabel (const Category& c, const juce::String& id) const
{
    if (id.isEmpty()) return {};
    auto* v = findValue (c, id);
    if (v == nullptr) return id + "?";
    if (v->label.isNotEmpty()) return v->label;
    if (c.hasTwoNames && getNaming (source (c).key()) == "second" && ! v->name2.isEmpty()) return text (v->name2);
    return text (v->name);
}

juce::StringArray Framework::valueDetails (const Value& v) const
{
    if (language == "en") return v.detailsEn.isEmpty() ? v.detailsFr : v.detailsEn;
    return v.detailsFr.isEmpty() ? v.detailsEn : v.detailsFr;
}

Framework::Colours Framework::valueColours (const Category& c, const juce::String& id) const
{
    auto* v = findValue (c, id);
    if (v == nullptr) return {};
    if (v->colours.colour != 0) return v->colours;
    for (auto& g : v->groups)
        if (auto* group = findGroup (c, g))
            if (group->colours.colour != 0) return group->colours;
    return {};
}

bool Framework::hasColours (const Category& c) const
{
    for (auto& v : valuesOf (c)) if (v.colours.colour != 0) return true;
    for (auto& g : groupsOf (c)) if (g.colours.colour != 0) return true;
    return false;
}

juce::String Framework::formatNumber (const Category& c, double v) const
{
    juce::String s;
    if (c.step >= 1.0) s = juce::String (juce::roundToInt (v));
    else if (c.step > 0.0) s = juce::String (v, juce::jlimit (1, 4, (int) std::ceil (-std::log10 (c.step) - 1.0e-9)));
    else s = trimNumber (v);
    return c.unit.isNotEmpty() ? s + " " + c.unit : s;
}

juce::String Framework::kindName (Kind k)
{
    switch (k)
    {
        case Kind::choice: return "choice";
        case Kind::tags:   return "tags";
        case Kind::number: return "number";
        case Kind::text:   return "text";
    }
    return {};
}

juce::String Framework::markName (Mark m)
{
    switch (m)
    {
        case Mark::none:  return "none";
        case Mark::strip: return "strip";
        case Mark::badge: return "badge";
        case Mark::label: return "label";
        case Mark::glyph: return "glyph";
        case Mark::seam:  return "seam";
        case Mark::end:   return "end";
    }
    return {};
}

Framework::Mark Framework::markFromName (const juce::String& s, Mark fallback)
{
    const auto n = s.trim().toLowerCase();
    if (n == "strip") return Mark::strip;
    if (n == "badge") return Mark::badge;
    if (n == "label") return Mark::label;
    if (n == "glyph") return Mark::glyph;
    if (n == "seam") return Mark::seam;
    if (n == "end") return Mark::end;
    if (n == "none") return Mark::none;
    return fallback;
}

juce::Array<Framework::Mark> Framework::marksFor (const Category& c)
{
    juce::Array<Mark> out { Mark::none };
    if (c.foldersOnly)
    {
        out.add (Mark::label);                  // on the folder's lane
        return out;
    }
    if (c.between)
    {
        out.addArray ({ Mark::seam, Mark::badge, Mark::label });
        return out;
    }
    if (c.kind == Kind::choice || c.kind == Kind::number) out.add (Mark::strip);
    if (c.kind == Kind::choice) out.add (Mark::glyph);
    bool signs = false;
    for (auto& v : c.values) signs = signs || v.sign.isNotEmpty();
    if (c.kind == Kind::choice && signs) out.add (Mark::end);
    out.add (Mark::badge);
    out.add (Mark::label);
    return out;
}

juce::Array<float> Framework::shapeEnvelope (const juce::Array<float>& env, int peakIndex, double peakAt, bool soft)
{
    auto out = env;
    const int n = out.size() / 2;
    if (n < 2) return out;

    // The peak moves; the points before and after it stretch to follow.
    if (peakAt >= 0.0 && juce::isPositiveAndBelow (peakIndex, n))
    {
        const float p = juce::jlimit (0.0f, 1.0f, (float) peakAt);
        const float tp = env[peakIndex * 2];
        for (int i = 0; i < n; ++i)
        {
            const float t = env[i * 2];
            float moved = p;
            if (i < peakIndex) moved = tp > 0.0f ? t / tp * p : 0.0f;
            else if (i > peakIndex) moved = tp < 1.0f ? p + (t - tp) / (1.0f - tp) * (1.0f - p) : 1.0f;
            out.set (i * 2, moved);
        }
    }
    if (! soft) return out;

    // Rounded: each stretch follows a half cosine, flat at every point (so the peak is round).
    juce::Array<float> round;
    constexpr int steps = 48;
    for (int s = 0; s <= steps; ++s)
    {
        const float t = (float) s / (float) steps;
        int k = 0;
        while (k < n - 2 && t > out[(k + 1) * 2]) ++k;
        const float t0 = out[k * 2], a0 = out[k * 2 + 1], t1 = out[(k + 1) * 2], a1 = out[(k + 1) * 2 + 1];
        float a = t <= t0 ? a0 : t >= t1 ? a1 : 0.0f;
        if (t > t0 && t < t1)
        {
            const float u = (t - t0) / (t1 - t0);
            a = a0 + (a1 - a0) * (1.0f - std::cos (juce::MathConstants<float>::pi * u)) * 0.5f;
        }
        round.addArray ({ t, a });
    }
    return round;
}

Framework::Mark Framework::chosenMark (const Category& c, const juce::StringPairArray& display)
{
    const auto chosen = display[c.key()];
    if (chosen.isEmpty()) return c.mark;
    const auto m = markFromName (chosen, c.mark);
    return marksFor (c).contains (m) ? m : c.mark;
}

//== Files =====================================================================

juce::File Framework::appFolder()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/Projet");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Projet");
   #endif
}

namespace { juce::File libraryOverride; }

juce::File Framework::libraryFolder()
{
    return libraryOverride != juce::File() ? libraryOverride : appFolder().getChildFile ("Frameworks");
}

void Framework::setLibraryFolder (const juce::File& f)
{
    libraryOverride = f;
}

juce::Array<juce::File> Framework::packageFolders (const juce::File& parent)
{
    juce::Array<juce::File> out;
    if (! parent.isDirectory()) return out;
    for (auto& f : parent.findChildFiles (juce::File::findDirectories, false))
        if (! f.getFileName().startsWithChar ('.') && f.getChildFile ("framework.json").existsAsFile())
            out.add (f);
    std::sort (out.begin(), out.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });
    return out;
}

int Framework::folderVersion (const juce::File& packageFolder)
{
    const auto file = packageFolder.getChildFile ("framework.json");
    if (! file.existsAsFile()) return 0;
    const auto root = juce::JSON::parse (file.loadFileAsString());
    return root.getDynamicObject() != nullptr ? (int) root.getProperty ("version", 1) : 0;
}

void Framework::loadFolder (const juce::File& parent, Report& report)
{
    raws.clear();
    for (auto& folder : packageFolders (parent))
    {
        juce::String error;
        if (! addPackage (folder.getChildFile ("framework.json").loadFileAsString(), folder.getFileName(), error,
                          folder.getChildFile ("glyphs.js").loadFileAsString()))
            report.errors.add (folder.getFileName() + ": " + error);
    }
    rebuild();
    for (auto& p : packages)
        for (auto& w : p.warnings) report.notes.add (p.id + ": " + w);
}

Framework Framework::builtIn()
{
    Framework f;
    for (auto& b : builtins())
    {
        juce::String error;
        const bool ok = f.addPackage (b.json, b.id, error, b.glyphs);
        jassertquiet (ok);
        juce::ignoreUnused (ok);
    }
    return f;
}

juce::Result Framework::writePackage (const juce::File& packageFolder, const juce::String& json, const juce::String& help,
                                      const juce::String& script)
{
    if (! packageFolder.createDirectory())
        return juce::Result::fail ("Could not make " + packageFolder.getFullPathName());
    if (! packageFolder.getChildFile ("framework.json").replaceWithText (json))
        return juce::Result::fail ("Could not write " + packageFolder.getChildFile ("framework.json").getFullPathName());
    if (help.isNotEmpty() && ! packageFolder.getChildFile ("help.md").replaceWithText (help))
        return juce::Result::fail ("Could not write " + packageFolder.getChildFile ("help.md").getFullPathName());
    if (script.isNotEmpty() && ! packageFolder.getChildFile ("glyphs.js").replaceWithText (script))
        return juce::Result::fail ("Could not write " + packageFolder.getChildFile ("glyphs.js").getFullPathName());
    return juce::Result::ok();
}

juce::Result Framework::writeBuiltin (const juce::File& parent, const juce::String& id)
{
    for (auto& b : builtins())
        if (b.id == id) return writePackage (parent.getChildFile (b.id), b.json, b.help, b.glyphs);
    return juce::Result::fail ("Projet has no package " + id);
}

juce::Result Framework::copyPackage (const juce::File& packageFolder, const juce::File& parent)
{
    const auto target = parent.getChildFile (packageFolder.getFileName());
    if (target == packageFolder) return juce::Result::ok();
    if (! parent.createDirectory()) return juce::Result::fail ("Could not make " + parent.getFullPathName());
    if (target.exists() && ! target.deleteRecursively()) return juce::Result::fail ("Could not replace " + target.getFullPathName());
    if (! packageFolder.copyDirectoryTo (target)) return juce::Result::fail ("Could not copy " + packageFolder.getFullPathName());
    return juce::Result::ok();
}

namespace
{
    juce::String helpFor (const juce::String& id)
    {
        for (auto& b : Framework::builtins()) if (b.id == id) return b.help;
        return {};
    }

    juce::String glyphsFor (const juce::String& id)
    {
        for (auto& b : Framework::builtins()) if (b.id == id) return b.glyphs;
        return {};
    }

    /** Writes converted 4a packages (each with the builtin's help and glyphs) into `parent`. */
    juce::Result writeConverted (const juce::File& parent, const juce::StringArray& ids, const juce::StringArray& jsons)
    {
        for (int i = 0; i < ids.size(); ++i)
        {
            const auto r = Framework::writePackage (parent.getChildFile (ids[i]), jsons[i], helpFor (ids[i]), glyphsFor (ids[i]));
            if (r.failed()) return r;
        }
        return juce::Result::ok();
    }

    /** Keeps the 4a file next to the new folder, under a name of its own. */
    void retire (const juce::File& old)
    {
        auto target = old.getSiblingFile ("framework-4a.json");
        if (target.exists()) target = target.getNonexistentSibling();
        old.moveFileTo (target);
    }
}

namespace
{
    /** The library remembers which of Projet's packages it was given, so a
        package Projet starts shipping arrives once, and one you remove from
        the library stays removed. */
    juce::File shippedList (const juce::File& lib)        { return lib.getChildFile (".shipped"); }

    juce::StringArray readShipped (const juce::File& lib)
    {
        juce::StringArray ids;
        ids.addTokens (shippedList (lib).loadFileAsString(), "\n", {});
        ids.trim();
        ids.removeEmptyStrings();
        return ids;
    }

    void writeShipped (const juce::File& lib, const juce::StringArray& ids)
    {
        shippedList (lib).replaceWithText (ids.joinIntoString ("\n") + "\n");
    }
}

juce::Result Framework::ensureLibrary (juce::String& language, juce::String& naming, Report& report)
{
    const auto lib = libraryFolder();
    if (lib.isDirectory())
    {
        // Packages Projet has started shipping since the library was made.
        auto shipped = readShipped (lib);
        if (shipped.isEmpty())                              // a library from before 4c: what it holds counts
            for (auto& f : packageFolders (lib)) shipped.add (f.getFileName());
        juce::StringArray added;
        for (auto& b : builtins())
        {
            if (shipped.contains (b.id)) continue;
            if (! lib.getChildFile (b.id).exists())
            {
                const auto r = writePackage (lib.getChildFile (b.id), b.json, b.help, b.glyphs);
                if (r.failed()) return r;
                added.add (b.id);
            }
            shipped.add (b.id);
        }
        writeShipped (lib, shipped);
        if (! added.isEmpty())
            report.notes.add ("New in the library: " + added.joinIntoString (", ")
                              + " (Framework > Frameworks... > Use in this project).");
        return juce::Result::ok();
    }
    if (! lib.createDirectory()) return juce::Result::fail ("Could not make " + lib.getFullPathName());

    const auto old = appFolder().getChildFile ("framework.json");
    if (old.existsAsFile())
    {
        juce::StringArray ids, jsons;
        juce::String error;
        if (convertLegacy (old.loadFileAsString(), ids, jsons, language, naming, error))
        {
            const auto r = writeConverted (lib, ids, jsons);
            if (r.failed()) return r;
            retire (old);
            report.notes.add ("The app's framework file became the packages of the library (kept as framework-4a.json).");
            juce::StringArray shipped;
            for (auto& b : builtins())                      // and the packages 4a did not have
            {
                if (! lib.getChildFile (b.id).exists())
                {
                    const auto w = writePackage (lib.getChildFile (b.id), b.json, b.help, b.glyphs);
                    if (w.failed()) return w;
                }
                shipped.add (b.id);
            }
            writeShipped (lib, shipped);
            return juce::Result::ok();
        }
        report.errors.add ("The app's framework file could not be converted (" + error + "); the library starts with Projet's packages.");
    }
    juce::StringArray shipped;
    for (auto& b : builtins())
    {
        const auto r = writePackage (lib.getChildFile (b.id), b.json, b.help, b.glyphs);
        if (r.failed()) return r;
        shipped.add (b.id);
    }
    writeShipped (lib, shipped);
    return juce::Result::ok();
}

juce::Result Framework::ensureProject (const juce::File& project, juce::String& language, juce::String& naming, Report& report)
{
    const auto dir = projectFolder (project);
    if (dir.isDirectory()) return juce::Result::ok();
    if (! dir.createDirectory()) return juce::Result::fail ("Could not make " + dir.getFullPathName());

    const auto old = project.getChildFile ("framework.json");
    if (old.existsAsFile())
    {
        juce::StringArray ids, jsons;
        juce::String error;
        if (convertLegacy (old.loadFileAsString(), ids, jsons, language, naming, error))
        {
            const auto r = writeConverted (dir, ids, jsons);
            if (r.failed()) return r;
            retire (old);
            report.notes.add ("The project's framework file became packages (kept as framework-4a.json).");
            return juce::Result::ok();
        }
        report.errors.add ("The project's framework file could not be converted (" + error + "); the project gets the library's packages.");
    }

    juce::String libLanguage, libNaming;
    ensureLibrary (libLanguage, libNaming, report);
    const auto library = packageFolders (libraryFolder());
    if (library.isEmpty())
    {
        for (auto& b : builtins())
        {
            const auto r = writePackage (dir.getChildFile (b.id), b.json, b.help, b.glyphs);
            if (r.failed()) return r;
        }
        return juce::Result::ok();
    }
    for (auto& f : library)
    {
        const auto r = copyPackage (f, dir);
        if (r.failed()) return r;
    }
    return juce::Result::ok();
}

//== Stage 4a ==================================================================

juce::StringPairArray Framework::legacyFields()
{
    juce::StringPairArray m;
    m.set ("energy", "fw.composition.energy");
    m.set ("energy2", "fw.composition.energy2");
    m.set ("level", "fw.composition.level");
    m.set ("levelAlong", "fw.composition.level.along");
    m.set ("role", "fw.composition.role");
    m.set ("roleAlong", "fw.composition.role.along");
    m.set ("typology", "fw.schaeffer.type");
    return m;
}

namespace
{
    juce::DynamicObject* object (const juce::var& v)      { return v.getDynamicObject(); }

    /** The category `id` inside a parsed package. */
    juce::var categoryIn (const juce::var& package, const juce::String& id)
    {
        if (auto* a = package["categories"].getArray())
            for (auto& c : *a) if (c["id"].toString() == id) return c;
        return {};
    }

    juce::var newObject()                                   { return juce::var (new juce::DynamicObject()); }

    juce::var copyKeys (const juce::var& from, std::initializer_list<const char*> keys)
    {
        auto out = newObject();
        for (auto* k : keys)
            if (from.hasProperty (k)) object (out)->setProperty (k, from[k]);
        return out;
    }

    void setList (juce::var& target, const char* key, const juce::Array<juce::var>& items)
    {
        if (auto* o = object (target)) o->setProperty (key, juce::var (items));
    }

    juce::String canonical (juce::var v)
    {
        if (auto* o = object (v)) { o->removeProperty ("language"); o->removeProperty ("roleNames"); }
        return juce::JSON::toString (v, true);
    }
}

bool Framework::convertLegacy (const juce::String& json, juce::StringArray& ids, juce::StringArray& jsons,
                               juce::String& language, juce::String& naming, juce::String& error)
{
    juce::var root;
    const auto parsed = juce::JSON::parse (json, root);
    if (parsed.failed()) { error = "not valid JSON: " + parsed.getErrorMessage(); return false; }
    if (object (root) == nullptr) { error = "the file does not hold a framework."; return false; }

    language = root.getProperty ("language", "fr").toString().toLowerCase() == "en" ? "en" : "fr";
    const auto roleNames = root.getProperty ("roleNames", "both").toString().toLowerCase().removeCharacters (" -_");
    naming = roleNames == "thoresen" ? "first" : roleNames == "vandegorne" ? "second" : "both";

    ids.clear();
    jsons.clear();
    if (canonical (juce::JSON::parse (json)) == canonical (juce::JSON::parse (legacyDefaultJson())))
    {
        // The 4a default, unchanged: Projet's packages as they are now.
        for (auto& b : builtins()) { ids.add (b.id); jsons.add (b.json); }
        return true;
    }

    auto builtinJson = [] (const juce::String& id)
    {
        for (auto& b : builtins()) if (b.id == id) return juce::JSON::parse (b.json);
        return juce::var();
    };

    // Your framework: energies (with their families), semiotic levels, roles.
    auto composition = builtinJson ("composition");
    {
        auto energy = categoryIn (composition, "energy");
        juce::Array<juce::var> groups, values;
        if (auto* a = root["families"].getArray())
            for (auto& f : *a) groups.add (copyKeys (f, { "id", "name", "colour", "dark", "light" }));
        if (auto* a = root["energies"].getArray())
            for (auto& e : *a)
            {
                auto v = copyKeys (e, { "id", "name", "glyph" });
                object (v)->setProperty ("groups", e["families"]);
                if (e.hasProperty ("factors")) object (v)->setProperty ("details", e["factors"]);
                values.add (v);
            }
        auto groupObject = copyKeys (energy["groups"], { "name" });
        object (groupObject)->setProperty ("list", juce::var (groups));
        object (energy)->setProperty ("groups", groupObject);
        setList (energy, "values", values);

        auto level = categoryIn (composition, "level");
        juce::Array<juce::var> levels;
        if (auto* a = root["levels"].getArray())
            for (auto& l : *a) levels.add (copyKeys (l, { "id", "order", "fill", "colour", "name", "about" }));
        std::stable_sort (levels.begin(), levels.end(), [] (const juce::var& a, const juce::var& b)
                          { return (int) a.getProperty ("order", 0) < (int) b.getProperty ("order", 0); });   // concrete to abstract, as 4a showed them
        setList (level, "values", levels);

        auto role = categoryIn (composition, "role");
        juce::Array<juce::var> roles;
        juce::String def;
        if (auto* a = root["roles"].getArray())
            for (auto& r : *a)
            {
                auto v = copyKeys (r, { "id", "weight" });
                object (v)->setProperty ("label", r.hasProperty ("letter") ? r["letter"] : r["id"]);
                object (v)->setProperty ("name", r["thoresen"]);
                object (v)->setProperty ("name2", r["vandeGorne"]);
                if ((bool) r.getProperty ("default", false)) def = r["id"].toString();
                roles.add (v);
            }
        std::stable_sort (roles.begin(), roles.end(), [] (const juce::var& a, const juce::var& b)
                          { return (int) a.getProperty ("weight", 0) > (int) b.getProperty ("weight", 0); });   // strongest first, as 4a showed them
        const auto& fg = root["figureGround"];
        if (roles.size() > 1 && fg.isObject())
        {
            object (roles.getReference (0))->setProperty ("details", fg["figure"]);
            object (roles.getReference (roles.size() - 1))->setProperty ("details", fg["ground"]);
        }
        setList (role, "values", roles);
        if (def.isNotEmpty()) object (role)->setProperty ("default", def);
        else object (role)->removeProperty ("default");
    }

    // Schaeffer: the typology.
    auto schaeffer = builtinJson ("schaeffer");
    {
        const auto& t = root["typology"];
        auto type = categoryIn (schaeffer, "type");
        juce::Array<juce::var> zones, values, rows, columns;
        if (auto* a = t["zones"].getArray())
            for (auto& z : *a) zones.add (copyKeys (z, { "id", "name", "colour", "dark", "light" }));
        if (auto* a = t["masses"].getArray())
            for (auto& m : *a) rows.add (copyKeys (m, { "id", "name" }));
        if (auto* a = t["sustainments"].getArray())
            for (auto& s : *a) columns.add (copyKeys (s, { "id", "name" }));
        if (auto* a = t["types"].getArray())
            for (auto& x : *a)
            {
                auto v = copyKeys (x, { "id", "label", "row", "col", "name" });
                juce::Array<juce::var> g;
                if (x.hasProperty ("zone")) g.add (x["zone"]);
                object (v)->setProperty ("groups", juce::var (g));
                values.add (v);
            }
        auto groupObject = copyKeys (type["groups"], { "name" });
        object (groupObject)->setProperty ("list", juce::var (zones));
        object (type)->setProperty ("groups", groupObject);
        auto chart = copyKeys (type["chart"], { "caption" });
        object (chart)->setProperty ("rows", juce::var (rows));
        object (chart)->setProperty ("columns", juce::var (columns));
        object (type)->setProperty ("chart", chart);
        setList (type, "values", values);
    }

    // The structuring language (operations, for stage 8).
    auto rk1 = builtinJson ("rk1");
    if (root.hasProperty ("operations")) object (rk1)->setProperty ("operations", root["operations"]);

    for (auto [id, v] : { std::pair<const char*, juce::var> { "composition", composition }, { "schaeffer", schaeffer }, { "rk1", rk1 } })
    {
        if (object (v) == nullptr) { error = "Projet's own " + juce::String (id) + " package is missing."; return false; }
        ids.add (id);
        jsons.add (juce::JSON::toString (v, false));
    }
    // And the packages 4a did not have (Thoresen, Vande Gorne), as Projet ships them.
    for (auto& b : builtins())
        if (! ids.contains (b.id)) { ids.add (b.id); jsons.add (b.json); }
    return true;
}

//== Along =====================================================================

namespace Along
{
    juce::Array<Change> parse (const juce::String& text)
    {
        juce::Array<Change> out;
        for (auto item : juce::StringArray::fromTokens (text, ";", {}))
        {
            auto parts = juce::StringArray::fromTokens (item.trim(), ":", {});
            if (parts.size() < 2) continue;
            Change c;
            c.at = juce::jlimit (0.0, 1.0, parts[0].trim().getDoubleValue());
            c.to = parts[1].trim();
            c.width = parts.size() > 2 ? juce::jlimit (0.0, 1.0, parts[2].trim().getDoubleValue()) : 0.0;
            out.add (c);
        }
        std::stable_sort (out.begin(), out.end(), [] (const Change& a, const Change& b) { return a.at < b.at; });
        return out;
    }

    juce::String format (const juce::Array<Change>& changes)
    {
        auto sorted = changes;
        std::stable_sort (sorted.begin(), sorted.end(), [] (const Change& a, const Change& b) { return a.at < b.at; });
        juce::StringArray items;
        for (auto& c : sorted)
            items.add (trimNumber (c.at) + ":" + c.to + ":" + trimNumber (c.width));
        return items.joinIntoString (";");
    }

    juce::String valueAt (const juce::String& base, const juce::Array<Change>& changes, double p)
    {
        auto v = base;
        for (auto& c : changes)
            if (c.at <= p + 1.0e-9) v = c.to;
        return v;
    }

    void section (const juce::String& base, const juce::Array<Change>& changes, double from, double to,
                  juce::String& newBase, juce::Array<Change>& newChanges)
    {
        newChanges.clear();
        newBase = valueAt (base, changes, from);
        const double len = to - from;
        if (len <= 1.0e-9) return;
        for (auto& c : changes)
            if (c.at > from + 1.0e-9 && c.at < to - 1.0e-9)
                newChanges.add ({ (c.at - from) / len, c.to, juce::jlimit (0.0, 1.0, c.width / len) });
        // A change exactly at the cut belongs to the right-hand part: it is its base.
    }
}
