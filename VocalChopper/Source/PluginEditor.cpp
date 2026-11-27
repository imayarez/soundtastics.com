#include "PluginEditor.h"

using namespace juce;
using namespace vc;

namespace
{
const char* kSvgPlay  = R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16"><path d="M4 2.5v11l9-5.5z" fill="#ffffff"/></svg>)";
const char* kSvgStop  = R"(<svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 14 14"><rect x="1.5" y="1.5" width="11" height="11" rx="2" fill="#ffffff"/></svg>)";
const char* kSvgRec   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16"><circle cx="8" cy="8" r="5.5" fill="none" stroke="#ff8a98" stroke-width="2.2"/></svg>)";
const char* kSvgLib   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2" stroke-linecap="round"><rect x="3" y="4" width="4" height="16" rx="1"/><rect x="9" y="4" width="4" height="16" rx="1"/><path d="M15.5 5.2l3.8-1 4 15.4-3.8 1z"/></svg>)";
const char* kSvgImp   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4v11M7 10l5 5 5-5M4 19h16"/></svg>)";
const char* kSvgSet   = R"(<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M4 7h10M18 7h2M4 17h4M12 17h8"/><circle cx="16" cy="7" r="2"/><circle cx="10" cy="17" r="2"/></svg>)";
const char* kSvgLatch = R"(<svg xmlns="http://www.w3.org/2000/svg" width="22" height="14" viewBox="0 0 26 14" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M13 7c-2.5-3-4-5-7-5a5 5 0 000 10c3 0 4.5-2 7-5s4-5 7-5a5 5 0 010 10c-3 0-4.5-2-7-5z"/></svg>)";
const char* kSvgLogo  = R"(<svg xmlns="http://www.w3.org/2000/svg" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2" stroke-linecap="round"><path d="M4 12h2M8 8v8M12 5v14M16 9v6M20 12h0"/></svg>)";

String knobSvg (const char* body)
{
    return String (R"(<svg xmlns="http://www.w3.org/2000/svg" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">)")
           + body + "</svg>";
}
const char* kKnobIco[5] = {
    R"(<path d="M3 12h18M7 8l-4 4 4 4M17 8l4 4-4 4"/>)",
    R"(<path d="M9 18V6l10-2v12"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="16.5" cy="16" r="2.5"/>)",
    R"(<rect x="4" y="4" width="16" height="16" rx="2"/><rect x="9" y="9" width="6" height="6" rx="1"/>)",
    R"(<path d="M3 13l3-6 3 9 3-11 3 13 3-8 3 3"/>)",
    R"(<circle cx="12" cy="12" r="3"/><circle cx="12" cy="12" r="7" stroke-dasharray="2 3"/><circle cx="12" cy="12" r="10.5" stroke-dasharray="1 4"/>)" };
const char* kKnobId[7] = { "stretch", "pitch", "size", "artifacts", "blur", "in", "out" };
const char* kKnobLabel[5] = { "STRETCH", "PITCH", "SIZE", "ARTIFACTS", "BLUR" };
const float kKnobDef[7] = { .5f, .5f, .4f, 0.0f, .15f, .5f, .75f };

std::vector<std::unique_ptr<Drawable>>& knobIcons()
{
    static std::vector<std::unique_ptr<Drawable>> v;
    if (v.empty()) for (auto* b : kKnobIco) v.push_back (ui::icon (knobSvg (b)));
    return v;
}
std::unique_ptr<Drawable>& logoIcon() { static auto d = ui::icon (kSvgLogo); return d; }

const String dot = String::fromUTF8 (" \xc2\xb7 ");
}

const String VocalChopperEditor::keyMap = "awsedftgyhujkolp;";

VocalChopperEditor::VocalChopperEditor (VocalChopperProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      bPlay (kSvgPlay), bStop (kSvgStop), bRec (kSvgRec), bLib (kSvgLib, ui::HwButton::round), bImp (kSvgImp),
      lcd (p), render (p), wave (p), setOv (p), libOv (p), sel1 (kSvgSet), sel2 (kSvgLatch)
{
    setLookAndFeel (&look);
    addAndMakeVisible (panel);
    panel.painter = [this] (Graphics& g) { paintPanel (g); };

    for (auto* b : { &bPlay, &bStop, &bRec, &bLib, &bImp }) panel.addAndMakeVisible (*b);
    bPlay.hasLed = bStop.hasLed = bRec.hasLed = true;
    bRec.ledRed = true;
    bPlay.setBounds (36, 86, 58, 46);
    bStop.setBounds (102, 86, 58, 46);
    bRec.setBounds (168, 86, 58, 46);
    bLib.setBounds (906, 22, 44, 48);
    bImp.setBounds (26, 599, 44, 48);
    bPlay.setTooltip ("Play (espacio). Dentro del DAW tambien arranca solo con el transporte");
    bStop.setTooltip ("Stop");
    bRec.setTooltip (U8 ("Grabar la entrada de audio del canal (o el micr\xc3\xb3" "fono en la versi\xc3\xb3n Standalone)"));
    bLib.setTooltip (U8 ("Librer\xc3\xad" "a de f\xc3\xa1" "brica"));
    bImp.setTooltip ("Importar audio (.wav .mp3 .aiff .flac .ogg)");

    bPlay.onClick = [this] { proc.togglePlay(); };
    bStop.onClick = [this] { proc.stopAll(); };
    bRec.onClick = [this] { String m; proc.toggleRecord (m); if (m.isNotEmpty()) afterLoad (m); };
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

    panel.addAndMakeVisible (lcd);
    lcd.setBounds (380, 82, 220, 40);
    lcd.setTooltip ("Tempo. Dentro del DAW sigue al proyecto; fuera, arrastra arriba/abajo para cambiarlo (doble clic = tempo del loop)");
    panel.addAndMakeVisible (render);
    render.setBounds (686, 80, 200, 52);
    render.toast = [this] (const String& m) { toast (m); };

    panel.addAndMakeVisible (wave);
    wave.setBounds (28, 142, 924, 252);
    wave.toast = [this] (const String& m) { toast (m); };
    wave.onSelectionChanged = [this] { refreshSelectors(); for (auto& k : knobs) k->repaint(); panel.repaint (0, 408, 980, 170); };
    wave.addChildComponent (setOv);
    wave.addChildComponent (libOv);
    setOv.setBounds (0, 0, 924, 252);
    libOv.setBounds (0, 0, 924, 252);
    setOv.onClose = [this] { showSettings (false); };
    setOv.toast = [this] (const String& m) { toast (m); };
    libOv.onClose = [this] { showLibrary (false); };
    libOv.close.onClick = [this] { showLibrary (false); };
    libOv.onPick = [this] (int i)
    {
        String m;
        proc.loadPatch (i, m);
        showLibrary (false);
        afterLoad (m);
    };

    setupKnobs();

    for (auto* s : { &sel1, &sel2 }) panel.addAndMakeVisible (*s);
    sel1.setBounds (150, 599, 276, 44);
    sel2.setBounds (446, 599, 276, 44);
    sel1.onIcon = [this] { showSettings (! setOv.isVisible()); };
    sel1.onName = [this] { openMenu (1); };
    sel1.onPrev = [this] { stepSel (1, -1); };
    sel1.onNext = [this] { stepSel (1, 1); };
    sel2.onIcon = [this]
    {
        const bool on = proc.pv ("latch") < .5f;
        proc.setParam ("latch", on ? 1.0f : 0.0f);
        toast (on ? U8 ("LATCH ON \xc2\xb7 el chop se queda en bucle") : String ("LATCH OFF"));
        refreshSelectors();
    };
    sel2.onName = [this] { openMenu (2); };
    sel2.onPrev = [this] { stepSel (2, -1); };
    sel2.onNext = [this] { stepSel (2, 1); };
    panel.addChildComponent (menu);

    panel.addAndMakeVisible (toastC);
    toastC.setBounds (0, 340, 980, 32);

    addMouseListener (this, true);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (588, 396, 1960, 1320);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (980.0 / 660.0);
    setSize (980, 660);

    refreshSelectors();
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
        const bool small = k >= 5;
        auto kn = std::make_unique<ui::Knob> (small);
        auto* id = kKnobId[k];
        const bool isPitch = k == 1;
        kn->bipolar = k == 0 || k == 1 || k == 5;
        kn->getValue = [this, id, isPitch] (bool alt) -> float
        {
            if (isPitch && alt) return proc.pv ("formant");
            if (isPitch && proc.selected >= 0) return (proc.getChopPitch (proc.selected) + 12) / 24.0f;
            return proc.pv (id);
        };
        kn->setValue = [this, id, isPitch, k] (float v, bool alt)
        {
            if (isPitch && alt) { if (auto* p = proc.apvts.getParameter ("formant")) p->setValueNotifyingHost (v); }
            else if (isPitch && proc.selected >= 0) proc.setChopPitch (proc.selected, (int) std::lround (v * 24 - 12));
            else if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
            if (k >= 5) toast ((k == 5 ? "IN " : "OUT ") + knobText (k));
        };
        kn->gesture = [this, id, isPitch] (bool begin, bool alt)
        {
            if (isPitch && ! alt && proc.selected >= 0) return;
            if (auto* p = proc.apvts.getParameter (isPitch && alt ? "formant" : id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); }
        };
        kn->onReset = [this, id, isPitch, k]
        {
            if (isPitch && proc.selected >= 0) proc.setChopPitch (proc.selected, 0);
            else proc.setParam (id, kKnobDef[k]);
        };
        if (isPitch)
            kn->getArc2 = [this] { const float f = proc.pv ("formant"); return std::abs (f - .5f) > .01f ? f : -1.0f; };
        kn->setTooltip (isPitch ? U8 ("Arrastra: semitonos \xc2\xb7 Alt/Shift + arrastra: formantes \xc2\xb7 doble clic: reset")
                                : U8 ("Arrastra arriba/abajo \xc2\xb7 Ctrl = fino \xc2\xb7 doble clic: reset"));
        panel.addAndMakeVisible (*kn);
        if (small) kn->setBounds (k == 5 ? 806 : 902, 592, 56, 56);
        else kn->setBounds (k * 196 + 50, 434, 96, 96);
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

void VocalChopperEditor::paint (Graphics& g) { g.fillAll (Colour (0xff070b22)); }

void VocalChopperEditor::resized()
{
    const float s = getWidth() / 980.0f;
    panel.setBounds (0, 0, 980, 660);
    panel.setTransform (AffineTransform::scale (s));
}

void VocalChopperEditor::paintPanel (Graphics& g)
{
    const auto B = Rectangle<float> (0, 0, 980, 660);
    Path shape; shape.addRoundedRectangle (B, 22);
    g.reduceClipRegion (shape);
    ColourGradient bg (Colour (0xff2d54f2), 0, 0, Colour (0xff1a37c0), 0, 660, false);
    bg.addColour (.48, Colour (0xff2246dc));
    g.setGradientFill (bg);
    g.fillPath (shape);
    g.setColour (Colours::white.withAlpha (.03f));
    for (int x = 0; x < 980; x += 3) g.fillRect ((float) x, 0.0f, 1.0f, 660.0f);
    g.setColour (Colours::white.withAlpha (.4f));
    g.drawHorizontalLine (0, 22, 958);

    // tornillos
    for (auto p : { Point<float> (9, 9), Point<float> (960, 9), Point<float> (9, 563), Point<float> (960, 563) })
    {
        auto r = Rectangle<float> (p.x, p.y, 11, 11);
        g.setColour (Colours::black.withAlpha (.5f)); g.fillEllipse (r.translated (0, 1));
        ColourGradient cg (Colour (0xffe9edf6), r.getX() + 3.8f, r.getY() + 3.3f, Colour (0xff4a5170), r.getRight(), r.getBottom(), true);
        cg.addColour (.7, Colour (0xff8790a8));
        g.setGradientFill (cg); g.fillEllipse (r);
        Path sl; sl.addRectangle (-3.5f, -.5f, 7, 1);
        g.setColour (Colour (0xff3a4060));
        g.fillPath (sl, AffineTransform::rotation (35 * MathConstants<float>::pi / 180).translated (r.getCentreX(), r.getCentreY()));
    }

    // hueco del logo oficial de Soundtastics
    {
        auto r = Rectangle<float> (30, 22, 42, 42);
        g.setColour (Colours::white.withAlpha (.1f)); g.fillRoundedRectangle (r, 12);
        g.setColour (Colours::white.withAlpha (.25f)); g.drawHorizontalLine (23, 36, 66);
        if (auto& d = logoIcon()) d->drawAt (g, 40, 32, 1.0f);
    }

    // titulo
    {
        const auto f1 = ui::font (19, false, .42f), f2 = ui::font (19, true, .42f);
        const String a = "VOCAL ", b = "CHOPPER";
        const float w1 = GlyphArrangement::getStringWidth (f1, a), w2 = GlyphArrangement::getStringWidth (f2, b);
        const float x = (980 - (w1 + w2)) / 2 + 4;
        ui::drawText (g, a, { x, 28, w1 + 2, 26 }, Justification::centredLeft, f1, Colours::white);
        ui::drawText (g, b, { x + w1, 28, w2 + 4, 26 }, Justification::centredLeft, f2, Colours::white);
        ui::drawText (g, "CHOP" + dot + "ROLL" + dot + "STUTTER", { 0, 54, 980, 14 }, Justification::centred, ui::font (9.5f, false, .3f),
                      Colours::white.withAlpha (.55f));
    }

    // pozo del transporte
    {
        auto r = Rectangle<float> (30, 80, 202, 54);
        g.setColour (Colours::black.withAlpha (.18f)); g.fillRoundedRectangle (r, 14);
        ui::innerShadow (g, r, 14, 3, .45f);
        g.setColour (Colours::white.withAlpha (.15f)); g.drawHorizontalLine (134, 44, 218);
    }

    // columna de cada macro: icono, nombre y valor
    for (int c = 0; c < 5; ++c)
    {
        const float x0 = c * 196.0f;
        if (c > 0)
        {
            ColourGradient sg (Colours::transparentBlack, x0, 422, Colours::transparentBlack, x0, 558, false);
            sg.addColour (.5, Colours::black.withAlpha (.22f));
            g.setGradientFill (sg); g.fillRect (x0, 422.0f, 1.0f, 136.0f);
            g.setColour (Colours::white.withAlpha (.08f)); g.fillRect (x0 + 1, 422.0f, 1.0f, 136.0f);
        }
        if (auto& d = knobIcons()[(size_t) c]) d->drawAt (g, x0 + 98 - 11, 408, .85f);
        const bool chopEdit = c == 1 && proc.selected >= 0;
        ui::drawText (g, kKnobLabel[c], { x0, 538, 196, 14 }, Justification::centred, ui::font (11, true, .16f),
                      chopEdit ? vc::col::acc : Colours::white);
        ui::drawText (g, knobText (c), { x0, 554, 196, 13 }, Justification::centred, ui::mono (10), vc::col::mut);
    }

    // barra inferior
    {
        g.setGradientFill (ColourGradient (Colour (0xff0c1756), 0, 582, Colour (0xff081040), 0, 660, false));
        g.fillRect (0.0f, 582.0f, 980.0f, 78.0f);
        g.setColour (Colours::white.withAlpha (.12f)); g.drawHorizontalLine (582, 0, 980);
        g.setColour (Colours::black.withAlpha (.3f)); g.fillRect (0.0f, 583.0f, 980.0f, 3.0f);
        const auto f = ui::font (9.5f, true, .14f);
        ui::drawText (g, "IN", { 740, 592, 58, 56 }, Justification::centredRight, f, Colours::white.withAlpha (.88f));
        ui::drawText (g, "OUT", { 836, 592, 58, 56 }, Justification::centredRight, f, Colours::white.withAlpha (.88f));
        auto led = Rectangle<float> (958, 590, 6, 6);
        if (proc.fx.limiterReductionDb.load() < -1.5f)
        {
            g.setColour (vc::col::red.withAlpha (.35f)); g.fillEllipse (led.expanded (3));
            g.setColour (vc::col::red); g.fillEllipse (led);
        }
        else { g.setColour (Colours::white.withAlpha (.12f)); g.fillEllipse (led); }
    }
}

void VocalChopperEditor::refreshSelectors()
{
    const int preset = (int) proc.pv ("preset");
    const String t1 = presets()[(size_t) clampv (preset, 0, 5)].name;
    String t2;
    if (proc.selected >= 0)
    {
        const int d = proc.getChopDir (proc.selected);
        t2 = String ("CHOP: ") + (d == 2 ? "REVERSE" : d == 1 ? "FORWARD" : "GLOBAL");
    }
    else t2 = dirNames()[(size_t) clampv ((int) proc.pv ("direction"), 0, 3)];
    const bool i1 = setOv.isVisible(), i2 = proc.pv ("latch") > .5f;
    if (t1 != sel1.text || i1 != sel1.iconOn) { sel1.text = t1; sel1.iconOn = i1; sel1.repaint(); }
    if (t2 != sel2.text || i2 != sel2.iconOn) { sel2.text = t2; sel2.iconOn = i2; sel2.repaint(); }
}

void VocalChopperEditor::openMenu (int which)
{
    if (menu.isVisible() && menuWhich == which) { closeMenu(); return; }
    menuWhich = which;
    menu.items.clear();
    if (which == 1)
    {
        const int cur = (int) proc.pv ("preset");
        for (int i = 0; i < 6; ++i) menu.items.push_back ({ presets()[(size_t) i].name, presets()[(size_t) i].info, i == cur });
        menu.onPick = [this] (int i) { proc.setParam ("preset", (float) i); closeMenu(); refreshSelectors(); };
    }
    else if (proc.selected >= 0)
    {
        const int d = proc.getChopDir (proc.selected);
        const char* names[3] = { "GLOBAL", "FORWARD", "REVERSE" };
        for (int i = 0; i < 3; ++i) menu.items.push_back ({ names[i], {}, i == d });
        menu.onPick = [this] (int i) { proc.setChopDir (proc.selected, i); closeMenu(); refreshSelectors(); };
    }
    else
    {
        const int d = (int) proc.pv ("direction");
        for (int i = 0; i < 4; ++i) menu.items.push_back ({ dirNames()[(size_t) i], {}, i == d });
        menu.onPick = [this] (int i) { proc.setParam ("direction", (float) i); closeMenu(); refreshSelectors(); };
    }
    auto& s = which == 1 ? sel1 : sel2;
    const int h = menu.idealHeight(), w = which == 1 ? 240 : 200;
    menu.setBounds (s.getX() + 44, s.getY() - 8 - h, w, h);
    menu.setVisible (true);
    menu.toFront (false);
}

void VocalChopperEditor::closeMenu() { menu.setVisible (false); }

void VocalChopperEditor::stepSel (int which, int d)
{
    if (which == 1)
    {
        const int p = modi ((int) proc.pv ("preset") + d, 6);
        proc.setParam ("preset", (float) p);
        toast (String (presets()[(size_t) p].name) + dot + presets()[(size_t) p].info);
    }
    else if (proc.selected >= 0) proc.setChopDir (proc.selected, modi (proc.getChopDir (proc.selected) + d, 3));
    else proc.setParam ("direction", (float) modi ((int) proc.pv ("direction") + d, 4));
    refreshSelectors();
}

void VocalChopperEditor::showSettings (bool show)
{
    if (show) { libOv.setVisible (false); setOv.refresh(); }
    setOv.setVisible (show);
    refreshSelectors();
}

void VocalChopperEditor::showLibrary (bool show)
{
    if (show) setOv.setVisible (false);
    libOv.setVisible (show);
    refreshSelectors();
}

void VocalChopperEditor::afterLoad (const String& msg)
{
    render.reset();
    wave.tick();
    setOv.refresh();
    refreshSelectors();
    panel.repaint();
    if (msg.isNotEmpty()) toast (msg);
}

void VocalChopperEditor::timerCallback()
{
    // seleccion de chop hecha por MIDI
    const int serial = proc.engine.uiSelectSerial.load();
    if (serial != lastSelSerial)
    {
        lastSelSerial = serial;
        proc.selected = proc.engine.uiSelected.load();
        refreshSelectors();
    }
    if (proc.selected >= proc.numChops()) proc.selected = -1;

    wave.tick();
    render.tick();
    lcd.repaint();

    auto setLed = [] (ui::HwButton& b, bool on) { if (b.led != on) { b.led = on; b.repaint(); } };
    setLed (bPlay, proc.enginePlaying.load());
    setLed (bRec, proc.isRecording());

    bool textChanged = false;
    for (int k = 0; k < 7; ++k)
    {
        auto& kn = *knobs[(size_t) k];
        const float v = kn.getValue (false) + (k == 1 ? proc.pv ("formant") * 10 + (proc.selected >= 0 ? 100.0f : 0.0f) : 0.0f);
        kn.chopMode = k == 1 && proc.selected >= 0;
        if (v != lastVal[(size_t) k]) { lastVal[(size_t) k] = v; kn.repaint(); }
        if (k < 5)
        {
            const auto t = knobText (k);
            if (t != lastText[(size_t) k]) { lastText[(size_t) k] = t; textChanged = true; }
        }
    }
    if (textChanged) panel.repaint (0, 530, 980, 40);
    const bool lim = proc.fx.limiterReductionDb.load() < -1.5f;
    if (lim != lastLim) { lastLim = lim; panel.repaint (950, 584, 20, 16); }
    refreshSelectors();
    if (setOv.isVisible()) setOv.refresh();
}

void VocalChopperEditor::mouseDown (const MouseEvent& e)
{
    if (menu.isVisible())
    {
        auto* c = e.eventComponent;
        if (c != &menu && c != &sel1 && c != &sel2) closeMenu();
    }
    if (! hasKeyboardFocus (true)) grabKeyboardFocus();
}

bool VocalChopperEditor::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::spaceKey) { proc.togglePlay(); return true; }
    if (k == KeyPress::escapeKey)
    {
        proc.selected = -1;
        closeMenu();
        showSettings (false);
        showLibrary (false);
        return true;
    }
    const juce_wchar ch = CharacterFunctions::toLowerCase (k.getTextCharacter());
    const int i = keyMap.indexOfChar (ch);
    if (i >= 0)
    {
        if (! heldKeys.containsChar (ch)) { heldKeys += String::charToString (ch); proc.pressChop (i); refreshSelectors(); }
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
