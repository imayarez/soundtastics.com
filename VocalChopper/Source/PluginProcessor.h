#pragma once
#include "Common.h"
#include "Engine.h"
#include "Fx.h"
#include "Analysis.h"
#include "Factory.h"

// Instantanea del contenido para dibujar la pantalla
struct ContentSnapshot
{
    std::shared_ptr<const vc::SampleData> sample;
    std::vector<vc::Chop> chops;
    double A = 0, B = 1, srcBpm = 0;
    bool isHit = false;
};

class VocalChopperProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
{
public:
    VocalChopperProcessor();
    ~VocalChopperProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vocal Chopper"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float pv (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const char* id, float plainValue);

    // ---- acciones desde la interfaz (hilo de mensajes)
    void togglePlay();
    void stopAll();
    void pressChop (int ci);
    void releaseChop (int ci);
    bool loadFile (const juce::File& f, juce::String& message);
    void loadPatch (int index, juce::String& message);
    juce::String redetect();
    bool toggleRecord (juce::String& message);
    bool isRecording() const { return recording.load(); }

    // edicion de cortes
    ContentSnapshot snapshot();
    void moveLine (int i, double t);
    void setMarker (bool start, double t);
    bool addCut (double t);
    void removeLine (int i);
    void setChopPitch (int ci, int semis);
    void setChopDir (int ci, int dir);
    int getChopPitch (int ci);
    int getChopDir (int ci);
    int numChops();

    // render
    juce::File renderToFile (std::function<void (float)> progress, int& barsOut);

    // ---- estado visible
    vc::Engine engine;
    vc::FxChain fx;
    std::atomic<bool> internalPlay { false }, hostPlaying { false }, enginePlaying { false }, hostTempo { false };
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<int> contentVersion { 0 };
    juce::String sampleName;
    int patchIndex = -1;
    int selected = -1;        // chop seleccionado (edicion individual)
    juce::String lastRenderPath;

private:
    void parameterChanged (const juce::String& id, float v) override;
    void handleAsyncUpdate() override;
    void setContent (std::shared_ptr<vc::SampleData> data, const std::vector<double>& cuts, bool isHit, double bpm,
                     double A, double B, const juce::String& name);
    void buildChops (const std::vector<double>& cuts, double dur);
    vc::EngineParams readEngineParams (double bpm) const;
    vc::FxParams readFxParams (double bpm) const;

    struct UiEvent { int type; int ci; };
    enum { evPress, evRelease, evPlay, evStop };
    juce::AbstractFifo fifo { 256 };
    std::array<UiEvent, 256> fifoData;
    void pushEvent (int type, int ci);

    bool lastHostPlaying = false, lastLatch = false;
    int lastPreset = -1, lastDir = -1;

    // grabacion
    std::atomic<bool> recording { false };
    juce::AudioBuffer<float> recBuf;
    std::atomic<int> recPos { 0 };
    double recSR = 44100;

    juce::AudioFormatManager formats;
    std::vector<float> monoTmp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalChopperProcessor)
};
