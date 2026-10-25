// SeniorMy Structure — análise de áudio em tempo real: FFT, cromagrama (pitch-class profile),
// croma do baixo, rastreador de acordes com histerese e estimativa de tonalidade.
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Harmony.h"
#include <complex>
#include <vector>
#include <cstring>

namespace sms
{
//==============================================================================
// FFT radix-2 iterativa (complexa), tabelas pré-calculadas; sem alocação no processamento.
class FFT
{
public:
    void init (int order)
    {
        n = 1 << order;
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i) tw[(size_t) i] = std::polar (1.0f, -2.0f * 3.14159265358979f * (float) i / (float) n);
    }
    int size() const { return n; }

    void perform (std::complex<float>* d) const
    {
        for (int i = 0; i < n; ++i) if (i < rev[(size_t) i]) std::swap (d[i], d[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const auto w = tw[(size_t) (j * step)];
                    const auto u = d[i + j], v = d[i + j + half] * w;
                    d[i + j] = u + v;
                    d[i + j + half] = u - v;
                }
        }
    }

private:
    int n = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

//==============================================================================
// Cromagrama: janela Hann, espectro de magnitude, branqueamento espectral (remove a média local
// em escala log), mapeamento log-frequência para 12 classes com ponderação gaussiana em cents,
// supressão de harmônicos simples (o 3º e o 5º harmônicos de cada nota forte são atenuados).
class ChromaAnalyzer
{
public:
    void prepare (double sampleRate, int fftOrder = 13)
    {
        sr = sampleRate;
        // em taxas altas aumenta a FFT para manter a resolução em Hz
        int order = fftOrder;
        if (sr > 50000) ++order;
        if (sr > 100000) ++order;
        fft.init (order);
        N = fft.size();
        hop = N / 4;
        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * 3.14159265f * (float) i / (float) (N - 1));
        ring.assign ((size_t) N, 0.0f);
        buf.assign ((size_t) N, {});
        mag.assign ((size_t) N / 2, 0.0f);
        white.assign ((size_t) N / 2, 0.0f);
        prefix.assign ((size_t) N / 2 + 1, 0.0f);
        writePos = 0; sinceLast = 0;
        buildMap();
        chroma.fill (0); bassChroma.fill (0);
        rms = 0;
    }

    // Retorna true quando um novo quadro de croma foi calculado.
    bool push (const float* mono, int num)
    {
        bool fresh = false;
        for (int i = 0; i < num; ++i)
        {
            ring[(size_t) writePos] = mono[i];
            writePos = (writePos + 1) % N;
            if (++sinceLast >= hop) { sinceLast = 0; computeFrame(); fresh = true; }
        }
        return fresh;
    }

    // Análise de um quadro já pronto (usado na importação offline).
    void analyseFrame (const float* frame /* N amostras */)
    {
        for (int i = 0; i < N; ++i) ring[(size_t) i] = frame[i];
        writePos = 0;
        computeFrame();
    }

    int frameSize() const { return N; }
    int hopSize() const { return hop; }
    int getHopSize() const { return hop; }
    const Chroma& getChroma() const { return chroma; }
    const Chroma& getBassChroma() const { return bassChroma; }
    float getRms() const { return rms; }

private:
    struct BinMap { int bin; int pitch; float w; };

    void buildMap()
    {
        map.clear(); bassMap.clear();
        const float binHz = (float) sr / (float) N;
        for (int b = 1; b < N / 2; ++b)
        {
            const float f = (float) b * binHz;
            if (f < 50.0f || f > 4200.0f) continue;
            const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
            const int nearest = (int) std::lround (midi);
            for (int m = nearest - 1; m <= nearest + 1; ++m)
            {
                const float dc = (midi - (float) m) * 100.0f;   // desvio em cents
                const float w = std::exp (-0.5f * (dc * dc) / (35.0f * 35.0f));
                if (w < 0.02f) continue;
                // peso por região: graves contam menos no croma geral (baixo vai para bassChroma)
                float region = 1.0f;
                if (f < 110.0f) region = 0.5f;
                else if (f > 2000.0f) region = 0.6f;
                map.push_back ({ b, m, w * region });
                if (f >= 38.0f && f <= 260.0f) bassMap.push_back ({ b, m, w * std::min (1.0f, 90.0f / f) });   // a nota mais grave pesa mais
            }
        }
    }

    void computeFrame()
    {
        // copia o anel em ordem temporal com janela
        float energy = 0;
        for (int i = 0; i < N; ++i)
        {
            const float s = ring[(size_t) ((writePos + i) % N)];
            energy += s * s;
            buf[(size_t) i] = { s * window[(size_t) i], 0.0f };
        }
        rms = std::sqrt (energy / (float) N);
        fft.perform (buf.data());
        for (int b = 0; b < N / 2; ++b) mag[(size_t) b] = std::abs (buf[(size_t) b]);

        // branqueamento: divide pela média móvel (≈ 1/3 de oitava) e mantém só picos acima dela
        const int W = std::max (4, N / 512);
        const int half = N / 2;
        prefix[0] = 0.0f;
        for (int b = 0; b < half; ++b) prefix[(size_t) b + 1] = prefix[(size_t) b] + mag[(size_t) b];
        for (int b = 0; b < half; ++b)
        {
            const int lo = std::max (0, b - W), hi = std::min (half, b + W + 1);
            const float mean = (prefix[(size_t) hi] - prefix[(size_t) lo]) / (float) (hi - lo) + 1e-9f;
            const float r = mag[(size_t) b] / mean;
            white[(size_t) b] = r > 1.4f ? (r - 1.4f) * mean : 0.0f;
        }

        float pitch[128] = {};
        float bassPitch[128] = {};
        for (const auto& e : map) if (e.pitch >= 0 && e.pitch < 128) pitch[e.pitch] += white[(size_t) e.bin] * e.w;
        for (const auto& e : bassMap) if (e.pitch >= 0 && e.pitch < 128) bassPitch[e.pitch] += white[(size_t) e.bin] * e.w;

        // supressão de harmônicos: notas a +19 (3º harm.) e +28 (5º harm.) semitons de uma nota forte
        for (int m = 127; m >= 0; --m)
        {
            if (m - 19 >= 0) pitch[m] = std::max (0.0f, pitch[m] - 0.6f * pitch[m - 19]);
            if (m - 28 >= 0) pitch[m] = std::max (0.0f, pitch[m] - 0.4f * pitch[m - 28]);
            if (m - 31 >= 0) pitch[m] = std::max (0.0f, pitch[m] - 0.3f * pitch[m - 31]);
        }

        Chroma c {}; Chroma bc {};
        for (int m = 0; m < 128; ++m) { c[(size_t) pc (m)] += std::sqrt (pitch[m]); bc[(size_t) pc (m)] += std::sqrt (bassPitch[m]); }
        float mx = 1e-9f, bmx = 1e-9f;
        for (int i = 0; i < 12; ++i) { mx = std::max (mx, c[(size_t) i]); bmx = std::max (bmx, bc[(size_t) i]); }
        for (int i = 0; i < 12; ++i) { c[(size_t) i] /= mx; bc[(size_t) i] /= bmx; }
        if (mx < 1e-6f) c.fill (0);
        if (bmx < 1e-6f) bc.fill (0);
        chroma = c; bassChroma = bc;
    }

    double sr = 44100;
    FFT fft;
    int N = 8192, hop = 2048, writePos = 0, sinceLast = 0;
    std::vector<float> window, ring, mag, white, prefix;
    std::vector<std::complex<float>> buf;
    std::vector<BinMap> map, bassMap;
    Chroma chroma {}, bassChroma {};
    float rms = 0;
};

//==============================================================================
// Rastreador: suaviza o croma, decide o acorde com histerese e estima a tonalidade a longo prazo.
struct TrackerSettings
{
    float sensitivity = 0.6f;     // 0..1: maior = muda de acorde mais rápido e com menos energia
    bool allowSevenths = true;
    bool lock = false;            // travar acorde atual
    bool autoKey = true;
    Key manualKey {};
    int scaleChoice = -1;         // -1 = mantém maior/menor detectado; senão força o modo
    bool conformToScale = false;  // ajusta acordes ao campo harmônico
};

class ChordTracker
{
public:
    void reset()
    {
        smooth.fill (0); bassSmooth.fill (0); longTerm.fill (0);
        current = {}; candidate = {}; candidateFrames = 0; framesSinceAny = 0;
        key = {}; keyConfidence = 0; frames = 0;
    }

    // Um novo quadro de croma (do áudio ou do MIDI). energy01: nível 0..1 (gate).
    // Retorna true quando o acorde mudou.
    bool update (const Chroma& c, const Chroma& bassC, float energy01, const TrackerSettings& s, float frameSeconds)
    {
        const float sens = std::clamp (s.sensitivity, 0.0f, 1.0f);
        const float alpha = 1.0f - std::exp (-frameSeconds / (0.05f + 0.35f * (1.0f - sens)));
        const float gate = 0.004f + 0.05f * (1.0f - sens) * (1.0f - sens);
        const bool active = energy01 > gate;

        for (int i = 0; i < 12; ++i)
        {
            const float tgt = active ? c[(size_t) i] : smooth[(size_t) i] * 0.98f;
            smooth[(size_t) i] += alpha * (tgt - smooth[(size_t) i]);
            const float btgt = active ? bassC[(size_t) i] : bassSmooth[(size_t) i] * 0.98f;
            bassSmooth[(size_t) i] += alpha * (btgt - bassSmooth[(size_t) i]);
            if (active) longTerm[(size_t) i] = longTerm[(size_t) i] * 0.9985f + c[(size_t) i] * energy01;
        }
        ++frames;

        // tonalidade
        if (s.autoKey)
        {
            float conf = 0;
            Key k = KeyFinder::estimate (longTerm, &conf);
            if (conf > 0.05f || frames < 50) key = applyScaleChoice (k, s.scaleChoice), keyConfidence = conf;
        }
        else key = s.manualKey, keyConfidence = 1.0f;

        if (s.lock || ! active)
        {
            if (! active) ++framesSinceAny;
            return false;
        }
        framesSinceAny = 0;

        float second = 0;
        Chord best = ChordTemplateMatch::best (smooth, bassSmooth, &key, s.allowSevenths, &second);
        if (s.conformToScale) best = conformToKey (best, key);
        if (! best.valid()) return false;

        // histerese: o candidato precisa vencer por alguns quadros seguidos
        const int needFrames = 1 + (int) std::lround (5.0f * (1.0f - sens));
        if (best == current) { candidateFrames = 0; current.confidence = best.confidence; return false; }
        if (best == candidate) ++candidateFrames; else { candidate = best; candidateFrames = 1; }
        const float margin = 0.02f + 0.06f * (1.0f - sens);
        const bool strong = best.confidence > 0.35f && (best.confidence - second) >= 0.0f;
        if (candidateFrames >= needFrames && strong && (best.confidence > current.confidence * (1.0f - margin) || candidateFrames > needFrames * 3 || ! current.valid()))
        {
            current = best;
            candidateFrames = 0;
            return true;
        }
        return false;
    }

    // Força um acorde (MIDI de entrada / progressão importada / acorde manual).
    bool setChord (const Chord& c)
    {
        if (c == current) return false;
        current = c;
        return true;
    }

    const Chord& getChord() const { return current; }
    const Key& getKey() const { return key; }
    float getKeyConfidence() const { return keyConfidence; }
    const Chroma& getSmoothedChroma() const { return smooth; }
    const Chroma& getLongTerm() const { return longTerm; }
    void feedLongTerm (const Chroma& c, float w) { for (int i = 0; i < 12; ++i) longTerm[(size_t) i] = longTerm[(size_t) i] * 0.9985f + c[(size_t) i] * w; }

private:
    Chroma smooth {}, bassSmooth {}, longTerm {};
    Chord current {}, candidate {};
    int candidateFrames = 0, framesSinceAny = 0;
    Key key {};
    float keyConfidence = 0;
    long frames = 0;
};

//==============================================================================
// Croma a partir de notas MIDI seguradas (a mais grave vira o baixo).
inline void chromaFromNotes (const bool held[128], const float vel[128], Chroma& c, Chroma& bass, int& lowest)
{
    c.fill (0); bass.fill (0); lowest = -1;
    for (int n = 0; n < 128; ++n)
        if (held[n])
        {
            c[(size_t) pc (n)] += 0.5f + 0.5f * vel[n];
            if (lowest < 0) lowest = n;
        }
    float mx = 0;
    for (float v : c) mx = std::max (mx, v);
    if (mx > 0) for (auto& v : c) v /= mx;
    if (lowest >= 0) bass[(size_t) pc (lowest)] = 1.0f;
}

// Acorde direto a partir de notas MIDI (exato; usa templates sobre o croma binário).
inline Chord chordFromNotes (const bool held[128], const Key* key, bool allowSevenths)
{
    float vel[128];
    for (int i = 0; i < 128; ++i) vel[i] = 1.0f;
    Chroma c, b; int lowest;
    chromaFromNotes (held, vel, c, b, lowest);
    if (lowest < 0) return {};
    int count = 0;
    for (float v : c) if (v > 0) ++count;
    if (count == 1)
    {
        // nota única: assume tríade diatônica (se houver tonalidade) com essa fundamental
        Chord ch; ch.root = pc (lowest); ch.quality = Quality::Major;
        if (key && ! key->contains (lowest + 4) && key->contains (lowest + 3)) ch.quality = Quality::Minor;
        if (key && ! key->contains (lowest + 7) && key->contains (lowest + 6) && key->contains (lowest + 3)) ch.quality = Quality::Dim;
        ch.confidence = 0.5f;
        return ch;
    }
    if (count == 2)
    {
        // díade: interpreta a quinta/terça
        Chroma bassOnly = b;
        return ChordTemplateMatch::best (c, bassOnly, key, false);
    }
    return ChordTemplateMatch::best (c, b, key, allowSevenths);
}
} // namespace sms
