
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

        // user controlled variables
        std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", juce::NormalisableRange<float>(0.f, 18.0f, .1f, 1.0f),0.0f, "dB"),
        std::make_unique<juce::AudioParameterFloat>("outputGain", "Output Gain", juce::NormalisableRange<float>(-18.f, 12.0f, .5f, 1.0f), 0.0f, "dB"),
        std::make_unique<juce::AudioParameterChoice>("saturationType", "Saturation Type", juce::StringArray{ "Soft", "Hard"}, 0),
        std::make_unique<juce::AudioParameterChoice>("analogType", "Analog Type", juce::StringArray{ "None", "Transformer", "Tape"}, 0),
        std::make_unique<juce::AudioParameterBool>("analogDrive", "Analog Drive", false),
        std::make_unique<juce::AudioParameterChoice>("os", "Over Sampling", juce::StringArray{ "None", "2x", "4x"}, 0),
        
        // experiemental variables - to be removed eventually
        std::make_unique<juce::AudioParameterFloat>("softLimitCoefficient","softLimitCoefficient",juce::NormalisableRange<float>(1.0f, 10.0f, .1f, 1.f), 1.0f),
        std::make_unique<juce::AudioParameterFloat>("transformerize", "transformerize", juce::NormalisableRange<float>(0.f, 1.0f, .1f, 1.f),0.1f),
        std::make_unique<juce::AudioParameterFloat>("evenAmount", "evenAmount", juce::NormalisableRange<float>(0.f, 1.0f, .1f, 1.f),0.f),

        // neve style preamp variables:
        // A_mix sets the ratio of clean signal to clipped in the soft clipper and defaults to 0.04. That is about what the Waves Omega-N sets it to
        // A_v is the amount of gain pushed into the clipped signal. Right now it is somehwhat but not completely redunant with input gain.
        // I tried adding a DC offset to allow for introduction of even-order harmonics but it doesn't sound great yet
        std::make_unique<juce::AudioParameterFloat>("A_mix", "A_mix", juce::NormalisableRange<float>(0.f, 1.0f, .01f, 1.f),0.04f),  
        std::make_unique<juce::AudioParameterFloat>("A_v", "A_v", juce::NormalisableRange<float>(0.f, 50.0f, 1.f, 1.f),0.f),
        std::make_unique<juce::AudioParameterFloat>("dcOffset", "DC Offset", juce::NormalisableRange<float>(0.f, 1.0f, .01f, 1.f), 0.0f),

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

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumInputChannels();

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
    float inputGainDB = *state.getRawParameterValue("inputGain");  
    float inputGainLinear = std::pow(10.0f, inputGainDB / 20.0f);  // converting from db to gain

    float outputGainDB = *state.getRawParameterValue("outputGain");
    float outputGainLinear = std::pow(10.0f, outputGainDB / 20.f); 

    float transformerizeCoefficient = *state.getRawParameterValue("transformerize");

    float A_mix = *state.getRawParameterValue("A_mix");
    float A_v = *state.getRawParameterValue("A_v");
    float dcOffset = *state.getRawParameterValue("dcOffset");

    float softLimitCoefficient = *state.getRawParameterValue("softLimitCoefficient");
    int os = state.getParameter("os")->getValue();

    float evenAmount = *state.getRawParameterValue("evenAmount");


    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        auto* typeParam = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter("saturationType"));
        if (typeParam != nullptr)
        {
            juce::String type = typeParam->getCurrentChoiceName();

            // Process each sample
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                // INPUT GAIN STAGE 
                float sample = channelData[i] * inputGainLinear;

                // CLIPPING STAGE

                if (type == "Soft")
                {
                    // Add DC offset  
                    float dcShifted = sample + dcOffset;

                    float soft_saturation = sample + A_mix * std::tanh(A_v * sample);

                    // Remove DC offset 
                    sample = soft_saturation - dcOffset;
                }
                else if (type == "Hard")
                {
                    // lop that ish off at 1.0
                    sample = std::max(-1.0f, std::min(1.0f, sample));
                }
                // ANALOG STAGE


                //OUTPUT GAIN STAGE
                sample *= outputGainLinear;

                channelData[i] = sample;
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
