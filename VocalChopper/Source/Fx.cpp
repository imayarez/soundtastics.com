#include "Fx.h"

namespace vc
{
static constexpr int kRing = 8192;

void FxChain::prepare (double s, int maxBlock)
{
    sr = s > 0 ? s : 44100;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) std::max (1, maxBlock), 2 };
    hp.prepare (spec); lp.prepare (spec);
    hp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    hp.setResonance (0.9f); lp.setResonance (0.9f);

    for (int c = 0; c < 2; ++c)
    {
        wowBuf[c].assign ((size_t) (sr * 0.05) + 8, 0.0f);
        crRing[c].assign (kRing, 0.0f);
    }
    dl.assign ((size_t) (sr * 2.1) + 8, 0.0f);
    dr.assign ((size_t) (sr * 2.1) + 8, 0.0f);
    haas.assign ((size_t) (sr * 0.05) + 8, 0.0f);
    wetL.assign ((size_t) maxBlock, 0.0f); wetR.assign ((size_t) maxBlock, 0.0f);
    sendL.assign ((size_t) maxBlock, 0.0f); sendR.assign ((size_t) maxBlock, 0.0f);

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.88f; rp.damping = 0.45f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
    reverb.setParameters (rp);
    reverb.setSampleRate (sr);

    for (auto* sv : { &inG, &outG, &tapeG, &revG, &dlyG, &fbG, &sendG, &wetOn, &deessGain })
        sv->reset (sr, 0.03);
    for (auto* sv : { &lpHz, &hpHz, &wowDepth, &haasSec, &dlySec })
        sv->reset (sr, 0.05);
    inG.setCurrentAndTargetValue (1); outG.setCurrentAndTargetValue (1); tapeG.setCurrentAndTargetValue (1);
    lpHz.setCurrentAndTargetValue (20000); hpHz.setCurrentAndTargetValue (20);
    dlySec.setCurrentAndTargetValue (0.375f); haasSec.setCurrentAndTargetValue (0.001f);
    lastDeess = lastFormant = 999; lastTapeLP = -1;
    hiDet.coefficients = Coefs::makeHighPass (sr, 6000.0f);
    reset();
}

void FxChain::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        deess[c].reset(); fHi[c].reset(); fLo[c].reset(); tapeLP[c].reset();
        std::fill (wowBuf[c].begin(), wowBuf[c].end(), 0.0f);
        std::fill (crRing[c].begin(), crRing[c].end(), 0.0f);
    }
    hiDet.reset();
    hp.reset(); lp.reset();
    reverb.reset();
    std::fill (dl.begin(), dl.end(), 0.0f);
    std::fill (dr.begin(), dr.end(), 0.0f);
    std::fill (haas.begin(), haas.end(), 0.0f);
    lvl = hiEnv = allEnv = limEnv = 0;
    crLeft = 0;
}

static inline float shapeTanh (float x, float k, float asym)
{
    if (k <= 0.01f) return x;
    x = juce::jlimit (-1.0f, 1.0f, x); // como la WaveShaper del navegador
    return std::tanh ((x + asym * x * x) * (1 + k)) / std::tanh (1 + k);
}

void FxChain::process (float* L, float* R, int n, const FxParams& p)
{
    if (n <= 0) return;
    if ((size_t) n > wetL.size())
    {
        // bloques mas grandes de lo anunciado: procesar en trozos
        int done = 0;
        while (done < n) { const int m = std::min ((int) wetL.size(), n - done); process (L + done, R + done, m, p); done += m; }
        return;
    }

    // ---------------- objetivos del bloque (applyAll)
    const float iv = (float) p.in;
    inG.setTargetValue (juce::Decibels::decibelsToGain ((float) inDb (iv)));
    const float clipK = iv > .5f ? (iv - .5f) * 2 * 4 : 0.0f;

    const float fs = (float) ((p.formant - .5) * 24);
    if (std::abs (fs - lastFormant) > 0.05f)
    {
        lastFormant = fs;
        auto hi = Coefs::makeHighShelf (sr, 1800.0f, 0.707f, juce::Decibels::decibelsToGain (fs * .7f));
        auto lo = Coefs::makeLowShelf (sr, 320.0f, 0.707f, juce::Decibels::decibelsToGain (-fs * .4f));
        for (int c = 0; c < 2; ++c) { fHi[c].coefficients = hi; fLo[c].coefficients = lo; }
    }

    const float d = (float) p.dj, nyq = (float) (sr * 0.45);
    lpHz.setTargetValue (std::min (nyq, d < -.02f ? 20000.0f * std::pow (180.0f / 20000.0f, -d) : 20000.0f));
    hpHz.setTargetValue (d > .02f ? 20.0f * std::pow (4000.0f / 20.0f, d) : 20.0f);

    const float a = (float) p.artifacts, t = std::min (1.0f, a / .5f), cr = std::max (0.0f, (a - .5f) / .5f);
    const float tapeK = t * 5, tapeAsym = t * .15f;
    tapeG.setTargetValue (1.0f / (1 + t * .45f));
    const float tlp = std::min (nyq, 20000.0f - t * 10500.0f);
    if (std::abs (tlp - lastTapeLP) > 20.0f)
    {
        lastTapeLP = tlp;
        auto c = Coefs::makeLowPass (sr, tlp, 0.5f);
        tapeLP[0].coefficients = c; tapeLP[1].coefficients = c;
    }
    wowDepth.setTargetValue (t * .0007f);
    const float bits = cr > 0 ? 16 - cr * 12 : 16;
    const int ds = cr > 0 ? 1 + (int) std::lround (cr * cr * 14) : 1;
    const float gl = cr > .25f ? (cr - .25f) * .12f : 0.0f;
    const bool crushOn = bits < 15.9f || ds > 1 || gl > 0;
    const float q = std::pow (2.0f, bits - 1);

    const float b = (float) p.blur, beat = (float) (60.0 / std::max (20.0, p.bpm));
    revG.setTargetValue (b * 1.1f);
    dlyG.setTargetValue (b * .55f);
    fbG.setTargetValue (.18f + b * .38f);
    dlySec.setTargetValue (std::min (2.0f, beat * .75f));
    haasSec.setTargetValue (.001f + b * .016f);
    wetOn.setTargetValue (b > 0 ? 1.0f : 0.0f);
    outG.setTargetValue (std::pow ((float) p.out / .75f, 2.0f));

    // de-esser ligado al PITCH (autoProc)
    const int up = std::max (0, globalSemis (p.pitch));
    const float ratio = allEnv > 1e-6f ? hiEnv / allEnv : 0.0f;
    const float dsg = -std::min (12.0f, up * .45f + (up > 0 ? std::max (0.0f, ratio - .18f) * 40 : 0.0f));
    if (std::abs (dsg - lastDeess) > 0.1f)
    {
        lastDeess = dsg;
        auto c = Coefs::makeHighShelf (sr, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (dsg));
        deess[0].coefficients = c; deess[1].coefficients = c;
    }
    // ducking de la cola de BLUR
    sendG.setTargetValue (b * (1 - .65f * juce::jlimit (0.0f, 1.0f, lvl * 5)));

    const int wowN = (int) wowBuf[0].size();
    const int dN = (int) dl.size(), hN = (int) haas.size();
    double sumSq = 0, hiSq = 0;

    // ---------------- cadena seca hasta 'pre'
    for (int i = 0; i < n; ++i)
    {
        float x[2] = { L[i], R[i] };
        const float ig = inG.getNextValue();
        const float lpf = lpHz.getNextValue(), hpf = hpHz.getNextValue();
        if ((i & 15) == 0) { lp.setCutoffFrequency (lpf); hp.setCutoffFrequency (hpf); }
        const float tg = tapeG.getNextValue();
        const float wd = wowDepth.getNextValue();
        lfoPh += 0.8 / sr; if (lfoPh >= 1) lfoPh -= 1;
        const float wowSec = .004f + std::sin ((float) (2 * kPi * lfoPh)) * wd;

        if (crushOn && (crBlock++ & 127) == 0)
        {
            if (crLeft > 0) --crLeft;
            if (crLeft <= 0 && rnd.nextFloat() < gl)
            {
                crLeft = 2 + rnd.nextInt (5);
                crLen = 64 * (1 + rnd.nextInt (10));
                crSt = crW; crPos = 0;
            }
        }
        if (crushOn && crC <= 0) crC = ds;

        for (int c = 0; c < 2; ++c)
        {
            float y = x[c] * ig;
            y = shapeTanh (y, clipK, 0);
            y = deess[c].processSample (y);
            y = fHi[c].processSample (y);
            y = fLo[c].processSample (y);
            y = hp.processSample (c, y);
            y = lp.processSample (c, y);
            y = shapeTanh (y, tapeK, tapeAsym) * tg;
            y = tapeLP[c].processSample (y);
            // wow
            wowBuf[c][(size_t) wowW] = y;
            {
                const double rp = wowW - wowSec * sr;
                double rr = rp < 0 ? rp + wowN : rp;
                const int i0 = (int) rr; const float fr = (float) (rr - i0);
                const float a0 = wowBuf[c][(size_t) (i0 % wowN)], a1 = wowBuf[c][(size_t) ((i0 + 1) % wowN)];
                y = a0 + (a1 - a0) * fr;
            }
            // bitcrusher / downsample / micro-glitch
            if (crushOn)
            {
                if (crC == ds) crHold[c] = std::round (y * q) / q;
                y = crHold[c];
                crRing[c][(size_t) crW] = y;
                if (crLeft > 0) y = crRing[c][(size_t) ((crSt - crLen + crPos + kRing) % kRing)];
            }
            x[c] = y;
        }
        if (crushOn)
        {
            --crC;
            if (crLeft > 0) crPos = (crPos + 1) % std::max (1, crLen);
            crW = (crW + 1) % kRing;
        }
        wowW = (wowW + 1) % wowN;

        L[i] = x[0]; R[i] = x[1];
        const float m = (x[0] + x[1]) * .5f;
        sumSq += m * m;
        const float h = hiDet.processSample (m);
        hiSq += h * h;
        const float sg = sendG.getNextValue();
        sendL[(size_t) i] = x[0] * sg;
        sendR[(size_t) i] = x[1] * sg;
    }

    // nivel (para ducking, de-esser y la pantalla)
    {
        const float rms = (float) std::sqrt (sumSq / n);
        const float al = 1.0f - std::pow (.6f, (float) n / (float) (sr * .03));
        lvl += (rms - lvl) * al;
        const float bl = 1.0f - std::exp (-(float) n / (float) (sr * .05));
        allEnv += ((float) std::sqrt (sumSq / n) - allEnv) * bl;
        hiEnv += ((float) std::sqrt (hiSq / n) - hiEnv) * bl;
        level = lvl;
    }

    // ---------------- BLUR (en paralelo)
    std::copy (sendL.begin(), sendL.begin() + n, wetL.begin());
    std::copy (sendR.begin(), sendR.begin() + n, wetR.begin());
    reverb.processStereo (wetL.data(), wetR.data(), n);

    float red = 0;
    for (int i = 0; i < n; ++i)
    {
        const float rg = revG.getNextValue(), dg = dlyG.getNextValue(), fb = fbG.getNextValue();
        const float dly = dlySec.getNextValue(), hs = haasSec.getNextValue(), wo = wetOn.getNextValue();
        const float dsamp = dly * (float) sr;
        auto rd = [&] (const std::vector<float>& buf, int w, float del, int N)
        {
            double rp = w - del; while (rp < 0) rp += N;
            const int i0 = (int) rp; const float fr = (float) (rp - i0);
            const float a0 = buf[(size_t) (i0 % N)], a1 = buf[(size_t) ((i0 + 1) % N)];
            return a0 + (a1 - a0) * fr;
        };
        const float dlOut = rd (dl, dW, dsamp, dN);
        const float drOut = rd (dr, dW, dsamp, dN);
        dl[(size_t) dW] = (sendL[(size_t) i] + sendR[(size_t) i]) * .5f + drOut * fb;
        dr[(size_t) dW] = dlOut;
        dW = (dW + 1) % dN;

        const float wl = wetL[(size_t) i] * rg + dlOut * dg;
        float wr = wetR[(size_t) i] * rg + drOut * dg;
        haas[(size_t) hW] = wr;
        wr = rd (haas, hW, hs * (float) sr, hN);
        hW = (hW + 1) % hN;

        const float og = outG.getNextValue();
        float yl = (L[i] + wl * wo) * og;
        float yr = (R[i] + wr * wo) * og;

        // limitador de seguridad (umbral -2 dB, ratio 20:1, ataque 1 ms, release 80 ms)
        const float pk = std::max (std::abs (yl), std::abs (yr));
        const float att = 1.0f - std::exp (-1.0f / (0.001f * (float) sr));
        const float rel = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));
        limEnv += (pk - limEnv) * (pk > limEnv ? att : rel);
        const float envDb = juce::Decibels::gainToDecibels (limEnv, -120.0f);
        const float grDb = envDb > -2.0f ? (-2.0f - envDb) * (1.0f - 1.0f / 20.0f) : 0.0f;
        red = std::min (red, grDb);
        const float g = juce::Decibels::decibelsToGain (grDb);
        yl *= g; yr *= g;
        // red de seguridad final: curva suave por encima de -1 dB, nunca pasa de 0 dB
        auto safe = [] (float y)
        {
            const float a = std::abs (y);
            if (a <= 0.89f) return y;
            return std::copysign (0.89f + 0.11f * std::tanh ((a - 0.89f) / 0.11f), y);
        };
        L[i] = safe (yl);
        R[i] = safe (yr);
    }
    limiterReductionDb = red;
}
} // namespace vc
