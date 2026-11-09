"""Confronta il cuore DSP in C++ (dsp_test) con il prototipo Python. Si esegue dopo aver compilato dsp_test:
    g++ -std=c++17 -O2 -o dsp_test dsp_test.cpp        (Windows: dsp_test.exe)
    python compare_with_python.py
"""
import os, subprocess, sys, tempfile
import numpy as np
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "..", "..", "prototipo_vst3"))
from crest_tamer_rt import RealtimeCrestTamer
exe = os.environ.get("DSP_TEST_EXE") or os.path.join(here, "dsp_test.exe" if os.name == "nt" else "dsp_test")

def music(seconds, sr=44100, seed=1):
    rng = np.random.default_rng(seed); n = int(seconds * sr); t = np.arange(n) / sr
    x = 0.12 * np.sin(2 * np.pi * 55 * t) + 0.08 * np.sin(2 * np.pi * 220 * t) + 0.05 * np.sin(2 * np.pi * 330 * t)
    for k in np.arange(0, seconds, 0.5):
        i = int(k * sr); m = min(n - i, 4000); tt = np.arange(m) / sr
        x[i:i + m] += 0.7 * np.exp(-tt * 40) * np.sin(2 * np.pi * (60 + 80 * np.exp(-tt * 30)) * tt)
    for k in np.arange(0.25, seconds, 0.5):
        i = int(k * sr); m = min(n - i, 5000); x[i:i + m] += 0.5 * np.exp(-np.arange(m) / sr * 35) * rng.standard_normal(m)
    return np.stack([x, 0.85 * x + 0.03 * rng.standard_normal(n)], axis=1).astype(np.float32)

ok = True
d = tempfile.mkdtemp()
for sr in (44100, 48000):
    x = music(6, sr)
    for ind in (0, 1):
        for q, thr, gi, go, blk in ((5.0, 9.0, 0.0, 0.0, 480), (1.0, 10.0, 3.0, -2.0, 512), (25.0, 8.0, 0.0, 0.0, 64), (2.0, 12.0, -3.0, 1.5, 4096)):
            xi = (x * 1.0).astype(np.float32); inp, outp = os.path.join(d, "i.raw"), os.path.join(d, "o.raw"); xi.tofile(inp)
            r = subprocess.run([exe, inp, outp, str(sr), "2", str(thr), str(q), str(gi), str(go), str(ind), str(blk)], capture_output=True, text=True)
            info = dict(kv.split("=") for kv in r.stdout.split())
            D = int(info["latenza"]); cpp = np.fromfile(outp, np.float32).reshape(-1, 2)
            py = RealtimeCrestTamer(sr, 2, threshold_db=thr, q=q, input_db=gi, output_db=go, independent=bool(ind))
            L = py.latency; out_py = py.process(xi)
            # C++ ha latenza fissa D (50 ms); Python ha L = larghezza di Q: py[j] corrisponde a cpp[j + D - L]
            a = cpp[D:]; b = out_py[L:len(xi) - (D - L)]; m = min(len(a), len(b)); diff = float(np.abs(a[:m] - b[:m]).max())
            good = diff < 2e-5 and 0 < float(np.abs(a).max()) 
            ok &= good
            print("sr %5d indip=%d Q=%4.1f thr=%4.1f in=%+.0f out=%+.1f blocco=%4d: latenza C++ %4d (=%.1f ms) | larghezza %.1f ms | differenza max C++/Python %.1e  %s"
                  % (sr, ind, q, thr, gi, go, blk, D, 1000 * D / sr, float(info["larghezza_ms"]), diff, "OK" if good else "ERRORE"))
print("RISULTATO:", "il C++ coincide con il prototipo Python" if ok else "CI SONO DIFFERENZE")
sys.exit(0 if ok else 1)
