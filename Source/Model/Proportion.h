// Proportion.h — proportions of a frame: typed expressions, names for values,
// and the divisions shown on the proportion ruler.
//
// What can be typed in a proportion field (all relative to the box's frame):
//   3/11          a fraction
//   2.5/11  3/7.5 decimal numerators and denominators
//   0.2727  27.3% a decimal or a percentage
//   2/3 of 3/4    "of" multiplies (= 1/2)
//   phi  φ        the golden ratio 1.618...; so 1/φ = 0.618 and 1-1/φ = 0.382
//   golden        0.618... (the golden section of the frame)
//   pi  π, sqrt(2)  √2   constants and square roots
//   3:8           a ratio: the first part of 3 + 8 (= 3/11)
//   m1            a named mark (stage 3b): where the mark is, as a proportion
//                 of this frame. "m2 - m1" is the distance between two marks.
//   + - * × / ( ) as usual
// Times (m:ss) are typed in the time fields, never here: a colon is a ratio.

#pragma once

#include <JuceHeader.h>
#include <optional>

namespace Proportion
{
    struct Result
    {
        bool ok = false;
        double value = 0.0;
        juce::String error;
    };

    /** Gives the value of a mark's name (as a proportion of the frame being
        measured), or nothing if there is no such mark. */
    using Resolver = std::function<std::optional<double> (const juce::String& name)>;

    Result parse (const juce::String& text, const Resolver& marks = {});

    /** A mark name: a letter, then letters, digits or _ ; not a word the
        expressions already use (phi, golden, pi, sqrt, of). */
    bool isValidName (const juce::String& name);
    /** The expression with every whole-word `oldName` replaced. */
    juce::String replaceName (const juce::String& expression, const juce::String& oldName, const juce::String& newName);
    bool refersTo (const juce::String& expression, const juce::String& name);

    /** Parts of a ratio split: "2:3:5:8" -> {2,3,5,8}; empty if not valid. */
    juce::Array<double> parseParts (const juce::String& text);
    /** The boundaries between the parts, as fractions written exactly
        ("2/18" reduced to "1/9"). */
    juce::StringArray partBoundaries (const juce::Array<double>& parts);

    /** Values as a small whole-number ratio ("3 : 8 : 5") when there is one
        within half a percent (marked "≈" when not exact), else relative to the
        first ("1 : 2.667"). */
    juce::String ratioText (const juce::Array<double>& values);

    /** A point of a series (golden section, Fibonacci, harmonic, your own). */
    struct SeriesPoint { double position = 0.0; juce::String label; };
    /** Series written as "golden:2;fibonacci:13;harmonic:8;custom:1/7,3/7". */
    std::vector<SeriesPoint> seriesPoints (const juce::String& spec);
    juce::String seriesPart (const juce::String& spec, const juce::String& key);
    juce::String withSeriesPart (const juce::String& spec, const juce::String& key, const juce::String& value);

    /** The closest n/d with d from 1 to maxDenominator (and the extra
        denominators, which may be decimal). */
    struct Rational { double n = 0.0, d = 1.0; double error = 0.0; };
    Rational closest (double value, int maxDenominator = 20, const juce::Array<double>& extra = {});

    /** "3/11" when the value is that fraction (within 1e-7), else a decimal
        with up to `decimals` places ("0.2731"). */
    juce::String name (double value, const juce::Array<double>& extra = {}, int decimals = 4);

    /** "3/11", or "0.2731 ≈ 3/11" when it is only close. */
    juce::String describe (double value, const juce::Array<double>& extra = {});

    /** How a value is written back after a drag: the fraction when it is
        exactly one, else six decimals. */
    juce::String expressionFor (double value, const juce::Array<double>& extra = {});

    /** A point on the proportion ruler. */
    struct Division
    {
        double position = 0.0;  // 0..1 of the frame
        double numerator = 1.0, denominator = 2.0;
        double rank = 2.0;      // smaller = simpler = drawn taller; the denominator, rounded up
        juce::String label() const;
    };

    /** Every division of the frame for these denominators, each position once
        (in its simplest form), sorted by position. A decimal denominator d
        gives floor(d) full parts and a shorter last part. */
    std::vector<Division> divisions (const juce::Array<double>& denominators);

    /** Index of a denominator in the list (compared with a tolerance), or -1. */
    int indexOf (const juce::Array<double>& list, double value);

    /** "2,3,4" <-> {2,3,4}. */
    juce::Array<double> parseList (const juce::String&);
    juce::String formatList (const juce::Array<double>&);
    juce::String formatNumber (double);
}
