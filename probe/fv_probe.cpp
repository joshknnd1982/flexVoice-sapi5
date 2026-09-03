// fv_probe.cpp -- FlexVoice 3.01 discovery probe (32-bit).
//
// Proves the 2002 MindMaker SDK links and runs under MSVC 2022, then dumps
// everything the engine will tell us about itself: version, data paths, the
// speaker parameter set with live values, and a rendered WAV per voice.
//
// Build: see probe/build_probe.bat

#ifdef _MSC_VER
#  pragma warning(disable : 4786)
#  pragma warning(disable : 4996)
#endif

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <windows.h>

#include <ttsapi/Engine.h>
#include <ttsapi/Speaker.h>
#include <ttsapi/FileOutputSite.h>
#include <ttsapi/FVVersion.h>
#include <ttsapi/FVLanguage.h>

using namespace MM_TTSAPI;

// Every parameter name Speaker.h documents, plus a few plausible aliases we
// want the engine itself to confirm or deny.
static const char* kStringParams[] = {
    "version", "name", "filename", "gender", "age", "synthType", "voiceDescr",
    "boundaryModel", "boundaryPostModel", "prominenceModel", "durationModel",
    "pitchModel", "loudnessModel", "loudnessModelType", "vsShape", "styleDescr",
    "vsShapeFileName", "language",
};

static const char* kIntParams[] = {
    "volumeSmoothWindow", "speedWPM", "defaultPitch", "pitchMin", "pitchMax",
};

static const char* kDoubleParams[] = {
    "volume", "speechRate", "pitchRate", "singingPitchRate", "intonationLevel",
    "headsize", "tilt", "richness", "breathiness", "creakiness", "smoothness",
    "fricationRate", "plosiveRate",
};

static const char* kStructParams[] = {
    "subharmonicFilter", "voicingSourceHarmFilter", "voicingSourceNoiseFilter",
    "vsHarmFilter", "vsNoiseFilter", "equalizer", "vsNoiseParams",
};

static void dumpAttribute(const IAttribute& a, const char* label)
{
    std::printf("\n--- attributes of %s ---\n", label);

    for (size_t i = 0; i < sizeof(kStringParams) / sizeof(kStringParams[0]); ++i) {
        char buf[1024] = {0};
        int req = 0;
        if (a.get(kStringParams[i], buf, sizeof(buf), &req)) {
            std::printf("  str %-22s = \"%s\"   (reqsize=%d)\n", kStringParams[i], buf, req);
        } else {
            std::printf("  str %-22s = <not supported>\n", kStringParams[i]);
        }
    }
    for (size_t i = 0; i < sizeof(kIntParams) / sizeof(kIntParams[0]); ++i) {
        int v = 0;
        if (a.get(kIntParams[i], v)) {
            std::printf("  int %-22s = %d\n", kIntParams[i], v);
        } else {
            std::printf("  int %-22s = <not supported>\n", kIntParams[i]);
        }
    }
    for (size_t i = 0; i < sizeof(kDoubleParams) / sizeof(kDoubleParams[0]); ++i) {
        double v = 0;
        if (a.get(kDoubleParams[i], v)) {
            std::printf("  dbl %-22s = %.6f\n", kDoubleParams[i], v);
        } else {
            std::printf("  dbl %-22s = <not supported>\n", kDoubleParams[i]);
        }
    }
    for (size_t i = 0; i < sizeof(kStructParams) / sizeof(kStructParams[0]); ++i) {
        const IAttribute* sub = a.getAttribute(kStructParams[i]);
        if (!sub) {
            std::printf("  obj %-22s = <not supported>\n", kStructParams[i]);
            continue;
        }
        std::printf("  obj %-22s = present\n", kStructParams[i]);
        static const char* subInt[] = {"f1", "f2", "channels"};
        static const char* subDbl[] = {"d1", "d2", "peekAmpl", "startPos", "floor",
                                       "noisePeakAmpl", "noiseStartPos", "noiseFloor"};
        static const char* subBool[] = {"lowpass"};
        for (size_t k = 0; k < sizeof(subInt) / sizeof(subInt[0]); ++k) {
            int v = 0;
            if (sub->get(subInt[k], v)) std::printf("        int %-16s = %d\n", subInt[k], v);
        }
        for (size_t k = 0; k < sizeof(subDbl) / sizeof(subDbl[0]); ++k) {
            double v = 0;
            if (sub->get(subDbl[k], v)) std::printf("        dbl %-16s = %.6f\n", subDbl[k], v);
        }
        for (size_t k = 0; k < sizeof(subBool) / sizeof(subBool[0]); ++k) {
            bool v = false;
            if (sub->get(subBool[k], v)) std::printf("        bul %-16s = %s\n", subBool[k], v ? "true" : "false");
        }
        int arr = 0;
        if (sub->getArraySize("f0", arr)) {
            std::printf("        arr f0 size      = %d\n", arr);
            for (int k = 0; k < arr && k < 32; ++k) {
                int f0 = 0; double gain = 0, bw = 0;
                sub->get("f0", f0, k);
                sub->get("gain", gain, k);
                sub->get("bw", bw, k);
                std::printf("           [%2d] f0=%-8d gain=%-10.4f bw=%.4f\n", k, f0, gain, bw);
            }
        }
    }
}

// Probe a numeric parameter's accepted range by binary-searching the point at
// which set() starts refusing (or silently clamping) the value.
static void probeDoubleRange(Speaker& sp, const char* name)
{
    double original = 0;
    if (!sp.get(name, original)) return;

    const double kProbes[] = {-1e9, -1000, -100, -10, -1, -0.5, -0.001, 0.0,
                              0.001, 0.01, 0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 100.0,
                              1000.0, 1e6, 1e9};
    std::string accepted;
    std::string clamped;
    for (size_t i = 0; i < sizeof(kProbes) / sizeof(kProbes[0]); ++i) {
        char b[64];
        if (!sp.set(name, kProbes[i])) {
            std::sprintf(b, "%g:REJ ", kProbes[i]);
            accepted += b;
            continue;
        }
        double back = 0;
        sp.get(name, back);
        if (back != kProbes[i]) {
            std::sprintf(b, "%g->%g ", kProbes[i], back);
            clamped += b;
        } else {
            std::sprintf(b, "%g ", kProbes[i]);
            accepted += b;
        }
    }
    sp.set(name, original);
    std::printf("  dbl %-22s accepted: %s\n", name, accepted.c_str());
    if (!clamped.empty()) std::printf("      %-22s CLAMPED : %s\n", "", clamped.c_str());
}

static void probeIntRange(Speaker& sp, const char* name)
{
    int original = 0;
    if (!sp.get(name, original)) return;

    const int kProbes[] = {-1000000, -1000, -1, 0, 1, 10, 50, 100, 200, 300, 500,
                           1000, 5000, 100000, 1000000};
    std::string accepted, clamped;
    for (size_t i = 0; i < sizeof(kProbes) / sizeof(kProbes[0]); ++i) {
        char b[64];
        if (!sp.set(name, kProbes[i])) {
            std::sprintf(b, "%d:REJ ", kProbes[i]);
            accepted += b;
            continue;
        }
        int back = 0;
        sp.get(name, back);
        if (back != kProbes[i]) {
            std::sprintf(b, "%d->%d ", kProbes[i], back);
            clamped += b;
        } else {
            std::sprintf(b, "%d ", kProbes[i]);
            accepted += b;
        }
    }
    sp.set(name, original);
    std::printf("  int %-22s accepted: %s\n", name, accepted.c_str());
    if (!clamped.empty()) std::printf("      %-22s CLAMPED : %s\n", "", clamped.c_str());
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
    if (argc < 3) {
        std::printf("usage: fv_probe <dataPath> <outDir> [text]\n");
        return 2;
    }
    const char* dataPath = argv[1];
    const char* outDir   = argv[2];
    const char* text = (argc > 3) ? argv[3]
        : "The quick brown fox jumps over the lazy dog. FlexVoice, version three point zero one.";

    CreateDirectoryA(outDir, NULL);

    std::printf("== FlexVoice probe ==\n");
    std::printf("header FLEX_VOICE_VERSION = %s\n", FLEX_VOICE_VERSION);

    try {
        std::printf("engine getVersion()       = %s\n", getVersion());
    } catch (...) {
        std::printf("engine getVersion()       = <threw>\n");
    }

    // What does the engine call each language?
    for (int lang = 0x0400; lang <= 0x0440; ++lang) {
        const char* n = NULL;
        try { n = getLangName(lang); } catch (...) { n = NULL; }
        if (n && *n) std::printf("  lang 0x%04x -> \"%s\"\n", lang, n);
    }
    static const char* kLangNames[] = {"English", "english", "en", "en_US", "American",
                                       "Hungarian", "hungarian", "hu", "Magyar", "German",
                                       "French", "Spanish", "Italian", "Dutch", "Polish"};
    for (size_t i = 0; i < sizeof(kLangNames) / sizeof(kLangNames[0]); ++i) {
        Language id = LNG_INVALID;
        try { id = getLangID(kLangNames[i]); } catch (...) { id = LNG_INVALID; }
        std::printf("  getLangID(\"%s\") = 0x%04x\n", kLangNames[i], id);
    }

    try {
        std::printf("\ncreating EngineFactory(\"%s\")...\n", dataPath);
        EngineFactory factory(dataPath);
        std::printf("  ok\n");

        const char* def = NULL;
        try { def = EngineFactory::getDefaultDataPath(); } catch (...) {}
        std::printf("  getDefaultDataPath() = %s\n", def ? def : "<null/threw>");

        std::printf("loading LNG_ENGLISH...\n");
        factory.loadLanguage(LNG_ENGLISH);
        std::printf("  ok\n");
        const char* langPath = factory.getLangDataPath(LNG_ENGLISH);
        std::printf("  getLangDataPath(ENGLISH) = %s\n", langPath ? langPath : "<null>");

        std::printf("trying LNG_HUNGARIAN...\n");
        try {
            factory.loadLanguage(LNG_HUNGARIAN);
            std::printf("  HUNGARIAN LOADED. path = %s\n", factory.getLangDataPath(LNG_HUNGARIAN));
        } catch (GenericException& e) {
            std::printf("  hungarian unavailable: %s / %s\n", e.what(), e.details());
        } catch (...) {
            std::printf("  hungarian unavailable: <unknown exception>\n");
        }

        // Enumerate the .tav voices sitting next to the language data.
        std::vector<std::string> voiceFiles;
        std::vector<std::string> voiceNames;
        {
            std::string base = langPath ? langPath : dataPath;
            const char* cands[] = {"default.tav", "Default.tav"};
            for (int i = 0; i < 2; ++i) {
                std::string p = base + "\\" + cands[i];
                if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    voiceFiles.push_back(p);
                    voiceNames.push_back("Default");
                    break;
                }
            }
            WIN32_FIND_DATAA fd;
            std::string pat = base + "\\Voices\\*.tav";
            HANDLE h = FindFirstFileA(pat.c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    std::string p = base + "\\Voices\\" + fd.cFileName;
                    std::string n = fd.cFileName;
                    size_t dot = n.rfind('.');
                    if (dot != std::string::npos) n = n.substr(0, dot);
                    voiceFiles.push_back(p);
                    voiceNames.push_back(n);
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
        }
        std::printf("\nfound %d voice file(s)\n", (int)voiceFiles.size());

        for (size_t v = 0; v < voiceFiles.size(); ++v) {
            std::printf("\n================ VOICE: %s ================\n  file: %s\n",
                        voiceNames[v].c_str(), voiceFiles[v].c_str());
            Speaker sp;
            try {
                sp.load(voiceFiles[v].c_str());
            } catch (GenericException& e) {
                std::printf("  load failed: %s / %s\n", e.what(), e.details());
                continue;
            }
            dumpAttribute(sp, voiceNames[v].c_str());

            if (v == 0) {
                std::printf("\n--- numeric range probe (Speaker) ---\n");
                for (size_t i = 0; i < sizeof(kDoubleParams) / sizeof(kDoubleParams[0]); ++i)
                    probeDoubleRange(sp, kDoubleParams[i]);
                for (size_t i = 0; i < sizeof(kIntParams) / sizeof(kIntParams[0]); ++i)
                    probeIntRange(sp, kIntParams[i]);
                Speaker fresh;
                fresh.load(voiceFiles[v].c_str());
                sp = fresh;
            }

            factory.addSpeaker(LNG_ENGLISH, sp, SCS_LOAD_IMMEDIATELY_DO_NOT_DELETE);

            WaveOutputFormat fmt(16000, 16, WaveOutputFormat::WC_PCM_SIGNED);
            WaveFileOutputSite out(fmt);
            std::string wav = std::string(outDir) + "\\" + voiceNames[v] + ".wav";
            out.open(wav.c_str(), WaveFileOutputSite::FF_WAV);

            DWORD t0 = GetTickCount();
            {
                std::auto_ptr<Engine> eng = factory.createEngine(&out, sp, LNG_ENGLISH);
                DWORD t1 = GetTickCount();
                std::printf("  createEngine took %lu ms\n", (unsigned long)(t1 - t0));

                std::printf("\n--- engine attribute() ---\n");
                dumpAttribute(eng->attribute(), "Engine");

                DWORD t2 = GetTickCount();
                eng->speakRequest(text, 1);
                eng->wait();
                DWORD t3 = GetTickCount();
                std::printf("  synthesis took %lu ms\n", (unsigned long)(t3 - t2));
            }
            out.close();
            std::printf("  wrote %s\n", wav.c_str());
        }

        std::printf("\n== probe complete ==\n");
    } catch (GenericException& e) {
        std::printf("\n!! FlexVoice exception: %s : %s\n", e.what(), e.details());
        return 1;
    } catch (...) {
        std::printf("\n!! unknown exception\n");
        return 1;
    }
    return 0;
}
