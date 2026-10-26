// SeniorMy Acoustic Guitars — importação de loops (WAV/AIFF/FLAC/MP3 e MIDI) com detecção de
// andamento, tonalidade e acordes, e exportação das partes geradas em MIDI (uma trilha por parte).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "core/LoopAnalysis.h"
#include "core/Engine.h"

namespace LoopImporter
{
    bool isSupported (const juce::File&);
    juce::String wildcard();
    std::shared_ptr<sms::Progression> analyse (const juce::File&, double hostBpm, juce::String& error);
    double bpmFromFileName (const juce::String&);
    // escreve a progressão detectada (acordes, tonalidade, andamento) no estado
    void applyToHarmony (const sms::Progression&, sms::EngineState&);
    juce::File exportMidi (const sms::EngineState&, int repeats, const juce::File& target, juce::String& error);
}
