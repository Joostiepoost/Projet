// Importer.h — turns an imported sound into a compact placeholder copy.
//
//   1. read the whole file (WAV, AIFF, FLAC, Ogg; on macOS also MP3, AAC/M4A, CAF)
//   2. stereo: mono is copied to both channels, more than two keep the first two
//   3. resample to the project rate if needed: Effet's sinc kernel ("High"),
//      applied as a precomputed polyphase filter for rational rate ratios
//      (44.1 -> 48, 96 -> 48, ...), spread over all cores
//   4. encode as Ogg Vorbis (default, quality 0.7) or FLAC 24-bit
//
// The placeholder is only a stand-in for sketching; the path of the original
// is kept so it can be swapped for the production file later.
//
// Importer runs jobs on a pool of threads (one sound per thread), reports
// progress through ImportProgress, can cancel, and calls back on the
// message thread.

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

namespace projet
{

struct ImportSettings
{
    double targetRate = 48000.0;
    juce::String format = "ogg";        // "ogg" or "flac"
    int vorbisQuality = 7;              // 0..10, q = index / 10
};

/** Shared between an import job and the UI. */
struct ImportProgress
{
    enum Phase { waiting = 0, reading, converting, encoding, done, failed, cancelled };

    juce::String name;
    std::atomic<int> phase { waiting };
    std::atomic<float> fraction { 0.0f };     // within the current phase, 0..1
    std::atomic<bool> cancel { false };
    double finishedAt = 0.0;                  // message thread: when it ended (ms)

    bool isFinished() const noexcept          { const int p = phase.load(); return p == done || p == failed || p == cancelled; }
    static const char* phaseName (int);
};

struct ImportResult
{
    bool ok = false, cancelled = false;
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

    /** High-quality sample-rate conversion of a 2-channel buffer, over `threads`
        threads. `progress` (0..1) and `cancel` may be null. Returns an empty
        buffer when cancelled. */
    juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double fromRate, double toRate,
                                       int threads, std::atomic<float>* progress = nullptr,
                                       const std::atomic<bool>* cancel = nullptr);

    /** The same conversion through SincReader at every position (slow; kept
        as the reference the polyphase path is checked against). */
    juce::AudioBuffer<float> resampleReference (const juce::AudioBuffer<float>& in, double fromRate, double toRate);

    /** Writes the buffer as Ogg Vorbis or FLAC. */
    juce::Result encode (const juce::AudioBuffer<float>& audio, double rate, const ImportSettings&,
                         const juce::File& destination, ImportProgress* progress = nullptr);

    /** All steps: read, resample, encode to `destination`. */
    ImportResult run (const juce::File& source, const juce::File& destination, const ImportSettings&,
                      juce::AudioFormatManager& formats, ImportProgress* progress = nullptr);
}

class Importer
{
public:
    Importer();
    ~Importer();

    /** Converts `source` into `destination` in the background; `done` is called
        on the message thread. The returned progress can be shown and cancelled. */
    std::shared_ptr<ImportProgress> start (const juce::File& source, const juce::File& destination,
                                           const ImportSettings&, std::function<void (const ImportResult&)> done);

    int getNumActive() const noexcept           { return active.load(); }
    juce::AudioFormatManager& getFormats()      { return formats; }

private:
    juce::AudioFormatManager formats;
    juce::ThreadPool pool;
    std::atomic<int> active { 0 };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace projet
