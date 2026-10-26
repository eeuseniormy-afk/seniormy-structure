// SeniorMy Structure — motor completo em tempo real (sem JUCE):
// entrada de áudio/MIDI → análise harmônica → arranjador → sampler orquestral + piano → mixer + reverb.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Analysis.h"
#include "Arranger.h"
#include "Sampler.h"
#include "Dsp.h"
#include "LoopAnalysis.h"
#include <atomic>
#include <array>
#include <vector>
#include <memory>
#include <mutex>

namespace sms
{
//==============================================================================
// Fila SPSC sem trava (áudio → interface).
template <typename T, int N>
class SpscQueue
{
public:
    bool push (const T& v)
    {
        const int w = write.load (std::memory_order_relaxed), nw = (w + 1) % N;
        if (nw == read.load (std::memory_order_acquire)) return false;
        buf[(size_t) w] = v;
        write.store (nw, std::memory_order_release);
        return true;
    }
    bool pop (T& v)
    {
        const int r = read.load (std::memory_order_relaxed);
        if (r == write.load (std::memory_order_acquire)) return false;
        v = buf[(size_t) r];
        read.store ((r + 1) % N, std::memory_order_release);
        return true;
    }
private:
    std::array<T, N> buf {};
    std::atomic<int> write { 0 }, read { 0 };
};

enum class SourceMode : int { Audio = 0, Midi, AudioAndMidi, Progression };

struct EngineParams
{
    SourceMode mode = SourceMode::AudioAndMidi;
    bool autoKey = true;
    int keyRoot = 0;
    int scaleChoice = -1;           // -1 = auto (maior/menor detectado)
    float sensitivity = 0.6f;
    bool allowSevenths = true;
    bool lock = false;
    bool conformToScale = true;
    ArrangerSettings arr;
    std::array<float, 6> sectionDb { 0, 0, 0, 0, 0, -3 };
    std::array<bool, kMaxLanes> laneOn { true, true, true, true, true, true, true, true };
    float attack01 = 0.4f, release01 = 0.5f;
    float reverbMix = 0.3f, reverbSize = 0.7f, reverbDecay = 0.55f;
    float pianoDb = -6.0f;
    bool pianoPlaysInput = false;   // o piano toca o MIDI de entrada/teclado
    bool pianoPlaysCover = false;   // o piano toca as notas da cobertura
    bool coverOn = true;            // cordas ligadas
    float inputMonitorDb = -100.0f; // retorno da entrada de áudio (≤ -60 = mudo)
    float masterDb = 0.0f;
    float internalBpm = 110.0f;
};

struct DisplayEvent { double ppq; uint8_t section, artic, note, velocity, lane; bool on; };

struct HostClock { double ppq = 0; double bpm = 120; bool playing = false; bool valid = false; };

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        maxBlockSize = std::max (16, maxBlock);
        analyzer.prepare (sr);
        tracker.reset();
        arranger.prepare (sr);
        sampler.prepare (sr);
        piano.prepare (sr);
        reverb.prepare (sr);
        for (auto& b : secL) b.assign ((size_t) maxBlockSize, 0.0f);
        for (auto& b : secR) b.assign ((size_t) maxBlockSize, 0.0f);
        mono.assign ((size_t) maxBlockSize, 0.0f);
        wetL.assign ((size_t) maxBlockSize, 0.0f); wetR.assign ((size_t) maxBlockSize, 0.0f);
        sendL.assign ((size_t) maxBlockSize, 0.0f); sendR.assign ((size_t) maxBlockSize, 0.0f);
        events.reserve (1024);
        pendingDispatch.reserve (1024);
        for (auto& h : held) h = false;
        silenceSamples = 0;
        lastChord = {};
        arranger.setArrangement (pendingArrangementCopy());
    }

    // --- thread de mensagens ---------------------------------------------------
    void setArrangement (const Arrangement& a)
    {
        std::lock_guard<std::mutex> g (arrLock);
        nextArrangement = a;
        arrangementDirty.store (true);
    }
    void setProgression (std::shared_ptr<const Progression> p)
    {
        std::lock_guard<std::mutex> g (progLock);
        retiredProg.clear();
        retiredProg.reserve (8);
        nextProgression = std::move (p);
        progressionDirty.store (true);
    }
    void setBank (std::shared_ptr<const InstrumentBank> b) { sampler.setBank (std::move (b)); }
    void collectGarbage() { sampler.collectGarbage(); std::unique_lock<std::mutex> g (progLock, std::try_to_lock); if (g.owns_lock()) retiredProg.clear(); }
    void requestPanic() { panicRequested.store (true); }
    void requestManualChord (const Chord& c) { manualRoot.store (c.root); manualQuality.store ((int) c.quality); manualBass.store (c.bass); manualPending.store (true); }

    // --- áudio --------------------------------------------------------------------
    void noteOnInput (int note, float vel01)
    {
        if (note < 0 || note > 127) return;
        held[note] = true; heldVel[note] = vel01; midiDirty = true;
        if (params.pianoPlaysInput) piano.noteOn (note, vel01);
    }
    void noteOffInput (int note)
    {
        if (note < 0 || note > 127) return;
        held[note] = false; midiDirty = true;
        piano.noteOff (note);
    }
    void sustainPedal (bool down) { piano.setSustain (down); }

    // input: canais de entrada (pode ser nullptr/0). out: estéreo. events MIDI já aplicados antes.
    void process (const float* const* in, int numIn, float* outL, float* outR, int num, const HostClock& host)
    {
        // aplica mudanças vindas da interface
        sampler.syncBank();
        pendingDispatch.clear();
        dispatchBlockStart = arranger.now();
        if (arrangementDirty.load())
        {
            std::unique_lock<std::mutex> g (arrLock, std::try_to_lock);
            if (g.owns_lock())
            {
                events.clear();
                arranger.allNotesOff (events);
                dispatch (events);
                arranger.setArrangement (nextArrangement);
                arrangementDirty.store (false);
            }
        }
        if (progressionDirty.load())
        {
            std::unique_lock<std::mutex> g (progLock, std::try_to_lock);
            if (g.owns_lock())
            {
                retiredProg.push_back (progression);
                progression = nextProgression;
                progressionDirty.store (false);
            }
        }
        if (panicRequested.exchange (false))
        {
            events.clear();
            arranger.allNotesOff (events);
            sampler.kill();
            piano.allOff();
            for (auto& h : held) h = false;
            tracker.reset();
        }

        const auto& P = params;
        arranger.settings = P.arr;
        for (int i = 0; i < kMaxLanes; ++i) arranger.setLaneEnabledOverride (i, P.laneOn[(size_t) i]);
        sampler.sustainAttack = 0.02f + P.attack01 * P.attack01 * 0.9f;
        sampler.sustainRelease = 0.12f + P.release01 * P.release01 * 2.6f;
        sampler.setDynamics (P.arr.dynamics);

        TrackerSettings ts;
        ts.sensitivity = P.sensitivity;
        ts.allowSevenths = P.allowSevenths && P.arr.extensions >= 1;
        ts.lock = P.lock;
        ts.autoKey = P.autoKey;
        ts.manualKey = { P.keyRoot, P.scaleChoice >= 0 ? (Scale) P.scaleChoice : Scale::Major };
        ts.scaleChoice = P.scaleChoice;
        ts.conformToScale = P.conformToScale;

        // --- análise da entrada de áudio
        float inPeak = 0;
        const bool useAudio = P.mode == SourceMode::Audio || P.mode == SourceMode::AudioAndMidi;
        if (numIn > 0 && in)
        {
            for (int i = 0; i < num; ++i)
            {
                float s = 0;
                for (int c = 0; c < numIn; ++c) if (in[c]) s += in[c][i];
                s /= (float) numIn;
                mono[(size_t) i] = s;
                inPeak = std::max (inPeak, std::fabs (s));
            }
        }
        else std::fill (mono.begin(), mono.begin() + num, 0.0f);
        inputLevel.store (inPeak);

        int heldCount = 0;
        for (bool h : held) heldCount += h ? 1 : 0;
        const bool midiActive = heldCount > 0 && (P.mode == SourceMode::Midi || P.mode == SourceMode::AudioAndMidi);

        bool audioActive = false;
        if (useAudio)
        {
            if (analyzer.push (mono.data(), num))
            {
                const float energy = std::min (1.0f, analyzer.getRms() * 4.0f);
                const float frameSec = (float) analyzer.getHopSize() / (float) sr;
                if (! midiActive) tracker.update (analyzer.getChroma(), analyzer.getBassChroma(), energy, ts, frameSec);
                for (int i = 0; i < 12; ++i) chromaView[(size_t) i].store (analyzer.getChroma()[(size_t) i]);
                lastEnergy = energy;
            }
            const float gate = 0.004f + 0.05f * (1.0f - P.sensitivity) * (1.0f - P.sensitivity);
            audioActive = lastEnergy > gate;
        }

        if (midiActive && midiDirty)
        {
            const Key k = tracker.getKey();
            Chord c = chordFromNotes (held, &k, ts.allowSevenths);
            if (P.conformToScale) c = conformToKey (c, k);
            if (! P.lock && c.valid()) tracker.setChord (c);
            Chroma ch, bc; int lowest;
            chromaFromNotes (held, heldVel, ch, bc, lowest);
            tracker.feedLongTerm (ch, 1.0f);
            tracker.update (ch, bc, 0.0f, ts, 0.0f);   // atualiza a tonalidade (sem decidir acorde)
            for (int i = 0; i < 12; ++i) chromaView[(size_t) i].store (ch[(size_t) i]);
        }
        midiDirty = false;

        if (manualPending.exchange (false))
        {
            Chord c; c.root = manualRoot.load(); c.quality = (Quality) manualQuality.load(); c.bass = manualBass.load(); c.confidence = 1.0f;
            tracker.setChord (c);
        }

        // --- relógio
        Arranger::Clock clk;
        clk.bpm = (host.valid && host.bpm > 1) ? host.bpm : P.internalBpm;
        clk.playing = host.valid && host.playing;
        clk.ppq = host.ppq;

        // --- acorde atual
        Chord chord = tracker.getChord();
        Key key = tracker.getKey();
        bool silent;
        if (P.mode == SourceMode::Progression && progression && ! progression->empty())
        {
            const double pos = clk.playing ? clk.ppq : freeProgPpq;
            chord = progression->chordAt (pos);
            if (! P.autoKey) key = ts.manualKey;
            else key = applyScaleChoice (progression->key, P.scaleChoice);
            silent = false;
            freeProgPpq = pos + num * clk.bpm / 60.0 / sr;
        }
        else
        {
            const bool active = audioActive || midiActive || manualHold;
            if (active) silenceSamples = 0; else silenceSamples += num;
            const double holdSec = 0.4 + 2.2 * (1.0 - P.sensitivity);
            silent = ! P.lock && silenceSamples > (int64_t) (holdSec * sr);
            if (P.mode == SourceMode::Midi && heldCount == 0 && ! P.lock) silent = silenceSamples > (int64_t) (0.15 * sr);
        }
        if (! P.coverOn) silent = true;

        // --- arranjo
        events.clear();
        arranger.process (num, clk, chord, key, silent, events);
        dispatch (events);

        // estado para a interface
        if (chord != lastChord) lastChord = chord;
        chordRoot.store (silent ? -1 : chord.root);
        chordQuality.store ((int) chord.quality);
        chordBass.store (chord.bass);
        chordConfidence.store (chord.confidence);
        keyTonic.store (key.tonic);
        keyScale.store ((int) key.scale);
        displayPpq.store (clk.playing ? clk.ppq : freeDisplayPpq);
        freeDisplayPpq += num * clk.bpm / 60.0 / sr;

        // --- render
        for (int s = 0; s < 6; ++s)
        {
            std::fill (secL[(size_t) s].begin(), secL[(size_t) s].begin() + num, 0.0f);
            std::fill (secR[(size_t) s].begin(), secR[(size_t) s].begin() + num, 0.0f);
        }
        renderSampler (0, num);

        std::fill (outL, outL + num, 0.0f);
        std::fill (outR, outR + num, 0.0f);
        for (int s = 0; s < 6; ++s)
        {
            const float g = P.sectionDb[(size_t) s] <= -59.0f ? 0.0f : std::pow (10.0f, P.sectionDb[(size_t) s] / 20.0f);
            float pk = 0;
            for (int i = 0; i < num; ++i)
            {
                const float l = secL[(size_t) s][(size_t) i] * g, r = secR[(size_t) s][(size_t) i] * g;
                outL[i] += l; outR[i] += r;
                pk = std::max (pk, std::max (std::fabs (l), std::fabs (r)));
            }
            sectionPeak[(size_t) s].store (std::max (pk, sectionPeak[(size_t) s].load() * 0.92f));
        }

        // piano (seco, com um pouco de sala)
        std::fill (sendL.begin(), sendL.begin() + num, 0.0f);
        std::fill (sendR.begin(), sendR.begin() + num, 0.0f);
        const float pg = P.pianoDb <= -59.0f ? 0.0f : std::pow (10.0f, P.pianoDb / 20.0f);
        if (pg > 0) piano.render (sendL.data(), sendR.data(), num, pg);

        // reverb: cordas com mix total, piano com 40% da mesma sala
        reverb.set (P.reverbSize, P.reverbDecay, 0.45f, 12.0f + 30.0f * P.reverbSize, 1.0f);
        for (int i = 0; i < num; ++i) { wetL[(size_t) i] = outL[i] + sendL[(size_t) i] * 0.4f; wetR[(size_t) i] = outR[i] + sendR[(size_t) i] * 0.4f; }
        reverb.process (wetL.data(), wetR.data(), wetL.data(), wetR.data(), num);
        const float wet = P.reverbMix, dry = 1.0f - 0.45f * P.reverbMix;
        const float mg = 0.75f * std::pow (10.0f, P.masterDb / 20.0f);   // −2,5 dB de folga antes do limitador suave
        const float mon = P.inputMonitorDb <= -59.0f ? 0.0f : std::pow (10.0f, P.inputMonitorDb / 20.0f);
        float peak = 0;
        for (int i = 0; i < num; ++i)
        {
            float l = (outL[i] * dry + wetL[(size_t) i] * wet * 1.4f + sendL[(size_t) i]) * mg + mono[(size_t) i] * mon;
            float r = (outR[i] * dry + wetR[(size_t) i] * wet * 1.4f + sendR[(size_t) i]) * mg + mono[(size_t) i] * mon;
            l = SoftClip::process (l); r = SoftClip::process (r);
            outL[i] = l; outR[i] = r;
            peak = std::max (peak, std::max (std::fabs (l), std::fabs (r)));
        }
        outputPeak.store (std::max (peak, outputPeak.load() * 0.9f));
        voiceCount.store (sampler.activeVoices());
    }

    // --- estado para a interface (leitura atômica) -------------------------------------
    std::atomic<int> chordRoot { -1 }, chordQuality { 0 }, chordBass { -1 }, keyTonic { 0 }, keyScale { 0 }, voiceCount { 0 };
    std::atomic<float> chordConfidence { 0 }, inputLevel { 0 }, outputPeak { 0 };
    std::array<std::atomic<float>, 12> chromaView {};
    std::array<std::atomic<float>, 6> sectionPeak {};
    std::atomic<double> displayPpq { 0 };
    SpscQueue<DisplayEvent, 2048> displayQueue;

    EngineParams params;          // escrito pelo processador antes de cada bloco (thread de áudio)
    bool manualHold = false;      // acorde manual mantém a cobertura soando
    Sampler& getSampler() { return sampler; }
    int getMaxBlockSize() const { return maxBlockSize; }

private:
    Arrangement pendingArrangementCopy() { std::lock_guard<std::mutex> g (arrLock); return nextArrangement; }

    // eventos são aplicados em ordem; o render é dividido nos instantes dos eventos
    void dispatch (const std::vector<NoteEvent>& ev) { for (auto& e : ev) pendingDispatch.push_back (e); }

    void renderSampler (int start, int num)
    {
        float* L[6]; float* R[6];
        int pos = start;
        size_t ei = 0;
        const auto& P = params;
        while (pos < start + num)
        {
            // aplica eventos devidos até 'pos'
            while (ei < pendingDispatch.size())
            {
                const auto& e = pendingDispatch[ei];
                const int off = (int) std::clamp<int64_t> (e.time - dispatchBlockStart, 0, num - 1);
                if (off > pos) break;
                apply (e, P);
                ++ei;
            }
            int next = start + num;
            if (ei < pendingDispatch.size())
                next = std::min (next, (int) std::clamp<int64_t> (pendingDispatch[ei].time - dispatchBlockStart, pos + 1, num));
            for (int s = 0; s < 6; ++s) { L[s] = secL[(size_t) s].data(); R[s] = secR[(size_t) s].data(); }
            sampler.render (L, R, pos, next - pos);
            pos = next;
        }
        for (; ei < pendingDispatch.size(); ++ei) apply (pendingDispatch[ei], P);
        pendingDispatch.clear();
    }

    void apply (const NoteEvent& e, const EngineParams& P)
    {
        if (e.on)
        {
            sampler.noteOn (e.section, e.artic, e.note, (float) e.velocity / 127.0f, P.arr.dynamics, e.time, e.lane);
            if (P.pianoPlaysCover && e.artic != Pizzicato) piano.noteOn (e.note, 0.5f * (float) e.velocity / 127.0f);
        }
        else
        {
            sampler.noteOff (e.section, e.artic, e.note, e.lane);
            if (P.pianoPlaysCover && ! held[e.note]) piano.noteOff (e.note);
        }
        displayQueue.push ({ displayPpq.load(), e.section, e.artic, e.note, e.velocity, e.lane, e.on });
    }

    double sr = 44100;
    int maxBlockSize = 512;
    ChromaAnalyzer analyzer;
    ChordTracker tracker;
    Arranger arranger;
    Sampler sampler;
    PianoSynth piano;
    HallReverb reverb;
    std::array<std::vector<float>, 6> secL, secR;
    std::vector<float> mono, wetL, wetR, sendL, sendR;
    std::vector<NoteEvent> events, pendingDispatch;
    int64_t dispatchBlockStart = 0;
    bool held[128] {};
    float heldVel[128] {};
    bool midiDirty = false;
    float lastEnergy = 0;
    int64_t silenceSamples = 0;
    Chord lastChord {};
    double freeProgPpq = 0, freeDisplayPpq = 0;

    std::mutex arrLock, progLock;
    Arrangement nextArrangement;
    std::atomic<bool> arrangementDirty { false }, progressionDirty { false }, panicRequested { false }, manualPending { false };
    std::atomic<int> manualRoot { -1 }, manualQuality { 0 }, manualBass { -1 };
    std::shared_ptr<const Progression> progression, nextProgression;
    std::vector<std::shared_ptr<const Progression>> retiredProg;
};
} // namespace sms
