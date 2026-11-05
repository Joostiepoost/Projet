// SoundData.cpp

#include "SoundData.h"

namespace projet
{

bool readWholeFile (const juce::File& file, juce::AudioFormatManager& formats, juce::AudioBuffer<float>& out,
                    double& sampleRate, int& channels, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        error = "Could not read " + file.getFileName() + ".";
        return false;
    }

    const auto length = reader->lengthInSamples;
    const double maxSeconds = 60.0 * 60.0;

    if (length <= 0)                                         { error = file.getFileName() + " is empty."; return false; }
    if ((double) length / reader->sampleRate > maxSeconds)   { error = file.getFileName() + " is longer than an hour."; return false; }

    sampleRate = reader->sampleRate;
    channels = (int) reader->numChannels;
    out.setSize (2, (int) length, false, false, true);
    reader->read (&out, 0, (int) length, 0, true, true);

    if (reader->numChannels == 1)
        out.copyFrom (1, 0, out, 0, 0, (int) length);

    return true;
}

std::shared_ptr<const SoundData> SoundData::load (const juce::File& file, const juce::String& id,
                                                  juce::AudioFormatManager& formats, juce::String& error)
{
    auto s = std::make_shared<SoundData>();
    int channels = 0;
    if (! readWholeFile (file, formats, s->audio, s->sampleRate, channels, error))
        return {};
    s->id = id;
    s->file = file;
    s->buildPeaks();
    return s;
}

std::shared_ptr<const SoundData> SoundData::fromBuffer (juce::AudioBuffer<float>&& buffer, double sampleRate,
                                                        const juce::String& id)
{
    auto s = std::make_shared<SoundData>();
    s->id = id;
    s->sampleRate = sampleRate;

    if (buffer.getNumChannels() == 2)
        s->audio = std::move (buffer);
    else
    {
        s->audio.setSize (2, buffer.getNumSamples());
        s->audio.copyFrom (0, 0, buffer, 0, 0, buffer.getNumSamples());
        s->audio.copyFrom (1, 0, buffer, buffer.getNumChannels() > 1 ? 1 : 0, 0, buffer.getNumSamples());
    }

    s->buildPeaks();
    return s;
}

void SoundData::buildPeaks()
{
    const int n = audio.getNumSamples();
    const int blocks = (n + peakBlock - 1) / peakBlock;
    peakMin.assign ((size_t) blocks, 0.0f);
    peakMax.assign ((size_t) blocks, 0.0f);

    for (int b = 0; b < blocks; ++b)
    {
        const int start = b * peakBlock;
        const int num = juce::jmin (peakBlock, n - start);
        float lo = 0.0f, hi = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto r = juce::FloatVectorOperations::findMinAndMax (audio.getReadPointer (ch, start), num);
            lo = juce::jmin (lo, r.getStart());
            hi = juce::jmax (hi, r.getEnd());
        }

        peakMin[(size_t) b] = lo;
        peakMax[(size_t) b] = hi;
    }
}

//==============================================================================

SoundPool::SoundPool()
{
    formats.registerBasicFormats();
}

SoundPool::~SoundPool()
{
    *alive = false;
    pool.removeAllJobs (true, 10000);
}

void SoundPool::clear()
{
    const juce::ScopedLock sl (lock);
    sounds.clear();
    pending.clear();
}

std::shared_ptr<const SoundData> SoundPool::get (const juce::String& id) const
{
    const juce::ScopedLock sl (lock);
    auto it = sounds.find (id);
    return it != sounds.end() ? it->second : nullptr;
}

bool SoundPool::isLoading() const
{
    const juce::ScopedLock sl (lock);
    return ! pending.empty();
}

void SoundPool::sync (const std::map<juce::String, juce::File>& wanted)
{
    std::vector<std::pair<juce::String, juce::File>> toLoad;
    {
        const juce::ScopedLock sl (lock);

        for (auto it = sounds.begin(); it != sounds.end();)
        {
            auto w = wanted.find (it->first);
            if (w == wanted.end() || w->second != it->second->file) it = sounds.erase (it);
            else ++it;
        }

        for (auto& [id, file] : wanted)
            if (sounds.find (id) == sounds.end() && pending.find (id) == pending.end())
            {
                pending[id] = file;
                toLoad.emplace_back (id, file);
            }
    }

    for (auto& [id, file] : toLoad)
    {
        auto aliveFlag = alive;
        pool.addJob ([this, aliveFlag, id = id, file = file]
        {
            juce::String error;
            auto data = SoundData::load (file, id, formats, error);

            juce::MessageManager::callAsync ([this, aliveFlag, id, file, data, error]
            {
                if (! *aliveFlag) return;
                {
                    const juce::ScopedLock sl (lock);
                    auto p = pending.find (id);
                    if (p == pending.end() || p->second != file) return;   // forgotten meanwhile
                    pending.erase (p);
                    if (data != nullptr) sounds[id] = data;
                }
                if (onLoaded) onLoaded (id, error);
            });
        });
    }
}

} // namespace projet
