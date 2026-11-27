#pragma once
#include "Common.h"
#include "PluginProcessor.h"

#define U8(x) juce::String::fromUTF8 (x)

namespace ui
{
juce::Font font (float cssPx, bool bold = false, float kern = 0.0f);
juce::Font mono (float cssPx, bool bold = false);
std::unique_ptr<juce::Drawable> icon (const juce::String& svg);
void drawText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification, const juce::Font&, juce::Colour);
void innerShadow (juce::Graphics&, juce::Rectangle<float>, float radius, float depth, float alpha);

// ------------------------------------------------ boton de hardware azul
class HwButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { blue, round, render };
    HwButton (const juce::String& svg, Style s = blue);
    std::function<void()> onClick;
    bool led = false, ledRed = false, hasLed = false;
    void setIcon (const juce::String& svg) { ico = icon (svg); repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override;
protected:
    std::unique_ptr<juce::Drawable> ico;
    Style style;
    bool down = false;
};

// ------------------------------------------------ mando giratorio
class Knob : public juce::Component, public juce::SettableTooltipClient
{
public:
    Knob (bool small) : isSmall (small) {}
    std::function<float (bool alt)> getValue;         // 0..1
    std::function<void (float, bool alt)> setValue;  // alt = Alt/Shift
    std::function<void (bool begin, bool alt)> gesture;
    std::function<float()> getArc2;                  // <0 = sin segundo arco
    std::function<void()> onReset;
    bool bipolar = false, chopMode = false;
    bool altHeld = false;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onReset) onReset(); repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    bool isSmall;
    float y0 = 0, v0 = 0;
    bool alt = false;
};

// ------------------------------------------------ lista desplegable (menus de los selectores)
class MenuList : public juce::Component
{
public:
    struct Item { juce::String name, info; bool on; };
    std::vector<Item> items;
    std::function<void (int)> onPick;
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override { hover = rowAt (e.y); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { const int r = rowAt (e.y); if (r >= 0 && onPick) onPick (r); }
    int idealHeight() const;
private:
    int rowAt (int y) const;
    int rowH (int i) const { return items[(size_t) i].info.isEmpty() ? 31 : 44; }
    int hover = -1;
};

// ------------------------------------------------ selector inferior (< STRAIGHT >)
class Selector : public juce::Component
{
public:
    Selector (const juce::String& iconSvg);
    std::function<void()> onIcon, onName, onPrev, onNext;
    juce::String text;
    bool iconOn = false;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    std::unique_ptr<juce::Drawable> ico, icoOn, chev;
};

// ------------------------------------------------ segmentos (AUTO | VOCAL | GOLPE)
class Seg : public juce::Component
{
public:
    juce::StringArray labels;
    int selectedIndex = 0;
    std::function<void (int)> onPick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
};

// ------------------------------------------------ boton de cerrar (x)
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
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return font (11, true); }
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int, int) override;
};

// ------------------------------------------------ overlay de ajustes del motor
class SettingsOverlay : public juce::Component
{
public:
    SettingsOverlay (VocalChopperProcessor&);
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

// ------------------------------------------------ overlay de libreria
class LibraryOverlay : public juce::Component
{
public:
    LibraryOverlay (VocalChopperProcessor& p) : proc (p) { addAndMakeVisible (close); }
    std::function<void()> onClose;
    std::function<void (int)> onPick;
    void paint (juce::Graphics&) override;
    void resized() override { close.setBounds (getWidth() - 42, 10, 30, 30); }
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

// ------------------------------------------------ visor central de onda
class WaveView : public juce::Component
{
public:
    explicit WaveView (VocalChopperProcessor&);
    std::function<void (const juce::String&)> toast;
    std::function<void()> onSelectionChanged;
    void paint (juce::Graphics&) override;
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
};

// ------------------------------------------------ boton RENDER FILE
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
    bool busy = false;
    bool ready = false, dragged = false;
    int bars = 0;
    juce::File file;
    std::unique_ptr<juce::Drawable> waveIco, dlIco;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

// ------------------------------------------------ pantalla de tempo
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

// ------------------------------------------------ mensajito flotante
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
