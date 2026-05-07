void LogInit(Str logFilePath);
void LogDestroy();
void logStr(Str s);
#define logf(fmt, ...) logStr(StrFmtTemp(fmt, __VA_ARGS__))
void logConsole(const char* fmt, ...);
void WaitForConsoleClose();
void SendEnterIfLoggedToConsole();
