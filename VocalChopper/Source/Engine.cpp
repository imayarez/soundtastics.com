#include "Engine.h"

namespace vc
{
static const int kScales[2][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 } };

void Engine::prepare (double sr)
{
    hostSR = sr > 0 ? sr : 44100.0;
    killAll (true);
}

double Engine::speed() const
{
    double s = spd (prm.stretch);
    if (prm.fit && isHit && srcBpm > 0) s *= prm.bpm / srcBpm;
    return s;
}

int Engine::snapScale (int st) const
{
    if (! prm.keyLock) return st;
    const int* sc = kScales[prm.scale ? 1 : 0];
    int best = st, bd = 99;
    for (int d = -2; d <= 2; ++d)
    {
        const int c = st + d;
        bool in = false;
        for (int i = 0; i < 7; ++i) if (sc[i] == modi (c, 12)) in = true;
        if (in && std::abs (d) < bd) { bd = std::abs (d); best = c; }
    }
    return best;
}

double Engine::rateFor (int ci) const
{
    const int cp = (ci >= 0 && ci < (int) chops.size()) ? chops[(size_t) ci].pitch : 0;
    return std::pow (2.0, snapScale (globalSemis (prm.pitch) + cp) / 12.0);
}

int Engine::chopAt (double p) const
{
    for (size_t i = 0; i < chops.size(); ++i)
        if (p >= chops[i].s && p < chops[i].e) return (int) i;
    return -1;
}

// Chops dentro de la region A-B (si no hay ninguno, todos). Sin reservar memoria: devuelve el j-esimo.
static int regionPick (const std::vector<Chop>& c, double A, double B, uint32_t j)
{
    int n = 0;
    for (auto& ch : c) if (ch.e > A + .01 && ch.s < B - .01) ++n;
    if (n == 0) return c.empty() ? -1 : (int) (j % (uint32_t) c.size());
    int want = (int) (j % (uint32_t) n), k = 0;
    for (size_t i = 0; i < c.size(); ++i)
        if (c[i].e > A + .01 && c[i].s < B - .01) { if (k == want) return (int) i; ++k; }
    return 0;
}

Engine::PosInfo Engine::posAt (double e, juce::int64 k, double dt) const
{
    const bool seq = prm.preset == pChopSeq;
    if ((seq || prm.dir == dRandom) && ! chops.empty())
    {
        const juce::int64 j = k / 2;
        const int ci = seq ? regionPick (chops, A, B, (uint32_t) j)
                           : regionPick (chops, A, B, hash32 ((uint32_t) (j * 31 + 7)));
        const double local = ((k % 2) * stepDur() + dt) * speed();
        return { chops[(size_t) ci].s + local, 1, ci, local, true };
    }
    const double L = std::max (.02, B - A);
    if (prm.dir == dForward)  return { A + modp (e, L), 1, -1, 0, false };
    if (prm.dir == dReverse)  return { B - modp (e, L), -1, -1, 0, false };
    const double q = modp (e, 2 * L);
    return q < L ? PosInfo { A + q, 1, -1, 0, false } : PosInfo { B - (q - L), -1, -1, 0, false };
}

bool Engine::presetRoll (juce::int64 k, juce::int64& key, double& sub) const
{
    auto K = [] (int type, juce::int64 g) { return ((juce::int64) type << 48) | (g & 0xffffffffffffLL); };
    switch (prm.preset)
    {
        case pStutter:
        {
            const juce::int64 b = k % 16;
            if (b == 12 || b == 13) { key = K (1, k >> 4); sub = 1.0 / 16; return true; }
            if (b >= 14)            { key = K (2, k >> 4); sub = 1.0 / 32; return true; }
            return false;
        }
        case pBuildUp:
        {
            const juce::int64 b = k % 32;
            if (b < 8) return false;
            key = K (3, k >> 5);
            sub = b < 16 ? 1.0 / 8 : b < 24 ? 1.0 / 16 : b < 28 ? 1.0 / 32 : 1.0 / 64;
            return true;
        }
        case pTriplets:
        {
            const juce::int64 b = k % 8;
            if (b < 4) return false;
            key = K (4, k >> 3);
            sub = ((k >> 3) % 2) ? 1.0 / 24 : 1.0 / 12;
            return true;
        }
        case pGlitch:
        {
            const uint32_t r = hash32 ((uint32_t) (k * 7919 + 13));
            if (r % 5) return false;
            static const double s[4] = { 1.0 / 32, 1.0 / 64, 1.0 / 48, 1.0 / 24 };
            key = K (5, k);
            sub = s[r % 4];
            return true;
        }
        default: return false;
    }
}

// ------------------------------------------------------------ transporte
void Engine::start (double stepPos)
{
    if (! sample) return;
    stopOneShot();
    killAll();
    playing = true;
    roll.active = false;
    manual.active = false;
    const double sd = stepDur();
    if (stepPos >= 0)
    {
        const juce::int64 k = (juce::int64) std::ceil (stepPos - 1e-6);
        stepIdx = k;
        nextStepTime = nowSec() + (k - stepPos) * sd;
        E = k * sd * speed();
    }
    else
    {
        stepIdx = 0;
        nextStepTime = nowSec() + 0.005;
        E = 0;
    }
    nextGrain = nextStepTime;
    lastStepT = nextStepTime; lastStepE = E; lastStepK = stepIdx; lastSp = speed();
}

void Engine::followHost (double stepPos)
{
    if (! playing) return;
    const double sd = stepDur();
    const double ours = stepIdx - (nextStepTime - nowSec()) / sd;
    if (std::abs (ours - stepPos) > 0.25)
    {
        // salto (loop del DAW, el usuario ha movido el cabezal...)
        const juce::int64 k = (juce::int64) std::ceil (stepPos - 1e-6);
        stepIdx = k;
        nextStepTime = nowSec() + (k - stepPos) * sd;
        E = k * sd * speed();
        nextGrain = nextStepTime;
        roll.active = false;
        if (manual.active && manual.end < 0) manual.start = k;
        else manual.active = false;
    }
}

void Engine::stop()
{
    playing = false;
    manual.active = false;
    roll.active = false;
    killAll();
}

void Engine::killAll (bool hard)
{
    for (auto& v : voices)
    {
        if (! v.on) continue;
        if (hard || v.startAt > clockS) { v.on = false; continue; }
        if (v.relLeft < 0) { v.relTotal = std::max (1, (int) (0.012 * hostSR)); v.relLeft = v.relTotal; }
    }
}

void Engine::press (int ci)
{
    if (ci < 0 || ci >= (int) chops.size() || ! sample) return;
    uiSelected = ci;
    ++uiSelectSerial;
    if (playing)
    {
        if (prm.latch && manual.active && manual.end < 0 && manual.ci == ci) { manual.end = stepIdx; return; }
        const int q = prm.quant ? 16 / prm.quant : 0;
        juce::int64 st = stepIdx;
        if (q > 1) st = (juce::int64) std::ceil ((double) stepIdx / q) * q;
        manual = { true, ++mid, ci, st, -1 };
    }
    else
    {
        for (auto& v : voices)
            if (v.on && v.shot && v.relLeft < 0 && prm.latch && v.chop == ci) { stopOneShot(); return; }
        oneShot (ci);
    }
}

void Engine::release (int ci)
{
    if (playing && manual.active && manual.ci == ci && manual.end < 0 && ! prm.latch)
    {
        const int q = prm.quant ? 16 / prm.quant : 0;
        manual.end = q > 1 ? std::max (manual.start + 1, (juce::int64) std::ceil ((double) stepIdx / q) * q)
                           : std::max (manual.start + 1, stepIdx);
    }
}

void Engine::latchOff()
{
    stopOneShot();
    if (manual.active && manual.end < 0) manual.end = stepIdx;
}

// ------------------------------------------------------------ secuenciador
void Engine::doStep (double t, juce::int64 k)
{
    const double sd = stepDur(), sp = speed(), e0 = E;
    E += sd * sp;
    lastStepT = t; lastStepE = e0; lastStepK = k; lastSp = sp;

    bool has = false;
    juce::int64 key = 0;
    double sub = 0;
    int rci = -1;
    if (manual.active && k >= manual.start && (manual.end < 0 || k < manual.end))
    {
        has = true;
        key = ((juce::int64) 9 << 48) | manual.id;
        sub = subs()[(size_t) subIdx (prm.size)].v;
        rci = manual.ci;
    }
    else
    {
        if (manual.active && manual.end >= 0 && k >= manual.end) manual.active = false;
        has = presetRoll (k, key, sub);
    }

    if (has && ! chops.empty())
    {
        if (! roll.active || roll.key != key)
        {
            double p; int dir = 1, ci;
            if (rci >= 0) { ci = std::min (rci, (int) chops.size() - 1); p = chops[(size_t) ci].s; }
            else
            {
                auto ps = posAt (e0, k, 0);
                p = ps.p; dir = ps.dir;
                ci = ps.ci >= 0 ? ps.ci : chopAt (p);
            }
            if (ci < 0) ci = 0;
            roll = { true, key, t, p, sub, dir, ci };
        }
        if (roll.sub != sub) { roll.sub = sub; roll.t0 = t; }
        const double d = roll.sub * 240.0 / prm.bpm;
        rollD = d;
        const double rate = rateFor (roll.ci);
        for (juce::int64 m = std::max<juce::int64> (0, (juce::int64) std::ceil ((t - roll.t0) / d - 1e-6));; ++m)
        {
            const double rt = roll.t0 + m * d;
            if (rt >= t + sd - 1e-6) break;
            double p = roll.p; int dir = roll.dir;
            const auto& ch = chops[(size_t) roll.ci];
            if (ch.dir)
            {
                const int want = ch.dir == 2 ? -1 : 1;
                if (want != dir) { p = dir > 0 ? ch.e - (p - ch.s) : ch.s + (ch.e - p); dir = want; }
            }
            seg (rt, p, dir, d * .94, rate, 1, .003, std::min (.006, d * .3));
        }
        nextGrain = t + sd;
    }
    else
    {
        roll.active = false;
        while (nextGrain < t + sd)
        {
            const double dt = nextGrain - t;
            grain (nextGrain, posAt (e0 + dt * sp, k, dt));
            nextGrain += HOP;
        }
    }
}

void Engine::grain (double tg, PosInfo ps)
{
    double p = ps.p; int dir = ps.dir;
    const int ci = ps.ci >= 0 ? ps.ci : chopAt (p);
    if (ci < 0) return;
    const auto& ch = chops[(size_t) ci];
    const double len = ch.e - ch.s;
    if (len <= 0) return;
    const double f = ps.hasLocal ? ps.local / len : (dir > 0 ? (p - ch.s) / len : (ch.e - p) / len);
    if (ch.dir)
    {
        const int want = ch.dir == 2 ? -1 : 1;
        if (want != dir) { p = ch.s + ch.e - p; dir = want; }
    }
    const double g = clampv ((gateFrac (prm.size) - f) * len / .012, 0.0, 1.0);
    if (g <= 0) return;
    seg (tg, p, dir, GR, rateFor (ci), g * .98, GR / 2, GR / 2);
}

Engine::Voice* Engine::allocVoice()
{
    Voice* best = nullptr;
    for (auto& v : voices) if (! v.on) return &v;
    for (auto& v : voices) if (! v.shot && (best == nullptr || v.age > best->age)) best = &v;
    return best != nullptr ? best : &voices[0];
}

void Engine::seg (double t, double p, int dir, double dur, double rate, double gain, double fi, double fo)
{
    if (! sample) return;
    const double D = sample->duration();
    const double off = dir > 0 ? p : D - p;
    if (off < 0 || off >= D - .001) return;
    auto* v = allocVoice();
    *v = Voice();
    v->on = true;
    v->startAt = std::max (clockS, (juce::int64) std::llround (t * hostSR));
    v->pos = p * sample->sr;
    v->inc = dir * rate * sample->sr / hostSR;
    v->len = std::max<juce::int64> (2, (juce::int64) std::llround (dur * hostSR));
    v->fi = std::max (1, (int) std::lround (fi * hostSR));
    v->fo = std::max (1, (int) std::lround (fo * hostSR));
    v->gain = (float) gain;
}

void Engine::oneShot (int ci)
{
    stopOneShot();
    const auto& ch = chops[(size_t) ci];
    int dir = prm.dir == dReverse ? -1 : 1;
    if (ch.dir) dir = ch.dir == 2 ? -1 : 1;
    const double rate = rateFor (ci);
    const double len = (ch.e - ch.s) * (prm.latch ? 1.0 : gateFrac (prm.size));
    auto* v = allocVoice();
    *v = Voice();
    v->on = true;
    v->shot = true;
    v->chop = ci;
    v->startAt = clockS + (juce::int64) (0.005 * hostSR);
    v->pos = (dir > 0 ? ch.s : ch.e) * sample->sr;
    v->inc = dir * rate * sample->sr / hostSR;
    v->fi = std::max (1, (int) (0.004 * hostSR));
    v->gain = 1;
    if (prm.latch)
    {
        v->loop = true;
        v->lo = ch.s * sample->sr;
        v->hi = ch.e * sample->sr;
        v->len = std::numeric_limits<juce::int64>::max() / 4;
        v->fo = 1;
    }
    else
    {
        const double du = len / rate;
        v->len = std::max<juce::int64> (2, (juce::int64) (du * hostSR));
        v->fo = std::max (1, (int) (std::min (0.006, du * .5) * hostSR));
    }
}

void Engine::stopOneShot()
{
    for (auto& v : voices)
        if (v.on && v.shot && v.relLeft < 0) { v.relTotal = std::max (1, (int) (0.02 * hostSR)); v.relLeft = v.relTotal; }
}

// ------------------------------------------------------------ audio
static inline float cubicAt (const float* d, int n, double pos)
{
    const int i = (int) std::floor (pos);
    if (i < 0 || i >= n) return 0.0f;
    const float t = (float) (pos - i);
    const float y0 = i > 0 ? d[i - 1] : d[i];
    const float y1 = d[i];
    const float y2 = i + 1 < n ? d[i + 1] : 0.0f;
    const float y3 = i + 2 < n ? d[i + 2] : 0.0f;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void Engine::render (float* L, float* R, int n)
{
    const SampleData* sd = sample.get();
    const float* s0 = sd ? sd->buf.getReadPointer (0) : nullptr;
    const float* s1 = sd ? sd->buf.getReadPointer (1) : nullptr;
    const int sn = sd ? sd->buf.getNumSamples() : 0;

    for (int i = 0; i < n; ++i)
    {
        if (playing && sd != nullptr)
        {
            int guard = 0;
            while ((double) clockS >= nextStepTime * hostSR - 1e-6 && guard++ < 8)
            {
                const double t = nextStepTime;
                doStep (t, stepIdx);
                nextStepTime = t + stepDur();
                ++stepIdx;
            }
        }
        if (sd != nullptr)
        {
            float l = 0, r = 0;
            for (auto& v : voices)
            {
                if (! v.on || clockS < v.startAt) continue;
                float env = v.gain;
                if (v.age < v.fi) env *= (float) v.age / (float) v.fi;
                const juce::int64 left = v.len - v.age;
                if (left < v.fo) env *= (float) left / (float) v.fo;
                if (v.relLeft >= 0) env *= (float) v.relLeft / (float) v.relTotal;
                if (env > 0)
                {
                    l += cubicAt (s0, sn, v.pos) * env;
                    r += cubicAt (s1, sn, v.pos) * env;
                }
                v.pos += v.inc;
                if (v.loop)
                {
                    const double span = v.hi - v.lo;
                    if (span > 1)
                    {
                        while (v.pos >= v.hi) v.pos -= span;
                        while (v.pos < v.lo) v.pos += span;
                    }
                }
                ++v.age;
                if (v.relLeft >= 0 && --v.relLeft <= 0) v.on = false;
                if (v.age >= v.len) v.on = false;
                if (! v.loop && (v.pos < -2 || v.pos > sn + 2)) v.on = false;
            }
            L[i] += l;
            R[i] += r;
        }
        ++clockS;
    }
    updateUi();
}

void Engine::updateUi()
{
    if (! sample) { uiPlayhead = -1.0; uiRollSub = -1; return; }
    const double now = nowSec();
    if (playing)
    {
        if (now < lastStepT) { uiPlayhead = -1.0; uiRollSub = -1; return; }
        if (roll.active && rollD > 0)
        {
            const double rate = rateFor (roll.ci);
            const double ph = modp (now - roll.t0, rollD) * rate;
            uiPlayhead = roll.dir > 0 ? roll.p + ph : roll.p - ph;
            const double len = rollD * rate;
            uiRollA = roll.dir > 0 ? roll.p : roll.p - len;
            uiRollB = roll.dir > 0 ? roll.p + len : roll.p;
            int idx = -1;
            for (size_t j = 0; j < subs().size(); ++j) if (std::abs (subs()[j].v - roll.sub) < 1e-9) idx = (int) j;
            uiRollSub = idx;
        }
        else
        {
            const double dt = now - lastStepT;
            uiPlayhead = posAt (lastStepE + dt * lastSp, lastStepK, dt).p;
            uiRollSub = -1;
        }
        return;
    }
    uiRollSub = -1;
    for (auto& v : voices)
        if (v.on && v.shot && v.relLeft < 0) { uiPlayhead = v.pos / sample->sr; return; }
    uiPlayhead = -1.0;
}
} // namespace vc
