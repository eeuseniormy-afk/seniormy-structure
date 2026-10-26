// SeniorMy Acoustic Guitars — vozes de corda: reprodução de amostras (interpolação cúbica,
// round robin real ou por amostra vizinha, brilho por velocity, abafamento com a palma, chuck)
// e corda modelada (guia de onda Karplus-Strong estendido: deslocamento triangular na posição
// do ataque + ruído de palheta, perda dependente da frequência, afinação fracionária por
// passa-tudo). O corpo modal dos instrumentos modelados fica no barramento da parte.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Instruments.h"
#include "Dsp.h"
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <cmath>
#include <cstdint>

namespace sms
{
struct SampleData
{
    std::vector<int16_t> d;       // mono, 16 bits (dither TPDF na conversão)
    double sampleRate = 44100;
    int frames() const { return (int) d.size(); }
    void assign (const float* x, int n)
    {
        d.resize ((size_t) n);
        uint32_t seed = 0x1234567u;
        for (int i = 0; i < n; ++i)
        {
            seed = seed * 1664525u + 1013904223u; const float d1 = (float) (seed >> 8) / 16777216.0f;
            seed = seed * 1664525u + 1013904223u; const float d2 = (float) (seed >> 8) / 16777216.0f;
            d[(size_t) i] = (int16_t) std::clamp ((int) std::lround (x[i] * 32767.0f + (d1 - d2)), -32768, 32767);
        }
    }
};

struct Zone
{
    std::shared_ptr<const SampleData> data;
    int root = 60, lo = 0, hi = 127, rr = 0;
    float tuneCents = 0, gain = 1;
};

struct SampleSet
{
    std::vector<Zone> zones;
    int numRR = 1;
    float gainDb = 0;
    // zona para a nota (round robin real, ou "pseudo" com a amostra vizinha quando só há uma)
    const Zone* find (int note, int rrIndex) const
    {
        const Zone* best = nullptr; int bestScore = 1 << 30;
        const int rr = numRR > 1 ? rrIndex % numRR : 0;
        for (const auto& z : zones)
        {
            int s = 0;
            if (note < z.lo) s += (z.lo - note) * 100; else if (note > z.hi) s += (note - z.hi) * 100;
            s += std::abs (z.root - note) * 3;
            if (z.rr != rr) s += 40;
            if (s < bestScore) { bestScore = s; best = &z; }
        }
        if (best && numRR == 1 && (rrIndex & 1))
        {
            // vizinha (≤ 2 semitons) como variação de timbre
            const Zone* alt = nullptr; int altD = 99;
            for (const auto& z : zones)
                if (&z != best) { const int dd = std::abs (z.root - note); if (dd <= 2 && dd < altD) { altD = dd; alt = &z; } }
            if (alt) return alt;
        }
        return best;
    }
};

struct Bank
{
    std::map<std::string, SampleSet> sets;
    std::shared_ptr<const SampleData> bodyTap;
    std::string status;
    const SampleSet* set (const char* name) const
    {
        if (! name || ! *name) return nullptr;
        auto it = sets.find (name);
        return (it == sets.end() || it->second.zones.empty()) ? nullptr : &it->second;
    }
};

//==============================================================================
struct VoiceStart
{
    int part = 0, course = 0, note = 60;
    float vel = 0.8f, mute = 0, gain = 1, pan = 0, cents = 0;
    uint8_t kind = 0;       // PluckKind
    int rr = 0;
    float decayScale = 1, release = 0.15f;
    bool upStroke = false;
};

class StringVoice
{
public:
    enum Type { Off, Sample, Model, Tap };
    Type type = Off;
    int part = -1, course = -1, note = 0;
    uint64_t age = 0;
    bool releasing = false;

    void prepareModel (int maxLen) { line.assign ((size_t) maxLen, 0.0f); }

    void startSample (const VoiceStart& v, const Zone& z, float setGainDb, double sr)
    {
        common (v, sr);
        type = Sample; zone = &z;
        pos = 0;
        inc = std::pow (2.0, ((double) (v.note - z.root) * 100.0 + z.tuneCents + v.cents) / 1200.0) * z.data->sampleRate / sr;
        amp = 0.45f * v.gain * z.gain * std::pow (10.0f, setGainDb / 20.0f) * velCurve (v.vel);
        // brilho por velocity e abafamento
        float fc = 1400.0f * std::pow (2.0f, v.vel * 3.6f);
        if (v.kind == 1) fc = 900.0f + 500.0f * v.vel;
        fc *= (1.0f - 0.85f * v.mute);
        lp.lowpass (std::max (300.0f, std::min (fc, (float) sr * 0.45f)), 0.6, sr);
        lp2 = lp;
        useLp = fc < sr * 0.4;
    }

    void startModel (const VoiceStart& v, const ModelParams& m, double sr, Rng& rnd)
    {
        common (v, sr);
        type = Model;
        const double f0 = 440.0 * std::pow (2.0, (v.note - 69 + v.cents / 100.0) / 12.0);
        const double period = sr / f0;
        lossS = 0.5f * (1.0f - std::clamp (m.brightness * (0.75f + 0.35f * v.vel) * (1.0f - 0.8f * v.mute), 0.0f, 0.97f));
        const double D = period - lossS;                  // o filtro de perda atrasa lossS amostra
        int N = (int) std::floor (D - 0.1);
        N = std::clamp (N, 4, (int) line.size() - 2);
        double frac = D - N;                               // 0,1 … 1,1
        apC = (float) ((1.0 - frac) / (1.0 + frac));
        apX1 = apY1 = 0;
        len = N; w = 0;
        // decaimento: interpola grave/agudo pela altura
        const float t = std::clamp ((float) (v.note - 40) / 44.0f, 0.0f, 1.0f);
        float t60 = (m.t60Low * (1 - t) + m.t60High * t) * v.decayScale;
        if (v.kind == 1) t60 = 0.05f; else t60 = t60 * (1.0f - v.mute) + 0.14f * v.mute;
        loopGain = (float) std::pow (10.0, -3.0 / (f0 * std::max (0.03f, t60)));
        lossX1 = 0;
        // excitação: deslocamento triangular (posição do ataque) + ruído filtrado pela velocity
        const float pp = std::clamp (m.pickPos * (1.0f + 0.15f * rnd.bi()), 0.05f, 0.45f);
        const int peak = std::max (1, (int) (pp * N));
        OnePole nf; nf.setCutoff (1200.0f + 9000.0f * v.vel * v.vel * (1.0f - 0.7f * v.mute), (float) sr);
        float mean = 0;
        for (int i = 0; i < N; ++i)
        {
            const float tri = i < peak ? (float) i / (float) peak : (float) (N - i) / (float) (N - peak);
            const float nz = nf.lp (rnd.bi()) * m.noise * 2.0f;
            line[(size_t) i] = tri + nz; mean += line[(size_t) i];
        }
        mean /= (float) N;
        for (int i = 0; i < N; ++i) line[(size_t) i] -= mean;
        // brilho inicial: suaviza o triângulo conforme a velocity (dedo × palheta)
        const int passes = v.vel > 0.75f ? 0 : (v.vel > 0.45f ? 1 : 2);
        for (int p = 0; p < passes + (v.mute > 0.4f ? 2 : 0); ++p)
        {
            float prev = line[(size_t) (N - 1)];
            for (int i = 0; i < N; ++i) { const float cur = line[(size_t) i]; line[(size_t) i] = 0.5f * (cur + prev); prev = cur; }
        }
        amp = v.gain * velCurve (v.vel) * 0.8f;
        clickLeft = (int) (0.004 * sr); clickAmp = amp * m.noise * 0.6f * v.vel;
        clickHp.setCutoff (2500.0f, (float) sr);
        silentCount = 0;
    }

    void startTap (const VoiceStart& v, std::shared_ptr<const SampleData> tap, double sr)
    {
        common (v, sr);
        type = Tap; tapData = std::move (tap); pos = 0;
        inc = tapData ? tapData->sampleRate / sr : 1.0;
        amp = v.gain * velCurve (v.vel) * 0.9f;
        thumpPhase = 0; thumpEnv = 1;
        squeak = v.course == -2;
        if (squeak) { tapData = nullptr; sqF = 3800.0f; amp = v.gain * v.vel * 0.5f; }
    }

    void release (float seconds)
    {
        if (type == Off || releasing) return;
        releasing = true;
        relMul = std::exp (-6.9f / std::max (1.0f, seconds * (float) rate));
        if (type == Model)
        {
            // dedo abafando a corda: perda forte e escura
            loopGain = std::min (loopGain, 0.985f); lossS = 0.5f;
        }
    }
    void kill (double sr) { release (0.008f); relMul = std::exp (-6.9f / (float) (0.008 * sr)); }

    // soma em L/R
    void render (float* L, float* R, int start, int n)
    {
        if (type == Off) return;
        for (int i = start; i < start + n; ++i)
        {
            if (delay > 0) { --delay; continue; }
            float x = 0;
            if (type == Sample)
            {
                const int fr = zone->data->frames();
                const int ip = (int) pos;
                if (ip >= fr - 3) { type = Off; return; }
                x = cubic (zone->data->d.data(), ip, (float) (pos - ip)) * (1.0f / 32768.0f);
                pos += inc;
                if (useLp) x = lp2.process (lp.process (x));
            }
            else if (type == Model)
            {
                // leitura do fim da linha (atraso N), perda, passa-tudo fracionário, realimentação
                const float y0 = line[(size_t) w];
                const float lo = loopGain * ((1.0f - lossS) * y0 + lossS * lossX1);
                lossX1 = y0;
                const float ap = apC * lo + apX1 - apC * apY1;
                apX1 = lo; apY1 = ap;
                line[(size_t) w] = ap;
                if (++w >= len) w = 0;
                x = y0;
                if (clickLeft > 0) { x += clickHp.hp (rng.bi()) * clickAmp; --clickLeft; }
                if (std::abs (x) < 1e-5f) { if (++silentCount > 2048) { type = Off; return; } } else silentCount = 0;
            }
            else // Tap
            {
                if (squeak)
                {
                    // chiado de traste: ruído em banda estreita que desce (dedo deslizando na corda enrolada)
                    sqF *= 0.99985f;
                    sqBp.bandpass (sqF, 6.0, rate);
                    x = sqBp.process (rng.bi()) * thumpEnv * 3.0f;
                    thumpEnv *= 0.99975f;
                    if (thumpEnv < 1e-3f) { type = Off; return; }
                }
                else if (tapData)
                {
                    const int ip = (int) pos;
                    if (ip >= tapData->frames() - 3) { type = Off; return; }
                    x = cubic (tapData->d.data(), ip, (float) (pos - ip)) * (1.0f / 32768.0f);
                    pos += inc;
                }
                else
                {
                    thumpPhase += 2.0f * 3.14159265f * 140.0f / (float) rate;
                    x = (std::sin (thumpPhase) * 0.8f + rng.bi() * 0.2f * thumpEnv) * thumpEnv;
                    thumpEnv *= 0.9992f;
                    if (thumpEnv < 1e-3f) { type = Off; return; }
                }
            }
            // envelope
            if (atk < 1.0f) { atk = std::min (1.0f, atk + atkInc); }
            env *= decMul;
            if (releasing) { env *= relMul; if (env < 1e-4f) { type = Off; return; } }
            const float s = x * amp * env * atk;
            L[i] += s * panL; R[i] += s * panR;
        }
    }

private:
    static float velCurve (float v) { return 0.12f + 0.88f * std::pow (std::clamp (v, 0.0f, 1.0f), 1.6f); }
    static float cubic (const int16_t* d, int i, float t)
    {
        const float y0 = d[i > 0 ? i - 1 : 0], y1 = d[i], y2 = d[i + 1], y3 = d[i + 2];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    void common (const VoiceStart& v, double sr)
    {
        rate = sr; part = v.part; course = v.course; note = v.note; releasing = false;
        env = 1; atk = 0; atkInc = 1.0f / (float) std::max (1.0, 0.0008 * sr);
        relMul = 1;
        float tau = 0;   // decaimento extra (s)
        if (v.decayScale < 0.999f) tau = 3.0f * v.decayScale;
        if (v.mute > 0.01f) { const float tm = 0.09f + (1.0f - v.mute) * 1.5f; tau = tau > 0 ? std::min (tau, tm) : tm; }
        if (v.kind == 1) tau = 0.03f;
        decMul = tau > 0 ? std::exp (-1.0f / (tau * (float) sr)) : 1.0f;
        const float p = std::clamp (v.pan, -1.0f, 1.0f);
        panL = std::cos ((p + 1.0f) * 0.25f * 3.14159265f) * 1.4142f;
        panR = std::sin ((p + 1.0f) * 0.25f * 3.14159265f) * 1.4142f;
        delay = 0;
        rng.s ^= (uint32_t) (v.note * 2654435761u + (uint32_t) v.course * 40503u + 1u);
    }

    double rate = 48000;
    const Zone* zone = nullptr;
    std::shared_ptr<const SampleData> tapData;
    double pos = 0, inc = 1;
    float amp = 1, env = 1, atk = 1, atkInc = 1, decMul = 1, relMul = 1, panL = 1, panR = 1;
    Biquad lp, lp2; bool useLp = false;
    // modelo
    std::vector<float> line;
    int len = 100, w = 0, clickLeft = 0, silentCount = 0;
    float loopGain = 0.99f, lossS = 0.3f, lossX1 = 0, apC = 0, apX1 = 0, apY1 = 0, clickAmp = 0;
    OnePole clickHp;
    float thumpPhase = 0, thumpEnv = 0, sqF = 3000;
    bool squeak = false;
    Biquad sqBp;
    Rng rng;
public:
    int delay = 0;   // amostras até começar (dentro do bloco)
    uint64_t startTime = 0;
};
} // namespace sms
