// Banco di prova del cuore DSP: legge audio float32 grezzo (interleaved), lo elabora a blocchi e scrive il risultato.
// Uso: dsp_test in.raw out.raw sr canali threshold q input_db output_db indipendente(0/1) [blocco=480]
#include "../Source/CrestTamerDSP.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
int main (int argc, char** argv)
{
    if (argc < 10) { std::fprintf (stderr, "uso: dsp_test in out sr ch thr q in out indip [blocco]\n"); return 1; }
    const double sr = std::atof (argv[3]); const int ch = std::atoi (argv[4]);
    const double thr = std::atof (argv[5]), q = std::atof (argv[6]), inDb = std::atof (argv[7]), outDb = std::atof (argv[8]);
    const bool ind = std::atoi (argv[9]) != 0; const int block = argc > 10 ? std::atoi (argv[10]) : 480;
    FILE* f = std::fopen (argv[1], "rb"); if (!f) return 2;
    std::fseek (f, 0, SEEK_END); const long bytes = std::ftell (f); std::fseek (f, 0, SEEK_SET);
    const long total = bytes / (long) sizeof (float) / ch; std::vector<float> in ((size_t) (total * ch));
    if (std::fread (in.data(), sizeof (float), in.size(), f) != in.size()) return 3; std::fclose (f);
    crest::CrestTamerDSP dsp; dsp.prepare (sr, ch); dsp.setParameters (thr, q, inDb, outDb, ind); dsp.reset();
    std::vector<float> out (in.size()); std::vector<std::vector<float>> bufs ((size_t) ch, std::vector<float> ((size_t) block));
    float* ptr[crest::CrestTamerDSP::kMaxCh];
    const auto t0 = std::chrono::steady_clock::now(); long minGainBlocks = 0;
    for (long i = 0; i < total; i += block)
    {
        const int m = (int) std::min<long> (block, total - i);
        for (int c = 0; c < ch; ++c) { ptr[c] = bufs[(size_t) c].data(); for (int k = 0; k < m; ++k) ptr[c][k] = in[(size_t) ((i + k) * ch + c)]; }
        dsp.process (ptr, ch, m);
        if (dsp.lastBlockMinGain() < 0.999f) ++minGainBlocks;
        for (int c = 0; c < ch; ++c) for (int k = 0; k < m; ++k) out[(size_t) ((i + k) * ch + c)] = ptr[c][k];
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    f = std::fopen (argv[2], "wb"); std::fwrite (out.data(), sizeof (float), out.size(), f); std::fclose (f);
    std::printf ("latenza=%d larghezza_ms=%.3f tempo=%.3f audio_s=%.2f blocchi_con_riduzione=%ld\n", dsp.latencySamples(), dsp.widthMs(), secs, total / sr, minGainBlocks);
    return 0;
}
