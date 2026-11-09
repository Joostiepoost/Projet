// Framework.cpp

#include "Framework.h"

namespace
{
    Framework::Name readName (const juce::var& v)
    {
        if (v.isString()) return { v.toString(), v.toString() };
        return { v["fr"].toString(), v["en"].toString() };
    }

    juce::uint32 readColour (const juce::var& v, juce::uint32 fallback = 0xff808080u)
    {
        auto s = v.toString().trim().trimCharactersAtStart ("#");
        if (s.length() == 6) return 0xff000000u | (juce::uint32) s.getHexValue32();
        if (s.length() == 8) return (juce::uint32) s.getHexValue32();
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

    juce::StringArray readStrings (const juce::var& v)
    {
        juce::StringArray out;
        if (auto* a = v.getArray())
            for (auto& item : *a) out.add (item.toString());
        return out;
    }

    juce::String formatNumber (double v)
    {
        auto s = juce::String (v, 4);
        if (s.containsChar ('.')) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s.isEmpty() ? juce::String ("0") : s;
    }
}

Framework::Framework()
{
    juce::String error;
    load (defaultJson(), error);
    jassert (error.isEmpty());
}

bool Framework::load (const juce::String& json, juce::String& error)
{
    juce::var root;
    const auto parsed = juce::JSON::parse (json, root);
    if (parsed.failed()) { error = "The framework file is not valid JSON: " + parsed.getErrorMessage(); return false; }
    if (root.getDynamicObject() == nullptr) { error = "The framework file does not hold a framework."; return false; }

    // Read into new lists first, so a file that fails changes nothing.
    juce::Array<Family> newFamilies;
    if (auto* a = root["families"].getArray())
        for (auto& v : *a)
        {
            Family x;
            x.id = v["id"].toString();
            x.name = readName (v["name"]);
            x.colour = readColour (v["colour"]);
            x.dark = readColour (v["dark"], mix (x.colour, 0xff000000u, 0.6));
            x.light = readColour (v["light"], mix (x.colour, 0xffffffffu, 0.4));
            if (x.id.isNotEmpty()) newFamilies.add (x);
        }

    juce::Array<Energy> newEnergies;
    if (auto* a = root["energies"].getArray())
        for (auto& v : *a)
        {
            Energy x;
            x.id = v["id"].toString();
            x.glyph = v.hasProperty ("glyph") ? v["glyph"].toString() : x.id;
            x.name = readName (v["name"]);
            x.families = readStrings (v["families"]);
            x.factorsFr = readStrings (v["factors"]["fr"]);
            x.factorsEn = readStrings (v["factors"]["en"]);
            if (x.id.isNotEmpty()) newEnergies.add (x);
        }

    juce::Array<Level> newLevels;
    if (auto* a = root["levels"].getArray())
        for (auto& v : *a)
        {
            Level x;
            x.id = v["id"].toString();
            x.order = (int) v.getProperty ("order", newLevels.size() + 1);
            x.fill = v["fill"].toString();
            x.colour = readColour (v["colour"]);
            x.name = readName (v["name"]);
            x.about = readName (v["about"]);
            if (x.id.isNotEmpty()) newLevels.add (x);
        }
    std::stable_sort (newLevels.begin(), newLevels.end(), [] (const Level& a, const Level& b) { return a.order < b.order; });

    juce::Array<Role> newRoles;
    if (auto* a = root["roles"].getArray())
        for (auto& v : *a)
        {
            Role x;
            x.id = v["id"].toString();
            x.letter = v.hasProperty ("letter") ? v["letter"].toString() : x.id;
            x.weight = (int) v.getProperty ("weight", 0);
            x.isDefault = (bool) v.getProperty ("default", false);
            x.thoresen = readName (v["thoresen"]);
            x.vandeGorne = readName (v["vandeGorne"]);
            if (x.id.isNotEmpty()) newRoles.add (x);
        }
    std::stable_sort (newRoles.begin(), newRoles.end(), [] (const Role& a, const Role& b) { return a.weight > b.weight; });

    const auto& typology = root["typology"];
    juce::Array<Axis> newMasses, newSustainments;
    for (auto [key, target] : { std::pair<const char*, juce::Array<Axis>*> { "masses", &newMasses }, { "sustainments", &newSustainments } })
        if (auto* a = typology[key].getArray())
            for (auto& v : *a)
                target->add ({ v["id"].toString(), readName (v["name"]) });

    juce::Array<Zone> newZones;
    if (auto* a = typology["zones"].getArray())
        for (auto& v : *a)
        {
            Zone x;
            x.id = v["id"].toString();
            x.name = readName (v["name"]);
            x.colour = readColour (v["colour"]);
            x.dark = readColour (v["dark"], mix (x.colour, 0xff000000u, 0.6));
            x.light = readColour (v["light"], mix (x.colour, 0xffffffffu, 0.4));
            if (x.id.isNotEmpty()) newZones.add (x);
        }

    juce::Array<Type> newTypes;
    if (auto* a = typology["types"].getArray())
        for (auto& v : *a)
        {
            Type x;
            x.id = v["id"].toString();
            x.label = v.hasProperty ("label") ? v["label"].toString() : x.id;
            x.zone = v["zone"].toString();
            x.row = (int) v.getProperty ("row", 0);
            x.col = (int) v.getProperty ("col", 0);
            x.name = readName (v["name"]);
            if (x.id.isNotEmpty()) newTypes.add (x);
        }

    if (newEnergies.isEmpty() && newLevels.isEmpty() && newTypes.isEmpty())
    {
        error = "The framework file has no energies, levels or types.";
        return false;
    }

    version = (int) root.getProperty ("version", 1);
    language = root.getProperty ("language", "fr").toString().toLowerCase() == "en" ? "en" : "fr";
    roleNames = root.getProperty ("roleNames", "both").toString().toLowerCase().removeCharacters (" -_");
    if (roleNames != "thoresen" && roleNames != "vandegorne") roleNames = "both";
    families = newFamilies;
    energies = newEnergies;
    levels = newLevels;
    roles = newRoles;
    zones = newZones;
    types = newTypes;
    masses = newMasses;
    sustainments = newSustainments;
    figureFr = readStrings (root["figureGround"]["figure"]["fr"]);
    figureEn = readStrings (root["figureGround"]["figure"]["en"]);
    groundFr = readStrings (root["figureGround"]["ground"]["fr"]);
    groundEn = readStrings (root["figureGround"]["ground"]["en"]);
    operations = root["operations"];
    error = {};
    return true;
}

juce::String Framework::text (const Name& n) const
{
    if (language == "en") return n.en.isNotEmpty() ? n.en : n.fr;
    return n.fr.isNotEmpty() ? n.fr : n.en;
}

juce::StringArray Framework::figureTraits (bool figure) const
{
    if (language == "en") return figure ? figureEn : groundEn;
    return figure ? figureFr : groundFr;
}

#define PROJET_FIND(Type, list) \
    for (auto& x : list) if (x.id == id) return &x; \
    return nullptr;

const Framework::Family* Framework::findFamily (const juce::String& id) const { PROJET_FIND (Family, families) }
const Framework::Energy* Framework::findEnergy (const juce::String& id) const { PROJET_FIND (Energy, energies) }
const Framework::Level*  Framework::findLevel (const juce::String& id) const  { PROJET_FIND (Level, levels) }
const Framework::Role*   Framework::findRole (const juce::String& id) const   { PROJET_FIND (Role, roles) }
const Framework::Zone*   Framework::findZone (const juce::String& id) const   { PROJET_FIND (Zone, zones) }
const Framework::Type*   Framework::findType (const juce::String& id) const   { PROJET_FIND (Type, types) }

#undef PROJET_FIND

juce::String Framework::energyName (const juce::String& id) const
{
    if (id.isEmpty()) return {};
    if (auto* e = findEnergy (id)) return text (e->name);
    return id + "?";
}

juce::String Framework::familyName (const juce::String& id) const
{
    if (id.isEmpty()) return {};
    if (auto* f = findFamily (id)) return text (f->name);
    return id + "?";
}

juce::String Framework::levelName (const juce::String& id) const
{
    if (id.isEmpty()) return {};
    if (auto* l = findLevel (id)) return text (l->name);
    return id + "?";
}

juce::String Framework::roleName (const juce::String& id, bool shortForm) const
{
    if (id.isEmpty()) return {};
    auto* r = findRole (id);
    if (r == nullptr) return id + "?";
    if (roleNames == "vandegorne")
    {
        const auto n = text (r->vandeGorne);
        return shortForm ? n.substring (0, 3) : n;
    }
    if (shortForm || roleNames == "thoresen") return shortForm ? r->letter : r->letter + " " + text (r->thoresen);
    // both: the letter, Thoresen's name and Vande Gorne's
    return r->letter + " " + text (r->thoresen) + " / " + text (r->vandeGorne);
}

juce::String Framework::typeLabel (const juce::String& id) const
{
    if (id.isEmpty()) return {};
    if (auto* t = findType (id)) return t->label;
    return id + "?";
}

juce::String Framework::typeName (const juce::String& id) const
{
    if (id.isEmpty()) return {};
    if (auto* t = findType (id)) return text (t->name);
    return id + "?";
}

juce::String Framework::defaultRole() const
{
    for (auto& r : roles) if (r.isDefault) return r.id;
    return roles.size() > 1 ? roles[roles.size() / 2].id : (roles.isEmpty() ? juce::String() : roles[0].id);
}

juce::uint32 Framework::energyColour (const juce::String& energyId) const
{
    if (auto* e = findEnergy (energyId))
        if (e->families.size() > 0)
            if (auto* f = findFamily (e->families[0])) return f->colour;
    return 0;
}

juce::uint32 Framework::levelColour (const juce::String& levelId) const
{
    if (auto* l = findLevel (levelId)) return l->colour;
    return 0;
}

juce::uint32 Framework::typeColour (const juce::String& typeId) const
{
    if (auto* t = findType (typeId))
        if (auto* z = findZone (t->zone)) return z->colour;
    return 0;
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

juce::Result Framework::ensureAppFile()
{
    const auto file = appFile();
    if (file.existsAsFile()) return juce::Result::ok();
    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("Could not make " + file.getParentDirectory().getFullPathName());
    if (! file.replaceWithText (defaultJson()))
        return juce::Result::fail ("Could not write " + file.getFullPathName());
    return juce::Result::ok();
}

juce::Result Framework::ensureProjectFile (const juce::File& projectFolder)
{
    const auto file = projectFile (projectFolder);
    if (file.existsAsFile()) return juce::Result::ok();
    ensureAppFile();
    const auto app = appFile();
    const auto text = app.existsAsFile() ? app.loadFileAsString() : defaultJson();
    if (! file.replaceWithText (text))
        return juce::Result::fail ("Could not write " + file.getFullPathName());
    return juce::Result::ok();
}

juce::Result Framework::copyAppToProject (const juce::File& projectFolder)
{
    ensureAppFile();
    const auto app = appFile();
    if (! app.existsAsFile()) return juce::Result::fail ("There is no app framework file.");
    if (! projectFile (projectFolder).replaceWithText (app.loadFileAsString()))
        return juce::Result::fail ("Could not write " + projectFile (projectFolder).getFullPathName());
    return juce::Result::ok();
}

juce::Result Framework::copyProjectToApp (const juce::File& projectFolder)
{
    const auto file = projectFile (projectFolder);
    if (! file.existsAsFile()) return juce::Result::fail ("This project has no framework file.");
    if (! appFolder().createDirectory()) return juce::Result::fail ("Could not make " + appFolder().getFullPathName());
    if (! appFile().replaceWithText (file.loadFileAsString()))
        return juce::Result::fail ("Could not write " + appFile().getFullPathName());
    return juce::Result::ok();
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
            items.add (formatNumber (c.at) + ":" + c.to + ":" + formatNumber (c.width));
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
