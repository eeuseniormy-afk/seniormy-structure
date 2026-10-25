// SeniorMy Structure — presets: de fábrica (somente leitura) e do usuário (JSON .smstructure).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_core/juce_core.h>
#include "core/Presets.h"

namespace PresetIO
{
    juce::var arrangementToVar (const sms::Arrangement&);
    sms::Arrangement arrangementFromVar (const juce::var&);
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
    // salva (novo ou substitui um preset do usuário com o mesmo nome)
    bool save (const sms::PresetData&, juce::String& error);
    bool remove (const Entry&);
    bool exists (const juce::String& name) const;
    int indexOf (const juce::String& name) const;

private:
    std::vector<sms::PresetData> factory;
    juce::Array<Entry> list;
};
