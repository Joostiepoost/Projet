// Proportion.cpp

#include "Proportion.h"
#include <numeric>

namespace Proportion
{

namespace
{
    constexpr double phiValue = 1.6180339887498949;

    // A small recursive-descent parser over UTF-32 characters.
    class Parser
    {
    public:
        Parser (const juce::String& text, const Resolver& r) : resolver (r)
        {
            for (auto p = text.getCharPointer(); ! p.isEmpty();)
                s.push_back (p.getAndAdvance());
        }

        Result run()
        {
            Result r;
            skip();
            if (pos >= s.size()) { r.error = "Type a proportion, for example 3/11."; return r; }

            double a = expr();
            skip();
            if (ok && peek() == U':')
            {
                ++pos;
                const double b = expr();
                if (ok && std::abs (a + b) < 1.0e-12) fail ("A ratio needs parts that add up to more than 0.");
                if (ok) a = a / (a + b);
                skip();
                if (ok && peek() == U':') fail ("Only two parts here (3:8). Splitting into more parts comes in stage 3b.");
            }
            skip();
            if (ok && pos < s.size()) fail ("Did not understand \"" + rest() + "\".");

            if (ok && ! std::isfinite (a)) fail ("That gives no number (division by 0?).");
            r.ok = ok;
            r.value = ok ? a : 0.0;
            r.error = error;
            return r;
        }

    private:
        const Resolver& resolver;
        std::vector<juce::juce_wchar> s;
        size_t pos = 0;
        bool ok = true;
        juce::String error;

        juce::juce_wchar peek() const   { return pos < s.size() ? s[pos] : 0; }
        void skip()                     { while (pos < s.size() && juce::CharacterFunctions::isWhitespace (s[pos])) ++pos; }
        void fail (const juce::String& e) { if (ok) { ok = false; error = e; } }

        juce::String rest() const
        {
            juce::String r;
            for (size_t i = pos; i < s.size(); ++i) r += juce::String::charToString (s[i]);
            return r;
        }

        bool word (const char* w)
        {
            size_t i = 0;
            while (w[i] != 0)
            {
                if (pos + i >= s.size() || juce::CharacterFunctions::toLowerCase (s[pos + i]) != (juce::juce_wchar) w[i]) return false;
                ++i;
            }
            // A whole word only.
            if (pos + i < s.size() && juce::CharacterFunctions::isLetter (s[pos + i])) return false;
            pos += i;
            return true;
        }

        double expr()
        {
            double v = term();
            for (;;)
            {
                skip();
                if (peek() == U'+')      { ++pos; v += term(); }
                else if (peek() == U'-' || peek() == 0x2212) { ++pos; v -= term(); }
                else return v;
            }
        }

        double term()
        {
            double v = unary();
            for (;;)
            {
                skip();
                if (peek() == U'*' || peek() == 0x00d7)      { ++pos; v *= unary(); }
                else if (peek() == U'/' || peek() == 0x2215) { ++pos; const double d = unary(); if (ok && std::abs (d) < 1.0e-300) fail ("Division by 0."); else v /= d; }
                else if (word ("of"))                        { v *= unary(); }
                else return v;
            }
        }

        double unary()
        {
            skip();
            if (peek() == U'-' || peek() == 0x2212) { ++pos; return -unary(); }
            if (peek() == U'+') { ++pos; return unary(); }
            return atom();
        }

        double atom()
        {
            skip();
            if (! ok) return 0.0;
            const auto c = peek();

            if (c == U'(')
            {
                ++pos;
                const double v = expr();
                skip();
                if (peek() == U')') ++pos; else fail ("A closing bracket is missing.");
                return v;
            }

            if (juce::CharacterFunctions::isDigit (c) || c == U'.' || c == U',')
            {
                juce::String num;
                bool point = false;
                while (pos < s.size())
                {
                    const auto d = s[pos];
                    if (juce::CharacterFunctions::isDigit (d)) num += juce::String::charToString (d);
                    else if ((d == U'.' || d == U',') && ! point) { num += "."; point = true; }
                    else break;
                    ++pos;
                }
                double v = num.getDoubleValue();
                skip();
                if (peek() == U'%') { ++pos; v /= 100.0; }
                return v;
            }

            if (c == 0x03c6 || c == 0x03d5 || c == 0x03a6) { ++pos; return phiValue; }   // φ ϕ Φ
            if (c == 0x03c0) { ++pos; return juce::MathConstants<double>::pi; }        // π
            if (c == 0x221a) { ++pos; const double v = atom(); return root (v); }      // √

            if (word ("phi"))    return phiValue;
            if (word ("golden")) return 1.0 / phiValue;
            if (word ("pi"))     return juce::MathConstants<double>::pi;
            if (word ("sqrt"))   { const double v = atom(); return root (v); }

            // A mark's name
            if (juce::CharacterFunctions::isLetter (c) || c == U'_')
            {
                juce::String name;
                while (pos < s.size() && (juce::CharacterFunctions::isLetterOrDigit (s[pos]) || s[pos] == U'_'))
                    name += juce::String::charToString (s[pos++]);
                if (resolver)
                    if (auto v = resolver (name)) return *v;
                fail ("There is no mark called \"" + name + "\".");
                return 0.0;
            }

            if (pos >= s.size()) fail ("Something is missing at the end.");
            else fail ("Did not understand \"" + rest() + "\".");
            return 0.0;
        }

        double root (double v)
        {
            if (v < 0.0) { fail ("No square root of a negative number."); return 0.0; }
            return std::sqrt (v);
        }
    };

}

static bool isWhole (double d)  { return std::abs (d - std::round (d)) < 1.0e-9; }

Result parse (const juce::String& text, const Resolver& marks)
{
    return Parser (text, marks).run();
}

bool isValidName (const juce::String& name)
{
    if (name.isEmpty() || name.length() > 40) return false;
    if (! (juce::CharacterFunctions::isLetter (name[0]) || name[0] == '_')) return false;
    for (auto c : name) if (! (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_')) return false;
    for (auto w : { "phi", "golden", "pi", "sqrt", "of" }) if (name.equalsIgnoreCase (w)) return false;
    return true;
}

static void forEachWord (const juce::String& text, std::function<void (int start, int end)> fn)
{
    const int n = text.length();
    int i = 0;
    while (i < n)
    {
        const auto c = text[i];
        if (juce::CharacterFunctions::isLetter (c) || c == '_')
        {
            int j = i;
            while (j < n && (juce::CharacterFunctions::isLetterOrDigit (text[j]) || text[j] == '_')) ++j;
            fn (i, j);
            i = j;
        }
        else if (juce::CharacterFunctions::isDigit (c))
        {
            // Skip numbers (so "2e" style junk is not taken as a name).
            while (i < n && (juce::CharacterFunctions::isDigit (text[i]) || text[i] == '.' || text[i] == ',')) ++i;
        }
        else ++i;
    }
}

juce::String replaceName (const juce::String& expression, const juce::String& oldName, const juce::String& newName)
{
    juce::String out;
    int last = 0;
    forEachWord (expression, [&] (int a, int b)
    {
        if (expression.substring (a, b) == oldName)
        {
            out << expression.substring (last, a) << newName;
            last = b;
        }
    });
    out << expression.substring (last);
    return out;
}

bool refersTo (const juce::String& expression, const juce::String& name)
{
    bool found = false;
    forEachWord (expression, [&] (int a, int b) { if (expression.substring (a, b) == name) found = true; });
    return found;
}

juce::Array<double> parseParts (const juce::String& text)
{
    juce::Array<double> parts;
    for (auto& t : juce::StringArray::fromTokens (text, ":", ""))
    {
        const auto r = parse (t);
        if (! r.ok || r.value <= 0.0) return {};
        parts.add (r.value);
    }
    return parts.size() >= 2 ? parts : juce::Array<double>();
}

juce::StringArray partBoundaries (const juce::Array<double>& parts)
{
    juce::StringArray out;
    double total = 0.0;
    for (auto p : parts) total += p;
    double cumulative = 0.0;
    for (int i = 0; i + 1 < parts.size(); ++i)
    {
        cumulative += parts[i];
        if (isWhole (cumulative) && isWhole (total))
        {
            auto n = std::llround (cumulative), d = std::llround (total);
            const auto g = std::gcd (n, d);
            out.add (g > 1 ? juce::String (n / g) + "/" + juce::String (d / g) : juce::String (n) + "/" + juce::String (d));
        }
        else
            out.add (formatNumber (cumulative) + "/" + formatNumber (total));
    }
    return out;
}

juce::String ratioText (const juce::Array<double>& values)
{
    if (values.isEmpty()) return {};
    double smallest = values[0];
    for (auto v : values) smallest = juce::jmin (smallest, v);
    if (smallest <= 0.0) return {};

    for (int m = 1; m <= 24; ++m)
    {
        juce::Array<juce::int64> ints;
        bool ok = true;
        double worst = 0.0;
        for (auto v : values)
        {
            const double scaled = v / smallest * m;
            const double r = std::round (scaled);
            if (r < 1.0 || std::abs (scaled - r) > 0.005 * scaled) { ok = false; break; }
            worst = juce::jmax (worst, std::abs (scaled - r) / scaled);
            ints.add ((juce::int64) r);
        }
        if (! ok) continue;
        juce::int64 g = 0, biggest = 0;
        for (auto i : ints) { g = std::gcd (g, i); biggest = juce::jmax (biggest, i); }
        // A near ratio only helps with small numbers (8 : 13 yes, 6 : 61 no).
        if (worst > 1.0e-5 && biggest / juce::jmax ((juce::int64) 1, g) > 24) continue;
        juce::StringArray parts;
        for (auto i : ints) parts.add (juce::String (i / juce::jmax ((juce::int64) 1, g)));
        // Exact (as from snapped edges), or only close: say so.
        return (worst > 1.0e-5 ? juce::String::fromUTF8 ("\xe2\x89\x88 ") : juce::String()) + parts.joinIntoString (" : ");
    }

    juce::StringArray parts;
    for (auto v : values) parts.add (formatNumber (v / values[0]));
    return parts.joinIntoString (" : ");
}

juce::String seriesPart (const juce::String& spec, const juce::String& key)
{
    for (auto& part : juce::StringArray::fromTokens (spec, ";", ""))
        if (part.upToFirstOccurrenceOf (":", false, false).trim() == key)
            return part.fromFirstOccurrenceOf (":", false, false).trim();
    return {};
}

juce::String withSeriesPart (const juce::String& spec, const juce::String& key, const juce::String& value)
{
    juce::StringArray out;
    for (auto& part : juce::StringArray::fromTokens (spec, ";", ""))
        if (part.trim().isNotEmpty() && part.upToFirstOccurrenceOf (":", false, false).trim() != key)
            out.add (part.trim());
    if (value.trim().isNotEmpty()) out.add (key + ":" + value.trim());
    return out.joinIntoString (";");
}

std::vector<SeriesPoint> seriesPoints (const juce::String& spec)
{
    std::vector<SeriesPoint> out;
    auto add = [&] (double p, const juce::String& label)
    {
        if (p <= 1.0e-9 || p >= 1.0 - 1.0e-9) return;
        for (auto& o : out) if (std::abs (o.position - p) < 1.0e-9) return;
        out.push_back ({ p, label });
    };

    // Golden section: level 1 cuts the frame at 0.382 and 0.618; each further
    // level cuts every part again the same way.
    const int levels = juce::jlimit (0, 4, seriesPart (spec, "golden").getIntValue());
    if (levels > 0)
    {
        const double g = 1.0 / phiValue;
        std::vector<std::pair<double, double>> parts { { 0.0, 1.0 } };
        for (int level = 0; level < levels; ++level)
        {
            std::vector<std::pair<double, double>> next;
            for (auto [a, b] : parts)
            {
                const double p1 = a + (b - a) * (1.0 - g), p2 = a + (b - a) * g;
                const bool top = level == 0;
                add (p1, top ? juce::String::fromUTF8 ("1-1/\xcf\x86") : formatNumber (std::round (p1 * 1000.0) / 1000.0));
                add (p2, top ? juce::String::fromUTF8 ("1/\xcf\x86") : formatNumber (std::round (p2 * 1000.0) / 1000.0));
                next.push_back ({ a, p1 });
                next.push_back ({ p1, p2 });
                next.push_back ({ p2, b });
            }
            parts = next;
        }
    }

    // Fibonacci: F(j)/F(n) for the chosen F(n) (5, 8, 13, 21, 34, 55...)
    const int fib = seriesPart (spec, "fibonacci").getIntValue();
    if (fib >= 3)
    {
        juce::Array<int> f { 1, 2 };
        while (f.getLast() < fib && f.size() < 20) f.add (f[f.size() - 1] + f[f.size() - 2]);
        const int n = f.getLast();
        for (auto v : f) if (v < n) add ((double) v / n, juce::String (v) + "/" + juce::String (n));
    }

    // Harmonic: 1/2, 1/3 ... 1/N
    const int harmonic = seriesPart (spec, "harmonic").getIntValue();
    for (int k = 2; k <= juce::jmin (harmonic, 64); ++k)
        add (1.0 / k, "1/" + juce::String (k));

    // Your own list of expressions
    for (auto& t : juce::StringArray::fromTokens (seriesPart (spec, "custom"), ",", ""))
    {
        const auto r = parse (t);
        if (r.ok) add (r.value, t.trim());
    }

    std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.position < b.position; });
    return out;
}

juce::String formatNumber (double v)
{
    if (isWhole (v)) return juce::String ((juce::int64) std::llround (v));
    auto s = juce::String (v, 4);
    while (s.endsWithChar ('0')) s = s.dropLastCharacters (1);
    if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
    return s;
}

Rational closest (double value, int maxDenominator, const juce::Array<double>& extra)
{
    Rational best { std::round (value), 1.0, std::abs (value - std::round (value)) };
    auto consider = [&] (double d)
    {
        if (d <= 0.0) return;
        const double n = std::round (value * d);
        const double e = std::abs (value - n / d);
        if (e < best.error - 1.0e-12) best = { n, d, e };
    };
    for (int d = 2; d <= maxDenominator; ++d) consider ((double) d);
    for (auto d : extra) if (! isWhole (d) || d > maxDenominator) consider (d);
    return best;
}

static juce::String fraction (const Rational& r)
{
    if (isWhole (r.d) && std::llround (r.d) == 1) return formatNumber (r.n);
    return formatNumber (r.n) + "/" + formatNumber (r.d);
}

juce::String name (double value, const juce::Array<double>& extra, int decimals)
{
    const auto r = closest (value, 20, extra);
    if (r.error < 1.0e-7) return fraction (r);
    auto s = juce::String (value, decimals);
    while (s.containsChar ('.') && s.endsWithChar ('0')) s = s.dropLastCharacters (1);
    if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
    return s;
}

juce::String describe (double value, const juce::Array<double>& extra)
{
    const auto r = closest (value, 20, extra);
    if (r.error < 1.0e-7) return fraction (r);
    return name (value, extra) + juce::String::fromUTF8 (" \xe2\x89\x88 ") + fraction (r);
}

juce::String expressionFor (double value, const juce::Array<double>& extra)
{
    return name (value, extra, 6);
}

juce::String Division::label() const
{
    return formatNumber (numerator) + "/" + formatNumber (denominator);
}

std::vector<Division> divisions (const juce::Array<double>& dens)
{
    std::vector<Division> all;
    for (auto d : dens)
    {
        if (d <= 1.0 || d > 1000.0) continue;
        if (isWhole (d))
        {
            const int di = (int) std::llround (d);
            for (int n = 1; n < di; ++n)
                if (std::gcd (n, di) == 1)
                    all.push_back ({ (double) n / di, (double) n, (double) di, (double) di });
        }
        else
        {
            for (int n = 1; n < d; ++n)
                all.push_back ({ n / d, (double) n, d, std::ceil (d) });
        }
    }

    // Each position once, keeping the simplest name.
    std::sort (all.begin(), all.end(), [] (const Division& a, const Division& b)
    {
        if (std::abs (a.position - b.position) > 1.0e-9) return a.position < b.position;
        return a.rank < b.rank;
    });
    std::vector<Division> out;
    for (auto& v : all)
        if (out.empty() || std::abs (out.back().position - v.position) > 1.0e-9)
            out.push_back (v);
    return out;
}

int indexOf (const juce::Array<double>& list, double value)
{
    for (int i = 0; i < list.size(); ++i)
        if (std::abs (list[i] - value) < 1.0e-9) return i;
    return -1;
}

juce::Array<double> parseList (const juce::String& text)
{
    juce::Array<double> out;
    for (auto& t : juce::StringArray::fromTokens (text, ",; ", ""))
    {
        const double v = t.trim().getDoubleValue();
        if (v > 1.0 && indexOf (out, v) < 0) out.add (v);
    }
    std::sort (out.begin(), out.end());
    return out;
}

juce::String formatList (const juce::Array<double>& list)
{
    juce::StringArray parts;
    for (auto v : list) parts.add (formatNumber (v));
    return parts.joinIntoString (",");
}

}
