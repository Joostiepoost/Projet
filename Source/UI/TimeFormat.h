// TimeFormat.h — times as m:ss.mmm.

#pragma once

#include <JuceHeader.h>

namespace TimeFormat
{
    /** 0:01.250 (decimals 3), 0:01.2 (decimals 1), 0:01 (decimals 0). */
    inline juce::String format (double seconds, int decimals = 3)
    {
        const bool negative = seconds < 0.0;
        seconds = std::abs (seconds);
        const double scale = std::pow (10.0, decimals);
        const auto units = (juce::int64) std::llround (seconds * scale);
        const auto whole = units / (juce::int64) scale;
        const auto frac = units % (juce::int64) scale;
        juce::String s = (negative ? "-" : "") + juce::String (whole / 60) + ":" + juce::String (whole % 60).paddedLeft ('0', 2);
        if (decimals > 0) s << "." << juce::String (frac).paddedLeft ('0', decimals);
        return s;
    }
}
