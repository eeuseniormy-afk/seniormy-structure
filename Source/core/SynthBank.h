// SeniorMy Structure — banco reserva sintetizado (usado só quando a biblioteca de amostras
// não está instalada, e nos testes). Ensemble de dente-de-serra limitado em banda com
// desafinação, filtro de corpo, pizzicato por Karplus–Strong. Gerado inteiramente pelo Titular.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Sampler.h"
#include "Arranger.h"
#include <memory>
#include <vector>
#include <cmath>

namespace sms
{
namespace synthbank
{
inline std::vector<float> bandlimitedSawTable (float f0, float sr, int size)
{
    std::vector<float> t ((size_t) size, 0.0f);
    const int maxK = std::max (1, (int) (sr * 0.45f / f0));
    for (int k = 1; k <= std::min (maxK, 80); ++k)
    {
        // rolagem suave nos agudos (corda com arco, não serra pura)
        const float a = (1.0f / (float) k) * (1.0f / (1.0f + std::pow ((float) k * f0 / 4500.0f, 2.0f)));
        for (int i = 0; i < size; ++i) t[(size_t) i] += a * std::sin (2.0f * 3.14159265f * (float) k * (float) i / (float) size);
    }
    float mx = 1e-9f;
    for (float v : t) mx = std::max (mx, std::fabs (v));
    for (auto& v : t) v /= mx;
    return t;
}

struct FloatStereo { std::vector<float> left, right; };

inline std::shared_ptr<SampleData> toSample (const FloatStereo& f, float sr)
{
    auto d = std::make_shared<SampleData>();
    d->sampleRate = sr;
    d->assign (f.left.data(), f.right.data(), (int) f.left.size());
    return d;
}

inline std::shared_ptr<SampleData> renderEnsemble (int root, float sr, float seconds, int voices, int artic, uint32_t seed)
{
    FloatStereo buf;
    FloatStereo* d = &buf;
    const int n = (int) (seconds * sr);
    d->left.assign ((size_t) n, 0.0f); d->right.assign ((size_t) n, 0.0f);
    const float f0 = 440.0f * std::pow (2.0f, (float) (root - 69) / 12.0f);
    const int T = 2048;
    const auto table = bandlimitedSawTable (f0, sr, T);
    auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };

    if (artic == Pizzicato)
    {
        // Karplus–Strong com dois "músicos"
        for (int v = 0; v < 2; ++v)
        {
            const float f = f0 * std::pow (2.0f, (rnd() - 0.5f) * 0.1f / 12.0f);
            const int L = std::max (2, (int) (sr / f));
            std::vector<float> line ((size_t) L);
            for (auto& x : line) x = rnd() * 2.0f - 1.0f;
            float lp = 0;
            int p = 0;
            const float damp = 0.996f - 0.002f * (float) (root - 40) / 40.0f;
            const int start = (int) (rnd() * 0.012f * sr);
            for (int i = start; i < n; ++i)
            {
                const float y = line[(size_t) p];
                const int q = (p + 1) % L;
                lp = 0.5f * (line[(size_t) p] + line[(size_t) q]);
                line[(size_t) p] = lp * damp;
                p = q;
                (v == 0 ? d->left : d->right)[(size_t) i] += y * 0.6f;
            }
        }
        return toSample (buf, sr);
    }

    for (int v = 0; v < voices; ++v)
    {
        const float detCents = (rnd() - 0.5f) * 18.0f;
        const float inc = (float) T * f0 * std::pow (2.0f, detCents / 1200.0f) / sr;
        float ph = rnd() * (float) T;
        const float pan = (rnd() - 0.5f) * 1.4f;
        const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
        const float vibRate = 4.8f + rnd() * 1.4f, vibDepth = 0.0025f + rnd() * 0.002f;
        float vibPh = rnd() * 6.28f;
        const float delay = rnd() * 0.04f * sr;
        for (int i = 0; i < n; ++i)
        {
            float env = 1.0f;
            const float t = (float) i / sr;
            if (artic == Spiccato) env = (t < 0.008f ? t / 0.008f : std::exp (-(t - 0.008f) * 14.0f));
            else env = std::min (1.0f, ((float) i - delay) / (0.08f * sr));
            if (env <= 0) continue;
            if (artic == Tremolo) env *= 0.65f + 0.35f * std::sin (2.0f * 3.14159265f * (11.0f + (float) v * 0.37f) * t);
            vibPh += 2.0f * 3.14159265f * vibRate / sr;
            ph += inc * (1.0f + vibDepth * std::sin (vibPh));
            while (ph >= (float) T) ph -= (float) T;
            const int i0 = (int) ph;
            const float fr = ph - (float) i0;
            const float s = table[(size_t) i0] + (table[(size_t) ((i0 + 1) % T)] - table[(size_t) i0]) * fr;
            d->left[(size_t) i] += s * env * gl;
            d->right[(size_t) i] += s * env * gr;
        }
    }
    // filtro de corpo simples (dois polos) + normalização
    float zl = 0, zr = 0, zl2 = 0, zr2 = 0;
    const float a = 1.0f - std::exp (-2.0f * 3.14159265f * std::min (7000.0f, f0 * 9.0f) / sr);
    float mx = 1e-9f;
    for (int i = 0; i < n; ++i)
    {
        zl += a * (d->left[(size_t) i] - zl); zl2 += a * (zl - zl2);
        zr += a * (d->right[(size_t) i] - zr); zr2 += a * (zr - zr2);
        d->left[(size_t) i] = zl2; d->right[(size_t) i] = zr2;
        mx = std::max (mx, std::max (std::fabs (zl2), std::fabs (zr2)));
    }
    const float g = 0.5f / mx;
    for (int i = 0; i < n; ++i) { d->left[(size_t) i] *= g; d->right[(size_t) i] *= g; }
    return toSample (buf, sr);
}
} // namespace synthbank

// Constrói o banco reserva completo (6 seções × 5 articulações). Leva ~1–2 s; chamar fora do áudio.
inline std::shared_ptr<InstrumentBank> makeSynthBank (float sr = 44100.0f)
{
    auto bank = std::make_shared<InstrumentBank>();
    bank->synthesized = true;
    bank->description = "Synth strings (reserva / fallback)";
    static const float secGainDb[6] = { -1.0f, -2.0f, -1.5f, -1.0f, 0.0f, -3.0f };
    static const float secPan[6] = { -0.55f, -0.2f, 0.25f, 0.5f, 0.7f, 0.0f };
    uint32_t seed = 777;
    for (int s = 0; s < numSections; ++s)
    {
        const auto r = sectionRange (s);
        for (int a = 0; a < numArtics; ++a)
        {
            if (a == Legato) continue;   // usa sustain
            auto& inst = bank->inst[s][a];
            inst.numLayers = 1; inst.numRR = 1;
            inst.gainDb = secGainDb[s];
            inst.pan = secPan[s];
            inst.oneShot = (a == Spiccato || a == Pizzicato);
            inst.attack = inst.oneShot ? 0.002f : 0.09f;
            inst.release = inst.oneShot ? 0.15f : 0.5f;
            const float secs = inst.oneShot ? (a == Pizzicato ? 1.6f : 0.5f) : 3.0f;
            for (int root = r.lo + 3; root <= r.hi + 3; root += 7)
            {
                Zone z;
                z.data = synthbank::renderEnsemble (root, sr, secs, s == Cb ? 3 : 5, a, seed += 101);
                z.root = root; z.loKey = root - 3; z.hiKey = root + 3;
                if (! inst.oneShot)
                {
                    z.loopStart = (int) (0.6f * sr);
                    z.loopEnd = z.data->frames() - (int) (0.1f * sr);
                }
                inst.zones.push_back (z);
            }
            if (! inst.zones.empty()) { inst.zones.front().loKey = 0; inst.zones.back().hiKey = 127; }
            bank->present[s][a] = true;
        }
    }
    return bank;
}
} // namespace sms
