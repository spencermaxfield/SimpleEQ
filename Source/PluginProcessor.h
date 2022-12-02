/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct SimpleEQSettings
{
    // Low cut settings
    float lowCutFreq { 0 };
    int lowCutSlope { 0 };
    
    // Peak band settings
    float peakFreq { 0 };
    float peakGain { 0 };
    float peakQ { 1.f };
    
    // High cut settings
    float highCutFreq { 0 };
    int highCutSlope { 0 };
};

SimpleEQSettings getChainSettings(juce::AudioProcessorValueTreeState& ap_tree_state);

// Parameter ID's
const std::string LOW_CUT_FREQ = "LOW_CUT_FREQ";
const std::string LOW_CUT_SLOPE = "LOW_CUT_SLOPE";

const std::string PEAK_FREQ = "PEAK_FREQ";
const std::string PEAK_GAIN = "PEAK_GAIN";
const std::string PEAK_Q = "PEAK_Q";

const std::string HIGH_CUT_FREQ = "HIGH_CUT_FREQ";
const std::string HIGH_CUT_SLOPE = "HIGH_CUT_SLOPE";

//Parameter Labels
const std::string LOW_CUT_FREQ_LABEL = "Low Cut Frequency";
const std::string LOW_CUT_SLOPE_LABEL = "Low Cut Slope";

const std::string PEAK_FREQ_LABEL = "Peak Frequency";
const std::string PEAK_GAIN_LABEL = "Peak Gain";
const std::string PEAK_Q_LABEL = "Peak Q";

const std::string HIGH_CUT_FREQ_LABEL = "High Cut Frequency";
const std::string HIGH_CUT_SLOPE_LABEL = "High Cut Slope";

//==============================================================================
/**
*/
class SimpleEQAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    SimpleEQAudioProcessor();
    ~SimpleEQAudioProcessor() override;

    //==============================================================================
    // Called when playback is about to start
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    // Called by host with blocks of audio to be processed
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
    
    juce::AudioProcessorValueTreeState ap_tree_state {*this, nullptr, "Parameters", createParameterLayout()};

private:
    
    // Alias for a single filter. We'll use this for the peak band
    using Filter = juce::dsp::IIR::Filter<float>;
    
    // Alias for the high and low cut bands. Each time that band is processed,
    // it's filtered at 12 db/octave. For each additional 12 db/octave increment,
    // we re-process the audio. Since we have options of 12, 24, 36, and 48 db/oct,
    // we need to process the audio up to four times for the low/high cut bands.
    // Therefore this is a chain of four filters.
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    
    // The entire processing chain for one channel of audio (aka 'mono') is
    // low cut, peak, high cut. Hence the <CutFilter, Filter, CutFilter> generics
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
    
    // Processing chains for both of the stereo channels (left and right)
    MonoChain leftChain, rightChain;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessor)
};
