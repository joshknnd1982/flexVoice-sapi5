# FlexVoice SAPI 5

Mindmaker's **FlexVoice** speech synthesiser, from 2002, as ordinary Windows
SAPI 5 voices — usable from NVDA, JAWS, Narrator, Balabolka or anything else
that speaks SAPI 5. Both 32-bit and 64-bit. No SAPI 4 runtime, no ActiveX, no
browser plugin, and nothing in the registry that the engine itself has to read.

Nine voices, a configuration utility that exposes every speech parameter the
engine actually responds to, and a text normalizer that stops the engine
crashing on things a screen reader says every minute of the day.

**[Download the installer from the Releases page](https://github.com/joshknnd1982/flexVoice-sapi5/releases).**
The engine and its voice data are not in this repository; the installer carries
them.

---

## What FlexVoice is

FlexVoice is a *hybrid* synthesiser: it concatenates recorded diphones, but it
does the concatenation in LPC parameter space rather than in the waveform. Its
authors' own description, from the paper they published about it:

> "FlexVoice is a concatenative, diphone-based LPC synthesis system. It is a
> hybrid system in the sense that it concatenates fundamental units but segment
> concatenation takes place in a parametric space instead of the conventional
> wave space."

That design is why the engine is tiny and startlingly fast — it renders about
**seventy times faster than real time** on a modern machine — and why it has so
many knobs. Because every segment is a parameter set rather than a fixed piece
of audio, head size, richness, breathiness and the rest can be moved after the
fact without the artefacts a waveform-domain synthesiser would produce.

Mindmaker Ltd. of Budapest closed long ago and FlexVoice has not been sold for
over twenty years. See [CREDITS.md](CREDITS.md).

---

## Voices

| Voice | Base | Gender | Age | Where it comes from |
|---|---|---|---|---|
| FlexVoice Custom Voice | either | — | — | configurable; every parameter comes from the utility |
| FlexVoice Julie | Julie | Female | Adult | FlexVoice 3.01 `default.tav` |
| FlexVoice Kim | Julie | Female | Adult | `Voices\Kim.tav` — byte-identical to `default.tav` |
| FlexVoice Tim | Tom | Male | Adult | `Voices\Tim.tav` |
| FlexVoice Bill | Julie | Male | Adult | FlexVoice **2.0** `Bill.tav`, ported |
| FlexVoice Julius | Julie | Male | Adult | FlexVoice **2.0** `Julius.tav`, ported |
| FlexVoice Julia | Julie | Female | Adult | FlexVoice **2.0** `Julie.tav`, ported |
| FlexVoice Jill | Julie | Female | Child | FlexVoice **2.0** `Jill.tav`, ported |
| FlexVoice Kit | Julie | Male | Child | FlexVoice **2.0** `Kit.tav`, ported |

Being honest about what is what:

* FlexVoice 3.01's English runtime contains **two** diphone databases —
  `Julie.bin` (female) and `Tom.bin` (male) — and three `.tav` speaker files,
  two of which are the same file under two names. So the engine as shipped has
  two genuinely distinct voices, not nine.
* The other five are **Mindmaker's own FlexVoice 2.0 speaker presets**, with
  their parameters re-expressed in the 3.01 schema. The 2.0 files themselves
  cannot be loaded: they carry obsolete `durationDescr` / `pitchDescr` /
  `volumeDescr` selectors and none of the model filenames 3.01 requires, and
  handing one to the 3.01 engine makes it print *"Undefined of improper
  featurevalue type in featValVal"* and call `abort()`. Ported across are
  gender, age and every prosody and voice-source parameter 3.01 still
  understands. Not ported are the 2.0 volume scale (different units) and its
  24-band equaliser (the 3.01 base is tuned for 12).
* Every voice carries a **measured level trim** so the roster is
  loudness-matched. Without it the ported presets run about 8 dB hot and clip.

Sample renders of all nine, of every parameter across its range, and of the
inputs that break the bare engine, are in [`samples/`](samples/).

---

## Languages

The header declares two languages. The engine binary knows four — asking it
`getLangName` across the LCID range returns:

| LCID | Name the engine reports | Data shipped? |
|---|---|---|
| 0x0409 | `English:US` | **yes** |
| 0x040e | `Hungarian` | no |
| 0x0405 | `Czech` | no |
| 0x043e | `Malay` | no |

Only English data survives. `loadLanguage` on any of the others throws
*"Cannot load language!"* — the code paths are in the DLL, the diphone
databases and letter-to-sound tables are not. The wrapper enumerates whichever
language directories are actually present under `engine\`, so if a Hungarian,
Czech or Malay data set ever turns up, dropping it in is all that is needed.

---

## Speech parameters

Fourteen parameters, each exposed in the configuration utility as a whole
percentage where **0 is the minimum the engine supports and 100 the maximum**.

The SDK documents 33 speaker attributes and gives a range for none of them, and
`Speaker::set()` accepts every value you hand it — including values that abort
the process. So every range below was found by sweeping the parameter and
measuring the rendered audio (duration, median F0, F0 spread, RMS, peak,
clipping). See `probe/sweep.py`.

| Parameter | Engine name | Range | Scale | What moves |
|---|---|---|---|---|
| Rate | `speechRate` (engine) | 0.25 – 6.0× | geometric | duration only; **no pitch change** |
| Volume | `volume` (engine) | 0.0 – 1.0 | linear | amplitude; 0 is true silence |
| Pitch | `defaultPitch` | 50 – 450 Hz | geometric | base F0, in hertz |
| Pitch scale | `pitchRate` (engine) | 0.5 – 2.0× | geometric | the whole F0 contour; duration unchanged |
| Pitch floor | `pitchMin` | 20 – 300 Hz | geometric | hard floor on the contour |
| Pitch ceiling | `pitchMax` | 150 – 1000 Hz | geometric | hard ceiling; a low one flattens the voice |
| Intonation | `intonationLevel` | 0.0 – 5.0 | linear | F0 variance: 0 is monotone |
| Head size | `headsize` | 0.5 – 2.0 | geometric | vocal tract scale |
| Tilt | `tilt` | 0.0 – 4.0 | linear | spectral tilt |
| Richness | `richness` | 0.0 – 2.0 | linear | voice source richness |
| Breathiness | `breathiness` | 0.0 – 1.5 | linear | added breath noise |
| Smoothness | `smoothness` | 0.3 – 1.2 | linear | voice source smoothing |
| Frication | `fricationRate` | 0.0 – 1.5 | linear | strength of s, f, sh |
| Plosives | `plosiveRate` | 0.0 – 2.0 | linear | strength of p, t, k bursts |

Ratio-like parameters use a geometric mapping so a given number of percent
means a given *proportional* change. A linear rate control spends most of its
travel in the fast half and is unusable at the slow end.

### Parameters that are documented but do nothing

Four of the SDK's documented attributes are **inert** in this build. Each was
swept across its full range and produced bit-identical audio every time, so
they are deliberately not exposed:

`speedWPM`, `creakiness`, `singingPitchRate`, `volumeSmoothWindow`.

`speedWPM` is the surprising one — it reads like the rate control and is not.
The real rate control is `speechRate`.

### Where each parameter is set

FlexVoice has two levels and they are not interchangeable. The `Speaker`
carries the voice's identity in absolute units — `volume` on a roughly 0–45
amplitude scale, `defaultPitch` in hertz. The `Engine`'s own `attribute()`
exposes exactly three multipliers over that — `speechRate`, `volume` and
`pitchRate`, all 1.0 by default — and they can be changed mid-utterance in
under a millisecond. `pitchRate` is undocumented at the engine level; it is
there, and it works.

So the client's rate/pitch/volume go on the Engine and the voice's identity
goes on the Speaker. Getting this backwards is silent rather than loud:
setting a 1.0 "unity gain" on the *Speaker* is near silence.

---

## How it is put together

```
   NVDA / JAWS / Narrator / any SAPI 5 client
        |                        |
   FlexVoiceSAPI.dll        x64\FlexVoiceSAPI.dll
   (32-bit COM)             (64-bit COM)
        |                        |
        +-----------+------------+
                    |  named pipe \\.\pipe\FlexVoiceTTS
            flexvoice_host.exe        (32-bit)
                    |
          FlexVoice_3_01_001.dll  +  engine\English\
```

**Both** bitnesses talk to the host over a pipe; the 32-bit DLL does not load
the engine in-process even though it could. The engine is x86-only, which
forces a host for 64-bit clients — but the reason it is used for 32-bit clients
too is that **the engine calls `abort()` on some malformed input**, and an
`abort()` inside NVDA's process would take the screen reader down with it. Here
it costs one host restart, which takes about 30 ms.

The host also runs a watchdog. Hostile input can wedge the engine permanently —
no audio, no completion, and a worker thread still inside the synthesiser so
the engine cannot even be destroyed safely. When that happens the host reports
the error and terminates itself; the next request gets a clean one.

### Responsiveness

Measured on this machine with `QueryPerformanceCounter`, through the whole
stack including the pipe. The pattern that matters is *arrowing* — speak,
cancel almost immediately, speak again — not one utterance after another to
completion, because a screen reader user interrupts constantly:

| | 32-bit client | 64-bit client |
|---|---|---|
| First audio per keystroke, warm | 6.1 ms mean, 7.3 ms worst | 7.0 ms mean |
| Cancel to silence | 0 ms | 0 ms |
| Render speed | ~70× real time | ~70× real time |

Cancel is genuinely instantaneous: `Engine::stop()` returns in 0 ms and not one
further audio buffer arrives afterwards, measured across repeated trials.

Getting there took three fixes, and the interesting part is that none of them
was the synthesiser:

* **The pipe instance was destroyed and recreated per client.** Since
  cancelling drops the connection, that happened on every keystroke, and it
  left a window in which the pipe name did not exist at all — so the client got
  `ERROR_FILE_NOT_FOUND` and backed off 100 ms. One instance, connected and
  disconnected in a loop, removes it entirely. This was the bulk of a
  user-visible ~250 ms lag while arrowing.
* **The `.tav` was reloaded and the speaker re-registered every utterance,**
  even when nothing but the rate had changed. Now the speaker is rebuilt only
  when something it actually depends on moves.
* **The host polled for engine output with `Sleep(1)`,** which on Windows'
  15.6 ms timer costs a full tick. The output site signals an event instead.

A fourth thing that looked obvious and was not: raising the system timer
resolution with `timeBeginPeriod(1)`. Measured, it changes nothing — 6.1 ms
either way — because nothing on this path sleeps any more. It is deliberately
not in the code.

One measurement warning: `GetTickCount` has a 15.6 ms granularity, the same
order as these numbers. It reported a flat "15 ms" for what is really 6 ms,
which is enough to send you hunting a delay that is not there. Use
`QueryPerformanceCounter`.

---

## Text handling

The engine's text front end is fragile in ways that matter enormously for a
screen reader. Measured, one input per process because a bad one takes the
process with it (`probe/textscan.py`):

| Input | Bare engine |
|---|---|
| `3`, `100`, `1000`, `x 1`, `a 1 b` | access violation |
| `chapter 3`, `the year 1999`, `9`, `10`, `12` | hangs forever |
| `3.14159`, `12:34 PM`, `50%`, `$19.99`, `1/2` | hangs or faults |
| `joshknnd1982@gmail.com`, `https://example.com/path?q=1` | hangs |
| `one hundred`, `zero` | fine |

**A numeral reaching this engine either faults it or wedges it.** So every
digit run is converted to English words before the engine sees it. The same is
true, less violently, of acronyms, vowel-less tokens and single consonant
letters, which the engine silently drops.

Because the text is rewritten, the normalizer also emits a map from each output
byte back to the input character, so `SPEI_WORD_BOUNDARY` events still land on
the right word in the caller's original string.

The rules, all of them applied in `src/text_normalize.cpp`:

* digit runs → English words (leading zeros and very long runs spoken digit by
  digit)
* backslash → the word "backslash" — it is the engine's embedded-command escape
  and there is no way to quote it
* `ALLCAPS` of 2–6 letters and vowel-less tokens → spelled as letter names
* single consonants → letter names (`s`/`r`/`l` as *ess*, *arr*, *ell*, which
  survive where shorter forms do not)
* contractions expanded before the single-letter rule can see their tails
* `scheme://` broken up, `.` `/` `@` `#` between alphanumerics said as words
* runs of whitespace collapsed — the engine turns a hard line break into a long
  pause, which makes say-all lurch

Two rules exist because of how a screen reader is actually used:

* **A lone character gets named.** Arrowing through a document one character at
  a time sends a one-character utterance, and the prose rules quite correctly
  reduce a bare `,` or `(` to a word boundary — which is silence. So when an
  utterance would say nothing at all, the characters are named instead
  ("comma", "left paren", "dash"). Prose is untouched: it always contains a
  letter, so the fallback never fires and `Hello, world.` is not read as
  "Hello comma world period".
* **Spelling is done here, not by the engine.** `\spell\ … \endspell\` is in
  the engine's keyword table and does not work: measured, every input produces
  the same 0.66 s of near-silence regardless of the word. So `<spell>` — which
  is how NVDA's spell-word command reaches an engine — is expanded by the
  wrapper into letter names, digit words and symbol names.

`probe/wrapscan.py` runs every one of those hostile inputs through the finished
wrapper and checks it speaks.

### SAPI features supported

`SPEI_WORD_BOUNDARY` and `SPEI_SENTENCE_BOUNDARY` (from the engine's own
bookmarks, so they are sample-accurate rather than estimated),
`SPEI_TTS_BOOKMARK`, `<silence>`, `<spell>` (expanded by the wrapper, since the
engine's own spell-out does nothing), and per-fragment `<rate>`, `<pitch>` and
`<volume>`.

---

## The configuration utility

`FlexVoiceConfig.exe`, on the Start menu and optionally on the desktop.

It configures the **FlexVoice Custom Voice**. The named presets keep their own
character — if the utility's absolute values were applied to all of them, every
voice in the list would sound alike.

Accessibility is the point of this program, so:

* Every control is created in explicit tab order, and every one carries a
  pinned MSAA name that spells its range out in words — *"Pitch, 0 is 50 hertz
  to 100 is 450 hertz"*.
* Numeric values are an **edit box with a spin buddy, never a slider**. MSAA
  reports a trackbar's position as a percentage of its range, so a 0–9 slider
  announces 5 as "55". A spin buddy announces the literal number.
* Every label has a unique access key.
* Changes save the instant they are made — there is no OK/Cancel model to
  explain and closing the window can never lose anything.
* The status line says what a percentage actually means in engine terms, so
  "Pitch: 64%" is followed by "(203 Hz)".

Settings live in `%APPDATA%\FlexVoiceSAPI\settings.ini`. The SAPI DLLs notice
the file's timestamp changing and reload, so a change takes effect on the very
next utterance without restarting the screen reader.

---

## Registration, and getting back out again

SAPI 5's voice list is one shared registry key. Every voice on the machine,
from every vendor, is a subkey of
`HKLM\SOFTWARE\Microsoft\Speech\Voices\Tokens` — twice over on 64-bit Windows,
once in the native view for 64-bit clients and once under `WOW6432Node` for
32-bit ones. So an installer that leaves a broken entry there is not leaving a
mess of its own. It is handing every other speech engine on the machine a voice
that cannot be created.

**1.0.2 did exactly that**, and this is worth writing down because the mistake
is easy to make and invisible until somebody uninstalls.

`DllUnregisterServer` removed each voice token with `RegDeleteKeyW`. That
function will not delete a key that has subkeys — it returns
`ERROR_ACCESS_DENIED` — and every voice token has an `Attributes` subkey,
because SAPI requires one. So **not one token was ever removed**, by any
version, and the failure was swallowed by a `catch (...)`. Uninstalling
FlexVoice deleted the DLL and left nine voices behind naming a CLSID that no
longer resolved.

What that does to a machine depends on the client. NVDA remembers its SAPI 5
voice by token path; if the remembered voice is one of the nine, the token is
still there to be found, creating it fails, and the SAPI 5 synthesiser will not
start at all. From the user's chair the entire SAPI 5 stack is broken, and
*reinstalling FlexVoice fixes it* — which is a confusing enough symptom that it
is worth naming.

The reason no test caught it: `sapi_probe` registers and unregisters with its
own helper, which used `RegDeleteTreeW`. The test cleaned up correctly while
the product did not. `test/registry_test.cpp` now drives
`write_voice_tokens` and `remove_voice_tokens` themselves.

Three things changed.

* **Deleting a registry key here always means deleting the subtree.**
  `registry::key::delete_subtree` refuses an empty name, because
  `RegDeleteTreeW` with an empty subkey empties the key the handle names —
  which for these callers is the machine's entire voice list.
* **The uninstall no longer depends on `regsvr32` running.** The installer
  carries `[Registry]` entries with `uninsdeletekey dontcreatekey`, which
  create nothing but record the deletion in `unins000.dat`, and a
  `CurUninstallStepChanged` sweep that matches tokens by their `FlexVoice_`
  prefix in both registry views — so a voice renamed in some earlier release
  is caught too. Three independent mechanisms, none of which can take the
  others down with it. No parent key is ever touched.
* **The dynamic token enumerator is gone.** Registering under
  `Voices\TokenEnums` as well as writing static tokens made SAPI list every
  FlexVoice voice *twice* — the enumerator gives its tokens ids under
  `TokenEnums\FlexVoice` rather than under `Tokens`, so SAPI cannot tell they
  are the same nine voices. It was never load-bearing; static tokens are what
  every SAPI 5 client reads. Installing 1.0.3 removes it.

If the machine's default voice points at a FlexVoice token, the uninstaller
clears that value rather than leaving it aimed at a voice that has stopped
existing. SAPI picks a voice on its own when the value is absent.

---

## Logging

Tick **Write a diagnostic log** in the utility, or set `debug=1` under
`[logging]` in `settings.ini`. Logs are written to
`%APPDATA%\FlexVoiceSAPI\logs\`, one file per component — `sapi32.log`,
`sapi64.log`, `host.log`, `config.log` — capped at 4 MB with one previous copy.

The installer keeps its own log at `<install dir>\logs\install.log`.

If speech stops working, `flexvoice_diag32.exe` and `flexvoice_diag64.exe` in
the install directory talk to the host directly:

```bat
flexvoice_diag64.exe ping
flexvoice_diag64.exe voices
flexvoice_diag64.exe speak 1 test.wav "Hello there."
flexvoice_diag64.exe bench 1
```

---

## Building

Needs Visual Studio 2022 Build Tools, CMake, and Inno Setup 6 for the
installer. The MindMaker SDK (headers, import library, engine DLL) and the
English voice data go under `bin\` and are not in the repository.

```bat
build_all.bat
```

That configures and builds both architectures, runs the host's self-test across
every voice, stages `output\`, and produces `output\FlexVoiceSAPI_Setup.exe`.

The engine host is the only thing that links the SDK. It builds as C++17 with
`_HAS_AUTO_PTR_ETC=1`, because `EngineFactory::createEngine` returns a
`std::auto_ptr` that C++17 removed — the mangled name is unchanged either way,
so the 2003 import library still resolves. RTTI is mandatory: `Engine.h` has an
`#error` without it.

### Verification programs

| | |
|---|---|
| `flexvoice_host.exe --selftest <dir>` | render one utterance per voice, no pipe |
| `probe/sweep.py` | sweep every parameter and measure the audio |
| `probe/textscan.py` | find what wedges or faults the bare engine |
| `probe/wrapscan.py` | prove the wrapper survives all of it |
| `probe/calibrate.py` | measure the per-voice level trims |
| `probe/make_samples.py` | regenerate `samples/` |
| `probe/charscan.py` | every printable character speaks, and spell-out really spells |
| `test/sapi_probe.exe` | drive the real SAPI stack, registering under HKCU so it needs no elevation |

`sapi_probe` is worth explaining: `DllRegisterServer` writes to HKLM, so
nothing would exercise the path SAPI actually takes until install time, and a
bug that only appears once SAPI reads a token back out of the registry would
pass every other test.

---

## Licence

BSD 3-Clause — see [LICENSE](LICENSE). That covers the wrapper. The FlexVoice
engine and its voice data are Mindmaker Ltd.'s work and are not licensed by
this project; they are distributed in the installer as abandonware, on the
basis that the company has been gone and the product unsold for over twenty
years.
