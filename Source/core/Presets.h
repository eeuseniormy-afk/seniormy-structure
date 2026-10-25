// SeniorMy Structure — presets de fábrica (arranjos + ajustes).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Arranger.h"
#include <string>
#include <utility>
#include <vector>

namespace sms
{
struct PresetData
{
    std::string name, category;
    Arrangement arrangement;
    std::vector<std::pair<std::string, float>> params;   // id do parâmetro → valor real (não normalizado)
};

namespace presetutil
{
inline uint8_t mask (std::initializer_list<int> secs) { uint8_t m = 0; for (int s : secs) m |= (uint8_t) (1u << s); return m; }

inline Lane pad (const char* name, int artic, uint8_t m, float gain = 1.0f, int octave = 0)
{
    Lane l; l.name = name; l.type = LaneType::Pad; l.artic = artic; l.sectionMask = m; l.gain = gain; l.octave = octave;
    return l;
}
inline Lane line (const char* name, uint8_t m, float gain = 0.9f, int octave = 0)
{
    Lane l; l.name = name; l.type = LaneType::Line; l.artic = Legato; l.sectionMask = m; l.gain = gain; l.octave = octave;
    return l;
}
inline Lane tutti (const char* name, uint8_t m, float gain = 1.0f)
{
    Lane l; l.name = name; l.type = LaneType::Tutti; l.artic = Sustain; l.sectionMask = m; l.gain = gain;
    return l;
}
// padrão: lista de (linha, passo, velocity)
inline Lane pattern (const char* name, int artic, uint8_t m, int stepsPerBeat, int numSteps, float gate,
                     std::initializer_list<std::array<int, 3>> cells, float gain = 1.0f, int octave = 0)
{
    Lane l; l.name = name; l.type = LaneType::Pattern; l.artic = artic; l.sectionMask = m;
    l.stepsPerBeat = stepsPerBeat; l.numSteps = numSteps; l.gate = gate; l.gain = gain; l.octave = octave;
    l.clearGrid();
    for (auto& c : cells)
        if (c[0] >= 0 && c[0] < kRows && c[1] >= 0 && c[1] < kMaxSteps) l.grid[(size_t) c[0]][(size_t) c[1]] = (uint8_t) c[2];
    return l;
}
} // namespace presetutil

inline std::vector<PresetData> factoryPresets()
{
    using namespace presetutil;
    std::vector<PresetData> v;

    { // 1. Cinematic Pad
        PresetData p; p.name = "Cinematic Pad"; p.category = "Pads";
        p.arrangement.name = p.name;
        p.arrangement.lanes.push_back (pad ("Pad cordas", Sustain, mask ({ Vln1, Vln2, Vla, Vc, Cb }), 1.0f));
        p.arrangement.lanes.push_back (line ("Linha superior", mask ({ Vln1 }), 0.75f, 0));
        auto t = pad ("Trêmolo (alto)", Tremolo, mask ({ Vln1, Vln2 }), 0.45f, 0); t.enabled = false;
        p.arrangement.lanes.push_back (t);
        p.params = { { "density", 0.75f }, { "extensions", 2 }, { "spread", 0.85f }, { "register", 0 },
                     { "dynamics", 0.6f }, { "attack", 0.6f }, { "release", 0.7f }, { "reverbMix", 0.42f },
                     { "reverbSize", 0.85f }, { "humanize", 0.25f } };
        v.push_back (p);
    }
    { // 2. Ballad Strings
        PresetData p; p.name = "Ballad Strings"; p.category = "Pads";
        p.arrangement.name = p.name;
        p.arrangement.lanes.push_back (pad ("Cama (violas/cellos/baixos)", Sustain, mask ({ Vln2, Vla, Vc, Cb }), 0.9f));
        p.arrangement.lanes.push_back (line ("Melodia de violinos", mask ({ Vln1 }), 0.95f, 0));
        p.arrangement.lanes.push_back (pattern ("Pizz no tempo 1", Pizzicato, mask ({ Cb }), 1, 4, 0.9f, { { 0, 0, 90 } }, 0.6f));
        p.params = { { "density", 0.5f }, { "extensions", 1 }, { "spread", 0.55f }, { "register", 0 },
                     { "dynamics", 0.55f }, { "attack", 0.45f }, { "release", 0.6f }, { "reverbMix", 0.32f },
                     { "reverbSize", 0.65f }, { "humanize", 0.3f } };
        v.push_back (p);
    }
    { // 3. Pop Ostinato
        PresetData p; p.name = "Pop Ostinato"; p.category = "Rhythmic";
        p.arrangement.name = p.name;
        p.arrangement.lanes.push_back (pad ("Pad suave", Sustain, mask ({ Vla, Vc }), 0.55f));
        p.arrangement.lanes.push_back (pattern ("Ostinato violinos", Spiccato, mask ({ Vln1, Vln2 }), 4, 16, 0.45f,
            { { 3, 0, 110 }, { 1, 1, 70 }, { 2, 2, 85 }, { 1, 3, 70 }, { 3, 4, 105 }, { 1, 5, 70 }, { 2, 6, 85 }, { 1, 7, 70 },
              { 3, 8, 110 }, { 1, 9, 70 }, { 2, 10, 85 }, { 1, 11, 70 }, { 4, 12, 105 }, { 2, 13, 75 }, { 3, 14, 85 }, { 2, 15, 75 } }, 0.9f));
        p.arrangement.lanes.push_back (pattern ("Baixos em colcheias", Spiccato, mask ({ Vc, Cb }), 2, 8, 0.5f,
            { { 0, 0, 110 }, { 0, 1, 80 }, { 0, 2, 95 }, { 0, 3, 80 }, { 0, 4, 105 }, { 0, 5, 80 }, { 0, 6, 95 }, { 0, 7, 85 } }, 0.85f));
        p.params = { { "density", 0.85f }, { "extensions", 1 }, { "spread", 0.45f }, { "register", 0 },
                     { "dynamics", 0.7f }, { "attack", 0.25f }, { "release", 0.35f }, { "reverbMix", 0.25f },
                     { "reverbSize", 0.5f }, { "humanize", 0.15f } };
        v.push_back (p);
    }
    { // 4. Pizzicato Light
        PresetData p; p.name = "Pizzicato Light"; p.category = "Rhythmic";
        p.arrangement.name = p.name;
        p.arrangement.lanes.push_back (pattern ("Baixo pizz", Pizzicato, mask ({ Vc, Cb }), 2, 8, 0.9f,
            { { 0, 0, 105 }, { 0, 4, 95 }, { 1, 6, 70 } }, 1.0f));
        p.arrangement.lanes.push_back (pattern ("Acordes pizz", Pizzicato, mask ({ Vla, Vln2, Vln1 }), 2, 8, 0.9f,
            { { 2, 1, 80 }, { 3, 1, 75 }, { 2, 3, 70 }, { 4, 3, 70 }, { 3, 5, 80 }, { 4, 5, 75 }, { 2, 7, 70 }, { 3, 7, 65 } }, 0.9f));
        auto pd = pad ("Pad (opcional)", Sustain, mask ({ Vla, Vc }), 0.4f); pd.enabled = false;
        p.arrangement.lanes.push_back (pd);
        p.params = { { "density", 0.7f }, { "extensions", 1 }, { "spread", 0.5f }, { "register", 0 },
                     { "dynamics", 0.6f }, { "attack", 0.2f }, { "release", 0.3f }, { "reverbMix", 0.22f },
                     { "reverbSize", 0.45f }, { "humanize", 0.2f } };
        v.push_back (p);
    }
    { // 5. Full Orchestra
        PresetData p; p.name = "Full Orchestra"; p.category = "Tutti";
        p.arrangement.name = p.name;
        p.arrangement.lanes.push_back (tutti ("Tutti (cordas + trompas)", mask ({ Vln1, Vln2, Vla, Vc, Cb, Horns }), 1.0f));
        p.arrangement.lanes.push_back (pattern ("Motor graves", Spiccato, mask ({ Vc, Cb }), 2, 8, 0.5f,
            { { 0, 0, 115 }, { 0, 1, 85 }, { 0, 2, 95 }, { 0, 3, 85 }, { 0, 4, 110 }, { 0, 5, 85 }, { 0, 6, 95 }, { 0, 7, 90 } }, 0.8f));
        p.arrangement.lanes.push_back (line ("Linha heroica", mask ({ Vln1 }), 0.9f, 1));
        p.params = { { "density", 0.9f }, { "extensions", 1 }, { "spread", 0.9f }, { "register", 0 },
                     { "dynamics", 0.85f }, { "attack", 0.35f }, { "release", 0.6f }, { "reverbMix", 0.38f },
                     { "reverbSize", 0.9f }, { "humanize", 0.2f } };
        v.push_back (p);
    }
    { // 6. Tremolo Tension
        PresetData p; p.name = "Tremolo Tension"; p.category = "Pads";
        p.arrangement.name = p.name;
        p.arrangement.lanes.push_back (pad ("Trêmolo", Tremolo, mask ({ Vln1, Vln2, Vla, Vc }), 0.9f));
        p.arrangement.lanes.push_back (pad ("Baixos sustain", Sustain, mask ({ Cb }), 0.8f));
        p.params = { { "density", 0.6f }, { "extensions", 0 }, { "spread", 0.4f }, { "register", 0 },
                     { "dynamics", 0.5f }, { "attack", 0.5f }, { "release", 0.5f }, { "reverbMix", 0.35f },
                     { "reverbSize", 0.75f }, { "humanize", 0.1f } };
        v.push_back (p);
    }
    { // 7. Warm Cellos & Basses
        PresetData p; p.name = "Warm Low Strings"; p.category = "Pads";
        p.arrangement.name = p.name;
        p.arrangement.lanes.push_back (pad ("Cellos e baixos", Sustain, mask ({ Vla, Vc, Cb }), 1.0f, -1));
        p.arrangement.lanes.push_back (line ("Linha de cello", mask ({ Vc }), 0.8f, -1));
        p.params = { { "density", 0.45f }, { "extensions", 1 }, { "spread", 0.6f }, { "register", -5 },
                     { "dynamics", 0.55f }, { "attack", 0.5f }, { "release", 0.65f }, { "reverbMix", 0.3f },
                     { "reverbSize", 0.6f }, { "humanize", 0.25f } };
        v.push_back (p);
    }
    return v;
}
} // namespace sms
