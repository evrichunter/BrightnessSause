// Brightness Sause By Evric Hunter - DSP core
// Framework-independent, causal, zero-latency, allocation-free in process().
// Chain: INPUT -> VINTAGE EXCITER -> AIR ENHANCER -> PARAMETRIC EQ -> (parallel MIX) -> OUTPUT
#pragma once
#include <algorithm>
#include <cmath>

namespace bs
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxChannels = 2;
constexpr int kControlRate = 16; // coefficient refresh interval in samples

struct Params
{
    // Vintage exciter
    float excite = 0.20f;     // 0..1
    float character = 0.5f;   // 0 warm .. 1 bright
    float harmonics = 0.5f;   // 0..1
    // Air enhancer
    float air = 0.15f;        // 0..1
    float presence = 0.0f;    // 0..1 (upper-mid enhancement)
    float softness = 0.5f;    // 0..1
    float brightness = 0.25f; // 0..1 macro: broad high shelf lift in the air stage
    // Parametric EQ
    float lowCutHz = 80.0f;
    float presFreqHz = 4000.0f;
    float presGainDb = 1.0f;
    float airFreqHz = 12000.0f;
    float airGainDb = 2.0f;
    float q = 0.8f;
    bool eqBypass = false;
    // Global
    float mix = 1.0f;         // 0..1
    float outputDb = 0.0f;    // -12..+12
    bool bypass = false;
};

// ---------------------------------------------------------------- helpers
struct Smoother
{
    double cur = 0, target = 0, a = 0;
    void setup (double sr, double ms) { a = std::exp (-1.0 / (0.001 * ms * sr)); }
    void snap (double v) { cur = target = v; }
    void set (double t) { target = t; }
    inline double next()
    {
        cur = target + (cur - target) * a;
        if (std::fabs (cur - target) < 1.0e-9) cur = target;
        return cur;
    }
};

struct Coef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

struct Biquad
{
    double z1 = 0, z2 = 0;
    inline double process (double x, const Coef& c)
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        if (std::fabs (z1) < 1.0e-30) z1 = 0; // flush denormals
        if (std::fabs (z2) < 1.0e-30) z2 = 0;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

namespace filt
{
inline double clampF (double f, double fs) { return std::min (std::max (f, 10.0), 0.45 * fs); }

inline Coef norm (double b0, double b1, double b2, double a0, double a1, double a2)
{
    Coef c;
    c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
    return c;
}

inline Coef highpass (double fs, double f, double Q)
{
    f = clampF (f, fs);
    const double w = 2 * kPi * f / fs, cw = std::cos (w), al = std::sin (w) / (2 * Q);
    return norm ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
}

inline Coef lowpass (double fs, double f, double Q)
{
    f = clampF (f, fs);
    const double w = 2 * kPi * f / fs, cw = std::cos (w), al = std::sin (w) / (2 * Q);
    return norm ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
}

inline Coef peaking (double fs, double f, double Q, double dB)
{
    f = clampF (f, fs);
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2 * kPi * f / fs, cw = std::cos (w), al = std::sin (w) / (2 * Q);
    return norm (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
}

// High shelf with RBJ shelf-slope parameter S (S<=1 is monotonic, smaller = gentler)
inline Coef highShelfS (double fs, double f, double dB, double S)
{
    f = clampF (f, fs);
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2 * kPi * f / fs, cw = std::cos (w), sw = std::sin (w);
    const double al = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
    const double tsa = 2.0 * std::sqrt (A) * al;
    return norm (A * ((A + 1) + (A - 1) * cw + tsa),
                 -2 * A * ((A - 1) + (A + 1) * cw),
                 A * ((A + 1) + (A - 1) * cw - tsa),
                 (A + 1) - (A - 1) * cw + tsa,
                 2 * ((A - 1) - (A + 1) * cw),
                 (A + 1) - (A - 1) * cw - tsa);
}

inline Coef highShelfQ (double fs, double f, double dB, double Q)
{
    f = clampF (f, fs);
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2 * kPi * f / fs, cw = std::cos (w), sw = std::sin (w);
    const double tsa = 2.0 * std::sqrt (A) * (sw / (2 * Q));
    return norm (A * ((A + 1) + (A - 1) * cw + tsa),
                 -2 * A * ((A - 1) + (A + 1) * cw),
                 A * ((A + 1) + (A - 1) * cw - tsa),
                 (A + 1) - (A - 1) * cw + tsa,
                 2 * ((A - 1) - (A + 1) * cw),
                 (A + 1) - (A - 1) * cw - tsa);
}
} // namespace filt

// ---------------------------------------------------------------- engine
class Engine
{
public:
    static constexpr double kFloor = 1.0e-4; // envelope floor (-80 dBFS)

    void prepare (double sampleRate)
    {
        fs = sampleRate > 1000.0 ? sampleRate : 44100.0;
        const double ms = 20.0;
        for (auto* s : { &sExcite, &sChar, &sHarm, &sAir, &sPres, &sSoft, &sBright, &sLow, &sPF, &sPG,
                         &sAF, &sAG, &sQ, &sEqOn, &sMix, &sOut, &sByp })
            s->setup (fs, ms);
        aAttack = std::exp (-1.0 / (0.001 * 1.0 * fs));  // 1 ms
        aRelease = std::exp (-1.0 / (0.001 * 60.0 * fs)); // 60 ms
        aSibAtt = std::exp (-1.0 / (0.001 * 0.5 * fs));
        aSibRel = std::exp (-1.0 / (0.001 * 40.0 * fs));
        setParams (Params {}, true);
        reset();
    }

    void reset()
    {
        for (auto& c : ch) c = Chan {};
        updateCoefs();
        tick = 0;
    }

    void setParams (const Params& p, bool snap = false)
    {
        const double outLin = std::pow (10.0, (double) clampf (p.outputDb, -12.f, 12.f) / 20.0);
        const double v[] = { clampf (p.excite, 0, 1), clampf (p.character, 0, 1), clampf (p.harmonics, 0, 1),
                             clampf (p.air, 0, 1), clampf (p.presence, 0, 1), clampf (p.softness, 0, 1),
                             clampf (p.brightness, 0, 1), clampf (p.lowCutHz, 20, 500), clampf (p.presFreqHz, 1000, 10000),
                             clampf (p.presGainDb, -6, 6), clampf (p.airFreqHz, 5000, 20000), clampf (p.airGainDb, -6, 9),
                             clampf (p.q, 0.3f, 4.0f), p.eqBypass ? 0.0 : 1.0, clampf (p.mix, 0, 1), outLin,
                             p.bypass ? 1.0 : 0.0 };
        Smoother* s[] = { &sExcite, &sChar, &sHarm, &sAir, &sPres, &sSoft, &sBright, &sLow, &sPF, &sPG,
                          &sAF, &sAG, &sQ, &sEqOn, &sMix, &sOut, &sByp };
        for (int i = 0; i < 17; ++i)
        {
            if (snap) s[i]->snap (v[i]); else s[i]->set (v[i]);
        }
        if (snap) updateCoefs();
    }

    // In-place processing. Handles 1 or 2 channels (extra channels are left untouched).
    void process (float* const* io, int numCh, int numSamples)
    {
        numCh = std::min (numCh, kMaxChannels);
        for (int n = 0; n < numSamples; ++n)
        {
            advanceSmoothers();
            if ((tick++ % kControlRate) == 0) updateCoefs();

            const double mix = sMix.cur, outG = sOut.cur, byp = sByp.cur;
            for (int c = 0; c < numCh; ++c)
            {
                double dry = io[c][n];
                if (! std::isfinite (dry)) dry = 0.0;
                double wet = processWet (ch[c], dry);
                if (! std::isfinite (wet)) { ch[c] = Chan {}; wet = dry; }

                const double processed = (dry + mix * (wet - dry)) * outG;
                double out = dry + (1.0 - byp) * (processed - dry);
                if (! std::isfinite (out)) out = 0.0;
                io[c][n] = (float) out;
            }
        }
    }

    int getLatencySamples() const noexcept { return 0; }

private:
    struct Chan
    {
        Biquad exHP, exLP, postHP, postLP, airShelf, presPk, brShelf, sibHP, eqHP, eqPk, eqSh;
        double envBand = 0, envFull = 0, envSib = 0;
    };

    static double clampf (double v, double lo, double hi) { return std::min (std::max (v, lo), hi); }

    void advanceSmoothers()
    {
        for (auto* s : { &sExcite, &sChar, &sHarm, &sAir, &sPres, &sSoft, &sBright, &sLow, &sPF, &sPG,
                         &sAF, &sAG, &sQ, &sEqOn, &sMix, &sOut, &sByp })
            s->next();
    }

    void updateCoefs()
    {
        // --- exciter
        const double c = sChar.cur;
        const double fcHP = 1800.0 * std::pow (2.0, 1.5 * c);                 // 1.8k (warm) .. ~5k (bright)
        const double fcPre = std::min (0.13 * fs, 7000.0);                    // limits band so 3rd harmonic stays below Nyquist
        const double fcPost = std::min (0.42 * fs, 9000.0 + 8000.0 * c);      // tames generated harmonics / aliasing
        cExHP = filt::highpass (fs, fcHP, 0.7071);
        cExLP = filt::lowpass (fs, std::max (fcPre, fcHP * 1.5), 0.7071);
        cPostHP = filt::highpass (fs, fcHP * 1.2, 0.7071);
        cPostLP = filt::lowpass (fs, fcPost, 0.7071);
        drive = 1.0 + 4.0 * sHarm.cur;
        bias = (1.0 - c) * 0.35;
        const double tb = std::tanh (bias);
        shaperNorm = 1.0 / (drive * (1.0 - tb * tb));
        exAmt = sExcite.cur;

        // --- air
        const double S = 1.0 - 0.65 * sSoft.cur;
        cAir = filt::highShelfS (fs, 10000.0, 9.0 * sAir.cur, S);
        cPres = filt::peaking (fs, 3200.0, 0.9, 5.0 * sPres.cur);
        cBright = filt::highShelfS (fs, 5000.0, 6.0 * sBright.cur, 0.6);
        cSibHP = filt::highpass (fs, 5500.0, 0.7071);

        // --- EQ
        cEqHP = filt::highpass (fs, sLow.cur, 0.7071);
        cEqPk = filt::peaking (fs, sPF.cur, sQ.cur, sPG.cur);
        cEqSh = filt::highShelfQ (fs, sAF.cur, sAG.cur, 0.6);
        eqOn = sEqOn.cur;
    }

    double processWet (Chan& s, double x)
    {
        // ---------- A. VINTAGE EXCITER (additive, band-limited harmonic generator)
        double y = x;
        {
            const double af = std::fabs (x); // shared full-band level tracker (used by exciter and sibilance guard)
            s.envFull = af > s.envFull ? af + (s.envFull - af) * aAttack : af + (s.envFull - af) * aRelease;
        }
        if (exAmt > 0.0)
        {
            const double band = s.exLP.process (s.exHP.process (x, cExHP), cExLP);
            const double ab = std::fabs (band);
            s.envBand = ab > s.envBand ? ab + (s.envBand - ab) * aAttack : ab + (s.envBand - ab) * aRelease;

            const double u = band / (s.envBand + kFloor);                      // level-normalised
            const double shaped = (std::tanh (drive * u + bias) - std::tanh (bias)) * shaperNorm;
            const double gen = s.envBand * shaped;                            // restore level, smooth nonlinearity

            // already-bright input protection
            const double ratio = s.envBand / (s.envFull + kFloor);
            const double prot = 1.0 - 0.7 * clampf ((ratio - 0.3) / 0.4, 0.0, 1.0);

            const double add = s.postLP.process (s.postHP.process (gen, cPostHP), cPostLP);
            y = x + exAmt * 0.6 * prot * add;
        }
        else
        {
            // keep detector filters warm so engaging the stage is click-free
            const double band = s.exLP.process (s.exHP.process (x, cExHP), cExLP);
            s.postLP.process (s.postHP.process (0.0 * band, cPostHP), cPostLP);
            s.envBand = 0;
        }

        // ---------- B. AIR ENHANCER (shelf + presence, with sibilance guard)
        {
            double a = s.airShelf.process (y, cAir);
            a = s.presPk.process (a, cPres);
            a = s.brShelf.process (a, cBright);

            const double sib = std::fabs (s.sibHP.process (y, cSibHP));
            s.envSib = sib > s.envSib ? sib + (s.envSib - sib) * aSibAtt : sib + (s.envSib - sib) * aSibRel;
            const double r = s.envSib / (s.envFull + kFloor);
            const double guard = 1.0 - 0.5 * clampf ((r - 0.5) / 0.4, 0.0, 1.0);
            y = y + guard * (a - y);
        }

        // ---------- C. PARAMETRIC EQ (minimum phase biquads, crossfaded bypass)
        {
            double e = s.eqHP.process (y, cEqHP);
            e = s.eqPk.process (e, cEqPk);
            e = s.eqSh.process (e, cEqSh);
            y = y + eqOn * (e - y);
        }
        return y;
    }

    double fs = 44100.0;
    Chan ch[kMaxChannels];
    Smoother sExcite, sChar, sHarm, sAir, sPres, sSoft, sBright, sLow, sPF, sPG, sAF, sAG, sQ, sEqOn, sMix, sOut, sByp;
    Coef cExHP, cExLP, cPostHP, cPostLP, cAir, cPres, cBright, cSibHP, cEqHP, cEqPk, cEqSh;
    double aAttack = 0, aRelease = 0, aSibAtt = 0, aSibRel = 0;
    double drive = 1, bias = 0, shaperNorm = 1, exAmt = 0, eqOn = 1;
    unsigned tick = 0;
};
} // namespace bs
