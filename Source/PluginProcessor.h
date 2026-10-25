// SeniorMy Structure — processador do plugin (AU/VST3/Standalone).
// Instrumento com entrada de áudio (trilha/sidechain) e MIDI; gera coberturas orquestrais.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "core/Engine.h"
#include "SampleLibrary.h"
#include "PresetManager.h"

class StructureProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    StructureProcessor();
    ~StructureProcessor() override;

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- interface ---------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    sms::Engine engine;
    PresetManager presets;

    const sms::Arrangement& getArrangement() const { return arrangement; }
    void setArrangement (const sms::Arrangement&, bool markEdited = true);
    void applyPreset (const sms::PresetData&);
    bool loadPresetEntry (int index);
    bool saveUserPreset (const juce::String& name, juce::String& error);
    sms::PresetData currentAsPreset (const juce::String& name) const;
    juce::String getPresetName() const { return presetName + (presetEdited ? " *" : ""); }

    void importLoop (const juce::File&, std::function<void (bool ok, juce::String message)> done);
    std::shared_ptr<const sms::Progression> getProgression() const { return progression; }
    void clearProgression();
    juce::File exportCoverMidi (int repeats, juce::String& error);
    sms::ArrangerSettings currentArrangerSettings() const;

    juce::String getLibraryStatus() const { return libraryStatus; }
    float getLibraryProgress() const { return loader.getProgress(); }
    bool isLibraryLoading() const { return loader.isLoading(); }
    bool isUsingFallbackBank() const { return usingFallback; }

    bool termsAccepted() const { return termsOk.load(); }
    void markTermsAccepted() { termsOk.store (true); }

    float uiScale = 1.0f;
    double lastHostBpm = 0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEngineParams();
    void timerCallback() override;
    void startLibraryLoad (double rate);

    sms::Arrangement arrangement;
    juce::String presetName { "Cinematic Pad" };
    bool presetEdited = false;
    int currentProgram = 0;
    std::shared_ptr<const sms::Progression> progression;
    SampleLibraryLoader loader;
    juce::String libraryStatus;
    bool usingFallback = false;
    double loadedRate = 0;
    std::atomic<bool> termsOk { false };
    juce::ThreadPool importPool { 1 };
    std::shared_ptr<int> alive = std::make_shared<int> (1);   // protege callbacks assíncronos
    juce::AudioBuffer<float> inCopy;

    // ponteiros de parâmetros (leitura rápida no áudio)
    struct Ptrs
    {
        std::atomic<float> *mode, *autoKey, *keyRoot, *scale, *sensitivity, *sevenths, *lock, *conform,
            *extensions, *inversion, *density, *reg, *spread, *humanize, *dynamics, *attack, *release,
            *reverbMix, *reverbSize, *reverbDecay, *pianoLevel, *pianoInput, *pianoCover, *coverOn,
            *monitor, *master, *bpm;
        std::atomic<float>* level[6];
        std::atomic<float>* lane[8];
    } ptr {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StructureProcessor)
};
