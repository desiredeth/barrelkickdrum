#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Standalone (no JUCE dependency) monophonic kick voice.
// Layers: Sub, Body, Click, Noise, Resonance (modal membrane model).
struct KickParams
{
    // Pitch envelope
    float pitchStart = 280.f, pitchEnd = 48.f, pitchTime = 0.045f, pitchCurve = 1.5f;
    // Sub
    float subLvl = 0.8f, subDecay = 0.45f, subRatio = 1.f;
    // Body
    float bodyLvl = 0.8f, bodyDecay = 0.25f;
    int   bodyShape = 1;
    // Click
    float clickLvl = 0.5f, clickDecay = 0.006f, clickTone = 5000.f;
    // Noise
    float noiseLvl = 0.15f, noiseDecay = 0.08f, noiseCut = 3000.f;
    // Resonance
    float resLvl = 0.4f, resFreq = 90.f, resDecay = 0.35f;
};

class KickEngine
{
public:
    void prepare (double sr) { sampleRate = sr; active = false; }

    void setParams (const KickParams& np)
    {
        p = np;
        const float sr = (float) sampleRate;
        clickCoef = 1.f - std::exp (-kTwoPi * std::min (p.clickTone, 0.45f * sr) / sr);
        noiseCoef = 1.f - std::exp (-kTwoPi * std::min (p.noiseCut,  0.45f * sr) / sr);
        for (int i = 0; i < kModes; ++i)
        {
            const float fm = std::min (p.resFreq * kRatios[i], 0.45f * sr);
            const float w  = kTwoPi * fm / sr;
            const float dec = std::max (0.005f, p.resDecay / (1.f + 0.7f * (float) i));
            const float r  = std::exp (-6.9078f / (dec * sr));
            a1[i] = 2.f * r * std::cos (w);
            a2[i] = -r * r;
            outG[i] = kGains[i] * std::sin (w);
        }
        endTime = 1.6f * std::max ({ p.subDecay, p.bodyDecay, p.noiseDecay, p.resDecay, p.clickDecay });
    }

    void trigger (float velocity)
    {
        active = true; t = 0.f; vel = velocity;
        phase = subPhase = 0.f;
        clickLP = noiseLP = 0.f;
        for (int i = 0; i < kModes; ++i) y1[i] = y2[i] = 0.f;
    }

    void kill() { active = false; }
    bool isActive() const { return active; }

    float process()
    {
        if (! active) return 0.f;
        const float dt = 1.f / (float) sampleRate;

        const float x = std::exp (-t / std::max (0.001f, p.pitchTime));
        const float f = p.pitchEnd + (p.pitchStart - p.pitchEnd) * std::pow (x, p.pitchCurve);
        phase    += f * dt;               phase    -= std::floor (phase);
        subPhase += f * p.subRatio * dt;  subPhase -= std::floor (subPhase);

        const float atk = std::min (1.f, t / 0.0006f);           // anti-click attack
        auto env = [&] (float d) { return std::exp (-6.9078f * t / std::max (0.001f, d)); };

        float out = 0.f;
        out += p.subLvl  * env (p.subDecay)  * std::sin (kTwoPi * subPhase);
        out += p.bodyLvl * env (p.bodyDecay) * wave (p.bodyShape, phase);

        const float n1 = nextNoise();
        clickLP += clickCoef * (n1 - clickLP);
        out += p.clickLvl * env (p.clickDecay) * clickLP * 1.5f;

        const float n2 = nextNoise();
        noiseLP += noiseCoef * (n2 - noiseLP);
        out += p.noiseLvl * env (p.noiseDecay) * noiseLP * 1.5f;

        // Modal resonator, excited by an area-normalised ~1ms pulse
        const float exc = 800.f * dt * std::exp (-800.f * t);
        float res = 0.f;
        for (int i = 0; i < kModes; ++i)
        {
            const float y = a1[i] * y1[i] + a2[i] * y2[i] + exc;
            y2[i] = y1[i]; y1[i] = y;
            res += outG[i] * y;
        }
        out += p.resLvl * res;

        t += dt;
        if (t > endTime) active = false;
        return out * atk * vel * 0.6f;
    }

private:
    static constexpr float kTwoPi = 6.28318530718f;
    static constexpr int   kModes = 3;
    static constexpr float kRatios[kModes] = { 1.f, 1.593f, 2.136f };   // circular membrane modes
    static constexpr float kGains[kModes]  = { 1.f, 0.6f, 0.4f };

    static float wave (int shape, float ph)
    {
        switch (shape)
        {
            case 0:  return std::sin (kTwoPi * ph);
            case 1:  return 4.f * std::abs (ph - 0.5f) - 1.f;      // triangle
            case 2:  return 1.f - 2.f * ph;                        // saw
            case 3:  return ph < 0.5f ? 1.f : -1.f;                // square
            default: return std::tanh (3.f * std::sin (kTwoPi * ph)); // soft square
        }
    }

    float nextNoise()
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (int32_t) seed * (1.f / 2147483648.f);
    }

    KickParams p;
    double sampleRate = 44100.0;
    bool   active = false;
    float  t = 0.f, vel = 1.f, phase = 0.f, subPhase = 0.f;
    float  clickLP = 0.f, noiseLP = 0.f, clickCoef = 0.5f, noiseCoef = 0.5f, endTime = 1.f;
    float  a1[kModes] {}, a2[kModes] {}, outG[kModes] {}, y1[kModes] {}, y2[kModes] {};
    uint32_t seed = 22222u;
};
