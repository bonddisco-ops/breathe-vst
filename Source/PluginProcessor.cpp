#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Designs the linear-phase band filters (with the Pultec curve folded in) on its own
// thread whenever the crossovers, the band knobs or the mode change.
class BreatheProcessor::Designer : public Thread
{
public:
    explicit Designer (BreatheProcessor& p) : Thread ("Breathe filters"), proc (p) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            if (auto s = designIfChanged (false))
                proc.splitter.offer (std::move (s));
            wait (20);
        }
    }

    std::unique_ptr<LinPhaseSplitter::Spectra> designIfChanged (bool force)
    {
        const double fs = proc.sampleRate;
        float x[3], e[4];
        for (int i = 0; i < 3; ++i) x[i] = proc.xo[i]->load();
        for (int b = 0; b < 4; ++b) e[b] = Dsp::taper (proc.knob[b]->load());
        const float m = proc.modeP->load();

        bool changed = force || fs != lastRate || m != lastMode;
        for (int i = 0; i < 3; ++i) changed |= std::abs (x[i] - lastX[i]) > 0.01f;
        for (int b = 0; b < 4; ++b) changed |= std::abs (e[b] - lastE[b]) > 0.0005f;
        if (! changed)
            return {};

        lastRate = fs;
        lastMode = m;
        std::copy (x, x + 3, lastX);
        std::copy (e, e + 4, lastE);

        Dsp::KernelSet ks;
        Dsp::designKernels (ks, fs, x, Dsp::pultec (fs, e, x, Dsp::blend (m).eq));
        return LinPhaseSplitter::makeSpectra (ks, fs);
    }

private:
    BreatheProcessor& proc;
    double lastRate = 0.0;
    float lastMode = -1.0f, lastX[3] {}, lastE[4] { -1, -1, -1, -1 };
};

//==============================================================================
BreatheProcessor::BreatheProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BREATHE", Params::layout())
{
    for (int b = 0; b < 4; ++b)
    {
        knob[b] = apvts.getRawParameterValue (Params::band[b]);
        soloP[b] = apvts.getRawParameterValue (Params::solo[b]);
        muteP[b] = apvts.getRawParameterValue (Params::mute[b]);
    }
    for (int i = 0; i < 3; ++i) xo[i] = apvts.getRawParameterValue (Params::xover[i]);
    mixP = apvts.getRawParameterValue (Params::mix);
    ageP = apvts.getRawParameterValue (Params::age);
    outP = apvts.getRawParameterValue (Params::output);
    inP = apvts.getRawParameterValue (Params::input);
    ovsP = apvts.getRawParameterValue (Params::ovs);
    linP = apvts.getRawParameterValue (Params::lin);
    bypassP = apvts.getRawParameterValue (Params::bypass);
    modeP = apvts.getRawParameterValue (Params::mode);

    // every unit is a little different: fixed part tolerances per channel (not on the low band: mono bass stays mono)
    Random r (Time::currentTimeMillis());
    for (int b = 1; b < 4; ++b)
        for (int ch = 0; ch < 2; ++ch)
        {
            bands[b].tolDrive[ch] = (r.nextFloat() * 2.0f - 1.0f) * 0.04f;
            bands[b].tolBias[ch] = (r.nextFloat() * 2.0f - 1.0f) * 0.12f;
        }

    designer = std::make_unique<Designer> (*this);
}

BreatheProcessor::~BreatheProcessor()
{
    designer->stopThread (2000);
}

bool BreatheProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::stereo() || out == AudioChannelSet::mono());
}

dsp::Oversampling<float>* BreatheProcessor::currentOs (int band) const
{
    return ovsIndex > 0 ? os[linear ? 1 : 0][ovsIndex][band].get() : nullptr;
}

void BreatheProcessor::prepareToPlay (double sr, int block)
{
    designer->stopThread (2000);
    sampleRate = sr;
    maxBlock = jlimit (16, 2048, block);

    const dsp::ProcessSpec spec { sr, (uint32) maxBlock, 2 };

    for (auto* f : { &split1, &split2, &split3, &comp1, &comp3, &dryAp1, &dryAp2, &dryAp3 })
        f->prepare (spec);
    for (auto* f : { &split1, &split2, &split3 }) f->setType (dsp::LinkwitzRileyFilterType::lowpass);
    for (auto* f : { &comp1, &comp3, &dryAp1, &dryAp2, &dryAp3 }) f->setType (dsp::LinkwitzRileyFilterType::allpass);

    for (int t = 0; t < 2; ++t)
        for (int f = 1; f <= 4; ++f)
            for (int b = 0; b < 4; ++b)
            {
                os[t][f][b] = std::make_unique<dsp::Oversampling<float>> (2, (size_t) f,
                    t == 0 ? dsp::Oversampling<float>::filterHalfBandPolyphaseIIR : dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                    true, true);
                os[t][f][b]->initProcessing ((size_t) maxBlock);
            }

    for (auto& bb : bandBuf) bb.setSize (2, maxBlock);
    dryBuf.setSize (2, maxBlock);
    wetBuf.setSize (2, maxBlock);
    apBuf.setSize (2, maxBlock);
    eqBuf.setSize (2, maxBlock);
    for (int ch = 0; ch < 2; ++ch)
        for (auto* v : { &gTubeA[ch], &bTubeA[ch], &gTapeA[ch], &tXfA[ch], &jitA[ch] })
            v->assign ((size_t) maxBlock, 0.0f);
    inGainA.assign ((size_t) maxBlock, 1.0f);

    // linear-phase filters for the current settings, ready before the first block
    splitter.prepare (designer->designIfChanged (true));

    for (auto* d : { &rawDelay, &apDelay }) { d->prepare (spec); d->setMaximumDelayInSamples (splitter.latency() + 4096); }
    for (auto& d : bandDelay) { d.prepare (spec); d.setMaximumDelayInSamples (4096); }

    int bi = 0;
    for (auto& b : bands)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            b.shimmer[ch].prepare (sr, 1000 + bi * 10 + ch);
            b.pPre[ch] = b.pPost[ch] = 1.0e-9;
            b.satGain[ch] = 1.0f;
            b.dcX[ch] = b.dcY[ch] = 0.0f;
        }
        b.pFast = b.pLift = 0.0;
        b.pRef = 1.0e-6;
        b.optoF = b.optoS = b.makeup = b.lift = b.sag = 0.0f;
        b.amount.reset (sr, 0.05);
        b.amount.setCurrentAndTargetValue (Dsp::taper (knob[bi]->load()));
        b.act = b.amount.getTargetValue() > 0.0f ? 1.0f : 0.0f;
        b.gain = 1.0f;
        ++bi;
    }
    for (auto& st : eq) for (auto& q : st) q.reset();

    mixS.reset (sr, 0.03);    mixS.setCurrentAndTargetValue (mixP->load());
    outS.reset (sr, 0.03);    outS.setCurrentAndTargetValue (Decibels::decibelsToGain (outP->load()));
    inS.reset (sr, 0.03);     inS.setCurrentAndTargetValue (Decibels::decibelsToGain (inP->load()));
    modeS.reset (sr, 0.05);   modeS.setCurrentAndTargetValue (modeP->load());
    bypassS.reset (sr, 0.02); bypassS.setCurrentAndTargetValue (bypassP->load() > 0.5f ? 1.0f : 0.0f);
    wetActiveS.reset (sr, 0.02);
    bool anyOn = false;
    for (int b = 0; b < 4; ++b) anyOn |= Dsp::isOn (knob[b]->load()) || soloP[b]->load() > 0.5f || muteP[b]->load() > 0.5f;
    wetActiveS.setCurrentAndTargetValue (anyOn ? 1.0f : 0.0f);
    inPow = 0.0;

    ovsIndex = -1;
    configure ((int) ovsP->load(), linP->load() > 0.5f);
    designer->startThread (Thread::Priority::low);
}

void BreatheProcessor::configure (int ovs, bool lin)
{
    ovsIndex = jlimit (0, 4, ovs);
    linear = lin;
    osLatency = 0;
    if (auto* o = currentOs (0))
        osLatency = roundToInt (o->getLatencyInSamples());
    for (int t = 0; t < 2; ++t) for (int f = 1; f <= 4; ++f) for (int b = 0; b < 4; ++b) os[t][f][b]->reset();
    splitter.reset();
    for (auto* f : { &split1, &split2, &split3, &comp1, &comp3, &dryAp1, &dryAp2, &dryAp3 }) f->reset();
    rawDelay.reset(); apDelay.reset();
    for (auto& d : bandDelay) d.reset();
    for (auto& b : bands) for (auto& s : b.sat) s.prepare (sampleRate * (double) (1 << ovsIndex));
    setLatencySamples (totalLatency());
}

void BreatheProcessor::refreshKernelsNow()
{
    if (auto s = designer->designIfChanged (false))
        splitter.offer (std::move (s));
}

//==============================================================================
void BreatheProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int numCh = jmin (2, buffer.getNumChannels());
    const int total = buffer.getNumSamples();
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, total);
    if (numCh == 0 || total == 0) return;

    const int wantOvs = (int) ovsP->load();
    const bool wantLin = linP->load() > 0.5f;
    if (wantOvs != ovsIndex || wantLin != linear)
        configure (wantOvs, wantLin);

    float pin[2] {}, pout[2] {};
    for (int ch = 0; ch < numCh; ++ch) pin[ch] = buffer.getMagnitude (ch, 0, total);
    if (numCh == 1) pin[1] = pin[0];

    for (int pos = 0; pos < total; pos += maxBlock)
    {
        const int n = jmin (maxBlock, total - pos);
        float* io[2] { buffer.getWritePointer (0, pos), buffer.getWritePointer (numCh > 1 ? 1 : 0, pos) };
        processChunk (io, numCh, n);
    }

    for (int ch = 0; ch < numCh; ++ch) pout[ch] = buffer.getMagnitude (ch, 0, total);
    if (numCh == 1) pout[1] = pout[0];
    for (int ch = 0; ch < 2; ++ch)
    {
        inPeak[ch] = jmax (inPeak[ch].load(), pin[ch]);
        outPeak[ch] = jmax (outPeak[ch].load(), pout[ch]);
    }

    // feed the spectrum on the screen
    {
        const auto* l = buffer.getReadPointer (0);
        const auto* r = buffer.getReadPointer (numCh > 1 ? 1 : 0);
        int s1, n1, s2, n2;
        scopeFifo.prepareToWrite (total, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) scopeData[(size_t) (s1 + i)] = 0.5f * (l[i] + r[i]);
        for (int i = 0; i < n2; ++i) scopeData[(size_t) (s2 + i)] = 0.5f * (l[n1 + i] + r[n1 + i]);
        scopeFifo.finishedWrite (n1 + n2);
    }
}

void BreatheProcessor::processChunk (float* const* io, int numCh, int n)
{
    const float age = ageP->load();
    const bool byp = bypassP->load() > 0.5f;

    // ---- targets
    bool anySolo = false, anyOn = false;
    for (int b = 0; b < 4; ++b) anySolo |= soloP[b]->load() > 0.5f;
    for (int b = 0; b < 4; ++b)
    {
        bands[b].amount.setTargetValue (Dsp::taper (knob[b]->load()));
        anyOn |= Dsp::isOn (knob[b]->load()) || soloP[b]->load() > 0.5f || muteP[b]->load() > 0.5f;
    }
    wetActiveS.setTargetValue (anyOn ? 1.0f : 0.0f);
    mixS.setTargetValue (mixP->load());
    outS.setTargetValue (Decibels::decibelsToGain (outP->load()));
    inS.setTargetValue (Decibels::decibelsToGain (inP->load()));
    modeS.setTargetValue (modeP->load());
    bypassS.setTargetValue (byp ? 1.0f : 0.0f);
    const auto ms = Dsp::blend (modeS.skip (n));

    float xoNow[3];
    for (int i = 0; i < 3; ++i)
    {
        lrFreq[i] += (xo[i]->load() - lrFreq[i]) * 0.35f;
        xoNow[i] = lrFreq[i];
    }

    // recipes follow the band centres
    const float edges[5] { 20.0f, xoNow[0], xoNow[1], xoNow[2], 20000.0f };
    for (int b = 0; b < 4; ++b) bands[b].recipe = Dsp::recipeAt (std::sqrt (edges[b] * edges[b + 1]));

    // ---- copy input; input gain (what the iron gets) and its level for the IN knob colour
    for (int ch = 0; ch < 2; ++ch)
        dryBuf.copyFrom (ch, 0, io[jmin (ch, numCh - 1)], n);
    {
        const double c = 1.0 - std::exp (-1.0 / (0.3 * sampleRate));
        const float* l = dryBuf.getReadPointer (0);
        const float* r = dryBuf.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float g = inS.getNextValue();
            inGainA[(size_t) i] = g;
            const double p = 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]) * g * g;
            inPow += c * (p - inPow);
        }
        inLevelDb = (float) (10.0 * std::log10 (inPow + 1.0e-12));
    }

    // ---- split into bands
    if (linear)
    {
        const float* in[2] { dryBuf.getReadPointer (0), dryBuf.getReadPointer (1) };
        float* const out[4][2] {
            { bandBuf[0].getWritePointer (0), bandBuf[0].getWritePointer (1) },
            { bandBuf[1].getWritePointer (0), bandBuf[1].getWritePointer (1) },
            { bandBuf[2].getWritePointer (0), bandBuf[2].getWritePointer (1) },
            { bandBuf[3].getWritePointer (0), bandBuf[3].getWritePointer (1) } };
        splitter.process (in, out, n);
    }
    else
    {
        // the Pultec curve sits before the iron in both phase modes, so both sound the same
        {
            float e[4];
            for (int b = 0; b < 4; ++b) e[b] = bands[b].amount.getCurrentValue();
            const auto k = Dsp::pultec (sampleRate, e, xoNow, ms.eq);
            for (int ch = 0; ch < 2; ++ch)
            {
                eqBuf.copyFrom (ch, 0, dryBuf, ch, 0, n);
                float* d = eqBuf.getWritePointer (ch);
                for (int st = 0; st < Dsp::pultecStages; ++st)
                {
                    const auto& c = k[(size_t) st];
                    eq[st][ch].k = c;
                    if (std::abs (c.b0 - 1.0) < 1.0e-9 && std::abs (c.a1) < 1.0e-9 && std::abs (c.b1) < 1.0e-9) { eq[st][ch].reset(); continue; }
                    for (int i = 0; i < n; ++i) d[i] = eq[st][ch].process (d[i]);
                }
            }
        }
        split1.setCutoffFrequency (xoNow[0]); comp1.setCutoffFrequency (xoNow[0]); dryAp1.setCutoffFrequency (xoNow[0]);
        split2.setCutoffFrequency (xoNow[1]); dryAp2.setCutoffFrequency (xoNow[1]);
        split3.setCutoffFrequency (xoNow[2]); comp3.setCutoffFrequency (xoNow[2]); dryAp3.setCutoffFrequency (xoNow[2]);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* x = eqBuf.getReadPointer (ch);
            const float* xd = dryBuf.getReadPointer (ch);
            float* b0 = bandBuf[0].getWritePointer (ch); float* b1 = bandBuf[1].getWritePointer (ch);
            float* b2 = bandBuf[2].getWritePointer (ch); float* b3 = bandBuf[3].getWritePointer (ch);
            float* ap = apBuf.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float lo, hi;
                split2.processSample (ch, x[i], lo, hi);
                lo = comp3.processSample (ch, lo);       // low branch gets the phase of the xover3 split
                hi = comp1.processSample (ch, hi);       // high branch gets the phase of the xover1 split
                split1.processSample (ch, lo, b0[i], b1[i]);
                split3.processSample (ch, hi, b2[i], b3[i]);
                ap[i] = dryAp3.processSample (ch, dryAp2.processSample (ch, dryAp1.processSample (ch, xd[i])));
            }
        }
    }

    // ---- bands
    for (int b = 0; b < 4; ++b)
        processBand (b, n, age, ms);

    // ---- sum with solo / mute
    wetBuf.clear (0, n);
    for (int b = 0; b < 4; ++b)
    {
        const float target = anySolo ? (soloP[b]->load() > 0.5f ? 1.0f : 0.0f) : (muteP[b]->load() > 0.5f ? 0.0f : 1.0f);
        const float g0 = bands[b].gain;
        bands[b].gain = target;
        for (int ch = 0; ch < 2; ++ch)
            wetBuf.addFromWithRamp (ch, 0, bandBuf[b].getReadPointer (ch), n, g0, target);
    }

    // ---- output: mix, idle path (true bypass when every knob is at zero), output gain, bypass
    const int lat = totalLatency();
    float env = envOut;
    const float envA = (float) std::exp (-1.0 / (0.15 * sampleRate));

    for (int i = 0; i < n; ++i)
    {
        const float m = mixS.getNextValue(), og = outS.getNextValue(), bp = bypassS.getNextValue(), wa = wetActiveS.getNextValue();
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            rawDelay.pushSample (ch, dryBuf.getSample (ch, i));
            const float raw = lat > 0 ? rawDelay.popSample (ch, (float) lat) : dryBuf.getSample (ch, i);
            float dryMix = raw;
            if (! linear)
            {
                apDelay.pushSample (ch, apBuf.getSample (ch, i));
                dryMix = osLatency > 0 ? apDelay.popSample (ch, (float) osLatency) : apBuf.getSample (ch, i);
            }
            float y = dryMix + (wetBuf.getSample (ch, i) - dryMix) * m;
            y = raw + (y - raw) * wa;
            y *= og;
            y = y + (raw - y) * bp;
            if (ch < numCh) io[ch][i] = y;
            peak = jmax (peak, std::abs (y));
        }
        env = peak > env ? peak : env * envA;
    }
    envOut = env;
    drive = jmin (1.0f, env);
}

//==============================================================================
void BreatheProcessor::processBand (int b, int n, float age, const Dsp::ModeScale& ms)
{
    auto& B = bands[b];
    auto& buf = bandBuf[b];
    const double fs = sampleRate;
    const float targetAct = B.amount.getTargetValue() > 0.0f ? 1.0f : 0.0f;
    const float actStep = (float) (1.0 / (0.02 * fs));

    // the untouched band, delayed like the processed one (for knobs at zero)
    float* d0 = buf.getWritePointer (0);
    float* d1 = buf.getWritePointer (1);
    if (B.act <= 0.0f && targetAct <= 0.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            bandDelay[b].pushSample (0, d0[i]); bandDelay[b].pushSample (1, d1[i]);
            if (osLatency > 0) { d0[i] = bandDelay[b].popSample (0, (float) osLatency); d1[i] = bandDelay[b].popSample (1, (float) osLatency); }
        }
        B.amount.skip (n);
        optoGr[b] = 0.0f; liftDb[b] = 0.0f;
        return;
    }

    // keep a delayed copy for the cross-fade between processed and untouched
    float dl0[2048], dl1[2048];
    for (int i = 0; i < n; ++i)
    {
        bandDelay[b].pushSample (0, d0[i]); bandDelay[b].pushSample (1, d1[i]);
        dl0[i] = osLatency > 0 ? bandDelay[b].popSample (0, (float) osLatency) : d0[i];
        dl1[i] = osLatency > 0 ? bandDelay[b].popSample (1, (float) osLatency) : d1[i];
    }

    // ---- dynamics at the base rate: opto squeeze + Dolby-style lift of quiet details
    const float oMax = ms.opto[b], lMax = ms.lift[b];
    auto coef = [fs] (double sec) { return 1.0 - std::exp (-1.0 / (sec * fs)); };
    const double cFast = coef (0.005), cRef = coef (3.0), cRefUp = coef (0.4), cLa = coef (0.003), cLr = coef (0.06), cMeas = coef (0.8);
    const float aF = (float) coef (0.010), rF = (float) coef (0.08), aS = (float) coef (0.6), rS = (float) coef (1.5), cMk = (float) coef (2.5);
    const float cLift = (float) coef (0.005), cSag = (float) coef (0.02);
    const auto& R = B.recipe;
    const float kTube = std::sqrt (7.2f * R.tube * ms.sat) / Dsp::refAmp, kTape = std::sqrt (12.0f * R.tape * ms.sat) / Dsp::refAmp;
    const float kXf = 4.0f * R.transformer * ms.sat / (Dsp::refAmp * Dsp::refAmp);
    const float baseBias = 0.222f * Dsp::refAmp;
    const float life = b == 0 ? 0.0f : ms.life;          // no wobble below ~100 Hz: the low end stays solid

    float lastGr = 0, lastLift = 0;
    for (int i = 0; i < n; ++i)
    {
        const float e = B.amount.getNextValue();
        const float gi = inGainA[(size_t) i];
        d0[i] *= gi; d1[i] *= gi;
        const double p = 0.5 * ((double) d0[i] * d0[i] + (double) d1[i] * d1[i]);
        B.pFast += cFast * (p - B.pFast);
        B.pRef += (p > B.pRef ? cRefUp : cRef) * (p - B.pRef);
        B.pLift += (p > B.pLift ? cLa : cLr) * (p - B.pLift);
        const float refDb = (float) (10.0 * std::log10 (B.pRef + 1.0e-12));

        float gainDb = 0.0f;
        if (oMax > 0.0f)
        {
            const float lvlDb = (float) (10.0 * std::log10 (B.pFast + 1.0e-12));
            const float over = lvlDb - (refDb + 2.0f);
            const float soft = over < -3.0f ? 0.0f : over < 3.0f ? (over + 3.0f) * (over + 3.0f) / 12.0f : over;
            const float grT = jmin (oMax * e, soft * 0.75f) * (refDb > -80.0f ? 1.0f : 0.0f);
            B.optoF += (grT > B.optoF ? aF : rF) * (grT - B.optoF);
            B.optoS += (grT > B.optoS ? aS : rS) * (grT - B.optoS);
            const float gr = jmax (B.optoF, B.optoS);
            B.makeup += cMk * (gr - B.makeup);
            gainDb += B.makeup - gr;
            lastGr = gr;
        }
        if (lMax > 0.0f)
        {
            const float lDb = (float) (10.0 * std::log10 (B.pLift + 1.0e-12));
            const float t = jlimit (0.0f, 1.0f, (refDb - 3.0f - lDb) / 20.0f);
            const float gate = jlimit (0.0f, 1.0f, (lDb + 85.0f) / 15.0f);
            const float target = lMax * e * t * t * (3.0f - 2.0f * t) * gate;
            B.lift += cLift * (target - B.lift);
            gainDb += B.lift;
            lastLift = B.lift;
        }
        if (gainDb != 0.0f)
        {
            const float g = std::exp (gainDb * 0.11512925f);
            d0[i] *= g; d1[i] *= g;
        }
        B.pPre[0] += cMeas * ((double) d0[i] * d0[i] - B.pPre[0]);
        B.pPre[1] += cMeas * ((double) d1[i] * d1[i] - B.pPre[1]);

        // the supply sags on loud moments: more drive, more bias (this is dynamics, not wobble, so the low band keeps it)
        const float sagT = jlimit (0.0f, 2.0f, (float) std::sqrt (B.pFast / (B.pRef + 1.0e-12)) - 1.0f);
        B.sag += cSag * (sagT - B.sag);
        const float se = std::sqrt (e);
        for (int ch = 0; ch < 2; ++ch)
        {
            // the channels differ a little (part tolerances) but never drift in level: only the harmonics shimmer
            // Drive (and so the level) is the same in both channels. The channels differ only in the tube bias:
            // that colours the even harmonics, and its fast shimmer is the "breathing" of the spectrum.
            const float driveMul = 1.0f + 0.15f * B.sag;
            const float shim = life > 0.0f ? (0.3f + 0.7f * age) * life * B.shimmer[ch].next() : 0.0f;
            const float biasMul = (1.0f + B.tolBias[ch] * (0.5f + age)) * (1.0f + 0.5f * B.sag) * (1.0f + jlimit (-0.9f, 2.0f, shim));
            jitA[ch][(size_t) i] = 0.0f;
            gTubeA[ch][(size_t) i] = kTube * se * driveMul;
            bTubeA[ch][(size_t) i] = baseBias * biasMul;
            gTapeA[ch][(size_t) i] = kTape * se * driveMul;
            tXfA[ch][(size_t) i] = kXf * e * driveMul * driveMul;
        }
    }
    optoGr[b] = lastGr;
    liftDb[b] = lastLift;

    // ---- non-linear stage, oversampled, in mid/side: whatever sits in the centre is treated identically
    //      in both channels, so the stereo image can never shift, however hard the iron is driven.
    auto linkedIron = [&B] (float* l, float* r, int count, int shift, const float* gT, const float* bT, const float* gP, const float* tX,
                            const float* bT2)
    {
        auto& sm = B.sat[0];
        auto& ss = B.sat[1];
        for (int i = 0; i < count; ++i)
        {
            const int k = i >> shift;
            const float mid = 0.5f * (l[i] + r[i]), side = 0.5f * (l[i] - r[i]);
            const float m = sm.process (mid, gT[k], bT[k], gP[k], tX[k], 0.0f);
            const float sd = ss.process (side, gT[k], bT2[k], gP[k], tX[k], 0.0f);
            l[i] = m + sd;
            r[i] = m - sd;
        }
    };
    auto blk = dsp::AudioBlock<float> (buf).getSubBlock (0, (size_t) n);
    if (auto* o = currentOs (b))
    {
        auto up = o->processSamplesUp (blk);
        linkedIron (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), ovsIndex,
                    gTubeA[0].data(), bTubeA[0].data(), gTapeA[0].data(), tXfA[0].data(), bTubeA[1].data());
        o->processSamplesDown (blk);
    }
    else
        linkedIron (d0, d1, n, 0, gTubeA[0].data(), bTubeA[0].data(), gTapeA[0].data(), tXfA[0].data(), bTubeA[1].data());

    // ---- level after the iron stays where it was, IN gain is taken back,
    //      DC blocker (bias makes a little DC), then cross-fade with the untouched band
    const float R5 = (float) (1.0 - 2.0 * Dsp::pi * 5.0 / fs);
    const float cSg = (float) coef (0.3);
    for (int i = 0; i < n; ++i)
    {
        B.pPost[0] += cMeas * ((double) d0[i] * d0[i] - B.pPost[0]);
        B.pPost[1] += cMeas * ((double) d1[i] * d1[i] - B.pPost[1]);
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buf.getWritePointer (ch);
        const float* dl = ch == 0 ? dl0 : dl1;
        // linked: one make-up gain for both channels, so left and right never drift apart
        const double post = B.pPost[0] + B.pPost[1], pre = B.pPre[0] + B.pPre[1];
        const float sgT = post > 1.0e-10 ? jlimit (0.5f, 2.0f, (float) std::sqrt (pre / post)) : 1.0f;
        float act = B.act, sg = B.satGain[ch];
        for (int i = 0; i < n; ++i)
        {
            sg += cSg * (sgT - sg);
            const float x = d[i] * sg / inGainA[(size_t) i];
            const float y = x - B.dcX[ch] + R5 * B.dcY[ch];
            B.dcX[ch] = x;
            B.dcY[ch] = y;
            act = targetAct > act ? jmin (1.0f, act + actStep) : jmax (0.0f, act - actStep);
            d[i] = dl[i] + (y - dl[i]) * act;
        }
        B.satGain[ch] = sg;
        if (ch == 1) B.act = act;
    }
    if (B.act <= 0.0f)
        for (auto& s : B.sat) s.reset();
}

//==============================================================================
AudioProcessorEditor* BreatheProcessor::createEditor() { return new BreatheEditor (*this); }

void BreatheProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BreatheProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BreatheProcessor(); }
