/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MimiEQAudioProcessor::MimiEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

MimiEQAudioProcessor::~MimiEQAudioProcessor()
{
}

//==============================================================================
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings chainSettings;
    
    chainSettings.lowCutFreq    = apvts.getRawParameterValue("LowCut Freq")->load();
    chainSettings.lowCutSlope   = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    chainSettings.highCutFreq   = apvts.getRawParameterValue("HighCut Freq")->load();
    chainSettings.highCutSlope  = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    chainSettings.peakFreq      = apvts.getRawParameterValue("Peak Freq")->load();
    chainSettings.peakQuality   = apvts.getRawParameterValue("Peak Quality")->load();
    chainSettings.peakGainDb    = apvts.getRawParameterValue("Peak Gain")->load();
    
    return chainSettings;
}

//==============================================================================
const juce::String MimiEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MimiEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MimiEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MimiEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MimiEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MimiEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MimiEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MimiEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MimiEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void MimiEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MimiEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    juce::dsp::ProcessSpec spec;
    
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    updateFilters();
}

void MimiEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MimiEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MimiEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // always update the parameter coefficients first before processing the audio
    updateFilters();
    
    juce::dsp::AudioBlock<float> block(buffer);
    
    // for stereo
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    // create processing contexts for each channel
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    leftChain.process(leftContext);
    rightChain.process(rightContext);

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
//    for (int channel = 0; channel < totalNumInputChannels; ++channel)
//    {
//        auto* channelData = buffer.getWritePointer (channel);
//
//        // ..do something to the data...
//    }
}

//==============================================================================
bool MimiEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MimiEQAudioProcessor::createEditor()
{
    // return new MimiEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void MimiEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    // state member is an instance of Juce Value Tree
    // we can use a memory output stream to write the apvts state to the memory block
    // because it serializes in memory super easily!
    
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void MimiEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    // use value tree helper function!
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
        updateFilters();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MimiEQAudioProcessor::createParameterLayout ()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // there are only going to be three parameter bands: low, high, parametric/peak
    
    // cut bands - frequency + slope controllers
    // parametric bands - frequency + width
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("LowCut Freq", 1),
                                                           "LowCut Freq",
                                                           juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 1.0f),
                                                           20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("HighCut Freq", 1),
                                                           "HighCut Freq",
                                                           juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 1.0f),
                                                           20000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("Peak Freq", 1),
                                                           "Peak Freq",
                                                           juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.5f),
                                                           750.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("Peak Gain", 1),
                                                           "Peak Gain",
                                                           juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f),
                                                           0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("Peak Quality", 1),
                                                           "Peak Quality",
                                                           juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f),
                                                           1.0f));
    
    // filters use parameters that are multiples of 12
    juce::StringArray arr;
    for (int i = 0; i < 4; i++)
    {
        juce::String str;
        str << (12 + i * 12);
        str << " db/Oct";
        arr.add(str);
    }
    
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("LowCut Slope", 1),
                                                            "LowCut Slope",
                                                            arr,
                                                            0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("HighCut Slope", 1),
                                                            "HighCut Slope",
                                                            arr,
                                                            0));
    
    return layout;
}

void MimiEQAudioProcessor::configurePeakChainCoefficients(const double& sampleRate, const ChainSettings& chainSettings) {
    auto peakCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                                          chainSettings.peakFreq,
                                                                          chainSettings.peakQuality,
                                                                          juce::Decibels::decibelsToGain(chainSettings.peakGainDb));
    
    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoeffs;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoeffs;
}

void MimiEQAudioProcessor::updateFilters() { 
    auto chainSettings = getChainSettings(apvts);
    
    // creates a filter out of the current settings of the GUI
    configurePeakChainCoefficients(getSampleRate(), chainSettings);
    
    auto lowCutCoeffs = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                 getSampleRate(),
                                                                                                 2 * (chainSettings.lowCutSlope + 1));
    configureCutChainCoefficients<ChainPositions::LowCut>(&leftChain, lowCutCoeffs, chainSettings.lowCutSlope);
    configureCutChainCoefficients<ChainPositions::LowCut>(&rightChain, lowCutCoeffs, chainSettings.lowCutSlope);
    
    auto highCutCoeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                                 getSampleRate(),
                                                                                                 2 * (chainSettings.highCutSlope + 1));
    configureCutChainCoefficients<ChainPositions::HighCut>(&leftChain, highCutCoeffs, chainSettings.highCutSlope);
    configureCutChainCoefficients<ChainPositions::HighCut>(&rightChain, highCutCoeffs, chainSettings.highCutSlope);
}


template <ChainPositions P, typename Coefficients>
void MimiEQAudioProcessor::configureCutChainCoefficients(MonoChain* chain, const Coefficients& cutCoeffs, const Slope& cutSlope) {
    auto& cut = chain->get<P>();
    
    // start by allowing all of them through
    cut.template setBypassed<SLOPE_12>(true);
    cut.template setBypassed<SLOPE_24>(true);
    cut.template setBypassed<SLOPE_32>(true);
    cut.template setBypassed<SLOPE_48>(true);
    
    // fall through because the slopes are cumulative
    switch (cutSlope) {
        case SLOPE_48:
        {
            *cut.template get<SLOPE_48>().coefficients = *cutCoeffs[SLOPE_48];
            cut.template setBypassed<SLOPE_48>(false);
        }
        case SLOPE_32:
        {
            *cut.template get<SLOPE_32>().coefficients = *cutCoeffs[SLOPE_32];
            cut.template setBypassed<SLOPE_32>(false);
        }
        case SLOPE_24:
        {
            *cut.template get<SLOPE_24>().coefficients = *cutCoeffs[SLOPE_24];
            cut.template setBypassed<SLOPE_24>(false);
        }
        case SLOPE_12:
        {
            *cut.template get<SLOPE_12>().coefficients = *cutCoeffs[SLOPE_12];
            cut.template setBypassed<SLOPE_12>(false);
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MimiEQAudioProcessor();
}
