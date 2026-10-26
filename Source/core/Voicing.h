// SeniorMy Acoustic Guitars — digitações idiomáticas de acordes para qualquer afinação.
// Busca exaustiva em janelas do braço (cordas soltas + até 4 dedos, pestana), com pontuação que
// favorece: baixo correto, cordas soltas na 1ª posição, todas as notas do acorde, poucas cordas
// abafadas e nenhuma abafada no meio. Resultado: shapes de violão, cavaquinho, ukulele, viola
// caipira (cebolão) e banjo como um músico tocaria.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Harmony.h"
#include "Instruments.h"
#include <array>
#include <cstdint>
#include <climits>

namespace sms
{
struct Voicing
{
    std::array<int, kMaxCourses> fret {};   // -1 = abafada
    int numCourses = 0;
    bool valid = false;
    int lowestFret() const { int m = 99; for (int i = 0; i < numCourses; ++i) if (fret[(size_t) i] > 0) m = std::min (m, fret[(size_t) i]); return m == 99 ? 0 : m; }
};

// nota soando na ordem (corda principal), com capotraste
inline int courseNote (const Tuning& t, const Voicing& v, int c, int capo)
{
    if (c < 0 || c >= v.numCourses || v.fret[(size_t) c] < 0) return -1;
    return t.courses[(size_t) c].note + capo + v.fret[(size_t) c];
}

struct VoicingRequest
{
    Chord chord;
    int position = 0;     // 0 = primeira posição (cordas soltas), 1 = ~5ª casa, 2 = ~7ª, 3 = ~9ª
    int capo = 0;
};

inline Voicing findVoicing (const InstrumentDef& inst, const Tuning& tun, const VoicingRequest& rq)
{
    Voicing best; best.numCourses = (int) tun.courses.size();
    if (! rq.chord.valid() || best.numCourses == 0) return best;
    const int n = best.numCourses;
    const auto& qi = qualityInfo (rq.chord.quality);
    const uint16_t chordMask = rq.chord.mask();
    const int rootPc = rq.chord.root, bassPc = rq.chord.bassPc();
    const int thirdPc = pc (rootPc + qi.tones[1]);
    const int seventhPc = qi.numTones == 4 ? pc (rootPc + qi.tones[3]) : -1;
    const int fifthPc = pc (rootPc + qi.tones[2]);
    const int capo = std::max (0, rq.capo);
    const int maxFret = std::max (4, inst.numFrets - capo);
    const int targetFret = rq.position == 0 ? 0 : (rq.position == 1 ? 5 : (rq.position == 2 ? 7 : 9));

    long bestScore = LONG_MIN;
    std::array<std::array<int, 8>, kMaxCourses> opts {};
    std::array<int, kMaxCourses> nopt {};
    std::array<int, kMaxCourses> cur {};

    for (int base = 1; base <= std::min (maxFret - 2, 12); ++base)
    {
        const int top = std::min (maxFret, base + inst.maxSpan - 1);
        for (int c = 0; c < n; ++c)
        {
            int k = 0;
            const auto& co = tun.courses[(size_t) c];
            const int open = co.note + capo;
            if (! co.drone) opts[(size_t) c][(size_t) k++] = -1;
            if ((chordMask >> pc (open)) & 1u) opts[(size_t) c][(size_t) k++] = 0;
            if (co.drone) { if (k == 0) opts[(size_t) c][(size_t) k++] = -1; nopt[(size_t) c] = k; continue; }
            for (int f = base; f <= top && k < 8; ++f)
                if ((chordMask >> pc (open + f)) & 1u) opts[(size_t) c][(size_t) k++] = f;
            nopt[(size_t) c] = k;
        }

        // enumeração (odômetro)
        std::array<int, kMaxCourses> idx {};
        while (true)
        {
            for (int c = 0; c < n; ++c) cur[(size_t) c] = opts[(size_t) c][(size_t) idx[(size_t) c]];

            // ---- avaliação
            int sounding = 0, lowestNote = 1000, lowestPc = -1, minF = 99, maxF = 0, opens = 0;
            uint16_t have = 0; int firstSound = -1, lastSound = -1, innerMutes = 0;
            for (int c = 0; c < n; ++c)
            {
                const int f = cur[(size_t) c];
                if (f < 0) continue;
                const auto& co = tun.courses[(size_t) c];
                int note = co.note + capo + f;
                if (co.pair >= 0) note = std::min (note, co.pair + capo + f);
                ++sounding; have |= (uint16_t) (1u << pc (note));
                if (! co.drone && note < lowestNote) { lowestNote = note; lowestPc = pc (note); }
                if (f == 0) ++opens; else { minF = std::min (minF, f); maxF = std::max (maxF, f); }
                if (firstSound < 0) firstSound = c;
                lastSound = c;
            }
            for (int c = firstSound + 1; c < lastSound; ++c) if (firstSound >= 0 && cur[(size_t) c] < 0) ++innerMutes;
            bool ok = sounding >= std::min (3, n) && ((have >> rootPc) & 1u) && ((have >> thirdPc) & 1u)
                      && (seventhPc < 0 || ((have >> seventhPc) & 1u));
            if (ok && maxF > 0 && maxF - minF >= inst.maxSpan) ok = false;
            // dedos: notas presas na casa mínima contam como pestana (1 dedo) se forem 2+
            int fingers = 0;
            if (ok && maxF > 0)
            {
                int atMin = 0;
                for (int c = 0; c < n; ++c) { const int f = cur[(size_t) c]; if (f > 0) { if (f == minF) ++atMin; else ++fingers; } }
                fingers += atMin > 0 ? 1 : 0;
                // pestana só vale se nenhuma corda solta ficar "por baixo" dela entre cordas presas
                if (atMin >= 2)
                {
                    int lo = -1, hi = -1;
                    for (int c = 0; c < n; ++c) if (cur[(size_t) c] == minF) { if (lo < 0) lo = c; hi = c; }
                    for (int c = lo; c <= hi; ++c) if (cur[(size_t) c] == 0) { fingers += atMin - 1; break; }
                }
                if (fingers > 4) ok = false;
            }
            if (ok)
            {
                long s = 0;
                s += (long) sounding * 220;
                s -= (long) innerMutes * 900;
                // corda solta "presa" entre notas pressionadas fora da 1ª posição soa estranho
                if (minF >= 1 && maxF >= 3)
                    for (int c = 1; c + 1 < n; ++c)
                        if (cur[(size_t) c] == 0 && cur[(size_t) (c - 1)] > 0 && cur[(size_t) (c + 1)] > 0) s -= 260;
                const float bw = inst.bassWeight;
                if (lowestPc == bassPc) s += (long) (700 * bw); else if (lowestPc == rootPc) s += (long) (250 * bw); else s -= (long) (500 * bw);
                if ((have >> bassPc) & 1u) s += 60;
                if (! ((have >> fifthPc) & 1u)) s -= 120;
                if (rq.position == 0)
                {
                    s += opens * 45;
                    s -= (long) (maxF > 0 ? (minF + maxF) : 0) * 18;
                    if (maxF > 4) s -= (long) (maxF - 4) * 160;
                }
                else
                {
                    const int centre = maxF > 0 ? (minF + maxF) / 2 : 0;
                    s -= (long) std::abs (centre - targetFret) * 90;
                    s -= opens * 20;
                }
                s -= (long) fingers * 15;
                // dobra excessiva da mesma nota no agudo soa estranha
                if (s > bestScore)
                {
                    bestScore = s; best.valid = true;
                    for (int c = 0; c < n; ++c) best.fret[(size_t) c] = cur[(size_t) c];
                }
            }

            // próximo
            int c = n - 1;
            while (c >= 0) { if (++idx[(size_t) c] < nopt[(size_t) c]) break; idx[(size_t) c] = 0; --c; }
            if (c < 0) break;
        }
    }
    return best;
}

// Cache pequeno (thread de áudio: sem alocação após o primeiro uso de cada acorde).
class VoicingCache
{
public:
    const Voicing& get (int inst, int tuning, const VoicingRequest& rq)
    {
        const uint32_t k = (uint32_t) inst | ((uint32_t) tuning << 4) | ((uint32_t) (rq.chord.root + 1) << 8)
                         | ((uint32_t) rq.chord.quality << 12) | ((uint32_t) (rq.chord.bass + 1) << 17)
                         | ((uint32_t) rq.position << 22) | ((uint32_t) rq.capo << 25);
        for (auto& e : entries) if (e.used && e.key == k) return e.v;
        auto& e = entries[(size_t) (next++ % entries.size())];
        e.used = true; e.key = k;
        e.v = findVoicing (instrument (inst), tuningOf (inst, tuning), rq);
        return e.v;
    }
private:
    struct E { bool used = false; uint32_t key = 0; Voicing v; };
    std::array<E, 96> entries {};
    size_t next = 0;
};
} // namespace sms
