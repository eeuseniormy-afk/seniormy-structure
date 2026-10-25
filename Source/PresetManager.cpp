#include "PresetManager.h"

using namespace juce;

namespace PresetIO
{
var arrangementToVar (const sms::Arrangement& a)
{
    auto* o = new DynamicObject();
    o->setProperty ("name", String (a.name));
    Array<var> lanes;
    for (auto& l : a.lanes)
    {
        auto* lo = new DynamicObject();
        lo->setProperty ("name", String::fromUTF8 (l.name.c_str()));
        lo->setProperty ("type", (int) l.type);
        lo->setProperty ("artic", l.artic);
        lo->setProperty ("sections", (int) l.sectionMask);
        lo->setProperty ("enabled", l.enabled);
        lo->setProperty ("gain", l.gain);
        lo->setProperty ("octave", l.octave);
        lo->setProperty ("stepsPerBeat", l.stepsPerBeat);
        lo->setProperty ("numSteps", l.numSteps);
        lo->setProperty ("gate", l.gate);
        Array<var> rows;
        for (int r = 0; r < sms::kRows; ++r)
        {
            String s;
            for (int c = 0; c < sms::kMaxSteps; ++c) s << String::toHexString ((int) l.grid[(size_t) r][(size_t) c]).paddedLeft ('0', 2);
            rows.add (s);
        }
        lo->setProperty ("grid", rows);
        lanes.add (var (lo));
    }
    o->setProperty ("lanes", lanes);
    return var (o);
}

sms::Arrangement arrangementFromVar (const var& v)
{
    sms::Arrangement a;
    a.name = v["name"].toString().toStdString();
    if (auto* lanes = v["lanes"].getArray())
        for (auto& lv : *lanes)
        {
            if ((int) a.lanes.size() >= sms::kMaxLanes) break;
            sms::Lane l;
            l.name = lv["name"].toString().toStdString();
            l.type = (sms::LaneType) jlimit (0, 3, (int) lv.getProperty ("type", 0));
            l.artic = jlimit (0, sms::numArtics - 1, (int) lv.getProperty ("artic", 0));
            l.sectionMask = (uint8_t) jlimit (0, 63, (int) lv.getProperty ("sections", 31));
            l.enabled = (bool) lv.getProperty ("enabled", true);
            l.gain = jlimit (0.0f, 1.5f, (float) (double) lv.getProperty ("gain", 1.0));
            l.octave = jlimit (-2, 2, (int) lv.getProperty ("octave", 0));
            l.stepsPerBeat = jlimit (1, 8, (int) lv.getProperty ("stepsPerBeat", 4));
            l.numSteps = jlimit (1, sms::kMaxSteps, (int) lv.getProperty ("numSteps", 16));
            l.gate = jlimit (0.05f, 1.0f, (float) (double) lv.getProperty ("gate", 0.5));
            l.clearGrid();
            if (auto* rows = lv["grid"].getArray())
                for (int r = 0; r < jmin (sms::kRows, rows->size()); ++r)
                {
                    const auto s = (*rows)[r].toString();
                    for (int c = 0; c < sms::kMaxSteps && c * 2 + 1 < s.length(); ++c)
                        l.grid[(size_t) r][(size_t) c] = (uint8_t) jlimit (0, 127, s.substring (c * 2, c * 2 + 2).getHexValue32());
                }
            a.lanes.push_back (l);
        }
    return a;
}

var presetToVar (const sms::PresetData& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("format", "SeniorMy Structure preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", String::fromUTF8 (p.name.c_str()));
    o->setProperty ("category", String::fromUTF8 (p.category.c_str()));
    o->setProperty ("arrangement", arrangementToVar (p.arrangement));
    auto* params = new DynamicObject();
    for (auto& [id, val] : p.params) params->setProperty (Identifier (String (id)), val);
    o->setProperty ("params", var (params));
    return var (o);
}

bool presetFromVar (const var& v, sms::PresetData& out)
{
    if (! v.isObject() || v["arrangement"].isVoid()) return false;
    out.name = v["name"].toString().toStdString();
    out.category = v["category"].toString().toStdString();
    out.arrangement = arrangementFromVar (v["arrangement"]);
    out.params.clear();
    if (auto* po = v["params"].getDynamicObject())
        for (auto& p : po->getProperties())
            out.params.push_back ({ p.name.toString().toStdString(), (float) (double) p.value });
    return true;
}
} // namespace PresetIO

PresetManager::PresetManager() : factory (sms::factoryPresets()) { refresh(); }

File PresetManager::userFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SeniorMy Structure").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    list.clear();
    for (int i = 0; i < (int) factory.size(); ++i)
        list.add ({ String::fromUTF8 (factory[(size_t) i].name.c_str()), String::fromUTF8 (factory[(size_t) i].category.c_str()), true, {}, i });
    auto dir = userFolder();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (File::findFiles, false, "*.smstructure");
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
    auto file = dir.getChildFile (File::createLegalFileName (name) + ".smstructure");
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
