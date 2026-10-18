#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "DSP.h"

// Linear-phase 4-band split as a uniformly partitioned FFT convolution (overlap-save).
// All four band filters share one history of the input, so when new filters arrive
// (knob or crossover moved) the switch is a one-block cross-fade on the same history:
// no gaps, no clicks, no stutter.
class LinPhaseSplitter
{
public:
    static constexpr int P = 256;          // block / partition size (adds P samples of latency)
    static constexpr int F = 2 * P;        // FFT size
    static constexpr int bins = P + 1;

    // Spectra of the four band kernels, partition by partition
    struct Spectra
    {
        int K = 0;
        double rate = 0.0;
        std::vector<float> h[4];           // K * bins complex values (re, im interleaved)
    };

    static std::unique_ptr<Spectra> makeSpectra (const Dsp::KernelSet& ks, double rate)
    {
        auto s = std::make_unique<Spectra>();
        const int n = ks.band[0].getNumSamples();
        s->K = n / P;
        s->rate = rate;
        juce::dsp::FFT fft (9);            // 512
        std::vector<float> buf ((size_t) F * 2);
        for (int b = 0; b < 4; ++b)
        {
            s->h[b].assign ((size_t) s->K * bins * 2, 0.0f);
            const float* k = ks.band[(size_t) b].getReadPointer (0);
            for (int p = 0; p < s->K; ++p)
            {
                std::fill (buf.begin(), buf.end(), 0.0f);
                std::copy (k + p * P, k + (p + 1) * P, buf.begin());
                fft.performRealOnlyForwardTransform (buf.data(), true);
                std::copy (buf.begin(), buf.begin() + bins * 2, s->h[b].begin() + (size_t) p * bins * 2);
            }
        }
        return s;
    }

    ~LinPhaseSplitter() { delete mailbox.exchange (nullptr); delete graveyard.exchange (nullptr); }

    // Call while audio is stopped. Takes the first set of kernels.
    void prepare (std::unique_ptr<Spectra> first)
    {
        current = std::move (first);
        next.reset();
        K = current->K;
        for (auto& c : chans)
        {
            c.frame.assign ((size_t) F * 2, 0.0f);
            c.prev.assign ((size_t) P, 0.0f);
            c.history.assign ((size_t) K * bins * 2, 0.0f);
            for (auto& o : c.out) o.assign ((size_t) P, 0.0f);
        }
        acc.assign ((size_t) F * 2, 0.0f);
        acc2.assign ((size_t) F * 2, 0.0f);
        ring = 0;
        pos = 0;
    }

    void reset()
    {
        for (auto& c : chans)
        {
            std::fill (c.prev.begin(), c.prev.end(), 0.0f);
            std::fill (c.history.begin(), c.history.end(), 0.0f);
            for (auto& o : c.out) std::fill (o.begin(), o.end(), 0.0f);
        }
        pos = 0;
    }

    // ---- from the designer thread
    void offer (std::unique_ptr<Spectra> s)
    {
        delete mailbox.exchange (s.release());     // an unclaimed older offer is simply replaced
        delete graveyard.exchange (nullptr);      // free what the audio thread retired
    }

    // ---- audio thread: in[ch] -> out[band][ch]
    void process (const float* const* in, float* const (*out)[2], int n)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& c = chans[ch];
                c.cur[(size_t) pos] = in[ch][i];
                for (int b = 0; b < 4; ++b) out[b][ch][i] = c.out[b][(size_t) pos];
            }
            if (++pos == P)
            {
                pos = 0;
                computeBlock();
            }
        }
    }

    int latency() const { return K * P / 2 + P; }

private:
    struct Chan
    {
        std::array<float, P> cur {};
        std::vector<float> prev, frame, history;
        std::vector<float> out[4];
    };
    Chan chans[2];
    std::unique_ptr<Spectra> current, next;
    std::atomic<Spectra*> mailbox { nullptr }, graveyard { nullptr };
    juce::dsp::FFT fft { 9 };
    std::vector<float> acc, acc2;
    int K = 0, ring = 0, pos = 0;

    static void multiplyAdd (float* y, const float* x, const float* h)
    {
        for (int k = 0; k < bins; ++k)
        {
            const float xr = x[2 * k], xi = x[2 * k + 1], hr = h[2 * k], hi = h[2 * k + 1];
            y[2 * k] += xr * hr - xi * hi;
            y[2 * k + 1] += xr * hi + xi * hr;
        }
    }

    void convolve (const Chan& c, const Spectra& s, int band, std::vector<float>& y)
    {
        std::fill (y.begin(), y.end(), 0.0f);
        for (int p = 0; p < K; ++p)
        {
            const int slot = (ring - p + K) % K;
            multiplyAdd (y.data(), c.history.data() + (size_t) slot * bins * 2, s.h[band].data() + (size_t) p * bins * 2);
        }
        fft.performRealOnlyInverseTransform (y.data());
    }

    void computeBlock()
    {
        // pick up new kernels (only one switch in flight; the old ones go to the graveyard)
        if (next == nullptr && graveyard.load() == nullptr)
            if (auto* s = mailbox.exchange (nullptr))
            {
                if (s->K == K) next.reset (s);
                else delete s;     // different rate: wait for prepare()
            }

        ring = (ring + 1) % K;
        for (auto& c : chans)
        {
            std::copy (c.prev.begin(), c.prev.end(), c.frame.begin());
            std::copy (c.cur.begin(), c.cur.end(), c.frame.begin() + P);
            std::fill (c.frame.begin() + F, c.frame.end(), 0.0f);
            std::copy (c.cur.begin(), c.cur.end(), c.prev.begin());
            fft.performRealOnlyForwardTransform (c.frame.data(), true);
            std::copy (c.frame.begin(), c.frame.begin() + bins * 2, c.history.begin() + (size_t) ring * bins * 2);

            for (int b = 0; b < 4; ++b)
            {
                convolve (c, *current, b, acc);
                if (next != nullptr)
                {
                    convolve (c, *next, b, acc2);
                    for (int i = 0; i < P; ++i)
                    {
                        const float r = (float) (i + 1) / (float) P;
                        c.out[b][(size_t) i] = acc[(size_t) (P + i)] + (acc2[(size_t) (P + i)] - acc[(size_t) (P + i)]) * r;
                    }
                }
                else
                    std::copy (acc.begin() + P, acc.begin() + F, c.out[b].begin());
            }
        }
        if (next != nullptr)
        {
            graveyard.store (current.release());
            current = std::move (next);
        }
    }
};
