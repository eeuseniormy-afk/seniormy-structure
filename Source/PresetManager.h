// SeniorMy Acoustic Guitars — presets (fábrica + usuário, JSON .smguitar) e padrões do usuário (.smpattern).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_core/juce_core.h>
#include "core/Presets.h"

namespace PresetIO
{
    juce::var patternToVar (const sms::Pattern&);
    bool patternFromVar (const juce::var&, sms::Pattern& out);
    juce::var stateToVar (const sms::EngineState&);
    bool stateFromVar (const juce::var&, sms::EngineState& out);
    juce::var presetToVar (const sms::PresetData&);
    bool presetFromVar (const juce::var&, sms::PresetData& out);
}

class PresetManager
{
public:
    struct Entry { juce::String name, category; bool factory = true; juce::File file; int factoryIndex = -1; };

    PresetManager();
    static juce::File userFolder();

    void refresh();
    const juce::Array<Entry>& entries() const { return list; }
    bool load (const Entry&, sms::PresetData& out) const;
    bool save (const sms::PresetData&, juce::String& error);
    bool remove (const Entry&);
    bool exists (const juce::String& name) const;
    int indexOf (const juce::String& name) const;

private:
    std::vector<sms::PresetData> factory;
    juce::Array<Entry> list;
};

namespace PatternLibrary
{
    juce::File userFolder();
    juce::Array<juce::File> userFiles();
    bool save (const sms::Pattern&, juce::String& error);
    bool load (const juce::File&, sms::Pattern& out);
}
