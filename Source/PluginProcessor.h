// SeniorMy Acoustic Guitars — processador do plugin (AU/VST3/Standalone): MIDI → motor,
// relógio da DAW, estado (presets, padrões, progressão), biblioteca e Termos de Uso.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "core/Engine.h"
#include "SampleLibrary.h"
#include "PresetManager.h"

class GuitarsProcessor : public juce::AudioProcessor
{
public:
    GuitarsProcessor();
    ~GuitarsProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    sms::Engine engine;
    PresetManager presets;

    // estado editável (thread de mensagens)
    sms::EngineState getState() const;
    void setState (const sms::EngineState&, bool markEdited = true);
    void applyPreset (const sms::PresetData&);
    bool loadPresetEntry (int index);
    bool saveUserPreset (const juce::String& name, juce::String& error);
    juce::String getPresetName() const { return presetName + (presetEdited ? " *" : ""); }
    juce::String getPresetDescription() const { return presetDescription; }

    void importLoop (const juce::File&, std::function<void (bool ok, juce::String message)> done);
    juce::File exportMidi (int repeats, const juce::File& target, juce::String& error);

    juce::String getLibraryStatus() const { return libraryStatus; }
    float getLibraryProgress() const { return loader.getProgress(); }
    bool isLibraryLoading() const { return loader.isLoading(); }
    bool hasLibrary() const { return ! banks.empty() && banks.back() != nullptr; }

    bool termsAccepted() const { return termsOk.load(); }
    void markTermsAccepted() { termsOk.store (true); }

    float uiScale = 1.0f;
    std::atomic<int> stateVersion { 0 };   // muda quando o estado vem de fora da interface
    int uiPart = 0;
    std::atomic<double> hostBpm { 0 };
    std::atomic<bool> hostPlaying { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushParamsFromState (const sms::EngineState&);

    mutable juce::SpinLock stateLock;
    sms::EngineState uiState {};
    std::atomic<bool> stateDirty { true };
    juce::String presetName, presetDescription;
    bool presetEdited = false;
    int currentProgram = 0;
    SampleLibraryLoader loader;
    std::vector<std::shared_ptr<sms::Bank>> banks;   // mantidos vivos até o fim
    juce::String libraryStatus;
    std::atomic<bool> termsOk { false };
    juce::ThreadPool importPool { 1 };
    std::shared_ptr<int> alive = std::make_shared<int> (1);
    struct Ptrs { std::atomic<float>* master; std::atomic<float>* reverb; std::atomic<float>* bpm; std::atomic<float>* play; std::atomic<float>* level[sms::kParts]; } ptr {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarsProcessor)
};
