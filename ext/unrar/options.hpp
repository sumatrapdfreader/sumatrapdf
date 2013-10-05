#ifndef _RAR_OPTIONS_
#define _RAR_OPTIONS_

#define DEFAULT_RECOVERY     -3

#define DEFAULT_RECVOLUMES  -10

#define VOLSIZE_AUTO   INT64NDF // Automatically detect the volume size.

enum PATH_EXCL_MODE {
  EXCL_UNCHANGED=0,    // Process paths as is (default).
  EXCL_SKIPWHOLEPATH,  // -ep  (exclude the path completely)
  EXCL_BASEPATH,       // -ep1 (exclude the base part of path)
  EXCL_SAVEFULLPATH,   // -ep2 (the full path without the disk letter)
  EXCL_ABSPATH,        // -ep3 (the full path with the disk letter)

  EXCL_SKIPABSPATH     // Works as EXCL_BASEPATH for fully qualified paths
                       // and as EXCL_UNCHANGED for relative paths.
                       // Used by WinRAR GUI only.
};

enum {SOLID_NONE=0,SOLID_NORMAL=1,SOLID_COUNT=2,SOLID_FILEEXT=4,
      SOLID_VOLUME_DEPENDENT=8,SOLID_VOLUME_INDEPENDENT=16};

enum {ARCTIME_NONE=0,ARCTIME_KEEP,ARCTIME_LATEST};

enum EXTTIME_MODE {
  EXTTIME_NONE=0,EXTTIME_1S,EXTTIME_HIGH1,EXTTIME_HIGH2,EXTTIME_HIGH3
};

enum {NAMES_ORIGINALCASE=0,NAMES_UPPERCASE,NAMES_LOWERCASE};

enum MESSAGE_TYPE {MSG_STDOUT=0,MSG_STDERR,MSG_ERRONLY,MSG_NULL};

enum RECURSE_MODE 
{
  RECURSE_NONE=0,    // no recurse switches
  RECURSE_DISABLE,   // switch -r-
  RECURSE_ALWAYS,    // switch -r
  RECURSE_WILDCARDS, // switch -r0
};

enum OVERWRITE_MODE 
{
  OVERWRITE_DEFAULT=0, // Ask when extracting, silently overwrite when archiving.
  OVERWRITE_ALL,
  OVERWRITE_NONE,
  OVERWRITE_AUTORENAME,
  OVERWRITE_FORCE_ASK
};


enum QOPEN_MODE { QOPEN_NONE, QOPEN_AUTO, QOPEN_ALWAYS };

enum RAR_CHARSET { RCH_DEFAULT=0,RCH_ANSI,RCH_OEM,RCH_UNICODE };

#define     MAX_FILTER_TYPES           16
enum FilterState {FILTER_DEFAULT=0,FILTER_AUTO,FILTER_FORCE,FILTER_DISABLE};


enum SAVECOPY_MODE {
  SAVECOPY_NONE=0, SAVECOPY_SILENT, SAVECOPY_LIST, SAVECOPY_LISTEXIT,
  SAVECOPY_DUPLISTEXIT
};

struct FilterMode
{
  FilterState State;
  int Param1;
  int Param2;
};

#define MAX_GENERATE_MASK  128


class RAROptions
{
  public:
    RAROptions();
    ~RAROptions();
    void Init();

    uint ExclFileAttr;
    uint InclFileAttr;
    bool InclAttrSet;
    size_t WinSize;
    wchar TempPath[NM];
#ifdef USE_QOPEN
    wchar SFXModule[NM];
    QOPEN_MODE QOpenMode;
#endif
    bool ConfigDisabled; // Switch -cfg-.
    wchar ExtrPath[NM];
    wchar CommentFile[NM];
    RAR_CHARSET CommentCharset;
    RAR_CHARSET FilelistCharset;
    RAR_CHARSET ErrlogCharset;

    wchar ArcPath[NM];
    SecPassword Password;
    bool EncryptHeaders;
    wchar LogName[NM];
    MESSAGE_TYPE MsgStream;
    bool Sound;
    OVERWRITE_MODE Overwrite;
    int Method;
    HASH_TYPE HashType;
    int Recovery;
    int RecVolNumber;
    bool DisablePercentage;
    bool DisableCopyright;
    bool DisableDone;
    int Solid;
    int SolidCount;
    bool ClearArc;
    bool AddArcOnly;
    bool DisableComment;
    bool FreshFiles;
    bool UpdateFiles;
    PATH_EXCL_MODE ExclPath;
    RECURSE_MODE Recurse;
    int64 VolSize;
    Array<int64> NextVolSizes;
    uint CurVolNum;
    bool AllYes;
    bool DisableSortSolid;
    int ArcTime;
    int ConvertNames;
    bool ProcessOwners;
    bool SaveSymLinks;
    bool SaveHardLinks;
    int Priority;
    int SleepTime;
    bool KeepBroken;
    bool OpenShared;
    bool DeleteFiles;


#ifndef SFX_MODULE
    bool GenerateArcName;
    wchar GenerateMask[MAX_GENERATE_MASK];
#endif
    bool SyncFiles;
    bool ProcessEA;
    bool SaveStreams;
    bool SetCompressedAttr;
    bool IgnoreGeneralAttr;
    RarTime FileTimeBefore;
    RarTime FileTimeAfter;
    int64 FileSizeLess;
    int64 FileSizeMore;
    bool Lock;
    bool Test;
    bool VolumePause;
    FilterMode FilterModes[MAX_FILTER_TYPES];
    wchar EmailTo[NM];
    uint VersionControl;
    bool AppendArcNameToPath;
    bool Shutdown;
    EXTTIME_MODE xmtime;
    EXTTIME_MODE xctime;
    EXTTIME_MODE xatime;
    wchar CompressStdin[NM];

    uint Threads; // We use it to init hash even if RAR_SMP is not defined.






#ifdef RARDLL
    wchar DllDestName[NM];
    int DllOpMode;
    int DllError;
    LPARAM UserData;
    UNRARCALLBACK Callback;
    CHANGEVOLPROC ChangeVolProc;
    PROCESSDATAPROC ProcessDataProc;
#endif
};
#endif
