// Framework.h — the composition framework, read from framework.json (stage 4a).
//
// Energies and their families (Vande Gorne), semiotic levels (Peirce / Bayle,
// with Thoresen's metonymic level), roles (Thoresen's layer functions, or
// Vande Gorne's figure and ground), Schaeffer's typology, and the operations
// of the structuring language (kept for stage 8). Nothing of it is hard-coded:
// the app keeps one file, each project a copy of it (see ensureProjectFile).
// Boxes store ids, never names, so renaming in the file breaks nothing; an id
// the file does not know is kept and shown as unknown.
//
// Changes along a box (Along): a semiotic level or a role can change at
// proportions of the box, suddenly or over a width. Stored on the box as text,
// e.g. "0.7:meson:0.08;0.9:ison:0" (at : value : width).

#pragma once

#include <JuceHeader.h>

class Framework
{
public:
    struct Name   { juce::String fr, en; };
    struct Family { juce::String id; Name name; juce::uint32 colour = 0, dark = 0, light = 0; };   // colours: 0xAARRGGBB
    struct Energy { juce::String id, glyph; Name name; juce::StringArray families, factorsFr, factorsEn; };
    struct Level  { juce::String id, fill; Name name, about; int order = 0; juce::uint32 colour = 0; };
    struct Role   { juce::String id, letter; Name thoresen, vandeGorne; int weight = 0; bool isDefault = false; };
    struct Zone   { juce::String id; Name name; juce::uint32 colour = 0, dark = 0, light = 0; };
    struct Type   { juce::String id, label, zone; Name name; int row = 0, col = 0; };
    struct Axis   { juce::String id; Name name; };

    /** The built-in default. */
    Framework();

    /** Replaces everything from a framework.json text; on failure (error set)
        nothing changes. */
    bool load (const juce::String& json, juce::String& error);
    /** The default framework.json text. */
    static juce::String defaultJson();

    int getVersion() const                                  { return version; }
    juce::String getLanguage() const                        { return language; }
    /** "thoresen", "vandegorne" or "both". */
    juce::String getRoleNames() const                       { return roleNames; }
    /** A name in the file's language (or the other one if missing). */
    juce::String text (const Name&) const;

    const juce::Array<Family>& getFamilies() const          { return families; }
    const juce::Array<Energy>& getEnergies() const          { return energies; }
    const juce::Array<Level>& getLevels() const             { return levels; }     // concrete to abstract
    const juce::Array<Role>& getRoles() const               { return roles; }      // strongest first
    const juce::Array<Zone>& getZones() const               { return zones; }
    const juce::Array<Type>& getTypes() const               { return types; }
    const juce::Array<Axis>& getMasses() const              { return masses; }     // typology rows
    const juce::Array<Axis>& getSustainments() const        { return sustainments; } // typology columns
    juce::StringArray figureTraits (bool figure) const;
    const juce::var& getOperations() const                  { return operations; }

    const Family* findFamily (const juce::String& id) const;
    const Energy* findEnergy (const juce::String& id) const;
    const Level*  findLevel (const juce::String& id) const;
    const Role*   findRole (const juce::String& id) const;
    const Zone*   findZone (const juce::String& id) const;
    const Type*   findType (const juce::String& id) const;

    /** Names for display; unknown ids come back as "id?" and empty ids as "". */
    juce::String energyName (const juce::String& id) const;
    juce::String familyName (const juce::String& id) const;
    juce::String levelName (const juce::String& id) const;
    /** "F", "F · figure" or "figure", as roleNames asks; short = letter only (for badges). */
    juce::String roleName (const juce::String& id, bool shortForm = false) const;
    juce::String typeLabel (const juce::String& id) const;
    juce::String typeName (const juce::String& id) const;
    /** The role used when a box has none. */
    juce::String defaultRole() const;

    /** Colours for the colour modes (0xAARRGGBB; 0 when the id is empty or
        unknown). The model has no graphics: the UI makes juce::Colours of them. */
    juce::uint32 energyColour (const juce::String& energyId) const;   // its first family
    juce::uint32 levelColour (const juce::String& levelId) const;
    juce::uint32 typeColour (const juce::String& typeId) const;       // its zone

    //== Files ================================================================
    /** ~/Library/Application Support/Projet (macOS). */
    static juce::File appFolder();
    static juce::File appFile()                             { return appFolder().getChildFile ("framework.json"); }
    /** Writes the default to the app file if there is none yet. */
    static juce::Result ensureAppFile();
    static juce::File projectFile (const juce::File& projectFolder) { return projectFolder.getChildFile ("framework.json"); }
    /** The project's copy, made from the app file (or the default) if missing. */
    static juce::Result ensureProjectFile (const juce::File& projectFolder);
    /** Replaces the project's copy with the app file. */
    static juce::Result copyAppToProject (const juce::File& projectFolder);
    /** Makes the project's copy the app file (for new projects). */
    static juce::Result copyProjectToApp (const juce::File& projectFolder);

private:
    int version = 1;
    juce::String language { "fr" }, roleNames { "both" };
    juce::Array<Family> families;
    juce::Array<Energy> energies;
    juce::Array<Level> levels;
    juce::Array<Role> roles;
    juce::Array<Zone> zones;
    juce::Array<Type> types;
    juce::Array<Axis> masses, sustainments;
    juce::StringArray figureFr, figureEn, groundFr, groundEn;
    juce::var operations;
};

//== Changes along a box =======================================================
namespace Along
{
    struct Change
    {
        double at = 0.0;        // proportion of the box, 0..1 (the middle of the change)
        juce::String to;        // the value from there on
        double width = 0.0;     // 0 = sudden; else the length of the gradual change
    };

    juce::Array<Change> parse (const juce::String&);
    juce::String format (const juce::Array<Change>&);
    /** The value in effect at proportion p. */
    juce::String valueAt (const juce::String& base, const juce::Array<Change>&, double p);
    /** The part [from, to] of a box as a box of its own: its base is the value
        at `from`, and its changes are those inside, rescaled. */
    void section (const juce::String& base, const juce::Array<Change>& changes, double from, double to,
                  juce::String& newBase, juce::Array<Change>& newChanges);
}
