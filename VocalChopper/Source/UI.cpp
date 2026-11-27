#include "UI.h"

using namespace juce;
using namespace vc;

namespace ui
{
Font font (float px, bool bold, float kern)
{
    return Font (FontOptions().withPointHeight (px).withStyle (bold ? "Bold" : "Regular").withKerningFactor (kern));
}

Font mono (float px, bool bold)
{
    return Font (FontOptions().withName (Font::getDefaultMonospacedFontName()).withPointHeight (px).withStyle (bold ? "Bold" : "Regular"));
}

std::unique_ptr<Drawable> icon (const String& svg)
{
    if (auto xml = parseXML (svg)) return Drawable::createFromSVG (*xml);
    return nullptr;
}

void drawText (Graphics& g, const String& s, Rectangle<float> r, Justification j, const Font& f, Colour c)
{
    g.setColour (c);
    g.setFont (f);
    g.drawText (s, r, j, false);
}

void innerShadow (Graphics& g, Rectangle<float> r, float radius, float depth, float alpha)
{
    Graphics::ScopedSaveState ss (g);
    Path clip; clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);
    Path outer; outer.addRectangle (r.expanded (20));
    outer.addRoundedRectangle (r.translated (0, depth * .5f), radius);
    outer.setUsingNonZeroWinding (false);
    DropShadow (Colours::black.withAlpha (alpha), (int) (depth * 2), { 0, (int) depth }).drawForPath (g, outer);
}

static void hwBody (Graphics& g, Rectangle<float> r, float rad, Colour top, Colour bot, Colour lip, bool down, float lipH = 3)
{
    if (! down)
    {
        Path sh; sh.addRoundedRectangle (r.translated (0, 4), rad);
        DropShadow (Colours::black.withAlpha (.35f), 14, { 0, 4 }).drawForPath (g, sh);
    }
    const float off = down ? 2.0f : 0.0f;
    const float lh = down ? 1.0f : lipH;
    g.setColour (lip);
    g.fillRoundedRectangle (r.translated (0, off + lh), rad);
    auto b = r.translated (0, off);
    g.setGradientFill (ColourGradient (top, b.getX(), b.getY(), bot, b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, rad);
    g.setColour (Colours::white.withAlpha (down ? .25f : .35f));
    g.drawLine (b.getX() + rad * .6f, b.getY() + .5f, b.getRight() - rad * .6f, b.getY() + .5f, 1.0f);
}

// ===================================================================== HwButton
HwButton::HwButton (const String& svg, Style s) : ico (icon (svg)), style (s)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void HwButton::mouseUp (const MouseEvent& e)
{
    down = false;
    repaint();
    if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
}

void HwButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedBottom (4);
    const float rad = style == round ? r.getHeight() / 2 : 10.0f;
    hwBody (g, r, rad, Colour (0xff3a5ff5), Colour (0xff1d3bc9), Colour (0xff10257f), down);
    auto b = r.translated (0, down ? 2.0f : 0.0f);
    if (hasLed)
    {
        auto lr = Rectangle<float> (b.getRight() - 7 - 5, b.getY() + 6, 5, 5);
        if (led)
        {
            const auto c = ledRed ? vc::col::red : vc::col::acc;
            g.setColour (c.withAlpha (.35f)); g.fillEllipse (lr.expanded (3));
            g.setColour (c); g.fillEllipse (lr);
        }
        else { g.setColour (Colours::black.withAlpha (.35f)); g.fillEllipse (lr); }
    }
    if (ico)
    {
        auto ib = ico->getDrawableBounds();
        ico->drawAt (g, b.getCentreX() - ib.getWidth() / 2 - ib.getX(), b.getCentreY() - ib.getHeight() / 2 - ib.getY(), 1.0f);
    }
}

// ===================================================================== Knob
void Knob::paint (Graphics& g)
{
    const float S = (float) getWidth(), k = S / 100.0f;
    const float cx = S / 2, cy = S / 2;
    const float v = getValue ? getValue (false) : 0.0f;
    const float a = -135.0f + 270.0f * v;
    const float r = (isSmall ? 40.0f : 45.0f) * k;
    auto rad = [] (float d) { return d * MathConstants<float>::pi / 180.0f; };
    auto arc = [&] (float a0, float a1, float rr)
    {
        if (std::abs (a1 - a0) < .5f) a1 = a0 + .5f * (a1 >= a0 ? 1.0f : -1.0f);
        Path p; p.addCentredArc (cx, cy, rr, rr, 0, rad (std::min (a0, a1)), rad (std::max (a0, a1)), true);
        return p;
    };
    const PathStrokeType st (4.0f * k, PathStrokeType::curved, PathStrokeType::rounded);
    g.setColour (Colours::black.withAlpha (.28f));
    g.strokePath (arc (-135, 135, r), st);
    g.setColour (chopMode ? vc::col::acc : Colours::white);
    g.strokePath (bipolar ? arc (0, a, r) : arc (-135, a, r), st);
    if (getArc2)
    {
        const float f = getArc2();
        if (f >= 0)
        {
            g.setColour (vc::col::cy);
            g.strokePath (arc (0, -135.0f + 270.0f * f, 38.0f * k), PathStrokeType (2.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    // tapa plateada
    const float off = isSmall ? 10.0f * S / 56.0f : 16.0f * S / 96.0f;
    const float cs = S - off * 2;
    auto cap = Rectangle<float> (off, off, cs, cs);
    {
        Path p; p.addEllipse (cap);
        DropShadow (Colour (5, 15, 70).withAlpha (.55f), (int) (cs * .22f), { 0, (int) (cs * .12f) }).drawForPath (g, p);
    }
    g.setColour (Colour (0xff7d86a0));
    g.fillEllipse (cap);
    g.setColour (Colour (0xffd3d9e6));
    {
        Path kn;
        for (int i = 0; i < 90; i += 2)
            kn.addPieSegment (cap, rad ((float) i * 4.0f), rad ((float) i * 4.0f + 4.0f), 0.0f);
        g.fillPath (kn);
    }
    const float ins = isSmall ? 4.0f * S / 56.0f : 6.0f * S / 96.0f;
    auto inner = cap.reduced (ins);
    {
        Path p; p.addEllipse (inner);
        DropShadow (Colours::black.withAlpha (.35f), 2, { 0, 1 }).drawForPath (g, p);
        ColourGradient cg (Colours::white, inner.getX() + inner.getWidth() * .38f, inner.getY() + inner.getHeight() * .30f,
                           Colour (0xffa7afc4), inner.getX() + inner.getWidth() * 1.05f, inner.getBottom() + inner.getHeight() * .1f, true);
        cg.addColour (.40, Colour (0xffe4e8f1));
        g.setGradientFill (cg);
        g.fillEllipse (inner);
        g.setColour (Colours::white.withAlpha (.8f));
        g.drawEllipse (inner.reduced (.5f), .8f);
    }
    // indicador
    {
        const float w = isSmall ? 2.0f : 3.0f, h = (isSmall ? 9.0f : 15.0f) * S / (isSmall ? 56.0f : 96.0f);
        const float top = (isSmall ? 3.0f : 6.0f) * S / (isSmall ? 56.0f : 96.0f);
        Path ind; ind.addRoundedRectangle (cx - w / 2, cap.getY() + top, w, h, w * .6f);
        g.setColour (vc::col::navy);
        g.fillPath (ind, AffineTransform::rotation (rad (a), cx, cy));
    }
}

void Knob::mouseDown (const MouseEvent& e)
{
    alt = e.mods.isAltDown() || e.mods.isShiftDown();
    altHeld = alt;
    y0 = (float) e.position.y;
    v0 = getValue ? getValue (alt) : 0.0f;
    if (gesture) gesture (true, alt);
    repaint();
}

void Knob::mouseDrag (const MouseEvent& e)
{
    const float f = (e.mods.isCtrlDown() || e.mods.isCommandDown()) ? 900.0f : 180.0f;
    if (setValue) setValue (jlimit (0.0f, 1.0f, v0 + (y0 - (float) e.position.y) / f), alt);
    repaint();
}

void Knob::mouseUp (const MouseEvent&)
{
    if (gesture) gesture (false, alt);
    altHeld = false;
    repaint();
}

void Knob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) < 1e-4f) return;
    const bool a = e.mods.isAltDown();
    if (gesture) gesture (true, a);
    if (setValue && getValue) setValue (jlimit (0.0f, 1.0f, getValue (a) + (w.deltaY > 0 ? .02f : -.02f)), a);
    if (gesture) gesture (false, a);
    repaint();
}

// ===================================================================== MenuList
int MenuList::idealHeight() const
{
    int h = 10;
    for (size_t i = 0; i < items.size(); ++i) h += rowH ((int) i);
    return h;
}

int MenuList::rowAt (int y) const
{
    int t = 5;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (y >= t && y < t + rowH ((int) i)) return (int) i;
        t += rowH ((int) i);
    }
    return -1;
}

void MenuList::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b1450));
    g.fillRoundedRectangle (b, 10);
    float t = 5;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const float h = (float) rowH ((int) i);
        auto r = Rectangle<float> (5, t, b.getWidth() - 10, h);
        if ((int) i == hover || items[i].on) { g.setColour (vc::col::sel); g.fillRoundedRectangle (r, 7); }
        drawText (g, items[i].name, r.withTrimmedLeft (10).withHeight (31), Justification::centredLeft, font (11, true, .1f), Colours::white);
        if (items[i].info.isNotEmpty())
            drawText (g, items[i].info, r.withTrimmedLeft (10).withTrimmedTop (22).withHeight (14), Justification::centredLeft,
                      font (9.5f), vc::col::mut);
        t += h;
    }
}

// ===================================================================== Selector
static const char* kChev = R"(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10" viewBox="0 0 10 10"><path d="M2 3.5l3 3 3-3" stroke="#ffffff" stroke-width="1.6" fill="none"/></svg>)";

Selector::Selector (const String& svg)
    : ico (icon (svg.replace ("currentColor", "#ffffff"))), icoOn (icon (svg.replace ("currentColor", "#ffd23f"))), chev (icon (kChev))
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Selector::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff050a2c));
    g.fillRoundedRectangle (b, 10);
    innerShadow (g, b, 10, 4, .7f);
    g.setColour (Colours::white.withAlpha (.1f));
    g.fillRect (44.0f, 0.0f, 1.0f, b.getHeight());
    g.fillRect (b.getWidth() - 72, 0.0f, 1.0f, b.getHeight());
    g.fillRect (b.getWidth() - 36, 0.0f, 1.0f, b.getHeight());
    if (auto* d = iconOn ? icoOn.get() : ico.get())
    {
        auto ib = d->getDrawableBounds();
        d->drawAt (g, 22 - ib.getWidth() / 2 - ib.getX(), b.getCentreY() - ib.getHeight() / 2 - ib.getY(), 1.0f);
    }
    if (chev) chev->drawAt (g, 56, b.getCentreY() - 5, 1.0f);
    drawText (g, text, Rectangle<float> (74, 0, b.getWidth() - 72 - 80, b.getHeight()), Justification::centredLeft,
              font (11, true, .14f), Colours::white);
    drawText (g, U8 ("\xe2\x80\xb9"), Rectangle<float> (b.getWidth() - 72, 0, 36, b.getHeight()), Justification::centred, font (15), Colours::white);
    drawText (g, U8 ("\xe2\x80\xba"), Rectangle<float> (b.getWidth() - 36, 0, 36, b.getHeight()), Justification::centred, font (15), Colours::white);
}

void Selector::mouseDown (const MouseEvent& e)
{
    const int x = e.x, w = getWidth();
    if (x < 44) { if (onIcon) onIcon(); }
    else if (x >= w - 36) { if (onNext) onNext(); }
    else if (x >= w - 72) { if (onPrev) onPrev(); }
    else if (onName) onName();
}

// ===================================================================== Seg / Close
void Seg::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colours::white.withAlpha (.07f));
    g.fillRoundedRectangle (b, 8);
    const int n = labels.size();
    if (n == 0) return;
    const float w = (b.getWidth() - 6 - 3 * (n - 1)) / n;
    for (int i = 0; i < n; ++i)
    {
        auto r = Rectangle<float> (3 + i * (w + 3), 3, w, b.getHeight() - 6);
        if (i == selectedIndex) { g.setColour (vc::col::sel); g.fillRoundedRectangle (r, 6); }
        drawText (g, labels[i], r, Justification::centred, font (10.5f, true, .08f), i == selectedIndex ? Colours::white : vc::col::mut);
    }
}

void Seg::mouseDown (const MouseEvent& e)
{
    const int n = labels.size();
    if (n == 0) return;
    const int i = jlimit (0, n - 1, (int) ((float) e.x / getWidth() * n));
    selectedIndex = i;
    repaint();
    if (onPick) onPick (i);
}

void CloseButton::paint (Graphics& g)
{
    g.setColour (Colours::white.withAlpha (.1f));
    g.fillEllipse (getLocalBounds().toFloat());
    drawText (g, U8 ("\xc3\x97"), getLocalBounds().toFloat(), Justification::centred, font (16), Colours::white);
}

// ===================================================================== Look
Look::Look()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff0b1450));
    setColour (PopupMenu::highlightedBackgroundColourId, vc::col::sel);
    setColour (PopupMenu::textColourId, Colours::white);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff0b1450));
    setColour (TooltipWindow::textColourId, Colours::white);
}

void Look::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider&)
{
    const float cy = y + h / 2.0f;
    g.setColour (Colours::white.withAlpha (.16f));
    g.fillRoundedRectangle ((float) x, cy - 2, (float) w, 4, 2);
    auto t = Rectangle<float> (pos - 8, cy - 8, 16, 16);
    Path p; p.addEllipse (t);
    DropShadow (Colours::black.withAlpha (.6f), 3, { 0, 1 }).drawForPath (g, p);
    g.setGradientFill (ColourGradient (Colours::white, t.getX() + 5, t.getY() + 4, Colour (0xffb9c1d6), t.getRight(), t.getBottom(), true));
    g.fillEllipse (t);
}

void Look::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    g.setColour (Colours::white.withAlpha (.08f));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6);
    Path a; a.startNewSubPath (w - 14.0f, h / 2.0f - 2); a.lineTo (w - 10.0f, h / 2.0f + 2); a.lineTo (w - 6.0f, h / 2.0f - 2);
    g.setColour (Colours::white.withAlpha (.7f));
    g.strokePath (a, PathStrokeType (1.4f));
}

void Look::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (2, 0, box.getWidth() - 16, box.getHeight());
    label.setFont (font (11));
}

void Look::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Colour (0xff0b1450));
    ignoreUnused (w, h);
}

void Look::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.fillAll (Colour (0xff0b1450));
    drawText (g, text, Rectangle<float> (0, 0, (float) w, (float) h).reduced (6, 2), Justification::centredLeft, font (11), Colours::white);
}

// ===================================================================== SettingsOverlay
SettingsOverlay::SettingsOverlay (VocalChopperProcessor& p) : proc (p)
{
    addAndMakeVisible (close);
    close.onClick = [this] { if (onClose) onClose(); };
    det.labels = { "AUTO", "VOCAL", "GOLPE" };
    key.labels = { "OFF", "ON" };
    fit.labels = { "OFF", "ON" };
    quant.labels = { "OFF", "1/16", "1/8", "1/4" };
    lock.labels = { "OFF", "ON" };
    for (auto* s : { &det, &key, &fit, &quant, &lock }) addAndMakeVisible (*s);
    det.onPick = [this] (int i) { proc.setParam ("detect", (float) i); };
    key.onPick = [this] (int i) { proc.setParam ("keylock", (float) i); };
    fit.onPick = [this] (int i) { proc.setParam ("fit", (float) i); };
    quant.onPick = [this] (int i) { proc.setParam ("quant", (float) i); };
    lock.onPick = [this] (int i)
    {
        proc.setParam ("fxlock", (float) i);
        if (toast) toast (i ? U8 ("FX LOCK: los efectos se mantienen al cambiar de audio") : U8 ("FX LOCK OFF"));
    };
    for (auto* s : { &sens, &dj })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }
    sensA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "sens", sens);
    djA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "dj", dj);
    sens.onValueChange = [this] { repaint(); };
    dj.onValueChange = [this]
    {
        if (std::abs (dj.getValue()) < .04 && dj.getValue() != 0.0) dj.setValue (0.0);
        repaint();
    };
    root.addItemList ({ "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 1);
    scale.addItemList ({ "MAY", "MEN" }, 1);
    addAndMakeVisible (root);
    addAndMakeVisible (scale);
    rootA = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "keyroot", root);
    scaleA = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "keyscale", scale);
    refresh();
}

void SettingsOverlay::refresh()
{
    det.selectedIndex = (int) proc.pv ("detect");
    key.selectedIndex = proc.pv ("keylock") > .5f ? 1 : 0;
    fit.selectedIndex = proc.pv ("fit") > .5f ? 1 : 0;
    quant.selectedIndex = (int) proc.pv ("quant");
    lock.selectedIndex = proc.pv ("fxlock") > .5f ? 1 : 0;
    for (auto* s : { &det, &key, &fit, &quant, &lock }) s->repaint();
}

static const float kColX[3] = { 20, 322, 624 };
static const float kColW = 280, kRowY[3] = { 44, 110, 176 };

void SettingsOverlay::resized()
{
    close.setBounds (getWidth() - 42, 10, 30, 30);
    auto at = [] (int c, int r, float w = kColW, float h = 31) { return Rectangle<float> (kColX[c], kRowY[r] + 18, w, h).toNearestInt(); };
    det.setBounds (at (0, 0));
    sens.setBounds (at (1, 0, kColW, 26));
    dj.setBounds (at (2, 0, kColW, 26));
    auto kr = at (0, 1);
    key.setBounds (kr.withWidth (kr.getWidth() - 124));
    root.setBounds (kr.getRight() - 118, kr.getY() + 2, 56, 27);
    scale.setBounds (kr.getRight() - 58, kr.getY() + 2, 58, 27);
    fit.setBounds (at (1, 1));
    quant.setBounds (at (2, 1));
    lock.setBounds (at (0, 2));
}

void SettingsOverlay::paint (Graphics& g)
{
    g.fillAll (Colour (3, 6, 26).withAlpha (.95f));
    drawText (g, U8 ("MOTOR \xc2\xb7 AJUSTES"), { 20, 14, 400, 16 }, Justification::centredLeft, font (11, true, .2f), vc::col::cy);
    auto label = [&] (int c, int r, const String& s)
    { drawText (g, s, { kColX[c], kRowY[r], kColW, 13 }, Justification::centredLeft, font (9.5f, true, .16f), vc::col::mut); };
    const float sv = (float) sens.getValue(), dv = (float) dj.getValue();
    label (0, 0, U8 ("DETECCI\xc3\x93N DE CORTES"));
    label (1, 0, "SENSIBILIDAD " + String (roundToInt (sv * 100)) + "%");
    label (2, 0, "FILTRO DJ " + (std::abs (dv) < .02f ? String ("OFF") : String (dv < 0 ? "LPF " : "HPF ") + String (roundToInt (std::abs (dv) * 100)) + "%"));
    label (0, 1, "KEY LOCK");
    label (1, 1, "SMART TEMPO FIT");
    label (2, 1, "QUANTIZE DE ENTRADA");
    label (0, 2, "FX LOCK");
    auto rv = [&] (int c, std::initializer_list<const char*> parts)
    {
        auto r = Rectangle<float> (kColX[c], kRowY[0] + 18 + 26, kColW, 12);
        const int n = (int) parts.size(); int i = 0;
        for (auto* p : parts)
        {
            auto j = i == 0 ? Justification::centredLeft : (i == n - 1 ? Justification::centredRight : Justification::centred);
            drawText (g, U8 (p), r, j, font (9.5f, false, .08f), Colours::white.withAlpha (.45f));
            ++i;
        }
    };
    rv (1, { "MENOS CORTES", "M\xc3\x81S CORTES" });
    rv (2, { "LPF", "OFF", "HPF" });
    g.setColour (Colours::white.withAlpha (.5f));
    g.setFont (font (10.5f));
    g.drawFittedText (U8 ("Teclado del ordenador: A W S E D F T G\xe2\x80\xa6 disparan los chops desde C1 (y por MIDI desde C1 = nota 36). "
                          "Espacio = play/stop. Doble clic en la onda a\xc3\xb1" "ade un corte; doble clic en una l\xc3\xadnea lo borra; "
                          "arrastra las l\xc3\xadneas para moverlas. Alt/Shift + PITCH = formantes."),
                      Rectangle<int> ((int) kColX[1], (int) kRowY[2] + 2, (int) (kColW * 2 + 22), 56), Justification::topLeft, 4, 1.0f);
}

// ===================================================================== LibraryOverlay
static std::vector<String> banks()
{
    std::vector<String> b;
    for (auto& p : patchList())
        if (std::find (b.begin(), b.end(), String (p.bank)) == b.end()) b.push_back (p.bank);
    return b;
}

Rectangle<float> LibraryOverlay::itemRect (int i) const
{
    const auto bk = banks();
    const float cw = (getWidth() - 40 - 42) / 4.0f;
    const auto& list = patchList();
    const int col = (int) (std::find (bk.begin(), bk.end(), String (list[(size_t) i].bank)) - bk.begin());
    int row = 0;
    for (int j = 0; j < i; ++j) if (String (list[(size_t) j].bank) == String (list[(size_t) i].bank)) ++row;
    return { 20 + col * (cw + 14), 44 + 20 + row * 46.0f, cw, 40 };
}

int LibraryOverlay::itemAt (Point<float> p) const
{
    for (int i = 0; i < (int) patchList().size(); ++i) if (itemRect (i).contains (p)) return i;
    return -1;
}

void LibraryOverlay::paint (Graphics& g)
{
    g.fillAll (Colour (3, 6, 26).withAlpha (.95f));
    drawText (g, U8 ("LIBRER\xc3\x8d" "A DE F\xc3\x81" "BRICA"), { 20, 14, 400, 16 }, Justification::centredLeft, font (11, true, .2f), vc::col::cy);
    const auto bk = banks();
    const float cw = (getWidth() - 40 - 42) / 4.0f;
    for (size_t c = 0; c < bk.size(); ++c)
        drawText (g, bk[c], { 20 + c * (cw + 14), 44, cw, 13 }, Justification::centredLeft, font (9.5f, true, .16f), vc::col::mut);
    const auto& list = patchList();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        auto r = itemRect (i);
        g.setColour (i == hover || i == proc.patchIndex ? vc::col::sel : Colours::white.withAlpha (.06f));
        g.fillRoundedRectangle (r, 8);
        drawText (g, U8 (list[(size_t) i].name), r.withTrimmedLeft (10).withTrimmedTop (5).withHeight (16), Justification::centredLeft, font (12), Colours::white);
        drawText (g, U8 (list[(size_t) i].info), r.withTrimmedLeft (10).withTrimmedTop (21).withHeight (13), Justification::centredLeft, font (9.5f), vc::col::mut);
    }
    g.setColour (Colours::white.withAlpha (.08f));
    g.fillRect (20.0f, 212.0f, getWidth() - 40.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (.5f));
    g.setFont (font (10.5f));
    g.drawFittedText (U8 ("Sonidos de muestra sintetizados. En la versi\xc3\xb3n final aqu\xc3\xad ir\xc3\xa1n vuestras grabaciones reales, "
                          "con sus cortes y ajustes guardados."), Rectangle<int> (20, 218, getWidth() - 40, 28), Justification::topLeft, 2, 1.0f);
}

// ===================================================================== WaveView
namespace
{
constexpr float VW = 924, VH = 252, PADX = 16, TOP = 30, BOT = 26;
}

WaveView::WaveView (VocalChopperProcessor& p) : proc (p) {}

float WaveView::X (double t) const { return PADX + (float) (t / dur) * (VW - 2 * PADX); }
double WaveView::Tm (float x) const { return clampv ((double) (x - PADX) / (VW - 2 * PADX), 0.0, 1.0) * dur; }

void WaveView::buildLayer (const SampleData& s)
{
    const int sc = 2;
    layer = Image (Image::ARGB, (int) VW * sc, (int) VH * sc, true);
    {
        Graphics g (layer);
        g.addTransform (AffineTransform::scale ((float) sc));
        const float* d0 = s.buf.getReadPointer (0);
        const float* d1 = s.buf.getReadPointer (1);
        const int n = s.buf.getNumSamples();
        const float mid = TOP + (VH - TOP - BOT) / 2, hh = (VH - TOP - BOT) / 2 - 6;
        ColourGradient gr (Colours::white, 0, mid - hh, Colours::white, 0, mid + hh, false);
        gr.addColour (.3, Colour (0xff8fe9ff)); gr.addColour (.5, Colour (0xff4f7bff)); gr.addColour (.7, Colour (0xff8fe9ff));
        g.setGradientFill (gr);
        for (float x = PADX; x < VW - PADX; x += 3)
        {
            const int a = (int) ((x - PADX) / (VW - 2 * PADX) * n), b = std::min (n, (int) ((x + 3 - PADX) / (VW - 2 * PADX) * n));
            float m = 0; double ss = 0;
            for (int i = a; i < b; i += 2)
            {
                const float v = std::max (std::abs (d0[i]), std::abs (d1[i]));
                if (v > m) m = v;
                ss += v * v;
            }
            const float rms = (float) std::sqrt (ss / std::max (1.0, (b - a) / 2.0));
            const float h = std::max (1.0f, std::pow (m, .8f) * hh), hr = std::max (.5f, std::pow (rms, .8f) * hh * 1.2f);
            g.setOpacity (.45f);
            g.fillRect (x, mid - h, 2.0f, h * 2);
            g.setOpacity (1.0f);
            g.fillRect (x, mid - hr, 2.0f, hr * 2);
        }
    }
    blurLayer = layer.rescaled (layer.getWidth() / 10, layer.getHeight() / 10, Graphics::highResamplingQuality);
}

void WaveView::tick()
{
    const int v = proc.contentVersion.load();
    if (v != version)
    {
        version = v;
        snap = proc.snapshot();
        if (snap.sample) { dur = std::max (0.01, snap.sample->duration()); buildLayer (*snap.sample); }
    }
    else
    {
        auto s = proc.snapshot();
        snap.chops = s.chops; snap.A = s.A; snap.B = s.B; snap.isHit = s.isHit; snap.srcBpm = s.srcBpm;
    }
    repaint();
}

WaveView::Hit WaveView::hitTest (float x) const
{
    if (std::abs (x - X (snap.A)) < 8) return { 0, -1 };
    if (std::abs (x - X (snap.B)) < 8) return { 1, -1 };
    for (int i = 1; i < (int) snap.chops.size(); ++i) if (std::abs (x - X (snap.chops[(size_t) i].s)) < 5) return { 2, i };
    const double t = Tm (x);
    for (int i = 0; i < (int) snap.chops.size(); ++i)
        if (t >= snap.chops[(size_t) i].s && t < snap.chops[(size_t) i].e) return { 3, i };
    return { 3, -1 };
}

void WaveView::paint (Graphics& g)
{
    Path clip; clip.addRoundedRectangle (getLocalBounds().toFloat(), 16);
    g.reduceClipRegion (clip);
    g.setGradientFill (ColourGradient (Colour (0xff071036), 0, 0, Colour (0xff02040f), 0, VH, false));
    g.fillAll();
    const int sel = proc.selected;

    if (snap.sample && layer.isValid())
    {
        const double phv = proc.engine.uiPlayhead.load();
        const bool hasPh = phv >= 0;
        int cur = -1;
        if (hasPh) for (int i = 0; i < (int) snap.chops.size(); ++i) if (phv >= snap.chops[(size_t) i].s && phv < snap.chops[(size_t) i].e) cur = i;
        const float mid = TOP + (VH - TOP - BOT) / 2;
        const float blur = proc.pv ("blur"), art = proc.pv ("artifacts");
        const float lv = clampv (proc.fx.level.load() * 6.0f, 0.0f, 1.0f);

        for (int i = 0; i < (int) snap.chops.size(); ++i)
        {
            const auto& ch = snap.chops[(size_t) i];
            const float a = X (ch.s), b = X (ch.e);
            Colour c = i == sel ? vc::col::acc.withAlpha (.13f) : i == cur ? vc::col::cy.withAlpha (.09f)
                     : (i % 2 ? Colours::white.withAlpha (.025f) : Colours::transparentWhite);
            g.setColour (c);
            g.fillRect (a, TOP - 8, b - a, VH - TOP - BOT + 16);
        }
        g.setColour (Colours::white.withAlpha (.06f));
        g.drawHorizontalLine ((int) mid, PADX, VW - PADX);

        if (blur > .02f)
        {
            Graphics::ScopedSaveState ss (g);
            g.setOpacity (std::min (.6f, blur * (.35f + lv * .8f)));
            const float sy = 1 + blur * .35f * (.4f + lv);
            g.addTransform (AffineTransform::scale (1, sy).translated (blur * 14, mid * (1 - sy)));
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (blurLayer, Rectangle<float> (0, 0, VW, VH));
        }
        g.setOpacity (1.0f);
        g.drawImage (layer, Rectangle<float> (0, 0, VW, VH));

        if (art > .5f && lv > .02f)
        {
            const int n = (int) std::floor ((art - .5f) * 16 * lv) + 1;
            for (int i = 0; i < n; ++i)
            {
                const float y = TOP + rnd.nextFloat() * (VH - TOP - BOT), h = 2 + rnd.nextFloat() * 10, dx = (rnd.nextFloat() - .5f) * 40 * art;
                g.drawImage (layer, (int) dx, (int) y, (int) VW, (int) h, 0, (int) y * 2, (int) VW * 2, (int) h * 2);
            }
        }
        else if (art > .05f && lv > .02f)
        {
            g.setColour (Colour (255, 180, 90).withAlpha (art * .08f * lv));
            g.fillRect (0.0f, TOP, VW, VH - TOP - BOT);
        }

        g.setColour (Colour (2, 4, 15).withAlpha (.62f));
        g.fillRect (0.0f, 0.0f, X (snap.A), VH);
        g.fillRect (X (snap.B), 0.0f, VW - X (snap.B), VH);

        const int rs = proc.engine.uiRollSub.load();
        if (hasPh && rs >= 0)
        {
            const float a = X (proc.engine.uiRollA.load()), b = X (proc.engine.uiRollB.load());
            g.setColour (vc::col::cy.withAlpha (.18f));
            g.fillRect (a, TOP - 8, std::max (2.0f, b - a), VH - TOP - BOT + 16);
        }

        // lineas de chop + etiquetas de nota
        const auto lf = mono (9, true);
        for (int i = 0; i < (int) snap.chops.size(); ++i)
        {
            const auto& ch = snap.chops[(size_t) i];
            const float x = X (ch.s);
            g.setColour (i == sel ? vc::col::acc.withAlpha (.9f) : Colours::white.withAlpha (.3f));
            g.fillRect (std::round (x), TOP - 8, 1.0f, VH - TOP - BOT + 16);
            const float w = X (ch.e) - x;
            if (w > 24 || i == sel)
            {
                const auto lab = noteName (36 + i);
                const float tw = GlyphArrangement::getStringWidth (lf, lab) + 10;
                g.setColour (i == sel ? vc::col::acc : i == cur ? vc::col::cy : Colours::white.withAlpha (.14f));
                g.fillRoundedRectangle (x + 2, 6, tw, 15, 7);
                drawText (g, lab, { x + 2, 6, tw, 15 }, Justification::centred, lf,
                          i == sel || i == cur ? Colour (0xff0a1240) : Colours::white.withAlpha (.85f));
            }
        }
        // marcadores de inicio / fin
        for (auto [t, s] : { std::pair<double, float> { snap.A, 1.0f }, { snap.B, -1.0f } })
        {
            const float x = X (t);
            g.setColour (Colours::white);
            g.fillRect (x - 1, TOP - 12, 2.0f, VH - TOP - BOT + 20);
            Path f;
            f.startNewSubPath (x, VH - BOT + 8); f.lineTo (x + 9 * s, VH - BOT + 8); f.lineTo (x + 9 * s, VH - BOT - 4); f.lineTo (x, VH - BOT - 14);
            f.closeSubPath();
            g.fillPath (f);
        }
        // estela + cabezal
        if (hasPh) { trail.push_back (X (phv)); if (trail.size() > 14) trail.erase (trail.begin()); }
        else if (! trail.empty()) trail.erase (trail.begin());
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const float a = (float) (i + 1) / (float) trail.size();
            g.setColour (vc::col::acc.withAlpha (jlimit (0.0f, 1.0f, a * .12f * (1 + blur * 2))));
            const float w = 2 + blur * 18 * a;
            g.fillRect (trail[i] - w / 2, TOP - 8, w, VH - TOP - BOT + 16);
        }
        if (hasPh)
        {
            const float x = X (phv);
            g.setColour (vc::col::acc);
            g.fillRect (x - 1, TOP - 12, 2.0f, VH - TOP - BOT + 20);
            g.fillEllipse (x - 4, TOP - 16, 8, 8);
        }
        // regla de tiempo
        {
            double stp = 120;
            for (double s : { .25, .5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0 }) if (dur / s <= 12) { stp = s; break; }
            const auto rf = mono (9.5f);
            for (double t = 0; t <= dur + 1e-6; t += stp)
            {
                const float x = X (t);
                g.setColour (Colours::white.withAlpha (.4f));
                g.fillRect (x, VH - 10, 1.0f, 4.0f);
                if (x > PADX + 8 && x < VW - PADX - 8)
                {
                    String lab = t >= 60 ? String ((int) (t / 60)) + ":" + String ((int) std::lround (std::fmod (t, 60.0))).paddedLeft ('0', 2)
                                         : (stp < 1 ? String (t, 2) : String ((int) std::lround (t))) + "s";
                    drawText (g, lab, { x - 30, VH - 23, 60, 12 }, Justification::centred, rf, Colours::white.withAlpha (.4f));
                }
            }
        }
        // ROLL 1/16
        if (hasPh && rs >= 0)
        {
            const String tag = "ROLL " + String (subs()[(size_t) rs].name);
            const auto f = font (10.5f, true, .12f);
            const float tw = GlyphArrangement::getStringWidth (f, tag) + 20;
            auto r = Rectangle<float> (VW - 14 - tw, 12, tw, 22);
            g.setColour (vc::col::cy.withAlpha (.14f)); g.fillRoundedRectangle (r, 11);
            drawText (g, tag, r, Justification::centred, f, vc::col::cy);
        }
        if (hoverX >= 0 && drag.type < 0)
        {
            g.setColour (Colours::white.withAlpha (.12f));
            g.fillRect (hoverX, TOP - 8, 1.0f, VH - TOP - BOT + 16);
        }
        // chip de edicion individual
        if (sel >= 0 && sel < (int) snap.chops.size())
        {
            const String t = "CHOP " + String (sel + 1) + U8 (" \xc2\xb7 ") + noteName (36 + sel) + U8 (" \xc2\xb7 EDICI\xc3\x93N INDIVIDUAL");
            const auto f = font (10.5f, true, .1f);
            const float tw = GlyphArrangement::getStringWidth (f, t);
            auto r = Rectangle<float> (14, 12, tw + 10 + 8 + 20 + 6, 30);
            g.setColour (vc::col::acc.withAlpha (.16f)); g.fillRoundedRectangle (r, 15);
            drawText (g, t, { r.getX() + 10, r.getY(), tw + 4, r.getHeight() }, Justification::centredLeft, f, vc::col::acc);
            auto x = Rectangle<float> (r.getRight() - 26, r.getY() + 5, 20, 20);
            g.setColour (vc::col::acc.withAlpha (.25f)); g.fillEllipse (x);
            drawText (g, U8 ("\xc3\x97"), x, Justification::centred, font (12), vc::col::acc);
        }
    }

    // brillo del cristal y sombra interior
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (.06f), 0, 0, Colours::transparentWhite, 0, VH * .3f, false));
    g.fillRect (0.0f, 0.0f, VW, VH * .3f);
    innerShadow (g, getLocalBounds().toFloat(), 16, 6, .9f);

    if (dragHint)
    {
        g.setColour (Colour (36, 72, 230).withAlpha (.35f));
        g.fillAll();
        drawText (g, U8 ("Suelta aqu\xc3\xad tu audio (.wav \xc2\xb7 .mp3 \xc2\xb7 .aiff \xc2\xb7 .flac)"), getLocalBounds().toFloat(),
                  Justification::centred, font (13, false, .06f), Colours::white);
    }
}

void WaveView::mouseDown (const MouseEvent& e)
{
    if (! snap.sample) return;
    const float x = e.position.x;
    // x del chip -> volver a edicion global
    const int sel = proc.selected;
    if (sel >= 0 && e.position.y >= 12 && e.position.y <= 42 && e.position.x < 420)
    {
        const String t = "CHOP " + String (sel + 1) + U8 (" \xc2\xb7 ") + noteName (36 + sel) + U8 (" \xc2\xb7 EDICI\xc3\x93N INDIVIDUAL");
        const float tw = GlyphArrangement::getStringWidth (font (10.5f, true, .1f), t);
        const float right = 14 + tw + 10 + 8 + 20 + 6;
        if (x >= right - 26 && x <= right - 6) { proc.selected = -1; if (onSelectionChanged) onSelectionChanged(); return; }
    }
    const auto h = hitTest (x);
    if (e.getNumberOfClicks() >= 2)
    {
        if (h.type == 2) { proc.removeLine (h.i); proc.selected = -1; }
        else if (h.type == 3 && h.i >= 0) proc.addCut (Tm (x));
        if (pressed >= 0) proc.releaseChop (pressed);
        pressed = -1;
        tick();
        if (onSelectionChanged) onSelectionChanged();
        if (toast) toast (String (proc.numChops()) + " chops");
        return;
    }
    if (h.type != 3) { drag = h; return; }
    if (h.i >= 0)
    {
        pressed = h.i;
        proc.pressChop (h.i);
        if (onSelectionChanged) onSelectionChanged();
    }
}

void WaveView::mouseDrag (const MouseEvent& e)
{
    if (! snap.sample || drag.type < 0) return;
    hoverX = e.position.x;
    const double t = Tm (e.position.x);
    if (drag.type == 0) proc.setMarker (true, t);
    else if (drag.type == 1) proc.setMarker (false, t);
    else proc.moveLine (drag.i, t);
}

void WaveView::mouseUp (const MouseEvent&)
{
    drag = { -1, -1 };
    if (pressed >= 0) { proc.releaseChop (pressed); pressed = -1; }
}

void WaveView::mouseMove (const MouseEvent& e)
{
    hoverX = e.position.x;
    if (! snap.sample) return;
    setMouseCursor (hitTest (e.position.x).type == 3 ? MouseCursor::PointingHandCursor : MouseCursor::LeftRightResizeCursor);
}

// ===================================================================== RenderButton
RenderButton::RenderButton (VocalChopperProcessor& p) : proc (p)
{
    waveIco = icon (R"(<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#14205e" stroke-width="2.6" stroke-linecap="round"><path d="M3 12h3l2-6 4 12 3-9 2 3h4"/></svg>)");
    dlIco = icon (R"(<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#ffd23f" stroke-width="2.6" stroke-linecap="round"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>)");
    setMouseCursor (MouseCursor::PointingHandCursor);
    setTooltip (U8 ("Renderiza el resultado a WAV 24 bits. Cuando est\xc3\xa9 listo, arr\xc3\xa1stralo a una pista o haz clic para guardarlo"));
}

RenderButton::~RenderButton() { *alive = false; }

void RenderButton::reset()
{
    ready = false;
    busy = false;
    repaint();
}

void RenderButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedBottom (4);
    if (ready) hwBody (g, r, 24, Colour (0xffffe066), Colour (0xfff5bf00), Colour (0xffa07a00), false);
    else hwBody (g, r, 24, Colour (0xff0f1a55), Colour (0xff070d33), Colour (0xff040820), false);
    auto dot = Rectangle<float> (6, (r.getHeight() - 36) / 2, 36, 36);
    if (ready) { g.setColour (vc::col::ink); g.fillEllipse (dot); }
    else
    {
        ColourGradient cg (Colour (0xfffff3b8), dot.getX() + 14, dot.getY() + 12, Colour (0xffc89b00), dot.getRight(), dot.getBottom(), true);
        cg.addColour (.6, vc::col::acc);
        g.setGradientFill (cg);
        g.fillEllipse (dot);
    }
    if (busy)
    {
        const float p = prog->load();
        Path ring;
        ring.addCentredArc (dot.getCentreX(), dot.getCentreY(), 20, 20, 0, 0, MathConstants<float>::twoPi * std::max (.01f, p), true);
        g.setColour (Colours::white);
        g.strokePath (ring, PathStrokeType (3, PathStrokeType::curved, PathStrokeType::rounded));
    }
    auto* ic = ready ? dlIco.get() : waveIco.get();
    if (ic) ic->drawAt (g, dot.getCentreX() - 9, dot.getCentreY() - 9, 1.0f);
    const String t = ready ? U8 ("ARR\xc3\x81STRAME") : busy ? String ("RENDERIZANDO") : String ("RENDER FILE");
    drawText (g, t, { 54, 0, r.getWidth() - 60, r.getHeight() }, Justification::centredLeft, font (11, true, .14f),
              ready ? vc::col::ink : Colours::white);
}

void RenderButton::mouseDown (const MouseEvent&) { dragged = false; }

void RenderButton::mouseDrag (const MouseEvent& e)
{
    if (! ready || dragged || e.getDistanceFromDragStart() < 6) return;
    dragged = true;
    DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void RenderButton::mouseUp (const MouseEvent& e)
{
    if (dragged || busy || ! getLocalBounds().contains (e.getPosition())) return;
    if (ready)
    {
        chooser = std::make_unique<FileChooser> ("Guardar render", File::getSpecialLocation (File::userDesktopDirectory).getChildFile (file.getFileName()), "*.wav");
        auto f = file;
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                              [this, f, al = alive] (const FileChooser& fc)
                              {
                                  if (! *al) return;
                                  auto dest = fc.getResult();
                                  if (dest != File() && f.copyFileTo (dest.withFileExtension ("wav")))
                                      if (toast) toast (U8 ("Guardado: ") + dest.getFileName());
                              });
        return;
    }
    busy = true;
    prog->store (0.0f);
    repaint();
    auto p = prog;
    auto al = alive;
    auto* pr = &proc;
    Thread::launch ([this, p, al, pr]
    {
        int bars = 0;
        auto f = pr->renderToFile ([p] (float v) { p->store (v); }, bars);
        MessageManager::callAsync ([this, al, f, bars]
        {
            if (! *al) return;
            busy = false;
            if (f.existsAsFile())
            {
                file = f;
                ready = true;
                this->bars = bars;
                if (toast) toast (U8 ("Render listo (") + String (bars) + U8 (" compases): arr\xc3\xa1stralo a una pista o haz clic para guardarlo"));
            }
            else if (toast) toast (U8 ("No he podido renderizar"));
            repaint();
        });
    });
}

// ===================================================================== Lcd
void Lcd::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff050a26));
    g.fillRoundedRectangle (b, 9);
    innerShadow (g, b, 9, 4, .8f);
    drawText (g, String (proc.currentBpm.load(), 1), b.withTrimmedLeft (12), Justification::centredLeft, mono (19), vc::col::cy);
    const bool host = proc.hostTempo.load();
    auto snap = proc.snapshot();
    const String l1 = host ? U8 ("BPM \xc2\xb7 HOST") : U8 ("BPM \xc2\xb7 INT");
    const String l2 = snap.isHit && snap.srcBpm > 0 ? ("LOOP ~" + String ((int) std::lround (snap.srcBpm)) + (proc.pv ("fit") > .5f ? U8 (" \xc2\xb7 FIT") : String()))
                                                    : String ("MODO VOCAL");
    const auto f = mono (9);
    drawText (g, l1, { 0, 8, b.getWidth() - 12, 12 }, Justification::centredRight, f, vc::col::cy.withAlpha (.6f));
    drawText (g, l2, { 0, 20, b.getWidth() - 12, 12 }, Justification::centredRight, f, vc::col::cy.withAlpha (.6f));
}

void Lcd::mouseDrag (const MouseEvent& e)
{
    if (proc.hostTempo.load()) return; // dentro del DAW manda el tempo del proyecto
    const float v = jlimit (60.0f, 200.0f, std::round ((b0 + (y0 - (float) e.y) / 3.0f) * 2.0f) / 2.0f);
    proc.setParam ("bpm", v);
    repaint();
}

void Lcd::mouseDoubleClick (const MouseEvent&)
{
    auto s = proc.snapshot();
    if (s.srcBpm > 0 && ! proc.hostTempo.load()) proc.setParam ("bpm", jlimit (60.0f, 200.0f, (float) std::round (s.srcBpm * 2) / 2.0f));
}

// ===================================================================== Toast
void Toast::show (const String& m)
{
    msg = m;
    alpha = 1;
    ticks = 0;
    startTimerHz (30);
    repaint();
}

void Toast::timerCallback()
{
    ++ticks;
    if (ticks > 66) alpha = std::max (0.0f, alpha - .14f);
    if (alpha <= 0) stopTimer();
    repaint();
}

void Toast::paint (Graphics& g)
{
    if (alpha <= 0 || msg.isEmpty()) return;
    const auto f = font (11, false, .04f);
    const float tw = std::min ((float) getWidth() - 20, GlyphArrangement::getStringWidth (f, msg) + 28);
    auto r = Rectangle<float> ((getWidth() - tw) / 2, (getHeight() - 28) / 2.0f, tw, 28);
    g.setColour (Colour (3, 6, 26).withAlpha (.9f * alpha));
    g.fillRoundedRectangle (r, 14);
    drawText (g, msg, r, Justification::centred, f, Colours::white.withAlpha (alpha));
}
} // namespace ui
