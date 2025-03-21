/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope {
    SLOPE_12,
    SLOPE_24,
    SLOPE_32,
    SLOPE_48
};

enum ChainPositions
{
    LowCut,
    Peak,
    HighCut
};

struct ChainSettings
{
    float peakFreq { 0 }, peakGainDb { 0 }, peakQuality { 1.0f };
    float lowCutFreq { 0 }, highCutFreq { 0 };
    Slope lowCutSlope { Slope::SLOPE_12 }, highCutSlope { Slope::SLOPE_12 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class MimiEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MimiEQAudioProcessor();
    ~MimiEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    juce::AudioProcessorValueTreeState apvts 
    {
        *this,
        nullptr,
        "Parameters",
        createParameterLayout(),
    };
    
private:
    using Filter = juce::dsp::IIR::Filter<float>; // filter alias
    
    // each filter within cutfilter has a 12db/octave response
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    // three filter sets total: lowcut, peak parametric, highcut
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
    
    MonoChain leftChain, rightChain;
    
    void configurePeakChainCoefficients(const double& sampleRate, const ChainSettings& chainSettings);
    
    template <ChainPositions P, typename Coefficients>
    void configureCutChainCoefficients(MonoChain* chain, const Coefficients& cutCoeffs, const Slope& cutSlope);
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MimiEQAudioProcessor)
};
