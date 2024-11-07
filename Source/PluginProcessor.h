/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


//==============================================================================
/**
*/
class CanvasClipperAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    CanvasClipperAudioProcessor();
    ~CanvasClipperAudioProcessor() override;

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

private:
    //==============================================================================

    juce::AudioProcessorValueTreeState state;

    // float smoothedGainCompensation = 1.0f;
    //const float smoothingCoeff = 0.995f; // changes smoothign amount for gain match
    int oversamplingFactor = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanvasClipperAudioProcessor)
};
