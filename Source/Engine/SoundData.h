// SoundData.h — a placeholder sound decoded into memory (two channels, 32-bit
// float, at the project's sample rate), with a min/max overview for drawing.
//
// SoundPool keeps the sounds of the open project, loading them on a
// background thread.

#pragma once

#include <JuceHeader.h>
#include <map>
#include <memory>

namespace projet
{

struct SoundData
{
    juce::String id;
    juce::File file;
    double sampleRate = 48000.0;
    juce::AudioBuffer<float> audio;     // always 2 channels

    int64_t length() const noexcept     { return audio.getNumSamples(); }
    double seconds() const noexcept     { return (double) length() / sampleRate; }

    static constexpr int peakBlock = 256;
    std::vector<float> peakMin, peakMax;    // per block, both channels together

    static std::shared_ptr<const SoundData> load (const juce::File&, const juce::String& id,
                                                  juce::AudioFormatManager&, juce::String& error);
    static std::shared_ptr<const SoundData> fromBuffer (juce::AudioBuffer<float>&&, double sampleRate,
                                                        const juce::String& id);

    void buildPeaks();
};

/** Reads a whole file into a 2-channel float buffer (mono is copied to both).
    `progress` (optional) gets 0..1 and returns false to stop reading. */
bool readWholeFile (const juce::File&, juce::AudioFormatManager&, juce::AudioBuffer<float>& out,
                    double& sampleRate, int& channels, juce::String& error,
                    std::function<bool (float)> progress = {});

class SoundPool
{
public:
    SoundPool();
    ~SoundPool();

    /** Loads (in the background) every sound in `wanted` (id -> file) not loaded
        yet, and forgets the others. */
    void sync (const std::map<juce::String, juce::File>& wanted);
    void clear();

    std::shared_ptr<const SoundData> get (const juce::String& id) const;
    bool isLoading() const;

    /** Message thread, after each sound has loaded (or failed). */
    std::function<void (const juce::String& id, const juce::String& error)> onLoaded;

    juce::AudioFormatManager& getFormats()     { return formats; }

private:
    juce::AudioFormatManager formats;
    std::map<juce::String, std::shared_ptr<const SoundData>> sounds;
    std::map<juce::String, juce::File> pending;
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withThreadName ("Projet sound loader").withNumberOfThreads (2) };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    mutable juce::CriticalSection lock;
};

} // namespace projet
