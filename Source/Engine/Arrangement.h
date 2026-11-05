// Arrangement.h — what the audio thread plays: a flat, immutable list of
// clips in samples, rebuilt on the message thread whenever the model changes.
//
// Stage 1 has only sound boxes on tracks. Nested boxes (later stages) will be
// flattened into the same list, so the audio thread never sees the tree.

#pragma once

#include <JuceHeader.h>
#include "SoundData.h"
#include "../Model/Project.h"

#include <cmath>
#include <memory>
#include <vector>

namespace projet
{

/** Gain of a fade at progress x (0 = silent end, 1 = full level). */
inline float fadeCurve (FadeShape shape, double x) noexcept
{
    x = x <= 0.0 ? 0.0 : (x >= 1.0 ? 1.0 : x);
    constexpr double pi = 3.14159265358979323846;
    switch (shape)
    {
        case FadeShape::equalPower: return (float) std::sin (0.5 * pi * x);
        case FadeShape::sCurve:     return (float) (0.5 - 0.5 * std::cos (pi * x));
        case FadeShape::linear:     break;
    }
    return (float) x;
}

struct Clip
{
    std::shared_ptr<const SoundData> sound;
    int64_t start = 0, length = 0;      // on the timeline, project samples
    int64_t offset = 0;                 // into the sound, project samples
    float gain = 1.0f;
    int64_t fadeIn = 0, fadeOut = 0;
    FadeShape inShape = FadeShape::sCurve, outShape = FadeShape::sCurve;

    int64_t end() const noexcept        { return start + length; }

    /** Gain at sample i of the clip (0 <= i < length), fades included. */
    float gainAt (double i) const noexcept
    {
        float g = gain;
        if (fadeIn > 0 && i < (double) fadeIn)
            g *= fadeCurve (inShape, i / (double) fadeIn);
        const double fromEnd = (double) (length - 1) - i;
        if (fadeOut > 0 && fromEnd < (double) fadeOut)
            g *= fadeCurve (outShape, fromEnd / (double) fadeOut);
        return g;
    }
};

struct Arrangement
{
    double sampleRate = 48000.0;
    std::vector<Clip> clips;            // sorted by start
    int64_t end = 0;

    /** From the project: sound boxes whose sound is loaded, mute/solo applied. */
    static std::shared_ptr<const Arrangement> build (Project&, const SoundPool&);
};

} // namespace projet
