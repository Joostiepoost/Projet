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
//   + - * × / ( ) as usual
// Times (m:ss) are typed in the time fields, never here: a colon is a ratio.

#pragma once

#include <JuceHeader.h>

namespace Proportion
{
    struct Result
    {
        bool ok = false;
        double value = 0.0;
        juce::String error;
    };

    Result parse (const juce::String& text);

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
