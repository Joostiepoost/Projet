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
//  10. Ruler layers and marks (stage 3b): marks following frames, names in
//      expressions, pins, ratio splits, series, ratios, presets, migration.
//  11. Framework (stage 4a): the default file, names in both languages, bad
//      files refused, changes along a box, splitting them, folder levels.
// --model runs only 9, 10 and 11.

#include <JuceHeader.h>
#include "../Source/Engine/Importer.h"
#include "../Source/Engine/SoundData.h"
#include "../Source/Engine/Arrangement.h"
#include "../Source/Engine/PlaybackEngine.h"
#include "../Source/Model/Project.h"
#include "../Source/Model/Proportion.h"
#include "../Source/Model/Framework.h"
#include "../Source/Model/Glyphs.h"
#include "../Source/Model/Lanes.h"

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

void testMarks()
{
    line ("## 10. Ruler layers and marks");
    line();
    juce::String error;

    // Names in expressions
    auto r = Proportion::parse ("m2 - m1", [] (const juce::String& n) -> std::optional<double>
    {
        if (n == "m1") return 0.25;
        if (n == "m2") return 0.75;
        return std::nullopt;
    });
    check (r.ok && std::abs (r.value - 0.5) < 1.0e-12, "m2 - m1 = 0.5");
    check (! Proportion::parse ("zz").ok, "unknown name refused");
    check (Proportion::isValidName ("a1") && ! Proportion::isValidName ("1a") && ! Proportion::isValidName ("phi"), "mark names");
    check (Proportion::replaceName ("a1 - a10 + a1", "a1", "b") == "b - a10 + b", "rename whole words only: " + Proportion::replaceName ("a1 - a10 + a1", "a1", "b"));

    // Ratios and series
    check (Proportion::ratioText ({ 3.0, 8.0, 5.0 }) == "3 : 8 : 5", "ratio 3:8:5");
    check (Proportion::ratioText ({ 6.0, 16.0 }) == "3 : 8", "ratio 3:8 from 6 and 16");
    check (Proportion::ratioText ({ 1.0, 1.6180339887 }) == juce::String::fromUTF8 ("\xe2\x89\x88 8 : 13"), "1 : phi is about 8 : 13, got " + Proportion::ratioText ({ 1.0, 1.6180339887 }));
    check (Proportion::ratioText ({ 1.0, 3.14159265 }) == juce::String::fromUTF8 ("\xe2\x89\x88 7 : 22"), "1 : pi is about 7 : 22");
    check (Proportion::ratioText ({ 1.0, 10.123 }).contains ("10.123"), "no near ratio: decimals, got " + Proportion::ratioText ({ 1.0, 10.123 }));
    check (Proportion::partBoundaries (Proportion::parseParts ("2:3:5:8")).joinIntoString (" ") == "1/9 5/18 5/9", "2:3:5:8 boundaries: "
           + Proportion::partBoundaries (Proportion::parseParts ("2:3:5:8")).joinIntoString (" "));
    auto golden = Proportion::seriesPoints ("golden:1");
    check (golden.size() == 2 && std::abs (golden[0].position - 0.381966) < 1.0e-5 && std::abs (golden[1].position - 0.618034) < 1.0e-5, "golden 1 level");
    check (Proportion::seriesPoints ("golden:2").size() == 8, "golden 2 levels: 8 points, got " + juce::String ((int) Proportion::seriesPoints ("golden:2").size()));
    auto fib = Proportion::seriesPoints ("fibonacci:13");
    check (fib.size() == 5 && fib.back().label == "8/13", "fibonacci of 13: 1 2 3 5 8 /13");
    check (Proportion::seriesPoints ("harmonic:4").size() == 3, "harmonic to 1/4");
    check (Proportion::seriesPoints ("harmonic:64").size() == 63, "harmonic to 1/64");
    check (Proportion::seriesPoints ("custom:1/7, golden").size() == 2, "own series");
    check (Proportion::withSeriesPart ("golden:2;harmonic:8", "golden", "") == "harmonic:8", "remove a series");
    {
        auto rev = Proportion::seriesPoints ("harmonic:8;harmonic.dir:reversed");
        check (rev.size() == 7 && rev.back().label == "7/8" && std::abs (rev.back().position - 0.875) < 1.0e-12, "harmonic to 1/8 reversed ends at 7/8");
        auto fibRev = Proportion::seriesPoints ("fibonacci:5;fibonacci.dir:reversed");
        check (fibRev.size() == 3 && fibRev[0].label == "2/5" && fibRev[2].label == "4/5", "fibonacci of 5 reversed: 2/5 3/5 4/5");
        auto both = Proportion::seriesPoints ("fibonacci:5;fibonacci.dir:both");
        check (both.size() == 4, "fibonacci of 5 both: 1/5 2/5 3/5 4/5 (3/5 once), got " + juce::String ((int) both.size()));
        auto own = Proportion::seriesPoints ("custom:1/7, golden;custom.dir:reversed");
        check (own.size() == 2 && own[1].label == "6/7", "own series reversed");
    }

    // Marks follow their frame; boxes name them
    Project p;
    p.resetToDefault ("Marks");
    check (p.getRulers().getNumChildren() == 2 && Project::isTimeLayer (p.getRulers().getChild (0)), "default layers: time + proportion");
    auto track = p.getTrack (0);
    auto a = p.addTextBox (track, 0.0, 220.0, "a");
    auto m1 = p.addMark (a[IDs::id].toString(), "3/11", "a1");
    check (std::abs (p.markTime (m1) - 60.0) < 1.0e-6, "mark at 3/11 of a = 60 s");
    p.trimEnd (a, 330.0);
    check (std::abs (p.markTime (m1) - 90.0) < 1.0e-6, "mark follows a stretched");

    auto x = p.addTextBox (track, 0.0, 10.0, "x");
    p.bindBox (x, "piece");
    check (p.setProportion (x, true, "a1", error), "start a1: " + error);
    check (std::abs (Project::boxStart (x) - 90.0) < 1.0e-6, "box starts at the mark (90 s), got " + juce::String (Project::boxStart (x)));
    p.setMarkExpression (m1, "1/2", error);
    check (std::abs (Project::boxStart (x) - 165.0) < 1.0e-6, "box follows the mark moved to 1/2 of a");

    auto m2 = p.addMark (a[IDs::id].toString(), "9/11", "a2");
    p.pinEdge (x, false, "a2");
    check (x[IDs::propLength].toString() == "a2 - a1", "end pinned: length a2 - a1, got " + x[IDs::propLength].toString());
    check (std::abs (Project::boxEnd (x) - p.markTime (m2)) < 1.0e-6, "end at a2");

    p.renameMark (m1, "start", error);
    check (x[IDs::propStart].toString() == "start" && x[IDs::propLength].toString() == "a2 - start", "rename reaches box proportions");
    check (! p.renameMark (m2, "start", error), "names stay unique");

    const double before = Project::boxStart (x);
    p.removeMark (m1);
    check (std::abs (Project::boxStart (x) - before) < 1.0e-6 && ! x[IDs::propStart].toString().contains ("start"), "deleted mark: box keeps its place");

    // Ratio split
    auto marks = p.splitFrameByRatio ("piece", { 1, 1, 2 }, "r");
    check (marks.size() == 2 && marks[0][IDs::expr].toString() == "1/4" && marks[1][IDs::expr].toString() == "1/2", "split 1:1:2 into marks");
    auto c = p.addTextBox (track, 0.0, 12.0, "c");
    auto pieces = p.cutBoxByRatio (c, { 1, 1, 2 });
    check (pieces.size() == 3 && std::abs (Project::boxLength (pieces[2]) - 6.0) < 1.0e-6, "cut 1:1:2 into 3, 3 and 6 s");

    // Presets round trip
    auto layers = p.layersToVar();
    p.addLayer ("proportion");
    p.layersFromVar (layers);
    check (p.getRulers().getNumChildren() == 2, "layers back from a preset");

    // Migration from stage 3a
    Project q;
    q.resetToDefault ("Old");
    auto st = q.getState();
    st.removeChild (st.getChildWithName (IDs::RULERS), nullptr);
    st.setProperty (IDs::rulerFrame, "span", nullptr);
    st.setProperty (IDs::denominators, "2,3,7.5", nullptr);
    st.setProperty (IDs::spanStart, 1.0, nullptr);
    st.setProperty (IDs::spanEnd, 5.0, nullptr);
    Project q2;
    q2.fromJson (q.toJson(), error);
    auto active = q2.getActiveLayer();
    check (active.isValid() && q2.layerFrame (active) == "span" && Proportion::indexOf (q2.layerDenominators (active), 7.5) >= 0,
           "a stage 3a project keeps its ruler settings");

    line ("- Marks follow their frames; boxes can name them (start \"a1\", length \"a2 - a1\"), follow them when they move, "
          "keep their place when a mark is deleted; renaming reaches every proportion.");
    line ("- Ratio splits into marks (2:3:5:8 gives 1/9, 5/18, 5/9) and into box parts; ratios (3 : 8 : 5); golden, Fibonacci, harmonic and own series; presets; stage 3a projects keep their ruler.");
    line();
}


void testFramework()
{
    line ("## 11. Framework packages");
    line();
    juce::String error;

    // == The packages Projet ships
    auto fw = Framework::builtIn();
    const auto& packages = fw.getPackages();
    check (packages.size() == 5 && packages[0].id == "composition" && packages[1].id == "schaeffer" && packages[2].id == "thoresen"
           && packages[3].id == "vandegorne" && packages[4].id == "rk1",
           "5 packages in order: composition, schaeffer, thoresen, vandegorne, rk1");
    for (auto& p : packages) check (p.warnings.isEmpty(), p.id + " loads without warnings " + p.warnings.joinIntoString ("; "));
    check (fw.getCategories().size() == 14, "14 categories, got " + juce::String (fw.getCategories().size()));
    auto* energy = fw.findCategory ("composition.energy");
    auto* energy2 = fw.findCategory ("composition.energy2");
    auto* level = fw.findCategory ("composition.level");
    auto* role = fw.findCategory ("composition.role");
    auto* profile = fw.findCategory ("composition.profile");
    auto* type = fw.findCategory ("schaeffer.type");
    check (energy != nullptr && energy2 != nullptr && level != nullptr && role != nullptr && profile != nullptr && type != nullptr,
           "energy, second energy, level, role, profile, type");
    if (energy == nullptr || energy2 == nullptr || level == nullptr || role == nullptr || profile == nullptr || type == nullptr) return;
    check (fw.groupsOf (*energy).size() == 5 && fw.valuesOf (*energy).size() == 10, "5 families, 10 energies");
    check (&fw.source (*energy2) == energy && fw.valuesOf (*energy2).size() == 10, "the second energy shares them");
    check (energy->control == "menu" && level->control == "buttons" && role->control == "buttons" && type->control == "chart", "controls: menu, buttons, chart");
    check (level->along && level->folders && role->along && ! role->folders && role->defaultValue == "M" && ! role->showDefault,
           "level: along, folders; role: along, default M");
    check (fw.valuesOf (*level).size() == 4 && fw.valuesOf (*level)[0].id == "ison" && fw.valuesOf (*level)[3].id == "meson", "4 levels, concrete to abstract");
    check (fw.valuesOf (*role).size() == 3 && fw.valuesOf (*role)[0].id == "F" && role->hasTwoNames, "roles F M B, with two names");
    check (fw.valuesOf (*type).size() == 29 && type->rows.size() == 4 && type->columns.size() == 7 && fw.groupsOf (*type).size() == 3,
           "29 types in a 4 x 7 chart, 3 zones");
    check (fw.findPackage ("rk1")->json["operations"]["primitives"].size() == 10 && fw.findPackage ("rk1")->json["operations"]["macros"].size() == 16,
           "rk1 carries 10 primitives and 16 macros");
    check (level->mark == Framework::Mark::strip && role->mark == Framework::Mark::badge && type->mark == Framework::Mark::badge
           && energy->mark == Framework::Mark::glyph && energy2->mark == Framework::Mark::glyph,
           "marks: level strip, role and type badges, energies glyphs");
    check (fw.valuesOf (*profile).size() == 7 && profile->control == "buttons" && profile->mark == Framework::Mark::badge
           && profile->defaultValue == "flat" && ! profile->showDefault && profile->hasTwoNames,
           "7 dynamic profiles: buttons, badge, default flat, two names");
    check (fw.findValue (*profile, "delta")->label == "<>" && fw.findValue (*profile, "crescendo")->env == juce::Array<float> { 0.0f, 0.06f, 1.0f, 1.0f }
           && fw.findValue (*profile, "resonance")->env.size() == 8 && fw.findValue (*profile, "flat")->env == juce::Array<float> { 0.0f, 1.0f, 1.0f, 1.0f },
           "profile labels and envelopes");
    check (fw.valueName (*profile, "resonance").contains (juce::String::fromUTF8 ("r\xc3\xa9sonance"))
           && fw.valueName (*profile, "resonance").contains (juce::String::fromUTF8 ("attaque-r\xc3\xa9sonance"))
           && fw.findValue (*profile, "decrescendo")->name2.fr.isEmpty(),
           "Schaeffer's names, Vande Gorne's where she has one: " + fw.valueName (*profile, "resonance"));
    check (fw.findValue (*energy, "rebond")->env.isEmpty(), "energies have no envelope of their own");
    check (level->property() == juce::Identifier ("fw.composition.level") && level->alongProperty() == juce::Identifier ("fw.composition.level.along"),
           "boxes store fw.<package>.<category>");

    // == Names
    check (fw.getLanguage() == "fr" && fw.valueName (*energy, "rebond") == "Rebond"
           && fw.valueName (*energy, "pression-deformation") == juce::String::fromUTF8 ("Pression-d\xc3\xa9" "formation"), "French by default, accents kept");
    check (fw.valueName (*energy, "nothing") == "nothing?" && fw.valueName (*energy, {}).isEmpty(), "unknown ids shown as unknown");
    check (fw.valueLabel (*type, "X''") == juce::String::fromUTF8 ("X\xe2\x80\xb3"), "X'' shown as X double prime");
    check (fw.valueColours (*energy, "rebond").colour == 0xffb9707eu, "rebond takes its first family's colour");
    check (fw.valueColours (*type, "Tx").colour == 0xff7a7d85u && fw.valueColours (*level, "ison").colour == 0xff4c8fd6u, "zone colours, level colours");
    check (fw.valueDetails (*fw.findValue (*role, "F")).size() == 11 && fw.valueDetails (*fw.findValue (*role, "B")).size() == 11
           && fw.valueDetails (*fw.findValue (*energy, "rebond")).size() == 3, "figure and ground traits, variation factors");
    check (fw.valueName (*role, "F") == "F premier plan / figure", "both names: " + fw.valueName (*role, "F"));
    fw.setLanguage ("en");
    fw.setNaming ("composition.role", "first");
    check (fw.valueName (*energy, "rebond") == "Bounce" && fw.valueName (*role, "F") == "F foreground" && fw.valueLabel (*role, "B") == "B",
           "English, Thoresen's names: " + fw.valueName (*role, "F"));
    fw.setNaming ("composition.role", "second");
    check (fw.valueName (*role, "B") == "ground" && fw.namingsToString() == "composition.role=second", "Vande Gorne's names");
    Framework names;
    names.namingsFromString (fw.namingsToString());
    check (names.getNaming ("composition.role") == "second" && names.getNaming ("other.thing") == "both", "namings saved and read back");
    fw.setLanguage ("fr");
    fw.setNaming ("composition.role", "both");

    // == Packages that are broken or odd
    check (! fw.addPackage ("{ not json", "broken", error) && error.isNotEmpty(), "a broken package is refused");
    check (fw.getPackages().size() == 5 && fw.getCategories().size() == 14, "and nothing changes");
    check (! fw.addPackage ("[ 1, 2 ]", "list", error), "a file that holds no package is refused");
    check (fw.addPackage (R"({ "categories": [ { "name": "no id" }, { "id": "a", "kind": "colour" },
                                              { "id": "b", "kind": "tags", "along": true }, { "id": "b" },
                                              { "id": "c", "valuesFrom": "nowhere" } ] })", "Odd One", error), "an odd package loads: " + error);
    auto* odd = fw.findPackage ("oddone");
    check (odd != nullptr && odd->categories.size() == 2 && odd->warnings.size() == 5,
           "its id from the folder; bad categories left out, with warnings: " + (odd != nullptr ? odd->warnings.joinIntoString (" | ") : juce::String()));
    fw.removePackage ("oddone");
    check (fw.findPackage ("oddone") == nullptr && fw.getCategories().size() == 14, "removed");

    // == A new framework needs only a folder: the example package
    const juce::File example (juce::String (PROJET_SOURCE_DIR) + "/Examples/Frameworks/example");
    check (fw.addPackage (example.getChildFile ("framework.json").loadFileAsString(), "example", error), "the example package loads: " + error);
    check (fw.findPackage ("example") != nullptr && fw.findPackage ("example")->warnings.isEmpty(), "without warnings");
    auto* motion = fw.findCategory ("example.motion");
    auto* materials = fw.findCategory ("example.materials");
    auto* density = fw.findCategory ("example.density");
    auto* source = fw.findCategory ("example.source");
    energy = fw.findCategory ("composition.energy");
    energy2 = fw.findCategory ("composition.energy2");
    level = fw.findCategory ("composition.level");
    role = fw.findCategory ("composition.role");
    check (motion != nullptr && materials != nullptr && density != nullptr && source != nullptr, "a choice, tags, a number, a text");
    if (motion == nullptr || materials == nullptr || density == nullptr || source == nullptr) return;
    check (motion->kind == Framework::Kind::choice && motion->control == "buttons" && motion->along && motion->folders
           && materials->kind == Framework::Kind::tags && density->kind == Framework::Kind::number && source->kind == Framework::Kind::text,
           "kinds and settings");
    check (fw.getCategories().size() == 18 && fw.getCategories()[14].key() == "example.motion", "after the builtins (order 60)");
    check (fw.valuesOf (*energy).size() == 11 && fw.findValue (*energy, "example-pulsation") != nullptr
           && fw.findValue (*energy, "example-pulsation")->from == "example" && fw.valuesOf (*energy2).size() == 11,
           "it adds an energy, which the second energy shares");
    check (fw.valueColours (*energy, "example-pulsation").colour == fw.findGroup (*energy, "loi")->colours.colour, "in the family it names");
    check (fw.formatNumber (*density, 6.0) == "6" && density->minimum == 0.0 && density->maximum == 10.0, "numbers");
    check (Tags::format (Tags::parse (" metal, glass,metal ")) == "metal,glass", "tags");
    check (Framework::marksFor (*materials).contains (Framework::Mark::label) && ! Framework::marksFor (*materials).contains (Framework::Mark::strip)
           && Framework::marksFor (*density).contains (Framework::Mark::strip), "the marks that suit each kind");
    check (fw.addPackage (R"({ "id": "clash", "additions": [ { "category": "composition.energy", "values": [ { "id": "rebond" } ] },
                                                            { "category": "nobody.nothing", "values": [ { "id": "x" } ] } ] })", "clash", error)
           && fw.findPackage ("clash")->warnings.size() == 2 && fw.valuesOf (*fw.findCategory ("composition.energy")).size() == 11,
           "an addition never replaces a value, and one to a missing category is reported");
    fw.removePackage ("clash");
    fw.removePackage ("example");
    check (fw.valuesOf (*fw.findCategory ("composition.energy")).size() == 10, "without the example, its addition goes");

    // == Converting 4a framework files
    juce::StringArray ids, jsons;
    juce::String lang, naming;
    check (Framework::convertLegacy (Framework::legacyDefaultJson(), ids, jsons, lang, naming, error) && ids.size() == 5
           && jsons[0] == Framework::builtins()[0].json && lang == "fr" && naming == "both", "an unedited 4a file becomes the builtins");
    auto edited = Framework::legacyDefaultJson().replace ("\"version\": 1,", "\"version\": 1, \"edited\": true,")
                                                .replace ("\"language\": \"fr\"", "\"language\": \"en\"")
                                                .replace ("\"roleNames\": \"both\"", "\"roleNames\": \"vandegorne\"");
    check (Framework::convertLegacy (edited, ids, jsons, lang, naming, error) && ids.size() == 5 && jsons[0] != Framework::builtins()[0].json
           && lang == "en" && naming == "second", "an edited one is converted, with its language and role names");
    Framework converted;
    for (int i = 0; i < ids.size(); ++i) check (converted.addPackage (jsons[i], ids[i], error), "converted " + ids[i] + " loads: " + error);
    auto builtin = Framework::builtIn();
    bool same = converted.getCategories().size() == builtin.getCategories().size();
    juce::String difference;
    for (int i = 0; same && i < builtin.getCategories().size(); ++i)
    {
        const auto& a = builtin.getCategories()[i];
        const auto& b = converted.getCategories()[i];
        auto differ = [&] (const juce::String& what) { same = false; difference = a.key() + ": " + what; };
        if (a.key() != b.key() || a.defaultValue != b.defaultValue || a.rows.size() != b.rows.size() || a.columns.size() != b.columns.size()) { differ ("settings"); break; }
        if (builtin.valuesOf (a).size() != converted.valuesOf (b).size() || builtin.groupsOf (a).size() != converted.groupsOf (b).size()) { differ ("sizes"); break; }
        for (int k = 0; same && k < builtin.groupsOf (a).size(); ++k)
        {
            const auto& x = builtin.groupsOf (a)[k];
            const auto& y = converted.groupsOf (b)[k];
            if (x.id != y.id || x.name.fr != y.name.fr || x.colours.colour != y.colours.colour || x.colours.dark != y.colours.dark) differ ("group " + x.id);
        }
        for (int k = 0; same && k < builtin.valuesOf (a).size(); ++k)
        {
            const auto& x = builtin.valuesOf (a)[k];
            const auto& y = converted.valuesOf (b)[k];
            if (x.id != y.id || x.label != y.label || x.name.fr != y.name.fr || x.name.en != y.name.en || x.name2.fr != y.name2.fr
                || x.colours.colour != y.colours.colour || x.groups != y.groups || x.detailsFr != y.detailsFr || x.detailsEn != y.detailsEn
                || x.row != y.row || x.col != y.col || x.weight != y.weight || x.glyph != y.glyph || x.fill != y.fill || x.about.en != y.about.en
                || x.env != y.env)
                differ ("value " + x.id);
        }
    }
    check (same, "converting 4a gives exactly the builtin content " + difference);
    auto renamed = Framework::legacyDefaultJson().replace ("\"fr\": \"Rebond\"", "\"fr\": \"Rebondi\"");
    check (Framework::convertLegacy (renamed, ids, jsons, lang, naming, error), "a renamed energy converts");
    Framework mine;
    for (int i = 0; i < ids.size(); ++i) mine.addPackage (jsons[i], ids[i], error);
    check (mine.valueName (*mine.findCategory ("composition.energy"), "rebond") == "Rebondi", "and keeps your name");
    check (! Framework::convertLegacy ("{ broken", ids, jsons, lang, naming, error), "a broken 4a file is refused");

    // == Values on boxes: own, the folder's, the default; along; split
    // (categories from a framework that no longer changes: adding or removing
    // packages rebuilds the list, so earlier pointers are stale)
    energy = builtin.findCategory ("composition.energy");
    energy2 = builtin.findCategory ("composition.energy2");
    level = builtin.findCategory ("composition.level");
    role = builtin.findCategory ("composition.role");
    type = builtin.findCategory ("schaeffer.type");
    Project p;
    p.resetToDefault ("t");
    auto track = p.getTrack (0);
    auto box = p.addTextBox (track, 0.0, 10.0, "b");
    p.setFrameworkValue (box, *level, "dison");
    box.setProperty (level->alongProperty(), "0.7:meson:0;0.9:ison:0", nullptr);
    box.setProperty (role->alongProperty(), "0.2:F:0", nullptr);
    check (p.frameworkValue (box, *level).value == "dison" && p.frameworkValue (box, *level).isOwn(), "a box's own level");
    check (p.frameworkValue (box, *role).value == "M" && p.frameworkValue (box, *role).isDefault(), "no role: the default, M");
    check (p.valuesShown (box, *level).joinIntoString (",") == "dison,meson,ison" && p.valuesShown (box, *role).joinIntoString (",") == "M,F",
           "the values a box shows, along it too");
    p.getUndoManager().beginNewTransaction ("Split");
    auto right = p.splitBox (box, 8.0);
    check (right.isValid() && right[level->property()].toString() == "meson" && right[level->alongProperty()].toString() == "0.5:ison:0",
           "right part: base meson, change at half: " + right[level->property()].toString() + " " + right[level->alongProperty()].toString());
    check (box[level->alongProperty()].toString() == "0.875:meson:0" && box[level->property()].toString() == "dison", "left part: " + box[level->alongProperty()].toString());
    check (right[role->property()].toString() == "F" && ! right.hasProperty (role->alongProperty()) && box[role->alongProperty()].toString() == "0.25:F:0", "roles split too");
    p.getUndoManager().undo();
    check (box[level->alongProperty()].toString() == "0.7:meson:0;0.9:ison:0" && Project::boxLength (box) == 10.0, "undo restores the changes");

    auto folder = p.putInNewFolder (track);
    auto plain = p.addTextBox (track, 20.0, 2.0, "plain");
    check (p.frameworkValue (plain, *level).value.isEmpty() && p.frameworkValue (plain, *level).from == Project::FrameworkValue::From::none, "no level anywhere");
    p.setFrameworkValue (folder, *level, "ison");
    check (p.frameworkValue (plain, *level).value == "ison" && p.frameworkValue (plain, *level).fromFolder(), "the folder gives its level");
    check (p.frameworkValue (box, *level).value == "dison" && p.frameworkValue (box, *level).isOwn(), "a box's own level overrides it");
    check (p.frameworkValue (plain, *energy).value.isEmpty(), "but only categories folders can give");
    auto outer = p.putInNewFolder (folder);
    p.setFrameworkValue (outer, *level, "meson");
    check (p.frameworkValue (plain, *level).value == "ison", "the nearest folder wins");
    p.setFrameworkValue (folder, *level, {});
    check (p.frameworkValue (plain, *level).value == "meson" && p.frameworkValue (plain, *level).fromFolder(), "else the outer folder's");
    p.setFrameworkValue (box, *type, "X''");
    check (box[type->property()].toString() == "X''", "Schaeffer's type");

    // == Saved and read back, and 4a projects
    juce::String err;
    Project q;
    check (q.fromJson (p.toJson(), err) && q.findBox (box[IDs::id].toString())[level->alongProperty()].toString() == box[level->alongProperty()].toString(),
           "changes along a box survive saving");
    check (q.findBox (box[IDs::id].toString())[type->property()].toString() == "X''" && q.findBox (box[IDs::id].toString()).getType() == IDs::BOX,
           "the type survives saving, and the box stays a box");

    Project old;
    old.resetToDefault ("4a");
    auto oldTrack = old.getTrack (0);
    auto oldBox = old.addTextBox (oldTrack, 0.0, 4.0, "old");
    auto oldFolder = old.putInNewFolder (oldTrack);
    for (auto [k, v] : { std::pair<const char*, const char*> { "energy", "rebond" }, { "energy2", "flux" }, { "level", "meson" },
                         { "levelAlong", "0.5:ison:0" }, { "role", "F" }, { "roleAlong", "0.5:B:0" }, { "typology", "K" } })
        oldBox.setProperty (juce::Identifier (k), v, nullptr);
    oldFolder.setProperty ("level", "dison", nullptr);
    old.getState().setProperty (IDs::colourMode, "family", nullptr);
    Project migrated;
    check (migrated.fromJson (old.toJson(), err), "a 4a project opens");
    auto m = migrated.findBox (oldBox[IDs::id].toString());
    check (m[energy->property()].toString() == "rebond" && m[energy2->property()].toString() == "flux" && m[level->property()].toString() == "meson"
           && m[level->alongProperty()].toString() == "0.5:ison:0" && m[role->property()].toString() == "F"
           && m[role->alongProperty()].toString() == "0.5:B:0" && m[type->property()].toString() == "K", "its box fields get their new keys");
    check (! m.hasProperty ("energy") && ! m.hasProperty ("typology") && ! m.hasProperty ("level"), "the old ones go");
    check (migrated.findTrack (oldFolder[IDs::id].toString())[level->property()].toString() == "dison", "folder levels too");
    check (migrated.getState()[IDs::colourMode].toString() == "composition.energy", "the colour mode too");

    // == Files: library, project copies, a broken package, a 4a project folder
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ProjetFrameworks").getNonexistentSibling();
    const auto lib = tmp.getChildFile ("Library");
    for (auto& b : Framework::builtins()) check (Framework::writeBuiltin (lib, b.id).wasOk(), "writes " + b.id);
    check (Framework::packageFolders (lib).size() == 5 && Framework::folderVersion (lib.getChildFile ("composition")) == 5
           && lib.getChildFile ("schaeffer").getChildFile ("help.md").existsAsFile(), "the library: 5 packages with their help");
    check (lib.getChildFile ("composition").getChildFile ("glyphs.js").existsAsFile()
           && ! lib.getChildFile ("schaeffer").getChildFile ("glyphs.js").exists(),
           "composition brings its glyphs.js; schaeffer has none");
    const auto proj = tmp.getChildFile ("Piece");
    check (Framework::copyPackage (lib.getChildFile ("schaeffer"), Framework::projectFolder (proj)).wasOk()
           && Framework::projectFolder (proj).getChildFile ("schaeffer").getChildFile ("framework.json").existsAsFile(), "a package copied into a project");
    Framework::writePackage (lib.getChildFile ("broken"), "{ nope", {});
    Framework fromDisk;
    Framework::Report report;
    fromDisk.loadFolder (lib, report);
    check (fromDisk.getPackages().size() == 5 && report.errors.size() == 1 && report.errors[0].startsWith ("broken"),
           "a broken package is reported, the others load: " + report.errors.joinIntoString ("; "));
    const auto piece4a = tmp.getChildFile ("Piece4a");
    piece4a.createDirectory();
    piece4a.getChildFile ("framework.json").replaceWithText (Framework::legacyDefaultJson().replace ("\"language\": \"fr\"", "\"language\": \"en\""));
    Framework::Report report4a;
    check (Framework::ensureProject (piece4a, lang, naming, report4a).wasOk() && Framework::packageFolders (Framework::projectFolder (piece4a)).size() == 5
           && piece4a.getChildFile ("framework-4a.json").existsAsFile() && ! piece4a.getChildFile ("framework.json").exists() && lang == "en",
           "a 4a project folder gets packages; its old file is kept as framework-4a.json");
    check (Framework::projectFolder (piece4a).getChildFile ("composition").getChildFile ("glyphs.js").existsAsFile(),
           "its converted composition package gets the glyphs.js");
    Framework::Report again;
    check (Framework::ensureProject (piece4a, lang, naming, again).wasOk() && again.notes.isEmpty(), "and only once");
    Framework::setLibraryFolder (lib);      // not the real library
    const auto fresh = tmp.getChildFile ("Fresh");
    fresh.createDirectory();
    juce::String freshLang, freshNaming;
    Framework::Report freshReport;
    check (Framework::ensureProject (fresh, freshLang, freshNaming, freshReport).wasOk()
           && Framework::packageFolders (Framework::projectFolder (fresh)).size() == Framework::packageFolders (lib).size()
           && freshLang.isEmpty(), "a new project gets copies of the library's packages");
    Framework::setLibraryFolder ({});
    tmp.deleteRecursively();
    line();
}


void testGlyphs()
{
    line ("## 12. Energy glyphs (JavaScript)");
    line();
    const juce::Array<float> flat { 0.0f, 1.0f, 1.0f, 1.0f };

    // == Projet's own script
    Glyphs::Script script;
    check (script.load (Glyphs::builtinSource(), "projet").wasOk(), "the built-in script loads");
    auto fw = Framework::builtIn();
    auto* energy = fw.findCategory ("composition.energy");
    juce::String missing;
    for (auto& v : fw.valuesOf (*energy))
        if (! script.has (v.glyph)) missing = v.glyph;
    check (missing.isEmpty(), "every energy has its shape " + missing);

    juce::Array<Glyphs::Stroke> strokes;
    check (script.run ("percussion-resonance", 240.0f, 28.0f, flat, 1, strokes).wasOk() && ! strokes.isEmpty(), "percussion-resonance draws");
    bool filled = false, inBounds = true;
    for (auto& st : strokes)
    {
        filled = filled || st.filled;
        for (int i = 0; i + 1 < st.points.size(); i += 2)
            inBounds = inBounds && st.points[i] > -4.0f && st.points[i] < 244.0f && st.points[i + 1] > -4.0f && st.points[i + 1] < 32.0f;
    }
    check (filled && inBounds, "a filled shape, inside the band");

    // The profile is the envelope: a crescendo starts the shape small.
    juce::Array<Glyphs::Stroke> plain, cres;
    check (script.run ("oscillation", 240.0f, 28.0f, flat, 1, plain).wasOk()
           && script.run ("oscillation", 240.0f, 28.0f, { 0.0f, 0.06f, 1.0f, 1.0f }, 1, cres).wasOk(), "oscillation draws with profiles");
    auto spread = [] (const juce::Array<Glyphs::Stroke>& list, float x0, float x1)
    {
        float top = 14.0f, bottom = 14.0f;      // the centre of a 28-high band
        for (auto& st : list)
            for (int i = 0; i + 1 < st.points.size(); i += 2)
                if (st.points[i] >= x0 && st.points[i] <= x1)
                {
                    top = juce::jmin (top, st.points[i + 1]);
                    bottom = juce::jmax (bottom, st.points[i + 1]);
                }
        return bottom - top;
    };
    check (spread (cres, 0.0f, 48.0f) < spread (plain, 0.0f, 48.0f) * 0.4f
           && spread (cres, 200.0f, 240.0f) > spread (plain, 200.0f, 240.0f) * 0.8f,
           "a crescendo starts small and ends full");

    // Seeds: the same seed repeats exactly, another differs.
    juce::Array<Glyphs::Stroke> a, b, c;
    script.run ("accumulation-corpuscules", 200.0f, 28.0f, flat, 3, a);
    script.run ("accumulation-corpuscules", 200.0f, 28.0f, flat, 3, b);
    script.run ("accumulation-corpuscules", 200.0f, 28.0f, flat, 4, c);
    check (! a.isEmpty() && a.size() == b.size() && a[0].dots == b[0].dots, "the same seed repeats the shape exactly");
    check (! c.isEmpty() && a[0].dots != c[0].dots, "another seed varies it");
    check (script.run ("nothing", 100.0f, 20.0f, flat, 1, a).failed(), "an unknown glyph is refused");

    // == Broken scripts fail politely
    Glyphs::Script broken;
    check (broken.load ("var glyphs = {", "p").failed(), "a syntax error is reported");
    const auto noObject = broken.load ("var x = 1;", "p");
    check (noObject.failed() && noObject.getErrorMessage().contains ("glyphs object"), "a script without glyphs is reported");
    Glyphs::Script endless;
    check (endless.load ("var glyphs = { x: function (w, h, rand) { while (true) {} } };", "p").wasOk(), "an endless glyph still loads");
    check (endless.run ("x", 100.0f, 20.0f, flat, 1, a).failed(), "but is stopped when it runs");

    // == The set: the value's own package first, then the built-in shapes
    auto full = Framework::builtIn();
    const juce::File example (juce::String (PROJET_SOURCE_DIR) + "/Examples/Frameworks/example");
    juce::String error;
    check (full.addPackage (example.getChildFile ("framework.json").loadFileAsString(), "example", error,
                            example.getChildFile ("glyphs.js").loadFileAsString()), "the example package loads with its script");
    Glyphs::Set set;
    set.rebuild (full);
    check (set.getProblems().isEmpty(), "no script problems: " + set.getProblems().joinIntoString ("; "));
    check (set.find ("example-pulse", { "example", "composition" }).owner == "example", "the example draws its added energy");
    check (set.find ("rebond", { "example", "composition" }).owner == "composition", "composition draws the others");
    check (set.find ("nothing", {}).script == nullptr, "an unknown glyph finds no script");
    Framework plainFw;
    check (plainFw.addPackage (Framework::builtins()[0].json, "composition", error), "composition without its script");
    Glyphs::Set fallback;
    fallback.rebuild (plainFw);
    check (fallback.find ("rebond", { "composition" }).owner == "projet", "the built-in shapes step in");
    juce::String badError;
    check (full.addPackage (R"({ "id": "bad" })", "bad", badError, "var glyphs = ("), "a package with a broken script still loads");
    Glyphs::Set reported;
    reported.rebuild (full);
    check (reported.getProblems().size() == 1 && reported.getProblems()[0].startsWith ("bad"),
           "the broken script is one problem: " + reported.getProblems().joinIntoString ("; "));
    line();
}

void testJunctions()
{
    line ("## 13. Junctions, endings, mixing relations (stage 4c)");
    line();
    auto fw = Framework::builtIn();
    auto* junction = fw.findCategory ("thoresen.junction");
    auto* ending = fw.findCategory ("thoresen.ending");
    auto* mixing = fw.findCategory ("vandegorne.mixing");
    auto* profile = fw.findCategory ("composition.profile");
    auto* peak = fw.findCategory ("composition.peak");
    auto* peakShape = fw.findCategory ("composition.peakshape");
    check (junction != nullptr && ending != nullptr && mixing != nullptr && peak != nullptr && peakShape != nullptr, "the new categories load");
    if (junction == nullptr || ending == nullptr || mixing == nullptr || peak == nullptr || peakShape == nullptr) return;
    for (auto& p : fw.getPackages()) check (p.warnings.isEmpty(), p.id + " loads cleanly " + p.warnings.joinIntoString ("; "));

    // == As data
    check (junction->between && junction->mark == Framework::Mark::seam && junction->derive.size() == 6 && fw.valuesOf (*junction).size() == 7,
           "the junction: a join, a seam sign, six timing rules, seven kinds");
    bool signs = true;
    for (auto& v : fw.valuesOf (*junction)) signs = signs && v.sign.isNotEmpty();
    check (signs && fw.findValue (*junction, "hinged")->endX == 33.0f && fw.findValue (*junction, "superimposed")->endX < 0.0f,
           "every kind has its sign; the end bracket where it has one");
    check (ending->atEnd && ending->mark == Framework::Mark::end && fw.valuesOf (*ending).size() == 6
           && fw.findValue (*ending, "conclusive")->seamSign.isNotEmpty(), "endings: at the end, with signs and seam marks");
    check (fw.findValue (*ending, "vague")->seamFade > 0.0f && fw.findValue (*ending, "vague")->seamSign.isEmpty()
           && fw.findValue (*ending, "open")->seamFade == 0.0f && fw.findValue (*junction, "hinged")->fadeX == 27.0f
           && fw.findValue (*junction, "joint")->fadeX < 0.0f, "a vague ending fades its line in a seam sign, up to the hinge");
    check (Framework::marksFor (*junction) == juce::Array<Framework::Mark> { Framework::Mark::none, Framework::Mark::seam, Framework::Mark::badge, Framework::Mark::label }
           && Framework::marksFor (*ending).contains (Framework::Mark::end) && ! Framework::marksFor (*profile).contains (Framework::Mark::end),
           "the marks that suit joins and endings");
    check (mixing->between && mixing->onlyWith.category == "thoresen.junction" && mixing->onlyWith.values.size() == 3
           && fw.findValue (*mixing, "relay")->onlyWith.values == juce::StringArray { "hinged" },
           "mixing relations only on overlaps; relay and metamorphosis only on hinged ones");
    check (peak->kind == Framework::Kind::number && peak->defaultValue == "50" && peak->envelope == "peak" && peak->onlyWith.category == "composition.profile"
           && peakShape->envelope == "shape" && fw.findValue (*profile, "delta")->peak == 1 && fw.findValue (*profile, "crescendo")->peak == -1,
           "the peak: a number moving delta and hollow peaks, and its shape");

    // == Joins found from the timing
    Project p;
    p.resetToDefault ("joins");
    p.setFramework (&fw);
    auto t1 = p.getTrack (0);
    auto t2 = p.addTrack ("other");
    auto a = p.addTextBox (t1, 0.0, 10.0, "a");
    auto b = p.addTextBox (t1, 11.0, 4.0, "b");         // gap 1 s
    auto c = p.addTextBox (t1, 15.03, 4.97, "c");       // 30 ms
    auto d = p.addTextBox (t1, 20.01, 4.99, "d");       // 10 ms
    auto e = p.addTextBox (t1, 24.0, 6.0, "e");         // overlap 1 s
    auto f = p.addTextBox (t1, 28.0, 22.0, "f");        // overlap 2 s
    auto g = p.addTextBox (t1, 35.0, 25.0, "g");        // overlap 15 s
    auto kind = [&] (const juce::ValueTree& box) { return p.frameworkValue (box, *junction).value; };
    check (! p.joinBefore (a).isValid() && kind (a).isEmpty(), "the first box has no junction");
    check (p.joinBefore (b).before == a && std::abs (p.joinBefore (b).gap - 1.0) < 1.0e-9 && p.boxAfter (a) == b, "the box before, and the gap");
    check (kind (b) == "separate" && kind (c) == "joint" && kind (d) == "close" && kind (e) == "hinged" && kind (f) == "hinged" && kind (g) == "overlapping",
           "separate, joint, close, hinged, overlapping: " + kind (b) + " " + kind (c) + " " + kind (d) + " " + kind (e) + " " + kind (f) + " " + kind (g));
    check (p.frameworkValue (b, *junction).fromTiming(), "found from the timing");
    auto bridge = p.addTextBox (t2, 9.0, 3.0, "bridge");
    check (kind (b) == "bridged" && p.joinBefore (b).covered, "a box on another track spanning the gap: bridged");
    t2.setProperty (IDs::mute, true, nullptr);
    check (kind (b) == "separate", "not while that track is muted");
    t2.setProperty (IDs::mute, false, nullptr);
    p.moveBox (bridge, t2, 10.5);
    check (kind (b) == "separate", "nor when it does not span the whole gap");

    // == Chosen junctions
    p.setFrameworkValue (b, *junction, "superimposed");
    check (kind (b) == "superimposed" && p.frameworkValue (b, *junction).isOwn(), "a chosen junction is kept");
    p.moveBox (b, t1, 10.0);
    check (kind (b) == "superimposed", "even when the timing changes");
    p.setFrameworkValue (b, *junction, {});
    check (kind (b) == "close" && p.frameworkValue (b, *junction).fromTiming(), "From the timing: back to what the timing says");

    // == Mixing relations: only on overlaps
    p.setFrameworkValue (c, *mixing, "fusion");
    p.setFrameworkValue (e, *mixing, "relay");
    p.setFrameworkValue (g, *mixing, "relay");
    check (p.frameworkValue (c, *mixing).value.isEmpty() && ! p.applies (c, *mixing), "no mixing relation on a joint");
    check (p.frameworkValue (e, *mixing).value == "relay", "relay of energy on a hinged overlap");
    check (p.applies (g, *mixing) && ! p.valueApplies (g, *mixing, "relay") && p.frameworkValue (g, *mixing).value.isEmpty()
           && p.valueApplies (g, *mixing, "fusion"), "but not on a longer overlap, where fusion still can");
    p.setFrameworkValue (g, *junction, "hinged");
    check (p.frameworkValue (g, *mixing).value == "relay", "a chosen hinged junction lets it through");

    // == Splits: the join stays left, the ending goes right
    p.setFrameworkValue (f, *junction, "overlapping");
    p.setFrameworkValue (f, *ending, "cut");
    p.getUndoManager().beginNewTransaction ("Split");
    auto right = p.splitBox (f, 40.0);
    check (right.isValid() && f[junction->property()].toString() == "overlapping" && ! right.hasProperty (junction->property())
           && ! f.hasProperty (ending->property()) && right[ending->property()].toString() == "cut",
           "the left part keeps the junction, the right part the ending");
    p.getUndoManager().undo();
    check (f[ending->property()].toString() == "cut", "undo brings the ending back");

    // == The profile's peak
    auto box = p.addTextBox (t2, 100.0, 10.0, "peaked");
    check (p.frameworkValue (box, *peak).value.isEmpty(), "no peak without a profile that has one");
    p.setFrameworkValue (box, *profile, "delta");
    check (p.frameworkValue (box, *peak).value == "50" && p.frameworkValue (box, *peak).isDefault()
           && p.frameworkValue (box, *peakShape).value == "hard", "a delta: peak in the middle, hard");
    const juce::Array<float> delta { 0.0f, 0.06f, 0.5f, 1.0f, 1.0f, 0.06f };
    check (Framework::shapeEnvelope (delta, 1, 0.25, false) == juce::Array<float> { 0.0f, 0.06f, 0.25f, 1.0f, 1.0f, 0.06f },
           "the peak moves to a quarter of the box");
    const auto soft = Framework::shapeEnvelope (delta, 1, 0.25, true);
    float top = 0.0f, topAt = -1.0f, quarterway = 0.0f;
    for (int i = 0; i + 1 < soft.size(); i += 2)
    {
        if (soft[i + 1] > top) { top = soft[i + 1]; topAt = soft[i]; }
        if (std::abs (soft[i] - 0.125f) < 0.011f) quarterway = soft[i + 1];
    }
    check (soft.size() == 98 && std::abs (top - 1.0f) < 1.0e-4f && std::abs (topAt - 0.25f) < 0.011f && std::abs (quarterway - 0.53f) < 0.02f,
           "a soft peak: rounded, still at the top where it was put");
    check (Framework::shapeEnvelope (delta, 1, -1.0, false) == delta && Framework::shapeEnvelope (delta, -1, 0.3, false) == delta,
           "no peak position, or no peak: unchanged");

    // == A library from before 4c gets the new packages, once
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ProjetLibrary4c").getNonexistentSibling();
    for (auto id : { "composition", "schaeffer", "rk1" }) Framework::writeBuiltin (tmp, id);
    Framework::setLibraryFolder (tmp);
    juce::String lang, naming;
    Framework::Report first;
    check (Framework::ensureLibrary (lang, naming, first).wasOk() && tmp.getChildFile ("thoresen").isDirectory()
           && tmp.getChildFile ("vandegorne").isDirectory() && first.notes.size() == 1, "the new packages arrive: " + first.notes.joinIntoString ("; "));
    tmp.getChildFile ("thoresen").deleteRecursively();
    Framework::Report second;
    check (Framework::ensureLibrary (lang, naming, second).wasOk() && ! tmp.getChildFile ("thoresen").exists() && second.notes.isEmpty(),
           "and one you remove stays removed");
    tmp.deleteRecursively();
    line();
}

void testOverview()
{
    line ("## 14. Folder fields and overview lanes (stage 4d.1)");
    line();
    auto fw = Framework::builtIn();
    for (auto& pk : fw.getPackages()) check (pk.warnings.isEmpty(), pk.id + " loads cleanly " + pk.warnings.joinIntoString ("; "));
    auto* writing = fw.findCategory ("vandegorne.writing");
    auto* felt = fw.findCategory ("vandegorne.timefelt");
    auto* movement = fw.findCategory ("vandegorne.timemove");
    auto* mixing = fw.findCategory ("vandegorne.mixing");
    auto* level = fw.findCategory ("composition.level");
    auto* role = fw.findCategory ("composition.role");
    auto* energy = fw.findCategory ("composition.energy");
    auto* energy2 = fw.findCategory ("composition.energy2");
    check (writing != nullptr && felt != nullptr && movement != nullptr && mixing != nullptr, "writing type and time-feel load");
    if (writing == nullptr || felt == nullptr || movement == nullptr || mixing == nullptr) return;

    // == As data
    check (writing->foldersOnly && writing->folders && felt->foldersOnly && movement->foldersOnly
           && fw.valuesOf (*writing).size() == 7 && fw.valuesOf (*felt).size() == 3 && fw.valuesOf (*movement).size() == 5,
           "on folders only: seven writing types, three times felt, five movements");
    check (Framework::marksFor (*writing) == juce::Array<Framework::Mark> { Framework::Mark::none, Framework::Mark::label } && writing->mark == Framework::Mark::label,
           "shown as a label (on the folder's lane) or not at all");
    const auto& imps = fw.implicationsFor (*mixing);
    juce::StringArray implied;
    for (auto& i : imps) implied.add (i.value + ">" + i.implied);
    check (imps.size() == 3 && implied.contains ("weaving>fusion") && implied.contains ("complex>complementarity") && implied.contains ("rhythms>complementarity"),
           "only where she names one: weaving gives fusion, complex drawing and rhythms complementarity (" + implied.joinIntoString (", ") + ")");
    {
        Framework odd = Framework::builtIn();
        juce::String error;
        check (odd.addPackage (R"({ "id": "odd", "order": 70, "categories": [
                 { "id": "w", "kind": "choice", "folders": "only", "values": [ { "id": "x", "implies": { "category": "vandegorne.mixing", "value": "nonsense" } } ] },
                 { "id": "b", "kind": "choice", "values": [ { "id": "y", "implies": { "category": "vandegorne.mixing", "value": "fusion" } } ] } ],
               "lanes": [ { "id": "l1", "kind": "rows", "category": "nothing" }, { "id": "l2", "kind": "pie", "category": "w" } ] })", "odd", error),
               "a package with odd implications and lanes loads");
        auto* pk = odd.findPackage ("odd");
        check (pk != nullptr && pk->warnings.size() == 4 && odd.implicationsFor (*odd.findCategory ("vandegorne.mixing")).size() == 3,
               "an unknown value, an implication from a box category, an unknown category and kind: warned and left out ("
               + (pk != nullptr ? pk->warnings.joinIntoString (" / ") : juce::String()) + ")");
    }

    Project p;
    p.resetToDefault ("overview");
    p.setFramework (&fw);
    auto first = p.getTrack (0);

    // == The semiotic track template: one folder per level
    p.getUndoManager().beginNewTransaction ("Folders");
    const int before = p.getTracks().getNumChildren();
    const auto made = p.addFoldersFor (*level);
    juce::StringArray names, values;
    for (auto f : made) { names.add (f[IDs::name].toString()); values.add (f[level->property()].toString()); }
    check (made.size() == 4 && values.joinIntoString (",") == "ison,dison,metonymic,meson" && p.getTracks().getNumChildren() == before + 8,
           "Folders from > Semiotic level: four folders in order, each with a track (" + names.joinIntoString (", ") + ")");
    auto isonTrack = p.getTracks().getChild (before + 1);
    check (! Project::isFolder (isonTrack) && p.folderOf (isonTrack) == made[0], "the track sits in its folder");
    auto onIson = p.addTextBox (isonTrack, 0.0, 4.0, "on I-son");
    check (p.frameworkValue (onIson, *level).value == "ison" && p.frameworkValue (onIson, *level).fromFolder(), "its boxes take the folder's level");
    check (p.addFoldersFor (*writing).isEmpty() && p.addFoldersFor (*energy).isEmpty(), "not for folder-only categories, nor for ones folders cannot give");
    p.getUndoManager().undo();
    check (p.getTracks().getNumChildren() == before, "one undo removes them");

    // == Writing types give their overlaps a mixing relation
    p.getUndoManager().beginNewTransaction ("Folder");
    auto folder = p.addFolder ("Section", -1);
    auto inside = p.addTrack ("voices", -1, folder[IDs::id].toString());
    auto a = p.addTextBox (inside, 0.0, 10.0, "a");
    auto b = p.addTextBox (inside, 9.0, 6.0, "b");          // hinged
    auto c = p.addTextBox (inside, 20.0, 5.0, "c");         // separate
    p.setFrameworkValue (folder, *writing, "weaving");
    auto mix = [&] (const juce::ValueTree& box) { return p.frameworkValue (box, *mixing); };
    check (mix (b).value == "fusion" && mix (b).isImplied() && mix (b).isDerived() && mix (b).source == "vandegorne.writing",
           "weaving: the hinged overlap takes fusion, from the folder's writing type");
    check (mix (c).value.isEmpty() && mix (a).value.isEmpty(), "not where there is no overlap");
    p.setFrameworkValue (b, *mixing, "rejection");
    check (mix (b).value == "rejection" && mix (b).isOwn() && p.frameworkValue (b, *mixing, true).value == "fusion",
           "a chosen relation wins; without it, the folder's");
    p.setFrameworkValue (b, *mixing, {});
    p.setFrameworkValue (folder, *writing, "stainedglass");
    check (mix (b).value.isEmpty(), "stained glass implies none");
    p.setFrameworkValue (folder, *writing, "complex");
    check (mix (b).value == "complementarity", "complex drawing: complementarity");
    auto inner = p.addFolder ("Inner", -1, folder[IDs::id].toString());
    p.moveTrack (inside, p.getTracks().getNumChildren(), inner[IDs::id].toString());
    p.setFrameworkValue (inner, *writing, "weaving");
    check (mix (b).value == "fusion", "the nearest folder with a writing type decides");
    b.setProperty (writing->property(), "rhythms", nullptr);
    check (p.frameworkValue (b, *writing).value.isEmpty(), "boxes never have a folder-only value");
    p.setFrameworkValue (folder, *felt, "suspended");
    check (folder[felt->property()].toString() == "suspended" && p.frameworkValue (a, *felt).value.isEmpty(), "time felt sits on the folder only");

    // == Lanes
    auto* energies = fw.findLane ("composition.energies");
    auto* semiotic = fw.findLane ("composition.semiotic");
    auto* typology = fw.findLane ("schaeffer.typology");
    check (fw.getLanes().size() == 3 && energies != nullptr && semiotic != nullptr && typology != nullptr, "three lanes: energies, semiotic profile, typology");
    if (energies == nullptr || semiotic == nullptr || typology == nullptr) return;
    check (energies->kind == Framework::Lane::Kind::rows && energies->second == "composition.energy2" && energies->byGroups
           && semiotic->kind == Framework::Lane::Kind::scale && semiotic->weightCategory == "composition.role"
           && semiotic->weightFor ("F") == 3.0 && semiotic->weightFor ("B") == 1.0 && semiotic->weightFor ("x") == 2.0,
           "as declared: rows by family with a second energy; a scale weighted by role (F 3, M 2, B 1)");

    Project q;
    q.resetToDefault ("lanes");
    q.setFramework (&fw);
    auto t1 = q.getTrack (0);
    auto t2 = q.addTrack ("two");
    auto t3 = q.addTrack ("muted");
    auto x = q.addTextBox (t1, 0.0, 10.0, "x");
    auto y = q.addTextBox (t2, 5.0, 10.0, "y");
    auto z = q.addTextBox (t3, 0.0, 20.0, "z");
    t3.setProperty (IDs::mute, true, nullptr);
    q.setFrameworkValue (x, *energy, "frottement-granulation");             // two families
    q.setFrameworkValue (y, *energy, "rotation");
    q.setFrameworkValue (y, *energy2, "frottement-granulation");
    q.setFrameworkValue (z, *energy, "rotation");
    const auto famRows = Lanes::rows (q, fw, *energies);
    auto rowFor = [&] (const juce::String& id) -> const Lanes::Row* { for (auto& r : famRows) if (r.id == id) return &r; return nullptr; };
    const auto* fx = fw.findValue (*energy, "frottement-granulation");
    const auto* fr = fw.findValue (*energy, "rotation");
    check (famRows.size() == 5 && fx != nullptr && fr != nullptr && fx->groups.size() == 2, "five rows, one per family; frottement in two");
    if (famRows.size() != 5 || fx == nullptr || fr == nullptr) return;
    const auto* r1 = rowFor (fx->groups[0]);
    const auto* r2 = rowFor (fx->groups[1]);
    const auto* rr = rowFor (fr->groups[0]);
    check (r1 != nullptr && r2 != nullptr && r1->main.size() == 1 && r2->main.size() == 1 && r1->main[0].start == 0.0 && r1->main[0].end == 10.0,
           "a box counts in both its families");
    check (r1 != nullptr && r1->second.size() == 1 && r1->second[0].start == 5.0 && r1->second[0].end == 15.0, "a second energy, apart");
    check (rr != nullptr && rr->main.size() == 1 && rr->main[0].start == 5.0 && rr->main[0].end == 15.0 && ! rr->boxIds.contains (z[IDs::id].toString()),
           "a muted track is left out");
    q.setFrameworkValue (x, *energy, "rotation");
    const auto famRows2 = Lanes::rows (q, fw, *energies);
    const Lanes::Row* rr2 = nullptr;
    for (auto& r : famRows2) if (r.id == fr->groups[0]) rr2 = &r;
    check (rr2 != nullptr && rr2->main.size() == 3 && rr2->main[0].count == 1 && rr2->main[1].count == 2 && rr2->main[1].start == 5.0
           && rr2->main[1].end == 10.0 && rr2->main[2].count == 1, "two boxes at once: counted (1, 2, 1)");
    auto x2 = q.addTextBox (t1, 10.0, 5.0, "x2");
    q.setFrameworkValue (x2, *energy, "rotation");
    const auto famRows3 = Lanes::rows (q, fw, *energies);
    for (auto& r : famRows3)
        if (r.id == fr->groups[0])
            check (r.main.size() == 2 && r.main[1].start == 5.0 && r.main[1].end == 15.0 && r.main[1].count == 2, "touching boxes leave no gap (one ends as the next begins: still 2)");
    check (Lanes::rows (q, fw, *typology).size() == 3, "typology: three rows (balanced, redundant, eccentric)");

    // The semiotic profile
    q.setFrameworkValue (x, *level, "ison");
    q.setFrameworkValue (x, *role, "F");
    q.setFrameworkValue (y, *level, "meson");
    q.setFrameworkValue (y, *role, "B");
    const auto sc = Lanes::scale (q, fw, *semiotic);
    check (sc.ids.joinIntoString (",") == "ison,dison,metonymic,meson", "the scale in its order");
    bool both = false, alone = false;
    for (auto& sg : sc.segments)
    {
        if (sg.start == 5.0 && sg.end == 10.0) both = sg.low == 0.0 && sg.high == 3.0 && std::abs (sg.mean - 0.75) < 1.0e-9;
        if (sg.start == 0.0 && sg.end == 5.0) alone = sg.low == 0.0 && sg.high == 0.0 && sg.mean == 0.0;
    }
    check (both && alone, "the range present, and the average weighted by role: (3 x 0 + 1 x 3) / 4 = 0.75");
    q.setChangesAlong (x, *level, { { 0.5, "metonymic", 0.0 } });
    const auto pieces = Lanes::piecesOf (q, x, *level);
    check (pieces.size() == 2 && pieces[0].value == "ison" && pieces[1].value == "metonymic" && pieces[1].start == 5.0, "changes along a box count");

    // == Lane layers
    const int layers = q.getRulers().getNumChildren();
    auto lane = q.addLaneLayer ("composition.energies", "Energies", 54);
    check (Project::isLaneLayer (lane) && ! Project::isProportionLayer (lane) && q.getRulers().getNumChildren() == layers + 1
           && lane[IDs::lane].toString() == "composition.energies", "a lane is a kind of ruler layer");
    q.setActiveLayer (lane);
    check (Project::isProportionLayer (q.getActiveLayer()), "never the active proportion layer");
    const int changes = q.getChangeCount();
    q.moveBox (x, t1, 1.0);
    check (q.getChangeCount() > changes, "a change counter for caches");
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
        else if (a == "--model") { testProportions(); testMarks(); testFramework(); testGlyphs(); testJunctions(); testOverview(); std::cout << (failures == 0 ? "All checks passed." : juce::String (failures) + " checks FAILED.") << std::endl; return failures == 0 ? 0 : 1; }
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
    std::cout << "Marks..." << std::endl;            testMarks();
    std::cout << "Framework packages..." << std::endl; testFramework();
    std::cout << "Energy glyphs..." << std::endl;      testGlyphs();
    std::cout << "Junctions..." << std::endl;          testJunctions();
    std::cout << "Overview..." << std::endl;           testOverview();

    md << "\n" << (failures == 0 ? "All checks passed." : juce::String (failures) + " checks failed.") << "\n";
    outDir.getChildFile ("report.md").replaceWithText (md);
    work.deleteRecursively();

    std::cout << (failures == 0 ? "All checks passed." : juce::String (failures) + " checks FAILED.") << std::endl;
    return failures == 0 ? 0 : 1;
}
