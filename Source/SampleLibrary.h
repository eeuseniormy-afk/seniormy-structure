// SeniorMy Structure — localiza e carrega a biblioteca de amostras (library.json + FLAC) numa thread
// própria e monta o InstrumentBank do núcleo. Sem biblioteca → banco de reserva sintetizado.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "core/Sampler.h"
#include <functional>

class SampleLibraryLoader : private juce::Thread
{
public:
    SampleLibraryLoader();
    ~SampleLibraryLoader() override;

    // Pastas onde a biblioteca é procurada (na ordem).
    static juce::Array<juce::File> candidateFolders();
    static juce::File findLibraryFolder();

    // Começa a carregar (assíncrono). onLoaded é chamado na thread de mensagens.
    void loadAsync (double hostSampleRate, std::function<void (std::shared_ptr<sms::InstrumentBank>, juce::String status)> onLoaded);

    juce::String getStatus() const;
    float getProgress() const { return progress.load(); }
    bool isLoading() const { return isThreadRunning(); }

    // Carregamento síncrono (testes e linha de comando).
    static std::shared_ptr<sms::InstrumentBank> loadFolder (const juce::File& folder, juce::String& status,
                                                            std::atomic<float>* progress = nullptr,
                                                            std::function<bool()> shouldExit = {});

private:
    void run() override;

    double rate = 44100;
    std::function<void (std::shared_ptr<sms::InstrumentBank>, juce::String)> callback;
    mutable juce::CriticalSection statusLock;
    juce::String status;
    std::atomic<float> progress { 0.0f };
};
