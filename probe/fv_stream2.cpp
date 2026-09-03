// fv_stream2.cpp -- follow-ups to fv_stream:
//   A. Confirm Engine::wait() races after a fresh createEngine, and find the
//      bookmark that reliably signals "utterance finished".
//   B. Verify client BM_USER bookmarks survive the round trip in order.
//   C. Find the exact speaker parameter values that crash the engine
//      (the volume and tilt sweeps died with an unknown exception).
//   D. Measure the real first-audio latency and chunk cadence.

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#  pragma warning(disable : 4996)
#  pragma warning(disable : 4250)
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

#include <ttsapi/Engine.h>
#include <ttsapi/Speaker.h>
#include <ttsapi/Bookmark.h>

using namespace MM_TTSAPI;

static DWORD g_t0;

static const char* bmName(int t)
{
    static const char* n[] = {
        "INVALID", "TEXT_BEGIN", "TEXT_END", "SECTION_BEGIN", "SECTION_END",
        "PARAGRAPH_BEGIN", "PARAGRAPH_END", "SENTENCE_BEGIN", "SENTENCE_END",
        "ITEM_BEGIN", "ITEM_END", "WORD_BEGIN", "WORD_END", "PHONEME_BEGIN",
        "PHONEME_END", "IPAPHONEME_BEGIN", "IPAPHONEME_END", "SPEAKERCHANGED",
        "PARAMCHANGED", "USER", "EMBEDDED", "PAUSE", "RESUME", "START", "STOP",
        "REPEAT" };
    if (t >= 0 && t < (int)(sizeof(n) / sizeof(n[0]))) return n[t];
    return "OUT_OF_RANGE";
}

class IndexBookmark : public Bookmark
{
public:
    explicit IndexBookmark(int idx) : index(idx), magic(0x53415049u) { type = BM_USER; }
    virtual Bookmark* clone() const { return new IndexBookmark(*this); }
    int index;
    unsigned int magic;
};

class Site : public IWaveOutputSite
{
public:
    Site(int sr, int bits) : m_fmt(sr, bits, WaveOutputFormat::WC_PCM_SIGNED)
    { InitializeCriticalSection(&cs); reset(); }
    ~Site() { DeleteCriticalSection(&cs); }

    CRITICAL_SECTION cs;
    unsigned puts, bytes;
    DWORD firstPutMs, lastPutMs;
    bool sawTextEnd, sawStop;
    std::vector<int> userIdx;
    std::vector<std::string> order;   // interleaving of audio and bookmarks
    bool trace;

    void reset()
    {
        puts = bytes = 0; firstPutMs = lastPutMs = 0;
        sawTextEnd = sawStop = false;
        userIdx.clear(); order.clear(); trace = false;
    }

    virtual const IOutputFormat& getOutputFormat() const { return m_fmt; }
    virtual void pause() {}
    virtual void play() {}
    virtual void clear() {}

    virtual void put(unsigned char*, unsigned int size)
    {
        EnterCriticalSection(&cs);
        if (!puts) firstPutMs = GetTickCount() - g_t0;
        puts++; bytes += size; lastPutMs = GetTickCount() - g_t0;
        if (trace && order.size() < 400) {
            char b[64]; std::sprintf(b, "audio(%u)", size); order.push_back(b);
        }
        LeaveCriticalSection(&cs);
    }

    virtual void setBookmark(Bookmark* bm)
    {
        if (!bm) return;
        EnterCriticalSection(&cs);
        if (bm->type == BM_TEXT_END) sawTextEnd = true;
        if (bm->type == BM_STOP) sawStop = true;
        IndexBookmark* ib = dynamic_cast<IndexBookmark*>(bm);
        if (ib) {
            userIdx.push_back(ib->magic == 0x53415049u ? ib->index : -999999);
            if (trace && order.size() < 400) {
                char b[64]; std::sprintf(b, "USER(%d)", ib->index); order.push_back(b);
            }
        } else if (trace && order.size() < 400 &&
                   (bm->type == BM_WORD_BEGIN || bm->type == BM_SENTENCE_BEGIN ||
                    bm->type == BM_TEXT_END || bm->type == BM_TEXT_BEGIN)) {
            char b[80]; std::sprintf(b, "%s(pos=%d,len=%d)", bmName(bm->type), bm->pos, bm->len);
            order.push_back(b);
        }
        LeaveCriticalSection(&cs);
        delete bm;
    }

    virtual void sendBookmark(Bookmark& bm)
    {
        EnterCriticalSection(&cs);
        if (bm.type == BM_STOP) sawStop = true;
        LeaveCriticalSection(&cs);
    }

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

// Wait for BM_TEXT_END instead of Engine::wait().
static bool waitForTextEnd(Site& s, DWORD timeoutMs)
{
    DWORD start = GetTickCount();
    for (;;) {
        EnterCriticalSection(&s.cs);
        bool done = s.sawTextEnd;
        LeaveCriticalSection(&s.cs);
        if (done) return true;
        if (GetTickCount() - start > timeoutMs) return false;
        Sleep(2);
    }
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
    g_t0 = GetTickCount();
    if (argc < 3) { std::printf("usage: fv_stream2 <dataPath> <tav>\n"); return 2; }
    const char* dataPath = argv[1];
    const char* tav = argv[2];

    EngineFactory factory(dataPath);
    factory.loadLanguage(LNG_ENGLISH);
    Speaker sp;
    sp.load(tav);
    factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

    // ---- A: is wait() reliable, and is BM_TEXT_END? -----------------------
    std::printf("=========== A: wait() vs BM_TEXT_END, 10 fresh engines ===========\n");
    int waitOk = 0, textEndOk = 0;
    for (int i = 0; i < 10; ++i) {
        Site site(16000, 16);
        std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
        eng->speakRequest("Testing completion detection on a fresh engine instance.", 1);
        eng->wait();
        unsigned afterWait = site.bytes;
        bool te = waitForTextEnd(site, 5000);
        unsigned afterTextEnd = site.bytes;
        if (afterWait > 0) waitOk++;
        if (te && afterTextEnd > 0) textEndOk++;
        std::printf("  run %2d: after wait()=%7u bytes | TEXT_END seen=%d, after=%7u bytes\n",
                    i, afterWait, (int)te, afterTextEnd);
    }
    std::printf("  => wait() produced audio %d/10 times; BM_TEXT_END worked %d/10 times\n",
                waitOk, textEndOk);

    // ---- B: client BM_USER bookmarks --------------------------------------
    std::printf("\n=========== B: client BM_USER bookmarks, interleaving ===========\n");
    {
        Site site(16000, 16);
        site.trace = true;
        std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
        eng->addFragment("Alpha bravo charlie. ");
        eng->addBookmark(new IndexBookmark(101));
        eng->addFragment("Delta echo foxtrot. ");
        eng->addBookmark(new IndexBookmark(202));
        eng->addFragment("Golf hotel india.");
        eng->addBookmark(new IndexBookmark(303));
        eng->speakRequest(1);
        bool te = waitForTextEnd(site, 10000);
        std::printf("  TEXT_END=%d bytes=%u  user bookmarks received:", (int)te, site.bytes);
        for (size_t i = 0; i < site.userIdx.size(); ++i) std::printf(" %d", site.userIdx[i]);
        std::printf("\n  event order:\n    ");
        int col = 0;
        for (size_t i = 0; i < site.order.size(); ++i) {
            std::printf("%s ", site.order[i].c_str());
            if (++col % 6 == 0) std::printf("\n    ");
        }
        std::printf("\n");
    }

    // ---- C: which parameter values kill the engine? -----------------------
    std::printf("\n=========== C: crash hunt on speaker parameters ===========\n");
    struct P { const char* name; bool isInt; double vals[12]; int n; };
    P probes[] = {
        {"volume",  false, {0.0, 1.0, 5.0, 10.0, 25.0, 40.0, 60.0, 80.0, 100.0, 150.0, 200.0, 0}, 11},
        {"tilt",    false, {-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 4.0, 0, 0, 0, 0}, 8},
        {"headsize",false, {0.05, 0.1, 0.2, 0.3, 4.0, 6.0, 10.0, 0, 0, 0, 0, 0}, 7},
        {"smoothness", false, {-1.0, 0.0, 3.0, 5.0, 10.0, 0, 0, 0, 0, 0, 0, 0}, 5},
        {"breathiness", false, {-1.0, 3.0, 5.0, 10.0, 0, 0, 0, 0, 0, 0, 0, 0}, 4},
        {"richness", false, {-1.0, 5.0, 10.0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 3},
        {"defaultPitch", true, {0, 1, 10, 20, 600, 800, 1000, 2000, 0, 0, 0, 0}, 8},
        {"pitchMin", true, {0, 1, 300, 400, 600, 0, 0, 0, 0, 0, 0, 0}, 5},
        {"pitchMax", true, {10, 40, 60, 100, 2000, 5000, 0, 0, 0, 0, 0, 0}, 6},
        {"intonationLevel", false, {-1.0, 10.0, 20.0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 3},
        {"fricationRate", false, {-1.0, 8.0, 20.0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 3},
        {"plosiveRate", false, {-1.0, 8.0, 20.0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 3},
    };
    for (size_t p = 0; p < sizeof(probes) / sizeof(probes[0]); ++p) {
        for (int k = 0; k < probes[p].n; ++k) {
            std::printf("  %-16s = %-10g ... ", probes[p].name, probes[p].vals[k]);
            std::fflush(stdout);
            try {
                Speaker t;
                t.load(tav);
                if (probes[p].isInt) t.set(probes[p].name, (int)probes[p].vals[k]);
                else                 t.set(probes[p].name, probes[p].vals[k]);
                factory.addSpeaker(LNG_ENGLISH, t, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);
                Site site(16000, 16);
                std::auto_ptr<Engine> eng = factory.createEngine(&site, t, LNG_ENGLISH);
                eng->speakRequest("Test one two three.", 1);
                bool te = waitForTextEnd(site, 8000);
                std::printf("ok  bytes=%-8u %s\n", site.bytes, te ? "" : "(NO TEXT_END - timed out)");
            } catch (GenericException& e) {
                std::printf("THREW %s : %s\n", e.what(), e.details());
            } catch (...) {
                std::printf("THREW <unknown>\n");
            }
            std::fflush(stdout);
        }
    }

    // ---- D: first-audio latency and chunk cadence -------------------------
    std::printf("\n=========== D: first-audio latency (warm engine, 10 utterances) ===========\n");
    {
        Site site(16000, 16);
        std::auto_ptr<Engine> eng = factory.createEngine(&site, sp, LNG_ENGLISH);
        eng->speakRequest("Warm up.", 1);
        waitForTextEnd(site, 5000);
        for (int i = 0; i < 10; ++i) {
            site.reset();
            DWORD a = GetTickCount();
            eng->speakRequest("The quick brown fox jumps over the lazy dog.", 1);
            while (site.puts == 0 && GetTickCount() - a < 3000) Sleep(1);
            DWORD firstAudio = GetTickCount() - a;
            waitForTextEnd(site, 8000);
            DWORD total = GetTickCount() - a;
            std::printf("  utt %2d: first audio %3lu ms, all %u chunks / %u bytes (%.2f s) in %lu ms\n",
                        i, (unsigned long)firstAudio, site.puts, site.bytes,
                        site.bytes / 32000.0, (unsigned long)total);
        }
    }

    std::printf("\n=========== done ===========\n");
    return 0;
}
