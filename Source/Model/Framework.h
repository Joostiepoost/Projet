// Framework.h — the frameworks a project uses: packages of categories (stage 4b).
//
// A package is a folder with a framework.json (and a help.md): its categories,
// each with a kind (one choice from a list, several tags, a number, a text),
// its values, groups of values (Vande Gorne's families, Schaeffer's zones), a
// chart for values that sit in rows and columns (Schaeffer's typology), and
// the mark it gets on the boxes by default (a strip, a badge, a label). A
// category can change along a box, be given by folders to the boxes inside,
// have a default value, share the values of another category, and give its
// values two names (Thoresen's and Vande Gorne's). A package can also add
// values to another package's category, so your additions survive an update
// of the original.
//
// Projet ships some packages (builtins), keeps a library of them (Application
// Support/Projet/Frameworks) and each project keeps copies of the ones it uses
// (<project>/Frameworks), so a piece stays readable whatever happens to the
// library. Nothing about the categories is hard-coded: the inspector, the
// menus, the colour modes, the highlight and the marks list whatever the
// loaded packages declare.
//
// Boxes store ids, never names, under fw.<package>.<category>; changes along a
// box under fw.<package>.<category>.along. An id no package knows is kept and
// shown as unknown ("id?").

#pragma once

#include <JuceHeader.h>

class Framework
{
public:
    struct Name
    {
        juce::String fr, en;
        bool isEmpty() const                    { return fr.isEmpty() && en.isEmpty(); }
    };

    /** Colours as 0xAARRGGBB (0 = none): the model has no graphics module. */
    struct Colours { juce::uint32 colour = 0, dark = 0, light = 0; };

    struct Group
    {
        juce::String id;
        Name name, about;
        Colours colours;
    };

    /** "Only with": a category (or value) that applies only while another
        category has one of these values (mixing relations only on overlaps). */
    struct Condition
    {
        juce::String category;                  // its key, after loading
        juce::StringArray values;
        bool isSet() const noexcept             { return category.isNotEmpty(); }
    };

    struct Value
    {
        juce::String id, label, glyph, fill;
        juce::String from;                      // the package that defined it (another one for additions)
        Name name, name2, about;
        juce::StringArray groups;
        Colours colours;                        // its own (else its first group's)
        int order = 0, weight = 0, row = -1, col = -1;
        juce::StringArray detailsFr, detailsEn;
        juce::Array<float> env;                 // intensity envelope, flat (t, a) pairs, t ascending
        int peak = -1;                          // the envelope point a peak position moves (an index of env's pairs)
        juce::String curve;                     // a peak shape: "soft" (rounded) or "hard"
        juce::String sign;                      // an SVG path drawn for it on the timeline (a seam or an end sign)
        juce::String seamSign;                  // an end value's mark on the end bracket of a seam sign, from (0, 0)
        float seamFade = 0.0f;                  // and how much of the line before that bracket turns to dashes
        float endX = -1.0f, endY = -1.0f;       // a seam sign's end bracket of the box before (where seamSign goes)
        float fadeX = -1.0f;                    // where a fading ending stops on its line, if before endX
        Condition onlyWith;
        juce::String impliesCategory, impliesValue;     // (4d) a folder value that gives its boxes a value of
                                                        // another category (a writing type's mixing relation)
        juce::String shape;                     // (4d.2) a span kind's shape in a drawn lane: forward, backward,
                                                // presence, neutral, delta
        juce::String stroke;                    // (4d.2) an emphasis: dashed, solid or bold
        float size = -1.0f;                     // a height's share of the row (low 0.6, high 1)
        float early = -1.0f;                    // a goal's: the main slope arrives this early (a blunted goal)
        juce::String signFill;                  // a filled part of the sign (an SVG path, beside the stroked `sign`)
    };

    struct Axis { juce::String id; Name name; };

    enum class Kind { choice, tags, number, text };
    enum class Mark { none, strip, badge, label, glyph, seam, end };

    /** A rule finding a join's value from the timing: the first that fits.
        The gap is in seconds, negative for an overlap. */
    struct Rule
    {
        juce::String value;
        double from = -1.0e300, to = 1.0e300;   // from <= gap < to
        int covered = -1;                       // -1 either way, 0 not covered, 1 covered by a box on another track
    };

    /** A remark on a range of gaps (Vande Gorne: between 20 and 50 ms the attack blurs). */
    struct Note
    {
        double from = 0.0, to = 0.0;
        Name text;
    };

    struct Category
    {
        juce::String id, package;
        /** "composition.level": how menus, settings and other packages refer to it. */
        juce::String key() const                { return package + "." + id; }
        /** The box (or folder) property: fw.composition.level */
        juce::Identifier property() const       { return "fw." + key(); }
        /** Changes along a box: fw.composition.level.along */
        juce::Identifier alongProperty() const  { return "fw." + key() + ".along"; }

        Name name, about, groupsName, detailsLabel, chartCaption;
        Name firstNames, secondNames;           // what the two names are (e.g. Thoresen, Vande Gorne)
        Kind kind = Kind::choice;
        juce::String control;                   // choice: "menu", "buttons" or "chart"
        bool along = false, folders = false, showDefault = true;
        juce::String defaultValue;
        juce::String valuesFrom;                // the key of the category whose values (and groups) it uses
        Mark mark = Mark::none;                 // the package's choice; the project's display settings can change it
        juce::uint32 colour = 0;                // for categories whose values have no colours
        double minimum = 0.0, maximum = 1.0, step = 0.0;
        juce::String unit;
        juce::Array<Group> groups;
        juce::Array<Value> values;
        juce::Array<Axis> rows, columns;        // chart
        bool hasTwoNames = false;               // some value has a second name
        int index = 0;                          // position in getCategories()

        // Stage 4c
        bool between = false;                   // the value belongs to the join with the box before (stored on the later box)
        bool atEnd = false;                     // the value describes how the box ends
        juce::Array<Rule> derive;               // a join's value from the timing, when the box has none of its own
        juce::Array<Note> notes;                // remarks on ranges of gaps
        Condition onlyWith;
        juce::String envelope;                  // "peak": moves the peak of the box's profile (a number, % of the box);
                                                // "shape": its values' curve rounds the profile (a choice)
        // Stage 4d
        bool foldersOnly = false;               // set on folders only (writing type, time-feel); boxes never have it
        bool laneOnly = false;                  // describes what is drawn in a lane (4d.2: the dynamic form)
        /** Something boxes can carry (not a folder's or a lane's only). */
        bool forBoxes() const noexcept          { return ! foldersOnly && ! laneOnly; }
    };

    /** A value one category gives another in the boxes of a folder: a
        folder whose writing type is "weaving" gives its overlaps fusion. */
    struct Implication
    {
        int source = -1;                        // the category set on the folder (an index of getCategories())
        juce::String value, implied;
    };

    /** An overview lane (stage 4d), a kind of ruler layer: "rows" draws one
        row per group (or value) of a category wherever boxes have it (the
        energies by family, the typology by zone); "scale" draws the range of
        an ordered category present at each moment and a line for its
        average, weighted by another category (the semiotic profile, weighted
        by role). */
    struct Lane
    {
        juce::String id, package;
        juce::String key() const                { return package + "." + id; }
        Name name, about;
        enum class Kind { rows, scale, drawn } kind = Kind::rows;
        juce::String category, second;          // keys; second: drawn lighter (the second energy)
        bool byGroups = true;                   // rows: one per group, else one per value
        juce::String weightCategory;            // scale: the category weighing each box
        juce::StringArray weightIds;
        juce::Array<double> weightValues;
        double defaultWeight = 1.0;
        // drawn (4d.2): what you draw in it, as categories "on": "lane" (keys)
        juce::String spans, emphasis, goal, points, height;
        juce::StringArray goalOn;               // the span kinds (ids) that can reach a goal
        juce::String pointBase;                 // the filled sign under every point (Thoresen's weighted point, a triangle)
        double weightFor (const juce::String& valueId) const
        {
            const int i = weightIds.indexOf (valueId);
            return i >= 0 ? weightValues[i] : defaultWeight;
        }
    };

    struct Package
    {
        juce::String id;
        int version = 0, order = 50;
        Name name, about;
        juce::String status;
        juce::StringArray categories;           // keys, in order
        juce::var json;                         // the whole file (operations and the like, for later stages)
        juce::String script;                    // its glyphs.js, if it has one
        juce::StringArray warnings;             // about this package, from the last rebuild
    };

    /** No packages (Builtin() for the ones Projet ships). */
    Framework() = default;

    //== Packages ==============================================================
    /** Adds (or replaces) a package from its framework.json. `folderName` is
        used when the file has no id. On failure (error set) nothing changes.
        `script` is the package's glyphs.js, if it has one. */
    bool addPackage (const juce::String& json, const juce::String& folderName, juce::String& error,
                     const juce::String& script = {});
    void removePackage (const juce::String& id);
    void clear();
    bool isEmpty() const                        { return packages.isEmpty(); }

    /** In order (their "order", then id). */
    const juce::Array<Package>& getPackages() const     { return packages; }
    const Package* findPackage (const juce::String& id) const;

    //== Categories ============================================================
    /** All categories of all packages, package by package, in order. */
    const juce::Array<Category>& getCategories() const  { return categories; }
    const Category* findCategory (const juce::String& key) const;
    /** The category whose values and groups `c` uses (itself, or its valuesFrom). */
    const Category& source (const Category& c) const;
    const juce::Array<Value>& valuesOf (const Category& c) const        { return source (c).values; }
    const juce::Array<Group>& groupsOf (const Category& c) const        { return source (c).groups; }
    const Value* findValue (const Category&, const juce::String& id) const;
    const Group* findGroup (const Category&, const juce::String& id) const;
    /** Categories that share `c`'s values (c's source and every category using it). */
    juce::Array<const Category*> sharing (const Category& c) const;
    /** What folder values give the boxes for category `c` (stage 4d). */
    const juce::Array<Implication>& implicationsFor (const Category& c) const;

    /** Nodes on folders (stage 4d.2, Vande Gorne): flagged when shorter or
        longer than these parts of the stretch before them. From the first
        package (in order) that declares "nodes". */
    struct NodeRules
    {
        bool present = false;
        juce::String package;
        Name name, about;
        double shortRatio = 0.02, longRatio = 0.25;
        double window = 0.05;                   // a track changes "within" a node up to this far outside it (s)
    };
    const NodeRules& getNodeRules() const               { return nodeRules; }

    //== Lanes (stage 4d) ======================================================
    const juce::Array<Lane>& getLanes() const           { return lanes; }
    /** Goes up whenever the packages change (for caches). */
    int getGeneration() const noexcept                  { return generation; }
    const Lane* findLane (const juce::String& key) const;

    //== Names =================================================================
    /** "fr" or "en"; names fall back to the other language when missing. */
    void setLanguage (const juce::String& lang)         { language = lang == "en" ? "en" : "fr"; }
    juce::String getLanguage() const                    { return language; }
    juce::String text (const Name&) const;
    /** For categories with two names per value: "both", "first" or "second". */
    void setNaming (const juce::String& categoryKey, const juce::String& naming);
    juce::String getNaming (const juce::String& categoryKey) const;
    /** All namings as "key=first;key=second" (for the project file). */
    juce::String namingsToString() const;
    void namingsFromString (const juce::String&);

    /** The name of a value, as the naming asks ("" for "", "id?" when unknown). */
    juce::String valueName (const Category&, const juce::String& id) const;
    /** Short: the value's label (F, N″), else its name. */
    juce::String valueLabel (const Category&, const juce::String& id) const;
    /** The details of a value (variation factors, traits), in the language. */
    juce::StringArray valueDetails (const Value&) const;
    /** A value's colours: its own, else its first group's, else none (0). */
    Colours valueColours (const Category&, const juce::String& id) const;
    /** Whether any value (or group) of the category has a colour. */
    bool hasColours (const Category&) const;
    /** Names for a number ("0.5 s", "6") with the category's step. */
    juce::String formatNumber (const Category&, double) const;

    /** "key=value;key=value" (display settings, namings) and back. */
    static juce::StringPairArray parsePairs (const juce::String&);
    static juce::String formatPairs (const juce::StringPairArray&);

    static juce::String kindName (Kind);
    static juce::String markName (Mark);
    static Mark markFromName (const juce::String&, Mark fallback = Mark::none);
    /** The marks that suit a category (none first). */
    static juce::Array<Mark> marksFor (const Category&);
    /** The mark a category gets: the project's display setting for it
        ("key=strip;..."), if it suits the category, else the package's. */
    static Mark chosenMark (const Category&, const juce::StringPairArray& display);

    //== Files =================================================================
    /** ~/Library/Application Support/Projet (macOS). */
    static juce::File appFolder();
    /** The library: appFolder()/Frameworks. */
    static juce::File libraryFolder();
    /** For the tests: another library folder (an empty File = the real one). */
    static void setLibraryFolder (const juce::File&);
    /** A project's packages: <project>/Frameworks. */
    static juce::File projectFolder (const juce::File& project) { return project.getChildFile ("Frameworks"); }
    /** The package folders (with a framework.json) inside `parent`. */
    static juce::Array<juce::File> packageFolders (const juce::File& parent);
    /** The version a package folder holds (0 if none or unreadable). */
    static int folderVersion (const juce::File& packageFolder);

    struct Report { juce::StringArray errors, notes; };
    /** Replaces everything with the packages inside `parent`; packages that
        cannot be read are reported and left out. */
    void loadFolder (const juce::File& parent, Report&);

    //== The packages Projet ships =============================================
    struct Builtin { juce::String id, json, help, glyphs; int version = 0; };
    static const juce::Array<Builtin>& builtins();
    /** All builtin packages, loaded. */
    static Framework builtIn();
    static juce::Result writePackage (const juce::File& packageFolder, const juce::String& json, const juce::String& help,
                                      const juce::String& script = {});

    /** A profile's envelope with its peak moved to `peakAt` (0..1; negative
        keeps it) and, if `soft`, rounded (resampled along cosine curves). */
    static juce::Array<float> shapeEnvelope (const juce::Array<float>& env, int peakIndex, double peakAt, bool soft);
    static juce::Result writeBuiltin (const juce::File& parent, const juce::String& id);
    /** Copies a package folder into `parent` (replacing the one there). */
    static juce::Result copyPackage (const juce::File& packageFolder, const juce::File& parent);

    /** Makes the library the first time: from the 4a framework.json of the
        app if there is one (kept as framework-4a.json), else the builtins.
        `language` and `naming` get the 4a file's settings, if any. */
    static juce::Result ensureLibrary (juce::String& language, juce::String& naming, Report&);
    /** Gives a project its Frameworks folder the first time: from its 4a
        framework.json (kept as framework-4a.json), else copies of the library. */
    static juce::Result ensureProject (const juce::File& project, juce::String& language, juce::String& naming, Report&);

    //== Stage 4a ==============================================================
    /** The framework.json Projet 4a shipped. */
    static juce::String legacyDefaultJson();
    /** Turns a 4a framework.json into packages (id, json): the builtins when it
        is the 4a default, else packages carrying its content. `language` and
        `naming` (the role naming) get its settings. */
    static bool convertLegacy (const juce::String& json, juce::StringArray& ids, juce::StringArray& jsons,
                               juce::String& language, juce::String& naming, juce::String& error);
    /** Old box and folder fields (energy, level, typology ...) and their new keys. */
    static juce::StringPairArray legacyFields();

private:
    struct Raw { Package package; juce::Array<Category> categories; juce::var additions; juce::Array<Lane> lanes; NodeRules nodes; };
    juce::Array<Raw> raws;
    juce::Array<Package> packages;
    juce::Array<Category> categories;
    juce::Array<Lane> lanes;
    juce::Array<juce::Array<Implication>> implications;   // by category index
    int generation = 0;
    NodeRules nodeRules;
    juce::String language { "fr" };
    juce::StringPairArray namings;
    void rebuild();
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

//== Tags (several values in one field) ========================================
namespace Tags
{
    inline juce::StringArray parse (const juce::String& s)
    {
        juce::StringArray out;
        for (auto t : juce::StringArray::fromTokens (s, ",", {}))
            if (t.trim().isNotEmpty()) out.addIfNotAlreadyThere (t.trim());
        return out;
    }
    inline juce::String format (const juce::StringArray& a) { return a.joinIntoString (","); }
}
