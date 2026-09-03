// fv_render.cpp -- general-purpose FlexVoice renderer used for voice research
// and for producing the sample WAVs.
//
//   fv_render <dataPath> <tavFile> <outWav> <text> [name=value ...]
//
// Overrides are applied to the loaded Speaker before synthesis. A value that
// parses as an integer and whose name is a known int parameter is set as int;
// everything else numeric is set as double; anything else as a string.
// Completion is detected via BM_TEXT_END, because Engine::wait() does not work
// with a client-supplied output site.

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#  pragma warning(disable : 4996)
#  pragma warning(disable : 4250)
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

#include <ttsapi/Engine.h>
#include <ttsapi/Speaker.h>
#include <ttsapi/Bookmark.h>

using namespace MM_TTSAPI;

static const char* kIntNames[] = {
    "volumeSmoothWindow", "speedWPM", "defaultPitch", "pitchMin", "pitchMax", 0
};

static bool isIntParam(const std::string& n)
{
    for (int i = 0; kIntNames[i]; ++i) if (n == kIntNames[i]) return true;
    return false;
}

class MemSite : public IWaveOutputSite
{
public:
    MemSite(int sr, int bits) : m_fmt(sr, bits, WaveOutputFormat::WC_PCM_SIGNED), done(false)
    { InitializeCriticalSection(&cs); }
    ~MemSite() { DeleteCriticalSection(&cs); }

    CRITICAL_SECTION cs;
    std::vector<unsigned char> pcm;
    bool done;

    virtual const IOutputFormat& getOutputFormat() const { return m_fmt; }
    virtual void pause() {}
    virtual void play() {}
    virtual void clear() {}
    virtual void put(unsigned char* b, unsigned int n)
    {
        EnterCriticalSection(&cs);
        pcm.insert(pcm.end(), b, b + n);
        LeaveCriticalSection(&cs);
    }
    virtual void setBookmark(Bookmark* bm)
    {
        if (!bm) return;
        if (bm->type == BM_TEXT_END) { EnterCriticalSection(&cs); done = true; LeaveCriticalSection(&cs); }
        delete bm;
    }
    virtual void sendBookmark(Bookmark&) {}
    virtual void registerNotify(INotify*, const BookmarkTypeList&) {}
    virtual void unregisterNotify(INotify*) {}
    virtual void addBookmarkTypes(INotify*, const BookmarkTypeList&) {}
    virtual void removeBookmarkTypes(INotify*, const BookmarkTypeList&) {}
    virtual bool set(const char*, bool, int) { return false; }
    virtual bool get(const char*, bool&, int) const { return false; }
    virtual bool set(const char*, char, int) { return false; }
    virtual bool get(const char*, char&, int) const { return false; }
    virtual bool set(const char*, int, int) { return false; }
    virtual bool get(const char*, int&, int) const { return false; }
    virtual bool set(const char*, double, int) { return false; }
    virtual bool get(const char*, double&, int) const { return false; }
    virtual bool set(const char*, const char*, int) { return false; }
    virtual bool get(const char*, char*, int, int*, int) const { return false; }
    virtual bool setArraySize(const char*, int) { return false; }
    virtual bool getArraySize(const char*, int&) const { return false; }
    virtual const IAttribute* getAttribute(const char*) const { return 0; }
    virtual IAttribute* getAttribute(const char*) { return 0; }
private:
    WaveOutputFormat m_fmt;
};

static bool writeWav(const char* path, const std::vector<unsigned char>& pcm, int sr, int bits)
{
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const unsigned dataLen = (unsigned)pcm.size();
    const unsigned short ch = 1;
    const unsigned short ba = (unsigned short)(ch * bits / 8);
    const unsigned byteRate = (unsigned)sr * ba;
    const unsigned riffLen = 36 + dataLen;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riffLen, 4, 1, f);
    std::fwrite("WAVEfmt ", 1, 8, f);
    unsigned fmtLen = 16; unsigned short pcmTag = 1, bps = (unsigned short)bits;
    unsigned srate = (unsigned)sr;
    std::fwrite(&fmtLen, 4, 1, f); std::fwrite(&pcmTag, 2, 1, f); std::fwrite(&ch, 2, 1, f);
    std::fwrite(&srate, 4, 1, f); std::fwrite(&byteRate, 4, 1, f);
    std::fwrite(&ba, 2, 1, f); std::fwrite(&bps, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dataLen, 4, 1, f);
    if (dataLen) std::fwrite(&pcm[0], 1, dataLen, f);
    std::fclose(f);
    return true;
}

#include <crtdbg.h>
#include <stdlib.h>

// The FlexVoice engine calls abort() on some malformed input (e.g. a
// FlexVoice 2.0 .tav fed to the 3.01 engine).  Never let the CRT put a modal
// "abnormal program termination" box on a screen-reader user's desktop.
static void silenceCrtPopups()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
}

int main(int argc, char** argv)
{
    silenceCrtPopups();
    if (argc < 5) {
        std::printf("usage: fv_render <dataPath> <tav> <outWav> <text> [name=value ...]\n");
        return 2;
    }
    const char* dataPath = argv[1];
    const char* tav      = argv[2];
    const char* outWav   = argv[3];
    const char* text     = argv[4];

    const int SR = 16000, BITS = 16;

    try {
        EngineFactory factory(dataPath);
        factory.loadLanguage(LNG_ENGLISH);

        Speaker sp;
        sp.load(tav);

        for (int i = 5; i < argc; ++i) {
            std::string kv = argv[i];
            size_t eq = kv.find('=');
            if (eq == std::string::npos) continue;
            std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
            bool ok;
            if (isIntParam(k))            ok = sp.set(k.c_str(), std::atoi(v.c_str()));
            else if (!v.empty() && (v[0] == '-' || v[0] == '.' || (v[0] >= '0' && v[0] <= '9')))
                                          ok = sp.set(k.c_str(), std::atof(v.c_str()));
            else                          ok = sp.set(k.c_str(), v.c_str());
            std::printf("  set %s=%s -> %s\n", k.c_str(), v.c_str(), ok ? "ok" : "REJECTED");
        }

        factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

        MemSite site(SR, BITS);
        DWORD t0 = GetTickCount();
        {
            std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
            eng->speakRequest(text, 1);
            DWORD start = GetTickCount();
            for (;;) {
                EnterCriticalSection(&site.cs);
                bool d = site.done;
                LeaveCriticalSection(&site.cs);
                if (d) break;
                if (GetTickCount() - start > 60000) { std::printf("TIMEOUT\n"); break; }
                Sleep(2);
            }
        }
        DWORD ms = GetTickCount() - t0;

        if (site.pcm.empty()) { std::printf("NO AUDIO\n"); return 1; }
        if (!writeWav(outWav, site.pcm, SR, BITS)) { std::printf("WRITE FAILED\n"); return 1; }
        std::printf("OK %s bytes=%u dur=%.3f render=%lums\n", outWav,
                    (unsigned)site.pcm.size(), site.pcm.size() / (double)(SR * BITS / 8),
                    (unsigned long)ms);
    } catch (GenericException& e) {
        std::printf("EXCEPTION %s : %s\n", e.what(), e.details());
        return 1;
    } catch (...) {
        std::printf("EXCEPTION unknown\n");
        return 1;
    }
    return 0;
}
