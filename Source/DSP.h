#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>

// Building blocks of the Breathe sound. Everything here is allocation-free once prepared.
namespace Dsp
{
    constexpr double pi = juce::MathConstants<double>::pi;

    // Knob position -> amount. The first half of the knob is the finest part of the travel.
    // A tiny dead zone at the bottom makes "zero" really zero, whatever the host sends back.
    constexpr float deadZone = 0.001f;
    inline bool isOn (float knob) { return knob >= deadZone; }
    inline float taper (float knob) { return isOn (knob) ? std::pow (juce::jlimit (0.0f, 1.0f, knob), 1.4f) : 0.0f; }

    // How hard each part works at full knob, per mode. MASTER is the default.
    struct ModeScale { float sat, opto[4], lift[4], life, eq; };
    constexpr ModeScale master { 2.5f, { 3.5f, 2.0f, 0.0f, 0.0f }, { 0.0f, 0.3f, 3.5f, 4.5f }, 1.5f, 1.33f };
    constexpr ModeScale mixMode { 7.0f, { 6.0f, 4.0f, 0.0f, 0.0f }, { 0.0f, 0.6f, 6.0f, 8.0f }, 3.0f, 4.5f };
    inline ModeScale blend (float m)
    {
        auto l = [m] (float a, float b) { return a + (b - a) * m; };
        ModeScale r {};
        r.sat = l (master.sat, mixMode.sat); r.life = l (master.life, mixMode.life); r.eq = l (master.eq, mixMode.eq);
        for (int i = 0; i < 4; ++i) { r.opto[i] = l (master.opto[i], mixMode.opto[i]); r.lift[i] = l (master.lift[i], mixMode.lift[i]); }
        return r;
    }

    //==========================================================================
    // Plain biquad (RBJ cookbook), transposed direct form II
    struct BiquadCoefs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

        static BiquadCoefs fromRaw (double b0, double b1, double b2, double a0, double a1, double a2)
        {
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }
        static BiquadCoefs peak (double fs, double f0, double q, double db)
        {
            if (std::abs (db) < 1.0e-4) return {};
            f0 = juce::jlimit (10.0, fs * 0.45, f0);
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return fromRaw (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        static BiquadCoefs lowShelf (double fs, double f0, double q, double db)
        {
            if (std::abs (db) < 1.0e-4) return {};
            f0 = juce::jlimit (10.0, fs * 0.45, f0);
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q), s = 2.0 * std::sqrt (A) * al;
            return fromRaw (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
                            (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
        }
        static BiquadCoefs highShelf (double fs, double f0, double q, double db)
        {
            if (std::abs (db) < 1.0e-4) return {};
            f0 = juce::jlimit (10.0, fs * 0.45, f0);
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q), s = 2.0 * std::sqrt (A) * al;
            return fromRaw (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
                            (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
        }
        // |H| at frequency f
        double magnitude (double fs, double f) const
        {
            const double w = 2.0 * pi * f / fs;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
            return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
        }
    };

    struct Biquad
    {
        BiquadCoefs k;
        double z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        float process (float xf)
        {
            const double x = xf, y = k.b0 * x + z1;
            z1 = k.b1 * x - k.a1 * y + z2;
            z2 = k.b2 * x - k.a2 * y;
            return (float) y;
        }
    };

    //==========================================================================
    // The Pultec-style curve, at ~10 % of the real unit's depth.
    // Every band knob drives its own part of the curve; it follows the crossovers.
    constexpr int pultecStages = 6;

    inline std::array<BiquadCoefs, pultecStages> pultec (double fs, const float e0[4], const float xo[3], float scale)
    {
        const double e[4] { e0[0] * scale, e0[1] * scale, e0[2] * scale, e0[3] * scale };
        return {
            BiquadCoefs::lowShelf (fs, juce::jlimit (25.0, 120.0, xo[0] * 0.5), 0.9, 1.5 * e[0]),          // big low end
            BiquadCoefs::peak (fs, juce::jlimit (120.0, 500.0, xo[0] * 1.7), 0.8, -0.7 * e[0]),            // ...and the dip above it
            BiquadCoefs::peak (fs, std::sqrt ((double) xo[0] * xo[1]), 0.6, -0.5 * e[1]),                  // mud
            BiquadCoefs::peak (fs, juce::jlimit (2500.0, 8000.0, std::sqrt ((double) xo[1] * xo[2]) * 1.8), 0.7, 0.5 * e[2]),   // presence
            BiquadCoefs::peak (fs, juce::jlimit (7000.0, 16000.0, xo[2] * 1.8), 0.5, 1.3 * e[3]),          // air
            BiquadCoefs::highShelf (fs, juce::jmin (17000.0, fs * 0.42), 0.7, -0.8 * e[3]),                // soft top
        };
    }

    //==========================================================================
    // Advanced trims (t = -1..+1, 0 = the factory recipe)
    constexpr float eqTrimMax = 9.0f;           // +100 %: a real Pultec (about +13.5 dB of low boost at full knob)
    inline float eqTrim (float base, float t)   { return t < 0.0f ? base * (1.0f + t) : base + t * (eqTrimMax - base); }
    inline float driveTrim (float t)            { return t < 0.0f ? 1.0f + t : std::pow (10.0f, 12.0f * t / 20.0f); }   // -100 % clean, +100 % +12 dB
    inline float dynTrim (float t)              { return 1.0f + t; }                                                        // -100 % off, +100 % double

    //==========================================================================
    // Harmonic recipe of a band, by its centre frequency. Values = THD (fraction) before the mode scale.
    struct Recipe { float transformer, tape, tube; };

    inline Recipe recipeAt (float centreHz)
    {
        struct Anchor { float f; Recipe r; };
        static const Anchor a[] = {
            { 50.0f,    { 0.006f, 0.012f, 0.008f } },
            { 350.0f,   { 0.000f, 0.003f, 0.010f } },
            { 2400.0f,  { 0.000f, 0.002f, 0.005f } },
            { 11000.0f, { 0.000f, 0.008f, 0.004f } },
        };
        const float lf = std::log (juce::jlimit (a[0].f, a[3].f, centreHz));
        for (int i = 0; i < 3; ++i)
        {
            const float l0 = std::log (a[i].f), l1 = std::log (a[i + 1].f);
            if (lf <= l1)
            {
                const float t = (lf - l0) / (l1 - l0);
                auto m = [t] (float x, float y) { return x + (y - x) * t; };
                return { m (a[i].r.transformer, a[i + 1].r.transformer), m (a[i].r.tape, a[i + 1].r.tape), m (a[i].r.tube, a[i + 1].r.tube) };
            }
        }
        return a[3].r;
    }


    //==========================================================================
    // Slow random wander (analog drift), output roughly -1..1
    struct Drift
    {
        juce::Random rnd;
        float target = 0, value = 0, slew = 0, step = 0;
        int count = 0, period = 1;
        void prepare (double fs, float rateHz, juce::int64 seed)
        {
            rnd.setSeed (seed);
            period = juce::jmax (1, (int) (fs / rateHz));
            slew = 1.0f / (float) period;
            target = rnd.nextFloat() * 2.0f - 1.0f;
            value = target;
            count = 0;
        }
        float next()
        {
            if (++count >= period)
            {
                count = 0;
                target = rnd.nextFloat() * 2.0f - 1.0f;
                step = (target - value) * slew;
            }
            value += step;
            return value;
        }
    };

    //==========================================================================
    // Scrape flutter: the tape rubbing the heads. A fast, noisy shiver of level on what sits above ~500 Hz,
    // its own random per channel, zero on average. Everything the shiver produces below 200 Hz is filtered out,
    // so the low end stays exactly as it is. No delay line, no latency, the average curve does not move.
    struct ScrapeFlutter
    {
        static constexpr int delay = 0;

        struct HP2   // 2nd-order Butterworth high-pass, TDF-II
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            void set (double fs, double fc)
            {
                const double w = 2.0 * pi * fc / fs, c = std::cos (w), al = std::sin (w) / std::sqrt (2.0), a0 = 1.0 + al;
                b0 = (float) ((1.0 + c) / 2.0 / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
                a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0); z1 = z2 = 0;
            }
            float operator() (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        };

        void prepare (double sampleRate)
        {
            fs = sampleRate;
            hpN = (float) (1.0 - std::exp (-2.0 * pi * 400.0 / fs));
            lpN = (float) (1.0 - std::exp (-2.0 * pi * 4000.0 / fs));
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[c];
                ch = {};
                ch.rnd.setSeed (777 + c * 31);
                ch.split.set (fs, 500.0);  ch.split2.set (fs, 500.0);
                ch.clean.set (fs, 200.0);  ch.clean2.set (fs, 200.0);
            }
        }

        // amount: 0 = new tape, 1 = AGE full in MASTER, 2 = AGE full in MIX
        void process (float* l, float* r, int n, float amount)
        {
            if (amount <= 0.0f && ! ringing) return;
            ringing = amount > 0.0f;
            const float rip = amount * 0.06f;
            float* io[2] { l, r };
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[c];
                for (int i = 0; i < n; ++i)
                {
                    const float x = io[c][i];
                    const float hi = ch.split2 (ch.split (x));
                    // band-limited noise 400 Hz - 4 kHz: the scrape
                    const float w = ch.rnd.nextFloat() * 2.0f - 1.0f;
                    ch.h += hpN * (w - ch.h);
                    ch.lp += lpN * ((w - ch.h) - ch.lp);
                    const float a = juce::jlimit (-1.0f, 1.0f, ch.lp * 2.5f);
                    const float delta = ch.clean2 (ch.clean (hi * rip * a));
                    io[c][i] = x + delta;
                }
            }
        }

    private:
        struct Chan { juce::Random rnd; HP2 split, split2, clean, clean2; float h = 0, lp = 0; };
        Chan chans[2];
        double fs = 48000.0;
        float hpN = 0, lpN = 0;
        bool ringing = false;
    };


    //==========================================================================
    // Fast random shimmer (band-limited noise ~3-30 Hz), output roughly -1..1
    struct Shimmer
    {
        juce::Random rnd;
        float lp1 = 0, lp2 = 0, hp = 0, c1 = 0, c2 = 0;
        void prepare (double fs, juce::int64 seed)
        {
            rnd.setSeed (seed);
            c1 = (float) (1.0 - std::exp (-2.0 * pi * 30.0 / fs));
            c2 = (float) (1.0 - std::exp (-2.0 * pi * 3.0 / fs));
            lp1 = lp2 = hp = 0;
        }
        float next()
        {
            const float w = rnd.nextFloat() * 2.0f - 1.0f;
            lp1 += c1 * (w - lp1);
            lp2 += c2 * (lp1 - lp2);
            return juce::jlimit (-1.0f, 1.0f, (lp1 - lp2) * 9.0f);
        }
    };

    //==========================================================================
    // Non-linear stage of one band, one channel. Runs at the oversampled rate.
    // Tube: biased tanh (even + odd), tape: tanh with a bit of magnetic memory,
    // transformer: saturates with low-frequency flux.
    struct Saturator
    {
        float tapePrevX = 0, tapePrevY = 0, flux = 0, fluxCoef = 0.01f;

        void prepare (double osRate)
        {
            fluxCoef = (float) (1.0 - std::exp (-2.0 * pi * 90.0 / osRate));
            reset();
        }
        void reset() { tapePrevX = tapePrevY = flux = 0; }

        static inline float tube (float u)
        {
            if (u >= 0.0f) return u / std::sqrt (1.0f + u * u);                          // soft
            const float u2 = u * u, u4 = u2 * u2;
            return u / std::sqrt (std::sqrt (std::sqrt (1.0f + u4 * u4)));                // firmer knee
        }
        static inline float tubeSlope (float u)                                           // d tube / du (u >= 0)
        {
            const float q = 1.0f + u * u;
            return 1.0f / (q * std::sqrt (q));
        }

        // In series, like a real chain: tube -> tape -> transformer.
        // gTube, bTube: tube drive and bias; gTape: tape drive; tXf: transformer amount;
        // jitter: fast tiny wobble of the harmonics only (the clean signal is not touched by it)
        inline float process (float x, float gTube, float bTube, float gTape, float tXf, float jitter)
        {
            float y = x;
            if (gTube > 1.0e-4f)
            {
                // triode-like: soft on one half, a firmer knee on the other (grid conduction).
                // The firmer knee is what gives the long, dense series of harmonics.
                const float u0 = gTube * bTube;
                const float y0 = tube (u0), slope = tubeSlope (u0);
                y = (tube (gTube * (y + bTube)) - y0) / (gTube * slope);
            }
            if (gTape > 1.0e-4f)
            {
                const float u = y + 0.12f * (tapePrevY - tapePrevX);
                const float t = std::tanh (gTape * u) / gTape;
                tapePrevX = y;
                tapePrevY = t;
                y = t;
            }
            if (tXf > 1.0e-6f)
            {
                flux += fluxCoef * (y - flux);
                y -= y * 0.3f * std::tanh (tXf * flux * flux * 3.3333f);     // soft: never squeezes more than 30 %
            }
            return x + (y - x) * (1.0f + jitter);
        }
    };

    // THD target -> drive values. Reference: a band playing at -18 dBFS RMS (typical of real music)
    constexpr float refAmp = 0.178f;
    inline float tubeDrive (float thd)  { return std::sqrt (7.2f * thd) / refAmp; }        // ~60 % of THD as the 3rd harmonic
    inline float tubeBias (float thd, float g)
    {
        if (g < 1.0e-4f) return 0.0f;
        const float s = juce::jlimit (0.0f, 0.5f, 1.6f * thd / (g * refAmp));            // ~80 % as the 2nd harmonic
        return std::atanh (s) / g;
    }
    inline float tapeDrive (float thd)  { return std::sqrt (12.0f * thd) / refAmp; }
    inline float transformerAmount (float thd) { return 4.0f * thd / (refAmp * refAmp); }

    //==========================================================================
    // Linear-phase band split: zero-phase masks with LR4-shaped slopes (24 dB/oct),
    // with the Pultec curve folded in. Kernels sum to a pure delay when the curve is flat.
    struct KernelSet
    {
        std::array<juce::AudioBuffer<float>, 4> band;
    };

    inline int kernelSizeFor (double fs)
    {
        return juce::nextPowerOfTwo ((int) std::ceil (fs * 0.08));
    }

    inline void designKernels (KernelSet& out, double fs, const float xo[3], const std::array<BiquadCoefs, pultecStages>& eq)
    {
        const int n = kernelSizeFor (fs);
        const int order = juce::roundToInt (std::log2 ((double) n));
        juce::dsp::FFT fft (order);
        std::vector<float> spec ((size_t) n * 2);
        for (int b = 0; b < 4; ++b)
        {
            std::fill (spec.begin(), spec.end(), 0.0f);
            for (int k = 0; k <= n / 2; ++k)
            {
                const double f = (double) k * fs / (double) n;
                auto lp = [f] (double fc) { const double r = f / fc, r4 = r * r * r * r; return 1.0 / (1.0 + r4); };
                const double l1 = lp (xo[0]), l2 = lp (xo[1]), l3 = lp (xo[2]);
                double m = b == 0 ? l1 : b == 1 ? l2 - l1 : b == 2 ? l3 - l2 : 1.0 - l3;
                double g = 1.0;
                for (const auto& c : eq) g *= c.magnitude (fs, juce::jmax (1.0, f));
                spec[(size_t) k * 2] = (float) (m * g);
            }
            fft.performRealOnlyInverseTransform (spec.data());
            auto& buf = out.band[(size_t) b];
            buf.setSize (1, n, false, false, true);
            auto* d = buf.getWritePointer (0);
            for (int i = 0; i < n; ++i)
            {
                const int src = (i + n / 2) % n;                                   // centre the zero-phase response
                const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) n);   // periodic Hann, 1 at the centre
                d[i] = (float) (spec[(size_t) src] * w);
            }
        }
    }
}
