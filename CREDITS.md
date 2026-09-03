# Credits

## The engine

**FlexVoice** was created by **Mindmaker Ltd.** of Budapest, Hungary, and sold
under the `flexvoice.com` name. The synthesiser and its voice data are their
work; this project only wraps them.

The engine is described in a peer-reviewed paper by its authors:

> György Balogh, Ervin Dobler, Tamás Grobler, Béla Smodics and Csaba
> Szepesvári. *FlexVoice: A Parametric Approach to High-Quality Speech
> Synthesis.* In P. Sojka, I. Kopeček and K. Pala (eds.), **Text, Speech and
> Dialogue (TSD 2000)**, LNAI 1902, pp. 189–194. Springer-Verlag, 2000.

Mindmaker Ltd. is long gone and FlexVoice has not been sold for over twenty
years. No engine binary or voice data is included in this repository; the
installer on the Releases page carries them so the abandoned engine stays
usable on a current version of Windows.

## The SAPI 5 plumbing

The COM server skeleton — the class factory, the hand-rolled `ISpDataKey` and
`IEnumSpObjectTokens` implementations, and the registration code — descends
from **Gozaltech's BestSpeech SAPI 5 wrapper**, by way of the Outloud, Lucent
and CyberTalk wrappers in this same family. Those files are BSD-licensed and
are reused here with the namespace renamed and the voice tables replaced.

## The prior FlexVoice work

An earlier NVDA-oriented wrapper around this same engine
(`Flexvoice-wrapper-main`, `fvwrap.dll`) worked out, over six revisions, which
inputs this engine cannot survive. Its normalizer's build tag reads
`FVWRAP-NORM-V6-NONATO`, and its source comments are blunt about what each rule
cost to learn — *"or digits will slip through and crash FlexVoice again"*.

This project's `src/text_normalize.cpp` is a fresh implementation, but the rule
*set* — digit runs to words, acronyms and vowel-less tokens spelled out, lone
consonants named, contractions expanded before the single-letter rule can see
their tails, `://` broken up — is that wrapper's hard-won knowledge, re-derived
and re-confirmed here by measurement (see `probe/textscan.py`).

## This wrapper

Josh Kennedy (joshknnd1982), 2026. Written with Claude (Anthropic).

Every parameter range, crash trigger, latency figure and voice level in this
project was measured against the engine rather than taken from documentation;
the programs that did the measuring are in `probe/` and the numbers they
produced are quoted in the source comments where they are used.
