// SeniorMy Acoustic Guitars — render offline de demonstrações com o motor do próprio plugin.
// Mesmo caminho do GuitarsProcessor: SampleLibraryLoader::loadFolder → sms::Engine::setBank,
// preset de fábrica → Engine::setState, parâmetros (volume, reverb, níveis, BPM, Tocar) → Engine::setLive,
// Engine::process (mixer + reverb + master com softclip). Relógio interno ligado ("Tocar") e
// progressão do preset ligada (o mesmo que clicar em "Progressão" na interface).
// Saída: WAV 48 kHz estéreo 24 bits, sem normalização.
// Uso: render_demo <pasta-da-biblioteca> <pasta-de-saída> [segundos]
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SampleLibrary.h"
#include "core/Presets.h"
#include <cstdio>
#include <cmath>

using namespace juce;

static bool writeWav24 (const File& f, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    f.deleteFile();
    FileOutputStream out (f);
    if (! out.openedOk()) return false;
    const uint32 frames = (uint32) L.size(), dataBytes = frames * 2u * 3u;
    auto u32 = [&] (uint32 v) { out.writeInt ((int) v); };      // little-endian
    auto u16 = [&] (uint16 v) { out.writeShort ((short) v); };
    out.write ("RIFF", 4); u32 (36 + dataBytes); out.write ("WAVE", 4);
    out.write ("fmt ", 4); u32 (16); u16 (1); u16 (2); u32 ((uint32) sr); u32 ((uint32) sr * 6u); u16 (6); u16 (24);
    out.write ("data", 4); u32 (dataBytes);
    MemoryBlock mb (dataBytes);
    auto* d = static_cast<uint8*> (mb.getData());
    for (uint32 i = 0; i < frames; ++i)
        for (int c = 0; c < 2; ++c)
        {
            const float s = jlimit (-1.0f, 1.0f, (c == 0 ? L : R)[i]);
            const int v = jlimit (-8388608, 8388607, (int) std::lround (s * 8388607.0));
            *d++ = (uint8) (v & 0xff); *d++ = (uint8) ((v >> 8) & 0xff); *d++ = (uint8) ((v >> 16) & 0xff);
        }
    out.write (mb.getData(), mb.getSize());
    return true;
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uso: render_demo <biblioteca> <saida> [segundos]\n"); return 2; }
    const File lib = File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argv[2]);
    const double seconds = argc > 3 ? std::atof (argv[3]) : 20.0;
    const double tail = 2.5;          // últimos segundos: "Tocar" desligado, cauda natural
    outDir.createDirectory();

    String status;
    auto bank = SampleLibraryLoader::loadFolder (lib, status);
    std::printf ("biblioteca: %s\n", status.toRawUTF8());
    if (! bank) { std::printf ("FALHA: biblioteca não carregada\n"); return 1; }

    // Só instrumentos com amostras gravadas (CC0).
    struct Job { const char* preset; const char* file; };
    const Job jobs[] = {
        { "Violão Folk – Batida",   "01-violao-aco-folk" },
        { "Bossa Nova – Nylon",     "02-violao-nylon-bossa-nova" },
        { "Ukulele – Island",       "03-ukulele-island" },
        { "Banjo – Bluegrass Roll", "04-banjo-bluegrass-roll" },
        { "Sitar – Raga",           "05-sitar-raga" },
    };
    const auto fp = sms::factoryPresets();
    const int sr = 48000, bs = 512;
    int fails = 0;
    for (const auto& j : jobs)
    {
        const sms::PresetData* pd = nullptr;
        for (auto& p : fp) if (p.name == j.preset) pd = &p;
        if (! pd) { std::printf ("FALHA: preset não encontrado: %s\n", j.preset); ++fails; continue; }

        auto eng = std::make_unique<sms::Engine>();
        eng->prepare (sr, bs);                       // GuitarsProcessor::prepareToPlay
        eng->setBank (bank.get());                   // callback do loader
        auto s = pd->state;
        s.master.internalPlay = false;               // applyPreset mantém o estado do botão (desligado)
        s.harmony.progressionOn = true;              // botão "Progressão" da interface
        eng->setState (s);                           // processBlock: stateDirty → engine.setState

        // fontes de cada parte ativa: amostras ou modelo?
        String src;
        for (int p = 0; p < sms::kParts; ++p)
        {
            const auto& ps = s.parts[(size_t) p];
            if (ps.mode == sms::ModeOff) continue;
            const bool smp = eng->instrumentHasSamples (ps.instrument);
            src << String::fromUTF8 (sms::instrument (ps.instrument).namePt) << "=" << (smp ? "AMOSTRAS" : "MODELO") << " ";
            if (! smp) { std::printf ("FALHA: %s toca pelo modelo (sem amostras)\n", j.preset); ++fails; }
        }

        // parâmetros do APVTS como pushParamsFromState os deixa
        std::array<float, sms::kParts> lev {};
        for (int p = 0; p < sms::kParts; ++p) lev[(size_t) p] = s.parts[(size_t) p].levelDb;

        const int total = (int) (seconds * sr);
        const int playUntil = (int) ((seconds - tail) * sr);
        std::vector<float> L ((size_t) total), R ((size_t) total), bl (bs), br (bs);
        sms::Transport tr;                           // sem DAW (como no Standalone)
        float peak = 0; double sum = 0;
        for (int pos = 0; pos < total; pos += bs)
        {
            const int n = std::min (bs, total - pos);
            eng->setLive (s.master.volumeDb, s.master.reverbMix, lev, s.master.bpm, pos < playUntil);
            eng->process (bl.data(), br.data(), n, tr);
            for (int i = 0; i < n; ++i)
            {
                L[(size_t) (pos + i)] = bl[(size_t) i]; R[(size_t) (pos + i)] = br[(size_t) i];
                peak = std::max (peak, std::max (std::abs (bl[(size_t) i]), std::abs (br[(size_t) i])));
                sum += 0.5 * (bl[(size_t) i] * bl[(size_t) i] + br[(size_t) i] * br[(size_t) i]);
            }
        }
        const double rms = std::sqrt (sum / total);
        const File f = outDir.getChildFile (String (j.file) + ".wav");
        const bool ok = writeWav24 (f, L, R, sr);
        std::printf ("%s | %s | %.0f BPM | %d acordes | pico %.4f (%.2f dBFS) | rms %.4f (%.2f dBFS) | %s| %s\n",
                     j.file, j.preset, s.master.bpm, s.harmony.numProg, peak, 20.0 * std::log10 (std::max (1e-9f, peak)),
                     rms, 20.0 * std::log10 (std::max (1e-12, rms)), src.toRawUTF8(), ok ? "gravado" : "FALHA ao gravar");
        if (! ok || rms < 0.003 || peak >= 1.0f) ++fails;
    }
    std::printf (fails ? "%d FALHA(S)\n" : "OK\n", fails);
    return fails ? 1 : 0;
}
