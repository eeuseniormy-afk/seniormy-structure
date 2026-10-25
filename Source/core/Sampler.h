// SeniorMy Structure — motor sampler orquestral: zonas por nota/velocity, round robin,
// camadas de velocity com crossfade contínuo (dinâmica), loop com crossfade para sustains,
// release exponencial, pan por seção e roubo de vozes. Sem alocação no áudio.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <vector>
#include <memory>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

namespace sms
{
// Amostras ficam na RAM em 16 bits (metade da memória de float; os arquivos em disco são 24 bits
// normalizados, e a conversão usa dither TPDF). Biblioteca inteira ≈ 300–400 MB de RAM.
struct SampleData
{
    std::vector<int16_t> left, right;   // mesmo tamanho; mono → right = left
    double sampleRate = 44100;
    int frames() const { return (int) left.size(); }

    void assign (const float* l, const float* r, int n)
    {
        left.resize ((size_t) n); right.resize ((size_t) n);
        uint32_t seed = 0x1234567u;
        auto q = [&seed] (float x)
        {
            seed = seed * 1664525u + 1013904223u; const float d1 = (float) (seed >> 8) / 16777216.0f;
            seed = seed * 1664525u + 1013904223u; const float d2 = (float) (seed >> 8) / 16777216.0f;
            const float v = x * 32767.0f + (d1 - d2);
            return (int16_t) std::clamp ((int) std::lround (v), -32768, 32767);
        };
        for (int i = 0; i < n; ++i) { left[(size_t) i] = q (l[i]); right[(size_t) i] = q (r ? r[i] : l[i]); }
    }
};

struct Zone
{
    std::shared_ptr<const SampleData> data;
    int root = 60;            // nota MIDI da amostra
    int loKey = 0, hiKey = 127;
    int layer = 0;            // índice da camada de velocity (0 = mais suave)
    int rr = 0;               // índice de round robin
    float gain = 1.0f;
    float tuneCents = 0.0f;
    int loopStart = -1, loopEnd = -1;   // em quadros; -1 = sem loop
    int startOffset = 0;      // quadros pulados no início (decorrelação de Violinos II)
};

// Um instrumento = uma seção × articulação.
struct Instrument
{
    std::vector<Zone> zones;
    int numLayers = 1;
    int numRR = 1;
    bool oneShot = false;     // pizzicato/spiccato: toca até o fim, ignora note-off (com cauda curta)
    float attack = 0.004f;    // s
    float release = 0.35f;    // s
    float gainDb = 0.0f;      // balanço orquestral
    float pan = 0.0f;         // -1..1
    float detuneCents = 0.0f; // Violinos II levemente desafinados para soar como outra estante
    int rrOffset = 0;
    int extraStart = 0;       // quadros pulados (decorrelação)

    // Encontra a zona para nota/camada/rr; procura a raiz mais próxima se não houver faixa exata.
    const Zone* find (int note, int layer, int rr) const
    {
        const Zone* best = nullptr;
        int bestScore = 1 << 30;
        for (const auto& z : zones)
        {
            int score = 0;
            if (note < z.loKey) score += (z.loKey - note) * 100;
            else if (note > z.hiKey) score += (note - z.hiKey) * 100;
            score += std::abs (z.layer - layer) * 1000;
            score += (z.rr == (rr % std::max (1, numRR))) ? 0 : 10;
            score += std::abs (z.root - note);
            if (score < bestScore) { bestScore = score; best = &z; }
        }
        return best;
    }
};

//==============================================================================
class SamplerVoice
{
public:
    bool active() const { return state != Off; }
    int note = -1, section = -1, artic = -1;
    uint8_t lane = 0;
    int64_t startedAt = 0;
    bool releasing() const { return state == Release; }

    void start (const Instrument& inst, int midiNote, float velocity01, float dynamics01, int rrIndex,
                double hostRate, int sec, int art, int64_t when, float attackOverride = -1.0f, float releaseOverride = -1.0f)
    {
        rate = hostRate;
        instrument = &inst;
        note = midiNote; section = sec; artic = art; startedAt = when;
        vel = velocity01;
        // camada: interpolação contínua entre duas camadas vizinhas pela dinâmica × velocity
        const float dyn = std::clamp (0.35f * velocity01 + 0.65f * dynamics01 * (0.6f + 0.4f * velocity01), 0.0f, 1.0f);
        const float lf = dyn * (float) std::max (0, inst.numLayers - 1);
        const int l0 = (int) std::floor (lf);
        const int l1 = std::min (inst.numLayers - 1, l0 + 1);
        layerMix = lf - (float) l0;
        const int rr = rrIndex + inst.rrOffset;
        zoneA = inst.find (midiNote, l0, rr);
        zoneB = (l1 != l0) ? inst.find (midiNote, l1, rr) : nullptr;
        if (! zoneA) { state = Off; return; }
        if (zoneB && zoneB->data == zoneA->data) zoneB = nullptr;
        setupHead (headA, *zoneA, inst, hostRate, midiNote);
        if (zoneB) setupHead (headB, *zoneB, inst, hostRate, midiNote);
        // compensação de volume: amostras suaves mais baixas naturalmente; ganho cresce pouco com velocity
        amp = (0.35f + 0.65f * velocity01) * std::pow (10.0f, inst.gainDb / 20.0f);
        env = 0.0f;
        attackInc = 1.0f / std::max (1.0f, (attackOverride > 0 ? attackOverride : inst.attack) * (float) hostRate);
        releaseMul = std::exp (-1.0f / std::max (1.0f, (releaseOverride > 0 ? releaseOverride : inst.release) * (float) hostRate / 6.9f));
        state = Attack;
        const float p = std::clamp (inst.pan, -1.0f, 1.0f);
        panL = std::cos ((p + 1.0f) * 0.25f * 3.14159265f);
        panR = std::sin ((p + 1.0f) * 0.25f * 3.14159265f);
        fadeOutRemaining = -1;
    }

    void noteOff()
    {
        if (state == Off || ! instrument) return;
        if (instrument->oneShot) { // pizz/spic: só encurta a cauda um pouco
            releaseMul = std::exp (-1.0f / std::max (1.0f, 0.25f * (float) rate / 6.9f));
            state = Release;
            return;
        }
        state = Release;
    }

    void steal (double hostRate)
    {
        // fade rápido de 5 ms
        fadeOutRemaining = std::max (1, (int) (0.005 * hostRate));
        fadeStep = env / (float) fadeOutRemaining;
    }

    void setDynamics (float dynamics01)
    {
        // crossfade de camadas em tempo real (como o modwheel de bibliotecas orquestrais)
        if (! instrument || ! zoneB) return;
        const float dyn = std::clamp (0.35f * vel + 0.65f * dynamics01 * (0.6f + 0.4f * vel), 0.0f, 1.0f);
        const float lf = dyn * (float) std::max (0, instrument->numLayers - 1);
        targetMix = std::clamp (lf - std::floor (lf), 0.0f, 1.0f);
        if (lf >= (float) (instrument->numLayers - 1)) targetMix = 1.0f;
    }

    void render (float* outL, float* outR, int start, int num)
    {
        if (state == Off) return;
        for (int i = start; i < start + num; ++i)
        {
            // envelope
            switch (state)
            {
                case Attack:  env += attackInc; if (env >= 1.0f) { env = 1.0f; state = Sustain; } break;
                case Sustain: break;
                case Release: env *= releaseMul; if (env < 0.0005f) { state = Off; return; } break;
                default: break;
            }
            if (fadeOutRemaining > 0)
            {
                env -= fadeStep;
                if (--fadeOutRemaining <= 0 || env <= 0) { state = Off; return; }
            }
            layerMix += (targetMix - layerMix) * 0.0005f;
            float l = 0, r = 0;
            bool aliveA = readHead (headA, *zoneA, l, r, zoneB ? (1.0f - layerMix) : 1.0f);
            bool aliveB = true;
            if (zoneB) aliveB = readHead (headB, *zoneB, l, r, layerMix);
            if (! aliveA && (! zoneB || ! aliveB)) { state = Off; return; }
            const float g = env * amp;
            outL[i] += l * g * panL;
            outR[i] += r * g * panR;
        }
    }

private:
    enum State { Off, Attack, Sustain, Release };
    struct Head
    {
        double pos = 0, inc = 1;
        int loopStart = -1, loopEnd = -1, xfade = 0;
    };

    static void setupHead (Head& h, const Zone& z, const Instrument& inst, double hostRate, int midiNote)
    {
        const double semis = (double) (midiNote - z.root) + (z.tuneCents + inst.detuneCents) / 100.0;
        h.inc = std::pow (2.0, semis / 12.0) * (z.data->sampleRate / hostRate);
        h.pos = (double) std::min (z.data->frames() - 2, z.startOffset + inst.extraStart);
        h.loopStart = z.loopStart; h.loopEnd = z.loopEnd;
        if (h.loopStart >= 0 && h.loopEnd > h.loopStart)
            h.xfade = std::min ((int) (0.12 * z.data->sampleRate), (h.loopEnd - h.loopStart) / 3);
        else h.xfade = 0;
    }

    static inline float cubic (const int16_t* d, int n, double pos)
    {
        constexpr float k = 1.0f / 32768.0f;
        const int i = (int) pos;
        const float f = (float) (pos - (double) i);
        const float y0 = d[std::max (0, i - 1)] * k, y1 = d[i] * k, y2 = d[std::min (n - 1, i + 1)] * k, y3 = d[std::min (n - 1, i + 2)] * k;
        const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    // Lê uma amostra; com loop: perto do fim do loop mistura (equal power) com o trecho antes do início.
    bool readHead (Head& h, const Zone& z, float& l, float& r, float w)
    {
        const auto& d = *z.data;
        const int n = d.frames();
        if (h.pos >= n - 2) return false;
        float sl = cubic (d.left.data(), n, h.pos), sr = cubic (d.right.data(), n, h.pos);
        if (h.loopEnd > 0 && h.xfade > 0)
        {
            const double fadeStart = (double) (h.loopEnd - h.xfade);
            if (h.pos >= fadeStart)
            {
                const double into = h.pos - fadeStart;
                const float x = (float) (into / (double) h.xfade);
                const double other = (double) (h.loopStart - h.xfade) + into;
                const float gOut = std::cos (x * 1.5707963f), gIn = std::sin (x * 1.5707963f);
                if (other >= 0)
                {
                    sl = sl * gOut + cubic (d.left.data(), n, other) * gIn;
                    sr = sr * gOut + cubic (d.right.data(), n, other) * gIn;
                }
                if (h.pos + h.inc >= (double) h.loopEnd) h.pos = other;   // salta para o equivalente
            }
        }
        l += sl * w * z.gain;
        r += sr * w * z.gain;
        h.pos += h.inc;
        return true;
    }

    const Instrument* instrument = nullptr;
    const Zone* zoneA = nullptr;
    const Zone* zoneB = nullptr;
    Head headA, headB;
    State state = Off;
    float vel = 1, amp = 1, env = 0, attackInc = 0.01f, releaseMul = 0.999f;
    float layerMix = 0, targetMix = 0, panL = 0.707f, panR = 0.707f, fadeStep = 0;
    int fadeOutRemaining = -1;
    double rate = 44100;
};

//==============================================================================
// Banco: todas as seções × articulações. Trocado por ponteiro atômico (carregado em outra thread).
struct InstrumentBank
{
    Instrument inst[6][5];            // [Section][Artic]
    bool present[6][5] = {};
    std::string description;
    bool synthesized = false;         // fallback gerado (sem biblioteca instalada)
};

class Sampler
{
public:
    static constexpr int kMaxVoices = 192;

    void prepare (double sampleRate) { sr = sampleRate; for (auto& v : voices) v = {}; for (auto& v : tailVoices) v = {}; for (auto& c : rrCounter) c = 0; }

    // Thread de mensagens: entrega um banco novo. O antigo é liberado aqui (nunca no áudio).
    void setBank (std::shared_ptr<const InstrumentBank> b)
    {
        std::lock_guard<std::mutex> g (bankLock);
        retired.clear();
        retired.reserve (8);
        pendingBank = std::move (b);
        bankDirty.store (true);
    }
    // Thread de mensagens: libera bancos aposentados (chamar de um timer).
    void collectGarbage()
    {
        if (bankDirty.load()) return;
        std::unique_lock<std::mutex> g (bankLock, std::try_to_lock);
        if (g.owns_lock()) retired.clear();
    }
    bool hasBank() const { return bankReady.load(); }
    std::string bankDescription() const { std::lock_guard<std::mutex> g (bankLock); return bank ? bank->description : std::string(); }

    // chamado no áudio no início do bloco
    void syncBank()
    {
        if (! bankDirty.load()) return;
        std::unique_lock<std::mutex> g (bankLock, std::try_to_lock);
        if (! g.owns_lock()) return;
        for (auto& v : voices) v = {};   // as vozes apontam para o banco antigo
        for (auto& v : tailVoices) v = {};
        retired.push_back (bank);
        bank = std::move (pendingBank);
        pendingBank.reset();
        bankReady.store (bank != nullptr);
        bankDirty.store (false);
    }

    void noteOn (int section, int artic, int note, float vel01, float dynamics01, int64_t when, uint8_t lane)
    {
        if (section < 0 || section >= 6 || artic < 0 || artic >= 5) return;
        if (! bank) return;
        int a = artic;
        if (a == 1 && ! bank->present[section][1]) a = 0; // sem amostras de legato próprias → sustain
        if (! bank->present[section][a])
        {
            // substituições: trêmolo → sustain; spiccato → pizz; seção ausente → vizinha
            if (a == 4 && bank->present[section][0]) a = 0;
            else if (a == 2 && bank->present[section][3]) a = 3;
            else if (a == 3 && bank->present[section][2]) a = 2;
            else return;
        }
        const auto& inst = bank->inst[section][a];
        auto& v = allocate (section, artic, note);
        int& rr = rrCounter[(size_t) (section * 5 + a)];
        // sustains seguem os controles de ataque/release; legato tem ataque mínimo de 60 ms para emendar
        float att = -1.0f, rel = -1.0f;
        if (! inst.oneShot)
        {
            att = sustainAttack > 0 ? sustainAttack : -1.0f;
            if (artic == 1) att = std::max (0.06f, att > 0 ? att * 0.5f : 0.06f);
            rel = sustainRelease > 0 ? sustainRelease : -1.0f;
        }
        v.start (inst, note, vel01, dynamics01, rr++, sr, section, artic, when, att, rel);
        v.lane = lane;
    }

    void noteOff (int section, int artic, int note, uint8_t lane)
    {
        for (auto& v : voices)
            if (v.active() && ! v.releasing() && v.section == section && v.artic == artic && v.note == note && v.lane == lane)
            { v.noteOff(); break; }
    }

    void allOff() { for (auto& v : voices) if (v.active()) v.noteOff(); }
    void kill() { for (auto& v : voices) v = {}; for (auto& v : tailVoices) v = {}; }

    float sustainAttack = -1.0f, sustainRelease = -1.0f;   // s (≤ 0 = padrão do instrumento)

    void setDynamics (float d) { for (auto& v : voices) if (v.active()) v.setDynamics (d); }

    // Renderiza em buffers por seção (para o mixer/balanço). sectionBuffers[s][0/1].
    void render (float* const* sectionL, float* const* sectionR, int start, int num)
    {
        for (auto& v : voices)
            if (v.active() && v.section >= 0) v.render (sectionL[v.section], sectionR[v.section], start, num);
        for (auto& v : tailVoices)
            if (v.active() && v.section >= 0) v.render (sectionL[v.section], sectionR[v.section], start, num);
    }

    int activeVoices() const { int n = 0; for (auto& v : voices) if (v.active()) ++n; return n; }

private:
    SamplerVoice& allocate (int section, int artic, int note)
    {
        // mesma nota já soando nessa seção/articulação → solta (evita empilhar)
        for (auto& v : voices)
            if (v.active() && v.section == section && v.artic == artic && v.note == note && ! v.releasing()) v.noteOff();
        for (auto& v : voices) if (! v.active()) return v;
        // rouba a mais antiga em release, senão a mais antiga
        SamplerVoice* victim = nullptr;
        for (auto& v : voices) if (v.releasing() && (! victim || v.startedAt < victim->startedAt)) victim = &v;
        if (! victim) for (auto& v : voices) if (! victim || v.startedAt < victim->startedAt) victim = &v;
        victim->steal (sr);
        // a vítima termina o fade de 5 ms num slot de cauda
        for (auto& v : tailVoices) if (! v.active()) { v = *victim; break; }
        *victim = SamplerVoice();
        return *victim;
    }

    double sr = 44100;
    SamplerVoice voices[kMaxVoices];
    SamplerVoice tailVoices[8];
    int rrCounter[30] = {};
    std::shared_ptr<const InstrumentBank> bank, pendingBank;
    std::vector<std::shared_ptr<const InstrumentBank>> retired;
    mutable std::mutex bankLock;
    std::atomic<bool> bankDirty { false }, bankReady { false };
};
} // namespace sms
