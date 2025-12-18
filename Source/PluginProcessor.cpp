#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PrismAudioProcessor::PrismAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

PrismAudioProcessor::~PrismAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PrismAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 1. 노브 3개
    layout.add(std::make_unique<juce::AudioParameterFloat>("input", "Input Drive", -24.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("output", "Output Level", -24.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 100.0f, 100.0f));

    // 2. 알고리즘 선택 (Prism의 핵심)
    juce::StringArray algos;
    algos.add("Tanh (Soft)");
    algos.add("ArcTan (Warm)");
    algos.add("Hard Clip");
    algos.add("Sigmoid");
    algos.add("Sine Fold");
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("algo", "Algorithm", algos, 0));

    return layout;
}

const juce::String PrismAudioProcessor::getName() const { return JucePlugin_Name; }
bool PrismAudioProcessor::acceptsMidi() const { return false; }
bool PrismAudioProcessor::producesMidi() const { return false; }
bool PrismAudioProcessor::isMidiEffect() const { return false; }
double PrismAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int PrismAudioProcessor::getNumPrograms() { return 1; }
int PrismAudioProcessor::getCurrentProgram() { return 0; }
void PrismAudioProcessor::setCurrentProgram (int index) {}
const juce::String PrismAudioProcessor::getProgramName (int index) { return {}; }
void PrismAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

// [중요] 충돌 방지를 위해 Stereo만 허용 (Mono 트랙도 Stereo로 변환되어 들어옴)
bool PrismAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())
        return true;
    return false;
}

void PrismAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 오버샘플링 초기화
    oversampler.initProcessing(samplesPerBlock);
    oversampler.reset();
}

void PrismAudioProcessor::releaseResources() {}

// 5가지 색깔의 Saturation 로직
float PrismAudioProcessor::applySaturation(float sample, int algoType)
{
    switch (algoType)
    {
        case 0: return std::tanh(sample); // 기본
        case 1: return (2.0f / juce::MathConstants<float>::pi) * std::atan(sample); // 따뜻함
        case 2: return juce::jlimit(-1.0f, 1.0f, sample); // 디지털
        case 3: return 2.0f * (1.0f / (1.0f + std::exp(-sample))) - 1.0f; // 시그모이드
        case 4: return std::sin(sample); // 금속성 (Wave Folding)
        default: return sample;
    }
}

void PrismAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    if (buffer.getNumSamples() == 0) return;

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // 파라미터 로드
    float inputDB = apvts.getRawParameterValue("input")->load();
    float outputDB = apvts.getRawParameterValue("output")->load();
    float mixPercent = apvts.getRawParameterValue("mix")->load();
    int algoType = (int)apvts.getRawParameterValue("algo")->load();

    float inputGain = juce::Decibels::decibelsToGain(inputDB);
    float outputGain = juce::Decibels::decibelsToGain(outputDB);
    float wetAmount = mixPercent / 100.0f;
    float dryAmount = 1.0f - wetAmount;

    // Dry 신호 보관 (Mix용)
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // --- 오버샘플링 처리 ---
    juce::dsp::AudioBlock<float> block(buffer);
    
    // 안전장치: 채널이 존재할 때만 처리
    if (block.getNumChannels() > 0)
    {
        // 2배로 뻥튀기 (Up)
        juce::dsp::AudioBlock<float> oversampledBlock = oversampler.processSamplesUp(block);

        for (int channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
        {
            float* data = oversampledBlock.getChannelPointer(channel);
            
            for (int sample = 0; sample < oversampledBlock.getNumSamples(); ++sample)
            {
                // Input Gain -> Saturation
                data[sample] *= inputGain;
                data[sample] = applySaturation(data[sample], algoType);
            }
        }
        // 원래대로 복구 (Down)
        oversampler.processSamplesDown(block);
    }

    // --- Mix (Dry + Wet) ---
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* wetData = buffer.getWritePointer(channel);
        const float* dryData = dryBuffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float processed = wetData[sample] * outputGain;
            wetData[sample] = (dryData[sample] * dryAmount) + (processed * wetAmount);
        }
    }
}

bool PrismAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PrismAudioProcessor::createEditor() { return new PrismAudioProcessorEditor (*this); }

void PrismAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PrismAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// 플러그인 생성 함수
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PrismAudioProcessor(); }
