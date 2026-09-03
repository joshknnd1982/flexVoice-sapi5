#include <new>
#include <sapi.h>

#include "com.hpp"
#include "registry.hpp"
#include "engine_client.h"
#include "flexvoice_log.h"
#include "ISpTTSEngineImpl.hpp"
#include "IEnumSpObjectTokensImpl.hpp"
#include "voice_registry.hpp"

namespace {

HINSTANCE g_dll_handle = nullptr;
FlexVoice::com::class_object_factory g_cls_obj_factory;

const std::wstring token_enums_path = L"Software\\Microsoft\\Speech\\Voices\\TokenEnums";

[[nodiscard]] std::wstring clsid_to_string(const GUID& clsid)
{
    wchar_t buf[64];
    StringFromGUID2(clsid, buf, 64);
    return std::wstring(buf);
}

void register_token_enumerator()
{
    using namespace FlexVoice::registry;

    const std::wstring clsid_str =
        clsid_to_string(__uuidof(FlexVoice::sapi::IEnumSpObjectTokensImpl));

    // TokenEnums is read from HKLM only -- an HKCU enumerator registers
    // cleanly and is then ignored -- so registration needs elevation and the
    // installer asks for it.
    key enums_key(HKEY_LOCAL_MACHINE, token_enums_path,
                  KEY_CREATE_SUB_KEY | KEY_SET_VALUE, true);
    key enum_key(enums_key, L"FlexVoice", KEY_SET_VALUE, true);

    enum_key.set(L"FlexVoice Voices");
    enum_key.set(L"CLSID", clsid_str);
}

void unregister_token_enumerator() noexcept
{
    using namespace FlexVoice::registry;
    try {
        key enums_key(HKEY_LOCAL_MACHINE, token_enums_path, KEY_ALL_ACCESS);
        enums_key.delete_subkey(L"FlexVoice");
    }
    catch (...) {
    }
}

}  // namespace

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_dll_handle = hInstance;
        DisableThreadLibraryCalls(hInstance);

#ifdef _WIN64
        FlexVoice::log::component() = L"sapi64";
#else
        FlexVoice::log::component() = L"sapi32";
#endif

        try {
            g_cls_obj_factory.register_class<FlexVoice::sapi::IEnumSpObjectTokensImpl>();
            g_cls_obj_factory.register_class<FlexVoice::sapi::ISpTTSEngineImpl>();
        }
        catch (...) {
            return FALSE;
        }
        // Nothing else here on purpose: the pipe client is created lazily on
        // the first utterance, so loading the DLL never starts a process.
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    return g_cls_obj_factory.create(rclsid, riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return FlexVoice::com::object_counter::is_zero() ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    try {
        FlexVoice::com::class_registrar r(g_dll_handle);
        r.register_class<FlexVoice::sapi::IEnumSpObjectTokensImpl>();
        r.register_class<FlexVoice::sapi::ISpTTSEngineImpl>();

        const std::wstring engine_clsid =
            clsid_to_string(__uuidof(FlexVoice::sapi::ISpTTSEngineImpl));

        // Unregister first, so a renamed or removed voice cannot leave an
        // orphaned token behind pointing at this CLSID.
        FlexVoice::sapi::remove_voice_tokens(HKEY_LOCAL_MACHINE);
        FlexVoice::sapi::write_voice_tokens(HKEY_LOCAL_MACHINE, engine_clsid);
        register_token_enumerator();
        return S_OK;
    }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_UNEXPECTED; }
}

STDAPI DllUnregisterServer()
{
    try {
        // Stop the host first so its files can be replaced by an upgrade.
        FlexVoice::EngineClient::shutdownServer();

        FlexVoice::sapi::remove_voice_tokens(HKEY_LOCAL_MACHINE);
        unregister_token_enumerator();

        FlexVoice::com::class_registrar r(g_dll_handle);
        r.unregister_class<FlexVoice::sapi::IEnumSpObjectTokensImpl>();
        r.unregister_class<FlexVoice::sapi::ISpTTSEngineImpl>();
        return S_OK;
    }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_UNEXPECTED; }
}
