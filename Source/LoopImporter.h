// SeniorMy Structure — importação de loops externos (WAV/AIFF/FLAC/MP3 e MIDI) e exportação
// das linhas de cobertura geradas como arquivo MIDI (uma trilha por seção).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "core/LoopAnalysis.h"
#include "core/Arranger.h"

namespace LoopImporter
{
    bool isSupported (const juce::File&);
    juce::String wildcard();
    // Analisa um arquivo; retorna nullptr e preenche error se falhar. hostBpm > 0 vira dica de andamento.
    std::shared_ptr<sms::Progression> analyse (const juce::File&, double hostBpm, juce::String& error);
    double bpmFromFileName (const juce::String&);

    // Gera a cobertura sobre a progressão e grava um .mid (formato 1, 480 PPQ). Retorna o arquivo.
    juce::File exportCoverMidi (const sms::Progression&, const sms::Arrangement&, const sms::ArrangerSettings&,
                                int repeats, const juce::File& target, juce::String& error);

    juce::var progressionToVar (const sms::Progression&);
    std::shared_ptr<sms::Progression> progressionFromVar (const juce::var&);
}
