// SeniorMy Acoustic Guitars — testes do núcleo (sem JUCE): digitações, campo harmônico, padrões,
// motor (render offline com modelos), estabilidade numérica e desempenho.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#include "core/Engine.h"
#include "core/Presets.h"
#include <cstdio>
#include <chrono>
#include <memory>

using namespace sms;
static int fails = 0;
#define CHECK(c, msg) do { if (! (c)) { std::printf ("FALHA: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++fails; } else std::printf ("ok: %s\n", msg); } while (0)

static std::string shape (int inst, int tun, int root, Quality q, int pos = 0)
{
    VoicingRequest rq; rq.chord.root = root; rq.chord.quality = q; rq.position = pos;
    auto v = findVoicing (instrument (inst), tuningOf (inst, tun), rq);
    std::string s;
    for (int c = 0; c < v.numCourses; ++c) { if (! s.empty()) s += ' '; s += v.fret[(size_t) c] < 0 ? "x" : std::to_string (v.fret[(size_t) c]); }
    return s;
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    // ---- digitações idiomáticas
    CHECK (shape (Steel, 0, 0, Quality::Major) == "x 3 2 0 1 0", "violão: Dó aberto");
    CHECK (shape (Steel, 0, 7, Quality::Major) == "3 2 0 0 0 3", "violão: Sol aberto");
    CHECK (shape (Steel, 0, 2, Quality::Major) == "x x 0 2 3 2", "violão: Ré aberto");
    CHECK (shape (Steel, 0, 9, Quality::Minor) == "x 0 2 2 1 0", "violão: Lá menor");
    CHECK (shape (Steel, 0, 5, Quality::Major) == "1 3 3 2 1 1", "violão: Fá com pestana");
    CHECK (shape (Steel, 0, 11, Quality::Minor) == "x 2 4 4 3 2", "violão: Si menor com pestana");
    CHECK (shape (Steel, 0, 7, Quality::Dom7) == "3 2 0 0 0 1", "violão: Sol com sétima");
    CHECK (shape (Cavaquinho, 0, 2, Quality::Major) == "0 2 3 4", "cavaquinho: Ré");
    CHECK (shape (Cavaquinho, 0, 7, Quality::Major) == "0 0 0 0", "cavaquinho: Sol solto");
    CHECK (shape (Ukulele, 0, 0, Quality::Major) == "0 0 0 3", "ukulele: Dó");
    CHECK (shape (Ukulele, 0, 9, Quality::Minor) == "2 0 0 0", "ukulele: Lá menor");
    CHECK (shape (Viola, 0, 4, Quality::Major) == "0 0 0 0 0", "viola caipira: Mi no cebolão");
    CHECK (shape (Banjo, 0, 7, Quality::Major) == "0 0 0 0 0", "banjo: Sol aberto");
    CHECK (shape (Mandolin, 0, 7, Quality::Major) == "0 0 2 3", "bandolim: Sol");
    {
        auto v = findVoicing (instrument (Steel), tuningOf (Steel, 0), { { 7, Quality::Major, -1, 0 }, 2, 0 });
        CHECK (v.valid && v.lowestFret() >= 3, "violão: Sol na 7ª posição usa casas altas");
    }
    // todas as combinações de instrumento × afinação × 12 fundamentais × qualidades comuns têm digitação
    int missing = 0, total = 0;
    const Quality qs[] = { Quality::Major, Quality::Minor, Quality::Dom7, Quality::Min7, Quality::Maj7, Quality::Sus4, Quality::Dim };
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < NumInstruments; ++i)
        for (int t = 0; t < (int) instrument (i).tunings.size(); ++t)
            for (int r = 0; r < 12; ++r)
                for (auto q : qs)
                {
                    VoicingRequest rq; rq.chord.root = r; rq.chord.quality = q;
                    ++total; if (! findVoicing (instrument (i), tuningOf (i, t), rq).valid) { ++missing; if (missing < 6) std::printf ("  sem digitação: %s afin %d raiz %d q %d\n", instrument (i).id, t, r, (int) q); }
                }
    const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("  %d digitações em %.1f ms (%.3f ms cada), %d sem solução\n", total, ms, ms / total, missing);
    CHECK (missing * 50 < total, "digitações encontradas para quase todos os acordes (≥ 98%)");
    CHECK (ms / total < 2.0, "busca de digitação rápida (< 2 ms)");

    // ---- campo harmônico
    Key C; C.tonic = 0; C.scale = Scale::Major;
    CHECK (diatonicChord (C, 1, false).quality == Quality::Minor && diatonicChord (C, 1, false).root == 2, "campo de Dó: II = Rém");
    CHECK (diatonicChord (C, 4, true).quality == Quality::Dom7, "campo de Dó: V7");
    CHECK (diatonicChord (C, 6, true).quality == Quality::HalfDim7, "campo de Dó: VIIm7(b5)");
    Key Am; Am.tonic = 9; Am.scale = Scale::HarmonicMinor;
    CHECK (diatonicChord (Am, 4, false).quality == Quality::Major, "Lá menor harmônica: V maior");

    // ---- padrões
    const auto pats = factoryPatterns();
    CHECK (pats.size() >= 20, "padrões de fábrica (≥ 20)");
    {
        StepContext c; c.inst = &instrument (Steel); c.tuning = &tuningOf (Steel, 0);
        Voicing v = findVoicing (*c.inst, *c.tuning, { { 0, Quality::Major, -1, 0 }, 0, 0 });
        c.voicing = &v; c.chord.root = 0;
        std::array<PluckEvent, 48> ev {};
        const int k = generateStep (pats[0], 0, c, 30.0f, ev);   // baixo + batida para baixo
        CHECK (k == 6, "folk passo 1: baixo + 5 cordas");
        CHECK (ev[0].course == 1, "baixo do Dó na 5ª corda");
        bool ordered = true; for (int i = 2; i < k; ++i) ordered &= ev[(size_t) i].delayMs >= ev[(size_t) (i - 1)].delayMs;
        CHECK (ordered && ev[(size_t) (k - 1)].delayMs > 20.0f, "batida espalha as cordas no tempo");
    }
    // ---- presets
    const auto presets = factoryPresets();
    CHECK (presets.size() >= 14, "presets de fábrica (≥ 14)");
    bool allInst[NumInstruments] = {};
    for (auto& p : presets) for (auto& ps : p.state.parts) if (ps.mode != ModeOff) allInst[ps.instrument] = true;
    bool cover = true; for (bool b : allInst) cover &= b;
    CHECK (cover, "todos os instrumentos aparecem em presets");

    // ---- motor: render offline (sem biblioteca → modelos)
    for (int pi = 0; pi < (int) presets.size(); ++pi)
    {
        auto eng = std::make_unique<Engine>();
        const double sr = 48000; const int bs = 256;
        eng->prepare (sr, bs);
        auto s = presets[(size_t) pi].state; s.master.internalPlay = false;
        eng->setState (s);
        std::vector<float> L (bs), R (bs);
        Transport tr;
        double sum = 0; float peak = 0; bool finite = true;
        const int blocks = (int) (4.0 * sr / bs);
        auto t1 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            if (b == 2) { eng->noteOn (48, 0.8f, 10); eng->noteOn (52, 0.8f, 10); eng->noteOn (55, 0.8f, 12); eng->noteOn (72, 0.7f, 20); }
            if (b == blocks / 2) { eng->noteOff (52, 0); eng->noteOn (53, 0.8f, 0); eng->noteOn (57, 0.8f, 0); eng->noteOff (55, 0); }
            eng->process (L.data(), R.data(), bs, tr);
            for (int i = 0; i < bs; ++i) { finite &= std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]); sum += L[(size_t) i] * L[(size_t) i]; peak = std::max (peak, std::abs (L[(size_t) i])); }
        }
        const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count() / 4.0;
        const double rms = std::sqrt (sum / (blocks * bs));
        std::printf ("  preset %-34s rms %.4f pico %.3f cpu %.1f%% vozes %d\n", presets[(size_t) pi].name.c_str(), rms, peak, cpu * 100.0, eng->activeVoices());
        if (! finite || rms < 0.002 || peak > 1.0 || cpu > 0.5) { std::printf ("FALHA: preset %s\n", presets[(size_t) pi].name.c_str()); ++fails; }
    }
    // progressão com relógio interno
    {
        auto eng = std::make_unique<Engine>(); eng->prepare (44100, 512);
        auto s = presets[0].state; s.harmony.progressionOn = true; s.master.internalPlay = true; s.master.bpm = 120;
        s.harmony.numProg = 2; s.harmony.prog[0] = { 0, 0, -1, 4 }; s.harmony.prog[1] = { 7, 0, -1, 4 };
        eng->setState (s);
        std::vector<float> L (512), R (512); Transport tr; int changes = 0, last = -1;
        for (int b = 0; b < (int) (8.5 * 44100 / 512); ++b) { eng->process (L.data(), R.data(), 512, tr); const int r = eng->getChord().root; if (r != last) { ++changes; last = r; } }
        CHECK (changes >= 4, "progressão troca de acorde no tempo (relógio interno)");
    }
    std::printf (fails ? "\n%d FALHA(S)\n" : "\nTODOS OS TESTES PASSARAM\n", fails);
    return fails ? 1 : 0;
}
