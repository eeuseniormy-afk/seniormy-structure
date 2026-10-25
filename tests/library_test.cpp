// SeniorMy Structure — teste com JUCE (sem interface):
//   library_test [pasta-da-biblioteca]
// 1) carrega a biblioteca preparada (se informada) e toca notas de cada seção/articulação;
// 2) importa um MIDI e um WAV gerados aqui e confere os acordes;
// 3) exporta a cobertura em MIDI e relê; 4) ida e volta dos presets em JSON.
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SampleLibrary.h"
#include "LoopImporter.h"
#include "PresetManager.h"
#include "core/Engine.h"

using namespace juce;
static int fails = 0, checks = 0;
#define CHECK(c, msg) do { ++checks; if (! (c)) { ++fails; std::cout << "FALHOU: " << msg << std::endl; } } while (0)

static void testLibrary (const File& folder)
{
    String status;
    std::atomic<float> prog { 0 };
    auto t0 = Time::getMillisecondCounterHiRes();
    auto bank = SampleLibraryLoader::loadFolder (folder, status, &prog);
    std::cout << status << " (" << String ((Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) << " s)" << std::endl;
    CHECK (bank != nullptr, "biblioteca carregada");
    if (! bank) return;
    int present = 0;
    for (int s = 0; s < 6; ++s)
    {
        String line = "  " + String::fromUTF8 (sms::sectionName (s)) + ":";
        for (int a = 0; a < 5; ++a) if (bank->present[s][a]) { ++present; line << " " << sms::articName (a) << "(" << (int) bank->inst[s][a].zones.size() << ")"; }
        std::cout << line << std::endl;
    }
    CHECK (present >= 18, "seções × articulações presentes: " + String (present));
    for (int s : { 0, 1, 2, 3, 4 }) CHECK (bank->present[s][0], "sustain da seção " + String (s));

    sms::Sampler smp; smp.prepare (48000);
    smp.setBank (bank); smp.syncBank();
    std::vector<std::vector<float>> L (6, std::vector<float> (48000 * 3)), R = L;
    float* pl[6]; float* pr[6];
    for (int i = 0; i < 6; ++i) { pl[i] = L[(size_t) i].data(); pr[i] = R[(size_t) i].data(); }
    for (int s = 0; s < 5; ++s)
        for (int a : { 0, 2, 3, 4 })
        {
            const auto r = sms::sectionRange (s);
            smp.noteOn (s, a, (r.sweetLo + r.sweetHi) / 2, 0.8f, 0.7f, 0, 0);
        }
    smp.render (pl, pr, 0, 48000 * 3);
    for (int s = 0; s < 5; ++s)
    {
        float pk = 0; bool fin = true;
        for (float v : L[(size_t) s]) { pk = jmax (pk, std::abs (v)); if (! std::isfinite (v)) fin = false; }
        CHECK (fin && pk > 0.01f && pk < 4.0f, "seção " + String (s) + " soa (pico " + String (pk, 3) + ")");
    }

    // motor completo com a biblioteca real: cada preset de fábrica sobre C–Am–F–G via MIDI (8 s)
    for (auto& preset : sms::factoryPresets())
    {
        sms::Engine e;
        const double sr = 48000; const int block = 256;
        e.prepare (sr, block);
        e.setBank (bank);
        e.setArrangement (preset.arrangement);
        e.params.mode = sms::SourceMode::Midi;
        e.params.pianoDb = -60.0f;
        for (auto& [id, v] : preset.params)
        {
            if (id == "density") e.params.arr.density = v;
            else if (id == "extensions") e.params.arr.extensions = (int) v;
            else if (id == "spread") e.params.arr.spread = v;
            else if (id == "dynamics") e.params.arr.dynamics = v;
            else if (id == "reverbMix") e.params.reverbMix = v;
            else if (id == "reverbSize") e.params.reverbSize = v;
            else if (id == "register") e.params.arr.registerShift = (int) v;
        }
        const int chords[4][3] = { { 48, 52, 55 }, { 45, 48, 52 }, { 41, 45, 48 }, { 43, 47, 50 } };
        std::vector<float> L ((size_t) block), R ((size_t) block);
        AudioBuffer<float> rec (2, (int) (sr * 9));
        float pk = 0; double sum = 0; long cnt = 0; bool fin = true; int maxV = 0;
        const int total = (int) (sr * 9) / block;
        for (int b = 0; b < total; ++b)
        {
            const double t = (double) b * block / sr;
            const int ci = (int) (t / 2.0);
            if (b * block % (int) (sr * 2) < block && ci < 4)
            {
                if (ci > 0) for (int n : chords[ci - 1]) e.noteOffInput (n);
                for (int n : chords[ci]) e.noteOnInput (n, 0.8f);
            }
            if (ci == 4 && b * block % (int) (sr * 2) < block) for (int n : chords[3]) e.noteOffInput (n);
            sms::HostClock hc; hc.valid = true; hc.playing = true; hc.bpm = 120; hc.ppq = t * 2.0;
            e.process (nullptr, 0, L.data(), R.data(), block, hc);
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (L[(size_t) i])) fin = false;
                pk = jmax (pk, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
                sum += L[(size_t) i] * L[(size_t) i]; ++cnt;
                rec.setSample (0, b * block + i, L[(size_t) i]); rec.setSample (1, b * block + i, R[(size_t) i]);
            }
            maxV = jmax (maxV, e.voiceCount.load());
        }
        const double rmsDb = 20.0 * std::log10 (std::sqrt (sum / (double) jmax (1L, cnt)) + 1e-12);
        std::cout << "  " << String (preset.name).paddedRight (' ', 18) << " pico " << String (pk, 3) << "  RMS " << String (rmsDb, 1)
                  << " dBFS  vozes máx " << maxV << std::endl;
        CHECK (fin && pk > 0.02f && pk < 1.0f, "preset " + String (preset.name) + " com a biblioteca real");
        CHECK (rmsDb > -45.0 && rmsDb < -6.0, "nível RMS razoável em " + String (preset.name));
        auto dir = SystemStats::getEnvironmentVariable ("SMS_RENDER_DIR", {});
        if (dir.isNotEmpty())
        {
            File f = File (dir).getChildFile (File::createLegalFileName (preset.name) + ".wav");
            f.deleteFile();
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
            if (w) w->writeFromAudioSampleBuffer (rec, 0, rec.getNumSamples());
        }
    }
}

static File writeTestMidi()
{
    MidiMessageSequence seq;
    const int prog[4][4] = { { 48, 60, 64, 67 }, { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 43, 55, 59, 62 } };
    for (int bar = 0; bar < 4; ++bar)
        for (int n : prog[bar])
        {
            seq.addEvent (MidiMessage::noteOn (1, n, (uint8) 90), bar * 4 * 480.0);
            seq.addEvent (MidiMessage::noteOff (1, n), (bar * 4 + 4) * 480.0 - 10);
        }
    seq.updateMatchedPairs();
    MidiFile mf; mf.setTicksPerQuarterNote (480);
    MidiMessageSequence meta; meta.addEvent (MidiMessage::tempoMetaEvent (600000), 0);   // 100 BPM
    mf.addTrack (meta); mf.addTrack (seq);
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("sms_test_progression.mid");
    f.deleteFile();
    FileOutputStream out (f);
    mf.writeTo (out);
    return f;
}

static File writeTestWav()
{
    const double sr = 44100, beat = 0.6;   // 100 BPM
    const int n = (int) (sr * beat * 16);
    AudioBuffer<float> b (1, n);
    b.clear();
    const int prog[4][4] = { { 45, 57, 60, 64 }, { 41, 57, 60, 65 }, { 48, 60, 64, 67 }, { 43, 55, 59, 62 } };
    for (int bar = 0; bar < 4; ++bar)
        for (int bt = 0; bt < 4; ++bt)
            for (int note : prog[bar])
            {
                const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                const int s0 = (int) ((bar * 4 + bt) * beat * sr);
                for (int i = 0; i < (int) (beat * sr * 0.95) && s0 + i < n; ++i)
                {
                    const double t = i / sr;
                    double v = 0;
                    for (int k = 1; k <= 6; ++k) v += std::sin (2 * MathConstants<double>::pi * f * k * t) / (k * k) * std::exp (-t * (1.0 + 0.5 * k));
                    b.addSample (0, s0 + i, (float) (v * 0.12));
                }
            }
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("sms_test_loop_100bpm.wav");
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 1, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI* noGui = nullptr; (void) noGui;
    std::cout << "SeniorMy Structure — library_test" << std::endl;
    if (argc > 1) testLibrary (File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    else
    {
        String st;
        auto f = SampleLibraryLoader::findLibraryFolder();
        std::cout << "(sem pasta informada; biblioteca instalada: " << (f.exists() ? f.getFullPathName() : String ("nenhuma")) << ")" << std::endl;
    }

    // MIDI
    String err;
    auto midi = writeTestMidi();
    auto p = LoopImporter::analyse (midi, 0, err);
    CHECK (p != nullptr, "MIDI importado: " + err);
    if (p)
    {
        String s; for (auto& c : p->chords) s << c.chord.name().c_str() << " ";
        std::cout << "MIDI: " << s << "| " << String (p->bpm, 1) << " BPM | " << p->key.name().c_str() << std::endl;
        CHECK (p->chords.size() == 4 && p->chords[0].chord.root == 0 && p->chords[3].chord.root == 7, "acordes do MIDI");
        CHECK (std::abs (p->bpm - 100.0) < 0.5, "BPM do MIDI");
        // exportação
        auto arr = sms::factoryPresets()[2].arrangement;
        auto out = File::getSpecialLocation (File::tempDirectory).getChildFile ("sms_test_cover.mid");
        auto f = LoopImporter::exportCoverMidi (*p, arr, {}, 2, out, err);
        CHECK (f.existsAsFile(), "MIDI da cobertura gravado: " + err);
        FileInputStream in (f); MidiFile mf;
        CHECK (mf.readFrom (in), "MIDI exportado legível");
        int notes = 0;
        for (int t = 0; t < mf.getNumTracks(); ++t) for (auto* e : *mf.getTrack (t)) if (e->message.isNoteOn()) ++notes;
        std::cout << "cobertura exportada: " << mf.getNumTracks() << " trilhas, " << notes << " notas" << std::endl;
        CHECK (mf.getNumTracks() >= 3 && notes > 50, "trilhas e notas na exportação");
        // ida e volta da progressão
        auto v = LoopImporter::progressionToVar (*p);
        auto p2 = LoopImporter::progressionFromVar (JSON::parse (JSON::toString (v)));
        CHECK (p2 && p2->chords.size() == p->chords.size(), "progressão salva/restaurada");
    }

    // WAV
    auto wav = writeTestWav();
    auto pw = LoopImporter::analyse (wav, 0, err);
    CHECK (pw != nullptr, "WAV importado: " + err);
    if (pw)
    {
        String s; for (auto& c : pw->chords) s << c.chord.name().c_str() << " ";
        std::cout << "WAV: " << s << "| " << String (pw->bpm, 1) << " BPM | " << pw->key.name().c_str() << std::endl;
        CHECK (std::abs (pw->bpm - 100.0) < 1.0, "BPM pelo nome do arquivo");
        CHECK (! pw->chords.empty() && pw->chords[0].chord.root == 9, "primeiro acorde Am");
    }
    CHECK (LoopImporter::bpmFromFileName ("Keys Loop 92 BPM Am") == 92.0, "BPM no nome (92 BPM)");
    CHECK (LoopImporter::bpmFromFileName ("piano_120bpm") == 120.0, "BPM no nome (120bpm)");

    // presets
    for (auto& fp : sms::factoryPresets())
    {
        sms::PresetData back;
        const auto txt = JSON::toString (PresetIO::presetToVar (fp));
        CHECK (PresetIO::presetFromVar (JSON::parse (txt), back), "preset JSON " + String (fp.name));
        bool same = back.arrangement.lanes.size() == fp.arrangement.lanes.size() && back.params.size() == fp.params.size();
        for (size_t i = 0; same && i < fp.arrangement.lanes.size(); ++i)
            same = back.arrangement.lanes[i].grid == fp.arrangement.lanes[i].grid && back.arrangement.lanes[i].type == fp.arrangement.lanes[i].type
                && back.arrangement.lanes[i].sectionMask == fp.arrangement.lanes[i].sectionMask;
        CHECK (same, "preset idêntico após JSON: " + String (fp.name));
    }

    std::cout << checks << " verificações, " << fails << " falhas" << std::endl << (fails ? "FALHOU" : "OK") << std::endl;
    return fails ? 1 : 0;
}
