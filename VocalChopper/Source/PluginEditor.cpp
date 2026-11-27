#include "PluginEditor.h"

using namespace juce;
using namespace vc;

namespace
{
const char* kSvgPlay  = R"(<svg xmlns="http://www.w3.org/2000/svg" width="28" height="28" viewBox="0 0 28 28"><path d="M7 4.4v19.2l15.8-9.6z" fill="currentColor"/></svg>)";
const char* kSvgStop  = R"(<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 20 20"><rect x="2" y="2" width="16" height="16" rx="3" fill="currentColor"/></svg>)";
const char* kSvgRec   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8.5" fill="none" stroke="#ff4d5a" stroke-width="3.6"/></svg>)";
const char* kSvgLib   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="4" height="16" rx="1"/><rect x="9" y="4" width="4" height="16" rx="1"/><path d="M15.5 5.2l3.8-1 4 15.4-3.8 1z"/></svg>)";
const char* kSvgImp   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4v11M7 10l5 5 5-5M4 19h16"/></svg>)";
const char* kSvgSet   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"><path d="M4 7h10M18 7h2M4 17h4M12 17h8"/><circle cx="16" cy="7" r="2"/><circle cx="10" cy="17" r="2"/></svg>)";

const char* kKnobId[7] = { "stretch", "pitch", "size", "artifacts", "blur", "in", "out" };
const char* kKnobLabel[5] = { "STRETCH", "PITCH", "SIZE", "ARTIFACTS", "BLUR" };
const char* kEnds[5][2] = { { "\xc2\xbd\xc3\x97", "2\xc3\x97" }, { "-12", "+12" }, { "1/4", "1/64" }, { "CLEAN", "CRUSH" }, { "DRY", "WET" } };
const float kKnobDef[7] = { .5f, .5f, .4f, 0.0f, .15f, .5f, .75f };
const char* kModes[6] = { "STRAIGHT", "STUTTER", "BUILD UP", "TRIPLETS", "GLITCH", "CHOP SEQ" };

// caja visible (como en el prototipo) -> limites del componente (anillo negro + sombra)
Rectangle<int> hb (int x, int y, int w, int h) { return { x - 2, y - 2, w + 4, h + 7 }; }

constexpr float kKnobY = 537.0f;              // centro vertical de los macros
float knobCx (int c) { return 150.0f + 136.0f * c + 68.0f; }

const String dot = String::fromUTF8 (" \xc2\xb7 ");
}

const String VocalChopperEditor::keyMap = "awsedftgyhujkolp;";

VocalChopperEditor::VocalChopperEditor (VocalChopperProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      bLib ({}, kSvgLib), bImp ("IMPORTAR", kSvgImp),
      bPlay ({}, kSvgPlay), bStop ({}, kSvgStop), bRec ({}, kSvgRec),
      bAjustes ("AJUSTES", kSvgSet), bLatch (U8 ("LATCH \xe2\x88\x9e"), {}, 12.5f),
      lcd (p), render (p), wave (p), setOv (p), libOv (p)
{
    bLib.text = U8 ("LIBRER\xc3\x8d" "A");
    setLookAndFeel (&look);
    addAndMakeVisible (panel);
    panel.painter = [this] (Graphics& g) { paintPanel (g); };

    // ---- cabecera
    for (auto* b : { &bLib, &bImp }) panel.addAndMakeVisible (*b);
    bLib.setBounds (hb (666, 20, 118, 36));
    bImp.setBounds (hb (792, 20, 118, 36));
    bLib.setTooltip (U8 ("Librer\xc3\xad" "a de f\xc3\xa1" "brica"));
    bImp.setTooltip ("Importar audio (.wav .mp3 .aiff .flac .ogg)");
    bLib.onClick = [this] { showLibrary (! libOv.isVisible()); };
    bImp.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Importar audio", File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) { String m; proc.loadFile (f, m); afterLoad (m); }
        });
    };

    // ---- columna izquierda: transporte
    for (auto* b : { &bPlay, &bStop, &bRec }) panel.addAndMakeVisible (*b);
    bPlay.setBounds (hb (32, 276, 86, 84));
    bStop.setBounds (hb (32, 370, 86, 56));
    bRec.setBounds (hb (32, 436, 86, 56));
    bRec.onColour = ui::c::rec;
    bPlay.setTooltip ("Play (espacio). Dentro del DAW arranca solo con el transporte");
    bStop.setTooltip ("Stop");
    bRec.setTooltip (U8 ("Grabar la entrada de audio del canal (o el micr\xc3\xb3" "fono en la versi\xc3\xb3n Standalone)"));
    bPlay.onClick = [this] { proc.togglePlay(); };
    bStop.onClick = [this] { proc.stopAll(); };
    bRec.onClick = [this] { String m; proc.toggleRecord (m); if (m.isNotEmpty()) afterLoad (m); };

    // ---- columna derecha
    panel.addAndMakeVisible (lcd);
    lcd.setBounds (851, 273, 108, 68);
    lcd.setTooltip ("Tempo. Dentro del DAW sigue al proyecto; fuera, arrastra arriba/abajo para cambiarlo (doble clic = tempo del loop)");
    panel.addAndMakeVisible (render);
    render.setBounds (852, 350, 106, 106);
    render.toast = [this] (const String& m) { toast (m); };
    panel.addAndMakeVisible (bAjustes);
    bAjustes.setBounds (hb (854, 520, 102, 40));
    bAjustes.setTooltip ("Ajustes del motor: cortes, filtro DJ, Key Lock, tempo, quantize, FX Lock");
    bAjustes.onClick = [this] { showSettings (! setOv.isVisible()); };

    // ---- panel CHOPPER: modos + pantalla
    for (int i = 0; i < 6; ++i)
    {
        auto b = std::make_unique<ui::BlackButton> (kModes[i], String(), 13.5f);
        const float w = (652.0f - 20.0f) / 6.0f;
        b->setBounds (hb (roundToInt (164 + i * (w + 4)), 126, roundToInt (w), 38));
        b->setTooltip (presets()[(size_t) i].info);
        b->onClick = [this, i] { proc.setParam ("preset", (float) i); refreshButtons(); };
        panel.addAndMakeVisible (*b);
        modeBtn[(size_t) i] = std::move (b);
    }
    panel.addAndMakeVisible (wave);
    wave.setBounds (166, 176, 648, 206);
    wave.toast = [this] (const String& m) { toast (m); };
    wave.onSelectionChanged = [this] { refreshButtons(); for (auto& k : knobs) k->repaint(); panel.repaint (150, 404, 680, 244); };
    wave.addChildComponent (setOv);
    wave.addChildComponent (libOv);
    setOv.setBounds (0, 0, 648, 206);
    libOv.setBounds (0, 0, 648, 206);
    setOv.onClose = [this] { showSettings (false); };
    setOv.toast = [this] (const String& m) { toast (m); };
    libOv.close.onClick = [this] { showLibrary (false); };
    libOv.onPick = [this] (int i)
    {
        String m;
        proc.loadPatch (i, m);
        showLibrary (false);
        afterLoad (m);
    };

    // ---- panel MACROS: direccion + latch + mandos
    {
        const float w = (396.0f - 72.0f - 4 * 4 - 6) / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto b = std::make_unique<ui::BlackButton> (dirNames()[(size_t) i], String(), 12.5f);
            b->setBounds (hb (roundToInt (420 + i * (w + 4)), 418, roundToInt (w), 34));
            b->onClick = [this, i] { pickDirection (i); };
            panel.addAndMakeVisible (*b);
            dirBtn[(size_t) i] = std::move (b);
        }
        panel.addAndMakeVisible (bLatch);
        bLatch.setBounds (hb (816 - 72, 418, 72, 34));
        bLatch.setTooltip ("Latch: el chop o el roll se queda en bucle");
        bLatch.onClick = [this]
        {
            const bool on = proc.pv ("latch") < .5f;
            proc.setParam ("latch", on ? 1.0f : 0.0f);
            toast (on ? U8 ("LATCH ON \xc2\xb7 el chop se queda en bucle") : String ("LATCH OFF"));
            refreshButtons();
        };
    }
    setupKnobs();

    panel.addAndMakeVisible (toastC);
    toastC.setBounds (0, 339, 980, 28);

    addMouseListener (this, true);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (588, 396, 1960, 1320);
    if (auto* cst = getConstrainer()) cst->setFixedAspectRatio (980.0 / 660.0);
    setSize (980, 660);

    refreshButtons();
    wave.tick();
    startTimerHz (30);
}

VocalChopperEditor::~VocalChopperEditor()
{
    stopTimer();
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void VocalChopperEditor::setupKnobs()
{
    for (int k = 0; k < 7; ++k)
    {
        const bool io = k >= 5;
        auto kn = std::make_unique<ui::Knob> (! io);
        auto* id = kKnobId[k];
        const bool isPitch = k == 1;
        kn->getValue = [this, id, isPitch] (bool alt) -> float
        {
            if (isPitch && alt) return proc.pv ("formant");
            if (isPitch && proc.selected >= 0) return (proc.getChopPitch (proc.selected) + 12) / 24.0f;
            return proc.pv (id);
        };
        kn->setValue = [this, id, isPitch, k] (float v, bool alt)
        {
            if (isPitch && alt) { if (auto* pr = proc.apvts.getParameter ("formant")) pr->setValueNotifyingHost (v); }
            else if (isPitch && proc.selected >= 0) proc.setChopPitch (proc.selected, (int) std::lround (v * 24 - 12));
            else if (auto* pr = proc.apvts.getParameter (id)) pr->setValueNotifyingHost (pr->convertTo0to1 (v));
            if (k >= 5) toast ((k == 5 ? "INPUT " : "OUTPUT ") + knobText (k));
        };
        kn->gesture = [this, id, isPitch] (bool begin, bool alt)
        {
            if (isPitch && ! alt && proc.selected >= 0) return;
            if (auto* pr = proc.apvts.getParameter (isPitch && alt ? "formant" : id)) { if (begin) pr->beginChangeGesture(); else pr->endChangeGesture(); }
        };
        kn->onReset = [this, id, isPitch, k]
        {
            if (isPitch && proc.selected >= 0) proc.setChopPitch (proc.selected, 0);
            else proc.setParam (id, kKnobDef[k]);
        };
        kn->setTooltip (isPitch ? U8 ("Arrastra: semitonos \xc2\xb7 Alt/Shift + arrastra: formantes \xc2\xb7 doble clic: reset")
                                : U8 ("Arrastra arriba/abajo \xc2\xb7 Ctrl = fino \xc2\xb7 doble clic: reset"));
        panel.addAndMakeVisible (*kn);
        if (io) kn->setBounds (k == 5 ? 75 - 56 : 905 - 56, 118, 112, 112);
        else kn->setBounds (roundToInt (knobCx (k) - 49), roundToInt (kKnobY - 49), 98, 98);
        knobs[(size_t) k] = std::move (kn);
    }
}

String VocalChopperEditor::knobText (int k)
{
    const float v = proc.pv (kKnobId[k]);
    switch (k)
    {
        case 0: return U8 ("LONG \xc3\x97") + String (1.0 / spd (v), 2);
        case 1:
        {
            if (knobs[1] && knobs[1]->altHeld)
            {
                const int f = (int) std::lround ((proc.pv ("formant") - .5) * 24);
                return "FORMANT " + String (f > 0 ? "+" : "") + String (f);
            }
            const int s = proc.selected >= 0 ? proc.getChopPitch (proc.selected) : globalSemis (v);
            return String (s > 0 ? "+" : "") + String (s) + " st" + (proc.pv ("keylock") > .5f ? dot + "KEY" : String());
        }
        case 2: return "GATE " + String (roundToInt (gateFrac (v) * 100)) + "%" + dot + "ROLL " + subs()[(size_t) subIdx (v)].name;
        case 3: return v < .02f ? String ("CLEAN") : v <= .5f ? "TAPE " + String (roundToInt (v * 200)) + "%" : "CRUSH " + String (roundToInt ((v - .5f) * 200)) + "%";
        case 4: return String (roundToInt (v * 100)) + "% SPACE";
        case 5: { const double d = inDb (v); return String (d > 0 ? "+" : "") + String (d, 1) + " dB" + (v > .5f ? " CLIP" : ""); }
        default: return v < .01f ? U8 ("-\xe2\x88\x9e") : String (40 * std::log10 (v / .75), 1) + " dB";
    }
}

void VocalChopperEditor::paint (Graphics& g) { g.fillAll (Colour (0xff0f1638)); }

void VocalChopperEditor::resized()
{
    const float s = getWidth() / 980.0f;
    panel.setBounds (0, 0, 980, 660);
    panel.setTransform (AffineTransform::scale (s));
}

// ------------------------------------------------------------------ dibujo del cuerpo (todo lo que no es un control)
void VocalChopperEditor::paintPanel (Graphics& g)
{
    using namespace ui;
    const auto B = Rectangle<float> (0, 0, 980, 660);
    Path shape; shape.addRoundedRectangle (B, 22);
    g.reduceClipRegion (shape);
    g.setColour (ui::c::body);
    g.fillPath (shape);
    g.setColour (Colours::black.withAlpha (.18f));
    g.strokePath (shape, PathStrokeType (6.0f));

    // tornillos
    for (auto p : { Point<float> (24, 24), Point<float> (956, 24), Point<float> (24, 636), Point<float> (956, 636) })
    {
        auto r = Rectangle<float> (p.x - 10, p.y - 10, 20, 20);
        g.setColour (Colours::white.withAlpha (.25f)); g.fillEllipse (r.translated (0, 1));
        ColourGradient cg (Colour (0xff55565c), r.getX() + 8, r.getY() + 7, Colour (0xff1b1c20), r.getRight(), r.getBottom(), true);
        g.setGradientFill (cg); g.fillEllipse (r);
        g.setColour (Colours::black);
        g.fillRoundedRectangle (p.x - 5, p.y - 1, 10, 2, 1);
        g.fillRoundedRectangle (p.x - 1, p.y - 5, 2, 10, 1);
    }

    // titulo en caja amarilla + lettering oficial "by soundtastics"
    {
        const auto f = heavy (25);
        const String t = "VOCAL CHOPPER";
        const float tw = GlyphArrangement::getStringWidth (f, t);
        auto box = Rectangle<float> (60, 14, tw + 28 + 8, 48);
        g.setColour (ui::c::ink); g.fillRoundedRectangle (box, 6);
        g.setColour (ui::c::yel); g.fillRoundedRectangle (box.reduced (4), 3);
        drawText (g, t, box.reduced (18, 4), Justification::centredLeft, f, ui::c::ink);
        auto img = lettering();
        if (img.isValid())
        {
            const float h = 24, w = h * img.getWidth() / (float) img.getHeight();
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (img, Rectangle<float> (404, 20, w, h));
        }
        g.setColour (ui::c::ink); g.fillRoundedRectangle (404, 52, 236, 3, 1.5f);
    }

    // etiquetas INPUT / OUTPUT y extremos de sus mandos
    auto labelBox = [&] (float cx, float y, const String& s)
    {
        const auto f = cond (17);
        const float tw = GlyphArrangement::getStringWidth (f, s) + 24;
        auto r = Rectangle<float> (cx - tw / 2 - 3, y, tw + 6, 31);
        g.setColour (ui::c::ink); g.fillRoundedRectangle (r, 6);
        g.setColour (ui::c::cream); g.fillRoundedRectangle (r.reduced (3), 3);
        drawText (g, s, r, Justification::centred, f, ui::c::ink);
    };
    labelBox (75, 80, "INPUT");
    labelBox (905, 80, "OUTPUT");
    drawText (g, "-24", { 20, 214, 50, 20 }, Justification::centredLeft, cond (16), ui::c::ink);
    drawText (g, "+12", { 80, 214, 50, 20 }, Justification::centredRight, cond (16), ui::c::ink);
    drawText (g, U8 ("-\xe2\x88\x9e"), { 850, 214, 50, 20 }, Justification::centredLeft, cond (16), ui::c::ink);
    drawText (g, "+6", { 910, 214, 50, 20 }, Justification::centredRight, cond (16), ui::c::ink);

    // piloto del limitador
    {
        auto led = Rectangle<float> (900, 472, 10, 10);
        g.setColour (ui::c::ink); g.fillEllipse (led.expanded (2));
        g.setColour (proc.fx.limiterReductionDb.load() < -1.5f ? ui::c::rec : Colour (0xff14181f)); g.fillEllipse (led);
        drawText (g, "LIMIT", { 842, 486, 126, 14 }, Justification::centred, cond (11), ui::c::ink);
    }

    // paneles de color
    auto pane = [&] (Rectangle<float> r, Colour col)
    {
        g.setColour (col); g.fillRoundedRectangle (r, 12);
        g.setColour (Colours::black.withAlpha (.12f)); g.drawRoundedRectangle (r.reduced (1), 11, 2);
    };
    pane ({ 150, 78, 680, 318 }, ui::c::mint);
    pane ({ 150, 404, 680, 244 }, ui::c::cyan);
    auto tag = [&] (float x, float y, const String& s)
    {
        const auto f = heavy (26);
        const float tw = GlyphArrangement::getStringWidth (f, s);
        auto r = Rectangle<float> (x, y, tw + 24, 33);
        g.setColour (ui::c::pink); g.fillRoundedRectangle (r, 5);
        drawText (g, s, r.withTrimmedLeft (12).withTrimmedBottom (1), Justification::centredLeft, f, ui::c::ink);
        return r;
    };
    auto t1 = tag (164, 90, "CHOPPER");
    g.setColour (ui::c::ink); g.fillRoundedRectangle (t1.getRight() + 12, 110, 816 - t1.getRight() - 12, 3, 1.5f);
    tag (164, 414, "MACROS");

    // marco de la pantalla
    {
        auto r = Rectangle<float> (164, 174, 652, 210);
        g.setColour (ui::c::ink); g.fillRoundedRectangle (r.expanded (3), 9);
        g.setColour (ui::c::screen); g.fillRoundedRectangle (r, 7);
    }

    // macros: nombre arriba, extremos y valor abajo, separadores
    for (int cidx = 0; cidx < 5; ++cidx)
    {
        const float cx = knobCx (cidx);
        const bool chopEdit = cidx == 1 && proc.selected >= 0;
        drawText (g, kKnobLabel[cidx], { cx - 68, 470, 136, 22 }, Justification::centred, cond (17), ui::c::ink);
        drawText (g, U8 (kEnds[cidx][0]), { cx - 59, 578, 60, 18 }, Justification::centredLeft, cond (14), ui::c::ink);
        drawText (g, U8 (kEnds[cidx][1]), { cx - 1, 578, 60, 18 }, Justification::centredRight, cond (14), ui::c::ink);
        drawText (g, knobText (cidx), { cx - 68, 600, 136, 14 }, Justification::centred, mono (10, true),
                  chopEdit ? ui::c::ink : Colours::black.withAlpha (.6f));
        if (cidx > 0)
        {
            g.setColour (ui::c::ink);
            g.fillRoundedRectangle (150 + 136.0f * cidx - 1.5f, 498, 3, 120, 1.5f);
        }
    }
}

// ------------------------------------------------------------------ estado de los botones
void VocalChopperEditor::refreshButtons()
{
    const int preset = (int) proc.pv ("preset");
    for (int i = 0; i < 6; ++i) modeBtn[(size_t) i]->setOn (i == preset);

    const int sel = proc.selected;
    if (sel >= 0)
    {
        // edicion individual: FORWARD / REVERSE se aplican solo a este chop
        const int d = proc.getChopDir (sel);
        for (int i = 0; i < 4; ++i)
        {
            auto& b = *dirBtn[(size_t) i];
            b.setEnabled (i < 2);
            b.setOn ((i == 0 && d == 1) || (i == 1 && d == 2));
            b.setTooltip (i < 2 ? U8 ("Direcci\xc3\xb3n solo de este chop (vuelve a pulsar para usar la global)") : String());
        }
    }
    else
    {
        const int d = (int) proc.pv ("direction");
        for (int i = 0; i < 4; ++i)
        {
            auto& b = *dirBtn[(size_t) i];
            b.setEnabled (true);
            b.setOn (i == d);
            b.setTooltip (U8 ("Direcci\xc3\xb3n de reproducci\xc3\xb3n"));
        }
    }
    bLatch.setOn (proc.pv ("latch") > .5f);
    bAjustes.setOn (setOv.isVisible());
    bLib.setOn (libOv.isVisible());
}

void VocalChopperEditor::pickDirection (int i)
{
    const int sel = proc.selected;
    if (sel >= 0)
    {
        if (i > 1) return;
        const int want = i == 0 ? 1 : 2;
        proc.setChopDir (sel, proc.getChopDir (sel) == want ? 0 : want);
    }
    else proc.setParam ("direction", (float) i);
    refreshButtons();
}

void VocalChopperEditor::showSettings (bool show)
{
    if (show) { libOv.setVisible (false); setOv.refresh(); }
    setOv.setVisible (show);
    refreshButtons();
}

void VocalChopperEditor::showLibrary (bool show)
{
    if (show) setOv.setVisible (false);
    libOv.setVisible (show);
    refreshButtons();
}

void VocalChopperEditor::afterLoad (const String& msg)
{
    render.reset();
    wave.tick();
    setOv.refresh();
    refreshButtons();
    panel.repaint();
    if (msg.isNotEmpty()) toast (msg);
}

void VocalChopperEditor::timerCallback()
{
    const int serial = proc.engine.uiSelectSerial.load();
    if (serial != lastSelSerial)
    {
        lastSelSerial = serial;
        proc.selected = proc.engine.uiSelected.load();
        panel.repaint (150, 404, 680, 244);
    }
    if (proc.selected >= proc.numChops()) proc.selected = -1;

    wave.tick();
    render.tick();
    lcd.repaint();
    bPlay.setOn (proc.enginePlaying.load());
    bRec.setOn (proc.isRecording());

    bool textChanged = false;
    for (int k = 0; k < 7; ++k)
    {
        auto& kn = *knobs[(size_t) k];
        const bool chop = k == 1 && proc.selected >= 0;
        const float v = kn.getValue (false) + (chop ? 100.0f : 0.0f);
        if (kn.chopMode != chop) { kn.chopMode = chop; kn.repaint(); }
        if (std::abs (v - lastVal[(size_t) k]) > 1e-6f) { lastVal[(size_t) k] = v; kn.repaint(); }
        if (k < 5)
        {
            const auto t = knobText (k);
            if (t != lastText[(size_t) k]) { lastText[(size_t) k] = t; textChanged = true; }
        }
    }
    if (textChanged) panel.repaint (150, 596, 680, 22);
    const bool lim = proc.fx.limiterReductionDb.load() < -1.5f;
    if (lim != lastLim) { lastLim = lim; panel.repaint (894, 466, 22, 22); }
    refreshButtons();
    if (setOv.isVisible()) setOv.refresh();
}

void VocalChopperEditor::mouseDown (const MouseEvent&)
{
    if (! hasKeyboardFocus (true)) grabKeyboardFocus();
}

bool VocalChopperEditor::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::spaceKey) { proc.togglePlay(); return true; }
    if (k == KeyPress::escapeKey)
    {
        proc.selected = -1;
        showSettings (false);
        showLibrary (false);
        return true;
    }
    const juce_wchar ch = CharacterFunctions::toLowerCase (k.getTextCharacter());
    const int i = keyMap.indexOfChar (ch);
    if (i >= 0)
    {
        if (! heldKeys.containsChar (ch)) { heldKeys += String::charToString (ch); proc.pressChop (i); refreshButtons(); }
        return true;
    }
    return false;
}

bool VocalChopperEditor::keyStateChanged (bool)
{
    for (int j = heldKeys.length() - 1; j >= 0; --j)
    {
        const juce_wchar ch = heldKeys[j];
        if (! KeyPress::isKeyCurrentlyDown ((int) CharacterFunctions::toUpperCase (ch)) && ! KeyPress::isKeyCurrentlyDown ((int) ch))
        {
            proc.releaseChop (keyMap.indexOfChar (ch));
            heldKeys = heldKeys.removeCharacters (String::charToString (ch));
        }
    }
    return false;
}

bool VocalChopperEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg")) return true;
    return false;
}

void VocalChopperEditor::filesDropped (const StringArray& files, int, int)
{
    wave.dragHint = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg"))
        {
            String m;
            proc.loadFile (File (f), m);
            afterLoad (m);
            return;
        }
}
