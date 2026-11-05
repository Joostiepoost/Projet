// Importer.cpp

#include "Importer.h"
#include "SoundData.h"
#include "SincKernel.h"
#include "SincReader.h"

#include <thread>
#include <vector>

namespace projet
{

namespace ImportSteps
{

juce::String hashOf (const juce::File& f)
{
    return juce::MD5 (f).toHexString();
}

juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double fromRate, double toRate, int threads)
{
    const int64_t inLength = in.getNumSamples();
    const double ratio = fromRate / toRate;                         // source samples per output sample
    const auto outLength = (int64_t) std::ceil ((double) inLength / ratio);

    juce::AudioBuffer<float> out (2, (int) outLength);
    const auto& kernel = KernelBank::get (Quality::High);
    const float* src[2] = { in.getReadPointer (0), in.getReadPointer (1) };

    threads = juce::jlimit (1, 16, threads);
    const int64_t chunk = (outLength + threads - 1) / threads;
    std::vector<std::thread> workers;

    for (int t = 0; t < threads; ++t)
    {
        const int64_t begin = t * chunk, end = juce::jmin (outLength, begin + chunk);
        if (begin >= end) break;

        workers.emplace_back ([&, begin, end]
        {
            SincReader reader;
            reader.prepare (ratio, kernel.halfWidth());
            float* dst[2] = { out.getWritePointer (0), out.getWritePointer (1) };
            double v[2];
            for (int64_t k = begin; k < end; ++k)
            {
                reader.read (kernel, src, 2, inLength, (double) k * ratio, ratio, v);
                dst[0][k] = (float) v[0];
                dst[1][k] = (float) v[1];
            }
        });
    }

    for (auto& w : workers) w.join();
    return out;
}

juce::Result encode (const juce::AudioBuffer<float>& audio, double rate, const ImportSettings& settings,
                     const juce::File& destination)
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
        if (! writer->writeFromAudioSampleBuffer (audio, pos, juce::jmin (65536, n - pos)))
        {
            writer = nullptr;
            destination.deleteFile();
            return juce::Result::fail ("Writing " + destination.getFileName() + " failed (disk full?).");
        }

    writer = nullptr;   // flushes and closes the file
    return juce::Result::ok();
}

ImportResult run (const juce::File& source, const juce::File& destination, const ImportSettings& settings,
                  juce::AudioFormatManager& formats)
{
    ImportResult r;
    r.original = source;
    r.placeholder = destination;

    auto t0 = juce::Time::getMillisecondCounterHiRes();
    r.hash = hashOf (source);

    juce::AudioBuffer<float> audio;
    if (! readWholeFile (source, formats, audio, r.originalRate, r.originalChannels, r.error))
        return r;
    auto t1 = juce::Time::getMillisecondCounterHiRes();
    r.readSeconds = (t1 - t0) * 0.001;

    if (std::abs (r.originalRate - settings.targetRate) > 0.01)
        audio = resample (audio, r.originalRate, settings.targetRate,
                          (int) juce::jmax (1u, std::thread::hardware_concurrency() / 2));
    auto t2 = juce::Time::getMillisecondCounterHiRes();
    r.resampleSeconds = (t2 - t1) * 0.001;

    const auto result = encode (audio, settings.targetRate, settings, destination);
    if (result.failed()) { r.error = result.getErrorMessage(); return r; }
    r.encodeSeconds = (juce::Time::getMillisecondCounterHiRes() - t2) * 0.001;

    r.lengthSamples = audio.getNumSamples();
    r.ok = true;
    return r;
}

} // namespace ImportSteps

//==============================================================================

Importer::Importer()
    : pool (juce::ThreadPoolOptions{}.withThreadName ("Projet import")
                                      .withNumberOfThreads (juce::jlimit (1, 4, (int) std::thread::hardware_concurrency() / 2)))
{
    formats.registerBasicFormats();
}

Importer::~Importer()
{
    *alive = false;
    pool.removeAllJobs (true, 30000);
}

void Importer::start (const juce::File& source, const juce::File& destination, const ImportSettings& settings,
                      std::function<void (const ImportResult&)> done)
{
    ++active;
    auto aliveFlag = alive;
    pool.addJob ([this, aliveFlag, source, destination, settings, done = std::move (done)]
    {
        auto result = ImportSteps::run (source, destination, settings, formats);
        juce::MessageManager::callAsync ([this, aliveFlag, result, done]
        {
            if (! *aliveFlag) return;
            --active;
            if (done) done (result);
        });
    });
}

} // namespace projet
