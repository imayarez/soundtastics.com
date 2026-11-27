#pragma once
#include "Common.h"
#include "PluginProcessor.h"

#define U8(x) juce::String::fromUTF8 (x)

namespace ui
{
// ---------------------------------------------------------------- colores (familia SFX, cuerpo azul)
namespace c
{
    const juce::Colour body  { 0xff2448e6 };
    const juce::Colour mint  { 0xff46e0b4 };
    const juce::Colour cyan  { 0xff38d3ec };
    const juce::Colour pink  { 0xffff7f9e };
    const juce::Colour blk   { 0xff232428 };
    const juce::Colour yel   { 0xffffe23a };
    const juce::Colour ink   { 0xff111111 };
    const juce::Colour cream { 0xfffff6c4 };
    const juce::Colour screen{ 0xff08090b };
    const juce::Colour rec   { 0xffff4d5a };
}

// ---------------------------------------------------------------- letras (incluidas en el plugin)
juce::Font heavy (float px);                 // Archivo Black: titulo y etiquetas rosas
juce::Font cond (float px);                  // Oswald SemiBold: botones y rotulos
juce::Font mono (float px, bool bold = false);
juce::Font font (float px, bool bold = false, float kern = 0.0f); // sistema, textos largos
std::unique_ptr<juce::Drawable> icon (const juce::String& svg);
void drawText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, const juce::Font&, juce::Colour);
juce::Image lettering();                     // "by soundtastics" oficial

// ---------------------------------------------------------------- boton negro (amarillo cuando esta activo)
class BlackButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BlackButton (const juce::String& text = {}, const juce::String& svg = {}, float textPx = 13.0f);
    std::function<void()> onClick;
    juce::String text;
    bool on = false;
    juce::Colour onColour = c::yel;
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (isEnabled()) { down = true; repaint(); } }
    void mouseUp (const juce::MouseEvent&) override;
private:
    std::unique_ptr<juce::Drawable> icoW, icoK;
    float px;
    bool down = false;
};

// ---------------------------------------------------------------- mando rojo con escala impresa
class Knob : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Knob (bool big) : isBig (big) {}
    std::function<float (bool alt)> getValue;
    std::function<void (float, bool alt)> setValue;
    std::function<void (bool begin, bool alt)> gesture;
    std::function<void()> onReset;
    bool chopMode = false, altHeld = false;
    // zona del mando dentro del componente (deja sitio a la escala)
    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onReset) onReset(); repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    bool isBig;
    float y0 = 0, v0 = 0;
    bool alt = false;
};

// ---------------------------------------------------------------- segmentos (ajustes)
class Seg : public juce::Component
{
public:
    juce::StringArray labels;
    int selectedIndex = 0;
    std::function<void (int)> onPick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
};

class CloseButton : public juce::Component
{
public:
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
};

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return cond (12); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return cond (13); }
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int, int) override;
};

// ---------------------------------------------------------------- overlay de ajustes (dentro de la pantalla)
class SettingsOverlay : public juce::Component
{
public:
    explicit SettingsOverlay (VocalChopperProcessor&);
    std::function<void()> onClose;
    std::function<void (const juce::String&)> toast;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    VocalChopperProcessor& proc;
    CloseButton close;
    Seg det, key, fit, quant, lock;
    juce::Slider sens, dj;
    juce::ComboBox root, scale;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensA, djA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootA, scaleA;
};

// ---------------------------------------------------------------- overlay de libreria
class LibraryOverlay : public juce::Component
{
public:
    explicit LibraryOverlay (VocalChopperProcessor& p) : proc (p) { addAndMakeVisible (close); }
    std::function<void (int)> onPick;
    void paint (juce::Graphics&) override;
    void resized() override { close.setBounds (getWidth() - 34, 6, 26, 26); }
    void mouseMove (const juce::MouseEvent& e) override { hover = itemAt (e.position); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { const int i = itemAt (e.position); if (i >= 0 && onPick) onPick (i); }
    CloseButton close;
private:
    int itemAt (juce::Point<float>) const;
    juce::Rectangle<float> itemRect (int i) const;
    VocalChopperProcessor& proc;
    int hover = -1;
};

// ---------------------------------------------------------------- pantalla de la onda
class WaveView : public juce::Component
{
public:
    explicit WaveView (VocalChopperProcessor&);
    std::function<void (const juce::String&)> toast;
    std::function<void()> onSelectionChanged;
    void paint (juce::Graphics&) override;
    void resized() override { version = -1; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverX = -1; }
    void tick();
    bool dragHint = false;
private:
    void buildLayer (const vc::SampleData&);
    float X (double t) const;
    double Tm (float x) const;
    struct Hit { int type; int i; }; // 0 = A, 1 = B, 2 = linea, 3 = chop
    Hit hitTest (float x) const;
    juce::Rectangle<float> chipRect() const;

    VocalChopperProcessor& proc;
    ContentSnapshot snap;
    int version = -1;
    juce::Image layer, blurLayer;
    std::vector<float> trail;
    float hoverX = -1;
    Hit drag { -1, -1 };
    int pressed = -1;
    double dur = 1;
    juce::Random rnd;
    float W = 648, H = 206;
    static constexpr float PADX = 12, TOP = 26, BOT = 22;
};

// ---------------------------------------------------------------- RENDER FILE
class RenderButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RenderButton (VocalChopperProcessor&);
    ~RenderButton() override;
    std::function<void (const juce::String&)> toast;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void reset();
    void tick() { if (busy) repaint(); }
private:
    VocalChopperProcessor& proc;
    std::shared_ptr<std::atomic<float>> prog = std::make_shared<std::atomic<float>> (0.0f);
    bool busy = false, ready = false, dragged = false, down = false;
    int bars = 0;
    juce::File file;
    std::unique_ptr<juce::Drawable> waveIco, dlIco;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

// ---------------------------------------------------------------- pantallita de tempo
class Lcd : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Lcd (VocalChopperProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { y0 = (float) e.y; b0 = proc.pv ("bpm"); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    VocalChopperProcessor& proc;
    float y0 = 0, b0 = 120;
};

// ---------------------------------------------------------------- aviso flotante
class Toast : public juce::Component, private juce::Timer
{
public:
    Toast() { setInterceptsMouseClicks (false, false); }
    void show (const juce::String& m);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::String msg;
    float alpha = 0;
    int ticks = 0;
};
} // namespace ui
