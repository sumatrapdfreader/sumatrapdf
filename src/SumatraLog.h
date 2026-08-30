// TTS / read-aloud debug. Flip 0/1. When 0, dbgtts() and DBG_TTS() vanish
// (arguments and block bodies are not compiled).
//   dbgtts("pos=%d\n", pos);
//   DBG_TTS({ static int last; ... });
#if 0
#define dbgtts(...) logf("tts: " __VA_ARGS__)
#define DBG_TTS(...) \
    do {             \
        __VA_ARGS__; \
    } while (0)
#else
#define dbgtts(...)
#define DBG_TTS(...)
#endif

extern Arena* gLogAllocator;
extern str::Builder* gLogBuf;
extern bool gLogToConsole;
extern bool gLogToDebugger;
extern bool gReducedLogging;
extern bool gLogToPipe;
extern Str gLogAppName;
extern Str gLogFilePath;
void StartLogToFile(Str path, bool removeIfExists);
bool WriteCurrentLogToFile(Str path);
void DestroyLogging();
void LogParentProcessChain();