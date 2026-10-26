// SeniorMy Acoustic Guitars — DSP: reverb de sala (FDN 8 linhas com reflexões iniciais),
// biquads (RBJ) e utilidades.
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
// Biquad (fórmulas RBJ), forma direta transposta II.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    { b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0); a1 = (float) (na1 / na0); a2 = (float) (na2 / na0); }
    void lowpass (double f, double q, double sr)
    { const double w = 2 * 3.141592653589793 * std::min (f, sr * 0.45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass (double f, double q, double sr)
    { const double w = 2 * 3.141592653589793 * std::min (f, sr * 0.45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void bandpass (double f, double q, double sr)   // ganho 1 no pico
    { const double w = 2 * 3.141592653589793 * std::min (f, sr * 0.45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      set (al, 0, -al, 1 + al, -2 * c, 1 - al); }
    void peak (double f, double q, double dB, double sr)
    { const double A = std::pow (10.0, dB / 40), w = 2 * 3.141592653589793 * std::min (f, sr * 0.45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
      set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
    void lowShelf (double f, double dB, double sr)
    { const double A = std::pow (10.0, dB / 40), w = 2 * 3.141592653589793 * f / sr, c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0), k = 2 * std::sqrt (A) * al;
      set (A * ((A + 1) - (A - 1) * c + k), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - k),
           (A + 1) + (A - 1) * c + k, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - k); }
    void highShelf (double f, double dB, double sr)
    { const double A = std::pow (10.0, dB / 40), w = 2 * 3.141592653589793 * std::min (f, sr * 0.45) / sr, c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0), k = 2 * std::sqrt (A) * al;
      set (A * ((A + 1) + (A - 1) * c + k), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - k),
           (A + 1) - (A - 1) * c + k, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - k); }
};

// Gerador pseudoaleatório leve (determinístico, seguro para a thread de áudio).
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) / 16777216.0f; }          // 0..1
    float bi() { return uni() * 2.0f - 1.0f; }                           // -1..1
};

} // namespace sms
