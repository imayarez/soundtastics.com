#pragma once
#include "UI.h"

// Panel fijo de 980x660 (tamano del prototipo); la ventana lo escala.
struct Panel : public juce::Component
{
    std::function<void (juce::Graphics&)> painter;
    void paint (juce::Graphics& g) override { if (painter) painter (g); }
};

class VocalChopperEditor : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit VocalChopperEditor (VocalChopperProcessor&);
    ~VocalChopperEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void toast (const juce::String& m) { toastC.show (m); }

    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { wave.dragHint = true; }
    void fileDragExit (const juce::StringArray&) override { wave.dragHint = false; }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    void paintPanel (juce::Graphics&);
    void setupKnobs();
    void refreshButtons();
    void afterLoad (const juce::String& msg);
    juce::String knobText (int k);
    void showSettings (bool show);
    void showLibrary (bool show);
    void pickDirection (int i);

    VocalChopperProcessor& proc;
    ui::Look look;
    Panel panel;
    ui::BlackButton bLib, bImp, bPlay, bStop, bRec, bAjustes, bLatch;
    std::array<std::unique_ptr<ui::BlackButton>, 6> modeBtn;
    std::array<std::unique_ptr<ui::BlackButton>, 4> dirBtn;
    ui::Lcd lcd;
    ui::RenderButton render;
    ui::WaveView wave;
    ui::SettingsOverlay setOv;
    ui::LibraryOverlay libOv;
    std::array<std::unique_ptr<ui::Knob>, 7> knobs; // stretch, pitch, size, artifacts, blur, in, out
    ui::Toast toastC;
    juce::TooltipWindow tips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;

    std::array<juce::String, 7> lastText;
    std::array<float, 7> lastVal {};
    int lastSelSerial = 0;
    bool lastLim = false;
    juce::String heldKeys;
    static const juce::String keyMap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalChopperEditor)
};
