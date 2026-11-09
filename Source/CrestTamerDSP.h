// Crest Tamer – cuore dell'elaborazione in tempo reale (C++17, nessuna dipendenza da JUCE).
//
// Stesso principio del programma attuale (niente compressore/limiter): ogni picco sopra il "tetto" viene
// abbassato da solo con una curva morbida (finestra di Hann) larga 50/Q millisecondi.
//   tetto(n) = RMS_mobile(n) * 10^(THRESHOLD/20)          (RMS mobile ~3 s, media esponenziale)
//   guadagno = minimo scorrevole dei guadagni richiesti, poi media pesata con la finestra di Hann
// LATENZA FISSA = 50 ms (larghezza massima, Q = 1): il programma ospite la compensa (PDC) e non cambia
// girando Q. Con Q più alto la finestra è più stretta e usa solo una parte del ritardo disponibile.
// Nessuna allocazione e nessun blocco in process(): adatto al thread audio.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace crest
{
class CrestTamerDSP
{
public:
    static constexpr double kMaxWidthMs = 50.0;   // larghezza con Q = 1 (e latenza del plugin)
    static constexpr double kQRefMs     = 50.0;   // larghezza del ritocco = 50 / Q  (ms)
    static constexpr int    kMaxCh      = 8;
    static constexpr double kPi         = 3.14159265358979323846;   // (M_PI non esiste in Visual Studio)

    // Da chiamare fuori dal thread audio (prepareToPlay): alloca tutta la memoria.
    void prepare (double sampleRate, int numChannels)
    {
        sr_ = sampleRate;
        ch_ = std::max (1, std::min (numChannels, kMaxCh));
        maxHalf_ = std::max (1, (int) (sr_ * kMaxWidthMs / 1000.0 / 2.0));
        wMax_ = 2 * maxHalf_ + 1;
        latency_ = 2 * maxHalf_;
        delay_.assign ((size_t) ch_, std::vector<float> ((size_t) latency_, 0.0f));
        envR_ = latency_ + wMax_ + 8;
        envP_.init (envR_);
        envN_.init (envR_);
        dqP_.init ((size_t) wMax_ + 2);
        dqN_.init ((size_t) wMax_ + 2);
        win_.assign ((size_t) wMax_, 0.0);
        rmsN_ = std::max (1.0, rmsSeconds_ * sr_);
        initCount_ = initWeightMs_ / 1000.0 * sr_;
        smoothCoef_ = 1.0 - std::exp (-1.0 / (0.02 * sr_));       // ~20 ms
        q_ = -1.0;                                                // forza il calcolo della finestra
        firstParams_ = true;
        setParameters (thrDb_, 5.0, inDb_, outDb_, independent_);
        reset();
    }

    void reset()
    {
        for (auto& d : delay_) std::fill (d.begin(), d.end(), 0.0f);
        envP_.fill (1.0);
        envN_.fill (1.0);
        dqP_.clear();
        dqN_.clear();
        delayPos_ = 0;
        ms_ = std::pow (10.0, initRmsDb_ / 10.0);
        count_ = initCount_;
        n_ = 0;
        kCur_ = kTgt_;
        inCur_ = inTgt_;
        outCur_ = outTgt_;
    }

    // Parametri (si possono cambiare in ogni blocco: threshold, gain e volume sono "smussati").
    void setParameters (double thresholdDb, double q, double inputDb, double outputDb, bool independent)
    {
        thrDb_ = thresholdDb; inDb_ = inputDb; outDb_ = outputDb; independent_ = independent;
        kTgt_   = std::pow (10.0, thresholdDb / 20.0);
        inTgt_  = std::pow (10.0, inputDb / 20.0);
        outTgt_ = std::pow (10.0, outputDb / 20.0);
        if (firstParams_) { kCur_ = kTgt_; inCur_ = inTgt_; outCur_ = outTgt_; firstParams_ = false; }
        if (std::abs (q - q_) > 1.0e-9 && sr_ > 0.0)
        {
            q_ = q;
            half_ = std::max (1, std::min (maxHalf_, (int) (sr_ * (kQRefMs / q) / 1000.0 / 2.0)));
            W_ = 2 * half_ + 1;
            double sum = 0.0;                                      // finestra di Hann normalizzata
            for (int k = 0; k < W_; ++k)
            {
                win_[(size_t) k] = 0.5 - 0.5 * std::cos (2.0 * kPi * (k + 1) / (W_ + 1));
                sum += win_[(size_t) k];
            }
            for (int k = 0; k < W_; ++k) win_[(size_t) k] /= sum;
        }
    }

    int    latencySamples() const noexcept { return latency_; }
    double widthMs() const noexcept        { return 1000.0 * (2 * half_ + 1) / sr_; }
    // guadagno minimo applicato nell'ultimo blocco (1 = nessuna riduzione): per il meter di riduzione
    float  lastBlockMinGain() const noexcept { return (float) blockMinGain_; }

    // Elabora in place. data[canale][campione]; funziona con blocchi di qualunque lunghezza.
    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        const int nch = std::min (numChannels, ch_);
        blockMinGain_ = 1.0;
        for (int i = 0; i < numSamples; ++i)
        {
            kCur_   += (kTgt_   - kCur_)   * smoothCoef_;
            inCur_  += (inTgt_  - inCur_)  * smoothCoef_;
            outCur_ += (outTgt_ - outCur_) * smoothCoef_;
            const float inG = (float) inCur_, outG = (float) outCur_;

            float x[kMaxCh];
            double e = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                x[c] = data[c][i] * inG;
                e += (double) x[c] * (double) x[c];
            }
            e /= nch;

            // RMS mobile: media esponenziale con avvio "a finestra crescente"
            count_ = std::min (count_ + 1.0, rmsN_);
            ms_ += (e - ms_) / count_;
            const double ceil = std::max (std::sqrt (ms_) * kCur_, floorLin_);

            // guadagno richiesto da questo campione (1 = nessuno), per picchi positivi e negativi
            double needP = 1.0, needN = 1.0;
            for (int c = 0; c < nch; ++c)
            {
                const double v = (double) x[c];
                if (v >  ceil) needP = std::min (needP, ceil / std::max ( v, 1e-30));
                if (v < -ceil) needN = std::min (needN, ceil / std::max (-v, 1e-30));
            }

            double gp, gn;
            if (independent_)
            {
                gp = smooth (dqP_, envP_, needP);
                gn = smooth (dqN_, envN_, needN);
            }
            else
            {
                gp = gn = smooth (dqP_, envP_, std::min (needP, needN));
            }
            blockMinGain_ = std::min (blockMinGain_, std::min (gp, gn));

            // audio ritardato di LATENZA campioni, scalato dal guadagno del proprio segno
            for (int c = 0; c < nch; ++c)
            {
                auto& d = delay_[(size_t) c];
                const float dv = d[(size_t) delayPos_];
                d[(size_t) delayPos_] = x[c];
                data[c][i] = (dv * (float) (dv >= 0.0f ? gp : gn)) * outG;
            }
            if (++delayPos_ == latency_) delayPos_ = 0;
            ++n_;
        }
    }

private:
    // Minimo scorrevole su una finestra di W campioni (coda monotona, senza allocazioni).
    struct MinDeque
    {
        std::vector<int64_t> idx; std::vector<double> val; uint64_t head = 0, tail = 0, mask = 0;
        void init (size_t cap) { size_t c = 1; while (c < cap) c <<= 1; idx.assign (c, 0); val.assign (c, 0.0); mask = c - 1; head = tail = 0; }
        void clear() { head = tail = 0; }
        double push (int64_t n, double v, int W)
        {
            while (tail > head && val[(tail - 1) & mask] >= v) --tail;
            idx[tail & mask] = n; val[tail & mask] = v; ++tail;
            while (idx[head & mask] <= n - W) ++head;
            return val[head & mask];
        }
    };

    // Ultimi valori dell'inviluppo in un buffer circolare "specchiato": ogni finestra è contigua in memoria.
    struct EnvRing
    {
        std::vector<double> buf; int R = 0, pos = 0;
        void init (int r) { R = r; buf.assign ((size_t) (2 * R), 1.0); pos = 0; }
        void fill (double v) { std::fill (buf.begin(), buf.end(), v); pos = 0; }
        void push (double v) { buf[(size_t) pos] = v; buf[(size_t) (pos + R)] = v; if (++pos == R) pos = 0; }
        const double* window (int back, int W) const     // W valori che finiscono 'back' posizioni prima dell'ultimo
        {
            int end = pos - 1 - back; while (end < 0) end += R;
            int start = end - (W - 1); while (start < 0) start += R;
            return buf.data() + start;
        }
    };

    double smooth (MinDeque& dq, EnvRing& env, double need)
    {
        env.push (dq.push ((int64_t) n_, need, W_));          // env[n - half]
        const double* p = env.window (latency_ - 2 * half_, W_);   // finestra centrata sul campione n - LATENZA
        double g = 0.0;
        for (int k = 0; k < W_; ++k) g += win_[(size_t) k] * p[k];
        return std::min (1.0, std::max (0.0, g));
    }

    double sr_ = 44100.0;
    int ch_ = 2, maxHalf_ = 1, wMax_ = 3, latency_ = 2, half_ = 1, W_ = 3, envR_ = 16, delayPos_ = 0;
    double q_ = -1.0, thrDb_ = 12.0, inDb_ = 0.0, outDb_ = 0.0;
    bool independent_ = false, firstParams_ = true;
    double kTgt_ = 1.0, inTgt_ = 1.0, outTgt_ = 1.0, kCur_ = 1.0, inCur_ = 1.0, outCur_ = 1.0, smoothCoef_ = 0.001;
    double rmsSeconds_ = 3.0, initRmsDb_ = -18.0, initWeightMs_ = 50.0, floorLin_ = std::pow (10.0, -90.0 / 20.0);
    double rmsN_ = 1.0, initCount_ = 1.0, ms_ = 0.0, count_ = 1.0, blockMinGain_ = 1.0;
    int64_t n_ = 0;
    std::vector<std::vector<float>> delay_;
    std::vector<double> win_;
    MinDeque dqP_, dqN_;
    EnvRing envP_, envN_;
};
} // namespace crest
