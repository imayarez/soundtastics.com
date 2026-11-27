#include "Factory.h"
#include <map>

namespace vc
{
namespace
{
constexpr double SR = 44100.0;

double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

struct Bq
{
    double c[5] { 0, 0, 0, 0, 0 };
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void setBp (double f, double q)
    {
        const double w = 2 * kPi * std::min (f, SR * 0.45) / SR, al = std::sin (w) / (2 * q), a0 = 1 + al;
        c[0] = al / a0; c[1] = 0; c[2] = -al / a0; c[3] = -2 * std::cos (w) / a0; c[4] = (1 - al) / a0;
    }
    Bq() = default;
    Bq (double f, double q) { setBp (f, q); }
    double run (double x)
    {
        const double y = c[0] * x + c[1] * x1 + c[2] * x2 - c[3] * y1 - c[4] * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

using F3 = std::array<double, 3>;
const std::map<std::string, F3>& VOW()
{
    static const std::map<std::string, F3> m { { "a", { 800, 1150, 2900 } }, { "e", { 420, 1950, 2650 } },
        { "i", { 300, 2300, 3000 } }, { "o", { 460, 820, 2800 } }, { "u", { 350, 650, 2650 } } };
    return m;
}
const std::map<std::string, F3>& CF()
{
    static const std::map<std::string, F3> m { { "n", { 250, 1700, 2600 } }, { "m", { 250, 1100, 2300 } },
        { "l", { 360, 1050, 2600 } }, { "y", { 280, 2250, 3000 } }, { "w", { 300, 620, 2300 } },
        { "d", { 300, 1700, 2600 } }, { "b", { 300, 800, 2300 } }, { "g", { 300, 1900, 2500 } } };
    return m;
}
const std::map<std::string, std::array<double, 4>>& NZ()
{
    static const std::map<std::string, std::array<double, 4>> m { { "s", { 6500, .8, .22, .09 } },
        { "sh", { 3000, 1.4, .25, .1 } }, { "t", { 4200, 1, .55, .025 } }, { "k", { 1900, 1.5, .55, .035 } },
        { "p", { 900, 1, .5, .025 } }, { "h", { 1200, 1.2, .22, .07 } }, { "f", { 5000, .7, .12, .08 } } };
    return m;
}
const double BW[3] { 90, 110, 150 }, FA[3] { 1, .55, .3 };

void normalize (juce::AudioBuffer<float>& b, double pk)
{
    float m = 1e-9f;
    for (int c = 0; c < b.getNumChannels(); ++c) m = std::max (m, b.getMagnitude (c, 0, b.getNumSamples()));
    b.applyGain ((float) (pk / m));
}

struct Syl
{
    std::string c, v, v2;
    std::vector<int> n;
    double d = 0.3, g = -1, gl = 0, a = 1;
};
struct VOpt
{
    int root = 64, voices = 1;
    double gap = .07, detune = 9, vib = .22, breath = .05, fshift = 1;
    uint32_t seed = 3;
};

// Atajo para escribir silabas como en el HTML
Syl S (const char* c, const char* v, std::vector<int> n, double d, double g = -1, double gl = 0, const char* v2 = "", double a = 1)
{
    Syl s; s.c = c; s.v = v; s.v2 = v2; s.n = std::move (n); s.d = d; s.g = g; s.gl = gl; s.a = a;
    return s;
}

GeneratedAudio synthVoice (const std::vector<Syl>& sy, const VOpt& o)
{
    const int V = o.voices;
    const double pad = .18;
    Rng rnd (o.seed);
    double tot = pad;
    for (auto& s : sy) tot += s.d + (s.g >= 0 ? s.g : o.gap);
    tot += .45;
    const int N = (int) std::ceil (tot * SR);
    GeneratedAudio out;
    out.buf.setSize (2, N);
    out.buf.clear();
    auto* L = out.buf.getWritePointer (0);
    auto* R = out.buf.getWritePointer (1);

    for (int v = 0; v < V; ++v)
    {
        const double det = V > 1 ? (v - (V - 1) / 2.0) * o.detune : 0;
        const double pan = V > 1 ? ((double) v / (V - 1)) * 1.6 - .8 : 0;
        const double gl = std::cos ((pan + 1) * kPi / 4) * std::sqrt (2.0), gr = std::sin ((pan + 1) * kPi / 4) * std::sqrt (2.0);
        const double vg = 1 / std::sqrt ((double) V);
        double t = pad, ph = rnd();
        Bq fl[3] { Bq (500, 5), Bq (1500, 10), Bq (2500, 15) };
        for (auto& s : sy)
        {
            if (v == 0) out.cuts.push_back (std::max (0.0, t - .008));
            const int st = (int) std::floor (t * SR), n0 = (int) std::floor (s.d * SR);
            const auto nzIt = NZ().find (s.c);
            const bool hasNz = ! s.c.empty() && nzIt != NZ().end();
            const double cd = hasNz ? nzIt->second[3] : 0;
            const auto fcIt = CF().find (s.c);
            const bool hasFc = ! s.c.empty() && fcIt != CF().end();
            const double note = s.n[(size_t) (v % (int) s.n.size())] + o.root;
            const F3 F1 = VOW().at (s.v), F2 = VOW().at (s.v2.empty() ? s.v : s.v2);
            Bq nf;
            if (hasNz) nf.setBp (nzIt->second[0], nzIt->second[1]);
            const double vd = s.d - cd, amp = s.a;
            for (int i = 0; i < n0; ++i)
            {
                const int idx = st + i;
                if (idx >= N) break;
                const double tt = i / SR;
                double o2 = 0;
                if (hasNz && tt < cd * 1.3 && v == 0)
                {
                    const double e = std::pow (std::max (0.0, 1 - tt / (cd * 1.3)), 1.4) * std::min (1.0, tt / .003);
                    o2 += nf.run (rnd() * 2 - 1) * e * nzIt->second[2] * 3;
                }
                const double tv = tt - cd;
                if (tv >= 0)
                {
                    const double fr = tv / vd, sm = fr * fr * (3 - 2 * fr);
                    const double vib = tv > .14 ? std::sin (2 * kPi * 5.3 * tt + v * 1.7) * o.vib * std::min (1.0, (tv - .14) / .25) : 0;
                    const double f0 = mtof (note + s.gl * fr + det / 100 + vib - .5 * (1 - std::min (1.0, tv / .06)));
                    ph += f0 / SR;
                    if (ph >= 1) ph -= 1;
                    const double src = (1 - 2 * ph) + (rnd() * 2 - 1) * o.breath;
                    if ((i & 31) == 0)
                    {
                        const double w = hasFc ? std::min (1.0, tv / .07) : 1;
                        for (int k = 0; k < 3; ++k)
                        {
                            double f = F1[(size_t) k] + (F2[(size_t) k] - F1[(size_t) k]) * sm;
                            if (hasFc) f = fcIt->second[(size_t) k] + (f - fcIt->second[(size_t) k]) * w;
                            f *= o.fshift;
                            fl[k].setBp (f, f / BW[k]);
                        }
                    }
                    double y = 0;
                    for (int k = 0; k < 3; ++k) y += fl[k].run (src) * FA[k];
                    const double a = std::min (1.0, tv / .025) * std::min (1.0, std::max (0.0, vd - tv) / .08)
                                   * (hasFc ? .55 + .45 * std::min (1.0, tv / .06) : 1);
                    o2 += y * a * amp;
                }
                L[idx] += (float) (o2 * gl * vg);
                R[idx] += (float) (o2 * gr * vg);
            }
            t += s.d + (s.g >= 0 ? s.g : o.gap);
        }
    }
    normalize (out.buf, .85);
    out.sr = SR;
    return out;
}

struct DrumPat { std::string k, s, c, h; };

GeneratedAudio synthDrums (double bpm, const DrumPat& pat, double swing, uint32_t seed = 7)
{
    const double sd = 60 / bpm / 4;
    const int N = (int) std::ceil ((32 * sd + .5) * SR);
    GeneratedAudio out;
    out.buf.setSize (2, N);
    out.buf.clear();
    auto* L = out.buf.getWritePointer (0);
    auto* R = out.buf.getWritePointer (1);
    Rng rnd (seed);
    auto add = [&] (double t, double len, auto fn, double pan)
    {
        const int st = (int) std::floor (t * SR), n = (int) std::floor (len * SR);
        const double gl = 1 - std::max (0.0, pan), gr = 1 + std::min (0.0, pan);
        for (int i = 0; i < n && st + i < N; ++i)
        {
            const double y = fn (i / SR);
            L[st + i] += (float) (y * gl);
            R[st + i] += (float) (y * gr);
        }
    };
    auto ch = [] (const std::string& p, int i) { return i < (int) p.size() && p[(size_t) i] != '.'; };
    for (int i = 0; i < 32; ++i)
    {
        const double t = i * sd + ((i % 2) ? swing * sd : 0);
        bool hit = false;
        if (ch (pat.k, i))
        {
            hit = true;
            double ph = 0;
            add (t, .4, [&] (double tt) { ph += (45 + 110 * std::exp (-tt * 28)) / SR;
                return std::sin (2 * kPi * ph) * std::exp (-tt * 7) * .95 + (rnd() * 2 - 1) * std::exp (-tt * 500) * .3; }, 0);
        }
        if (ch (pat.s, i))
        {
            hit = true;
            Bq f (1900, .7);
            add (t, .25, [&] (double tt) { return std::sin (2 * kPi * 185 * tt) * std::exp (-tt * 22) * .45
                                                 + f.run (rnd() * 2 - 1) * std::exp (-tt * 15) * 1.6; }, .05);
        }
        if (ch (pat.c, i))
        {
            hit = true;
            Bq f (1300, 1.2);
            add (t, .3, [&] (double tt) { const double b = (tt < .03 ? std::exp (-modp (tt, .011) * 300) : 0) + std::exp (-tt * 17) * .6;
                                          return f.run (rnd() * 2 - 1) * b * 2.2; }, -.1);
        }
        if (ch (pat.h, i))
        {
            hit = true;
            const bool op = pat.h[(size_t) i] == 'o';
            double p = 0;
            add (t, op ? .3 : .06, [&] (double tt) { const double r = rnd() * 2 - 1, y = r - p; p = r;
                                                      return y * std::exp (-tt * (op ? 12 : 70)) * .22; }, .25);
        }
        if (hit) out.cuts.push_back (std::max (0.0, t - .002));
    }
    normalize (out.buf, .9);
    out.bpm = bpm;
    out.sr = SR;
    return out;
}
} // namespace

const std::vector<PatchInfo>& patchList()
{
    static const std::vector<PatchInfo> p {
        { "VOCAL PHRASES", "Daylight", "Frase pop, Mi mayor", false, { 0, 0, .4, .08, .25 } },
        { "VOCAL PHRASES", "Hold On", "Frase soul, Do mayor", false, { 5, 0, .45, .2, .3 } },
        { "VOCAL PHRASES", "Neon Hymn", "Frase oscura, Re menor", false, { 3, 2, .5, .35, .45 } },
        { "AD-LIBS", "Yeah Stack", "Ad-libs con caida", false, { 1, 0, .4, .15, .2 } },
        { "AD-LIBS", "Hey!", "Gritos cortos", false, { 4, 0, .55, .55, .15 } },
        { "AD-LIBS", "Oh-Oh", "Ad-lib melodico", false, { 2, 0, .4, .1, .3 } },
        { "TEXTURAL / CHOIRS", "Cathedral Air", "Coro de 5 voces", false, { 0, 0, .3, 0, .6 } },
        { "TEXTURAL / CHOIRS", "Glass Choir", "Coro agudo brillante", false, { 3, 0, .6, .3, .7 } },
        { "HITS & LOOPS", "Dusty Break 96", "Break de bateria - 96 BPM", true, { 1, 0, .4, .3, .12 } },
        { "HITS & LOOPS", "Clap Grid 120", "Clap + kick - 120 BPM", true, { 4, 0, .6, .62, .18 } },
    };
    return p;
}

GeneratedAudio generatePatch (int i)
{
    switch (i)
    {
        case 0: { VOpt o; o.root = 64; o.seed = 11;
            return synthVoice ({ S ("d","e",{0},.3), S ("y","a",{2},.26), S ("l","a",{4},.42,.18,0,"i"), S ("w","o",{7},.34),
                                 S ("h","o",{4},.3), S ("n","a",{2},.55,.22,-2), S ("s","e",{0},.28), S ("t","i",{2},.24), S ("l","a",{4},.6) }, o); }
        case 1: { VOpt o; o.root = 60; o.seed = 5; o.vib = .3;
            return synthVoice ({ S ("h","o",{7},.45), S ("l","o",{9},.32,.04), S ("d","a",{7},.5,.2,-3), S ("","o",{4},.3),
                                 S ("n","o",{5},.7,.25), S ("b","e",{4},.26), S ("y","u",{2},.62,-1,-2) }, o); }
        case 2: { VOpt o; o.root = 62; o.seed = 9;
            return synthVoice ({ S ("n","i",{0},.36), S ("y","o",{3},.34), S ("n","a",{5},.46,.16), S ("s","e",{7},.3),
                                 S ("k","a",{8},.4,-1,-1), S ("l","u",{7},.66,.25), S ("m","a",{3},.5) }, o); }
        case 3: { VOpt o; o.root = 64; o.seed = 21; o.vib = .15;
            return synthVoice ({ S ("y","e",{7},.4,.22,-3,"a"), S ("y","e",{12},.34,.3,-4,"a"), S ("w","o",{9},.5,.25,-5),
                                 S ("y","e",{4},.42,-1,-2,"a") }, o); }
        case 4: { VOpt o; o.root = 62; o.seed = 4; o.vib = .05;
            return synthVoice ({ S ("h","e",{5},.22,.3,0,"i",1.2), S ("h","e",{5},.22,.3,0,"i",1.2), S ("h","o",{7},.3,.3),
                                 S ("h","e",{10},.26,-1,-3,"i") }, o); }
        case 5: { VOpt o; o.root = 64; o.seed = 13;
            return synthVoice ({ S ("","o",{4},.3,.05), S ("w","o",{0},.45,.2), S ("","o",{7},.28,.05), S ("w","o",{4},.55,-1,-2) }, o); }
        case 6: { VOpt o; o.root = 57; o.voices = 5; o.seed = 31; o.vib = .12; o.breath = .09;
            return synthVoice ({ S ("","a",{0,4,7,12,16},1.6,.06,0,"o"), S ("","o",{-3,0,4,9,12},1.6,.06,0,"a"),
                                 S ("","a",{-7,-3,0,5,9},1.6,.06,0,"e"), S ("","o",{-5,-1,2,7,11},1.9) }, o); }
        case 7: { VOpt o; o.root = 69; o.voices = 4; o.seed = 41; o.vib = .1; o.breath = .08; o.detune = 14;
            return synthVoice ({ S ("","i",{0,7,12,16},1.3,.05,0,"e"), S ("","e",{2,9,14,17},1.3,.05,0,"i"),
                                 S ("","i",{-1,7,11,16},1.5) }, o); }
        case 8:
            return synthDrums (96, { "x.....x...x.....x.x...x...x.....", "....x.......x.......x.......x..x", "",
                                     "x.x.x.x.x.x.x.o.x.x.x.x.x.x.x.o." }, .12);
        default:
            return synthDrums (120, { "x...x...x...x...x...x...x...x.x.", "", "....x.......x.......x.......x...",
                                      "..x...x...x...x...x...x...x...xx" }, 0);
    }
}
} // namespace vc
