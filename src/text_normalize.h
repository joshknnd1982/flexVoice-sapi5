// text_normalize.h -- make text safe for the FlexVoice engine.
//
// This is not cosmetic. Measured, one input per process because a bad one
// takes the process with it:
//
//   "3"                 access violation
//   "100"               access violation
//   "a 1 b"             access violation
//   "chapter 3"         hangs forever, no audio, no BM_TEXT_END
//   "3.14159"           hangs
//   "12:34 PM"          hangs
//   "$19.99"            hangs
//   "50%"               hangs
//   "joshknnd1982@..."  hangs
//   "one hundred"       fine
//
// A numeral reaching the engine either faults it or wedges it, and a wedged
// engine never recovers -- it cannot even be destroyed safely. A screen reader
// speaks numbers constantly, so every digit run has to become English words
// before the engine sees it. The same is true, less violently, of acronyms,
// vowel-less tokens and single consonant letters, which the engine silently
// drops.
//
// Because the text is rewritten, word-boundary events would land on the wrong
// characters, so normalization also emits a map from each output byte back to
// the input character it came from.

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace FlexVoice {
namespace text {

struct Normalized {
    std::string           text;    // engine-safe single-byte text
    std::vector<uint32_t> srcMap;  // srcMap[i] is the source index of text[i]
};

// `src` is UTF-16; `codepage` is the engine's single-byte codepage for the
// language in use. Offsets in srcMap are indices into `src`.
Normalized normalize(const wchar_t* src, size_t len, unsigned codepage);

// Same rules over bytes already in the engine's codepage, for callers that do
// not need an offset map (the host's safety net, the preview box).
std::string normalize_bytes(const std::string& in);

// Digit run -> English words. Exposed because it is the load-bearing rule and
// deserves its own tests.
std::string number_to_words(const std::string& digits);

}  // namespace text
}  // namespace FlexVoice
