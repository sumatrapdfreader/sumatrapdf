void LogInit(Str logFilePath);
void LogDestroy();
void logStr(Str s);
#define logf(fmt, ...) logStr(StrFmtTemp(fmt, __VA_ARGS__))
void LogConsole(Str s);
void WaitForConsoleClose();
void SendEnterIfLoggedToConsole();
