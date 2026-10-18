// Offline test for Breathe: transparency at zero, band split, harmonics, opto, lift, Pultec curve,
// latency, CPU, and screenshots of the panel in every theme.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr double SR = 48000.0;
constexpr int BS = 512;

void setP (BreatheProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void defaults (BreatheProcessor& p, bool lin, int ovs)
{
    for (int b = 0; b < 4; ++b) { setP (p, Params::band[b], 0); setP (p, Params::solo[b], 0); setP (p, Params::mute[b], 0); }
    for (int i = 0; i < 3; ++i) setP (p, Params::xover[i], Params::xoDef[i]);
    setP (p, Params::mix, 1); setP (p, Params::age, 0.4f); setP (p, Params::output, 0);
    setP (p, Params::lin, lin ? 1.0f : 0.0f); setP (p, Params::ovs, (float) ovs); setP (p, Params::bypass, 0);
    setP (p, Params::input, 0); setP (p, Params::mode, 0);
}

AudioBuffer<float> run (BreatheProcessor& p, const AudioBuffer<float>& in, int bs = BS)
{
    AudioBuffer<float> out (in);
    MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += bs)
    {
        const int n = jmin (bs, out.getNumSamples() - pos);
        AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, pos, n);
        p.processBlock (blk, midi);
    }
    return out;
}

AudioBuffer<float> sine (float f, float secs, float amp)
{
    AudioBuffer<float> b (2, (int) (SR * secs));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float v = amp * std::sin (MathConstants<float>::twoPi * f * (float) i / (float) SR);
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    return b;
}

AudioBuffer<float> noise (float secs, float amp, int seed)
{
    Random r (seed);
    AudioBuffer<float> b (2, (int) (SR * secs));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float v = amp * (r.nextFloat() * 2.0f - 1.0f);
        b.setSample (0, i, v); b.setSample (1, i, 0.7f * v + 0.3f * amp * (r.nextFloat() * 2.0f - 1.0f));
    }
    return b;
}

float rms (const AudioBuffer<float>& b, int ch, int start, int len)
{
    double s = 0;
    for (int i = start; i < start + len; ++i) s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return (float) std::sqrt (s / jmax (1, len));
}

// amplitude of harmonic k of f in a window (single-bin DFT)
float harmonic (const AudioBuffer<float>& b, int ch, int start, int len, float f)
{
    double re = 0, im = 0;
    for (int i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * MathConstants<double>::pi * i / (len - 1));
        const double ph = 2.0 * MathConstants<double>::pi * f * (start + i) / SR;
        re += w * b.getSample (ch, start + i) * std::cos (ph);
        im += w * b.getSample (ch, start + i) * std::sin (ph);
    }
    return (float) (2.0 * std::sqrt (re * re + im * im) / (len * 0.5));
}

void prepare (BreatheProcessor& p)
{
    p.prepareToPlay (SR, BS);
}
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir (argc > 1 ? argv[1] : "/tmp/brt");
    outDir.createDirectory();
    int failures = 0;
    auto check = [&] (bool ok, const String& what) { std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n"; if (! ok) ++failures; };

    BreatheProcessor p;
    p.setPlayConfigDetails (2, 2, SR, BS);

    if (argc > 2 && String (argv[2]) == "lm")
    {
        for (float lm : { 0.0f, 1.0f })
        {
            defaults (p, true, 1);
            setP (p, Params::band[0], 1); setP (p, Params::band[2], 1); setP (p, Params::band[3], 1); setP (p, Params::band[1], lm);
            prepare (p);
            String line = "LM " + String (lm) + ": ";
            for (float f : { 60.0f, 120.0f, 200.0f, 350.0f, 600.0f, 1000.0f })
            {
                auto o = run (p, sine (f, 3.0f, 0.05f));
                line << f << " Hz " << String (Decibels::gainToDecibels (rms (o, 0, 120000, 19200) / (0.05f / std::sqrt (2.0f))), 2) << "  ";
            }
            std::cout << line << "\n";
        }
        return 0;
    }

    // ---------------------------------------------------------------- transparency
    for (int lin = 1; lin >= 0; --lin)
    {
        const String mode = lin ? "LIN" : "analog phase";
        std::cout << "== " << mode << "\n";
        for (int ovs : { 0, 2, 4 })
        {
            defaults (p, lin, ovs);
            prepare (p);
            const int lat = p.getLatencySamples();
            auto in = noise (1.0f, 0.5f, 3);
            auto out = run (p, in);
            double err = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = lat; i < in.getNumSamples(); ++i)
                    err = jmax (err, (double) std::abs (out.getSample (ch, i) - in.getSample (ch, i - lat)));
            std::cout << "   ovs " << Params::ovsNames[ovs] << ": latency " << lat << " samples (" << String (lat / SR * 1000.0, 1) << " ms), max error at zero " << err << "\n";
            check (err == 0.0, mode + " " + Params::ovsNames[ovs] + ": all knobs at zero = untouched, bit for bit");
        }

        // band split alone: solo every band (forces the processed path) with knobs at zero
        defaults (p, lin, 1);
        for (int b = 0; b < 4; ++b) setP (p, Params::solo[b], 1);
        prepare (p);
        float worst = 0;
        for (float f : { 40.0f, 100.0f, 120.0f, 250.0f, 1000.0f, 3000.0f, 6000.0f, 12000.0f, 18000.0f })
        {
            auto o = run (p, sine (f, 0.6f, 0.25f));
            const float g = Decibels::gainToDecibels (rms (o, 0, 14400, 14400) / (0.25f / std::sqrt (2.0f)));
            worst = jmax (worst, std::abs (g));
        }
        std::cout << "   bands summed back, worst deviation " << worst << " dB\n";
        check (worst < 0.05f, mode + ": the four bands sum back flat");
    }

    // ---------------------------------------------------------------- harmonics per band at full knob
    for (int mode = 0; mode < 2; ++mode)
    {
        std::cout << "== harmonics at full knob, " << (mode ? "MIX" : "MASTER") << " (sine at -18 dBFS RMS in the band)\n";
        const float centres[4] { 60.0f, 350.0f, 2400.0f, 9000.0f };
        for (int b = 0; b < 4; ++b)
        {
            defaults (p, true, 2);
            setP (p, Params::age, 0.0f);
            setP (p, Params::mode, (float) mode);
            setP (p, Params::band[b], 1.0f);
            prepare (p);
            const float f = centres[b];
            auto o = run (p, sine (f, 2.0f, 0.178f));
            const int st = 72000, len = 19200;
            const float h1 = harmonic (o, 0, st, len, f), h2 = harmonic (o, 0, st, len, 2 * f), h3 = harmonic (o, 0, st, len, 3 * f);
            const float thd = std::sqrt (h2 * h2 + h3 * h3) / h1 * 100.0f;
            std::cout << "   " << Params::bandName[b] << " @ " << f << " Hz: H2 " << String (h2 / h1 * 100.0f, 2) << " %, H3 " << String (h3 / h1 * 100.0f, 2)
                      << " %, THD " << String (thd, 2) << " %\n";
            check (thd < (mode ? 9.0f : 5.0f), String (Params::bandName[b]) + ": harmonics within the planned range");
        }
    }

    // ---------------------------------------------------------------- Pultec curve (steady quiet sines: no opto, no lift)
    std::cout << "== Pultec curve, every knob at full\n";
    for (int pass = 0; pass < 3; ++pass)
    {
        const int lin = pass == 1 ? 0 : 1, mode = pass == 2 ? 1 : 0;
        defaults (p, lin, 1);
        setP (p, Params::mode, (float) mode);
        for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 1.0f);
        prepare (p);
        String line = mode ? "   MIX:    " : lin ? "   LIN:    " : "   analog: ";
        float maxBoost = 0;
        for (float f : { 30.0f, 60.0f, 200.0f, 350.0f, 1000.0f, 4400.0f, 11000.0f, 18000.0f })
        {
            auto o = run (p, sine (f, 3.0f, 0.05f));
            const float g = Decibels::gainToDecibels (rms (o, 0, 120000, 19200) / (0.05f / std::sqrt (2.0f)));
            line << f << " Hz " << String (g, 2) << " dB   ";
            maxBoost = jmax (maxBoost, g);
        }
        std::cout << line << "\n";
        check (maxBoost < (mode ? 8.0f : 2.4f), String (mode ? "MIX" : lin ? "LIN" : "analog") + ": the curve tops out where planned");
    }

    // ---------------------------------------------------------------- opto on LOW: bass bursts
    {
        std::cout << "== opto on LOW (bass hits)\n";
        defaults (p, true, 1);
        setP (p, Params::band[0], 1.0f);
        prepare (p);
        AudioBuffer<float> in (2, (int) SR * 4);
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const double t = i / SR, ph = std::fmod (t, 0.5);
            const float env = (float) (0.15 + 0.6 * std::exp (-ph * 10.0));
            const float v = env * (float) std::sin (2.0 * MathConstants<double>::pi * 55.0 * t);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        float maxGr = 0;
        MidiBuffer midi;
        AudioBuffer<float> out (in);
        for (int pos = 0; pos < out.getNumSamples(); pos += BS)
        {
            AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, pos, BS);
            p.processBlock (blk, midi);
            if (pos > SR) maxGr = jmax (maxGr, p.optoGr[0].load());
        }
        const float inR = rms (in, 0, (int) SR * 2, (int) SR * 2), outR = rms (out, 0, (int) SR * 2 + p.getLatencySamples(), (int) SR * 2 - p.getLatencySamples());
        std::cout << "   max gain reduction " << maxGr << " dB, level change " << Decibels::gainToDecibels (outR / inR) << " dB\n";
        check (maxGr > 0.5f && maxGr <= 2.6f, "LOW opto squeezes up to ~2.5 dB");
        check (std::abs (Decibels::gainToDecibels (outR / inR)) < 1.5f, "LOW keeps its level");
    }

    // ---------------------------------------------------------------- lift on HIGH: loud then quiet
    {
        std::cout << "== quiet lift on HIGH\n";
        defaults (p, true, 1);
        setP (p, Params::band[3], 1.0f);
        prepare (p);
        AudioBuffer<float> in (2, (int) SR * 6);
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const double t = i / SR;
            const float amp = t < 4.0 ? 0.3f : 0.012f;
            const float v = amp * (float) std::sin (2.0 * MathConstants<double>::pi * 9000.0 * t);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        auto out = run (p, in);
        const int lat = p.getLatencySamples();
        const float loud = rms (out, 0, (int) (SR * 3) + lat, 9600) / rms (in, 0, (int) (SR * 3), 9600);
        const float quiet = rms (out, 0, (int) (SR * 4.2) + lat, 9600) / rms (in, 0, (int) (SR * 4.2), 9600);
        std::cout << "   loud part " << Decibels::gainToDecibels (loud) << " dB, quiet part right after " << Decibels::gainToDecibels (quiet) << " dB\n";
        check (Decibels::gainToDecibels (quiet) - Decibels::gainToDecibels (loud) > 1.0f, "HIGH lifts the quiet part more than the loud");
    }

    // ---------------------------------------------------------------- no stutter while knobs / crossovers move (LIN)
    for (int what = 0; what < 2; ++what)
    {
        defaults (p, true, 1);
        for (int b = 0; b < 4; ++b) setP (p, Params::band[b], what == 0 ? 0.0f : 0.6f);
        setP (p, Params::mode, 1);
        prepare (p);
        AudioBuffer<float> in (2, (int) SR * 3);
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const double t = i / SR;
            const float v = 0.15f * (float) (std::sin (2 * MathConstants<double>::pi * 90 * t) + std::sin (2 * MathConstants<double>::pi * 700 * t) + std::sin (2 * MathConstants<double>::pi * 5000 * t));
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        AudioBuffer<float> out (in);
        MidiBuffer midi;
        const int nb = out.getNumSamples() / BS;
        for (int k = 0; k < nb; ++k)
        {
            const float ph = (float) k / (float) nb;
            if (what == 0) for (int b = 0; b < 4; ++b) setP (p, Params::band[b], std::fmod (ph * 3.0f, 1.0f));      // three full sweeps
            else setP (p, Params::xover[1], 400.0f + 2500.0f * (0.5f + 0.5f * std::sin (ph * 12.0f)));                 // crossover wobbling
            p.refreshKernelsNow();
            AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, k * BS, BS);
            p.processBlock (blk, midi);
        }
        // the envelope may glide but must never drop out: compare neighbouring 128-sample windows
        float worst = 0;
        const int w = 1200, hop = 256;    // windows longer than the lowest tone's period
        for (int i = (int) SR / 2; i + w + hop < nb * BS; i += hop)
        {
            const float a = rms (out, 0, i, w), b = rms (out, 0, i + hop, w);
            worst = jmax (worst, std::abs (Decibels::gainToDecibels (b / jmax (1.0e-6f, a))));
        }
        std::cout << "== " << (what == 0 ? "knobs sweeping" : "crossover moving") << " in LIN: largest jump between neighbouring 25 ms windows (5 ms apart) " << worst << " dB\n";
        check (worst < 0.5f, String (what == 0 ? "knobs" : "crossover") + " move smoothly, no stutter");
    }

    // ---------------------------------------------------------------- IN: more colour, same level
    {
        float lvl[2], thd[2];
        for (int k = 0; k < 2; ++k)
        {
            defaults (p, true, 2);
            setP (p, Params::band[1], 0.6f);
            setP (p, Params::input, k == 0 ? 0.0f : 12.0f);
            prepare (p);
            auto o = run (p, sine (350.0f, 3.0f, 0.1f));
            const int st = 96000, len = 19200;
            const float h1 = harmonic (o, 0, st, len, 350.0f), h2 = harmonic (o, 0, st, len, 700.0f), h3 = harmonic (o, 0, st, len, 1050.0f);
            lvl[k] = Decibels::gainToDecibels (rms (o, 0, st, len));
            thd[k] = std::sqrt (h2 * h2 + h3 * h3) / h1 * 100.0f;
        }
        std::cout << "== IN +12 dB: level change " << (lvl[1] - lvl[0]) << " dB, THD " << thd[0] << " % -> " << thd[1] << " %\n";
        check (std::abs (lvl[1] - lvl[0]) < 1.0f && thd[1] > thd[0] * 2.0f, "IN drives the iron harder without changing the level");
    }

    // ---------------------------------------------------------------- bypass keeps latency
    {
        defaults (p, true, 2);
        for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 0.8f);
        setP (p, Params::bypass, 1);
        prepare (p);
        const int lat = p.getLatencySamples();
        auto in = noise (0.5f, 0.4f, 9);
        auto out = run (p, in);
        double err = 0;
        for (int i = lat + 2000; i < in.getNumSamples(); ++i) err = jmax (err, (double) std::abs (out.getSample (0, i) - in.getSample (0, i - lat)));
        check (err < 1.0e-6, "Bypass: original sound, same latency");
    }

    // ---------------------------------------------------------------- CPU
    {
        std::cout << "== CPU (10 s of stereo noise, all knobs at 70 %)\n";
        for (int lin = 1; lin >= 0; --lin)
            for (int ovs : { 1, 2, 4 })
            {
                defaults (p, lin, ovs);
                for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 0.7f);
                prepare (p);
                auto in = noise (10.0f, 0.3f, 5);
                const auto t0 = Time::getMillisecondCounterHiRes();
                auto out = run (p, in);
                const auto ms = Time::getMillisecondCounterHiRes() - t0;
                bool finite = true;
                for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i));
                std::cout << "   " << (lin ? "LIN " : "analog ") << Params::ovsNames[ovs] << ": " << String (ms / 100.0, 2) << " % of one core\n";
                check (finite, "output stays finite");
            }
    }

    // ---------------------------------------------------------------- screenshots
    {
        BreatheProcessor q;
        q.setPlayConfigDetails (2, 2, SR, BS);
        defaults (q, true, 2);
        setP (q, Params::band[0], 0.35f); setP (q, Params::band[1], 0.5f); setP (q, Params::band[2], 0.45f); setP (q, Params::band[3], 0.85f);
        q.prepareToPlay (SR, BS);
        auto in = noise (0.6f, 0.5f, 11);
        for (const auto& theme : Theme::all())
        {
            q.apvts.state.setProperty ("theme", theme.name, nullptr);
            std::unique_ptr<BreatheEditor> ed (dynamic_cast<BreatheEditor*> (q.createEditor()));
            ed->applyTheme (theme.name);
            ed->setSize (BreatheEditor::baseW, BreatheEditor::baseH);
            ed->hideWelcomeForTest();
            for (int i = 0; i < 12; ++i) { auto blk = run (q, in); ed->renderForTest (0.05); }
            if (theme.name == "Asphalt") ed->hoverForTest ({ 820.0f, 450.0f });
            ed->renderForTest (0.5);
            if (theme.name == "Asphalt") std::cout << "   input level seen by IN: " << q.inLevelDb.load() << " dB RMS\n";
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
            File f = outDir.getChildFile ("ui_" + theme.name + ".png");
            f.deleteFile();
            if (auto os = f.createOutputStream()) PNGImageFormat().writeImageToStream (img, *os);
            if (theme.name == "Asphalt")
            {
                ed->startTourForTest (2);
                auto timg = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                File f2 = outDir.getChildFile ("ui_tour.png");
                f2.deleteFile();
                if (auto os = f2.createOutputStream()) PNGImageFormat().writeImageToStream (timg, *os);
            }
        }
    }

    std::cout << (failures == 0 ? "\nALL OK\n" : "\nFAILURES: " + String (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
