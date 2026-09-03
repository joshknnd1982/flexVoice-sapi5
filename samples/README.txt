FlexVoice SAPI5 -- sample renders
==================================

All 16 kHz, 16-bit, mono. Every file was produced through the
finished wrapper (pipe, engine host and text normalizer), not the
bare engine.

01-voices/ -- every registered FlexVoice voice, same sentence
    00-Custom-Voice.wav -- configurable; shown at its factory defaults
    01-Julie.wav -- FlexVoice 3.01 default.tav, the Julie diphone database
    02-Kim.wav -- Voices/Kim.tav, byte-identical to default.tav
    03-Tim.wav -- Voices/Tim.tav, the Tom diphone database
    04-Bill.wav -- FlexVoice 2.0 Bill preset, ported to the 3.01 schema
    05-Julius.wav -- FlexVoice 2.0 Julius preset, ported
    06-Julia.wav -- FlexVoice 2.0 Julie preset, ported
    07-Jill.wav -- FlexVoice 2.0 Jill preset, ported (child, female)
    08-Kit.wav -- FlexVoice 2.0 Kit preset, ported (child, male)

02-parameters/ -- the Custom Voice with one parameter moved;
    0 is the minimum the engine supports and 100 the maximum
    Rate-000..100.wav -- 0 is slowest, 100 is fastest
    Volume-000..100.wav -- 0 is silence, 100 is full level
    Pitch-000..100.wav -- 0 is 50 Hz, 100 is 450 Hz
    PitchScale-000..100.wav -- scales the whole pitch contour
    PitchFloor-000..100.wav -- hard floor on the pitch contour
    PitchCeiling-000..100.wav -- hard ceiling; a low one flattens the voice
    Intonation-000..100.wav -- 0 is monotone, 100 is highly expressive
    HeadSize-000..100.wav -- vocal tract scale
    Tilt-000..100.wav -- spectral tilt
    Richness-000..100.wav -- voice source richness
    Breathiness-000..100.wav -- added breath noise
    Smoothness-000..100.wav -- voice source smoothing
    Frication-000..100.wav -- strength of s, f, sh sounds
    Plosives-000..100.wav -- strength of p, t, k bursts

03-sapi/ -- driven through the real Windows SAPI5 stack:
    rate_-10..+10, pitch_-10..+10 (the <pitch> XML tag), vol_025..100

04-text/ -- input that faults or wedges the bare FlexVoice engine,
    spoken safely through the wrapper's normalizer
    numbers.wav -- Chapter 3 has 1999 items, 12 of them at 12:34 PM.
    decimals-and-money.wav -- Pi is 3.14159, the price is $19.99, that is 50% off.
    paths-and-urls.wav -- Open C:\Users\joshk\Documents or https://example.com/path
    email.wav -- Write to joshknnd1982@gmail.com about it.
    acronyms.wav -- NVDA and JAWS both read HTML and use the SAPI API.
    code.wav -- int x = 0; if (a == b) return; SELECT * FROM t WHERE id=5
    contractions.wav -- It's fine, you can't tell, we'll see, I'd rather not.
    accents.wav -- The eleve was naive at the cafe in Zurich.
