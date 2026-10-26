// SeniorMy Acoustic Guitars — análise offline de loops importados (áudio ou MIDI):
// andamento (autocorrelação do envelope de ataques), acordes por tempo, tonalidade e
// padrão rítmico de 16 passos (para criar ostinatos a partir do groove do loop).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Analysis.h"
#include <vector>
#include <cmath>

namespace sms
{
struct ProgressionChord { double startBeat = 0, lengthBeats = 4; Chord chord; };

struct Progression
{
    std::vector<ProgressionChord> chords;
    double lengthBeats = 0;       // tamanho do loop
    double bpm = 120;
    Key key {};
    std::array<float, 16> rhythm {};   // força de ataque por semicolcheia (0..1), média do loop
    std::string sourceName;
    bool empty() const { return chords.empty(); }

    Chord chordAt (double beat) const
    {
        if (chords.empty() || lengthBeats <= 0) return {};
        double b = std::fmod (beat, lengthBeats);
        if (b < 0) b += lengthBeats;
        for (const auto& c : chords) if (b >= c.startBeat && b < c.startBeat + c.lengthBeats) return c.chord;
        return chords.back().chord;
    }
};

struct MidiNoteIn { double startBeat, lengthBeats; int note; float velocity; };

namespace loopanalysis
{
// junta tempos consecutivos com o mesmo acorde e elimina acordes de 1 tempo fracos
inline void mergeChords (std::vector<ProgressionChord>& v)
{
    std::vector<ProgressionChord> out;
    for (auto& c : v)
    {
        if (! out.empty() && out.back().chord == c.chord) out.back().lengthBeats += c.lengthBeats;
        else out.push_back (c);
    }
    // acordes isolados de 1 tempo com baixa confiança viram o anterior
    for (size_t i = 1; i + 1 < out.size(); ++i)
        if (out[i].lengthBeats <= 1.01 && out[i].chord.confidence < 0.55f && out[i - 1].chord == out[i + 1].chord)
        {
            out[i - 1].lengthBeats += out[i].lengthBeats + out[i + 1].lengthBeats;
            out.erase (out.begin() + (long) i, out.begin() + (long) i + 2);
            --i;
        }
    v = out;
}

inline float estimateBpm (const std::vector<float>& onset, float framesPerSecond, float lo = 70, float hi = 170)
{
    if (onset.size() < 16) return 120.0f;
    float best = 120, bestV = -1;
    for (float bpm = lo; bpm <= hi; bpm += 0.5f)
    {
        const float lag = framesPerSecond * 60.0f / bpm;
        float acc = 0;
        for (int mult = 1; mult <= 4; ++mult)
        {
            const float L = lag * (float) mult;
            const int li = (int) L;
            const float f = L - (float) li;
            float s = 0;
            for (size_t i = (size_t) li + 1; i < onset.size(); ++i)
                s += onset[i] * (onset[i - (size_t) li] * (1 - f) + onset[i - (size_t) li - 1] * f);
            acc += s / (float) mult;
        }
        // leve preferência por 90–130
        acc *= 1.0f - 0.15f * std::fabs (bpm - 110.0f) / 60.0f;
        if (acc > bestV) { bestV = acc; best = bpm; }
    }
    return best;
}
} // namespace loopanalysis

// Áudio mono → progressão. bpmHint > 0 usa o andamento informado (do host ou do nome do arquivo).
inline Progression analyseAudioLoop (const std::vector<float>& mono, double sampleRate, double bpmHint = 0)
{
    Progression p;
    if (mono.size() < (size_t) (sampleRate * 0.5)) return p;

    // 1) envelope de ataques (fluxo espectral simplificado por energia em bandas, hop 512)
    const int hop = 512;
    const size_t frames = mono.size() / (size_t) hop;
    std::vector<float> onset (frames, 0.0f);
    float prevE = 0;
    float hpZ = 0;
    for (size_t f = 0; f < frames; ++f)
    {
        float e = 0;
        for (int i = 0; i < hop; ++i)
        {
            const float x = mono[f * (size_t) hop + (size_t) i];
            hpZ += 0.2f * (x - hpZ);
            const float h = x - hpZ;   // realça ataques
            e += h * h;
        }
        e = std::log1p (1000.0f * e / (float) hop);
        onset[f] = std::max (0.0f, e - prevE);
        prevE = e;
    }
    const float fps = (float) sampleRate / (float) hop;
    double bpm = bpmHint > 0 ? bpmHint : loopanalysis::estimateBpm (onset, fps);
    // arredonda o tamanho para compassos inteiros de 4 tempos
    const double seconds = (double) mono.size() / sampleRate;
    double beats = seconds * bpm / 60.0;
    double bars = std::max (1.0, std::round (beats / 4.0));
    if (bpmHint <= 0)
    {
        // reajusta o BPM para o loop fechar em compassos inteiros (loops costumam ser exatos)
        bpm = bars * 4.0 * 60.0 / seconds;
        if (bpm < 60) bpm *= 2;
        if (bpm > 200) bpm /= 2;
        beats = seconds * bpm / 60.0;
        bars = std::max (1.0, std::round (beats / 4.0));
    }
    p.bpm = bpm;
    p.lengthBeats = bars * 4.0;

    // 2) ritmo em 16 passos (média por posição no compasso)
    std::array<float, 16> acc {}; std::array<int, 16> cnt {};
    for (size_t f = 0; f < frames; ++f)
    {
        const double beat = (double) f / fps * bpm / 60.0;
        const int step = (int) std::floor (std::fmod (beat * 4.0 + 0.25, 16.0));
        if (step >= 0 && step < 16) { acc[(size_t) step] += onset[f]; ++cnt[(size_t) step]; }
    }
    float mx = 1e-9f;
    for (int i = 0; i < 16; ++i) { acc[(size_t) i] = cnt[(size_t) i] ? acc[(size_t) i] / (float) cnt[(size_t) i] : 0; mx = std::max (mx, acc[(size_t) i]); }
    for (int i = 0; i < 16; ++i) p.rhythm[(size_t) i] = acc[(size_t) i] / mx;

    // 3) croma por tempo
    ChromaAnalyzer ca;
    ca.prepare (sampleRate);
    const int N = ca.frameSize();
    std::vector<float> frame ((size_t) N);
    const int numBeats = (int) p.lengthBeats;
    std::vector<Chroma> beatChroma ((size_t) numBeats), beatBass ((size_t) numBeats);
    Chroma total {};
    for (int b = 0; b < numBeats; ++b)
    {
        Chroma c {}, bc {};
        // 3 quadros por tempo
        for (int k = 0; k < 3; ++k)
        {
            const double t = ((double) b + 0.2 + 0.3 * k) * 60.0 / bpm;
            const long center = (long) (t * sampleRate);
            for (int i = 0; i < N; ++i)
            {
                const long idx = center - N / 2 + i;
                frame[(size_t) i] = (idx >= 0 && idx < (long) mono.size()) ? mono[(size_t) idx] : 0.0f;
            }
            ca.analyseFrame (frame.data());
            for (int j = 0; j < 12; ++j) { c[(size_t) j] += ca.getChroma()[(size_t) j]; bc[(size_t) j] += ca.getBassChroma()[(size_t) j]; }
        }
        beatChroma[(size_t) b] = c; beatBass[(size_t) b] = bc;
        for (int j = 0; j < 12; ++j) total[(size_t) j] += c[(size_t) j];
    }
    p.key = KeyFinder::estimate (total);

    // 4) acorde por meio compasso (2 tempos), com refinamento por tempo quando a mudança é clara
    std::vector<ProgressionChord> raw;
    for (int b = 0; b < numBeats; b += 2)
    {
        Chroma c {}, bc {};
        for (int k = b; k < std::min (numBeats, b + 2); ++k)
            for (int j = 0; j < 12; ++j) { c[(size_t) j] += beatChroma[(size_t) k][(size_t) j]; bc[(size_t) j] += beatBass[(size_t) k][(size_t) j]; }
        ProgressionChord pc; pc.startBeat = b; pc.lengthBeats = std::min (2, numBeats - b);
        pc.chord = ChordTemplateMatch::best (c, bc, &p.key, true);
        if (! pc.chord.valid()) { if (! raw.empty()) pc.chord = raw.back().chord; else continue; }
        raw.push_back (pc);
    }
    loopanalysis::mergeChords (raw);
    p.chords = raw;
    return p;
}

// Notas MIDI → progressão (exato por classe de altura, um acorde por tempo, depois une).
inline Progression analyseMidiLoop (const std::vector<MidiNoteIn>& notes, double bpm)
{
    Progression p;
    p.bpm = bpm > 0 ? bpm : 120.0;
    if (notes.empty()) return p;
    double end = 0;
    for (auto& n : notes) end = std::max (end, n.startBeat + n.lengthBeats);
    p.lengthBeats = std::max (4.0, std::ceil (end / 4.0 - 1e-6) * 4.0);
    const int numBeats = (int) p.lengthBeats;

    Chroma total {};
    std::vector<Chroma> bc ((size_t) numBeats), bb ((size_t) numBeats);
    std::vector<int> lowest ((size_t) numBeats, 200);
    std::array<float, 16> acc {};
    for (auto& n : notes)
    {
        for (int b = std::max (0, (int) std::floor (n.startBeat)); b < std::min (numBeats, (int) std::ceil (n.startBeat + n.lengthBeats)); ++b)
        {
            const double ov = std::min ((double) b + 1, n.startBeat + n.lengthBeats) - std::max ((double) b, n.startBeat);
            if (ov <= 0) continue;
            bc[(size_t) b][(size_t) pc (n.note)] += (float) ov * (0.5f + n.velocity);
            lowest[(size_t) b] = std::min (lowest[(size_t) b], n.note);
        }
        total[(size_t) pc (n.note)] += (float) n.lengthBeats;
    }
    // ritmo: cada instante de ataque conta uma vez (notas simultâneas = um ataque), pela maior velocity
    {
        std::vector<std::pair<long, float>> onsets;
        for (auto& n : notes) onsets.push_back ({ std::lround (n.startBeat * 4.0), n.velocity });
        std::sort (onsets.begin(), onsets.end());
        std::array<int, 16> bars {};
        long last = -1; float lastV = 0;
        auto flush = [&] { if (last >= 0) { acc[(size_t) (last % 16)] += lastV; ++bars[(size_t) (last % 16)]; } };
        for (auto& o : onsets)
        {
            if (o.first != last) { flush(); last = o.first; lastV = o.second; }
            else lastV = std::max (lastV, o.second);
        }
        flush();
        const float nBars = (float) std::max (1.0, p.lengthBeats / 4.0);
        for (int i = 0; i < 16; ++i) acc[(size_t) i] /= nBars;
    }
    float mx = 1e-9f;
    for (float a : acc) mx = std::max (mx, a);
    for (int i = 0; i < 16; ++i) p.rhythm[(size_t) i] = acc[(size_t) i] / mx;
    p.key = KeyFinder::estimate (total);

    std::vector<ProgressionChord> raw;
    for (int b = 0; b < numBeats; ++b)
    {
        Chroma bass {};
        if (lowest[(size_t) b] < 200) bass[(size_t) pc (lowest[(size_t) b])] = 1.0f;
        ProgressionChord c; c.startBeat = b; c.lengthBeats = 1;
        float s = 0;
        for (float v : bc[(size_t) b]) s += v;
        if (s <= 0) { if (! raw.empty()) { raw.back().lengthBeats += 1; } continue; }
        c.chord = ChordTemplateMatch::best (bc[(size_t) b], bass, &p.key, true);
        c.chord.confidence = std::max (c.chord.confidence, 0.6f);
        raw.push_back (c);
    }
    loopanalysis::mergeChords (raw);
    p.chords = raw;
    return p;
}

} // namespace sms
