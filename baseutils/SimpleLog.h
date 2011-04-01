#ifndef SimpleLog_h
#define SimpleLog_h

#include "Vec.h"

namespace Log {

class Logger {
public:
	virtual ~Logger();
	virtual void Log(char *s, bool takeOwnership=false) = 0;
};

class FileLogger : Logger {

	FILE *	fp;
	TCHAR *	fileName;
	bool 	failedToOpen;

	bool OpenLogFile()
	{
		if (failedToOpen)
			return false;
		fp = _tfopen(fileName, _T("ab"));
		if (!fp)
			failedToOpen = true;
		return !failedToOpen;
	}

public:
	FileLogger(TCHAR *fileName)
	{
		this->fileName = Str::Dup(fileName);
		fp = NULL;
		failedToOpen = false;
	}

	virtual ~FileLogger()
	{
		if (fp)
			fclose(fp);
		free(fileName);
	}

	virtual void Log(char *s, bool takeOwnership=false)
	{
		if (!OpenLogFile())
			return;
		fwrite(s, Str::Len(s), 1, fp);
		fflush(fp);
		if (takeOwnership)
			free(s);
	}
};

class MemoryLogger : Logger {
	Vec<char *> lines;
public:
	MemoryLogger()
	{
	}

	virtual ~MemoryLogger()
	{
		FreeVecMembers(lines);
	}

	virtual void Log(char* s, bool takeOwnership=false)
	{
		char *tmp = s;
		if (!takeOwnership)
			tmp = Str::Dup(s);
		if (tmp)
			lines.Append(tmp);
	}

	// TODO: some way to get the lines as text ?
};

void Initialize();
void Destroy();

void AddLogger(Logger *);
void RemoveLogger(Logger *);

void Log(char *s, bool takeOwnership=false);
void LogFmt(char *fmt, ...);

} // namespace Log

#endif
