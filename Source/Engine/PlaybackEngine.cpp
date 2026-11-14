// PlaybackEngine.cpp

#include "PlaybackEngine.h"

namespace projet
{

PlaybackEngine::PlaybackEngine()
{
    KernelBank::buildAll();
    reader.prepare (4.0, KernelBank::get (Quality::High).halfWidth());
    for (auto& s : scratch) s.assign (32768, 0.0f);
}

PlaybackEngine::~PlaybackEngine() = default;

void PlaybackEngine::setArrangement (std::shared_ptr<const Arrangement> a)
{
    if (current != nullptr)
        retired.push_back ({ current, juce::Time::getMillisecondCounterHiRes() });
    current = std::move (a);
    live.store (current.get());
}

void PlaybackEngine::purgeRetired()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (const Retired& r) { return now - r.time > 3000.0; }),
                   retired.end());
}

void PlaybackEngine::locate (double seconds)
{
    pendingLocate.store (juce::jmax (0.0, seconds));
    locateCounter.fetch_add (1);
    uiSeconds.store (juce::jmax (0.0, seconds));
}

void PlaybackEngine::prepare (double sampleRate, int maximumBlockSize)
{
    juce::ignoreUnused (maximumBlockSize);
    deviceRate.store (sampleRate);
}

void PlaybackEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void PlaybackEngine::audioDeviceStopped() {}

void PlaybackEngine::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs,
                                                       int numOutputs, int numSamples,
                                                       const juce::AudioIODeviceCallbackContext&)
{
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    process (outputs, numOutputs, numSamples);
    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    const double available = (double) numSamples / juce::jmax (1.0, deviceRate.load());
    uiLoad.store (uiLoad.load() * 0.9 + 0.1 * (used / available));
}

void PlaybackEngine::process (float* const* outputs, int numOutputs, int numSamples) noexcept
{
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputs[ch], numSamples);

    const auto* arr = live.load();
    const double projectRate = arr != nullptr ? arr->sampleRate : 48000.0;

    const auto lc = locateCounter.load();
    if (lc != lastLocate)
    {
        lastLocate = lc;
        position = pendingLocate.load() * projectRate;
    }

    if (! playing.load() || arr == nullptr || numOutputs < 1)
    {
        uiPeak[0].store (0.0f);
        uiPeak[1].store (0.0f);
        return;
    }

    float* l = outputs[0];
    float* r = numOutputs > 1 ? outputs[1] : nullptr;
    const double ratio = projectRate / juce::jmax (1.0, deviceRate.load());

    if (r == nullptr)
    {
        // Mono device: process in small chunks into a stack buffer for the right channel.
        float right[256];
        for (int done = 0; done < numSamples;)
        {
            const int n = juce::jmin (256, numSamples - done);
            juce::FloatVectorOperations::clear (right, n);
            if (std::abs (ratio - 1.0) < 1.0e-12) processExact (*arr, l + done, right, n);
            else                                  processResampled (*arr, l + done, right, n, ratio);
            juce::FloatVectorOperations::add (l + done, right, n);
            juce::FloatVectorOperations::multiply (l + done, 0.5f, n);
            done += n;
        }
    }
    else if (std::abs (ratio - 1.0) < 1.0e-12)
        processExact (*arr, l, r, numSamples);
    else
        processResampled (*arr, l, r, numSamples, ratio);

    uiSeconds.store (position / projectRate);
    auto peakOf = [numSamples] (const float* x)
    {
        auto range = juce::FloatVectorOperations::findMinAndMax (x, numSamples);
        return juce::jmax (range.getEnd(), -range.getStart());
    };
    uiPeak[0].store (peakOf (l));
    uiPeak[1].store (r != nullptr ? peakOf (r) : uiPeak[0].load());
}

void PlaybackEngine::mix (const Arrangement& arr, int64_t from, int n, float* l, float* r) noexcept
{
    const int64_t to = from + n;

    for (const auto& c : arr.clips)
    {
        if (c.playFrom >= to) break;                    // sorted by playFrom
        if (c.playTo <= from) continue;

        // Only what sounds: inside the clip and the containers around it.
        const int64_t a = juce::jmax (c.playFrom, from);
        const int64_t b = juce::jmin (c.playTo, to);
        const float* srcL = c.sound->audio.getReadPointer (0);
        const float* srcR = c.sound->audio.getReadPointer (1);
        const bool flat = c.fadeIn == 0 && c.fadeOut == 0 && c.windows.empty();

        for (int64_t s = a; s < b; ++s)
        {
            const int64_t i = s - c.start;              // sample within the clip
            const int64_t src = c.offset + i;
            const float g = flat ? c.gain : c.gainAtTime (s);
            const auto k = (size_t) (s - from);
            l[k] += srcL[src] * g;
            r[k] += srcR[src] * g;
        }
    }
}

void PlaybackEngine::processExact (const Arrangement& arr, float* l, float* r, int numSamples) noexcept
{
    const auto blockStart = (int64_t) std::llround (position);
    mix (arr, blockStart, numSamples, l, r);
    position = (double) (blockStart + numSamples);
}

void PlaybackEngine::processResampled (const Arrangement& arr, float* l, float* r, int numSamples, double ratio) noexcept
{
    const auto& kernel = KernelBank::get (Quality::High);
    const int margin = (int) std::ceil ((double) kernel.halfWidth() * juce::jmax (1.0, ratio)) + 2;
    const int capacity = (int) scratch[0].size();
    const int maxChunk = juce::jmax (1, (int) ((double) (capacity - 2 * margin - 4) / ratio));

    for (int done = 0; done < numSamples;)
    {
        const int n = juce::jmin (maxChunk, numSamples - done);
        const double first = position;
        const auto from = (int64_t) std::floor (first) - margin;
        const int span = (int) ((int64_t) std::ceil (first + ratio * (double) n) + margin - from);

        float* sl = scratch[0].data();
        float* sr = scratch[1].data();
        juce::FloatVectorOperations::clear (sl, span);
        juce::FloatVectorOperations::clear (sr, span);
        mix (arr, from, span, sl, sr);

        const float* data[2] = { sl, sr };
        double v[2];
        for (int k = 0; k < n; ++k)
        {
            reader.read (kernel, data, 2, span, first + ratio * (double) k - (double) from, ratio, v);
            l[done + k] += (float) v[0];
            r[done + k] += (float) v[1];
        }

        position = first + ratio * (double) n;
        done += n;
    }
}

} // namespace projet
