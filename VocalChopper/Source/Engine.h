#pragma once
#include "Common.h"

namespace vc
{
struct Chop
{
    double s = 0, e = 0; // segundos dentro del audio
    int pitch = 0;       // semitonos propios del chop
    int dir = 0;         // 0 = global, 1 = FORWARD, 2 = REVERSE
};

struct SampleData
{
    juce::AudioBuffer<float> buf; // siempre 2 canales
    double sr = 44100.0;
    double duration() const { return buf.getNumSamples() / sr; }
};

// Valores de los mandos que el motor necesita, copiados al principio de cada bloque
struct EngineParams
{
    double stretch = .5, pitch = .5, size = .4;
    int preset = 0, dir = 0;
    bool latch = false, keyLock = false, fit = true;
    int scale = 0; // 0 mayor, 1 menor
    int quant = 16; // 0, 16, 8, 4
    double bpm = 120;
};

// Motor de chops: secuenciador por semicorcheas, flujo por granos y rolls (port de doStep() del HTML).
class Engine
{
public:
    void prepare (double hostSampleRate);

    // ---- contenido (protegido por 'lock')
    juce::SpinLock lock;
    std::shared_ptr<const SampleData> sample;
    std::vector<Chop> chops;
    double A = 0, B = 1, srcBpm = 0;
    bool isHit = false;

    EngineParams prm;

    // ---- transporte (hilo de audio)
    void start (double stepPos = -1.0); // stepPos = posicion del DAW en semicorcheas (-1 = desde cero)
    void stop();
    void followHost (double stepPos);   // corrige deriva / saltos de loop del DAW
    bool isPlaying() const { return playing; }
    void press (int ci);
    void release (int ci);
    void latchOff();
    void resetRoll() { roll.active = false; }
    void killAll (bool hard = false);

    // Genera audio sumando sobre L/R (deben venir limpios). Requiere tener 'lock'.
    void render (float* L, float* R, int n);

    // ---- informacion para la pantalla
    std::atomic<double> uiPlayhead { -1.0 };
    std::atomic<int> uiRollSub { -1 };
    std::atomic<double> uiRollA { 0 }, uiRollB { 0 };
    std::atomic<int> uiSelected { -1 };
    std::atomic<int> uiSelectSerial { 0 };

    double speed() const;
    double rateFor (int ci) const;
    int chopAt (double p) const;

private:
    struct Voice
    {
        bool on = false;
        juce::int64 startAt = 0, age = 0, len = 0;
        double pos = 0, inc = 0;
        int fi = 1, fo = 1;
        float gain = 1;
        bool loop = false;
        double lo = 0, hi = 0;
        int relLeft = -1, relTotal = 1;
        bool shot = false;
        int chop = -1;
    };
    struct PosInfo { double p; int dir; int ci; double local; bool hasLocal; };
    struct Roll { bool active = false; juce::int64 key = 0; double t0 = 0, p = 0, sub = 0; int dir = 1, ci = 0; };
    struct Manual { bool active = false; int id = 0, ci = 0; juce::int64 start = 0, end = -1; };

    double stepDur() const { return 60.0 / prm.bpm / 4.0; }
    int snapScale (int st) const;
    std::vector<int> regionChops() const;
    PosInfo posAt (double e, juce::int64 k, double dt) const;
    bool presetRoll (juce::int64 k, juce::int64& key, double& sub) const;
    void doStep (double t, juce::int64 k);
    void grain (double tg, PosInfo ps);
    void seg (double t, double p, int dir, double dur, double rate, double gain, double fi, double fo);
    Voice* allocVoice();
    void oneShot (int ci);
    void stopOneShot();
    void updateUi();

    double hostSR = 44100.0;
    juce::int64 clockS = 0;
    double nowSec() const { return clockS / hostSR; }

    bool playing = false;
    juce::int64 stepIdx = 0;
    double nextStepTime = 0, nextGrain = 0, E = 0;
    double lastStepT = 0, lastStepE = 0, lastSp = 1;
    juce::int64 lastStepK = 0;
    Roll roll;
    double rollD = 0;
    Manual manual;
    int mid = 0;

    static constexpr int kVoices = 64;
    std::array<Voice, kVoices> voices;

    static constexpr double GR = .06, HOP = .03;
};
} // namespace vc
