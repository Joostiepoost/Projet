// Arrangement.h — what the audio thread plays: a flat, immutable list of
// clips in samples, rebuilt on the message thread whenever the model changes.
//
// Containers (stage 5a) are flattened into the same list, so the audio thread
// never sees the tree: a sound inside a container becomes a clip at the place
// where it sounds, with the container's gain, and a window for each container
// around it: the clip sounds only inside every window (cut with a short fade
// where it crosses an edge), shaped by the containers' own fades.

#pragma once

#include <JuceHeader.h>
#include "SoundData.h"
#include "../Model/Project.h"

#include <cmath>
#include <functional>
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

/** A container around a clip: where the clip may sound, and the fades there
    (timeline samples). */
struct Window
{
    int64_t start = 0, end = 0;
    int64_t fadeIn = 0, fadeOut = 0;
    FadeShape inShape = FadeShape::sCurve, outShape = FadeShape::sCurve;

    float gainAt (int64_t s) const noexcept
    {
        float g = 1.0f;
        if (fadeIn > 0 && s - start < fadeIn)
            g *= fadeCurve (inShape, (double) (s - start) / (double) fadeIn);
        const int64_t fromEnd = end - 1 - s;
        if (fadeOut > 0 && fromEnd < fadeOut)
            g *= fadeCurve (outShape, (double) fromEnd / (double) fadeOut);
        return g;
    }
};

struct Clip
{
    std::shared_ptr<const SoundData> sound;
    int64_t start = 0, length = 0;      // on the timeline, project samples
    int64_t offset = 0;                 // into the sound, project samples
    float gain = 1.0f;
    int64_t fadeIn = 0, fadeOut = 0;
    FadeShape inShape = FadeShape::sCurve, outShape = FadeShape::sCurve;
    /** What sounds: [playFrom, playTo), inside the clip and every window. */
    int64_t playFrom = 0, playTo = 0;
    std::vector<Window> windows;        // the containers around it, outermost first

    int64_t end() const noexcept        { return start + length; }

    /** Gain at sample i of the clip (0 <= i < length), fades included
        (not the windows'). */
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

    /** Gain at timeline sample s, windows included. */
    float gainAtTime (int64_t s) const noexcept
    {
        float g = gainAt ((double) (s - start));
        for (const auto& w : windows) g *= w.gainAt (s);
        return g;
    }
};

struct Arrangement
{
    /** The fade a sound gets where a container's edge cuts it (seconds). */
    static constexpr double cutFade = 0.005;

    double sampleRate = 48000.0;
    std::vector<Clip> clips;            // sorted by playFrom
    int64_t end = 0;

    /** From the project: sound boxes whose sound is loaded, at every depth,
        mute/solo applied in each timeline. */
    static std::shared_ptr<const Arrangement> build (Project&, const SoundPool&);
    /** The same, with the sounds from `lookup` (nullptr: not loaded). */
    using Lookup = std::function<std::shared_ptr<const SoundData> (const juce::String& soundId)>;
    static std::shared_ptr<const Arrangement> build (Project&, const Lookup& lookup);
};

} // namespace projet
