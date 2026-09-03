// fv_frag2.cpp -- which text makes the FlexVoice engine hang for good?
//
// " rspd=100 " produces no audio and no BM_TEXT_END, ever. This narrows the
// trigger down to a character class so the wrapper can neutralise it, and
// gives each case its own engine because a hung engine never recovers.

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#  pragma warning(disable : 4996)
#  pragma warning(disable : 4250)
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <windows.h>
#include <crtdbg.h>

#include <ttsapi/Engine.h>
#include <ttsapi/Speaker.h>
#include <ttsapi/Bookmark.h>

using namespace MM_TTSAPI;

class Site : public IWaveOutputSite {
public:
    Site() : fmt_(16000, 16, WaveOutputFormat::WC_PCM_SIGNED)
    { InitializeCriticalSection(&cs); }
    ~Site() { DeleteCriticalSection(&cs); }
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

static void waitDone(Site& s, DWORD ms)
{
    const DWORD start = GetTickCount();
    for (;;) {
        EnterCriticalSection(&s.cs);
        const bool d = s.done;
        LeaveCriticalSection(&s.cs);
        if (d || GetTickCount() - start > ms) return;
        Sleep(1);
    }
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    if (argc < 3) { std::printf("usage: fv_frag2 <dataPath> <tav>\n"); return 2; }
    EngineFactory factory(argv[1]);
    factory.loadLanguage(LNG_ENGLISH);
    Speaker sp;
    sp.load(argv[2]);
    factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

    static const char* kCases[] = {
        "rspd=100", " rspd=100 ", "rspd = 100", "a=1", "a = 1", "ab=12",
        "x=y", "100=100", "=", "= ", "a=", "=1", "one=two",
        "hello world", "3+4", "a<b", "50%", "a&b", "a_b", "a#b", "a$b",
        "C:/Users/joshk", "e-mail@example.com", "1/2", "a*b", "a|b",
        "a~b", "a^b", "a`b", "a[b]", "a{b}", "a\"b", "a'b", "a;b",
        "12:34", "a\tb", "\n", " ", "", ".", "...", "!!!",
        "The quick brown fox.",
        0
    };

    std::printf("factory ready, %d cases\n",
                (int)(sizeof(kCases) / sizeof(kCases[0])) - 1);
    std::fflush(stdout);

    for (int i = 0; kCases[i]; ++i) {
        std::printf("case %d ...", i); std::fflush(stdout);
        // A hung engine is never safe to destroy: its worker thread is still
        // inside the synthesiser. Leak it instead.
        Site* sitep = new Site();
        Site& site = *sitep;
        Engine* e = factory.createEngine(&site, sp, LNG_ENGLISH).release();
        e->addFragment(kCases[i]);
        e->speakRequest(1);
        waitDone(site, 4000);

        // Print the case with control characters escaped so the table stays readable.
        std::string shown;
        for (const char* p = kCases[i]; *p; ++p) {
            if (*p == '\t') shown += "\\t";
            else if (*p == '\n') shown += "\\n";
            else shown += *p;
        }
        std::printf("\r  [%-22s] %8u bytes  %s\n", shown.c_str(), site.bytes,
                    site.done ? "ok" : "HANGS (no TEXT_END)");
        std::fflush(stdout);
        e->stop();
        // Only tear a healthy engine down; a hung one still has a worker
        // thread inside the synthesiser and destroying it faults.
        if (site.done) { delete e; delete sitep; }
    }
    return 0;
}
