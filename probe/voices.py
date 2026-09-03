"""Build the FlexVoice voice roster and render a sample for each.

FlexVoice 3.01 ships only two diphone databases (Julie, female; Tom, male) and
three .tav files, two of which are byte-identical. The extra voices come from
MindMaker's own FlexVoice 2.0 speaker presets, whose parameters are grafted
onto the 3.01 schema -- the 2.0 files themselves make the 3.01 engine abort(),
because they carry the obsolete durationDescr/pitchDescr fields and none of the
model-file names 3.01 requires.

Anything that is not a MindMaker parameter set is marked as such in ORIGIN.
"""

import json
import os
import subprocess
import sys

ROOT = r"C:\Users\joshk\OneDrive\dev\sapivoice"
RENDER = os.path.join(ROOT, "build_probe", "fv_render.exe")
DATA = os.path.join(ROOT, "bin", "fv")
ENG = os.path.join(DATA, "English")
TAV_F = os.path.join(ENG, "default.tav")             # Julie diphone database
TAV_M = os.path.join(ENG, "Voices", "Tim.tav")       # Tom diphone database

TEXT = ("The quick brown fox jumps over the lazy dog. "
        "This is FlexVoice speaking through a Windows S A P I five voice.")

# name -> (base tav, origin, {param: value})
VOICES = {
    # --- shipped with the FlexVoice 3.01 English runtime -------------------
    "Julie": (TAV_F, "FlexVoice 3.01 English/default.tav", {"volume": 31.70}),
    "Kim":   (TAV_F, "FlexVoice 3.01 English/Voices/Kim.tav (identical to default.tav)", {"volume": 31.70}),
    "Tim":   (TAV_M, "FlexVoice 3.01 English/Voices/Tim.tav", {"volume": 44.58}),

    # --- MindMaker FlexVoice 2.0 speaker presets, ported to the 3.01 schema
    # Ported: gender/age and every prosody + voice-source parameter 3.01 still
    # understands. Not ported: volume (2.0 used a different scale), the 24-band
    # equalizer (3.01's Julie base is tuned for 12), and the obsolete
    # durationDescr/pitchDescr/volumeDescr model selectors.
    "Bill": (TAV_F, "FlexVoice 2.0 Bill.tav (parameters ported to 3.01)", {
        "volume": 13.53, "gender": "male", "age": "adult", "defaultPitch": 90, "headsize": 1.2,
        "richness": 0.6, "smoothness": 0.61, "fricationRate": 0.81,
        "plosiveRate": 1.0, "intonationLevel": 1.0, "speechRate": 1.0,
    }),
    "Jill": (TAV_F, "FlexVoice 2.0 Jill.tav (parameters ported to 3.01)", {
        "volume": 14.19, "gender": "female", "age": "child", "defaultPitch": 275, "headsize": 0.74,
        "richness": 0.99, "smoothness": 0.55, "fricationRate": 0.81,
        "plosiveRate": 1.0, "intonationLevel": 1.0, "speechRate": 0.81,
    }),
    "Julius": (TAV_F, "FlexVoice 2.0 Julius.tav (parameters ported to 3.01)", {
        "volume": 18.90, "gender": "male", "age": "adult", "defaultPitch": 90, "headsize": 1.2,
        "richness": 0.6, "smoothness": 0.74, "fricationRate": 0.57,
        "plosiveRate": 0.74, "intonationLevel": 1.0, "speechRate": 1.0,
    }),
    "Kit": (TAV_F, "FlexVoice 2.0 Kit.tav (parameters ported to 3.01)", {
        "volume": 11.41, "gender": "male", "age": "child", "defaultPitch": 115, "headsize": 0.92,
        "richness": 0.76, "smoothness": 0.77, "fricationRate": 0.96,
        "plosiveRate": 1.0, "intonationLevel": 1.02, "speechRate": 1.0,
    }),
    "Julia": (TAV_F, "FlexVoice 2.0 Julie.tav (parameters ported to 3.01)", {
        "volume": 15.53, "gender": "female", "age": "adult", "defaultPitch": 170, "headsize": 1.0,
        "richness": 0.99, "smoothness": 0.78, "fricationRate": 0.2,
        "plosiveRate": 1.2, "intonationLevel": 1.0, "speechRate": 1.0,
    }),
}


def render(name, outdir):
    base, origin, params = VOICES[name]
    out = os.path.join(outdir, "%s.wav" % name)
    argv = [RENDER, DATA, base, out, TEXT]
    for k, v in params.items():
        argv.append("%s=%s" % (k, v))
    p = subprocess.run(argv, capture_output=True, text=True, timeout=180)
    ok = p.returncode == 0 and os.path.exists(out)
    return {"name": name, "origin": origin, "base": os.path.basename(base),
            "params": params, "ok": ok, "wav": out if ok else None,
            "out": (p.stdout or "").strip().splitlines()[-1:] or [(p.stderr or "").strip()[:200]]}


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build_probe", "roster")
    os.makedirs(outdir, exist_ok=True)
    results = []
    for name in VOICES:
        r = render(name, outdir)
        results.append(r)
        print("%-8s %-6s %s" % (name, "OK" if r["ok"] else "FAIL", r["out"][0] if r["out"] else ""))
    with open(os.path.join(outdir, "roster.json"), "w") as fh:
        json.dump(results, fh, indent=2)


if __name__ == "__main__":
    main()


def calibrate(outdir):
    """Pick a per-voice `volume` that peaks near -2 dBFS without clipping.

    Engine volume is linear in amplitude, so one quiet reference render is
    enough to solve for the value that hits the target peak.
    """
    import math, struct, wave
    TARGET = 27000          # ~ -1.7 dBFS, leaves headroom for SAPI volume > 100%
    REF = 8.0
    out = {}
    for name in VOICES:
        base, origin, params = VOICES[name]
        probe = os.path.join(outdir, "_cal_%s.wav" % name)
        argv = [RENDER, DATA, base, probe, TEXT, "volume=%g" % REF]
        for k, v in params.items():
            if k != "volume":
                argv.append("%s=%s" % (k, v))
        p = subprocess.run(argv, capture_output=True, text=True, timeout=180)
        if p.returncode != 0 or not os.path.exists(probe):
            print("  %-8s calibration FAILED" % name)
            continue
        w = wave.open(probe, "rb"); n = w.getnframes()
        s = struct.unpack("<%dh" % n, w.readframes(n)); w.close()
        peak = max(max(s), -min(s)) or 1
        vol = round(REF * TARGET / float(peak), 2)
        out[name] = vol
        print("  %-8s ref peak %5d @ vol %g  ->  volume=%.2f" % (name, peak, REF, vol))
        os.remove(probe)
    return out
