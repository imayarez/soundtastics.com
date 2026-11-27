#include "Analysis.h"

namespace vc
{
static std::vector<float> envDb (const std::vector<float>& m, int hop, int win)
{
    const size_t n = m.size();
    std::vector<double> P (n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) P[i + 1] = P[i] + (double) m[i] * m[i];
    const int F = (int) (n / (size_t) hop);
    std::vector<float> e ((size_t) std::max (0, F));
    for (int f = 0; f < F; ++f)
    {
        const long c = (long) f * hop;
        const long a = std::max (0L, c - (win >> 1));
        const long b = std::min ((long) n, c + (win >> 1));
        e[(size_t) f] = (float) (10.0 * std::log10 ((P[(size_t) b] - P[(size_t) a]) / std::max (1L, b - a) + 1e-10));
    }
    return e;
}

static long zeroSnap (const std::vector<float>& m, long i, int r = 200)
{
    const long n = (long) m.size();
    for (int k = 0; k < r; ++k)
        for (long j : { i - k, i + k })
            if (j > 0 && j < n && ((m[(size_t) j - 1] <= 0) != (m[(size_t) j] <= 0)))
                return j;
    return i;
}

AnalysisResult analyse (const juce::AudioBuffer<float>& buf, double SR, int mode, double sens)
{
    AnalysisResult res;
    const int nch = std::max (1, buf.getNumChannels());
    const int n = buf.getNumSamples();
    std::vector<float> m ((size_t) n, 0.0f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* d = buf.getReadPointer (c);
        for (int i = 0; i < n; ++i) m[(size_t) i] += d[i] / (float) nch;
    }

    const int hop = 256;
    const double dur = n / SR;
    auto es = envDb (m, hop, 512);
    auto el = envDb (m, hop, 2048);
    const int F = (int) es.size();
    res.A = 0; res.B = dur; res.cuts = { 0.0 }; res.bpm = 0;
    if (F < 8) return res;

    float mx = -200;
    for (auto x : el) mx = std::max (mx, x);
    const float floorDb = mx - 42.0f;
    const double fps = SR / hop;

    std::vector<float> od ((size_t) F, 0.0f);
    for (int f = 3; f < F; ++f)
        od[(size_t) f] = std::max (0.0f, es[(size_t) f] - std::min ({ es[(size_t) f - 1], es[(size_t) f - 2], es[(size_t) f - 3] }));

    // ---- onsets
    std::vector<int> ons;
    const double dth = 13.0 - sens * 8.0;
    const int gap = (int) std::lround ((0.24 - sens * 0.16) * fps);
    int last = -1000000000;
    for (int f = 3; f < F - 3; ++f)
    {
        if (od[(size_t) f] < dth || es[(size_t) f] < floorDb) continue;
        bool pk = true;
        for (int k = -3; k <= 3; ++k) if (od[(size_t) (f + k)] > od[(size_t) f]) pk = false;
        if (pk && f - last >= gap) { ons.push_back (f); last = f; }
    }

    bool hit = mode == detHit;
    if (mode == detAuto)
    {
        int dec = 0;
        for (int f : ons)
        {
            float p = -200;
            for (int k = 0; k < 6 && f + k < F; ++k) p = std::max (p, es[(size_t) (f + k)]);
            const float later = es[(size_t) std::min (F - 1, f + 22)];
            if (p - later > 6) ++dec;
        }
        hit = ons.size() >= 3 && (double) dec / (double) ons.size() > 0.5;
    }

    int first = 0, lastS = F - 1;
    while (first < F && el[(size_t) first] < floorDb + 3) ++first;
    while (lastS > 0 && el[(size_t) lastS] < floorDb + 3) --lastS;
    const double A = std::max (0.0, (first - 2) * (double) hop / SR);
    const double B = std::min (dur, (lastS + 8) * (double) hop / SR);

    std::vector<double> cuts;
    if (hit)
    {
        for (int f : ons)
        {
            const long c = (long) f * hop;
            float lp = 0;
            for (long i = c; i < std::min ((long) n, c + 2048); ++i) lp = std::max (lp, std::abs (m[(size_t) i]));
            long s = std::max (0L, c - 1024);
            for (long i = s; i < c + 512 && i < n; ++i)
                if (std::abs (m[(size_t) i]) > lp * 0.12f) { s = i; break; }
            long z = s;
            for (int k = 0; k < 96 && z > 0; ++k, --z)
                if ((m[(size_t) z - 1] <= 0) != (m[(size_t) z] <= 0)) break;
            cuts.push_back (z / SR);
        }
    }
    else
    {
        const int W = (int) std::lround (0.07 * fps), side = (int) std::lround (0.35 * fps);
        const double depth = 10.0 - sens * 6.5, minSeg = (0.34 - sens * 0.22) * fps;
        double lc = -1e9;
        for (int f = first + W; f < lastS - W; ++f)
        {
            bool isMin = true;
            for (int k = -W; k <= W; ++k) if (el[(size_t) (f + k)] < el[(size_t) f]) { isMin = false; break; }
            if (! isMin) continue;
            float ml = -200, mr = -200;
            for (int k = 1; k <= side; ++k)
            {
                if (f - k >= 0) ml = std::max (ml, el[(size_t) (f - k)]);
                if (f + k < F)  mr = std::max (mr, el[(size_t) (f + k)]);
            }
            if (std::min (ml, mr) - el[(size_t) f] < depth || f - lc < minSeg) continue;
            int g = f;
            if (el[(size_t) f] < floorDb + 6)
            {
                while (g < F - 1 && el[(size_t) g] < floorDb + 6) ++g;
                g = std::max (f, g - 3);
            }
            cuts.push_back (zeroSnap (m, (long) g * hop) / SR);
            lc = f;
        }
        cuts.insert (cuts.begin(), A);
    }

    std::vector<double> filt;
    for (double c : cuts) if (c >= A - 0.01 && c < B - 0.05) filt.push_back (c);
    std::sort (filt.begin(), filt.end());
    std::vector<double> cl;
    for (double c : filt) if (cl.empty() || c - cl.back() > 0.045) cl.push_back (c);
    if (cl.empty()) cl.push_back (A);

    // ---- tempo: autocorrelacion de onsets + ajuste fino por rejilla
    double best = 0, bb = 0;
    for (double bpm = 70; bpm <= 180; bpm += 0.5)
    {
        const double lag = 60.0 / bpm * fps;
        double s = 0;
        for (int f = 0; f + lag + 1 < F; ++f)
        {
            const int l = (int) std::floor (f + lag);
            const double fr = f + lag - l;
            s += od[(size_t) f] * (od[(size_t) l] * (1 - fr) + od[(size_t) l + 1] * fr);
        }
        s *= std::exp (-0.5 * std::pow (std::log2 (bpm / 118.0) / 0.7, 2.0));
        if (s > bb) { bb = s; best = bpm; }
    }
    if (best > 0 && ons.size() >= 4)
    {
        std::vector<double> ts;
        for (int f : ons) ts.push_back (f * (double) hop / SR);
        double bs = -1e9, bb2 = best;
        for (double b = best * 0.92; b <= best * 1.08; b += 0.1)
        {
            const double per = 30.0 / b;
            double sc = 0;
            for (double t : ts) sc += std::cos (2 * kPi * (t - ts[0]) / per);
            if (sc > bs) { bs = sc; bb2 = b; }
        }
        best = bb2;
    }
    if (best > 0)
    {
        const double Ld = B - A;
        for (int k : { 1, 2, 3, 4, 6, 8, 16 })
        {
            const double b = k * 240.0 / Ld;
            if (std::abs (b - best) / best < 0.02) { best = b; break; }
        }
    }

    res.isHit = hit;
    res.cuts = cl;
    res.A = A;
    res.B = B;
    res.bpm = std::round (best * 10.0) / 10.0;
    return res;
}
} // namespace vc
