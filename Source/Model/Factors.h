// Factors.h — variation factors drawn as curves along a box (stage 8,
// framework layer 4): how much of an energy's speed, density, grain ... at
// each point of the box, from 0 to 1.
//
// A curve is a list of points (t from 0 to 1 along the box, a value from 0
// to 1), joined by smooth curves (monotone, so they never overshoot the
// points) or, from a point marked straight, by a line. A box keeps the
// curves of a category's factors in one property, fw.<package>.<category>.factors:
//
//     vitesse=0:0.5 0.6:0.9L 1:0.2;densite=0:0.3 1:0.7
//
// (each point t:value, L after a point: a straight line to the next one).

#pragma once

#include <JuceHeader.h>
#include <map>

namespace Factors
{
    struct Point
    {
        double t = 0.0, v = 0.5;
        bool straight = false;          // a straight line to the next point (else a smooth curve)
        bool operator== (const Point& o) const noexcept   { return std::abs (t - o.t) < 1.0e-9 && std::abs (v - o.v) < 1.0e-9 && straight == o.straight; }
    };
    using Curve = juce::Array<Point>;
    using Curves = std::map<juce::String, Curve>;

    Curves parse (const juce::String& text);
    juce::String format (const Curves&);
    Curve curveOf (const juce::String& text, const juce::String& factorId);
    /** The text with this factor's curve replaced (an empty curve removes it). */
    juce::String withCurve (const juce::String& text, const juce::String& factorId, const Curve&);

    /** Points sorted by t, within 0..1, at most one per t. */
    Curve tidy (Curve);
    /** The value at t (an empty curve: `neutral`). */
    double valueAt (const Curve&, double t, double neutral);
    /** n + 1 values, at t = 0, 1/n ... 1. */
    juce::Array<float> sample (const Curve&, int n, double neutral);
    /** The part from a to b, stretched to 0..1 (for splitting a box). */
    Curve section (const Curve&, double a, double b);
    /** Shapes to start from: "flat", "rising", "falling", "swell", "dip", "steps". */
    Curve preset (const juce::String& shape, double neutral);
    juce::StringArray presetNames();
}
