"""Measure the per-voice level trim so the roster is loudness-matched.

Each voice's .tav sets an absolute amplitude, and the FlexVoice 2.0 presets
were written against a different volume scale, so out of the box the ported
voices run 8 dB hot and hit the host's limiter while the shipped ones sit well
under it.

Engine-level volume is exactly linear in amplitude (measured: RMS 149 at 0.05,
2990 at 1.0, 5959 at 2.0), so one render per voice at a gain low enough to stay
under the limiter's 29000 knee is enough to solve for the trim that puts every
voice at the same peak.
"""

import json
import os
import struct
import subprocess
import sys
import wave

ROOT = r"C:\Users\joshk\OneDrive\dev\sapivoice"
BIN = os.path.join(ROOT, "build_x86", "bin", "Release")
CLIENT = os.path.join(BIN, "client_test.exe")

TARGET_PEAK = 27000        # about -1.7 dBFS, leaving room for SAPI volume > 100
REF_GAIN = 0.25            # low enough that nothing reaches the limiter
TEXT = ("The quick brown fox jumps over the lazy dog. "
        "This is FlexVoice, speaking as a Windows S A P I five voice.")

NAMES = ["Custom", "Julie", "Kim", "Tim", "Bill", "Julius", "Julia", "Jill", "Kit"]


def peak_at(index, gain, path):
    subprocess.run([CLIENT, "speak", str(index), path, TEXT, str(gain)],
                   capture_output=True, text=True, timeout=120)
    if not os.path.exists(path):
        return 0
    with wave.open(path, "rb") as w:
        raw = w.readframes(w.getnframes())
    if not raw:
        return 0
    s = struct.unpack("<%dh" % (len(raw) // 2), raw)
    return max(max(s), -min(s))


def main():
    tmp = os.path.join(BIN, "_cal.wav")
    trims = {}
    for i, name in enumerate(NAMES):
        peak = peak_at(i, REF_GAIN, tmp)
        if peak <= 0:
            print("  %-8s FAILED" % name)
            continue
        trim = round(REF_GAIN * TARGET_PEAK / float(peak), 3)
        # Verify by rendering at the trim we just solved for.
        check = peak_at(i, trim, tmp)
        trims[name] = trim
        print("  %-8s peak %5d at gain %.2f -> trim %.3f (verified peak %d)"
              % (name, peak, REF_GAIN, trim, check))
    print()
    print(json.dumps(trims, indent=2))
    with open(os.path.join(BIN, "trims.json"), "w") as fh:
        json.dump(trims, fh, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
