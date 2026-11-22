#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// TPT state-variable filter (Cytomic)
struct SVF
{
    float ic1 = 0.f, ic2 = 0.f, k = 1.f, a1 = 0.f, a2 = 0.f, a3 = 0.f;
    void set (float fc, float sr, float q)
    {
        const float g = std::tan (3.14159265f * std::min (fc, 0.45f * sr) / sr);
        k = 1.f / q;
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0.f; }
    inline void run (float v0, float& lp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
        lp = v2; hp = v0 - k * v1 - v2;
    }
};

// 8-line feedback delay network (Householder mixing) with in-loop damping.
// Mono in -> stereo out.
class RumbleFDN
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        cap = (int) (0.26f * sr) + 8;
        for (auto& b : buf) b.assign ((size_t) cap, 0.f);
        for (auto& v : lp) v = 0.f;
        w = 0;
        set (1.f, 2.f, 1500.f);
    }

    void set (float size, float rt60, float dampHz)
    {
        for (int i = 0; i < N; ++i)
        {
            len[i] = std::clamp ((int) (kMs[i] * 0.001f * sr * size), 16, cap - 2);
            g[i] = std::pow (10.f, -3.f * (float) len[i] / (sr * std::max (0.1f, rt60)));
        }
        dampCoef = 1.f - std::exp (-6.2831853f * std::min (dampHz, 0.45f * sr) / sr);
    }

    inline void process (float in, float& l, float& r)
    {
        float y[N]; float sum = 0.f;
        for (int i = 0; i < N; ++i)
        {
            int rp = w - len[i]; if (rp < 0) rp += cap;
            lp[i] += dampCoef * (buf[i][(size_t) rp] - lp[i]);
            y[i] = lp[i] * g[i];
            sum += y[i];
        }
        const float s = sum * (2.f / (float) N);
        for (int i = 0; i < N; ++i)
            buf[i][(size_t) w] = in * 0.35f * kSign[i] + y[i] - s;
        if (++w >= cap) w = 0;

        l = r = 0.f;
        for (int i = 0; i < N; ++i) ((i & 1) ? r : l) += y[i];
        l *= 0.5f; r *= 0.5f;
    }

private:
    static constexpr float kMs[N]   = { 29.7f, 37.1f, 41.3f, 53.9f, 61.7f, 71.9f, 83.3f, 97.1f };
    static constexpr float kSign[N] = { 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, -1.f, -1.f };
    std::vector<float> buf[N];
    float lp[N] {}, g[N] {};
    int len[N] {};
    int cap = 1, w = 0;
    float sr = 44100.f, dampCoef = 0.2f;
};
