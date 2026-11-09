// Glyphs.h — the energy glyphs, drawn by JavaScript (stage 4b.2).
//
// A framework package can draw the shapes of its values: a glyphs.js next to
// its framework.json defines one object, glyphs, whose entries are functions
// (w, h, rand) returning strokes in a w by h band (see builtinSource() for
// the format — it is written into the composition package, so users can read
// and copy it). Projet appends a small runtime that applies the box's
// profile (the intensity envelope) and seeds the random generator, then
// reads the strokes back.
//
// Scripts run in JUCE's JavaScript engine (QuickJS) with nothing registered:
// no files, no network — they can only compute shapes. A script that runs
// too long is stopped (maximumExecutionTime). Everything here is for the
// message thread.

#pragma once

#include <JuceHeader.h>
#include "Framework.h"

namespace Glyphs
{
    /** One stroke of a glyph: a polyline (x,y pairs) or circles (x,y,r). */
    struct Stroke
    {
        juce::Array<float> points;      // x,y pairs: a line through them
        juce::Array<float> dots;        // x,y,r triples: filled circles
        bool closed = false, filled = false;
        float alpha = 1.0f, width = 1.0f;
    };

    /** The runtime appended to every glyphs.js (ids, envelope, seeded rand). */
    juce::String runtime();
    /** Projet's own glyphs: the ten energy models, revised 9 November. */
    juce::String builtinSource();

    //==========================================================================
    /** One glyphs.js, loaded into its own engine. */
    class Script
    {
    public:
        /** Compiles the script (plus the runtime) and reads which glyph ids it
            defines. On failure the script is unusable and the result says why
            (prefixed with `name`). */
        juce::Result load (const juce::String& source, const juce::String& name);

        const juce::StringArray& ids() const noexcept   { return glyphIds; }
        bool has (const juce::String& id) const         { return glyphIds.contains (id); }

        /** Runs one glyph in a w by h band. `envPairs` is the profile envelope
            as t,value pairs (empty or all 1 = flat); `seed` varies the random
            shapes (grain clouds) deterministically. */
        juce::Result run (const juce::String& id, float w, float h,
                          const juce::Array<float>& envPairs, int seed,
                          juce::Array<Stroke>& out) const;

    private:
        mutable std::unique_ptr<juce::JavascriptEngine> engine;
        juce::StringArray glyphIds;
    };

    //==========================================================================
    /** The glyph scripts of a framework's packages, with Projet's own as the
        fallback, so a package without a glyphs.js still gets the built-in
        shapes its values name. */
    class Set
    {
    public:
        void rebuild (const Framework&);

        struct Found
        {
            const Script* script = nullptr;
            juce::String owner;             // the package id, or "projet"
        };
        /** The script that draws this glyph: the preferred packages first (the
            value's own package, then its category's), then any package, then
            the built-in shapes. */
        Found find (const juce::String& glyphId, const juce::StringArray& preferPackages) const;

        /** Scripts that could not be loaded, one line each ("<package>: why"). */
        const juce::StringArray& getProblems() const noexcept   { return problems; }

    private:
        juce::OwnedArray<Script> scripts;
        juce::StringArray packageIds;
        std::unique_ptr<Script> builtin;
        juce::StringArray problems;
    };
}
