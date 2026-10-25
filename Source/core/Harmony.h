// SeniorMy Structure — teoria musical: classes de altura, escalas/modos, acordes, tonalidade.
// Núcleo puro C++17 (sem JUCE), testado em tests/core_test.cpp.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace sms
{
using Chroma = std::array<float, 12>;

inline int pc (int midiNote) { return ((midiNote % 12) + 12) % 12; }

inline const char* pcName (int p, bool flats = false)
{
    static const char* sharp[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flat[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return flats ? flat[pc (p)] : sharp[pc (p)];
}

//==============================================================================
// Escalas e modos
enum class Scale : int
{
    Major = 0, NaturalMinor, HarmonicMinor, MelodicMinor,
    Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    count
};

inline const char* scaleName (Scale s, bool pt = true)
{
    static const char* ptN[] = { "Maior", "Menor natural", "Menor harmônica", "Menor melódica",
                                 "Dórico", "Frígio", "Lídio", "Mixolídio", "Lócrio" };
    static const char* enN[] = { "Major", "Natural minor", "Harmonic minor", "Melodic minor",
                                 "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian" };
    const int i = std::clamp ((int) s, 0, (int) Scale::count - 1);
    return pt ? ptN[i] : enN[i];
}

inline std::array<int, 7> scaleSteps (Scale s)
{
    switch (s)
    {
        case Scale::Major:         return { 0, 2, 4, 5, 7, 9, 11 };
        case Scale::NaturalMinor:  return { 0, 2, 3, 5, 7, 8, 10 };
        case Scale::HarmonicMinor: return { 0, 2, 3, 5, 7, 8, 11 };
        case Scale::MelodicMinor:  return { 0, 2, 3, 5, 7, 9, 11 };
        case Scale::Dorian:        return { 0, 2, 3, 5, 7, 9, 10 };
        case Scale::Phrygian:      return { 0, 1, 3, 5, 7, 8, 10 };
        case Scale::Lydian:        return { 0, 2, 4, 6, 7, 9, 11 };
        case Scale::Mixolydian:    return { 0, 2, 4, 5, 7, 9, 10 };
        case Scale::Locrian:       return { 0, 1, 3, 5, 6, 8, 10 };
        default:                   return { 0, 2, 4, 5, 7, 9, 11 };
    }
}

inline bool scaleIsMinorish (Scale s)
{
    return s == Scale::NaturalMinor || s == Scale::HarmonicMinor || s == Scale::MelodicMinor
        || s == Scale::Dorian || s == Scale::Phrygian || s == Scale::Locrian;
}

struct Key
{
    int tonic = 0;              // 0..11
    Scale scale = Scale::Major;

    uint16_t mask() const
    {
        uint16_t m = 0;
        for (int st : scaleSteps (scale)) m |= (uint16_t) (1u << pc (tonic + st));
        return m;
    }
    bool contains (int p) const { return (mask() >> pc (p)) & 1u; }
    bool operator== (const Key& o) const { return tonic == o.tonic && scale == o.scale; }
    bool operator!= (const Key& o) const { return ! (*this == o); }

    std::string name (bool pt = true) const
    {
        return std::string (pcName (tonic, usesFlats())) + " " + scaleName (scale, pt);
    }
    bool usesFlats() const
    {
        // armadura com bemóis? usa a relativa maior
        static const bool flatMajor[12] = { false, true, false, true, false, true, false, false, true, false, true, false };
        int relMajor = tonic;
        switch (scale)
        {
            case Scale::NaturalMinor: case Scale::HarmonicMinor: case Scale::MelodicMinor: relMajor = tonic + 3; break;
            case Scale::Dorian: relMajor = tonic - 2; break;
            case Scale::Phrygian: relMajor = tonic - 4; break;
            case Scale::Lydian: relMajor = tonic - 5; break;
            case Scale::Mixolydian: relMajor = tonic - 7; break;
            case Scale::Locrian: relMajor = tonic + 1; break;
            default: break;
        }
        return flatMajor[pc (relMajor)] || pc (relMajor) == 5;
    }
};

//==============================================================================
// Acordes
enum class Quality : int
{
    Major = 0, Minor, Dim, Aug, Sus2, Sus4, Dom7, Maj7, Min7, HalfDim7, Dim7, MinMaj7, Maj6, Min6,
    count
};

struct QualityInfo { const char* suffix; std::array<int, 4> tones; int numTones; };

inline const QualityInfo& qualityInfo (Quality q)
{
    static const QualityInfo info[] = {
        { "",      { 0, 4, 7, -1 },  3 },
        { "m",     { 0, 3, 7, -1 },  3 },
        { "dim",   { 0, 3, 6, -1 },  3 },
        { "aug",   { 0, 4, 8, -1 },  3 },
        { "sus2",  { 0, 2, 7, -1 },  3 },
        { "sus4",  { 0, 5, 7, -1 },  3 },
        { "7",     { 0, 4, 7, 10 },  4 },
        { "maj7",  { 0, 4, 7, 11 },  4 },
        { "m7",    { 0, 3, 7, 10 },  4 },
        { "m7b5",  { 0, 3, 6, 10 },  4 },
        { "dim7",  { 0, 3, 6, 9 },   4 },
        { "m(maj7)", { 0, 3, 7, 11 }, 4 },
        { "6",     { 0, 4, 7, 9 },   4 },
        { "m6",    { 0, 3, 7, 9 },   4 },
    };
    return info[std::clamp ((int) q, 0, (int) Quality::count - 1)];
}

struct Chord
{
    int root = -1;               // -1 = sem acorde (N.C.)
    Quality quality = Quality::Major;
    int bass = -1;               // classe do baixo (-1 = fundamental)
    float confidence = 0.0f;

    bool valid() const { return root >= 0; }
    bool operator== (const Chord& o) const { return root == o.root && quality == o.quality && bass == o.bass; }
    bool operator!= (const Chord& o) const { return ! (*this == o); }

    uint16_t mask() const
    {
        if (! valid()) return 0;
        const auto& qi = qualityInfo (quality);
        uint16_t m = 0;
        for (int i = 0; i < qi.numTones; ++i) m |= (uint16_t) (1u << pc (root + qi.tones[(size_t) i]));
        return m;
    }
    int third() const
    {
        const auto& qi = qualityInfo (quality);
        return pc (root + qi.tones[1]);
    }
    int fifth() const { return pc (root + qualityInfo (quality).tones[2]); }
    bool isMinorThird() const { return pc (third() - root) == 3; }
    int bassPc() const { return bass >= 0 ? bass : root; }

    std::string name (bool flats = false) const
    {
        if (! valid()) return "N.C.";
        std::string s = std::string (pcName (root, flats)) + qualityInfo (quality).suffix;
        if (bass >= 0 && bass != root) s += std::string ("/") + pcName (bass, flats);
        return s;
    }
};

//==============================================================================
// Extensões (tons de cor) permitidas pela escala.
// level: 0 = tríade, 1 = + sétima, 2 = + nona, 3 = + 11/13 (cores)
inline std::vector<int> chordPitchClasses (const Chord& c, const Key& key, int extensionLevel)
{
    std::vector<int> out;
    if (! c.valid()) return out;
    const auto& qi = qualityInfo (c.quality);
    const int baseCount = (extensionLevel >= 1) ? qi.numTones : std::min (3, qi.numTones);
    for (int i = 0; i < baseCount; ++i) out.push_back (pc (c.root + qi.tones[(size_t) i]));

    auto addIfScale = [&] (int interval)
    {
        const int p = pc (c.root + interval);
        if (std::find (out.begin(), out.end(), p) != out.end()) return;
        if (key.contains (p)) out.push_back (p);
    };
    if (extensionLevel >= 1 && qi.numTones == 3 && c.quality != Quality::Sus2 && c.quality != Quality::Sus4)
    {
        // tríade: adiciona a sétima diatônica
        if (key.contains (c.root + 11) && c.quality == Quality::Major) addIfScale (11);
        else if (key.contains (c.root + 10)) addIfScale (10);
        else if (key.contains (c.root + 9) && c.quality == Quality::Dim) addIfScale (9);
    }
    if (extensionLevel >= 2) addIfScale (2);                       // 9
    if (extensionLevel >= 3)
    {
        if (c.isMinorThird() || c.quality == Quality::Sus4) addIfScale (5);  // 11 só em acordes menores
        else addIfScale (9);                                       // 13 (6) em maiores/dominantes
    }
    return out;
}

//==============================================================================
// Detecção de acordes por modelos (templates) sobre o cromagrama.
struct ChordTemplateMatch
{
    // Pontua todas as fundamentais × qualidades. bassChroma é opcional (pode ser zeros).
    static Chord best (const Chroma& chroma, const Chroma& bassChroma, const Key* keyPrior,
                       bool allowSevenths, float* secondBestScore = nullptr)
    {
        float norm = 0;
        for (float v : chroma) norm += v * v;
        norm = std::sqrt (norm);
        Chord bestC; float bestS = -1e9f, second = -1e9f;
        if (norm < 1e-9f) { if (secondBestScore) *secondBestScore = 0; return bestC; }

        float bnorm = 0, maxC = 0;
        for (float v : bassChroma) bnorm += v;
        for (float v : chroma) maxC = std::max (maxC, v);

        static const Quality triads[] = { Quality::Major, Quality::Minor, Quality::Dim, Quality::Aug, Quality::Sus2, Quality::Sus4 };
        static const Quality sevenths[] = { Quality::Dom7, Quality::Maj7, Quality::Min7, Quality::HalfDim7, Quality::Dim7 };

        auto score = [&] (int root, Quality q)
        {
            const auto& qi = qualityInfo (q);
            // pesos: fundamental, terça, quinta, sétima
            static const float w[4] = { 1.0f, 0.85f, 0.75f, 0.6f };
            float t[12] = {};
            float tn = 0;
            for (int i = 0; i < qi.numTones; ++i)
            {
                t[pc (root + qi.tones[(size_t) i])] = w[i];
                tn += w[i] * w[i];
            }
            float dot = 0, outside = 0;
            for (int i = 0; i < 12; ++i)
            {
                dot += chroma[(size_t) i] * t[i];
                if (t[i] == 0) outside += chroma[(size_t) i];
            }
            float s = dot / (norm * std::sqrt (tn));
            s -= 0.12f * outside / (norm * 3.4641f);
            // preferências: tríades comuns, penalidade leve para qualidades raras
            if (q == Quality::Aug || q == Quality::Dim7) s -= 0.05f;
            if (q == Quality::Dim || q == Quality::Sus2 || q == Quality::Sus4) s -= 0.03f;
            if (qi.numTones == 4)
            {
                // sétima só vence se estiver realmente presente (harmônicos do piano criam 7ªs falsas)
                const float sev = chroma[(size_t) pc (root + qi.tones[3])] / std::max (1e-6f, maxC);
                s -= 0.015f + (sev < 0.45f ? 0.12f * (0.45f - sev) / 0.45f + 0.03f : 0.0f);
                if (q == Quality::Maj7) s -= 0.01f;
            }
            if (bnorm > 1e-6f) s += 0.10f * (bassChroma[(size_t) root] / bnorm);
            if (keyPrior)
            {
                int inKey = 0;
                for (int i = 0; i < qi.numTones; ++i) inKey += keyPrior->contains (root + qi.tones[(size_t) i]) ? 1 : 0;
                s += 0.04f * ((float) inKey / (float) qi.numTones) - 0.02f;
            }
            return s;
        };

        auto consider = [&] (int root, Quality q)
        {
            const float s = score (root, q);
            if (s > bestS) { second = bestS; bestS = s; bestC.root = root; bestC.quality = q; }
            else if (s > second) second = s;
        };
        for (int r = 0; r < 12; ++r)
        {
            for (auto q : triads) consider (r, q);
            if (allowSevenths) for (auto q : sevenths) consider (r, q);
        }
        bestC.confidence = std::clamp (bestS, 0.0f, 1.0f);
        if (secondBestScore) *secondBestScore = second;

        // inversão: baixo dominante diferente da fundamental e pertencente ao acorde
        if (bnorm > 1e-6f)
        {
            int bmax = 0;
            for (int i = 1; i < 12; ++i) if (bassChroma[(size_t) i] > bassChroma[(size_t) bmax]) bmax = i;
            if (bmax != bestC.root && ((bestC.mask() >> bmax) & 1u) && bassChroma[(size_t) bmax] > 1.6f * bassChroma[(size_t) bestC.root] + 0.05f)
                bestC.bass = bmax;
        }
        return bestC;
    }
};

//==============================================================================
// Tonalidade por perfis de Krumhansl–Kessler (24 tonalidades maiores/menores).
struct KeyFinder
{
    static Key estimate (const Chroma& longTerm, float* confidence = nullptr)
    {
        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float minr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        auto corr = [&] (const float* prof, int tonic)
        {
            float mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += longTerm[(size_t) i]; my += prof[i]; }
            mx /= 12; my /= 12;
            float sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const float x = longTerm[(size_t) pc (i + tonic)] - mx, y = prof[i] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return (sxx < 1e-12f) ? 0.0f : sxy / std::sqrt (sxx * syy);
        };
        Key best; float bs = -2, second = -2;
        for (int t = 0; t < 12; ++t)
        {
            const float a = corr (maj, t), b = corr (minr, t);
            if (a > bs) { second = bs; bs = a; best = { t, Scale::Major }; } else if (a > second) second = a;
            if (b > bs) { second = bs; bs = b; best = { t, Scale::NaturalMinor }; } else if (b > second) second = b;
        }
        if (confidence) *confidence = std::max (0.0f, bs - std::max (second, 0.0f) * 0.5f);
        return best;
    }
};

// Ajusta uma tonalidade detectada (maior/menor) ao modo escolhido pelo usuário mantendo a tônica.
inline Key applyScaleChoice (Key detected, int scaleChoice /* -1 = auto */)
{
    if (scaleChoice >= 0 && scaleChoice < (int) Scale::count) detected.scale = (Scale) scaleChoice;
    return detected;
}

// Corrige um acorde detectado para soar dentro da escala (opcional, "respeitar escala").
inline Chord conformToKey (Chord c, const Key& key)
{
    if (! c.valid()) return c;
    const auto m = c.mask();
    int outside = 0;
    for (int i = 0; i < 12; ++i) if (((m >> i) & 1u) && ! key.contains (i)) ++outside;
    if (outside == 0) return c;
    // tenta trocar a qualidade da terça/quinta pela versão diatônica
    static const Quality cands[] = { Quality::Major, Quality::Minor, Quality::Dim, Quality::Sus4, Quality::Sus2 };
    if (key.contains (c.root))
    {
        for (auto q : cands)
        {
            Chord t = c; t.quality = q;
            const auto tm = t.mask();
            bool ok = true;
            for (int i = 0; i < 12; ++i) if (((tm >> i) & 1u) && ! key.contains (i)) ok = false;
            if (ok) { if (t.bass >= 0 && ! ((tm >> t.bass) & 1u)) t.bass = -1; return t; }
        }
    }
    return c; // empréstimo modal genuíno: mantém
}
} // namespace sms
