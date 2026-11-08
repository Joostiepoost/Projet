// Arrangement.cpp

#include "Arrangement.h"

namespace projet
{

std::shared_ptr<const Arrangement> Arrangement::build (Project& project, const SoundPool& pool)
{
    auto a = std::make_shared<Arrangement>();
    a->sampleRate = project.getSampleRate();
    const double rate = a->sampleRate;

    // Folders pass their mute and solo on to the tracks inside them.
    const bool anySolo = project.anySolo();

    for (auto track : project.getTracks())
    {
        if (Project::isFolder (track) || project.effectiveMute (track) || (anySolo && ! project.effectiveSolo (track)))
            continue;

        for (auto box : track)
        {
            if (box[IDs::kind].toString() != "sound") continue;

            auto sound = pool.get (box[IDs::soundId].toString());
            if (sound == nullptr) continue;

            Clip c;
            c.sound = sound;
            c.start = std::llround (Project::boxStart (box) * rate);
            c.offset = juce::jlimit ((int64_t) 0, sound->length(), (int64_t) std::llround ((double) box[IDs::offset] * rate));
            c.length = juce::jmin ((int64_t) std::llround (Project::boxLength (box) * rate), sound->length() - c.offset);
            if (c.length <= 0) continue;

            c.gain = juce::Decibels::decibelsToGain ((float) (double) box[IDs::gainDb], -100.0f);
            c.fadeIn = juce::jlimit ((int64_t) 0, c.length, (int64_t) std::llround ((double) box[IDs::fadeIn] * rate));
            c.fadeOut = juce::jlimit ((int64_t) 0, c.length, (int64_t) std::llround ((double) box[IDs::fadeOut] * rate));
            c.inShape = (FadeShape) juce::jlimit (0, numFadeShapes - 1, (int) box[IDs::fadeInShape]);
            c.outShape = (FadeShape) juce::jlimit (0, numFadeShapes - 1, (int) box[IDs::fadeOutShape]);

            a->end = juce::jmax (a->end, c.end());
            a->clips.push_back (std::move (c));
        }
    }

    std::sort (a->clips.begin(), a->clips.end(), [] (const Clip& x, const Clip& y) { return x.start < y.start; });
    return a;
}

} // namespace projet
