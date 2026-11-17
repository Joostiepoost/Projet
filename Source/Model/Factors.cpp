// Factors.cpp — see Factors.h.

#include "Factors.h"

namespace Factors
{

Curves parse (const juce::String& text)
{
    Curves out;
    for (auto& part : juce::StringArray::fromTokens (text, ";", ""))
    {
        const auto id = part.upToFirstOccurrenceOf ("=", false, false).trim();
        if (id.isEmpty()) continue;
        Curve c;
        for (auto& p : juce::StringArray::fromTokens (part.fromFirstOccurrenceOf ("=", false, false), " ", ""))
        {
            auto s = p.trim();
            if (s.isEmpty() || ! s.containsChar (':')) continue;
            Point pt;
            pt.straight = s.endsWithIgnoreCase ("L");
            if (pt.straight) s = s.dropLastCharacters (1);
            pt.t = s.upToFirstOccurrenceOf (":", false, false).getDoubleValue();
            pt.v = s.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
            c.add (pt);
        }
        c = tidy (c);
        if (! c.isEmpty()) out[id] = c;
    }
    return out;
}

juce::String format (const Curves& curves)
{
    juce::StringArray parts;
    for (auto& [id, c] : curves)
    {
        if (c.isEmpty()) continue;
        juce::StringArray pts;
        for (auto& p : c)
            pts.add (juce::String (p.t, 4).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + ":"
                     + juce::String (p.v, 4).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + (p.straight ? "L" : ""));
        parts.add (id + "=" + pts.joinIntoString (" "));
    }
    return parts.joinIntoString (";");
}

Curve curveOf (const juce::String& text, const juce::String& factorId)
{
    const auto all = parse (text);
    auto it = all.find (factorId);
    return it != all.end() ? it->second : Curve();
}

juce::String withCurve (const juce::String& text, const juce::String& factorId, const Curve& c)
{
    auto all = parse (text);
    const auto tidied = tidy (c);
    if (tidied.isEmpty()) all.erase (factorId);
    else all[factorId] = tidied;
    return format (all);
}

Curve tidy (Curve c)
{
    for (auto& p : c)
    {
        p.t = juce::jlimit (0.0, 1.0, std::isfinite (p.t) ? p.t : 0.0);
        p.v = juce::jlimit (0.0, 1.0, std::isfinite (p.v) ? p.v : 0.5);
    }
    std::stable_sort (c.begin(), c.end(), [] (const Point& a, const Point& b) { return a.t < b.t; });
    Curve out;
    for (auto& p : c)
    {
        if (! out.isEmpty() && std::abs (out.getLast().t - p.t) < 1.0e-6) out.getReference (out.size() - 1) = p;
        else out.add (p);
    }
    return out;
}

namespace
{
    // Fritsch-Carlson tangents: monotone between the points, no overshoot.
    juce::Array<double> tangents (const Curve& c)
    {
        const int n = c.size();
        juce::Array<double> d, m;
        for (int i = 0; i + 1 < n; ++i)
        {
            const double dt = c[i + 1].t - c[i].t;
            d.add (dt > 1.0e-12 ? (c[i + 1].v - c[i].v) / dt : 0.0);
        }
        for (int i = 0; i < n; ++i)
        {
            if (i == 0) m.add (n > 1 ? d[0] : 0.0);
            else if (i == n - 1) m.add (d[n - 2]);
            else m.add (d[i - 1] * d[i] <= 0.0 ? 0.0 : (d[i - 1] + d[i]) / 2.0);
        }
        for (int i = 0; i + 1 < n; ++i)
        {
            if (std::abs (d[i]) < 1.0e-12) { m.set (i, 0.0); m.set (i + 1, 0.0); continue; }
            const double a = m[i] / d[i], b = m[i + 1] / d[i];
            const double s = a * a + b * b;
            if (s > 9.0)
            {
                const double k = 3.0 / std::sqrt (s);
                m.set (i, k * a * d[i]);
                m.set (i + 1, k * b * d[i]);
            }
        }
        return m;
    }

    double evaluate (const Curve& c, const juce::Array<double>& m, double t)
    {
        const int n = c.size();
        if (t <= c[0].t) return c[0].v;
        if (t >= c[n - 1].t) return c[n - 1].v;
        int i = 0;
        while (i + 1 < n && c[i + 1].t < t) ++i;
        const auto& a = c[i];
        const auto& b = c[i + 1];
        const double h = b.t - a.t;
        if (h <= 1.0e-12) return b.v;
        const double s = (t - a.t) / h;
        if (a.straight) return a.v + (b.v - a.v) * s;
        const double s2 = s * s, s3 = s2 * s;
        const double v = (2 * s3 - 3 * s2 + 1) * a.v + (s3 - 2 * s2 + s) * h * m[i]
                       + (-2 * s3 + 3 * s2) * b.v + (s3 - s2) * h * m[i + 1];
        return juce::jlimit (0.0, 1.0, v);
    }
}

double valueAt (const Curve& c, double t, double neutral)
{
    if (c.isEmpty()) return neutral;
    if (c.size() == 1) return c[0].v;
    return evaluate (c, tangents (c), t);
}

juce::Array<float> sample (const Curve& c, int n, double neutral)
{
    juce::Array<float> out;
    n = juce::jmax (1, n);
    if (c.size() < 2)
    {
        const float v = (float) (c.isEmpty() ? neutral : c[0].v);
        for (int i = 0; i <= n; ++i) out.add (v);
        return out;
    }
    const auto m = tangents (c);
    for (int i = 0; i <= n; ++i) out.add ((float) evaluate (c, m, (double) i / n));
    return out;
}

Curve section (const Curve& c, double a, double b)
{
    if (c.isEmpty() || b <= a) return {};
    Curve out;
    const double span = b - a;
    out.add ({ 0.0, valueAt (c, a, 0.5), false });
    for (int i = 0; i < c.size(); ++i)
        if (c[i].t > a + 1.0e-9 && c[i].t < b - 1.0e-9)
            out.add ({ (c[i].t - a) / span, c[i].v, c[i].straight });
    out.add ({ 1.0, valueAt (c, b, 0.5), false });
    // A straight stretch cut in two stays straight on both sides.
    for (int i = 0; i + 1 < c.size(); ++i)
        if (c[i].straight && c[i].t <= a && c[i + 1].t > a) out.getReference (0).straight = true;
    return tidy (out);
}

Curve preset (const juce::String& shape, double neutral)
{
    auto curve = [] (std::initializer_list<Point> points) { Curve c; for (auto& p : points) c.add (p); return c; };
    if (shape == "rising")  return curve ({ Point { 0.0, 0.1, false }, Point { 1.0, 0.9, false } });
    if (shape == "falling") return curve ({ Point { 0.0, 0.9, false }, Point { 1.0, 0.1, false } });
    if (shape == "swell")   return curve ({ Point { 0.0, 0.15, false }, Point { 0.5, 0.9, false }, Point { 1.0, 0.15, false } });
    if (shape == "dip")     return curve ({ Point { 0.0, 0.85, false }, Point { 0.5, 0.1, false }, Point { 1.0, 0.85, false } });
    if (shape == "steps")   return curve ({ Point { 0.0, 0.2, true }, Point { 0.33, 0.2, true }, Point { 0.34, 0.5, true },
                                            Point { 0.66, 0.5, true }, Point { 0.67, 0.8, true }, Point { 1.0, 0.8, true } });
    return curve ({ Point { 0.0, neutral, false }, Point { 1.0, neutral, false } });
}

juce::StringArray presetNames()
{
    return { "flat", "rising", "falling", "swell", "dip", "steps" };
}

}
