// SeniorMy Acoustic Guitars — motor: 3 partes (instrumento + modo + padrão + mixer), campo
// harmônico, progressão de acordes, reconhecimento de acorde na zona da mão esquerda, relógio
// (DAW ou BPM interno), agendamento das batidas com atraso por corda e humanização, vozes,
// corpo modal, EQ, largura estéreo, reverb de sala e master.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Analysis.h"
#include "Patterns.h"
#include "Voices.h"
#include "Dsp.h"
#include <atomic>
#include <array>
#include <vector>
#include <memory>
#include <cstring>

namespace sms
{
constexpr int kParts = 3;
constexpr int kMaxProg = 16;
constexpr int kVoices = 96;

enum PartMode : int { ModeOff = 0, ModePattern, ModeStrumOnce, ModeKeys, NumModes };
inline const char* modeNamePt (int m)
{
    static const char* n[] = { "Desligada", "Padrão (batida/dedilhado)", "Batida única por acorde", "Teclado (notas)" };
    return n[std::clamp (m, 0, (int) NumModes - 1)];
}

struct PartState
{
    int instrument = Steel, tuning = 0, capo = 0, position = 0;
    int mode = ModePattern;
    int octave = 0;              // modo Teclado: transposição em oitavas
    Pattern pattern {};
    float levelDb = 0, pan = 0, tone = 0, body = 0.3f, width = 0.7f, reverb = 0.25f, noises = 0.35f;
    bool mute = false, solo = false;
};

struct ProgStep { int root = 0; int quality = 0; int bass = -1; int beats = 4; };

struct HarmonyState
{
    int keyTonic = 0, keyScale = 0;
    bool tetrads = false;
    bool fieldMode = false;      // teclas brancas da zona de acordes = graus do campo harmônico
    int split = 60;              // notas abaixo: acordes; a partir daqui: modo Teclado
    bool progressionOn = false;
    bool latch = true;           // mantém o padrão tocando depois de soltar o acorde
    int numProg = 0;
    std::array<ProgStep, kMaxProg> prog {};
};

struct MasterState
{
    float room = 0.45f, reverbMix = 0.22f, width = 1.0f, volumeDb = -3.0f;
    float bpm = 96.0f;
    bool internalPlay = false;
};

struct EngineState
{
    std::array<PartState, kParts> parts {};
    HarmonyState harmony {};
    MasterState master {};
};

struct Transport { bool hostPlaying = false; bool hasPpq = false; double ppq = 0; double bpm = 120; };

inline Chord diatonicChord (const Key& key, int degree, bool tetrad)
{
    const auto st = scaleSteps (key.scale);
    const int d = ((degree % 7) + 7) % 7;
    const int r = st[(size_t) d];
    auto iv = [&] (int k) { return ((st[(size_t) ((d + k) % 7)] - r) % 12 + 12) % 12; };
    const int t = iv (2), f = iv (4), s = iv (6);
    Chord c; c.root = pc (key.tonic + r);
    if (t == 4 && f == 7) c.quality = tetrad ? (s == 11 ? Quality::Maj7 : Quality::Dom7) : Quality::Major;
    else if (t == 3 && f == 7) c.quality = tetrad ? (s == 10 ? Quality::Min7 : Quality::MinMaj7) : Quality::Minor;
    else if (t == 3 && f == 6) c.quality = tetrad ? (s == 10 ? Quality::HalfDim7 : Quality::Dim7) : Quality::Dim;
    else if (t == 4 && f == 8) c.quality = Quality::Aug;
    else c.quality = Quality::Major;
    return c;
}

inline const char* degreeRoman (int d, bool minorish)
{
    static const char* up[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* lo[] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    return minorish ? lo[d % 7] : up[d % 7];
}

//==============================================================================
class Engine
{
public:
    Engine()
    {
        for (auto& v : voices) v.prepareModel (4096);
        patterns = factoryPatterns();
    }

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        for (auto& b : partL) b.assign ((size_t) maxBlock, 0.0f);
        for (auto& b : partR) b.assign ((size_t) maxBlock, 0.0f);
        sendL.assign ((size_t) maxBlock, 0.0f); sendR.assign ((size_t) maxBlock, 0.0f);
        wetL.assign ((size_t) maxBlock, 0.0f); wetR.assign ((size_t) maxBlock, 0.0f);
        reverb.prepare (sr);
        for (auto& v : voices) v.type = StringVoice::Off;
        pendingCount = 0; sampleClock = 0;
        configured = false;
        applyState();
    }

    void setBank (const Bank* b) { bank.store (b); }
    // parâmetros automatizáveis (baratos: não refazem filtros)
    void setLive (float volumeDb, float reverbMix, const std::array<float, kParts>& levelsDb, float bpm, bool play)
    {
        st.master.volumeDb = volumeDb; st.master.reverbMix = reverbMix; st.master.bpm = bpm; st.master.internalPlay = play;
        for (int p = 0; p < kParts; ++p) st.parts[(size_t) p].levelDb = levelsDb[(size_t) p];
    }
    void setState (const EngineState& s) { st = s; applyState(); }
    const EngineState& state() const { return st; }
    const std::vector<Pattern>& factory() const { return patterns; }

    // ---- MIDI
    void noteOn (int note, float vel, int offset)
    {
        if (note < 0 || note > 127) return;
        const bool keysOnly = ! anyChordPart();
        if (! keysOnly && note < st.harmony.split)
        {
            held[(size_t) note] = true; heldVel[(size_t) note] = vel; chordDirty = true; chordVel = vel;
            chordOffset = offset;
            if (! anyHeld) newPress = true;
            anyHeld = true;
            return;
        }
        for (int p = 0; p < kParts; ++p)
            if (st.parts[(size_t) p].mode == ModeKeys) playKey (p, note, vel, offset);
        keyDown[(size_t) note] = true;
    }
    void noteOff (int note, int)
    {
        if (note < 0 || note > 127) return;
        if (held[(size_t) note]) { held[(size_t) note] = false; chordDirty = true; return; }
        keyDown[(size_t) note] = false;
        if (sustain) { sustained[(size_t) note] = true; return; }
        releaseKey (note);
    }
    void setSustain (bool down)
    {
        sustain = down;
        if (! down) for (int n = 0; n < 128; ++n) if (sustained[(size_t) n]) { sustained[(size_t) n] = false; if (! keyDown[(size_t) n]) releaseKey (n); }
    }
    void allNotesOff()
    {
        for (int n = 0; n < 128; ++n) { held[(size_t) n] = keyDown[(size_t) n] = sustained[(size_t) n] = false; }
        anyHeld = false; chordDirty = true; latched = false;
        for (auto& v : voices) v.release (0.08f);
        pendingCount = 0;
    }
    // acorde vindo da interface (botões do campo harmônico)
    void auditionChord (const Chord& c, float vel) { uiChord = c; uiChordVel = vel; uiChordPending.store (true); }
    void stopAll() { stopRequest.store (true); }

    // ---- áudio
    void process (float* outL, float* outR, int n, const Transport& t)
    {
        if (! configured) applyState();
        std::fill (outL, outL + n, 0.0f); std::fill (outR, outR + n, 0.0f);
        for (int p = 0; p < kParts; ++p) { std::fill (partL[(size_t) p].begin(), partL[(size_t) p].begin() + n, 0.0f); std::fill (partR[(size_t) p].begin(), partR[(size_t) p].begin() + n, 0.0f); }
        std::fill (sendL.begin(), sendL.begin() + n, 0.0f); std::fill (sendR.begin(), sendR.begin() + n, 0.0f);

        if (stopRequest.exchange (false)) { latched = false; currentChord = {}; for (auto& v : voices) v.release (0.1f); pendingCount = 0; }
        if (uiChordPending.exchange (false)) setChord (uiChord, uiChordVel, 0, true);
        if (chordDirty) updateHeldChord();

        // ---- relógio
        const double bpm = (t.hostPlaying && t.bpm > 0) ? t.bpm : std::max (30.0f, st.master.bpm);
        const double beatsPerSample = bpm / 60.0 / sr;
        const bool chordActive = anyHeld || latched;
        const bool wantRun = t.hostPlaying || st.master.internalPlay || (chordActive && currentChord.valid());
        double b0;
        if (t.hostPlaying && t.hasPpq) b0 = t.ppq;
        else
        {
            if (wantRun && ! running) internalBeat = 0;   // começa no tempo 1 ao tocar o acorde
            b0 = internalBeat;
        }
        const double b1 = b0 + beatsPerSample * n;
        running = wantRun;

        // progressão
        if (st.harmony.progressionOn && st.harmony.numProg > 0 && running && ! anyHeld)
        {
            const Chord pch = progressionChordAt (b0);
            if (pch.valid() && pch != currentChord) setChord (pch, 0.85f, 0, false);
        }

        // ---- agenda os passos de cada parte
        if (running && currentChord.valid())
            for (int p = 0; p < kParts; ++p)
            {
                const auto& ps = st.parts[(size_t) p];
                if (ps.mode != ModePattern) continue;
                const auto& pat = ps.pattern;
                const double stepLen = 1.0 / std::max (1, pat.stepsPerBeat);
                const long long kFirst = (long long) std::floor (b0 / stepLen) - 1;
                const long long kLast = (long long) std::floor (b1 / stepLen) + 1;
                for (long long k = kFirst; k <= kLast; ++k)
                {
                    double tb = (double) k * stepLen;
                    if (pat.swing > 0 && (k & 1)) tb += pat.swing * stepLen * 0.5;
                    if (tb < b0 || tb >= b1) continue;
                    const int off = std::clamp ((int) ((tb - b0) / beatsPerSample), 0, n - 1);
                    fireStep (p, (int) (k % std::max (1, pat.numSteps)), off, (float) (60.0 / bpm));
                    stepNow[(size_t) p].store ((int) (k % std::max (1, pat.numSteps)));
                }
            }
        if (! (t.hostPlaying && t.hasPpq)) internalBeat = running ? b1 : 0.0;
        beatNow.store (b0);

        // ---- eventos pendentes → vozes
        dispatchPending (n);

        // ---- render das vozes nos barramentos das partes
        for (auto& v : voices)
        {
            if (v.type == StringVoice::Off || v.part < 0) continue;
            int start = 0;
            if (v.delay >= n) { v.delay -= n; continue; }
            if (v.delay > 0) { start = v.delay; v.delay = 0; }
            v.render (partL[(size_t) v.part].data(), partR[(size_t) v.part].data(), start, n - start);
        }

        // ---- mixer
        bool anySolo = false; for (auto& ps : st.parts) anySolo |= ps.solo;
        for (int p = 0; p < kParts; ++p)
        {
            auto& ps = st.parts[(size_t) p]; auto& fx = partFx[(size_t) p];
            float* L = partL[(size_t) p].data(); float* R = partR[(size_t) p].data();
            const bool audible = ! ps.mute && (! anySolo || ps.solo) && ps.mode != ModeOff;
            const float g = audible ? std::pow (10.0f, ps.levelDb / 20.0f) : 0.0f;
            const float pan = std::clamp (ps.pan, -1.0f, 1.0f);
            const float gl = g * std::min (1.0f, 1.0f - pan), gr = g * std::min (1.0f, 1.0f + pan);
            const float wdt = std::clamp (ps.width, 0.0f, 1.0f) * 1.3f;
            const bool modeled = instrument (ps.instrument).source == Source::Modeled || ! instrumentHasSamples (ps.instrument);
            const auto& m = instrument (ps.instrument).model;
            const float rv = std::clamp (ps.reverb, 0.0f, 1.0f);
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R[i];
                if (modeled)
                {
                    float bl = 0, br = 0;
                    for (int k = 0; k < 6; ++k) { bl += fx.bodyL[(size_t) k].process (l) * m.bodyGain[(size_t) k]; br += fx.bodyR[(size_t) k].process (r) * m.bodyGain[(size_t) k]; }
                    l = l * (1.0f - m.bodyMix) + bl * m.bodyMix * 2.2f;
                    r = r * (1.0f - m.bodyMix) + br * m.bodyMix * 2.2f;
                    l = fx.dcL.hp (l); r = fx.dcR.hp (r);
                }
                l = fx.hiL.process (fx.loL.process (fx.bodyPkL.process (l)));
                r = fx.hiR.process (fx.loR.process (fx.bodyPkR.process (r)));
                const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * wdt;
                l = (mid + side) * gl; r = (mid - side) * gr;
                outL[i] += l; outR[i] += r;
                sendL[(size_t) i] += l * rv; sendR[(size_t) i] += r * rv;
            }
        }

        // ---- master
        reverb.process (sendL.data(), sendR.data(), wetL.data(), wetR.data(), n);
        const float vol = std::pow (10.0f, st.master.volumeDb / 20.0f);
        const float rmix = std::clamp (st.master.reverbMix, 0.0f, 1.0f) * 2.0f;
        const float mw = std::clamp (st.master.width, 0.0f, 1.5f);
        float pk = 0;
        for (int i = 0; i < n; ++i)
        {
            float l = outL[i] + wetL[(size_t) i] * rmix, r = outR[i] + wetR[(size_t) i] * rmix;
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * mw;
            l = (mid + side) * vol; r = (mid - side) * vol;
            l = softClip (l); r = softClip (r);
            if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0; }
            outL[i] = l; outR[i] = r; pk = std::max (pk, std::max (std::abs (l), std::abs (r)));
        }
        meter.store (std::max (pk, meter.load() * 0.85f));
        sampleClock += (uint64_t) n;
    }

    // ---- leituras para a interface
    Chord getChord() const { Chord c; const int v = chordCode.load(); if (v >= 0) { c.root = v & 15; c.quality = (Quality) ((v >> 4) & 31); c.bass = ((v >> 9) & 15) - 1; } return c; }
    int getStep (int p) const { return stepNow[(size_t) p].load(); }
    float getMeter() const { return meter.load(); }
    double getBeat() const { return beatNow.load(); }
    bool isRunning() const { return running; }
    std::array<int, kMaxCourses> getFrets (int p) const { std::array<int, kMaxCourses> f {}; for (int i = 0; i < kMaxCourses; ++i) f[(size_t) i] = frets[(size_t) p][(size_t) i].load(); return f; }
    bool instrumentHasSamples (int inst) const
    {
        const auto* b = bank.load();
        return b && b->set (instrument (inst).sampleSet) != nullptr;
    }
    int activeVoices() const { int k = 0; for (auto& v : voices) if (v.type != StringVoice::Off) ++k; return k; }

    // progressão: acorde no tempo (com repetição)
    Chord progressionChordAt (double beat) const
    {
        const auto& h = st.harmony;
        int total = 0; for (int i = 0; i < h.numProg; ++i) total += std::max (1, h.prog[(size_t) i].beats);
        if (total <= 0) return {};
        double b = std::fmod (beat, (double) total); if (b < 0) b += total;
        int acc = 0;
        for (int i = 0; i < h.numProg; ++i)
        {
            acc += std::max (1, h.prog[(size_t) i].beats);
            if (b < acc) { Chord c; c.root = h.prog[(size_t) i].root; c.quality = (Quality) h.prog[(size_t) i].quality; c.bass = h.prog[(size_t) i].bass; return c; }
        }
        return {};
    }
    int progressionIndexAt (double beat) const
    {
        const auto& h = st.harmony;
        int total = 0; for (int i = 0; i < h.numProg; ++i) total += std::max (1, h.prog[(size_t) i].beats);
        if (total <= 0) return -1;
        double b = std::fmod (beat, (double) total); if (b < 0) b += total;
        int acc = 0;
        for (int i = 0; i < h.numProg; ++i) { acc += std::max (1, h.prog[(size_t) i].beats); if (b < acc) return i; }
        return -1;
    }

    Key key() const { Key k; k.tonic = st.harmony.keyTonic; k.scale = (Scale) std::clamp (st.harmony.keyScale, 0, (int) Scale::count - 1); return k; }

private:
    struct PartFx
    {
        std::array<Biquad, 6> bodyL {}, bodyR {};
        Biquad bodyPkL, bodyPkR, loL, loR, hiL, hiR;
        OnePole dcL, dcR;
    };
    struct Pending { uint64_t time; int part, course, note; float vel, mute, gain, pan, cents; uint8_t kind; bool up; };

    bool anyChordPart() const { for (auto& p : st.parts) if (p.mode == ModePattern || p.mode == ModeStrumOnce) return true; return false; }

    void applyState()
    {
        configured = true;
        for (int p = 0; p < kParts; ++p)
        {
            auto& ps = st.parts[(size_t) p]; auto& fx = partFx[(size_t) p];
            ps.pattern.numSteps = std::clamp (ps.pattern.numSteps, 1, kMaxSteps);
            ps.pattern.stepsPerBeat = std::clamp (ps.pattern.stepsPerBeat, 1, 8);
            const auto& d = instrument (ps.instrument);
            for (int k = 0; k < 6; ++k)
            {
                fx.bodyL[(size_t) k].bandpass (d.model.bodyHz[(size_t) k], d.model.bodyQ[(size_t) k], sr);
                fx.bodyR[(size_t) k] = fx.bodyL[(size_t) k];
                fx.bodyL[(size_t) k].reset(); fx.bodyR[(size_t) k].reset();
            }
            const float bodyF = d.source == Source::Modeled ? d.model.bodyHz[0] * 1.1f : (d.tunings[0].courses[0].note < 50 ? 115.0f : 230.0f);
            fx.bodyPkL.peak (bodyF, 1.1, ps.body * 7.0 - 1.5, sr); fx.bodyPkR = fx.bodyPkL;
            fx.loL.lowShelf (220.0, -ps.tone * 4.0, sr); fx.loR = fx.loL;
            fx.hiL.highShelf (3200.0, ps.tone * 6.0 + d.brightDb, sr); fx.hiR = fx.hiL;
            fx.dcL.setCutoff (30.0f, (float) sr); fx.dcR = fx.dcL;
        }
        reverb.set (st.master.room, 0.25f + st.master.room * 0.5f, 0.45f, 12.0f, 1.0f);
        // revoicing com o novo instrumento/afinação
        if (currentChord.valid()) for (int p = 0; p < kParts; ++p) computeVoicing (p);
    }

    void updateHeldChord()
    {
        chordDirty = false;
        int count = 0; for (int i = 0; i < 128; ++i) if (held[(size_t) i]) ++count;
        if (count == 0)
        {
            anyHeld = false;
            if (! st.harmony.latch && ! (st.harmony.progressionOn && running))
            {
                for (auto& v : voices) if (v.part >= 0 && isChordPart (v.part)) v.release (0.12f);
                currentChord = {}; chordCode.store (-1); pendingCount = removePendingChordParts();
                latched = false;
            }
            return;
        }
        Chord c;
        if (st.harmony.fieldMode)
        {
            int lowest = -1; for (int i = 0; i < 128; ++i) if (held[(size_t) i]) { lowest = i; break; }
            static const int whiteDegree[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
            const int d = whiteDegree[pc (lowest)];
            if (d >= 0) c = diatonicChord (key(), d, st.harmony.tetrads);
            else { c = diatonicChord (key(), whiteDegree[pc (lowest - 1)] >= 0 ? whiteDegree[pc (lowest - 1)] : 0, st.harmony.tetrads); c.quality = st.harmony.tetrads ? Quality::Dom7 : Quality::Major; c.root = pc (c.root + 1); }
        }
        else if (count == 1)
        {
            // uma nota só: acorde diatônico com essa fundamental (ou maior, fora da escala)
            int note = 0; for (int i = 0; i < 128; ++i) if (held[(size_t) i]) note = i;
            const auto k = key(); int deg = -1; const auto stp = scaleSteps (k.scale);
            for (int d = 0; d < 7; ++d) if (pc (k.tonic + stp[(size_t) d]) == pc (note)) deg = d;
            if (deg >= 0) c = diatonicChord (k, deg, st.harmony.tetrads); else { c.root = pc (note); c.quality = Quality::Major; }
        }
        else
        {
            bool h[128]; for (int i = 0; i < 128; ++i) h[i] = held[(size_t) i];
            const Key k = key();
            c = chordFromNotes (h, &k, true);
        }
        if (c.valid())
        {
            const bool changed = c != currentChord || newPress;
            if (changed) setChord (c, chordVel, chordOffset, newPress);
            latched = st.harmony.latch;
        }
        newPress = false;
    }

    bool isChordPart (int p) const { const int m = st.parts[(size_t) p].mode; return m == ModePattern || m == ModeStrumOnce; }

    int removePendingChordParts()
    {
        int k = 0;
        for (int i = 0; i < pendingCount; ++i) if (! isChordPart (pending[(size_t) i].part)) pending[(size_t) k++] = pending[(size_t) i];
        return k;
    }

    void setChord (const Chord& c, float vel, int offset, bool fresh)
    {
        currentChord = c;
        chordCode.store (c.root | ((int) c.quality << 4) | ((c.bass + 1) << 9));
        for (int p = 0; p < kParts; ++p)
        {
            if (! isChordPart (p)) continue;
            const auto old = voicing[(size_t) p];
            computeVoicing (p);
            // dedos levantam: cordas cuja casa mudou param de soar
            for (auto& v : voices)
                if (v.part == p && v.type != StringVoice::Off && ! v.releasing && v.course >= 0
                    && v.course < old.numCourses && old.fret[(size_t) v.course] != voicing[(size_t) p].fret[(size_t) v.course])
                    v.release (0.06f);
            if (st.parts[(size_t) p].mode == ModeStrumOnce) strumOnce (p, vel, offset);
            else if (fresh && st.parts[(size_t) p].mode == ModePattern && ! st.master.internalPlay && ! running) { /* o padrão começa no próximo bloco */ }
            if (st.parts[(size_t) p].noises > 0.01f && old.valid) fretNoise (p, offset);
        }
    }

    void computeVoicing (int p)
    {
        const auto& ps = st.parts[(size_t) p];
        VoicingRequest rq; rq.chord = currentChord; rq.position = ps.position; rq.capo = ps.capo;
        // com capotraste, o acorde soa como escrito: a forma é calculada no tom abaixo
        if (ps.capo > 0) { rq.chord.root = pc (currentChord.root - ps.capo); if (rq.chord.bass >= 0) rq.chord.bass = pc (rq.chord.bass - ps.capo); }
        Voicing v = cache.get (ps.instrument, ps.tuning, rq);
        voicing[(size_t) p] = v;
        for (int i = 0; i < kMaxCourses; ++i) frets[(size_t) p][(size_t) i].store (i < v.numCourses ? v.fret[(size_t) i] : -2);
    }

    StepContext ctx (int p) const
    {
        StepContext c; const auto& ps = st.parts[(size_t) p];
        c.inst = &instrument (ps.instrument); c.tuning = &tuningOf (ps.instrument, ps.tuning);
        c.voicing = &voicing[(size_t) p]; c.capo = ps.capo; c.chord = currentChord;
        return c;
    }

    void fireStep (int p, int step, int offset, float secPerBeat)
    {
        const auto& ps = st.parts[(size_t) p];
        const auto c = ctx (p);
        std::array<PluckEvent, 48> ev {};
        const float strum = c.inst->strumMs * std::clamp (secPerBeat / 0.6f, 0.6f, 1.3f);
        const int k = generateStep (ps.pattern, step, c, strum, ev);
        const float hum = ps.pattern.humanize;
        const float tJit = (rng.bi() * 9.0f + rng.bi() * 4.0f) * hum;          // ms
        const float vJit = 1.0f + rng.bi() * 0.12f * hum;
        for (int i = 0; i < k; ++i) schedule (p, ev[(size_t) i], offset, tJit + rng.bi() * 1.5f * hum, vJit);
    }

    void strumOnce (int p, float vel, int offset)
    {
        const auto c = ctx (p);
        Pattern one; one.numSteps = 1; one.stepsPerBeat = 1; one.grid[RDown][0] = (uint8_t) std::clamp ((int) (vel * 127), 20, 127);
        one.humanize = 0.2f; one.mute = st.parts[(size_t) p].pattern.mute;
        std::array<PluckEvent, 48> ev {};
        const int k = generateStep (one, 0, c, c.inst->strumMs * 1.2f, ev);
        for (int i = 0; i < k; ++i) schedule (p, ev[(size_t) i], offset, 0.0f, 1.0f);
    }

    void fretNoise (int p, int offset)
    {
        // chiado de traste ao trocar de acorde (só ordens graves de corda de aço/nylon enrolada)
        const auto& ps = st.parts[(size_t) p];
        if (rng.uni() > 0.45f) return;
        PluckEvent e; e.course = -2; e.vel = 0.25f * ps.noises; e.kind = KTap;
        schedule (p, e, offset, -12.0f, 1.0f);
    }

    void schedule (int p, const PluckEvent& e, int offset, float extraMs, float velMul)
    {
        const auto& ps = st.parts[(size_t) p];
        const auto& d = instrument (ps.instrument);
        const auto& tun = tuningOf (ps.instrument, ps.tuning);
        const auto& vc = voicing[(size_t) p];
        const uint64_t base = sampleClock + (uint64_t) std::max (0, offset);
        auto push = [&] (float ms, int course, int note, float vel, float gain, float pan, float cents, uint8_t kind)
        {
            if (pendingCount >= (int) pending.size()) return;
            const double dt = std::max (0.0, (double) ms) * 0.001 * sr;
            pending[(size_t) pendingCount++] = { base + (uint64_t) dt, p, course, note, std::clamp (vel * velMul, 0.02f, 1.0f), e.mute, gain, pan, cents, kind, e.up };
        };
        const float ms = std::max (0.0f, e.delayMs + extraMs + 3.0f);
        if (e.kind == KTap) { push (ms, e.course, 60, e.vel, e.course == -2 ? 0.35f : 1.0f, 0.0f, 0.0f, KTap); return; }
        if (e.course < 0 || e.course >= vc.numCourses) return;
        const auto& co = tun.courses[(size_t) e.course];
        const int fret = std::max (0, vc.fret[(size_t) e.course]);
        const int n = (int) tun.courses.size();
        const float spread = n > 1 ? ((float) e.course / (float) (n - 1) - 0.5f) : 0.0f;
        const float pan = spread * 0.7f;
        const int note = co.note + ps.capo + fret;
        push (ms, e.course, note, e.vel, 1.0f, pan, rng.bi() * 2.0f, e.kind);
        if (co.pair >= 0)
        {
            const int pn = co.pair + ps.capo + fret;
            const bool octave = pn != note;
            float g = octave ? (d.twelveOctaveGain > 0 ? d.twelveOctaveGain : 0.75f) : 0.82f;
            if (d.source == Source::Modeled) g = d.model.pairGain * (octave ? 0.8f : 1.0f);
            const float cents = (rng.uni() > 0.5f ? 1.0f : -1.0f) * (d.source == Source::Modeled ? d.model.pairDetuneCents : 4.0f);
            // a corda mais fina do par é atingida primeiro na batida para baixo
            const float pairMs = e.up ? ms - 2.5f : ms + 3.5f;
            push (std::max (0.0f, pairMs), e.course, pn, e.vel * 0.95f, g, pan + 0.04f, cents, e.kind);
        }
    }

    void dispatchPending (int n)
    {
        const uint64_t end = sampleClock + (uint64_t) n;
        int k = 0;
        for (int i = 0; i < pendingCount; ++i)
        {
            const auto& e = pending[(size_t) i];
            if (e.time < end) startVoice (e, (int) (e.time > sampleClock ? e.time - sampleClock : 0));
            else pending[(size_t) k++] = e;
        }
        pendingCount = k;
    }

    void startVoice (const Pending& e, int offset)
    {
        const auto& ps = st.parts[(size_t) e.part];
        const auto& d = instrument (ps.instrument);
        // corda monofônica: o novo toque abafa o anterior na mesma ordem (o par soa junto)
        if (e.kind != KTap)
            for (auto& v : voices)
                if (v.type != StringVoice::Off && v.part == e.part && v.course == e.course && ! v.releasing
                    && v.startTime + (uint64_t) (0.008 * sr) < e.time)
                    v.kill (sr);
        auto& v = allocate();
        VoiceStart s; s.part = e.part; s.course = e.course; s.note = e.note; s.vel = e.vel; s.mute = e.mute; s.gain = e.gain * std::pow (10.0f, d.gainDb / 20.0f);
        s.pan = e.pan * std::clamp (ps.width, 0.0f, 1.0f); s.cents = e.cents; s.kind = e.kind; s.rr = rrCounter++;
        s.decayScale = d.decayScale; s.release = d.release; s.upStroke = e.up;
        const Bank* b = bank.load();
        if (e.kind == KTap)
        {
            v.startTap (s, b ? b->bodyTap : nullptr, sr);
        }
        else if (const auto* set = (b && d.source != Source::Modeled) ? b->set (d.sampleSet) : nullptr)
        {
            const Zone* z = set->find (e.note, s.rr);
            if (! z) { v.type = StringVoice::Off; return; }
            v.startSample (s, *z, set->gainDb, sr);
        }
        else
        {
            // instrumento modelado (ou biblioteca ausente: modelo genérico do instrumento)
            ModelParams m = d.model;
            if (d.source != Source::Modeled)
            {
                m.t60Low = 5.0f; m.t60High = 1.4f; m.brightness = d.id[0] == 'n' || d.id[0] == 'f' ? 0.45f : 0.7f;
                m.bodyHz = { 105, 210, 390, 780, 1500, 2900 };
            }
            v.startModel (s, m, sr, rng);
        }
        v.delay = offset;
        v.startTime = e.time;
        v.age = ++ageCounter;
        v.part = e.part; v.course = e.course;
    }

    StringVoice& allocate()
    {
        for (auto& v : voices) if (v.type == StringVoice::Off) return v;
        StringVoice* best = &voices[0];
        for (auto& v : voices) if ((v.releasing && ! best->releasing) || (v.releasing == best->releasing && v.age < best->age)) best = &v;
        best->type = StringVoice::Off;
        return *best;
    }

    void playKey (int p, int note, float vel, int offset)
    {
        const auto& ps = st.parts[(size_t) p];
        const auto& tun = tuningOf (ps.instrument, ps.tuning);
        const int target = note + 12 * ps.octave;
        // escolhe a ordem: a mais aguda cuja corda solta ≤ nota (como um violonista), dentro do braço,
        // evitando ordens já ocupadas por outra tecla segurada
        int best = 0, bestOpen = -1000;
        for (int pass = 0; pass < 2 && bestOpen < -999; ++pass)
            for (int c = 0; c < (int) tun.courses.size(); ++c)
            {
                const auto& co = tun.courses[(size_t) c];
                if (co.drone) continue;
                bool busy = false;
                if (pass == 0) for (int k = 0; k < 128; ++k) if (keyCourse[(size_t) p][(size_t) k] == c && k != note) { busy = true; break; }
                if (busy) continue;
                const int open = co.note + ps.capo;
                if (open <= target && target - open <= instrument (ps.instrument).numFrets && open > bestOpen) { bestOpen = open; best = c; }
            }
        if (bestOpen < -999) best = 0;
        const int fret = std::max (0, target - (tun.courses[(size_t) best].note + ps.capo));
        // monta uma "digitação" temporária para o agendador
        Voicing& vc = voicing[(size_t) p];
        if (! vc.valid || vc.numCourses != (int) tun.courses.size()) { vc.numCourses = (int) tun.courses.size(); vc.valid = true; for (auto& f : vc.fret) f = -1; }
        vc.fret[(size_t) best] = fret;
        PluckEvent e; e.course = best; e.vel = vel; e.mute = 0;
        keyCourse[(size_t) p][(size_t) note] = best;
        schedule (p, e, offset, -3.0f, 1.0f);
        frets[(size_t) p][(size_t) best].store (fret);
    }
    void releaseKey (int note)
    {
        for (int p = 0; p < kParts; ++p)
        {
            if (st.parts[(size_t) p].mode != ModeKeys) continue;
            const int c = keyCourse[(size_t) p][(size_t) note];
            if (c < 0) continue;
            keyCourse[(size_t) p][(size_t) note] = -1;
            const float rel = instrument (st.parts[(size_t) p].instrument).release;
            for (auto& v : voices) if (v.part == p && v.course == c && v.type != StringVoice::Off) v.release (std::max (0.08f, rel));
        }
    }

    static float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 0.85f) return x;
        const float y = 0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f);
        return x < 0 ? -y : y;
    }

    double sr = 48000;
    EngineState st {};
    bool configured = false;
    std::vector<Pattern> patterns;
    std::atomic<const Bank*> bank { nullptr };
    std::array<StringVoice, kVoices> voices;
    std::array<std::vector<float>, kParts> partL, partR;
    std::vector<float> sendL, sendR, wetL, wetR;
    std::array<PartFx, kParts> partFx {};
    HallReverb reverb;
    VoicingCache cache;
    std::array<Voicing, kParts> voicing {};
    std::array<std::array<std::atomic<int>, kMaxCourses>, kParts> frets {};
    std::array<std::array<int, 128>, kParts> keyCourse = [] { std::array<std::array<int, 128>, kParts> a {}; for (auto& r : a) r.fill (-1); return a; }();
    std::array<Pending, 1024> pending {};
    int pendingCount = 0;
    uint64_t sampleClock = 0, ageCounter = 0;
    int rrCounter = 0;
    Rng rng;

    std::array<bool, 128> held {}, keyDown {}, sustained {};
    std::array<float, 128> heldVel {};
    bool anyHeld = false, chordDirty = false, newPress = false, latched = false, sustain = false, running = false;
    float chordVel = 0.8f; int chordOffset = 0;
    Chord currentChord {};
    double internalBeat = 0;
    std::atomic<int> chordCode { -1 };
    std::array<std::atomic<int>, kParts> stepNow {};
    std::atomic<float> meter { 0 };
    std::atomic<double> beatNow { 0 };
    Chord uiChord {}; float uiChordVel = 0.8f; std::atomic<bool> uiChordPending { false }, stopRequest { false };
};
} // namespace sms
