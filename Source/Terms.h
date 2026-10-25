// SeniorMy Structure — Termos de Uso: texto (pt-BR e en), registro do aceite e tela de aceite.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace Terms
{
    inline constexpr const char* version = "1.0";

    // 0 = pt-BR, 1 = en
    juce::StringArray languageNames();
    int defaultLanguage();
    juce::String text (int language);

    bool isAccepted();       // aceite desta versão registrado neste computador
    void recordAcceptance(); // grava versão + data/hora (ISO 8601) no computador do usuário
}

// Camada que cobre o editor até o usuário aceitar os Termos (bloqueia todo o uso).
class TermsOverlay : public juce::Component
{
public:
    explicit TermsOverlay (std::function<void()> onAccepted, bool readOnly = false);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setLanguage (int lang);
    void refreshButtons();

    std::function<void()> onAccepted;
    bool readOnly;
    juce::Label title, subtitle;
    juce::ComboBox language;
    juce::TextEditor body;
    juce::ToggleButton agree;
    juce::TextButton accept, close;
};
