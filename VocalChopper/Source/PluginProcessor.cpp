#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vc;

namespace
{
std::atomic<bool> suppressRedetect { false };
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalChopperProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto F = [&] (const char* id, const char* name, float lo, float hi, float def)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def)); };
    auto C = [&] (const char* id, const char* name, StringArray items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto B = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    F ("stretch", "Stretch", 0, 1, .5f);
    F ("pitch", "Pitch", 0, 1, .5f);
    F ("formant", "Formant", 0, 1, .5f);
    F ("size", "Size", 0, 1, .4f);
    F ("artifacts", "Artifacts", 0, 1, 0);
    F ("blur", "Blur", 0, 1, .15f);
    F ("in", "In", 0, 1, .5f);
    F ("out", "Out", 0, 1, .75f);
    StringArray pr, dr;
    for (auto& x : presets()) pr.add (x.name);
    for (auto* x : dirNames()) dr.add (x);
    C ("preset", "Mode", pr, 0);
    C ("direction", "Direction", dr, 0);
    B ("latch", "Latch", false);
    C ("detect", "Detection", { "AUTO", "VOCAL", "GOLPE" }, 0);
    F ("sens", "Sensitivity", 0, 1, .5f);
    F ("dj", "DJ Filter", -1, 1, 0);
    B ("keylock", "Key Lock", false);
    C ("keyroot", "Key Root", { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 4);
    C ("keyscale", "Key Scale", { "MAY", "MEN" }, 0);
    B ("fit", "Smart Tempo Fit", true);
    C ("quant", "Input Quantize", { "OFF", "1/16", "1/8", "1/4" }, 1);
    B ("fxlock", "FX Lock", false);
    F ("bpm", "Internal BPM", 60, 200, 120);
    return { p.begin(), p.end() };
}

VocalChopperProcessor::VocalChopperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VocalChopper", createLayout())
{
    formats.registerBasicFormats();
    apvts.addParameterListener ("detect", this);
    apvts.addParameterListener ("sens", this);
    juce::String msg;
    loadPatch (0, msg);
}

VocalChopperProcessor::~VocalChopperProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener ("detect", this);
    apvts.removeParameterListener ("sens", this);
}

bool VocalChopperProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void VocalChopperProcessor::setParam (const char* id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void VocalChopperProcessor::prepareToPlay (double sr, int block)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    engine.prepare (sr);
    fx.prepare (sr, std::max (block, 32));
    monoTmp.assign ((size_t) std::max (block, 32) * 2, 0.0f);
    lastHostPlaying = false;
}

EngineParams VocalChopperProcessor::readEngineParams (double bpm) const
{
    EngineParams e;
    e.stretch = pv ("stretch");
    e.pitch = pv ("pitch");
    e.size = pv ("size");
    e.preset = (int) pv ("preset");
    e.dir = (int) pv ("direction");
    e.latch = pv ("latch") > .5f;
    e.keyLock = pv ("keylock") > .5f;
    e.scale = (int) pv ("keyscale");
    e.fit = pv ("fit") > .5f;
    static const int qs[4] = { 0, 16, 8, 4 };
    e.quant = qs[clampv ((int) pv ("quant"), 0, 3)];
    e.bpm = bpm;
    return e;
}

FxParams VocalChopperProcessor::readFxParams (double bpm) const
{
    FxParams f;
    f.in = pv ("in"); f.out = pv ("out"); f.formant = pv ("formant");
    f.artifacts = pv ("artifacts"); f.blur = pv ("blur"); f.dj = pv ("dj");
    f.pitch = pv ("pitch"); f.bpm = bpm;
    return f;
}

void VocalChopperProcessor::pushEvent (int type, int ci)
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0) fifoData[(size_t) scope.startIndex1] = { type, ci };
    else if (scope.blockSize2 > 0) fifoData[(size_t) scope.startIndex2] = { type, ci };
}

void VocalChopperProcessor::togglePlay()
{
    const bool now = ! internalPlay.load();
    internalPlay = now;
    if (! now && hostPlaying.load()) { /* el DAW manda mientras esta en marcha */ }
}

void VocalChopperProcessor::stopAll() { internalPlay = false; pushEvent (evStop, 0); }
void VocalChopperProcessor::pressChop (int ci) { selected = ci; pushEvent (evPress, ci); }
void VocalChopperProcessor::releaseChop (int ci) { pushEvent (evRelease, ci); }

void VocalChopperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();

    // ---- grabacion de la entrada del canal
    if (recording.load() && nIn > 0)
    {
        int pos = recPos.load();
        const int cap = recBuf.getNumSamples();
        const int m = std::min (n, cap - pos);
        if (m > 0)
        {
            for (int c = 0; c < 2; ++c)
                recBuf.copyFrom (c, pos, buffer, std::min (c, nIn - 1), 0, m);
            recPos = pos + m;
        }
    }

    // ---- tempo y transporte del DAW
    double bpm = pv ("bpm");
    bool hp = false, haveTempo = false;
    double ppq = -1;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { if (*b > 20 && *b < 400) { bpm = *b; haveTempo = true; } }
            hp = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    }
    hostPlaying = hp;
    hostTempo = haveTempo;
    currentBpm = bpm;

    buffer.clear();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R == nullptr)
    {
        if ((int) monoTmp.size() < n) monoTmp.resize ((size_t) n); // solo pasa con salida mono
        std::fill (monoTmp.begin(), monoTmp.begin() + n, 0.0f);
        R = monoTmp.data();
    }

    {
        const juce::SpinLock::ScopedLockType sl (engine.lock);
        engine.prm = readEngineParams (bpm);

        if (engine.prm.preset != lastPreset || engine.prm.dir != lastDir) engine.resetRoll();
        lastPreset = engine.prm.preset; lastDir = engine.prm.dir;
        if (lastLatch && ! engine.prm.latch) engine.latchOff();
        lastLatch = engine.prm.latch;

        // eventos de la interfaz
        {
            const auto scope = fifo.read (fifo.getNumReady());
            auto run = [&] (int start, int size)
            {
                for (int i = 0; i < size; ++i)
                {
                    const auto& ev = fifoData[(size_t) (start + i)];
                    if (ev.type == evPress) engine.press (ev.ci);
                    else if (ev.type == evRelease) engine.release (ev.ci);
                    else if (ev.type == evStop) engine.stop();
                }
            };
            run (scope.startIndex1, scope.blockSize1);
            run (scope.startIndex2, scope.blockSize2);
        }

        const double stepPos = ppq >= 0 ? ppq * 4.0 : -1.0;
        const bool shouldPlay = internalPlay.load() || hp;
        if (hp && ! lastHostPlaying) engine.start (stepPos);          // el DAW arranca: nos alineamos al compas
        else if (shouldPlay && ! engine.isPlaying()) engine.start (hp ? stepPos : -1.0);
        else if (! shouldPlay && engine.isPlaying()) engine.stop();
        if (hp && engine.isPlaying() && stepPos >= 0) engine.followHost (stepPos);
        lastHostPlaying = hp;

        // render con MIDI preciso a la muestra
        int pos = 0;
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos) { engine.render (L + pos, R + pos, at - pos); pos = at; }
            if (msg.isNoteOn()) engine.press (msg.getNoteNumber() - 36);
            else if (msg.isNoteOff()) engine.release (msg.getNoteNumber() - 36);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.killAll();
        }
        if (pos < n) engine.render (L + pos, R + pos, n - pos);
        enginePlaying = engine.isPlaying();
    }

    fx.process (L, R, n, readFxParams (bpm));
    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < n; ++i) L[i] = (L[i] + R[i]) * .5f;
    midi.clear();
}

// ------------------------------------------------------------------ contenido
void VocalChopperProcessor::buildChops (const std::vector<double>& cuts, double d)
{
    std::vector<double> pts = cuts;
    pts.push_back (std::min (d, engine.B));
    engine.chops.clear();
    for (size_t i = 0; i + 1 < pts.size(); ++i)
        if (pts[i + 1] - pts[i] > .02) engine.chops.push_back ({ pts[i], pts[i + 1], 0, 0 });
    if (engine.chops.empty()) engine.chops.push_back ({ 0, d, 0, 0 });
    engine.B = std::max (engine.B, engine.chops.back().e);
}

void VocalChopperProcessor::setContent (std::shared_ptr<SampleData> data, const std::vector<double>& cuts, bool isHit,
                                        double bpm, double A, double B, const juce::String& name)
{
    std::shared_ptr<const SampleData> old;
    {
        const juce::SpinLock::ScopedLockType sl (engine.lock);
        old = engine.sample;
        engine.killAll (true);
        engine.sample = data;
        engine.isHit = isHit;
        engine.srcBpm = bpm;
        engine.A = A;
        engine.B = B;
        buildChops (cuts, data->duration());
    }
    sampleName = name;
    selected = -1;
    ++contentVersion;
}

static std::shared_ptr<SampleData> toSample (juce::AudioBuffer<float>&& b, double sr)
{
    auto d = std::make_shared<SampleData>();
    d->sr = sr;
    if (b.getNumChannels() == 1)
    {
        d->buf.setSize (2, b.getNumSamples());
        d->buf.copyFrom (0, 0, b, 0, 0, b.getNumSamples());
        d->buf.copyFrom (1, 0, b, 0, 0, b.getNumSamples());
    }
    else d->buf = std::move (b);
    return d;
}

void VocalChopperProcessor::loadPatch (int i, juce::String& msg)
{
    const auto& list = patchList();
    i = clampv (i, 0, (int) list.size() - 1);
    const auto& info = list[(size_t) i];
    auto g = generatePatch (i);
    auto an = analyse (g.buf, g.sr, pv ("detect"), pv ("sens"));
    const double A = g.cuts.empty() ? an.A : std::max (0.0, g.cuts[0] - .005);
    auto data = toSample (std::move (g.buf), g.sr);
    setContent (data, g.cuts.empty() ? an.cuts : g.cuts, info.isHit, g.bpm > 0 ? g.bpm : an.bpm, A, an.B, info.name);
    patchIndex = i;

    suppressRedetect = true;
    setParam ("preset", (float) info.set.preset);
    setParam ("direction", (float) info.set.dir);
    setParam ("size", (float) info.set.size);
    setParam ("pitch", .5f);
    setParam ("stretch", .5f);
    if (pv ("fxlock") < .5f)
    {
        setParam ("artifacts", (float) info.set.artifacts);
        setParam ("blur", (float) info.set.blur);
    }
    suppressRedetect = false;
    msg = juce::String (info.name) + " - " + juce::String (numChops()) + " chops desde C1";
}

bool VocalChopperProcessor::loadFile (const juce::File& f, juce::String& msg)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->sampleRate <= 0) { msg = "No he podido abrir ese archivo"; return false; }
    const auto maxLen = (juce::int64) (r->sampleRate * 600.0); // hasta 10 minutos
    const int len = (int) std::min (r->lengthInSamples, maxLen);
    const int ch = (int) std::min<unsigned int> (2, r->numChannels);
    juce::AudioBuffer<float> b (ch, len);
    r->read (&b, 0, len, 0, true, ch > 1);
    auto an = analyse (b, r->sampleRate, pv ("detect"), pv ("sens"));
    auto data = toSample (std::move (b), r->sampleRate);
    setContent (data, an.cuts, an.isHit, an.bpm, an.A, an.B, f.getFileNameWithoutExtension());
    patchIndex = -1;
    msg = sampleName + " - " + juce::String (numChops()) + " chops - " + (an.isHit ? "GOLPE" : "VOCAL");
    return true;
}

juce::String VocalChopperProcessor::redetect()
{
    std::shared_ptr<const SampleData> s;
    { const juce::SpinLock::ScopedLockType sl (engine.lock); s = engine.sample; }
    if (! s) return {};
    auto an = analyse (s->buf, s->sr, pv ("detect"), pv ("sens"));
    {
        const juce::SpinLock::ScopedLockType sl (engine.lock);
        engine.isHit = an.isHit;
        engine.srcBpm = an.bpm;
        engine.A = an.A;
        engine.B = an.B;
        buildChops (an.cuts, s->duration());
        engine.resetRoll();
    }
    selected = -1;
    ++contentVersion;
    return juce::String (numChops()) + " chops - modo " + (an.isHit ? "GOLPE" : "VOCAL");
}

void VocalChopperProcessor::parameterChanged (const juce::String&, float)
{
    if (! suppressRedetect.load()) triggerAsyncUpdate();
}

void VocalChopperProcessor::handleAsyncUpdate()
{
    auto msg = redetect();
    if (auto* ed = dynamic_cast<VocalChopperEditor*> (getActiveEditor()))
        if (msg.isNotEmpty()) ed->toast (msg);
}

ContentSnapshot VocalChopperProcessor::snapshot()
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    return { engine.sample, engine.chops, engine.A, engine.B, engine.srcBpm, engine.isHit };
}

int VocalChopperProcessor::numChops() { const juce::SpinLock::ScopedLockType sl (engine.lock); return (int) engine.chops.size(); }

void VocalChopperProcessor::moveLine (int i, double t)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    auto& c = engine.chops;
    if (i <= 0 || i >= (int) c.size()) return;
    const double a = c[(size_t) i - 1].s + .02, b = c[(size_t) i].e - .02;
    const double v = clampv (t, a, b);
    c[(size_t) i - 1].e = v;
    c[(size_t) i].s = v;
}

void VocalChopperProcessor::setMarker (bool start, double t)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    if (! engine.sample) return;
    if (start) engine.A = clampv (t, 0.0, engine.B - .05);
    else engine.B = clampv (t, engine.A + .05, engine.sample->duration());
}

bool VocalChopperProcessor::addCut (double t)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    const int i = engine.chopAt (t);
    if (i < 0) return false;
    auto ch = engine.chops[(size_t) i];
    if (t - ch.s > .03 && ch.e - t > .03)
    {
        engine.chops.insert (engine.chops.begin() + i + 1, Chop { t, ch.e, ch.pitch, ch.dir });
        engine.chops[(size_t) i].e = t;
        engine.resetRoll();
        return true;
    }
    return false;
}

void VocalChopperProcessor::removeLine (int i)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    auto& c = engine.chops;
    if (i <= 0 || i >= (int) c.size()) return;
    c[(size_t) i - 1].e = c[(size_t) i].e;
    c.erase (c.begin() + i);
    engine.resetRoll();
    engine.killAll();
}

void VocalChopperProcessor::setChopPitch (int ci, int st)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    if (ci >= 0 && ci < (int) engine.chops.size()) engine.chops[(size_t) ci].pitch = clampv (st, -12, 12);
}
void VocalChopperProcessor::setChopDir (int ci, int d)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    if (ci >= 0 && ci < (int) engine.chops.size()) engine.chops[(size_t) ci].dir = clampv (d, 0, 2);
    engine.resetRoll();
}
int VocalChopperProcessor::getChopPitch (int ci)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    return (ci >= 0 && ci < (int) engine.chops.size()) ? engine.chops[(size_t) ci].pitch : 0;
}
int VocalChopperProcessor::getChopDir (int ci)
{
    const juce::SpinLock::ScopedLockType sl (engine.lock);
    return (ci >= 0 && ci < (int) engine.chops.size()) ? engine.chops[(size_t) ci].dir : 0;
}

// ------------------------------------------------------------------ grabacion
bool VocalChopperProcessor::toggleRecord (juce::String& msg)
{
    if (! recording.load())
    {
        recSR = getSampleRate() > 0 ? getSampleRate() : 44100.0;
        recBuf.setSize (2, (int) (recSR * 300.0), false, true, false); // hasta 5 minutos
        recBuf.clear();
        recPos = 0;
        recording = true;
        msg = getTotalNumInputChannels() > 0 ? "Grabando la entrada... pulsa otra vez para parar"
                                             : "Este canal no tiene entrada de audio";
        return true;
    }
    recording = false;
    juce::Thread::sleep (30); // deja terminar el bloque de audio en curso
    const int len = recPos.load();
    float peak = 0;
    for (int c = 0; c < 2; ++c) peak = std::max (peak, recBuf.getMagnitude (c, 0, std::max (0, len)));
    if (len < (int) (recSR * .2) || peak < 1e-4f)
    {
        msg = "No ha llegado audio a la entrada del plugin";
        recBuf.setSize (2, 0);
        return false;
    }
    juce::AudioBuffer<float> b (2, len);
    for (int c = 0; c < 2; ++c) b.copyFrom (c, 0, recBuf, c, 0, len);
    recBuf.setSize (2, 0);
    b.applyGain (0.85f / peak);
    auto an = analyse (b, recSR, pv ("detect"), pv ("sens"));
    auto data = toSample (std::move (b), recSR);
    setContent (data, an.cuts, an.isHit, an.bpm, an.A, an.B, "Grabacion");
    patchIndex = -1;
    msg = "Grabacion - " + juce::String (numChops()) + " chops - " + (an.isHit ? "GOLPE" : "VOCAL");
    return true;
}

// ------------------------------------------------------------------ render a WAV 24 bits
juce::File VocalChopperProcessor::renderToFile (std::function<void (float)> progress, int& barsOut)
{
    auto snap = snapshot();
    if (! snap.sample) return {};
    const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;
    const double bpm = currentBpm.load();

    auto eng = std::make_unique<Engine>();
    auto fxc = std::make_unique<FxChain>();
    eng->prepare (sr);
    eng->sample = snap.sample;
    eng->chops = snap.chops;
    eng->A = snap.A; eng->B = snap.B; eng->srcBpm = snap.srcBpm; eng->isHit = snap.isHit;
    eng->prm = readEngineParams (bpm);
    eng->prm.latch = false;
    const int block = 512;
    fxc->prepare (sr, block);
    const auto fp = readFxParams (bpm);

    const int bars = clampv ((int) std::ceil ((snap.B - snap.A) / eng->speed() * bpm / 240.0), 2, 8);
    barsOut = bars;
    const double lenSec = bars * 240.0 / bpm;
    const int total = (int) std::lround (lenSec * sr);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    eng->start (0.0);
    for (int pos = 0; pos < total; pos += block)
    {
        const int m = std::min (block, total - pos);
        float* L = out.getWritePointer (0, pos);
        float* R = out.getWritePointer (1, pos);
        eng->render (L, R, m);
        fxc->process (L, R, m, fp);
        if (progress && (pos / block) % 64 == 0) progress ((float) pos / (float) total);
    }

    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Soundtastics").getChildFile ("Vocal Chopper").getChildFile ("Renders");
    dir.createDirectory();
    auto base = (sampleName.isEmpty() ? juce::String ("chop") : sampleName).retainCharacters (
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ").trim().replaceCharacter (' ', '_');
    auto file = dir.getNonexistentChildFile (base + "_chop_" + juce::String ((int) std::lround (bpm)) + "bpm", ".wav");

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (os.get())->openedOk()) return {};
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) return {};
    w->writeFromAudioSampleBuffer (out, 0, total);
    w.reset();
    if (progress) progress (1.0f);
    lastRenderPath = file.getFullPathName();
    return file;
}

// ------------------------------------------------------------------ estado en la sesion del DAW
void VocalChopperProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto snap = snapshot();
    auto* c = xml->createNewChildElement ("Content");
    c->setAttribute ("name", sampleName);
    c->setAttribute ("patch", patchIndex);
    c->setAttribute ("isHit", snap.isHit);
    c->setAttribute ("srcBpm", snap.srcBpm);
    c->setAttribute ("A", snap.A);
    c->setAttribute ("B", snap.B);
    for (auto& ch : snap.chops)
    {
        auto* e = c->createNewChildElement ("Chop");
        e->setAttribute ("s", ch.s); e->setAttribute ("e", ch.e);
        e->setAttribute ("pitch", ch.pitch); e->setAttribute ("dir", ch.dir);
    }
    if (snap.sample)
    {
        juce::MemoryBlock mb;
        {
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::MemoryOutputStream> (mb, false);
            auto w = flac.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (snap.sample->sr)
                                                   .withNumChannels (2).withBitsPerSample (24));
            if (w != nullptr) w->writeFromAudioSampleBuffer (snap.sample->buf, 0, snap.sample->buf.getNumSamples());
        }
        c->setAttribute ("sr", snap.sample->sr);
        c->setAttribute ("audio", mb.toBase64Encoding());
    }
    copyXmlToBinary (*xml, dest);
}

void VocalChopperProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr) return;
    std::unique_ptr<juce::XmlElement> content;
    if (auto* c = xml->getChildByName ("Content"))
    {
        content = std::make_unique<juce::XmlElement> (*c);
        xml->removeChildElement (c, true);
    }
    suppressRedetect = true;
    if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
    suppressRedetect = false;
    if (content == nullptr) return;

    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (content->getStringAttribute ("audio"))) return;
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> r (flac.createReaderFor (new juce::MemoryInputStream (mb, false), true));
    if (r == nullptr) return;
    const int len = (int) r->lengthInSamples;
    juce::AudioBuffer<float> b (2, len);
    r->read (&b, 0, len, 0, true, true);
    auto sd = toSample (std::move (b), content->getDoubleAttribute ("sr", r->sampleRate));

    std::vector<Chop> chops;
    for (auto* e : content->getChildWithTagNameIterator ("Chop"))
        chops.push_back ({ e->getDoubleAttribute ("s"), e->getDoubleAttribute ("e"), e->getIntAttribute ("pitch"), e->getIntAttribute ("dir") });
    {
        const juce::SpinLock::ScopedLockType sl (engine.lock);
        engine.killAll (true);
        engine.sample = sd;
        engine.isHit = content->getBoolAttribute ("isHit");
        engine.srcBpm = content->getDoubleAttribute ("srcBpm");
        engine.A = content->getDoubleAttribute ("A");
        engine.B = content->getDoubleAttribute ("B", sd->duration());
        if (! chops.empty()) engine.chops = chops;
        else engine.chops = { { 0, sd->duration(), 0, 0 } };
    }
    sampleName = content->getStringAttribute ("name");
    patchIndex = content->getIntAttribute ("patch", -1);
    selected = -1;
    ++contentVersion;
}

juce::AudioProcessorEditor* VocalChopperProcessor::createEditor() { return new VocalChopperEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalChopperProcessor(); }
