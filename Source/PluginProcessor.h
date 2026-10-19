#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"
#include "LinConv.h"
#include "Params.h"

class BreatheProcessor : public juce::AudioProcessor
{
public:
    BreatheProcessor();
    ~BreatheProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Breathe"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // A / B / C / D settings slots, copy / paste, full reset (message thread)
    int slot = 0;
    juce::ValueTree slots[4], clipboard;
    void selectSlot (int i);
    void copySettings() { clipboard = apvts.copyState(); }
    void pasteSettings();
    void resetAll();

    // ---- read by the editor
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> optoGr[4] {}, liftDb[4] {};       // what the band dynamics do right now (dB)
    std::atomic<float> drive { 0.0f };                    // output envelope, 0..1
    std::atomic<float> inLevelDb { -100.0f };             // RMS after the IN knob: what the "iron" gets
    std::atomic<float> liftRel[4] {}, thrDb[4] {};        // Advanced screen: band level vs its average (dB), opto threshold (dB RMS)
    std::atomic<int> scopeBand { -1 };                    // band the Advanced scope looks at (-1 = none)
    std::atomic<int> scopeSide { 0 };                     // which side the Advanced panel edits (0 = M / L, 1 = S / R)
    std::atomic<float> sideGr[2][4] {}, sideLift[2][4] {}, sideRel[2][4] {}, sideThr[2][4] {};   // per side: opto GR, lift, level vs average, threshold (dB)
    juce::AbstractFifo bandFifo { 1 << 17 };              // pairs: band before / after processing (mono)
    std::vector<float> bandData = std::vector<float> (1 << 17, 0.0f);
    juce::AbstractFifo scopeFifo { 32768 };
    std::vector<float> scopeData = std::vector<float> (32768, 0.0f);
    double currentRate() const { return sampleRate; }

    // test helper: design and offer kernels for the current settings right now
    void refreshKernelsNow();

private:
    double sampleRate = 48000.0;
    int maxBlock = 512;

    // parameters
    std::atomic<float>* knob[4] {};
    std::atomic<float>* xo[3] {};
    std::atomic<float>* soloP[4] {};
    std::atomic<float>* muteP[4] {};
    std::atomic<float> *mixP = nullptr, *ageP = nullptr, *outP = nullptr, *inP = nullptr, *ovsP = nullptr,
                       *linP = nullptr, *bypassP = nullptr, *modeP = nullptr, *monitorP = nullptr;
    std::atomic<float>* advEqP[4] {}, *advDriveP[4] {}, *advDynP[4] {};
    // second side (Side in M/S, Right in L/R): used when Link is off
    std::atomic<float>* knob2[4] {};
    std::atomic<float>* advEq2P[4] {}, *advDrive2P[4] {}, *advDyn2P[4] {};
    std::atomic<float> *linkP = nullptr, *procModeP = nullptr;
    bool linked = true, procMs = true;
    std::atomic<float> *autoInP = nullptr, *autoOutP = nullptr;
    // AUTO IN: slow input level -> gain that puts the iron at its working level; AUTO OUT: loudness match
    double autoInPow = 0.0158, autoOutIn = 1.0e-6, autoOutOut = 1.0e-6;   // 0.0158 = -18 dB RMS
    bool autoInPrimed = false;
    Dsp::Biquad kwIn[2][2], kwOut[2][2];                                  // rough K-weighting (low cut + presence shelf)
    juce::SmoothedValue<float> autoOutS;
public:
    std::atomic<float> autoInDb { 0.0f }, autoOutDb { 0.0f };         // what AUTO is doing right now, for the editor
private:
    float trimEq[2][4] {}, trimDrive[2][4] {}, trimDyn[2][4] {};      // [side][band], smoothed, -1..+1

    // ---- configuration that changes the latency
    int ovsIndex = -1;
    bool linear = true;
    int osLatency = 0;
    void configure (int ovs, bool lin);
    int totalLatency() const { return (linear ? splitter.latency() : 0) + osLatency + Dsp::ScrapeFlutter::delay; }
    Dsp::ScrapeFlutter flutter;

    // ---- minimum-phase crossovers (Linkwitz-Riley 24 dB/oct with phase compensation)
    juce::dsp::LinkwitzRileyFilter<float> split1, split2, split3, comp1, comp3, dryAp1, dryAp2, dryAp3;
    float lrFreq[3] { 120.0f, 1000.0f, 6000.0f };

    // ---- linear-phase split (Pultec curve folded in), filters designed on their own thread
    LinPhaseSplitter splitter;
    class Designer;
    std::unique_ptr<Designer> designer;

    // ---- oversampling: [type 0 = IIR (min phase), 1 = FIR (linear)][factor 1..4]
    std::unique_ptr<juce::dsp::Oversampling<float>> os[2][5][4];
    juce::dsp::Oversampling<float>* currentOs (int band) const;

    // ---- per band state
    struct Band
    {
        Dsp::Saturator sat[2];
        Dsp::Shimmer shimmer[2];
        float tolDrive[2] {}, tolBias[2] {};
        // [c] = processing channel: L/R, or M/S when the sides are unlinked in M/S mode.
        // Linked: one detector (index 0) drives both channels.
        double pFast[2] {}, pRef[2] { 1.0e-6, 1.0e-6 }, pLift[2] {}, pPre[2] { 1.0e-9, 1.0e-9 }, pPost[2] { 1.0e-9, 1.0e-9 };
        float optoF[2] {}, optoS[2] {}, makeup[2] {}, lift[2] {}, sag[2] {}, satGain[2] { 1, 1 };
        float dcX[2] {}, dcY[2] {};
        float act[2] {}, gain = 1;
        juce::SmoothedValue<float> amount[2];
        bool active() const { return act[0] > 0.0f || act[1] > 0.0f || amount[0].getTargetValue() > 0.0f || amount[1].getTargetValue() > 0.0f; }
        Dsp::Recipe recipe {};
    } bands[4];

    juce::AudioBuffer<float> bandBuf[4], dryBuf, wetBuf, apBuf;
    std::vector<float> gTubeA[2], bTubeA[2], gTapeA[2], tXfA[2], jitA[2], inGainA, scopePre;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> rawDelay { 1 << 16 }, apDelay { 1 << 16 }, bandDelay[4];

    // ---- post EQ for minimum-phase mode
    Dsp::Biquad eq[Dsp::pultecStages][2];

    juce::SmoothedValue<float> mixS, outS, bypassS, wetActiveS, inS, modeS;
    float envOut = 0.0f, ageGate = 1.0f;     // AGE fades out while any band is soloed or muted
    double inPow = 0.0;

    void processChunk (float* const* io, int numCh, int n);
    void processBand (int b, int n, float age, const Dsp::ModeScale& ms);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreatheProcessor)
};
