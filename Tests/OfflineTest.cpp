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
    setP (p, Params::mix, 1); setP (p, Params::age, 0.0f); setP (p, Params::output, 0);
    setP (p, Params::lin, lin ? 1.0f : 0.0f); setP (p, Params::ovs, (float) ovs); setP (p, Params::bypass, 0);
    setP (p, Params::input, 0); setP (p, Params::mode, 0);
    for (int b = 0; b < 4; ++b) { setP (p, Params::advEq[b], 0); setP (p, Params::advDrive[b], 0); setP (p, Params::advDyn[b], 0); }
    setP (p, Params::autoIn, 0); setP (p, Params::autoOut, 0);
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

    if (argc > 2 && String (argv[2]) == "pd")
    {
        for (float amp : { 0.1f, 1.0f, 10.0f })
            for (int lin = 1; lin >= 0; --lin)
            {
                defaults (p, lin, 2);
                setP (p, Params::band[0], 0.65f); setP (p, Params::band[1], 0.9f); setP (p, Params::band[2], 1); setP (p, Params::band[3], 1);
                setP (p, Params::input, -9.0f); setP (p, Params::age, 0.1f);
                prepare (p);
                String line = String (Decibels::gainToDecibels (amp), 0) + " dBFS " + (lin ? "LIN    " : "analog ");
                for (float f : { 30.0f, 100.0f, 300.0f, 1000.0f, 3000.0f, 6000.0f, 10000.0f, 16000.0f })
                {
                    auto o = run (p, sine (f, 2.0f, amp));
                    const float l = Decibels::gainToDecibels (harmonic (o, 0, 72000, 19200, f) / amp), r = Decibels::gainToDecibels (harmonic (o, 1, 72000, 19200, f) / amp);
                    line << String (f / 1000.0f, 2) << "k " << String (l, 1) << "/" << String (r, 1) << "  ";
                }
                std::cout << line << "\n";
            }
        return 0;
    }

    if (argc > 2 && String (argv[2]) == "same")
    {
        for (int mode = 0; mode < 2; ++mode)
            for (int lin = 1; lin >= 0; --lin)
            {
                defaults (p, lin, 2);
                for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 1.0f);
                setP (p, Params::input, 15.0f); setP (p, Params::age, 1.0f); setP (p, Params::mode, (float) mode);
                prepare (p);
                AudioBuffer<float> sw (2, (int) SR * 4);
                Random r (9);
                for (int i = 0; i < sw.getNumSamples(); ++i) { const float v = 0.4f * (r.nextFloat() * 2 - 1); sw.setSample (0, i, v); sw.setSample (1, i, v); }
                auto o2 = run (p, sw);
                double diff = 0;
                for (int i = 0; i < o2.getNumSamples(); ++i) diff = jmax (diff, (double) std::abs (o2.getSample (0, i) - o2.getSample (1, i)));
                std::cout << (mode ? "MIX " : "MASTER ") << (lin ? "LIN" : "analog") << ": identical L/R in, IN +15, AGE full -> max L-R difference out " << diff << "\n";
            }
        return 0;
    }

    if (argc > 2 && String (argv[2]) == "mock")
    {
        // every state the header / split mockup can show, rendered by the real editor
        BreatheProcessor q;
        q.setPlayConfigDetails (2, 2, SR, BS);
        auto in = noise (0.5f, 0.3f, 11);
        const float A[4] { 0.35f, 0.5f, 0.45f, 0.8f }, B[4] { 0.2f, 0.72f, 0.3f, 0.55f };
        const float advA[12] { 40.5f, 62.0f, 18.5f, 0, -30.0f, 0, 25.5f, 0, 47.5f, 60.0f, 9.5f, 35.0f };
        const float advB[12] { -20.0f, 15.0f, 0, 10.0f, 30.0f, -12.5f, 0, -25.0f, 20.0f, 35.0f, 0, -10.0f };
        auto setup = [&] (bool lk, int mode)
        {
            defaults (q, true, 1);
            setP (q, Params::link, lk ? 1.0f : 0.0f); setP (q, Params::procMode, (float) mode);
            setP (q, Params::autoIn, 1); setP (q, Params::autoOut, 1);
            for (int b = 0; b < 4; ++b)
            {
                setP (q, Params::band[b], A[b]); setP (q, Params::band2[b], lk ? A[b] : B[b]);
                setP (q, Params::advEq[b], advA[b * 3]); setP (q, Params::advDrive[b], advA[b * 3 + 1]); setP (q, Params::advDyn[b], advA[b * 3 + 2]);
                setP (q, Params::advEq2[b], lk ? advA[b * 3] : advB[b * 3]); setP (q, Params::advDrive2[b], lk ? advA[b * 3 + 1] : advB[b * 3 + 1]);
                setP (q, Params::advDyn2[b], lk ? advA[b * 3 + 2] : advB[b * 3 + 2]);
            }
        };
        auto shoot = [&] (const String& name, std::function<void (BreatheEditor&)> prep, bool spin)
        {
            q.apvts.state.setProperty ("theme", "Asphalt", nullptr);
            std::unique_ptr<BreatheEditor> ed (dynamic_cast<BreatheEditor*> (q.createEditor()));
            ed->applyTheme ("Asphalt");
            ed->hideWelcomeForTest();
            prep (*ed);
            if (spin) for (int i = 0; i < 6; ++i) { auto blk = run (q, in); ed->renderForTest (0.5); }
            else ed->renderForTest (0.1);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File f = outDir.getChildFile (name + ".png");
            f.deleteFile();
            if (auto os = f.createOutputStream()) PNGImageFormat().writeImageToStream (img, *os);
        };
        q.prepareToPlay (SR, BS);
        // bodies
        for (int lk = 0; lk < 2; ++lk)
            for (int mode = 0; mode < 2; ++mode)
                for (int sp = 0; sp < 2; ++sp)
                    for (int adv = 0; adv < 2; ++adv)
                        for (int sd = 0; sd < ((lk || sp) ? 1 : 2); ++sd)
                        {
                            setup (lk == 1, mode);
                            const String name = String ("body_l") + String (lk) + "_m" + String (mode) + "_s" + String (sp) + "_a" + String (adv) + "_d" + String (sd);
                            shoot (name, [&] (BreatheEditor& e)
                            {
                                e.setAdvancedOpen (adv == 1);
                                e.setSplit (sp == 1);
                                e.setSideForTest (sd);
                                const float hl = (float) (BreatheEditor::headerH + BreatheEditor::baseH + BreatheEditor::stripH + (adv ? BreatheEditor::advH : 0) + (sp ? BreatheEditor::splitH : 0));
                                e.setSize (BreatheEditor::baseW, (int) hl);
                            }, true);
                        }
        // header variants: one render per item state
        struct HV { const char* name; std::function<void()> set; std::function<void (BreatheEditor&)> ed; };
        auto none = [] (BreatheEditor&) {};
        std::vector<HV> hv {
            { "h_base", [&] { setup (true, 0); }, none },
            { "h_power0", [&] { setup (true, 0); setP (q, Params::bypass, 1); }, none },
            { "h_autoOut0", [&] { setup (true, 0); setP (q, Params::autoOut, 0); }, none },
            { "h_autoIn0", [&] { setup (true, 0); setP (q, Params::autoIn, 0); }, none },
            { "h_mon1", [&] { setup (true, 0); setP (q, Params::monitor, 1); }, none },
            { "h_mon2", [&] { setup (true, 0); setP (q, Params::monitor, 2); }, none },
            { "h_mon3", [&] { setup (true, 0); setP (q, Params::monitor, 3); }, none },
            { "h_mon4", [&] { setup (true, 0); setP (q, Params::monitor, 4); }, none },
            { "h_mon5", [&] { setup (true, 0); setP (q, Params::monitor, 5); }, none },
            { "h_mode1", [&] { setup (true, 1); }, none },
            { "h_unlinkA", [&] { setup (false, 0); }, [] (BreatheEditor& e) { e.setSideForTest (0); } },
            { "h_unlinkB", [&] { setup (false, 0); }, [] (BreatheEditor& e) { e.setSideForTest (1); } },
            { "h_unlinkA_lr", [&] { setup (false, 1); }, [] (BreatheEditor& e) { e.setSideForTest (0); } },
            { "h_unlinkB_lr", [&] { setup (false, 1); }, [] (BreatheEditor& e) { e.setSideForTest (1); } },
            { "h_link_lr", [&] { setup (true, 1); }, none },
            { "h_splitU", [&] { setup (false, 0); }, [] (BreatheEditor& e) { e.setSplit (true); } },
            { "h_splitU_lr", [&] { setup (false, 1); }, [] (BreatheEditor& e) { e.setSplit (true); } },
        };
        for (int sl = 1; sl < 4; ++sl)
            hv.push_back ({ sl == 1 ? "h_slot1" : sl == 2 ? "h_slot2" : "h_slot3", [&, sl] { setup (true, 0); q.slot = sl; }, none });
        for (auto& v : hv)
        {
            v.set();
            shoot (v.name, [&] (BreatheEditor& e) { v.ed (e); e.setAdvancedOpen (false); }, false);
            q.slot = 0;
        }
        return 0;
    }

    if (argc > 2 && String (argv[2]) == "shots")
    {
        BreatheProcessor q;
        q.setPlayConfigDetails (2, 2, SR, BS);
        defaults (q, true, 1);
        setP (q, Params::band[0], 0.6f); setP (q, Params::band[1], 0.5f); setP (q, Params::band[2], 0.55f); setP (q, Params::band[3], 0.85f);
        setP (q, Params::advEq[0], 40.5f); setP (q, Params::advDrive[0], 62.0f); setP (q, Params::advDyn[0], 70.0f); setP (q, Params::advDyn[1], 50.0f);
        setP (q, Params::advDrive[1], -30.0f); setP (q, Params::advEq[2], 25.5f); setP (q, Params::advDyn[2], 47.5f);
        setP (q, Params::advEq[3], 60.0f); setP (q, Params::advDrive[3], 9.5f); setP (q, Params::advDyn[3], 35.0f);
        q.prepareToPlay (SR, BS);
        // something with dynamics: a kick every half second, a bass line, hats and a quiet room
        AudioBuffer<float> in (2, (int) (SR * 0.5));
        Random rr (3);
        float hp = 0.0f;
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const double t = i / SR;
            const float kick = (float) (std::exp (-t * 9.0) * std::sin (2 * MathConstants<double>::pi * (50.0 + 80.0 * std::exp (-t * 30.0)) * t)) * 0.7f;
            const float bass = 0.18f * (float) std::sin (2 * MathConstants<double>::pi * 55.0 * t);
            const float w = rr.nextFloat() * 2 - 1;
            const float hat = (w - hp) * (float) std::exp (-std::fmod (t, 0.125) * 40.0) * 0.12f; hp = w;
            const float room = (rr.nextFloat() * 2 - 1) * 0.004f;
            for (int ch = 0; ch < 2; ++ch) in.setSample (ch, i, kick + bass + hat + room + 0.05f * (float) std::sin (2 * MathConstants<double>::pi * 440.0 * t));
        }
        struct Shot { const char* file; int view; int band; bool about; };
        for (auto sh : { Shot { "adv_eq.png", 0, 0, false }, Shot { "adv_comp.png", 1, 0, false }, Shot { "adv_lift.png", 1, 3, false }, Shot { "about.png", 0, 0, true } })
        {
            q.apvts.state.setProperty ("theme", "Asphalt", nullptr);
            std::unique_ptr<BreatheEditor> ed (dynamic_cast<BreatheEditor*> (q.createEditor()));
            ed->applyTheme ("Asphalt");
            ed->setAdvancedOpen (true);
            ed->setSize (BreatheEditor::baseW, BreatheEditor::baseH + BreatheEditor::stripH + BreatheEditor::advH);
            ed->hideWelcomeForTest();
            if (sh.view == 1) ed->showDynForTest (sh.band);
            for (int i = 0; i < 12; ++i) { auto blk = run (q, in); ed->renderForTest (0.5); }
            if (sh.about) ed->showAboutForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            File f = outDir.getChildFile (sh.file);
            f.deleteFile();
            if (auto os = f.createOutputStream()) PNGImageFormat().writeImageToStream (img, *os);
        }
        {
            // a host that refuses to grow the window: the panel must shrink to fit, nothing cut off
            std::unique_ptr<BreatheEditor> ed (dynamic_cast<BreatheEditor*> (q.createEditor()));
            ed->setAdvancedOpen (true);
            ed->setSize (BreatheEditor::baseW, BreatheEditor::baseH + BreatheEditor::stripH);
            ed->hideWelcomeForTest();
            for (int i = 0; i < 4; ++i) { auto blk = run (q, in); ed->renderForTest (0.5); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File f = outDir.getChildFile ("squeezed.png");
            f.deleteFile();
            if (auto os = f.createOutputStream()) PNGImageFormat().writeImageToStream (img, *os);
        }
        {
            std::unique_ptr<BreatheEditor> ed (dynamic_cast<BreatheEditor*> (q.createEditor()));
            ed->setAdvancedOpen (false);
            ed->setSize (BreatheEditor::baseW, BreatheEditor::baseH + BreatheEditor::stripH);
            ed->hideWelcomeForTest();
            for (int i = 0; i < 6; ++i) { auto blk = run (q, in); ed->renderForTest (0.5); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            File f = outDir.getChildFile ("closed.png");
            f.deleteFile();
            if (auto os = f.createOutputStream()) PNGImageFormat().writeImageToStream (img, *os);
        }
        return 0;
    }

    if (argc > 2 && String (argv[2]) == "inlevel")
    {
        // pink-ish noise at about -18 dBFS RMS, all knobs high; response at a few frequencies vs IN
        auto in = noise (6.0f, 0.12f, 77);
        if (argc > 3)   // dynamic material: loud and quiet passages
            for (int i = 0; i < in.getNumSamples(); ++i)
            {
                const double t = i / SR, ph = std::fmod (t, 1.0);
                const float env = ph < 0.4 ? 1.0f : ph < 0.5 ? (float) std::exp (-(ph - 0.4) * 40.0) : 0.03f;
                for (int ch = 0; ch < 2; ++ch) in.setSample (ch, i, in.getSample (ch, i) * env * 2.0f);
            }
        for (int mode = 0; mode < 2; ++mode)
            for (float inDb : { 0.0f, -15.0f, -30.0f, 12.0f })
            {
                defaults (p, false, 1);
                for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 1.0f);
                setP (p, Params::mode, (float) mode); setP (p, Params::input, inDb);
                prepare (p);
                auto o = run (p, in);
                const int lat = p.getLatencySamples();
                String line = String (mode ? "MIX    " : "MASTER ") + "IN " + String (inDb, 0) + ": ";
                for (float f : { 40.0f, 200.0f, 1000.0f, 4400.0f, 11000.0f })
                {
                    // band-limited energy ratio around f (third octave) out / in
                    auto bandE = [&] (const AudioBuffer<float>& b, int off)
                    {
                        dsp::IIR::Filter<float> hp, lp;
                        hp.coefficients = dsp::IIR::Coefficients<float>::makeHighPass (SR, f / 1.12f, 2.0);
                        lp.coefficients = dsp::IIR::Coefficients<float>::makeLowPass (SR, f * 1.12f, 2.0);
                        double e = 0;
                        for (int i = 0; i < b.getNumSamples() - off; ++i)
                        {
                            const float v = lp.processSample (hp.processSample (b.getSample (0, i + off)));
                            if (i > (int) SR * 2) e += (double) v * v;
                        }
                        return e;
                    };
                    const double eo = bandE (o, lat), ei = bandE (in, 0);
                    line << String (f, 0) << " Hz " << String (10.0 * std::log10 (eo / ei), 2) << "  ";
                }
                line << "| GR " << String (p.optoGr[0].load(), 2) << "/" << String (p.optoGr[1].load(), 2) << "  lift " << String (p.liftDb[2].load(), 2) << "/" << String (p.liftDb[3].load(), 2);
                std::cout << line << "\n";
            }
        return 0;
    }

    if (argc > 2 && String (argv[2]) == "series")
    {
        for (int mode = 0; mode < 2; ++mode)
            for (float inDb : { -12.0f, 0.0f })
            {
                defaults (p, true, 1);
                for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 1.0f);
                setP (p, Params::mode, (float) mode); setP (p, Params::input, inDb);
                prepare (p);
                const float f = 438.7f;
                auto o = run (p, sine (f, 3.0f, 1.0f));
                const float h1 = harmonic (o, 0, 96000, 38400, f);
                String line = String (mode ? "MIX    " : "MASTER ") + "IN " + String (inDb, 0) + " dB: ";
                for (int k = 2; k <= 9; ++k) line << "H" << k << " " << String (Decibels::gainToDecibels (harmonic (o, 0, 96000, 38400, f * k) / h1), 0) << "  ";
                line << " | floor@60Hz " << String (Decibels::gainToDecibels (harmonic (o, 0, 96000, 38400, 61.3f) / h1), 0) << " dB";
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
            check (thd < (mode ? 25.0f : 10.0f), String (Params::bandName[b]) + ": harmonics within the planned range");
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

    // ---------------------------------------------------------------- hot signals: no holes, LIN = analog, L = R
    {
        std::cout << "== hot signal (+20 dBFS sine), every knob high\n";
        float worstHole = 0, best = -100, worstDiff = 0, worstLR = 0;
        for (float f : { 100.0f, 145.0f, 1000.0f, 3000.0f, 6000.0f, 10000.0f })
        {
            float g[2] {};
            for (int lin = 0; lin < 2; ++lin)
            {
                defaults (p, lin, 2);
                setP (p, Params::xover[0], 145.0f);
                setP (p, Params::band[0], 0.65f); setP (p, Params::band[1], 0.9f); setP (p, Params::band[2], 1); setP (p, Params::band[3], 1);
                setP (p, Params::age, 1.0f);
                prepare (p);
                auto o = run (p, sine (f, 2.0f, 10.0f));
                const float l = Decibels::gainToDecibels (harmonic (o, 0, 72000, 19200, f) / 10.0f), r = Decibels::gainToDecibels (harmonic (o, 1, 72000, 19200, f) / 10.0f);
                g[lin] = l;
                worstLR = jmax (worstLR, std::abs (l - r));
                worstHole = jmin (worstHole, l);
                best = jmax (best, l);
            }
            worstDiff = jmax (worstDiff, std::abs (g[0] - g[1]));
            std::cout << "   " << f << " Hz: analog " << String (g[0], 1) << " dB, LIN " << String (g[1], 1) << " dB\n";
        }
        check (best - worstHole < 6.0f, "no holes at the crossovers even when slammed (spread " + String (best - worstHole, 1) + " dB)");
        check (worstDiff < 2.0f, "LIN and analog phase give the same curve when slammed (+20 dBFS, deep in the red) (diff " + String (worstDiff, 2) + " dB)");
        check (worstLR < 1.5f, "left and right stay together when slammed (diff " + String (worstLR, 2) + " dB)");
    }
    {
        // channels must not wander apart in level, even with AGE and MIX at full
        defaults (p, true, 2);
        for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 1.0f);
        setP (p, Params::mode, 1); setP (p, Params::age, 1.0f);
        prepare (p);
        auto o = run (p, noise (8.0f, 0.3f, 21));
        float worst = 0;
        for (int i = (int) SR * 2; i + 4800 < o.getNumSamples(); i += 4800)
            worst = jmax (worst, std::abs (Decibels::gainToDecibels (rms (o, 0, i, 4800) / rms (o, 1, i, 4800))));
        auto in = noise (8.0f, 0.3f, 21);
        float base = 0;
        for (int i = (int) SR * 2; i + 4800 < in.getNumSamples(); i += 4800)
            base = jmax (base, std::abs (Decibels::gainToDecibels (rms (in, 0, i, 4800) / rms (in, 1, i, 4800))));
        std::cout << "== L/R level wander with AGE + MIX at full: " << worst << " dB (input itself " << base << " dB)\n";
        check (worst - base < 0.6f, "channels keep their level, only the harmonics shimmer");
    }

    // ---------------------------------------------------------------- different music left and right, hot: same transfer in both channels
    for (int lin = 1; lin >= 0; --lin)
    {
        defaults (p, lin, 2);
        for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 0.8f);
        setP (p, Params::input, 15.0f); setP (p, Params::age, 1.0f);
        prepare (p);
        Random rl (5), rr (6);
        AudioBuffer<float> in (2, (int) SR * 4);
        float bl = 0, br = 0;
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const double t = i / SR;
            const float probe = 0.05f * (float) std::sin (2 * MathConstants<double>::pi * 1000.0 * t);
            bl += 0.02f * (rl.nextFloat() * 2 - 1 - bl * 0.02f); br += 0.02f * (rr.nextFloat() * 2 - 1 - br * 0.02f);   // different rumbly "music"
            const float kickL = (float) std::exp (-std::fmod (t, 0.5) * 8.0) * 0.4f * (float) std::sin (2 * MathConstants<double>::pi * 55 * t);
            in.setSample (0, i, probe + 0.3f * bl + kickL);
            in.setSample (1, i, probe + 0.3f * br + 0.3f * (rr.nextFloat() * 2 - 1));
        }
        auto out = run (p, in);
        const float gl = harmonic (out, 0, 96000, 96000, 1000.0f), gr = harmonic (out, 1, 96000, 96000, 1000.0f);
        const float d = std::abs (Decibels::gainToDecibels (gl / gr));
        std::cout << "== different music in L and R, IN +15, AGE full (" << (lin ? "LIN" : "analog") << "): probe tone L/R difference " << d << " dB\n";
        check (d < 0.5f, "linked: both channels treat the same tone the same way");
    }

    // ---------------------------------------------------------------- AGE = scrape flutter: fast ripple up high, nothing down low
    {
        auto ripple = [&] (float f, float age)
        {
            defaults (p, true, 1);
            for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 0.3f);
            setP (p, Params::age, age);
            prepare (p);
            auto o = run (p, sine (f, 3.0f, 0.1f));
            // the shiver shows up as a noisy skirt around the tone (sidebands 400 Hz - 4 kHz away); L and R levels must agree
            const int st = 96000, len = 48000;
            const float h1 = harmonic (o, 0, st, len, f);
            double sk = 0; int nk = 0;
            for (float off : { 520.0f, 830.0f, 1270.0f, 1910.0f, 2730.0f })
            {
                if (f - off > 20.0f) { const float v = harmonic (o, 0, st, len, f - off); sk += v * v; ++nk; }
                const float v = harmonic (o, 0, st, len, f + off); sk += v * v; ++nk;
            }
            const float skirt = Decibels::gainToDecibels ((float) std::sqrt (sk / nk) / h1, -200.0f);
            const float lr = std::abs (Decibels::gainToDecibels (h1 / harmonic (o, 1, st, len, f)));
            const float floor60 = Decibels::gainToDecibels (harmonic (o, 0, st, len, 61.3f) / h1, -200.0f);
            return std::array<float, 3> { skirt, lr, floor60 };
        };
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const auto a0 = ripple (f, 0.0f), a1 = ripple (f, 1.0f);
            std::cout << "== AGE scrape at " << f << " Hz: skirt AGE 0 " << String (a0[0], 1) << " dB, AGE 100 " << String (a1[0], 1)
                      << " dB; L/R level " << String (a1[1], 3) << " dB; at 60 Hz " << String (a0[2], 0) << " -> " << String (a1[2], 0) << " dB\n";
            if (f < 200) check (a1[0] < -100.0f, "AGE leaves the low end alone");
            check (a1[1] < 0.1f, "AGE: L and R agree on average");
            check (a1[2] < a0[2] + 6.0f || a1[2] < -100.0f, "AGE adds nothing at 60 Hz");
            if (f > 5000) check (a1[0] > a0[0] + 20.0f, "AGE makes the top shiver");
        }
        // with a band soloed (or muted) AGE is out of the way
        for (int sm = 0; sm < 2; ++sm)
        {
            defaults (p, true, 1);
            for (int b = 0; b < 4; ++b) setP (p, Params::band[b], 0.3f);
            setP (p, sm == 0 ? Params::solo[3] : Params::mute[0], 1.0f);
            float sk[2];
            for (int k = 0; k < 2; ++k)
            {
                setP (p, Params::age, k == 0 ? 0.0f : 1.0f);
                prepare (p);
                auto o = run (p, sine (10000.0f, 3.0f, 0.1f));
                const float h1 = harmonic (o, 0, 96000, 48000, 10000.0f), v = harmonic (o, 0, 96000, 48000, 10000.0f - 830.0f);
                sk[k] = Decibels::gainToDecibels (v / h1, -200.0f);
            }
            std::cout << "== AGE with " << (sm == 0 ? "Solo" : "Mute") << " on: skirt at AGE 0 / 100 = " << String (sk[0], 1) << " / " << String (sk[1], 1) << " dB\n";
            check (sk[1] < sk[0] + 3.0f, String ("AGE is off while a band is ") + (sm == 0 ? "soloed" : "muted"));
        }
    }

    // ---------------------------------------------------------------- Advanced trims
    {
        defaults (p, true, 2);
        for (int b = 0; b < 4; ++b) { setP (p, Params::advEq[b], 100); setP (p, Params::advDrive[b], 100); setP (p, Params::advDyn[b], 100); }
        prepare (p);
        const int lat = p.getLatencySamples();
        auto in = noise (1.0f, 0.5f, 5);
        auto out = run (p, in);
        double err = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = lat; i < in.getNumSamples(); ++i)
                err = jmax (err, (double) std::abs (out.getSample (ch, i) - in.getSample (ch, i - lat)));
        std::cout << "== Advanced at +100 % with every big knob at zero: max error " << err << "\n";
        check (err == 0.0, "Advanced does nothing while the big knobs sit at zero");

        auto curveAt = [&] (float trim, int mode, float f)
        {
            defaults (p, false, 1);
            setP (p, Params::mode, (float) mode);
            for (int b = 0; b < 4; ++b) { setP (p, Params::band[b], 1.0f); setP (p, Params::advEq[b], trim); setP (p, Params::advDrive[b], -100); setP (p, Params::advDyn[b], -100); }
            prepare (p);
            auto o = run (p, sine (f, 1.5f, 0.01f));
            return Decibels::gainToDecibels (harmonic (o, 0, 48000, 19200, f) / 0.01f);
        };
        for (int mode = 0; mode < 2; ++mode)
        {
            const float m100 = curveAt (-100, mode, 30), z = curveAt (0, mode, 30), p100 = curveAt (100, mode, 30);
            const float a100 = curveAt (100, mode, 11000);
            std::cout << "== EQ trim " << (mode ? "MIX" : "MASTER") << ": 30 Hz at -100/0/+100 % = " << String (m100, 2) << " / " << String (z, 2) << " / " << String (p100, 2)
                      << " dB, 11 kHz at +100 % = " << String (a100, 2) << " dB\n";
            check (std::abs (m100) < 0.3f && p100 > 10.0f && p100 > z + 3.0f, "EQ trim runs from no curve to a real Pultec");
        }

        auto thdAt = [&] (float trim, float& lvl)
        {
            defaults (p, true, 2);
            setP (p, Params::band[1], 1.0f); setP (p, Params::advDrive[1], trim);
            prepare (p);
            auto o = run (p, sine (350.0f, 2.0f, 0.178f));
            const float h1 = harmonic (o, 0, 72000, 19200, 350), h2 = harmonic (o, 0, 72000, 19200, 700), h3 = harmonic (o, 0, 72000, 19200, 1050);
            lvl = Decibels::gainToDecibels (h1 / 0.178f);
            return std::sqrt (h2 * h2 + h3 * h3) / h1 * 100.0f;
        };
        float l0, l1, l2;
        const float t0 = thdAt (-100, l0), t1 = thdAt (0, l1), t2 = thdAt (100, l2);
        std::cout << "== DRIVE trim LOW-MID: THD " << String (t0, 2) << " / " << String (t1, 2) << " / " << String (t2, 2) << " %, level " << String (l0, 2) << " / " << String (l1, 2) << " / " << String (l2, 2) << " dB\n";
        check (t0 < 0.05f && t2 > t1 * 2.5f && std::abs (l2 - l1) < 1.5f, "DRIVE trim runs from clean to much hotter at the same level");

        // COMP trim = threshold: + squeezes more, - less; attack/release stay optical
        auto grWith = [&] (float trim)
        {
            defaults (p, false, 1);
            setP (p, Params::band[0], 1.0f); setP (p, Params::advDyn[0], trim);
            prepare (p);
            AudioBuffer<float> in (2, (int) (SR * 4));
            for (int i = 0; i < in.getNumSamples(); ++i)
            {
                const double t = i / SR, ph = std::fmod (t, 0.5);
                const float v = (float) ((0.08 + 0.5 * std::exp (-ph * 8.0)) * std::sin (2 * MathConstants<double>::pi * 55.0 * t));
                in.setSample (0, i, v); in.setSample (1, i, v);
            }
            float worst = 0.0f;
            MidiBuffer midi;
            for (int pos = 0; pos < in.getNumSamples(); pos += BS)
            {
                AudioBuffer<float> blk (in.getArrayOfWritePointers(), 2, pos, jmin (BS, in.getNumSamples() - pos));
                p.processBlock (blk, midi);
                if (pos > SR * 2) worst = jmax (worst, p.optoGr[0].load());
            }
            return worst;
        };
        const float gm = grWith (-100), g0 = grWith (0), gp = grWith (100);
        std::cout << "== COMP trim on LOW (MASTER): peak gain reduction " << String (gm, 2) << " / " << String (g0, 2) << " / " << String (gp, 2) << " dB\n";
        check (gm < g0 && gp > g0 * 1.3f, "COMP trim moves the threshold: + squeezes more, - less");

        defaults (p, true, 2);
        for (int b = 0; b < 4; ++b) { setP (p, Params::band[b], 1.0f); setP (p, Params::advEq[b], 100); setP (p, Params::advDrive[b], 100); setP (p, Params::advDyn[b], 100); }
        setP (p, Params::mode, 1);
        prepare (p);
        auto mono = noise (3.0f, 0.3f, 9);
        for (int i = 0; i < mono.getNumSamples(); ++i) mono.setSample (1, i, mono.getSample (0, i));
        auto o = run (p, mono);
        double d = 0;
        bool finite = true;
        for (int i = 0; i < o.getNumSamples(); ++i) { d = jmax (d, (double) std::abs (o.getSample (0, i) - o.getSample (1, i))); finite &= std::isfinite (o.getSample (0, i)); }
        std::cout << "== everything at +100 %, mono in: L-R difference " << d << "\n";
        check (finite && d < 1.0e-6, "mono stays mono with Advanced slammed");
    }

    // ---------------------------------------------------------------- AUTO IN / AUTO OUT
    {
        // bit-exact with both on and every knob at zero
        defaults (p, true, 2);
        setP (p, Params::autoIn, 1); setP (p, Params::autoOut, 1);
        prepare (p);
        const int lat = p.getLatencySamples();
        auto in = noise (2.0f, 0.3f, 8);
        auto out = run (p, in);
        double err = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = lat; i < in.getNumSamples(); ++i)
                err = jmax (err, (double) std::abs (out.getSample (ch, i) - in.getSample (ch, i - lat)));
        std::cout << "== AUTO IN + AUTO OUT on, knobs at zero: max error " << err << "\n";
        check (err == 0.0, "AUTO modes leave the sound untouched while every knob is at zero");

        // AUTO IN: a quiet and a loud track get the same colour
        auto thd = [&] (float amp, bool autoIn)
        {
            defaults (p, true, 2);
            setP (p, Params::band[1], 1.0f); setP (p, Params::autoIn, autoIn ? 1.0f : 0.0f);
            prepare (p);
            auto o = run (p, sine (350.0f, 6.0f, amp));
            const int st = (int) (SR * 5), len = 19200;
            const float h1 = harmonic (o, 0, st, len, 350), h2 = harmonic (o, 0, st, len, 700), h3 = harmonic (o, 0, st, len, 1050);
            return std::sqrt (h2 * h2 + h3 * h3) / h1 * 100.0f;
        };
        const float qOff = thd (0.01f, false), lOff = thd (0.5f, false), qOn = thd (0.01f, true), lOn = thd (0.5f, true);
        std::cout << "== AUTO IN: THD quiet / loud track  off " << String (qOff, 2) << " / " << String (lOff, 2) << " %,  on " << String (qOn, 2) << " / " << String (lOn, 2) << " %\n";
        check (std::abs (qOn - lOn) < 0.35f * jmax (qOn, lOn) && qOn > qOff * 3.0f, "AUTO IN gives quiet and loud tracks the same colour");

        // AUTO OUT: a big Pultec boost in MIX comes back to the input loudness
        auto loud = [&] (bool autoOut)
        {
            defaults (p, false, 1);
            setP (p, Params::mode, 1);
            for (int b = 0; b < 4; ++b) { setP (p, Params::band[b], 1.0f); setP (p, Params::advEq[b], 100.0f); }
            setP (p, Params::autoOut, autoOut ? 1.0f : 0.0f);
            prepare (p);
            auto src = noise (8.0f, 0.2f, 12);
            auto o = run (p, src);
            const int lat = p.getLatencySamples();
            // K-weighted-ish level difference over the last 3 s
            Dsp::Biquad a[2], c[2];
            a[0].k = c[0].k = Dsp::BiquadCoefs::lowShelf (SR, 100.0, 0.7, -12.0);
            a[1].k = c[1].k = Dsp::BiquadCoefs::highShelf (SR, 1500.0, 0.7, 4.0);
            double ei = 0, eo = 0;
            for (int i = 0; i + lat < src.getNumSamples(); ++i)
            {
                const float x = a[1].process (a[0].process (src.getSample (0, i))), y = c[1].process (c[0].process (o.getSample (0, i + lat)));
                if (i > (int) SR * 5) { ei += (double) x * x; eo += (double) y * y; }
            }
            return (float) (10.0 * std::log10 (eo / ei));
        };
        const float off = loud (false), on = loud (true);
        std::cout << "== AUTO OUT: loudness change with a full Pultec boost  off " << String (off, 2) << " dB,  on " << String (on, 2) << " dB\n";
        check (std::abs (on) < 1.0f && std::abs (off) > 3.0f, "AUTO OUT brings the processed loudness back to the input");
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
