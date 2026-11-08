// EngineTests.cpp — Projet's measurement program (./build.sh tests).
//
// Writes Measurements/report.md:
//   1. Import: placement, length and level of the placeholder copies (Ogg
//      Vorbis and FLAC) for sources at 44.1, 48 and 96 kHz.
//   2. Playback timing: boxes land on exactly the right sample, at any block size.
//   3. Locating inside a box.
//   4. Fade shapes and crossfades of overlapping boxes.
//   5. Playback with the device at another rate than the project.
//   6. CPU.
//   7. Import speed and placeholder size (Ogg vs FLAC).
//   8. The fast (polyphase) rate conversion against the reference.
//   9. Proportions (stage 3a): typed expressions, bound boxes following their
//      frames, undo, and track folders.
// --model runs only 9.

#include <JuceHeader.h>
#include "../Source/Engine/Importer.h"
#include "../Source/Engine/SoundData.h"
#include "../Source/Engine/Arrangement.h"
#include "../Source/Engine/PlaybackEngine.h"
#include "../Source/Model/Project.h"
#include "../Source/Model/Proportion.h"

#include <random>
#include <thread>

using namespace projet;

namespace
{

juce::String md;
bool quick = false;
int failures = 0;

void line (const juce::String& s = {})       { md << s << "\n"; }
void check (bool ok, const juce::String& what)
{
    if (! ok) { ++failures; std::cout << "  FAIL: " << what << std::endl; }
}

juce::String fmt (double v, int decimals)    { return juce::String (v, decimals); }

double now()                                  { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

/** Peak position near `expected` (samples), with parabolic interpolation. */
double findPeak (const float* x, int64_t length, double expected, int window)
{
    const auto c = (int64_t) std::llround (expected);
    int64_t best = juce::jlimit ((int64_t) 0, length - 1, c);
    float bestV = -1.0f;
    for (int64_t i = juce::jmax ((int64_t) 0, c - window); i < juce::jmin (length, c + window + 1); ++i)
        if (std::abs (x[i]) > bestV) { bestV = std::abs (x[i]); best = i; }

    if (best <= 0 || best >= length - 1) return (double) best;
    const double a = std::abs (x[best - 1]), b = std::abs (x[best]), d = std::abs (x[best + 1]);
    const double den = a - 2.0 * b + d;
    return (double) best + (std::abs (den) > 1.0e-12 ? 0.5 * (a - d) / den : 0.0);
}

/** A test source: quiet noise, impulses at the given times, a 1 kHz sine from 8 to 9 s. */
juce::AudioBuffer<float> makeSource (double rate, double seconds, const std::vector<double>& impulses)
{
    const int n = (int) std::llround (seconds * rate);
    juce::AudioBuffer<float> b (2, n);
    std::mt19937 rng (1234);
    std::normal_distribution<float> noise (0.0f, 0.001f);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* x = b.getWritePointer (ch);
        for (int i = 0; i < n; ++i) x[i] = noise (rng);
        for (double t : impulses)
        {
            const auto k = (int) std::llround (t * rate);
            if (k < n) x[k] = 0.8f;
        }
        for (int i = (int) (8.0 * rate); i < juce::jmin (n, (int) (9.0 * rate)); ++i)
            x[i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) i / rate);
    }
    return b;
}

bool writeWav (const juce::AudioBuffer<float>& b, double rate, const juce::File& f, int bits = 24)
{
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> out = f.createOutputStream();
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (bits));
    if (w == nullptr) return false;
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return true;
}

double rmsDb (const float* x, int64_t from, int64_t to)
{
    double s = 0.0;
    for (int64_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return 10.0 * std::log10 (s / (double) juce::jmax ((int64_t) 1, to - from) + 1.0e-30);
}

//==============================================================================
void testImport (const juce::File& work, juce::AudioFormatManager& formats)
{
    line ("## 1. Import: placement, length and level");
    line();
    line ("A 10 s test file (quiet noise, single-sample clicks at 1.000, 2.500 and 7.250 s, a 1 kHz sine from 8 to 9 s) "
          "imported into a 48 kHz project. **Click error**: where each click lands in the placeholder, compared with where it "
          "should be, in samples at 48 kHz (largest of the three; parabolic interpolation, so fractions are meaningful). "
          "**Length**: placeholder length minus the expected length. **Level**: change of the sine's level.");
    line();
    line ("| Source rate | Placeholder | Click error (samples) | Length (samples) | Level (dB) |");
    line ("| --- | --- | --- | --- | --- |");

    const std::vector<double> clicks { 1.0, 2.5, 7.25 };
    const double target = 48000.0;

    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto source = makeSource (rate, 10.0, clicks);
        auto wav = work.getChildFile ("source " + juce::String ((int) rate) + ".wav");
        writeWav (source, rate, wav);

        for (auto format : { juce::String ("ogg"), juce::String ("flac") })
        {
            ImportSettings settings;
            settings.targetRate = target;
            settings.format = format;
            auto dest = work.getChildFile ("placeholder " + juce::String ((int) rate) + "." + format);
            auto r = ImportSteps::run (wav, dest, settings, formats);
            check (r.ok, "import " + dest.getFileName() + ": " + r.error);
            if (! r.ok) continue;

            juce::String err;
            auto data = SoundData::load (dest, "x", formats, err);
            check (data != nullptr, "load " + dest.getFileName());
            if (data == nullptr) continue;

            double worst = 0.0;
            for (double t : clicks)
                worst = juce::jmax (worst, std::abs (findPeak (data->audio.getReadPointer (0), data->length(), t * target, 64) - t * target));

            const auto expectedLength = (int64_t) std::ceil (10.0 * target);
            const auto lengthError = data->length() - expectedLength;

            const double levelIn = rmsDb (source.getReadPointer (0), (int64_t) (8.2 * rate), (int64_t) (8.8 * rate));
            const double levelOut = rmsDb (data->audio.getReadPointer (0), (int64_t) (8.2 * target), (int64_t) (8.8 * target));

            line ("| " + fmt (rate / 1000.0, 1) + " kHz | " + (format == "ogg" ? "Ogg Vorbis q0.7" : "FLAC 24-bit") + " | "
                  + fmt (worst, 3) + " | " + juce::String (lengthError) + " | " + fmt (levelOut - levelIn, 3) + " |");

            check (worst < 1.0, "click placement " + dest.getFileName());
            check (lengthError == 0, "length " + dest.getFileName());
            check (std::abs (levelOut - levelIn) < 0.1, "level " + dest.getFileName());
        }
    }
    line();
}

//==============================================================================
/** Runs the engine offline and returns the output. */
juce::AudioBuffer<float> render (PlaybackEngine& engine, double deviceRate, double seconds, int block, double from = 0.0)
{
    engine.prepare (deviceRate, block);
    engine.locate (from);
    engine.play();
    const int n = (int) std::llround (seconds * deviceRate);
    juce::AudioBuffer<float> out (2, n);
    out.clear();
    for (int pos = 0; pos < n; pos += block)
    {
        const int num = juce::jmin (block, n - pos);
        float* ch[2] = { out.getWritePointer (0, pos), out.getWritePointer (1, pos) };
        engine.process (ch, 2, num);
    }
    engine.stop();
    return out;
}

std::shared_ptr<const SoundData> impulseSound (double rate, double seconds)
{
    juce::AudioBuffer<float> b (2, (int) std::llround (seconds * rate));
    b.clear();
    b.setSample (0, 0, 1.0f);
    b.setSample (1, 0, 1.0f);
    return SoundData::fromBuffer (std::move (b), rate, "impulse");
}

std::shared_ptr<const SoundData> constantSound (double rate, double seconds, float value = 1.0f)
{
    juce::AudioBuffer<float> b (2, (int) std::llround (seconds * rate));
    for (int ch = 0; ch < 2; ++ch) juce::FloatVectorOperations::fill (b.getWritePointer (ch), value, b.getNumSamples());
    return SoundData::fromBuffer (std::move (b), rate, "dc");
}

std::shared_ptr<const SoundData> rampSound (double rate, double seconds)
{
    const int n = (int) std::llround (seconds * rate);
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i) { b.setSample (0, i, (float) i / (float) n); b.setSample (1, i, (float) i / (float) n); }
    return SoundData::fromBuffer (std::move (b), rate, "ramp");
}

Clip makeClip (std::shared_ptr<const SoundData> s, double startSeconds, double rate)
{
    Clip c;
    c.sound = s;
    c.start = std::llround (startSeconds * rate);
    c.length = s->length();
    return c;
}

std::shared_ptr<const Arrangement> arrangementOf (std::vector<Clip> clips, double rate)
{
    auto a = std::make_shared<Arrangement>();
    a->sampleRate = rate;
    a->clips = std::move (clips);
    std::sort (a->clips.begin(), a->clips.end(), [] (const Clip& x, const Clip& y) { return x.start < y.start; });
    for (auto& c : a->clips) a->end = juce::jmax (a->end, c.end());
    return a;
}

void testTiming()
{
    line ("## 2. Playback timing");
    line();
    line ("Boxes holding a single click, placed at 1.000, 2.500, 3.333 333 and 5.000 021 s in a 48 kHz project, played "
          "with the device at 48 kHz. **Error**: largest distance between where a click sounds and the sample its box "
          "starts on (round(start x 48000)).");
    line();
    line ("| Block size | Error (samples) |");
    line ("| --- | --- |");

    const double rate = 48000.0;
    auto click = impulseSound (rate, 0.1);
    const std::vector<double> starts { 1.0, 2.5, 3.333333, 5.000021 };
    std::vector<Clip> clips;
    for (double t : starts) clips.push_back (makeClip (click, t, rate));

    PlaybackEngine engine;
    engine.setArrangement (arrangementOf (clips, rate));

    for (int block : { 32, 64, 333, 512, 4096 })
    {
        auto out = render (engine, rate, 6.0, block);
        int64_t worst = 0;
        for (double t : starts)
        {
            const auto expected = std::llround (t * rate);
            int64_t found = -1;
            for (int64_t i = expected - 200; i < expected + 200; ++i)
                if (out.getSample (0, (int) i) > 0.5f) { found = i; break; }
            worst = juce::jmax (worst, found < 0 ? (int64_t) 999999 : (int64_t) std::abs (found - expected));
        }
        line ("| " + juce::String (block) + " | " + juce::String (worst) + " |");
        check (worst == 0, "timing at block " + juce::String (block));
    }
    line();
}

void testLocate()
{
    line ("## 3. Locating inside a box");
    line();
    line ("A box holding a ramp (each sample's value is its index) starts at 1.000 s. Playback starts at a position inside "
          "it; the first output sample must be the ramp's sample at that position.");
    line();
    line ("| Start position (s) | Expected sample | Played sample | Match |");
    line ("| --- | --- | --- | --- |");

    const double rate = 48000.0;
    auto ramp = rampSound (rate, 4.0);
    PlaybackEngine engine;
    engine.setArrangement (arrangementOf ({ makeClip (ramp, 1.0, rate) }, rate));

    for (double from : { 1.0, 1.2345, 2.5, 4.99997 })
    {
        auto out = render (engine, rate, 0.01, 512, from);
        const auto expectedIndex = std::llround (from * rate) - std::llround (1.0 * rate);
        const float expected = (float) expectedIndex / (float) ramp->length();
        const float got = out.getSample (0, 0);
        const auto playedIndex = std::llround ((double) got * (double) ramp->length());
        const bool ok = std::abs (got - expected) < 1.0e-6f;
        line ("| " + fmt (from, 5) + " | " + juce::String (expectedIndex) + " | " + juce::String (playedIndex) + " | " + (ok ? "yes" : "no") + " |");
        check (ok, "locate " + fmt (from, 5));
    }
    line();
}

void testFades()
{
    line ("## 4. Fades and crossfades");
    line();
    line ("A box of constant level 1.0 with a 100 ms fade in and out. Gain halfway through the fade (expected: linear 0.5, "
          "equal power 0.707, S-curve 0.5), at the last sample of the fade in (expected 1 within a sample's step) and at "
          "the last sample of the box (expected 0).");
    line();
    line ("| Shape | Halfway | End of fade in | Last sample |");
    line ("| --- | --- | --- | --- |");

    const double rate = 48000.0;
    auto dc = constantSound (rate, 1.0);

    for (auto shape : { FadeShape::linear, FadeShape::equalPower, FadeShape::sCurve })
    {
        auto c = makeClip (dc, 0.5, rate);
        c.fadeIn = c.fadeOut = (int64_t) (0.1 * rate);
        c.inShape = c.outShape = shape;
        PlaybackEngine engine;
        engine.setArrangement (arrangementOf ({ c }, rate));
        auto out = render (engine, rate, 2.0, 512);
        const int s = (int) c.start;
        const float half = out.getSample (0, s + (int) (c.fadeIn / 2));
        const float endIn = out.getSample (0, s + (int) c.fadeIn - 1);
        const float last = out.getSample (0, s + (int) c.length - 1);
        line ("| " + juce::String (fadeShapeName (shape)) + " | " + fmt (half, 4) + " | " + fmt (endIn, 4) + " | " + fmt (last, 4) + " |");
        const double expectHalf = shape == FadeShape::equalPower ? std::sqrt (0.5) : 0.5;
        check (std::abs (half - expectHalf) < 0.001, "fade halfway " + juce::String (fadeShapeName (shape)));
        check (endIn > 0.999f, "fade end");
        check (last == 0.0f, "fade last sample");
    }
    line();

    line ("Two overlapping boxes with a 500 ms overlap, the first fading out while the second fades in over the "
          "overlap. Equal power keeps the summed *power* constant (right for unrelated sounds); linear and S-curve keep "
          "the summed *level* constant (right for identical sounds). Largest deviation inside the overlap:");
    line();
    line ("| Shape | Summed level (min / max) | Summed power (min / max) |");
    line ("| --- | --- | --- |");

    for (auto shape : { FadeShape::linear, FadeShape::equalPower, FadeShape::sCurve })
    {
        auto a = makeClip (dc, 0.0, rate);
        auto b = makeClip (dc, 0.5, rate);
        a.fadeOut = b.fadeIn = (int64_t) (0.5 * rate);
        a.outShape = b.inShape = shape;
        double minL = 9, maxL = -9, minP = 9, maxP = -9;
        for (int64_t i = b.start; i < a.end(); ++i)
        {
            const double ga = a.gainAt ((double) (i - a.start)), gb = b.gainAt ((double) (i - b.start));
            minL = juce::jmin (minL, ga + gb); maxL = juce::jmax (maxL, ga + gb);
            minP = juce::jmin (minP, ga * ga + gb * gb); maxP = juce::jmax (maxP, ga * ga + gb * gb);
        }
        line ("| " + juce::String (fadeShapeName (shape)) + " | " + fmt (minL, 4) + " / " + fmt (maxL, 4) + " | "
              + fmt (minP, 4) + " / " + fmt (maxP, 4) + " |");
    }
    line();
}

void testResampledPlayback()
{
    line ("## 5. Device at another rate than the project");
    line();
    line ("A 48 kHz project played on a device at 44.1 or 96 kHz goes through the sinc kernel (\"High\"). A click box at "
          "1.000 s must sound at 1.000 s of device time; a 1 kHz sine must keep its level.");
    line();
    line ("| Device rate | Click error (device samples) | Sine level change (dB) |");
    line ("| --- | --- | --- |");

    const double rate = 48000.0;
    auto click = impulseSound (rate, 0.1);
    const int n = (int) (2.0 * rate);
    juce::AudioBuffer<float> sine (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) i / rate);
        sine.setSample (0, i, v); sine.setSample (1, i, v);
    }
    auto sineSound = SoundData::fromBuffer (std::move (sine), rate, "sine");

    for (double device : { 44100.0, 96000.0 })
    {
        PlaybackEngine engine;
        engine.setArrangement (arrangementOf ({ makeClip (click, 1.0, rate), makeClip (sineSound, 2.0, rate) }, rate));
        auto out = render (engine, device, 4.5, 512);
        const double pos = findPeak (out.getReadPointer (0), out.getNumSamples(), 1.0 * device, 200);
        const double level = rmsDb (out.getReadPointer (0), (int64_t) (2.5 * device), (int64_t) (3.5 * device));
        const double expected = 20.0 * std::log10 (0.5 / std::sqrt (2.0));
        line ("| " + fmt (device / 1000.0, 1) + " kHz | " + fmt (pos - 1.0 * device, 3) + " | " + fmt (level - expected, 3) + " |");
        check (std::abs (pos - device) < 0.5, "resampled click");
        check (std::abs (level - expected) < 0.05, "resampled level");
    }
    line();
}

void testCpu()
{
    line ("## 6. CPU");
    line();
    line ("Time to produce 1 s of stereo output, as a share of one core of this machine.");
    line();
    line ("| Case | Load |");
    line ("| --- | --- |");

    const double rate = 48000.0;
    auto dc = constantSound (rate, 12.0, 0.01f);

    auto measure = [&] (int clips, double device, bool fades)
    {
        std::vector<Clip> cs;
        for (int i = 0; i < clips; ++i)
        {
            auto c = makeClip (dc, 0.0, rate);
            if (fades) { c.fadeIn = c.fadeOut = (int64_t) (5.0 * rate); }
            cs.push_back (c);
        }
        PlaybackEngine engine;
        engine.setArrangement (arrangementOf (cs, rate));
        const double seconds = quick ? 2.0 : 10.0;
        const double t0 = now();
        render (engine, device, seconds, 512);
        return (now() - t0) / seconds;
    };

    line ("| 8 overlapping boxes, device = project rate | " + fmt (100.0 * measure (8, rate, false), 2) + "% |");
    line ("| 32 overlapping boxes, device = project rate | " + fmt (100.0 * measure (32, rate, false), 2) + "% |");
    line ("| 32 overlapping boxes in long fades, device = project rate | " + fmt (100.0 * measure (32, rate, true), 2) + "% |");
    line ("| 8 overlapping boxes, device 44.1 kHz (resampled) | " + fmt (100.0 * measure (8, 44100.0, false), 2) + "% |");
    line ("| 32 overlapping boxes, device 44.1 kHz (resampled) | " + fmt (100.0 * measure (32, 44100.0, false), 2) + "% |");
    line();
}

void testImportSpeed (const juce::File& work, juce::AudioFormatManager& formats)
{
    const double minutes = quick ? 0.5 : 2.0;
    line ("## 7. Import speed and size");
    line();
    line ("A " + fmt (minutes, 1) + "-minute stereo file of coloured noise, imported into a 48 kHz project. Times for one file "
          "(rate conversion uses all cores); two files import at a time.");
    line();
    line ("| Source | Placeholder | Read | Resample | Encode | Total | x real time | Size per minute |");
    line ("| --- | --- | --- | --- | --- | --- | --- | --- |");

    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const int n = (int) std::llround (minutes * 60.0 * rate);
        juce::AudioBuffer<float> b (2, n);
        std::mt19937 rng (99);
        std::normal_distribution<float> noise (0.0f, 0.1f);
        for (int ch = 0; ch < 2; ++ch)
        {
            float y = 0.0f;
            auto* x = b.getWritePointer (ch);
            for (int i = 0; i < n; ++i) { y = 0.97f * y + noise (rng); x[i] = 0.3f * y; }
        }
        auto wav = work.getChildFile ("speed " + juce::String ((int) rate) + ".wav");
        writeWav (b, rate, wav);
        const double wavMbPerMin = (double) wav.getSize() / 1.0e6 / minutes;

        for (auto format : { juce::String ("ogg"), juce::String ("flac") })
        {
            ImportSettings settings;
            settings.format = format;
            auto dest = work.getChildFile ("speed " + juce::String ((int) rate) + "." + format);
            auto r = ImportSteps::run (wav, dest, settings, formats);
            check (r.ok, "speed import");
            const double total = r.readSeconds + r.resampleSeconds + r.encodeSeconds;
            line ("| " + fmt (rate / 1000.0, 1) + " kHz WAV 24-bit (" + fmt (wavMbPerMin, 1) + " MB/min) | "
                  + (format == "ogg" ? "Ogg Vorbis q0.7" : "FLAC 24-bit") + " | "
                  + fmt (r.readSeconds, 2) + " s | " + fmt (r.resampleSeconds, 2) + " s | " + fmt (r.encodeSeconds, 2) + " s | "
                  + fmt (total, 2) + " s | " + juce::String (juce::roundToInt (minutes * 60.0 / juce::jmax (1.0e-6, total))) + "x | "
                  + fmt ((double) dest.getSize() / 1.0e6 / minutes, 2) + " MB |");
        }
    }
    line();
}

void testPolyphase()
{
    line ("## 8. Fast sample-rate conversion");
    line();
    line ("Imports convert the rate with a precomputed polyphase table built from the same sinc kernel as SincReader, "
          "spread over all cores. Checked against SincReader evaluated at every position: largest difference on 10 s of "
          "noise (away from the edges), and the time each takes.");
    line();
    line ("| Conversion | Largest difference (dB below full scale) | Polyphase | Reference |");
    line ("| --- | --- | --- | --- |");

    std::mt19937 rng (7);
    std::uniform_real_distribution<float> uni (-0.5f, 0.5f);
    for (auto pr : { std::pair<double, double> { 44100.0, 48000.0 }, { 96000.0, 48000.0 }, { 88200.0, 48000.0 }, { 48000.0, 96000.0 } })
    {
        const int n = (int) (10.0 * pr.first);
        juce::AudioBuffer<float> in (2, n);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) in.setSample (ch, i, uni (rng));

        double t0 = now();
        auto fast = ImportSteps::resample (in, pr.first, pr.second, (int) std::thread::hardware_concurrency());
        double t1 = now();
        auto ref = ImportSteps::resampleReference (in, pr.first, pr.second);
        double t2 = now();

        double worst = 0.0;
        const int m = juce::jmin (fast.getNumSamples(), ref.getNumSamples());
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 2000; i < m - 2000; ++i)
                worst = juce::jmax (worst, (double) std::abs (fast.getSample (ch, i) - ref.getSample (ch, i)));
        const double db = 20.0 * std::log10 (worst + 1.0e-12);
        line ("| " + fmt (pr.first / 1000.0, 1) + " to " + fmt (pr.second / 1000.0, 1) + " kHz | " + fmt (-db, 1) + " dB | "
              + fmt (t1 - t0, 2) + " s | " + fmt (t2 - t1, 2) + " s |");
        check (fast.getNumSamples() == ref.getNumSamples(), "polyphase length");
        check (db < -100.0, "polyphase accuracy");
    }
    line();
}

void testProportions()
{
    line ("## 9. Proportions and folders");
    line();

    // Typed expressions
    struct Case { const char* text; double expected; };
    const Case cases[] = {
        { "3/11", 3.0 / 11.0 }, { "2.5/11", 2.5 / 11.0 }, { "3/7.5", 0.4 }, { "0.2727", 0.2727 }, { "27.3%", 0.273 },
        { "2/3 of 3/4", 0.5 }, { "1-1/phi", 0.3819660113 }, { "1/\xcf\x86", 0.6180339887 }, { "golden", 0.6180339887 },
        { "3:8", 3.0 / 11.0 }, { "(1+2)/12", 0.25 }, { "sqrt(2)/2", 0.7071067812 }, { "0,5", 0.5 }, { "1/2 + 1/3", 5.0 / 6.0 }
    };
    int parsed = 0;
    for (auto& c : cases)
    {
        const auto r = Proportion::parse (juce::String::fromUTF8 (c.text));
        const bool ok = r.ok && std::abs (r.value - c.expected) < 1.0e-6;
        check (ok, juce::String ("parse ") + c.text + " -> " + juce::String (r.value, 6) + " " + r.error);
        if (ok) ++parsed;
    }
    for (auto bad : { "", "3/", "abc", "1/0", "3:8:5", "(1+2" })
        check (! Proportion::parse (bad).ok, juce::String ("rejects \"") + bad + "\"");
    line ("- Expressions: " + juce::String (parsed) + " of " + juce::String ((int) std::size (cases)) + " read correctly; 6 malformed ones refused.");

    check (Proportion::name (3.0 / 11.0) == "3/11", "name 3/11");
    check (Proportion::name (0.5) == "1/2", "name 1/2");
    check (Proportion::describe (0.2731).contains ("3/11"), "describe near 3/11");
    check (Proportion::name (0.4, { 7.5 }) == "2/5", "name prefers simplest");

    // Divisions: each position once, simplest name, decimal denominators
    auto d = Proportion::divisions ({ 2, 4, 3, 6 });
    check (d.size() == 7, "divisions 2,3,4,6: 1/6 1/4 1/3 1/2 2/3 3/4 5/6, got " + juce::String ((int) d.size()));
    auto half = std::find_if (d.begin(), d.end(), [] (auto& x) { return std::abs (x.position - 0.5) < 1.0e-9; });
    check (half != d.end() && half->label() == "1/2", "1/2 named simply");
    auto dec = Proportion::divisions ({ 7.5 });
    check (dec.size() == 7 && std::abs (dec.back().position - 7.0 / 7.5) < 1.0e-9, "7.5 gives 7 divisions");

    // Bound boxes follow their frames
    Project p;
    p.resetToDefault ("Test");
    auto track = p.getTrack (0);
    auto a = p.addTextBox (track, 0.0, 240.0, "a");
    auto a1 = p.addTextBox (track, 0.0, 10.0, "a1");
    p.bindBox (a1, a[IDs::id].toString());
    juce::String error;
    check (p.setProportion (a1, false, "3/11", error), "set 3/11: " + error);
    check (std::abs (Project::boxLength (a1) - 240.0 * 3.0 / 11.0) < 1.0e-6, "a1 is 3/11 of a");

    p.getUndoManager().beginNewTransaction ("stretch a");
    p.trimEnd (a, 330.0);
    check (std::abs (Project::boxLength (a1) - 90.0) < 1.0e-6, "a1 follows a stretched to 330 s (90 s)");
    p.getUndoManager().beginNewTransaction ("move a");
    p.moveBox (a, track, 60.0);
    check (std::abs (Project::boxStart (a1) - 60.0) < 1.0e-6, "a1 follows a moved");
    p.getUndoManager().undo();
    p.getUndoManager().undo();
    check (std::abs (Project::boxLength (a1) - 240.0 * 3.0 / 11.0) < 1.0e-6 && std::abs (Project::boxStart (a1)) < 1.0e-6, "undo brings a1 back");

    // Dragging a bound box rewrites its proportions
    p.getUndoManager().beginNewTransaction ("move a1");
    p.moveBox (a1, track, 120.0);
    check (a1[IDs::propStart].toString() == "1/2", "a1 dragged to the middle reads 1/2, got " + a1[IDs::propStart].toString());

    // Chains: a2 measured against a1
    auto a2 = p.addTextBox (track, 0.0, 5.0, "a2");
    p.bindBox (a2, a1[IDs::id].toString());
    p.setProportion (a2, true, "0", error);
    p.setProportion (a2, false, "1/2", error);
    p.trimEnd (a, 480.0);
    check (std::abs (Project::boxLength (a2) - 0.5 * Project::boxLength (a1)) < 1.0e-6, "a2 follows a1 when a changes");
    check (! p.canUseAsFrame (a, a2[IDs::id].toString()), "no loops: a cannot be framed by a2");

    // Unlocked boxes stay put
    p.setLock (a1, false);
    const double before = Project::boxStart (a1);
    p.moveBox (a, track, 100.0);
    check (std::abs (Project::boxStart (a1) - before) < 1.0e-6, "unlocked box stays");

    // The piece frame freezes its length when a box is bound to it
    Project q;
    q.resetToDefault ("Piece");
    auto t0 = q.getTrack (0);
    q.addTextBox (t0, 0.0, 600.0, "whole");
    auto b = q.addTextBox (t0, 0.0, 10.0, "b");
    q.bindBox (b, "piece");
    check (q.hasSetPieceLength() && std::abs (q.getPieceLength() - 600.0) < 1.0e-6, "piece length set when binding");
    q.setProportion (b, true, "golden", error);
    check (std::abs (Project::boxStart (b) - 600.0 / 1.6180339887) < 1.0e-3, "b at the golden section");

    // Folders
    Project f;
    f.resetToDefault ("Folders");
    auto t1 = f.getTrack (0);
    auto t2 = f.addTrack ("T2");
    auto t3 = f.addTrack ("T3");
    auto folder = f.putInNewFolder (t1);
    check (f.getTracks().indexOf (folder) == 0 && f.folderOf (t1) == folder, "track put in a new folder");
    f.moveTrack (t3, f.blockEnd (folder), folder[IDs::id].toString());
    check (f.folderOf (t3) == folder && f.getTracks().indexOf (t3) == 2, "track dragged into the folder, got index " + juce::String (f.getTracks().indexOf (t3)));
    folder.setProperty (IDs::colour, 5, nullptr);
    t3.setProperty (IDs::colour, 2, nullptr);
    check (f.effectiveColour (t3) == 5, "folder colour wins");
    folder.setProperty (IDs::mute, true, nullptr);
    check (f.effectiveMute (t1) && ! f.effectiveMute (t2), "folder mute");
    folder.setProperty (IDs::folded, true, nullptr);
    check (! f.isVisible (t1) && f.isVisible (t2), "folded folder hides its tracks");
    auto inner = f.putInNewFolder (t3);
    check (f.depthOf (t3) == 2 && f.effectiveColour (t3) == 5, "folder in a folder");
    f.moveTrack (folder, f.getTracks().getNumChildren(), {});
    check (f.getTracks().getChild (0) == t2 && f.blockEnd (folder) == f.getTracks().getNumChildren(), "folder moves with its tracks");
    f.ungroupFolder (inner);
    check (f.folderOf (t3) == folder, "ungroup moves tracks up one level");
    f.removeTrack (folder);
    check (f.getNumTracks() == 1 && f.getTrack (0) == t2, "deleting a folder deletes its tracks");

    line ("- Bound boxes follow their frame when it moves or stretches, also through a chain of frames; undo brings everything back; "
          "dragging a bound box rewrites its proportions (1/2 when dropped in the middle); unlocked boxes stay put.");
    line ("- Folders: putting tracks in and out, nesting, colour and mute passed on, folding, moving and deleting with their tracks.");
    line();
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;   // message manager for the formats

    juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("Measurements");
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--quick") quick = true;
        else if (a == "--model") { testProportions(); std::cout << (failures == 0 ? "All checks passed." : juce::String (failures) + " checks FAILED.") << std::endl; return failures == 0 ? 0 : 1; }
        else if (a == "--out" && i + 1 < argc) outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]);
    }
    outDir.createDirectory();

    auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ProjetTests");
    work.deleteRecursively();
    work.createDirectory();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    KernelBank::buildAll();

    md << "# Projet engine: measurements\n\n";
    md << "Machine: " << juce::SystemStats::getCpuModel() << ", " << juce::SystemStats::getOperatingSystemName()
       << ". Date: " << juce::Time::getCurrentTime().toString (true, true) << ".\n\n";

    std::cout << "Import..." << std::endl;           testImport (work, formats);
    std::cout << "Timing..." << std::endl;           testTiming();
    std::cout << "Locate..." << std::endl;           testLocate();
    std::cout << "Fades..." << std::endl;            testFades();
    std::cout << "Other device rates..." << std::endl; testResampledPlayback();
    std::cout << "CPU..." << std::endl;              testCpu();
    std::cout << "Import speed..." << std::endl;     testImportSpeed (work, formats);
    std::cout << "Polyphase..." << std::endl;        testPolyphase();
    std::cout << "Proportions..." << std::endl;      testProportions();

    md << "\n" << (failures == 0 ? "All checks passed." : juce::String (failures) + " checks failed.") << "\n";
    outDir.getChildFile ("report.md").replaceWithText (md);
    work.deleteRecursively();

    std::cout << (failures == 0 ? "All checks passed." : juce::String (failures) + " checks FAILED.") << std::endl;
    return failures == 0 ? 0 : 1;
}
