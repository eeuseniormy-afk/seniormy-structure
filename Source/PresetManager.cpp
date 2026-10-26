#include "PresetManager.h"

using namespace juce;

namespace PresetIO
{
static String instId (int i) { return sms::instrument (i).id; }
static int instFromId (const String& s)
{
    for (int i = 0; i < sms::NumInstruments; ++i) if (s == sms::instrument (i).id) return i;
    return sms::Steel;
}

var patternToVar (const sms::Pattern& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("name", String::fromUTF8 (p.name));
    o->setProperty ("category", String::fromUTF8 (p.category));
    o->setProperty ("stepsPerBeat", p.stepsPerBeat);
    o->setProperty ("numSteps", p.numSteps);
    o->setProperty ("swing", p.swing);
    o->setProperty ("strumScale", p.strumScale);
    o->setProperty ("humanize", p.humanize);
    o->setProperty ("mute", p.mute);
    o->setProperty ("accent", p.accent);
    Array<var> rows;
    for (int r = 0; r < sms::NumRows; ++r)
    {
        StringArray cells;
        for (int s = 0; s < p.numSteps; ++s) cells.add (String ((int) p.grid[(size_t) r][(size_t) s]));
        rows.add (cells.joinIntoString (","));
    }
    o->setProperty ("rows", rows);
    return var (o);
}

bool patternFromVar (const var& v, sms::Pattern& p)
{
    if (! v.isObject()) return false;
    p = {};
    p.setName (v.getProperty ("name", "Padrão").toString().toRawUTF8(), v.getProperty ("category", "").toString().toRawUTF8());
    p.stepsPerBeat = jlimit (1, 8, (int) v.getProperty ("stepsPerBeat", 2));
    p.numSteps = jlimit (1, sms::kMaxSteps, (int) v.getProperty ("numSteps", 8));
    p.swing = (float) (double) v.getProperty ("swing", 0.0);
    p.strumScale = (float) (double) v.getProperty ("strumScale", 1.0);
    p.humanize = (float) (double) v.getProperty ("humanize", 0.35);
    p.mute = (float) (double) v.getProperty ("mute", 0.0);
    p.accent = (float) (double) v.getProperty ("accent", 0.5);
    const auto rows = v.getProperty ("rows", var());
    for (int r = 0; r < jmin (rows.size(), (int) sms::NumRows); ++r)
    {
        StringArray cells; cells.addTokens (rows[r].toString(), ",", "");
        for (int s = 0; s < jmin (cells.size(), p.numSteps); ++s) p.grid[(size_t) r][(size_t) s] = (uint8_t) jlimit (0, 127, cells[s].getIntValue());
    }
    return true;
}

var stateToVar (const sms::EngineState& st)
{
    auto* o = new DynamicObject();
    Array<var> parts;
    for (auto& ps : st.parts)
    {
        auto* p = new DynamicObject();
        p->setProperty ("instrument", instId (ps.instrument));
        p->setProperty ("tuning", ps.tuning); p->setProperty ("capo", ps.capo); p->setProperty ("position", ps.position);
        p->setProperty ("mode", ps.mode); p->setProperty ("octave", ps.octave);
        p->setProperty ("level", ps.levelDb); p->setProperty ("pan", ps.pan); p->setProperty ("tone", ps.tone);
        p->setProperty ("body", ps.body); p->setProperty ("width", ps.width); p->setProperty ("reverb", ps.reverb);
        p->setProperty ("noises", ps.noises); p->setProperty ("mute", ps.mute); p->setProperty ("solo", ps.solo);
        p->setProperty ("pattern", patternToVar (ps.pattern));
        parts.add (var (p));
    }
    o->setProperty ("parts", parts);
    auto* h = new DynamicObject();
    const auto& hs = st.harmony;
    h->setProperty ("keyTonic", hs.keyTonic); h->setProperty ("keyScale", hs.keyScale); h->setProperty ("tetrads", hs.tetrads);
    h->setProperty ("fieldMode", hs.fieldMode); h->setProperty ("split", hs.split); h->setProperty ("progressionOn", hs.progressionOn);
    h->setProperty ("latch", hs.latch);
    Array<var> prog;
    for (int i = 0; i < hs.numProg; ++i)
    {
        const auto& s = hs.prog[(size_t) i];
        prog.add (String (s.root) + ":" + String (s.quality) + ":" + String (s.bass) + ":" + String (s.beats));
    }
    h->setProperty ("progression", prog);
    o->setProperty ("harmony", var (h));
    auto* m = new DynamicObject();
    m->setProperty ("room", st.master.room); m->setProperty ("reverbMix", st.master.reverbMix);
    m->setProperty ("width", st.master.width); m->setProperty ("volume", st.master.volumeDb); m->setProperty ("bpm", st.master.bpm);
    o->setProperty ("master", var (m));
    return var (o);
}

bool stateFromVar (const var& v, sms::EngineState& st)
{
    if (! v.isObject()) return false;
    sms::EngineState s;
    const auto parts = v.getProperty ("parts", var());
    for (int i = 0; i < jmin (parts.size(), sms::kParts); ++i)
    {
        const auto& p = parts[i]; auto& ps = s.parts[(size_t) i];
        ps.instrument = instFromId (p.getProperty ("instrument", "steel").toString());
        ps.tuning = jlimit (0, (int) sms::instrument (ps.instrument).tunings.size() - 1, (int) p.getProperty ("tuning", 0));
        ps.capo = jlimit (0, 9, (int) p.getProperty ("capo", 0));
        ps.position = jlimit (0, 3, (int) p.getProperty ("position", 0));
        ps.mode = jlimit (0, (int) sms::NumModes - 1, (int) p.getProperty ("mode", 1));
        ps.octave = jlimit (-2, 2, (int) p.getProperty ("octave", 0));
        ps.levelDb = (float) (double) p.getProperty ("level", 0.0); ps.pan = (float) (double) p.getProperty ("pan", 0.0);
        ps.tone = (float) (double) p.getProperty ("tone", 0.0); ps.body = (float) (double) p.getProperty ("body", 0.3);
        ps.width = (float) (double) p.getProperty ("width", 0.7); ps.reverb = (float) (double) p.getProperty ("reverb", 0.25);
        ps.noises = (float) (double) p.getProperty ("noises", 0.35);
        ps.mute = (bool) p.getProperty ("mute", false); ps.solo = (bool) p.getProperty ("solo", false);
        patternFromVar (p.getProperty ("pattern", var()), ps.pattern);
    }
    const auto h = v.getProperty ("harmony", var());
    auto& hs = s.harmony;
    hs.keyTonic = jlimit (0, 11, (int) h.getProperty ("keyTonic", 0));
    hs.keyScale = jlimit (0, (int) sms::Scale::count - 1, (int) h.getProperty ("keyScale", 0));
    hs.tetrads = (bool) h.getProperty ("tetrads", false); hs.fieldMode = (bool) h.getProperty ("fieldMode", false);
    hs.split = jlimit (0, 128, (int) h.getProperty ("split", 60)); hs.progressionOn = (bool) h.getProperty ("progressionOn", false);
    hs.latch = (bool) h.getProperty ("latch", true);
    const auto prog = h.getProperty ("progression", var());
    hs.numProg = 0;
    for (int i = 0; i < jmin (prog.size(), sms::kMaxProg); ++i)
    {
        StringArray t; t.addTokens (prog[i].toString(), ":", "");
        if (t.size() < 4) continue;
        auto& ps = hs.prog[(size_t) hs.numProg++];
        ps.root = jlimit (0, 11, t[0].getIntValue()); ps.quality = jlimit (0, (int) sms::Quality::count - 1, t[1].getIntValue());
        ps.bass = jlimit (-1, 11, t[2].getIntValue()); ps.beats = jlimit (1, 32, t[3].getIntValue());
    }
    const auto m = v.getProperty ("master", var());
    s.master.room = (float) (double) m.getProperty ("room", 0.45); s.master.reverbMix = (float) (double) m.getProperty ("reverbMix", 0.22);
    s.master.width = (float) (double) m.getProperty ("width", 1.0); s.master.volumeDb = (float) (double) m.getProperty ("volume", -3.0);
    s.master.bpm = jlimit (30.0f, 260.0f, (float) (double) m.getProperty ("bpm", 96.0));
    st = s;
    return true;
}

var presetToVar (const sms::PresetData& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("product", "SeniorMy Acoustic Guitars");
    o->setProperty ("version", 1);
    o->setProperty ("name", String::fromUTF8 (p.name.c_str()));
    o->setProperty ("category", String::fromUTF8 (p.category.c_str()));
    o->setProperty ("description", String::fromUTF8 (p.description.c_str()));
    o->setProperty ("state", stateToVar (p.state));
    return var (o);
}

bool presetFromVar (const var& v, sms::PresetData& out)
{
    if (! v.isObject()) return false;
    out.name = v.getProperty ("name", "").toString().toStdString();
    out.category = v.getProperty ("category", "").toString().toStdString();
    out.description = v.getProperty ("description", "").toString().toStdString();
    return stateFromVar (v.getProperty ("state", var()), out.state);
}
} // namespace PresetIO

PresetManager::PresetManager() : factory (sms::factoryPresets()) { refresh(); }

//==============================================================================
File PatternLibrary::userFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SeniorMy Acoustic Guitars").getChildFile ("Padroes");
}

Array<File> PatternLibrary::userFiles()
{
    auto files = userFolder().findChildFiles (File::findFiles, false, "*.smpattern");
    files.sort();
    return files;
}

bool PatternLibrary::save (const sms::Pattern& p, String& error)
{
    const auto name = String::fromUTF8 (p.name).trim();
    if (name.isEmpty()) { error = String::fromUTF8 ("Dê um nome ao padrão."); return false; }
    auto dir = userFolder();
    if (! dir.createDirectory()) { error = String::fromUTF8 ("Não foi possível criar ") + dir.getFullPathName(); return false; }
    auto f = dir.getChildFile (File::createLegalFileName (name) + ".smpattern");
    if (! f.replaceWithText (JSON::toString (PresetIO::patternToVar (p), false))) { error = String::fromUTF8 ("Falha ao gravar ") + f.getFullPathName(); return false; }
    return true;
}

bool PatternLibrary::load (const File& f, sms::Pattern& out) { return PresetIO::patternFromVar (JSON::parse (f), out); }

File PresetManager::userFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SeniorMy Acoustic Guitars").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    list.clear();
    for (int i = 0; i < (int) factory.size(); ++i)
        list.add ({ String::fromUTF8 (factory[(size_t) i].name.c_str()), String::fromUTF8 (factory[(size_t) i].category.c_str()), true, {}, i });
    auto dir = userFolder();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (File::findFiles, false, "*.smguitar");
        files.sort();
        for (auto& f : files)
        {
            const auto v = JSON::parse (f);
            if (! v.isObject()) continue;
            list.add ({ v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension(),
                        String::fromUTF8 ("Usuário"), false, f, -1 });
        }
    }
}

bool PresetManager::load (const Entry& e, sms::PresetData& out) const
{
    if (e.factory)
    {
        if (e.factoryIndex < 0 || e.factoryIndex >= (int) factory.size()) return false;
        out = factory[(size_t) e.factoryIndex];
        return true;
    }
    return PresetIO::presetFromVar (JSON::parse (e.file), out);
}

bool PresetManager::save (const sms::PresetData& p, String& error)
{
    const auto name = String::fromUTF8 (p.name.c_str()).trim();
    if (name.isEmpty()) { error = String::fromUTF8 ("Dê um nome ao preset."); return false; }
    for (auto& f : factory)
        if (name.equalsIgnoreCase (String::fromUTF8 (f.name.c_str())))
        { error = String::fromUTF8 ("Esse nome é de um preset de fábrica. Escolha outro nome."); return false; }
    auto dir = userFolder();
    if (! dir.createDirectory()) { error = "Não foi possível criar " + dir.getFullPathName(); return false; }
    auto file = dir.getChildFile (File::createLegalFileName (name) + ".smguitar");
    sms::PresetData copy = p;
    copy.category = "User";
    if (! file.replaceWithText (JSON::toString (PresetIO::presetToVar (copy), false)))
    { error = "Falha ao gravar " + file.getFullPathName(); return false; }
    refresh();
    return true;
}

bool PresetManager::remove (const Entry& e)
{
    if (e.factory) return false;
    const bool ok = e.file.deleteFile();
    refresh();
    return ok;
}

bool PresetManager::exists (const String& name) const { return indexOf (name) >= 0; }

int PresetManager::indexOf (const String& name) const
{
    for (int i = 0; i < list.size(); ++i) if (list[i].name.equalsIgnoreCase (name)) return i;
    return -1;
}
