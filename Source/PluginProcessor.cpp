/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQAudioProcessor::SimpleEQAudioProcessor()
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

SimpleEQAudioProcessor::~SimpleEQAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    // Set up the processing spec to be used by each processing chain.
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;  // We process left and right channels separately, so 1 channel per spec
    spec.sampleRate = sampleRate;
    
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    applyCoefficients(sampleRate);
}

void SimpleEQAudioProcessor::applyCoefficients(double sampleRate)
{
    auto chainSettings = getChainSettings(ap_tree_state);
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, chainSettings.peakFreq, chainSettings.peakQ, juce::Decibels::decibelsToGain(chainSettings.peakGain));
    
    // Apply the peak band coefficients to both channels
    leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

    // Apply the low cut to both channels. First calculate the order
    // based on the selected slope. Then get the coefficients and apply them.
    int lowCutOrder = (chainSettings.lowCutSlope + 1) * 2;
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq, sampleRate, lowCutOrder);
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();

    // Apply the low cut to both channels
    applyCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    applyCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    
    // Apply the high cut to both channels. First calculate the order
    // based on the selected slope. Then get the coefficients and apply them.
    int highCutOrder = (chainSettings.highCutSlope + 1) * 2;
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq, sampleRate, highCutOrder);
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

    // Apply the high cut to both channels
    applyCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    applyCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
}

void SimpleEQAudioProcessor::applyCutFilter(CutFilter& cutFilter, juce::ReferenceCountedArray<juce::dsp::FilterDesign<float>::IIRCoefficients> coefficients, Slope slope)
{
    
    // The first filter is always enabled since a 12 db/oct slope
    // is the lowest option
    cutFilter.setBypassed<Slope_12>(false);
    cutFilter.get<Slope_12>().coefficients = *coefficients[Slope_12];
    
    // Bypass the remaining cut filters to start. We'll re-enable
    // them as needed if a higher slope is selected
    cutFilter.setBypassed<Slope_24>(true);
    cutFilter.setBypassed<Slope_36>(true);
    cutFilter.setBypassed<Slope_48>(true);
    
    switch(slope)
    {
        case Slope_12:
            // do nothing. See comment above.
            break;
        case Slope_24:
            cutFilter.get<Slope_24>().coefficients = *coefficients[Slope_24];
            cutFilter.setBypassed<Slope_24>(false);
            break;
        case Slope_36:
            cutFilter.get<Slope_24>().coefficients = *coefficients[Slope_24];
            cutFilter.setBypassed<Slope_24>(false);
            cutFilter.get<Slope_36>().coefficients = *coefficients[Slope_36];
            cutFilter.setBypassed<Slope_36>(false);
            break;
        case Slope_48:
            cutFilter.get<Slope_24>().coefficients = *coefficients[Slope_24];
            cutFilter.setBypassed<Slope_24>(false);
            cutFilter.get<Slope_36>().coefficients = *coefficients[Slope_36];
            cutFilter.setBypassed<Slope_36>(false);
            cutFilter.get<Slope_48>().coefficients = *coefficients[Slope_48];
            cutFilter.setBypassed<Slope_48>(false);
            break;
    }
}

void SimpleEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SimpleEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    
    // Apply the coefficients to each channel before processing the audio
    applyCoefficients(getSampleRate());

    // Extract the right and left channels
    juce::dsp::AudioBlock<float> block(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    // Set up the contexts for each channel
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    // Process the audio
    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool SimpleEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEQAudioProcessor::createEditor()
{
//    return new SimpleEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

SimpleEQSettings getChainSettings(juce::AudioProcessorValueTreeState& ap_tree_state)
{
    SimpleEQSettings settings;
    
    settings.lowCutFreq = ap_tree_state.getRawParameterValue(LOW_CUT_FREQ)->load();
    settings.lowCutSlope = static_cast<Slope>(ap_tree_state.getRawParameterValue(LOW_CUT_SLOPE)->load());
    settings.highCutFreq = ap_tree_state.getRawParameterValue(HIGH_CUT_FREQ)->load();
    settings.highCutSlope = static_cast<Slope>(ap_tree_state.getRawParameterValue(HIGH_CUT_SLOPE)->load());
    settings.peakFreq = ap_tree_state.getRawParameterValue(PEAK_FREQ)->load();
    settings.peakGain = ap_tree_state.getRawParameterValue(PEAK_GAIN)->load();
    settings.peakQ = ap_tree_state.getRawParameterValue(PEAK_Q)->load();
    
    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Slope choices for the low and high cut parameters
    juce::StringArray cut_slopes;
    cut_slopes.add("12 db/oct");
    cut_slopes.add("24 db/oct");
    cut_slopes.add("36 db/oct");
    cut_slopes.add("48 db/oct");
    
    // Low Cut parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
                                                           LOW_CUT_FREQ,
                                                           LOW_CUT_FREQ_LABEL,
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f),
                                                           20.f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
                                                            LOW_CUT_SLOPE,
                                                            LOW_CUT_SLOPE_LABEL,
                                                            cut_slopes,
                                                            0));
    
    // High Cut parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
                                                           HIGH_CUT_FREQ,
                                                           HIGH_CUT_FREQ_LABEL,
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f),
                                                           20000.f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
                                                            HIGH_CUT_SLOPE,
                                                            HIGH_CUT_SLOPE_LABEL,
                                                            cut_slopes,
                                                            0));
    
    // Peak Band parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
                                                           PEAK_FREQ,
                                                           PEAK_FREQ_LABEL,
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.5f),
                                                           750.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
                                                           PEAK_GAIN,
                                                           PEAK_GAIN_LABEL,
                                                           juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 0.25),
                                                           0.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
                                                           PEAK_Q,
                                                           PEAK_Q_LABEL,
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 0.25),
                                                           1.f));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEQAudioProcessor();
}
