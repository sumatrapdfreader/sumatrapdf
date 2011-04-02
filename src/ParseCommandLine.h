/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ParseCommandLine_h
#define ParseCommandLine_h

#include "DisplayState.h"
#include "Vec.h"

class CommandLineInfo {
public:
    StrVec      fileNames;
    // filesToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (NULL if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    StrVec      filesToBenchmark;
    bool        makeDefault;
    bool        exitOnPrint;
    bool        printDialog;
    TCHAR *     printerName;
    int         bgColor;
    TCHAR *     inverseSearchCmdLine;
    int         fwdsearchOffset;
    int         fwdsearchWidth;
    int         fwdsearchColor;
    bool        fwdsearchPermanent;
    bool        escToExit;
    bool        reuseInstance;
    char *      lang;
    TCHAR *     destName;
    int         pageNumber;
    bool        restrictedUse;
    bool        invertColors;
    bool        enterPresentation;
    bool        enterFullscreen;
    DisplayMode startView;
    float       startZoom;
    PointI      startScroll;
    bool        showConsole;
    HWND        hwndPluginParent;
    bool        exitImmediately;
    bool        silent;

    CommandLineInfo() : makeDefault(false), exitOnPrint(false), printDialog(false),
        printerName(NULL), bgColor(-1), inverseSearchCmdLine(NULL),
        fwdsearchOffset(-1), fwdsearchWidth(-1), fwdsearchColor(-1),
        fwdsearchPermanent(FALSE), escToExit(FALSE),
        reuseInstance(false), lang(NULL), destName(NULL), pageNumber(-1),
        restrictedUse(false), invertColors(FALSE),
        enterPresentation(false), enterFullscreen(false), hwndPluginParent(NULL),
        startView(DM_AUTOMATIC), startZoom(INVALID_ZOOM), startScroll(PointI(-1, -1)),
        showConsole(false), exitImmediately(false), silent(false)
    { }

    ~CommandLineInfo() {
        free(printerName);
        free(inverseSearchCmdLine);
        free(lang);
        free(destName);
    }

    void ParseCommandLine(TCHAR *cmdLine);

protected:
    void SetPrinterName(TCHAR *s) {
        free(printerName);
        printerName = Str::Dup(s);
    }

    void SetInverseSearchCmdLine(TCHAR *s) {
        free(inverseSearchCmdLine);
        inverseSearchCmdLine = Str::Dup(s);
    }

    void SetLang(TCHAR *s) {
        free(lang);
        lang = Str::Conv::ToAnsi(s);
    }
    
    void SetDestName(TCHAR *s) {
        free(destName);
        destName = Str::Dup(s);
    }
};

class ParsedCmdLineArguments : public StrVec {
public:
    /* 'cmdLine' contains one or several arguments can be:
        - escaped, in which case it starts with '"', ends with '"' and
          each '"' that is part of the name is escaped with '\\'
        - unescaped, in which case it start with != '"' and ends with ' ' or '\0' */
    ParsedCmdLineArguments(const TCHAR *cmdLine)
    {
        while (cmdLine) {
            // skip whitespace
            while (_istspace(*cmdLine))
                cmdLine++;
            if ('"' == *cmdLine)
                cmdLine = ParseQuoted(cmdLine);
            else if ('\0' != *cmdLine)
                cmdLine = ParseUnquoted(cmdLine);
            else
                cmdLine = NULL;
        }
    }

private:
    /* returns the next character in '*txt' that isn't a backslash */
    static TCHAR SkipBackslashs(const TCHAR *txt)
    {
        assert(txt && '\\' == *txt);
        while ('\\' == *++txt);
        return *txt;
    }

    /* appends the next quoted argument and returns the position after it */
    const TCHAR *ParseQuoted(const TCHAR *arg)
    {
        assert(arg && '"' == *arg);
        arg++;

        Str::Str<TCHAR> txt(Str::Len(arg) / 2);
        const TCHAR *next;
        for (next = arg; *next && *next != '"'; next++) {
            // skip escaped quotation marks according to
            // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
            if ('\\' == *next && '"' == SkipBackslashs(next))
                next++;
            txt.Append(*next);
        }
        this->Append(txt.StealData());

        if ('"' == *next)
            next++;
        return next;
    }

    /* appends the next unquoted argument and returns the position after it */
    const TCHAR *ParseUnquoted(const TCHAR *arg)
    {
        assert(arg && *arg && '"' != *arg && !_istspace(*arg));

        const TCHAR *next;
        // contrary to http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
        // we don't treat quotation marks or backslashes in non-quoted
        // arguments in any special way
        for (next = arg; *next && !_istspace(*next); next++);
        this->Append(Str::DupN(arg, next - arg));

        return next;
    }
};

// in plugin mode, the window's frame isn't drawn and closing and
// fullscreen are disabled, so that SumatraPDF can be displayed
// embedded (e.g. in a web browser)
extern bool gPluginMode;

class WindowInfo;
void MakePluginWindow(WindowInfo *win, HWND hwndParent);

#endif
