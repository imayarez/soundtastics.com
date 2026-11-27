#include "UI.h"
#include "BinaryData.h"

using namespace juce;
using namespace vc;

namespace ui
{
// ===================================================================== letras e imagenes
static Typeface::Ptr tfArchivo()
{
    static Typeface::Ptr t = Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    return t;
}
static Typeface::Ptr tfOswald()
{
    static Typeface::Ptr t = Typeface::createSystemTypefaceFor (BinaryData::OswaldSemiBold_ttf, BinaryData::OswaldSemiBold_ttfSize);
    return t;
}

Font heavy (float px) { return Font (FontOptions (tfArchivo()).withPointHeight (px)); }
Font cond (float px)  { return Font (FontOptions (tfOswald()).withPointHeight (px)); }

Font mono (float px, bool bold)
{
    return Font (FontOptions().withName (Font::getDefaultMonospacedFontName()).withPointHeight (px).withStyle (bold ? "Bold" : "Regular"));
}

Font font (float px, bool bold, float kern)
{
    return Font (FontOptions().withPointHeight (px).withStyle (bold ? "Bold" : "Regular").withKerningFactor (kern));
}

Image lettering()
{
    static Image img = ImageCache::getFromMemory (BinaryData::by_soundtastics_white_png, BinaryData::by_soundtastics_white_pngSize);
    return img;
}

std::unique_ptr<Drawable> icon (const String& svg)
{
    if (svg.isEmpty()) return nullptr;
    if (auto xml = parseXML (svg)) return Drawable::createFromSVG (*xml);
    return nullptr;
}

void drawText (Graphics& g, const String& s, Rectangle<float> r, Justification j, const Font& f, Colour col)
{
    g.setColour (col);
    g.setFont (f);
    g.drawText (s, r, j, false);
}

// ===================================================================== BlackButton
BlackButton::BlackButton (const String& t, const String& svg, float textPx)
    : text (t), icoW (icon (svg.replace ("currentColor", "#ffffff"))), icoK (icon (svg.replace ("currentColor", "#111111"))), px (textPx)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void BlackButton::mouseUp (const MouseEvent& e)
{
    down = false;
    repaint();
    if (isEnabled() && getLocalBounds().contains (e.getPosition()) && onClick) onClick();
}

void BlackButton::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2).withTrimmedBottom (3);
    const float off = down ? 2.0f : 0.0f;
    const bool dim = ! isEnabled();
    if (dim) g.beginTransparencyLayer (.35f);
    // sombra dura debajo
    g.setColour (Colours::black.withAlpha (.35f));
    g.fillRoundedRectangle (b.translated (0, down ? 1.0f : 3.0f).expanded (2), 6);
    auto r = b.translated (0, off);
    g.setColour (c::ink);
    g.fillRoundedRectangle (r.expanded (2), 6.5f);
    g.setColour (on ? onColour : c::blk);
    g.fillRoundedRectangle (r, 5);
    g.setColour (Colours::white.withAlpha (on ? .3f : .12f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4, r.getRight() - 4);

    const Colour tc = on ? c::ink : Colours::white;
    auto* d = on ? icoK.get() : icoW.get();
    const auto f = cond (px);
    const float tw = text.isEmpty() ? 0.0f : GlyphArrangement::getStringWidth (f, text);
    float iw = 0, ih = 0;
    if (d != nullptr) { auto ib = d->getDrawableBounds(); iw = ib.getWidth(); ih = ib.getHeight(); }
    const float gap = (d != nullptr && tw > 0) ? 7.0f : 0.0f;
    float x = r.getCentreX() - (iw + gap + tw) / 2;
    if (d != nullptr)
    {
        auto ib = d->getDrawableBounds();
        d->drawAt (g, x - ib.getX(), r.getCentreY() - ih / 2 - ib.getY(), 1.0f);
        x += iw + gap;
    }
    if (tw > 0)
    {
        // varias lineas si no cabe (p. ej. "BUILD UP" en botones estrechos)
        if (tw > r.getWidth() - 8 && text.containsChar (' '))
        {
            g.setColour (tc);
            g.setFont (cond (px * .92f));
            g.drawFittedText (text.replaceCharacter (' ', '\n'), r.toNearestInt(), Justification::centred, 2, 1.0f);
        }
        else drawText (g, text, { x, r.getY(), tw + 2, r.getHeight() }, Justification::centredLeft, f, tc);
    }
    if (dim) g.endTransparencyLayer();
}

// ===================================================================== Knob
Rectangle<float> Knob::knobArea() const
{
    // la escala sobresale un 14 % alrededor del mando
    const float s = std::min (getWidth(), getHeight()) / 1.16f;
    return { (getWidth() - s) / 2, (getHeight() - s) / 2, s, s };
}

void Knob::paint (Graphics& g)
{
    const auto A = knobArea();
    const float k = A.getWidth() / 100.0f, cx = A.getCentreX(), cy = A.getCentreY();
    const float v = getValue ? getValue (false) : 0.0f;
    const float ang = -135.0f + 270.0f * v;
    auto rad = [] (float d) { return d * MathConstants<float>::pi / 180.0f; };

    // escala impresa: 11 marcas, la central y los extremos mas largas
    g.setColour (c::ink);
    for (int j = 0; j <= 10; ++j)
    {
        const float a = rad (-135.0f + 27.0f * j - 90.0f);
        const float r1 = 47 * k, r2 = (j % 5 == 0 ? 57 : 54) * k;
        Line<float> ln (cx + r1 * std::cos (a), cy + r1 * std::sin (a), cx + r2 * std::cos (a), cy + r2 * std::sin (a));
        g.drawLine (ln, 2.6f * k * (isBig ? 1.0f : 1.1f));
    }

    // tapa roja (amarilla cuando se edita un chop concreto)
    const float capF = isBig ? 64.0f / 84.0f : 72.0f / 96.0f;
    const float cs = A.getWidth() * capF;
    auto cap = Rectangle<float> (cx - cs / 2, cy - cs / 2, cs, cs);
    const bool y = chopMode;
    const Colour hi = y ? Colour (0xfffff38a) : Colour (0xffff6a5e);
    const Colour mid = y ? Colour (0xffffd23f) : Colour (0xffe3262b);
    const Colour lo = y ? Colour (0xffc79c00) : Colour (0xffa10f14);
    const Colour lip = y ? Colour (0xff8a6a00) : Colour (0xff6e0a0d);
    {
        Path p; p.addEllipse (cap.translated (0, cs * .1f));
        DropShadow (Colours::black.withAlpha (.35f), (int) (cs * .16f), { 0, 0 }).drawForPath (g, p);
    }
    g.setColour (lip);
    g.fillEllipse (cap.translated (0, cs * .075f));
    ColourGradient cg (hi, cap.getX() + cs * .38f, cap.getY() + cs * .30f, lo, cap.getRight(), cap.getBottom(), true);
    cg.addColour (.45, mid);
    g.setGradientFill (cg);
    g.fillEllipse (cap);
    // moleteado en el borde
    {
        Graphics::ScopedSaveState ss (g);
        Path ring; ring.addEllipse (cap.reduced (cs * .06f));
        Path inner; inner.addEllipse (cap.reduced (cs * .06f + cs * .44f * .2f));
        ring.addPath (inner); ring.setUsingNonZeroWinding (false);
        g.reduceClipRegion (ring);
        g.setColour (Colours::black.withAlpha (.18f));
        Path kn;
        for (int i = 0; i < 120; i += 2) kn.addPieSegment (cap, rad (i * 3.0f), rad (i * 3.0f + 3.0f), 0.0f);
        g.fillPath (kn);
    }
    g.setColour (Colours::black.withAlpha (.2f));
    g.drawEllipse (cap.reduced (cs * .17f), cs * .035f);
    g.setColour (Colours::white.withAlpha (.35f));
    {
        Path hl; hl.addCentredArc (cx, cy, cs / 2 - 1.5f, cs / 2 - 1.5f, 0, rad (-50), rad (50), true);
        g.strokePath (hl, PathStrokeType (1.6f));
    }
    // aguja
    {
        const float w = (isBig ? 4.0f : 4.5f) * cs / 64.0f, h = cs * (isBig ? 22.0f / 64.0f : 24.0f / 72.0f);
        const float top = cs * 5.0f / 64.0f;
        Path ind; ind.addRoundedRectangle (cx - w / 2, cap.getY() + top, w, h, w / 2);
        g.setColour (y ? c::ink : Colours::white);
        g.fillPath (ind, AffineTransform::rotation (rad (ang), cx, cy));
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

// ===================================================================== Seg / Close / Look
void Seg::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colours::white.withAlpha (.08f));
    g.fillRoundedRectangle (b, 4);
    const int n = labels.size();
    if (n == 0) return;
    const float w = (b.getWidth() - 4 - 2 * (n - 1)) / n;
    for (int i = 0; i < n; ++i)
    {
        auto r = Rectangle<float> (2 + i * (w + 2), 2, w, b.getHeight() - 4);
        if (i == selectedIndex) { g.setColour (c::yel); g.fillRoundedRectangle (r, 3); }
        drawText (g, labels[i], r, Justification::centred, cond (12), i == selectedIndex ? c::ink : Colours::white.withAlpha (.65f));
    }
}

void Seg::mouseDown (const MouseEvent& e)
{
    const int n = labels.size();
    if (n == 0) return;
    selectedIndex = jlimit (0, n - 1, (int) ((float) e.x / getWidth() * n));
    repaint();
    if (onPick) onPick (selectedIndex);
}

void CloseButton::paint (Graphics& g)
{
    g.setColour (Colours::white.withAlpha (.12f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4);
    drawText (g, U8 ("\xc3\x97"), getLocalBounds().toFloat(), Justification::centred, font (15), Colours::white);
}

Look::Look()
{
    setColour (PopupMenu::backgroundColourId, c::blk);
    setColour (PopupMenu::highlightedBackgroundColourId, c::yel);
    setColour (PopupMenu::textColourId, Colours::white);
    setColour (PopupMenu::highlightedTextColourId, c::ink);
    setColour (ComboBox::textColourId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, c::blk);
    setColour (TooltipWindow::textColourId, Colours::white);
}

void Look::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider&)
{
    const float cy = y + h / 2.0f;
    g.setColour (Colours::white.withAlpha (.2f));
    g.fillRoundedRectangle ((float) x, cy - 2, (float) w, 4, 2);
    g.setColour (Colour (0xffff8a2a));
    g.fillRoundedRectangle (pos - 7, cy - 9, 14, 18, 3);
    g.setColour (Colours::black.withAlpha (.3f));
    g.drawHorizontalLine ((int) cy, pos - 5, pos + 5);
}

void Look::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    g.setColour (Colours::white.withAlpha (.1f));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4);
    Path a; a.startNewSubPath (w - 13.0f, h / 2.0f - 2); a.lineTo (w - 9.0f, h / 2.0f + 2); a.lineTo (w - 5.0f, h / 2.0f - 2);
    g.setColour (Colours::white.withAlpha (.7f));
    g.strokePath (a, PathStrokeType (1.4f));
}

void Look::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (2, 0, box.getWidth() - 14, box.getHeight());
    label.setFont (cond (12));
}

void Look::drawPopupMenuBackground (Graphics& g, int, int) { g.fillAll (c::blk); }

void Look::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.fillAll (c::blk);
    drawText (g, text, Rectangle<float> (0, 0, (float) w, (float) h).reduced (6, 2), Justification::centredLeft, font (11), Colours::white);
}

// ===================================================================== SettingsOverlay (648 x 206)
namespace
{
const float kOvColX[3] = { 14, 226, 438 };
const float kOvColW = 196, kOvRowY[3] = { 30, 82, 140 };
}

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
        if (toast) toast (i ? U8 ("FX LOCK: los efectos se mantienen al cambiar de audio") : String ("FX LOCK OFF"));
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
        if (std::abs (dj.getValue()) < .04 && std::abs (dj.getValue()) > 1e-9) dj.setValue (0.0);
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

void SettingsOverlay::resized()
{
    close.setBounds (getWidth() - 34, 6, 26, 26);
    auto at = [] (int c, int r, float h = 26) { return Rectangle<float> (kOvColX[c], kOvRowY[r] + 16, kOvColW, h).toNearestInt(); };
    det.setBounds (at (0, 0));
    sens.setBounds (at (1, 0, 22));
    dj.setBounds (at (2, 0, 22));
    auto kr = at (0, 1);
    key.setBounds (kr.withWidth (kr.getWidth() - 112));
    root.setBounds (kr.getRight() - 108, kr.getY(), 50, 26);
    scale.setBounds (kr.getRight() - 54, kr.getY(), 54, 26);
    fit.setBounds (at (1, 1));
    quant.setBounds (at (2, 1));
    lock.setBounds (at (0, 2));
}

void SettingsOverlay::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b0c0e));
    drawText (g, "MOTOR / AJUSTES", { 14, 6, 300, 18 }, Justification::centredLeft, cond (15), c::yel);
    auto label = [&] (int col, int r, const String& s)
    { drawText (g, s, { kOvColX[col], kOvRowY[r], kOvColW, 14 }, Justification::centredLeft, cond (12), Colours::white.withAlpha (.65f)); };
    const float sv = (float) sens.getValue(), dv = (float) dj.getValue();
    label (0, 0, U8 ("DETECCI\xc3\x93N DE CORTES"));
    label (1, 0, "SENSIBILIDAD " + String (roundToInt (sv * 100)) + "%");
    label (2, 0, "FILTRO DJ " + (std::abs (dv) < .02f ? String ("OFF") : String (dv < 0 ? "LPF " : "HPF ") + String (roundToInt (std::abs (dv) * 100)) + "%"));
    label (0, 1, "KEY LOCK");
    label (1, 1, "SMART TEMPO FIT");
    label (2, 1, "QUANTIZE DE ENTRADA");
    label (0, 2, "FX LOCK");
    auto rv = [&] (int col, std::initializer_list<const char*> parts)
    {
        auto r = Rectangle<float> (kOvColX[col], kOvRowY[0] + 16 + 22, kOvColW, 11);
        const int n = (int) parts.size(); int i = 0;
        for (auto* p : parts)
        {
            auto j = i == 0 ? Justification::centredLeft : (i == n - 1 ? Justification::centredRight : Justification::centred);
            drawText (g, U8 (p), r, j, cond (10), Colours::white.withAlpha (.45f));
            ++i;
        }
    };
    rv (1, { "MENOS CORTES", "M\xc3\x81S CORTES" });
    rv (2, { "LPF", "OFF", "HPF" });
    g.setColour (Colours::white.withAlpha (.5f));
    g.setFont (font (10.5f));
    g.drawFittedText (U8 ("Teclado: A W S E D F T G\xe2\x80\xa6 disparan los chops desde C1 (y por MIDI desde C1 = nota 36). Espacio = play/stop. "
                          "Doble clic en la onda a\xc3\xb1" "ade un corte; doble clic en una l\xc3\xadnea lo borra; arrastra las l\xc3\xadneas para moverlas. "
                          "Alt/Shift + PITCH = formantes."),
                      Rectangle<int> ((int) kOvColX[1], (int) kOvRowY[2], (int) (kOvColW * 2 + 16), 60), Justification::topLeft, 4, 1.0f);
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
    const float cw = (getWidth() - 28 - 30) / 4.0f;
    const auto& list = patchList();
    const int col = (int) (std::find (bk.begin(), bk.end(), String (list[(size_t) i].bank)) - bk.begin());
    int row = 0;
    for (int j = 0; j < i; ++j) if (String (list[(size_t) j].bank) == String (list[(size_t) i].bank)) ++row;
    return { 14 + col * (cw + 10), 48 + row * 37.0f, cw, 33 };
}

int LibraryOverlay::itemAt (Point<float> p) const
{
    for (int i = 0; i < (int) patchList().size(); ++i) if (itemRect (i).contains (p)) return i;
    return -1;
}

void LibraryOverlay::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b0c0e));
    drawText (g, U8 ("LIBRER\xc3\x8d" "A DE F\xc3\x81" "BRICA"), { 14, 6, 300, 18 }, Justification::centredLeft, cond (15), c::yel);
    const auto bk = banks();
    const float cw = (getWidth() - 28 - 30) / 4.0f;
    for (size_t col = 0; col < bk.size(); ++col)
        drawText (g, bk[col], { 14 + col * (cw + 10), 30, cw, 14 }, Justification::centredLeft, cond (11), Colours::white.withAlpha (.6f));
    const auto& list = patchList();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        auto r = itemRect (i);
        const bool on = i == hover || i == proc.patchIndex;
        g.setColour (on ? c::yel : Colours::white.withAlpha (.08f));
        g.fillRoundedRectangle (r, 4);
        const Colour tc = on ? c::ink : Colours::white;
        drawText (g, U8 (list[(size_t) i].name), r.withTrimmedLeft (8).withTrimmedTop (3).withHeight (16), Justification::centredLeft, cond (13), tc);
        drawText (g, U8 (list[(size_t) i].info), r.withTrimmedLeft (8).withTrimmedTop (18).withHeight (12), Justification::centredLeft, font (9.5f), tc.withAlpha (.7f));
    }
    g.setColour (Colours::white.withAlpha (.45f));
    g.setFont (font (10));
    g.drawFittedText (U8 ("Sonidos de muestra sintetizados. En la versi\xc3\xb3n final aqu\xc3\xad ir\xc3\xa1n vuestras grabaciones reales, con sus cortes y ajustes guardados."),
                      Rectangle<int> (14, getHeight() - 30, getWidth() - 28, 26), Justification::topLeft, 2, 1.0f);
}

// ===================================================================== WaveView
WaveView::WaveView (VocalChopperProcessor& p) : proc (p) {}

float WaveView::X (double t) const { return PADX + (float) (t / dur) * (W - 2 * PADX); }
double WaveView::Tm (float x) const { return clampv ((double) (x - PADX) / (W - 2 * PADX), 0.0, 1.0) * dur; }

void WaveView::buildLayer (const SampleData& s)
{
    W = (float) std::max (100, getWidth());
    H = (float) std::max (60, getHeight());
    const int sc = 2;
    layer = Image (Image::ARGB, (int) W * sc, (int) H * sc, true);
    {
        Graphics g (layer);
        g.addTransform (AffineTransform::scale ((float) sc));
        const float* d0 = s.buf.getReadPointer (0);
        const float* d1 = s.buf.getReadPointer (1);
        const int n = s.buf.getNumSamples();
        const float mid = TOP + (H - TOP - BOT) / 2, hh = (H - TOP - BOT) / 2 - 6;

        // Onda por frecuencias (estilo CDJ): graves violeta, medios azul, agudos cian-hielo
        struct Bq2
        {
            double b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            Bq2 (bool hp, double f, double sr)
            {
                const double w = 2 * vc::kPi * f / sr, al = std::sin (w) / (2 * std::sqrt (0.5)), cw = std::cos (w), a0 = 1 + al;
                b0 = (hp ? (1 + cw) / 2 : (1 - cw) / 2) / a0; b1 = (hp ? -(1 + cw) : 1 - cw) / a0; b2 = b0;
                a1 = -2 * cw / a0; a2 = (1 - al) / a0;
            }
            double run (double x) { const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
        };
        Bq2 l1 (false, 200, s.sr), l2 (false, 200, s.sr), h1 (true, 2500, s.sr), h2 (true, 2500, s.sr);
        std::vector<float> LO ((size_t) n), HI ((size_t) n), MI ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double x = (d0[i] + d1[i]) * 0.5, l = l2.run (l1.run (x)), h = h2.run (h1.run (x));
            LO[(size_t) i] = (float) l; HI[(size_t) i] = (float) h; MI[(size_t) i] = (float) (x - l - h);
        }
        struct Col { float x, m, rms, eL, eM, eH; };
        std::vector<Col> cols;
        float mL = 1e-9f, mM = 1e-9f, mH = 1e-9f;
        for (float x = PADX; x < W - PADX; x += 3)
        {
            const int a = (int) ((x - PADX) / (W - 2 * PADX) * n), b = std::min (n, (int) ((x + 3 - PADX) / (W - 2 * PADX) * n));
            float m = 0; double ss = 0, eL = 0, eM = 0, eH = 0;
            for (int i = a; i < b; i += 2)
            {
                const float v = std::max (std::abs (d0[i]), std::abs (d1[i]));
                if (v > m) m = v;
                ss += v * v;
                eL += LO[(size_t) i] * LO[(size_t) i]; eM += MI[(size_t) i] * MI[(size_t) i]; eH += HI[(size_t) i] * HI[(size_t) i];
            }
            Col cl { x, m, (float) std::sqrt (ss / std::max (1.0, (b - a) / 2.0)), (float) std::sqrt (eL), (float) std::sqrt (eM), (float) std::sqrt (eH) };
            mL = std::max (mL, cl.eL); mM = std::max (mM, cl.eM); mH = std::max (mH, cl.eH);
            cols.push_back (cl);
        }
        const float CL[3] { 164, 92, 255 }, CM[3] { 63, 125, 255 }, CH[3] { 178, 246, 255 };
        for (auto& kc : cols)
        {
            const float h = std::max (1.0f, std::pow (kc.m, .8f) * hh), hr = std::max (.5f, std::pow (kc.rms, .8f) * hh * 1.2f);
            const float wL = std::pow (kc.eL / mL, 2.2f), wM = std::pow (kc.eM / mM, 2.2f) * .8f, wH = std::pow (kc.eH / mH, 2.2f) * 1.2f;
            const float Wt = wL + wM + wH > 0 ? wL + wM + wH : 1.0f;
            auto ch = [&] (int j) { return (uint8) jlimit (0, 255, roundToInt ((CL[j] * wL + CM[j] * wM + CH[j] * wH) / Wt)); };
            const Colour col = kc.rms < .004f ? Colours::white.withAlpha (.28f) : Colour (ch (0), ch (1), ch (2));
            g.setColour (col.withMultipliedAlpha (.45f));
            g.fillRect (kc.x, mid - h, 2.0f, h * 2);
            g.setColour (col);
            g.fillRect (kc.x, mid - hr, 2.0f, hr * 2);
        }
    }
    blurLayer = layer.rescaled (std::max (1, layer.getWidth() / 10), std::max (1, layer.getHeight() / 10), Graphics::highResamplingQuality);
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
    if (std::abs (x - X (snap.A)) < 7) return { 0, -1 };
    if (std::abs (x - X (snap.B)) < 7) return { 1, -1 };
    for (int i = 1; i < (int) snap.chops.size(); ++i) if (std::abs (x - X (snap.chops[(size_t) i].s)) < 4) return { 2, i };
    const double t = Tm (x);
    for (int i = 0; i < (int) snap.chops.size(); ++i)
        if (t >= snap.chops[(size_t) i].s && t < snap.chops[(size_t) i].e) return { 3, i };
    return { 3, -1 };
}

static String chipText (int sel)
{
    return "CHOP " + String (sel + 1) + U8 (" \xc2\xb7 ") + noteName (36 + sel) + U8 (" \xc2\xb7 EDICI\xc3\x93N INDIVIDUAL");
}

Rectangle<float> WaveView::chipRect() const
{
    const float tw = GlyphArrangement::getStringWidth (cond (12), chipText (std::max (0, proc.selected)));
    return { 10, 8, tw + 9 + 8 + 18 + 5, 24 };
}

void WaveView::paint (Graphics& g)
{
    Path clip; clip.addRoundedRectangle (getLocalBounds().toFloat(), 5);
    g.reduceClipRegion (clip);
    g.fillAll (c::screen);
    const int sel = proc.selected;

    if (snap.sample && layer.isValid())
    {
        const double phv = proc.engine.uiPlayhead.load();
        const bool hasPh = phv >= 0;
        int cur = -1;
        if (hasPh) for (int i = 0; i < (int) snap.chops.size(); ++i) if (phv >= snap.chops[(size_t) i].s && phv < snap.chops[(size_t) i].e) cur = i;
        const float mid = TOP + (H - TOP - BOT) / 2;
        const float blur = proc.pv ("blur"), art = proc.pv ("artifacts");
        const float lv = clampv (proc.fx.level.load() * 6.0f, 0.0f, 1.0f);

        for (int i = 0; i < (int) snap.chops.size(); ++i)
        {
            const auto& ch = snap.chops[(size_t) i];
            const float a = X (ch.s), b = X (ch.e);
            Colour col = i == sel ? vc::col::acc.withAlpha (.13f) : i == cur ? vc::col::cy.withAlpha (.09f)
                       : (i % 2 ? Colours::white.withAlpha (.025f) : Colours::transparentWhite);
            g.setColour (col);
            g.fillRect (a, TOP - 6, b - a, H - TOP - BOT + 12);
        }
        g.setColour (Colours::white.withAlpha (.06f));
        g.drawHorizontalLine ((int) mid, PADX, W - PADX);

        if (blur > .02f)
        {
            Graphics::ScopedSaveState ss (g);
            g.setOpacity (std::min (.6f, blur * (.35f + lv * .8f)));
            const float sy = 1 + blur * .35f * (.4f + lv);
            g.addTransform (AffineTransform::scale (1, sy).translated (blur * 10, mid * (1 - sy)));
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (blurLayer, Rectangle<float> (0, 0, W, H));
        }
        g.setOpacity (1.0f);
        g.drawImage (layer, Rectangle<float> (0, 0, W, H));

        if (art > .5f && lv > .02f)
        {
            const int n = (int) std::floor ((art - .5f) * 16 * lv) + 1;
            for (int i = 0; i < n; ++i)
            {
                const float y = TOP + rnd.nextFloat() * (H - TOP - BOT), h = 2 + rnd.nextFloat() * 8, dx = (rnd.nextFloat() - .5f) * 30 * art;
                g.drawImage (layer, (int) dx, (int) y, (int) W, (int) h, 0, (int) y * 2, (int) W * 2, (int) h * 2);
            }
        }
        else if (art > .05f && lv > .02f)
        {
            g.setColour (Colour (255, 180, 90).withAlpha (art * .08f * lv));
            g.fillRect (0.0f, TOP, W, H - TOP - BOT);
        }

        g.setColour (Colour (2, 3, 6).withAlpha (.62f));
        g.fillRect (0.0f, 0.0f, X (snap.A), H);
        g.fillRect (X (snap.B), 0.0f, W - X (snap.B), H);

        const int rs = proc.engine.uiRollSub.load();
        if (hasPh && rs >= 0)
        {
            const float a = X (proc.engine.uiRollA.load()), b = X (proc.engine.uiRollB.load());
            g.setColour (vc::col::cy.withAlpha (.18f));
            g.fillRect (a, TOP - 6, std::max (2.0f, b - a), H - TOP - BOT + 12);
        }

        const auto lf = mono (8.5f, true);
        for (int i = 0; i < (int) snap.chops.size(); ++i)
        {
            const auto& ch = snap.chops[(size_t) i];
            const float x = X (ch.s);
            g.setColour (i == sel ? vc::col::acc.withAlpha (.9f) : Colours::white.withAlpha (.3f));
            g.fillRect (std::round (x), TOP - 6, 1.0f, H - TOP - BOT + 12);
            const float w = X (ch.e) - x;
            if (w > 22 || i == sel)
            {
                const auto lab = noteName (36 + i);
                const float tw = GlyphArrangement::getStringWidth (lf, lab) + 9;
                g.setColour (i == sel ? vc::col::acc : i == cur ? vc::col::cy : Colours::white.withAlpha (.14f));
                g.fillRoundedRectangle (x + 2, 5, tw, 14, 7);
                drawText (g, lab, { x + 2, 5, tw, 14 }, Justification::centred, lf,
                          i == sel || i == cur ? Colour (0xff0a1240) : Colours::white.withAlpha (.85f));
            }
        }
        for (auto [t, s] : { std::pair<double, float> { snap.A, 1.0f }, { snap.B, -1.0f } })
        {
            const float x = X (t);
            g.setColour (Colours::white);
            g.fillRect (x - 1, TOP - 10, 2.0f, H - TOP - BOT + 16);
            Path f;
            f.startNewSubPath (x, H - BOT + 6); f.lineTo (x + 8 * s, H - BOT + 6); f.lineTo (x + 8 * s, H - BOT - 4); f.lineTo (x, H - BOT - 12);
            f.closeSubPath();
            g.fillPath (f);
        }
        if (hasPh) { trail.push_back (X (phv)); if (trail.size() > 14) trail.erase (trail.begin()); }
        else if (! trail.empty()) trail.erase (trail.begin());
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const float a = (float) (i + 1) / (float) trail.size();
            g.setColour (vc::col::acc.withAlpha (jlimit (0.0f, 1.0f, a * .12f * (1 + blur * 2))));
            const float w = 2 + blur * 14 * a;
            g.fillRect (trail[i] - w / 2, TOP - 6, w, H - TOP - BOT + 12);
        }
        if (hasPh)
        {
            const float x = X (phv);
            g.setColour (vc::col::acc);
            g.fillRect (x - 1, TOP - 10, 2.0f, H - TOP - BOT + 16);
            g.fillEllipse (x - 3.5f, TOP - 14, 7, 7);
        }
        {
            double stp = 120;
            for (double s : { .25, .5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0 }) if (dur / s <= 9) { stp = s; break; }
            const auto rf = mono (9);
            for (double t = 0; t <= dur + 1e-6; t += stp)
            {
                const float x = X (t);
                g.setColour (Colours::white.withAlpha (.4f));
                g.fillRect (x, H - 8, 1.0f, 4.0f);
                if (x > PADX + 8 && x < W - PADX - 8)
                {
                    String lab = t >= 60 ? String ((int) (t / 60)) + ":" + String ((int) std::lround (std::fmod (t, 60.0))).paddedLeft ('0', 2)
                                         : (stp < 1 ? String (t, 2) : String ((int) std::lround (t))) + "s";
                    drawText (g, lab, { x - 30, H - 20, 60, 11 }, Justification::centred, rf, Colours::white.withAlpha (.4f));
                }
            }
        }
        if (hasPh && rs >= 0)
        {
            const String tag = "ROLL " + String (subs()[(size_t) rs].name);
            const auto f = cond (12);
            const float tw = GlyphArrangement::getStringWidth (f, tag) + 18;
            auto r = Rectangle<float> (W - 10 - tw, 8, tw, 22);
            g.setColour (vc::col::cy); g.fillRoundedRectangle (r, 4);
            drawText (g, tag, r, Justification::centred, f, c::ink);
        }
        if (hoverX >= 0 && drag.type < 0)
        {
            g.setColour (Colours::white.withAlpha (.12f));
            g.fillRect (hoverX, TOP - 6, 1.0f, H - TOP - BOT + 12);
        }
        if (sel >= 0 && sel < (int) snap.chops.size())
        {
            auto r = chipRect();
            g.setColour (c::yel); g.fillRoundedRectangle (r, 4);
            drawText (g, chipText (sel), { r.getX() + 9, r.getY(), r.getWidth() - 30, r.getHeight() }, Justification::centredLeft, cond (12), c::ink);
            auto x = Rectangle<float> (r.getRight() - 23, r.getY() + 3, 18, 18);
            g.setColour (Colours::black.withAlpha (.15f)); g.fillRoundedRectangle (x, 3);
            drawText (g, U8 ("\xc3\x97"), x, Justification::centred, font (12), c::ink);
        }
    }

    // sombra interior del cristal
    {
        ColourGradient sg (Colours::black.withAlpha (.7f), 0, 0, Colours::transparentBlack, 0, 10, false);
        g.setGradientFill (sg);
        g.fillRect (0.0f, 0.0f, W, 10.0f);
    }
    if (dragHint)
    {
        g.setColour (Colour (36, 72, 230).withAlpha (.55f));
        g.fillAll();
        drawText (g, U8 ("Suelta aqu\xc3\xad tu audio (.wav \xc2\xb7 .mp3 \xc2\xb7 .aiff \xc2\xb7 .flac)"), getLocalBounds().toFloat(),
                  Justification::centred, cond (15), Colours::white);
    }
}

void WaveView::mouseDown (const MouseEvent& e)
{
    if (! snap.sample) return;
    const float x = e.position.x;
    if (proc.selected >= 0)
    {
        auto r = chipRect();
        if (Rectangle<float> (r.getRight() - 24, r.getY(), 24, r.getHeight()).contains (e.position))
        {
            proc.selected = -1;
            if (onSelectionChanged) onSelectionChanged();
            return;
        }
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
    waveIco = icon (R"(<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#111111" stroke-width="2.8" stroke-linecap="round"><path d="M3 12h3l2-6 4 12 3-9 2 3h4"/></svg>)");
    dlIco = icon (R"(<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#ffe23a" stroke-width="2.8" stroke-linecap="round"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>)");
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
    auto b = getLocalBounds().toFloat().reduced (2).withTrimmedBottom (4);
    const float off = down ? 3.0f : 0.0f;
    g.setColour (Colours::black.withAlpha (.4f));
    g.fillRoundedRectangle (b.translated (0, down ? 1.0f : 4.0f).expanded (2), 7);
    auto r = b.translated (0, off);
    g.setColour (c::ink); g.fillRoundedRectangle (r.expanded (2), 7.5f);
    g.setColour (ready ? c::yel : c::blk); g.fillRoundedRectangle (r, 6);
    g.setColour (Colours::white.withAlpha (.12f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 5, r.getRight() - 5);

    auto dot = Rectangle<float> (r.getCentreX() - 17, r.getY() + 16, 34, 34);
    g.setColour (ready ? c::ink : c::yel);
    g.fillEllipse (dot);
    if (busy)
    {
        Path ring;
        ring.addCentredArc (dot.getCentreX(), dot.getCentreY(), 21, 21, 0, 0, MathConstants<float>::twoPi * std::max (.01f, prog->load()), true);
        g.setColour (Colours::white);
        g.strokePath (ring, PathStrokeType (3, PathStrokeType::curved, PathStrokeType::rounded));
    }
    if (auto* ic = ready ? dlIco.get() : waveIco.get()) ic->drawAt (g, dot.getCentreX() - 9, dot.getCentreY() - 9, 1.0f);
    const String t = ready ? U8 ("ARR\xc3\x81STRAME") : busy ? String ("RENDER...") : String ("RENDER FILE");
    drawText (g, t, { r.getX(), dot.getBottom() + 6, r.getWidth(), 22 }, Justification::centred, cond (15), ready ? c::ink : Colours::white);
}

void RenderButton::mouseDown (const MouseEvent&) { dragged = false; down = true; repaint(); }

void RenderButton::mouseDrag (const MouseEvent& e)
{
    if (! ready || dragged || e.getDistanceFromDragStart() < 6) return;
    dragged = true;
    down = false;
    repaint();
    DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void RenderButton::mouseUp (const MouseEvent& e)
{
    down = false;
    repaint();
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
        int nb = 0;
        auto f = pr->renderToFile ([p] (float v) { p->store (v); }, nb);
        MessageManager::callAsync ([this, al, f, nb]
        {
            if (! *al) return;
            busy = false;
            if (f.existsAsFile())
            {
                file = f;
                ready = true;
                bars = nb;
                if (toast) toast (U8 ("Render listo (") + String (nb) + U8 (" compases): arr\xc3\xa1stralo a una pista o haz clic para guardarlo"));
            }
            else if (toast) toast (U8 ("No he podido renderizar"));
            repaint();
        });
    });
}

// ===================================================================== Lcd
void Lcd::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (3);
    g.setColour (c::ink); g.fillRoundedRectangle (b.expanded (3), 8);
    g.setColour (Colour (0xff0d0e10)); g.fillRoundedRectangle (b, 6);
    drawText (g, String (proc.currentBpm.load(), 1), { b.getX(), b.getY() + 6, b.getWidth(), 28 }, Justification::centred, mono (24, true), c::yel);
    const bool host = proc.hostTempo.load();
    auto snap = proc.snapshot();
    const String l1 = host ? U8 ("BPM \xc2\xb7 HOST") : String ("BPM");
    const String l2 = snap.isHit && snap.srcBpm > 0 ? ("LOOP ~" + String ((int) std::lround (snap.srcBpm)) + (proc.pv ("fit") > .5f ? U8 (" \xc2\xb7 FIT") : String()))
                                                    : String ("MODO VOCAL");
    drawText (g, l1, { b.getX(), b.getY() + 34, b.getWidth(), 10 }, Justification::centred, mono (8.5f, true), Colours::white.withAlpha (.55f));
    drawText (g, l2, { b.getX(), b.getY() + 44, b.getWidth(), 10 }, Justification::centred, mono (8.5f, true), Colours::white.withAlpha (.55f));
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
    const auto f = cond (13);
    const float tw = std::min ((float) getWidth() - 20, GlyphArrangement::getStringWidth (f, msg) + 26);
    auto r = Rectangle<float> ((getWidth() - tw) / 2, (getHeight() - 26) / 2.0f, tw, 26);
    g.setColour (c::ink.withAlpha (alpha)); g.fillRoundedRectangle (r.expanded (2), 6);
    g.setColour (c::yel.withAlpha (alpha)); g.fillRoundedRectangle (r, 5);
    drawText (g, msg, r, Justification::centred, f, c::ink.withAlpha (alpha));
}
} // namespace ui
