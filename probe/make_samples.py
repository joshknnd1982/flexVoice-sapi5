"""Render the sample set that demonstrates every FlexVoice voice and parameter.

Everything here goes through the finished wrapper -- the pipe, the host, the
normalizer -- not the bare engine, so the samples show what a user actually
gets. Output:

    samples/
      01-voices/       one file per registered voice
      02-parameters/   each of the 14 speech parameters at 0, 25, 50, 75, 100 %
      03-sapi/         SAPI rate, pitch and volume through the real SAPI stack
      04-text/         inputs that crash or wedge the bare engine, spoken safely
      05-characters/   single characters and spell-out, both silent before 1.0.2
      README.txt       what each file is
"""

import os
import struct
import subprocess
import sys
import wave

ROOT = r"C:\Users\joshk\OneDrive\dev\sapivoice"
BIN = os.path.join(ROOT, "build_x86", "bin", "Release")
CLIENT = os.path.join(BIN, "client_test.exe")
SAPI = os.path.join(BIN, "sapi_probe.exe")
OUT = os.path.join(ROOT, "samples")

VOICES = [
    (0, "FlexVoice Custom Voice", "configurable; shown at its factory defaults"),
    (1, "FlexVoice Julie", "FlexVoice 3.01 default.tav, the Julie diphone database"),
    (2, "FlexVoice Kim", "Voices/Kim.tav, byte-identical to default.tav"),
    (3, "FlexVoice Tim", "Voices/Tim.tav, the Tom diphone database"),
    (4, "FlexVoice Bill", "FlexVoice 2.0 Bill preset, ported to the 3.01 schema"),
    (5, "FlexVoice Julius", "FlexVoice 2.0 Julius preset, ported"),
    (6, "FlexVoice Julia", "FlexVoice 2.0 Julie preset, ported"),
    (7, "FlexVoice Jill", "FlexVoice 2.0 Jill preset, ported (child, female)"),
    (8, "FlexVoice Kit", "FlexVoice 2.0 Kit preset, ported (child, male)"),
]

VOICE_TEXT = ("The quick brown fox jumps over the lazy dog. "
              "This is FlexVoice, speaking as a Windows S A P I five voice.")

PARAM_TEXT = "She had your dark suit in greasy wash water all year."

# name, ini key, what the extremes sound like
PARAMS = [
    ("rate",        "Rate",         "0 is slowest, 100 is fastest"),
    ("volume",      "Volume",       "0 is silence, 100 is full level"),
    ("pitch",       "Pitch",        "0 is 50 Hz, 100 is 450 Hz"),
    ("pitchScale",  "PitchScale",   "scales the whole pitch contour"),
    ("pitchFloor",  "PitchFloor",   "hard floor on the pitch contour"),
    ("pitchCeil",   "PitchCeiling", "hard ceiling; a low one flattens the voice"),
    ("intonation",  "Intonation",   "0 is monotone, 100 is highly expressive"),
    ("headSize",    "HeadSize",     "vocal tract scale"),
    ("tilt",        "Tilt",         "spectral tilt"),
    ("richness",    "Richness",     "voice source richness"),
    ("breathiness", "Breathiness",  "added breath noise"),
    ("smoothness",  "Smoothness",   "voice source smoothing"),
    ("frication",   "Frication",    "strength of s, f, sh sounds"),
    ("plosive",     "Plosives",     "strength of p, t, k bursts"),
]

# One character at a time, which is what arrowing through a document sends,
# and the spell-word command. Both were silent before 1.0.2.
CHAR_CASES = [
    ("letters", ["a", "b", "z", "Q"]),
    ("digits", ["0", "3", "7"]),
    ("punctuation", [",", ".", "-", "(", "?", "%", "/", "@"]),
]
SPELL_CASES = ["cat", "hello", "NVDA", "flexvoice"]

TEXT_CASES = [
    ("numbers", "Chapter 3 has 1999 items, 12 of them at 12:34 PM."),
    ("decimals-and-money", "Pi is 3.14159, the price is $19.99, that is 50% off."),
    ("paths-and-urls", "Open C:\\Users\\joshk\\Documents or https://example.com/path"),
    ("email", "Write to joshknnd1982@gmail.com about it."),
    ("acronyms", "NVDA and JAWS both read HTML and use the SAPI API."),
    ("code", "int x = 0; if (a == b) return; SELECT * FROM t WHERE id=5"),
    ("contractions", "It's fine, you can't tell, we'll see, I'd rather not."),
    ("accents", "The eleve was naive at the cafe in Zurich."),
]


def ini_path():
    return os.path.join(os.environ["APPDATA"], "FlexVoiceSAPI", "settings.ini")


def write_settings(overrides):
    """Write settings.ini with every parameter at its default bar the overrides."""
    lines = ["[voice]", "language=eng", "baseVoice=0", "", "[speech]"]
    defaults = {
        "rate": 44, "volume": 100, "pitch": 64, "pitchScale": 50,
        "pitchFloor": 33, "pitchCeil": 64, "intonation": 34, "headSize": 46,
        "tilt": 0, "richness": 50, "breathiness": 0, "smoothness": 53,
        "frication": 8, "plosive": 20,
    }
    defaults.update(overrides)
    for k, v in defaults.items():
        lines.append("%s=%d" % (k, v))
    lines += ["", "[options]", "sampleRate=16000", "applyToAllVoices=0",
              "", "[logging]", "debug=0", ""]
    path = ini_path()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as fh:
        fh.write("\r\n".join(lines))


def speak(voice_index, out_path, text):
    p = subprocess.run([CLIENT, "speak", str(voice_index), out_path, text],
                       capture_output=True, text=True, timeout=120)
    return os.path.exists(out_path) and os.path.getsize(out_path) > 1000, p.stdout


def measure(path):
    try:
        with wave.open(path, "rb") as w:
            n, sr = w.getnframes(), w.getframerate()
            raw = w.readframes(n)
        s = struct.unpack("<%dh" % (len(raw) // 2), raw)
        peak = max(max(s), -min(s)) if s else 0
        return n / float(sr), peak
    except Exception:
        return 0.0, 0


def main():
    os.makedirs(OUT, exist_ok=True)
    notes = []
    failures = []

    # ---- 1. voices -------------------------------------------------------
    d = os.path.join(OUT, "01-voices")
    os.makedirs(d, exist_ok=True)
    write_settings({})
    notes.append("01-voices/ -- every registered FlexVoice voice, same sentence")
    for idx, name, origin in VOICES:
        fn = os.path.join(d, "%02d-%s.wav" % (idx, name.replace("FlexVoice ", "").replace(" ", "-")))
        ok, _ = speak(idx, fn, VOICE_TEXT)
        dur, peak = measure(fn)
        print("  %-28s %s  %.2fs peak %d" % (name, "ok" if ok else "FAILED", dur, peak),
              flush=True)
        if not ok:
            failures.append(name)
        notes.append("    %s -- %s" % (os.path.basename(fn), origin))

    # ---- 2. parameters ---------------------------------------------------
    d = os.path.join(OUT, "02-parameters")
    os.makedirs(d, exist_ok=True)
    notes.append("")
    notes.append("02-parameters/ -- the Custom Voice with one parameter moved;")
    notes.append("    0 is the minimum the engine supports and 100 the maximum")
    for key, label, what in PARAMS:
        for pct in (0, 25, 50, 75, 100):
            write_settings({key: pct})
            fn = os.path.join(d, "%s-%03d.wav" % (label, pct))
            ok, _ = speak(0, fn, PARAM_TEXT)
            if not ok and not (key == "volume" and pct == 0):
                failures.append("%s=%d" % (key, pct))
        dur, _ = measure(os.path.join(d, "%s-100.wav" % label))
        print("  %-14s swept 0..100  (%s)" % (label, what), flush=True)
        notes.append("    %s-000..100.wav -- %s" % (label, what))
    write_settings({})

    # ---- 3. SAPI rate / pitch / volume ------------------------------------
    d = os.path.join(OUT, "03-sapi")
    os.makedirs(d, exist_ok=True)
    subprocess.run([SAPI, "rate", "FlexVoice Julie", d],
                   capture_output=True, text=True, timeout=300)
    n = len([f for f in os.listdir(d) if f.endswith(".wav")])
    print("  SAPI rate/pitch/volume: %d files" % n, flush=True)
    notes.append("")
    notes.append("03-sapi/ -- driven through the real Windows SAPI5 stack:")
    notes.append("    rate_-10..+10, pitch_-10..+10 (the <pitch> XML tag), vol_025..100")

    # ---- 4. text that breaks the bare engine ------------------------------
    d = os.path.join(OUT, "04-text")
    os.makedirs(d, exist_ok=True)
    notes.append("")
    notes.append("04-text/ -- input that faults or wedges the bare FlexVoice engine,")
    notes.append("    spoken safely through the wrapper's normalizer")
    for name, text in TEXT_CASES:
        fn = os.path.join(d, "%s.wav" % name)
        ok, _ = speak(1, fn, text)
        print("  %-22s %s" % (name, "ok" if ok else "FAILED"), flush=True)
        if not ok:
            failures.append(name)
        notes.append("    %s.wav -- %s" % (name, text))

    # ---- 5. character navigation and spelling -----------------------------
    d = os.path.join(OUT, "05-characters")
    os.makedirs(d, exist_ok=True)
    notes.append("")
    notes.append("05-characters/ -- one character at a time (what arrowing sends)")
    notes.append("    and the spell-word command; both were silent before 1.0.2")
    for group, chars in CHAR_CASES:
        for ch in chars:
            safe = "".join(c if c.isalnum() else "x%02x" % ord(c) for c in ch)
            fn = os.path.join(d, "char-%s-%s.wav" % (group, safe))
            ok, _ = speak(1, fn, ch)
            if not ok:
                failures.append("char %r" % ch)
        print("  characters: %-12s %d file(s)" % (group, len(chars)), flush=True)
        notes.append("    char-%s-*.wav -- %s" % (group, " ".join(chars)))
    for w in SPELL_CASES:
        fn = os.path.join(d, "spell-%s.wav" % w)
        p = subprocess.run([CLIENT, "spell", "1", fn, w],
                           capture_output=True, text=True, timeout=120)
        ok = os.path.exists(fn) and os.path.getsize(fn) > 1000
        print("  spell %-12s %s" % (w, "ok" if ok else "FAILED"), flush=True)
        if not ok:
            failures.append("spell %s" % w)
        notes.append("    spell-%s.wav -- the word %s spelled out" % (w, w))

    with open(os.path.join(OUT, "README.txt"), "w") as fh:
        fh.write("FlexVoice SAPI5 -- sample renders\n")
        fh.write("=" * 34 + "\n\n")
        fh.write("All 16 kHz, 16-bit, mono. Every file was produced through the\n")
        fh.write("finished wrapper (pipe, engine host and text normalizer), not the\n")
        fh.write("bare engine.\n\n")
        fh.write("\n".join(notes))
        fh.write("\n")

    total = sum(len(files) for _, _, files in os.walk(OUT))
    print("\n%d files in %s" % (total, OUT))
    if failures:
        print("FAILURES: %s" % ", ".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
