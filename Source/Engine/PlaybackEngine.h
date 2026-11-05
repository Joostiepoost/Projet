// PlaybackEngine.h — plays the arrangement, sample-accurately.
//
// One clock: the timeline position, in project samples. When the device runs
// at the project rate (the normal case) every clip is copied sample for
// sample, so a box at 1.000 s sounds exactly at sample 48000. When the device
// runs at another rate, the result goes through Effet's sinc kernel
// ("High") instead: the clips are mixed at the project rate and the mix is
// read at fractional positions.
//
// The message thread hands over a new arrangement (lock-free pointer swap;
// the old one is kept alive for a few seconds) and sends play / stop /
// locate requests. The engine also runs offline for the measurement program.

#pragma once

#include <JuceHeader.h>
#include "Arrangement.h"
#include "SincKernel.h"
#include "SincReader.h"

#include <atomic>
#include <memory>
#include <vector>

namespace projet
{

class PlaybackEngine : public juce::AudioIODeviceCallback
{
public:
    PlaybackEngine();
    ~PlaybackEngine() override;

    //== Message thread ======================================================
    void setArrangement (std::shared_ptr<const Arrangement>);
    std::shared_ptr<const Arrangement> getArrangement() const      { return current; }

    void play()                                                     { playing.store (true); }
    void stop()                                                     { playing.store (false); }
    bool isPlaying() const noexcept                                 { return playing.load(); }

    /** Moves the playhead (seconds on the timeline). */
    void locate (double seconds);

    /** Frees arrangements the audio thread can no longer be using. Call regularly. */
    void purgeRetired();

    //== Read by the UI ======================================================
    double getPositionSeconds() const noexcept                      { return uiSeconds.load(); }
    double getDeviceRate() const noexcept                           { return deviceRate.load(); }
    float getPeak (int channel) const noexcept                      { return uiPeak[channel & 1].load(); }
    /** Share of the block time spent processing (0..1), smoothed. */
    double getLoad() const noexcept                                 { return uiLoad.load(); }

    //== Processing ==========================================================
    void prepare (double sampleRate, int maximumBlockSize);
    void process (float* const* outputs, int numOutputs, int numSamples) noexcept;

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

private:
    /** Adds the clips' samples [from, from + n) (project samples) into l and r. */
    static void mix (const Arrangement&, int64_t from, int n, float* l, float* r) noexcept;
    void processExact (const Arrangement&, float* l, float* r, int numSamples) noexcept;
    void processResampled (const Arrangement&, float* l, float* r, int numSamples, double ratio) noexcept;

    std::shared_ptr<const Arrangement> current;
    std::atomic<const Arrangement*> live { nullptr };
    struct Retired { std::shared_ptr<const Arrangement> object; double time; };
    std::vector<Retired> retired;

    std::atomic<bool> playing { false };
    std::atomic<double> pendingLocate { 0.0 };
    std::atomic<uint32_t> locateCounter { 0 };

    // Audio thread
    std::atomic<double> deviceRate { 48000.0 };
    uint32_t lastLocate = 0;
    double position = 0.0;                    // project samples
    // Other device rate: the clips are mixed at the project rate into scratch,
    // and the mix is read once through the kernel (cost independent of the
    // number of boxes).
    std::vector<float> scratch[2];
    SincReader reader;

    std::atomic<double> uiSeconds { 0.0 }, uiLoad { 0.0 };
    std::atomic<float> uiPeak[2] { { 0.0f }, { 0.0f } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackEngine)
};

} // namespace projet
