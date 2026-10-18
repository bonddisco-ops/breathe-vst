#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"
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

    juce::AudioProcessorValueTreeState apvts;

    // ---- read by the editor
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    std::atomic<float> optoGr[4] {}, liftDb[4] {};       // what the band dynamics do right now (dB)
    std::atomic<float> activity { 0.0f };                 // how hard the unit works, 0..1 (drives the tube)
    std::atomic<float> drive { 0.0f };                    // output envelope, 0..1
    juce::AbstractFifo scopeFifo { 32768 };
    std::vector<float> scopeData = std::vector<float> (32768, 0.0f);
    double currentRate() const { return sampleRate; }

    // test helper: block until the newest linear-phase kernels are in use
    void waitForKernels();

private:
    double sampleRate = 48000.0;
    int maxBlock = 512;

    // parameters
    std::atomic<float>* knob[4] {};
    std::atomic<float>* xo[3] {};
    std::atomic<float>* soloP[4] {};
    std::atomic<float>* muteP[4] {};
    std::atomic<float> *mixP = nullptr, *ageP = nullptr, *outP = nullptr, *ovsP = nullptr, *linP = nullptr, *bypassP = nullptr;

    // ---- configuration that changes the latency
    int ovsIndex = -1;
    bool linear = true;
    int kernelSize = 4096;
    int osLatency = 0;
    void configure (int ovs, bool lin);
    int totalLatency() const { return (linear ? kernelSize / 2 : 0) + osLatency; }

    // ---- minimum-phase crossovers (Linkwitz-Riley 24 dB/oct with phase compensation)
    juce::dsp::LinkwitzRileyFilter<float> split1, split2, split3, comp1, comp3, dryAp1, dryAp2, dryAp3;
    float lrFreq[3] { 120.0f, 1000.0f, 6000.0f };

    // ---- linear-phase crossovers: one FIR per band, designed off the audio thread
    juce::dsp::ConvolutionMessageQueue convQueue;
    std::array<std::unique_ptr<juce::dsp::Convolution>, 4> conv;
    class Designer;
    std::unique_ptr<Designer> designer;
    juce::SpinLock pendingLock;
    Dsp::KernelSet pending;
    int pendingVersion = 0, loadedVersion = 0;
    double pendingRate = 0.0;
    void takeNewKernels();

    // ---- oversampling: [type 0 = IIR (min phase), 1 = FIR (linear)][factor 1..4]
    std::unique_ptr<juce::dsp::Oversampling<float>> os[2][5][4];
    juce::dsp::Oversampling<float>* currentOs (int band) const;

    // ---- per band state
    struct Band
    {
        Dsp::Saturator sat[2];
        Dsp::Drift driftDrive[2], driftBias[2];
        float tolDrive[2] {}, tolBias[2] {};
        double pFast = 0, pRef = 1.0e-6, pLift = 0;
        float optoF = 0, optoS = 0, makeup = 0, lift = 0, sag = 0;
        float dcX[2] {}, dcY[2] {};
        float act = 0, gain = 1;
        juce::SmoothedValue<float> amount;
        Dsp::Recipe recipe {};
    } bands[4];

    juce::AudioBuffer<float> bandBuf[4], dryBuf, wetBuf, apBuf;
    std::vector<float> gTubeA[2], bTubeA[2], gTapeA[2], tXfA[2];
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> rawDelay { 1 << 16 }, apDelay { 1 << 16 }, bandDelay[4];

    // ---- post EQ for minimum-phase mode
    Dsp::Biquad eq[Dsp::pultecStages][2];

    juce::SmoothedValue<float> mixS, outS, bypassS, wetActiveS;
    double humPhase = 0.0;
    float envOut = 0.0f;
    juce::Random rng;

    void processChunk (float* const* io, int numCh, int n);
    void processBand (int b, int n, float age);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreatheProcessor)
};
