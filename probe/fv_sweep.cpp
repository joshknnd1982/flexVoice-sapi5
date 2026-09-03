// fv_sweep.cpp -- render one WAV per value of a FlexVoice speaker/engine
// parameter, so the audible range of every parameter can be measured rather
// than guessed. The Speaker API accepts any number without complaint, so the
// real limits have to come from the audio.
//
// usage:
//   fv_sweep <dataPath> <tavFile> <outDir> <scope> <type> <name> <v1,v2,...> [text]
//     scope : speaker | engine
//     type  : int | dbl
//
// Writes <outDir>\<name>_<index>.wav and prints a manifest line per render:
//   VALUE <index> <value> <file> <synthMs>

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#  pragma warning(disable : 4996)
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

#include <ttsapi/Engine.h>
#include <ttsapi/Speaker.h>
#include <ttsapi/FileOutputSite.h>

using namespace MM_TTSAPI;

static std::vector<std::string> split(const char* s, char sep)
{
    std::vector<std::string> out;
    std::string cur;
    for (const char* p = s; ; ++p) {
        if (*p == sep || *p == '\0') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
            if (*p == '\0') break;
        } else {
            cur += *p;
        }
    }
    return out;
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
    if (argc < 8) {
        std::printf("usage: fv_sweep <dataPath> <tav> <outDir> <speaker|engine> <int|dbl> <name> <v1,v2,..> [text]\n");
        return 2;
    }
    const char* dataPath = argv[1];
    const char* tav      = argv[2];
    const char* outDir   = argv[3];
    const bool  onEngine = (std::strcmp(argv[4], "engine") == 0);
    const bool  isInt    = (std::strcmp(argv[5], "int") == 0);
    const char* name     = argv[6];
    std::vector<std::string> values = split(argv[7], ',');
    const char* text = (argc > 8) ? argv[8]
        : "She had your dark suit in greasy wash water all year.";

    CreateDirectoryA(outDir, NULL);

    try {
        EngineFactory factory(dataPath);
        factory.loadLanguage(LNG_ENGLISH);

        for (size_t i = 0; i < values.size(); ++i) {
            Speaker sp;
            sp.load(tav);

            const double dv = std::atof(values[i].c_str());
            const int    iv = std::atoi(values[i].c_str());

            if (!onEngine) {
                bool ok = isInt ? sp.set(name, iv) : sp.set(name, dv);
                if (!ok) {
                    std::printf("REJECT %d %s\n", (int)i, values[i].c_str());
                    continue;
                }
            }

            factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

            char fn[MAX_PATH];
            std::sprintf(fn, "%s\\%s_%02d.wav", outDir, name, (int)i);

            WaveOutputFormat fmt(16000, 16, WaveOutputFormat::WC_PCM_SIGNED);
            WaveFileOutputSite out(fmt);
            out.open(fn, WaveFileOutputSite::FF_WAV);

            DWORD t0 = GetTickCount();
            {
                std::auto_ptr<Engine> eng = factory.createEngine(&out, sp, LNG_ENGLISH);
                if (onEngine) {
                    bool ok = isInt ? eng->attribute().set(name, iv)
                                    : eng->attribute().set(name, dv);
                    if (!ok) {
                        std::printf("REJECT %d %s\n", (int)i, values[i].c_str());
                        out.close();
                        continue;
                    }
                }
                eng->speakRequest(text, 1);
                eng->wait();
            }
            DWORD t1 = GetTickCount();
            out.close();

            std::printf("VALUE %d %s %s %lu\n", (int)i, values[i].c_str(), fn,
                        (unsigned long)(t1 - t0));
            std::fflush(stdout);
        }
    } catch (GenericException& e) {
        std::printf("EXCEPTION %s : %s\n", e.what(), e.details());
        return 1;
    } catch (...) {
        std::printf("EXCEPTION unknown\n");
        return 1;
    }
    return 0;
}
