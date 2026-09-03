"""Find which text makes the FlexVoice engine hang or fault.

Each case runs in its own fv_one.exe, because a hung engine cannot be torn
down and a faulted one takes the process with it. Slow, but it is the only way
to survey a range of inputs against this engine.
"""

import os
import subprocess
import sys

ROOT = r"C:\Users\joshk\OneDrive\dev\sapivoice"
EXE = os.path.join(ROOT, "build_probe", "fv_one.exe")
DATA = os.path.join(ROOT, "bin", "fv")
TAV = os.path.join(DATA, "English", "default.tav")

CASES = [
    # the trigger, and the pieces of it
    "rspd=100", " rspd=100 ", "rspd = 100", "rspd equals 100",
    "a=1", "a = 1", "ab=12", "abc=123", "x=y", "100=100",
    "=", "= ", " = ", "a=", "=1", "one=two", "a=b=c",
    # is it '=' at all, or the digits after it?
    "a=z", "word=word", "q=9", "9=q",
    # other punctuation, one at a time
    "a+b", "a-b", "a*b", "a/b", "a<b", "a>b", "a&b", "a|b", "a^b",
    "a~b", "a%b", "a#b", "a$b", "a@b", "a_b", "a;b", "a:b", "a,b",
    "a!b", "a?b", "a(b)", "a[b]", "a{b}", 'a"b', "a'b", "a\\b",
    # realistic screen-reader input
    "C:\\Users\\joshk\\Documents",
    "https://example.com/path?q=1",
    "joshknnd1982@gmail.com",
    "12:34 PM", "3.14159", "50%", "1/2", "$19.99",
    "int x = 0;", "if (a == b) return;", "SELECT * FROM t WHERE id=5",
    "-- a comment", "<html>", "</div>",
    # degenerate
    "", " ", "\t", ".", "...", "!!!", "?", "-", "a",
    "The quick brown fox jumps over the lazy dog.",
]


def run(text, timeout_ms=4000):
    try:
        p = subprocess.run([EXE, DATA, TAV, text, str(timeout_ms)],
                           capture_output=True, text=True,
                           timeout=(timeout_ms / 1000.0) + 25)
    except subprocess.TimeoutExpired:
        return "TIMEOUT", 0
    out = (p.stdout or "").strip()
    if p.returncode == 0 and out.startswith("OK"):
        return "ok", int(out.split()[1])
    if out.startswith("HANG"):
        return "HANG", int(out.split()[1])
    if out.startswith("THROW"):
        return "THROW", 0
    return "FAULT(0x%08x)" % (p.returncode & 0xFFFFFFFF), 0


def main():
    cases = sys.argv[1:] or CASES
    bad = []
    for text in cases:
        status, nbytes = run(text)
        shown = text.replace("\t", "\\t") or "(empty)"
        flag = "" if status == "ok" else "   <-- "
        print("  [%-30s] %-16s %7d bytes%s" % (shown[:30], status, nbytes, flag),
              flush=True)
        if status != "ok":
            bad.append((text, status))
    print()
    if bad:
        print("%d of %d inputs did not speak:" % (len(bad), len(cases)))
        for text, status in bad:
            print("   %-16s %r" % (status, text))
    else:
        print("all %d inputs spoke cleanly" % len(cases))


if __name__ == "__main__":
    main()
