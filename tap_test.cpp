// Prova delle "colonne" della forma d'onda: devono descrivere esattamente ingresso e uscita dello stesso istante.
#include "../Source/CrestTamerDSP.h"
#include <cstdio>
#include <random>
int main()
{
    const double sr = 44100.0; const int total = (int) (sr * 6), block = 512;
    std::mt19937 rng (5); std::normal_distribution<float> nd (0.0f, 0.08f);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    for (int i = 0; i < total; ++i) { L[(size_t) i] = nd (rng); R[(size_t) i] = 0.9f * L[(size_t) i]; }
    for (int k = 4000; k < total; k += 9000) { L[(size_t) k] += 0.8f; R[(size_t) k] -= 0.7f; }          // picchi
    int allBad = 0;
    for (int indep = 0; indep < 2; ++indep)
    {
        crest::CrestTamerDSP dsp; dsp.prepare (sr, 2); dsp.setParameters (9.0, 5.0, 0.0, 0.0, indep != 0); dsp.reset();
        const int D = dsp.latencySamples(), cs = std::max (1, (int) (sr / 200.0));
        std::vector<float> oL (L), oR (R); std::vector<crest::WaveColumn> cols; crest::WaveColumn w;
        for (int i = 0; i < total; i += block)
        {
            const int m = std::min (block, total - i); float* p[2] = { oL.data() + i, oR.data() + i };
            dsp.process (p, 2, m);
            while (dsp.popWaveColumn (w)) cols.push_back (w);
        }
        int checked = 0, bad = 0, reduced = 0; float worstCeil = 1e9f;
        for (size_t c = 0; c < cols.size(); ++c)
        {
            const int a = (int) c * cs, b = a + cs;                       // campioni d'ingresso della colonna
            if (b + D > total) break;
            float imn = 1e9f, imx = -1e9f, omn = 1e9f, omx = -1e9f;
            for (int n = a; n < b; ++n)
            {
                imn = std::min ({ imn, L[(size_t) n], R[(size_t) n] }); imx = std::max ({ imx, L[(size_t) n], R[(size_t) n] });
                omn = std::min ({ omn, oL[(size_t) (n + D)], oR[(size_t) (n + D)] });                    // uscita D campioni dopo
                omx = std::max ({ omx, oL[(size_t) (n + D)], oR[(size_t) (n + D)] });
            }
            ++checked;
            if (cols[c].inMin != imn || cols[c].inMax != imx || cols[c].outMin != omn || cols[c].outMax != omx) ++bad;
            if (cols[c].gMinP < 0.999f || cols[c].gMinN < 0.999f) ++reduced;
            worstCeil = std::min (worstCeil, cols[c].ceil);
        }
        std::printf ("indipendente=%d: colonne %zu | controllate %d | ingresso/uscita diversi dal reale: %d | colonne con riduzione: %d | tetto minimo %.4f | latenza %d (%d colonne)\n",
                     indep, cols.size(), checked, bad, reduced, worstCeil, D, D / cs);
        allBad += bad + (reduced == 0 ? 1 : 0) + (checked < 1000 ? 1 : 0);
    }
    std::printf ("%s\n", allBad == 0 ? "RISULTATO: le colonne descrivono esattamente ingresso e uscita" : "RISULTATO: ERRORI");
    return allBad;
}
