// SeniorMy Acoustic Guitars — presets de fábrica: instrumento(s), afinação, padrão, tonalidade,
// progressão, andamento e mixagem.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Engine.h"
#include <string>
#include <vector>
#include <cstring>

namespace sms
{
struct PresetData
{
    std::string name, category, description;
    EngineState state {};
};

inline const Pattern& patternByName (const char* name)
{
    static const std::vector<Pattern> pats = factoryPatterns();
    for (auto& p : pats) if (std::strcmp (p.name, name) == 0) return p;
    return pats[0];
}

inline std::vector<PresetData> factoryPresets()
{
    std::vector<PresetData> v;
    // progressão como texto: "G:4 D:4 Em:4 C:4" (sufixos: m, 7, maj7, m7, sus4, dim, 6)
    auto prog = [] (HarmonyState& h, const char* text)
    {
        h.numProg = 0;
        const char* s = text;
        while (*s && h.numProg < kMaxProg)
        {
            while (*s == ' ') ++s;
            if (! *s) break;
            static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A..G
            int root = base[(s[0] - 'A') % 7]; ++s;
            if (*s == '#') { ++root; ++s; } else if (*s == 'b') { --root; ++s; }
            char suf[8] = {}; int k = 0;
            while (*s && *s != ':' && *s != ' ' && k < 7) suf[k++] = *s++;
            int beats = 4;
            if (*s == ':') { ++s; beats = 0; while (*s >= '0' && *s <= '9') beats = beats * 10 + (*s++ - '0'); }
            Quality q = Quality::Major;
            const std::string sf (suf);
            if (sf == "m") q = Quality::Minor; else if (sf == "7") q = Quality::Dom7; else if (sf == "maj7") q = Quality::Maj7;
            else if (sf == "m7") q = Quality::Min7; else if (sf == "sus4") q = Quality::Sus4; else if (sf == "dim") q = Quality::Dim;
            else if (sf == "6") q = Quality::Maj6; else if (sf == "m6") q = Quality::Min6; else if (sf == "m7b5") q = Quality::HalfDim7;
            auto& st = h.prog[(size_t) h.numProg++];
            st.root = pc (root); st.quality = (int) q; st.bass = -1; st.beats = std::max (1, beats);
        }
    };
    auto mk = [&] (const char* name, const char* cat, const char* desc, int tonic, Scale sc, float bpm, const char* progression) -> PresetData&
    {
        v.emplace_back(); auto& p = v.back();
        p.name = name; p.category = cat; p.description = desc;
        p.state.harmony.keyTonic = tonic; p.state.harmony.keyScale = (int) sc;
        p.state.master.bpm = bpm;
        prog (p.state.harmony, progression);
        for (auto& ps : p.state.parts) ps.mode = ModeOff;
        return p;
    };
    auto part = [] (PresetData& p, int idx, int inst, const char* pattern, int tuning = 0, float levelDb = 0, float pan = 0)
    {
        auto& ps = p.state.parts[(size_t) idx];
        ps.instrument = inst; ps.tuning = tuning; ps.mode = ModePattern; ps.pattern = patternByName (pattern);
        ps.levelDb = levelDb; ps.pan = pan;
        return &ps;
    };

    { auto& p = mk ("Violão Folk – Batida", "Violão", "Violão de aço com a batida pop/folk clássica.", 7, Scale::Major, 96, "G:4 D:4 Em:4 C:4");
      part (p, 0, Steel, "Folk pop (↓ ↓↑ ↑↓↑)"); }
    { auto& p = mk ("Violão Folk – Travis Picking", "Violão", "Dedilhado com polegar alternado.", 0, Scale::Major, 88, "C:4 Am:4 F:4 G:4");
      part (p, 0, Steel, "Travis picking"); }
    { auto& p = mk ("Bossa Nova – Nylon", "Brasil", "Violão de nylon com a levada de bossa nova e tétrades.", 0, Scale::Major, 132, "Dm7:8 G7:8 Cmaj7:8 A7:8");
      p.state.harmony.tetrads = true; auto* ps = part (p, 0, Nylon, "Bossa nova"); ps->tone = -0.15f; ps->reverb = 0.3f; }
    { auto& p = mk ("Balada – Nylon Dedilhado", "Violão", "Arpejo p-i-m-a no violão de nylon.", 9, Scale::NaturalMinor, 76, "Am:4 F:4 C:4 G:4");
      part (p, 0, Nylon, "Balada dedilhada (p-i-m-a)"); }
    { auto& p = mk ("Flamenco – Rumba", "Flamenco", "Rumba flamenca com golpes no tampo (cadência andaluza).", 9, Scale::Phrygian, 108, "Am:4 G:4 F:4 E:4");
      part (p, 0, Flamenco, "Rumba flamenca"); }
    { auto& p = mk ("Flamenco – Rasgueado", "Flamenco", "Rasgueado em leque sobre o modo frígio de Mi.", 4, Scale::Phrygian, 100, "E:4 F:4 G:4 F:4");
      part (p, 0, Flamenco, "Rasgueado (leque)"); }
    { auto& p = mk ("Doze Cordas – Folk Rock", "Violão", "Violão de 12 cordas com batida aberta.", 2, Scale::Major, 104, "D:4 A:4 Bm:4 G:4");
      auto* ps = part (p, 0, Twelve, "Folk pop (↓ ↓↑ ↑↓↑)"); ps->width = 0.9f; }
    { auto& p = mk ("Viola Caipira – Ponteado", "Viola", "Viola caipira no cebolão em Mi, ponteado.", 4, Scale::Major, 92, "E:8 B7:8 E:8 A:8");
      part (p, 0, Viola, "Ponteado caipira"); }
    { auto& p = mk ("Viola Caipira – Pagode de Viola", "Viola", "Rasqueado sincopado no cebolão em Ré.", 2, Scale::Major, 112, "D:4 A7:4 D:4 G:4");
      part (p, 0, Viola, "Pagode de viola", 1); }
    { auto& p = mk ("Viola Caipira – Toada (3/4)", "Viola", "Toada/guarânia em compasso ternário.", 4, Scale::Major, 84, "E:6 A:6 B7:6 E:6");
      part (p, 0, Viola, "Toada / guarânia (3/4)"); }
    { auto& p = mk ("Sertanejo – Viola e Violão", "Viola", "Viola ponteando com violão de aço na base.", 4, Scale::Major, 92, "E:8 A:8 B7:8 E:8");
      part (p, 0, Viola, "Ponteado caipira", 0, 0, -0.3f);
      auto* ps = part (p, 1, Steel, "Folk pop (↓ ↓↑ ↑↓↑)", 0, -5, 0.35f); ps->capo = 0; }
    { auto& p = mk ("Cavaquinho – Samba/Pagode", "Brasil", "Cavaquinho na levada de samba (centro).", 2, Scale::Major, 96, "D:4 Bm:4 Em:4 A7:4");
      part (p, 0, Cavaquinho, "Samba / pagode (cavaco)"); }
    { auto& p = mk ("Roda de Choro", "Brasil", "Cavaquinho de choro (D G B E) com violão de nylon.", 7, Scale::Major, 100, "G:4 E7:4 Am:4 D7:4");
      part (p, 0, Cavaquinho, "Choro (cavaco centro)", 1, 0, 0.25f);
      part (p, 1, Nylon, "Balada dedilhada (p-i-m-a)", 0, -4, -0.3f); }
    { auto& p = mk ("Baião Nordestino", "Brasil", "Violão de aço no baião, modo mixolídio.", 7, Scale::Mixolydian, 104, "G:4 F:4 G:4 D:4");
      part (p, 0, Steel, "Baião"); }
    { auto& p = mk ("Ukulele – Island", "Ukulele", "Ukulele tenor com a batida havaiana e chuck.", 0, Scale::Major, 100, "C:4 G:4 Am:4 F:4");
      part (p, 0, Ukulele, "Island strum (ukulele)"); }
    { auto& p = mk ("Banjo – Bluegrass Roll", "Banjo", "Roll para frente no banjo em Sol aberto.", 7, Scale::Major, 120, "G:8 C:8 G:8 D:8");
      part (p, 0, Banjo, "Roll para frente (banjo)"); }
    { auto& p = mk ("Banjo – Polegar Alternado", "Banjo", "Roll de polegar alternado, afinação de Dó dupla.", 0, Scale::Major, 110, "C:8 F:8 C:8 G:8");
      part (p, 0, Banjo, "Roll de polegar alternado", 1); }
    { auto& p = mk ("Bandolim – Trêmolo", "Bandolim", "Bandolim em trêmolo com violão de nylon de base.", 2, Scale::Major, 84, "D:4 G:4 A:4 D:4");
      part (p, 0, Mandolin, "Trêmolo (bandolim)", 0, 0, 0.2f);
      part (p, 1, Nylon, "Balada dedilhada (p-i-m-a)", 0, -4, -0.25f); }
    { auto& p = mk ("Bluegrass Trio", "Banjo", "Banjo, bandolim (chop) e violão de aço juntos.", 7, Scale::Major, 124, "G:8 C:8 G:8 D:8");
      part (p, 0, Banjo, "Roll para frente (banjo)", 0, -1, 0.3f);
      part (p, 1, Mandolin, "Chop bluegrass", 0, -4, -0.35f);
      part (p, 2, Steel, "Folk pop (↓ ↓↑ ↑↓↑)", 0, -6, 0.0f); }
    { auto& p = mk ("Sitar – Raga", "Sitar", "Arpejo lento de sitar sobre bordão.", 0, Scale::Mixolydian, 70, "C:16 Bb:8 C:8");
      auto* ps = part (p, 0, Sitar, "Raga lento (sitar)"); ps->reverb = 0.45f; }
    { auto& p = mk ("Reggae – Violão", "Violão", "Contratempo abafado no violão de aço.", 9, Scale::Dorian, 80, "Am:8 D:8");
      part (p, 0, Steel, "Reggae (contratempo)"); }
    { auto& p = mk ("Valsa Seresteira", "Violão", "Valsa em 3/4 no violão de nylon.", 2, Scale::HarmonicMinor, 120, "Dm:6 A7:6 Dm:6 D7:6 Gm:6 Dm:6 A7:6 Dm:6");
      part (p, 0, Nylon, "Valsa (3/4)"); }
    { auto& p = mk ("Teclado – Violão de Nylon", "Teclado", "Toque nota a nota no violão de nylon (sem padrão).", 0, Scale::Major, 96, "C:4");
      auto& ps = p.state.parts[0]; ps.instrument = Nylon; ps.mode = ModeKeys; ps.pattern = patternByName ("Batida única por compasso"); p.state.harmony.numProg = 0; }
    { auto& p = mk ("Acordes – Batida Única (Aço)", "Teclado", "Cada acorde tocado na mão esquerda vira uma batida.", 0, Scale::Major, 96, "C:4");
      auto& ps = p.state.parts[0]; ps.instrument = Steel; ps.mode = ModeStrumOnce; ps.pattern = patternByName ("Batida única por compasso");
      auto& k = p.state.parts[1]; k.instrument = Steel; k.mode = ModeKeys; k.levelDb = -2; p.state.harmony.numProg = 0; }
    return v;
}
} // namespace sms
