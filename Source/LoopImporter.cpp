#include "LoopImporter.h"

using namespace juce;

namespace LoopImporter
{
bool isSupported (const File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;mid;midi");
}

String wildcard() { return "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.mid;*.midi"; }

double bpmFromFileName (const String& name)
{
    // "loop_120bpm", "Piano 96 BPM", "92_Am_keys"
    const auto lower = name.toLowerCase();
    const int idx = lower.indexOf ("bpm");
    if (idx > 0)
    {
        int end = idx;
        while (end > 0 && lower[end - 1] == ' ') --end;
        int start = end;
        while (start > 0 && CharacterFunctions::isDigit (lower[start - 1])) --start;
        const double v = lower.substring (start, end).getDoubleValue();
        if (v >= 50 && v <= 220) return v;
    }
    StringArray tokens;
    tokens.addTokens (lower, " _-.", "");
    for (auto& t : tokens)
        if (t.containsOnly ("0123456789") && t.length() >= 2 && t.length() <= 3)
        {
            const double v = t.getDoubleValue();
            if (v >= 60 && v <= 200) return v;
        }
    return 0;
}

static std::shared_ptr<sms::Progression> analyseMidi (const File& f, String& error)
{
    FileInputStream in (f);
    MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) { error = String::fromUTF8 ("Arquivo MIDI inválido."); return nullptr; }
    const short tf = mf.getTimeFormat();
    double bpm = 120;
    MidiMessageSequence tempos;
    mf.findAllTempoEvents (tempos);
    if (tempos.getNumEvents() > 0) bpm = 60.0 / tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote();
    std::vector<sms::MidiNoteIn> notes;
    if (tf > 0)
    {
        const double tpq = (double) tf;
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            MidiMessageSequence seq (*mf.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn() || e->message.getChannel() == 10) continue;
                const double start = e->message.getTimeStamp() / tpq;
                const double end = e->noteOffObject ? e->noteOffObject->message.getTimeStamp() / tpq : start + 0.5;
                notes.push_back ({ start, jmax (0.05, end - start), e->message.getNoteNumber(), e->message.getFloatVelocity() });
            }
        }
    }
    else
    {
        mf.convertTimestampTicksToSeconds();
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            MidiMessageSequence seq (*mf.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* e = seq.getEventPointer (i);
                if (! e->message.isNoteOn() || e->message.getChannel() == 10) continue;
                const double start = e->message.getTimeStamp() * bpm / 60.0;
                const double end = e->noteOffObject ? e->noteOffObject->message.getTimeStamp() * bpm / 60.0 : start + 0.5;
                notes.push_back ({ start, jmax (0.05, end - start), e->message.getNoteNumber(), e->message.getFloatVelocity() });
            }
        }
    }
    if (notes.empty()) { error = String::fromUTF8 ("O arquivo MIDI não tem notas (fora o canal de bateria)."); return nullptr; }
    // começa no primeiro compasso com nota
    double first = 1e9;
    for (auto& n : notes) first = jmin (first, n.startBeat);
    const double shift = std::floor (first / 4.0) * 4.0;
    for (auto& n : notes) n.startBeat -= shift;
    auto p = std::make_shared<sms::Progression> (sms::analyseMidiLoop (notes, bpm));
    if (p->lengthBeats > 64) p->lengthBeats = 64;   // até 16 compassos
    return p;
}

std::shared_ptr<sms::Progression> analyse (const File& f, double hostBpm, String& error)
{
    std::shared_ptr<sms::Progression> p;
    if (f.hasFileExtension ("mid;midi")) p = analyseMidi (f, error);
    else
    {
        AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
        if (! r) { error = String::fromUTF8 ("Formato de áudio não suportado."); return nullptr; }
        const auto maxLen = (int64) (r->sampleRate * 120.0);   // até 2 minutos
        const int n = (int) jmin (r->lengthInSamples, maxLen);
        if (n < r->sampleRate * 0.5) { error = String::fromUTF8 ("Arquivo curto demais."); return nullptr; }
        AudioBuffer<float> buf ((int) jmax (1u, jmin (2u, r->numChannels)), n);
        r->read (&buf, 0, n, 0, true, true);
        std::vector<float> mono ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float s = 0;
            for (int c = 0; c < buf.getNumChannels(); ++c) s += buf.getSample (c, i);
            mono[(size_t) i] = s / (float) buf.getNumChannels();
        }
        double hint = bpmFromFileName (f.getFileNameWithoutExtension());
        if (hint <= 0 && hostBpm > 0)
        {
            // se o loop fecha em compassos inteiros no andamento do projeto, usa-o
            const double beats = (double) n / r->sampleRate * hostBpm / 60.0;
            if (std::abs (beats / 4.0 - std::round (beats / 4.0)) < 0.04) hint = hostBpm;
        }
        p = std::make_shared<sms::Progression> (sms::analyseAudioLoop (mono, r->sampleRate, hint));
    }
    if (p && p->empty()) { error = String::fromUTF8 ("Não encontrei harmonia nesse arquivo."); return nullptr; }
    if (p) p->sourceName = f.getFileName().toStdString();
    return p;
}

void applyToHarmony (const sms::Progression& p, sms::EngineState& st)
{
    auto& h = st.harmony;
    h.keyTonic = p.key.tonic; h.keyScale = (int) p.key.scale;
    h.numProg = 0;
    for (auto& c : p.chords)
    {
        if (h.numProg >= sms::kMaxProg) break;
        if (! c.chord.valid()) continue;
        const int beats = jlimit (1, 32, (int) std::lround (c.lengthBeats));
        if (h.numProg > 0)
        {
            auto& last = h.prog[(size_t) (h.numProg - 1)];
            if (last.root == c.chord.root && last.quality == (int) c.chord.quality) { last.beats = jmin (32, last.beats + beats); continue; }
        }
        auto& s = h.prog[(size_t) h.numProg++];
        s.root = c.chord.root; s.quality = (int) c.chord.quality; s.bass = c.chord.bass; s.beats = beats;
    }
    h.progressionOn = h.numProg > 0;
    if (p.bpm >= 40 && p.bpm <= 240) st.master.bpm = (float) p.bpm;
}

// Gera as partes (padrão sobre a progressão) e grava um .mid formato 1, 480 PPQ, uma trilha por parte.
File exportMidi (const sms::EngineState& st, int repeats, const File& target, String& error)
{
    const auto& h = st.harmony;
    if (h.numProg <= 0) { error = String::fromUTF8 ("Monte uma progressão de acordes antes de exportar."); return {}; }
    const double bpm = st.master.bpm;
    const int ppq = 480;
    MidiFile mf; mf.setTicksPerQuarterNote (ppq);
    MidiMessageSequence meta;
    meta.addEvent (MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0);
    meta.addEvent (MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    meta.addEvent (MidiMessage::textMetaEvent (3, "SeniorMy Acoustic Guitars"), 0);
    mf.addTrack (meta);
    int tracks = 0;
    for (int p = 0; p < sms::kParts; ++p)
    {
        const auto& ps = st.parts[(size_t) p];
        if (ps.mode != sms::ModePattern && ps.mode != sms::ModeStrumOnce) continue;
        const auto& inst = sms::instrument (ps.instrument);
        const auto& tun = sms::tuningOf (ps.instrument, ps.tuning);
        MidiMessageSequence seq;
        seq.addEvent (MidiMessage::textMetaEvent (3, String::fromUTF8 (inst.namePt) + " (" + String (p + 1) + ")"), 0);
        const int ch = p + 1;
        std::array<double, sms::kMaxCourses> openEnd {}; std::array<int, sms::kMaxCourses> openNote {};
        openNote.fill (-1);
        auto noteOffCourse = [&] (int c, double beat)
        {
            if (openNote[(size_t) c] >= 0) seq.addEvent (MidiMessage::noteOff (ch, openNote[(size_t) c]), beat * ppq);
            openNote[(size_t) c] = -1;
        };
        double beatBase = 0;
        juce::Random rnd (1234 + p);
        for (int rep = 0; rep < jmax (1, repeats); ++rep)
            for (int ci = 0; ci < h.numProg; ++ci)
            {
                const auto& cs = h.prog[(size_t) ci];
                sms::Chord chord; chord.root = cs.root; chord.quality = (sms::Quality) cs.quality; chord.bass = cs.bass;
                sms::VoicingRequest rq; rq.chord = chord; rq.position = ps.position; rq.capo = ps.capo;
                if (ps.capo > 0) { rq.chord.root = sms::pc (chord.root - ps.capo); if (rq.chord.bass >= 0) rq.chord.bass = sms::pc (rq.chord.bass - ps.capo); }
                const auto v = sms::findVoicing (inst, tun, rq);
                sms::StepContext c; c.inst = &inst; c.tuning = &tun; c.voicing = &v; c.capo = ps.capo; c.chord = chord;
                const double len = jmax (1, cs.beats);
                // cordas que mudam de casa param na troca de acorde
                for (int k = 0; k < sms::kMaxCourses; ++k) if (openNote[(size_t) k] >= 0) noteOffCourse (k, beatBase);
                sms::Pattern pat = ps.pattern;
                if (ps.mode == sms::ModeStrumOnce) { pat = {}; pat.numSteps = 1; pat.stepsPerBeat = 1; pat.grid[sms::RDown][0] = 110; }
                const double stepLen = 1.0 / jmax (1, pat.stepsPerBeat);
                const int steps = ps.mode == sms::ModeStrumOnce ? 1 : (int) std::floor (len / stepLen + 1e-6);
                for (int s = 0; s < steps; ++s)
                {
                    const long long gk = (long long) std::llround ((beatBase) / stepLen) + s;
                    double tb = beatBase + s * stepLen;
                    if (pat.swing > 0 && (gk & 1)) tb += pat.swing * stepLen * 0.5;
                    std::array<sms::PluckEvent, 48> ev {};
                    const int n = sms::generateStep (pat, (int) (gk % pat.numSteps), c, inst.strumMs, ev);
                    for (int e = 0; e < n; ++e)
                    {
                        const auto& pe = ev[(size_t) e];
                        if (pe.course < 0) continue;   // golpe: sem nota
                        const double t = tb + (pe.delayMs + rnd.nextFloat() * 4.0f * pat.humanize) * 0.001 * bpm / 60.0;
                        const auto& co = tun.courses[(size_t) pe.course];
                        const int note = co.note + ps.capo + jmax (0, v.fret[(size_t) pe.course]);
                        noteOffCourse (pe.course, t);
                        const float vel = jlimit (0.05f, 1.0f, pe.vel * (pe.kind == sms::KChuck ? 0.5f : 1.0f));
                        seq.addEvent (MidiMessage::noteOn (ch, note, vel), t * ppq);
                        openNote[(size_t) pe.course] = note;
                        if (pe.kind == sms::KChuck || pe.mute > 0.5f) noteOffCourse (pe.course, t + 0.12);
                    }
                }
                beatBase += len;
            }
        for (int k = 0; k < sms::kMaxCourses; ++k) noteOffCourse (k, beatBase);
        (void) openEnd;
        seq.updateMatchedPairs();
        mf.addTrack (seq);
        ++tracks;
    }
    if (tracks == 0) { error = String::fromUTF8 ("Nenhuma parte em modo Padrão ou Batida única para exportar."); return {}; }
    File f = target;
    if (f.isDirectory()) f = f.getChildFile ("SeniorMy Acoustic Guitars.mid");
    f.deleteFile();
    FileOutputStream out (f);
    if (! out.openedOk() || ! mf.writeTo (out)) { error = String::fromUTF8 ("Falha ao gravar ") + f.getFullPathName(); return {}; }
    out.flush();
    return f;
}
} // namespace LoopImporter
