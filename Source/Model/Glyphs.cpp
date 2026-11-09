// Glyphs.cpp

#include "Glyphs.h"

namespace Glyphs
{

juce::String runtime()
{
    return juce::String::fromUTF8 (R"JS(
// —— Projet's glyph runtime, appended to every glyphs.js ——
function __projetGlyphIds()
{
    return (typeof glyphs === "object" && glyphs !== null) ? Object.keys (glyphs) : null;
}
function __projetGlyph (id, w, h, env, seed)
{
    var fn = glyphs[id];
    if (typeof fn !== "function") return null;
    var s = ((seed * 2654435761) + 104729) >>> 0;
    var rand = function() { s = ((s * 1664525) + 1013904223) >>> 0; return s / 4294967296; };
    var strokes = fn (w, h, rand) || [];

    // The profile: scale everything vertically around the band's centre by
    // the envelope at that point.
    var at = function (t)
    {
        if (! env || env.length < 2) return 1;
        if (t <= env[0][0]) return env[0][1];
        for (var i = 1; i < env.length; ++i)
            if (t <= env[i][0])
            {
                var a = env[i - 1], b = env[i], span = b[0] - a[0];
                return span <= 0 ? b[1] : a[1] + (b[1] - a[1]) * (t - a[0]) / span;
            }
        return env[env.length - 1][1];
    };
    var flat = true;
    if (env) for (var i = 0; i < env.length; ++i) if (Math.abs (env[i][1] - 1) > 1e-6) flat = false;
    if (flat) return strokes;

    var cy = h / 2;
    for (var k = 0; k < strokes.length; ++k)
    {
        var st = strokes[k];
        if (st.pts)
            for (var i = 0; i + 1 < st.pts.length; i += 2)
            {
                var e = at (st.pts[i] / w);
                st.pts[i + 1] = cy + (st.pts[i + 1] - cy) * e;
            }
        if (st.dots)
            for (var i = 0; i + 2 < st.dots.length; i += 3)
            {
                var e = at (st.dots[i] / w);
                st.dots[i + 1] = cy + (st.dots[i + 1] - cy) * e;
                st.dots[i + 2] = st.dots[i + 2] * (0.55 + 0.45 * e);
            }
    }
    return strokes;
}
)JS");
}

//== Script ====================================================================

juce::Result Script::load (const juce::String& source, const juce::String& name)
{
    glyphIds.clear();
    engine = std::make_unique<juce::JavascriptEngine>();
    engine->maximumExecutionTime = juce::RelativeTime::seconds (2.0);

    const auto compiled = engine->execute (source + "\n" + runtime());
    if (compiled.failed())
    {
        engine.reset();
        return juce::Result::fail (name + ": " + compiled.getErrorMessage().trim());
    }

    auto err = juce::Result::ok();
    const auto ids = engine->evaluate ("__projetGlyphIds()", &err);
    if (err.failed() || ids.getArray() == nullptr)
    {
        engine.reset();
        return juce::Result::fail (name + ": the script defines no glyphs object (var glyphs = { \"id\": function (w, h, rand) { ... } }).");
    }
    for (auto& id : *ids.getArray())
        if (id.toString().isNotEmpty())
            glyphIds.add (id.toString());

    engine->maximumExecutionTime = juce::RelativeTime::seconds (0.5);
    return juce::Result::ok();
}

namespace
{
    bool toFloats (const juce::var& v, juce::Array<float>& out, int groupSize)
    {
        auto* a = v.getArray();
        if (a == nullptr || a->size() % groupSize != 0) return false;
        out.ensureStorageAllocated (a->size());
        for (auto& x : *a)
        {
            if (! (x.isDouble() || x.isInt() || x.isInt64())) return false;
            const auto f = (float) (double) x;
            if (! std::isfinite (f)) return false;
            out.add (f);
        }
        return true;
    }
}

juce::Result Script::run (const juce::String& id, float w, float h,
                          const juce::Array<float>& envPairs, int seed,
                          juce::Array<Stroke>& out) const
{
    out.clear();
    if (engine == nullptr) return juce::Result::fail ("the script did not load");
    if (! has (id)) return juce::Result::fail ("no glyph " + id);

    juce::String env ("[");
    for (int i = 0; i + 1 < envPairs.size(); i += 2)
        env << (i > 0 ? "," : "") << "[" << juce::String (envPairs[i], 4) << "," << juce::String (envPairs[i + 1], 4) << "]";
    env << "]";

    const auto call = "__projetGlyph(" + juce::JSON::toString (juce::var (id), true) + ","
                      + juce::String (w, 2) + "," + juce::String (h, 2) + "," + env + "," + juce::String (seed) + ")";
    auto err = juce::Result::ok();
    const auto result = engine->evaluate (call, &err);
    if (err.failed()) return juce::Result::fail (id + ": " + err.getErrorMessage().trim());

    auto* list = result.getArray();
    if (list == nullptr) return juce::Result::fail (id + " returned no strokes");

    int floats = 0;
    for (auto& sv : *list)
    {
        Stroke s;
        if (auto* o = sv.getDynamicObject())
        {
            if (o->hasProperty ("pts") && ! toFloats (o->getProperty ("pts"), s.points, 2))
                return juce::Result::fail (id + ": pts should be x,y number pairs");
            if (o->hasProperty ("dots") && ! toFloats (o->getProperty ("dots"), s.dots, 3))
                return juce::Result::fail (id + ": dots should be x,y,r number triples");
            s.closed = (bool) o->getProperty ("close");
            s.filled = (bool) o->getProperty ("fill");
            if (o->hasProperty ("width")) s.width = juce::jlimit (0.25f, 6.0f, (float) (double) o->getProperty ("width"));
            if (o->hasProperty ("alpha")) s.alpha = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("alpha"));
            else if ((bool) o->getProperty ("faint")) s.alpha = 0.35f;
        }
        else if (sv.isArray())
        {
            if (! toFloats (sv, s.points, 2))
                return juce::Result::fail (id + ": a stroke should be x,y number pairs");
        }
        else continue;

        floats += s.points.size() + s.dots.size();
        if (floats > 60000) return juce::Result::fail (id + " returns too many points");
        if (s.points.size() >= 4 || s.dots.size() >= 3)
            out.add (std::move (s));
    }
    return juce::Result::ok();
}

//== Set =======================================================================

void Set::rebuild (const Framework& framework)
{
    scripts.clear();
    packageIds.clear();
    problems.clear();

    for (auto& p : framework.getPackages())
    {
        if (p.script.isEmpty()) continue;
        auto script = std::make_unique<Script>();
        const auto loaded = script->load (p.script, p.id);
        if (loaded.failed()) { problems.add (loaded.getErrorMessage()); continue; }
        packageIds.add (p.id);
        scripts.add (script.release());
    }

    builtin = std::make_unique<Script>();
    const auto loaded = builtin->load (builtinSource(), "projet");
    jassertquiet (loaded.wasOk());
    if (loaded.failed()) builtin.reset();
}

Set::Found Set::find (const juce::String& glyphId, const juce::StringArray& preferPackages) const
{
    if (glyphId.isEmpty()) return {};
    for (auto& preferred : preferPackages)
    {
        const int i = packageIds.indexOf (preferred);
        if (i >= 0 && scripts[i]->has (glyphId)) return { scripts[i], packageIds[i] };
    }
    for (int i = 0; i < scripts.size(); ++i)
        if (scripts[i]->has (glyphId)) return { scripts[i], packageIds[i] };
    if (builtin != nullptr && builtin->has (glyphId)) return { builtin.get(), "projet" };
    return {};
}

} // namespace Glyphs
