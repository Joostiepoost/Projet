// Arrangement.cpp

#include "Arrangement.h"

namespace projet
{

namespace
{
    /** Where a timeline lies on the piece's: piece seconds = a + b * t, with
        the gain and windows of the containers around it. */
    struct Level
    {
        double a = 0.0, b = 1.0;
        float gain = 1.0f;
        struct Edge { double start, end, fadeIn, fadeOut; FadeShape inShape, outShape; };
        std::vector<Edge> windows;
    };

    FadeShape shapeOf (const juce::ValueTree& box, const juce::Identifier& id)
    {
        return (FadeShape) juce::jlimit (0, numFadeShapes - 1, (int) box.getProperty (id, (int) FadeShape::sCurve));
    }

    void addTimeline (const juce::ValueTree& tracks, const Level& level, Project& project, const Arrangement::Lookup& pool, Arrangement& a)
    {
        const double rate = a.sampleRate;
        auto samples = [rate] (double seconds) { return (int64_t) std::llround (seconds * rate); };

        // Folders pass their mute and solo on to the tracks inside them; a
        // solo counts in its own timeline.
        const bool anySolo = Project::anySolo (tracks);

        for (auto track : tracks)
        {
            if (Project::isFolder (track) || project.effectiveMute (track) || (anySolo && ! project.effectiveSolo (track)))
                continue;

            for (auto box : track)
            {
                const double boxGain = juce::Decibels::decibelsToGain ((double) box.getProperty (IDs::gainDb, 0.0), -100.0);

                if (Project::isContainer (box))
                {
                    // Its inside: inner t lies at start + (t - innerOffset) * scale here.
                    const double start = level.a + level.b * Project::boxStart (box);
                    const double length = level.b * Project::boxLength (box);
                    Level inner = level;
                    inner.b = level.b * Project::containerScale (box);
                    inner.a = start - Project::innerOffset (box) * inner.b;
                    inner.gain = level.gain * (float) boxGain;
                    inner.windows.push_back ({ start, start + length,
                                               level.b * (double) box.getProperty (IDs::fadeIn, 0.0),
                                               level.b * (double) box.getProperty (IDs::fadeOut, 0.0),
                                               shapeOf (box, IDs::fadeInShape), shapeOf (box, IDs::fadeOutShape) });
                    addTimeline (Project::innerTracks (box), inner, project, pool, a);
                    continue;
                }

                if (! Project::isSound (box)) continue;
                auto sound = pool (box[IDs::soundId].toString());
                if (sound == nullptr) continue;

                // Sounds keep their speed and length: only the start moves.
                Clip c;
                c.sound = sound;
                c.start = samples (level.a + level.b * Project::boxStart (box));
                c.offset = juce::jlimit ((int64_t) 0, sound->length(), samples ((double) box[IDs::offset]));
                c.length = juce::jmin (samples (Project::boxLength (box)), sound->length() - c.offset);
                if (c.length <= 0) continue;

                c.gain = level.gain * (float) boxGain;
                c.fadeIn = juce::jlimit ((int64_t) 0, c.length, samples ((double) box[IDs::fadeIn]));
                c.fadeOut = juce::jlimit ((int64_t) 0, c.length, samples ((double) box[IDs::fadeOut]));
                c.inShape = shapeOf (box, IDs::fadeInShape);
                c.outShape = shapeOf (box, IDs::fadeOutShape);

                // Inside every container around it; a short fade where an edge cuts it.
                c.playFrom = c.start;
                c.playTo = c.end();
                const int64_t cut = samples (Arrangement::cutFade);
                for (auto& e : level.windows)
                {
                    Window w;
                    w.start = samples (e.start);
                    w.end = samples (e.end);
                    w.fadeIn = samples (e.fadeIn);
                    w.fadeOut = samples (e.fadeOut);
                    w.inShape = e.inShape;
                    w.outShape = e.outShape;
                    if (c.start < w.start && w.fadeIn < cut) { w.fadeIn = cut; w.inShape = FadeShape::linear; }
                    if (c.end() > w.end && w.fadeOut < cut)   { w.fadeOut = cut; w.outShape = FadeShape::linear; }
                    c.playFrom = juce::jmax (c.playFrom, w.start);
                    c.playTo = juce::jmin (c.playTo, w.end);
                    if (w.fadeIn > 0 || w.fadeOut > 0) c.windows.push_back (w);
                }
                if (c.playTo <= c.playFrom) continue;

                a.end = juce::jmax (a.end, c.playTo);
                a.clips.push_back (std::move (c));
            }
        }
    }
}

std::shared_ptr<const Arrangement> Arrangement::build (Project& project, const SoundPool& pool)
{
    return build (project, [&pool] (const juce::String& id) { return pool.get (id); });
}

std::shared_ptr<const Arrangement> Arrangement::build (Project& project, const Lookup& lookup)
{
    auto a = std::make_shared<Arrangement>();
    a->sampleRate = project.getSampleRate();
    addTimeline (project.getTracks(), Level(), project, lookup, *a);
    std::sort (a->clips.begin(), a->clips.end(), [] (const Clip& x, const Clip& y) { return x.playFrom < y.playFrom; });
    return a;
}

} // namespace projet
