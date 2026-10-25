// SeniorMy Structure — editor (interface completa, escalável).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "PluginProcessor.h"
#include "Components.h"
#include "Terms.h"

class CoverPage;
class ArrangePage;
class ImportPage;
class AboutPage;

class StructureEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseW = 1200, baseH = 800;

    explicit StructureEditor (StructureProcessor&);
    ~StructureEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    StructureProcessor& proc;
    SkyLookAndFeel laf;

    void refreshPresetBox();
    void arrangementChanged();      // páginas avisam que o arranjo mudou
    void showTermsReadOnly();

private:
    void timerCallback() override;
    void promptSaveAs();
    void takeSnapshots();

    class Root : public juce::Component
    {
    public:
        explicit Root (StructureEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        StructureEditor& ed;
        float t = 0;
    };

    Root root { *this };
    juce::Image logo;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn, saveAsBtn, deleteBtn, panicBtn;
    juce::Label statusLabel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<CoverPage> cover;
    std::unique_ptr<ArrangePage> arrange;
    std::unique_ptr<ImportPage> importPage;
    std::unique_ptr<AboutPage> about;

    // piano / rodapé
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<Knob> pianoLevel, bpm;
    std::unique_ptr<ParamToggle> pianoInput, pianoCover, coverOn;
    LevelMeter outMeter;

    std::unique_ptr<TermsOverlay> terms;
    bool presetBoxUpdating = false;
    int snapshotCountdown = -1;
    juce::File snapshotDir;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    friend class Root;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StructureEditor)
};
