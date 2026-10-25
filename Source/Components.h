// SeniorMy Structure — componentes de interface.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "core/Engine.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Botão giratório com rótulo e valor
class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& paramId, const juce::String& label);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class ParamToggle : public juce::ToggleButton
{
public:
    ParamToggle (APVTS&, const juce::String& paramId, const juce::String& text);
private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

class ParamCombo : public juce::Component
{
public:
    ParamCombo (APVTS&, const juce::String& paramId, const juce::String& label);
    void resized() override;
    juce::ComboBox box;
    juce::Label label;
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

// Painel com título (vidro fosco, borda preta, filete dourado)
class Panel : public juce::Component
{
public:
    explicit Panel (const juce::String& title = {}, bool dark = false) : title (title), dark (dark) {}
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (14).withTrimmedTop (title.isNotEmpty() ? 22 : 0); }
    juce::String title;
    bool dark;
};

// Acorde atual em letras grandes + tonalidade + confiança
class ChordDisplay : public juce::Component
{
public:
    void set (int root, int quality, int bass, float confidence, int keyTonic, int keyScale, bool locked);
    void paint (juce::Graphics&) override;
private:
    juce::String chord { "—" }, key;
    float conf = 0;
    bool locked = false;
};

// Cromagrama em roda (12 classes) com o acorde destacado
class ChromaWheel : public juce::Component
{
public:
    void set (const std::array<float, 12>& c, uint16_t chordMask, int root) { chroma = c; mask = chordMask; this->root = root; repaint(); }
    void paint (juce::Graphics&) override;
private:
    std::array<float, 12> chroma {};
    uint16_t mask = 0;
    int root = -1;
};

class LevelMeter : public juce::Component
{
public:
    void set (float v) { const float n = juce::jlimit (0.0f, 1.0f, v); if (std::abs (n - level) > 0.003f) { level = n; repaint(); } }
    void paint (juce::Graphics&) override;
    bool vertical = true;
private:
    float level = 0;
};

// Fader de seção (nível dB + medidor + nome)
class SectionStrip : public juce::Component
{
public:
    SectionStrip (APVTS&, const juce::String& paramId, int section);
    void resized() override;
    void paint (juce::Graphics&) override;
    LevelMeter meter;
    juce::Slider fader;
    int section;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// Editor de padrão (piano roll simples): linhas = vozes do acorde, colunas = passos
class LaneGrid : public juce::Component
{
public:
    std::function<void()> onChange;
    void setLane (sms::Lane* l) { lane = l; repaint(); }
    void setPlayStep (int s) { if (s != playStep) { playStep = s; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    bool cellAt (juce::Point<float>, int& row, int& step) const;
    juce::Rectangle<float> gridArea() const { return getLocalBounds().toFloat().reduced (2.0f).withTrimmedLeft (64.0f).withTrimmedTop (16.0f); }
    sms::Lane* lane = nullptr;
    int playStep = -1, dragRow = -1, dragStep = -1;
    bool painting = true;
    float dragStartY = 0; int dragStartVel = 0;
};

// Rolo das linhas geradas (o que o arranjador está tocando), por seção
class GeneratedRoll : public juce::Component
{
public:
    void push (const sms::DisplayEvent& e);
    void setNow (double ppq) { now = ppq; }
    void paint (juce::Graphics&) override;
    int laneFilter = -1;
private:
    struct NoteBar { double start, end; int note, section, artic, lane; float vel; bool open; };
    std::vector<NoteBar> bars;
    double now = 0;
};

// Progressão importada: blocos de acordes + cursor
class ProgressionView : public juce::Component
{
public:
    void set (std::shared_ptr<const sms::Progression> p) { prog = std::move (p); repaint(); }
    void setPosition (double ppq) { if (std::abs (ppq - pos) > 0.05) { pos = ppq; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    std::shared_ptr<const sms::Progression> prog;
    double pos = 0;
};

// Área de arrastar e soltar arquivos
class DropZone : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    std::function<void (const juce::File&)> onFile;
    juce::String hint;
    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { over = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { over = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    bool over = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Peça arrastável que leva o arquivo MIDI exportado para a DAW
class DragOutTile : public juce::Component
{
public:
    std::function<juce::File()> makeFile;
    juce::String text;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    bool dragging = false;
};
