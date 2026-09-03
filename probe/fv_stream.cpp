// fv_stream.cpp -- answer the questions that decide the SAPI5 wrapper's shape:
//
//   1. Can a client-side IWaveOutputSite receive audio from the 2002 DLL at all
//      (vtable + std::bitset across the VC6/MSVC-2022 boundary)?
//   2. Is speakRequest() asynchronous, and which thread calls put()?
//   3. How long after Engine::stop() does audio keep arriving?  (SAPI cancel latency)
//   4. Do BM_USER bookmarks we allocate come back in order, interleaved with audio?
//   5. Do the engine's own BM_WORD_BEGIN / BM_SENTENCE_BEGIN bookmarks work,
//      and do they carry usable input character offsets for SAPI word events?
//   6. Is it safe to delete a Bookmark the engine allocated?
//   7. What is the first-audio latency from speakRequest to the first put()?

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#  pragma warning(disable : 4996)
#  pragma warning(disable : 4250)   // dominance via virtual inheritance
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

#include <ttsapi/Engine.h>
#include <ttsapi/Speaker.h>
#include <ttsapi/Bookmark.h>
#include <ttsapi/Notifier.h>

using namespace MM_TTSAPI;

static DWORD g_t0;
static DWORD g_mainThread;
static CRITICAL_SECTION g_cs;

static void logf(const char* fmt, ...)
{
    EnterCriticalSection(&g_cs);
    va_list ap;
    va_start(ap, fmt);
    std::printf("[%5lu ms  tid %5lu] ", (unsigned long)(GetTickCount() - g_t0),
                (unsigned long)GetCurrentThreadId());
    std::vprintf(fmt, ap);
    va_end(ap);
    std::fflush(stdout);
    LeaveCriticalSection(&g_cs);
}

// A bookmark we allocate ourselves, to be handed to Engine::addBookmark.
class IndexBookmark : public Bookmark
{
public:
    explicit IndexBookmark(int idx) : index(idx), magic(0x53415049) { type = BM_USER; id = 0x53415049; }
    virtual Bookmark* clone() const { return new IndexBookmark(*this); }
    int index;
    unsigned int magic;
};

struct Stats
{
    unsigned int puts;
    unsigned int bytes;
    DWORD        firstPutMs;
    DWORD        lastPutMs;
    DWORD        stopIssuedMs;
    unsigned int putsAfterStop;
    unsigned int bytesAfterStop;
    bool         stopped;
    Stats() : puts(0), bytes(0), firstPutMs(0), lastPutMs(0), stopIssuedMs(0),
              putsAfterStop(0), bytesAfterStop(0), stopped(false) {}
};

class ProbeSite : public IWaveOutputSite
{
public:
    ProbeSite(int sr, int bits, bool verbose)
        : m_fmt(sr, bits, WaveOutputFormat::WC_PCM_SIGNED)
        , m_verbose(verbose)
        , m_putThread(0)
        , m_sleepPerPutMs(0)
    {}

    Stats stats;
    DWORD m_putThread;
    DWORD m_sleepPerPutMs;   // simulate a real audio device that blocks

    // --- IOutputSite ------------------------------------------------------
    virtual const IOutputFormat& getOutputFormat() const { return m_fmt; }
    virtual void pause() { logf("site.pause()\n"); }
    virtual void play()  { logf("site.play()\n"); }
    virtual void clear() { logf("site.clear()\n"); }

    virtual void put(unsigned char* buffer, unsigned int size)
    {
        if (!m_putThread) {
            m_putThread = GetCurrentThreadId();
            stats.firstPutMs = GetTickCount() - g_t0;
            logf("FIRST put() size=%u  (main tid was %lu)\n", size, (unsigned long)g_mainThread);
        }
        stats.puts++;
        stats.bytes += size;
        stats.lastPutMs = GetTickCount() - g_t0;
        if (stats.stopped) { stats.putsAfterStop++; stats.bytesAfterStop += size; }
        if (m_verbose && stats.puts <= 6)
            logf("  put #%u size=%u\n", stats.puts, size);
        (void)buffer;
        if (m_sleepPerPutMs) Sleep(m_sleepPerPutMs);
    }

    virtual void setBookmark(Bookmark* bm)
    {
        if (!bm) return;
        describe("setBookmark", *bm);
        // Ownership is transferred to us.  Bookmark has a virtual destructor, so
        // the deleting destructor in the vtable belongs to whichever module
        // constructed the object -- deleting an engine-allocated bookmark here
        // should therefore free it on the engine's heap, not ours.
        delete bm;
    }

    virtual void sendBookmark(Bookmark& bm) { describe("sendBookmark", bm); }

    // --- INotifyDispatcher ------------------------------------------------
    virtual void registerNotify(INotify*, const BookmarkTypeList&) { logf("site.registerNotify()\n"); }
    virtual void unregisterNotify(INotify*) {}
    virtual void addBookmarkTypes(INotify*, const BookmarkTypeList&) {}
    virtual void removeBookmarkTypes(INotify*, const BookmarkTypeList&) {}

    // --- IAttribute (we support nothing) ----------------------------------
    virtual bool set(const char*, bool, int)   { return false; }
    virtual bool get(const char*, bool&, int) const { return false; }
    virtual bool set(const char*, char, int)   { return false; }
    virtual bool get(const char*, char&, int) const { return false; }
    virtual bool set(const char*, int, int)    { return false; }
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
    void describe(const char* how, const Bookmark& bm)
    {
        const char* kind = "?";
        switch (bm.type) {
        case BM_TEXT_BEGIN: kind = "TEXT_BEGIN"; break;
        case BM_TEXT_END: kind = "TEXT_END"; break;
        case BM_SENTENCE_BEGIN: kind = "SENTENCE_BEGIN"; break;
        case BM_SENTENCE_END: kind = "SENTENCE_END"; break;
        case BM_WORD_BEGIN: kind = "WORD_BEGIN"; break;
        case BM_WORD_END: kind = "WORD_END"; break;
        case BM_PHONEME_BEGIN: kind = "PHONEME_BEGIN"; break;
        case BM_PHONEME_END: kind = "PHONEME_END"; break;
        case BM_USER: kind = "USER"; break;
        case BM_EMBEDDED: kind = "EMBEDDED"; break;
        case BM_START: kind = "START"; break;
        case BM_STOP: kind = "STOP"; break;
        case BM_PAUSE: kind = "PAUSE"; break;
        case BM_RESUME: kind = "RESUME"; break;
        case BM_PARAMCHANGED: kind = "PARAMCHANGED"; break;
        case BM_SPEAKERCHANGED: kind = "SPEAKERCHANGED"; break;
        case BM_REPEAT: kind = "REPEAT"; break;
        default: break;
        }
        const IndexBookmark* ib = dynamic_cast<const IndexBookmark*>(&bm);
        logf("%s type=%-15s pos=%-5d len=%-4d dur=%-5d id=%lu%s\n",
             how, kind, bm.pos, bm.len, bm.dur, bm.id,
             ib ? "   <-- OUR IndexBookmark" : "");
        if (ib) logf("        our index = %d (magic %s)\n", ib->index,
                     ib->magic == 0x53415049u ? "ok" : "CORRUPT");
    }

    WaveOutputFormat m_fmt;
    bool m_verbose;
};

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
    InitializeCriticalSection(&g_cs);
    g_t0 = GetTickCount();
    g_mainThread = GetCurrentThreadId();

    if (argc < 3) { std::printf("usage: fv_stream <dataPath> <tav>\n"); return 2; }
    const char* dataPath = argv[1];
    const char* tav = argv[2];

    const char* longText =
        "This is the first sentence of a deliberately long passage. "
        "The second sentence continues the thought at some length. "
        "A third sentence follows, and then a fourth, so that there is "
        "plenty of audio still queued when the cancel request arrives. "
        "Five. Six. Seven. Eight. Nine. Ten. Eleven. Twelve. Thirteen. Fourteen.";

    try {
        logf("EngineFactory(\"%s\")\n", dataPath);
        EngineFactory factory(dataPath);
        factory.loadLanguage(LNG_ENGLISH);

        Speaker sp;
        sp.load(tav);
        factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

        // ---------------- test 1: plain streaming ------------------------
        std::printf("\n=========== TEST 1: streaming, no blocking in put() ===========\n");
        {
            ProbeSite site(16000, 16, true);
            std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
            logf("speakRequest(...)\n");
            DWORD a = GetTickCount();
            eng->speakRequest(longText, 1);
            DWORD b = GetTickCount();
            logf("speakRequest returned after %lu ms  -> %s\n",
                 (unsigned long)(b - a), (b - a) < 50 ? "ASYNCHRONOUS" : "possibly synchronous");
            eng->wait();
            logf("wait() returned. puts=%u bytes=%u (%.2f s of audio) first put at %lu ms\n",
                 site.stats.puts, site.stats.bytes, site.stats.bytes / 32000.0,
                 (unsigned long)site.stats.firstPutMs);
            logf("put() ran on tid %lu (main is %lu) -> %s\n",
                 (unsigned long)site.m_putThread, (unsigned long)g_mainThread,
                 site.m_putThread == g_mainThread ? "SAME THREAD as caller"
                                                  : "ENGINE WORKER THREAD");
        }

        // ---------------- test 2: cancel latency -------------------------
        std::printf("\n=========== TEST 2: Engine::stop() latency ===========\n");
        for (int trial = 0; trial < 3; ++trial) {
            ProbeSite site(16000, 16, false);
            site.m_sleepPerPutMs = 20;   // pretend put() writes to a sound card
            std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
            eng->speakRequest(longText, 1);
            Sleep(250);                   // let it get going
            DWORD s0 = GetTickCount();
            site.stats.stopped = true;
            site.stats.stopIssuedMs = s0 - g_t0;
            eng->stop();
            DWORD s1 = GetTickCount();
            Sleep(200);                   // observe any trailing puts
            logf("trial %d: stop() blocked %lu ms; %u put(s) / %u bytes (%.0f ms audio) arrived after stop\n",
                 trial, (unsigned long)(s1 - s0), site.stats.putsAfterStop,
                 site.stats.bytesAfterStop, site.stats.bytesAfterStop / 32.0);
        }

        // ---------------- test 3: our own BM_USER bookmarks --------------
        std::printf("\n=========== TEST 3: client bookmarks via addFragment/addBookmark ===========\n");
        {
            ProbeSite site(16000, 16, false);
            std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
            eng->addFragment("Alpha bravo. ");
            eng->addBookmark(new IndexBookmark(101));
            eng->addFragment("Charlie delta. ");
            eng->addBookmark(new IndexBookmark(202));
            eng->addFragment("Echo foxtrot.");
            eng->addBookmark(new IndexBookmark(303));
            eng->speakRequest(1);
            eng->wait();
            logf("done. puts=%u bytes=%u\n", site.stats.puts, site.stats.bytes);
        }

        // ---------------- test 4: engine's own bookmark types ------------
        std::printf("\n=========== TEST 4: engine bookmarks (word/sentence/phoneme) ===========\n");
        {
            ProbeSite site(16000, 16, false);
            std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
            // The engine decides which bookmark types it emits to the site; the
            // site sees everything and we just log it.
            eng->speakRequest("One two three.", 1);
            eng->wait();
            logf("done. puts=%u bytes=%u\n", site.stats.puts, site.stats.bytes);
        }

        // ---------------- test 5: reuse + rapid restart ------------------
        std::printf("\n=========== TEST 5: rapid stop/speak cycling on one engine ===========\n");
        {
            ProbeSite site(16000, 16, false);
            std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
            DWORD worst = 0;
            for (int i = 0; i < 12; ++i) {
                eng->speakRequest(longText, 1);
                Sleep(30);
                DWORD a = GetTickCount();
                eng->stop();
                DWORD d = GetTickCount() - a;
                if (d > worst) worst = d;
            }
            logf("12 speak/stop cycles survived; worst stop() = %lu ms; total puts=%u\n",
                 (unsigned long)worst, site.stats.puts);
            eng->speakRequest("Still alive after cycling.", 1);
            eng->wait();
            logf("post-cycle utterance produced %u bytes total\n", site.stats.bytes);
        }

        // ---------------- test 6: live parameter changes -----------------
        std::printf("\n=========== TEST 6: changing rate/pitch/volume mid-stream ===========\n");
        {
            ProbeSite site(16000, 16, false);
            std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
            eng->speakRequest(longText, 1);
            Sleep(100);
            DWORD a = GetTickCount();
            bool r1 = eng->attribute().set("speechRate", 2.0);
            bool r2 = eng->attribute().set("pitchRate", 1.4);
            bool r3 = eng->attribute().set("volume", 0.6);
            logf("mid-stream attribute sets: rate=%d pitch=%d vol=%d, took %lu ms\n",
                 (int)r1, (int)r2, (int)r3, (unsigned long)(GetTickCount() - a));
            eng->wait();
            logf("done. bytes=%u\n", site.stats.bytes);
        }

        std::printf("\n=========== all tests completed without crashing ===========\n");
    } catch (GenericException& e) {
        std::printf("\n!! FlexVoice exception: %s : %s\n", e.what(), e.details());
        return 1;
    } catch (...) {
        std::printf("\n!! unknown exception\n");
        return 1;
    }
    return 0;
}
