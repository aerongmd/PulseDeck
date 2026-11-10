#pragma once
// PulseDeck core: 16-voice sampler engine + 15-knob effect chain. No JUCE dependency.
#include "SoundBank.h"
#include <array>
#include <cstdint>

namespace pd
{
struct Params
{
    int sound = 0;
    float pitch = 0.0f;     // semitones
    float attack = 0.003f;  // seconds
    float release = 0.35f;  // seconds
    float master = 0.8f;

    // the 15 effect knobs, all 0..1
    float drive = 0.0f, bitcrush = 0.0f, downsample = 0.0f;
    float lowpass = 1.0f, highpass = 0.0f, resonance = 0.15f;
    float chorusDepth = 0.0f, chorusRate = 0.3f;
    float delayMix = 0.0f, delayTime = 0.4f, delayFeedback = 0.35f;
    float reverbMix = 0.0f, reverbSize = 0.5f;
    float width = 0.5f, tremolo = 0.0f;
};

namespace fx
{
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (double fc, double q, double sr)
    {
        fc = std::clamp (fc, 10.0, sr * 0.45);
        const double g = std::tan (kPi * fc / sr);
        k = (float) (1.0 / q);
        a1 = (float) (1.0 / (1.0 + g * (g + 1.0 / q)));
        a2 = (float) (g * a1);
        a3 = (float) (g * a2);
    }
    void process (float x, float& low, float& high)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2;
        high = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
};

struct Chorus
{
    std::vector<float> buf[2];
    int mask = 0, w = 0;
    double phase = 0.0;

    void prepare (double sr)
    {
        int size = 1;
        while (size < (int) (sr * 0.06)) size <<= 1;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        mask = size - 1; w = 0; phase = 0.0;
    }
    float read (int c, double d) const
    {
        const double rp = (double) w - d;
        const int i0 = (int) std::floor (rp);
        const float fr = (float) (rp - i0);
        const float a = buf[c][(size_t) (i0 & mask)], b = buf[c][(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * fr;
    }
    void process (float& l, float& r, double sr, float depth, float rate)
    {
        buf[0][(size_t) w] = l;
        buf[1][(size_t) w] = r;
        if (depth > 0.001f)
        {
            const double base = 0.012 * sr, mod = 0.006 * (double) depth * sr;
            const double dl = base + mod * std::sin (kTwoPi * phase);
            const double dr = base + mod * std::sin (kTwoPi * phase + kPi * 0.5);
            const float mix = 0.5f * depth;
            l = l * (1.0f - mix) + read (0, dl) * mix;
            r = r * (1.0f - mix) + read (1, dr) * mix;
            phase += (0.1 + 4.9 * (double) rate) / sr;
            if (phase >= 1.0) phase -= 1.0;
        }
        w = (w + 1) & mask;
    }
};

struct PingPongDelay
{
    std::vector<float> buf[2];
    int size = 0, w = 0;
    float dampL = 0, dampR = 0;
    double smoothed = 0.0;

    void prepare (double sr)
    {
        size = (int) (sr * 1.1) + 4;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        w = 0; dampL = dampR = 0.0f; smoothed = 0.4 * sr;
    }
    int wrap (int x) const { x %= size; return x < 0 ? x + size : x; }
    float read (int c, double d) const
    {
        const double rp = (double) w - d;
        const int i0 = (int) std::floor (rp);
        const float fr = (float) (rp - i0);
        const float a = buf[c][(size_t) wrap (i0)], b = buf[c][(size_t) wrap (i0 + 1)];
        return a + (b - a) * fr;
    }
    void process (float& l, float& r, double sr, float time, float fb, float mix)
    {
        const double target = (0.05 + 0.95 * (double) time) * sr;
        smoothed += 0.0008 * (target - smoothed);
        const float dl = read (0, smoothed), dr = read (1, smoothed);
        dampL += 0.55f * (dl - dampL);
        dampR += 0.55f * (dr - dampR);
        const float f = std::clamp (fb, 0.0f, 0.92f);
        buf[0][(size_t) w] = l + f * dampR;
        buf[1][(size_t) w] = r + f * dampL;
        l += mix * dl;
        r += mix * dr;
        w = (w + 1) % size;
    }
};

struct Comb
{
    std::vector<float> b; int i = 0; float store = 0.0f;
    void init (int n) { b.assign ((size_t) std::max (n, 1), 0.0f); i = 0; store = 0.0f; }
    float process (float in, float fb, float damp)
    {
        const float o = b[(size_t) i];
        store = o * (1.0f - damp) + store * damp;
        b[(size_t) i] = in + store * fb;
        if (++i >= (int) b.size()) i = 0;
        return o;
    }
};

struct Allpass
{
    std::vector<float> b; int i = 0;
    void init (int n) { b.assign ((size_t) std::max (n, 1), 0.0f); i = 0; }
    float process (float in)
    {
        const float bo = b[(size_t) i];
        const float o = -in + bo;
        b[(size_t) i] = in + bo * 0.5f;
        if (++i >= (int) b.size()) i = 0;
        return o;
    }
};

// Freeverb-style stereo reverb
struct Reverb
{
    Comb combL[8], combR[8];
    Allpass apL[4], apR[4];

    void prepare (double sr)
    {
        static const int ct[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int at[4] = { 556, 441, 341, 225 };
        const double s = sr / 44100.0;
        for (int i = 0; i < 8; ++i) { combL[i].init ((int) (ct[i] * s)); combR[i].init ((int) ((ct[i] + 23) * s)); }
        for (int i = 0; i < 4; ++i) { apL[i].init ((int) (at[i] * s)); apR[i].init ((int) ((at[i] + 23) * s)); }
    }
    void process (float& l, float& r, float size, float mix)
    {
        const float in = (l + r) * 0.015f;
        const float fb = 0.7f + 0.28f * size, damp = 0.35f;
        float wl = 0.0f, wr = 0.0f;
        for (int i = 0; i < 8; ++i) { wl += combL[i].process (in, fb, damp); wr += combR[i].process (in, fb, damp); }
        for (int i = 0; i < 4; ++i) { wl = apL[i].process (wl); wr = apR[i].process (wr); }
        l = l * (1.0f - 0.5f * mix) + wl * mix * 3.0f;
        r = r * (1.0f - 0.5f * mix) + wr * mix * 3.0f;
    }
};

inline float softLimit (float x)
{
    const float a = std::fabs (x);
    if (a <= 0.95f) return x;
    return std::copysign (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f), x);
}
} // namespace fx

class Engine
{
public:
    explicit Engine (const SoundBank& b) : bank (b) {}

    void prepare (double sr)
    {
        sampleRate = sr;
        chorus.prepare (sr);
        delay.prepare (sr);
        reverb.prepare (sr);
        for (auto& v : voices) v = Voice {};
        for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); holdVal[c] = 0.0f; }
        holdCount = 0.0f; tremPhase = 0.0;
    }

    void setParams (const Params& p) { params = p; }

    void noteOn (int note, float vel)
    {
        Voice* v = nullptr;
        for (auto& c : voices) if (! c.active) { v = &c; break; }
        if (v == nullptr)
            for (auto& c : voices)
                if (v == nullptr || (c.releasing && ! v->releasing)
                    || (c.releasing == v->releasing && c.order < v->order))
                    v = &c;

        v->data = &bank.get (params.sound);
        v->note = note;
        v->order = ++orderCounter;
        v->pos = 0.0;
        v->step = std::pow (2.0, ((double) (note - kRootNote) + (double) params.pitch) / 12.0) * kBankRate / sampleRate;
        v->gain = 0.45f * (0.3f + 0.7f * std::clamp (vel, 0.0f, 1.0f));
        v->env = 0.0f;
        v->attInc = 1.0f / std::max (1.0f, params.attack * (float) sampleRate);
        v->releasing = false;
        v->active = true;
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && ! v.releasing && v.note == note)
            {
                v.releasing = true;
                v.relDec = 1.0f / std::max (1.0f, params.release * (float) sampleRate);
            }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active && ! v.releasing)
            {
                v.releasing = true;
                v.relDec = 1.0f / std::max (1.0f, 0.02f * (float) sampleRate);
            }
    }

    // overwrites L and R
    void render (float* L, float* R, int n)
    {
        updateFx();
        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const auto& d = *v.data;
                const int idx = (int) v.pos;
                if (idx + 1 >= (int) d.size()) { v.active = false; continue; }
                const float fr = (float) (v.pos - idx);
                const float s = d[(size_t) idx] + (d[(size_t) idx + 1] - d[(size_t) idx]) * fr;
                v.pos += v.step;
                if (v.releasing) { v.env -= v.relDec; if (v.env <= 0.0f) { v.env = 0.0f; v.active = false; continue; } }
                else v.env = std::min (1.0f, v.env + v.attInc);
                l += s * v.env * v.gain;
            }
            float r = l;
            processFx (l, r);
            L[i] = fx::softLimit (l * params.master);
            R[i] = fx::softLimit (r * params.master);
        }
    }

private:
    struct Voice
    {
        bool active = false, releasing = false;
        int note = -1;
        uint64_t order = 0;
        double pos = 0.0, step = 1.0;
        float gain = 0.0f, env = 0.0f, attInc = 0.0f, relDec = 0.0f;
        const std::vector<float>* data = nullptr;
    };

    void updateFx()
    {
        const auto& p = params;
        driveOn = p.drive > 0.001f;
        driveGain = 1.0f + 19.0f * p.drive;
        driveComp = 1.0f / std::sqrt (driveGain);
        crushOn = p.bitcrush > 0.001f;
        crushQ = std::pow (2.0f, 15.0f - 14.0f * p.bitcrush);
        holdOn = p.downsample > 0.001f;
        holdFactor = 1.0f + 31.0f * p.downsample;
        lpOn = p.lowpass < 0.999f;
        hpOn = p.highpass > 0.001f;
        for (int c = 0; c < 2; ++c)
        {
            if (lpOn) lp[c].set (20.0 * std::pow (1000.0, (double) p.lowpass), 0.707 + (double) p.resonance * 9.3, sampleRate);
            else lp[c].reset();
            if (hpOn) hp[c].set (20.0 * std::pow (100.0, (double) p.highpass), 0.707, sampleRate);
            else hp[c].reset();
        }
    }

    void processFx (float& l, float& r)
    {
        const auto& p = params;
        float* ch[2] = { &l, &r };

        for (int c = 0; c < 2; ++c)
        {
            float x = *ch[c];
            if (driveOn) x = std::tanh (driveGain * x) * driveComp;
            if (crushOn) x = std::round (x * crushQ) / crushQ;
            if (holdOn)
            {
                if (c == 0) { holdCount += 1.0f; if (holdCount >= holdFactor) { holdCount -= holdFactor; fresh = true; } else fresh = false; }
                if (fresh) holdVal[c] = x;
                x = holdVal[c];
            }
            float lo, hi;
            if (lpOn) { lp[c].process (x, lo, hi); x = lo; }
            if (hpOn) { hp[c].process (x, lo, hi); x = hi; }
            *ch[c] = x;
        }

        if (p.tremolo > 0.001f)
        {
            const float g = 1.0f - p.tremolo * (0.5f + 0.5f * (float) std::sin (kTwoPi * tremPhase));
            tremPhase += 5.5 / sampleRate; if (tremPhase >= 1.0) tremPhase -= 1.0;
            l *= g; r *= g;
        }

        chorus.process (l, r, sampleRate, p.chorusDepth, p.chorusRate);
        delay.process (l, r, sampleRate, p.delayTime, p.delayFeedback, p.delayMix);
        reverb.process (l, r, p.reverbSize, p.reverbMix);

        const float m = 0.5f * (l + r), s = 0.5f * (l - r) * (p.width * 2.0f);
        l = m + s; r = m - s;
    }

    const SoundBank& bank;
    Params params;
    double sampleRate = 44100.0;
    std::array<Voice, 16> voices;
    uint64_t orderCounter = 0;

    bool driveOn = false, crushOn = false, holdOn = false, lpOn = false, hpOn = false, fresh = true;
    float driveGain = 1.0f, driveComp = 1.0f, crushQ = 32768.0f, holdFactor = 1.0f, holdCount = 0.0f;
    float holdVal[2] = { 0.0f, 0.0f };
    double tremPhase = 0.0;
    fx::Svf lp[2], hp[2];
    fx::Chorus chorus;
    fx::PingPongDelay delay;
    fx::Reverb reverb;
};
} // namespace pd
