// Importer.h — turns an imported sound into a compact placeholder copy.
//
//   1. read the whole file (WAV, AIFF, FLAC, Ogg; on macOS also MP3, AAC/M4A, CAF)
//   2. stereo: mono is copied to both channels, more than two keep the first two
//   3. resample to the project rate if needed (Effet's sinc kernel, "High")
//   4. encode as Ogg Vorbis (default, quality 0.7) or FLAC 24-bit
//
// The placeholder is only a stand-in for sketching; the path of the original
// is kept so it can be swapped for the production file later.
//
// Importer runs jobs on a pool of threads (one sound per thread) and reports
// on the message thread.

#pragma once

#include <JuceHeader.h>
#include <memory>

namespace projet
{

struct ImportSettings
{
    double targetRate = 48000.0;
    juce::String format = "ogg";        // "ogg" or "flac"
    int vorbisQuality = 7;              // 0..10, q = index / 10
};

struct ImportResult
{
    bool ok = false;
    juce::String error;
    juce::File original, placeholder;
    juce::String hash;
    int64_t lengthSamples = 0;          // placeholder length, at the target rate
    double originalRate = 0.0;
    int originalChannels = 0;

    // Timings, seconds (for the measurement report).
    double readSeconds = 0.0, resampleSeconds = 0.0, encodeSeconds = 0.0;
};

namespace ImportSteps
{
    /** MD5 of the file's contents (to notice the same file imported twice). */
    juce::String hashOf (const juce::File&);

    /** High-quality sample-rate conversion of a 2-channel buffer. Splits the
        work over `threads` threads. */
    juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double fromRate, double toRate,
                                       int threads = 4);

    /** Writes the buffer as Ogg Vorbis or FLAC. */
    juce::Result encode (const juce::AudioBuffer<float>& audio, double rate, const ImportSettings&,
                         const juce::File& destination);

    /** All steps: read, resample, encode to `destination`. */
    ImportResult run (const juce::File& source, const juce::File& destination, const ImportSettings&,
                      juce::AudioFormatManager& formats);
}

class Importer
{
public:
    Importer();
    ~Importer();

    /** Converts `source` into `destination` in the background; `done` is called
        on the message thread. */
    void start (const juce::File& source, const juce::File& destination, const ImportSettings&,
                std::function<void (const ImportResult&)> done);

    int getNumActive() const noexcept           { return active.load(); }
    juce::AudioFormatManager& getFormats()      { return formats; }

private:
    juce::AudioFormatManager formats;
    juce::ThreadPool pool;
    std::atomic<int> active { 0 };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace projet
