// fv_one.cpp -- speak exactly one string, in its own process, and report.
//
// The engine can both hang forever and fault on hostile input, and a hung
// engine cannot be torn down safely, so a single process cannot survey a range
// of inputs. This does one case and exits; probe/textscan.py drives it.
//
//   fv_one <dataPath> <tav> <text> [timeoutMs]
//
// exit 0  spoke, got BM_TEXT_END        (prints "OK <bytes>")
// exit 1  no BM_TEXT_END before timeout (prints "HANG <bytes>")
// exit 2  bad arguments
// exit 3  the engine threw               (prints "THROW <what>")
// anything else: the engine faulted, and the exit code is the fault.

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#  pragma warning(disable : 4996)
#  pragma warning(disable : 4250)
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <windows.h>
#include <crtdbg.h>

#include <ttsapi/Engine.h>
#include <ttsapi/Speaker.h>
#include <ttsapi/Bookmark.h>

using namespace MM_TTSAPI;

namespace {

class Site : public IWaveOutputSite {
public:
    Site() : fmt_(16000, 16, WaveOutputFormat::WC_PCM_SIGNED)
    { InitializeCriticalSection(&cs); }
    CRITICAL_SECTION cs;
    unsigned bytes = 0;
    bool done = false;

    virtual const IOutputFormat& getOutputFormat() const { return fmt_; }
    virtual void pause() {}
    virtual void play() {}
    virtual void clear() {}
    virtual void put(unsigned char*, unsigned int n)
    { EnterCriticalSection(&cs); bytes += n; LeaveCriticalSection(&cs); }
    virtual void setBookmark(Bookmark* bm)
    {
        if (!bm) return;
        EnterCriticalSection(&cs);
        if (bm->type == BM_TEXT_END) done = true;
        LeaveCriticalSection(&cs);
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
    WaveOutputFormat fmt_;
};

}  // namespace

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);

    if (argc < 4) { std::printf("usage: fv_one <dataPath> <tav> <text> [timeoutMs]\n"); return 2; }
    const DWORD timeout = (argc > 4) ? static_cast<DWORD>(std::atoi(argv[4])) : 4000;

    try {
        EngineFactory factory(argv[1]);
        factory.loadLanguage(LNG_ENGLISH);
        Speaker sp;
        sp.load(argv[2]);
        factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

        // Leaked on purpose: if the engine hangs, its worker thread is still
        // inside the synthesiser and destroying it faults.
        Site* site = new Site();
        Engine* e = factory.createEngine(site, sp, LNG_ENGLISH).release();
        e->addFragment(argv[3]);
        e->speakRequest(1);

        const DWORD start = GetTickCount();
        for (;;) {
            EnterCriticalSection(&site->cs);
            const bool d = site->done;
            LeaveCriticalSection(&site->cs);
            if (d) { std::printf("OK %u\n", site->bytes); return 0; }
            if (GetTickCount() - start > timeout) {
                std::printf("HANG %u\n", site->bytes);
                std::fflush(stdout);
                // Do not unwind: TerminateProcess is the only safe exit from a
                // hung engine.
                TerminateProcess(GetCurrentProcess(), 1);
            }
            Sleep(1);
        }
    } catch (GenericException& ex) {
        std::printf("THROW %s: %s\n", ex.what(), ex.details());
        return 3;
    } catch (...) {
        std::printf("THROW unknown\n");
        return 3;
    }
}
