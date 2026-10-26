// SeniorMy Acoustic Guitars — localiza e carrega a biblioteca de amostras (library.json + FLAC)
// numa thread própria e monta o Bank do núcleo. Sem biblioteca → instrumentos modelados.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "core/Voices.h"
#include <functional>

class SampleLibraryLoader : private juce::Thread
{
public:
    SampleLibraryLoader();
    ~SampleLibraryLoader() override;

    static juce::Array<juce::File> candidateFolders();
    static juce::File findLibraryFolder();

    // Começa a carregar (assíncrono). onLoaded é chamado na thread de mensagens.
    void loadAsync (std::function<void (std::shared_ptr<sms::Bank>, juce::String status)> onLoaded);
    float getProgress() const { return progress.load(); }
    bool isLoading() const { return isThreadRunning(); }

    // Carregamento síncrono (testes).
    static std::shared_ptr<sms::Bank> loadFolder (const juce::File& folder, juce::String& status,
                                                  std::atomic<float>* progress = nullptr,
                                                  std::function<bool()> shouldExit = {});
private:
    void run() override;
    std::function<void (std::shared_ptr<sms::Bank>, juce::String)> callback;
    std::atomic<float> progress { 0.0f };
};
