// SeniorMy Structure — DSP: reverb de sala (FDN 8 linhas com reflexões iniciais),
// piano interno sintetizado (modal, leve) e utilidades.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace sms
{
//==============================================================================
struct OnePole
{
    float z = 0, a = 0.5f;
    void setCutoff (float hz, float sr) { a = 1.0f - std::exp (-2.0f * 3.14159265f * hz / sr); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
};

class DelayLine
{
public:
    void resize (int n) { buf.assign ((size_t) std::max (2, n), 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    float tap (float delay) const   // atraso fracionário (≥ 1)
    {
        const int n = (int) buf.size();
        float rp = (float) w - delay;
        while (rp < 0) rp += (float) n;
        const int i0 = (int) rp;
        const float f = rp - (float) i0;
        const float a = buf[(size_t) (i0 % n)], b = buf[(size_t) ((i0 + 1) % n)];
        return a + (b - a) * f;
    }
    int size() const { return (int) buf.size(); }
private:
    std::vector<float> buf;
    int w = 0;
};

//==============================================================================
// Reverb de sala de concerto: pré-delay, 12 reflexões iniciais estéreo, FDN 8×8 (Householder)
// com amortecimento por linha (T60 dependente da frequência), modulação lenta anti-metálica.
class HallReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        pre.resize ((int) (0.25f * sr) + 4);
        early.resize ((int) (0.12f * sr) + 4);
        static const float baseMs[8] = { 37.1f, 41.9f, 47.3f, 53.7f, 59.3f, 67.1f, 73.9f, 83.3f };
        for (int i = 0; i < 8; ++i)
        {
            lines[(size_t) i].resize ((int) (baseMs[i] * 0.001f * sr * 2.2f) + 64);
            lenBase[(size_t) i] = baseMs[i] * 0.001f * sr;
            damp[(size_t) i] = {};
            phase[(size_t) i] = (float) i * 0.7f;
        }
        inHp.setCutoff (90.0f, sr);
        inHp2 = inHp;
        update();
        clear();
    }
    void clear()
    {
        pre.clear(); early.clear();
        for (auto& l : lines) l.clear();
        for (auto& d : damp) d.z = 0;
    }

    // size 0..1 (sala de câmara → grande sala), decay 0..1, damping 0..1, predelay ms
    void set (float size01, float decay01, float damping01, float predelayMs, float width01)
    {
        size = std::clamp (size01, 0.0f, 1.0f);
        decay = std::clamp (decay01, 0.0f, 1.0f);
        damping = std::clamp (damping01, 0.0f, 1.0f);
        preMs = std::clamp (predelayMs, 0.0f, 200.0f);
        width = std::clamp (width01, 0.0f, 1.0f);
        update();
    }

    // processa em lugar: devolve só o sinal molhado em wetL/wetR
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n)
    {
        static const int erTapsL[6] = { 7, 13, 19, 29, 43, 61 };
        static const int erTapsR[6] = { 9, 17, 23, 37, 47, 71 };
        static const float erGain[6] = { 0.62f, 0.5f, 0.42f, 0.33f, 0.26f, 0.2f };
        for (int i = 0; i < n; ++i)
        {
            const float mono = inHp.hp (0.5f * (inL[i] + inR[i]));
            pre.push (mono);
            const float x = pre.tap (std::max (1.0f, preMs * 0.001f * sr));
            early.push (x);
            float eL = 0, eR = 0;
            const float erScale = (0.5f + size) * 0.001f * sr;
            for (int k = 0; k < 6; ++k)
            {
                eL += early.tap (std::max (1.0f, (float) erTapsL[k] * erScale)) * erGain[k];
                eR += early.tap (std::max (1.0f, (float) erTapsR[k] * erScale)) * erGain[k];
            }

            // FDN
            float o[8];
            for (int k = 0; k < 8; ++k)
            {
                phase[(size_t) k] += modRate;
                if (phase[(size_t) k] > 6.2831853f) phase[(size_t) k] -= 6.2831853f;
                const float d = len[(size_t) k] + modDepth * std::sin (phase[(size_t) k]);
                o[k] = damp[(size_t) k].lp (lines[(size_t) k].tap (d)) * fb[(size_t) k];
            }
            float sum = 0;
            for (int k = 0; k < 8; ++k) sum += o[k];
            sum *= 0.25f;   // Householder: x - 2/N * soma
            const float inj = (x * 0.6f + (eL + eR) * 0.2f);
            for (int k = 0; k < 8; ++k)
                lines[(size_t) k].push (o[k] - sum + inj * ((k & 1) ? 0.5f : -0.5f));
            const float lateL = o[0] + o[2] + o[4] + o[6];
            const float lateR = o[1] + o[3] + o[5] + o[7];
            const float m = 0.5f * (lateL + lateR), sd = 0.5f * (lateL - lateR) * width;
            wetL[i] = (m + sd) * 0.5f + eL * 0.35f;
            wetR[i] = (m - sd) * 0.5f + eR * 0.35f;
        }
    }

private:
    void update()
    {
        const float scale = 0.55f + size * 1.6f;
        const float t60 = 0.6f + decay * 5.4f;   // s
        for (int i = 0; i < 8; ++i)
        {
            len[(size_t) i] = std::min ((float) lines[(size_t) i].size() - 40.0f, lenBase[(size_t) i] * scale);
            fb[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / (t60 * sr));
            damp[(size_t) i].setCutoff (1800.0f + (1.0f - damping) * 9000.0f, sr);
        }
        modRate = 2.0f * 3.14159265f * 0.35f / sr;
        modDepth = 0.0012f * sr * 0.5f;
    }

    float sr = 44100;
    DelayLine pre, early;
    std::array<DelayLine, 8> lines;
    std::array<float, 8> lenBase {}, len {}, fb {}, phase {};
    std::array<OnePole, 8> damp {};
    OnePole inHp, inHp2;
    float size = 0.6f, decay = 0.5f, damping = 0.4f, preMs = 20, width = 1.0f, modRate = 0, modDepth = 0;
};

//==============================================================================
// Piano interno: síntese modal leve. Cada nota = parciais inarmônicas (B), duas cordas levemente
// desafinadas (batimento), decaimento em dois estágios, brilho por velocity e ruído de martelo.
class PianoSynth
{
public:
    static constexpr int kVoices = 24, kPartials = 14;

    void prepare (double sampleRate) { sr = (float) sampleRate; for (auto& v : voices) v.active = false; body.setCutoff (5200.0f, sr); }

    void noteOn (int note, float vel01)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.note == note) { v = &x; break; }
        if (! v) for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (! v) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
        v->start (note, vel01, sr);
        for (auto& x : voices) ++x.age;
        v->age = 0;
    }
    void noteOff (int note)
    {
        for (auto& v : voices) if (v.active && v.note == note) { if (sustainPedal) v.pedalHeld = true; else v.release (sr); }
    }
    void setSustain (bool down)
    {
        sustainPedal = down;
        if (! down) for (auto& v : voices) if (v.active && v.pedalHeld) { v.pedalHeld = false; v.release (sr); }
    }
    void allOff() { for (auto& v : voices) v.active = false; }

    void render (float* L, float* R, int n, float gain)
    {
        for (auto& v : voices)
            if (v.active) v.render (L, R, n, gain);
    }

private:
    struct Voice
    {
        bool active = false, pedalHeld = false, released = false;
        int note = 60, age = 0;
        float re[kPartials][2] {}, im[kPartials][2] {}, cs[kPartials][2] {}, sn[kPartials][2] {};
        float amp[kPartials] {}, dec[kPartials] {}, dec2[kPartials] {};
        float relMul = 1.0f, env = 1.0f, noise = 0, noiseDec = 0, pan = 0;
        int np = 0, sample = 0, knee = 0;
        uint32_t seed = 1;

        void start (int n, float vel, float sr)
        {
            note = n; active = true; released = false; pedalHeld = false; env = 1.0f; relMul = 1.0f; sample = 0;
            const float f0 = 440.0f * std::pow (2.0f, (float) (n - 69) / 12.0f);
            const float B = 0.00008f * std::pow (2.0f, (float) (n - 21) / 24.0f);   // inarmonicidade
            np = 0;
            const float bright = 0.35f + 0.65f * vel;
            const float t60base = std::clamp (14.0f * std::pow (0.5f, (float) (n - 21) / 18.0f), 0.6f, 16.0f);
            for (int k = 0; k < kPartials; ++k)
            {
                const float kk = (float) (k + 1);
                const float f = f0 * kk * std::sqrt (1.0f + B * kk * kk);
                if (f > sr * 0.45f) break;
                // posição do martelo (~1/8 da corda) zera parciais múltiplos de 8
                const float hammer = std::fabs (std::sin (3.14159265f * kk / 8.3f));
                amp[k] = hammer * std::pow (kk, -(1.6f - 0.9f * bright)) * (0.6f + 0.4f * vel);
                const float t60 = t60base / (1.0f + 0.35f * (kk - 1.0f));
                dec[k] = std::pow (10.0f, -3.0f / (t60 * 0.35f * sr));     // estágio rápido
                dec2[k] = std::pow (10.0f, -3.0f / (t60 * sr));            // estágio lento
                for (int s = 0; s < 2; ++s)
                {
                    const float detune = (s == 0 ? -0.6f : 0.6f) * (0.6f + 0.05f * kk);   // cents
                    const float w = 2.0f * 3.14159265f * f * std::pow (2.0f, detune / 1200.0f) / sr;
                    cs[k][s] = std::cos (w); sn[k][s] = std::sin (w);
                    re[k][s] = 0.0f; im[k][s] = 1.0f;
                }
                ++np;
            }
            knee = (int) (0.12f * sr);
            noise = 0.12f * vel * vel; noiseDec = std::pow (10.0f, -3.0f / (0.03f * sr));
            pan = std::clamp ((float) (n - 64) / 40.0f, -0.7f, 0.7f);
            seed = 0x9E3779B9u ^ (uint32_t) n;
        }
        void release (float sr) { released = true; relMul = std::pow (10.0f, -3.0f / ((note > 88 ? 0.8f : 0.25f) * sr)); }

        void render (float* L, float* R, int n, float gain)
        {
            const float pl = std::sqrt (0.5f * (1.0f - pan)), pr = std::sqrt (0.5f * (1.0f + pan));
            for (int i = 0; i < n; ++i)
            {
                float s = 0;
                const bool second = sample > knee;
                for (int k = 0; k < np; ++k)
                {
                    const float d = second ? dec2[k] : dec[k];
                    amp[k] *= d;
                    for (int c = 0; c < 2; ++c)
                    {
                        const float r = re[k][c] * cs[k][c] - im[k][c] * sn[k][c];
                        const float m = re[k][c] * sn[k][c] + im[k][c] * cs[k][c];
                        re[k][c] = r; im[k][c] = m;
                        s += m * amp[k];
                    }
                }
                seed = seed * 1664525u + 1013904223u;
                s += noise * ((float) (seed >> 9) / 4194304.0f - 1.0f);
                noise *= noiseDec;
                if (released) env *= relMul;
                const float o = s * env * gain * 0.18f;
                L[i] += o * pl; R[i] += o * pr;
                ++sample;
            }
            // renormaliza os osciladores (evita deriva numérica)
            for (int k = 0; k < np; ++k)
                for (int c = 0; c < 2; ++c)
                {
                    const float mag = std::sqrt (re[k][c] * re[k][c] + im[k][c] * im[k][c]);
                    if (mag > 0) { re[k][c] /= mag; im[k][c] /= mag; }
                }
            if (env < 0.0003f || (amp[0] < 0.00005f && sample > knee)) active = false;
        }
    };

    float sr = 44100;
    Voice voices[kVoices];
    OnePole body;
    bool sustainPedal = false;
};

//==============================================================================
// Limitador suave de saída (proteção contra picos).
struct SoftClip
{
    static inline float process (float x)
    {
        const float a = std::fabs (x);
        if (a < 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
        return x < 0 ? -y : y;
    }
};
} // namespace sms
