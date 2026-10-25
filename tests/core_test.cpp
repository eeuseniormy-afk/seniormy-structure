// SeniorMy Structure — testes do núcleo (sem JUCE). Compila com qualquer C++17:
//   c++ -std=c++17 -O2 -I Source tests/core_test.cpp -o core_test && ./core_test
#include "core/Engine.h"
#include "core/SynthBank.h"
#include "core/Presets.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace sms;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("FALHOU: %s — ", #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static float midiHz (int n) { return 440.0f * std::pow (2.0f, (float) (n - 69) / 12.0f); }

// "piano" sintético: parciais com decaimento, para testar a análise em material polifônico
static void addNote (std::vector<float>& buf, double sr, double start, double dur, int note, float amp = 0.2f)
{
    const int s0 = (int) (start * sr), n = (int) (dur * sr);
    const float f = midiHz (note);
    for (int i = 0; i < n && s0 + i < (int) buf.size(); ++i)
    {
        const float t = (float) i / (float) sr;
        float v = 0;
        for (int k = 1; k <= 8; ++k)
        {
            if (f * k > sr * 0.45) break;
            v += std::sin (2.0f * 3.14159265f * f * (float) k * t) / (float) (k * k * 0.7f + 0.3f) * std::exp (-t * (0.8f + 0.4f * k));
        }
        const float a = std::min (1.0f, t / 0.003f);
        buf[(size_t) (s0 + i)] += v * amp * a;
    }
}

static Chord detectStatic (const std::vector<int>& notes, double sr = 44100.0)
{
    std::vector<float> buf ((size_t) (sr * 1.5), 0.0f);
    for (int n : notes) addNote (buf, sr, 0.0, 1.5, n);
    ChromaAnalyzer ca; ca.prepare (sr);
    std::vector<float> frame ((size_t) ca.frameSize());
    Chroma c {}, b {};
    for (int k = 0; k < 4; ++k)
    {
        const int off = (int) (0.1 * sr) + k * 2048;
        for (int i = 0; i < ca.frameSize(); ++i) frame[(size_t) i] = buf[(size_t) std::min ((int) buf.size() - 1, off + i)];
        ca.analyseFrame (frame.data());
        for (int j = 0; j < 12; ++j) { c[(size_t) j] += ca.getChroma()[(size_t) j]; b[(size_t) j] += ca.getBassChroma()[(size_t) j]; }
    }
    return ChordTemplateMatch::best (c, b, nullptr, true);
}

static void testHarmony()
{
    Key c { 0, Scale::Major };
    CHECK (c.contains (4) && ! c.contains (1), "C maior");
    Key a { 9, Scale::HarmonicMinor };
    CHECK (a.contains (8) && ! a.contains (7), "A menor harmônica tem G#");
    Key d { 2, Scale::Dorian };
    CHECK (d.contains (11) && ! d.contains (10), "D dórico tem B natural");
    Chord g7; g7.root = 7; g7.quality = Quality::Dom7;
    CHECK (g7.name() == "G7", "nome %s", g7.name().c_str());
    Chord fs; fs.root = 5; fs.quality = Quality::Major; fs.bass = 9;
    CHECK (fs.name() == "F/A", "nome %s", fs.name().c_str());
    auto pcs = chordPitchClasses ({ 0, Quality::Major }, c, 2);
    CHECK (pcs.size() == 5, "C com 7ª e 9ª diatônicas: %d tons", (int) pcs.size());
    CHECK (std::find (pcs.begin(), pcs.end(), 11) != pcs.end(), "C maj7 (B) em C maior");
    Key bb { 10, Scale::Major };
    CHECK (bb.usesFlats(), "Bb usa bemóis");
    Chord e; e.root = 4; e.quality = Quality::Major;
    Chord conf = conformToKey (e, c);
    CHECK (conf.quality == Quality::Minor, "E → Em em C maior");
    bool held[128] = {};
    held[48] = held[52] = held[55] = held[58] = true;   // C E G Bb
    Chord m = chordFromNotes (held, &c, true);
    CHECK (m.root == 0 && m.quality == Quality::Dom7, "MIDI C7: %s", m.name().c_str());
    bool h2[128] = {}; h2[45] = h2[60] = h2[64] = h2[67] = true;  // A + C E G → Am7
    Chord m2 = chordFromNotes (h2, &c, true);
    CHECK (m2.root == 9 || (m2.root == 0 && m2.bass == 9), "A-C-E-G: %s", m2.name().c_str());
}

static void testChromaDetection()
{
    struct Case { std::vector<int> notes; int root; Quality q; };
    std::vector<Case> cases = {
        { { 48, 60, 64, 67 }, 0, Quality::Major },
        { { 45, 57, 60, 64 }, 9, Quality::Minor },
        { { 41, 53, 57, 60 }, 5, Quality::Major },
        { { 43, 55, 59, 62, 65 }, 7, Quality::Dom7 },
        { { 50, 62, 65, 69 }, 2, Quality::Minor },
        { { 40, 52, 56, 59 }, 4, Quality::Major },
    };
    int ok = 0;
    for (auto& cs : cases)
    {
        const Chord ch = detectStatic (cs.notes);
        const bool good = ch.root == cs.root && (ch.quality == cs.q || (cs.q == Quality::Dom7 && ch.quality == Quality::Major));
        if (good) ++ok; else std::printf ("  (detecção: esperado %s%s, obtido %s)\n", pcName (cs.root), qualityInfo (cs.q).suffix, ch.name().c_str());
    }
    CHECK (ok >= 5, "acordes estáticos detectados: %d/6", ok);
    // inversão: C/E (baixo E)
    const Chord inv = detectStatic ({ 40, 60, 64, 67 });
    CHECK (inv.root == 0 && inv.bass == 4, "C/E: %s", inv.name().c_str());
    // 48 kHz e 96 kHz
    CHECK (detectStatic ({ 45, 57, 60, 64 }, 48000.0).root == 9, "Am a 48 kHz");
    CHECK (detectStatic ({ 45, 57, 60, 64 }, 96000.0).root == 9, "Am a 96 kHz");
}

static void testTrackerProgression()
{
    const double sr = 44100;
    const std::vector<std::vector<int>> prog = { { 48, 60, 64, 67 }, { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 43, 55, 59, 62 } };
    std::vector<float> buf ((size_t) (sr * 8.5), 0.0f);
    for (int rep = 0; rep < 2; ++rep)
        for (size_t c = 0; c < prog.size(); ++c)
            for (int n : prog[c]) addNote (buf, sr, rep * 4.0 + (double) c, 1.0, n, 0.18f);
    ChromaAnalyzer ca; ca.prepare (sr);
    ChordTracker tr; tr.reset();
    TrackerSettings ts; ts.sensitivity = 0.7f;
    std::vector<std::string> seq;
    for (size_t pos = 0; pos + 256 <= buf.size(); pos += 256)
    {
        if (ca.push (buf.data() + pos, 256))
            if (tr.update (ca.getChroma(), ca.getBassChroma(), std::min (1.0f, ca.getRms() * 4.0f), ts, (float) ca.getHopSize() / (float) sr))
                seq.push_back (tr.getChord().name());
    }
    std::string s;
    for (auto& x : seq) s += x + " ";
    std::printf ("  sequência detectada: %s\n  tonalidade: %s\n", s.c_str(), tr.getKey().name().c_str());
    int hits = 0;
    for (const char* want : { "C", "Am", "F", "G" }) if (s.find (std::string (want) + " ") != std::string::npos) ++hits;
    CHECK (hits >= 3, "progressão C-Am-F-G reconhecida (%d/4)", hits);
    CHECK (tr.getKey().tonic == 0 || tr.getKey().tonic == 9, "tonalidade C maior/A menor: %s", tr.getKey().name().c_str());
    CHECK (seq.size() <= 14, "sem trocas espúrias demais: %d", (int) seq.size());
}

static void testVoicer()
{
    Voicer v;
    ArrangerSettings s;
    Key k { 0, Scale::Major };
    const Chord prog[] = { { 0, Quality::Major }, { 9, Quality::Minor }, { 5, Quality::Major }, { 7, Quality::Dom7 }, { 0, Quality::Major } };
    std::vector<int> prev;
    int totalMove = 0;
    for (auto& c : prog)
    {
        auto vo = v.voice (c, k, s, 5);
        CHECK ((int) vo.size() == 5, "5 vozes");
        bool hasThird = false;
        for (int n : vo) if (pc (n) == c.third()) hasThird = true;
        CHECK (hasThird, "voicing de %s tem a terça", c.name().c_str());
        CHECK (std::is_sorted (vo.begin(), vo.end()), "sem cruzamento");
        CHECK (vo.front() >= 28 && vo.front() <= 52, "baixo no registro grave (%d)", vo.front());
        if (! prev.empty()) for (size_t i = 1; i < vo.size(); ++i) totalMove += std::abs (vo[i] - prev[i]);
        prev = vo;
    }
    std::printf ("  movimento total das vozes superiores em 4 trocas: %d semitons\n", totalMove);
    CHECK (totalMove <= 40, "condução suave (%d)", totalMove);
    // inversões forçadas
    s.inversion = InversionMode::First;
    Voicer v2;
    auto inv = v2.voice ({ 0, Quality::Major }, k, s, 4);
    CHECK (pc (inv.front()) == 4, "1ª inversão: baixo na terça");
}

static void runArranger (const PresetData& p, int& ons, int& offs, bool& rangeOk)
{
    Arranger a; a.prepare (48000);
    a.setArrangement (p.arrangement);
    const Chord prog[] = { { 0, Quality::Major }, { 9, Quality::Minor }, { 5, Quality::Major }, { 7, Quality::Major } };
    std::vector<NoteEvent> out;
    Arranger::Clock clk; clk.bpm = 120; clk.playing = true;
    const int block = 480;
    ons = offs = 0; rangeOk = true;
    for (int b = 0; b < 800; ++b)      // 8 s = 16 tempos
    {
        clk.ppq = b * block * 2.0 / 48000.0;
        const Chord c = prog[(int) (clk.ppq / 4.0) % 4];
        out.clear();
        a.process (block, clk, c, { 0, Scale::Major }, false, out);
        for (auto& e : out)
        {
            if (e.on) ++ons; else ++offs;
            const auto r = sectionRange (e.section);
            if (e.note < r.lo || e.note > r.hi) rangeOk = false;
        }
    }
    out.clear();
    a.allNotesOff (out);
    for (auto& e : out) { if (e.on) ++ons; else ++offs; }
}

static void testArrangerPresets()
{
    for (auto& p : factoryPresets())
    {
        int ons, offs; bool rangeOk;
        runArranger (p, ons, offs, rangeOk);
        std::printf ("  %-18s notas: %4d on / %4d off\n", p.name.c_str(), ons, offs);
        CHECK (ons > 0, "%s gera notas", p.name.c_str());
        CHECK (ons == offs, "%s: toda nota é desligada (%d/%d)", p.name.c_str(), ons, offs);
        CHECK (rangeOk, "%s: notas dentro da extensão de cada seção", p.name.c_str());
    }
}

static void testEngine()
{
    const double sr = 48000;
    const int block = 256;
    Engine e;
    e.prepare (sr, block);
    e.setBank (makeSynthBank ((float) sr));
    e.setArrangement (factoryPresets()[0].arrangement);
    e.params.mode = SourceMode::AudioAndMidi;
    e.params.reverbMix = 0.35f;
    std::vector<float> in ((size_t) (sr * 6), 0.0f);
    const std::vector<std::vector<int>> prog = { { 48, 60, 64, 67 }, { 45, 57, 60, 64 }, { 41, 57, 60, 65 } };
    for (size_t c = 0; c < prog.size(); ++c) for (int n : prog[c]) addNote (in, sr, (double) c * 1.5, 1.5, n, 0.15f);
    std::vector<float> L ((size_t) block), R ((size_t) block);
    float peak = 0; bool finite = true; int maxVoices = 0; double energy = 0;
    std::vector<int> roots;
    for (size_t pos = 0; pos + block <= in.size(); pos += block)
    {
        const float* chans[1] = { in.data() + pos };
        HostClock hc; hc.valid = true; hc.playing = true; hc.bpm = 100; hc.ppq = (double) pos / sr * 100.0 / 60.0;
        e.process (chans, 1, L.data(), R.data(), block, hc);
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) finite = false;
            peak = std::max (peak, std::max (std::fabs (L[(size_t) i]), std::fabs (R[(size_t) i])));
            energy += L[(size_t) i] * L[(size_t) i];
        }
        maxVoices = std::max (maxVoices, e.voiceCount.load());
        const int r = e.chordRoot.load();
        if (r >= 0 && (roots.empty() || roots.back() != r)) roots.push_back (r);
    }
    std::string rs; for (int r : roots) rs += std::string (pcName (r)) + " ";
    std::printf ("  motor: pico %.3f, vozes máx %d, fundamentais: %s\n", peak, maxVoices, rs.c_str());
    CHECK (finite, "saída sem NaN/Inf");
    CHECK (peak > 0.01f && peak <= 1.0f, "nível de saída razoável (%.3f)", peak);
    CHECK (maxVoices >= 4, "cobertura soando (%d vozes)", maxVoices);
    CHECK (roots.size() >= 2, "acompanha as trocas de acorde");

    // MIDI + piano interno
    Engine m; m.prepare (sr, block);
    m.setBank (makeSynthBank ((float) sr));
    m.setArrangement (factoryPresets()[2].arrangement);   // Pop Ostinato
    m.params.mode = SourceMode::Midi;
    m.params.pianoPlaysInput = true;
    m.process (nullptr, 0, L.data(), R.data(), block, {});
    m.noteOnInput (50, 0.8f); m.noteOnInput (53, 0.8f); m.noteOnInput (57, 0.8f);  // Dm
    int maxV = 0; float pk = 0;
    for (int b = 0; b < 400; ++b)
    {
        HostClock hc; hc.valid = true; hc.playing = true; hc.bpm = 120; hc.ppq = b * block / sr * 2.0;
        m.process (nullptr, 0, L.data(), R.data(), block, hc);
        maxV = std::max (maxV, m.voiceCount.load());
        for (int i = 0; i < block; ++i) pk = std::max (pk, std::fabs (L[(size_t) i]));
    }
    CHECK (m.chordRoot.load() == 2 && m.chordQuality.load() == (int) Quality::Minor, "MIDI Dm reconhecido (%d/%d)", m.chordRoot.load(), m.chordQuality.load());
    CHECK (maxV > 3 && pk > 0.01f, "ostinato soando via MIDI (vozes %d, pico %.3f)", maxV, pk);
    m.noteOffInput (50); m.noteOffInput (53); m.noteOffInput (57);
    for (int b = 0; b < 600; ++b) m.process (nullptr, 0, L.data(), R.data(), block, {});
    float tail = 0;
    for (int i = 0; i < block; ++i) tail = std::max (tail, std::fabs (L[(size_t) i]));
    CHECK (tail < 0.02f, "silêncio após soltar o acorde (%.4f)", tail);
}

static void testLoopAnalysis()
{
    const double sr = 44100, bpm = 100;
    const double beat = 60.0 / bpm;
    std::vector<float> buf ((size_t) (sr * beat * 16), 0.0f);
    const std::vector<std::vector<int>> prog = { { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 48, 60, 64, 67 }, { 43, 55, 59, 62 } };
    for (int bar = 0; bar < 4; ++bar)
        for (int b = 0; b < 4; ++b)
            for (int n : prog[(size_t) bar]) addNote (buf, sr, (bar * 4 + b) * beat, beat * 0.95, n, b == 0 ? 0.2f : 0.12f);
    auto p = analyseAudioLoop (buf, sr);
    std::string s; for (auto& c : p.chords) s += c.chord.name() + "(" + std::to_string ((int) c.lengthBeats) + ") ";
    std::printf ("  loop de áudio: %.1f BPM, %.0f tempos, tom %s: %s\n", p.bpm, p.lengthBeats, p.key.name().c_str(), s.c_str());
    CHECK (std::fabs (p.bpm - bpm) < 3.0 || std::fabs (p.bpm - bpm * 2) < 6.0, "BPM estimado %.1f", p.bpm);
    CHECK (p.chords.size() >= 3 && p.chords.size() <= 8, "acordes do loop (%d)", (int) p.chords.size());
    CHECK (! p.chords.empty() && p.chords.front().chord.root == 9, "primeiro acorde Am");

    std::vector<MidiNoteIn> notes;
    for (int bar = 0; bar < 4; ++bar) for (int n : prog[(size_t) bar]) notes.push_back ({ bar * 4.0, 4.0, n, 0.8f });
    for (int s16 = 0; s16 < 64; s16 += 2) notes.push_back ({ s16 * 0.25, 0.2, 33, 0.9f });
    auto pm = analyseMidiLoop (notes, 96);
    std::string ms; for (auto& c : pm.chords) ms += c.chord.name() + " ";
    std::printf ("  loop MIDI: %s| tom %s\n", ms.c_str(), pm.key.name().c_str());
    CHECK (pm.chords.size() == 4, "4 acordes no MIDI (%d)", (int) pm.chords.size());
    CHECK (pm.chords.size() >= 3 && pm.chords[1].chord.root == 5 && pm.chords[2].chord.root == 0, "F e C nas posições certas");
    auto lane = laneFromRhythm (pm, Spiccato, 3, "Do loop");
    int cells = 0; for (int s = 0; s < 16; ++s) cells += lane.stepHasAny (s) ? 1 : 0;
    CHECK (cells >= 4, "ostinato criado a partir do ritmo (%d passos)", cells);
    CHECK (p.chordAt (0.5).valid() && p.chordAt (100.0).valid(), "progressão em loop");
}

static void testReverbAndPiano()
{
    HallReverb rv; rv.prepare (48000); rv.set (0.8f, 0.7f, 0.4f, 20, 1);
    std::vector<float> in (48000 * 6, 0.0f), l (in.size()), r (in.size());
    in[0] = 1.0f;
    rv.process (in.data(), in.data(), l.data(), r.data(), (int) in.size());
    float early = 0, late = 0; bool fin = true;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (! std::isfinite (l[i])) fin = false;
        if (i < 48000) early = std::max (early, std::fabs (l[i])); else if (i > 5 * 48000) late = std::max (late, std::fabs (l[i]));
    }
    CHECK (fin && early > 0.001f && late < early * 0.05f, "reverb decai (cedo %.4f, tarde %.6f)", early, late);

    PianoSynth p; p.prepare (48000);
    p.noteOn (60, 0.8f);
    std::vector<float> a (4800, 0.0f), b (4800, 0.0f);
    p.render (a.data(), b.data(), 4800, 1.0f);
    float pk = 0; for (float x : a) pk = std::max (pk, std::fabs (x));
    CHECK (pk > 0.01f && pk < 1.0f, "piano interno soa (%.3f)", pk);
}

static void testSamplerLoop()
{
    auto bank = makeSynthBank (44100);
    Sampler s; s.prepare (44100); s.setBank (bank); s.syncBank();
    s.noteOn (Vla, Sustain, 62, 0.8f, 0.7f, 0, 0);
    std::vector<std::vector<float>> L (6, std::vector<float> (44100 * 8, 0.0f)), R = L;
    float* pl[6]; float* pr[6];
    for (int i = 0; i < 6; ++i) { pl[i] = L[(size_t) i].data(); pr[i] = R[(size_t) i].data(); }
    s.render (pl, pr, 0, 44100 * 8);   // 8 s > amostra de 3 s → precisa do loop
    float late = 0, maxJump = 0;
    for (int i = 44100 * 6; i < 44100 * 8; ++i)
    {
        late = std::max (late, std::fabs (L[Vla][(size_t) i]));
        maxJump = std::max (maxJump, std::fabs (L[Vla][(size_t) i] - L[Vla][(size_t) i - 1]));
    }
    CHECK (late > 0.01f, "sustain continua soando com loop (%.3f)", late);
    CHECK (maxJump < 0.2f, "sem estalo no loop (salto máx %.3f)", maxJump);
}

int main()
{
    std::printf ("SeniorMy Structure — testes do núcleo\n");
    testHarmony();
    testChromaDetection();
    testTrackerProgression();
    testVoicer();
    testArrangerPresets();
    testEngine();
    testLoopAnalysis();
    testReverbAndPiano();
    testSamplerLoop();
    std::printf ("%d verificações, %d falhas\n", checks, failures);
    std::printf (failures == 0 ? "OK\n" : "FALHOU\n");
    return failures == 0 ? 0 : 1;
}
