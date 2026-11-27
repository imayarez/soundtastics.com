#pragma once
#include "Common.h"

namespace vc
{
struct FxParams
{
    double in = .5, out = .75, formant = .5, artifacts = 0, blur = .15, dj = 0, pitch = .5, bpm = 120;
};

// Cadena de efectos (igual que build()/applyAll()/autoProc() del prototipo):
// IN + soft-clip > de-esser > formantes > filtro DJ > cinta + wow > bitcrusher/glitch
//   > (seco) + (BLUR: reverb + delay ping-pong 3/16 + Haas, con ducking) > OUT > limitador
class FxChain
{
public:
    void prepare (double sr, int maxBlock);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p);
    std::atomic<float> limiterReductionDb { 0 };
    std::atomic<float> level { 0 };

private:
    double sr = 44100;
    using Filt = juce::dsp::IIR::Filter<float>;
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    Filt deess[2], fHi[2], fLo[2], tapeLP[2], hiDet;
    juce::dsp::StateVariableTPTFilter<float> hp, lp;

    float lastDeess = 999, lastFormant = 999, lastTapeLP = -1;
    juce::SmoothedValue<float> inG, outG, tapeG, revG, dlyG, fbG, sendG, wetOn, deessGain;
    juce::SmoothedValue<float> lpHz, hpHz, wowDepth, haasSec, dlySec;

    // wow (delay modulado)
    std::vector<float> wowBuf[2];
    int wowW = 0;
    double lfoPh = 0;

    // bitcrusher + micro-glitch (port del AudioWorklet 'crush')
    float crHold[2] { 0, 0 };
    int crC = 0;
    std::vector<float> crRing[2];
    int crW = 0, crLeft = 0, crLen = 0, crPos = 0, crSt = 0, crBlock = 0;
    juce::Random rnd;

    // BLUR
    juce::Reverb reverb;
    std::vector<float> dl, dr, haas;
    int dW = 0, hW = 0;
    std::vector<float> wetL, wetR, sendL, sendR;

    // automaticos
    float lvl = 0, hiEnv = 0, allEnv = 0;
    float limEnv = 0;
};
}
