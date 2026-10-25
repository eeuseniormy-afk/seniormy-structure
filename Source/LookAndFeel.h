// SeniorMy Structure — aparência: céu (gradientes, nuvens e estrelas sutis), ouro acetinado,
// letras e bordas em preto fosco. Acabamento fosco, sem brilho.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace Sky
{
    // céu
    const juce::Colour zenith   { 0xff16223b };   // azul-noite no alto
    const juce::Colour midSky   { 0xff2d4670 };
    const juce::Colour lowSky   { 0xff6f8db5 };
    const juce::Colour horizon  { 0xffb9cade };   // névoa clara
    // painéis (vidro fosco claro sobre o céu)
    const juce::Colour panel    { 0xd9e9eef4 };
    const juce::Colour panelAlt { 0xcfdfe6ef };
    const juce::Colour panelDark{ 0xe61b2233 };
    // ouro acetinado
    const juce::Colour gold      { 0xffbf9f5f };
    const juce::Colour goldLight { 0xffd8c08a };
    const juce::Colour goldDark  { 0xff8c7240 };
    // preto fosco
    const juce::Colour ink       { 0xff1b1b1e };
    const juce::Colour inkSoft   { 0xff3a3b40 };
    const juce::Colour inkFaint  { 0x661b1b1e };
    const juce::Colour white     { 0xfff4f1ea };

    // cores das seções (acentos discretos)
    juce::Colour sectionColour (int section);
    juce::Colour articColour (int artic);
}

class SkyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkyLookAndFeel();

    static juce::Font uiFont (float h, bool bold = false);
    static juce::Font titleFont (float h);

    // fundo de céu com nuvens e estrelas (determinístico)
    static void paintSky (juce::Graphics&, juce::Rectangle<float> area, float time = 0.0f);
    // painel fosco com borda preta fina e filete dourado
    static void paintPanel (juce::Graphics&, juce::Rectangle<float> r, bool dark = false, float radius = 10.0f);

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
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
