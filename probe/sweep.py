"""Sweep every FlexVoice speech parameter and measure what it actually does.

The Speaker API accepts any number silently, so usable ranges have to be
derived from the rendered audio: duration for rate-like parameters, median F0
for pitch-like ones, RMS/clipping for level, spectral centroid for timbre.

    python sweep.py            # run every sweep, write sweep_results.json
    python sweep.py volume     # run one parameter
"""

import json
import math
import os
import struct
import subprocess
import sys
import wave

ROOT = r"C:\Users\joshk\OneDrive\dev\sapivoice"
EXE = os.path.join(ROOT, "build_probe", "fv_sweep.exe")
DATA = os.path.join(ROOT, "bin", "fv")
TAV = os.path.join(DATA, "English", "default.tav")
TAV_M = os.path.join(DATA, "English", "Voices", "Tim.tav")
OUTROOT = os.path.join(ROOT, "build_probe", "sweep")

TEXT = "She had your dark suit in greasy wash water all year."


def frange(a, b, n):
    if n == 1:
        return [a]
    return [a + (b - a) * i / (n - 1.0) for i in range(n)]


# name -> (scope, type, values, what we expect it to move)
SWEEPS = {
    # --- engine-level (the three the engine exposes through attribute()) ---
    "eng:speechRate": ("engine", "dbl", [0.1, 0.25, 0.4, 0.5, 0.7, 0.85, 1.0, 1.25,
                                         1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 8.0], "dur"),
    "eng:volume": ("engine", "dbl", [0.0, 0.05, 0.1, 0.25, 0.5, 0.75, 1.0, 1.5,
                                     2.0, 3.0, 5.0, 10.0], "rms"),
    "eng:pitchRate": ("engine", "dbl", [0.25, 0.4, 0.5, 0.6, 0.75, 0.9, 1.0, 1.1,
                                        1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0], "f0"),

    # --- speaker-level ---
    "speechRate": ("speaker", "dbl", [0.1, 0.25, 0.5, 0.75, 1.0, 1.055, 1.5, 2.0,
                                      3.0, 4.0, 6.0], "dur"),
    "speedWPM": ("speaker", "int", [30, 60, 90, 120, 150, 180, 219, 260, 300, 400,
                                    500, 700, 900], "dur"),
    "volume": ("speaker", "dbl", [0.0, 1.0, 5.0, 10.0, 25.0, 40.0, 60.0, 80.0,
                                  100.0, 150.0, 200.0], "rms"),
    "defaultPitch": ("speaker", "int", [40, 60, 80, 100, 120, 150, 180, 203, 240,
                                        300, 360, 450, 550], "f0"),
    "pitchRate": ("speaker", "dbl", [0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0], "f0"),
    "pitchMin": ("speaker", "int", [20, 50, 100, 150, 200, 250], "f0"),
    "pitchMax": ("speaker", "int", [150, 250, 350, 500, 700, 1000], "f0"),
    "intonationLevel": ("speaker", "dbl", [0.0, 0.25, 0.5, 1.0, 1.71, 2.5, 3.5,
                                           5.0, 8.0], "f0sd"),
    "headsize": ("speaker", "dbl", [0.3, 0.5, 0.7, 0.85, 0.95, 1.1, 1.3, 1.6,
                                    2.0, 3.0], "cent"),
    "tilt": ("speaker", "dbl", [-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 4.0], "cent"),
    "richness": ("speaker", "dbl", [0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0], "cent"),
    "breathiness": ("speaker", "dbl", [0.0, 0.1, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0], "cent"),
    "creakiness": ("speaker", "dbl", [0.0, 0.1, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0], "cent"),
    "smoothness": ("speaker", "dbl", [0.0, 0.2, 0.4, 0.6, 0.78, 1.0, 1.5, 2.0], "cent"),
    "fricationRate": ("speaker", "dbl", [0.0, 0.05, 0.12, 0.25, 0.5, 1.0, 2.0, 4.0], "cent"),
    "plosiveRate": ("speaker", "dbl", [0.0, 0.1, 0.2, 0.4, 0.7, 1.0, 2.0, 4.0], "cent"),
    "singingPitchRate": ("speaker", "dbl", [0.0, 0.25, 0.63, 1.0, 1.5, 2.0], "f0"),
    "volumeSmoothWindow": ("speaker", "int", [0, 1, 2, 4, 8, 16, 32], "rms"),
}


def read_wav(path):
    with wave.open(path, "rb") as w:
        n, sr = w.getnframes(), w.getframerate()
        raw = w.readframes(n)
    if n == 0:
        return [], sr
    return list(struct.unpack("<%dh" % (len(raw) // 2), raw)), sr


def measure(path):
    """Duration, RMS, peak, clipping, median F0, F0 spread, spectral centroid."""
    s, sr = read_wav(path)
    if not s:
        return {"dur": 0.0, "rms": 0.0, "peak": 0, "clip": 0.0,
                "f0": 0.0, "f0sd": 0.0, "cent": 0.0, "silent": True}

    n = len(s)
    dur = n / float(sr)
    rms = math.sqrt(sum(float(x) * x for x in s) / n)
    peak = max(max(s), -min(s))
    clip = sum(1 for x in s if abs(x) >= 32700) / float(n)

    # Frame-wise autocorrelation F0 over voiced frames only.
    win = int(sr * 0.040)
    hop = int(sr * 0.020)
    lo, hi = int(sr / 500.0), int(sr / 50.0)   # 50..500 Hz
    f0s = []
    cents = []
    for start in range(0, n - win, hop):
        fr = s[start:start + win]
        e = math.sqrt(sum(float(x) * x for x in fr) / win)
        if e < rms * 0.5:
            continue
        mean = sum(fr) / float(win)
        fr = [x - mean for x in fr]
        r0 = sum(x * x for x in fr)
        if r0 <= 0:
            continue
        best, bestlag = 0.0, 0
        for lag in range(lo, min(hi, win - 1)):
            acc = 0.0
            for i in range(0, win - lag, 2):     # decimate for speed
                acc += fr[i] * fr[i + lag]
            acc /= (r0 * 0.5)
            if acc > best:
                best, bestlag = acc, lag
        if best > 0.35 and bestlag:
            f0s.append(sr / float(bestlag))
        # crude spectral centroid via zero-crossing rate (cheap, monotone in
        # brightness, and enough to tell whether a timbre knob does anything)
        zc = sum(1 for i in range(1, win) if (fr[i - 1] < 0) != (fr[i] < 0))
        cents.append(zc * sr / (2.0 * win))

    f0s.sort()
    f0 = f0s[len(f0s) // 2] if f0s else 0.0
    if len(f0s) > 3:
        m = sum(f0s) / len(f0s)
        f0sd = math.sqrt(sum((x - m) ** 2 for x in f0s) / len(f0s))
    else:
        f0sd = 0.0
    cent = (sum(cents) / len(cents)) if cents else 0.0

    return {"dur": round(dur, 3), "rms": round(rms, 1), "peak": peak,
            "clip": round(clip, 5), "f0": round(f0, 1), "f0sd": round(f0sd, 1),
            "cent": round(cent, 1), "silent": rms < 20}


def run(key):
    scope, typ, values, metric = SWEEPS[key]
    name = key.split(":")[-1]
    outdir = os.path.join(OUTROOT, key.replace(":", "_"))
    os.makedirs(outdir, exist_ok=True)

    argv = [EXE, DATA, TAV, outdir, scope, typ, name,
            ",".join(str(v) for v in values), TEXT]
    p = subprocess.run(argv, capture_output=True, text=True, timeout=900)

    rows = []
    for line in p.stdout.splitlines():
        parts = line.split()
        if parts and parts[0] == "VALUE":
            idx, val, path, ms = parts[1], parts[2], parts[3], parts[4]
            m = measure(path)
            m.update({"value": float(val), "ms": int(ms), "file": path})
            rows.append(m)
        elif parts and parts[0] in ("REJECT", "EXCEPTION"):
            rows.append({"value": None, "error": line})
    return {"scope": scope, "type": typ, "metric": metric, "rows": rows,
            "stderr": p.stderr[-2000:] if p.stderr else ""}


def main():
    keys = sys.argv[1:] or list(SWEEPS)
    out = {}
    for k in keys:
        if k not in SWEEPS:
            print("unknown sweep:", k)
            continue
        print("sweeping", k, flush=True)
        try:
            out[k] = run(k)
        except Exception as exc:                     # keep going on one failure
            out[k] = {"error": repr(exc)}
            print("  FAILED:", exc, flush=True)
            continue
        met = out[k]["metric"]
        for r in out[k]["rows"]:
            if r.get("value") is None:
                print("   ", r.get("error"))
                continue
            print("    %-10s dur=%-7s rms=%-8s f0=%-7s f0sd=%-6s cent=%-8s clip=%s%s"
                  % (r["value"], r["dur"], r["rms"], r["f0"], r["f0sd"], r["cent"],
                     r["clip"], "  <-- " + met if met else ""), flush=True)

    dest = os.path.join(ROOT, "build_probe", "sweep_results.json")
    with open(dest, "w") as fh:
        json.dump(out, fh, indent=2)
    print("wrote", dest)


if __name__ == "__main__":
    main()
