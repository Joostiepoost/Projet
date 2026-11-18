// Previewer.h — plays one sound fragment on its own, beside the timeline's
// playback (stage 9b): a sound card heard on the board. A second device
// callback; the device manager mixes it with the engine's.
//
// The message thread hands over a fragment (sound, from, to, gain, fades) or
// a stop; the audio thread reads it at the device rate (linear
// interpolation when the device runs at another rate: it is for listening
// on the board, not for the piece), with a short fade at a stop.

#pragma once

#include <JuceHeader.h>
#include "SoundData.h"

#include <atomic>
#include <memory>

namespace projet
{

class Previewer : public juce::AudioIODeviceCallback
{
public:
    Previewer() = default;
    ~Previewer() override = default;

    /** Message thread. `from` and `to` in seconds into the sound. */
    void play (std::shared_ptr<const SoundData>, double from, double to, float gain, double fadeIn, double fadeOut, const juce::String& tag);
    void stop();
    bool isPlaying() const noexcept                 { return playingNow.load(); }
    /** What plays (the tag given), and where it is (seconds into the sound). */
    juce::String getTag() const;
    double getPositionSeconds() const noexcept      { return uiSeconds.load(); }
    /** Message thread, now and then: forgets a finished fragment. */
    void purge();

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

private:
    struct Job
    {
        std::shared_ptr<const SoundData> sound;
        double from = 0.0, to = 0.0;            // samples of the sound
        float gain = 1.0f;
        double fadeIn = 0.0, fadeOut = 0.0;     // samples
    };
    juce::SpinLock lock;
    std::shared_ptr<Job> pending, current;      // pending: handed over; current: the audio thread's
    bool newJob = false, stopRequested = false;
    std::shared_ptr<Job> finished;              // kept for the message thread to free

    double position = 0.0;                      // samples of the sound (audio thread)
    float stopFade = 1.0f;
    bool stopping = false;
    std::atomic<double> deviceRate { 48000.0 };
    std::atomic<bool> playingNow { false };
    std::atomic<double> uiSeconds { 0.0 };
    juce::String tag;
    mutable juce::CriticalSection tagLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Previewer)
};

} // namespace projet
