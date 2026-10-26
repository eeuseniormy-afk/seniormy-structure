// SeniorMy Acoustic Guitars — componentes da interface: ilustração do instrumento, seletor de
// instrumentos, diagrama do acorde no braço, editor de padrões (grade), faixa de progressão e
// controles rotativos com rótulo.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "core/Engine.h"
#include "LookAndFeel.h"

juce::Image instrumentImage (int inst);

//==============================================================================
class InstrumentFace : public juce::Component
{
public:
    void setInstrument (int inst, bool hasSamples);
    void setPartLabel (const juce::String& s) { partLabel = s; repaint(); }
    void paint (juce::Graphics&) override;
private:
    int instrument = 0; bool samples = true;
    juce::String partLabel;
};

//==============================================================================
class InstrumentPicker : public juce::Component
{
public:
    std::function<void (int)> onPick;
    void setSelected (int i) { selected = i; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    juce::Rectangle<float> cell (int i) const;
    int cellIndexAt (juce::Point<float>) const;
    int selected = 0, hover = -1;
};

//==============================================================================
class ChordDiagram : public juce::Component
{
public:
    void set (int inst, int tuning, int capo, const std::array<int, sms::kMaxCourses>& frets, const juce::String& chordName);
    void paint (juce::Graphics&) override;
private:
    int instrument = 0, tuning = 0, capo = 0;
    std::array<int, sms::kMaxCourses> frets {};
    juce::String name;
};

//==============================================================================
class PatternGrid : public juce::Component
{
public:
    std::function<void()> onEdit;
    void setPattern (const sms::Pattern& p) { pattern = p; repaint(); }
    const sms::Pattern& getPattern() const { return pattern; }
    void setPlayStep (int s) { if (s != playStep) { playStep = s; repaint(); } }
    void setStringCount (int n) { if (n != strings) { strings = n; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    bool cellAt (juce::Point<float>, int& row, int& step) const;
    juce::Rectangle<float> gridArea() const;
    bool rowVisible (int r) const { return r < sms::RS1 || (r - sms::RS1) < strings; }
    int visibleRows() const;
    int rowAtIndex (int idx) const;
    int indexOfRow (int r) const;
    sms::Pattern pattern;
    int playStep = -1, strings = 6;
    uint8_t paintValue = 96;
};

//==============================================================================
class ProgressionStrip : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    void setData (const sms::HarmonyState& h, int selectedSlot, int playing);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    sms::HarmonyState harmony {};
    int selected = -1, playing = -1;
};

//==============================================================================
struct LabeledKnob : public juce::Component
{
    LabeledKnob (const juce::String& name, double lo, double hi, double step, double def, const juce::String& suffix = {});
    void resized() override;
    juce::Slider slider;
    juce::Label label;
};
