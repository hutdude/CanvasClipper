
/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CanvasClipperAudioProcessor::CanvasClipperAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif 
    , state(*this, nullptr, "STATE", {
        std::make_unique<juce::AudioParameterFloat>(
            "inputGain",
            "Input Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.0f, 1.0f),
            0.0f,
            "dB"
        ),
        std::make_unique<juce::AudioParameterChoice>(
            "type",
            "Type",
            juce::StringArray{ "Soft", "Hard"},
            0
        ),
           
        })
{
}

CanvasClipperAudioProcessor::~CanvasClipperAudioProcessor()
{
}

//==============================================================================
const juce::String CanvasClipperAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CanvasClipperAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool CanvasClipperAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool CanvasClipperAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double CanvasClipperAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CanvasClipperAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int CanvasClipperAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CanvasClipperAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String CanvasClipperAudioProcessor::getProgramName(int index)
{
    return {};
}

void CanvasClipperAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void CanvasClipperAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void CanvasClipperAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CanvasClipperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
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

void CanvasClipperAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // user specified parameters
    float inputGainDB = state.getParameter("inputGain")->getValue();
    float inputGainLinear = std::pow(10.0f, inputGainDB / 20.0f);  // converting from db to gain
    bool gainMatch = state.getParameter("gainMatch")->getValue();

    

    // Process the audio
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        // account for user input gain
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            channelData[i] *= inputGainLinear;
        }

        auto* typeParam = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter("type"));
        if (typeParam != nullptr)
        {
            juce::String type = typeParam->getCurrentChoiceName();

            if (type == "Soft")
            {
                // this is where we can change reponse of soft clipper. If the function is changed from tanh, the response will be changed
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    channelData[i] = std::tanh(channelData[i]);
                }
            }
            else if (type == "Hard")
            {
                // Lop that shit off at 1.0
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    channelData[i] = std::clamp(channelData[i], -1.0f, 1.0f);
                }
            }
        }
    }


   

    // Clear any extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
}

//==============================================================================
bool CanvasClipperAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* CanvasClipperAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void CanvasClipperAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    if (auto xmlState = state.copyState().createXml()) {
        copyXmlToBinary(*xmlState, destData);
    }
}

void CanvasClipperAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes)) {
        state.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CanvasClipperAudioProcessor();
}
