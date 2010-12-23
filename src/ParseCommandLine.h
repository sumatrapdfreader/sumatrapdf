#ifndef _PARSE_COMMAND_LINE_H__
#define _PARSE_COMMAND_LINE_H__

class CommandLineInfo {
public:
    VStrList    fileNames;
    bool        makeDefault;
    bool        exitOnPrint;
    bool        printDialog;
    TCHAR *     printerName;
    int         bgColor;
    TCHAR *     inverseSearchCmdLine;
    int         fwdsearchOffset;
    int         fwdsearchWidth;
    int         fwdsearchColor;
    BOOL        fwdsearchPermanent;
    BOOL        escToExit;
    bool        reuseInstance;
    char *      lang;
    TCHAR *     destName;
    int         pageNumber;
    bool        restrictedUse;
    TCHAR *     newWindowTitle;
    BOOL        invertColors;
    bool        enterPresentation;
    HWND        hwndPluginParent;
    // Delete the files which were passed into the program by command line.
    bool        deleteFilesOnClose;

    CommandLineInfo() : makeDefault(false), exitOnPrint(false), printDialog(false),
        printerName(NULL), bgColor(-1), inverseSearchCmdLine(NULL),
        fwdsearchOffset(-1), fwdsearchWidth(-1), fwdsearchColor(-1),
        fwdsearchPermanent(FALSE), escToExit(FALSE),
        reuseInstance(false), lang(NULL), destName(NULL), pageNumber(-1),
        restrictedUse(false), newWindowTitle(NULL), invertColors(FALSE),
        enterPresentation(false), hwndPluginParent(NULL),
        deleteFilesOnClose(false)
    {}

   ~CommandLineInfo() {
        free(printerName);
        free(inverseSearchCmdLine);
        free(lang);
        free(destName);
        free(newWindowTitle);
    }

    void SetPrinterName(TCHAR *s) {
        free(printerName);
        printerName = s;
    }
};

void ParseCommandLine(CommandLineInfo& i, TCHAR *cmdLine);

#endif
