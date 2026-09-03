#pragma once

#include <windows.h>
#include <sapi.h>
#include <string>

#include "registry.hpp"
#include "voice_attributes.hpp"

namespace FlexVoice {
namespace sapi {

// Every voice is written as a static registry token as well as being offered
// by the dynamic enumerator. Static tokens are what every SAPI5 client reads,
// Windows Narrator included, so they are the form with the widest reach; the
// enumerator exists so a future runtime-discovered voice list stays in sync
// without a re-registration.
inline constexpr const wchar_t* voices_path =
    L"Software\\Microsoft\\Speech\\Voices\\Tokens";

inline void write_voice_tokens(HKEY root, const std::wstring& clsid_str)
{
    using namespace FlexVoice::registry;

    key tokens(root, voices_path, KEY_CREATE_SUB_KEY | KEY_SET_VALUE, true);

    for (int i = 0; i < voices::kVoiceCount; ++i) {
        const voice_attributes v(i);
        const std::wstring name = v.get_name();

        key token(tokens, v.token_id(), KEY_CREATE_SUB_KEY | KEY_SET_VALUE, true);
        token.set(name);
        token.set(L"CLSID", clsid_str);
        // SAPI looks a display name up under a value named for the LCID it is
        // asking about, falling back to the key's default value set just above.
        token.set(v.get_language(), name);

        key attrs(token, L"Attributes", KEY_SET_VALUE, true);
        attrs.set(L"Name", name);
        attrs.set(L"Gender", v.get_gender());
        attrs.set(L"Age", v.get_age());
        attrs.set(L"Language", v.get_language());
        attrs.set(L"Vendor", L"MindMaker");
        // Read back by SetObjectToken, so the voice is recovered exactly rather
        // than by parsing a display name apart.
        wchar_t index_str[16];
        swprintf_s(index_str, L"%d", i);
        attrs.set(L"FvIndex", index_str);
    }
}

// Sweeps the full set regardless of what is currently registered, so a rename
// or a removed voice cannot leave an orphaned token pointing at the engine.
inline void remove_voice_tokens(HKEY root) noexcept
{
    using namespace FlexVoice::registry;
    try {
        key tokens(root, voices_path, KEY_ALL_ACCESS);
        for (int i = 0; i < voices::kVoiceCount; ++i) {
            try {
                tokens.delete_subkey(voice_attributes(i).token_id());
            }
            catch (...) {
            }
        }
    }
    catch (...) {
    }
}

}  // namespace sapi
}  // namespace FlexVoice
