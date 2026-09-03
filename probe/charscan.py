"""Every single character must speak, and spell-out must actually spell.

Two bugs this guards against, both reported from real NVDA use:

  * Arrowing character by character was silent on every punctuation mark. The
    prose rules quite correctly reduce a lone "," or "(" to a word boundary,
    which is silence -- so the normalizer now falls back to naming the
    character when an utterance would otherwise say nothing.
  * The spell-word command spelled nothing. The engine's own \\spell\\ block
    renders the same 0.66 s of near-silence for any input, so the wrapper
    spells the text itself.

The tell for the second one is that every word used to produce byte-identical
audio; this checks that different words produce different lengths.
"""

import os
import string
import subprocess
import sys

ROOT = r"C:\Users\joshk\OneDrive\dev\sapivoice"
BIN = os.path.join(ROOT, "build_x86", "bin", "Release")
CLIENT = os.path.join(BIN, "client_test.exe")
WAV = os.path.join(BIN, "_charscan.wav")

# Every printable ASCII character a user can arrow onto.
CHARS = list(string.ascii_lowercase + string.ascii_uppercase + string.digits) + \
        list("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ")

SPELL_WORDS = ["cat", "dog", "hello", "NVDA", "a", "A1", "a-b", "x", "1", ","]

# Prose must not turn into a punctuation recital.
PROSE = [
    ("Hello, world.", 1.0, 3.0),
    ("This is a normal sentence; nothing unusual about it.", 2.0, 6.0),
    ("Well, yes - that is right (mostly).", 1.5, 5.5),
]


def render(mode, text):
    p = subprocess.run([CLIENT, mode, "1", WAV, text],
                       capture_output=True, text=True, timeout=120)
    for line in (p.stdout or "").splitlines():
        if "bytes (" in line:
            try:
                n = int(line.split(":")[1].strip().split()[0])
            except (IndexError, ValueError):
                for tok in line.split():
                    if tok.isdigit():
                        n = int(tok)
                        break
                else:
                    n = 0
            return n
    return 0


def main():
    failures = []

    print("single characters (arrowing character by character)")
    silent = []
    for c in CHARS:
        n = render("speak", c)
        if n < 3000:
            silent.append(c)
    if silent:
        print("  SILENT: %s" % " ".join(repr(c) for c in silent))
        failures.append("%d character(s) silent" % len(silent))
    else:
        print("  all %d characters speak" % len(CHARS))

    print("spell-out")
    sizes = {}
    for w in SPELL_WORDS:
        sizes[w] = render("spell", w)
        print("  %-8s %7d bytes" % (repr(w), sizes[w]))
    if any(v < 3000 for v in sizes.values()):
        failures.append("spell-out produced silence")
    # The old bug's signature: identical output for every input.
    distinct = len(set(sizes[w] for w in ("cat", "dog", "hello", "NVDA")))
    if distinct < 3:
        failures.append("spell-out gives near-identical audio for different words "
                        "- it is not actually spelling")
    else:
        print("  different words give different audio, so it is spelling")

    print("prose is not turned into a punctuation recital")
    for text, lo, hi in PROSE:
        secs = render("speak", text) / 32000.0
        ok = lo <= secs <= hi
        print("  %-52s %.2f s%s" % (repr(text)[:52], secs, "" if ok else "   <-- OUT OF RANGE"))
        if not ok:
            failures.append("prose duration out of range: %r" % text)

    print()
    if failures:
        for f in failures:
            print("FAILED: %s" % f)
        return 1
    print("character navigation and spell-out both work")
    return 0


if __name__ == "__main__":
    sys.exit(main())
