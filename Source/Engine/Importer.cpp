// Importer.cpp

#include "Importer.h"
#include "SoundData.h"
#include "SincKernel.h"
#include "SincReader.h"

#include <numeric>
#include <thread>
#include <vector>

namespace projet
{

const char* ImportProgress::phaseName (int p)
{
    switch (p)
    {
        case waiting:    return "Waiting";
        case reading:    return "Reading";
        case converting: return "Converting sample rate";
        case encoding:   return "Encoding";
        case done:       return "Done";
        case failed:     return "Failed";
        case cancelled:  return "Cancelled";
        default:         return "";
    }
}

namespace
{

int allCores()
{
    return juce::jlimit (1, 32, (int) std::thread::hardware_concurrency());
}

/** Runs body(begin, end) over [0, total) on `threads` threads. */
template <typename Body>
void parallelFor (int64_t total, int threads, Body body)
{
    threads = juce::jlimit (1, 32, threads);
    const int64_t chunk = (total + threads - 1) / threads;
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t)
    {
        const int64_t begin = t * chunk, end = juce::jmin (total, begin + chunk);
        if (begin >= end) break;
        workers.emplace_back ([=] { body (begin, end); });
    }
    for (auto& w : workers) w.join();
}

/** The rate ratio as from/to = M/L in lowest terms, if both rates are whole
    numbers of Hz and L is small enough for a table (all usual rates are). */
bool rationalRatio (double fromRate, double toRate, int64_t& M, int64_t& L)
{
    const auto f = (int64_t) std::llround (fromRate), t = (int64_t) std::llround (toRate);
    if (std::abs ((double) f - fromRate) > 1.0e-6 || std::abs ((double) t - toRate) > 1.0e-6 || f <= 0 || t <= 0)
        return false;
    const auto g = std::gcd (f, t);
    M = f / g;
    L = t / g;
    return L <= 4000;
}

} // namespace

namespace ImportSteps
{

juce::String hashOf (const juce::File& f)
{
    return juce::MD5 (f).toHexString();
}

juce::AudioBuffer<float> resampleReference (const juce::AudioBuffer<float>& in, double fromRate, double toRate)
{
    const int64_t inLength = in.getNumSamples();
    const double ratio = fromRate / toRate;
    const auto outLength = (int64_t) std::ceil ((double) inLength / ratio);
    juce::AudioBuffer<float> out (2, (int) outLength);
    const auto& kernel = KernelBank::get (Quality::High);
    const float* src[2] = { in.getReadPointer (0), in.getReadPointer (1) };

    parallelFor (outLength, allCores(), [&] (int64_t begin, int64_t end)
    {
        SincReader reader;
        reader.prepare (ratio, kernel.halfWidth());
        double v[2];
        for (int64_t k = begin; k < end; ++k)
        {
            reader.read (kernel, src, 2, inLength, (double) k * ratio, ratio, v);
            out.setSample (0, (int) k, (float) v[0]);
            out.setSample (1, (int) k, (float) v[1]);
        }
    });
    return out;
}

juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double fromRate, double toRate, int threads,
                                   std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    int64_t M = 0, L = 0;
    if (! rationalRatio (fromRate, toRate, M, L))
        return resampleReference (in, fromRate, toRate);

    // Output sample k sits at input position p = k * M / L: base n0 = floor (k M / L),
    // phase phi = (k M) mod L, fraction phi / L. The same kernel as SincReader,
    // h(t) = g(c |t|) with c = min (1, L / M), normalised per phase.
    const auto& kernel = KernelBank::get (Quality::High);
    const double c = juce::jmin (1.0, (double) L / (double) M);
    const int W = (int) std::ceil ((double) kernel.halfWidth() / c);     // half support, input samples
    const int taps = 2 * W;

    std::vector<float> table ((size_t) (L * taps));
    for (int64_t phi = 0; phi < L; ++phi)
    {
        const double f = (double) phi / (double) L;
        double sum = 0.0;
        std::vector<double> w ((size_t) taps);
        for (int j = 0; j < taps; ++j)
        {
            const int d = j - (W - 1);                 // input sample n0 + d
            const double t = std::abs (f - (double) d);
            w[(size_t) j] = kernel.evaluate (c * t);
            sum += w[(size_t) j];
        }
        for (int j = 0; j < taps; ++j)
            table[(size_t) (phi * taps + j)] = (float) (w[(size_t) j] / sum);
    }

    const int64_t inLength = in.getNumSamples();
    const auto outLength = (int64_t) std::ceil ((double) inLength * (double) L / (double) M);
    juce::AudioBuffer<float> out (2, (int) outLength);
    std::atomic<int64_t> produced { 0 };
    std::atomic<bool> stopped { false };

    parallelFor (outLength, threads, [&] (int64_t begin, int64_t end)
    {
        const float* x[2] = { in.getReadPointer (0), in.getReadPointer (1) };
        float* y[2] = { out.getWritePointer (0), out.getWritePointer (1) };
        int64_t sinceReport = 0;

        for (int64_t k = begin; k < end; ++k)
        {
            const int64_t num = k * M;
            const int64_t n0 = num / L;
            const float* w = table.data() + (size_t) ((num % L) * taps);
            const int64_t first = n0 - (W - 1);
            const int j0 = (int) juce::jmax ((int64_t) 0, -first);
            const int j1 = (int) juce::jmin ((int64_t) taps, inLength - first);

            for (int ch = 0; ch < 2; ++ch)
            {
                const float* xs = x[ch] + first;
                float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
                int j = j0;
                for (; j + 4 <= j1; j += 4)
                {
                    a0 += w[j] * xs[j];
                    a1 += w[j + 1] * xs[j + 1];
                    a2 += w[j + 2] * xs[j + 2];
                    a3 += w[j + 3] * xs[j + 3];
                }
                for (; j < j1; ++j) a0 += w[j] * xs[j];
                y[ch][k] = (a0 + a1) + (a2 + a3);
            }

            if (++sinceReport == 8192)
            {
                if (progress != nullptr)
                    progress->store ((float) (produced.fetch_add (sinceReport) + sinceReport) / (float) outLength);
                sinceReport = 0;
                if (cancel != nullptr && cancel->load()) { stopped = true; return; }
            }
        }
    });

    if (stopped.load()) return {};
    return out;
}

juce::Result encode (const juce::AudioBuffer<float>& audio, double rate, const ImportSettings& settings,
                     const juce::File& destination, ImportProgress* progress)
{
    std::unique_ptr<juce::AudioFormat> format;
    auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2);

    if (settings.format == "flac")
    {
        format = std::make_unique<juce::FlacAudioFormat>();
        options = options.withBitsPerSample (24).withQualityOptionIndex (5);
    }
    else
    {
        format = std::make_unique<juce::OggVorbisAudioFormat>();
        options = options.withBitsPerSample (16).withQualityOptionIndex (juce::jlimit (0, 10, settings.vorbisQuality));
    }

    destination.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = destination.createOutputStream();
    if (stream == nullptr)
        return juce::Result::fail ("Could not write " + destination.getFullPathName() + ".");

    auto writer = format->createWriterFor (stream, options);
    if (writer == nullptr)
    {
        stream = nullptr;
        destination.deleteFile();
        return juce::Result::fail ("The encoder refused " + juce::String (rate, 0) + " Hz.");
    }

    const int n = audio.getNumSamples();
    for (int pos = 0; pos < n; pos += 65536)
    {
        if (progress != nullptr && progress->cancel.load())
        {
            writer = nullptr;
            destination.deleteFile();
            return juce::Result::fail ("Cancelled");
        }
        if (! writer->writeFromAudioSampleBuffer (audio, pos, juce::jmin (65536, n - pos)))
        {
            writer = nullptr;
            destination.deleteFile();
            return juce::Result::fail ("Writing " + destination.getFileName() + " failed (disk full?).");
        }
        if (progress != nullptr)
            progress->fraction.store ((float) juce::jmin (n, pos + 65536) / (float) juce::jmax (1, n));
    }

    writer = nullptr;   // flushes and closes the file
    return juce::Result::ok();
}

ImportResult run (const juce::File& source, const juce::File& destination, const ImportSettings& settings,
                  juce::AudioFormatManager& formats, ImportProgress* progress)
{
    ImportResult r;
    r.original = source;
    r.placeholder = destination;

    auto setPhase = [progress] (int p) { if (progress != nullptr) { progress->fraction.store (0.0f); progress->phase.store (p); } };
    auto fail = [&] (const juce::String& error)
    {
        r.error = error;
        r.cancelled = progress != nullptr && progress->cancel.load();
        setPhase (r.cancelled ? ImportProgress::cancelled : ImportProgress::failed);
        destination.deleteFile();
        return r;
    };

    auto t0 = juce::Time::getMillisecondCounterHiRes();
    setPhase (ImportProgress::reading);
    r.hash = hashOf (source);

    juce::AudioBuffer<float> audio;
    auto readProgress = [progress] (float f)
    {
        if (progress == nullptr) return true;
        progress->fraction.store (f);
        return ! progress->cancel.load();
    };
    juce::String error;
    if (! readWholeFile (source, formats, audio, r.originalRate, r.originalChannels, error, readProgress))
        return fail (error);
    auto t1 = juce::Time::getMillisecondCounterHiRes();
    r.readSeconds = (t1 - t0) * 0.001;

    if (std::abs (r.originalRate - settings.targetRate) > 0.01)
    {
        setPhase (ImportProgress::converting);
        audio = resample (audio, r.originalRate, settings.targetRate, allCores(),
                          progress != nullptr ? &progress->fraction : nullptr,
                          progress != nullptr ? &progress->cancel : nullptr);
        if (audio.getNumSamples() == 0) return fail ("Cancelled");
    }
    auto t2 = juce::Time::getMillisecondCounterHiRes();
    r.resampleSeconds = (t2 - t1) * 0.001;

    setPhase (ImportProgress::encoding);
    const auto result = encode (audio, settings.targetRate, settings, destination, progress);
    if (result.failed()) return fail (result.getErrorMessage());
    r.encodeSeconds = (juce::Time::getMillisecondCounterHiRes() - t2) * 0.001;

    r.lengthSamples = audio.getNumSamples();
    r.ok = true;
    if (progress != nullptr) { progress->fraction.store (1.0f); progress->phase.store (ImportProgress::done); }
    return r;
}

} // namespace ImportSteps

//==============================================================================

Importer::Importer()
    : pool (juce::ThreadPoolOptions{}.withThreadName ("Projet import").withNumberOfThreads (2))
{
    formats.registerBasicFormats();
}

Importer::~Importer()
{
    *alive = false;
    pool.removeAllJobs (true, 30000);
}

std::shared_ptr<ImportProgress> Importer::start (const juce::File& source, const juce::File& destination,
                                                 const ImportSettings& settings,
                                                 std::function<void (const ImportResult&)> done)
{
    auto progress = std::make_shared<ImportProgress>();
    progress->name = source.getFileName();
    ++active;
    auto aliveFlag = alive;
    pool.addJob ([this, aliveFlag, source, destination, settings, progress, done = std::move (done)]
    {
        auto result = progress->cancel.load() ? ImportResult {} : ImportSteps::run (source, destination, settings, formats, progress.get());
        if (! result.ok && result.error.isEmpty())
        {
            result.original = source;
            result.placeholder = destination;
            result.cancelled = true;
            result.error = "Cancelled";
            progress->phase.store (ImportProgress::cancelled);
            destination.deleteFile();
        }
        juce::MessageManager::callAsync ([this, aliveFlag, result, done]
        {
            if (! *aliveFlag) return;
            --active;
            if (done) done (result);
        });
    });
    return progress;
}

} // namespace projet
