// SeniorMy Acoustic Guitars — padrões de batida e dedilhado (grade editável) e geração dos
// toques de cada passo sobre a digitação atual: baixo, baixo alternado, batida para baixo/cima,
// batida nas agudas, abafado (chuck), golpe no tampo e cordas individuais (1 = mais aguda).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#pragma once
#include "Voicing.h"
#include "Dsp.h"
#include <array>
#include <cstring>
#include <vector>

namespace sms
{
enum Row : int
{
    RBass = 0, RAltBass, RDown, RUp, RDownTop, RChuck, RTap, RS1, RS2, RS3, RS4, RS5, RS6,
    NumRows
};

inline const char* rowNamePt (int r)
{
    static const char* n[] = { "Baixo", "Baixo alternado", "Batida ↓", "Batida ↑", "Agudas ↓", "Abafado (chuck)",
                               "Golpe no tampo", "Corda 1 (aguda)", "Corda 2", "Corda 3", "Corda 4", "Corda 5", "Corda 6" };
    return n[std::clamp (r, 0, (int) NumRows - 1)];
}

constexpr int kMaxSteps = 64;

struct Pattern
{
    char name[48] {};
    char category[24] {};
    int stepsPerBeat = 2;      // 1, 2, 3, 4, 6, 8
    int numSteps = 8;          // ≤ kMaxSteps
    float swing = 0.0f;        // 0..0.6 (atrasa os passos pares)
    float strumScale = 1.0f;   // multiplica o tempo de batida do instrumento
    float humanize = 0.35f;    // 0..1 (tempo ± e dinâmica)
    float mute = 0.0f;         // abafamento com a palma (0..1)
    float accent = 0.5f;       // intensidade das acentuações
    std::array<std::array<uint8_t, kMaxSteps>, NumRows> grid {};

    void setName (const char* n, const char* cat = "")
    {
        std::strncpy (name, n, sizeof (name) - 1); name[sizeof (name) - 1] = 0;
        std::strncpy (category, cat, sizeof (category) - 1); category[sizeof (category) - 1] = 0;
    }
    // "X" 120, "x" 96, "o" 72, "-" 48, "." nada; repete a figura até preencher numSteps
    void row (int r, const char* s)
    {
        const int len = (int) std::strlen (s);
        if (len == 0) return;
        for (int i = 0; i < numSteps; ++i)
        {
            const char ch = s[i % len];
            grid[(size_t) r][(size_t) i] = (uint8_t) (ch == 'X' ? 120 : ch == 'x' ? 96 : ch == 'o' ? 72 : ch == '-' ? 48 : 0);
        }
    }
    bool rowUsed (int r) const { for (int i = 0; i < numSteps; ++i) if (grid[(size_t) r][(size_t) i]) return true; return false; }
    double lengthBeats() const { return (double) numSteps / (double) std::max (1, stepsPerBeat); }
};

enum PluckKind : uint8_t { KNormal = 0, KChuck = 1, KTap = 2 };

struct PluckEvent
{
    float delayMs = 0;     // atraso dentro do passo (batida)
    int course = -1;       // -1 = golpe
    float vel = 0.8f;      // 0..1
    float mute = 0;        // 0..1
    uint8_t kind = KNormal;
    bool up = false;       // batida para cima
};

struct StepContext
{
    const InstrumentDef* inst = nullptr;
    const Tuning* tuning = nullptr;
    const Voicing* voicing = nullptr;
    int capo = 0;
    Chord chord;
};

// ordens soando, na ordem física
inline int soundingCourses (const StepContext& c, std::array<int, kMaxCourses>& out, bool includeDrone)
{
    int k = 0;
    for (int i = 0; i < c.voicing->numCourses; ++i)
        if (c.voicing->fret[(size_t) i] >= 0 && (includeDrone || ! c.tuning->courses[(size_t) i].drone)) out[(size_t) k++] = i;
    return k;
}

inline int courseSoundingNote (const StepContext& c, int i)
{
    const auto& co = c.tuning->courses[(size_t) i];
    const int f = std::max (0, c.voicing->fret[(size_t) i]);
    int n = co.note + c.capo + f;
    if (co.pair >= 0) n = std::min (n, co.pair + c.capo + f);
    return n;
}

inline int bassCourse (const StepContext& c)
{
    int best = -1, bestNote = 1000;
    std::array<int, kMaxCourses> s {}; const int k = soundingCourses (c, s, false);
    // preferir a ordem mais grave que toca o baixo do acorde
    for (int j = 0; j < k; ++j)
    {
        const int n = courseSoundingNote (c, s[(size_t) j]);
        const bool isBass = c.chord.valid() && pc (n) == c.chord.bassPc();
        const int score = n - (isBass ? 24 : 0);
        if (score < bestNote) { bestNote = score; best = s[(size_t) j]; }
    }
    return best;
}

inline int altBassCourse (const StepContext& c, int bass)
{
    std::array<int, kMaxCourses> s {}; const int k = soundingCourses (c, s, false);
    // as 3 ordens mais graves (exceto o baixo); prefere a quinta do acorde
    std::array<std::pair<int, int>, kMaxCourses> v {}; int m = 0;
    for (int j = 0; j < k; ++j) if (s[(size_t) j] != bass) v[(size_t) m++] = { courseSoundingNote (c, s[(size_t) j]), s[(size_t) j] };
    std::sort (v.begin(), v.begin() + m);
    const int fifth = c.chord.valid() ? c.chord.fifth() : -1;
    for (int j = 0; j < std::min (m, 3); ++j) if (pc (v[(size_t) j].first) == fifth) return v[(size_t) j].second;
    return m > 0 ? v[0].second : bass;
}

// Gera os toques de um passo. Retorna o número de eventos escritos.
inline int generateStep (const Pattern& p, int step, const StepContext& c, float strumMs,
                         std::array<PluckEvent, 48>& out)
{
    int n = 0;
    if (! c.voicing || ! c.voicing->valid) return 0;
    const int s = ((step % p.numSteps) + p.numSteps) % p.numSteps;
    std::array<int, kMaxCourses> snd {}; const int k = soundingCourses (c, snd, true);
    std::array<int, kMaxCourses> sndND {}; const int kND = soundingCourses (c, sndND, false);
    auto add = [&] (float d, int course, float v, uint8_t kind, bool up = false)
    {
        if (n >= (int) out.size()) return;
        PluckEvent e; e.delayMs = d; e.course = course; e.vel = std::clamp (v, 0.02f, 1.0f); e.mute = p.mute; e.kind = kind; e.up = up;
        out[(size_t) n++] = e;
    };
    const float sm = std::max (2.0f, strumMs * p.strumScale);
    for (int r = 0; r < NumRows; ++r)
    {
        const uint8_t g = p.grid[(size_t) r][(size_t) s];
        if (! g) continue;
        float v = (float) g / 127.0f;
        v = 0.5f + (v - 0.5f) * (0.6f + 0.8f * p.accent);
        switch (r)
        {
            case RBass: { const int b = bassCourse (c); if (b >= 0) add (0, b, v, KNormal); break; }
            case RAltBass: { const int b = altBassCourse (c, bassCourse (c)); if (b >= 0) add (0, b, v, KNormal); break; }
            case RDown:
                for (int j = 0; j < k; ++j)
                    add (k > 1 ? sm * (float) j / (float) (k - 1) : 0.0f, snd[(size_t) j], v * (1.0f - 0.14f * (float) j / (float) std::max (1, k - 1)), KNormal);
                break;
            case RUp:
            {
                const int cnt = std::max (std::min (3, kND), (int) std::ceil (kND * 0.67f));
                for (int j = 0; j < cnt; ++j)
                    add (cnt > 1 ? sm * 0.8f * (float) j / (float) (cnt - 1) : 0.0f, sndND[(size_t) (kND - 1 - j)], v * 0.86f * (1.0f - 0.1f * (float) j / (float) std::max (1, cnt - 1)), KNormal, true);
                break;
            }
            case RDownTop:
            {
                const int cnt = std::min (kND, kND >= 5 ? 4 : 3);
                for (int j = 0; j < cnt; ++j)
                    add (cnt > 1 ? sm * 0.55f * (float) j / (float) (cnt - 1) : 0.0f, sndND[(size_t) (kND - cnt + j)], v * 0.92f, KNormal);
                break;
            }
            case RChuck:
                for (int j = 0; j < k; ++j) add (k > 1 ? sm * 0.45f * (float) j / (float) (k - 1) : 0.0f, snd[(size_t) j], v * 0.8f, KChuck);
                break;
            case RTap: add (0, -1, v, KTap); break;
            default:
            {
                const int stringNo = r - RS1 + 1;                       // 1 = mais aguda (última ordem física)
                const int course = c.voicing->numCourses - stringNo;
                if (course >= 0 && c.voicing->fret[(size_t) course] >= 0) add (0, course, v, KNormal);
                break;
            }
        }
    }
    return n;
}

//==============================================================================
inline std::vector<Pattern> factoryPatterns()
{
    std::vector<Pattern> v;
    auto mk = [&] (const char* name, const char* cat, int spb, int steps) -> Pattern&
    {
        v.emplace_back(); auto& p = v.back(); p.setName (name, cat); p.stepsPerBeat = spb; p.numSteps = steps; return p;
    };
    { auto& p = mk ("Folk pop (↓ ↓↑ ↑↓↑)", "Violão", 2, 8);
      p.row (RBass, "x......."); p.row (RDown, "X.x...x."); p.row (RUp, "...o.o.o"); p.humanize = 0.35f; }
    { auto& p = mk ("Balada dedilhada (p-i-m-a)", "Dedilhado", 2, 8);
      p.row (RBass, "x......."); p.row (RAltBass, "....x..."); p.row (RS3, ".x...x.."); p.row (RS2, "..x...x."); p.row (RS1, "...x...x"); }
    { auto& p = mk ("Travis picking", "Dedilhado", 2, 8);
      p.row (RBass, "x...x..."); p.row (RAltBass, "..x...x."); p.row (RS2, ".x...x.."); p.row (RS1, "...x...x"); p.row (RS3, "...-...."); }
    { auto& p = mk ("Arpejo ascendente", "Dedilhado", 4, 16);
      p.row (RBass, "x......."); p.row (RS4, ".x......"); p.row (RS3, "..x....x"); p.row (RS2, "...x.x.."); p.row (RS1, "....x..."); }
    { auto& p = mk ("Bossa nova", "Brasil", 4, 32);
      p.row (RBass, "x.......x......."); p.row (RAltBass, "....x.......x...");
      const char* pinch = "x..x..x...x..x....x..x...x..x...";
      p.row (RS1, pinch); p.row (RS2, pinch); p.row (RS3, pinch); p.strumScale = 0.15f; p.humanize = 0.25f; }
    { auto& p = mk ("Samba / pagode (cavaco)", "Brasil", 4, 16);
      p.row (RDown, "X.x.x.X.x.X.x.x."); p.row (RUp, ".-.o.-.o.-.o.-.o"); p.swing = 0.12f; p.humanize = 0.3f; }
    { auto& p = mk ("Choro (cavaco centro)", "Brasil", 4, 16);
      p.row (RDownTop, "x..x..x.x..x..x."); p.row (RUp, "..o.....o......."); p.swing = 0.08f; }
    { auto& p = mk ("Baião", "Brasil", 4, 16);
      p.row (RBass, "x..x....x..x...."); p.row (RDownTop, "..x...x...x...x."); p.row (RChuck, "....x.......x..."); }
    { auto& p = mk ("Ponteado caipira", "Viola", 2, 16);
      p.row (RBass, "x...x...x...x..."); p.row (RS3, ".x...x...x...x.."); p.row (RS2, "..x...x...x...x."); p.row (RS1, "...x...x...x...x"); }
    { auto& p = mk ("Pagode de viola", "Viola", 4, 16);
      p.row (RBass, "x.......x......."); p.row (RDown, "X..x..x.X.x..x.."); p.row (RUp, ".o...o...o...o.."); p.strumScale = 0.8f; }
    { auto& p = mk ("Cururu (rasqueado)", "Viola", 4, 16);
      p.row (RDown, "X.xxX.x.X.xxX.x."); p.row (RUp, ".o...o.o.o...o.o"); }
    { auto& p = mk ("Toada / guarânia (3/4)", "Viola", 2, 6);
      p.row (RBass, "x....."); p.row (RDown, "..x.x."); p.row (RUp, "...o.o"); }
    { auto& p = mk ("Valsa (3/4)", "Violão", 2, 6);
      p.row (RBass, "x....."); p.row (RDownTop, "..x.x."); }
    { auto& p = mk ("Rumba flamenca", "Flamenco", 4, 16);
      p.row (RDown, "X..x..X.X..x..X."); p.row (RUp, "..o..o.....o..o."); p.row (RTap, "....x.......x..."); p.strumScale = 0.8f; }
    { auto& p = mk ("Rasgueado (leque)", "Flamenco", 4, 16);
      p.row (RDown, "XxxxX...XxxxX..."); p.row (RUp, ".....o.......o.."); p.row (RTap, "......x.......x."); p.strumScale = 0.55f; p.accent = 0.8f; }
    { auto& p = mk ("Reggae (contratempo)", "Violão", 2, 8);
      p.row (RDownTop, "..x...x."); p.mute = 0.55f; }
    { auto& p = mk ("Island strum (ukulele)", "Ukulele", 2, 8);
      p.row (RDown, "X.x...x."); p.row (RUp, "...o.o.o"); p.row (RChuck, "....x..."); }
    { auto& p = mk ("Roll para frente (banjo)", "Banjo", 2, 8);
      p.row (RS2, "x..x..x."); p.row (RS1, ".x..x..x"); p.row (RS5, "..x..x.."); p.humanize = 0.2f; }
    { auto& p = mk ("Roll de polegar alternado", "Banjo", 2, 8);
      p.row (RS3, "x......."); p.row (RS2, ".x...x.."); p.row (RS5, "..x...x."); p.row (RS1, "...x...x"); p.row (RS4, "....x..."); }
    { auto& p = mk ("Trêmolo (bandolim)", "Bandolim", 4, 16);
      p.row (RS1, "XoxoXoxoXoxoXoxo"); p.row (RS2, "-..."); p.strumScale = 0.3f; p.humanize = 0.15f; }
    { auto& p = mk ("Chop bluegrass", "Bandolim", 2, 8);
      p.row (RDownTop, "..x...x."); p.mute = 0.8f; p.row (RBass, "x...x..."); }
    { auto& p = mk ("Raga lento (sitar)", "Sitar", 2, 16);
      p.row (RS4, "x..............."); p.row (RS3, "....x.......x..."); p.row (RS2, "......x.....x..."); p.row (RS1, "..x.....x.x....x"); }
    { auto& p = mk ("Batida única por compasso", "Básico", 1, 4);
      p.row (RDown, "X..."); }
    { auto& p = mk ("Semínimas", "Básico", 1, 4);
      p.row (RDown, "Xxxx"); }
    { auto& p = mk ("Colcheias ↓↑", "Básico", 2, 8);
      p.row (RDown, "X.x.x.x."); p.row (RUp, ".o.o.o.o"); }
    return v;
}
} // namespace sms
