// BACKRVERB - reverse reverb render engine
// Offline renderer: takes a captured sound, builds a tempo-synced reverse-reverb swell
// that ends exactly where the sound starts.
#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

namespace brv
{

//==============================================================================
struct RenderSettings
{
    int    source        = 0;      // 0 = vocal, 1 = instrument
    int    lengthIndex   = 2;      // 1/4 bar, 1/2 bar, 1 bar, 2 bars, 4 bars
    float  size          = 0.5f;
    float  tone          = 0.5f;
    float  width         = 0.5f;
    float  tremDepth     = 0.0f;
    bool   tremOn        = false;
    int    tremRateIndex = 2;
    bool   lowCut        = false;
    bool   normalize     = true;
    double bpm           = 120.0;
    int    tsNum         = 4;
    int    tsDen         = 4;
    double sampleRate    = 48000.0;

    bool operator== (const RenderSettings& o) const
    {
        return source == o.source && lengthIndex == o.lengthIndex
            && size == o.size && tone == o.tone && width == o.width
            && tremDepth == o.tremDepth && tremOn == o.tremOn && tremRateIndex == o.tremRateIndex
            && lowCut == o.lowCut && normalize == o.normalize
            && std::abs (bpm - o.bpm) < 1.0e-3 && tsNum == o.tsNum && tsDen == o.tsDen
            && sampleRate == o.sampleRate;
    }
    bool operator!= (const RenderSettings& o) const { return ! (*this == o); }
};

inline const juce::StringArray& lengthNames()
{
    static const juce::StringArray n { "1/4 BAR", "1/2 BAR", "1 BAR", "2 BARS", "4 BARS" };
    return n;
}

inline const juce::StringArray& tremRateNames()
{
    static const juce::StringArray n { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    return n;
}

/** Length in quarter-note beats. A bar = num * 4 / den quarter notes. */
inline double lengthInBeats (int idx, int num, int den)
{
    static const double bars[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
    const double barBeats = juce::jmax (1, num) * 4.0 / juce::jmax (1, den);
    return bars[juce::jlimit (0, 4, idx)] * barBeats;
}

inline double tremDivisionBeats (int idx)
{
    static const double d[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    return d[juce::jlimit (0, 5, idx)];
}

inline int lengthInSamples (const RenderSettings& s)
{
    const double secs = lengthInBeats (s.lengthIndex, s.tsNum, s.tsDen) * 60.0 / juce::jlimit (20.0, 999.0, s.bpm);
    return (int) juce::jlimit (s.sampleRate * 0.05, s.sampleRate * 40.0, std::round (secs * s.sampleRate));
}

//==============================================================================
/** Transposed direct form II biquad (RBJ cookbook). */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowPass (double sr, double f, double q = 0.7071)  { design (sr, f, q, 0); }
    void setHighPass (double sr, double f, double q = 0.7071) { design (sr, f, q, 1); }

    void design (double sr, double f, double q, int type)
    {
        f = juce::jlimit (10.0, sr * 0.45, f);
        const double w = juce::MathConstants<double>::twoPi * f / sr;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        if (type == 0) { b0 = (1 - cw) * 0.5; b1 = 1 - cw;    b2 = (1 - cw) * 0.5; }
        else           { b0 = (1 + cw) * 0.5; b1 = -(1 + cw); b2 = (1 + cw) * 0.5; }
        a1 = -2.0 * cw; a2 = 1.0 - alpha;
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        z1 = z2 = 0;
    }

    inline float process (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    void processBuffer (float* d, int n) noexcept { for (int i = 0; i < n; ++i) d[i] = process (d[i]); }
};

//==============================================================================
/** Schroeder allpass diffuser. */
struct Allpass
{
    std::vector<float> buf;
    int pos = 0;
    float g = 0.6f;

    void prepare (int len, float gain) { buf.assign ((size_t) juce::jmax (1, len), 0.0f); pos = 0; g = gain; }

    inline float process (float x) noexcept
    {
        const float d = buf[(size_t) pos];
        const float w = x + g * d;
        const float y = d - g * w;
        buf[(size_t) pos] = w;
        if (++pos >= (int) buf.size()) pos = 0;
        return y;
    }
};

//==============================================================================
/** 8-line feedback delay network with modulated delays, damping and input diffusion. */
class FDNReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sr, float sizeScale, float rt60, float dampHz, float diffusion, double preDelayMs)
    {
        static const double baseMs[N] = { 29.71, 37.13, 41.11, 43.73, 53.09, 61.31, 67.93, 73.37 };
        static const double apMsL[4]  = { 4.77, 3.59, 12.73, 9.31 };
        static const double apMsR[4]  = { 4.53, 3.77, 12.29, 9.67 };

        modDepth = (float) (0.00035 * sr); // ~0.35 ms of chorus-like movement
        for (int i = 0; i < N; ++i)
        {
            len[i] = (int) std::round (baseMs[i] * 0.001 * sr * sizeScale);
            const int cap = len[i] + (int) modDepth + 8;
            lines[i].assign ((size_t) cap, 0.0f);
            wp[i] = 0;
            g[i] = (float) std::pow (10.0, -3.0 * len[i] / (rt60 * sr));
            lp[i] = 0.0f;
            phase[i] = (float) i * 0.785f;
            phaseInc[i] = (float) (juce::MathConstants<double>::twoPi * (0.11 + 0.07 * i) / sr);
        }
        lpCoef = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jlimit (500.0, sr * 0.45, (double) dampHz) / sr);

        for (int k = 0; k < 4; ++k)
        {
            apL[k].prepare ((int) std::round (apMsL[k] * 0.001 * sr * (0.8 + 0.4 * sizeScale)), diffusion);
            apR[k].prepare ((int) std::round (apMsR[k] * 0.001 * sr * (0.8 + 0.4 * sizeScale)), diffusion);
        }

        preLen = juce::jmax (1, (int) std::round (preDelayMs * 0.001 * sr));
        preL.assign ((size_t) preLen, 0.0f);
        preR.assign ((size_t) preLen, 0.0f);
        prePos = 0;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        std::array<float, N> s {};
        for (int t = 0; t < n; ++t)
        {
            // pre-delay
            float l = preL[(size_t) prePos], r = preR[(size_t) prePos];
            preL[(size_t) prePos] = inL[t];
            preR[(size_t) prePos] = inR[t];
            if (++prePos >= preLen) prePos = 0;

            for (auto& a : apL) l = a.process (l);
            for (auto& a : apR) r = a.process (r);

            // read modulated taps + damping + decay
            for (int i = 0; i < N; ++i)
            {
                const int cap = (int) lines[i].size();
                const float d = (float) len[i] - modDepth * 0.5f * (1.0f + std::sin (phase[i]));
                phase[i] += phaseInc[i];
                if (phase[i] > juce::MathConstants<float>::twoPi) phase[i] -= juce::MathConstants<float>::twoPi;

                float rp = (float) wp[i] - d;
                while (rp < 0.0f) rp += (float) cap;
                const int i0 = (int) rp;
                const float fr = rp - (float) i0;
                const int i1 = (i0 + 1) % cap;
                const float v = lines[i][(size_t) i0] + fr * (lines[i][(size_t) i1] - lines[i][(size_t) i0]);

                lp[i] = v + lpCoef * (lp[i] - v);
                s[i] = lp[i] * g[i];
            }

            outL[t] = 0.5f * (s[0] - s[2] + s[4] - s[6] + 0.6f * (s[1] - s[5]));
            outR[t] = 0.5f * (s[1] - s[3] + s[5] - s[7] + 0.6f * (s[2] - s[6]));

            hadamard (s);

            for (int i = 0; i < N; ++i)
            {
                const float in = (i & 1) ? r : l;
                const float sign = ((i >> 1) & 1) ? -1.0f : 1.0f;
                lines[i][(size_t) wp[i]] = s[i] + sign * 0.5f * in;
                if (++wp[i] >= (int) lines[i].size()) wp[i] = 0;
            }
        }
    }

private:
    static void hadamard (std::array<float, N>& x) noexcept
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[(size_t) j], b = x[(size_t) (j + h)];
                    x[(size_t) j] = a + b;
                    x[(size_t) (j + h)] = a - b;
                }
        const float k = 0.35355339f; // 1/sqrt(8)
        for (auto& v : x) v *= k;
    }

    std::array<std::vector<float>, N> lines;
    std::array<int, N> len {}, wp {};
    std::array<float, N> g {}, lp {}, phase {}, phaseInc {};
    float lpCoef = 0.0f, modDepth = 0.0f;
    std::array<Allpass, 4> apL, apR;
    std::vector<float> preL, preR;
    int preLen = 1, prePos = 0;
};

//==============================================================================
/** Trims leading/trailing silence of a capture. Returns a stereo buffer starting at the onset. */
inline juce::AudioBuffer<float> trimCapture (const juce::AudioBuffer<float>& in, int numValid)
{
    numValid = juce::jmin (numValid, in.getNumSamples());
    juce::AudioBuffer<float> out;
    if (numValid <= 0) return out;

    const int nc = in.getNumChannels();
    float peak = 0.0f;
    for (int c = 0; c < nc; ++c) peak = juce::jmax (peak, in.getMagnitude (c, 0, numValid));
    if (peak < 1.0e-5f) return out;

    auto amp = [&] (int i) { float a = 0; for (int c = 0; c < nc; ++c) a = juce::jmax (a, std::abs (in.getSample (c, i))); return a; };

    const float onT = juce::jmax (peak * 0.02f, 1.0e-4f), offT = juce::jmax (peak * 0.003f, 3.0e-5f);
    int start = 0;
    while (start < numValid && amp (start) < onT) ++start;
    start = juce::jmax (0, start - 16); // keep the very first edge of the transient
    int end = numValid - 1;
    while (end > start && amp (end) < offT) --end;

    const int len = end - start + 1;
    out.setSize (2, len);
    for (int c = 0; c < 2; ++c)
        out.copyFrom (c, 0, in, juce::jmin (c, nc - 1), start, len);

    // tiny fade-out on the tail so the preview never clicks
    const int fo = juce::jmin (len, 256);
    for (int c = 0; c < 2; ++c) out.applyGainRamp (c, len - fo, fo, 1.0f, 0.0f);
    return out;
}

inline void normalizeTo (juce::AudioBuffer<float>& b, float target)
{
    float peak = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c) peak = juce::jmax (peak, b.getMagnitude (c, 0, b.getNumSamples()));
    if (peak > 1.0e-6f) b.applyGain (target / peak);
}

//==============================================================================
/** Renders the reverse-reverb swell. The last sample of the result is the instant the dry sound begins. */
inline juce::AudioBuffer<float> renderSwell (const juce::AudioBuffer<float>& dry, const RenderSettings& s)
{
    juce::ScopedNoDenormals noDenormals;
    const double sr = s.sampleRate;
    const int L = dry.getNumSamples();
    const int N = lengthInSamples (s);
    juce::AudioBuffer<float> result (2, N);
    result.clear();
    if (L <= 0) return result;

    const bool vocal = (s.source == 0);
    const double lenSec = N / sr;

    // 1) reversed, pre-filtered input
    const int maxShift = (int) (0.25 * sr); // search window for the reverb's build-up peak
    const int total = L + maxShift + N;
    std::vector<float> inL ((size_t) total, 0.0f), inR ((size_t) total, 0.0f);
    {
        Biquad hpL, hpR, lpL, lpR;
        hpL.setHighPass (sr, vocal ? 160.0 : 45.0); hpR = hpL;
        lpL.setLowPass  (sr, vocal ? 12000.0 : 16000.0); lpR = lpL;
        const float* dl = dry.getReadPointer (0);
        const float* dr = dry.getReadPointer (juce::jmin (1, dry.getNumChannels() - 1));
        for (int i = 0; i < L; ++i)
        {
            inL[(size_t) i] = lpL.process (hpL.process (dl[L - 1 - i]));
            inR[(size_t) i] = lpR.process (hpR.process (dr[L - 1 - i]));
        }
    }

    // 2) reverb (wet only). Decay scales with the chosen length so the swell always fills it.
    const float sizeScale = 0.65f + 0.95f * s.size;
    const float rt60 = (float) juce::jlimit (0.6, 30.0, lenSec * (0.45 + 1.6 * s.size) * (vocal ? 0.9 : 1.05));
    const float dampHz = (float) (2200.0 * std::pow (6.5, (double) s.tone) * (vocal ? 1.2 : 0.9));
    FDNReverb rev;
    rev.prepare (sr, sizeScale, rt60, dampHz, vocal ? 0.68f : 0.62f, vocal ? 12.0 : 4.0);

    const int W = maxShift + N;
    std::vector<float> wetL ((size_t) W), wetR ((size_t) W);
    {
        constexpr int block = 4096;
        std::vector<float> oL (block), oR (block);
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            rev.process (inL.data() + pos, inR.data() + pos, oL.data(), oR.data(), n);
            for (int i = 0; i < n; ++i)
            {
                const int idx = pos + i - L; // tail after the reversed sound ended
                if (idx >= 0 && idx < W) { wetL[(size_t) idx] = oL[(size_t) i]; wetR[(size_t) idx] = oR[(size_t) i]; }
            }
        }
    }

    // 3) skip the reverb's short build-up so the swell peaks right at the hit
    int shift = 0;
    {
        const int win = juce::jmax (1, (int) (0.004 * sr));
        float best = -1.0f;
        for (int st = 0; st + win <= maxShift; st += win)
        {
            float e = 0.0f;
            for (int k = st; k < st + win; ++k) e += std::abs (wetL[(size_t) k]) + std::abs (wetR[(size_t) k]);
            if (e > best) { best = e; shift = st; }
        }
    }

    // 4) reverse the tail -> swell that rises into the original onset
    float* rl = result.getWritePointer (0);
    float* rr = result.getWritePointer (1);
    for (int i = 0; i < N; ++i) { rl[i] = wetL[(size_t) (shift + N - 1 - i)]; rr[i] = wetR[(size_t) (shift + N - 1 - i)]; }

    // 5) tone + low cut (forward time)
    {
        const double toneHz = 1600.0 * std::pow (11.0, (double) s.tone);
        Biquad tl, tr, hl, hr, dcl, dcr;
        tl.setLowPass (sr, toneHz); tr = tl;
        hl.setHighPass (sr, vocal ? 280.0 : 220.0, 0.8); hr = hl;
        dcl.setHighPass (sr, 25.0); dcr = dcl;
        tl.processBuffer (rl, N); tr.processBuffer (rr, N);
        dcl.processBuffer (rl, N); dcr.processBuffer (rr, N);
        if (s.lowCut) { hl.processBuffer (rl, N); hr.processBuffer (rr, N); }
    }

    // 6) stereo width (0 = mono, 0.5 = natural, 1 = extra wide)
    {
        const float sideGain = s.width * 2.0f;
        for (int i = 0; i < N; ++i)
        {
            const float m = 0.5f * (rl[i] + rr[i]), sd = 0.5f * (rl[i] - rr[i]) * sideGain;
            rl[i] = m + sd; rr[i] = m - sd;
        }
    }

    // 7) tempo-synced tremolo, phase-locked so the swell peaks exactly at the hit
    if (s.tremOn && s.tremDepth > 0.0f)
    {
        const double period = tremDivisionBeats (s.tremRateIndex) * 60.0 / s.bpm * sr;
        for (int i = 0; i < N; ++i)
        {
            const double ph = (double) (N - 1 - i) / period;
            const float gmod = 1.0f - s.tremDepth * (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * ph));
            rl[i] *= gmod; rr[i] *= gmod;
        }
    }

    // 8) envelope: soft fade-in, micro fade-out (no click at the hit)
    {
        const int fi = juce::jmax (1, (int) (N * 0.12));
        for (int i = 0; i < fi; ++i) { const float k = (float) i / (float) fi; rl[i] *= k * k; rr[i] *= k * k; }
        const int fo = juce::jmin (N, (int) (0.002 * sr));
        result.applyGainRamp (0, N - fo, fo, 1.0f, 0.0f);
        result.applyGainRamp (1, N - fo, fo, 1.0f, 0.0f);
    }

    if (s.normalize) normalizeTo (result, 0.891f); // -1 dBFS
    else             result.applyGain (0.5f);
    return result;
}

/** Peak overview for the waveform display. */
inline std::vector<float> computePeaks (const juce::AudioBuffer<float>& b, int bins)
{
    std::vector<float> p ((size_t) bins, 0.0f);
    const int n = b.getNumSamples();
    if (n == 0) return p;
    for (int k = 0; k < bins; ++k)
    {
        const int s0 = (int) ((int64_t) k * n / bins), s1 = juce::jmax (s0 + 1, (int) ((int64_t) (k + 1) * n / bins));
        float m = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c) m = juce::jmax (m, b.getMagnitude (c, s0, juce::jmin (s1, n) - s0));
        p[(size_t) k] = m;
    }
    return p;
}

} // namespace brv
