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
    { Params p = def; p.excite = 0; p.air = 0; p.presence = 0; p.brightness = 0; p.eqBypass = true; p.deessOn = false;
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

    // ------------------------------------------------------------------ V2: de-esser
    std::printf("9. De-esser\n");
    {
        // voice-like test signal: sustained vowel (200+600 Hz) with a 5-9 kHz noise burst (the "s") from 0.5 s to 1.0 s
        auto makeVoice = [&] (bool withS, unsigned seed)
        {
            std::vector<float> v (N * 3 / 2);
            auto nz = noise ((int) v.size(), 1.0f, seed);
            Biquad h, l, h2, l2; auto ch5 = filt::highpass (fs, 5000, 0.7071), cl9 = filt::lowpass (fs, 9000, 0.7071);
            for (size_t i = 0; i < v.size(); ++i)
            {
                double vowel = 0.25 * std::sin (2 * kPi * 200 * i / fs) + 0.10 * std::sin (2 * kPi * 600 * i / fs);
                double sib = l.process (h.process (nz[i], ch5), cl9);
                sib = l2.process (h2.process (sib, ch5), cl9) * 0.55; // steeper skirts, ~0.12 rms
                const bool inBurst = i > (size_t) (0.5 * fs) && i < (size_t) (1.0 * fs);
                v[i] = (float) (vowel + (withS && inBurst ? sib : 0.0));
            }
            return v;
        };
        auto bandRms = [&] (const std::vector<float>& x, double a, double b)
        {   // RMS of 4k-10k band in [a,b] seconds
            Biquad h, l; auto c1 = filt::highpass (fs, 4000, 0.7071), c2 = filt::lowpass (fs, 10000, 0.7071);
            double sum = 0; int n = 0;
            for (size_t i = 0; i < x.size(); ++i) { double y = l.process (h.process (x[i], c1), c2);
                if (i >= (size_t) (a * fs) && i < (size_t) (b * fs)) { sum += y * y; ++n; } }
            return std::sqrt (sum / std::max (1, n));
        };
        Params base; base.excite = 0; base.air = 0; base.presence = 0; base.brightness = 0; base.eqBypass = true;
        base.deessFreqHz = 7000; base.deessSens = 0.8f; base.deessRangeDb = 6; base.deessOn = true;

        auto sv = makeVoice (true, 5), pv = makeVoice (false, 5);
        auto on = sv, off = sv; Params poff = base; poff.deessOn = false;
        { Engine e; e.prepare (fs); e.setParams (base, true); run (e, on); }
        { Engine e; e.prepare (fs); e.setParams (poff, true); run (e, off); }
        double red = 20 * std::log10 (bandRms (off, 0.6, 0.95) / bandRms (on, 0.6, 0.95));
        { Engine e; e.prepare (fs); e.setParams (base, true); auto t = sv; double mx = 0, mxVowel = 0;
          for (int p0 = 0; p0 < (int) t.size(); p0 += 256) { float* ptr[1] = { t.data() + p0 }; e.process (ptr, 1, std::min (256, (int) t.size() - p0));
              if (p0 > (int) (0.6 * fs) && p0 < (int) (0.95 * fs)) mx = std::max (mx, e.getLastReductionDb());
              if (p0 > 4800 && p0 < (int) (0.45 * fs)) mxVowel = std::max (mxVowel, e.getLastReductionDb()); }
          CHECK (mx > 5.0 && mx <= 6.001, "peak gain reduction in burst reaches RANGE (%.2f dB of 6)", mx);
          CHECK (mxVowel < 0.05, "gain reduction on the vowel stays ~0 (%.3f dB)", mxVowel); }
        std::printf("  info  sibilant-band reduction at RANGE 6 dB: %.2f dB\n", red);
        CHECK (red > 3.0 && red <= 6.3, "detects sibilance and reduces it (%.2f dB, limit 6 dB)", red);

        double redVowel = 20 * std::log10 (bandRms (off, 0.1, 0.4) / std::max (1e-9, bandRms (on, 0.1, 0.4)));
        CHECK (maxDiff (off, on, 0) < 1e-9 || std::fabs (redVowel) < 0.05, "no change on non-sibilant parts (%.3f dB)", redVowel);
        { auto a = pv, b = pv; Engine e1, e2; e1.prepare (fs); e2.prepare (fs); e1.setParams (base, true); e2.setParams (poff, true);
          run (e1, a); run (e2, b); CHECK (maxDiff (a, b, 4800) < 1e-4, "voice without sibilance is practically untouched (max diff %.3g, below -80 dBFS)", maxDiff (a, b, 4800)); }

        for (float rg : { 0.0f, 2.0f, 4.0f })
        { Params q = base; q.deessRangeDb = rg; auto t = sv; Engine e; e.prepare (fs); e.setParams (q, true); run (e, t);
          double r = 20 * std::log10 (bandRms (off, 0.6, 0.95) / bandRms (t, 0.6, 0.95));
          CHECK (r <= rg + 0.35 && (rg == 0.0f ? r < 0.01 : r > 0.5 * rg), "RANGE %.0f dB -> reduction %.2f dB", rg, r); }

        { Params q = base; q.deessSens = 0.0f; auto t = sv; Engine e; e.prepare (fs); e.setParams (q, true); run (e, t);
          Params q2 = base; q2.deessSens = 1.0f; auto t2 = sv; Engine e2; e2.prepare (fs); e2.setParams (q2, true); run (e2, t2);
          double r0 = 20 * std::log10 (bandRms (off, 0.6, 0.95) / bandRms (t, 0.6, 0.95)), r1 = 20 * std::log10 (bandRms (off, 0.6, 0.95) / bandRms (t2, 0.6, 0.95));
          CHECK (r1 >= r0, "SENSITIVITY: more sensitivity never reduces less (%.2f dB -> %.2f dB)", r0, r1); }

        { Params q = base; q.deessFreqHz = 3000; auto a = sv; Engine e; e.prepare (fs); e.setParams (q, true); run (e, a);
          Params q2 = base; q2.deessFreqHz = 12000; auto b = sv; Engine e2; e2.prepare (fs); e2.setParams (q2, true); run (e2, b);
          CHECK (peak (a) > 0 && peak (b) > 0 && std::isfinite (peak (a)) && std::isfinite (peak (b)), "FREQUENCY 3k and 12k both operate"); }

        { Params q = base; q.deessOn = false; Engine e; e.prepare (fs); e.setParams (q, true); auto t = sv; run (e, t);
          Params q0 = base; q0.deessOn = true; q0.deessRangeDb = 0; Engine e0; e0.prepare (fs); e0.setParams (q0, true); auto t0 = sv; run (e0, t0);
          CHECK (maxDiff (t, t0) < 1e-12, "DE-ESS off == RANGE 0 (independent bypass)"); }

        { Params q = base; q.deessListen = true; Engine e; e.prepare (fs); e.setParams (q, true); auto t = sv; run (e, t);
          double lo = goertzel (t, fs, 200, N / 8), loDry = goertzel (sv, fs, 200, N / 8);
          CHECK (peak (t) > 0.05 && lo < 0.05 * loDry, "LISTEN outputs only the detected band (200 Hz at %.1f%% of dry)", 100 * lo / loDry); }

        // stereo / mono
        { Params q = base; Engine em, es; em.prepare (fs); es.prepare (fs); em.setParams (q, true); es.setParams (q, true);
          auto m = sv, l = sv, r = sv; run (em, m); run (es, l, &r);
          CHECK (maxDiff (m, l) == 0.0 && maxDiff (l, r) == 0.0, "de-esser: mono == stereo L == stereo R");
          Engine ei; ei.prepare (fs); ei.setParams (q, true); auto a = sv; std::vector<float> z (a.size(), 0.f); run (ei, a, &z);
          CHECK (peak (z) == 0.0, "de-esser: silent channel stays silent");
          Engine el; el.prepare (fs); el.setParams (q, true); auto L = sv, R = sv; for (auto& x : R) x *= 0.5f; run (el, L, &R);
          double m2 = 0; for (size_t i = 0; i < L.size(); ++i) m2 = std::max (m2, (double) std::fabs (R[i] - 0.5f * L[i]));
          CHECK (m2 < 1e-5, "linked gain keeps stereo balance (L/R ratio error %.2g)", m2); }

        // numerical stability and click safety
        { bool ok = true;
          for (double r : { 22050.0, 44100.0, 96000.0, 192000.0 })
          { Engine e; e.prepare (r); Params q; q.deessFreqHz = 12000; q.deessRangeDb = 6; q.deessSens = 1; q.excite = 1; q.air = 1; q.brightness = 1; q.deessListen = true;
            e.setParams (q, true); auto x = noise ((int) r, 1.0f, 9); auto y = x; run (e, y, nullptr, 97);
            for (float v : y) if (! std::isfinite (v)) ok = false;
            for (int k = 0; k < 40; ++k) { q.deessFreqHz = k % 2 ? 3000.f : 12000.f; q.deessOn = k % 3 != 0; q.deessListen = k % 5 == 0; e.setParams (q);
              auto t = noise (251, 1.0f, k); run (e, t, nullptr, 31); for (float v : t) if (! std::isfinite (v)) ok = false; } }
          CHECK (ok, "stable at extreme settings, 22-192 kHz, toggling on/off/listen");
          Engine e; e.prepare (fs); Params q = base; q.excite = 0.2f; q.air = 0.15f; q.brightness = 0.25f; q.eqBypass = false; e.setParams (q, true);
          auto x = sine (fs, 500, 0.3, N); std::vector<float> y; double jump = 0; float prev = 0; std::mt19937 g (4); std::uniform_real_distribution<float> d (0, 1);
          for (int b = 0; b < N / 64; ++b)
          { q.deessOn = d (g) > 0.5f; q.deessFreqHz = 3000 + 9000 * d (g); q.deessRangeDb = 6 * d (g); q.deessSens = d (g);
            q.presFreqHz = 1000 + 19000 * d (g); q.airFreqHz = 2000 + 18000 * d (g); q.presGainDb = -6 + 12 * d (g); q.airGainDb = -6 + 15 * d (g); q.lowCutHz = 20 + 480 * d (g);
            e.setParams (q); std::vector<float> t (x.begin() + b * 64, x.begin() + b * 64 + 64); run (e, t, nullptr, 64);
            for (float v : t) { if (b > 20) jump = std::max (jump, (double) std::fabs (v - prev)); prev = v; } }
          CHECK (jump < 0.06, "no clicks under random parameter automation every 64 samples (max step %.4f, input max step 0.0196)", jump); }
    }

    std::printf("10. Extended EQ range (2 kHz .. 20 kHz)\n");
    { Params q; q.excite = 0; q.air = 0; q.presence = 0; q.brightness = 0; q.deessOn = false; q.lowCutHz = 20; q.presGainDb = 0;
      q.airFreqHz = 2000; q.airGainDb = 6; double g8 = gainDb (fs, 8000, q), g500 = gainDb (fs, 500, q);
      CHECK (g8 > 4.0 && std::fabs (g500) < 1.0, "air shelf at 2 kHz: +%.1f dB at 8k, %.2f dB at 500 Hz", g8, g500);
      Params w = q; w.airGainDb = 0; w.presFreqHz = 20000; w.presGainDb = 6; w.q = 1.0f;
      double g19 = gainDb (fs, 19000, w); CHECK (g19 > 3.0 && std::isfinite (g19), "presence peak at 20 kHz works (+%.1f dB at 19k)", g19);
      Params w2 = w; w2.presFreqHz = 1000; w2.presGainDb = 6; CHECK (gainDb (fs, 1000, w2) > 4.0, "presence still reaches down to 1 kHz"); }

    std::printf("11. Tonal regression vs v1 (de-esser off)\n");
    { Params q = def; q.deessOn = false; const double v1[] = { -8.78, -1.49, 0.03, 0.14, 1.09, 2.23, 2.73, 4.16 }; const double fr[] = { 50, 100, 500, 1000, 3000, 6000, 10000, 15000 };
      double worst = 0; for (int i = 0; i < 8; ++i) worst = std::max (worst, std::fabs (gainDb (fs, fr[i], q) - v1[i]));
      CHECK (worst < 0.02, "default response identical to v1 (max deviation %.3f dB)", worst);
      Params d2 = def; double w2 = 0; for (int i = 0; i < 8; ++i) w2 = std::max (w2, std::fabs (gainDb (fs, fr[i], d2) - v1[i]));
      std::printf("  info  with DE-ESS enabled at defaults, steady non-sibilant tones deviate by up to %.3f dB\n", w2); }

    std::printf("12. Latency with every stage active\n");
    { Engine e; e.prepare (fs); Params q = def; q.excite = 1; q.air = 1; q.presence = 1; q.brightness = 1; q.deessRangeDb = 6; e.setParams (q, true);
      std::vector<float> imp (64, 0.f); imp[0] = 0.25f; run (e, imp);
      CHECK (e.getLatencySamples() == 0 && std::fabs (imp[0]) > 0.05f, "output responds on sample 0 with all stages on (%.3f)", imp[0]); }

    std::printf("Info: default frequency response (dB)\n  ");
    for (double f : { 50.0, 100.0, 500.0, 1000.0, 3000.0, 6000.0, 10000.0, 15000.0 }) std::printf("%gHz:%.2f  ", f, gainDb (fs, f, def));
    std::printf("\n\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL CHECKS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
