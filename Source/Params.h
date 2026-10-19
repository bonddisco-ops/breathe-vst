#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs and layout for Breathe
namespace Params
{
    constexpr int numBands = 4;

    inline const char* const band[numBands] = { "low", "lowMid", "highMid", "high" };
    inline const char* const bandName[numBands] = { "LOW", "LOW-MID", "HIGH-MID", "HIGH" };
    inline const char* const xover[3] = { "xover1", "xover2", "xover3" };
    inline const char* const solo[numBands] = { "solo1", "solo2", "solo3", "solo4" };
    inline const char* const mute[numBands] = { "mute1", "mute2", "mute3", "mute4" };

    inline const char* const mix = "mix";
    inline const char* const age = "age";
    inline const char* const output = "output";
    inline const char* const ovs = "ovs";
    inline const char* const lin = "linear";
    inline const char* const bypass = "bypass";
    inline const char* const input = "input";
    inline const char* const mode = "mode";

    // Advanced trims, -100..+100 %, centre = the factory recipe
    inline const char* const advEq[numBands] = { "advEq1", "advEq2", "advEq3", "advEq4" };
    inline const char* const advDrive[numBands] = { "advDrive1", "advDrive2", "advDrive3", "advDrive4" };
    inline const char* const advDyn[numBands] = { "advComp1", "advComp2", "advLift3", "advLift4" };   // COMP on LOW/LOW-MID, LIFT on HIGH-MID/HIGH
    inline const char* const advDynName[numBands] = { "COMP", "COMP", "LIFT", "LIFT" };

    inline const juce::StringArray ovsNames { "1x", "2x", "4x", "8x", "16x" };

    // crossover ranges (Hz) and defaults
    constexpr float xoMin[3] = { 40.0f, 250.0f, 2000.0f };
    constexpr float xoMax[3] = { 400.0f, 4000.0f, 16000.0f };
    constexpr float xoDef[3] = { 120.0f, 1000.0f, 6000.0f };

    inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout l;
        auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };

        for (int b = 0; b < numBands; ++b)
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { band[b], 1 }, bandName[b],
                       NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                       AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

        for (int i = 0; i < 3; ++i)
        {
            NormalisableRange<float> r (xoMin[i], xoMax[i]);
            r.setSkewForCentre (std::sqrt (xoMin[i] * xoMax[i]));
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { xover[i], 1 }, String ("Crossover ") + String (i + 1), r, xoDef[i],
                       AudioParameterFloatAttributes().withLabel ("Hz")
                           .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; })));
        }

        for (int b = 0; b < numBands; ++b)
        {
            l.add (std::make_unique<AudioParameterBool> (ParameterID { solo[b], 1 }, String (bandName[b]) + " Solo", false));
            l.add (std::make_unique<AudioParameterBool> (ParameterID { mute[b], 1 }, String (bandName[b]) + " Mute", false));
        }

        l.add (std::make_unique<AudioParameterFloat> (ParameterID { mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f,
                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { age, 1 }, "Age", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { output, 1 }, "Output", NormalisableRange<float> (-30.0f, 30.0f), 0.0f,
                   AudioParameterFloatAttributes().withLabel ("dB")
                       .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { input, 1 }, "Input", NormalisableRange<float> (-30.0f, 30.0f), 0.0f,
                   AudioParameterFloatAttributes().withLabel ("dB")
                       .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
        auto trim = [] (float v, int) { return (v > 0.04f ? "+" : v < -0.04f ? "-" : "") + String (std::abs (v), 1) + " %"; };
        for (int b = 0; b < numBands; ++b)
        {
            const auto attr = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (trim);
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { advEq[b], 1 }, String (bandName[b]) + " EQ", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, attr));
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { advDrive[b], 1 }, String (bandName[b]) + " Drive", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, attr));
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { advDyn[b], 1 }, String (bandName[b]) + " " + (b < 2 ? "Comp" : "Lift"), NormalisableRange<float> (-100.0f, 100.0f), 0.0f, attr));
        }
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { mode, 1 }, "Mode", StringArray { "MASTER", "MIX" }, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ovs, 1 }, "Oversampling", ovsNames, 1));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { lin, 1 }, "Linear Phase", true));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));
        return l;
    }
}
