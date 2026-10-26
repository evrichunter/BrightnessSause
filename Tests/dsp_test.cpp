// Headless validation of the DSP core. Build: g++ -O2 -std=c++17 -I../Source dsp_test.cpp -o dsp_test && ./dsp_test
#include "DSP.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <limits>

using namespace bs;
static int fails = 0;
#define CHECK(cond, ...) do { if (cond) std::printf("  PASS  "); else { std::printf("  FAIL  "); ++fails; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static std::vector<float> sine (double fs, double f, double amp, int n)
{
    std::vector<float> v (n);
    for (int i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2 * kPi * f * i / fs));
    return v;
}
static std::vector<float> noise (int n, float amp, unsigned seed = 1)
{
    std::mt19937 g (seed); std::uniform_real_distribution<float> d (-amp, amp);
    std::vector<float> v (n); for (auto& x : v) x = d (g); return v;
}
static void run (Engine& e, std::vector<float>& l, std::vector<float>* r = nullptr, int block = 512)
{
    const int n = (int) l.size();
    for (int p = 0; p < n; p += block)
    {
        const int m = std::min (block, n - p);
        float* ptrs[2] = { l.data() + p, r ? r->data() + p : nullptr };
        e.process (ptrs, r ? 2 : 1, m);
    }
}
static double maxDiff (const std::vector<float>& a, const std::vector<float>& b, int skip = 0)
{ double m = 0; for (size_t i = skip; i < a.size(); ++i) m = std::max (m, (double) std::fabs (a[i] - b[i])); return m; }
static double peak (const std::vector<float>& a) { double m = 0; for (float x : a) m = std::max (m, (double) std::fabs (x)); return m; }
static double goertzel (const std::vector<float>& x, double fs, double f, int skip)
{
    double w = 2 * kPi * f / fs, c = 2 * std::cos (w), s1 = 0, s2 = 0; int n = 0;
    for (size_t i = skip; i < x.size(); ++i, ++n) { double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return std::sqrt (re * re + im * im) * 2.0 / n;
}
// Gain at f measured with a 200 Hz companion tone so the sibilance guard sees a realistic, non-sibilant spectrum
static double gainDb (double fs, double f, const Params& p)
{
    Engine e; e.prepare (fs); e.setParams (p, true);
    auto x = sine (fs, f, 0.1, (int) fs); auto lo = sine (fs, 200, 0.3, (int) fs);
    for (size_t i = 0; i < x.size(); ++i) x[i] += lo[i];
    auto y = x; run (e, y);
    return 20 * std::log10 (goertzel (y, fs, f, (int) fs / 2) / goertzel (x, fs, f, (int) fs / 2));
}

int main()
{
    const double fs = 48000; const int N = 48000;
    Params def;

    std::printf("1. Dry at 0%% MIX\n");
    { Params p = def; p.mix = 0; Engine e; e.prepare (fs); e.setParams (p, true);
      auto x = noise (N, 0.5f); auto y = x; run (e, y);
      CHECK (maxDiff (x, y) == 0.0, "bit-exact dry (max diff %.3g)", maxDiff (x, y)); }

    std::printf("2. Processed at 100%% MIX\n");
    { Engine e; e.prepare (fs); e.setParams (def, true);
      auto x = noise (N, 0.3f); auto y = x; run (e, y);
      CHECK (maxDiff (x, y) > 1e-3, "wet differs from dry (max diff %.4f)", maxDiff (x, y)); }

    std::printf("3. Intermediate blend\n");
    { Params p1 = def, p5 = def; p5.mix = 0.5f;
      Engine e1, e5; e1.prepare (fs); e5.prepare (fs); e1.setParams (p1, true); e5.setParams (p5, true);
      auto x = noise (N, 0.3f); auto w = x, h = x; run (e1, w); run (e5, h);
      double m = 0; for (int i = 0; i < N; ++i) m = std::max (m, (double) std::fabs (h[i] - 0.5 * (x[i] + w[i])));
      CHECK (m < 1e-5, "mix 50%% == average of dry and wet (err %.3g)", m); }

    std::printf("4. Independent stage bypass\n");
    { Params p = def; p.excite = 0; p.air = 0; p.presence = 0; p.brightness = 0; p.eqBypass = true;
      Engine e; e.prepare (fs); e.setParams (p, true);
      auto x = noise (N, 0.5f); auto y = x; run (e, y);
      CHECK (maxDiff (x, y) < 1e-6, "all stages off -> transparent (diff %.3g)", maxDiff (x, y));
      Params q = p; q.eqBypass = false; q.lowCutHz = 80; q.presGainDb = 0; q.airGainDb = 0;
      Engine e2; e2.prepare (fs); e2.setParams (q, true);
      CHECK (std::fabs (gainDb (fs, 40, q)) > 2.0 && std::fabs (gainDb (fs, 1000, q)) < 0.1, "EQ stage alone = low cut only (40Hz %.1f dB, 1k %.2f dB)", gainDb (fs, 40, q), gainDb (fs, 1000, q));
      Params a = p; a.air = 1; CHECK (gainDb (fs, 15000, a) > 5.0, "air stage alone lifts 15k by %.1f dB", gainDb (fs, 15000, a));
      Params b = p; b.excite = 1; b.harmonics = 1; Engine e3; e3.prepare (fs); e3.setParams (b, true);
      auto s = sine (fs, 3000, 0.3, N); auto o = s; run (e3, o);
      double h3 = goertzel (o, fs, 9000, N / 2), h3d = goertzel (s, fs, 9000, N / 2);
      CHECK (h3 > 20 * h3d + 1e-4, "exciter generates harmonics (9k: %.5f vs dry %.5f)", h3, h3d);
      Params g = def; g.bypass = true; Engine e4; e4.prepare (fs); e4.setParams (g, true);
      auto x2 = noise (N, 0.5f); auto y2 = x2; run (e4, y2); CHECK (maxDiff (x2, y2) == 0.0, "global bypass bit-exact"); }

    std::printf("5. Mono / stereo / independence\n");
    { Engine em, es; em.prepare (fs); es.prepare (fs); em.setParams (def, true); es.setParams (def, true);
      auto m = noise (N, 0.3f); auto l = m, r = m; run (em, m); run (es, l, &r);
      CHECK (maxDiff (m, l) == 0.0 && maxDiff (l, r) == 0.0, "mono == stereo L == stereo R");
      Engine ei; ei.prepare (fs); ei.setParams (def, true);
      auto a = noise (N, 0.3f); std::vector<float> z (N, 0.f); run (ei, a, &z);
      CHECK (peak (z) == 0.0, "silent channel stays silent (no crosstalk)"); }

    std::printf("6/7. Extremes, NaN, clipping, sample rates, block sizes\n");
    { bool ok = true; double worst = 0;
      const double rates[] = { 22050, 44100, 48000, 96000, 192000 };
      const int blocks[] = { 1, 7, 64, 512, 4096 };
      for (double r : rates) for (int blk : blocks)
      {
          Params p; p.excite = p.character = p.harmonics = p.air = p.presence = p.brightness = 1; p.softness = 0;
          p.lowCutHz = 500; p.presFreqHz = 10000; p.presGainDb = 6; p.airFreqHz = 20000; p.airGainDb = 9; p.q = 4; p.outputDb = 12;
          Engine e; e.prepare (r); e.setParams (p, true);
          auto x = noise ((int) r / 2, 0.7f, 3); auto y = x; auto y2 = x; run (e, y, &y2, blk);
          for (size_t i = 0; i < y.size(); ++i) if (! std::isfinite (y[i]) || ! std::isfinite (y2[i])) ok = false;
          worst = std::max (worst, peak (y));
          // parameter slam
          Params lo; lo.excite = 0; lo.air = 0; lo.mix = 0; lo.outputDb = -12; lo.lowCutHz = 20; lo.q = 0.3f;
          for (int k = 0; k < 20; ++k) { e.setParams (k & 1 ? p : lo); auto t = noise (r > 1000 ? 997 : 100, 1.0f, k); run (e, t, nullptr, blk);
              for (float v : t) if (! std::isfinite (v)) ok = false; }
      }
      CHECK (ok, "no NaN/inf across 5 rates x 5 block sizes, extreme params, param slamming (worst peak %.2f at +12 dB out)", worst);
      Engine e; e.prepare (fs); e.setParams (def, true);
      std::vector<float> bad (4096, 0.1f); bad[10] = std::numeric_limits<float>::quiet_NaN(); bad[20] = std::numeric_limits<float>::infinity(); bad[30] = -std::numeric_limits<float>::infinity();
      run (e, bad); bool fin = true; for (float v : bad) if (! std::isfinite (v)) fin = false;
      auto good = sine (fs, 440, 0.3, 4096); run (e, good);
      CHECK (fin && std::isfinite (good[4000]) && peak (good) > 0.1, "NaN/inf input is sanitised and engine recovers");
      Engine d; d.prepare (fs); d.setParams (def, true);
      auto v = sine (fs, 220, 0.5, N); auto vy = v; run (d, vy);
      CHECK (peak (vy) < 1.0, "default settings on 0.5 FS sine: peak %.3f (no clipping, no hidden limiter)", peak (vy)); }

    std::printf("8. Zero latency\n");
    { Engine e; e.prepare (fs); Params p = def; p.lowCutHz = 20; p.excite = 0; e.setParams (p, true);
      std::vector<float> imp (256, 0.f); imp[0] = 1.f; run (e, imp);
      CHECK (e.getLatencySamples() == 0 && std::fabs (imp[0]) > 0.5f, "reports 0 latency; impulse response starts at sample 0 (%.3f)", imp[0]); }

    std::printf("Info: default frequency response (dB)\n  ");
    for (double f : { 50.0, 100.0, 500.0, 1000.0, 3000.0, 6000.0, 10000.0, 15000.0 }) std::printf("%gHz:%.2f  ", f, gainDb (fs, f, def));
    std::printf("\n\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL CHECKS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
