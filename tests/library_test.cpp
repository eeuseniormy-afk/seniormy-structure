// SeniorMy Acoustic Guitars — teste com JUCE (sem interface): carrega a biblioteca preparada,
// toca cada instrumento com as amostras, confere presets JSON (ida e volta), exporta MIDI e
// reimporta o MIDI exportado (detecção de acordes).
// © 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SampleLibrary.h"
#include "PresetManager.h"
#include "LoopImporter.h"

using namespace juce;
static int fails = 0;
static void check (bool c, const String& m) { std::printf ("%s: %s\n", c ? "ok" : "FALHA", m.toRawUTF8()); if (! c) ++fails; }

int main (int argc, char** argv)
{
    const File folder = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File();
    String status;
    auto t0 = Time::getMillisecondCounterHiRes();
    auto bank = SampleLibraryLoader::loadFolder (folder, status);
    std::printf ("biblioteca: %s (%.0f ms)\n", status.toRawUTF8(), Time::getMillisecondCounterHiRes() - t0);
    check (bank != nullptr, "biblioteca carregada");
    if (bank)
    {
        for (auto* s : { "steel", "nylon", "ukulele", "banjo", "sitar" })
        {
            const auto* set = bank->set (s);
            check (set != nullptr && set->zones.size() >= 10, String ("conjunto ") + s + " (" + String (set ? (int) set->zones.size() : 0) + " amostras)");
        }
        check (bank->bodyTap != nullptr, "golpe no tampo (ukulele) carregado");
    }
    // cada instrumento soa com a biblioteca
    for (int i = 0; i < sms::NumInstruments; ++i)
    {
        auto eng = std::make_unique<sms::Engine>();
        eng->prepare (48000, 512);
        eng->setBank (bank.get());
        sms::EngineState s;
        s.parts[0].instrument = i; s.parts[0].mode = sms::ModeStrumOnce;
        s.parts[1].mode = sms::ModeOff; s.parts[2].mode = sms::ModeOff;
        eng->setState (s);
        std::vector<float> L (512), R (512); sms::Transport tr;
        double sum = 0; float pk = 0; bool finite = true;
        for (int b = 0; b < 190; ++b)
        {
            if (b == 1) { eng->noteOn (48, 0.9f, 0); eng->noteOn (52, 0.9f, 0); eng->noteOn (55, 0.9f, 0); }
            eng->process (L.data(), R.data(), 512, tr);
            for (int k = 0; k < 512; ++k) { sum += L[(size_t) k] * L[(size_t) k]; pk = jmax (pk, std::abs (L[(size_t) k])); finite &= std::isfinite (L[(size_t) k]); }
        }
        const double rms = std::sqrt (sum / (190.0 * 512));
        check (finite && rms > 0.003 && pk <= 1.0f, String::fromUTF8 (sms::instrument (i).namePt) + " soa (rms " + String (rms, 4) + ", pico " + String (pk, 3)
               + (eng->instrumentHasSamples (i) ? ", amostras)" : ", modelo)"));
    }
    // presets: ida e volta em JSON
    const auto fp = sms::factoryPresets();
    bool same = true;
    for (auto& p : fp)
    {
        sms::PresetData back;
        const auto json = JSON::toString (PresetIO::presetToVar (p));
        same &= PresetIO::presetFromVar (JSON::parse (json), back) && back.name == p.name
                && back.state.harmony.numProg == p.state.harmony.numProg
                && back.state.parts[0].pattern.grid == p.state.parts[0].pattern.grid
                && back.state.parts[0].instrument == p.state.parts[0].instrument;
    }
    check (same, "presets de fábrica sobrevivem a JSON (" + String ((int) fp.size()) + ")");
    // exporta MIDI e reimporta
    auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("smag_test.mid");
    String err;
    auto f = LoopImporter::exportMidi (fp[0].state, 2, tmp, err);
    check (f.existsAsFile() && f.getSize() > 200, "exporta MIDI (" + String (f.getSize()) + " bytes) " + err);
    auto prog = LoopImporter::analyse (f, 96, err);
    check (prog != nullptr && ! prog->empty(), "reimporta o MIDI exportado " + err);
    if (prog)
    {
        String names;
        for (auto& c : prog->chords) names << String::fromUTF8 (c.chord.name().c_str()) << " ";
        std::printf ("  acordes detectados: %s| tom %s | %.1f BPM\n", names.toRawUTF8(), prog->key.name().c_str(), prog->bpm);
        bool hasG = false, hasD = false;
        for (auto& c : prog->chords) { hasG |= c.chord.root == 7; hasD |= c.chord.root == 2; }
        check (hasG && hasD, "acordes G e D reconhecidos no MIDI exportado");
        sms::EngineState s; LoopImporter::applyToHarmony (*prog, s);
        check (s.harmony.numProg >= 2 && s.harmony.progressionOn, "progressão aplicada ao estado");
    }
    tmp.deleteFile();
    std::printf (fails ? "\n%d FALHA(S)\n" : "\nTODOS OS TESTES PASSARAM\n", fails);
    return fails ? 1 : 0;
}
