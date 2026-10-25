// SeniorMy Structure — arranjador: condução de vozes e geração das linhas de cobertura
// (pads sustentados, linha superior legato, ostinato/spiccato, pizzicato, trêmolo, tutti).
// Determinístico: o mesmo código roda no áudio em tempo real e na exportação MIDI offline.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Harmony.h"
#include <array>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace sms
{
//==============================================================================
enum Section : int { Vln1 = 0, Vln2, Vla, Vc, Cb, Horns, numSections };
enum Artic : int { Sustain = 0, Legato, Spiccato, Pizzicato, Tremolo, numArtics };
enum class LaneType : int { Pad = 0, Line, Pattern, Tutti };

inline const char* sectionName (int s, bool pt = true)
{
    static const char* p[] = { "Violinos I", "Violinos II", "Violas", "Violoncelos", "Contrabaixos", "Trompas" };
    static const char* e[] = { "Violins I", "Violins II", "Violas", "Cellos", "Basses", "Horns" };
    return (pt ? p : e)[std::clamp (s, 0, (int) numSections - 1)];
}
inline const char* articName (int a, bool pt = true)
{
    static const char* p[] = { "Sustain", "Legato", "Spiccato", "Pizzicato", "Trêmolo" };
    static const char* e[] = { "Sustain", "Legato", "Spiccato", "Pizzicato", "Tremolo" };
    return (pt ? p : e)[std::clamp (a, 0, (int) numArtics - 1)];
}

struct Range { int lo, hi, sweetLo, sweetHi; };
inline Range sectionRange (int s)
{
    switch (s)
    {
        // limites seguem a extensão amostrada (VSCO 2 CE) para evitar transposições grandes
        case Vln1:  return { 55, 93, 62, 88 };
        case Vln2:  return { 55, 91, 58, 84 };
        case Vla:   return { 48, 84, 53, 76 };
        case Vc:    return { 36, 79, 41, 67 };
        case Cb:    return { 28, 60, 31, 52 };
        case Horns: return { 41, 77, 48, 72 };
        default:    return { 36, 96, 48, 84 };
    }
}

//==============================================================================
constexpr int kMaxSteps = 32;
constexpr int kRows = 6;
constexpr int kMaxLanes = 8;

struct Lane
{
    std::string name = "Lane";
    LaneType type = LaneType::Pad;
    int artic = Sustain;
    uint8_t sectionMask = 0x1F;      // bits por Section
    bool enabled = true;
    float gain = 1.0f;               // 0..1.5
    int octave = 0;                  // -2..+2
    int stepsPerBeat = 4;            // 2 = colcheias, 3 = tercinas, 4 = semicolcheias
    int numSteps = 16;               // 1..32
    float gate = 0.5f;               // duração da nota (fração do passo) nos padrões
    std::array<std::array<uint8_t, kMaxSteps>, kRows> grid {};   // velocity 0..127 (0 = vazio); linha 0 = baixo

    void clearGrid() { for (auto& r : grid) r.fill (0); }
    bool stepHasAny (int step) const
    {
        for (int r = 0; r < kRows; ++r) if (grid[(size_t) r][(size_t) step] > 0) return true;
        return false;
    }
};

struct Arrangement
{
    std::string name = "Init";
    std::vector<Lane> lanes;
};

enum class InversionMode : int { Auto = 0, Root, First, Second, FollowInput };

struct ArrangerSettings
{
    int extensions = 1;            // 0..3
    InversionMode inversion = InversionMode::Auto;
    float density = 0.6f;          // 0..1
    int registerShift = 0;         // semitons (-12..+12)
    float spread = 0.5f;           // 0 = fechado, 1 = aberto
    float humanizeTime = 0.2f;     // 0..1 → até 30 ms
    float humanizeVel = 0.2f;      // 0..1 → até ±20 velocity
    float dynamics = 0.75f;        // 0..1 (expressão global)
    bool respectScale = true;      // linha superior usa notas de passagem só da escala
    uint32_t seed = 12345;
};

struct NoteEvent
{
    int64_t time = 0;              // amostra absoluta
    uint8_t section = 0, artic = 0, note = 60, velocity = 100, lane = 0;
    bool on = true;
};

//==============================================================================
// Condução de vozes: escolhe um voicing de N vozes minimizando um custo musical.
class Voicer
{
public:
    void reset() { prev.clear(); }
    const std::vector<int>& last() const { return prev; }

    std::vector<int> voice (const Chord& c, const Key& key, const ArrangerSettings& s, int numVoices)
    {
        std::vector<int> out;
        if (! c.valid()) return out;
        const auto pcs = chordPitchClasses (c, key, s.extensions);
        if (pcs.empty()) return out;

        int bassPc = c.root;
        switch (s.inversion)
        {
            case InversionMode::Root:   bassPc = c.root; break;
            case InversionMode::First:  bassPc = c.third(); break;
            case InversionMode::Second: bassPc = c.fifth(); break;
            case InversionMode::FollowInput: bassPc = c.bassPc(); break;
            case InversionMode::Auto:
            default:
                bassPc = c.bassPc();
                break;
        }

        // baixo: nota com a classe pedida, perto do baixo anterior (ou do centro grave)
        const int bassCenter = (prev.empty() ? 40 : prev.front()) ;
        int bass = -1, bestD = 1000;
        for (int n = 28 + std::max (0, s.registerShift / 2); n <= 52; ++n)
            if (pc (n) == bassPc)
            {
                int d = std::abs (n - bassCenter);
                if (s.inversion == InversionMode::Auto && ! prev.empty())
                {
                    // em modo automático permite trocar o baixo para outro tom do acorde se andar menos
                    d += 0;
                }
                if (d < bestD) { bestD = d; bass = n; }
            }
        if (s.inversion == InversionMode::Auto && ! prev.empty() && c.bass < 0)
        {
            // condução automática: fundamental ou terça/quinta, o que mover menos (preferindo fundamental)
            for (int alt : { c.third(), c.fifth() })
                for (int n = 31; n <= 52; ++n)
                    if (pc (n) == alt)
                    {
                        const int d = std::abs (n - bassCenter) + 4;   // custo extra por inversão
                        if (d < bestD && d < 4 + 2) { bestD = d; bass = n; }
                    }
        }
        if (bass < 0) bass = 36 + bassPc;
        out.push_back (bass);
        if (numVoices <= 1) { prev = out; return out; }

        // pool de candidatos acima do baixo
        const int center = 66 + s.registerShift;
        const int width = 14 + (int) std::lround (s.spread * 10.0f);
        const int lo = std::max (bass + 5, center - width);
        const int hi = std::min (100, center + width);
        std::vector<int> pool;
        for (int n = lo; n <= hi; ++n)
            if (std::find (pcs.begin(), pcs.end(), pc (n)) != pcs.end()) pool.push_back (n);

        const int k = std::min ((int) pool.size(), numVoices - 1);
        if (k <= 0) { prev = out; return out; }

        // reduz o pool aos mais próximos do voicing anterior/centro se ficar grande
        if (pool.size() > 16)
        {
            std::vector<std::pair<int, int>> d;
            for (int n : pool)
            {
                int dist = std::abs (n - center);
                for (size_t i = 1; i < prev.size(); ++i) dist = std::min (dist, std::abs (n - prev[i]));
                d.push_back ({ dist, n });
            }
            std::sort (d.begin(), d.end());
            pool.clear();
            for (size_t i = 0; i < 16; ++i) pool.push_back (d[i].second);
            std::sort (pool.begin(), pool.end());
        }

        std::vector<int> idx ((size_t) k), best;
        float bestCost = 1e30f;
        for (int i = 0; i < k; ++i) idx[(size_t) i] = i;
        const int P = (int) pool.size();
        std::vector<int> cand ((size_t) k + 1);
        while (true)
        {
            cand[0] = bass;
            for (int i = 0; i < k; ++i) cand[(size_t) i + 1] = pool[(size_t) idx[(size_t) i]];
            const float cost = evaluate (cand, c, key, pcs, s, center);
            if (cost < bestCost) { bestCost = cost; best = cand; }
            // próxima combinação
            int i = k - 1;
            while (i >= 0 && idx[(size_t) i] == P - k + i) --i;
            if (i < 0) break;
            ++idx[(size_t) i];
            for (int j = i + 1; j < k; ++j) idx[(size_t) j] = idx[(size_t) j - 1] + 1;
        }
        prev = best;
        return best;
    }

private:
    float evaluate (const std::vector<int>& v, const Chord& c, const Key& key, const std::vector<int>& pcs,
                    const ArrangerSettings& s, int center) const
    {
        float cost = 0;
        // movimento (voz a voz, ordenado)
        if (prev.size() == v.size())
        {
            for (size_t i = 1; i < v.size(); ++i) cost += (float) std::abs (v[i] - prev[i]) * 1.0f;
            // voz superior: preferência por grau conjunto
            const int topMove = std::abs (v.back() - prev.back());
            if (topMove > 4) cost += (float) (topMove - 4) * 1.5f;
            // quintas/oitavas paralelas
            for (size_t i = 0; i < v.size(); ++i)
                for (size_t j = i + 1; j < v.size(); ++j)
                {
                    const int a0 = pc (prev[j] - prev[i]), a1 = pc (v[j] - v[i]);
                    const int m1 = v[i] - prev[i], m2 = v[j] - prev[j];
                    if (m1 != 0 && m2 != 0 && ((m1 > 0) == (m2 > 0)) && a0 == a1 && (a1 == 7 || a1 == 0)) cost += 6.0f;
                }
        }
        else if (! prev.empty())
        {
            // número de vozes mudou: compara com a nota anterior mais próxima
            for (size_t i = 1; i < v.size(); ++i)
            {
                int d = 99;
                for (size_t j = 1; j < prev.size(); ++j) d = std::min (d, std::abs (v[i] - prev[j]));
                cost += (float) d;
            }
        }

        // cobertura dos tons do acorde
        int count[12] = {};
        for (int n : v) ++count[pc (n)];
        const bool hasThird = c.quality != Quality::Sus2 && c.quality != Quality::Sus4;
        if (hasThird && count[c.third()] == 0) cost += 14.0f;
        if (count[c.root] == 0) cost += 5.0f;
        const auto& qi = qualityInfo (c.quality);
        if (s.extensions >= 1 && qi.numTones == 4 && count[pc (c.root + qi.tones[3])] == 0) cost += 6.0f;
        for (size_t i = 3; i < pcs.size(); ++i) if (count[pcs[i]] == 0) cost += 2.5f;   // extensões
        if (hasThird && count[c.third()] > 1) cost += 4.0f;                              // terça dobrada
        const int leading = pc (key.tonic + 11);
        if (key.contains (leading) && count[leading] > 1) cost += 4.0f;                   // sensível dobrada

        // espaçamento
        const float targetGap = 3.5f + s.spread * 5.0f;
        for (size_t i = 1; i + 1 < v.size(); ++i)
        {
            const int g = v[i + 1] - v[i];
            if (g > 12) cost += (float) (g - 12) * 1.2f;
            if (g < 2 && s.extensions < 3) cost += 3.0f;
            cost += std::abs ((float) g - targetGap) * 0.25f;
        }
        if (v.size() > 1)
        {
            const int g = v[1] - v[0];
            if (v[0] < 48 && g < 7) cost += 5.0f;   // intervalos estreitos no grave embolam
            if (g > 24) cost += (float) (g - 24) * 0.8f;
        }
        // centro de registro
        float mean = 0;
        for (size_t i = 1; i < v.size(); ++i) mean += (float) v[i];
        mean /= (float) std::max<size_t> (1, v.size() - 1);
        cost += std::abs (mean - (float) center) * 0.35f;
        return cost;
    }

    std::vector<int> prev;
};

//==============================================================================
// Gerador em tempo real.
class Arranger
{
public:
    struct Clock
    {
        double ppq = 0;        // posição (semínimas) no início do bloco
        double bpm = 120;
        bool playing = false;  // transporte do host tocando
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        pending.reserve (1024);
        active.reserve (512);
        reset();
    }

    void reset()
    {
        voicer.reset();
        for (auto& l : laneState) l = {};
        pending.clear();
        active.clear();
        current = {};
        sampleClock = 0;
        freePpq = 0;
        rng = settings.seed;
        lineNote = -1;
    }

    void setArrangement (const Arrangement& a)
    {
        arrangement = a;
        if (arrangement.lanes.size() > (size_t) kMaxLanes) arrangement.lanes.resize ((size_t) kMaxLanes);
        forceRevoice = true;
    }
    const Arrangement& getArrangement() const { return arrangement; }
    ArrangerSettings settings;

    // Liga/desliga uma faixa (parâmetro automatizável). Desligar solta as notas dela.
    void setLaneEnabledOverride (int lane, bool on)
    {
        if (lane < 0 || lane >= kMaxLanes) return;
        laneOverride[(size_t) lane] = on;
    }

    // Processa um bloco. chord/key: estado harmônico atual. silence: sem entrada → solta tudo.
    // Eventos gerados (no bloco) vão para 'out' com tempo absoluto em amostras.
    void process (int numSamples, const Clock& clockIn, const Chord& chord, const Key& key, bool silence,
                  std::vector<NoteEvent>& out)
    {
        const int64_t blockStart = sampleClock;
        const int64_t blockEnd = sampleClock + numSamples;
        Clock clk = clockIn;
        if (! clk.playing) clk.ppq = freePpq;  // relógio livre para tocar ao vivo
        const double ppqPerSample = clk.bpm / 60.0 / sr;

        // lanes desligadas → solta as notas delas
        for (size_t li = 0; li < arrangement.lanes.size(); ++li)
        {
            const bool en = laneIsOn ((int) li);
            if (! en && laneState[li].wasOn) releaseLane ((int) li, blockStart);
            laneState[li].wasOn = en;
        }

        // mudança de acorde → revoicing
        const bool chordChanged = chord != current || forceRevoice;
        if (silence && current.valid())
        {
            releaseAll (blockStart);
            current = {};
            lineNote = -1;
        }
        else if (! silence && chord.valid() && chordChanged)
        {
            current = chord;
            currentKey = key;
            forceRevoice = false;
            onChordChange (blockStart, ppqPerSample);
        }
        currentKey = key;

        // passos dos padrões e da linha
        if (current.valid())
        {
            for (size_t li = 0; li < arrangement.lanes.size(); ++li)
            {
                const auto& lane = arrangement.lanes[li];
                if (! laneIsOn ((int) li)) continue;
                if (lane.type == LaneType::Pattern) runPattern ((int) li, lane, clk, blockStart, numSamples, ppqPerSample);
                else if (lane.type == LaneType::Line) runLine ((int) li, lane, clk, blockStart, numSamples, ppqPerSample);
            }
        }

        // notas de padrão com duração: desliga as vencidas
        for (auto it = active.begin(); it != active.end();)
        {
            if (it->offTime >= 0 && it->offTime < blockEnd)
            {
                emit ({ std::max (it->offTime, blockStart), it->section, it->artic, it->note, 0, it->lane, false });
                it = active.erase (it);
            }
            else ++it;
        }

        // entrega os eventos devidos neste bloco (humanização pode atrasar)
        // ordenação estável por inserção (sem alocação; a lista é curta)
        for (size_t i = 1; i < pending.size(); ++i)
        {
            const NoteEvent e = pending[i];
            size_t j = i;
            while (j > 0 && pending[j - 1].time > e.time) { pending[j] = pending[j - 1]; --j; }
            pending[j] = e;
        }
        size_t n = 0;
        for (; n < pending.size() && pending[n].time < blockEnd; ++n) out.push_back (pending[n]);
        pending.erase (pending.begin(), pending.begin() + (long) n);

        sampleClock = blockEnd;
        freePpq = clk.ppq + numSamples * ppqPerSample;
        if (clockIn.playing) freePpq = clockIn.ppq + numSamples * ppqPerSample;
    }

    // Solta tudo imediatamente (stop, troca de preset).
    void allNotesOff (std::vector<NoteEvent>& out)
    {
        releaseAll (sampleClock);
        for (auto& e : pending) out.push_back (e);
        pending.clear();
        current = {};
        lineNote = -1;
    }

    const std::vector<int>& currentVoicing() const { return voicing; }
    int64_t now() const { return sampleClock; }

private:
    struct ActiveNote { uint8_t section, artic, note, lane; int64_t offTime; };   // offTime -1 = sustentada
    struct LaneState { bool wasOn = false; double lastStepPos = -1; int64_t lastStepIndex = -1; };

    bool laneIsOn (int li) const
    {
        if (li < 0 || li >= (int) arrangement.lanes.size()) return false;
        return arrangement.lanes[(size_t) li].enabled && laneOverride[(size_t) li];
    }

    uint32_t nextRand() { rng = rng * 1664525u + 1013904223u; return rng; }
    float rand01() { return (float) ((nextRand() >> 8) & 0xFFFF) / 65535.0f; }

    int humanizeDelay()
    {
        const float maxMs = 30.0f * settings.humanizeTime;
        return (int) (rand01() * maxMs * 0.001f * (float) sr);
    }
    uint8_t humanVel (float v)
    {
        const float var = (rand01() * 2.0f - 1.0f) * 20.0f * settings.humanizeVel;
        return (uint8_t) std::clamp ((int) std::lround (v + var), 1, 127);
    }

    void emit (const NoteEvent& e) { pending.push_back (e); }

    void noteOn (int64_t t, int lane, int section, int artic, int note, float vel, int64_t offTime)
    {
        if (note < 0 || note > 127) return;
        // se já existe a mesma nota ativa nessa lane/seção/articulação, solta antes (retrigger)
        for (auto it = active.begin(); it != active.end(); ++it)
            if (it->lane == lane && it->section == section && it->artic == artic && it->note == note)
            {
                emit ({ t, (uint8_t) section, (uint8_t) artic, (uint8_t) note, 0, (uint8_t) lane, false });
                active.erase (it);
                break;
            }
        emit ({ t, (uint8_t) section, (uint8_t) artic, (uint8_t) note, humanVel (vel), (uint8_t) lane, true });
        active.push_back ({ (uint8_t) section, (uint8_t) artic, (uint8_t) note, (uint8_t) lane, offTime });
    }

    void releaseLane (int lane, int64_t t)
    {
        for (auto it = active.begin(); it != active.end();)
        {
            if (it->lane == lane) { emit ({ t, it->section, it->artic, it->note, 0, it->lane, false }); it = active.erase (it); }
            else ++it;
        }
    }
    void releaseAll (int64_t t)
    {
        for (auto& a : active) emit ({ t, a.section, a.artic, a.note, 0, a.lane, false });
        active.clear();
    }

    int voicesForDensity() const { return 4 + (int) std::lround (settings.density * 3.0f); }   // 4..7

    // distribui um voicing (grave → agudo) entre as seções da máscara
    std::vector<std::pair<int, int>> distribute (const std::vector<int>& v, uint8_t mask, int octave, bool doubleOctaves) const
    {
        std::vector<std::pair<int, int>> res;   // (section, note)
        if (v.empty()) return res;
        auto has = [&] (int s) { return (mask >> s) & 1; };
        const int shift = octave * 12;
        // baixo → contrabaixos (e cellos uma oitava acima se não houver contrabaixo)
        const int bass = v.front() + shift;
        if (has (Cb)) res.push_back ({ Cb, fitRange (Cb, bass) });
        if (has (Vc)) res.push_back ({ Vc, fitRange (Vc, has (Cb) ? bass + 12 : bass) });
        // vozes superiores: dos agudos para os graves → Vln1, Vln2, Vla, Vc
        std::vector<int> upper (v.begin() + 1, v.end());
        for (auto& n : upper) n += shift;
        std::vector<int> order;
        for (int s : { (int) Vln1, (int) Vln2, (int) Vla }) if (has (s)) order.push_back (s);
        if (order.empty() && has (Vc)) order.push_back (Vc);
        const int U = (int) upper.size();
        for (int i = 0; i < U; ++i)
        {
            const int note = upper[(size_t) (U - 1 - i)];   // do agudo ao grave
            // seção = proporcional à posição
            const int idx = order.empty() ? -1 : std::min ((int) order.size() - 1, i * (int) order.size() / std::max (1, U));
            if (idx < 0) break;
            const int s = order[(size_t) idx];
            res.push_back ({ s, fitRange (s, note) });
        }
        if (has (Horns))
        {
            // trompas: as duas/três vozes centrais
            for (int i = std::max (0, U / 2 - 1); i < std::min (U, U / 2 + 2); ++i)
                res.push_back ({ Horns, fitRange (Horns, upper[(size_t) i]) });
        }
        if (doubleOctaves && has (Vln1) && U > 0)
            res.push_back ({ Vln1, fitRange (Vln1, upper.back() + 12) });
        // remove duplicatas exatas
        std::sort (res.begin(), res.end());
        res.erase (std::unique (res.begin(), res.end()), res.end());
        return res;
    }

    static int fitRange (int section, int note)
    {
        const auto r = sectionRange (section);
        while (note < r.lo) note += 12;
        while (note > r.hi) note -= 12;
        return note;
    }

    void onChordChange (int64_t t, double ppqPerSample)
    {
        (void) ppqPerSample;
        voicing = voicer.voice (current, currentKey, settings, voicesForDensity());

        for (size_t li = 0; li < arrangement.lanes.size(); ++li)
        {
            const auto& lane = arrangement.lanes[li];
            if (! laneIsOn ((int) li)) continue;
            if (lane.type != LaneType::Pad && lane.type != LaneType::Tutti) continue;

            const bool tutti = lane.type == LaneType::Tutti;
            auto targets = distribute (voicing, lane.sectionMask, lane.octave, tutti);
            const float baseVel = (tutti ? 82.0f : 78.0f) * lane.gain * (0.45f + 0.75f * settings.dynamics);

            // notas comuns continuam soando (legato), as outras soltam
            for (auto it = active.begin(); it != active.end();)
            {
                if (it->lane != li) { ++it; continue; }
                const bool keep = std::find (targets.begin(), targets.end(), std::make_pair ((int) it->section, (int) it->note)) != targets.end();
                if (! keep) { emit ({ t, it->section, it->artic, it->note, 0, it->lane, false }); it = active.erase (it); }
                else ++it;
            }
            for (auto& [sec, note] : targets)
            {
                bool already = false;
                for (auto& a : active) if (a.lane == li && a.section == sec && a.note == note) already = true;
                if (already) continue;
                noteOn (t + humanizeDelay(), (int) li, sec, lane.artic, note, baseVel, -1);
            }
        }

        // linha superior: reposiciona no tom do acorde mais próximo
        if (lineNote >= 0)
        {
            const auto pcs = chordPitchClasses (current, currentKey, std::min (settings.extensions, 2));
            lineNote = nearestPc (lineNote, pcs);
            lineRetarget = true;
        }
    }

    static int nearestPc (int note, const std::vector<int>& pcs)
    {
        int best = note, bd = 99;
        for (int d = 0; d <= 6; ++d)
            for (int sgn : { 1, -1 })
            {
                const int n = note + d * sgn;
                if (std::find (pcs.begin(), pcs.end(), pc (n)) != pcs.end() && d < bd) { bd = d; best = n; }
            }
        return best;
    }

    // escolhe a nota da linha (do padrão) a partir do voicing atual
    int rowNote (int row, int octave) const
    {
        if (voicing.empty()) return -1;
        const int V = (int) voicing.size();
        int n;
        if (row < V) n = voicing[(size_t) row];
        else n = voicing[(size_t) (1 + (row - 1) % std::max (1, V - 1))] + 12 * (1 + (row - V) / std::max (1, V - 1));
        return n + octave * 12;
    }

    int sectionForNote (uint8_t mask, int note) const
    {
        // seção cuja faixa "doce" contém a nota; preferência pela mais aguda possível para notas altas
        int best = -1, bestScore = 1 << 20;
        for (int s = 0; s < numSections; ++s)
        {
            if (! ((mask >> s) & 1)) continue;
            const auto r = sectionRange (s);
            int score = 0;
            if (note < r.sweetLo) score += (r.sweetLo - note) * 3;
            if (note > r.sweetHi) score += (note - r.sweetHi) * 3;
            if (note < r.lo || note > r.hi) score += 100;
            if (score < bestScore) { bestScore = score; best = s; }
        }
        return best;
    }

    void runPattern (int li, const Lane& lane, const Clock& clk, int64_t blockStart, int numSamples, double ppqPerSample)
    {
        const double stepLen = 1.0 / std::max (1, lane.stepsPerBeat);
        const int steps = std::clamp (lane.numSteps, 1, kMaxSteps);
        const double startPos = clk.ppq, endPos = clk.ppq + numSamples * ppqPerSample;
        int64_t firstStep = (int64_t) std::ceil (startPos / stepLen - 1e-9);
        auto& st = laneState[(size_t) li];
        for (int64_t si = firstStep; (double) si * stepLen < endPos; ++si)
        {
            if (si == st.lastStepIndex) continue;
            st.lastStepIndex = si;
            const int step = (int) (((si % steps) + steps) % steps);
            const double pos = (double) si * stepLen;
            const int64_t t = blockStart + (int64_t) std::max (0.0, (pos - startPos) / ppqPerSample);
            const int64_t len = (int64_t) (stepLen * std::clamp (lane.gate, 0.05f, 1.0f) / ppqPerSample);
            // densidade: menos densidade → pula passos fracos (fora dos tempos)
            const bool strong = (step % std::max (1, lane.stepsPerBeat)) == 0;
            if (! strong && rand01() > 0.35f + 0.65f * settings.density) continue;
            for (int r = 0; r < kRows; ++r)
            {
                const int v = lane.grid[(size_t) r][(size_t) step];
                if (v <= 0) continue;
                const int note = rowNote (r, lane.octave);
                if (note < 0) continue;
                const int sec = sectionForNote (lane.sectionMask, note);
                if (sec < 0) continue;
                const float vel = (float) v * lane.gain * (0.55f + 0.6f * settings.dynamics);
                const int64_t tt = t + humanizeDelay();
                noteOn (tt, li, sec, lane.artic, fitRange (sec, note), vel, tt + std::max<int64_t> (32, len));
            }
        }
    }

    // Linha superior legato: nota de acorde no topo, notas de passagem pela escala conforme a densidade.
    void runLine (int li, const Lane& lane, const Clock& clk, int64_t blockStart, int numSamples, double ppqPerSample)
    {
        const auto pcs = chordPitchClasses (current, currentKey, std::min (settings.extensions, 2));
        const int top = voicing.empty() ? 72 : voicing.back();
        const int lineCenter = top + 5 + lane.octave * 12;
        uint8_t mask = lane.sectionMask;
        if (! mask) mask = 1 << Vln1;

        auto play = [&] (int64_t t, int note, float velScale)
        {
            int sec = sectionForNote (mask, note);
            if (sec < 0) return;
            const int fitted = fitRange (sec, note);
            // legato: solta a nota anterior um pouco depois (sobreposição curta)
            for (auto it = active.begin(); it != active.end();)
            {
                if (it->lane == li && it->offTime < 0) { it->offTime = t + (int64_t) (0.06 * sr); ++it; }
                else ++it;
            }
            noteOn (t, li, sec, lane.artic, fitted, 84.0f * lane.gain * velScale * (0.5f + 0.7f * settings.dynamics), -1);
        };

        if (lineNote < 0)
        {
            lineNote = nearestPc (lineCenter, pcs);
            play (blockStart + humanizeDelay(), lineNote, 1.0f);
            lineTarget = lineNote;
            lineRetarget = false;
            return;
        }
        if (lineRetarget)
        {
            lineRetarget = false;
            play (blockStart + humanizeDelay(), lineNote, 1.0f);
            lineTarget = lineNote;
            return;
        }

        // movimento: a cada meio tempo (densidade alta) ou a cada 2 tempos (baixa)
        const double every = settings.density > 0.66f ? 0.5 : (settings.density > 0.33f ? 1.0 : 2.0);
        const double startPos = clk.ppq, endPos = clk.ppq + numSamples * ppqPerSample;
        auto& st = laneState[(size_t) li];
        int64_t first = (int64_t) std::ceil (startPos / every - 1e-9);
        for (int64_t si = first; (double) si * every < endPos; ++si)
        {
            if (si == st.lastStepIndex) continue;
            st.lastStepIndex = si;
            if (rand01() > 0.25f + 0.6f * settings.density) continue;
            const int64_t t = blockStart + (int64_t) std::max (0.0, ((double) si * every - startPos) / ppqPerSample);
            // escolhe um novo alvo (tom do acorde) perto do centro, e anda por grau conjunto da escala
            if (lineNote == lineTarget)
            {
                int dir = (lineNote > lineCenter + 3) ? -1 : (lineNote < lineCenter - 3 ? 1 : ((nextRand() & 1) ? 1 : -1));
                int cand = lineNote + dir;
                for (int k = 0; k < 8; ++k, cand += dir)
                    if (std::find (pcs.begin(), pcs.end(), pc (cand)) != pcs.end() && cand != lineNote) break;
                lineTarget = cand;
            }
            const int dir = lineTarget > lineNote ? 1 : -1;
            int next = lineNote + dir;
            if (settings.respectScale)
                while (! currentKey.contains (next) && std::abs (next - lineNote) < 3) next += dir;
            lineNote = next;
            if ((dir > 0 && lineNote > lineTarget) || (dir < 0 && lineNote < lineTarget)) lineNote = lineTarget;
            play (t + humanizeDelay(), lineNote, lineNote == lineTarget ? 1.0f : 0.85f);
        }
    }

    double sr = 44100;
    Arrangement arrangement;
    std::array<bool, kMaxLanes> laneOverride { true, true, true, true, true, true, true, true };
    std::array<LaneState, kMaxLanes> laneState {};
    Voicer voicer;
    std::vector<int> voicing;
    std::vector<NoteEvent> pending;
    std::vector<ActiveNote> active;
    Chord current {};
    Key currentKey {};
    bool forceRevoice = false;
    int64_t sampleClock = 0;
    double freePpq = 0;
    uint32_t rng = 12345;
    int lineNote = -1, lineTarget = -1;
    bool lineRetarget = false;
};
} // namespace sms
