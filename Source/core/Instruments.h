// SeniorMy Acoustic Guitars — definição dos instrumentos: afinações (cordas/ordens, oitavas),
// origem do som (gravado, derivado, modelado), parâmetros de timbre e de modelagem física.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

namespace sms
{
enum class Source : int { Sampled = 0, Derived, Modeled };

enum InstId : int
{
    Steel = 0, Nylon, Flamenco, Twelve, Viola, Cavaquinho, Mandolin, Ukulele, Banjo, Sitar,
    NumInstruments
};

constexpr int kMaxCourses = 7;

// Uma ordem (course): uma corda, ou um par (uníssono ou oitava). Na ordem física de cima para
// baixo (a batida "para baixo" percorre do índice 0 ao último).
struct Course
{
    int note = 40;          // nota MIDI da corda principal (solta)
    int pair = -1;          // segunda corda: -1 = nenhuma; == note → uníssono; note+12 → oitava
    bool drone = false;     // 5ª corda do banjo: só solta
};

struct Tuning
{
    const char* name;
    std::vector<Course> courses;
};

// Modelagem física (guia de onda + corpo modal).
struct ModelParams
{
    float t60Low = 6.0f, t60High = 1.6f;   // decaimento (s) nos graves / agudos
    float brightness = 0.6f;               // filtro de perda (0 escuro … 1 brilhante)
    float pickPos = 0.14f;                 // posição do ataque (fração do comprimento)
    float pairDetuneCents = 4.0f;
    float pairGain = 0.8f;
    std::array<float, 6> bodyHz { 110, 220, 400, 800, 1600, 3000 };
    std::array<float, 6> bodyQ  { 8, 9, 9, 7, 5, 4 };
    std::array<float, 6> bodyGain { 0.5f, 0.45f, 0.35f, 0.3f, 0.25f, 0.2f };
    float bodyMix = 0.55f;
    float noise = 0.25f;                   // ruído de palheta/unha
};

struct InstrumentDef
{
    const char* id;
    const char* namePt;
    const char* nameEn;
    Source source;
    const char* sampleSet;          // conjunto de amostras ("" = modelado)
    std::vector<Tuning> tunings;
    int numFrets = 15;
    int maxSpan = 4;                // abertura máxima da mão (casas)
    float strumMs = 32.0f;          // tempo padrão de uma batida completa
    float decayScale = 1.0f;        // < 1 encurta a sustentação (derivados)
    float brightDb = 0.0f;          // realce de agudos fixo (derivados)
    float release = 0.18f;          // abafamento ao soltar (s)
    float gainDb = 0.0f;
    float twelveOctaveGain = 0.0f;  // derivado 12 cordas: nível da corda de oitava
    uint32_t colour = 0xffc9a227;   // destaque na interface
    const char* imageName = "";     // ilustração (BinaryData)
    const char* notePt = "";        // origem do som (texto da interface)
    ModelParams model {};
    bool reentrant = false;         // afinação reentrante (ukulele): baixo não é a corda de cima
    float bassWeight = 1.0f;        // exigência de fundamental no baixo (violões 1; agudos/reentrantes menos)
};

inline const char* sourceNamePt (Source s)
{
    switch (s) { case Source::Sampled: return "Gravado"; case Source::Derived: return "Derivado"; default: return "Modelado"; }
}

inline const std::vector<InstrumentDef>& instruments()
{
    static const std::vector<InstrumentDef> defs = [] {
        std::vector<InstrumentDef> v;
        const std::vector<Tuning> guitarTunings {
            { "Padrão (E A D G B E)", { {40}, {45}, {50}, {55}, {59}, {64} } },
            { "Drop D (D A D G B E)", { {38}, {45}, {50}, {55}, {59}, {64} } },
            { "DADGAD", { {38}, {45}, {50}, {55}, {57}, {62} } },
            { "Sol aberto (D G D G B D)", { {38}, {43}, {50}, {55}, {59}, {62} } },
            { "Meio tom abaixo (Eb)", { {39}, {44}, {49}, {54}, {58}, {63} } },
        };
        {
            InstrumentDef d { "steel", "Violão Aço", "Steel-String Guitar", Source::Sampled, "steel", guitarTunings };
            d.numFrets = 17; d.strumMs = 30; d.release = 0.2f; d.colour = 0xffd08a3a; d.imageName = "steel_png";
            d.notePt = "Gravado: Martin HD-28 (Jeff Learman, CC0)";
            v.push_back (d);
        }
        {
            InstrumentDef d { "nylon", "Violão Nylon", "Nylon Guitar", Source::Sampled, "nylon", guitarTunings };
            d.numFrets = 17; d.strumMs = 38; d.release = 0.22f; d.colour = 0xffc8643c; d.imageName = "nylon_png";
            d.notePt = "Gravado: violão espanhol (FreePats, CC0)";
            v.push_back (d);
        }
        {
            InstrumentDef d { "flamenco", "Violão Flamenco", "Flamenco Guitar", Source::Derived, "nylon", guitarTunings };
            d.numFrets = 17; d.strumMs = 18; d.decayScale = 0.55f; d.brightDb = 5.0f; d.release = 0.12f; d.gainDb = 1.0f;
            d.colour = 0xffe8d27a; d.imageName = "flamenco_png";
            d.notePt = "Derivado do violão de nylon: ataque mais seco e brilhante, sustentação curta";
            v.push_back (d);
        }
        {
            InstrumentDef d { "twelve", "Violão 12 Cordas", "12-String Guitar", Source::Derived, "steel",
                { { "Padrão 12 cordas", { {40, 52}, {45, 57}, {50, 62}, {55, 67}, {59, 59}, {64, 64} } },
                  { "Meio tom abaixo", { {39, 51}, {44, 56}, {49, 61}, {54, 66}, {58, 58}, {63, 63} } } } };
            d.numFrets = 15; d.strumMs = 40; d.twelveOctaveGain = 0.62f; d.release = 0.25f; d.gainDb = -2.0f;
            d.colour = 0xffb48cff; d.imageName = "twelve_png";
            d.notePt = "Derivado do violão de aço: ordens em oitava/uníssono com cordas reais mais agudas";
            v.push_back (d);
        }
        {
            InstrumentDef d { "viola", "Viola Caipira", "Viola Caipira", Source::Modeled, "",
                { { "Cebolão em Mi", { {47, 59}, {52, 64}, {56, 68}, {59, 59}, {64, 64} } },
                  { "Cebolão em Ré", { {45, 57}, {50, 62}, {54, 66}, {57, 57}, {62, 62} } },
                  { "Rio Abaixo (Sol)", { {43, 55}, {50, 62}, {55, 67}, {59, 59}, {62, 62} } },
                  { "Boiadeira (Mi)", { {47, 59}, {52, 64}, {56, 68}, {59, 59}, {62, 62} } } } };
            d.numFrets = 12; d.maxSpan = 4; d.strumMs = 28; d.release = 0.2f; d.gainDb = 0.0f;
            d.colour = 0xffe0a040; d.imageName = "viola_png";
            d.notePt = "Modelado: guia de onda com 5 ordens duplas (oitavas) e corpo modal";
            d.model.t60Low = 7.0f; d.model.t60High = 2.2f; d.model.brightness = 0.78f; d.model.pickPos = 0.11f;
            d.model.pairDetuneCents = 3.0f; d.model.pairGain = 0.85f;
            d.model.bodyHz = { 120, 235, 410, 760, 1450, 2900 }; d.model.bodyQ = { 9, 10, 9, 7, 5, 4 };
            d.model.bodyGain = { 0.5f, 0.5f, 0.38f, 0.3f, 0.28f, 0.22f }; d.model.bodyMix = 0.55f; d.model.noise = 0.32f;
            d.bassWeight = 0.2f;
            v.push_back (d);
        }
        {
            InstrumentDef d { "cavaquinho", "Cavaquinho", "Cavaquinho", Source::Modeled, "",
                { { "Natural (D G B D)", { {62}, {67}, {71}, {74} } },
                  { "Afinação de choro (D G B E)", { {62}, {67}, {71}, {76} } } } };
            d.numFrets = 14; d.maxSpan = 4; d.strumMs = 14; d.release = 0.1f; d.gainDb = -1.0f;
            d.colour = 0xff8a5a3a; d.imageName = "cavaquinho_png";
            d.notePt = "Modelado: guia de onda de cordas de aço e corpo pequeno";
            d.model.t60Low = 3.2f; d.model.t60High = 1.1f; d.model.brightness = 0.82f; d.model.pickPos = 0.12f;
            d.model.bodyHz = { 230, 420, 780, 1350, 2400, 4200 }; d.model.bodyQ = { 8, 9, 8, 6, 5, 4 };
            d.model.bodyGain = { 0.45f, 0.5f, 0.4f, 0.32f, 0.26f, 0.2f }; d.model.bodyMix = 0.6f; d.model.noise = 0.35f;
            d.bassWeight = 0.2f;
            v.push_back (d);
        }
        {
            InstrumentDef d { "mandolin", "Bandolim", "Mandolin", Source::Modeled, "",
                { { "Padrão (G D A E)", { {55, 55}, {62, 62}, {69, 69}, {76, 76} } } } };
            d.numFrets = 17; d.maxSpan = 5; d.strumMs = 16; d.release = 0.12f; d.gainDb = -1.5f;
            d.colour = 0xffc0303a; d.imageName = "mandolin_png";
            d.notePt = "Modelado: guia de onda com 4 ordens duplas em uníssono e tampo arqueado";
            d.model.t60Low = 3.6f; d.model.t60High = 1.0f; d.model.brightness = 0.8f; d.model.pickPos = 0.13f;
            d.model.pairDetuneCents = 3.5f; d.model.pairGain = 0.9f;
            d.model.bodyHz = { 280, 540, 1050, 1750, 2900, 4600 }; d.model.bodyQ = { 9, 9, 8, 6, 5, 4 };
            d.model.bodyGain = { 0.4f, 0.48f, 0.42f, 0.34f, 0.28f, 0.2f }; d.model.bodyMix = 0.62f; d.model.noise = 0.38f;
            d.bassWeight = 0.2f;
            v.push_back (d);
        }
        {
            InstrumentDef d { "ukulele", "Ukulele", "Ukulele", Source::Sampled, "ukulele",
                { { "Reentrante (G C E A)", { {67}, {60}, {64}, {69} } },
                  { "Sol grave (G C E A)", { {55}, {60}, {64}, {69} } } } };
            d.numFrets = 15; d.maxSpan = 4; d.strumMs = 20; d.release = 0.12f; d.reentrant = true;
            d.colour = 0xffe08a20; d.imageName = "ukulele_png";
            d.notePt = "Gravado: ukulele tenor (Mateusz Dąbrowski / FreePats, CC0)";
            d.bassWeight = 0.2f;
            v.push_back (d);
        }
        {
            InstrumentDef d { "banjo", "Banjo", "Banjo", Source::Sampled, "banjo",
                { { "Sol aberto (g D G B D)", { {67, -1, true}, {50}, {55}, {59}, {62} } },
                  { "Dó dupla (g C G C D)", { {67, -1, true}, {48}, {55}, {60}, {62} } } } };
            d.numFrets = 17; d.maxSpan = 4; d.strumMs = 18; d.release = 0.1f; d.gainDb = -1.0f;
            d.colour = 0xffe8e2d0; d.imageName = "banjo_png";
            d.notePt = "Gravado: banjo SX (itsclipping, CC0), tocado na afinação de 5 cordas";
            d.bassWeight = 0.2f;
            v.push_back (d);
        }
        {
            InstrumentDef d { "sitar", "Cítara Indiana (Sitar)", "Sitar", Source::Sampled, "sitar",
                { { "Sa-Pa (C G C)", { {48}, {55}, {60}, {67} } },
                  { "Sa-Ma (C F C)", { {48}, {53}, {60}, {65} } } } };
            d.numFrets = 19; d.maxSpan = 5; d.strumMs = 45; d.release = 0.4f; d.gainDb = -1.0f;
            d.colour = 0xff9a6a30; d.imageName = "sitar_png";
            d.notePt = "Gravado: sitar (Dr. Narayan B. Raikar, CC0)";
            d.bassWeight = 0.2f;
            v.push_back (d);
        }
        return v;
    }();
    return defs;
}

inline const InstrumentDef& instrument (int id) { return instruments()[(size_t) std::clamp (id, 0, (int) NumInstruments - 1)]; }
inline const Tuning& tuningOf (int inst, int tuning)
{
    const auto& d = instrument (inst);
    return d.tunings[(size_t) std::clamp (tuning, 0, (int) d.tunings.size() - 1)];
}
} // namespace sms
