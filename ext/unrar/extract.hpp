#ifndef _RAR_EXTRACT_
#define _RAR_EXTRACT_

enum EXTRACT_ARC_CODE {EXTRACT_ARC_NEXT,EXTRACT_ARC_REPEAT};

class CmdExtract
{
  private:
    EXTRACT_ARC_CODE ExtractArchive(CommandData *Cmd);
    bool ExtractFileCopy(File &New,wchar *ArcName,wchar *NameNew,wchar *NameExisting,size_t NameExistingSize);
    void ExtrPrepareName(CommandData *Cmd,Archive &Arc,const wchar *ArcFileName,wchar *DestName,size_t DestSize);
#ifdef RARDLL
    bool ExtrDllGetPassword(CommandData *Cmd);
#else
    bool ExtrGetPassword(CommandData *Cmd,Archive &Arc,const wchar *ArcFileName);
#endif
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    void ConvertDosPassword(Archive &Arc,SecPassword &DestPwd);
#endif
    void ExtrCreateDir(CommandData *Cmd,Archive &Arc,const wchar *ArcFileName);
    bool ExtrCreateFile(CommandData *Cmd,Archive &Arc,File &CurFile);
    bool CheckUnpVer(Archive &Arc,const wchar *ArcFileName);

    RarTime StartTime; // time when extraction started

    ComprDataIO DataIO;
    Unpack *Unp;
    unsigned long TotalFileCount;

    unsigned long FileCount;
    unsigned long MatchedArgs;
    bool FirstFile;
    bool AllMatchesExact;
    bool ReconstructDone;

    // If any non-zero solid file was successfully unpacked before current.
    // If true and if current encrypted file is broken, obviously
    // the password is correct and we can report broken CRC without
    // any wrong password hints.
    bool AnySolidDataUnpackedWell;

    wchar ArcName[NM];

    SecPassword Password;
    bool PasswordAll;
    bool PrevExtracted;
    wchar DestFileName[NM];
    bool PasswordCancelled;
  public:
    CmdExtract(CommandData *Cmd);
    ~CmdExtract();
    void DoExtract(CommandData *Cmd);
    void ExtractArchiveInit(CommandData *Cmd,Archive &Arc);
    bool ExtractCurrentFile(CommandData *Cmd,Archive &Arc,size_t HeaderSize,
                            bool &Repeat);
    static void UnstoreFile(ComprDataIO &DataIO,int64 DestUnpSize);
};

#endif
