// Previewer.cpp — see Previewer.h.

#include "Previewer.h"

namespace projet
{

void Previewer::play (std::shared_ptr<const SoundData> sound, double from, double to, float gain, double fadeIn, double fadeOut, const juce::String& t)
{
    if (sound == nullptr || sound->length() <= 0) return;
    auto job = std::make_shared<Job>();
    const double rate = sound->sampleRate;
    job->sound = std::move (sound);
    job->from = juce::jlimit (0.0, (double) job->sound->length(), from * rate);
    job->to = juce::jlimit (job->from, (double) job->sound->length(), to * rate);
    job->gain = gain;
    job->fadeIn = juce::jmax (0.0, fadeIn * rate);
    job->fadeOut = juce::jmax (0.0, fadeOut * rate);
    if (job->to - job->from < 1.0) return;
    {
        const juce::ScopedLock sl (tagLock);
        tag = t;
    }
    std::shared_ptr<Job> old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (pending);
        pending = std::move (job);
        newJob = true;
        stopRequested = false;
    }
    uiSeconds.store (from);
    playingNow.store (true);
}

void Previewer::stop()
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        if (newJob) { pending.reset(); newJob = false; }
        stopRequested = true;
    }
    playingNow.store (false);
}

juce::String Previewer::getTag() const
{
    const juce::ScopedLock sl (tagLock);
    return playingNow.load() ? tag : juce::String();
}

void Previewer::purge()
{
    std::shared_ptr<Job> done;
    const juce::SpinLock::ScopedLockType sl (lock);
    done = std::move (finished);
}

void Previewer::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr) deviceRate.store (device->getCurrentSampleRate());
}

void Previewer::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOutputs,
                                                  int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOutputs; ++c)
        if (outputs[c] != nullptr) juce::FloatVectorOperations::clear (outputs[c], numSamples);

    {
        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (tl.isLocked())
        {
            if (newJob)
            {
                if (current != nullptr) finished = std::move (current);
                current = std::move (pending);
                newJob = false;
                stopping = false;
                stopFade = 1.0f;
                position = current != nullptr ? current->from : 0.0;
            }
            if (stopRequested)
            {
                stopRequested = false;
                if (current != nullptr) stopping = true;
            }
        }
    }
    if (current == nullptr) return;

    const auto& job = *current;
    const auto& audio = job.sound->audio;
    const double step = job.sound->sampleRate / juce::jmax (1.0, deviceRate.load());
    const float fadeStep = 1.0f / (float) juce::jmax (1.0, 0.01 * deviceRate.load());
    const int last = audio.getNumSamples() - 1;
    float* left = numOutputs > 0 ? outputs[0] : nullptr;
    float* right = numOutputs > 1 ? outputs[1] : nullptr;
    bool done = false;
    for (int i = 0; i < numSamples; ++i)
    {
        if (position >= job.to || position >= (double) last) { done = true; break; }
        const auto index = (int) position;
        const auto frac = (float) (position - index);
        const int next = juce::jmin (last, index + 1);
        float g = job.gain;
        const double fromStart = position - job.from, toEnd = job.to - position;
        if (job.fadeIn > 0.0 && fromStart < job.fadeIn) g *= (float) (fromStart / job.fadeIn);
        if (job.fadeOut > 0.0 && toEnd < job.fadeOut) g *= (float) (toEnd / job.fadeOut);
        if (stopping)
        {
            stopFade -= fadeStep;
            if (stopFade <= 0.0f) { done = true; break; }
            g *= stopFade;
        }
        const float l = audio.getSample (0, index) + frac * (audio.getSample (0, next) - audio.getSample (0, index));
        const float r = audio.getSample (1, index) + frac * (audio.getSample (1, next) - audio.getSample (1, index));
        if (left != nullptr) left[i] = l * g;
        if (right != nullptr) right[i] = r * g;
        position += step;
    }
    uiSeconds.store (position / job.sound->sampleRate);
    if (done)
    {
        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (tl.isLocked() && ! newJob)
        {
            finished = std::move (current);
            playingNow.store (false);
        }
    }
}

} // namespace projet
