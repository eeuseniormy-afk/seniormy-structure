#include "SampleLibrary.h"
#include "core/SynthBank.h"
#include "core/Arranger.h"
#include <map>
#include <thread>

using namespace juce;

namespace
{
    int sectionIndex (const String& s)
    {
        static const char* ids[] = { "vln1", "vln2", "vla", "vc", "cb", "horns" };
        for (int i = 0; i < 6; ++i) if (s.equalsIgnoreCase (ids[i])) return i;
        return -1;
    }
    int articIndex (const String& s)
    {
        static const char* ids[] = { "sustain", "legato", "spiccato", "pizzicato", "tremolo" };
        for (int i = 0; i < 5; ++i) if (s.equalsIgnoreCase (ids[i])) return i;
        return -1;
    }
}

SampleLibraryLoader::SampleLibraryLoader() : Thread ("SeniorMy Structure library loader") {}
SampleLibraryLoader::~SampleLibraryLoader() { stopThread (8000); }

Array<File> SampleLibraryLoader::candidateFolders()
{
    Array<File> c;
    const auto env = SystemStats::getEnvironmentVariable ("SENIORMY_STRUCTURE_LIBRARY", {});
    if (env.isNotEmpty()) c.add (File (env));
   #if JUCE_MAC
    c.add (File ("/Library/Application Support/SeniorMy/Structure Library"));
    c.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Application Support/SeniorMy/Structure Library"));
   #elif JUCE_WINDOWS
    c.add (File::getSpecialLocation (File::commonDocumentsDirectory).getChildFile ("SeniorMy/Structure Library"));
    c.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("SeniorMy/Structure Library"));
   #else
    c.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("SeniorMy/Structure Library"));
   #endif
    // ao lado do executável (desenvolvimento)
    c.add (File::getSpecialLocation (File::currentExecutableFile).getParentDirectory().getChildFile ("Structure Library"));
    return c;
}

File SampleLibraryLoader::findLibraryFolder()
{
    for (auto& f : candidateFolders())
        if (f.getChildFile ("library.json").existsAsFile()) return f;
    return {};
}

void SampleLibraryLoader::loadAsync (double hostRate, std::function<void (std::shared_ptr<sms::InstrumentBank>, String)> cb)
{
    stopThread (8000);
    rate = hostRate > 0 ? hostRate : 44100.0;
    callback = std::move (cb);
    progress.store (0.0f);
    startThread (Thread::Priority::low);
}

String SampleLibraryLoader::getStatus() const { const ScopedLock sl (statusLock); return status; }

void SampleLibraryLoader::run()
{
    String st;
    std::shared_ptr<sms::InstrumentBank> bank;
    const auto folder = findLibraryFolder();
    if (folder.exists())
    {
        { const ScopedLock sl (statusLock); status = String::fromUTF8 ("Carregando biblioteca orquestral…"); }
        bank = loadFolder (folder, st, &progress, [this] { return threadShouldExit(); });
    }
    if (threadShouldExit()) return;
    if (! bank)
    {
        { const ScopedLock sl (statusLock); status = String::fromUTF8 ("Biblioteca não encontrada — usando cordas sintetizadas de reserva"); }
        bank = sms::makeSynthBank ((float) rate);
        st = String::fromUTF8 ("Biblioteca não instalada: usando timbres de reserva sintetizados. Reinstale o SeniorMy Structure para obter a biblioteca orquestral.");
    }
    progress.store (1.0f);
    { const ScopedLock sl (statusLock); status = st; }
    auto cb = callback;
    MessageManager::callAsync ([cb, bank, st] { if (cb) cb (bank, st); });
}

std::shared_ptr<sms::InstrumentBank> SampleLibraryLoader::loadFolder (const File& folder, String& status,
                                                                      std::atomic<float>* progress, std::function<bool()> shouldExit)
{
    const auto json = JSON::parse (folder.getChildFile ("library.json"));
    auto* root = json.getDynamicObject();
    if (root == nullptr) { status = "library.json inválido"; return nullptr; }
    const auto instruments = json["instruments"];
    if (! instruments.isArray() || instruments.size() == 0) { status = "library.json sem instrumentos"; return nullptr; }

    auto bank = std::make_shared<sms::InstrumentBank>();
    bank->description = json["name"].toString().toStdString();
    std::map<String, std::shared_ptr<sms::SampleData>> cache;

    int totalZones = 0, done = 0, missing = 0;
    for (auto& inst : *instruments.getArray()) totalZones += inst["zones"].size();

    // 1) decodifica todos os arquivos únicos em paralelo (FLAC → 16 bits na RAM)
    {
        StringArray unique;
        for (auto& iv : *instruments.getArray())
            if (auto* zones = iv["zones"].getArray())
                for (auto& zv : *zones) unique.addIfNotAlreadyThere (zv["file"].toString());
        std::vector<std::shared_ptr<sms::SampleData>> decoded ((size_t) unique.size());
        std::atomic<int> next { 0 }, finished { 0 };
        auto worker = [&]
        {
            AudioFormatManager local;
            local.registerBasicFormats();
            for (;;)
            {
                const int i = next.fetch_add (1);
                if (i >= unique.size() || (shouldExit && shouldExit())) break;
                std::unique_ptr<AudioFormatReader> reader (local.createReaderFor (folder.getChildFile (unique[i])));
                if (reader != nullptr && reader->lengthInSamples > 0 && reader->lengthInSamples <= (int64) 48000 * 60)
                {
                    const int n = (int) reader->lengthInSamples;
                    AudioBuffer<float> buf ((int) jmax (1u, jmin (2u, reader->numChannels)), n);
                    reader->read (&buf, 0, n, 0, true, buf.getNumChannels() > 1);
                    auto d = std::make_shared<sms::SampleData>();
                    d->sampleRate = reader->sampleRate;
                    d->assign (buf.getReadPointer (0), buf.getNumChannels() > 1 ? buf.getReadPointer (1) : nullptr, n);
                    decoded[(size_t) i] = d;
                }
                const int f = ++finished;
                if (progress) progress->store (0.95f * (float) f / (float) jmax (1, unique.size()));
            }
        };
        const int numThreads = jlimit (2, 8, SystemStats::getNumCpus());
        std::vector<std::thread> pool;
        for (int t = 0; t < numThreads; ++t) pool.emplace_back (worker);
        for (auto& t : pool) t.join();
        if (shouldExit && shouldExit()) return nullptr;
        for (int i = 0; i < unique.size(); ++i) if (decoded[(size_t) i]) cache[unique[i]] = decoded[(size_t) i];
    }

    for (auto& iv : *instruments.getArray())
    {
        const int s = sectionIndex (iv["section"].toString());
        const int a = articIndex (iv["artic"].toString());
        if (s < 0 || a < 0) continue;
        auto& inst = bank->inst[s][a];
        inst.numLayers = jmax (1, (int) iv.getProperty ("numLayers", 1));
        inst.numRR = jmax (1, (int) iv.getProperty ("numRR", 1));
        inst.oneShot = (bool) iv.getProperty ("oneShot", false);
        inst.attack = (float) (double) iv.getProperty ("attack", 0.01);
        inst.release = (float) (double) iv.getProperty ("release", 0.4);
        inst.gainDb = (float) (double) iv.getProperty ("gainDb", 0.0);
        inst.pan = (float) (double) iv.getProperty ("pan", 0.0);
        inst.detuneCents = (float) (double) iv.getProperty ("detuneCents", 0.0);
        inst.rrOffset = (int) iv.getProperty ("rrOffset", 0);
        inst.extraStart = (int) iv.getProperty ("extraStartFrames", 0);

        if (auto* zones = iv["zones"].getArray())
            for (auto& zv : *zones)
            {
                if (shouldExit && shouldExit()) return nullptr;
                const auto rel = zv["file"].toString();
                std::shared_ptr<sms::SampleData> data;
                if (auto it = cache.find (rel); it != cache.end()) data = it->second;
                else { ++missing; ++done; continue; }
                sms::Zone z;
                z.data = data;
                z.root = (int) zv.getProperty ("root", 60);
                z.loKey = (int) zv.getProperty ("lo", z.root);
                z.hiKey = (int) zv.getProperty ("hi", z.root);
                z.layer = (int) zv.getProperty ("layer", 0);
                z.rr = (int) zv.getProperty ("rr", 0);
                z.gain = (float) (double) zv.getProperty ("gain", 1.0);
                z.tuneCents = (float) (double) zv.getProperty ("tune", 0.0);
                z.loopStart = (int) zv.getProperty ("loopStart", -1);
                z.loopEnd = (int) zv.getProperty ("loopEnd", -1);
                z.startOffset = (int) zv.getProperty ("start", 0);
                inst.zones.push_back (z);
                ++done;
            }
        bank->present[s][a] = ! inst.zones.empty();
    }

    int present = 0;
    for (int s = 0; s < 6; ++s) for (int a = 0; a < 5; ++a) present += bank->present[s][a] ? 1 : 0;
    if (present == 0) { status = "Nenhuma amostra carregada"; return nullptr; }

    size_t bytes = 0;
    for (auto& [k, d] : cache) bytes += d->left.size() * 4;
    status = String::fromUTF8 ("Biblioteca orquestral: ") + String (bank->description) + " — "
           + String ((int) cache.size()) + String::fromUTF8 (" amostras, ") + String (roundToInt ((double) bytes / 1048576.0)) + " MB";
    if (missing > 0) status << " (" << missing << " ausentes)";
    return bank;
}
