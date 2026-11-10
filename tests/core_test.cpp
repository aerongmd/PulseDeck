// Standalone test for the PulseDeck core (no JUCE needed).
// Build: g++ -std=c++17 -O2 -Wall -Wextra tests/core_test.cpp -o core_test
// Run:   ./core_test [demo.wav]
#include "../Source/Core/Engine.h"

#include <chrono>
#include <cstdio>
#include <fstream>

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t frames = (uint32_t) L.size(), dataBytes = frames * 4;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVEfmt ", 8);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (dataBytes);
    for (size_t i = 0; i < L.size(); ++i)
    {
        const int16_t a = (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32767.0f);
        const int16_t b = (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32767.0f);
        f.write ((const char*) &a, 2); f.write ((const char*) &b, 2);
    }
}

struct Capture
{
    std::vector<float> L, R;
    float peak = 0.0f;
    bool finite = true;
    void run (pd::Engine& e, int frames)
    {
        std::vector<float> bl (256), br (256);
        for (int done = 0; done < frames; done += 256)
        {
            const int n = std::min (256, frames - done);
            e.render (bl.data(), br.data(), n);
            for (int i = 0; i < n; ++i)
            {
                finite = finite && std::isfinite (bl[(size_t) i]) && std::isfinite (br[(size_t) i]);
                peak = std::max ({ peak, std::fabs (bl[(size_t) i]), std::fabs (br[(size_t) i]) });
                L.push_back (bl[(size_t) i]); R.push_back (br[(size_t) i]);
            }
        }
    }
};

int main (int argc, char** argv)
{
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    pd::SoundBank bank;
    const double genSec = std::chrono::duration<double> (clock::now() - t0).count();
    std::printf ("Generated %d sounds in %.2f s\n", pd::kNumSounds, genSec);

    // 1. every sound is non-empty, finite and normalised
    for (int i = 0; i < pd::kNumSounds; ++i)
    {
        const auto& d = bank.get (i);
        float peak = 0.0f; bool ok = ! d.empty();
        for (float x : d) { ok = ok && std::isfinite (x); peak = std::max (peak, std::fabs (x)); }
        CHECK (ok && peak > 0.5f && peak <= 0.91f, "sound %d (%s) bad: size=%zu peak=%f", i, pd::soundName (i).c_str(), d.size(), peak);
    }

    // 2. every sound plays through the engine and produces finite, audible, bounded output
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        pd::Engine e (bank);
        e.prepare (sr);
        for (int i = 0; i < pd::kNumSounds; ++i)
        {
            pd::Params p; p.sound = i; e.setParams (p);
            e.noteOn (60, 0.8f);
            Capture c; c.run (e, (int) (0.3 * sr));
            CHECK (c.finite && c.peak > 0.005f && c.peak <= 1.0f, "sr %.0f sound %d: peak=%f finite=%d", sr, i, c.peak, (int) c.finite);
            e.noteOff (60);
            Capture t; t.run (e, (int) (0.8 * sr));
            CHECK (t.finite, "sr %.0f sound %d: release not finite", sr, i);
        }
    }

    // 3. effect stress: all knobs maxed, 24 notes on 16 voices (voice stealing)
    {
        pd::Engine e (bank); e.prepare (48000.0);
        pd::Params p;
        p.sound = 25; p.drive = p.bitcrush = p.downsample = 1.0f; p.lowpass = 0.7f; p.highpass = 0.3f;
        p.resonance = 1.0f; p.chorusDepth = p.chorusRate = 1.0f; p.delayMix = p.delayTime = p.delayFeedback = 1.0f;
        p.reverbMix = p.reverbSize = 1.0f; p.width = 1.0f; p.tremolo = 1.0f; p.pitch = 12.0f; p.master = 1.5f;
        e.setParams (p);
        Capture c;
        for (int n = 0; n < 24; ++n) { e.noteOn (36 + n * 2, 1.0f); c.run (e, 2000); }
        e.allNotesOff(); c.run (e, 48000 * 2);
        CHECK (c.finite && c.peak <= 1.0f, "stress: peak=%f finite=%d", c.peak, (int) c.finite);
    }

    // 4. each effect knob changes the sound
    {
        pd::Params base; base.sound = 25;
        using M = float pd::Params::*;
        const std::pair<const char*, M> knobs[] = {
            { "drive", &pd::Params::drive }, { "bitcrush", &pd::Params::bitcrush }, { "downsample", &pd::Params::downsample },
            { "lowpass", &pd::Params::lowpass }, { "highpass", &pd::Params::highpass },
            { "chorus", &pd::Params::chorusDepth }, { "delay", &pd::Params::delayMix },
            { "reverb", &pd::Params::reverbMix }, { "width", &pd::Params::width }, { "tremolo", &pd::Params::tremolo } };
        for (auto& [name, m] : knobs)
        {
            // Width only matters when the signal is already stereo, so give it a chorus
            // in both the reference and the test render.
            pd::Params refP = base;
            if (m == &pd::Params::width) refP.chorusDepth = 0.8f;
            pd::Engine er (bank); er.prepare (44100.0); er.setParams (refP); er.noteOn (60, 0.8f);
            Capture rc; rc.run (er, 20000);

            pd::Params p = refP;
            p.*m = (m == &pd::Params::lowpass) ? 0.3f : (m == &pd::Params::width ? 1.0f : 0.8f);
            pd::Engine e (bank); e.prepare (44100.0); e.setParams (p); e.noteOn (60, 0.8f);
            Capture c; c.run (e, 20000);
            double diff = 0.0;
            for (size_t i = 0; i < rc.L.size(); ++i) diff += std::fabs (c.L[i] - rc.L[i]) + std::fabs (c.R[i] - rc.R[i]);
            CHECK (diff > 1.0, "knob '%s' has no audible effect (diff=%f)", name, diff);
        }
    }

    // 5. demo render: one sound per category, then an effected arpeggio
    {
        const int sr = 44100;
        pd::Engine e (bank); e.prepare (sr);
        Capture c;
        for (int cat = 0; cat < pd::kNumCategories; ++cat)
        {
            pd::Params p; p.sound = cat * pd::kVariants + 2; e.setParams (p);
            e.noteOn (60, 0.8f); e.noteOn (67, 0.7f);
            c.run (e, (int) (0.9 * sr)); e.noteOff (60); e.noteOff (67);
            c.run (e, (int) (0.5 * sr));
        }
        pd::Params p; p.sound = 2 * pd::kVariants + 4; p.chorusDepth = 0.6f; p.delayMix = 0.4f; p.delayTime = 0.35f;
        p.reverbMix = 0.35f; p.reverbSize = 0.8f; p.drive = 0.2f; p.release = 1.2f; e.setParams (p);
        for (int note : { 60, 64, 67, 72, 76, 72, 67, 64 }) { e.noteOn (note, 0.75f); c.run (e, sr / 4); e.noteOff (note); }
        c.run (e, 3 * sr);
        CHECK (c.finite && c.peak <= 1.0f, "demo: peak=%f", c.peak);
        if (argc > 1) { writeWav (argv[1], c.L, c.R, sr); std::printf ("Wrote %s (%.1f s)\n", argv[1], (double) c.L.size() / sr); }
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
