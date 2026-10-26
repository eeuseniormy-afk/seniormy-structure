// SeniorMy Acoustic Guitars — aparência: fundo amadeirado (nogueira), painéis em preto fosco,
// filetes em ouro acetinado e acentos violeta (identidade do ícone).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace Wood
{
    const juce::Colour black     { 0xff111113 };   // preto fosco
    const juce::Colour panel     { 0xe0141417 };   // painel preto translúcido sobre a madeira
    const juce::Colour panelSoft { 0xc81c1c21 };
    const juce::Colour well      { 0xff0b0b0d };   // poços de controles
    const juce::Colour gold      { 0xffc8a45e };   // ouro acetinado
    const juce::Colour goldLight { 0xffe2c88f };
    const juce::Colour goldDark  { 0xff8d7038 };
    const juce::Colour violet    { 0xff9a6bff };
    const juce::Colour violetDim { 0xff5b3d9e };
    const juce::Colour ivory     { 0xfff2eadb };
    const juce::Colour ivorySoft { 0xffbdb4a5 };
    const juce::Colour faint     { 0x66f2eadb };
    const juce::Colour wood      { 0xff4a3020 };
}

class WoodLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WoodLookAndFeel();

    static juce::Font uiFont (float h, bool bold = false);
    static juce::Font titleFont (float h);

    static void paintWood (juce::Graphics&, juce::Rectangle<float> area);
    static void paintPanel (juce::Graphics&, juce::Rectangle<float> r, float radius = 10.0f, bool soft = false);

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
    int getTabButtonBestWidth (juce::TabBarButton&, int depth) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
};
