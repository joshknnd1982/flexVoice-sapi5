"""Run the inputs that break the raw engine through the whole wrapper.

textscan.py proves what wedges or faults the bare engine. This proves the
normalizer, the host watchdog and the restart path turn every one of them into
speech. A regression here is a screen reader going silent, so it is worth
running before every release.
"""

import os
import subprocess
import sys

ROOT = r"C:\Users\joshk\OneDrive\dev\sapivoice"
BIN = os.path.join(ROOT, "build_x86", "bin", "Release")
CLIENT = os.path.join(BIN, "client_test.exe")
WAV = os.path.join(BIN, "_wrapscan.wav")

# Every string here either faulted or hung the bare engine, plus a control set.
CASES = [
    "3", "1", "100", "1000", "a 1 b", "x 1", "chapter 3", "the year 1999",
    "3.14159", "12:34 PM", "50%", "1/2", "$19.99", "9", "10", "12",
    "rspd=100", "a=1", "q=9", "9=q", "=1", "abc=123", "100=100",
    "SELECT * FROM t WHERE id=5", "https://example.com/path?q=1",
    "joshknnd1982@gmail.com", "C:\\Users\\joshk\\Documents",
    "int x = 0;", "NVDA", "msgs", "http", "it's", "can't", "IBM-TTS", "C++",
    "U.S.A.", "etc", "Version 1.0.0 build 4567",
    "Cell A1 contains 42 and B2 contains 3.5",
    "Press F1 for help, or Alt+F4 to quit.",
    "\u00e9l\u00e8ve na\u00efve caf\u00e9",
    "", " ", ".", "The quick brown fox jumps over the lazy dog.",
]


def run(text):
    try:
        p = subprocess.run([CLIENT, "speak", "1", WAV, text],
                           capture_output=True, text=True, timeout=90)
    except subprocess.TimeoutExpired:
        return "TIMEOUT", 0
    for line in (p.stdout or "").splitlines():
        if "bytes (" in line:
            try:
                n = int(line.split(":")[1].strip().split()[0])
            except (IndexError, ValueError):
                n = 0
            return ("ok" if n > 0 else "SILENT"), n
    if "FAILED" in (p.stdout or ""):
        return "FAILED", 0
    return "NO OUTPUT", 0


def main():
    cases = sys.argv[1:] or CASES
    bad = []
    for text in cases:
        status, nbytes = run(text)
        shown = repr(text)[1:-1] or "(empty)"
        print("  [%-34s] %-10s %7d bytes%s" %
              (shown[:34], status, nbytes, "" if status == "ok" else "   <--"),
              flush=True)
        # An empty or whitespace-only string legitimately produces near-silence.
        if status != "ok" and text.strip(" .!?,;:"):
            bad.append((text, status))
    print()
    if bad:
        print("%d input(s) still fail through the wrapper:" % len(bad))
        for text, status in bad:
            print("   %-10s %r" % (status, text))
        return 1
    print("every input spoke through the wrapper")
    return 0


if __name__ == "__main__":
    sys.exit(main())
