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
        explicit Parser (const juce::String& text)
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

    bool isWhole (double d)  { return std::abs (d - std::round (d)) < 1.0e-9; }
}

Result parse (const juce::String& text)
{
    return Parser (text).run();
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
