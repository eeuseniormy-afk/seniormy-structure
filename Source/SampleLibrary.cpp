#include "SampleLibrary.h"

using namespace juce;

SampleLibraryLoader::SampleLibraryLoader() : Thread ("SeniorMy Acoustic Guitars library loader") {}
SampleLibraryLoader::~SampleLibraryLoader() { stopThread (8000); }

Array<File> SampleLibraryLoader::candidateFolders()
{
    Array<File> c;
    const auto env = SystemStats::getEnvironmentVariable ("SENIORMY_GUITARS_LIBRARY", {});
    if (env.isNotEmpty()) c.add (File (env));
   #if JUCE_MAC
    c.add (File ("/Library/Application Support/SeniorMy/Acoustic Guitars Library"));
    c.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Application Support/SeniorMy/Acoustic Guitars Library"));
   #elif JUCE_WINDOWS
    c.add (File::getSpecialLocation (File::commonDocumentsDirectory).getChildFile ("SeniorMy/Acoustic Guitars Library"));
    c.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("SeniorMy/Acoustic Guitars Library"));
   #else
    c.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("SeniorMy/Acoustic Guitars Library"));
   #endif
    c.add (File::getSpecialLocation (File::currentExecutableFile).getParentDirectory().getChildFile ("Acoustic Guitars Library"));
    return c;
}

File SampleLibraryLoader::findLibraryFolder()
{
    for (auto& f : candidateFolders())
        if (f.getChildFile ("library.json").existsAsFile()) return f;
    return {};
}

void SampleLibraryLoader::loadAsync (std::function<void (std::shared_ptr<sms::Bank>, String)> cb)
{
    stopThread (8000);
    callback = std::move (cb);
    progress.store (0.0f);
    startThread (Thread::Priority::low);
}

void SampleLibraryLoader::run()
{
    String status;
    auto bank = loadFolder (findLibraryFolder(), status, &progress, [this] { return threadShouldExit(); });
    if (threadShouldExit()) return;
    auto cb = callback;
    MessageManager::callAsync ([cb, bank, status] { if (cb) cb (bank, status); });
}

static std::shared_ptr<const sms::SampleData> readFlac (AudioFormatManager& fm, const File& f)
{
    std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
    if (! r || r->lengthInSamples <= 8) return nullptr;
    const int n = (int) jmin<int64> (r->lengthInSamples, (int64) (r->sampleRate * 40.0));
    AudioBuffer<float> buf ((int) jmax (1u, r->numChannels), n);
    r->read (&buf, 0, n, 0, true, true);
    if (buf.getNumChannels() > 1)
    {
        buf.addFrom (0, 0, buf, 1, 0, n);
        buf.applyGain (0, 0, n, 0.5f);
    }
    auto d = std::make_shared<sms::SampleData>();
    d->sampleRate = r->sampleRate;
    d->assign (buf.getReadPointer (0), n);
    return d;
}

std::shared_ptr<sms::Bank> SampleLibraryLoader::loadFolder (const File& folder, String& status,
                                                            std::atomic<float>* progress, std::function<bool()> shouldExit)
{
    if (! folder.getChildFile ("library.json").existsAsFile())
    {
        status = String::fromUTF8 ("Biblioteca não encontrada — usando instrumentos modelados.");
        return nullptr;
    }
    const auto json = JSON::parse (folder.getChildFile ("library.json"));
    auto* insts = json.getProperty ("instruments", var()).getDynamicObject();
    if (insts == nullptr) { status = String::fromUTF8 ("library.json inválido."); return nullptr; }
    AudioFormatManager fm;
    fm.registerBasicFormats();
    auto bank = std::make_shared<sms::Bank>();
    int total = 0, done = 0, failed = 0;
    for (auto& p : insts->getProperties()) total += p.value.getProperty ("zones", var()).size();
    for (auto& p : insts->getProperties())
    {
        sms::SampleSet set;
        set.numRR = jmax (1, (int) p.value.getProperty ("numRR", 1));
        set.gainDb = (float) (double) p.value.getProperty ("gainDb", 0.0);
        const auto zones = p.value.getProperty ("zones", var());
        for (int i = 0; i < zones.size(); ++i)
        {
            if (shouldExit && shouldExit()) return nullptr;
            const auto& z = zones[i];
            auto data = readFlac (fm, folder.getChildFile (z.getProperty ("file", "").toString()));
            ++done;
            if (progress) progress->store ((float) done / (float) jmax (1, total));
            if (! data) { ++failed; continue; }
            sms::Zone zz;
            zz.data = data;
            zz.root = (int) z.getProperty ("root", 60);
            zz.lo = (int) z.getProperty ("lo", zz.root);
            zz.hi = (int) z.getProperty ("hi", zz.root);
            zz.rr = (int) z.getProperty ("rr", 0);
            zz.tuneCents = (float) (double) z.getProperty ("tune", 0.0);
            zz.gain = (float) (double) z.getProperty ("gain", 1.0);
            set.zones.push_back (std::move (zz));
        }
        bank->sets[p.name.toString().toStdString()] = std::move (set);
    }
    const auto tap = json.getProperty ("noises", var()).getProperty ("bodyTap", "").toString();
    if (tap.isNotEmpty()) bank->bodyTap = readFlac (fm, folder.getChildFile (tap));
    status = String (done - failed) + String::fromUTF8 (" amostras carregadas") + (failed ? " (" + String (failed) + " falharam)" : String());
    bank->status = status.toStdString();
    return bank;
}
