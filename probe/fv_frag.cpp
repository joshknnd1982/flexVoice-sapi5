// fv_frag.cpp -- why does a request built from several addFragment() calls
// only speak the first fragment?
//
// Compares, against a fresh engine each time:
//   A  one fragment holding the whole text
//   B  four consecutive addFragment calls
//   C  four addFragment calls with a bookmark after each
//   D  four addFragment calls, engine reset() before building
//   E  four addFragment calls on a REUSED engine (what the host does)

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

static const char* kPieces[] = { "Alpha bravo. ", "Charlie delta. ",
                                 "Echo foxtrot. ", "Golf hotel. " };

class Mark : public Bookmark {
public:
    explicit Mark(int i) : idx(i) { type = BM_USER; }
    virtual Bookmark* clone() const { return new Mark(*this); }
    int idx;
};

class Site : public IWaveOutputSite {
public:
    Site() : fmt_(16000, 16, WaveOutputFormat::WC_PCM_SIGNED) { InitializeCriticalSection(&cs); reset(); }
    ~Site() { DeleteCriticalSection(&cs); }
    CRITICAL_SECTION cs;
    unsigned bytes = 0;
    bool done = false;
    int marks = 0;
    void reset() { bytes = 0; done = false; marks = 0; }

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
        if (dynamic_cast<Mark*>(bm)) ++marks;
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

static void report(const char* label, Site& s)
{
    std::printf("  %-52s %7u bytes (%.2f s)  %d user marks  %s\n", label, s.bytes,
                s.bytes / 32000.0, s.marks, s.done ? "" : "NO TEXT_END");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    if (argc < 3) { std::printf("usage: fv_frag <dataPath> <tav>\n"); return 2; }
    EngineFactory factory(argv[1]);
    factory.loadLanguage(LNG_ENGLISH);
    Speaker sp;
    sp.load(argv[2]);
    factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

    std::string whole;
    for (int i = 0; i < 4; ++i) whole += kPieces[i];

    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->speakRequest(whole.c_str(), 1);
        waitDone(site, 15000);
        report("A  one speakRequest(text) with the whole string", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        for (int i = 0; i < 4; ++i) e->addFragment(kPieces[i]);
        e->speakRequest(1);
        waitDone(site, 15000);
        report("B  four addFragment, then speakRequest(1)", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        for (int i = 0; i < 4; ++i) { e->addFragment(kPieces[i]); e->addBookmark(new Mark(i)); }
        e->speakRequest(1);
        waitDone(site, 15000);
        report("C  four addFragment each followed by addBookmark", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->reset();
        for (int i = 0; i < 4; ++i) e->addFragment(kPieces[i]);
        e->speakRequest(1);
        waitDone(site, 15000);
        report("D  reset(), then four addFragment", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->speakRequest("Warm up. ", 1);
        waitDone(site, 15000);
        site.reset();
        for (int i = 0; i < 4; ++i) e->addFragment(kPieces[i]);
        e->speakRequest(1);
        waitDone(site, 15000);
        report("E  reused engine, four addFragment", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->speakRequest("Warm up. ", 1);
        waitDone(site, 15000);
        site.reset();
        e->reset();
        for (int i = 0; i < 4; ++i) e->addFragment(kPieces[i]);
        e->speakRequest(1);
        waitDone(site, 15000);
        report("H  reused engine, reset() first, four addFragment", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->speakRequest("Warm up. ", 1);
        waitDone(site, 15000);
        site.reset();
        e->stop();
        for (int i = 0; i < 4; ++i) e->addFragment(kPieces[i]);
        e->speakRequest(1);
        waitDone(site, 15000);
        report("I  reused engine, stop() first, four addFragment", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->speakRequest("Warm up. ", 1);
        waitDone(site, 15000);
        site.reset();
        std::string joined;
        for (int i = 0; i < 4; ++i) joined += kPieces[i];
        e->speakRequest(joined.c_str(), 1);
        waitDone(site, 15000);
        report("J  reused engine, single speakRequest(text)", site);
    }
    {
        // Three utterances in a row on one engine, each a single string.
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        for (int k = 0; k < 3; ++k) {
            site.reset();
            e->speakRequest(whole.c_str(), 1);
            waitDone(site, 15000);
            char label[80];
            std::sprintf(label, "K%d reused engine, speakRequest(text) round %d", k, k);
            report(label, site);
        }
    }
    {
        // Does a single addFragment plus speakRequest(1) even work?
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->addFragment(whole.c_str());
        e->speakRequest(1);
        waitDone(site, 15000);
        report("F  one addFragment with the whole string", site);
    }
    {
        // The exact fragment set the SAPI wrapper produced when it hung.
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->addFragment(" rspd=100 ");
        e->addFragment(" rpit=100 ");
        e->addFragment(" vol=100 ");
        e->addFragment("Testing one two three.");
        e->speakRequest(1);
        waitDone(site, 8000);
        report("L  the SAPI fragment set that hung", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->addFragment(" rspd=100 ");
        e->addFragment("Testing one two three.");
        e->speakRequest(1);
        waitDone(site, 8000);
        report("M  one command-ish fragment plus text", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->addFragment(" rspd=100 ");
        e->speakRequest(1);
        waitDone(site, 8000);
        report("N  a single fragment of \" rspd=100 \"", site);
    }
    {
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->addFragment(" a ");
        e->addFragment(" b ");
        e->addFragment(" c ");
        e->addFragment("Testing one two three.");
        e->speakRequest(1);
        waitDone(site, 8000);
        report("O  three tiny fragments plus a sentence", site);
    }
    {
        // Real embedded commands, not the mangled ones.
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->addFragment("\\rspd=100\\");
        e->addFragment("Testing one two three.");
        e->speakRequest(1);
        waitDone(site, 8000);
        report("P  a genuine embedded command plus text", site);
    }
    {
        // Fragments that do not end at a sentence boundary.
        Site site;
        std::auto_ptr<Engine> e = factory.createEngine(&site, sp, LNG_ENGLISH);
        e->addFragment("Alpha ");
        e->addFragment("bravo ");
        e->addFragment("charlie ");
        e->addFragment("delta.");
        e->speakRequest(1);
        waitDone(site, 15000);
        report("G  four mid-sentence fragments", site);
    }
    return 0;
}
