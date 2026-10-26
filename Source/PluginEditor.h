// SeniorMy Acoustic Guitars — interface: cabeçalho (presets, BPM, tocar), ilustração do
// instrumento ativo, abas Instrumento / Harmonia / Padrão / Mixer, acorde atual e teclado.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "Components.h"
#include "Terms.h"

class GuitarsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GuitarsEditor (GuitarsProcessor&);
    ~GuitarsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1240, baseH = 800;

private:
    struct Root : public juce::Component
    {
        explicit Root (GuitarsEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        GuitarsEditor& ed;
    };
    struct Page : public juce::Component
    {
        void paint (juce::Graphics& g) override { WoodLookAndFeel::paintPanel (g, getLocalBounds().toFloat(), 10.0f); }
        void resized() override { if (onResized) onResized(); }
        std::function<void()> onResized;
    };
    struct MixStrip : public juce::Component
    {
        MixStrip();
        void resized() override;
        void paint (juce::Graphics&) override;
        juce::String title; juce::Colour accent;
        juce::Slider level;
        LabeledKnob pan { juce::String::fromUTF8 ("Pan"), -1, 1, 0.01, 0 }, tone { juce::String::fromUTF8 ("Timbre"), -1, 1, 0.01, 0 },
                    body { juce::String::fromUTF8 ("Corpo"), 0, 1, 0.01, 0.3 }, width { juce::String::fromUTF8 ("Largura"), 0, 1, 0.01, 0.7 },
                    reverb { juce::String::fromUTF8 ("Reverb"), 0, 1, 0.01, 0.25 }, noises { juce::String::fromUTF8 ("Ruídos"), 0, 1, 0.01, 0.35 };
        juce::ToggleButton mute { "Mudo" }, solo { "Solo" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void timerCallback() override;
    void layoutPages();
    void reloadFromProcessor();
    void refreshAll();
    void refreshPresetBox();
    void push();                          // envia o estado local ao processador
    sms::PartState& part() { return st.parts[(size_t) proc.uiPart]; }
    void selectPart (int p);
    void refreshPatternBox();
    void loadPatternChoice (int id);
    void assignDegree (int degree);
    void showAbout();
    void showTermsReadOnly();
    void takeSnapshots();
    juce::String chordName (const sms::Chord&) const;

    GuitarsProcessor& proc;
    WoodLookAndFeel lnf;
    Root root { *this };
    sms::EngineState st {};
    int seenVersion = -1;
    juce::Image logo;

    // cabeçalho
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, savePreset { "Salvar" }, deletePreset { "Excluir" },
                     termsButton { "Termos" }, aboutButton { "Sobre" }, playButton { "Tocar" }, stopButton { "Parar" };
    juce::Slider bpm;
    juce::Label bpmLabel, statusLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAtt;

    // partes + ilustração
    std::array<juce::TextButton, sms::kParts> partButtons;
    InstrumentFace face;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    Page pageInst, pageHarm, pagePat, pageMix;

    // Instrumento
    InstrumentPicker picker;
    ChordDiagram diagram;
    juce::ComboBox modeBox, tuningBox, capoBox, positionBox, octaveBox;
    juce::Label modeL, tuningL, capoL, positionL, octaveL, descLabel;

    // Harmonia
    juce::ComboBox keyBox, scaleBox, splitBox, beatsBox;
    juce::Label keyL, scaleL, splitL, beatsL, progHint, importStatus;
    juce::ToggleButton tetrads { juce::String::fromUTF8 ("Tétrades (7ª)") }, fieldMode { juce::String::fromUTF8 ("Campo harmônico nas teclas brancas") },
                       latch { juce::String::fromUTF8 ("Segurar acorde") }, progOn { juce::String::fromUTF8 ("Progressão ligada") };
    std::array<juce::TextButton, 7> degreeButtons;
    ProgressionStrip progStrip;
    int selectedSlot = 0;
    juce::TextButton removeSlot { "Remover" }, clearProg { "Limpar" }, importButton { juce::String::fromUTF8 ("Importar loop…") },
                     exportButton { juce::String::fromUTF8 ("Exportar MIDI…") };

    // Padrão
    juce::ComboBox patternBox, spbBox, stepsBox;
    juce::Label patternL, spbL, stepsL;
    juce::TextButton savePattern { juce::String::fromUTF8 ("Salvar padrão") }, clearPattern { "Limpar grade" };
    LabeledKnob swing { "Swing", 0, 0.6, 0.01, 0 }, strum { "Velocidade da batida", 0.1, 3.0, 0.01, 1.0, "x" },
                humanize { "Humanizar", 0, 1, 0.01, 0.35 }, palm { "Abafar (palma)", 0, 1, 0.01, 0 }, accent { juce::String::fromUTF8 ("Acentuação"), 0, 1, 0.01, 0.5 };
    PatternGrid grid;
    juce::Array<juce::File> userPatterns;

    // Mixer
    std::array<MixStrip, sms::kParts> strips;
    juce::Slider masterVol, masterRev;
    LabeledKnob room { "Sala", 0, 1, 0.01, 0.45 }, masterWidth { juce::String::fromUTF8 ("Largura geral"), 0, 1.5, 0.01, 1.0 };
    juce::Label masterL, masterVolL, masterRevL;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAtt, revAtt;

    // rodapé
    juce::MidiKeyboardComponent keyboard { proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::Label chordLabel, keyInfo;
    float meterValue = 0;

    std::unique_ptr<TermsOverlay> terms;
    std::unique_ptr<juce::Component> about;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File snapshotDir;
    int snapshotCountdown = -1;
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarsEditor)
};
