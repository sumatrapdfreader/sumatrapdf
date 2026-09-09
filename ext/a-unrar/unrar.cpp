#define INCLUDEGLOBAL

#ifdef _MSC_VER
#pragma hdrstop
#endif

#ifndef _RAR_RARCOMMON_
#define _RAR_RARCOMMON_

#ifndef _RAR_RAROS_
#define _RAR_RAROS_

#if defined(__WIN32__) || defined(_WIN32)
  #define _WIN_ALL
  #ifdef _M_X64
    #define _WIN_64
  #else
    #define _WIN_32
  #endif
#endif

#if defined(ANDROID) || defined(__ANDROID__)
  #define _UNIX
  #define _ANDROID
#endif

#ifdef __APPLE__
  #define _UNIX
  #define _APPLE
#endif

#if !defined(_WIN_ALL) && !defined(_UNIX)
  #define _UNIX
#endif

#endif

#ifndef _RAR_TYPES_
#define _RAR_TYPES_

#include <stdint.h>

typedef uint8_t          byte;
typedef uint16_t         ushort;
typedef unsigned int     uint;
typedef uint32_t         uint32;
typedef int32_t          int32;
typedef uint64_t         uint64;
typedef int64_t          int64;
typedef wchar_t          wchar;

#define GET_SHORT16(x) (sizeof(ushort)==2 ? (ushort)(x):((x)&0xffff))

#define INT32TO64(high,low) ((((uint64)(high))<<32)+((uint64)low))

#define MAX_INT64 int64(INT32TO64(0x7fffffff,0xffffffff))

#define INT64NDF INT32TO64(0x7fffffff,0x7fffffff)

#endif

#ifndef _RAR_OS_
#define _RAR_OS_

#define FALSE 0
#define TRUE  1

#if defined(RARDLL) && !defined(SILENT)
#define SILENT
#endif

#include <new>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <algorithm>

#ifdef _WIN_ALL

#define LITTLE_ENDIAN

#ifndef STRICT
#define STRICT 1
#endif

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#define WINVER _WIN32_WINNT_WINXP
#define _WIN32_WINNT _WIN32_WINNT_WINXP

#if !defined(ZIPSFX)
#define RAR_SMP
#endif

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <prsht.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <powrprof.h>
#pragma comment(lib, "PowrProf.lib")
#include <shellapi.h>
#include <shlobj.h>
#include <winioctl.h>
#include <wincrypt.h>
#include <wchar.h>
#include <wctype.h>
#include <sddl.h>
#include <ntsecapi.h>

#include <comdef.h>
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

#include <sys/types.h>
#include <sys/stat.h>
#include <dos.h>
#include <direct.h>
#include <intrin.h>

#if defined(_M_IX86) || defined(_M_X64)
  #define USE_SSE
  #define SSE_ALIGNMENT 16
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>
#include <io.h>
#include <time.h>
#include <signal.h>

#define SAVE_LINKS

#define ENABLE_ACCESS

#define DefConfigName  L"rar.ini"
#define DefLogName     L"rar.log"

#define SPATHDIVIDER L"\\"
#define CPATHDIVIDER L'\\'
#define MASKALL      L"*"

#define READBINARY   "rb"
#define READTEXT     "rt"
#define UPDATEBINARY "r+b"
#define CREATEBINARY "w+b"
#define WRITEBINARY  "wb"
#define APPENDTEXT   "at"

#define _stdfunction __cdecl
#define _forceinline __forceinline

#endif

#ifdef _UNIX

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#if defined(__QNXNTO__)
  #include <sys/param.h>
#endif
#ifdef _APPLE
  #include <sys/sysctl.h>
#endif
#ifndef SFX_MODULE
    #include <sys/statvfs.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <utime.h>
#include <locale.h>

#ifdef __GNUC__
  #if defined(__i386__) || defined(__x86_64__)
    #include <x86intrin.h>

    #define USE_SSE
    #define SSE_ALIGNMENT 16
  #endif
#endif

#if defined(__aarch64__) && (defined(__ARM_FEATURE_CRYPTO) || defined(__ARM_FEATURE_CRC32))
#include <arm_neon.h>
#ifndef _APPLE
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif
#ifdef __ARM_FEATURE_CRYPTO
#define USE_NEON_AES
#endif
#ifdef __ARM_FEATURE_CRC32
#define USE_NEON_CRC32
#endif
#endif

#ifdef  S_IFLNK
#define SAVE_LINKS
#endif

#if defined(__linux) || defined(__FreeBSD__)
#include <sys/time.h>
#define USE_LUTIMES
#endif

#define ENABLE_ACCESS

#define DefConfigName  L".rarrc"
#define DefLogName     L".rarlog"

#define SPATHDIVIDER L"/"
#define CPATHDIVIDER L'/'
#define MASKALL      L"*"

#define READBINARY   "r"
#define READTEXT     "r"
#define UPDATEBINARY "r+"
#define CREATEBINARY "w+"
#define WRITEBINARY  "w"
#define APPENDTEXT   "a"

#define _stdfunction
#define _forceinline inline

#ifdef _APPLE
  #if defined(__BIG_ENDIAN__) && !defined(BIG_ENDIAN)
    #define BIG_ENDIAN
    #undef LITTLE_ENDIAN
  #endif
  #if defined(__i386__) && !defined(LITTLE_ENDIAN)
    #define LITTLE_ENDIAN
    #undef BIG_ENDIAN
  #endif
#endif

#if defined(__sparc) || defined(sparc) || defined(__hpux)
  #ifndef BIG_ENDIAN
     #define BIG_ENDIAN
  #endif
#endif

#ifdef __VMS
# define LITTLE_ENDIAN
#endif

#if _POSIX_C_SOURCE >= 200809L || defined(__APPLE__) && defined(__arm64__)
  #define UNIX_TIME_NS
#endif

#endif

  typedef const wchar* MSGID;

#ifndef SSE_ALIGNMENT
  #define SSE_ALIGNMENT 1
#endif

#if defined(_LITTLE_ENDIAN) && !defined(LITTLE_ENDIAN)
  #define LITTLE_ENDIAN
#endif
#if defined(_BIG_ENDIAN) && !defined(BIG_ENDIAN)
  #define BIG_ENDIAN
#endif

#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
  #if defined(__i386) || defined(i386) || defined(__i386__) || defined(__x86_64)
    #define LITTLE_ENDIAN
  #elif defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN || defined(__LITTLE_ENDIAN__)
    #define LITTLE_ENDIAN
  #elif defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN || defined(__BIG_ENDIAN__)
    #define BIG_ENDIAN
  #else
    #error "Neither LITTLE_ENDIAN nor BIG_ENDIAN are defined. Define one of them."
  #endif
#endif

#if defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
  #if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
    #undef LITTLE_ENDIAN
  #elif defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN
    #undef BIG_ENDIAN
  #else
    #error "Both LITTLE_ENDIAN and BIG_ENDIAN are defined. Undef one of them."
  #endif
#endif

#if !defined(BIG_ENDIAN) && defined(_WIN_ALL) || defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)

#define ALLOW_MISALIGNED
#endif

#endif

#ifdef RARDLL
#ifndef _UNRAR_DLL_
#define _UNRAR_DLL_

#pragma pack(push, 1)

#define ERAR_SUCCESS             0
#define ERAR_END_ARCHIVE        10
#define ERAR_NO_MEMORY          11
#define ERAR_BAD_DATA           12
#define ERAR_BAD_ARCHIVE        13
#define ERAR_UNKNOWN_FORMAT     14
#define ERAR_EOPEN              15
#define ERAR_ECREATE            16
#define ERAR_ECLOSE             17
#define ERAR_EREAD              18
#define ERAR_EWRITE             19
#define ERAR_SMALL_BUF          20
#define ERAR_UNKNOWN            21
#define ERAR_MISSING_PASSWORD   22
#define ERAR_EREFERENCE         23
#define ERAR_BAD_PASSWORD       24
#define ERAR_LARGE_DICT         25

#define RAR_OM_LIST              0
#define RAR_OM_EXTRACT           1
#define RAR_OM_LIST_INCSPLIT     2

#define RAR_SKIP              0
#define RAR_TEST              1
#define RAR_EXTRACT           2

#define RAR_VOL_ASK           0
#define RAR_VOL_NOTIFY        1

#define RAR_DLL_VERSION       9

#define RAR_HASH_NONE         0
#define RAR_HASH_CRC32        1
#define RAR_HASH_BLAKE2       2

#ifdef _UNIX
#define CALLBACK
#define PASCAL
#define LONG long
#define HANDLE void *
#define LPARAM long
#define UINT unsigned int
#endif

#define RHDF_SPLITBEFORE 0x01
#define RHDF_SPLITAFTER  0x02
#define RHDF_ENCRYPTED   0x04
#define RHDF_SOLID       0x10
#define RHDF_DIRECTORY   0x20

struct RARHeaderData
{
  char         ArcName[260];
  char         FileName[260];
  unsigned int Flags;
  unsigned int PackSize;
  unsigned int UnpSize;
  unsigned int HostOS;
  unsigned int FileCRC;
  unsigned int FileTime;
  unsigned int UnpVer;
  unsigned int Method;
  unsigned int FileAttr;
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
};

struct RARHeaderDataEx
{
  char         ArcName[1024];
  wchar_t      ArcNameW[1024];
  char         FileName[1024];
  wchar_t      FileNameW[1024];
  unsigned int Flags;
  unsigned int PackSize;
  unsigned int PackSizeHigh;
  unsigned int UnpSize;
  unsigned int UnpSizeHigh;
  unsigned int HostOS;
  unsigned int FileCRC;
  unsigned int FileTime;
  unsigned int UnpVer;
  unsigned int Method;
  unsigned int FileAttr;
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
  unsigned int DictSize;
  unsigned int HashType;
  char         Hash[32];
  unsigned int RedirType;
  wchar_t      *RedirName;
  unsigned int RedirNameSize;
  unsigned int DirTarget;
  unsigned int MtimeLow;
  unsigned int MtimeHigh;
  unsigned int CtimeLow;
  unsigned int CtimeHigh;
  unsigned int AtimeLow;
  unsigned int AtimeHigh;
  wchar_t      *ArcNameEx;
  unsigned int ArcNameExSize;
  wchar_t      *FileNameEx;
  unsigned int FileNameExSize;
  unsigned int Reserved[982];
};

struct RAROpenArchiveData
{
  char         *ArcName;
  unsigned int OpenMode;
  unsigned int OpenResult;
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
};

typedef int (CALLBACK *UNRARCALLBACK)(UINT msg,LPARAM UserData,LPARAM P1,LPARAM P2);

#define ROADF_VOLUME       0x0001
#define ROADF_COMMENT      0x0002
#define ROADF_LOCK         0x0004
#define ROADF_SOLID        0x0008
#define ROADF_NEWNUMBERING 0x0010
#define ROADF_SIGNED       0x0020
#define ROADF_RECOVERY     0x0040
#define ROADF_ENCHEADERS   0x0080
#define ROADF_FIRSTVOLUME  0x0100

#define ROADOF_KEEPBROKEN  0x0001

struct RAROpenArchiveDataEx
{
  char         *ArcName;
  wchar_t      *ArcNameW;
  unsigned int  OpenMode;
  unsigned int  OpenResult;
  char         *CmtBuf;
  unsigned int  CmtBufSize;
  unsigned int  CmtSize;
  unsigned int  CmtState;
  unsigned int  Flags;
  UNRARCALLBACK Callback;
  LPARAM        UserData;
  unsigned int  OpFlags;
  wchar_t      *CmtBufW;
  wchar_t      *MarkOfTheWeb;
  unsigned int  Reserved[23];
};

enum UNRARCALLBACK_MESSAGES {
  UCM_CHANGEVOLUME,UCM_PROCESSDATA,UCM_NEEDPASSWORD,UCM_CHANGEVOLUMEW,
  UCM_NEEDPASSWORDW,UCM_LARGEDICT
};

typedef int (PASCAL *CHANGEVOLPROC)(char *ArcName,int Mode);
typedef int (PASCAL *PROCESSDATAPROC)(unsigned char *Addr,int Size);

#ifdef __cplusplus
extern "C" {
#endif

HANDLE PASCAL RAROpenArchive(struct RAROpenArchiveData *ArchiveData);
HANDLE PASCAL RAROpenArchiveEx(struct RAROpenArchiveDataEx *ArchiveData);
int    PASCAL RARCloseArchive(HANDLE hArcData);
int    PASCAL RARReadHeader(HANDLE hArcData,struct RARHeaderData *HeaderData);
int    PASCAL RARReadHeaderEx(HANDLE hArcData,struct RARHeaderDataEx *HeaderData);
int    PASCAL RARProcessFile(HANDLE hArcData,int Operation,char *DestPath,char *DestName);
int    PASCAL RARProcessFileW(HANDLE hArcData,int Operation,wchar_t *DestPath,wchar_t *DestName);
void   PASCAL RARSetCallback(HANDLE hArcData,UNRARCALLBACK Callback,LPARAM UserData);
void   PASCAL RARSetChangeVolProc(HANDLE hArcData,CHANGEVOLPROC ChangeVolProc);
void   PASCAL RARSetProcessDataProc(HANDLE hArcData,PROCESSDATAPROC ProcessDataProc);
void   PASCAL RARSetPassword(HANDLE hArcData,char *Password);
int    PASCAL RARGetDllVersion();

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif

#endif

#define RARVER_MAJOR     7
#define RARVER_MINOR    11
#define RARVER_BETA      0
#define RARVER_DAY      20
#define RARVER_MONTH     3
#define RARVER_YEAR   2025

#ifndef _RAR_DEFS_
#define _RAR_DEFS_

#define  Min(x,y) (((x)<(y)) ? (x):(y))
#define  Max(x,y) (((x)>(y)) ? (x):(y))

#define  Abs(x) (((x)<0) ? -(x):(x))

#define  ASIZE(x) (sizeof(x)/sizeof(x[0]))

#define  MAXPASSWORD       512
#define  MAXPASSWORD_RAR   128

#define  MAXPATHSIZE       0x10000

#define  MAXSFXSIZE        0x400000

#define  MAXCMTSIZE        0x40000

#ifdef _WIN_32
#define  DefSFXName        L"default32.sfx"
#else
#define  DefSFXName        L"default.sfx"
#endif
#define  DefSortListName   L"rarfiles.lst"

#define PACK_MAX_DICT      0x1000000000ULL

#define UNPACK_MAX_DICT    0x1000000000ULL

#ifndef SFX_MODULE
#define USE_QOPEN
#endif

#define ALIGN_VALUE(v,a) (size_t(v) + ( (~size_t(v) + 1) & (a - 1) ) )

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
#define PROPAGATE_MOTW
#endif

#endif

#ifndef _RAR_LANG_
#define _RAR_LANG_

  #ifdef USE_RC
    #include "rarres.hpp"
  #else
#define   MYesNo             L"_Yes_No"
#define   MYesNoAll          L"_Yes_No_All"
#define   MYesNoAllQ         L"_Yes_No_All_nEver_Quit"
#define   MYesNoAllRenQ      L"_Yes_No_All_nEver_Rename_Quit"
#define   MContinueQuit      L"_Continue_Quit"
#define   MRetryAbort        L"_Retry_Abort"
#define   MIgnoreAllRetryQuit L"_Ignore_iGnore all_Retry_Quit"
#define   MCopyright         L"\nRAR %s   Copyright (c) 1993-%d Alexander Roshal   %d %s %d"
#define   MRegTo             L"\nRegistered to %s\n"
#define   MShare             L"\nTrial version             Type 'rar -?' for help\n"
#define   MRegKeyWarning     L"\nAvailable license key is valid only for %s\n"
#define   MUCopyright        L"\nUNRAR %s freeware      Copyright (c) 1993-%d Alexander Roshal\n"
#define   MBeta              L"beta"
#define   Mx86               L"x86"
#define   Mx64               L"x64"
#define   MMonthJan          L"Jan"
#define   MMonthFeb          L"Feb"
#define   MMonthMar          L"Mar"
#define   MMonthApr          L"Apr"
#define   MMonthMay          L"May"
#define   MMonthJun          L"Jun"
#define   MMonthJul          L"Jul"
#define   MMonthAug          L"Aug"
#define   MMonthSep          L"Sep"
#define   MMonthOct          L"Oct"
#define   MMonthNov          L"Nov"
#define   MMonthDec          L"Dec"
#define   MRARTitle1         L"\nUsage:     rar <command> -<switch 1> -<switch N> <archive> <files...>"
#define   MUNRARTitle1       L"\nUsage:     unrar <command> -<switch 1> -<switch N> <archive> <files...>"
#define   MRARTitle2         L"\n               <@listfiles...> <path_to_extract\\>"
#define   MFwrSlTitle2       L"\n               <@listfiles...> <path_to_extract/>"
#define   MCHelpCmd          L"\n\n<Commands>"
#define   MCHelpCmdA         L"\n  a             Add files to archive"
#define   MCHelpCmdC         L"\n  c             Add archive comment"
#define   MCHelpCmdCH        L"\n  ch            Change archive parameters"
#define   MCHelpCmdCW        L"\n  cw            Write archive comment to file"
#define   MCHelpCmdD         L"\n  d             Delete files from archive"
#define   MCHelpCmdE         L"\n  e             Extract files without archived paths"
#define   MCHelpCmdF         L"\n  f             Freshen files in archive"
#define   MCHelpCmdI         L"\n  i[par]=<str>  Find string in archives"
#define   MCHelpCmdK         L"\n  k             Lock archive"
#define   MCHelpCmdL         L"\n  l[t[a],b]     List archive contents [technical[all], bare]"
#define   MCHelpCmdM         L"\n  m[f]          Move to archive [files only]"
#define   MCHelpCmdP         L"\n  p             Print file to stdout"
#define   MCHelpCmdR         L"\n  r             Repair archive"
#define   MCHelpCmdRC        L"\n  rc            Reconstruct missing volumes"
#define   MCHelpCmdRN        L"\n  rn            Rename archived files"
#define   MCHelpCmdRR        L"\n  rr[N]         Add data recovery record"
#define   MCHelpCmdRV        L"\n  rv[N]         Create recovery volumes"
#define   MCHelpCmdS         L"\n  s[name|-]     Convert archive to or from SFX"
#define   MCHelpCmdT         L"\n  t             Test archive files"
#define   MCHelpCmdU         L"\n  u             Update files in archive"
#define   MCHelpCmdV         L"\n  v[t[a],b]     Verbosely list archive contents [technical[all],bare]"
#define   MCHelpCmdX         L"\n  x             Extract files with full path"
#define   MCHelpSw           L"\n\n<Switches>"
#define   MCHelpSwm          L"\n  -             Stop switches scanning"
#define   MCHelpSwAT         L"\n  @[+]          Disable [enable] file lists"
#define   MCHelpSwAC         L"\n  ac            Clear Archive attribute after compression or extraction"
#define   MCHelpSwAD         L"\n  ad[1,2]       Alternate destination path"
#define   MCHelpSwAG         L"\n  ag[format]    Generate archive name using the current date"
#define   MCHelpSwAI         L"\n  ai            Ignore file attributes"
#define   MCHelpSwAM         L"\n  am[s,r]       Archive name and time [save, restore]"
#define   MCHelpSwAO         L"\n  ao            Add files with Archive attribute set"
#define   MCHelpSwAP         L"\n  ap<path>      Set path inside archive"
#define   MCHelpSwAS         L"\n  as            Synchronize archive contents"
#define   MCHelpSwCm         L"\n  c-            Disable comments show"
#define   MCHelpSwCFGm       L"\n  cfg-          Disable read configuration"
#define   MCHelpSwCL         L"\n  cl            Convert names to lower case"
#define   MCHelpSwCU         L"\n  cu            Convert names to upper case"
#define   MCHelpSwDF         L"\n  df            Delete files after archiving"
#define   MCHelpSwDH         L"\n  dh            Open shared files"
#define   MCHelpSwDR         L"\n  dr            Delete files to Recycle Bin"
#define   MCHelpSwDS         L"\n  ds            Disable name sort for solid archive"
#define   MCHelpSwDW         L"\n  dw            Wipe files after archiving"
#define   MCHelpSwEa         L"\n  e[+]<attr>    Set file exclude and include attributes"
#define   MCHelpSwED         L"\n  ed            Do not add empty directories"
#define   MCHelpSwEP         L"\n  ep            Exclude paths from names"
#define   MCHelpSwEP1        L"\n  ep1           Exclude base directory from names"
#define   MCHelpSwEP2        L"\n  ep2           Expand paths to full"
#define   MCHelpSwEP3        L"\n  ep3           Expand paths to full including the drive letter"
#define   MCHelpSwEP4        L"\n  ep4<path>     Exclude the path prefix from names"
#define   MCHelpSwF          L"\n  f             Freshen files"
#define   MCHelpSwHP         L"\n  hp[password]  Encrypt both file data and headers"
#define   MCHelpSwHT         L"\n  ht[b|c]       Select hash type [BLAKE2,CRC32] for file checksum"
#define   MCHelpSwIDP        L"\n  id[c,d,n,p,q] Display or disable messages"
#define   MCHelpSwIEML       L"\n  ieml[addr]    Send archive by email"
#define   MCHelpSwIERR       L"\n  ierr          Send all messages to stderr"
#define   MCHelpSwILOG       L"\n  ilog[name]    Log errors to file"
#define   MCHelpSwINUL       L"\n  inul          Disable all messages"
#define   MCHelpSwIOFF       L"\n  ioff[n]       Turn PC off after completing an operation"
#define   MCHelpSwISND       L"\n  isnd[-]       Control notification sounds"
#define   MCHelpSwIVER       L"\n  iver          Display the version number"
#define   MCHelpSwK          L"\n  k             Lock archive"
#define   MCHelpSwKB         L"\n  kb            Keep broken extracted files"
#define   MCHelpSwLog        L"\n  log[f][=name] Write names to log file"
#define   MCHelpSwMn         L"\n  m<0..5>       Set compression level (0-store...3-default...5-maximal)"
#define   MCHelpSwMC         L"\n  mc<par>       Set advanced compression parameters"
#define   MCHelpSwMD         L"\n  md[x]<n>[kmg] Dictionary size in KB, MB or GB"
#define   MCHelpSwME         L"\n  me[par]       Set encryption parameters"
#define   MCHelpSwMLP        L"\n  mlp           Use large memory pages"
#define   MCHelpSwMS         L"\n  ms[ext;ext]   Specify file types to store"
#define   MCHelpSwMT         L"\n  mt<threads>   Set the number of threads"
#define   MCHelpSwN          L"\n  n<file>       Additionally filter included files"
#define   MCHelpSwNa         L"\n  n@            Read additional filter masks from stdin"
#define   MCHelpSwNal        L"\n  n@<list>      Read additional filter masks from list file"
#define   MCHelpSwO          L"\n  o[+|-]        Set the overwrite mode"
#define   MCHelpSwOC         L"\n  oc            Set NTFS Compressed attribute"
#define   MCHelpSwOH         L"\n  oh            Save hard links as the link instead of the file"
#define   MCHelpSwOI         L"\n  oi[0-4][:min] Save identical files as references"
#define   MCHelpSwOL         L"\n  ol[a,-]       Process symbolic links as the link [absolute paths, skip]"
#define   MCHelpSwOM         L"\n  om[-|1][=lst] Propagate Mark of the Web"
#define   MCHelpSwONI        L"\n  oni           Allow potentially incompatible names"
#define   MCHelpSwOP         L"\n  op<path>      Set the output path for extracted files"
#define   MCHelpSwOR         L"\n  or            Rename files automatically"
#define   MCHelpSwOS         L"\n  os            Save NTFS streams"
#define   MCHelpSwOW         L"\n  ow            Save or restore file owner and group"
#define   MCHelpSwP          L"\n  p[password]   Set password"
#define   MCHelpSwQO         L"\n  qo[-|+]       Add quick open information [none|force]"
#define   MCHelpSwR          L"\n  r             Recurse subdirectories"
#define   MCHelpSwRm         L"\n  r-            Disable recursion"
#define   MCHelpSwR0         L"\n  r0            Recurse subdirectories for wildcard names only"
#define   MCHelpSwRI         L"\n  ri<P>[:<S>]   Set priority (0-default,1-min..15-max) and sleep time in ms"
#define   MCHelpSwRR         L"\n  rr[N]         Add data recovery record"
#define   MCHelpSwRV         L"\n  rv[N]         Create recovery volumes"
#define   MCHelpSwS          L"\n  s[<N>,v[-],e] Create solid archive"
#define   MCHelpSwSm         L"\n  s-            Disable solid archiving"
#define   MCHelpSwSC         L"\n  sc<chr>[obj]  Specify the character set"
#define   MCHelpSwSFX        L"\n  sfx[name]     Create SFX archive"
#define   MCHelpSwSI         L"\n  si[name]      Read data from standard input (stdin)"
#define   MCHelpSwSL         L"\n  sl<size>[u]   Process files with size less than specified"
#define   MCHelpSwSM         L"\n  sm<size>[u]   Process files with size more than specified"
#define   MCHelpSwT          L"\n  t             Test files after archiving"
#define   MCHelpSwTK         L"\n  tk            Keep original archive time"
#define   MCHelpSwTL         L"\n  tl            Set archive time to latest file"
#define   MCHelpSwTN         L"\n  tn[mcao]<t>   Process files newer than <t> time"
#define   MCHelpSwTO         L"\n  to[mcao]<t>   Process files older than <t> time"
#define   MCHelpSwTA         L"\n  ta[mcao]<d>   Process files modified after <d> YYYYMMDDHHMMSS date"
#define   MCHelpSwTB         L"\n  tb[mcao]<d>   Process files modified before <d> YYYYMMDDHHMMSS date"
#define   MCHelpSwTS         L"\n  ts[m,c,a,p]   Save or restore time (modification, creation, access, preserve)"
#define   MCHelpSwU          L"\n  u             Update files"
#define   MCHelpSwV          L"\n  v             Create volumes with size autodetection or list all volumes"
#define   MCHelpSwVUnr       L"\n  v             List all volumes"
#define   MCHelpSwVn         L"\n  v<size>[u]    Create volumes with size in [bBkKmMgGtT] units"
#define   MCHelpSwVD         L"\n  vd            Erase disk contents before creating volume"
#define   MCHelpSwVER        L"\n  ver[n]        File version control"
#define   MCHelpSwVP         L"\n  vp            Pause before each volume"
#define   MCHelpSwW          L"\n  w<path>       Assign work directory"
#define   MCHelpSwX          L"\n  x<file>       Exclude specified file"
#define   MCHelpSwXa         L"\n  x@            Read file names to exclude from stdin"
#define   MCHelpSwXal        L"\n  x@<list>      Exclude files listed in specified list file"
#define   MCHelpSwY          L"\n  y             Assume Yes on all queries"
#define   MCHelpSwZ          L"\n  z[file]       Read archive comment from file"
#define   MBadArc            L"\nERROR: Bad archive %s\n"
#define   MAskPsw            L"Enter password (will not be echoed)"
#define   MAskPswFor         L"\nEnter password (will not be echoed) for %s: "
#define   MReAskPsw          L"\nReenter password: "
#define   MNotMatchPsw       L"\nERROR: Passwords do not match\n"
#define   MErrWrite          L"Write error in the file %s"
#define   MErrRead           L"Read error in the file %s"
#define   MErrSeek           L"Seek error in the file %s"
#define   MErrFClose         L"Cannot close the file %s"
#define   MErrOutMem         L"Not enough memory"
#define   MErrBrokenArc      L"Corrupt archive - use 'Repair' command"
#define   MProgAborted       L"Program aborted"
#define   MErrRename         L"\nCannot rename %s to %s"
#define   MAbsNextVol        L"\nCannot find volume %s"
#define   MBreak             L"\nUser break\n"
#define   MAskCreatVol       L"\nCreate next volume?"
#define   MAskNextDisk       L"\nDisk full. Insert next"
#define   MCreatVol          L"\n\nCreating %sarchive %s\n"
#define   MAskNextVol        L"\nInsert disk with %s"
#define   MTestVol           L"\n\nTesting archive %s\n"
#define   MExtrVol           L"\n\nExtracting from %s\n"
#define   MConverting        L"\nConverting %s"
#define   MCvtToSFX          L"\nConvert archives to SFX"
#define   MCvtFromSFX        L"\nRemoving SFX module"
#define   MNotSFX            L"\n%s is not SFX archive"
#define   MNotRAR            L"\n%s is not RAR archive"
#define   MNotFirstVol       L"\n%s is not the first volume"
#define   MCvtOldFormat      L"\n%s - cannot convert to SFX archive with old format"
#define   MCannotCreate      L"\nCannot create %s"
#define   MCannotOpen        L"\nCannot open %s"
#define   MUnknownMeth       L"\nUnknown method in %s"
#define   MNewRarFormat      L"\nUnsupported archive format. Please update RAR to a newer version."
#define   MOk                L" OK"
#define   MDone              L"\nDone"
#define   MLockingArc        L"\nLocking archive"
#define   MNotMdfOld         L"\n\nERROR: Cannot modify old format archive"
#define   MNotMdfLock        L"\n\nERROR: Locked archive"
#define   MNotMdfVol         L"\n\nERROR: Cannot modify volume"
#define   MPackAskReg        L"\nEvaluation copy. Please register.\n"
#define   MCreateArchive     L"\nCreating %sarchive %s\n"
#define   MUpdateArchive     L"\nUpdating %sarchive %s\n"
#define   MAddSolid          L"solid "
#define   MAddFile           L"\nAdding    %-58s     "
#define   MUpdFile           L"\nUpdating  %-58s     "
#define   MAddPoints         L"\n...       %-58s     "
#define   MMoveDelFiles      L"\n\nDeleting files %s..."
#define   MMoveDelDirs       L"and directories"
#define   MMoveDelFile       L"\nDeleting %-30s"
#define   MMoveDeleted       L"    deleted"
#define   MMoveNotDeleted    L"    NOT DELETED"
#define   MClearAttrib       L"\n\nClearing attributes..."
#define   MMoveDelDir        L"\nDeleting directory %-30s"
#define   MWarErrFOpen       L"\nWARNING: Cannot open %d %s"
#define   MErrOpenFiles      L"files"
#define   MErrOpenFile       L"file"
#define   MAddNoFiles        L"\nWARNING: No files"
#define   MMdfEncrSol        L"\n%s: encrypted"
#define   MAddAnalyze        L"\nAnalyzing archived files: "
#define   MRepacking         L"\nRepacking archived files: "
#define   MCRCFailed         L"\n%-20s - checksum error"
#define   MExtrTest          L"\n\nTesting archive %s\n"
#define   MExtracting        L"\n\nExtracting from %s\n"
#define   MUseCurPsw         L"\n%s - use current password?"
#define   MCreatDir          L"\nCreating    %-56s"
#define   MExtrSkipFile      L"\nSkipping    %-56s"
#define   MExtrTestFile      L"\nTesting     %-56s"
#define   MExtrFile          L"\nExtracting  %-56s"
#define   MExtrPoints        L"\n...         %-56s"
#define   MExtrErrMkDir      L"\nCannot create directory %s"
#define   MExtrPrinting      L"\n------ Printing %s\n\n"
#define   MEncrBadCRC        L"\nChecksum error in the encrypted file %s. Corrupt file or wrong password."
#define   MExtrNoFiles       L"\nNo files to extract"
#define   MExtrAllOk         L"\nAll OK"
#define   MExtrTotalErr      L"\nTotal errors: %ld"
#define   MAskReplace        L"\n\nWould you like to replace the existing file %s\n%6s bytes, modified on %s\nwith a new one\n%6s bytes, modified on %s\n"
#define   MAskOverwrite      L"\nOverwrite %s?"
#define   MAskNewName        L"\nEnter new name: "
#define   MHeaderBroken      L"\nCorrupt header is found"
#define   MMainHeaderBroken  L"\nMain archive header is corrupt"
#define   MLogFileHead       L"\n%s - the file header is corrupt"
#define   MLogProtectHead    L"The data recovery header is corrupt"
#define   MArcComment        L"\nArchive comment"
#define   MReadStdinCmt      L"\nReading comment from stdin\n"
#define   MReadCommFrom      L"\nReading comment from %s"
#define   MDelComment        L"\nDeleting a comment from %s"
#define   MAddComment        L"\nAdding a comment to %s"
#define   MLogCommBrk        L"\nThe archive comment is corrupt"
#define   MCommAskCont       L"\nPress 'Enter' to continue or 'Q' to quit:"
#define   MWriteCommTo       L"\nWrite comment to %s"
#define   MCommNotPres       L"\nComment is not present"
#define   MDelFrom           L"\nDeleting from %s"
#define   MDeleting          L"\nDeleting %s"
#define   MEraseArc          L"\nErasing empty archive %s"
#define   MNoDelFiles        L"\nNo files to delete"
#define   MLogTitle          L"--------  %2d %s %d, archive %s"
#define   MPathTooLong       L"\nERROR: Path too long\n"
#define   MListArchive       L"Archive"
#define   MListDetails       L"Details"
#define   MListSolid         L"solid"
#define   MListSFX           L"SFX"
#define   MListVolume        L"volume"
#define   MListRR            L"recovery record"
#define   MListLock          L"lock"
#define   MListEnc           L"encrypted"
#define   MListEncHead       L"encrypted headers"
#define   MListTitleL        L" Attributes      Size     Date    Time   Name"
#define   MListTitleV        L" Attributes      Size    Packed Ratio    Date    Time   Checksum  Name"
#define   MListName          L"Name"
#define   MListType          L"Type"
#define   MListFile          L"File"
#define   MListDir           L"Directory"
#define   MListUSymlink      L"Unix symbolic link"
#define   MListWSymlink      L"Windows symbolic link"
#define   MListJunction      L"NTFS junction point"
#define   MListHardlink      L"Hard link"
#define   MListCopy          L"File reference"
#define   MListStream        L"NTFS alternate data stream"
#define   MListTarget        L"Target"
#define   MListSize          L"Size"
#define   MListPacked        L"Packed size"
#define   MListRatio         L"Ratio"
#define   MListMtime         L"mtime"
#define   MListCtime         L"ctime"
#define   MListAtime         L"atime"
#define   MListModified      L"Modified"
#define   MListCreated       L"Created"
#define   MListAccessed      L"Accessed"
#define   MListAttr          L"Attributes"
#define   MListFlags         L"Flags"
#define   MListCompInfo      L"Compression"
#define   MListHostOS        L"Host OS"
#define   MListFileVer       L"File version"
#define   MListService       L"Service"
#define   MListNTACLHead     L"\n   NTFS security data"
#define   MListStrmHead      L"\n   NTFS stream: %s"
#define   MListUnkHead       L"\n   Unknown subheader type: 0x%04x"
#define   MYes               L"Yes"
#define   MNo                L"No"
#define   MListNoFiles       L"  0 files\n"
#define   MRprReconstr       L"\nReconstructing %s"
#define   MRprBuild          L"\nBuilding %s"
#define   MRprOldFormat      L"\nCannot repair archive with old format"
#define   MRprFind           L"\nFound  %s"
#define   MRprAskIsSol       L"\nThe archive header is corrupt. Mark archive as solid?"
#define   MRprNoFiles        L"\nNo files found"
#define   MLogUnexpEOF       L"\nUnexpected end of archive"
#define   MRepAskReconst     L"\nReconstruct archive structure?"
#define   MRRSearch          L"\nSearching for recovery record"
#define   MAnalyzeFileData   L"\nAnalyzing file data"
#define   MRecRNotFound      L"\nData recovery record not found"
#define   MRecRFound         L"\nData recovery record found"
#define   MRecSecDamage      L"\nSector %ld (offsets %lX...%lX) damaged"
#define   MRecCorrected      L" - data recovered"
#define   MRecFailed         L" - cannot recover data"
#define   MAddRecRec         L"\nAdding the data recovery record"
#define   MEraseForVolume    L"\n\nErasing contents of drive %c:\n"
#define   MGetOwnersError    L"\nWARNING: Cannot get %s owner and group\n"
#define   MErrGetOwnerID     L"\nWARNING: Cannot get owner %s ID\n"
#define   MErrGetGroupID     L"\nWARNING: Cannot get group %s ID\n"
#define   MOwnersBroken      L"\nERROR: %s group and owner data are corrupt\n"
#define   MSetOwnersError    L"\nWARNING: Cannot set %s owner and group\n"
#define   MErrLnkRead        L"\nWARNING: Cannot read symbolic link %s"
#define   MSymLinkExists     L"\nWARNING: Symbolic link %s already exists"
#define   MAskRetryCreate    L"\nCannot create %s. Retry?"
#define   MDataBadCRC        L"\n%-20s : packed data checksum error in volume %s"
#define   MFileRO            L"\n%s is read-only"
#define   MACLGetError       L"\nWARNING: Cannot get %s security data\n"
#define   MACLSetError       L"\nWARNING: Cannot set %s security data\n"
#define   MACLBroken         L"\nERROR: %s security data are corrupt\n"
#define   MACLUnknown        L"\nWARNING: Unknown format of %s security data\n"
#define   MStreamBroken      L"\nERROR: %s stream data are corrupt\n"
#define   MStreamUnknown     L"\nWARNING: Unknown format of %s stream data\n"
#define   MInvalidName       L"\nERROR: Invalid file name %s"
#define   MProcessArc        L"\n\nProcessing archive %s"
#define   MCorrectingName    L"\nWARNING: Attempting to correct the invalid file or directory name"
#define   MUnpCannotMerge    L"\nWARNING: You need to start extraction from a previous volume to unpack %s"
#define   MUnknownOption     L"\nERROR: Unknown option: %s"
#define   MSwSyntaxError     L"\nERROR: '-' is expected in the beginning of: %s"
#define   MSubHeadCorrupt    L"\nERROR: Corrupt data header found, ignored"
#define   MSubHeadUnknown    L"\nWARNING: Unknown data header format, ignored"
#define   MSubHeadDataCRC    L"\nERROR: Corrupt %s data block"
#define   MScanError         L"\nCannot read contents of %s"
#define   MNotVolume         L"\n%s is not volume"
#define   MRecVolDiffSets    L"\nERROR: %s and %s belong to different sets"
#define   MRecVolMissing     L"\n%d volumes missing"
#define   MRecVolFound       L"\n%d recovery volumes found"
#define   MRecVolAllExist    L"\nNothing to reconstruct"
#define   MRecVolCannotFix   L"\nReconstruction impossible"
#define   MReconstructing    L"\nReconstructing..."
#define   MCreating          L"\nCreating %s"
#define   MRenaming          L"\nRenaming %s to %s"
#define   MNTFSRequired      L"\nWrite error: only NTFS file system supports files larger than 4 GB"
#define   MFAT32Size         L"\nWARNING: FAT32 file system does not support 4 GB or larger files"
#define   MErrChangeAttr     L"\nWARNING: Cannot change attributes of %s"
#define   MWrongSFXVer       L"\nERROR: default SFX module does not support RAR %d.%d archives"
#define   MHeadEncMismatch   L"\nCannot change the header encryption mode in already encrypted archive"
#define   MCannotEmail       L"\nCannot email the file %s"
#define   MCopyrightS        L"\nRAR SFX archive"
#define   MSHelpCmd          L"\n\n<Commands>"
#define   MSHelpCmdE         L"\n  -x      Extract from archive (default)"
#define   MSHelpCmdT         L"\n  -t      Test archive files"
#define   MSHelpCmdV         L"\n  -v      Verbosely list contents of archive"
#define   MRecVolLimit       L"\nTotal number of usual and recovery volumes must not exceed %d"
#define   MVolumeNumber      L"volume %d"
#define   MCannotDelete      L"\nCannot delete %s"
#define   MRecycleFailed     L"\nCannot move some files and directories to Recycle Bin"
#define   MCalcCRC           L"\nCalculating the checksum"
#define   MTooLargeSFXArc    L"\nToo large SFX archive. Windows cannot run the executable file exceeding 4 GB."
#define   MCalcCRCAllVol     L"\nCalculating checksums of all volumes."
#define   MNotEnoughDisk     L"\nERROR: Not enough disk space for %s."
#define   MNewerRAR          L"\nYou may need a newer version of RAR."
#define   MUnkEncMethod      L"\nUnknown encryption method in %s"
#define   MWrongPassword     L"\nThe specified password is incorrect."
#define   MWrongFilePassword L"\nIncorrect password for %s"
#define   MAreaDamaged       L"\nCorrupt %d bytes at %08x %08x"
#define   MBlocksRecovered   L"\n%u blocks are recovered, %u blocks are relocated"
#define   MRRDamaged         L"\nRecovery record is corrupt."
#define   MTestingRR         L"\nTesting the recovery record"
#define   MFailed            L"Failed"
#define   MIncompatSwitch    L"\n%s switch is not supported for RAR %d.x archive format."
#define   MSearchDupFiles    L"\nSearching for identical files"
#define   MNumFound          L"%d found."
#define   MUnknownExtra      L"\nUnknown extra field in %s."
#define   MCorruptExtra      L"\nCorrupt %s extra field in %s."
#define   MCopyError         L"\nCannot copy %s to %s."
#define   MCopyErrorHint     L"\nYou need to unpack the entire archive to create file reference entries."
#define   MCopyingData       L"\nCopying data"
#define   MErrCreateLnkS     L"\nCannot create symbolic link %s"
#define   MErrCreateLnkH     L"\nCannot create hard link %s"
#define   MErrLnkTarget      L"\nYou need to unpack the link target first"
#define   MNeedAdmin         L"\nYou may need to run RAR as administrator"
#define   MDictOutMem        L"\nNot enough memory for %d MB compression dictionary, changed to %d MB."
#define   MUseSmalllerDict   L"\nPlease use a smaller compression dictionary."
#define   MExtrDictOutMem    L"\nNot enough memory to unpack the archive with %u GB compression dictionary."
#define   MSuggest64bit      L"\n64-bit RAR version is necessary."
#define   MOpenErrAtime      L"\nYou may need to remove -tsp switch or run RAR as administrator to open this file."
#define   MErrReadInfo       L"\nChoose 'Ignore' to continue with the already read file part only, 'Ignore all' to do it for all read errors, 'Retry' to repeat read and 'Quit' to abort."
#define   MErrReadTrunc      L"\n%s is archived incompletely because of read error.\n"
#define   MErrReadCount      L"\n%u files are archived incompletely because of read errors."
#define   MDirNameExists     L"\nDirectory with such name already exists"
#define   MStdinNoInput      L"\nKeyboard input is not allowed when reading data from stdin"
#define   MTruncPsw          L"\nPassword exceeds the maximum allowed length of %u characters and will be truncated."
#define   MAdjustValue       L"\nAdjusting %s value to %s."
#define   MOpFailed          L"\nOperation failed"
#define   MSkipEncArc        L"\nSkipping the encrypted archive %s"
#define   MOrigName          L"Original name"
#define   MOriginalTime      L"Original time"
#define   MFileRenamed       L"\n%s is renamed to %s"
#define   MDictNotAllowed    L"\n%u GB dictionary exceeds %u GB limit and needs more than %u GB memory to unpack."
#define   MDictExtrAnyway    L"\nUse -md%ug or -mdx%ug switches to extract anyway."
#define   MDictComprLimit    L"\n%u GB dictionary exceeds %u GB limit and not allowed when compressing data."
#define   MNeedSFX64         L"\n64-bit self-extracting module is necessary for %u GB compression dictionary."
#define   MSkipUnsafeLink    L"\nSkipping the potentially unsafe %s -> %s link. For archives from a trustworthy source use -ola to extract it anyway."
#define   MTruncService      L"\nTruncated at the service block: %s"
#define   MHeaderQO          L"quick open information"
#define   MHeaderRR          L"recovery record"
#define   MLockInMemoryNeeded L"-mlp switch requires ""Lock pages in memory"" privilege. Do you wish to assign it to the current user account?"
#define   MPrivilegeAssigned L"User privilege has been successfully assigned and will be activated after Windows restart. Restart now?"

  #endif

#endif

#ifndef _RAR_RAWINT_
#define _RAR_RAWINT_

#define  rotls(x,n,xsize)  (((x)<<(n)) | ((x)>>(xsize-(n))))
#define  rotrs(x,n,xsize)  (((x)>>(n)) | ((x)<<(xsize-(n))))
#define  rotl32(x,n)       rotls(x,n,32)
#define  rotr32(x,n)       rotrs(x,n,32)

inline uint RawGet2(const void *Data)
{
  byte *D=(byte *)Data;
  return D[0]+(D[1]<<8);
}

inline uint32 RawGet4(const void *Data)
{
#if defined(BIG_ENDIAN) || !defined(ALLOW_MISALIGNED)
  byte *D=(byte *)Data;
  return D[0]+(D[1]<<8)+(D[2]<<16)+(D[3]<<24);
#else
  return *(uint32 *)Data;
#endif
}

inline uint64 RawGet8(const void *Data)
{
#if defined(BIG_ENDIAN) || !defined(ALLOW_MISALIGNED)
  byte *D=(byte *)Data;
  return INT32TO64(RawGet4(D+4),RawGet4(D));
#else
  return *(uint64 *)Data;
#endif
}

inline void RawPut2(uint Field,void *Data)
{
  byte *D=(byte *)Data;
  D[0]=(byte)(Field);
  D[1]=(byte)(Field>>8);
}

inline void RawPut4(uint Field,void *Data)
{
#if defined(BIG_ENDIAN) || !defined(ALLOW_MISALIGNED)
  byte *D=(byte *)Data;
  D[0]=(byte)(Field);
  D[1]=(byte)(Field>>8);
  D[2]=(byte)(Field>>16);
  D[3]=(byte)(Field>>24);
#else
  *(uint32 *)Data=(uint32)Field;
#endif
}

inline void RawPut8(uint64 Field,void *Data)
{
#if defined(BIG_ENDIAN) || !defined(ALLOW_MISALIGNED)
  byte *D=(byte *)Data;
  D[0]=(byte)(Field);
  D[1]=(byte)(Field>>8);
  D[2]=(byte)(Field>>16);
  D[3]=(byte)(Field>>24);
  D[4]=(byte)(Field>>32);
  D[5]=(byte)(Field>>40);
  D[6]=(byte)(Field>>48);
  D[7]=(byte)(Field>>56);
#else
  *(uint64 *)Data=Field;
#endif
}

#if defined(LITTLE_ENDIAN) && defined(ALLOW_MISALIGNED)
#define USE_MEM_BYTESWAP
#endif

inline uint32 RawGetBE4(const byte *m)
{
#if defined(USE_MEM_BYTESWAP) && defined(_MSC_VER)
  return _byteswap_ulong(*(uint32 *)m);
#elif defined(USE_MEM_BYTESWAP) && (defined(__clang__) || defined(__GNUC__))
  return __builtin_bswap32(*(uint32 *)m);
#else
  return uint32(m[0])<<24 | uint32(m[1])<<16 | uint32(m[2])<<8 | m[3];
#endif
}

inline uint64 RawGetBE8(const byte *m)
{
#if defined(USE_MEM_BYTESWAP) && defined(_MSC_VER)
  return _byteswap_uint64(*(uint64 *)m);
#elif defined(USE_MEM_BYTESWAP) && (defined(__clang__) || defined(__GNUC__))
  return __builtin_bswap64(*(uint64 *)m);
#else
  return uint64(m[0])<<56 | uint64(m[1])<<48 | uint64(m[2])<<40 | uint64(m[3])<<32 |
         uint64(m[4])<<24 | uint64(m[5])<<16 | uint64(m[6])<<8 | m[7];
#endif
}

inline void RawPutBE4(uint i,byte *mem)
{
#if defined(USE_MEM_BYTESWAP) && defined(_MSC_VER)
  *(uint32*)mem = _byteswap_ulong((uint32)i);
#elif defined(USE_MEM_BYTESWAP) && (defined(__clang__) || defined(__GNUC__))
  *(uint32*)mem = __builtin_bswap32((uint32)i);
#else
  mem[0]=byte(i>>24);
  mem[1]=byte(i>>16);
  mem[2]=byte(i>>8);
  mem[3]=byte(i);
#endif
}

inline void RawPutBE8(uint64 i,byte *mem)
{
#if defined(USE_MEM_BYTESWAP) && defined(_MSC_VER)
  *(uint64*)mem = _byteswap_uint64(i);
#elif defined(USE_MEM_BYTESWAP) && (defined(__clang__) || defined(__GNUC__))
  *(uint64*)mem = __builtin_bswap64(i);
#else
  mem[0]=byte(i>>56);
  mem[1]=byte(i>>48);
  mem[2]=byte(i>>40);
  mem[3]=byte(i>>32);
  mem[4]=byte(i>>24);
  mem[5]=byte(i>>16);
  mem[6]=byte(i>>8);
  mem[7]=byte(i);
#endif
}

inline uint32 ByteSwap32(uint32 i)
{
#ifdef _MSC_VER
  return _byteswap_ulong(i);
#elif defined(__clang__) || defined(__GNUC__)
  return  __builtin_bswap32(i);
#else
  return (rotl32(i,24)&0xFF00FF00)|(rotl32(i,8)&0x00FF00FF);
#endif
}

inline bool IsPow2(uint64 n)
{
  return (n & (n-1))==0;
}

inline uint64 GetGreaterOrEqualPow2(uint64 n)
{
  uint64 p=1;
  while (p<n)
    p*=2;
  return p;
}

inline uint64 GetLessOrEqualPow2(uint64 n)
{
  uint64 p=1;
  while (p*2<=n)
    p*=2;
  return p;
}
#endif

#ifndef _RAR_UNICODE_
#define _RAR_UNICODE_

#if defined( _WIN_ALL)
#define DBCS_SUPPORTED
#endif

bool WideToChar(const wchar *Src,char *Dest,size_t DestSize);
bool CharToWide(const char *Src,wchar *Dest,size_t DestSize);
bool WideToChar(const std::wstring &Src,std::string &Dest);
bool CharToWide(const std::string &Src,std::wstring &Dest);
byte* WideToRaw(const wchar *Src,size_t SrcSize,byte *Dest,size_t DestSize);
void WideToRaw(const std::wstring &Src,std::vector<byte> &Dest);
wchar* RawToWide(const byte *Src,wchar *Dest,size_t DestSize);
std::wstring RawToWide(const std::vector<byte> &Src);
void WideToUtf(const wchar *Src,char *Dest,size_t DestSize);
void WideToUtf(const std::wstring &Src,std::string &Dest);
size_t WideToUtfSize(const wchar *Src);
bool UtfToWide(const char *Src,wchar *Dest,size_t DestSize);
bool UtfToWide(const char *Src,std::wstring &Dest);

bool IsTextUtf8(const byte *Src);
bool IsTextUtf8(const byte *Src,size_t SrcSize);

int wcsicomp(const wchar *s1,const wchar *s2);
inline int wcsicomp(const std::wstring &s1,const std::wstring &s2) {return wcsicomp(s1.c_str(),s2.c_str());}
int wcsnicomp(const wchar *s1,const wchar *s2,size_t n);
inline int wcsnicomp(const std::wstring &s1,const std::wstring &s2,size_t n) {return wcsnicomp(s1.c_str(),s2.c_str(),n);}
const wchar_t* wcscasestr(const wchar_t *str, const wchar_t *search);
std::wstring::size_type wcscasestr(const std::wstring &str, const std::wstring &search);
#ifndef SFX_MODULE
wchar* wcslower(wchar *s);
void wcslower(std::wstring &s);
wchar* wcsupper(wchar *s);
void wcsupper(std::wstring &s);
#endif
int toupperw(int ch);
int tolowerw(int ch);
int atoiw(const std::wstring &s);
int64 atoilw(const std::wstring &s);

#ifdef DBCS_SUPPORTED
class SupportDBCS
{
  public:
    SupportDBCS();
    void Init();
    char* charnext(const char *s);

    bool IsLeadByte[256];
    bool DBCSMode;
};
extern SupportDBCS gdbcs;

inline char* charnext(const char *s) {return (char *)(gdbcs.DBCSMode ? gdbcs.charnext(s):s+1);}
inline bool IsDBCSMode() {return gdbcs.DBCSMode;}

#else
#define charnext(s) ((s)+1)
#define IsDBCSMode() (false)
#endif

#endif

#ifndef _RAR_ERRHANDLER_
#define _RAR_ERRHANDLER_

enum RAR_EXIT
{
  RARX_SUCCESS   =   0,
  RARX_WARNING   =   1,
  RARX_FATAL     =   2,
  RARX_CRC       =   3,
  RARX_LOCK      =   4,
  RARX_WRITE     =   5,
  RARX_OPEN      =   6,
  RARX_USERERROR =   7,
  RARX_MEMORY    =   8,
  RARX_CREATE    =   9,
  RARX_NOFILES   =  10,
  RARX_BADPWD    =  11,
  RARX_READ      =  12,
  RARX_BADARC    =  13,
  RARX_USERBREAK = 255
};

class ErrorHandler
{
  private:
    RAR_EXIT ExitCode;
    uint ErrCount;
    bool EnableBreak;
    bool Silent;
    bool DisableShutdown;
    bool ReadErrIgnoreAll;
  public:
    ErrorHandler();
    void Clean();
    void MemoryError();
    void OpenError(const std::wstring &FileName);
    void CloseError(const std::wstring &FileName);
    void ReadError(const std::wstring &FileName);
    void AskRepeatRead(const std::wstring &FileName,bool &Ignore,bool &Retry,bool &Quit);
    void WriteError(const std::wstring &ArcName,const std::wstring &FileName);
    void WriteErrorFAT(const std::wstring &FileName);
    bool AskRepeatWrite(const std::wstring &FileName,bool DiskFull);
    void SeekError(const std::wstring &FileName);
    void GeneralErrMsg(const wchar *fmt,...);
    void MemoryErrorMsg();
    void OpenErrorMsg(const std::wstring &FileName);
    void OpenErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void CreateErrorMsg(const std::wstring &FileName);
    void CreateErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void ReadErrorMsg(const std::wstring &FileName);
    void ReadErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void WriteErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void ArcBrokenMsg(const std::wstring &ArcName);
    void ChecksumFailedMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void UnknownMethodMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void Exit(RAR_EXIT ExitCode);
    void SetErrorCode(RAR_EXIT Code);
    RAR_EXIT GetErrorCode() {return ExitCode;}
    uint GetErrorCount() {return ErrCount;}
    void SetSignalHandlers(bool Enable);
    void Throw(RAR_EXIT Code);
    void SetSilent(bool Mode) {Silent=Mode;}
    bool GetSysErrMsg(std::wstring &Msg);
    void SysErrMsg();
    int GetSystemErrorCode();
    void SetSystemErrorCode(int Code);
    void SetDisableShutdown() {DisableShutdown=true;}
    bool IsShutdownEnabled() {return !DisableShutdown;}

    bool UserBreak;
    bool MainExit;
};

#endif

#ifndef _RAR_SECURE_PASSWORD_
#define _RAR_SECURE_PASSWORD_

class SecPassword
{
  private:
    void Process(const wchar *Src,size_t SrcSize,wchar *Dst,size_t DstSize,bool Encode);

    std::vector<wchar> Password = std::vector<wchar>(MAXPASSWORD);
    bool PasswordSet;
  public:
    SecPassword();
    ~SecPassword();
    void Clean();
    void Get(wchar *Psw,size_t MaxSize);
    void Get(std::wstring &Psw);
    void Set(const wchar *Psw);
    bool IsSet() {return PasswordSet;}
    size_t Length();
    bool operator == (SecPassword &psw);
};

void cleandata(void *data,size_t size);
inline void cleandata(std::wstring &s) {cleandata(&s[0],s.size()*sizeof(s[0]));}
void SecHideData(void *Data,size_t DataSize,bool Encode,bool CrossProcess);

#endif

#ifndef _RAR_STRLIST_
#define _RAR_STRLIST_

class StringList
{
  private:
    std::vector<wchar> StringData;
    size_t CurPos;

    size_t StringsCount;

    size_t SaveCurPos[16],SavePosNumber;
  public:
    StringList();
    void Reset();

    void AddString(const wchar *Str);
    void AddString(const std::wstring &Str);

    bool GetString(wchar *Str,size_t MaxLength);
    bool GetString(std::wstring &Str);
    bool GetString(wchar *Str,size_t MaxLength,int StringNum);
    bool GetString(std::wstring &Str,int StringNum);
    wchar* GetString();
    bool GetString(wchar **Str);
    void Rewind();
    size_t ItemsCount() {return StringsCount;};
    size_t GetCharCount() {return StringData.size();}
    bool Search(const std::wstring &Str,bool CaseSensitive);
    void SavePosition();
    void RestorePosition();
};

#endif

#ifndef _RAR_TIMEFN_
#define _RAR_TIMEFN_

struct RarLocalTime
{
  uint Year;
  uint Month;
  uint Day;
  uint Hour;
  uint Minute;
  uint Second;
  uint Reminder;
  uint wDay;
  uint yDay;
};

class RarTime
{
  private:
    static const uint TICKS_PER_SECOND = 1000000000;

    uint64 itime;
  public:

    static const uint REMINDER_PRECISION = TICKS_PER_SECOND;
  public:
    RarTime() {Reset();}
    bool operator == (const RarTime &rt) const {return itime==rt.itime;}
    bool operator != (const RarTime &rt) const {return itime!=rt.itime;}
    bool operator < (const RarTime &rt)  const {return itime<rt.itime;}
    bool operator <= (const RarTime &rt) const {return itime<rt.itime || itime==rt.itime;}
    bool operator > (const RarTime &rt)  const {return itime>rt.itime;}
    bool operator >= (const RarTime &rt) const {return itime>rt.itime || itime==rt.itime;}

    void GetLocal(RarLocalTime *lt);
    void SetLocal(RarLocalTime *lt);
#ifdef _WIN_ALL
    void GetWinFT(FILETIME *ft);
    void SetWinFT(FILETIME *ft);
#endif
    uint64 GetWin();
    void SetWin(uint64 WinTime);
    time_t GetUnix();
    void SetUnix(time_t ut);
    uint64 GetUnixNS();
    void SetUnixNS(uint64 ns);
    uint GetDos();
    void SetDos(uint DosTime);
    void GetText(wchar *DateStr,size_t MaxSize,bool FullMS);
    void SetIsoText(const wchar *TimeText);
    void SetAgeText(const wchar *TimeText);
    void SetCurrentTime();
    void Reset() {itime=0;}
    bool IsSet() {return itime!=0;}
    void Adjust(int64 ns);
};

const wchar *GetMonthName(uint Month);
bool IsLeapYear(uint Year);

#endif

#ifndef _RAR_SHA1_
#define _RAR_SHA1_

typedef struct {
    uint32 state[5];
    uint64 count;
    unsigned char buffer[64];
} sha1_context;

void sha1_init( sha1_context * c );
void sha1_process(sha1_context * c, const byte *data, size_t len);
void sha1_process_rar29(sha1_context *context, const unsigned char *data, size_t len);
void sha1_done( sha1_context * c, uint32 digest[5] );

#endif

#ifndef _RAR_SHA256_
#define _RAR_SHA256_

#define SHA256_DIGEST_SIZE 32

typedef struct
{
  uint32 H[8];
  uint64 Count;
  byte Buffer[64];
} sha256_context;

void sha256_init(sha256_context *ctx);
void sha256_process(sha256_context *ctx, const void *Data, size_t Size);
void sha256_done(sha256_context *ctx, byte *Digest);
void sha256_get(const void *Data, size_t Size, byte *Digest);

#endif

#ifndef _RAR_BLAKE2_
#define _RAR_BLAKE2_

#define BLAKE2_DIGEST_SIZE 32
#define BLAKE2_THREADS_NUMBER 8

constexpr size_t BLAKE2S_BLOCKBYTES = 64;
constexpr size_t BLAKE2S_OUTBYTES = 32;

struct blake2s_state
{

  static constexpr size_t BLAKE_ALIGNMENT = 64;

  static constexpr size_t BLAKE_DATA_SIZE = 48 + 2 * BLAKE2S_BLOCKBYTES;

  byte ubuf[BLAKE_DATA_SIZE + BLAKE_ALIGNMENT];

  byte   *buf;
  uint32 *h, *t, *f;

  size_t   buflen;
  byte  last_node;

  blake2s_state()
  {
    set_pointers();
  }

  blake2s_state(blake2s_state &st)
  {
    set_pointers();
    *this=st;
  }

  void set_pointers()
  {

    buf = (byte *) ALIGN_VALUE(ubuf, BLAKE_ALIGNMENT);
    h   = (uint32 *) (buf + 2 * BLAKE2S_BLOCKBYTES);
    t   = h + 8;
    f   = t + 2;
  }

  void init()
  {
    memset( ubuf, 0, sizeof( ubuf ) );
    buflen = 0;
    last_node = 0;
  }

  blake2s_state& operator = (blake2s_state &st)
  {
    if (this != &st)
    {
      memcpy(buf, st.buf, BLAKE_DATA_SIZE);
      buflen = st.buflen;
      last_node = st.last_node;
    }
    return *this;
  }
};

#ifdef RAR_SMP
class ThreadPool;
#endif

struct blake2sp_state
{
  blake2s_state S[8];
  blake2s_state R;
  byte buf[8 * BLAKE2S_BLOCKBYTES];
  size_t buflen;

#ifdef RAR_SMP
  ThreadPool *ThPool;
  uint MaxThreads;
#endif
};

void blake2sp_init( blake2sp_state *S );
void blake2sp_update( blake2sp_state *S, const byte *in, size_t inlen );
void blake2sp_final( blake2sp_state *S, byte *digest );

#endif

#ifndef _RAR_DATAHASH_
#define _RAR_DATAHASH_

enum HASH_TYPE {HASH_NONE,HASH_RAR14,HASH_CRC32,HASH_BLAKE2};

struct HashValue
{
  void Init(HASH_TYPE Type);

  bool operator == (const HashValue &cmp) const;

  bool operator != (const HashValue &cmp) const {return !(*this==cmp);}

  HASH_TYPE Type;
  union
  {
    uint CRC32;
    byte Digest[SHA256_DIGEST_SIZE];
  };
};

#ifdef RAR_SMP
class ThreadPool;
class DataHash;
#endif

class DataHash
{
  public:
    struct CRC32ThreadData
    {
      void *Data;
      size_t DataSize;
      uint DataCRC;
    };
  private:
    void UpdateCRC32MT(const void *Data,size_t DataSize);
    uint BitReverse32(uint N);
    uint gfMulCRC(uint A, uint B);
    uint gfExpCRC(uint N);

    static const uint CRC32_POOL_THREADS=8;

    static const uint HASH_POOL_THREADS=Max(BLAKE2_THREADS_NUMBER,CRC32_POOL_THREADS);

    HASH_TYPE HashType;
    uint CurCRC32;
    blake2sp_state *blake2ctx;

#ifdef RAR_SMP
    ThreadPool *ThPool;

    uint MaxThreads;
#endif
  public:
    DataHash();
    ~DataHash();
    void Init(HASH_TYPE Type,uint MaxThreads);
    void Update(const void *Data,size_t DataSize);
    void Result(HashValue *Result);
    uint GetCRC32();
    bool Cmp(HashValue *CmpValue,byte *Key);
    HASH_TYPE Type() {return HashType;}
};

#endif

#ifndef _RAR_OPTIONS_
#define _RAR_OPTIONS_

#define DEFAULT_RECOVERY     -3

#define DEFAULT_RECVOLUMES  -10

#define VOLSIZE_AUTO   INT64NDF

enum PATH_EXCL_MODE {
  EXCL_UNCHANGED=0,
  EXCL_SKIPWHOLEPATH,
  EXCL_BASEPATH,
  EXCL_SAVEFULLPATH,
  EXCL_ABSPATH
};

enum {SOLID_NONE=0,SOLID_NORMAL=1,SOLID_COUNT=2,SOLID_FILEEXT=4,
      SOLID_VOLUME_DEPENDENT=8,SOLID_VOLUME_INDEPENDENT=16};

enum {ARCTIME_NONE=0,ARCTIME_KEEP,ARCTIME_LATEST};

enum EXTTIME_MODE {
  EXTTIME_NONE=0,EXTTIME_1S,EXTTIME_MAX
};

enum {NAMES_ORIGINALCASE=0,NAMES_UPPERCASE,NAMES_LOWERCASE};

enum MESSAGE_TYPE {MSG_STDOUT=0,MSG_STDERR,MSG_ERRONLY,MSG_NULL};

enum RECURSE_MODE
{
  RECURSE_NONE=0,
  RECURSE_DISABLE,
  RECURSE_ALWAYS,
  RECURSE_WILDCARDS
};

enum OVERWRITE_MODE
{
  OVERWRITE_DEFAULT=0,
  OVERWRITE_ALL,
  OVERWRITE_NONE,
  OVERWRITE_AUTORENAME,
  OVERWRITE_FORCE_ASK
};

enum ARC_METADATA
{
  ARCMETA_NONE=0,
  ARCMETA_SAVE,
  ARCMETA_RESTORE
};

enum QOPEN_MODE { QOPEN_NONE=0, QOPEN_AUTO, QOPEN_ALWAYS };

enum RAR_CHARSET { RCH_DEFAULT=0,RCH_ANSI,RCH_OEM,RCH_UNICODE,RCH_UTF8 };

#define     MAX_FILTER_TYPES           16

enum FilterState {
  FILTER_DEFAULT=0,
  FILTER_AUTO,
  FILTER_FORCE,
  FILTER_DISABLE
};

enum SAVECOPY_MODE {
  SAVECOPY_NONE=0, SAVECOPY_SILENT, SAVECOPY_LIST, SAVECOPY_LISTEXIT,
  SAVECOPY_DUPLISTEXIT
};

enum APPENDARCNAME_MODE
{
  APPENDARCNAME_NONE=0,APPENDARCNAME_DESTPATH,APPENDARCNAME_OWNSUBDIR,
  APPENDARCNAME_OWNDIR
};

enum POWER_MODE {
  POWERMODE_KEEP=0,POWERMODE_OFF,POWERMODE_HIBERNATE,POWERMODE_SLEEP,
  POWERMODE_RESTART
};

enum SOUND_NOTIFY_MODE {SOUND_NOTIFY_DEFAULT=0,SOUND_NOTIFY_ON,SOUND_NOTIFY_OFF};

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
    void Init();

    uint ExclFileAttr;
    uint InclFileAttr;

    bool ExclDir;
    bool InclDir;

    bool InclAttrSet;
    uint64 WinSize;
    uint64 WinSizeLimit;

#ifdef USE_QOPEN
    QOPEN_MODE QOpenMode;
#endif

    bool ConfigDisabled;
    RAR_CHARSET CommentCharset;
    RAR_CHARSET FilelistCharset;
    RAR_CHARSET ErrlogCharset;
    RAR_CHARSET RedirectCharset;

    bool EncryptHeaders;
    bool SkipEncrypted;

    bool ManualPassword;

    MESSAGE_TYPE MsgStream;
    SOUND_NOTIFY_MODE Sound;
    OVERWRITE_MODE Overwrite;
    int Method;
    HASH_TYPE HashType;
    int Recovery;
    int RecVolNumber;
    ARC_METADATA ArcMetadata;
    bool DisablePercentage;
    bool DisableCopyright;
    bool DisableDone;
    bool DisableNames;
    bool PrintVersion;
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
    uint CurVolNum;
    bool AllYes;
    bool VerboseOutput;
    bool DisableSortSolid;
    int ArcTime;
    int ConvertNames;
    bool ProcessOwners;
    bool SaveSymLinks;
    bool SaveHardLinks;
    bool AbsoluteLinks;
    bool SkipSymLinks;
    int Priority;
    int SleepTime;

    bool UseLargePages;

    bool SetupComplete;

    bool KeepBroken;
    bool OpenShared;
    bool DeleteFiles;

#ifdef _WIN_ALL
    bool AllowIncompatNames;
#endif

#ifndef SFX_MODULE
    bool GenerateArcName;
    wchar GenerateMask[MAX_GENERATE_MASK];
    wchar DefGenerateMask[MAX_GENERATE_MASK];
#endif
    bool SyncFiles;
    bool ProcessEA;
    bool SaveStreams;
#ifdef PROPAGATE_MOTW
    bool MotwAllFields;
#endif
    bool SetCompressedAttr;
    bool IgnoreGeneralAttr;
    RarTime FileMtimeBefore,FileCtimeBefore,FileAtimeBefore;
    bool FileMtimeBeforeOR,FileCtimeBeforeOR,FileAtimeBeforeOR;
    RarTime FileMtimeAfter,FileCtimeAfter,FileAtimeAfter;
    bool FileMtimeAfterOR,FileCtimeAfterOR,FileAtimeAfterOR;
    int64 FileSizeLess;
    int64 FileSizeMore;
    bool Lock;
    bool Test;
    bool VolumePause;
    FilterMode FilterModes[MAX_FILTER_TYPES];
    uint VersionControl;
    APPENDARCNAME_MODE AppendArcNameToPath;
    POWER_MODE Shutdown;
    EXTTIME_MODE xmtime;
    EXTTIME_MODE xctime;
    EXTTIME_MODE xatime;
    bool PreserveAtime;

    uint Threads;

#ifdef RARDLL
    int DllOpMode;
    int DllError;
    LPARAM UserData;
    UNRARCALLBACK Callback;
    CHANGEVOLPROC ChangeVolProc;
    PROCESSDATAPROC ProcessDataProc;
#endif

};
#endif

#ifndef _RIJNDAEL_H_
#define _RIJNDAEL_H_

#define _MAX_KEY_COLUMNS (256/32)
#define _MAX_ROUNDS      14
#define MAX_IV_SIZE      16

class Rijndael
{
  private:

#ifdef USE_SSE
#ifdef __GNUC__
    __attribute__((target("aes")))
#endif
    void blockEncryptSSE(const byte *input,size_t numBlocks,byte *outBuffer);
#ifdef __GNUC__
    __attribute__((target("aes")))
#endif
    void blockDecryptSSE(const byte *input, size_t numBlocks, byte *outBuffer);

    bool AES_NI;
#endif

#ifdef USE_NEON_AES

    __attribute__((target("+crypto")))
    void blockEncryptNeon(const byte *input,size_t numBlocks,byte *outBuffer);
    __attribute__((target("+crypto")))
    void blockDecryptNeon(const byte *input, size_t numBlocks, byte *outBuffer);

    bool AES_Neon;
#endif

    void keySched(byte key[_MAX_KEY_COLUMNS][4]);
    void keyEncToDec();
    void GenerateTables();

    bool     CBCMode;

    int      m_uRounds;
    byte     m_initVector[MAX_IV_SIZE];
    byte     m_expandedKey[_MAX_ROUNDS+1][4][4];
  public:
    Rijndael();
    void Init(bool Encrypt,const byte *key,uint keyLen,const byte *initVector);
    void blockEncrypt(const byte *input, size_t inputLen, byte *outBuffer);
    void blockDecrypt(const byte *input, size_t inputLen, byte *outBuffer);
    void SetCBCMode(bool Mode) {CBCMode=Mode;}
};

#endif

#ifndef _RAR_CRYPT_
#define _RAR_CRYPT_

enum CRYPT_METHOD {
  CRYPT_NONE,CRYPT_RAR13,CRYPT_RAR15,CRYPT_RAR20,CRYPT_RAR30,CRYPT_RAR50,
  CRYPT_UNKNOWN
};

#define SIZE_SALT50              16
#define SIZE_SALT30               8
#define SIZE_INITV               16
#define SIZE_PSWCHECK             8
#define SIZE_PSWCHECK_CSUM        4

#define CRYPT_BLOCK_SIZE         16
#define CRYPT_BLOCK_MASK         (CRYPT_BLOCK_SIZE-1)

#define CRYPT5_KDF_LG2_COUNT     15
#define CRYPT5_KDF_LG2_COUNT_MAX 24
#define CRYPT_VERSION             0

class CryptData
{
  struct KDF5CacheItem
  {
    SecPassword Pwd;
    byte Salt[SIZE_SALT50];
    byte Key[32];
    uint Lg2Count;
    byte PswCheckValue[SHA256_DIGEST_SIZE];
    byte HashKeyValue[SHA256_DIGEST_SIZE];

    KDF5CacheItem() {Clean();}
    ~KDF5CacheItem() {Clean();}

    void Clean()
    {
      cleandata(Salt,sizeof(Salt));
      cleandata(Key,sizeof(Key));
      cleandata(&Lg2Count,sizeof(Lg2Count));
      cleandata(PswCheckValue,sizeof(PswCheckValue));
      cleandata(HashKeyValue,sizeof(HashKeyValue));
    }
  };

  struct KDF3CacheItem
  {
    SecPassword Pwd;
    byte Salt[SIZE_SALT30];
    byte Key[16];
    byte Init[16];
    bool SaltPresent;

    KDF3CacheItem() {Clean();}
    ~KDF3CacheItem() {Clean();}

    void Clean()
    {
      cleandata(Salt,sizeof(Salt));
      cleandata(Key,sizeof(Key));
      cleandata(Init,sizeof(Init));
      cleandata(&SaltPresent,sizeof(SaltPresent));
    }
  };

  private:
    void SetKey13(const char *Password);
    void Decrypt13(byte *Data,size_t Count);

    void SetKey15(const char *Password);
    void Crypt15(byte *Data,size_t Count);

    void SetKey20(const char *Password);
    void Swap20(byte *Ch1,byte *Ch2);
    void UpdKeys20(byte *Buf);
    void EncryptBlock20(byte *Buf);
    void DecryptBlock20(byte *Buf);

    void SetKey30(bool Encrypt,SecPassword *Password,const wchar *PwdW,const byte *Salt);
    bool SetKey50(bool Encrypt,SecPassword *Password,const wchar *PwdW,const byte *Salt,const byte *InitV,uint Lg2Cnt,byte *HashKey,byte *PswCheck);

    KDF3CacheItem KDF3Cache[4];
    uint KDF3CachePos;

    KDF5CacheItem KDF5Cache[4];
    uint KDF5CachePos;

    CRYPT_METHOD Method;

    Rijndael rin;

    uint CRCTab[256];

    byte SubstTable20[256];
    uint Key20[4];

    byte Key13[3];
    ushort Key15[4];
  public:
    CryptData();
    bool SetCryptKeys(bool Encrypt,CRYPT_METHOD Method,SecPassword *Password,
         const byte *Salt,const byte *InitV,uint Lg2Cnt,
         byte *HashKey,byte *PswCheck);
    void SetCmt13Encryption();
    void EncryptBlock(byte *Buf,size_t Size);
    void DecryptBlock(byte *Buf,size_t Size);
    static void SetSalt(byte *Salt,size_t SaltSize);
};

class CheckPassword
{
  public:
    enum CONFIDENCE {CONFIDENCE_HIGH,CONFIDENCE_MEDIUM,CONFIDENCE_LOW};
    virtual CONFIDENCE GetConfidence()=0;
    virtual bool Check(SecPassword *Password)=0;
};

class RarCheckPassword:public CheckPassword
{
  private:
    CryptData *Crypt;
    uint Lg2Count;
    byte Salt[SIZE_SALT50];
    byte InitV[SIZE_INITV];
    byte PswCheck[SIZE_PSWCHECK];
  public:
    RarCheckPassword()
    {
      Crypt=NULL;
    }
    ~RarCheckPassword()
    {
      delete Crypt;
    }
    void Set(byte *Salt,byte *InitV,uint Lg2Count,byte *PswCheck)
    {
      if (Crypt==NULL)
        Crypt=new CryptData;
      memcpy(this->Salt,Salt,sizeof(this->Salt));
      memcpy(this->InitV,InitV,sizeof(this->InitV));
      this->Lg2Count=Lg2Count;
      memcpy(this->PswCheck,PswCheck,sizeof(this->PswCheck));
    }
    bool IsSet() {return Crypt!=NULL;}

    CONFIDENCE GetConfidence() {return CONFIDENCE_HIGH;}

    bool Check(SecPassword *Password)
    {
      byte PswCheck[SIZE_PSWCHECK];
      Crypt->SetCryptKeys(false,CRYPT_RAR50,Password,Salt,InitV,Lg2Count,NULL,PswCheck);
      return memcmp(PswCheck,this->PswCheck,sizeof(this->PswCheck))==0;
    }
};

void GetRnd(byte *RndBuf,size_t BufSize);

void hmac_sha256(const byte *Key,size_t KeyLength,const byte *Data,
                 size_t DataLength,byte *ResDigest);
void pbkdf2(const byte *pass, size_t pass_len, const byte *salt,
            size_t salt_len,byte *key, byte *Value1, byte *Value2,
            uint rounds);

void ConvertHashToMAC(HashValue *Value,byte *Key);

#endif

#ifndef _RAR_HEADERS5_
#define _RAR_HEADERS5_

#define  SIZEOF_MARKHEAD5        8
#define  SIZEOF_SHORTBLOCKHEAD5  7

#define HFL_EXTRA           0x0001

#define HFL_DATA            0x0002

#define HFL_SKIPIFUNKNOWN   0x0004

#define HFL_SPLITBEFORE     0x0008

#define HFL_SPLITAFTER      0x0010

#define HFL_CHILD           0x0020

#define HFL_INHERITED       0x0040

#define MHFL_VOLUME         0x0001
#define MHFL_VOLNUMBER      0x0002
#define MHFL_SOLID          0x0004
#define MHFL_PROTECT        0x0008
#define MHFL_LOCK           0x0010

#define FHFL_DIRECTORY      0x0001
#define FHFL_UTIME          0x0002
#define FHFL_CRC32          0x0004
#define FHFL_UNPUNKNOWN     0x0008

#define EHFL_NEXTVOLUME     0x0001

#define CHFL_CRYPT_PSWCHECK 0x0001

#define FCI_ALGO_BIT0       0x00000001
#define FCI_ALGO_BIT1       0x00000002
#define FCI_ALGO_BIT2       0x00000004
#define FCI_ALGO_BIT3       0x00000008
#define FCI_ALGO_BIT4       0x00000010
#define FCI_ALGO_BIT5       0x00000020
#define FCI_SOLID           0x00000040
#define FCI_METHOD_BIT0     0x00000080
#define FCI_METHOD_BIT1     0x00000100
#define FCI_METHOD_BIT2     0x00000200
#define FCI_DICT_BIT0       0x00000400
#define FCI_DICT_BIT1       0x00000800
#define FCI_DICT_BIT2       0x00001000
#define FCI_DICT_BIT3       0x00002000
#define FCI_DICT_BIT4       0x00004000
#define FCI_DICT_FRACT0     0x00008000
#define FCI_DICT_FRACT1     0x00010000
#define FCI_DICT_FRACT2     0x00020000
#define FCI_DICT_FRACT3     0x00040000
#define FCI_DICT_FRACT4     0x00080000
#define FCI_RAR5_COMPAT     0x00100000

#define MHEXTRA_LOCATOR       0x01
#define MHEXTRA_METADATA      0x02

#define MHEXTRA_LOCATOR_QLIST 0x01
#define MHEXTRA_LOCATOR_RR    0x02

#define MHEXTRA_METADATA_NAME      0x01
#define MHEXTRA_METADATA_CTIME     0x02
#define MHEXTRA_METADATA_UNIXTIME  0x04
#define MHEXTRA_METADATA_UNIX_NS   0x08

#define FHEXTRA_CRYPT         0x01
#define FHEXTRA_HASH          0x02
#define FHEXTRA_HTIME         0x03
#define FHEXTRA_VERSION       0x04
#define FHEXTRA_REDIR         0x05
#define FHEXTRA_UOWNER        0x06
#define FHEXTRA_SUBDATA       0x07

#define FHEXTRA_HASH_BLAKE2    0x00

#define FHEXTRA_HTIME_UNIXTIME 0x01
#define FHEXTRA_HTIME_MTIME    0x02
#define FHEXTRA_HTIME_CTIME    0x04
#define FHEXTRA_HTIME_ATIME    0x08
#define FHEXTRA_HTIME_UNIX_NS  0x10

#define FHEXTRA_CRYPT_PSWCHECK 0x01
#define FHEXTRA_CRYPT_HASHMAC  0x02

#define FHEXTRA_REDIR_DIR      0x01

#define FHEXTRA_UOWNER_UNAME   0x01
#define FHEXTRA_UOWNER_GNAME   0x02
#define FHEXTRA_UOWNER_NUMUID  0x04
#define FHEXTRA_UOWNER_NUMGID  0x08

#endif

#ifndef _RAR_HEADERS_
#define _RAR_HEADERS_

#define  SIZEOF_MARKHEAD3        7
#define  SIZEOF_MAINHEAD14       7
#define  SIZEOF_MAINHEAD3       13
#define  SIZEOF_FILEHEAD14      21
#define  SIZEOF_FILEHEAD3       32
#define  SIZEOF_SHORTBLOCKHEAD   7
#define  SIZEOF_LONGBLOCKHEAD   11
#define  SIZEOF_SUBBLOCKHEAD    14
#define  SIZEOF_COMMHEAD        13
#define  SIZEOF_PROTECTHEAD     26
#define  SIZEOF_STREAMHEAD      26

#define  VER_PACK               29U
#define  VER_PACK5              50U
#define  VER_PACK7              70U
#define  VER_UNPACK             29U
#define  VER_UNPACK5            50U
#define  VER_UNPACK7            70U
#define  VER_UNKNOWN          9999U

#define  MHD_VOLUME         0x0001U

#define  MHD_COMMENT        0x0002U

#define  MHD_LOCK           0x0004U
#define  MHD_SOLID          0x0008U
#define  MHD_PACK_COMMENT   0x0010U
#define  MHD_NEWNUMBERING   0x0010U
#define  MHD_AV             0x0020U
#define  MHD_PROTECT        0x0040U
#define  MHD_PASSWORD       0x0080U
#define  MHD_FIRSTVOLUME    0x0100U

#define  LHD_SPLIT_BEFORE   0x0001U
#define  LHD_SPLIT_AFTER    0x0002U
#define  LHD_PASSWORD       0x0004U

#define  LHD_COMMENT        0x0008U

#define  LHD_SOLID          0x0010U

#define  LHD_WINDOWMASK     0x00e0U
#define  LHD_WINDOW64       0x0000U
#define  LHD_WINDOW128      0x0020U
#define  LHD_WINDOW256      0x0040U
#define  LHD_WINDOW512      0x0060U
#define  LHD_WINDOW1024     0x0080U
#define  LHD_WINDOW2048     0x00a0U
#define  LHD_WINDOW4096     0x00c0U
#define  LHD_DIRECTORY      0x00e0U

#define  LHD_LARGE          0x0100U
#define  LHD_UNICODE        0x0200U
#define  LHD_SALT           0x0400U
#define  LHD_VERSION        0x0800U
#define  LHD_EXTTIME        0x1000U

#define  SKIP_IF_UNKNOWN    0x4000U
#define  LONG_BLOCK         0x8000U

#define  EARC_NEXT_VOLUME   0x0001U
#define  EARC_DATACRC       0x0002U
#define  EARC_REVSPACE      0x0004U
#define  EARC_VOLNUMBER     0x0008U

enum HEADER_TYPE {

  HEAD_MARK=0x00, HEAD_MAIN=0x01, HEAD_FILE=0x02, HEAD_SERVICE=0x03,
  HEAD_CRYPT=0x04, HEAD_ENDARC=0x05, HEAD_UNKNOWN=0xff,

  HEAD3_MARK=0x72,HEAD3_MAIN=0x73,HEAD3_FILE=0x74,HEAD3_CMT=0x75,
  HEAD3_AV=0x76,HEAD3_OLDSERVICE=0x77,HEAD3_PROTECT=0x78,HEAD3_SIGN=0x79,
  HEAD3_SERVICE=0x7a,HEAD3_ENDARC=0x7b
};

enum { EA_HEAD=0x100,UO_HEAD=0x101,MAC_HEAD=0x102,BEEA_HEAD=0x103,
       NTACL_HEAD=0x104,STREAM_HEAD=0x105 };

enum HOST_SYSTEM {

  HOST5_WINDOWS=0,HOST5_UNIX=1,

  HOST_MSDOS=0,HOST_OS2=1,HOST_WIN32=2,HOST_UNIX=3,HOST_MACOS=4,
  HOST_BEOS=5,HOST_MAX
};

enum HOST_SYSTEM_TYPE {
  HSYS_WINDOWS, HSYS_UNIX, HSYS_UNKNOWN
};

enum FILE_SYSTEM_REDIRECT {
  FSREDIR_NONE=0, FSREDIR_UNIXSYMLINK, FSREDIR_WINSYMLINK, FSREDIR_JUNCTION,
  FSREDIR_HARDLINK, FSREDIR_FILECOPY
};

#define SUBHEAD_TYPE_CMT      L"CMT"
#define SUBHEAD_TYPE_QOPEN    L"QO"
#define SUBHEAD_TYPE_ACL      L"ACL"
#define SUBHEAD_TYPE_STREAM   L"STM"
#define SUBHEAD_TYPE_UOWNER   L"UOW"
#define SUBHEAD_TYPE_AV       L"AV"
#define SUBHEAD_TYPE_RR       L"RR"
#define SUBHEAD_TYPE_OS2EA    L"EA2"

#define SUBHEAD_FLAGS_INHERITED    0x80000000

#define SUBHEAD_FLAGS_CMT_UNICODE  0x00000001

struct MarkHeader
{
  byte Mark[8];

  uint HeadSize;
};

struct BaseBlock
{
  uint HeadCRC;
  HEADER_TYPE HeaderType;
  uint Flags;
  uint HeadSize;

  bool SkipIfUnknown;

  void Reset()
  {
    SkipIfUnknown=false;
  }

  void SetBaseBlock(BaseBlock &Src)
  {
    *this=Src;
  }
};

struct BlockHeader:BaseBlock
{
  uint DataSize;
};

struct MainHeader:BaseBlock
{
  ushort HighPosAV;
  uint PosAV;
  bool CommentInHeader;
  bool PackComment;
  bool Locator;
  uint64 QOpenOffset;
  uint64 QOpenMaxSize;
  uint64 RROffset;
  uint64 RRMaxSize;
  size_t MetaNameMaxSize;
  std::wstring OrigName;
  RarTime OrigTime;

  void Reset();
};

struct FileHeader:BlockHeader
{
  byte HostOS;
  uint UnpVer;
  byte Method;
  union {
    uint FileAttr;
    uint SubFlags;
  };
  std::wstring FileName;

  std::vector<byte> SubData;

  RarTime mtime;
  RarTime ctime;
  RarTime atime;

  int64 PackSize;
  int64 UnpSize;
  int64 MaxSize;

  HashValue FileHash;

  uint FileFlags;

  bool SplitBefore;
  bool SplitAfter;

  bool UnknownUnpSize;

  bool Encrypted;
  CRYPT_METHOD CryptMethod;
  bool SaltSet;
  byte Salt[SIZE_SALT50];
  byte InitV[SIZE_INITV];
  bool UsePswCheck;
  byte PswCheck[SIZE_PSWCHECK];

  bool UseHashKey;

  byte HashKey[SHA256_DIGEST_SIZE];

  uint Lg2Count;

  bool Solid;
  bool Dir;
  bool CommentInHeader;
  bool Version;
  uint64 WinSize;
  bool Inherited;

  bool LargeFile;

  bool SubBlock;

  HOST_SYSTEM_TYPE HSType;

  FILE_SYSTEM_REDIRECT RedirType;
  std::wstring RedirName;
  bool DirTarget;

  bool UnixOwnerSet,UnixOwnerNumeric,UnixGroupNumeric;
  char UnixOwnerName[256],UnixGroupName[256];
#ifdef _UNIX
  uid_t UnixOwnerID;
  gid_t UnixGroupID;
#else
  uint UnixOwnerID;
  uint UnixGroupID;
#endif

  void Reset(size_t SubDataSize=0);

  bool CmpName(const wchar *Name)
  {
    return FileName==Name;
  }

};

struct EndArcHeader:BaseBlock
{

  uint ArcDataCRC;

  uint VolNumber;

  bool NextVolume;
  bool DataCRC;
  bool RevSpace;
  bool StoreVolNumber;
  void Reset()
  {
    BaseBlock::Reset();
    NextVolume=false;
    DataCRC=false;
    RevSpace=false;
    StoreVolNumber=false;
  }
};

struct CryptHeader:BaseBlock
{
  bool UsePswCheck;
  uint Lg2Count;
  byte Salt[SIZE_SALT50];
  byte PswCheck[SIZE_PSWCHECK];
};

struct SubBlockHeader:BlockHeader
{
  ushort SubType;
  byte Level;
};

struct CommentHeader:BaseBlock
{
  ushort UnpSize;
  byte UnpVer;
  byte Method;
  ushort CommCRC;
};

struct ProtectHeader:BlockHeader
{
  byte Version;
  ushort RecSectors;
  uint TotalBlocks;
  byte Mark[8];
};

struct EAHeader:SubBlockHeader
{
  uint UnpSize;
  byte UnpVer;
  byte Method;
  uint EACRC;
};

struct StreamHeader:SubBlockHeader
{
  uint UnpSize;
  byte UnpVer;
  byte Method;
  uint StreamCRC;
  ushort StreamNameSize;
  std::string StreamName;
};

#endif

#ifndef _RAR_PATHFN_
#define _RAR_PATHFN_

wchar* PointToName(const wchar *Path);
std::wstring PointToName(const std::wstring &Path);
size_t GetNamePos(const std::wstring &Path);
wchar* PointToLastChar(const wchar *Path);
wchar GetLastChar(const std::wstring &Path);
size_t ConvertPath(const std::wstring *SrcPath,std::wstring *DestPath);
void SetName(std::wstring &FullName,const std::wstring &Name);
void SetExt(std::wstring &Name,std::wstring NewExt);
void RemoveExt(std::wstring &Name);
void SetSFXExt(std::wstring &SFXName);
wchar *GetExt(const wchar *Name);
std::wstring GetExt(const std::wstring &Name);
std::wstring::size_type GetExtPos(const std::wstring &Name);
bool CmpExt(const std::wstring &Name,const std::wstring &Ext);
bool IsWildcard(const std::wstring &Str);
bool IsPathDiv(int Ch);
bool IsDriveDiv(int Ch);
bool IsDriveLetter(const std::wstring &Path);
int GetPathDisk(const std::wstring &Path);
void AddEndSlash(std::wstring &Path);
void MakeName(const std::wstring &Path,const std::wstring &Name,std::wstring &Pathname);
void GetPathWithSep(const std::wstring &FullName,std::wstring &Path);
void RemoveNameFromPath(std::wstring &Path);
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool GetAppDataPath(std::wstring &Path,bool Create);
void GetRarDataPath(std::wstring &Path,bool Create);
#endif
#ifdef _WIN_ALL
bool SHGetPathStrFromIDList(PCIDLIST_ABSOLUTE pidl,std::wstring &Path);
#endif
#ifndef SFX_MODULE
bool EnumConfigPaths(uint Number,std::wstring &Path,bool Create);
void GetConfigName(const std::wstring &Name,std::wstring &FullName,bool CheckExist,bool Create);
#endif
size_t GetVolNumPos(const std::wstring &ArcName);
void NextVolumeName(std::wstring &ArcName,bool OldNumbering);
bool IsNameUsable(const std::wstring &Name);
void MakeNameUsable(std::wstring &Name,bool Extended);

void UnixSlashToDos(const char *SrcName,char *DestName,size_t MaxLength);
void UnixSlashToDos(const wchar *SrcName,wchar *DestName,size_t MaxLength);
void UnixSlashToDos(const std::string &SrcName,std::string &DestName);
void UnixSlashToDos(const std::wstring &SrcName,std::wstring &DestName);
void DosSlashToUnix(const char *SrcName,char *DestName,size_t MaxLength);
void DosSlashToUnix(const wchar *SrcName,wchar *DestName,size_t MaxLength);
void DosSlashToUnix(const std::string &SrcName,std::string &DestName);
void DosSlashToUnix(const std::wstring &SrcName,std::wstring &DestName);

inline void SlashToNative(const char *SrcName,char *DestName,size_t MaxLength)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName,MaxLength);
#else
  DosSlashToUnix(SrcName,DestName,MaxLength);
#endif
}

inline void SlashToNative(const std::string &SrcName,std::string &DestName)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName);
#else
  DosSlashToUnix(SrcName,DestName);
#endif
}

inline void SlashToNative(const wchar *SrcName,wchar *DestName,size_t MaxLength)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName,MaxLength);
#else
  DosSlashToUnix(SrcName,DestName,MaxLength);
#endif
}

inline void SlashToNative(const std::wstring &SrcName,std::wstring &DestName)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName);
#else
  DosSlashToUnix(SrcName,DestName);
#endif
}

void ConvertNameToFull(const std::wstring &Src,std::wstring &Dest);
bool IsFullPath(const std::wstring &Path);
bool IsFullRootPath(const std::wstring &Path);
void GetPathRoot(const std::wstring &Path,std::wstring &Root);
int ParseVersionFileName(std::wstring &Name,bool Truncate);
size_t VolNameToFirstName(const std::wstring &VolName,std::wstring &FirstName,bool NewNumbering);

#ifndef SFX_MODULE
void GenerateArchiveName(std::wstring &ArcName,const std::wstring &GenerateMask,bool Archiving);
#endif

#ifdef _WIN_ALL
bool GetWinLongPath(const std::wstring &Src,std::wstring &Dest);
void ConvertToPrecomposed(std::wstring &Name);
void MakeNameCompatible(std::wstring &Name);
#endif

#ifdef _WIN_ALL
std::wstring GetModuleFileStr();
std::wstring GetProgramFile(const std::wstring &Name);
#endif

#if defined(_WIN_ALL)
bool SetCurDir(const std::wstring &Dir);
#endif

#ifdef _WIN_ALL
bool GetCurDir(std::wstring &Dir);
#endif

#endif

#ifndef _RAR_STRFN_
#define _RAR_STRFN_

const char* NullToEmpty(const char *Str);
const wchar* NullToEmpty(const wchar *Str);
void OemToExt(const std::string &Src,std::string &Dest);

enum ACTW_ENCODING { ACTW_DEFAULT, ACTW_OEM, ACTW_UTF8};
void ArcCharToWide(const char *Src,std::wstring &Dest,ACTW_ENCODING Encoding);

int stricomp(const char *s1,const char *s2);
int strnicomp(const char *s1,const char *s2,size_t n);
wchar* RemoveEOL(wchar *Str);
void RemoveEOL(std::wstring &Str);
wchar* RemoveLF(wchar *Str);
void RemoveLF(std::wstring &Str);

void strncpyz(char *dest, const char *src, size_t maxlen);
void wcsncpyz(wchar *dest, const wchar *src, size_t maxlen);
void strncatz(char* dest, const char* src, size_t maxlen);
void wcsncatz(wchar* dest, const wchar* src, size_t maxlen);

#if defined(SFX_MODULE)
unsigned char etoupper(unsigned char c);
#endif
wchar etoupperw(wchar c);

bool IsDigit(int ch);
bool IsSpace(int ch);
bool IsAlpha(int ch);

void BinToHex(const byte *Bin,size_t BinSize,std::wstring &Hex);

#ifndef SFX_MODULE
uint GetDigits(uint Number);
#endif

bool LowAscii(const std::string &Str);
bool LowAscii(const std::wstring &Str);

int wcsicompc(const wchar *s1,const wchar *s2);
int wcsicompc(const std::wstring &s1,const std::wstring &s2);
int wcsnicompc(const wchar *s1,const wchar *s2,size_t n);
int wcsnicompc(const std::wstring &s1,const std::wstring &s2,size_t n);

void itoa(int64 n,char *Str,size_t MaxSize);
void itoa(int64 n,wchar *Str,size_t MaxSize);
void fmtitoa(int64 n,wchar *Str,size_t MaxSize);
std::wstring GetWide(const char *Src);
bool GetCmdParam(const std::wstring &CmdLine,std::wstring::size_type &Pos,std::wstring &Param);
#ifndef RARDLL
void PrintfPrepareFmt(const wchar *Org,std::wstring &Cvt);
std::wstring wstrprintf(const wchar *fmt,...);
std::wstring vwstrprintf(const wchar *fmt,va_list arglist);
#endif

#ifdef _WIN_ALL
bool ExpandEnvironmentStr(std::wstring &Str);
#endif

void TruncateAtZero(std::wstring &Str);
void ReplaceEsc(std::wstring &Str);

#endif

#ifdef _WIN_ALL
#ifndef _RAR_ISNT_
#define _RAR_ISNT_

enum WINNT_VERSION {
  WNT_NONE=0,WNT_NT351=0x0333,WNT_NT4=0x0400,WNT_W2000=0x0500,
  WNT_WXP=0x0501,WNT_W2003=0x0502,WNT_VISTA=0x0600,WNT_W7=0x0601,
  WNT_W8=0x0602,WNT_W81=0x0603,WNT_W10=0x0a00
};

DWORD WinNT();

bool IsWindows11OrGreater();

#endif

#endif
#ifdef PROPAGATE_MOTW
#ifndef _RAR_MOTW_
#define _RAR_MOTW_

class MarkOfTheWeb
{
  private:
    const size_t MOTW_STREAM_MAX_SIZE=1024;
    const wchar* MOTW_STREAM_NAME=L":Zone.Identifier";

    int ParseZoneIdStream(std::string &Stream);

    std::string ZoneIdStream;
    int ZoneIdValue;
    bool AllFields;
  public:
    MarkOfTheWeb();
    void Clear();
    void ReadZoneIdStream(const std::wstring &FileName,bool AllFields);
    void CreateZoneIdStream(const std::wstring &Name,StringList &MotwList);
    bool IsNameConflicting(const std::wstring &StreamName);
    bool IsFileStreamMoreSecure(std::string &FileStream);
};

#endif

#endif
#ifndef _RAR_FILE_
#define _RAR_FILE_

#define FILE_USE_OPEN

#ifdef _WIN_ALL
  typedef HANDLE FileHandle;
  #define FILE_BAD_HANDLE INVALID_HANDLE_VALUE
#elif defined(FILE_USE_OPEN)
  typedef off_t FileHandle;
  #define FILE_BAD_HANDLE -1
#else
  typedef FILE* FileHandle;
  #define FILE_BAD_HANDLE NULL
#endif

enum FILE_HANDLETYPE {FILE_HANDLENORMAL,FILE_HANDLESTD};

enum FILE_ERRORTYPE {FILE_SUCCESS,FILE_NOTFOUND,FILE_READERROR};

enum FILE_MODE_FLAGS {

  FMF_READ=0,

  FMF_UPDATE=1,

  FMF_WRITE=2,

  FMF_OPENSHARED=4,

  FMF_OPENEXCLUSIVE=8,

  FMF_SHAREREAD=16,

  FMF_STANDARDNAMES=32,

  FMF_UNDEFINED=256
};

enum FILE_READ_ERROR_MODE {
  FREM_ASK,
  FREM_TRUNCATE,
  FREM_IGNORE
};

class File
{
  private:
    FileHandle hFile;
    bool LastWrite;
    FILE_HANDLETYPE HandleType;

    bool LineInput;

    bool SkipClose;
    FILE_READ_ERROR_MODE ReadErrorMode;
    bool NewFile;
    bool AllowDelete;
    bool AllowExceptions;
#ifdef _WIN_ALL
    bool NoSequentialRead;
    uint CreateMode;
#endif
    bool PreserveAtime;
    bool TruncatedAfterReadError;

    int64 CurFilePos;
  protected:
    bool OpenShared;
  public:
    std::wstring FileName;

    FILE_ERRORTYPE ErrorType;
  public:
    File();
    virtual ~File();
    void operator = (File &SrcFile);

    virtual bool Open(const std::wstring &Name,uint Mode=FMF_READ);
    void TOpen(const std::wstring &Name);
    bool WOpen(const std::wstring &Name);
    bool Create(const std::wstring &Name,uint Mode=FMF_UPDATE|FMF_SHAREREAD);
    void TCreate(const std::wstring &Name,uint Mode=FMF_UPDATE|FMF_SHAREREAD);
    bool WCreate(const std::wstring &Name,uint Mode=FMF_UPDATE|FMF_SHAREREAD);
    virtual bool Close();
    bool Delete();
    bool Rename(const std::wstring &NewName);
    bool Write(const void *Data,size_t Size);
    virtual int Read(void *Data,size_t Size);
    int DirectRead(void *Data,size_t Size);
    virtual void Seek(int64 Offset,int Method);
    bool RawSeek(int64 Offset,int Method);
    virtual int64 Tell();
    void Prealloc(int64 Size);
    byte GetByte();
    void PutByte(byte Byte);
    bool Truncate();
    void Flush();
    void SetOpenFileTime(RarTime *ftm,RarTime *ftc=NULL,RarTime *fta=NULL);
    void SetCloseFileTime(RarTime *ftm,RarTime *fta=NULL);
    static void SetCloseFileTimeByName(const std::wstring &Name,RarTime *ftm,RarTime *fta);
#ifdef _UNIX
    static void StatToRarTime(struct stat &st,RarTime *ftm,RarTime *ftc,RarTime *fta);
#endif
    void GetOpenFileTime(RarTime *ftm,RarTime *ftc=NULL,RarTime *fta=NULL);
    virtual bool IsOpened() {return hFile!=FILE_BAD_HANDLE;}
    virtual int64 FileLength();
    void SetHandleType(FILE_HANDLETYPE Type) {HandleType=Type;}
    void SetLineInputMode(bool Mode) {LineInput=Mode;}
    FILE_HANDLETYPE GetHandleType() {return HandleType;}
    bool IsSeekable() {return HandleType!=FILE_HANDLESTD;}
    bool IsDevice();
    static bool RemoveCreated();
    FileHandle GetHandle() {return hFile;}
    void SetHandle(FileHandle Handle) {Close();hFile=Handle;}
    void SetReadErrorMode(FILE_READ_ERROR_MODE Mode) {ReadErrorMode=Mode;}
    int64 Copy(File &Dest,int64 Length=INT64NDF);
    void SetAllowDelete(bool Allow) {AllowDelete=Allow;}
    void SetExceptions(bool Allow) {AllowExceptions=Allow;}
    void SetPreserveAtime(bool Preserve) {PreserveAtime=Preserve;}
    bool IsTruncatedAfterReadError() {return TruncatedAfterReadError;}
#ifdef _UNIX
    int GetFD()
    {
#ifdef FILE_USE_OPEN
      return hFile;
#else
      return fileno(hFile);
#endif
    }
#endif
    static size_t CopyBufferSize()
    {

      return 0x400000;
    }
};

#endif

#ifndef _RAR_CRC_
#define _RAR_CRC_

void InitCRC32(uint *CRCTab);

uint CRC32(uint StartCRC,const void *Addr,size_t Size);

#ifndef SFX_MODULE
ushort Checksum14(ushort StartCRC,const void *Addr,size_t Size);
#endif

#endif

#ifndef _RAR_FILEFN_
#define _RAR_FILEFN_

enum MKDIR_CODE {MKDIR_SUCCESS,MKDIR_ERROR,MKDIR_BADPATH};

MKDIR_CODE MakeDir(const std::wstring &Name,bool SetAttr,uint Attr);
bool CreateDir(const std::wstring &Name);
bool CreatePath(const std::wstring &Path,bool SkipLastName,bool Silent);

void SetDirTime(const std::wstring &Name,RarTime *ftm,RarTime *ftc,RarTime *fta);

bool IsRemovable(const std::wstring &Name);

#ifndef SFX_MODULE
int64 GetFreeDisk(const std::wstring &Name);
#endif

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
bool IsFAT(const std::wstring &Root);
#endif

bool FileExist(const std::wstring &Name);
bool WildFileExist(const std::wstring &Name);
bool IsDir(uint Attr);
bool IsUnreadable(uint Attr);
bool IsLink(uint Attr);
void SetSFXMode(const std::wstring &FileName);
void EraseDiskContents(const std::wstring &FileName);
bool IsDeleteAllowed(uint FileAttr);
void PrepareToDelete(const std::wstring &Name);
uint GetFileAttr(const std::wstring &Name);
bool SetFileAttr(const std::wstring &Name,uint Attr);
bool MkTemp(std::wstring &Name,const wchar *Ext);

enum CALCFSUM_FLAGS {CALCFSUM_SHOWTEXT=1,CALCFSUM_SHOWPERCENT=2,CALCFSUM_SHOWPROGRESS=4,CALCFSUM_CURPOS=8};

void CalcFileSum(File *SrcFile,uint *CRC32,byte *Blake2,uint Threads,int64 Size=INT64NDF,uint Flags=0);

bool RenameFile(const std::wstring &SrcName,const std::wstring &DestName);
bool DelFile(const std::wstring &Name);
bool DelDir(const std::wstring &Name);

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool SetFileCompression(const std::wstring &Name,bool State);
bool SetFileCompression(HANDLE hFile,bool State);
void ResetFileCache(const std::wstring &Name);
#endif

bool LinksToDirs(const std::wstring &SrcName,const std::wstring &SkipPart,std::wstring &LastChecked);

#endif

#ifndef _RAR_FILESTR_
#define _RAR_FILESTR_

bool ReadTextFile(
  const std::wstring &Name,
  StringList *List,
  bool Config,
  bool AbortOnError=false,
  RAR_CHARSET SrcCharset=RCH_DEFAULT,
  bool Unquote=false,
  bool SkipComments=false,
  bool ExpandEnvStr=false
);

RAR_CHARSET DetectTextEncoding(const byte *Data,size_t DataSize);

#endif

#ifndef _RAR_FINDDATA_
#define _RAR_FINDDATA_

enum FINDDATA_FLAGS {
  FDDF_SECONDDIR=1
};

struct FindData
{
  std::wstring Name;
  uint64 Size;
  uint FileAttr;
  bool IsDir;
  bool IsLink;
  RarTime mtime;
  RarTime ctime;
  RarTime atime;
#ifdef _WIN_ALL
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
#endif
  uint Flags;
  bool Error;
};

class FindFile
{
  private:
#ifdef _WIN_ALL
    static HANDLE Win32Find(HANDLE hFind,const std::wstring &Mask,FindData *fd);
#endif

    std::wstring FindMask;
    bool FirstCall;
#ifdef _WIN_ALL
    HANDLE hFind;
#else
    DIR *dirp;
#endif
  public:
    FindFile();
    ~FindFile();
    void SetMask(const std::wstring &Mask);
    bool Next(FindData *fd,bool GetSymLink=false);
    static bool FastFind(const std::wstring &FindMask,FindData *fd,bool GetSymLink=false);
};

#endif

#ifndef _RAR_SCANTREE_
#define _RAR_SCANTREE_

enum SCAN_DIRS
{
  SCAN_SKIPDIRS,
  SCAN_GETDIRS,
  SCAN_GETDIRSTWICE,
  SCAN_GETCURDIRS
};

enum SCAN_CODE { SCAN_SUCCESS,SCAN_DONE,SCAN_ERROR,SCAN_NEXT };

class CommandData;

class ScanTree
{
  private:
    static constexpr size_t MAXSCANDEPTH = MAXPATHSIZE/2;

    bool ExpandFolderMask();
    bool GetFilteredMask();
    bool GetNextMask();
    SCAN_CODE FindProc(FindData *FD);
    void ScanError(bool &Error);

    std::vector<FindFile *> FindStack;
    int Depth;

    int SetAllMaskDepth;

    StringList *FileMasks;
    RECURSE_MODE Recurse;
    bool GetLinks;
    SCAN_DIRS GetDirs;
    uint Errors;

    bool ScanEntireDisk;

    std::wstring CurMask;
    std::wstring OrigCurMask;

    StringList ExpandedFolderList;

    StringList FilterList;

    StringList *ErrDirList;
    std::vector<uint> *ErrDirSpecPathLength;

    bool FolderWildcards;

    bool SearchAllInRoot;
    size_t SpecPathLength;

    std::wstring ErrArcName;

    CommandData *Cmd;
  public:
    ScanTree(StringList *FileMasks,RECURSE_MODE Recurse,bool GetLinks,SCAN_DIRS GetDirs);
    ~ScanTree();
    SCAN_CODE GetNext(FindData *FindData);
    size_t GetSpecPathLength() {return SpecPathLength;}
    uint GetErrors() {return Errors;};
    void SetErrArcName(const std::wstring &Name) {ErrArcName=Name;}
    void SetCommandData(CommandData *Cmd) {ScanTree::Cmd=Cmd;}
    void SetErrDirList(StringList *List,std::vector<uint> *Lengths)
    {
      ErrDirList=List;
      ErrDirSpecPathLength=Lengths;
    }
};

#endif

#ifndef _RAR_GETBITS_
#define _RAR_GETBITS_

class BitInput
{
  public:
    enum BufferSize {MAX_SIZE=0x8000};

    int InAddr;
    int InBit;

    bool ExternalBuffer;
  public:
    BitInput(bool AllocBuffer);
    ~BitInput();

    byte *InBuf;

    void InitBitInput()
    {
      InAddr=InBit=0;
    }

    void addbits(uint Bits)
    {
      Bits+=InBit;
      InAddr+=Bits>>3;
      InBit=Bits&7;
    }

    uint getbits()
    {
#if defined(LITTLE_ENDIAN) && defined(ALLOW_MISALIGNED)
      uint32 BitField=RawGetBE4(InBuf+InAddr);
      BitField >>= (16-InBit);
#else
      uint BitField=(uint)InBuf[InAddr] << 16;
      BitField|=(uint)InBuf[InAddr+1] << 8;
      BitField|=(uint)InBuf[InAddr+2];
      BitField >>= (8-InBit);
#endif
      return BitField & 0xffff;
    }

    uint getbits32()
    {
      uint BitField=RawGetBE4(InBuf+InAddr);
      BitField <<= InBit;
      BitField|=(uint)InBuf[InAddr+4] >> (8-InBit);
      return BitField & 0xffffffff;
    }

    uint64 getbits64()
    {
      uint64 BitField=RawGetBE8(InBuf+InAddr);
      BitField <<= InBit;
      BitField|=(uint)InBuf[InAddr+8] >> (8-InBit);
      return BitField;
    }

    void faddbits(uint Bits);
    uint fgetbits();

    bool Overflow(uint IncPtr)
    {
      return InAddr+IncPtr>=MAX_SIZE;
    }

    void SetExternalBuffer(byte *Buf);
};
#endif

#ifndef _RAR_DATAIO_
#define _RAR_DATAIO_

class Archive;
class CmdAdd;
class Unpack;
class ArcFileSearch;

class ComprDataIO
{
  private:
    void ShowUnpRead(int64 ArcPos,int64 ArcSize);
    void ShowUnpWrite();

    bool UnpackFromMemory;
    size_t UnpackFromMemorySize;
    byte *UnpackFromMemoryAddr;

    bool UnpackToMemory;
    size_t UnpackToMemorySize;
    byte *UnpackToMemoryAddr;

    size_t UnpWrSize;
    byte *UnpWrAddr;

    int64 UnpPackedSize;
    int64 UnpPackedLeft;

    bool ShowProgress;
    bool TestMode;
    bool SkipUnpCRC;
    bool NoFileHeader;

    File *SrcFile;
    File *DestFile;

    CmdAdd *Command;

    FileHeader *SubHead;
    int64 *SubHeadPos;

#ifndef RAR_NOCRYPT
    CryptData *Crypt;
    CryptData *Decrypt;
#endif

    int LastPercent;

    wchar CurrentCommand;

  public:
    ComprDataIO();
    ~ComprDataIO();
    void Init();
    int UnpRead(byte *Addr,size_t Count);
    void UnpWrite(byte *Addr,size_t Count);
    void EnableShowProgress(bool Show) {ShowProgress=Show;}
    void GetUnpackedData(byte **Data,size_t *Size);
    void SetPackedSizeToRead(int64 Size) {UnpPackedSize=UnpPackedLeft=Size;}
    void SetTestMode(bool Mode) {TestMode=Mode;}
    void SetSkipUnpCRC(bool Skip) {SkipUnpCRC=Skip;}
    void SetNoFileHeader(bool Mode) {NoFileHeader=Mode;}
    void SetFiles(File *SrcFile,File *DestFile);
    void SetCommand(CmdAdd *Cmd) {Command=Cmd;}
    void SetSubHeader(FileHeader *hd,int64 *Pos) {SubHead=hd;SubHeadPos=Pos;}
    bool SetEncryption(bool Encrypt,CRYPT_METHOD Method,SecPassword *Password,
         const byte *Salt,const byte *InitV,uint Lg2Cnt,byte *HashKey,byte *PswCheck);
    void SetCmt13Encryption();
    void SetUnpackToMemory(byte *Addr,uint Size);
    void SetCurrentCommand(wchar Cmd) {CurrentCommand=Cmd;}
    void AdjustTotalArcSize(Archive *Arc);

    bool PackVolume;
    bool UnpVolume;
    bool NextVolumeMissing;
    int64 CurPackRead,CurPackWrite,CurUnpRead,CurUnpWrite;

    int64 ProcessedArcSize;

    int64 LastArcSize;

    int64 TotalArcSize;

    DataHash PackedDataHash;
    DataHash PackHash;
    DataHash UnpHash;

    bool Encryption;
    bool Decryption;
};

#endif

#ifdef USE_QOPEN
#ifndef _RAR_QOPEN_
#define _RAR_QOPEN_

struct QuickOpenItem
{
  byte *Header;
  size_t HeaderSize;
  uint64 ArcPos;
  QuickOpenItem *Next;
};

class Archive;
class RawRead;

class QuickOpen
{
  private:
    void Close();

    uint ReadBuffer();
    bool ReadRaw(RawRead &Raw);
    bool ReadNext();

    Archive *Arc;
    bool WriteMode;

    QuickOpenItem *ListStart;
    QuickOpenItem *ListEnd;

    byte *Buf;
    static const size_t MaxBufSize=0x10000;
    size_t CurBufSize;
#ifndef RAR_NOCRYPT
    CryptData Crypt;
#endif

    bool Loaded;
    uint64 QOHeaderPos;
    uint64 RawDataStart;
    uint64 RawDataSize;
    uint64 RawDataPos;
    size_t ReadBufSize;
    size_t ReadBufPos;
    std::vector<byte> LastReadHeader;
    uint64 LastReadHeaderPos;
    uint64 SeekPos;
    bool UnsyncSeekPos;
  public:
    QuickOpen();
    ~QuickOpen();
    void Init(Archive *Arc,bool WriteMode);
    void Load(uint64 BlockPos);
    void Unload() { Loaded=false; }
    bool Read(void *Data,size_t Size,size_t &Result);
    bool Seek(int64 Offset,int Method);
    bool Tell(int64 *Pos);
};

#endif

#endif
#ifndef _RAR_ARCHIVE_
#define _RAR_ARCHIVE_

class PPack;
class RawRead;
class RawWrite;

enum NOMODIFY_FLAGS
{
  NMDF_ALLOWLOCK=1,NMDF_ALLOWANYVOLUME=2,NMDF_ALLOWFIRSTVOLUME=4
};

enum RARFORMAT {RARFMT_NONE,RARFMT14,RARFMT15,RARFMT50,RARFMT_FUTURE};

enum ADDSUBDATA_FLAGS
{
  ASDF_SPLIT          = 1,
  ASDF_COMPRESS       = 2,
  ASDF_CRYPT          = 4,
  ASDF_CRYPTIFHEADERS = 8
};

#define MAX_HEADER_SIZE_RAR5 0x200000

class Archive:public File
{
  private:
    void UpdateLatestTime(FileHeader *CurBlock);
    void ConvertNameCase(std::wstring &Name);
    void ConvertFileHeader(FileHeader *hd);
    size_t ReadHeader14();
    size_t ReadHeader15();
    size_t ReadHeader50();
    void ProcessExtra50(RawRead *Raw,size_t ExtraSize,const BaseBlock *bb);
    void RequestArcPassword(RarCheckPassword *SelPwd);
    void UnexpEndArcMsg();
    void BrokenHeaderMsg();
    void UnkEncVerMsg(const std::wstring &Name,const std::wstring &Info);
    bool DoGetComment(std::wstring &CmtData);
    bool ReadCommentData(std::wstring &CmtData);

#if !defined(RAR_NOCRYPT)
    CryptData HeadersCrypt;
#endif
    ComprDataIO SubDataIO;
    bool DummyCmd;
    CommandData *Cmd;

    int RecoveryPercent;

    RarTime LatestTime;
    int LastReadBlock;
    HEADER_TYPE CurHeaderType;

    bool SilentOpen;
#ifdef USE_QOPEN
    QuickOpen QOpen;
    bool ProhibitQOpen;
#endif
  public:
    Archive(CommandData *InitCmd=nullptr);
    ~Archive();
    static RARFORMAT IsSignature(const byte *D,size_t Size);
    bool IsArchive(bool EnableBroken);
    size_t SearchBlock(HEADER_TYPE HeaderType);
    size_t SearchSubBlock(const wchar *Type);
    size_t SearchRR();
    int GetRecoveryPercent() {return RecoveryPercent;}
    size_t ReadHeader();
    void CheckArc(bool EnableBroken);
    void CheckOpen(const std::wstring &Name);
    bool WCheckOpen(const std::wstring &Name);
    bool GetComment(std::wstring &CmtData);
    void ViewComment();
    void SetLatestTime(RarTime *NewTime);
    void SeekToNext();
    bool CheckAccess();
    bool IsArcDir();
    void ConvertAttributes();
    void VolSubtractHeaderSize(size_t SubSize);
    uint FullHeaderSize(size_t Size);
    int64 GetStartPos();
    void AddSubData(const byte *SrcData,uint64 DataSize,File *SrcFile,
         const wchar *Name,uint Flags);
    bool ReadSubData(std::vector<byte> *UnpData,File *DestFile,bool TestMode);
    HEADER_TYPE GetHeaderType() {return CurHeaderType;}
    CommandData* GetCommandData() {return Cmd;}
    void SetSilentOpen(bool Mode) {SilentOpen=Mode;}
#ifdef USE_QOPEN
    bool Open(const std::wstring &Name,uint Mode=FMF_READ) override;
    int Read(void *Data,size_t Size) override;
    void Seek(int64 Offset,int Method) override;
    int64 Tell() override;
    void QOpenUnload() {QOpen.Unload();}
    void SetProhibitQOpen(bool Mode) {ProhibitQOpen=Mode;}
#endif
    static uint64 GetWinSize(uint64 Size,uint &Flags);

    using File::Open;

    BaseBlock ShortBlock;
    MarkHeader MarkHead;
    MainHeader MainHead;
    CryptHeader CryptHead;
    FileHeader FileHead;
    EndArcHeader EndArcHead;
    SubBlockHeader SubBlockHead;
    FileHeader SubHead;
    CommentHeader CommHead;
    ProtectHeader ProtectHead;
    EAHeader EAHead;
    StreamHeader StreamHead;

    int64 CurBlockPos;
    int64 NextBlockPos;

    RARFORMAT Format;
    bool Solid;
    bool Volume;
    bool MainComment;
    bool Locked;
    bool Signed;
    bool FirstVolume;
    bool NewNumbering;
    bool Protected;
    bool Encrypted;
    size_t SFXSize;
    bool BrokenHeader;
    bool FailedHeaderDecryption;

#if !defined(RAR_NOCRYPT)
    byte ArcSalt[SIZE_SALT50];
#endif

    bool Splitting;

    uint VolNumber;
    int64 VolWrite;

    uint64 AddingFilesSize;

    uint64 AddingHeadersSize;

    bool NewArchive;

    std::wstring FirstVolumeName;
#ifdef PROPAGATE_MOTW
    MarkOfTheWeb Motw;
#endif
};

#endif

#ifndef _RAR_MATCH_
#define _RAR_MATCH_

enum {
   MATCH_NAMES,

   MATCH_SUBPATHONLY,

   MATCH_EXACT,

   MATCH_ALLWILD,

   MATCH_EXACTPATH,

   MATCH_SUBPATH,

   MATCH_WILDSUBPATH

};

#define MATCH_MODEMASK           0x0000ffff
#define MATCH_FORCECASESENSITIVE 0x80000000

bool CmpName(const wchar *Wildcard,const wchar *Name,uint CmpMode);

inline bool CmpName(const std::wstring &Wildcard,const std::wstring &Name,uint CmpMode)
{
  return CmpName(Wildcard.c_str(),Name.c_str(),CmpMode);
}

#endif

#ifndef _RAR_CMDDATA_
#define _RAR_CMDDATA_

#if defined(_WIN_ALL) && !defined(SFX_MODULE)

#define CUSTOM_CMDLINE_PARSER
#endif

#define DefaultStoreList L"7z;arj;bz2;cab;gz;jpeg;jpg;lha;lz;lzh;mp3;rar;taz;tbz;tbz2;tgz;txz;xz;z;zip;zipx;zst;tzst"

enum RAR_CMD_LIST_MODE {RCLM_AUTO,RCLM_REJECT_LISTS,RCLM_ACCEPT_LISTS};

enum IS_PROCESS_FILE_FLAGS {IPFF_EXCLUDE_PARENT=1};

class CommandData:public RAROptions
{
  private:
    void ProcessSwitch(const wchar *Switch);
    void BadSwitch(const wchar *Switch);
    uint GetExclAttr(const wchar *Str,bool &Dir);
#if !defined(SFX_MODULE)
    void SetTimeFilters(const wchar *Mod,bool Before,bool Age);
    void SetStoreTimeMode(const wchar *S);
#endif
    int64 GetVolSize(const wchar *S,uint DefMultiplier);

    bool FileLists;
    bool NoMoreSwitches;
    RAR_CMD_LIST_MODE ListMode;
    bool BareOutput;
  public:
    CommandData();
    void Init();

    void ParseCommandLine(bool Preprocess,int argc, char *argv[]);
    void ParseArg(const wchar *ArgW);
    void ParseDone();
    void ParseEnvVar();
    void ReadConfig();
    void PreprocessArg(const wchar *Arg);
    void ProcessSwitchesString(const std::wstring &Str);
    void OutTitle();
    void OutHelp(RAR_EXIT ExitCode);
    bool IsSwitch(int Ch);
    bool ExclCheck(const std::wstring &CheckName,bool Dir,bool CheckFullPath,bool CheckInclList);
    static bool CheckArgs(StringList *Args,bool Dir,const std::wstring &CheckName,bool CheckFullPath,int MatchMode);
    bool ExclDirByAttr(uint FileAttr);
    bool TimeCheck(RarTime &ftm,RarTime &ftc,RarTime &fta);
    bool SizeCheck(int64 Size);
    bool AnyFiltersActive();
    int IsProcessFile(FileHeader &FileHead,bool *ExactMatch,int MatchType,
                      bool Flags,std::wstring *MatchedArg);
    void ProcessCommand();
    void AddArcName(const std::wstring &Name);
    bool GetArcName(wchar *Name,int MaxSize);
    bool GetArcName(std::wstring &Name);

#ifndef SFX_MODULE
    void ReportWrongSwitches(RARFORMAT Format);
#endif

    void GetBriefMaskList(const std::wstring &Masks,StringList &Args);

    std::wstring Command;
    std::wstring ArcName;
    std::wstring ExtrPath;
    std::wstring TempPath;
    std::wstring SFXModule;
    std::wstring CommentFile;
    std::wstring ArcPath;
    std::wstring ExclArcPath;
    std::wstring LogName;
    std::wstring EmailTo;

    std::wstring UseStdin;

    StringList FileArgs;
    StringList ExclArgs;
    StringList InclArgs;
    StringList ArcNames;
    StringList StoreArgs;
#ifdef PROPAGATE_MOTW
    StringList MotwList;
#endif

    SecPassword Password;

    std::vector<int64> NextVolSizes;

#ifdef RARDLL
    std::wstring DllDestName;
#endif
};

#endif

#ifndef _RAR_UI_
#define _RAR_UI_

enum UIMESSAGE_CODE {
  UIERROR_SYSERRMSG, UIERROR_GENERALERRMSG, UIERROR_INCERRCOUNT,
  UIERROR_CHECKSUM, UIERROR_CHECKSUMENC, UIERROR_CHECKSUMPACKED,
  UIERROR_BADPSW, UIERROR_MEMORY, UIERROR_FILEOPEN, UIERROR_FILECREATE,
  UIERROR_FILECLOSE, UIERROR_FILESEEK, UIERROR_FILEREAD, UIERROR_FILEWRITE,
  UIERROR_FILEDELETE, UIERROR_RECYCLEFAILED, UIERROR_FILERENAME,
  UIERROR_FILEATTR, UIERROR_FILECOPY, UIERROR_FILECOPYHINT,
  UIERROR_DIRCREATE, UIERROR_SLINKCREATE, UIERROR_HLINKCREATE,
  UIERROR_NOLINKTARGET, UIERROR_NEEDADMIN, UIERROR_ARCBROKEN,
  UIERROR_HEADERBROKEN, UIERROR_MHEADERBROKEN, UIERROR_FHEADERBROKEN,
  UIERROR_SUBHEADERBROKEN, UIERROR_SUBHEADERUNKNOWN,
  UIERROR_SUBHEADERDATABROKEN, UIERROR_RRDAMAGED, UIERROR_UNKNOWNMETHOD,
  UIERROR_UNKNOWNENCMETHOD, UIERROR_RENAMING, UIERROR_NEWERRAR,
  UIERROR_NOTSFX, UIERROR_OLDTOSFX,UIERROR_WRONGSFXVER,
  UIERROR_HEADENCMISMATCH, UIERROR_DICTOUTMEM,UIERROR_EXTRDICTOUTMEM,
  UIERROR_USESMALLERDICT, UIERROR_MODIFYUNKNOWN, UIERROR_MODIFYOLD,
  UIERROR_MODIFYLOCKED, UIERROR_MODIFYVOLUME, UIERROR_NOTVOLUME,
  UIERROR_NOTFIRSTVOLUME, UIERROR_RECVOLLIMIT, UIERROR_RECVOLDIFFSETS,
  UIERROR_RECVOLALLEXIST, UIERROR_RECVOLFOUND, UIERROR_RECONSTRUCTING,
  UIERROR_RECVOLCANNOTFIX, UIERROR_OPFAILED, UIERROR_UNEXPEOF,
  UIERROR_TRUNCSERVICE, UIERROR_BADARCHIVE, UIERROR_CMTBROKEN,
  UIERROR_INVALIDNAME, UIERROR_NEWRARFORMAT, UIERROR_NOTSUPPORTED,
  UIERROR_ENCRNOTSUPPORTED, UIERROR_RARZIPONLY, UIERROR_REPAIROLDFORMAT,
  UIERROR_NOFILESREPAIRED, UIERROR_NOFILESTOADD, UIERROR_NOFILESTODELETE,
  UIERROR_NOFILESTOEXTRACT, UIERROR_MISSINGVOL, UIERROR_NEEDPREVVOL,
  UIERROR_UNKNOWNEXTRA, UIERROR_CORRUPTEXTRA, UIERROR_NTFSREQUIRED,
  UIERROR_ZIPVOLSFX, UIERROR_FILERO, UIERROR_TOOLARGESFX, UIERROR_NOZIPSFX,
  UIERROR_NEEEDSFX64, UIERROR_EMAIL, UIERROR_ACLGET, UIERROR_ACLBROKEN,
  UIERROR_ACLUNKNOWN, UIERROR_ACLSET, UIERROR_STREAMBROKEN,
  UIERROR_STREAMUNKNOWN, UIERROR_INCOMPATSWITCH, UIERROR_PATHTOOLONG,
  UIERROR_DIRSCAN, UIERROR_UOWNERGET, UIERROR_UOWNERBROKEN,
  UIERROR_UOWNERGETOWNERID, UIERROR_UOWNERGETGROUPID, UIERROR_UOWNERSET,
  UIERROR_ULINKREAD, UIERROR_ULINKEXIST, UIERROR_OPENPRESERVEATIME,
  UIERROR_READERRTRUNCATED, UIERROR_READERRCOUNT, UIERROR_DIRNAMEEXISTS,
  UIERROR_TRUNCPSW, UIERROR_ADJUSTVALUE, UIERROR_SKIPUNSAFELINK,

  UIMSG_FIRST,
  UIMSG_STRING, UIMSG_BUILD, UIMSG_RRSEARCH, UIMSG_ANALYZEFILEDATA,
  UIMSG_RRFOUND, UIMSG_RRNOTFOUND, UIMSG_RRDAMAGED, UIMSG_BLOCKSRECOVERED,
  UIMSG_COPYINGDATA, UIMSG_AREADAMAGED, UIMSG_SECTORDAMAGED,
  UIMSG_SECTORRECOVERED, UIMSG_SECTORNOTRECOVERED, UIMSG_FOUND,
  UIMSG_CORRECTINGNAME, UIMSG_BADARCHIVE, UIMSG_CREATING, UIMSG_RENAMING,
  UIMSG_RECVOLCALCCHECKSUM, UIMSG_RECVOLFOUND, UIMSG_RECVOLMISSING,
  UIMSG_MISSINGVOL, UIMSG_RECONSTRUCTING, UIMSG_CHECKSUM, UIMSG_FAT32SIZE,
  UIMSG_SKIPENCARC, UIMSG_FILERENAME,

  UIWAIT_FIRST,
  UIWAIT_DISKFULLNEXT, UIWAIT_FCREATEERROR, UIWAIT_BADPSW,

  UIEVENT_FIRST,
  UIEVENT_SEARCHDUPFILESSTART, UIEVENT_SEARCHDUPFILESEND,
  UIEVENT_CLEARATTRSTART, UIEVENT_CLEARATTRFILE,
  UIEVENT_DELADDEDSTART, UIEVENT_DELADDEDFILE, UIEVENT_FILESFOUND,
  UIEVENT_ERASEDISK, UIEVENT_FILESUMSTART, UIEVENT_FILESUMPROGRESS,
  UIEVENT_FILESUMEND, UIEVENT_PROTECTSTART, UIEVENT_PROTECTEND,
  UIEVENT_TESTADDEDSTART, UIEVENT_TESTADDEDEND, UIEVENT_RRTESTINGSTART,
  UIEVENT_RRTESTINGEND, UIEVENT_NEWARCHIVE, UIEVENT_NEWREVFILE
};

enum UIASKREP_FLAGS {
  UIASKREP_F_NORENAME=1,UIASKREP_F_EXCHSRCDEST=2,UIASKREP_F_SHOWNAMEONLY=4
};

enum UIASKREP_RESULT {
  UIASKREP_R_REPLACE,UIASKREP_R_SKIP,UIASKREP_R_REPLACEALL,UIASKREP_R_SKIPALL,
  UIASKREP_R_RENAME,UIASKREP_R_RENAMEAUTO,UIASKREP_R_CANCEL,UIASKREP_R_UNUSED
};

UIASKREP_RESULT uiAskReplace(std::wstring &Name,int64 FileSize,RarTime *FileTime,uint Flags);
UIASKREP_RESULT uiAskReplaceEx(CommandData *Cmd,std::wstring &Name,int64 FileSize,RarTime *FileTime,uint Flags);

void uiInit(SOUND_NOTIFY_MODE Sound);

void uiStartArchiveExtract(bool Extract,const std::wstring &ArcName);
bool uiStartFileExtract(const std::wstring &FileName,bool Extract,bool Test,bool Skip);
void uiExtractProgress(int64 CurFileSize,int64 TotalFileSize,int64 CurSize,int64 TotalSize);
void uiProcessProgress(const char *Command,int64 CurSize,int64 TotalSize);

enum UIPASSWORD_TYPE
{
  UIPASSWORD_GLOBAL,
  UIPASSWORD_FILE,
  UIPASSWORD_ARCHIVE,
};

bool uiGetPassword(UIPASSWORD_TYPE Type,const std::wstring &FileName,SecPassword *Password,CheckPassword *CheckPwd);
bool uiIsGlobalPasswordSet();

enum UIALARM_TYPE {UIALARM_ERROR, UIALARM_INFO, UIALARM_QUESTION};
void uiAlarm(UIALARM_TYPE Type);

void uiEolAfterMsg();

bool uiAskNextVolume(std::wstring &VolName);
#if !defined(SILENT) && !defined(SFX_MODULE)
void uiAskRepeatRead(const std::wstring &FileName,bool &Ignore,bool &All,bool &Retry,bool &Quit);
#endif
bool uiAskRepeatWrite(const std::wstring &FileName,bool DiskFull);

bool uiDictLimit(CommandData *Cmd,const std::wstring &FileName,uint64 DictSize,uint64 MaxDictSize);

#ifndef SFX_MODULE
const wchar *uiGetMonthName(uint Month);
#endif

class uiMsgStore
{
  private:
    static const size_t MAX_MSG = 8;
    const wchar *Str[MAX_MSG];
    uint Num[MAX_MSG];
    uint StrSize,NumSize;
    UIMESSAGE_CODE Code;
  public:
    uiMsgStore(UIMESSAGE_CODE Code)
    {

      for (uint I=0;I<ASIZE(Str);I++)
        Str[I]=L"";
      memset(Num,0,sizeof(Num));

      NumSize=StrSize=0;
      this->Code=Code;
    }
    uiMsgStore& operator << (const wchar *s)
    {
      if (StrSize<MAX_MSG)
        Str[StrSize++]=s;
      return *this;
    }
    uiMsgStore& operator << (const std::wstring &s)
    {
      if (StrSize<MAX_MSG)
        Str[StrSize++]=s.c_str();
      return *this;
    }
    uiMsgStore& operator << (uint n)
    {
      if (NumSize<MAX_MSG)
        Num[NumSize++]=n;
      return *this;
    }

    void Msg();
};

inline void uiMsgBase(uiMsgStore &Store)
{

}

template<class T1,class... TN> void uiMsgBase(uiMsgStore &Store,T1&& a1,TN&&... aN)
{

  Store<<a1;
  uiMsgBase(Store,aN...);
}

template<class... TN> void uiMsg(UIMESSAGE_CODE Code,TN&&... aN)
{
  uiMsgStore Store(Code);
  uiMsgBase(Store,aN...);
  Store.Msg();
}

#endif

#ifndef _RAR_FILECREATE_
#define _RAR_FILECREATE_

bool FileCreate(CommandData *Cmd,File *NewFile,std::wstring &Name,
                bool *UserReject,int64 FileSize=INT64NDF,
                RarTime *FileTime=NULL,bool WriteOnly=false);

#if defined(_WIN_ALL)
bool UpdateExistingShortName(const std::wstring &Name);
#endif

#endif

#ifndef _RAR_CONSIO_
#define _RAR_CONSIO_

void InitConsole();
void SetConsoleMsgStream(MESSAGE_TYPE MsgStream);
void SetConsoleRedirectCharset(RAR_CHARSET RedirectCharset);
void ProhibitConsoleInput();
void OutComment(const std::wstring &Comment);
bool IsConsoleOutputPresent();

#ifndef SILENT
bool GetConsolePassword(UIPASSWORD_TYPE Type,const std::wstring &FileName,SecPassword *Password);
#endif

#ifdef SILENT
  inline void mprintf(const wchar *fmt,...) {}
  inline void eprintf(const wchar *fmt,...) {}
  inline void Alarm() {}
  inline int Ask(const wchar *AskStr) {return 0;}
  inline void getwstr(std::wstring &str) {}
#else
  void mprintf(const wchar *fmt,...);
  void eprintf(const wchar *fmt,...);
  void Alarm();
  int Ask(const wchar *AskStr);
  void getwstr(std::wstring &str);
#endif

#endif

#ifndef _RAR_SYSTEM_
#define _RAR_SYSTEM_

#ifdef _WIN_ALL
#ifndef BELOW_NORMAL_PRIORITY_CLASS
#define BELOW_NORMAL_PRIORITY_CLASS     0x00004000
#define ABOVE_NORMAL_PRIORITY_CLASS     0x00008000
#endif
#ifndef PROCESS_MODE_BACKGROUND_BEGIN
#define PROCESS_MODE_BACKGROUND_BEGIN   0x00100000
#define PROCESS_MODE_BACKGROUND_END     0x00200000
#endif
#ifndef SHTDN_REASON_MAJOR_APPLICATION
#define SHTDN_REASON_MAJOR_APPLICATION  0x00040000
#define SHTDN_REASON_FLAG_PLANNED       0x80000000
#define SHTDN_REASON_MINOR_MAINTENANCE  0x00000001
#endif
#endif

void InitSystemOptions(int SleepTime);
void SetPriority(int Priority);
clock_t MonoClock();
void Wait();
bool EmailFile(const std::wstring &FileName,std::wstring MailToW);
#ifdef _WIN_ALL
bool SetPrivilege(LPCTSTR PrivName);
#endif
void Shutdown(POWER_MODE Mode);
bool ShutdownCheckAnother(bool Open);

#ifdef _WIN_ALL
HMODULE WINAPI LoadSysLibrary(const wchar *Name);
bool IsUserAdmin();
#endif

#ifdef USE_SSE
enum SSE_VERSION {SSE_NONE,SSE_SSE,SSE_SSE2,SSE_SSSE3,SSE_SSE41,SSE_AVX2};
SSE_VERSION GetSSEVersion();
extern SSE_VERSION _SSE_Version;
#endif

#endif

#ifndef _RAR_LOG_
#define _RAR_LOG_

void InitLogOptions(const std::wstring &LogFileName,RAR_CHARSET CSet);
void CloseLogOptions();

#ifdef SILENT
inline void Log(const wchar *ArcName,const wchar *fmt,...) {}
#else
void Log(const wchar *ArcName,const wchar *fmt,...);
#endif

#endif

#ifndef _RAR_RAWREAD_
#define _RAR_RAWREAD_

class RawRead
{
  private:
    std::vector<byte> Data;
    File *SrcFile;
    size_t DataSize;
    size_t ReadPos;
    CryptData *Crypt;
  public:
    RawRead();
    RawRead(File *SrcFile);
    void Reset();
    size_t Read(size_t Size);
    void Read(byte *SrcData,size_t Size);
    byte   Get1();
    ushort Get2();
    uint   Get4();
    uint64 Get8();
    uint64 GetV();
    uint   GetVSize(size_t Pos);
    size_t GetB(void *Field,size_t Size);
    void GetW(wchar *Field,size_t Size);
    uint GetCRC15(bool ProcessedOnly);
    uint GetCRC50();
    byte* GetDataPtr() {return &Data[0];}
    size_t Size() {return DataSize;}
    size_t PaddedSize() {return Data.size()-DataSize;}
    size_t DataLeft() {return DataSize-ReadPos;}
    size_t GetPos() {return ReadPos;}
    void SetPos(size_t Pos) {ReadPos=Pos;}
    void Skip(size_t Size) {ReadPos+=Size;}
    void Rewind() {SetPos(0);}
    void SetCrypt(CryptData *Crypt) {RawRead::Crypt=Crypt;}
};

uint64 RawGetV(const byte *Data,uint &ReadPos,uint DataSize,bool &Overflow);

#endif

#ifndef _RAR_ENCNAME_
#define _RAR_ENCNAME_

class EncodeFileName
{
  private:
    void AddFlags(byte Value,std::vector<byte> &EncName);

    byte Flags;
    uint FlagBits;
    size_t FlagsPos;
    size_t DestSize;
  public:
    EncodeFileName();
    void Encode(const std::string &Name,const std::wstring &NameW,std::vector<byte> &EncName);
    void Decode(const char *Name,size_t NameSize,const byte *EncName,size_t EncSize,std::wstring &NameW);
};

#endif

#ifndef _RAR_RESOURCE_
#define _RAR_RESOURCE_

#ifdef RARDLL
#define St(x) (L"")
#else
const wchar *St(MSGID StringId);
#endif

#endif

#ifndef _RAR_COMPRESS_
#define _RAR_COMPRESS_

class PackDef
{
  public:

    static const uint MAX_LZ_MATCH = 0x1001;

    static const uint MAX_INC_LZ_MATCH = MAX_LZ_MATCH + 3;

    static const uint MAX3_LZ_MATCH = 0x101;
    static const uint MAX3_INC_LZ_MATCH = MAX3_LZ_MATCH + 3;
    static const uint LOW_DIST_REP_COUNT = 16;

    static const uint NC    = 306;
    static const uint DCB   = 64;
    static const uint DCX   = 80;
    static const uint LDC   = 16;
    static const uint RC    = 44;
    static const uint HUFF_TABLE_SIZEB = NC + DCB + RC + LDC;
    static const uint HUFF_TABLE_SIZEX = NC + DCX + RC + LDC;
    static const uint BC    = 20;

    static const uint NC30  = 299;
    static const uint DC30  = 60;
    static const uint LDC30 = 17;
    static const uint RC30  = 28;
    static const uint BC30  = 20;
    static const uint HUFF_TABLE_SIZE30 = NC30 + DC30 + RC30 + LDC30;

    static const uint NC20  = 298;
    static const uint DC20  = 48;
    static const uint RC20  = 28;
    static const uint BC20  = 19;
    static const uint MC20  = 257;

    static const uint LARGEST_TABLE_SIZE = 306;

};

enum FilterType {

  FILTER_DELTA=0, FILTER_E8, FILTER_E8E9, FILTER_ARM,
  FILTER_AUDIO, FILTER_RGB, FILTER_ITANIUM, FILTER_TEXT,

  FILTER_LONGRANGE,FILTER_EXHAUSTIVE,FILTER_NONE
};

#endif

#ifndef _RAR_VM_
#define _RAR_VM_

#define VM_MEMSIZE                  0x40000
#define VM_MEMMASK           (VM_MEMSIZE-1)

enum VM_StandardFilters {
  VMSF_NONE, VMSF_E8, VMSF_E8E9, VMSF_ITANIUM, VMSF_RGB, VMSF_AUDIO,
  VMSF_DELTA
};

struct VM_PreparedProgram
{
  VM_PreparedProgram()
  {
    FilteredDataSize=0;
    Type=VMSF_NONE;
  }
  VM_StandardFilters Type;
  uint InitR[7];
  byte *FilteredData;
  uint FilteredDataSize;
};

class RarVM
{
  private:
    bool ExecuteStandardFilter(VM_StandardFilters FilterType);
    uint FilterItanium_GetBits(byte *Data,uint BitPos,uint BitCount);
    void FilterItanium_SetBits(byte *Data,uint BitField,uint BitPos,uint BitCount);

    byte *Mem;
    uint R[8];
  public:
    RarVM();
    ~RarVM();
    void Init();
    void Prepare(byte *Code,uint CodeSize,VM_PreparedProgram *Prg);
    void Execute(VM_PreparedProgram *Prg);
    void SetMemory(size_t Pos,byte *Data,size_t DataSize);
    static uint ReadData(BitInput &Inp);
};

#endif

#ifndef _RAR_PPMMODEL_
#define _RAR_PPMMODEL_

class RangeCoder
{
  public:
    void InitDecoder(Unpack *UnpackRead);
    inline int GetCurrentCount();
    inline uint GetCurrentShiftCount(uint SHIFT);
    inline void Decode();
    inline void PutChar(unsigned int c);
    inline byte GetChar();

    uint low, code, range;
    struct SUBRANGE
    {
      uint LowCount, HighCount, scale;
    } SubRange;

    Unpack *UnpackRead;
};

#if !defined(_SUBALLOC_H_)
#define _SUBALLOC_H_

#if defined(__GNUC__) && defined(ALLOW_MISALIGNED)
#define RARPPM_PACK_ATTR __attribute__ ((packed))
#else
#define RARPPM_PACK_ATTR
#endif

#ifdef ALLOW_MISALIGNED
#pragma pack(1)
#endif

struct RARPPM_MEM_BLK
{
  ushort Stamp, NU;
  RARPPM_MEM_BLK* next, * prev;
  void insertAt(RARPPM_MEM_BLK* p)
  {
    next=(prev=p)->next;
    p->next=next->prev=this;
  }
  void remove()
  {
    prev->next=next;
    next->prev=prev;
  }
} RARPPM_PACK_ATTR;

#ifdef ALLOW_MISALIGNED
#ifdef _AIX
#pragma pack(pop)
#else
#pragma pack()
#endif
#endif

class SubAllocator
{
  private:
    static const int N1=4, N2=4, N3=4, N4=(128+3-1*N1-2*N2-3*N3)/4;
    static const int N_INDEXES=N1+N2+N3+N4;

    struct RAR_NODE
    {
      RAR_NODE* next;
    };

    inline void InsertNode(void* p,int indx);
    inline void* RemoveNode(int indx);
    inline uint U2B(int NU);
    inline void SplitBlock(void* pv,int OldIndx,int NewIndx);
    inline void GlueFreeBlocks();
    void* AllocUnitsRare(int indx);
    inline RARPPM_MEM_BLK* MBPtr(RARPPM_MEM_BLK *BasePtr,int Items);

    long SubAllocatorSize;
    byte Indx2Units[N_INDEXES], Units2Indx[128], GlueCount;
    byte *HeapStart,*LoUnit, *HiUnit;
    struct RAR_NODE FreeList[N_INDEXES];
  public:
    SubAllocator();
    ~SubAllocator() {StopSubAllocator();}
    void Clean();
    bool StartSubAllocator(int SASize);
    void StopSubAllocator();
    void  InitSubAllocator();
    inline void* AllocContext();
    inline void* AllocUnits(int NU);
    inline void* ExpandUnits(void* ptr,int OldNU);
    inline void* ShrinkUnits(void* ptr,int OldNU,int NewNU);
    inline void  FreeUnits(void* ptr,int OldNU);
    long GetAllocatedMemory() {return(SubAllocatorSize);}

    byte *pText, *UnitsStart,*HeapEnd,*FakeUnitsStart;
};

#endif

#ifdef ALLOW_MISALIGNED
#pragma pack(1)
#endif

struct RARPPM_DEF
{
  static const int INT_BITS=7, PERIOD_BITS=7, TOT_BITS=INT_BITS+PERIOD_BITS,
    INTERVAL=1 << INT_BITS, BIN_SCALE=1 << TOT_BITS, MAX_FREQ=124;
};

struct RARPPM_SEE2_CONTEXT : RARPPM_DEF
{
  ushort Summ;
  byte Shift, Count;
  void init(int InitVal)
  {
    Summ=InitVal << (Shift=PERIOD_BITS-4);
    Count=4;
  }
  uint getMean()
  {
    short RetVal=GET_SHORT16(Summ) >> Shift;
    Summ -= RetVal;
    return RetVal+(RetVal == 0);
  }
  void update()
  {
    if (Shift < PERIOD_BITS && --Count == 0)
    {
      Summ += Summ;
      Count=3 << Shift++;
    }
  }
};

class ModelPPM;
struct RARPPM_CONTEXT;

struct RARPPM_STATE
{
  byte Symbol;
  byte Freq;
  RARPPM_CONTEXT* Successor;
};

struct RARPPM_CONTEXT : RARPPM_DEF
{
    ushort NumStats;

    struct FreqData
    {
      ushort SummFreq;
      RARPPM_STATE RARPPM_PACK_ATTR * Stats;
    };

    union
    {
      FreqData U;
      RARPPM_STATE OneState;
    };

    RARPPM_CONTEXT* Suffix;
    inline void encodeBinSymbol(ModelPPM *Model,int symbol);
    inline void encodeSymbol1(ModelPPM *Model,int symbol);
    inline void encodeSymbol2(ModelPPM *Model,int symbol);
    inline void decodeBinSymbol(ModelPPM *Model);
    inline bool decodeSymbol1(ModelPPM *Model);
    inline bool decodeSymbol2(ModelPPM *Model);
    inline void update1(ModelPPM *Model,RARPPM_STATE* p);
    inline void update2(ModelPPM *Model,RARPPM_STATE* p);
    void rescale(ModelPPM *Model);
    inline RARPPM_CONTEXT* createChild(ModelPPM *Model,RARPPM_STATE* pStats,RARPPM_STATE& FirstState);
    inline RARPPM_SEE2_CONTEXT* makeEscFreq2(ModelPPM *Model,int Diff);
};

#ifdef ALLOW_MISALIGNED
#ifdef _AIX
#pragma pack(pop)
#else
#pragma pack()
#endif
#endif

class ModelPPM : RARPPM_DEF
{
  private:
    friend struct RARPPM_CONTEXT;

    RARPPM_SEE2_CONTEXT SEE2Cont[25][16], DummySEE2Cont;

    struct RARPPM_CONTEXT *MinContext, *MedContext, *MaxContext;
    RARPPM_STATE* FoundState;
    int NumMasked, InitEsc, OrderFall, MaxOrder, RunLength, InitRL;
    byte CharMask[256], NS2Indx[256], NS2BSIndx[256], HB2Flag[256];
    byte EscCount, PrevSuccess, HiBitsFlag;
    ushort BinSumm[128][64];

    RangeCoder Coder;
    SubAllocator SubAlloc;

    void RestartModelRare();
    void StartModelRare(int MaxOrder);
    inline RARPPM_CONTEXT* CreateSuccessors(bool Skip,RARPPM_STATE* p1);

    inline void UpdateModel();
    inline void ClearMask();
  public:
    ModelPPM();
    void CleanUp();
    bool DecodeInit(Unpack *UnpackRead,int &EscChar);
    int DecodeChar();
};

#endif

#ifndef _RAR_THREADPOOL_
#define _RAR_THREADPOOL_

#ifndef RAR_SMP
const uint MaxPoolThreads=1;
#else

const uint MaxPoolThreads=64;

#ifdef _UNIX
  #include <pthread.h>
  #include <semaphore.h>
#endif

#define     USE_THREADS

#ifdef _UNIX
  #define NATIVE_THREAD_TYPE void*
  typedef void* (*NATIVE_THREAD_PTR)(void *Data);
  typedef pthread_t THREAD_HANDLE;
  typedef pthread_mutex_t CRITSECT_HANDLE;
#else
  #define NATIVE_THREAD_TYPE DWORD WINAPI
  typedef DWORD (WINAPI *NATIVE_THREAD_PTR)(void *Data);
  typedef HANDLE THREAD_HANDLE;
  typedef CRITICAL_SECTION CRITSECT_HANDLE;
#endif

typedef void (*PTHREAD_PROC)(void *Data);
#define THREAD_PROC(fn) void fn(void *Data)

uint GetNumberOfCPU();
uint GetNumberOfThreads();

class ThreadPool
{
  private:
    struct QueueEntry
    {
    	PTHREAD_PROC Proc;
      void *Param;
    };

    void CreateThreads();
    static NATIVE_THREAD_TYPE PoolThread(void *Param);
  	void PoolThreadLoop();
  	bool GetQueuedTask(QueueEntry *Task);

    uint MaxAllowedThreads;
  	THREAD_HANDLE ThreadHandles[MaxPoolThreads];

    uint ThreadsCreatedCount;

    uint ActiveThreads;

  	QueueEntry TaskQueue[MaxPoolThreads];
  	uint QueueTop;
  	uint QueueBottom;

    bool Closing;

#ifdef _WIN_ALL

  	HANDLE QueuedTasksCnt;

    HANDLE NoneActive;

#elif defined(_UNIX)

    uint QueuedTasksCnt;
    pthread_cond_t QueuedTasksCntCond;
    pthread_mutex_t QueuedTasksCntMutex;

    bool AnyActive;
    pthread_cond_t AnyActiveCond;
    pthread_mutex_t AnyActiveMutex;
#endif

  	CRITSECT_HANDLE CritSection;
  public:
    ThreadPool(uint MaxThreads);
    ~ThreadPool();
    void AddTask(PTHREAD_PROC Proc,void *Data);
    void WaitDone();

#ifdef _WIN_ALL
    static int ThreadPriority;
    static void SetPriority(int Priority) {ThreadPriority=Priority;}
#endif
};

#endif

#endif

#ifndef _RAR_LARGEPAGE_
#define _RAR_LARGEPAGE_

class LargePageAlloc
{
  private:
    static constexpr const wchar *LOCKMEM_SWITCH=L"isetup_privilege_lockmem";

    void* new_large(size_t Size);
    bool delete_large(void *Addr);
#ifdef _WIN_ALL
    std::vector<void*> LargeAlloc;
    SIZE_T PageSize;
#endif
    bool UseLargePages;
  public:
    LargePageAlloc();
    void AllowLargePages(bool Allow);
    static bool IsPrivilegeAssigned();
    static bool AssignPrivilege();
    static bool AssignPrivilegeBySid(const std::wstring &Sid);
    static bool AssignConfirmation();

    static bool ProcessSwitch(CommandData *Cmd,const wchar *Switch)
    {
      if (Switch[0]==LOCKMEM_SWITCH[0])
      {
        size_t Length=wcslen(LOCKMEM_SWITCH);
        if (wcsncmp(Switch,LOCKMEM_SWITCH,Length)==0)
        {
          LargePageAlloc::AssignPrivilegeBySid(Switch+Length);
          return true;
        }
      }
      return false;
    }

    template <class T> T* new_l(size_t Size,bool Clear=false)
    {
      T *Allocated=(T*)new_large(Size*sizeof(T));
      if (Allocated==nullptr)
        Allocated=Clear ? new T[Size]{} : new T[Size];
      return Allocated;
    }

    template <class T> void delete_l(T *Addr)
    {
      if (!delete_large(Addr))
        delete[] Addr;
    }
};

#endif

#ifndef _RAR_UNPACK_
#define _RAR_UNPACK_

#define MAX_QUICK_DECODE_BITS       9

#define MAX_UNPACK_FILTERS       8192

#define MAX3_UNPACK_FILTERS      8192

#define MAX3_UNPACK_CHANNELS      1024

#define MAX_FILTER_BLOCK_SIZE 0x400000

#define UNPACK_MAX_WRITE      0x400000

struct DecodeTable:PackDef
{

  uint MaxNum;

  uint DecodeLen[16];

  uint DecodePos[16];

  uint QuickBits;

  byte QuickLen[1<<MAX_QUICK_DECODE_BITS];

  ushort QuickNum[1<<MAX_QUICK_DECODE_BITS];

  ushort DecodeNum[LARGEST_TABLE_SIZE];
};

struct UnpackBlockHeader
{
  int BlockSize;
  int BlockBitSize;
  int BlockStart;
  int HeaderSize;
  bool LastBlockInFile;
  bool TablePresent;
};

struct UnpackBlockTables
{
  DecodeTable LD;
  DecodeTable DD;
  DecodeTable LDD;
  DecodeTable RD;
  DecodeTable BD;
};

#ifdef RAR_SMP
enum UNP_DEC_TYPE {
  UNPDT_LITERAL=0,UNPDT_MATCH,UNPDT_FULLREP,UNPDT_REP,UNPDT_FILTER
};

struct UnpackDecodedItem
{
  byte Type;
  ushort Length;
  union
  {
    size_t Distance;
    byte Literal[8];
  };
};

struct UnpackThreadData
{
  Unpack *UnpackPtr;
  BitInput Inp;
  bool HeaderRead;
  UnpackBlockHeader BlockHeader;
  bool TableRead;
  UnpackBlockTables BlockTables;
  int DataSize;
  bool DamagedData;
  bool LargeBlock;
  bool NoDataLeft;
  bool Incomplete;

  UnpackDecodedItem *Decoded;
  uint DecodedSize;
  uint DecodedAllocated;
  uint ThreadNumber;

  UnpackThreadData()
  :Inp(false)
  {
    Decoded=NULL;
  }
  ~UnpackThreadData()
  {
    if (Decoded!=NULL)
      free(Decoded);
  }
};
#endif

struct UnpackFilter
{

  byte Type;
  byte Channels;
  bool NextWindow;

  size_t BlockStart;
  uint BlockLength;
};

struct UnpackFilter30
{
  unsigned int BlockStart;
  unsigned int BlockLength;
  bool NextWindow;

  unsigned int ParentFilter;

  VM_PreparedProgram Prg;
};

struct AudioVariables
{
  int K1,K2,K3,K4,K5;
  int D1,D2,D3,D4;
  int LastDelta;
  unsigned int Dif[11];
  unsigned int ByteCount;
  int LastChar;
};

class FragmentedWindow
{
  private:
    enum {MAX_MEM_BLOCKS=32};

    void Reset();
    byte *Mem[MAX_MEM_BLOCKS];
    size_t MemSize[MAX_MEM_BLOCKS];
    size_t LastAllocated;
  public:
    FragmentedWindow();
    ~FragmentedWindow();
    void Init(size_t WinSize);
    byte& operator [](size_t Item);
    void CopyString(uint Length,size_t Distance,size_t &UnpPtr,bool FirstWinDone,size_t MaxWinSize);
    void CopyData(byte *Dest,size_t WinPos,size_t Size);
    size_t GetBlockSize(size_t StartPos,size_t RequiredSize);
    size_t GetWinSize() {return LastAllocated;}
};

class Unpack:PackDef
{
  private:

    void Unpack5(bool Solid);
    void Unpack5MT(bool Solid);
    bool UnpReadBuf();
    void UnpWriteBuf();
    byte* ApplyFilter(byte *Data,uint DataSize,UnpackFilter *Flt);
    void UnpWriteArea(size_t StartPtr,size_t EndPtr);
    void UnpWriteData(byte *Data,size_t Size);
    _forceinline uint SlotToLength(BitInput &Inp,uint Slot);
    void UnpInitData50(bool Solid);
    bool ReadBlockHeader(BitInput &Inp,UnpackBlockHeader &Header);
    bool ReadTables(BitInput &Inp,UnpackBlockHeader &Header,UnpackBlockTables &Tables);
    void MakeDecodeTables(byte *LengthTable,DecodeTable *Dec,uint Size);
    _forceinline uint DecodeNumber(BitInput &Inp,DecodeTable *Dec);
    inline void InsertOldDist(size_t Distance);
    void UnpInitData(bool Solid);
    _forceinline void CopyString(uint Length,size_t Distance);
    uint ReadFilterData(BitInput &Inp);
    bool ReadFilter(BitInput &Inp,UnpackFilter &Filter);
    bool AddFilter(UnpackFilter &Filter);
    bool AddFilter();
    void InitFilters();

    ComprDataIO *UnpIO;
    BitInput Inp;

#ifdef RAR_SMP
    void InitMT();
    bool UnpackLargeBlock(UnpackThreadData &D);
    bool ProcessDecoded(UnpackThreadData &D);

    ThreadPool *UnpThreadPool;
    UnpackThreadData *UnpThreadData;
    uint MaxUserThreads;
    byte *ReadBufMT;
#endif

    LargePageAlloc Alloc;

    std::vector<byte> FilterSrcMemory;
    std::vector<byte> FilterDstMemory;

    std::vector<UnpackFilter> Filters;

    size_t OldDist[4],OldDistPtr;
    uint LastLength;

    uint LastDist;

    size_t UnpPtr;

    size_t PrevPtr;
    bool FirstWinDone;

    size_t WrPtr;

    int ReadTop;

    int ReadBorder;

    UnpackBlockHeader BlockHeader;
    UnpackBlockTables BlockTables;

    size_t WriteBorder;

    byte *Window;

    FragmentedWindow FragWindow;
    bool Fragmented;

    int64 DestUnpSize;

    bool Suspended;
    bool UnpSomeRead;
    int64 WrittenFileSize;
    bool FileExtracted;

    void Unpack15(bool Solid);
    void ShortLZ();
    void LongLZ();
    void HuffDecode();
    void GetFlagsBuf();
    void UnpInitData15(bool Solid);
    void InitHuff();
    void CorrHuff(ushort *CharSet,byte *NumToPlace);
    void CopyString15(uint Distance,uint Length);
    uint DecodeNum(uint Num,uint StartPos,uint *DecTab,uint *PosTab);

    ushort ChSet[256],ChSetA[256],ChSetB[256],ChSetC[256];
    byte NToPl[256],NToPlB[256],NToPlC[256];
    uint FlagBuf,AvrPlc,AvrPlcB,AvrLn1,AvrLn2,AvrLn3;
    int Buf60,NumHuf,StMode,LCount,FlagsCnt;
    uint Nhfb,Nlzb,MaxDist3;

    void Unpack20(bool Solid);

    DecodeTable MD[4];

    unsigned char UnpOldTable20[MC20*4];
    bool UnpAudioBlock;
    uint UnpChannels,UnpCurChannel;
    int UnpChannelDelta;
    void CopyString20(uint Length,uint Distance);
    bool ReadTables20();
    void UnpWriteBuf20();
    void UnpInitData20(int Solid);
    void ReadLastTables();
    byte DecodeAudio(int Delta);
    struct AudioVariables AudV[4];

    enum BLOCK_TYPES {BLOCK_LZ,BLOCK_PPM};

    void UnpInitData30(bool Solid);
    void Unpack29(bool Solid);
    void InitFilters30(bool Solid);
    bool ReadEndOfBlock();
    bool ReadVMCode();
    bool ReadVMCodePPM();
    bool AddVMCode(uint FirstByte,byte *Code,uint CodeSize);
    int SafePPMDecodeChar();
    bool ReadTables30();
    bool UnpReadBuf30();
    void UnpWriteBuf30();
    void ExecuteCode(VM_PreparedProgram *Prg);

    int PrevLowDist,LowDistRepCount;

    ModelPPM PPM;
    int PPMEscChar;

    byte UnpOldTable[HUFF_TABLE_SIZE30];
    int UnpBlockType;

    bool TablesRead2,TablesRead3,TablesRead5;

    RarVM VM;

    BitInput VMCodeInp;

    std::vector<UnpackFilter30 *> Filters30;

    std::vector<UnpackFilter30 *> PrgStack;

    std::vector<int> OldFilterLengths;

    int LastFilter;

  public:
    Unpack(ComprDataIO *DataIO);
    ~Unpack();
    void Init(uint64 WinSize,bool Solid);
    void AllowLargePages(bool Allow) {Alloc.AllowLargePages(Allow);}
    void DoUnpack(uint Method,bool Solid);
    bool IsFileExtracted() {return FileExtracted;}
    void SetDestSize(int64 DestSize) {DestUnpSize=DestSize;FileExtracted=false;}
    void SetSuspended(bool Suspended) {Unpack::Suspended=Suspended;}

#ifdef RAR_SMP
    void SetThreads(uint Threads);
    void UnpackDecode(UnpackThreadData &D);
#endif

    uint64 AllocWinSize;
    size_t MaxWinSize;
    size_t MaxWinMask;

    bool ExtraDist;

    byte GetChar()
    {
      if (Inp.InAddr>BitInput::MAX_SIZE-30)
      {
        UnpReadBuf();
        if (Inp.InAddr>=BitInput::MAX_SIZE)
          return 0;
      }
      return Inp.InBuf[Inp.InAddr++];
    }

    inline size_t WrapDown(size_t WinPos)
    {
      return WinPos >= MaxWinSize ? WinPos + MaxWinSize : WinPos;
    }

    inline size_t WrapUp(size_t WinPos)
    {
      return WinPos >= MaxWinSize ? WinPos - MaxWinSize : WinPos;
    }
};

#endif

#ifndef _RAR_EXTINFO_
#define _RAR_EXTINFO_

bool IsRelativeSymlinkSafe(CommandData *Cmd,const std::wstring &SrcName,std::wstring PrepSrcName,const std::wstring &TargetName);
bool ExtractSymlink(CommandData *Cmd,ComprDataIO &DataIO,Archive &Arc,const std::wstring &LinkName,bool &UpLink);
#ifdef _UNIX
void SetUnixOwner(Archive &Arc,const std::wstring &FileName);
#endif

bool ExtractHardlink(CommandData *Cmd,const std::wstring &NameNew,const std::wstring &NameExisting);

std::wstring GetStreamNameNTFS(Archive &Arc);

void SetExtraInfo20(CommandData *Cmd,Archive &Arc,const std::wstring &Name);
void SetExtraInfo(CommandData *Cmd,Archive &Arc,const std::wstring &Name);
void SetFileHeaderExtra(CommandData *Cmd,Archive &Arc,const std::wstring &Name);

#endif

#ifndef _RAR_EXTRACT_
#define _RAR_EXTRACT_

enum EXTRACT_ARC_CODE {EXTRACT_ARC_NEXT,EXTRACT_ARC_REPEAT};

class CmdExtract
{
  private:
    struct ExtractRef
    {
      std::wstring RefName;
      std::wstring TmpName;
      uint64 RefCount;
    };
    std::vector<ExtractRef> RefList;

    struct AnalyzeData
    {
      std::wstring StartName;
      uint64 StartPos;
      std::wstring EndName;
      uint64 EndPos;
    } Analyze;

    bool ArcAnalyzed;

    void FreeAnalyzeData();
    EXTRACT_ARC_CODE ExtractArchive();
    bool ExtractFileCopy(File &New,const std::wstring &ArcName,const std::wstring &RedirName,const std::wstring &NameNew,const std::wstring &NameExisting,int64 UnpSize);
    void ExtrPrepareName(Archive &Arc,const std::wstring &ArcFileName,std::wstring &DestName);
#ifdef RARDLL
    bool ExtrDllGetPassword();
#else
    bool ExtrGetPassword(Archive &Arc,const std::wstring &ArcFileName,RarCheckPassword *CheckPwd);
#endif
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    void ConvertDosPassword(Archive &Arc,SecPassword &DestPwd);
#endif
    void ExtrCreateDir(Archive &Arc,const std::wstring &ArcFileName);
    bool ExtrCreateFile(Archive &Arc,File &CurFile,bool WriteOnly);
    bool CheckUnpVer(Archive &Arc,const std::wstring &ArcFileName);
#ifndef SFX_MODULE
    void AnalyzeArchive(const std::wstring &ArcName,bool Volume,bool NewNumbering);
    void GetFirstVolIfFullSet(const std::wstring &SrcName,bool NewNumbering,std::wstring &DestName);
#endif
    bool CheckWinLimit(Archive &Arc,std::wstring &ArcFileName);

    RarTime StartTime;

    CommandData *Cmd;

    ComprDataIO DataIO;
    Unpack *Unp;
    unsigned long TotalFileCount;

    unsigned long FileCount;
    unsigned long MatchedArgs;
    bool FirstFile;
    bool AllMatchesExact;
    bool ReconstructDone;
    bool UseExactVolName;

    bool AnySolidDataUnpackedWell;

    std::wstring ArcName;

    bool GlobalPassword;
    bool PrevProcessed;
    std::wstring DestFileName;
    bool SuppressNoFilesMessage;

    bool ConvertSymlinkPaths;

    std::wstring LastCheckedSymlink;

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
    bool Fat32,NotFat32;
#endif
  public:
    CmdExtract(CommandData *Cmd);
    ~CmdExtract();
    void DoExtract();
    void ExtractArchiveInit(Archive &Arc);
    bool ExtractCurrentFile(Archive &Arc,size_t HeaderSize,bool &Repeat);
    static void UnstoreFile(ComprDataIO &DataIO,int64 DestUnpSize);
};

#endif

#ifndef _RAR_LIST_
#define _RAR_LIST_

void ListArchive(CommandData *Cmd);

#endif

#ifndef _RAR_RS_
#define _RAR_RS_

#define MAXPAR 255
#define MAXPOL 512

class RSCoder
{
  private:
    void gfInit();
    int gfMult(int a,int b);
    void pnInit();
    void pnMult(int *p1,int *p2,int *r);

    int gfExp[MAXPOL];
    int gfLog[MAXPAR+1];

    int GXPol[MAXPOL*2];

    int ErrorLocs[MAXPAR+1],ErrCount;
    int Dnm[MAXPAR+1];

    int ParSize;
    int ELPol[MAXPOL];
    bool FirstBlockDone;
  public:
    void Init(int ParSize);
    void Encode(byte *Data,int DataSize,byte *DestData);
    bool Decode(byte *Data,int DataSize,int *EraLoc,int EraSize);
};

#endif

#ifndef _RAR_RS16_
#define _RAR_RS16_

class RSCoder16
{
  private:
    static const uint gfSize=65535;
    void gfInit();
    inline uint gfAdd(uint a,uint b);
    inline uint gfMul(uint a,uint b);
    inline uint gfInv(uint a);
    uint *gfExp;
    uint *gfLog;

    void MakeEncoderMatrix();
    void MakeDecoderMatrix();
    void InvertDecoderMatrix();

#ifdef USE_SSE
#if defined(USE_SSE) && defined(__GNUC__)
    __attribute__((target("ssse3")))
#endif
    bool SSE_UpdateECC(uint DataNum, uint ECCNum, const byte *Data, byte *ECC, size_t BlockSize);
#endif

    bool Decoding;
    uint ND;
    uint NR;
    uint NE;
    bool *ValidFlags;
    uint *MX;

    uint *DataLog;
    size_t DataLogSize;

  public:
    RSCoder16();
    ~RSCoder16();

    bool Init(uint DataCount, uint RecCount, bool *ValidityFlags);
#if 0
    void Process(const uint *Data, uint *Out);
#endif
    void UpdateECC(uint DataNum, uint ECCNum, const byte *Data, byte *ECC, size_t BlockSize);
};

#endif

#ifndef _RAR_RECVOL_
#define _RAR_RECVOL_

#define REV5_SIGN      "Rar!\x1aRev"
#define REV5_SIGN_SIZE             8

class RecVolumes3
{
  private:
    File *SrcFile[256];
    std::vector<byte> Buf;

#ifdef RAR_SMP
    ThreadPool *RSThreadPool;
#endif
  public:
    RecVolumes3(CommandData *Cmd,bool TestOnly);
    ~RecVolumes3();
    void Make(CommandData *Cmd,std::wstring ArcName);
    bool Restore(CommandData *Cmd,const std::wstring &Name,bool Silent);
    void Test(CommandData *Cmd,const std::wstring &Name);
};

struct RecVolItem
{
  File *f;
  std::wstring Name;
  uint CRC;
  uint64 FileSize;
  bool New;
  bool Valid;
};

class RecVolumes5;
struct RecRSThreadData
{
  RecVolumes5 *RecRSPtr;
  RSCoder16 *RS;
  bool Encode;
  uint DataNum;
  const byte *Data;
  size_t StartPos;
  size_t Size;
};

class RecVolumes5
{
  private:
    void ProcessRS(CommandData *Cmd,uint DataNum,const byte *Data,uint MaxRead,bool Encode);
    void ProcessRS(CommandData *Cmd,uint MaxRead,bool Encode);
    uint ReadHeader(File *RecFile,bool FirstRev);

    std::vector<RecVolItem> RecItems;

    byte *RealReadBuffer;
    byte *ReadBuffer;

    byte *RealBuf;
    byte *Buf;
    size_t RecBufferSize;

    uint DataCount;
    uint RecCount;
    uint TotalCount;

    bool *ValidFlags;
    uint MissingVolumes;

#ifdef RAR_SMP
    ThreadPool *RecThreadPool;
#endif
    uint MaxUserThreads;
    RecRSThreadData *ThreadData;
  public:
    void ProcessAreaRS(RecRSThreadData *td);
  public:
    RecVolumes5(CommandData *Cmd,bool TestOnly);
    ~RecVolumes5();
    bool Restore(CommandData *Cmd,const std::wstring &Name,bool Silent);
    void Test(CommandData *Cmd,const std::wstring &Name);
};

bool RecVolumesRestore(CommandData *Cmd,const std::wstring &Name,bool Silent);
void RecVolumesTest(CommandData *Cmd,Archive *Arc,const std::wstring &Name);

#endif

#ifndef _RAR_VOLUME_
#define _RAR_VOLUME_

bool MergeArchive(Archive &Arc,ComprDataIO *DataIO,bool ShowFileName,
                  wchar Command);

#endif

#ifndef _RAR_SMALLFN_
#define _RAR_SMALLFN_

int ToPercent(int64 N1,int64 N2);
int ToPercentUnlim(int64 N1,int64 N2);

#endif

#ifndef _RAR_GLOBAL_
#define _RAR_GLOBAL_

#ifdef INCLUDEGLOBAL
  #define EXTVAR
#else
  #define EXTVAR extern
#endif

EXTVAR ErrorHandler ErrHandler;

#endif

#endif

static bool IsAnsiEscComment(const wchar *Data,size_t Size);

bool Archive::GetComment(std::wstring &CmtData)
{
  if (!MainComment)
    return false;
  int64 SavePos=Tell();
  bool Success=DoGetComment(CmtData);
  Seek(SavePos,SEEK_SET);
  return Success;
}

bool Archive::DoGetComment(std::wstring &CmtData)
{
#ifndef SFX_MODULE
  uint CmtLength;
  if (Format==RARFMT14)
  {
    Seek(SFXSize+SIZEOF_MAINHEAD14,SEEK_SET);
    CmtLength=GetByte();
    CmtLength+=(GetByte()<<8);
  }
  else
#endif
  {
    if (MainHead.CommentInHeader)
    {

      Seek(SFXSize+SIZEOF_MARKHEAD3+SIZEOF_MAINHEAD3,SEEK_SET);
      if (!ReadHeader() || GetHeaderType()!=HEAD3_CMT)
        return false;
    }
    else
    {

      Seek(GetStartPos(),SEEK_SET);
      if (SearchSubBlock(SUBHEAD_TYPE_CMT)!=0)
        if (ReadCommentData(CmtData))
          return true;
        else
          uiMsg(UIERROR_CMTBROKEN,FileName);
      return false;
    }
#ifndef SFX_MODULE

    if (BrokenHeader || CommHead.HeadSize<SIZEOF_COMMHEAD)
    {
      uiMsg(UIERROR_CMTBROKEN,FileName);
      return false;
    }
    CmtLength=CommHead.HeadSize-SIZEOF_COMMHEAD;
#endif
  }
#ifndef SFX_MODULE
  if (Format==RARFMT14 && MainHead.PackComment || Format!=RARFMT14 && CommHead.Method!=0x30)
  {
    if (Format!=RARFMT14 && (CommHead.UnpVer < 15 || CommHead.UnpVer > VER_UNPACK || CommHead.Method > 0x35))
      return false;
    ComprDataIO DataIO;
    DataIO.SetTestMode(true);
    uint UnpCmtLength;
    if (Format==RARFMT14)
    {
#ifdef RAR_NOCRYPT
      return false;
#else
      UnpCmtLength=GetByte();
      UnpCmtLength+=(GetByte()<<8);
      if (CmtLength<2)
        return false;
      CmtLength-=2;
      DataIO.SetCmt13Encryption();
      CommHead.UnpVer=15;
#endif
    }
    else
      UnpCmtLength=CommHead.UnpSize;
    DataIO.SetFiles(this,NULL);
    DataIO.EnableShowProgress(false);
    DataIO.SetPackedSizeToRead(CmtLength);
    DataIO.UnpHash.Init(HASH_CRC32,1);
    DataIO.SetNoFileHeader(true);

    Unpack CmtUnpack(&DataIO);
    CmtUnpack.Init(0x10000,false);
    CmtUnpack.SetDestSize(UnpCmtLength);
    CmtUnpack.DoUnpack(CommHead.UnpVer,false);

    if (Format!=RARFMT14 && (DataIO.UnpHash.GetCRC32()&0xffff)!=CommHead.CommCRC)
    {
      uiMsg(UIERROR_CMTBROKEN,FileName);
      return false;
    }
    else
    {
      byte *UnpData;
      size_t UnpDataSize;
      DataIO.GetUnpackedData(&UnpData,&UnpDataSize);
      if (UnpDataSize>0)
      {
#ifdef _WIN_ALL

        OemToCharBuffA((char *)UnpData,(char *)UnpData,(DWORD)UnpDataSize);
#endif
        std::string UnpStr((char*)UnpData,UnpDataSize);
        CharToWide(UnpStr,CmtData);
      }
    }
  }
  else
  {
    if (CmtLength==0)
      return false;
    std::vector<byte> CmtRaw(CmtLength);
    int ReadSize=Read(CmtRaw.data(),CmtLength);
    if (ReadSize>=0 && (uint)ReadSize<CmtLength)
    {
      CmtLength=ReadSize;
      CmtRaw.resize(CmtLength);
    }

    if (Format!=RARFMT14 && CommHead.CommCRC!=(~CRC32(0xffffffff,&CmtRaw[0],CmtLength)&0xffff))
    {
      uiMsg(UIERROR_CMTBROKEN,FileName);
      return false;
    }

    CmtRaw.push_back(0);
#ifdef _WIN_ALL

    OemToCharA((char *)CmtRaw.data(),(char *)CmtRaw.data());
#endif
    CharToWide((const char *)CmtRaw.data(),CmtData);

  }
#endif
  return CmtData.size() > 0;
}

bool Archive::ReadCommentData(std::wstring &CmtData)
{
  std::vector<byte> CmtRaw;
  if (!ReadSubData(&CmtRaw,NULL,false))
    return false;
  size_t CmtSize=CmtRaw.size();
  CmtRaw.push_back(0);

  if (Format==RARFMT50)
    UtfToWide((char *)CmtRaw.data(),CmtData);
  else
    if ((SubHead.SubFlags & SUBHEAD_FLAGS_CMT_UNICODE)!=0)
    {
      CmtData=RawToWide(CmtRaw);
    }
    else
    {
      CharToWide((const char *)CmtRaw.data(),CmtData);
    }

  return true;
}

void Archive::ViewComment()
{
  if (Cmd->DisableComment)
    return;
  std::wstring CmtBuf;
  if (GetComment(CmtBuf))
  {
    size_t CmtSize=CmtBuf.size();
    auto EndPos=CmtBuf.find(0x1A);
    if (EndPos!=std::wstring::npos)
      CmtSize=EndPos;
    mprintf(St(MArcComment));
    mprintf(L":\n");
    OutComment(CmtBuf);
  }
}

Archive::Archive(CommandData *InitCmd)
{
  Cmd=NULL;

  DummyCmd=(InitCmd==NULL);
  Cmd=DummyCmd ? (new CommandData):InitCmd;

  OpenShared=Cmd->OpenShared;
  Format=RARFMT_NONE;
  Solid=false;
  Volume=false;
  MainComment=false;
  Locked=false;
  Signed=false;
  FirstVolume=false;
  NewNumbering=false;
  SFXSize=0;
  LatestTime.Reset();
  Protected=false;
  Encrypted=false;
  FailedHeaderDecryption=false;
  BrokenHeader=false;
  LastReadBlock=0;
  CurHeaderType=HEAD_UNKNOWN;

  CurBlockPos=0;
  NextBlockPos=0;

  RecoveryPercent=-1;

  MainHead.Reset();
  CryptHead={};
  EndArcHead.Reset();

  VolNumber=0;
  VolWrite=0;
  AddingFilesSize=0;
  AddingHeadersSize=0;

  Splitting=false;
  NewArchive=false;

  SilentOpen=false;

#ifdef USE_QOPEN
  ProhibitQOpen=false;
#endif

}

Archive::~Archive()
{
  if (DummyCmd)
    delete Cmd;
}

void Archive::CheckArc(bool EnableBroken)
{
  if (!IsArchive(EnableBroken))
  {

    if (!FailedHeaderDecryption)
      uiMsg(UIERROR_BADARCHIVE,FileName);
    ErrHandler.Exit(RARX_BADARC);
  }
}

#if !defined(SFX_MODULE)
void Archive::CheckOpen(const std::wstring &Name)
{
  TOpen(Name);
  CheckArc(false);
}
#endif

bool Archive::WCheckOpen(const std::wstring &Name)
{
  if (!WOpen(Name))
    return false;
  if (!IsArchive(false))
  {
    uiMsg(UIERROR_BADARCHIVE,FileName);
    Close();
    return false;
  }
  return true;
}

RARFORMAT Archive::IsSignature(const byte *D,size_t Size)
{
  RARFORMAT Type=RARFMT_NONE;
  if (Size>=1 && D[0]==0x52)
#ifndef SFX_MODULE
    if (Size>=4 && D[1]==0x45 && D[2]==0x7e && D[3]==0x5e)
      Type=RARFMT14;
    else
#endif
      if (Size>=7 && D[1]==0x61 && D[2]==0x72 && D[3]==0x21 && D[4]==0x1a && D[5]==0x07)
      {

#ifndef SFX_MODULE
        if (D[6]==0)
          Type=RARFMT15;
        else
#endif
          if (D[6]==1)
            Type=RARFMT50;
          else
            if (D[6]>1 && D[6]<5)
              Type=RARFMT_FUTURE;
      }
  return Type;
}

bool Archive::IsArchive(bool EnableBroken)
{
  Encrypted=false;
  BrokenHeader=false;

#ifndef SFX_MODULE
  if (IsDevice())
  {
    uiMsg(UIERROR_INVALIDNAME,FileName,FileName);
    return false;
  }
#endif
  if (Read(MarkHead.Mark,SIZEOF_MARKHEAD3)!=SIZEOF_MARKHEAD3)
    return false;
  SFXSize=0;

  RARFORMAT Type;
  if ((Type=IsSignature(MarkHead.Mark,SIZEOF_MARKHEAD3))!=RARFMT_NONE)
  {
    Format=Type;
    if (Format==RARFMT14)
      Seek(Tell()-SIZEOF_MARKHEAD3,SEEK_SET);
  }
  else
  {
    std::vector<char> Buffer(MAXSFXSIZE);
    long CurPos=(long)Tell();
    int ReadSize=Read(Buffer.data(),Buffer.size()-16);
    for (int I=0;I<ReadSize;I++)
      if (Buffer[I]==0x52 && (Type=IsSignature((byte *)&Buffer[I],ReadSize-I))!=RARFMT_NONE)
      {
        Format=Type;
        if (Format==RARFMT14 && I>0 && CurPos<28 && ReadSize>31)
        {
          char *D=&Buffer[28-CurPos];
          if (D[0]!=0x52 || D[1]!=0x53 || D[2]!=0x46 || D[3]!=0x58)
            continue;
        }
        SFXSize=CurPos+I;
        Seek(SFXSize,SEEK_SET);
        if (Format==RARFMT15 || Format==RARFMT50)
          Read(MarkHead.Mark,SIZEOF_MARKHEAD3);
        break;
      }
    if (SFXSize==0)
      return false;
  }
  if (Format==RARFMT_FUTURE)
  {
    uiMsg(UIERROR_NEWRARFORMAT,FileName);
    return false;
  }
  if (Format==RARFMT50)
  {
    if (Read(MarkHead.Mark+SIZEOF_MARKHEAD3,1)!=1 || MarkHead.Mark[SIZEOF_MARKHEAD3]!=0)
      return false;
    MarkHead.HeadSize=SIZEOF_MARKHEAD5;
  }
  else
    MarkHead.HeadSize=SIZEOF_MARKHEAD3;

#ifdef RARDLL

  if (Cmd->Callback==NULL)
    SilentOpen=true;
#endif

  bool HeadersLeft;
  bool StartFound=false;

  while ((HeadersLeft=(ReadHeader()!=0))==true)
  {
    SeekToNext();

    HEADER_TYPE Type=GetHeaderType();

    StartFound=Type==HEAD_MAIN || SilentOpen && Type==HEAD_CRYPT;
    if (StartFound)
      break;
  }

  if (FailedHeaderDecryption && !EnableBroken)
    return false;

  if (BrokenHeader || !StartFound)
  {
    if (!FailedHeaderDecryption)
      uiMsg(UIERROR_MHEADERBROKEN,FileName);
    if (!EnableBroken)
      return false;
  }

  MainComment=MainHead.CommentInHeader;

  if (HeadersLeft && (!SilentOpen || !Encrypted) && IsSeekable())
  {
    int64 SavePos=Tell();
    int64 SaveCurBlockPos=CurBlockPos,SaveNextBlockPos=NextBlockPos;
    HEADER_TYPE SaveCurHeaderType=CurHeaderType;

    while (ReadHeader()!=0)
    {
      HEADER_TYPE HeaderType=GetHeaderType();
      if (HeaderType==HEAD_SERVICE)
      {

        FirstVolume=Volume && !SubHead.SplitBefore;
      }
      else
        if (HeaderType==HEAD_FILE)
        {
          FirstVolume=Volume && !FileHead.SplitBefore;
          break;
        }
        else
          if (HeaderType==HEAD_ENDARC)
            break;
      SeekToNext();
    }
    CurBlockPos=SaveCurBlockPos;
    NextBlockPos=SaveNextBlockPos;
    CurHeaderType=SaveCurHeaderType;
    Seek(SavePos,SEEK_SET);
  }
  if (!Volume || FirstVolume)
    FirstVolumeName=FileName;

  return true;
}

void Archive::SeekToNext()
{
  Seek(NextBlockPos,SEEK_SET);
}

uint Archive::FullHeaderSize(size_t Size)
{
  if (Encrypted)
  {
    Size = ALIGN_VALUE(Size, CRYPT_BLOCK_SIZE);
    if (Format == RARFMT50)
      Size += SIZE_INITV;
    else
      Size += SIZE_SALT30;
  }
  return uint(Size);
}

#ifdef USE_QOPEN
bool Archive::Open(const std::wstring &Name,uint Mode)
{

  QOpen.Unload();

  return File::Open(Name,Mode);
}

int Archive::Read(void *Data,size_t Size)
{
  size_t Result;
  if (QOpen.Read(Data,Size,Result))
    return (int)Result;
  return File::Read(Data,Size);
}

void Archive::Seek(int64 Offset,int Method)
{
  if (!QOpen.Seek(Offset,Method))
    File::Seek(Offset,Method);
}

int64 Archive::Tell()
{
  int64 QPos;
  if (QOpen.Tell(&QPos))
    return QPos;
  return File::Tell();
}
#endif

uint64 Archive::GetWinSize(uint64 Size,uint &Flags)
{
  Flags=0;

  if (Size<0x20000 || Size>0x10000000000ULL)
    return 0;
  uint64 Pow2=0x20000;
  for (;2*Pow2<=Size;Pow2*=2)
    Flags+=FCI_DICT_BIT0;
  if (Size==Pow2)
    return Size;

  uint64 Fraction=(Size-Pow2)/(Pow2/32);
  Flags+=(uint)Fraction*FCI_DICT_FRACT0;
  return Pow2+Fraction*(Pow2/32);
}

size_t Archive::ReadHeader()
{

  if (FailedHeaderDecryption)
    return 0;

  CurBlockPos=Tell();

  size_t ReadSize=0;

  switch(Format)
  {
#ifndef SFX_MODULE
    case RARFMT14:
      ReadSize=ReadHeader14();
      break;
    case RARFMT15:
      ReadSize=ReadHeader15();
      break;
#endif
    case RARFMT50:
      ReadSize=ReadHeader50();
      break;
  }

  if (ReadSize>0 && NextBlockPos<=CurBlockPos)
  {
    BrokenHeaderMsg();
    ReadSize=0;
  }

  if (ReadSize==0)
    CurHeaderType=HEAD_UNKNOWN;

  return ReadSize;
}

size_t Archive::SearchBlock(HEADER_TYPE HeaderType)
{
  size_t Size,Count=0;
  while ((Size=ReadHeader())!=0 &&
         (HeaderType==HEAD_ENDARC || GetHeaderType()!=HEAD_ENDARC))
  {
    if ((++Count & 127)==0)
      Wait();
    if (GetHeaderType()==HeaderType)
      return Size;
    SeekToNext();
  }
  return 0;
}

size_t Archive::SearchSubBlock(const wchar *Type)
{
  size_t Size,Count=0;
  while ((Size=ReadHeader())!=0 && GetHeaderType()!=HEAD_ENDARC)
  {
    if ((++Count & 127)==0)
      Wait();
    if (GetHeaderType()==HEAD_SERVICE && SubHead.CmpName(Type))
      return Size;
    SeekToNext();
  }
  return 0;
}

size_t Archive::SearchRR()
{

  if (MainHead.Locator && MainHead.RROffset!=0)
  {
    uint64 CurPos=Tell();
    Seek(MainHead.RROffset,SEEK_SET);
    size_t Size=ReadHeader();
    if (Size!=0 && !BrokenHeader && GetHeaderType()==HEAD_SERVICE && SubHead.CmpName(SUBHEAD_TYPE_RR))
      return Size;
    Seek(CurPos,SEEK_SET);
  }

  return SearchSubBlock(SUBHEAD_TYPE_RR);
}

void Archive::UnexpEndArcMsg()
{
  int64 ArcSize=FileLength();

  if (CurBlockPos!=ArcSize || NextBlockPos!=ArcSize)
  {
    uiMsg(UIERROR_UNEXPEOF,FileName);
    if (CurHeaderType!=HEAD_FILE && CurHeaderType!=HEAD_UNKNOWN)
      uiMsg(UIERROR_TRUNCSERVICE,FileName,SubHead.FileName);

    ErrHandler.SetErrorCode(RARX_WARNING);
  }
}

void Archive::BrokenHeaderMsg()
{
  uiMsg(UIERROR_HEADERBROKEN,FileName);
  BrokenHeader=true;
  ErrHandler.SetErrorCode(RARX_CRC);
}

void Archive::UnkEncVerMsg(const std::wstring &Name,const std::wstring &Info)
{
  uiMsg(UIERROR_UNKNOWNENCMETHOD,FileName,Name,Info);
  ErrHandler.SetErrorCode(RARX_FATAL);
}

inline int64 SafeAdd(int64 v1,int64 v2,int64 f)
{
  return v1>=0 && v2>=0 && v1<=MAX_INT64-v2 ? v1+v2 : f;
}

#ifndef SFX_MODULE
size_t Archive::ReadHeader15()
{
  RawRead Raw(this);

  bool Decrypt=Encrypted && CurBlockPos>(int64)SFXSize+SIZEOF_MARKHEAD3;

  if (Decrypt)
  {
#ifdef RAR_NOCRYPT
    return 0;
#else
    RequestArcPassword(NULL);

    byte Salt[SIZE_SALT30];
    if (Read(Salt,SIZE_SALT30)!=SIZE_SALT30)
    {
      UnexpEndArcMsg();
      return 0;
    }
    HeadersCrypt.SetCryptKeys(false,CRYPT_RAR30,&Cmd->Password,Salt,NULL,0,NULL,NULL);
    Raw.SetCrypt(&HeadersCrypt);
#endif
  }

  Raw.Read(SIZEOF_SHORTBLOCKHEAD);
  if (Raw.Size()==0)
  {
    UnexpEndArcMsg();
    return 0;
  }

  ShortBlock.HeadCRC=Raw.Get2();

  ShortBlock.Reset();

  uint HeaderType=Raw.Get1();
  ShortBlock.Flags=Raw.Get2();
  ShortBlock.SkipIfUnknown=(ShortBlock.Flags & SKIP_IF_UNKNOWN)!=0;
  ShortBlock.HeadSize=Raw.Get2();

  ShortBlock.HeaderType=(HEADER_TYPE)HeaderType;
  if (ShortBlock.HeadSize<SIZEOF_SHORTBLOCKHEAD)
  {
    BrokenHeaderMsg();
    return 0;
  }

  switch(ShortBlock.HeaderType)
  {
    case HEAD3_MAIN:    ShortBlock.HeaderType=HEAD_MAIN;     break;
    case HEAD3_FILE:    ShortBlock.HeaderType=HEAD_FILE;     break;
    case HEAD3_SERVICE: ShortBlock.HeaderType=HEAD_SERVICE;  break;
    case HEAD3_ENDARC:  ShortBlock.HeaderType=HEAD_ENDARC;   break;
  }
  CurHeaderType=ShortBlock.HeaderType;

  if (ShortBlock.HeaderType==HEAD3_CMT)
  {

    Raw.Read(SIZEOF_COMMHEAD-SIZEOF_SHORTBLOCKHEAD);
  }
  else
    if (ShortBlock.HeaderType==HEAD_MAIN && (ShortBlock.Flags & MHD_COMMENT)!=0)
    {

      Raw.Read(SIZEOF_MAINHEAD3-SIZEOF_SHORTBLOCKHEAD);
    }
    else
      Raw.Read(ShortBlock.HeadSize-SIZEOF_SHORTBLOCKHEAD);

  NextBlockPos=CurBlockPos+FullHeaderSize(ShortBlock.HeadSize);

  switch(ShortBlock.HeaderType)
  {
    case HEAD_MAIN:
      MainHead.Reset();
      MainHead.SetBaseBlock(ShortBlock);
      MainHead.HighPosAV=Raw.Get2();
      MainHead.PosAV=Raw.Get4();

      Volume=(MainHead.Flags & MHD_VOLUME)!=0;
      Solid=(MainHead.Flags & MHD_SOLID)!=0;
      Locked=(MainHead.Flags & MHD_LOCK)!=0;
      Protected=(MainHead.Flags & MHD_PROTECT)!=0;
      Encrypted=(MainHead.Flags & MHD_PASSWORD)!=0;
      Signed=MainHead.PosAV!=0 || MainHead.HighPosAV!=0;
      MainHead.CommentInHeader=(MainHead.Flags & MHD_COMMENT)!=0;

      FirstVolume=(MainHead.Flags & MHD_FIRSTVOLUME)!=0;

      NewNumbering=(MainHead.Flags & MHD_NEWNUMBERING)!=0;
      break;
    case HEAD_FILE:
    case HEAD_SERVICE:
      {
        bool FileBlock=ShortBlock.HeaderType==HEAD_FILE;
        FileHeader *hd=FileBlock ? &FileHead:&SubHead;
        hd->Reset();

        hd->SetBaseBlock(ShortBlock);

        hd->SplitBefore=(hd->Flags & LHD_SPLIT_BEFORE)!=0;
        hd->SplitAfter=(hd->Flags & LHD_SPLIT_AFTER)!=0;
        hd->Encrypted=(hd->Flags & LHD_PASSWORD)!=0;
        hd->SaltSet=(hd->Flags & LHD_SALT)!=0;

        hd->Solid=FileBlock && (hd->Flags & LHD_SOLID)!=0;

        hd->SubBlock=!FileBlock && (hd->Flags & LHD_SOLID)!=0;
        hd->Dir=(hd->Flags & LHD_WINDOWMASK)==LHD_DIRECTORY;
        hd->WinSize=hd->Dir ? 0:0x10000<<((hd->Flags & LHD_WINDOWMASK)>>5);
        hd->CommentInHeader=(hd->Flags & LHD_COMMENT)!=0;
        hd->Version=(hd->Flags & LHD_VERSION)!=0;

        hd->DataSize=Raw.Get4();
        uint LowUnpSize=Raw.Get4();
        hd->HostOS=Raw.Get1();

        hd->FileHash.Type=HASH_CRC32;
        hd->FileHash.CRC32=Raw.Get4();

        uint FileTime=Raw.Get4();
        hd->UnpVer=Raw.Get1();

        hd->Method=Raw.Get1()-0x30;
        size_t NameSize=Raw.Get2();
        hd->FileAttr=Raw.Get4();

        if (hd->UnpVer<20 && (hd->FileAttr & 0x10)!=0)
          hd->Dir=true;

        hd->CryptMethod=CRYPT_NONE;
        if (hd->Encrypted)
          switch(hd->UnpVer)
          {
            case 13: hd->CryptMethod=CRYPT_RAR13; break;
            case 15: hd->CryptMethod=CRYPT_RAR15; break;
            case 20:
            case 26: hd->CryptMethod=CRYPT_RAR20; break;
            default: hd->CryptMethod=CRYPT_RAR30; break;
          }

        hd->HSType=HSYS_UNKNOWN;
        if (hd->HostOS==HOST_UNIX || hd->HostOS==HOST_BEOS)
          hd->HSType=HSYS_UNIX;
        else
          if (hd->HostOS<HOST_MAX)
            hd->HSType=HSYS_WINDOWS;

        hd->RedirType=FSREDIR_NONE;

        if (hd->HostOS==HOST_UNIX && (hd->FileAttr & 0xF000)==0xA000)
        {
          hd->RedirType=FSREDIR_UNIXSYMLINK;
          hd->RedirName.clear();
        }

        hd->Inherited=!FileBlock && (hd->SubFlags & SUBHEAD_FLAGS_INHERITED)!=0;

        hd->LargeFile=(hd->Flags & LHD_LARGE)!=0;

        uint HighPackSize,HighUnpSize;
        if (hd->LargeFile)
        {
          HighPackSize=Raw.Get4();
          HighUnpSize=Raw.Get4();
          hd->UnknownUnpSize=(LowUnpSize==0xffffffff && HighUnpSize==0xffffffff);
        }
        else
        {
          HighPackSize=HighUnpSize=0;

          hd->UnknownUnpSize=(LowUnpSize==0xffffffff);
        }
        hd->PackSize=INT32TO64(HighPackSize,hd->DataSize);
        hd->UnpSize=INT32TO64(HighUnpSize,LowUnpSize);
        if (hd->UnknownUnpSize)
          hd->UnpSize=INT64NDF;

        size_t ReadNameSize=Min(NameSize,MAXPATHSIZE);
        std::string FileName(ReadNameSize,0);
        Raw.GetB((byte *)&FileName[0],ReadNameSize);

        if (FileBlock)
        {
          hd->FileName.clear();
          if ((hd->Flags & LHD_UNICODE)!=0)
          {
            EncodeFileName NameCoder;
            size_t Length=strlen(FileName.data());
            Length++;
            if (ReadNameSize>Length)
              NameCoder.Decode(FileName.data(),ReadNameSize,
                               (byte *)&FileName[Length],
                               ReadNameSize-Length,hd->FileName);
          }

          if (hd->FileName.empty())
            ArcCharToWide(FileName.data(),hd->FileName,ACTW_OEM);

#ifndef SFX_MODULE
          ConvertNameCase(hd->FileName);
#endif
          ConvertFileHeader(hd);
        }
        else
        {
          CharToWide(FileName.data(),hd->FileName);

          int DataSize=int(hd->HeadSize-NameSize-SIZEOF_FILEHEAD3);
          if ((hd->Flags & LHD_SALT)!=0)
            DataSize-=SIZE_SALT30;

          if (DataSize>0)
          {

            hd->SubData.resize(DataSize);
            Raw.GetB(hd->SubData.data(),DataSize);

          }

          if (hd->CmpName(SUBHEAD_TYPE_CMT))
            MainComment=true;
        }

        if ((hd->Flags & LHD_SALT)!=0)
          Raw.GetB(hd->Salt,SIZE_SALT30);
        hd->mtime.SetDos(FileTime);
        if ((hd->Flags & LHD_EXTTIME)!=0)
        {
          ushort Flags=Raw.Get2();
          RarTime *tbl[4];
          tbl[0]=&FileHead.mtime;
          tbl[1]=&FileHead.ctime;
          tbl[2]=&FileHead.atime;
          tbl[3]=NULL;
          for (int I=0;I<4;I++)
          {
            RarTime *CurTime=tbl[I];
            uint rmode=Flags>>(3-I)*4;
            if ((rmode & 8)==0 || CurTime==NULL)
              continue;
            if (I!=0)
            {
              uint DosTime=Raw.Get4();
              CurTime->SetDos(DosTime);
            }
            RarLocalTime rlt;
            CurTime->GetLocal(&rlt);
            if (rmode & 4)
              rlt.Second++;
            rlt.Reminder=0;
            uint count=rmode&3;
            for (uint J=0;J<count;J++)
            {
              byte CurByte=Raw.Get1();
              rlt.Reminder|=(((uint)CurByte)<<((J+3-count)*8));
            }

            rlt.Reminder*=RarTime::REMINDER_PRECISION/10000000;
            CurTime->SetLocal(&rlt);
          }
        }

        NextBlockPos=SafeAdd(NextBlockPos,hd->PackSize,0);

        bool CRCProcessedOnly=hd->CommentInHeader;
        uint HeaderCRC=Raw.GetCRC15(CRCProcessedOnly);
        if (hd->HeadCRC!=HeaderCRC)
        {
          BrokenHeader=true;
          ErrHandler.SetErrorCode(RARX_WARNING);

          if (!Decrypt)
            uiMsg(UIERROR_FHEADERBROKEN,Archive::FileName,hd->FileName);
        }
      }
      break;
    case HEAD_ENDARC:
      EndArcHead.SetBaseBlock(ShortBlock);
      EndArcHead.NextVolume=(EndArcHead.Flags & EARC_NEXT_VOLUME)!=0;
      EndArcHead.DataCRC=(EndArcHead.Flags & EARC_DATACRC)!=0;
      EndArcHead.RevSpace=(EndArcHead.Flags & EARC_REVSPACE)!=0;
      EndArcHead.StoreVolNumber=(EndArcHead.Flags & EARC_VOLNUMBER)!=0;
      if (EndArcHead.DataCRC)
        EndArcHead.ArcDataCRC=Raw.Get4();
      if (EndArcHead.StoreVolNumber)
        VolNumber=EndArcHead.VolNumber=Raw.Get2();
      break;
#ifndef SFX_MODULE
    case HEAD3_CMT:
      CommHead.SetBaseBlock(ShortBlock);
      CommHead.UnpSize=Raw.Get2();
      CommHead.UnpVer=Raw.Get1();
      CommHead.Method=Raw.Get1();
      CommHead.CommCRC=Raw.Get2();
      break;
    case HEAD3_PROTECT:
      ProtectHead.SetBaseBlock(ShortBlock);
      ProtectHead.DataSize=Raw.Get4();
      ProtectHead.Version=Raw.Get1();
      ProtectHead.RecSectors=Raw.Get2();
      ProtectHead.TotalBlocks=Raw.Get4();
      Raw.GetB(ProtectHead.Mark,8);
      NextBlockPos+=ProtectHead.DataSize;
      break;
    case HEAD3_OLDSERVICE:
      SubBlockHead.SetBaseBlock(ShortBlock);
      SubBlockHead.DataSize=Raw.Get4();
      NextBlockPos+=SubBlockHead.DataSize;
      SubBlockHead.SubType=Raw.Get2();
      SubBlockHead.Level=Raw.Get1();
      switch(SubBlockHead.SubType)
      {
        case NTACL_HEAD:
          *(SubBlockHeader *)&EAHead=SubBlockHead;
          EAHead.UnpSize=Raw.Get4();
          EAHead.UnpVer=Raw.Get1();
          EAHead.Method=Raw.Get1();
          EAHead.EACRC=Raw.Get4();
          break;
        case STREAM_HEAD:
          *(SubBlockHeader *)&StreamHead=SubBlockHead;
          StreamHead.UnpSize=Raw.Get4();
          StreamHead.UnpVer=Raw.Get1();
          StreamHead.Method=Raw.Get1();
          StreamHead.StreamCRC=Raw.Get4();
          StreamHead.StreamNameSize=Raw.Get2();

          const size_t MaxStreamName20=260;
          if (StreamHead.StreamNameSize>MaxStreamName20)
            StreamHead.StreamNameSize=MaxStreamName20;

          StreamHead.StreamName.resize(StreamHead.StreamNameSize);
          Raw.GetB(&StreamHead.StreamName[0],StreamHead.StreamNameSize);
          break;
      }
      break;
#endif
    default:
      if (ShortBlock.Flags & LONG_BLOCK)
        NextBlockPos+=Raw.Get4();
      break;
  }

  uint HeaderCRC=Raw.GetCRC15(false);

  if (ShortBlock.HeadCRC!=HeaderCRC && ShortBlock.HeaderType!=HEAD3_SIGN &&
      ShortBlock.HeaderType!=HEAD3_AV &&
      (ShortBlock.HeaderType!=HEAD3_OLDSERVICE || SubBlockHead.SubType!=UO_HEAD))
  {
    bool Recovered=false;
    if (ShortBlock.HeaderType==HEAD_ENDARC && EndArcHead.RevSpace)
    {

      int64 Length=Tell();
      Seek(Length-7,SEEK_SET);
      Recovered=true;
      for (int J=0;J<7;J++)
        if (GetByte()!=0)
          Recovered=false;
    }
    if (!Recovered)
    {
      BrokenHeader=true;
      ErrHandler.SetErrorCode(RARX_CRC);

      if (Decrypt)
      {
        uiMsg(UIERROR_CHECKSUMENC,FileName,FileName);
        FailedHeaderDecryption=true;
        return 0;
      }
    }
  }

  return Raw.Size();
}
#endif

size_t Archive::ReadHeader50()
{
  RawRead Raw(this);

  bool Decrypt=Encrypted && CurBlockPos>(int64)SFXSize+SIZEOF_MARKHEAD5;

  if (Decrypt)
  {
#if defined(RAR_NOCRYPT)
    return 0;
#else

    if (Cmd->SkipEncrypted)
    {
      uiMsg(UIMSG_SKIPENCARC,FileName);
      FailedHeaderDecryption=true;
      return 0;
    }

    byte HeadersInitV[SIZE_INITV];
    if (Read(HeadersInitV,SIZE_INITV)!=SIZE_INITV)
    {
      UnexpEndArcMsg();
      return 0;
    }

    bool GlobalPassword=Cmd->Password.IsSet() || uiIsGlobalPasswordSet();

    RarCheckPassword CheckPwd;
    if (CryptHead.UsePswCheck && !BrokenHeader)
      CheckPwd.Set(CryptHead.Salt,HeadersInitV,CryptHead.Lg2Count,CryptHead.PswCheck);

    while (true)
    {
      RequestArcPassword(CheckPwd.IsSet() ? &CheckPwd:NULL);

      byte PswCheck[SIZE_PSWCHECK];
      bool EncSet=HeadersCrypt.SetCryptKeys(false,CRYPT_RAR50,&Cmd->Password,CryptHead.Salt,HeadersInitV,CryptHead.Lg2Count,NULL,PswCheck);

      if (EncSet && CryptHead.UsePswCheck && !BrokenHeader &&
          memcmp(PswCheck,CryptHead.PswCheck,SIZE_PSWCHECK)!=0)
      {
        if (GlobalPassword)
        {

          uiMsg(UIERROR_BADPSW,FileName,FileName);
          FailedHeaderDecryption=true;
          ErrHandler.SetErrorCode(RARX_BADPWD);
          return 0;
        }
        else
        {

          uiMsg(UIWAIT_BADPSW,FileName,FileName);
          Cmd->Password.Clean();
        }

#ifdef RARDLL

        ErrHandler.SetErrorCode(RARX_BADPWD);
        Cmd->DllError=ERAR_BAD_PASSWORD;
        ErrHandler.Exit(RARX_BADPWD);
#else
        continue;
#endif
      }
      break;
    }

    Raw.SetCrypt(&HeadersCrypt);
#endif
  }

  const size_t FirstReadSize=7;
  if (Raw.Read(FirstReadSize)<FirstReadSize)
  {
    UnexpEndArcMsg();
    return 0;
  }

  ShortBlock.Reset();
  ShortBlock.HeadCRC=Raw.Get4();
  uint SizeBytes=Raw.GetVSize(4);
  uint64 BlockSize=Raw.GetV();

  if (BlockSize==0 || SizeBytes==0)
  {
    BrokenHeaderMsg();
    return 0;
  }

  int SizeToRead=int(BlockSize);
  SizeToRead-=int(FirstReadSize-SizeBytes-4);
  uint HeaderSize=4+SizeBytes+(uint)BlockSize;

  if (SizeToRead<0 || HeaderSize<SIZEOF_SHORTBLOCKHEAD5)
  {
    BrokenHeaderMsg();
    return 0;
  }

  Raw.Read(SizeToRead);

  if (Raw.Size()<HeaderSize)
  {
    UnexpEndArcMsg();
    return 0;
  }

  uint HeaderCRC=Raw.GetCRC50();

  ShortBlock.HeaderType=(HEADER_TYPE)Raw.GetV();
  ShortBlock.Flags=(uint)Raw.GetV();
  ShortBlock.SkipIfUnknown=(ShortBlock.Flags & HFL_SKIPIFUNKNOWN)!=0;
  ShortBlock.HeadSize=HeaderSize;

  CurHeaderType=ShortBlock.HeaderType;

  bool BadCRC=(ShortBlock.HeadCRC!=HeaderCRC);
  if (BadCRC)
  {
    BrokenHeaderMsg();

    BrokenHeader=true;
    ErrHandler.SetErrorCode(RARX_CRC);

    if (Decrypt)
    {
      uiMsg(UIERROR_CHECKSUMENC,FileName,FileName);
      FailedHeaderDecryption=true;
      return 0;
    }
  }

  uint64 ExtraSize=0;
  if ((ShortBlock.Flags & HFL_EXTRA)!=0)
  {
    ExtraSize=Raw.GetV();
    if (ExtraSize>=ShortBlock.HeadSize)
    {
      BrokenHeaderMsg();
      return 0;
    }
  }

  uint64 DataSize=0;
  if ((ShortBlock.Flags & HFL_DATA)!=0)
    DataSize=Raw.GetV();

  NextBlockPos=CurBlockPos+FullHeaderSize(ShortBlock.HeadSize);

  NextBlockPos=SafeAdd(NextBlockPos,DataSize,0);

  switch(ShortBlock.HeaderType)
  {
    case HEAD_CRYPT:
      {
        CryptHead.SetBaseBlock(ShortBlock);
        uint CryptVersion=(uint)Raw.GetV();
        if (CryptVersion>CRYPT_VERSION)
        {
          UnkEncVerMsg(FileName,L"h" + std::to_wstring(CryptVersion));
          FailedHeaderDecryption=true;
          return 0;
        }
        uint EncFlags=(uint)Raw.GetV();
        CryptHead.UsePswCheck=(EncFlags & CHFL_CRYPT_PSWCHECK)!=0;
        CryptHead.Lg2Count=Raw.Get1();
        if (CryptHead.Lg2Count>CRYPT5_KDF_LG2_COUNT_MAX)
        {
          UnkEncVerMsg(FileName,L"hc" + std::to_wstring(CryptHead.Lg2Count));
          FailedHeaderDecryption=true;
          return 0;
        }

        Raw.GetB(CryptHead.Salt,SIZE_SALT50);
        if (CryptHead.UsePswCheck)
        {
          Raw.GetB(CryptHead.PswCheck,SIZE_PSWCHECK);

          byte csum[SIZE_PSWCHECK_CSUM];
          Raw.GetB(csum,SIZE_PSWCHECK_CSUM);

#ifndef RAR_NOCRYPT
          byte Digest[SHA256_DIGEST_SIZE];
          sha256_get(CryptHead.PswCheck, SIZE_PSWCHECK, Digest);

          CryptHead.UsePswCheck=memcmp(csum,Digest,SIZE_PSWCHECK_CSUM)==0;
#endif
        }
        Encrypted=true;
      }
      break;
    case HEAD_MAIN:
      {
        MainHead.Reset();
        MainHead.SetBaseBlock(ShortBlock);
        uint ArcFlags=(uint)Raw.GetV();

        Volume=(ArcFlags & MHFL_VOLUME)!=0;
        Solid=(ArcFlags & MHFL_SOLID)!=0;
        Locked=(ArcFlags & MHFL_LOCK)!=0;
        Protected=(ArcFlags & MHFL_PROTECT)!=0;
        Signed=false;
        NewNumbering=true;

        if ((ArcFlags & MHFL_VOLNUMBER)!=0)
          VolNumber=(uint)Raw.GetV();
        else
          VolNumber=0;
        FirstVolume=Volume && VolNumber==0;

        if (ExtraSize!=0)
          ProcessExtra50(&Raw,(size_t)ExtraSize,&MainHead);

#ifdef USE_QOPEN
        if (!ProhibitQOpen && MainHead.Locator && MainHead.QOpenOffset>0 && Cmd->QOpenMode!=QOPEN_NONE)
        {

          int64 SaveCurBlockPos=CurBlockPos,SaveNextBlockPos=NextBlockPos;
          HEADER_TYPE SaveCurHeaderType=CurHeaderType;

          QOpen.Init(this,false);
          QOpen.Load(MainHead.QOpenOffset);

          CurBlockPos=SaveCurBlockPos;
          NextBlockPos=SaveNextBlockPos;
          CurHeaderType=SaveCurHeaderType;
        }
#endif
      }
      break;
    case HEAD_FILE:
    case HEAD_SERVICE:
      {
        FileHeader *hd=ShortBlock.HeaderType==HEAD_FILE ? &FileHead:&SubHead;
        hd->Reset();
        *(BaseBlock *)hd=ShortBlock;

        bool FileBlock=ShortBlock.HeaderType==HEAD_FILE;

        hd->LargeFile=true;

        hd->PackSize=DataSize;
        hd->FileFlags=(uint)Raw.GetV();
        hd->UnpSize=Raw.GetV();

        hd->UnknownUnpSize=(hd->FileFlags & FHFL_UNPUNKNOWN)!=0;
        if (hd->UnknownUnpSize)
          hd->UnpSize=INT64NDF;

        hd->MaxSize=Max(hd->PackSize,hd->UnpSize);
        hd->FileAttr=(uint)Raw.GetV();
        if ((hd->FileFlags & FHFL_UTIME)!=0)
          hd->mtime.SetUnix((time_t)Raw.Get4());

        hd->FileHash.Type=HASH_NONE;
        if ((hd->FileFlags & FHFL_CRC32)!=0)
        {
          hd->FileHash.Type=HASH_CRC32;
          hd->FileHash.CRC32=Raw.Get4();
        }

        hd->RedirType=FSREDIR_NONE;

        uint CompInfo=(uint)Raw.GetV();
        hd->Method=(CompInfo>>7) & 7;

        uint UnpVer=(CompInfo & 0x3f);
        if (UnpVer==0)
          hd->UnpVer=VER_PACK5;
        else
          if (UnpVer==1)
            hd->UnpVer=VER_PACK7;
          else
            hd->UnpVer=VER_UNKNOWN;

        hd->HostOS=(byte)Raw.GetV();
        size_t NameSize=(size_t)Raw.GetV();
        hd->Inherited=(ShortBlock.Flags & HFL_INHERITED)!=0;

        hd->HSType=HSYS_UNKNOWN;
        if (hd->HostOS==HOST5_UNIX)
          hd->HSType=HSYS_UNIX;
        else
          if (hd->HostOS==HOST5_WINDOWS)
            hd->HSType=HSYS_WINDOWS;

        hd->SplitBefore=(hd->Flags & HFL_SPLITBEFORE)!=0;
        hd->SplitAfter=(hd->Flags & HFL_SPLITAFTER)!=0;
        hd->SubBlock=(hd->Flags & HFL_CHILD)!=0;
        hd->Solid=FileBlock && (CompInfo & FCI_SOLID)!=0;
        hd->Dir=(hd->FileFlags & FHFL_DIRECTORY)!=0;
        if (hd->Dir || UnpVer>1)
          hd->WinSize=0;
        else
        {
          hd->WinSize=0x20000ULL<<((CompInfo>>10)&(UnpVer==0 ? 0x0f:0x1f));
          if (UnpVer==1)
          {
            hd->WinSize+=hd->WinSize/32*((CompInfo>>15)&0x1f);

            if ((CompInfo & FCI_RAR5_COMPAT)!=0)
              hd->UnpVer=VER_PACK5;
            if (hd->WinSize>UNPACK_MAX_DICT)
              hd->UnpVer=VER_UNKNOWN;
          }
        }

        size_t ReadNameSize=Min(NameSize,MAXPATHSIZE);
        std::string FileName(ReadNameSize,0);
        Raw.GetB((byte *)&FileName[0],ReadNameSize);

        UtfToWide(FileName.data(),hd->FileName);

        if (ExtraSize!=0)
          ProcessExtra50(&Raw,(size_t)ExtraSize,hd);

        if (FileBlock)
        {
#ifndef SFX_MODULE
          ConvertNameCase(hd->FileName);
#endif
          ConvertFileHeader(hd);
        }

        if (!FileBlock && hd->CmpName(SUBHEAD_TYPE_CMT))
          MainComment=true;

        if (!FileBlock && hd->CmpName(SUBHEAD_TYPE_RR) && hd->SubData.size()>0)
        {

          RawRead RawPercent;
          RawPercent.Read(hd->SubData.data(),hd->SubData.size());
          RecoveryPercent=(int)RawPercent.GetV();

        }

        if (BadCRC)
          uiMsg(UIERROR_FHEADERBROKEN,Archive::FileName,hd->FileName);
      }
      break;
    case HEAD_ENDARC:
      {
        EndArcHead.SetBaseBlock(ShortBlock);
        uint ArcFlags=(uint)Raw.GetV();
        EndArcHead.NextVolume=(ArcFlags & EHFL_NEXTVOLUME)!=0;
        EndArcHead.StoreVolNumber=false;
        EndArcHead.DataCRC=false;
        EndArcHead.RevSpace=false;
      }
      break;
  }

  return Raw.Size();
}

#if !defined(RAR_NOCRYPT)
void Archive::RequestArcPassword(RarCheckPassword *CheckPwd)
{
  if (!Cmd->Password.IsSet())
  {
#ifdef RARDLL
    if (Cmd->Callback!=NULL)
    {
      wchar PasswordW[MAXPASSWORD];
      *PasswordW=0;
      if (Cmd->Callback(UCM_NEEDPASSWORDW,Cmd->UserData,(LPARAM)PasswordW,ASIZE(PasswordW))==-1)
        *PasswordW=0;
      if (*PasswordW==0)
      {
        char PasswordA[MAXPASSWORD];
        *PasswordA=0;
        if (Cmd->Callback(UCM_NEEDPASSWORD,Cmd->UserData,(LPARAM)PasswordA,ASIZE(PasswordA))==-1)
          *PasswordA=0;
        CharToWide(PasswordA,PasswordW,ASIZE(PasswordW));
        cleandata(PasswordA,sizeof(PasswordA));
      }
      Cmd->Password.Set(PasswordW);
      cleandata(PasswordW,sizeof(PasswordW));
    }
    if (!Cmd->Password.IsSet())
    {
      Close();
      Cmd->DllError=ERAR_MISSING_PASSWORD;
      ErrHandler.Exit(RARX_USERBREAK);
    }
#else
    if (!uiGetPassword(UIPASSWORD_ARCHIVE,FileName,&Cmd->Password,CheckPwd))
    {
      Close();
      uiMsg(UIERROR_INCERRCOUNT);
      ErrHandler.Exit(RARX_USERBREAK);
    }
#endif
    Cmd->ManualPassword=true;
  }
}
#endif

void Archive::ProcessExtra50(RawRead *Raw,size_t ExtraSize,const BaseBlock *bb)
{

  size_t ExtraStart=Raw->Size()-ExtraSize;
  if (ExtraStart<Raw->GetPos())
    return;
  Raw->SetPos(ExtraStart);
  while (Raw->DataLeft()>=2)
  {
    int64 FieldSize=Raw->GetV();
    if (FieldSize<=0 || Raw->DataLeft()==0 || FieldSize>(int64)Raw->DataLeft())
      break;
    size_t NextPos=size_t(Raw->GetPos()+FieldSize);
    uint64 FieldType=Raw->GetV();

    FieldSize=int64(NextPos-Raw->GetPos());

    if (FieldSize<0)
      break;

    if (bb->HeaderType==HEAD_MAIN)
    {
      MainHeader *hd=(MainHeader *)bb;
      switch(FieldType)
      {
        case MHEXTRA_LOCATOR:
          {
            hd->Locator=true;
            uint Flags=(uint)Raw->GetV();
            if ((Flags & MHEXTRA_LOCATOR_QLIST)!=0)
            {
              uint64 Offset=Raw->GetV();
              if (Offset!=0)
                hd->QOpenOffset=Offset+CurBlockPos;
            }
            if ((Flags & MHEXTRA_LOCATOR_RR)!=0)
            {
              uint64 Offset=Raw->GetV();
              if (Offset!=0)
                hd->RROffset=Offset+CurBlockPos;
            }
          }
          break;
        case MHEXTRA_METADATA:
          {
            uint Flags=(uint)Raw->GetV();
            if ((Flags & MHEXTRA_METADATA_NAME)!=0)
            {
              uint64 NameSize=Raw->GetV();
              if (NameSize>0 && NameSize<MAXPATHSIZE)
              {
                std::string NameU((size_t)NameSize,0);
                Raw->GetB(&NameU[0],(size_t)NameSize);

                if (NameU[0]!=0)
                  UtfToWide(&NameU[0],hd->OrigName);
              }
            }
            if ((Flags & MHEXTRA_METADATA_CTIME)!=0)
              if ((Flags & MHEXTRA_METADATA_UNIXTIME)!=0)
                if ((Flags & MHEXTRA_METADATA_UNIX_NS)!=0)
                  hd->OrigTime.SetUnixNS(Raw->Get8());
                else
                  hd->OrigTime.SetUnix((time_t)Raw->Get4());
              else
                hd->OrigTime.SetWin(Raw->Get8());
          }
          break;
      }
    }

    if (bb->HeaderType==HEAD_FILE || bb->HeaderType==HEAD_SERVICE)
    {
      FileHeader *hd=(FileHeader *)bb;
      switch(FieldType)
      {
#ifndef RAR_NOCRYPT
        case FHEXTRA_CRYPT:
          {
            FileHeader *hd=(FileHeader *)bb;
            uint EncVersion=(uint)Raw->GetV();
            if (EncVersion>CRYPT_VERSION)
            {
              UnkEncVerMsg(hd->FileName,L"x" + std::to_wstring(EncVersion));
              hd->CryptMethod=CRYPT_UNKNOWN;
            }
            else
            {
              uint Flags=(uint)Raw->GetV();
              hd->Lg2Count=Raw->Get1();
              if (hd->Lg2Count>CRYPT5_KDF_LG2_COUNT_MAX)
              {
                UnkEncVerMsg(hd->FileName,L"xc" + std::to_wstring(hd->Lg2Count));
                hd->CryptMethod=CRYPT_UNKNOWN;
              }
              else
              {
                hd->UsePswCheck=(Flags & FHEXTRA_CRYPT_PSWCHECK)!=0;
                hd->UseHashKey=(Flags & FHEXTRA_CRYPT_HASHMAC)!=0;

                Raw->GetB(hd->Salt,SIZE_SALT50);
                Raw->GetB(hd->InitV,SIZE_INITV);
                if (hd->UsePswCheck)
                {
                  Raw->GetB(hd->PswCheck,SIZE_PSWCHECK);

                  byte csum[SIZE_PSWCHECK_CSUM];
                  Raw->GetB(csum,SIZE_PSWCHECK_CSUM);

                  byte Digest[SHA256_DIGEST_SIZE];
                  sha256_get(hd->PswCheck, SIZE_PSWCHECK, Digest);

                  hd->UsePswCheck=memcmp(csum,Digest,SIZE_PSWCHECK_CSUM)==0;

                  if (bb->HeaderType==HEAD_SERVICE && memcmp(hd->PswCheck,"\0\0\0\0\0\0\0\0",SIZE_PSWCHECK)==0)
                    hd->UsePswCheck=0;
                }
                hd->SaltSet=true;
                hd->CryptMethod=CRYPT_RAR50;
                hd->Encrypted=true;
              }
            }
          }
          break;
#endif
        case FHEXTRA_HASH:
          {
            FileHeader *hd=(FileHeader *)bb;
            uint Type=(uint)Raw->GetV();
            if (Type==FHEXTRA_HASH_BLAKE2)
            {
              hd->FileHash.Type=HASH_BLAKE2;
              Raw->GetB(hd->FileHash.Digest,BLAKE2_DIGEST_SIZE);
            }
          }
          break;
        case FHEXTRA_HTIME:
          if (FieldSize>=5)
          {
            byte Flags=(byte)Raw->GetV();
            bool UnixTime=(Flags & FHEXTRA_HTIME_UNIXTIME)!=0;
            if ((Flags & FHEXTRA_HTIME_MTIME)!=0)
              if (UnixTime)
                hd->mtime.SetUnix(Raw->Get4());
              else
                hd->mtime.SetWin(Raw->Get8());
            if ((Flags & FHEXTRA_HTIME_CTIME)!=0)
              if (UnixTime)
                hd->ctime.SetUnix(Raw->Get4());
              else
                hd->ctime.SetWin(Raw->Get8());
            if ((Flags & FHEXTRA_HTIME_ATIME)!=0)
              if (UnixTime)
                hd->atime.SetUnix((time_t)Raw->Get4());
              else
                hd->atime.SetWin(Raw->Get8());
            if (UnixTime && (Flags & FHEXTRA_HTIME_UNIX_NS)!=0)
            {
              uint ns;
              if ((Flags & FHEXTRA_HTIME_MTIME)!=0 && (ns=(Raw->Get4() & 0x3fffffff))<1000000000)
                hd->mtime.Adjust(ns);
              if ((Flags & FHEXTRA_HTIME_CTIME)!=0 && (ns=(Raw->Get4() & 0x3fffffff))<1000000000)
                hd->ctime.Adjust(ns);
              if ((Flags & FHEXTRA_HTIME_ATIME)!=0 && (ns=(Raw->Get4() & 0x3fffffff))<1000000000)
                hd->atime.Adjust(ns);
            }
          }
          break;
        case FHEXTRA_VERSION:
          if (FieldSize>=1)
          {
            Raw->GetV();
            uint Version=(uint)Raw->GetV();
            if (Version!=0)
            {
              hd->Version=true;
              hd->FileName += L';' + std::to_wstring(Version);
            }
          }
          break;
        case FHEXTRA_REDIR:
          {
            FILE_SYSTEM_REDIRECT RedirType=(FILE_SYSTEM_REDIRECT)Raw->GetV();
            uint Flags=(uint)Raw->GetV();
            size_t NameSize=(size_t)Raw->GetV();

            if (NameSize>0 && NameSize<MAXPATHSIZE)
            {
              std::string UtfName(NameSize,0);
              hd->RedirType=RedirType;
              hd->DirTarget=(Flags & FHEXTRA_REDIR_DIR)!=0;
              Raw->GetB(&UtfName[0],NameSize);
              UtfToWide(&UtfName[0],hd->RedirName);
#ifdef _WIN_ALL
              UnixSlashToDos(hd->RedirName,hd->RedirName);
#endif
            }
          }
          break;
        case FHEXTRA_UOWNER:
          {
            uint Flags=(uint)Raw->GetV();
            hd->UnixOwnerNumeric=(Flags & FHEXTRA_UOWNER_NUMUID)!=0;
            hd->UnixGroupNumeric=(Flags & FHEXTRA_UOWNER_NUMGID)!=0;
            *hd->UnixOwnerName=*hd->UnixGroupName=0;
            if ((Flags & FHEXTRA_UOWNER_UNAME)!=0)
            {
              size_t Length=(size_t)Raw->GetV();
              Length=Min(Length,ASIZE(hd->UnixOwnerName)-1);
              Raw->GetB(hd->UnixOwnerName,Length);
              hd->UnixOwnerName[Length]=0;
            }
            if ((Flags & FHEXTRA_UOWNER_GNAME)!=0)
            {
              size_t Length=(size_t)Raw->GetV();
              Length=Min(Length,ASIZE(hd->UnixGroupName)-1);
              Raw->GetB(hd->UnixGroupName,Length);
              hd->UnixGroupName[Length]=0;
            }
#ifdef _UNIX
            if (hd->UnixOwnerNumeric)
              hd->UnixOwnerID=(uid_t)Raw->GetV();
            if (hd->UnixGroupNumeric)
              hd->UnixGroupID=(gid_t)Raw->GetV();
#else

            if (hd->UnixOwnerNumeric)
              hd->UnixOwnerID=(uint)Raw->GetV();
            if (hd->UnixGroupNumeric)
              hd->UnixGroupID=(uint)Raw->GetV();
#endif
            hd->UnixOwnerSet=true;
          }
          break;
        case FHEXTRA_SUBDATA:
          {

            if (bb->HeaderType==HEAD_SERVICE && Raw->Size()-NextPos==1)
              FieldSize++;

            hd->SubData.resize((size_t)FieldSize);
            Raw->GetB(hd->SubData.data(),(size_t)FieldSize);
          }
          break;
      }
    }

    Raw->SetPos(NextPos);
  }
}

#ifndef SFX_MODULE
size_t Archive::ReadHeader14()
{
  RawRead Raw(this);
  if (CurBlockPos<=(int64)SFXSize)
  {
    Raw.Read(SIZEOF_MAINHEAD14);
    MainHead.Reset();
    byte Mark[4];
    Raw.GetB(Mark,4);
    uint HeadSize=Raw.Get2();
    if (HeadSize<7)
      return 0;
    byte Flags=Raw.Get1();
    NextBlockPos=CurBlockPos+HeadSize;
    CurHeaderType=HEAD_MAIN;

    Volume=(Flags & MHD_VOLUME)!=0;
    Solid=(Flags & MHD_SOLID)!=0;
    Locked=(Flags & MHD_LOCK)!=0;
    MainHead.CommentInHeader=(Flags & MHD_COMMENT)!=0;
    MainHead.PackComment=(Flags & MHD_PACK_COMMENT)!=0;
  }
  else
  {
    Raw.Read(SIZEOF_FILEHEAD14);
    FileHead.Reset();

    FileHead.HeaderType=HEAD_FILE;
    FileHead.DataSize=Raw.Get4();
    FileHead.UnpSize=Raw.Get4();
    FileHead.FileHash.Type=HASH_RAR14;
    FileHead.FileHash.CRC32=Raw.Get2();
    FileHead.HeadSize=Raw.Get2();
    if (FileHead.HeadSize<21)
      return 0;
    uint FileTime=Raw.Get4();
    FileHead.FileAttr=Raw.Get1();
    FileHead.Flags=Raw.Get1()|LONG_BLOCK;
    FileHead.UnpVer=(Raw.Get1()==2) ? 13 : 10;
    size_t NameSize=Raw.Get1();
    FileHead.Method=Raw.Get1();

    FileHead.SplitBefore=(FileHead.Flags & LHD_SPLIT_BEFORE)!=0;
    FileHead.SplitAfter=(FileHead.Flags & LHD_SPLIT_AFTER)!=0;
    FileHead.Encrypted=(FileHead.Flags & LHD_PASSWORD)!=0;
    FileHead.CryptMethod=FileHead.Encrypted ? CRYPT_RAR13:CRYPT_NONE;

    FileHead.PackSize=FileHead.DataSize;
    FileHead.WinSize=0x10000;
    FileHead.Dir=(FileHead.FileAttr & 0x10)!=0;

    FileHead.HostOS=HOST_MSDOS;
    FileHead.HSType=HSYS_WINDOWS;

    FileHead.mtime.SetDos(FileTime);

    Raw.Read(NameSize);

    std::string FileName(NameSize,0);
    Raw.GetB((byte *)&FileName[0],NameSize);
    std::string NameA;
    OemToExt(FileName,NameA);
    CharToWide(NameA,FileHead.FileName);
    ConvertNameCase(FileHead.FileName);
    ConvertFileHeader(&FileHead);

    if (Raw.Size()!=0)
      NextBlockPos=CurBlockPos+FileHead.HeadSize+FileHead.PackSize;
    CurHeaderType=HEAD_FILE;
  }
  return NextBlockPos>CurBlockPos ? Raw.Size() : 0;
}
#endif

#ifndef SFX_MODULE
void Archive::ConvertNameCase(std::wstring &Name)
{
  if (Cmd->ConvertNames==NAMES_UPPERCASE)
    wcsupper(Name);
  if (Cmd->ConvertNames==NAMES_LOWERCASE)
    wcslower(Name);
}
#endif

bool Archive::IsArcDir()
{
  return FileHead.Dir;
}

void Archive::ConvertAttributes()
{
#ifdef _WIN_ALL
  if (FileHead.HSType!=HSYS_WINDOWS)
    FileHead.FileAttr=FileHead.Dir ? 0x10 : 0x20;
#endif
#ifdef _UNIX

  static mode_t mask = (mode_t) -1;

  if (mask == (mode_t) -1)
  {

    mask = umask(022);

    umask(mask);
  }

  switch(FileHead.HSType)
  {
    case HSYS_WINDOWS:
      {

        if (FileHead.FileAttr & 0x10)
        {

          FileHead.FileAttr=0777 & ~mask;
        }
        else
          if (FileHead.FileAttr & 1)
          {

            FileHead.FileAttr=0444 & ~mask;
          }
          else
          {

            FileHead.FileAttr=0666 & ~mask;
          }
      }
      break;
    case HSYS_UNIX:
      break;
    default:
      if (FileHead.Dir)
        FileHead.FileAttr=0x41ff & ~mask;
      else
        FileHead.FileAttr=0x81b6 & ~mask;
      break;
  }
#endif
}

void Archive::ConvertFileHeader(FileHeader *hd)
{

#ifdef _WIN_ALL
  if (hd->HSType==HSYS_UNIX)
    ConvertToPrecomposed(hd->FileName);
#endif

  for (uint I=0;I<hd->FileName.size();I++)
  {
    wchar *s=&hd->FileName[I];

#ifdef _UNIX

    if (*s=='\\' && Format==RARFMT50 && hd->HSType==HSYS_WINDOWS)
      *s='_';
#endif

#ifdef _WIN_ALL

    if (*s=='\\' && Format==RARFMT50)
      *s='_';

    if (*s==':')
      *s='_';
#endif

    if (*s=='/' || *s=='\\' && Format!=RARFMT50)
      *s=CPATHDIVIDER;
  }

  TruncateAtZero(hd->FileName);
}

int64 Archive::GetStartPos()
{
  int64 StartPos=SFXSize+MarkHead.HeadSize;
  if (Format==RARFMT15)
    StartPos+=MainHead.HeadSize;
  else
    StartPos+=CryptHead.HeadSize+FullHeaderSize(MainHead.HeadSize);
  return StartPos;
}

bool Archive::ReadSubData(std::vector<byte> *UnpData,File *DestFile,bool TestMode)
{
  if (BrokenHeader)
  {
    uiMsg(UIERROR_SUBHEADERBROKEN,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return false;
  }
  if (SubHead.Method>5 || SubHead.UnpVer>(Format==RARFMT50 ? VER_UNPACK7:VER_UNPACK))
  {
    uiMsg(UIERROR_SUBHEADERUNKNOWN,FileName);
    return false;
  }

  if (SubHead.PackSize==0 && !SubHead.SplitAfter)
    return true;

  SubDataIO.Init();
  Unpack Unpack(&SubDataIO);
  Unpack.Init(SubHead.WinSize,false);

  if (DestFile==NULL)
  {
    if (SubHead.UnpSize>0x1000000)
    {

      uiMsg(UIERROR_SUBHEADERUNKNOWN,FileName);
      return false;
    }
    if (UnpData==NULL)
      SubDataIO.SetTestMode(true);
    else
    {
      UnpData->resize((size_t)SubHead.UnpSize);
      SubDataIO.SetUnpackToMemory(&(*UnpData)[0],(uint)SubHead.UnpSize);
    }
  }
  if (SubHead.Encrypted)
    if (Cmd->Password.IsSet())
      SubDataIO.SetEncryption(false,SubHead.CryptMethod,&Cmd->Password,
                SubHead.SaltSet ? SubHead.Salt:NULL,SubHead.InitV,
                SubHead.Lg2Count,SubHead.HashKey,SubHead.PswCheck);
    else
      return false;
  SubDataIO.UnpHash.Init(SubHead.FileHash.Type,1);
  SubDataIO.SetPackedSizeToRead(SubHead.PackSize);
  SubDataIO.EnableShowProgress(false);
  SubDataIO.SetFiles(this,DestFile);
  SubDataIO.SetTestMode(TestMode);
  SubDataIO.UnpVolume=SubHead.SplitAfter;
  SubDataIO.SetSubHeader(&SubHead,NULL);
  Unpack.SetDestSize(SubHead.UnpSize);
  if (SubHead.Method==0)
    CmdExtract::UnstoreFile(SubDataIO,SubHead.UnpSize);
  else
    Unpack.DoUnpack(SubHead.UnpVer,false);

  if (!SubDataIO.UnpHash.Cmp(&SubHead.FileHash,SubHead.UseHashKey ? SubHead.HashKey:NULL))
  {
    uiMsg(UIERROR_SUBHEADERDATABROKEN,FileName,SubHead.FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    if (UnpData!=NULL)
      UnpData->clear();
    return false;
  }
  return true;
}

static const byte blake2s_sigma[10][16] =
{
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
  { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
  { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
  {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
  {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
  {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
  { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
  { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
  {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
  { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 } ,
};

#ifdef USE_SSE

static __m128i blake2s_IV_0_3, blake2s_IV_4_7;

static __m128i crotr8, crotr16;

#ifdef __GNUC__
__attribute__((target("sse2")))
#endif
static void blake2s_init_sse()
{

  blake2s_IV_0_3 = _mm_setr_epi32( 0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A );
  blake2s_IV_4_7 = _mm_setr_epi32( 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19 );

  crotr8 = _mm_set_epi8( 12, 15, 14, 13, 8, 11, 10, 9, 4, 7, 6, 5, 0, 3, 2, 1 );
  crotr16 = _mm_set_epi8( 13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2 );
}

#define LOAD(p)  _mm_load_si128( (__m128i *)(p) )
#define STORE(p,r) _mm_store_si128((__m128i *)(p), r)

#define mm_rotr_epi32(r, c) ( \
                c==8 ? _mm_shuffle_epi8(r,crotr8) \
              : c==16 ? _mm_shuffle_epi8(r,crotr16) \
              : _mm_xor_si128(_mm_srli_epi32( (r), c ),_mm_slli_epi32( (r), 32-c )) )

#define G1(row1,row2,row3,row4,buf) \
  row1 = _mm_add_epi32( _mm_add_epi32( row1, buf), row2 ); \
  row4 = _mm_xor_si128( row4, row1 ); \
  row4 =  mm_rotr_epi32(row4, 16); \
  row3 = _mm_add_epi32( row3, row4 );   \
  row2 = _mm_xor_si128( row2, row3 ); \
  row2 =  mm_rotr_epi32(row2, 12);

#define G2(row1,row2,row3,row4,buf) \
  row1 = _mm_add_epi32( _mm_add_epi32( row1, buf), row2 ); \
  row4 = _mm_xor_si128( row4, row1 ); \
  row4 =  mm_rotr_epi32(row4, 8); \
  row3 = _mm_add_epi32( row3, row4 );   \
  row2 = _mm_xor_si128( row2, row3 ); \
  row2 =  mm_rotr_epi32(row2, 7);

#define DIAGONALIZE(row1,row2,row3,row4) \
  row4 = _mm_shuffle_epi32( row4, _MM_SHUFFLE(2,1,0,3) ); \
  row3 = _mm_shuffle_epi32( row3, _MM_SHUFFLE(1,0,3,2) ); \
  row2 = _mm_shuffle_epi32( row2, _MM_SHUFFLE(0,3,2,1) );

#define UNDIAGONALIZE(row1,row2,row3,row4) \
  row4 = _mm_shuffle_epi32( row4, _MM_SHUFFLE(0,3,2,1) ); \
  row3 = _mm_shuffle_epi32( row3, _MM_SHUFFLE(1,0,3,2) ); \
  row2 = _mm_shuffle_epi32( row2, _MM_SHUFFLE(2,1,0,3) );

#define SSE_ROUND(m,row,r) \
{ \
  __m128i buf; \
  buf=_mm_set_epi32(m[blake2s_sigma[r][6]],m[blake2s_sigma[r][4]],m[blake2s_sigma[r][2]],m[blake2s_sigma[r][0]]); \
  G1(row[0],row[1],row[2],row[3],buf); \
  buf=_mm_set_epi32(m[blake2s_sigma[r][7]],m[blake2s_sigma[r][5]],m[blake2s_sigma[r][3]],m[blake2s_sigma[r][1]]); \
  G2(row[0],row[1],row[2],row[3],buf); \
  DIAGONALIZE(row[0],row[1],row[2],row[3]); \
  buf=_mm_set_epi32(m[blake2s_sigma[r][14]],m[blake2s_sigma[r][12]],m[blake2s_sigma[r][10]],m[blake2s_sigma[r][8]]); \
  G1(row[0],row[1],row[2],row[3],buf); \
  buf=_mm_set_epi32(m[blake2s_sigma[r][15]],m[blake2s_sigma[r][13]],m[blake2s_sigma[r][11]],m[blake2s_sigma[r][9]]); \
  G2(row[0],row[1],row[2],row[3],buf); \
  UNDIAGONALIZE(row[0],row[1],row[2],row[3]); \
}

#ifdef __GNUC__
__attribute__((target("ssse3")))
#endif
static int blake2s_compress_sse( blake2s_state *S, const byte block[BLAKE2S_BLOCKBYTES] )
{
  __m128i row[4];
  __m128i ff0, ff1;

  const uint32  *m = ( uint32 * )block;

  row[0] = ff0 = LOAD( &S->h[0] );
  row[1] = ff1 = LOAD( &S->h[4] );

  row[2] = blake2s_IV_0_3;
  row[3] = _mm_xor_si128( blake2s_IV_4_7, LOAD( &S->t[0] ) );
  SSE_ROUND( m, row, 0 );
  SSE_ROUND( m, row, 1 );
  SSE_ROUND( m, row, 2 );
  SSE_ROUND( m, row, 3 );
  SSE_ROUND( m, row, 4 );
  SSE_ROUND( m, row, 5 );
  SSE_ROUND( m, row, 6 );
  SSE_ROUND( m, row, 7 );
  SSE_ROUND( m, row, 8 );
  SSE_ROUND( m, row, 9 );
  STORE( &S->h[0], _mm_xor_si128( ff0, _mm_xor_si128( row[0], row[2] ) ) );
  STORE( &S->h[4], _mm_xor_si128( ff1, _mm_xor_si128( row[1], row[3] ) ) );
  return 0;
}

#endif

static void blake2s_init_param( blake2s_state *S, uint32 node_offset, uint32 node_depth);
static void blake2s_update( blake2s_state *S, const byte *in, size_t inlen );
static void blake2s_final( blake2s_state *S, byte *digest );

#define PARALLELISM_DEGREE 8

void blake2sp_init( blake2sp_state *S )
{
  memset( S->buf, 0, sizeof( S->buf ) );
  S->buflen = 0;

  blake2s_init_param( &S->R, 0, 1 );

  for( uint32 i = 0; i < PARALLELISM_DEGREE; ++i )
    blake2s_init_param( &S->S[i], i, 0 );

  S->R.last_node = 1;
  S->S[PARALLELISM_DEGREE - 1].last_node = 1;
}

struct Blake2ThreadData
{
  void Update();
  blake2s_state *S;
  const byte *in;
  size_t inlen;
};

void Blake2ThreadData::Update()
{
  size_t inlen__ = inlen;
  const byte *in__ = ( const byte * )in;

  while( inlen__ >= PARALLELISM_DEGREE * BLAKE2S_BLOCKBYTES )
  {
#ifdef USE_SSE

    if (_SSE_Version>=SSE_SSE && inlen__ >= 2 * PARALLELISM_DEGREE * BLAKE2S_BLOCKBYTES)
      _mm_prefetch((char*)(in__ +  PARALLELISM_DEGREE * BLAKE2S_BLOCKBYTES), _MM_HINT_T0);
#endif

    blake2s_update( S, in__, BLAKE2S_BLOCKBYTES );
    in__ += PARALLELISM_DEGREE * BLAKE2S_BLOCKBYTES;
    inlen__ -= PARALLELISM_DEGREE * BLAKE2S_BLOCKBYTES;
  }
}

#ifdef RAR_SMP
THREAD_PROC(Blake2Thread)
{
  Blake2ThreadData *td=(Blake2ThreadData *)Data;
  td->Update();
}
#endif

void blake2sp_update( blake2sp_state *S, const byte *in, size_t inlen )
{
  size_t left = S->buflen;
  size_t fill = sizeof( S->buf ) - left;

  if( left && inlen >= fill )
  {
    memcpy( S->buf + left, in, fill );

    for( size_t i = 0; i < PARALLELISM_DEGREE; ++i )
      blake2s_update( &S->S[i], S->buf + i * BLAKE2S_BLOCKBYTES, BLAKE2S_BLOCKBYTES );

    in += fill;
    inlen -= fill;
    left = 0;
  }

  Blake2ThreadData btd_array[PARALLELISM_DEGREE];

#ifdef RAR_SMP
  uint ThreadNumber = inlen < 0x1000 ? 1 : S->MaxThreads;

  if (ThreadNumber==6 || ThreadNumber==7)
    ThreadNumber=4;
#else
  uint ThreadNumber=1;
#endif

  for (size_t id__=0;id__<PARALLELISM_DEGREE;)
  {
    for (uint Thread=0;Thread<ThreadNumber && id__<PARALLELISM_DEGREE;Thread++)
    {
      Blake2ThreadData *btd=btd_array+Thread;

      btd->inlen = inlen;
      btd->in = in + id__ * BLAKE2S_BLOCKBYTES;
      btd->S = &S->S[id__];

#ifdef RAR_SMP
      if (ThreadNumber>1)
        S->ThPool->AddTask(Blake2Thread,(void*)btd);
      else
        btd->Update();
#else
      btd->Update();
#endif
      id__++;
    }
#ifdef RAR_SMP
    if (S->ThPool!=NULL)
      S->ThPool->WaitDone();
#endif
  }

  in += inlen - inlen % ( PARALLELISM_DEGREE * BLAKE2S_BLOCKBYTES );
  inlen %= PARALLELISM_DEGREE * BLAKE2S_BLOCKBYTES;

  if( inlen > 0 )
    memcpy( S->buf + left, in, (size_t)inlen );

  S->buflen = left + (size_t)inlen;
}

void blake2sp_final( blake2sp_state *S, byte *digest )
{
  byte hash[PARALLELISM_DEGREE][BLAKE2S_OUTBYTES];

  for( size_t i = 0; i < PARALLELISM_DEGREE; ++i )
  {
    if( S->buflen > i * BLAKE2S_BLOCKBYTES )
    {
      size_t left = S->buflen - i * BLAKE2S_BLOCKBYTES;

      if( left > BLAKE2S_BLOCKBYTES ) left = BLAKE2S_BLOCKBYTES;

      blake2s_update( &S->S[i], S->buf + i * BLAKE2S_BLOCKBYTES, left );
    }

    blake2s_final( &S->S[i], hash[i] );
  }

  for( size_t i = 0; i < PARALLELISM_DEGREE; ++i )
    blake2s_update( &S->R, hash[i], BLAKE2S_OUTBYTES );

  blake2s_final( &S->R, digest );
}

static const uint32 blake2s_IV[8] =
{
  0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
  0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static inline void blake2s_set_lastnode( blake2s_state *S )
{
  S->f[1] = ~0U;
}

static inline void blake2s_set_lastblock( blake2s_state *S )
{
  if( S->last_node ) blake2s_set_lastnode( S );

  S->f[0] = ~0U;
}

static inline void blake2s_increment_counter( blake2s_state *S, const uint32 inc )
{
  S->t[0] += inc;
  S->t[1] += ( S->t[0] < inc );
}

void blake2s_init_param( blake2s_state *S, uint32 node_offset, uint32 node_depth)
{
#ifdef USE_SSE
  if (_SSE_Version>=SSE_SSE2)
    blake2s_init_sse();
#endif

  S->init();
  for( int i = 0; i < 8; ++i )
    S->h[i] = blake2s_IV[i];

  S->h[0] ^= 0x02080020;
  S->h[2] ^= node_offset;
  S->h[3] ^= (node_depth<<16)|0x20000000;
}

#define G(r,i,m,a,b,c,d) \
  a = a + b + m[blake2s_sigma[r][2*i+0]]; \
  d = rotr32(d ^ a, 16); \
  c = c + d; \
  b = rotr32(b ^ c, 12); \
  a = a + b + m[blake2s_sigma[r][2*i+1]]; \
  d = rotr32(d ^ a, 8); \
  c = c + d; \
  b = rotr32(b ^ c, 7);

static void blake2s_compress( blake2s_state *S, const byte block[BLAKE2S_BLOCKBYTES] )
{
  uint32 m[16];
  uint32 v[16];

  for( size_t i = 0; i < 16; ++i )
    m[i] = RawGet4( block + i * 4 );

  for( size_t i = 0; i < 8; ++i )
    v[i] = S->h[i];

  v[ 8] = blake2s_IV[0];
  v[ 9] = blake2s_IV[1];
  v[10] = blake2s_IV[2];
  v[11] = blake2s_IV[3];
  v[12] = S->t[0] ^ blake2s_IV[4];
  v[13] = S->t[1] ^ blake2s_IV[5];
  v[14] = S->f[0] ^ blake2s_IV[6];
  v[15] = S->f[1] ^ blake2s_IV[7];

  for ( uint r = 0; r <= 9; ++r )
  {
    G(r,0,m,v[ 0],v[ 4],v[ 8],v[12]);
    G(r,1,m,v[ 1],v[ 5],v[ 9],v[13]);
    G(r,2,m,v[ 2],v[ 6],v[10],v[14]);
    G(r,3,m,v[ 3],v[ 7],v[11],v[15]);
    G(r,4,m,v[ 0],v[ 5],v[10],v[15]);
    G(r,5,m,v[ 1],v[ 6],v[11],v[12]);
    G(r,6,m,v[ 2],v[ 7],v[ 8],v[13]);
    G(r,7,m,v[ 3],v[ 4],v[ 9],v[14]);
  }

  for( size_t i = 0; i < 8; ++i )
    S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
}

void blake2s_update( blake2s_state *S, const byte *in, size_t inlen )
{
  while( inlen > 0 )
  {
    size_t left = S->buflen;
    size_t fill = 2 * BLAKE2S_BLOCKBYTES - left;

    if( inlen > fill )
    {
      memcpy( S->buf + left, in, fill );
      S->buflen += fill;
      blake2s_increment_counter( S, BLAKE2S_BLOCKBYTES );

#ifdef USE_SSE
      if (_SSE_Version>=SSE_SSSE3)
        blake2s_compress_sse( S, S->buf );
      else
        blake2s_compress( S, S->buf );
#else
      blake2s_compress( S, S->buf );
#endif

      memcpy( S->buf, S->buf + BLAKE2S_BLOCKBYTES, BLAKE2S_BLOCKBYTES );
      S->buflen -= BLAKE2S_BLOCKBYTES;
      in += fill;
      inlen -= fill;
    }
    else
    {
      memcpy( S->buf + left, in, (size_t)inlen );
      S->buflen += (size_t)inlen;
      in += inlen;
      inlen = 0;
    }
  }
}

void blake2s_final( blake2s_state *S, byte *digest )
{
  if( S->buflen > BLAKE2S_BLOCKBYTES )
  {
    blake2s_increment_counter( S, BLAKE2S_BLOCKBYTES );
    blake2s_compress( S, S->buf );
    S->buflen -= BLAKE2S_BLOCKBYTES;
    memcpy( S->buf, S->buf + BLAKE2S_BLOCKBYTES, S->buflen );
  }

  blake2s_increment_counter( S, ( uint32 )S->buflen );
  blake2s_set_lastblock( S );
  memset( S->buf + S->buflen, 0, 2 * BLAKE2S_BLOCKBYTES - S->buflen );
  blake2s_compress( S, S->buf );

  for( int i = 0; i < 8; ++i )
    RawPut4( S->h[i], digest + 4 * i );
}

bool CommandData::ExclCheck(const std::wstring &CheckName,bool Dir,bool CheckFullPath,bool CheckInclList)
{
  if (CheckArgs(&ExclArgs,Dir,CheckName,CheckFullPath,MATCH_WILDSUBPATH))
    return true;
  if (!CheckInclList || InclArgs.ItemsCount()==0)
    return false;
  if (CheckArgs(&InclArgs,Dir,CheckName,CheckFullPath,MATCH_WILDSUBPATH))
    return false;
  return true;
}

bool CommandData::CheckArgs(StringList *Args,bool Dir,const std::wstring &CheckName,bool CheckFullPath,int MatchMode)
{
  std::wstring Name,FullName,CurMask;
  ConvertPath(&CheckName,&Name);
  Args->Rewind();
  while (Args->GetString(CurMask))
  {
    wchar LastMaskChar=GetLastChar(CurMask);
    bool DirMask=IsPathDiv(LastMaskChar);

    if (Dir)
    {

      if (DirMask)
      {

        CurMask.pop_back();
      }
      else
      {

        std::wstring Name=PointToName(CurMask);
        if (IsWildcard(Name) && Name!=L"*" && Name!=L"*.*")
         continue;
      }
    }
    else
    {

      if (DirMask)
        CurMask+=L"*";
    }

#ifndef SFX_MODULE
    if (CheckFullPath && IsFullPath(CurMask))
    {

      if (FullName.empty())
        ConvertNameToFull(CheckName,FullName);
      if (CmpName(CurMask,FullName,MatchMode))
        return true;
    }
    else
#endif
    {
      std::wstring CurName=Name;

      size_t MaskOffset=ConvertPath(&CurMask,nullptr);
      std::wstring CmpMask=CurMask.substr(MaskOffset);

      if (CmpMask[0]=='*' && IsPathDiv(CmpMask[1]))
      {

        CurName=L'.';
        CurName+=CPATHDIVIDER;
        CurName+=Name;
      }

      if (CmpName(CmpMask,CurName,MatchMode))
        return true;
    }
  }
  return false;
}

#ifndef SFX_MODULE

bool CommandData::ExclDirByAttr(uint FileAttr)
{
#ifdef _WIN_ALL
  if ((FileAttr & FILE_ATTRIBUTE_REPARSE_POINT)!=0 &&
      (ExclFileAttr & FILE_ATTRIBUTE_REPARSE_POINT)!=0)
    return true;
#endif
  return false;
}
#endif

#if !defined(SFX_MODULE)
void CommandData::SetTimeFilters(const wchar *Mod,bool Before,bool Age)
{
  bool ModeOR=false,TimeMods=false;
  const wchar *S=Mod;

  for (;*S!=0 && wcschr(L"MCAOmcao",*S)!=NULL;S++)
    if (*S=='o' || *S=='O')
      ModeOR=true;
    else
      TimeMods=true;

  if (!TimeMods)
    Mod=L"m";

  for (;*Mod!=0 && wcschr(L"MCAOmcao",*Mod)!=NULL;Mod++)
    switch(toupperw(*Mod))
    {
      case 'M':
        if (Before)
        {
          Age ? FileMtimeBefore.SetAgeText(S):FileMtimeBefore.SetIsoText(S);
          FileMtimeBeforeOR=ModeOR;
        }
        else
        {
          Age ? FileMtimeAfter.SetAgeText(S):FileMtimeAfter.SetIsoText(S);
          FileMtimeAfterOR=ModeOR;
        }
        break;
      case 'C':
        if (Before)
        {
          Age ? FileCtimeBefore.SetAgeText(S):FileCtimeBefore.SetIsoText(S);
          FileCtimeBeforeOR=ModeOR;
        }
        else
        {
          Age ? FileCtimeAfter.SetAgeText(S):FileCtimeAfter.SetIsoText(S);
          FileCtimeAfterOR=ModeOR;
        }
        break;
      case 'A':
        if (Before)
        {
          Age ? FileAtimeBefore.SetAgeText(S):FileAtimeBefore.SetIsoText(S);
          FileAtimeBeforeOR=ModeOR;
        }
        else
        {
          Age ? FileAtimeAfter.SetAgeText(S):FileAtimeAfter.SetIsoText(S);
          FileAtimeAfterOR=ModeOR;
        }
        break;
    }
}
#endif

#ifndef SFX_MODULE

bool CommandData::TimeCheck(RarTime &ftm,RarTime &ftc,RarTime &fta)
{
  bool FilterOR=false;

  if (FileMtimeBefore.IsSet())
    if (ftm>=FileMtimeBefore)
      if (FileMtimeBeforeOR)
        FilterOR=true;
      else
        return true;
    else
      if (FileMtimeBeforeOR)
        return false;

  if (FileMtimeAfter.IsSet())
    if (ftm<FileMtimeAfter)
      if (FileMtimeAfterOR)
        FilterOR=true;
      else
        return true;
    else
      if (FileMtimeAfterOR)
        return false;

  if (FileCtimeBefore.IsSet())
    if (ftc>=FileCtimeBefore)
      if (FileCtimeBeforeOR)
        FilterOR=true;
      else
        return true;
    else
      if (FileCtimeBeforeOR)
        return false;

  if (FileCtimeAfter.IsSet())
    if (ftc<FileCtimeAfter)
      if (FileCtimeAfterOR)
        FilterOR=true;
      else
        return true;
    else
      if (FileCtimeAfterOR)
        return false;

  if (FileAtimeBefore.IsSet())
    if (fta>=FileAtimeBefore)
      if (FileAtimeBeforeOR)
        FilterOR=true;
      else
        return true;
    else
      if (FileAtimeBeforeOR)
        return false;

  if (FileAtimeAfter.IsSet())
    if (fta<FileAtimeAfter)
      if (FileAtimeAfterOR)
        FilterOR=true;
      else
        return true;
    else
      if (FileAtimeAfterOR)
        return false;

  return FilterOR;
}
#endif

#ifndef SFX_MODULE

bool CommandData::SizeCheck(int64 Size)
{
  if (Size==INT64NDF)
    return false;
  if (FileSizeLess!=INT64NDF && Size>=FileSizeLess)
    return true;
  if (FileSizeMore!=INT64NDF && Size<=FileSizeMore)
    return true;
  return false;
}
#endif

int CommandData::IsProcessFile(FileHeader &FileHead,bool *ExactMatch,int MatchType,
                               bool Flags,std::wstring *MatchedArg)
{
  if (MatchedArg!=NULL)
    MatchedArg->clear();
  bool Dir=FileHead.Dir;
  if (ExclCheck(FileHead.FileName,Dir,false,true))
    return 0;
#ifndef SFX_MODULE
  if (TimeCheck(FileHead.mtime,FileHead.ctime,FileHead.atime))
    return 0;
  if ((FileHead.FileAttr & ExclFileAttr)!=0 || FileHead.Dir && ExclDir)
    return 0;
  if (InclAttrSet && (FileHead.FileAttr & InclFileAttr)==0 &&
      (!FileHead.Dir || !InclDir))
    return 0;
  if (!Dir && SizeCheck(FileHead.UnpSize))
    return 0;
#endif
  std::wstring ArgName;
  FileArgs.Rewind();
  for (int StringCount=1;FileArgs.GetString(ArgName);StringCount++)
  {

    if (CmpName(ArgName,FileHead.FileName,MatchType))
    {
      if (ExactMatch!=NULL)
        *ExactMatch=wcsicompc(ArgName,FileHead.FileName)==0;
      if (MatchedArg!=NULL)
        *MatchedArg=ArgName;
      return StringCount;
    }
  }
  return 0;
}

#if !defined(SFX_MODULE)
void CommandData::SetStoreTimeMode(const wchar *S)
{
  if (*S==0 || IsDigit(*S) || *S=='-' || *S=='+')
  {

    EXTTIME_MODE Mode=EXTTIME_MAX;
    if (*S=='-')
      Mode=EXTTIME_NONE;
    if (*S=='1')
      Mode=EXTTIME_1S;
    xmtime=xctime=xatime=Mode;
    S++;
  }

  while (*S!=0)
  {
    EXTTIME_MODE Mode=EXTTIME_MAX;
    if (S[1]=='-')
      Mode=EXTTIME_NONE;
    if (S[1]=='1')
      Mode=EXTTIME_1S;
    switch(toupperw(*S))
    {
      case 'M':
        xmtime=Mode;
        break;
      case 'C':
        xctime=Mode;
        break;
      case 'A':
        xatime=Mode;
        break;
      case 'P':
        PreserveAtime=true;
        break;
    }
    S++;
  }
}
#endif

void CommandData::OutTitle()
{
  if (BareOutput || DisableCopyright)
    return;
#if defined(__GNUC__) && defined(SFX_MODULE)
  mprintf(St(MCopyrightS));
#else
#ifndef SILENT
  static bool TitleShown=false;
  if (TitleShown)
    return;
  TitleShown=true;

  wchar Version[80];
  if (RARVER_BETA!=0)
    swprintf(Version,ASIZE(Version),L"%d.%02d %ls %d",RARVER_MAJOR,RARVER_MINOR,St(MBeta),RARVER_BETA);
  else
    swprintf(Version,ASIZE(Version),L"%d.%02d",RARVER_MAJOR,RARVER_MINOR);
#if defined(_WIN_32) || defined(_WIN_64)
  wcsncatz(Version,L" ",ASIZE(Version));
#endif
#ifdef _WIN_32
  wcsncatz(Version,St(Mx86),ASIZE(Version));
#endif
#ifdef _WIN_64
  wcsncatz(Version,St(Mx64),ASIZE(Version));
#endif
  if (PrintVersion)
  {
    mprintf(L"%s",Version);
    exit(0);
  }
  mprintf(St(MUCopyright),Version,RARVER_YEAR);
#endif
#endif
}

inline bool CmpMSGID(MSGID i1,MSGID i2)
{
#ifdef MSGID_INT
  return i1==i2;
#else

  return wcscmp(i1,i2)==0;
#endif
}

void CommandData::OutHelp(RAR_EXIT ExitCode)
{
#if !defined(SILENT)
  OutTitle();
  static MSGID Help[]={
#ifdef SFX_MODULE

    MCHelpCmd,MSHelpCmdE,MSHelpCmdT,MSHelpCmdV
#else

    MUNRARTitle1,MRARTitle2,MCHelpCmd,MCHelpCmdE,MCHelpCmdL,
    MCHelpCmdP,MCHelpCmdT,MCHelpCmdV,MCHelpCmdX,MCHelpSw,MCHelpSwm,
    MCHelpSwAT,MCHelpSwAC,MCHelpSwAD,MCHelpSwAG,MCHelpSwAI,MCHelpSwAP,
    MCHelpSwCm,MCHelpSwCFGm,MCHelpSwCL,MCHelpSwCU,MCHelpSwDH,MCHelpSwEP,
    MCHelpSwEP3,MCHelpSwEP4,MCHelpSwF,MCHelpSwIDP,MCHelpSwIERR,
    MCHelpSwINUL,MCHelpSwIOFF,MCHelpSwKB,MCHelpSwME,MCHelpSwMLP,
    MCHelpSwN,MCHelpSwNa,MCHelpSwNal,MCHelpSwO,MCHelpSwOC,MCHelpSwOL,
    MCHelpSwOM,MCHelpSwOP,MCHelpSwOR,MCHelpSwOW,MCHelpSwP,MCHelpSwR,
    MCHelpSwRI,MCHelpSwSC,MCHelpSwSI,MCHelpSwSL,MCHelpSwSM,MCHelpSwTA,
    MCHelpSwTB,MCHelpSwTN,MCHelpSwTO,MCHelpSwTS,MCHelpSwU,MCHelpSwVUnr,
    MCHelpSwVER,MCHelpSwVP,MCHelpSwX,MCHelpSwXa,MCHelpSwXal,MCHelpSwY
#endif
  };

  for (uint I=0;I<ASIZE(Help);I++)
  {
#ifndef SFX_MODULE
    if (CmpMSGID(Help[I],MCHelpSwV))
      continue;
#ifndef _WIN_ALL
    static MSGID Win32Only[]={
      MCHelpSwIEML,MCHelpSwVD,MCHelpSwAO,MCHelpSwOS,MCHelpSwIOFF,
      MCHelpSwEP2,MCHelpSwMLP,MCHelpSwOC,MCHelpSwONI,MCHelpSwDR,MCHelpSwRI
    };
    bool Found=false;
    for (uint J=0;J<ASIZE(Win32Only);J++)
      if (CmpMSGID(Help[I],Win32Only[J]))
      {
        Found=true;
        break;
      }
    if (Found)
      continue;
#endif
#ifdef _UNIX
    if (CmpMSGID(Help[I],MRARTitle2))
    {
      mprintf(St(MFwrSlTitle2));
      continue;
    }
#endif
#if !defined(_UNIX) && !defined(_WIN_ALL)
    if (CmpMSGID(Help[I],MCHelpSwOW))
      continue;
#endif
#ifndef _WIN_ALL
    if (CmpMSGID(Help[I],MCHelpSwAC))
      continue;
#endif
#ifndef SAVE_LINKS
    if (CmpMSGID(Help[I],MCHelpSwOL))
      continue;
#endif
#ifndef RAR_SMP
    if (CmpMSGID(Help[I],MCHelpSwMT))
      continue;
#endif
#endif
    mprintf(St(Help[I]));
  }
  mprintf(L"\n");
  ErrHandler.Exit(ExitCode);
#endif
}

CommandData::CommandData()
{
  Init();
}

void CommandData::Init()
{
  RAROptions::Init();

  Command.clear();
  ArcName.clear();
  ExtrPath.clear();
  TempPath.clear();
  SFXModule.clear();
  CommentFile.clear();
  ArcPath.clear();
  ExclArcPath.clear();
  LogName.clear();
  EmailTo.clear();
  UseStdin.clear();

  FileLists=false;
  NoMoreSwitches=false;

  ListMode=RCLM_AUTO;

  BareOutput=false;

  FileArgs.Reset();
  ExclArgs.Reset();
  InclArgs.Reset();
  ArcNames.Reset();
  StoreArgs.Reset();
#ifdef PROPAGATE_MOTW
  MotwList.Reset();
#endif
  Password.Clean();
  NextVolSizes.clear();
#ifdef RARDLL
  DllDestName.clear();
#endif
}

#if !defined(SFX_MODULE)
void CommandData::ParseCommandLine(bool Preprocess,int argc, char *argv[])
{
  Command.clear();
  NoMoreSwitches=false;
#ifdef CUSTOM_CMDLINE_PARSER

  std::wstring CmdLine=GetCommandLine();

  std::wstring Param;
  std::wstring::size_type Pos=0;

  for (bool FirstParam=true;;FirstParam=false)
  {
    if (!GetCmdParam(CmdLine,Pos,Param))
      break;
    if (!FirstParam)
      if (Preprocess)
        PreprocessArg(Param.data());
      else
        ParseArg(Param.data());
  }
#else
  for (int I=1;I<argc;I++)
  {
    std::wstring Arg;
    CharToWide(argv[I],Arg);
    if (Preprocess)
      PreprocessArg(Arg.data());
    else
      ParseArg(Arg.data());
  }
#endif
  if (!Preprocess)
    ParseDone();
}
#endif

#if !defined(SFX_MODULE)
void CommandData::ParseArg(const wchar *Arg)
{
  if (IsSwitch(*Arg) && !NoMoreSwitches)
    if (Arg[1]=='-' && Arg[2]==0)
      NoMoreSwitches=true;
    else
      ProcessSwitch(Arg+1);
  else
    if (Command.empty())
    {
      Command=Arg;

      Command[0]=toupperw(Command[0]);

      if (Command[0]!='I' && Command[0]!='S')
        wcsupper(Command);
      if (Command[0]=='P')
      {
        MsgStream=MSG_ERRONLY;
        SetConsoleMsgStream(MSG_ERRONLY);
      }
    }
    else
      if (ArcName.empty())
        ArcName=Arg;
      else
      {

        size_t Length=wcslen(Arg);
        wchar EndChar=Length==0 ? 0:Arg[Length-1];

        bool FolderArg=IsDriveDiv(EndChar) || IsPathDiv(EndChar);

        if (IsDriveLetter(Arg) && Arg[2]=='.' && (Arg[3]==0 || Arg[3]=='.' && Arg[4]==0))
          FolderArg=true;

        size_t L=Length;
        if (L>0 && Arg[L-1]=='.' && (L==1 || L>=2 && (IsPathDiv(Arg[L-2]) ||
            Arg[L-2]=='.' && (L==2 || L>=3 && IsPathDiv(Arg[L-3])))))
          FolderArg=true;

        wchar CmdChar=toupperw(Command[0]);
        bool Add=wcschr(L"AFUM",CmdChar)!=NULL;
        bool Extract=CmdChar=='X' || CmdChar=='E';
        bool Repair=CmdChar=='R' && Command[1]==0;
        if (FolderArg && !Add)
          ExtrPath=Arg;
        else
          if ((Add || CmdChar=='T') && (*Arg!='@' || ListMode==RCLM_REJECT_LISTS))
            FileArgs.AddString(Arg);
          else
          {
            FindData FileData;
            bool Found=FindFile::FastFind(Arg,&FileData);
            if ((!Found || ListMode==RCLM_ACCEPT_LISTS) &&
                ListMode!=RCLM_REJECT_LISTS && *Arg=='@' && !IsWildcard(Arg+1))
            {
              FileLists=true;

              ReadTextFile(Arg+1,&FileArgs,false,true,FilelistCharset,true,true,true);

            }
            else
              if (Found && FileData.IsDir && (Extract || Repair) && ExtrPath.empty())
              {
                ExtrPath=Arg;
                AddEndSlash(ExtrPath);
              }
              else
                FileArgs.AddString(Arg);
          }
      }
}
#endif

void CommandData::ParseDone()
{
  if (FileArgs.ItemsCount()==0 && !FileLists)
    FileArgs.AddString(MASKALL);
  wchar CmdChar=toupperw(Command[0]);
  bool Extract=CmdChar=='X' || CmdChar=='E' || CmdChar=='P';
  if (Test && Extract)
    Test=false;

  if ((CmdChar=='L' || CmdChar=='V') && Command[1]=='B')
    BareOutput=true;
}

#if !defined(SFX_MODULE)
void CommandData::ParseEnvVar()
{
  char *EnvVar=getenv("RARINISWITCHES");
  if (EnvVar!=NULL)
  {
    std::wstring EnvStr;
    CharToWide(EnvVar,EnvStr);
    ProcessSwitchesString(EnvStr);
  }
}
#endif

#if !defined(SFX_MODULE)

void CommandData::PreprocessArg(const wchar *Arg)
{
  if (IsSwitch(Arg[0]) && !NoMoreSwitches)
  {
    Arg++;
    if (Arg[0]=='-' && Arg[1]==0)
      NoMoreSwitches=true;
    if (wcsicomp(Arg,L"cfg-")==0)
      ProcessSwitch(Arg);
    if (wcsnicomp(Arg,L"ilog",4)==0)
    {

      ProcessSwitch(Arg);
      InitLogOptions(LogName,ErrlogCharset);
    }
    if (wcsnicomp(Arg,L"sc",2)==0)
    {

      ProcessSwitch(Arg);
      if (!LogName.empty())
        InitLogOptions(LogName,ErrlogCharset);
    }
  }
  else
    if (Command.empty())
      Command=Arg;
}
#endif

#if !defined(SFX_MODULE)
void CommandData::ReadConfig()
{
  StringList List;
  if (ReadTextFile(DefConfigName,&List,true))
  {
    wchar *Str;
    while ((Str=List.GetString())!=NULL)
    {
      while (IsSpace(*Str))
        Str++;
      if (wcsnicomp(Str,L"switches=",9)==0)
        ProcessSwitchesString(Str+9);
      if (!Command.empty())
      {
        wchar Cmd[16];
        wcsncpyz(Cmd,Command.c_str(),ASIZE(Cmd));
        wchar C0=toupperw(Cmd[0]);
        wchar C1=toupperw(Cmd[1]);
        if (C0=='I' || C0=='L' || C0=='M' || C0=='S' || C0=='V')
          Cmd[1]=0;
        if (C0=='R' && (C1=='R' || C1=='V'))
          Cmd[2]=0;
        wchar SwName[16+ASIZE(Cmd)];
        swprintf(SwName,ASIZE(SwName),L"switches_%ls=",Cmd);
        size_t Length=wcslen(SwName);
        if (wcsnicomp(Str,SwName,Length)==0)
          ProcessSwitchesString(Str+Length);
      }
    }
  }
}
#endif

#if !defined(SFX_MODULE)
void CommandData::ProcessSwitchesString(const std::wstring &Str)
{
  std::wstring Par;
  std::wstring::size_type Pos=0;
  while (GetCmdParam(Str,Pos,Par))
  {
    if (IsSwitch(Par[0]))
      ProcessSwitch(&Par[1]);
    else
    {
      mprintf(St(MSwSyntaxError),Par.c_str());
      ErrHandler.Exit(RARX_USERERROR);
    }
  }
}
#endif

#if !defined(SFX_MODULE)
void CommandData::ProcessSwitch(const wchar *Switch)
{

  if (LargePageAlloc::ProcessSwitch(this,Switch))
    return;

  switch(toupperw(Switch[0]))
  {
    case '@':
      ListMode=Switch[1]=='+' ? RCLM_ACCEPT_LISTS:RCLM_REJECT_LISTS;
      break;
    case 'A':
      switch(toupperw(Switch[1]))
      {
        case 'C':
          ClearArc=true;
          break;
        case 'D':
          if (Switch[2]==0)
            AppendArcNameToPath=APPENDARCNAME_DESTPATH;
          else
            if (Switch[2]=='1')
              AppendArcNameToPath=APPENDARCNAME_OWNSUBDIR;
            else
              if (Switch[2]=='2')
                AppendArcNameToPath=APPENDARCNAME_OWNDIR;
          break;
#ifndef SFX_MODULE
        case 'G':
          if (Switch[2]=='-' && Switch[3]==0)
            GenerateArcName=0;
          else
            if (toupperw(Switch[2])=='F')
              wcsncpyz(DefGenerateMask,Switch+3,ASIZE(DefGenerateMask));
            else
            {
              GenerateArcName=true;
              wcsncpyz(GenerateMask,Switch+2,ASIZE(GenerateMask));
            }
          break;
#endif
        case 'I':
          IgnoreGeneralAttr=true;
          break;
        case 'M':
          switch(toupperw(Switch[2]))
          {
            case 0:
            case 'S':
              ArcMetadata=ARCMETA_SAVE;
              break;
            case 'R':
              ArcMetadata=ARCMETA_RESTORE;
              break;
            default:
              BadSwitch(Switch);
              break;
          }
          break;
        case 'O':
          AddArcOnly=true;
          break;
        case 'P':

          SlashToNative(Switch+2,ArcPath);
          break;
        case 'S':
          SyncFiles=true;
          break;
        default:
          BadSwitch(Switch);
          break;
      }
      break;
    case 'C':
      if (Switch[2]!=0)
      {
          if (wcsicomp(Switch+1,L"FG-")==0)
            ConfigDisabled=true;
          else
            BadSwitch(Switch);
      }
      else
        switch(toupperw(Switch[1]))
        {
          case '-':
            DisableComment=true;
            break;
          case 'U':
            ConvertNames=NAMES_UPPERCASE;
            break;
          case 'L':
            ConvertNames=NAMES_LOWERCASE;
            break;
          default:
            BadSwitch(Switch);
            break;
        }
      break;
    case 'D':
      if (Switch[2]!=0)
        BadSwitch(Switch);
      else
        switch(toupperw(Switch[1]))
        {
          case 'S':
            DisableSortSolid=true;
            break;
          case 'H':
            OpenShared=true;
            break;
          case 'F':
            DeleteFiles=true;
            break;
          default:
            BadSwitch(Switch);
            break;
        }
      break;
    case 'E':
      switch(toupperw(Switch[1]))
      {
        case 'P':
          switch(Switch[2])
          {
            case 0:
              ExclPath=EXCL_SKIPWHOLEPATH;
              break;
            case '1':
              ExclPath=EXCL_BASEPATH;
              break;
            case '2':
              ExclPath=EXCL_SAVEFULLPATH;
              break;
            case '3':
              ExclPath=EXCL_ABSPATH;
              break;
            case '4':

              SlashToNative(Switch+3,ExclArcPath);
              break;
            default:
              BadSwitch(Switch);
              break;
          }
          break;
        default:
          if (Switch[1]=='+')
          {
            InclFileAttr|=GetExclAttr(Switch+2,InclDir);
            InclAttrSet=true;
          }
          else
            ExclFileAttr|=GetExclAttr(Switch+1,ExclDir);
          break;
      }
      break;
    case 'F':
      if (Switch[1]==0)
        FreshFiles=true;
      else
        BadSwitch(Switch);
      break;
    case 'H':
      switch (toupperw(Switch[1]))
      {
        case 'P':
          EncryptHeaders=true;
          if (Switch[2]!=0)
          {
            if (wcslen(Switch+2)>=MAXPASSWORD)
              uiMsg(UIERROR_TRUNCPSW,MAXPASSWORD-1);
            Password.Set(Switch+2);
            cleandata((void *)Switch,wcslen(Switch)*sizeof(Switch[0]));
          }
          else
            if (!Password.IsSet())
            {
              uiGetPassword(UIPASSWORD_GLOBAL,L"",&Password,NULL);
              eprintf(L"\n");
            }
          break;
        default :
          BadSwitch(Switch);
          break;
      }
      break;
    case 'I':
      if (wcsnicomp(Switch+1,L"LOG",3)==0)
      {
        LogName=Switch[4]!=0 ? Switch+4:DefLogName;
        break;
      }
      if (wcsnicomp(Switch+1,L"SND",3)==0)
      {
        Sound=Switch[4]=='-' ? SOUND_NOTIFY_OFF : SOUND_NOTIFY_ON;
        break;
      }
      if (wcsicomp(Switch+1,L"ERR")==0)
      {
        MsgStream=MSG_STDERR;

        SetConsoleMsgStream(MSG_STDERR);
        break;
      }
      if (wcsnicomp(Switch+1,L"EML",3)==0)
      {
        EmailTo=Switch[4]!=0 ? Switch+4:L"@";
        break;
      }
      if (wcsicomp(Switch+1,L"M")==0)
      {
        VerboseOutput=true;
        break;
      }
      if (wcsicomp(Switch+1,L"NUL")==0)
      {
        MsgStream=MSG_NULL;
        SetConsoleMsgStream(MSG_NULL);
        break;
      }
      if (toupperw(Switch[1])=='D')
      {
        for (uint I=2;Switch[I]!=0;I++)
          switch(toupperw(Switch[I]))
          {
            case 'Q':
              MsgStream=MSG_ERRONLY;
              SetConsoleMsgStream(MSG_ERRONLY);
              break;
            case 'C':
              DisableCopyright=true;
              break;
            case 'D':
              DisableDone=true;
              break;
            case 'P':
              DisablePercentage=true;
              break;
            case 'N':
              DisableNames=true;
              break;
            case 'V':
              VerboseOutput=true;
              break;
          }
        break;
      }
      if (wcsnicomp(Switch+1,L"OFF",3)==0)
      {
        switch(Switch[4])
        {
          case 0:
          case '1':
            Shutdown=POWERMODE_OFF;
            break;
          case '2':
            Shutdown=POWERMODE_HIBERNATE;
            break;
          case '3':
            Shutdown=POWERMODE_SLEEP;
            break;
          case '4':
            Shutdown=POWERMODE_RESTART;
            break;
        }
        break;
      }
      if (wcsicomp(Switch+1,L"VER")==0)
      {
        PrintVersion=true;
        break;
      }
      break;
    case 'K':
      switch(toupperw(Switch[1]))
      {
        case 'B':
          KeepBroken=true;
          break;
        case 0:
          Lock=true;
          break;
      }
      break;
    case 'M':
      switch(toupperw(Switch[1]))
      {
        case 'C':
          {
            const wchar *Str=Switch+2;
            if (*Str=='-')
              for (uint I=0;I<ASIZE(FilterModes);I++)
                FilterModes[I].State=FILTER_DISABLE;
            else
              while (*Str!=0)
              {
                int Param1=0,Param2=0;
                FilterState State=FILTER_AUTO;
                FilterType Type=FILTER_NONE;
                if (IsDigit(*Str))
                {
                  Param1=atoiw(Str);
                  while (IsDigit(*Str))
                    Str++;
                }
                if (*Str==':' && IsDigit(Str[1]))
                {
                  Param2=atoiw(++Str);
                  while (IsDigit(*Str))
                    Str++;
                }
                switch(toupperw(*(Str++)))
                {

                  case 'E': Type=FILTER_E8;          break;
                  case 'D': Type=FILTER_DELTA;       break;

                  case 'L': Type=FILTER_LONGRANGE;   break;
                  case 'X': Type=FILTER_EXHAUSTIVE;  break;
                }
                if (*Str=='+' || *Str=='-')
                  State=*(Str++)=='+' ? FILTER_FORCE:FILTER_DISABLE;
                FilterModes[Type].State=State;
                FilterModes[Type].Param1=Param1;
                FilterModes[Type].Param2=Param2;
              }
            }
          break;
        case 'D':
          {
            bool SetDictLimit=toupperw(Switch[2])=='X';

            uint64 Size=atoiw(Switch+(SetDictLimit ? 3 : 2));
            wchar LastChar=toupperw(Switch[wcslen(Switch)-1]);
            if (IsDigit(LastChar))
              LastChar=SetDictLimit ? 'G':'M';
            switch(LastChar)
            {
              case 'K':
                Size*=1024;
                break;
              case 'M':
                Size*=1024*1024;
                break;
              case 'G':
                Size*=1024*1024*1024;
                break;
              default:
                BadSwitch(Switch);
            }

            uint Flags;
            if ((Size=Archive::GetWinSize(Size,Flags))==0 ||
                Size<=0x100000000ULL && !IsPow2(Size))
              BadSwitch(Switch);
            else
              if (SetDictLimit)
                WinSizeLimit=Size;
              else
              {
                WinSize=Size;
              }
          }
          break;
        case 'E':
          if (toupperw(Switch[2])=='S' && Switch[3]==0)
            SkipEncrypted=true;
          break;
        case 'L':
          if (toupperw(Switch[2])=='P')
          {
            UseLargePages=true;
            if (!LargePageAlloc::IsPrivilegeAssigned() && LargePageAlloc::AssignConfirmation())
            {
              LargePageAlloc::AssignPrivilege();

              SetupComplete=true;
            }
          }
          break;
        case 'M':
          break;
        case 'S':
          GetBriefMaskList(Switch[2]==0 ? DefaultStoreList:Switch+2,StoreArgs);
          break;
#ifdef RAR_SMP
        case 'T':
          Threads=atoiw(Switch+2);
          if (Threads>MaxPoolThreads || Threads<1)
            BadSwitch(Switch);
          break;
#endif
        default:
          Method=Switch[1]-'0';
          if (Method>5 || Method<0)
            BadSwitch(Switch);
          break;
      }
      break;
    case 'N':
    case 'X':
      if (Switch[1]!=0)
      {
        StringList *Args=toupperw(Switch[0])=='N' ? &InclArgs:&ExclArgs;
        if (Switch[1]=='@' && !IsWildcard(Switch))
          ReadTextFile(Switch+2,Args,false,true,FilelistCharset,true,true,true);
        else
          Args->AddString(Switch+1);
      }
      break;
    case 'O':
      switch(toupperw(Switch[1]))
      {
        case '+':
          Overwrite=OVERWRITE_ALL;
          break;
        case '-':
          Overwrite=OVERWRITE_NONE;
          break;
        case 0:
          Overwrite=OVERWRITE_FORCE_ASK;
          break;
#ifdef _WIN_ALL
        case 'C':
          SetCompressedAttr=true;
          break;
#endif
        case 'H':
          SaveHardLinks=true;
          break;

#ifdef SAVE_LINKS
        case 'L':
          SaveSymLinks=true;
          for (uint I=2;Switch[I]!=0;I++)
            switch(toupperw(Switch[I]))
            {
              case 'A':
                AbsoluteLinks=true;
                break;
              case '-':
                SkipSymLinks=true;
                break;
              default:
                BadSwitch(Switch);
                break;
            }
          break;
#endif
#ifdef PROPAGATE_MOTW
        case 'M':
          {
            MotwAllFields=Switch[2]=='1';
            const wchar *Sep=wcschr(Switch+2,'=');
            if (Switch[2]=='-')
              MotwList.Reset();
            else
              GetBriefMaskList(Sep==nullptr ? L"*":Sep+1,MotwList);
          }
          break;
#endif
#ifdef _WIN_ALL
        case 'N':
          if (toupperw(Switch[2])=='I')
            AllowIncompatNames=true;
          break;
#endif
        case 'P':
          ExtrPath=Switch+2;
          AddEndSlash(ExtrPath);
          break;
        case 'R':
          Overwrite=OVERWRITE_AUTORENAME;
          break;
#ifdef _WIN_ALL
        case 'S':
          SaveStreams=true;
          break;
#endif
        case 'W':
          ProcessOwners=true;
          break;
        default :
          BadSwitch(Switch);
          break;
      }
      break;
    case 'P':
      if (Switch[1]==0)
      {
        uiGetPassword(UIPASSWORD_GLOBAL,L"",&Password,NULL);
        eprintf(L"\n");
      }
      else
      {
        if (wcslen(Switch+1)>=MAXPASSWORD)
          uiMsg(UIERROR_TRUNCPSW,MAXPASSWORD-1);
        Password.Set(Switch+1);
        cleandata((void *)Switch,wcslen(Switch)*sizeof(Switch[0]));
      }
      break;
#ifndef SFX_MODULE
    case 'Q':
      if (toupperw(Switch[1])=='O')
        switch(toupperw(Switch[2]))
        {
          case 0:
            QOpenMode=QOPEN_AUTO;
            break;
          case '-':
            QOpenMode=QOPEN_NONE;
            break;
          case '+':
            QOpenMode=QOPEN_ALWAYS;
            break;
          default:
            BadSwitch(Switch);
            break;
        }
      else
        BadSwitch(Switch);
      break;
#endif
    case 'R':
      switch(toupperw(Switch[1]))
      {
        case 0:
          Recurse=RECURSE_ALWAYS;
          break;
        case '-':
          Recurse=RECURSE_DISABLE;
          break;
        case '0':
          Recurse=RECURSE_WILDCARDS;
          break;
        case 'I':
          {
            Priority=atoiw(Switch+2);
            if (Priority<0 || Priority>15)
              BadSwitch(Switch);
            const wchar *ChPtr=wcschr(Switch+2,':');
            if (ChPtr!=NULL)
            {
              SleepTime=atoiw(ChPtr+1);
              if (SleepTime>1000)
                BadSwitch(Switch);
              InitSystemOptions(SleepTime);
            }
            SetPriority(Priority);
          }
          break;
      }
      break;
    case 'S':
      if (IsDigit(Switch[1]))
      {
        Solid|=SOLID_COUNT;
        SolidCount=atoiw(&Switch[1]);
      }
      else
        switch(toupperw(Switch[1]))
        {
          case 0:
            Solid|=SOLID_NORMAL;
            break;
          case '-':
            Solid=SOLID_NONE;
            break;
          case 'E':
            Solid|=SOLID_FILEEXT;
            break;
          case 'V':
            Solid|=Switch[2]=='-' ? SOLID_VOLUME_DEPENDENT:SOLID_VOLUME_INDEPENDENT;
            break;
          case 'D':
            Solid|=SOLID_VOLUME_DEPENDENT;
            break;
          case 'I':
            ProhibitConsoleInput();
            UseStdin=Switch[2] ? Switch+2:L"stdin";
            break;
          case 'L':
            if (IsDigit(Switch[2]))
              FileSizeLess=GetVolSize(Switch+2,1);
            break;
          case 'M':
            if (IsDigit(Switch[2]))
              FileSizeMore=GetVolSize(Switch+2,1);
            break;
          case 'C':
            {
              bool AlreadyBad=false;

              RAR_CHARSET rch=RCH_DEFAULT;
              switch(toupperw(Switch[2]))
              {
                case 'A':
                  rch=RCH_ANSI;
                  break;
                case 'O':
                  rch=RCH_OEM;
                  break;
                case 'U':
                  rch=RCH_UNICODE;
                  break;
                case 'F':
                  rch=RCH_UTF8;
                  break;
                default :
                  BadSwitch(Switch);
                  AlreadyBad=true;
                  break;
              };
              if (!AlreadyBad)
                if (Switch[3]==0)
                  CommentCharset=FilelistCharset=ErrlogCharset=RedirectCharset=rch;
                else
                  for (uint I=3;Switch[I]!=0 && !AlreadyBad;I++)
                    switch(toupperw(Switch[I]))
                    {
                      case 'C':
                        CommentCharset=rch;
                        break;
                      case 'L':
                        FilelistCharset=rch;
                        break;
                      case 'R':
                        RedirectCharset=rch;
                        break;
                      default:
                        BadSwitch(Switch);
                        AlreadyBad=true;
                        break;
                    }

              SetConsoleRedirectCharset(RedirectCharset);
            }
            break;

        }
      break;
    case 'T':
      switch(toupperw(Switch[1]))
      {
        case 'K':
          ArcTime=ARCTIME_KEEP;
          break;
        case 'L':
          ArcTime=ARCTIME_LATEST;
          break;
        case 'O':
          SetTimeFilters(Switch+2,true,true);
          break;
        case 'N':
          SetTimeFilters(Switch+2,false,true);
          break;
        case 'B':
          SetTimeFilters(Switch+2,true,false);
          break;
        case 'A':
          SetTimeFilters(Switch+2,false,false);
          break;
        case 'S':
          SetStoreTimeMode(Switch+2);
          break;
        case '-':
          Test=false;
          break;
        case 0:
          Test=true;
          break;
        default:
          BadSwitch(Switch);
          break;
      }
      break;
    case 'U':
      if (Switch[1]==0)
        UpdateFiles=true;
      else
        BadSwitch(Switch);
      break;
    case 'V':
      switch(toupperw(Switch[1]))
      {
        case 'P':
          VolumePause=true;
          break;
        case 'E':
          if (toupperw(Switch[2])=='R')
            VersionControl=atoiw(Switch+3)+1;
          break;
        case '-':
          VolSize=0;
          break;
        default:
          VolSize=VOLSIZE_AUTO;
          break;
      }
      break;
    case 'W':
      TempPath=Switch+1;
      AddEndSlash(TempPath);
      break;
    case 'Y':
      AllYes=true;
      break;
    case 'Z':
      if (Switch[1]==0)
      {

        CommentFile=L"stdin";
      }
      else
        CommentFile=Switch+1;
      break;
    case '?' :
      OutHelp(RARX_SUCCESS);
      break;
    default :
      BadSwitch(Switch);
      break;
  }
}
#endif

#if !defined(SFX_MODULE)
void CommandData::BadSwitch(const wchar *Switch)
{
  mprintf(St(MUnknownOption),Switch);
  ErrHandler.Exit(RARX_USERERROR);
}
#endif

void CommandData::ProcessCommand()
{
#ifndef SFX_MODULE

  const wchar *SingleCharCommands=L"FUADPXETK";

  if (Command.empty() && UseLargePages || SetupComplete)
    return;

  if (Command[0]!=0 && Command[1]!=0 && wcschr(SingleCharCommands,Command[0])!=NULL || ArcName.empty())
    OutHelp(Command.empty() ? RARX_SUCCESS:RARX_USERERROR);

  size_t ExtPos=GetExtPos(ArcName);
#ifdef _UNIX

  if (ExtPos==std::wstring::npos && (!FileExist(ArcName) || IsDir(GetFileAttr(ArcName))))
    ArcName+=L".rar";
#else
  if (ExtPos==std::wstring::npos)
    ArcName+=L".rar";
#endif

  if (ExtPos!=std::wstring::npos && wcsnicomp(&ArcName[ExtPos],L".part",5)==0 &&
      IsDigit(ArcName[ExtPos+5]) && !FileExist(ArcName))
  {
    std::wstring Name=ArcName+L".rar";
    if (FileExist(Name))
      ArcName=Name;
  }

  if (wcschr(L"AFUMD",Command[0])==NULL && UseStdin.empty())
  {
    if (GenerateArcName)
    {
      const wchar *Mask=*GenerateMask!=0 ? GenerateMask:DefGenerateMask;
      GenerateArchiveName(ArcName,Mask,false);
    }

    StringList ArcMasks;
    ArcMasks.AddString(ArcName);
    ScanTree Scan(&ArcMasks,Recurse,SaveSymLinks,SCAN_SKIPDIRS);
    FindData FindData;
    while (Scan.GetNext(&FindData)==SCAN_SUCCESS)
      AddArcName(FindData.Name);
  }
  else
    AddArcName(ArcName);
#endif

  switch(Command[0])
  {
    case 'P':
    case 'X':
    case 'E':
    case 'T':
      {
        CmdExtract Extract(this);
        Extract.DoExtract();
      }
      break;
#ifndef SILENT
    case 'V':
    case 'L':
      ListArchive(this);
      break;
    default:
      OutHelp(RARX_USERERROR);
#endif
  }

  if (!BareOutput)
    if (MsgStream==MSG_ERRONLY && IsConsoleOutputPresent())
      eprintf(L"\n");
    else
      mprintf(L"\n");
}

void CommandData::AddArcName(const std::wstring &Name)
{
  ArcNames.AddString(Name);
}

bool CommandData::GetArcName(wchar *Name,int MaxSize)
{
  return ArcNames.GetString(Name,MaxSize);
}

bool CommandData::GetArcName(std::wstring &Name)
{
  return ArcNames.GetString(Name);
}

bool CommandData::IsSwitch(int Ch)
{
#ifdef _WIN_ALL
  return Ch=='-' || Ch=='/';
#else
  return Ch=='-';
#endif
}

#ifndef SFX_MODULE
uint CommandData::GetExclAttr(const wchar *Str,bool &Dir)
{
  if (IsDigit(*Str))
    return wcstol(Str,NULL,0);

  uint Attr=0;
  while (*Str!=0)
  {
    switch(toupperw(*Str))
    {
      case 'D':
        Dir=true;
        break;
#ifdef _UNIX
      case 'V':
        Attr|=S_IFCHR;
        break;
#elif defined(_WIN_ALL)
      case 'R':
        Attr|=0x1;
        break;
      case 'H':
        Attr|=0x2;
        break;
      case 'S':
        Attr|=0x4;
        break;
      case 'A':
        Attr|=0x20;
        break;
#endif
    }
    Str++;
  }
  return Attr;
}
#endif

#ifndef SFX_MODULE
void CommandData::ReportWrongSwitches(RARFORMAT Format)
{
  if (Format==RARFMT15)
  {
    if (HashType!=HASH_CRC32)
      uiMsg(UIERROR_INCOMPATSWITCH,L"-ht",4);
#ifdef _WIN_ALL
    if (SaveSymLinks)
      uiMsg(UIERROR_INCOMPATSWITCH,L"-ol",4);
#endif
    if (SaveHardLinks)
      uiMsg(UIERROR_INCOMPATSWITCH,L"-oh",4);

#ifdef _WIN_ALL

#endif
    if (QOpenMode!=QOPEN_AUTO)
      uiMsg(UIERROR_INCOMPATSWITCH,L"-qo",4);
  }
  if (Format==RARFMT50)
  {
  }
}
#endif

int64 CommandData::GetVolSize(const wchar *S,uint DefMultiplier)
{
  int64 Size=0,FloatingDivider=0;
  for (uint I=0;S[I]!=0;I++)
    if (IsDigit(S[I]))
    {
      Size=Size*10+S[I]-'0';
      FloatingDivider*=10;
    }
    else
      if (S[I]=='.')
        FloatingDivider=1;

  if (*S!=0)
  {
    const wchar *ModList=L"bBkKmMgGtT";
    const wchar *Mod=wcschr(ModList,S[wcslen(S)-1]);
    if (Mod==NULL)
      Size*=DefMultiplier;
    else
      for (ptrdiff_t I=2;I<=Mod-ModList;I+=2)
        Size*=((Mod-ModList)&1)!=0 ? 1000:1024;
  }
  if (FloatingDivider!=0)
    Size/=FloatingDivider;
  return Size;
}

void CommandData::GetBriefMaskList(const std::wstring &Masks,StringList &Args)
{
  size_t Pos=0;
  while (Pos<Masks.size())
  {
    if (Masks[Pos]=='.')
      Pos++;
    size_t EndPos=Masks.find(';',Pos);
    std::wstring Mask=Masks.substr(Pos,EndPos==std::wstring::npos ? EndPos:EndPos-Pos);
    if (Mask.find_first_of(L"*?.")==std::wstring::npos)
      Mask.insert(0,L"*.");
    Args.AddString(Mask);
    if (EndPos==std::wstring::npos)
      break;
    Pos=EndPos+1;
  }
}

void InitLogOptions(const std::wstring &LogFileName,RAR_CHARSET CSet)
{
}

void CloseLogOptions()
{
}

#ifndef SILENT
void Log(const wchar *ArcName,const wchar *fmt,...)
{

  int Code=ErrHandler.GetSystemErrorCode();

  uiAlarm(UIALARM_ERROR);

  va_list arglist;
  va_start(arglist,fmt);

  std::wstring s=vwstrprintf(fmt,arglist);

  ReplaceEsc(s);

  va_end(arglist);
  eprintf(L"%ls",s.c_str());
  ErrHandler.SetSystemErrorCode(Code);
}
#endif

static MESSAGE_TYPE MsgStream=MSG_STDOUT;
static RAR_CHARSET RedirectCharset=RCH_DEFAULT;
static bool ProhibitInput=false;
static bool ConsoleOutputPresent=false;

static bool StdoutRedirected=false,StderrRedirected=false,StdinRedirected=false;

#ifdef _WIN_ALL
static bool IsRedirected(DWORD nStdHandle)
{
  HANDLE hStd=GetStdHandle(nStdHandle);
  DWORD Mode;
  return GetFileType(hStd)!=FILE_TYPE_CHAR || GetConsoleMode(hStd,&Mode)==0;
}
#endif

void InitConsole()
{
#ifdef _WIN_ALL

  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  StdoutRedirected=IsRedirected(STD_OUTPUT_HANDLE);
  StderrRedirected=IsRedirected(STD_ERROR_HANDLE);
  StdinRedirected=IsRedirected(STD_INPUT_HANDLE);
#ifdef _MSC_VER
  if (!StdoutRedirected)
    _setmode(_fileno(stdout), _O_U16TEXT);
  if (!StderrRedirected)
    _setmode(_fileno(stderr), _O_U16TEXT);
#endif
#elif defined(_UNIX)
  StdoutRedirected=!isatty(fileno(stdout));
  StderrRedirected=!isatty(fileno(stderr));
  StdinRedirected=!isatty(fileno(stdin));
#endif
}

void SetConsoleMsgStream(MESSAGE_TYPE MsgStream)
{
  ::MsgStream=MsgStream;
}

void SetConsoleRedirectCharset(RAR_CHARSET RedirectCharset)
{
  ::RedirectCharset=RedirectCharset;
}

void ProhibitConsoleInput()
{
  ProhibitInput=true;
}

#ifndef SILENT
static void cvt_wprintf(FILE *dest,const wchar *fmt,va_list arglist)
{
  ConsoleOutputPresent=true;

  std::wstring s=vwstrprintf(fmt,arglist);

  ReplaceEsc(s);

#ifdef _WIN_ALL
  if (dest==stdout && StdoutRedirected || dest==stderr && StderrRedirected)
  {
    HANDLE hOut=GetStdHandle(dest==stdout ? STD_OUTPUT_HANDLE:STD_ERROR_HANDLE);
    DWORD Written;
    if (RedirectCharset==RCH_UNICODE)
      WriteFile(hOut,s.data(),(DWORD)s.size()*sizeof(s[0]),&Written,NULL);
    else
    {

      std::string MsgA;
      if (RedirectCharset==RCH_UTF8)
        WideToUtf(s,MsgA);
      else
        WideToChar(s,MsgA);
      if (RedirectCharset==RCH_DEFAULT || RedirectCharset==RCH_OEM)
        CharToOemA(&MsgA[0],&MsgA[0]);

      WriteFile(hOut,MsgA.data(),(DWORD)MsgA.size(),&Written,NULL);
    }
    return;
  }

  HANDLE hOut=GetStdHandle(dest==stderr ? STD_ERROR_HANDLE:STD_OUTPUT_HANDLE);
  DWORD Written;
  WriteConsole(hOut,s.data(),(DWORD)s.size(),&Written,NULL);
#else
  fputws(s.c_str(),dest);

  fflush(dest);
#endif
}

void mprintf(const wchar *fmt,...)
{
  if (MsgStream==MSG_NULL || MsgStream==MSG_ERRONLY)
    return;

  fflush(stderr);

  va_list arglist;
  va_start(arglist,fmt);
  FILE *dest=MsgStream==MSG_STDERR ? stderr:stdout;
  cvt_wprintf(dest,fmt,arglist);
  va_end(arglist);
}
#endif

#ifndef SILENT
void eprintf(const wchar *fmt,...)
{
  if (MsgStream==MSG_NULL)
    return;

  fflush(stdout);

  va_list arglist;
  va_start(arglist,fmt);
  cvt_wprintf(stderr,fmt,arglist);
  va_end(arglist);
}
#endif

#ifndef SILENT
static void QuitIfInputProhibited()
{

  if (ProhibitInput)
  {
    mprintf(St(MStdinNoInput));
    ErrHandler.Exit(RARX_FATAL);
  }
}

static void GetPasswordText(std::wstring &Str)
{
  QuitIfInputProhibited();
  if (StdinRedirected)
    getwstr(Str);
  else
  {
#ifdef _WIN_ALL
    HANDLE hConIn=GetStdHandle(STD_INPUT_HANDLE);
    DWORD ConInMode;
    GetConsoleMode(hConIn,&ConInMode);
    SetConsoleMode(hConIn,ENABLE_LINE_INPUT);

    std::vector<wchar> Buf(MAXPASSWORD);

    DWORD Read=0;
    ReadConsole(hConIn,Buf.data(),(DWORD)Buf.size()-1,&Read,NULL);
    Buf[Read]=0;
    Str=Buf.data();
    cleandata(Buf.data(),Buf.size()*sizeof(Buf[0]));

    SetConsoleMode(hConIn,ConInMode);

#else
    std::vector<char> StrA(MAXPASSWORD*4);
#ifdef __VMS
    fgets(StrA.data(),StrA.size()-1,stdin);
#elif defined(__sun)
    strncpyz(StrA.data(),getpassphrase(""),StrA.size());
#else
    strncpyz(StrA.data(),getpass(""),StrA.size());
#endif
    CharToWide(StrA.data(),Str);
    cleandata(StrA.data(),StrA.size()*sizeof(StrA[0]));
#endif
  }
  RemoveLF(Str);
}
#endif

#ifndef SILENT
bool GetConsolePassword(UIPASSWORD_TYPE Type,const std::wstring &FileName,SecPassword *Password)
{
  if (!StdinRedirected)
    uiAlarm(UIALARM_QUESTION);

  while (true)
  {

      if (Type==UIPASSWORD_GLOBAL)
        eprintf(L"\n%s: ",St(MAskPsw));
      else
        eprintf(St(MAskPswFor),FileName.c_str());

    std::wstring PlainPsw;
    GetPasswordText(PlainPsw);
    if (PlainPsw.empty() && Type==UIPASSWORD_GLOBAL)
      return false;
    if (PlainPsw.size()>=MAXPASSWORD)
    {
      PlainPsw.erase(MAXPASSWORD-1);
      uiMsg(UIERROR_TRUNCPSW,MAXPASSWORD-1);
    }
    if (!StdinRedirected && Type==UIPASSWORD_GLOBAL)
    {
      eprintf(St(MReAskPsw));
      std::wstring CmpStr;
      GetPasswordText(CmpStr);
      if (CmpStr.empty() || PlainPsw!=CmpStr)
      {
        eprintf(St(MNotMatchPsw));
        cleandata(&PlainPsw[0],PlainPsw.size()*sizeof(PlainPsw[0]));
        cleandata(&CmpStr[0],CmpStr.size()*sizeof(CmpStr[0]));
        continue;
      }
      cleandata(&CmpStr[0],CmpStr.size()*sizeof(CmpStr[0]));
    }
    Password->Set(PlainPsw.c_str());
    cleandata(&PlainPsw[0],PlainPsw.size()*sizeof(PlainPsw[0]));
    break;
  }
  return true;
}
#endif

#ifndef SILENT
void getwstr(std::wstring &str)
{

  fflush(stderr);

  QuitIfInputProhibited();

  str.clear();

  const size_t MaxRead=MAXPATHSIZE;

#if defined(_WIN_ALL)

  if (StdinRedirected)
  {

    std::vector<char> StrA(MaxRead*4);
    File SrcFile;
    SrcFile.SetHandleType(FILE_HANDLESTD);
    SrcFile.SetLineInputMode(true);
    int ReadSize=SrcFile.Read(&StrA[0],StrA.size()-1);
    if (ReadSize<=0)
    {

     ErrHandler.ReadError(L"stdin");
    }
    StrA[ReadSize]=0;

    CharToWide(&StrA[0],str);
    cleandata(&StrA[0],StrA.size());
  }
  else
  {
    std::vector<wchar> Buf(MaxRead);
    DWORD SizeToRead=(DWORD)Buf.size()-1;

    if (WinNT()<=WNT_W10)
      SizeToRead=Min(SizeToRead,0x4000);

    DWORD ReadSize=0;
    if (ReadConsole(GetStdHandle(STD_INPUT_HANDLE),&Buf[0],SizeToRead,&ReadSize,NULL)==0)
      ErrHandler.ReadError(L"stdin");
    Buf[ReadSize]=0;
    str=Buf.data();
  }
#else
  std::vector<wchar> Buf(MaxRead);
  if (fgetws(&Buf[0],Buf.size(),stdin)==NULL)
    ErrHandler.ReadError(L"stdin");
  str=Buf.data();
#endif
  RemoveLF(str);
}
#endif

#ifndef SILENT

int Ask(const wchar *AskStr)
{
  uiAlarm(UIALARM_QUESTION);

  const uint MaxItems=10;
  wchar Item[MaxItems][40];
  uint ItemKeyPos[MaxItems],NumItems=0;

  for (const wchar *NextItem=AskStr;NextItem!=nullptr;NextItem=wcschr(NextItem+1,'_'))
  {
    wchar *CurItem=Item[NumItems];
    wcsncpyz(CurItem,NextItem+1,ASIZE(Item[0]));
    wchar *EndItem=wcschr(CurItem,'_');
    if (EndItem!=nullptr)
      *EndItem=0;
    uint KeyPos=0,CurKey;
    while ((CurKey=CurItem[KeyPos])!=0)
    {
      bool Found=false;
      for (uint I=0;I<NumItems && !Found;I++)
        if (toupperw(Item[I][ItemKeyPos[I]])==toupperw(CurKey))
          Found=true;
      if (!Found && CurKey!=' ')
        break;
      KeyPos++;
    }
    ItemKeyPos[NumItems]=KeyPos;
    NumItems++;
  }

  for (uint I=0;I<NumItems;I++)
  {
    eprintf(I==0 ? (NumItems>3 ? L"\n":L" "):L", ");
    uint KeyPos=ItemKeyPos[I];
    for (uint J=0;J<KeyPos;J++)
      eprintf(L"%c",Item[I][J]);
    eprintf(L"[%c]%ls",Item[I][KeyPos],&Item[I][KeyPos+1]);
  }
  eprintf(L" ");
  std::wstring Str;
  getwstr(Str);
  wchar Ch=toupperw(Str[0]);
  for (uint I=0;I<NumItems;I++)
    if (Ch==Item[I][ItemKeyPos[I]])
      return I+1;
  return 0;
}
#endif

static bool IsCommentUnsafe(const std::wstring &Data)
{
  for (size_t I=0;I<Data.size();I++)
    if (Data[I]==27 && Data[I+1]=='[')
      for (size_t J=I+2;J<Data.size();J++)
      {

        if (Data[J]=='\"')
          return true;
        if (!IsDigit(Data[J]) && Data[J]!=';')
          break;
      }
  return false;
}

void OutComment(const std::wstring &Comment)
{
  if (IsCommentUnsafe(Comment))
    return;
  const size_t MaxOutSize=0x400;
  for (size_t I=0;I<Comment.size();I+=MaxOutSize)
  {
    size_t CopySize=Min(MaxOutSize,Comment.size()-I);
    mprintf(L"%s",Comment.substr(I,CopySize).c_str());
  }
  mprintf(L"\n");
}

bool IsConsoleOutputPresent()
{
  return ConsoleOutputPresent;
}

#ifndef SFX_MODULE

#define USE_SLICING
#endif

static uint crc_tables[16][256];

#ifdef USE_NEON_CRC32
static bool CRC_Neon;
#endif

void InitCRC32(uint *CRCTab)
{
  if (CRCTab[1]!=0)
    return;
  for (uint I=0;I<256;I++)
  {
    uint C=I;
    for (uint J=0;J<8;J++)
      C=(C & 1) ? (C>>1)^0xEDB88320 : (C>>1);
    CRCTab[I]=C;
  }

#ifdef USE_NEON_CRC32
  #ifdef _APPLE

    uint Value=0;
    size_t Size=sizeof(Value);
    int RetCode=sysctlbyname("hw.optional.armv8_crc32",&Value,&Size,NULL,0);
    CRC_Neon=RetCode==0 && Value!=0;
  #else
    CRC_Neon=(getauxval(AT_HWCAP) & HWCAP_CRC32)!=0;
  #endif
#endif

}

static void InitTables()
{
  InitCRC32(crc_tables[0]);

#ifdef USE_SLICING
  for (uint I=0;I<256;I++)
  {
    uint C=crc_tables[0][I];
    for (uint J=1;J<16;J++)
    {
      C=crc_tables[0][(byte)C]^(C>>8);
      crc_tables[J][I]=C;
    }
  }
#endif
}

struct CallInitCRC {CallInitCRC() {InitTables();}} static CallInit32;

uint CRC32(uint StartCRC,const void *Addr,size_t Size)
{
  byte *Data=(byte *)Addr;

#ifdef USE_NEON_CRC32
  if (CRC_Neon)
  {
    for (;Size>=8;Size-=8,Data+=8)
#ifdef __clang__
      StartCRC = __builtin_arm_crc32d(StartCRC, RawGet8(Data));
#else
      StartCRC = __builtin_aarch64_crc32x(StartCRC, RawGet8(Data));
#endif
    for (;Size>0;Size--,Data++)
#ifdef __clang__
      StartCRC = __builtin_arm_crc32b(StartCRC, *Data);
#else
      StartCRC = __builtin_aarch64_crc32b(StartCRC, *Data);
#endif
    return StartCRC;
  }
#endif

#ifdef USE_SLICING

  for (;Size>0 && ((size_t)Data & 15)!=0;Size--,Data++)
    StartCRC=crc_tables[0][(byte)(StartCRC^Data[0])]^(StartCRC>>8);

  for (;Size>=16;Size-=16,Data+=16)
  {
#ifdef BIG_ENDIAN
    StartCRC ^= RawGet4(Data);
    uint D1 = RawGet4(Data+4);
    uint D2 = RawGet4(Data+8);
    uint D3 = RawGet4(Data+12);
#else

    StartCRC ^= *(uint32 *) Data;
    uint D1 = *(uint32 *) (Data+4);
    uint D2 = *(uint32 *) (Data+8);
    uint D3 = *(uint32 *) (Data+12);
#endif
    StartCRC = crc_tables[15][(byte) StartCRC       ] ^
               crc_tables[14][(byte)(StartCRC >> 8) ] ^
               crc_tables[13][(byte)(StartCRC >> 16)] ^
               crc_tables[12][(byte)(StartCRC >> 24)] ^
               crc_tables[11][(byte) D1             ] ^
               crc_tables[10][(byte)(D1       >> 8) ] ^
               crc_tables[ 9][(byte)(D1       >> 16)] ^
               crc_tables[ 8][(byte)(D1       >> 24)] ^
               crc_tables[ 7][(byte) D2             ] ^
               crc_tables[ 6][(byte)(D2       >>  8)] ^
               crc_tables[ 5][(byte)(D2       >> 16)] ^
               crc_tables[ 4][(byte)(D2       >> 24)] ^
               crc_tables[ 3][(byte) D3             ] ^
               crc_tables[ 2][(byte)(D3       >>  8)] ^
               crc_tables[ 1][(byte)(D3       >> 16)] ^
               crc_tables[ 0][(byte)(D3       >> 24)];
  }
#endif

  for (;Size>0;Size--,Data++)
    StartCRC=crc_tables[0][(byte)(StartCRC^Data[0])]^(StartCRC>>8);

  return StartCRC;
}

#ifndef SFX_MODULE

ushort Checksum14(ushort StartCRC,const void *Addr,size_t Size)
{
  byte *Data=(byte *)Addr;
  for (size_t I=0;I<Size;I++)
  {
    StartCRC=(StartCRC+Data[I])&0xffff;
    StartCRC=((StartCRC<<1)|(StartCRC>>15))&0xffff;
  }
  return StartCRC;
}
#endif

#if 0
static void TestCRC();
struct TestCRCStruct {TestCRCStruct() {TestCRC();exit(0);}} GlobalTesCRC;

void TestCRC()
{

  _SSE_Version=GetSSEVersion();

  const uint FirstSize=300;
  byte b[FirstSize];

  if ((CRC32(0xffffffff,(byte*)"testtesttest",12)^0xffffffff)==0x44608e84)
    mprintf(L"\nCRC32 test1 OK");
  else
    mprintf(L"\nCRC32 test1 FAILED");

  if (CRC32(0,(byte*)"te\x80st",5)==0xB2E5C5AE)
    mprintf(L"\nCRC32 test2 OK");
  else
    mprintf(L"\nCRC32 test2 FAILED");

  for (uint I=0;I<14;I++)
    b[I]=(byte)0x7f+I;
  if ((CRC32(0xffffffff,b,14)^0xffffffff)==0x1DFA75DA)
    mprintf(L"\nCRC32 test3 OK");
  else
    mprintf(L"\nCRC32 test3 FAILED");

  for (uint I=0;I<FirstSize;I++)
    b[I]=(byte)I;
  uint r32=CRC32(0xffffffff,b,FirstSize);
  for (uint I=FirstSize;I<1024;I++)
  {
    b[0]=(byte)I;
    r32=CRC32(r32,b,1);
  }
  if ((r32^0xffffffff)==0xB70B4C26)
    mprintf(L"\nCRC32 test4 OK");
  else
    mprintf(L"\nCRC32 test4 FAILED");

  if ((CRC64(0xffffffffffffffff,(byte*)"testtesttest",12)^0xffffffffffffffff)==0x7B1C2D230EDEB436)
    mprintf(L"\nCRC64 test1 OK");
  else
    mprintf(L"\nCRC64 test1 FAILED");

  if (CRC64(0,(byte*)"te\x80st",5)==0xB5DBF9583A6EED4A)
    mprintf(L"\nCRC64 test2 OK");
  else
    mprintf(L"\nCRC64 test2 FAILED");

  for (uint I=0;I<14;I++)
    b[I]=(byte)0x7f+I;
  if ((CRC64(0xffffffffffffffff,b,14)^0xffffffffffffffff)==0xE019941C05B2820C)
    mprintf(L"\nCRC64 test3 OK");
  else
    mprintf(L"\nCRC64 test3 FAILED");

  for (uint I=0;I<FirstSize;I++)
    b[I]=(byte)I;
  uint64 r64=CRC64(0xffffffffffffffff,b,FirstSize);
  for (uint I=FirstSize;I<1024;I++)
  {
    b[0]=(byte)I;
    r64=CRC64(r64,b,1);
  }
  if ((r64^0xffffffffffffffff)==0xD51FB58DC789C400)
    mprintf(L"\nCRC64 test4 OK");
  else
    mprintf(L"\nCRC64 test4 FAILED");

  const size_t BufSize=0x100000;
  byte *Buf=new byte[BufSize];
  GetRnd(Buf,BufSize);

  clock_t StartTime=clock();
  r32=0xffffffff;
  const uint64 BufCount=5000;
  for (uint I=0;I<BufCount;I++)
    r32=CRC32(r32,Buf,BufSize);
  if (r32!=0)
    mprintf(L"\nCRC32 speed: %llu MB/s",BufCount*CLOCKS_PER_SEC/(clock()-StartTime));

  StartTime=clock();
  DataHash Hash;
  Hash.Init(HASH_CRC32,MaxPoolThreads);
  const uint64 BufCountMT=20000;
  for (uint I=0;I<BufCountMT;I++)
    Hash.Update(Buf,BufSize);
  HashValue Result;
  Hash.Result(&Result);
  mprintf(L"\nCRC32 MT speed: %llu MB/s",BufCountMT*CLOCKS_PER_SEC/(clock()-StartTime));

  StartTime=clock();
  Hash.Init(HASH_BLAKE2,MaxPoolThreads);
  for (uint I=0;I<BufCount;I++)
    Hash.Update(Buf,BufSize);
  Hash.Result(&Result);
  mprintf(L"\nBlake2sp speed: %llu MB/s",BufCount*CLOCKS_PER_SEC/(clock()-StartTime));

  StartTime=clock();
  r64=0xffffffffffffffff;
  for (uint I=0;I<BufCount;I++)
    r64=CRC64(r64,Buf,BufSize);
  if (r64!=0)
    mprintf(L"\nCRC64 speed: %llu MB/s",BufCount*CLOCKS_PER_SEC/(clock()-StartTime));
}
#endif

#ifndef SFX_MODULE
void CryptData::SetKey13(const char *Password)
{
  Key13[0]=Key13[1]=Key13[2]=0;
  for (size_t I=0;Password[I]!=0;I++)
  {
    byte P=Password[I];
    Key13[0]+=P;
    Key13[1]^=P;
    Key13[2]+=P;
    Key13[2]=(byte)rotls(Key13[2],1,8);
  }
}

void CryptData::SetKey15(const char *Password)
{
  InitCRC32(CRCTab);
  uint PswCRC=CRC32(0xffffffff,Password,strlen(Password));
  Key15[0]=PswCRC&0xffff;
  Key15[1]=(PswCRC>>16)&0xffff;
  Key15[2]=Key15[3]=0;
  for (size_t I=0;Password[I]!=0;I++)
  {
    byte P=Password[I];
    Key15[2]^=P^CRCTab[P];
    Key15[3]+=ushort(P+(CRCTab[P]>>16));
  }
}

void CryptData::SetCmt13Encryption()
{
  Method=CRYPT_RAR13;
  Key13[0]=0;
  Key13[1]=7;
  Key13[2]=77;
}

void CryptData::Decrypt13(byte *Data,size_t Count)
{
  while (Count--)
  {
    Key13[1]+=Key13[2];
    Key13[0]+=Key13[1];
    *Data-=Key13[0];
    Data++;
  }
}

void CryptData::Crypt15(byte *Data,size_t Count)
{
  while (Count--)
  {
    Key15[0]+=0x1234;
    Key15[1]^=CRCTab[(Key15[0] & 0x1fe)>>1];
    Key15[2]-=ushort(CRCTab[(Key15[0] & 0x1fe)>>1]>>16);
    Key15[0]^=Key15[2];
    Key15[3]=rotrs(Key15[3]&0xffff,1,16)^Key15[1];
    Key15[3]=rotrs(Key15[3]&0xffff,1,16);
    Key15[0]^=Key15[3];
    *Data^=(byte)(Key15[0]>>8);
    Data++;
  }
}

#define NROUNDS 32

#define substLong(t) ( (uint)SubstTable20[(uint)t&255] | \
           ((uint)SubstTable20[(int)(t>> 8)&255]<< 8) | \
           ((uint)SubstTable20[(int)(t>>16)&255]<<16) | \
           ((uint)SubstTable20[(int)(t>>24)&255]<<24) )

static byte InitSubstTable20[256]={
  215, 19,149, 35, 73,197,192,205,249, 28, 16,119, 48,221,  2, 42,
  232,  1,177,233, 14, 88,219, 25,223,195,244, 90, 87,239,153,137,
  255,199,147, 70, 92, 66,246, 13,216, 40, 62, 29,217,230, 86,  6,
   71, 24,171,196,101,113,218,123, 93, 91,163,178,202, 67, 44,235,
  107,250, 75,234, 49,167,125,211, 83,114,157,144, 32,193,143, 36,
  158,124,247,187, 89,214,141, 47,121,228, 61,130,213,194,174,251,
   97,110, 54,229,115, 57,152, 94,105,243,212, 55,209,245, 63, 11,
  164,200, 31,156, 81,176,227, 21, 76, 99,139,188,127, 17,248, 51,
  207,120,189,210,  8,226, 41, 72,183,203,135,165,166, 60, 98,  7,
  122, 38,155,170, 69,172,252,238, 39,134, 59,128,236, 27,240, 80,
  131,  3, 85,206,145, 79,154,142,159,220,201,133, 74, 64, 20,129,
  224,185,138,103,173,182, 43, 34,254, 82,198,151,231,180, 58, 10,
  118, 26,102, 12, 50,132, 22,191,136,111,162,179, 45,  4,148,108,
  161, 56, 78,126,242,222, 15,175,146, 23, 33,241,181,190, 77,225,
    0, 46,169,186, 68, 95,237, 65, 53,208,253,168,  9, 18,100, 52,
  116,184,160, 96,109, 37, 30,106,140,104,150,  5,204,117,112, 84
};

void CryptData::SetKey20(const char *Password)
{
  InitCRC32(CRCTab);

  char Psw[MAXPASSWORD];
  strncpyz(Psw,Password,ASIZE(Psw));
  size_t PswLength=strlen(Psw);

  Key20[0]=0xD3A3B879L;
  Key20[1]=0x3F6D12F7L;
  Key20[2]=0x7515A235L;
  Key20[3]=0xA4E7F123L;

  memcpy(SubstTable20,InitSubstTable20,sizeof(SubstTable20));
  for (uint J=0;J<256;J++)
    for (size_t I=0;I<PswLength;I+=2)
    {
      uint N1=(byte)CRCTab [ (byte(Password[I])   - J) &0xff];
      uint N2=(byte)CRCTab [ (byte(Password[I+1]) + J) &0xff];
      for (int K=1;N1!=N2;N1=(N1+1)&0xff,K++)
        Swap20(&SubstTable20[N1],&SubstTable20[(N1+I+K)&0xff]);
    }

  if ((PswLength & CRYPT_BLOCK_MASK)!=0)
    for (size_t I=PswLength;I<=(PswLength|CRYPT_BLOCK_MASK);I++)
       Psw[I]=0;

  for (size_t I=0;I<PswLength;I+=CRYPT_BLOCK_SIZE)
    EncryptBlock20((byte *)Psw+I);
}

void CryptData::EncryptBlock20(byte *Buf)
{
  uint A,B,C,D,T,TA,TB;
  A=RawGet4(Buf+0)^Key20[0];
  B=RawGet4(Buf+4)^Key20[1];
  C=RawGet4(Buf+8)^Key20[2];
  D=RawGet4(Buf+12)^Key20[3];
  for(int I=0;I<NROUNDS;I++)
  {
    T=((C+rotls(D,11,32))^Key20[I&3]);
    TA=A^substLong(T);
    T=((D^rotls(C,17,32))+Key20[I&3]);
    TB=B^substLong(T);
    A=C;
    B=D;
    C=TA;
    D=TB;
  }
  RawPut4(C^Key20[0],Buf+0);
  RawPut4(D^Key20[1],Buf+4);
  RawPut4(A^Key20[2],Buf+8);
  RawPut4(B^Key20[3],Buf+12);
  UpdKeys20(Buf);
}

void CryptData::DecryptBlock20(byte *Buf)
{
  byte InBuf[16];
  uint A,B,C,D,T,TA,TB;
  A=RawGet4(Buf+0)^Key20[0];
  B=RawGet4(Buf+4)^Key20[1];
  C=RawGet4(Buf+8)^Key20[2];
  D=RawGet4(Buf+12)^Key20[3];
  memcpy(InBuf,Buf,sizeof(InBuf));
  for(int I=NROUNDS-1;I>=0;I--)
  {
    T=((C+rotls(D,11,32))^Key20[I&3]);
    TA=A^substLong(T);
    T=((D^rotls(C,17,32))+Key20[I&3]);
    TB=B^substLong(T);
    A=C;
    B=D;
    C=TA;
    D=TB;
  }
  RawPut4(C^Key20[0],Buf+0);
  RawPut4(D^Key20[1],Buf+4);
  RawPut4(A^Key20[2],Buf+8);
  RawPut4(B^Key20[3],Buf+12);
  UpdKeys20(InBuf);
}

void CryptData::UpdKeys20(byte *Buf)
{
  for (int I=0;I<16;I+=4)
  {
    Key20[0]^=CRCTab[Buf[I]];
    Key20[1]^=CRCTab[Buf[I+1]];
    Key20[2]^=CRCTab[Buf[I+2]];
    Key20[3]^=CRCTab[Buf[I+3]];
  }
}

void CryptData::Swap20(byte *Ch1,byte *Ch2)
{
  byte Ch=*Ch1;
  *Ch1=*Ch2;
  *Ch2=Ch;
}

#endif
void CryptData::SetKey30(bool Encrypt,SecPassword *Password,const wchar *PwdW,const byte *Salt)
{
  byte AESKey[16],AESInit[16];

  bool Cached=false;
  for (uint I=0;I<ASIZE(KDF3Cache);I++)
    if (KDF3Cache[I].Pwd==*Password &&
        (Salt==NULL && !KDF3Cache[I].SaltPresent || Salt!=NULL &&
        KDF3Cache[I].SaltPresent && memcmp(KDF3Cache[I].Salt,Salt,SIZE_SALT30)==0))
    {
      memcpy(AESKey,KDF3Cache[I].Key,sizeof(AESKey));
      SecHideData(AESKey,sizeof(AESKey),false,false);
      memcpy(AESInit,KDF3Cache[I].Init,sizeof(AESInit));
      Cached=true;
      break;
    }

  if (!Cached)
  {
    byte RawPsw[2*MAXPASSWORD+SIZE_SALT30];
    size_t PswLength=wcslen(PwdW);
    size_t RawLength=2*PswLength;
    WideToRaw(PwdW,PswLength,RawPsw,RawLength);
    if (Salt!=NULL)
    {
      memcpy(RawPsw+RawLength,Salt,SIZE_SALT30);
      RawLength+=SIZE_SALT30;
    }
    sha1_context c;
    sha1_init(&c);

    const uint HashRounds=0x40000;
    for (uint I=0;I<HashRounds;I++)
    {
      sha1_process_rar29( &c, RawPsw, RawLength );
      byte PswNum[3];
      PswNum[0]=(byte)I;
      PswNum[1]=(byte)(I>>8);
      PswNum[2]=(byte)(I>>16);
      sha1_process(&c, PswNum, 3);
      if (I%(HashRounds/16)==0)
      {
        sha1_context tempc=c;
        uint32 digest[5];
        sha1_done( &tempc, digest );
        AESInit[I/(HashRounds/16)]=(byte)digest[4];
      }
    }
    uint32 digest[5];
    sha1_done( &c, digest );
    for (uint I=0;I<4;I++)
      for (uint J=0;J<4;J++)
        AESKey[I*4+J]=(byte)(digest[I]>>(J*8));

    KDF3Cache[KDF3CachePos].Pwd=*Password;
    if ((KDF3Cache[KDF3CachePos].SaltPresent=(Salt!=NULL))==true)
      memcpy(KDF3Cache[KDF3CachePos].Salt,Salt,SIZE_SALT30);
    memcpy(KDF3Cache[KDF3CachePos].Key,AESKey,sizeof(AESKey));
    SecHideData(KDF3Cache[KDF3CachePos].Key,sizeof(KDF3Cache[KDF3CachePos].Key),true,false);
    memcpy(KDF3Cache[KDF3CachePos].Init,AESInit,sizeof(AESInit));
    KDF3CachePos=(KDF3CachePos+1)%ASIZE(KDF3Cache);

    cleandata(RawPsw,sizeof(RawPsw));
  }
  rin.Init(Encrypt, AESKey, 128, AESInit);
  cleandata(AESKey,sizeof(AESKey));
  cleandata(AESInit,sizeof(AESInit));
}

static void hmac_sha256(const byte *Key,size_t KeyLength,const byte *Data,
                        size_t DataLength,byte *ResDigest,
                        sha256_context *ICtxOpt,bool *SetIOpt,
                        sha256_context *RCtxOpt,bool *SetROpt)
{
  const size_t Sha256BlockSize=64;

  byte KeyHash[SHA256_DIGEST_SIZE];
  if (KeyLength > Sha256BlockSize)
  {
    sha256_context KCtx;
    sha256_init(&KCtx);
    sha256_process(&KCtx, Key, KeyLength);
    sha256_done(&KCtx, KeyHash);

    Key = KeyHash;
    KeyLength = SHA256_DIGEST_SIZE;
  }

  byte KeyBuf[Sha256BlockSize];
  sha256_context ICtx;

  if (ICtxOpt!=NULL && *SetIOpt)
    ICtx=*ICtxOpt;
  else
  {

    for (size_t I = 0; I < KeyLength; I++)
      KeyBuf[I] = Key[I] ^ 0x36;
    for (size_t I = KeyLength; I < Sha256BlockSize; I++)
      KeyBuf[I] = 0x36;

    sha256_init(&ICtx);
    sha256_process(&ICtx, KeyBuf, Sha256BlockSize);
  }

  if (ICtxOpt!=NULL && !*SetIOpt)
  {
    *ICtxOpt=ICtx;
    *SetIOpt=true;
  }

  sha256_process(&ICtx, Data, DataLength);

  byte IDig[SHA256_DIGEST_SIZE];
  sha256_done(&ICtx, IDig);

  sha256_context RCtx;

  if (RCtxOpt!=NULL && *SetROpt)
    RCtx=*RCtxOpt;
  else
  {

    for (size_t I = 0; I < KeyLength; I++)
      KeyBuf[I] = Key[I] ^ 0x5c;
    for (size_t I = KeyLength; I < Sha256BlockSize; I++)
      KeyBuf[I] = 0x5c;

    sha256_init(&RCtx);
    sha256_process(&RCtx, KeyBuf, Sha256BlockSize);
  }

  if (RCtxOpt!=NULL && !*SetROpt)
  {
    *RCtxOpt=RCtx;
    *SetROpt=true;
  }

  sha256_process(&RCtx, IDig, SHA256_DIGEST_SIZE);

  sha256_done(&RCtx, ResDigest);
}

void pbkdf2(const byte *Pwd, size_t PwdLength,
            const byte *Salt, size_t SaltLength,
            byte *Key, byte *V1, byte *V2, uint Count)
{
  const size_t MaxSalt=64;
  byte SaltData[MaxSalt+4];
  memcpy(SaltData, Salt, Min(SaltLength,MaxSalt));

  SaltData[SaltLength + 0] = 0;
  SaltData[SaltLength + 1] = 0;
  SaltData[SaltLength + 2] = 0;
  SaltData[SaltLength + 3] = 1;

  byte U1[SHA256_DIGEST_SIZE];
  hmac_sha256(Pwd, PwdLength, SaltData, SaltLength + 4, U1, NULL, NULL, NULL, NULL);
  byte Fn[SHA256_DIGEST_SIZE];
  memcpy(Fn, U1, sizeof(Fn));

  uint  CurCount[] = { Count-1, 16, 16 };
  byte *CurValue[] = { Key    , V1, V2 };

  sha256_context ICtxOpt,RCtxOpt;
  bool SetIOpt=false,SetROpt=false;

  byte U2[SHA256_DIGEST_SIZE];
  for (uint I = 0; I < 3; I++)
  {
    for (uint J = 0; J < CurCount[I]; J++)
    {

      hmac_sha256(Pwd, PwdLength, U1, sizeof(U1), U2, &ICtxOpt, &SetIOpt, &RCtxOpt, &SetROpt);
      memcpy(U1, U2, sizeof(U1));
      for (uint K = 0; K < sizeof(Fn); K++)
        Fn[K] ^= U1[K];
    }
    memcpy(CurValue[I], Fn, SHA256_DIGEST_SIZE);
  }

  cleandata(SaltData, sizeof(SaltData));
  cleandata(Fn, sizeof(Fn));
  cleandata(U1, sizeof(U1));
  cleandata(U2, sizeof(U2));
}

bool CryptData::SetKey50(bool Encrypt,SecPassword *Password,const wchar *PwdW,
     const byte *Salt,const byte *InitV,uint Lg2Cnt,byte *HashKey,
     byte *PswCheck)
{
  if (Lg2Cnt>CRYPT5_KDF_LG2_COUNT_MAX)
    return false;

  byte Key[32],PswCheckValue[SHA256_DIGEST_SIZE],HashKeyValue[SHA256_DIGEST_SIZE];
  bool Found=false;
  for (uint I=0;I<ASIZE(KDF5Cache);I++)
  {
    KDF5CacheItem *Item=KDF5Cache+I;
    if (Item->Pwd==*Password && Item->Lg2Count==Lg2Cnt &&
        memcmp(Item->Salt,Salt,SIZE_SALT50)==0)
    {
      memcpy(Key,Item->Key,sizeof(Key));
      SecHideData(Key,sizeof(Key),false,false);

      memcpy(PswCheckValue,Item->PswCheckValue,sizeof(PswCheckValue));
      memcpy(HashKeyValue,Item->HashKeyValue,sizeof(HashKeyValue));
      Found=true;
      break;
    }
  }

  if (!Found)
  {
    char PwdUtf[MAXPASSWORD*4];
    WideToUtf(PwdW,PwdUtf,ASIZE(PwdUtf));

    pbkdf2((byte *)PwdUtf,strlen(PwdUtf),Salt,SIZE_SALT50,Key,HashKeyValue,PswCheckValue,(1<<Lg2Cnt));
    cleandata(PwdUtf,sizeof(PwdUtf));

    KDF5CacheItem *Item=KDF5Cache+(KDF5CachePos++ % ASIZE(KDF5Cache));
    Item->Lg2Count=Lg2Cnt;
    Item->Pwd=*Password;
    memcpy(Item->Salt,Salt,SIZE_SALT50);
    memcpy(Item->Key,Key,sizeof(Item->Key));
    memcpy(Item->PswCheckValue,PswCheckValue,sizeof(PswCheckValue));
    memcpy(Item->HashKeyValue,HashKeyValue,sizeof(HashKeyValue));
    SecHideData(Item->Key,sizeof(Item->Key),true,false);
  }
  if (HashKey!=NULL)
    memcpy(HashKey,HashKeyValue,SHA256_DIGEST_SIZE);
  if (PswCheck!=NULL)
  {
    memset(PswCheck,0,SIZE_PSWCHECK);
    for (uint I=0;I<SHA256_DIGEST_SIZE;I++)
      PswCheck[I%SIZE_PSWCHECK]^=PswCheckValue[I];
    cleandata(PswCheckValue,sizeof(PswCheckValue));
  }

  if (InitV!=NULL)
    rin.Init(Encrypt, Key, 256, InitV);

  cleandata(Key,sizeof(Key));
  return true;
}

void ConvertHashToMAC(HashValue *Value,byte *Key)
{
  if (Value->Type==HASH_CRC32)
  {
    byte RawCRC[4];
    RawPut4(Value->CRC32,RawCRC);
    byte Digest[SHA256_DIGEST_SIZE];
    hmac_sha256(Key,SHA256_DIGEST_SIZE,RawCRC,sizeof(RawCRC),Digest,NULL,NULL,NULL,NULL);
    Value->CRC32=0;
    for (uint I=0;I<ASIZE(Digest);I++)
      Value->CRC32^=Digest[I] << ((I & 3) * 8);
    Value->CRC32&=0xffffffff;
  }
  if (Value->Type==HASH_BLAKE2)
  {
    byte Digest[BLAKE2_DIGEST_SIZE];
    hmac_sha256(Key,BLAKE2_DIGEST_SIZE,Value->Digest,sizeof(Value->Digest),Digest,NULL,NULL,NULL,NULL);
    memcpy(Value->Digest,Digest,sizeof(Value->Digest));
  }
}

#if 0
static void TestPBKDF2();
struct TestKDF {TestKDF() {TestPBKDF2();exit(0);}} GlobalTestKDF;

void TestPBKDF2()
{
  byte Key[32],V1[32],V2[32];

  pbkdf2((byte *)"password", 8, (byte *)"salt", 4, Key, V1, V2, 1);
  byte Res1[32]={0x12, 0x0f, 0xb6, 0xcf, 0xfc, 0xf8, 0xb3, 0x2c, 0x43, 0xe7, 0x22, 0x52, 0x56, 0xc4, 0xf8, 0x37, 0xa8, 0x65, 0x48, 0xc9, 0x2c, 0xcc, 0x35, 0x48, 0x08, 0x05, 0x98, 0x7c, 0xb7, 0x0b, 0xe1, 0x7b };
  mprintf(L"\nPBKDF2 test1: %s", memcmp(Key,Res1,32)==0 ? L"OK":L"Failed");

  pbkdf2((byte *)"password", 8, (byte *)"salt", 4, Key, V1, V2, 4096);
  byte Res2[32]={0xc5, 0xe4, 0x78, 0xd5, 0x92, 0x88, 0xc8, 0x41, 0xaa, 0x53, 0x0d, 0xb6, 0x84, 0x5c, 0x4c, 0x8d, 0x96, 0x28, 0x93, 0xa0, 0x01, 0xce, 0x4e, 0x11, 0xa4, 0x96, 0x38, 0x73, 0xaa, 0x98, 0x13, 0x4a };
  mprintf(L"\nPBKDF2 test2: %s", memcmp(Key,Res2,32)==0 ? L"OK":L"Failed");

  pbkdf2((byte *)"just some long string pretending to be a password", 49, (byte *)"salt, salt, salt, a lot of salt", 31, Key, V1, V2, 65536);
  byte Res3[32]={0x08, 0x0f, 0xa3, 0x1d, 0x42, 0x2d, 0xb0, 0x47, 0x83, 0x9b, 0xce, 0x3a, 0x3b, 0xce, 0x49, 0x51, 0xe2, 0x62, 0xb9, 0xff, 0x76, 0x2f, 0x57, 0xe9, 0xc4, 0x71, 0x96, 0xce, 0x4b, 0x6b, 0x6e, 0xbf};
  mprintf(L"\nPBKDF2 test3: %s", memcmp(Key,Res3,32)==0 ? L"OK":L"Failed");
}
#endif

CryptData::CryptData()
{
  Method=CRYPT_NONE;
  KDF3CachePos=0;
  KDF5CachePos=0;
  memset(CRCTab,0,sizeof(CRCTab));
}

void CryptData::DecryptBlock(byte *Buf,size_t Size)
{
  switch(Method)
  {
#ifndef SFX_MODULE
    case CRYPT_RAR13:
      Decrypt13(Buf,Size);
      break;
    case CRYPT_RAR15:
      Crypt15(Buf,Size);
      break;
    case CRYPT_RAR20:
      for (size_t I=0;I<Size;I+=CRYPT_BLOCK_SIZE)
        DecryptBlock20(Buf+I);
      break;
#endif
    case CRYPT_RAR30:
    case CRYPT_RAR50:
      rin.blockDecrypt(Buf,Size,Buf);
      break;
  }
}

bool CryptData::SetCryptKeys(bool Encrypt,CRYPT_METHOD Method,
     SecPassword *Password,const byte *Salt,
     const byte *InitV,uint Lg2Cnt,byte *HashKey,byte *PswCheck)
{
  if (Method==CRYPT_NONE || !Password->IsSet())
    return false;

  CryptData::Method=Method;

  wchar PwdW[MAXPASSWORD];
  Password->Get(PwdW,ASIZE(PwdW));
  PwdW[Min(MAXPASSWORD_RAR,MAXPASSWORD)-1]=0;

  char PwdA[MAXPASSWORD];
  WideToChar(PwdW,PwdA,ASIZE(PwdA));
  PwdA[Min(MAXPASSWORD_RAR,MAXPASSWORD)-1]=0;

  bool Success=true;

  switch(Method)
  {
#ifndef SFX_MODULE
    case CRYPT_RAR13:
      SetKey13(PwdA);
      break;
    case CRYPT_RAR15:
      SetKey15(PwdA);
      break;
    case CRYPT_RAR20:
      SetKey20(PwdA);
      break;
#endif
    case CRYPT_RAR30:
      SetKey30(Encrypt,Password,PwdW,Salt);
      break;
    case CRYPT_RAR50:
      Success=SetKey50(Encrypt,Password,PwdW,Salt,InitV,Lg2Cnt,HashKey,PswCheck);
      break;
  }
  cleandata(PwdA,sizeof(PwdA));
  cleandata(PwdW,sizeof(PwdW));
  return Success;
}

static void TimeRandomize(byte *RndBuf,size_t BufSize)
{
  static uint Count=0;
  RarTime CurTime;
  CurTime.SetCurrentTime();
  uint64 Random=CurTime.GetWin()+clock();
  for (size_t I=0;I<BufSize;I++)
  {
    byte RndByte = byte (Random >> ( (I & 7) * 8 ));
    RndBuf[I]=byte( (RndByte ^ I) + Count++);
  }
}

void GetRnd(byte *RndBuf,size_t BufSize)
{
  bool Success=false;
#if defined(_WIN_ALL)
  HCRYPTPROV hProvider = 0;
  if (CryptAcquireContext(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
  {
    Success=CryptGenRandom(hProvider, (DWORD)BufSize, RndBuf) != FALSE;
    CryptReleaseContext(hProvider, 0);
  }
#elif defined(_UNIX)
  FILE *rndf = fopen("/dev/urandom", "r");
  if (rndf!=NULL)
  {
    Success=fread(RndBuf, BufSize, 1, rndf) == BufSize;
    fclose(rndf);
  }
#endif

  if (!Success)
    TimeRandomize(RndBuf,BufSize);
}

static int RarErrorToDll(RAR_EXIT ErrCode);

struct DataSet
{
  CommandData Cmd;
  Archive Arc;
  CmdExtract Extract;
  int OpenMode;
  int HeaderSize;

  DataSet():Arc(&Cmd),Extract(&Cmd) {};
};

HANDLE PASCAL RAROpenArchive(struct RAROpenArchiveData *r)
{
  RAROpenArchiveDataEx rx{};
  rx.ArcName=r->ArcName;
  rx.OpenMode=r->OpenMode;
  rx.CmtBuf=r->CmtBuf;
  rx.CmtBufSize=r->CmtBufSize;
  HANDLE hArc=RAROpenArchiveEx(&rx);
  r->OpenResult=rx.OpenResult;
  r->CmtSize=rx.CmtSize;
  r->CmtState=rx.CmtState;
  return hArc;
}

HANDLE PASCAL RAROpenArchiveEx(struct RAROpenArchiveDataEx *r)
{
  DataSet *Data=nullptr;
  try
  {
    ErrHandler.Clean();

    r->OpenResult=0;
    Data=new DataSet;
    Data->Cmd.DllError=0;
    Data->OpenMode=r->OpenMode;
    Data->Cmd.FileArgs.AddString(L"*");
    Data->Cmd.KeepBroken=(r->OpFlags&ROADOF_KEEPBROKEN)!=0;

    std::string AnsiArcName;
    if (r->ArcName!=nullptr)
    {
      AnsiArcName=r->ArcName;
#ifdef _WIN_ALL
      if (!AreFileApisANSI())
        OemToExt(r->ArcName,AnsiArcName);
#endif
    }

    std::wstring ArcName;
    if (r->ArcNameW!=nullptr && *r->ArcNameW!=0)
      ArcName=r->ArcNameW;
    else
      CharToWide(AnsiArcName,ArcName);

    Data->Cmd.AddArcName(ArcName);
    Data->Cmd.Overwrite=OVERWRITE_ALL;
    Data->Cmd.VersionControl=1;

    Data->Cmd.Callback=r->Callback;
    Data->Cmd.UserData=r->UserData;

    Data->Cmd.OpenShared = true;
    if (!Data->Arc.Open(ArcName,FMF_OPENSHARED))
    {
      r->OpenResult=ERAR_EOPEN;
      delete Data;
      return nullptr;
    }
    if (!Data->Arc.IsArchive(true))
    {
      if (Data->Cmd.DllError!=0)
        r->OpenResult=Data->Cmd.DllError;
      else
      {
        RAR_EXIT ErrCode=ErrHandler.GetErrorCode();
        if (ErrCode!=RARX_SUCCESS && ErrCode!=RARX_WARNING)
          r->OpenResult=RarErrorToDll(ErrCode);
        else
          r->OpenResult=ERAR_BAD_ARCHIVE;
      }
      delete Data;
      return nullptr;
    }
    r->Flags=0;

    if (Data->Arc.Volume)
      r->Flags|=ROADF_VOLUME;
    if (Data->Arc.MainComment)
      r->Flags|=ROADF_COMMENT;
    if (Data->Arc.Locked)
      r->Flags|=ROADF_LOCK;
    if (Data->Arc.Solid)
      r->Flags|=ROADF_SOLID;
    if (Data->Arc.NewNumbering)
      r->Flags|=ROADF_NEWNUMBERING;
    if (Data->Arc.Signed)
      r->Flags|=ROADF_SIGNED;
    if (Data->Arc.Protected)
      r->Flags|=ROADF_RECOVERY;
    if (Data->Arc.Encrypted)
      r->Flags|=ROADF_ENCHEADERS;
    if (Data->Arc.FirstVolume)
      r->Flags|=ROADF_FIRSTVOLUME;

    std::wstring CmtDataW;
    if (r->CmtBufSize!=0 && Data->Arc.GetComment(CmtDataW))
    {
      if (r->CmtBufW!=nullptr)
      {

        size_t Size=wcslen(CmtDataW.data())+1;

        r->CmtState=Size>r->CmtBufSize ? ERAR_SMALL_BUF:1;
        r->CmtSize=(uint)Min(Size,r->CmtBufSize);
        memcpy(r->CmtBufW,CmtDataW.data(),(r->CmtSize-1)*sizeof(*r->CmtBufW));
        r->CmtBufW[r->CmtSize-1]=0;
      }
      else
        if (r->CmtBuf!=NULL)
        {
          std::vector<char> CmtData(CmtDataW.size()*4+1);
          WideToChar(&CmtDataW[0],&CmtData[0],CmtData.size()-1);
          size_t Size=strlen(CmtData.data())+1;

          r->CmtState=Size>r->CmtBufSize ? ERAR_SMALL_BUF:1;
          r->CmtSize=(uint)Min(Size,r->CmtBufSize);
          memcpy(r->CmtBuf,CmtData.data(),r->CmtSize-1);
          r->CmtBuf[r->CmtSize-1]=0;
        }
    }
    else
      r->CmtState=r->CmtSize=0;

#ifdef PROPAGATE_MOTW
    if (r->MarkOfTheWeb!=nullptr)
    {
      Data->Cmd.MotwAllFields=r->MarkOfTheWeb[0]=='1';
      const wchar *Sep=wcschr(r->MarkOfTheWeb,'=');
      if (r->MarkOfTheWeb[0]=='-')
        Data->Cmd.MotwList.Reset();
      else
        Data->Cmd.GetBriefMaskList(Sep==nullptr ? L"*":Sep+1,Data->Cmd.MotwList);
    }
#endif

    Data->Extract.ExtractArchiveInit(Data->Arc);
    return (HANDLE)Data;
  }
  catch (RAR_EXIT ErrCode)
  {
    if (Data!=NULL && Data->Cmd.DllError!=0)
      r->OpenResult=Data->Cmd.DllError;
    else
      r->OpenResult=RarErrorToDll(ErrCode);
    if (Data != NULL)
      delete Data;
    return NULL;
  }
  catch (std::bad_alloc&)
  {
    r->OpenResult=ERAR_NO_MEMORY;
    if (Data != NULL)
      delete Data;
  }
  return NULL;
}

int PASCAL RARCloseArchive(HANDLE hArcData)
{
  DataSet *Data=(DataSet *)hArcData;
  try
  {
    bool Success=Data==NULL ? false:Data->Arc.Close();
    delete Data;
    return Success ? ERAR_SUCCESS : ERAR_ECLOSE;
  }
  catch (RAR_EXIT ErrCode)
  {
    return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrCode);
  }
}

int PASCAL RARReadHeader(HANDLE hArcData,struct RARHeaderData *D)
{
  struct RARHeaderDataEx X{};

  int Code=RARReadHeaderEx(hArcData,&X);

  strncpyz(D->ArcName,X.ArcName,ASIZE(D->ArcName));
  strncpyz(D->FileName,X.FileName,ASIZE(D->FileName));
  D->Flags=X.Flags;
  D->PackSize=X.PackSize;
  D->UnpSize=X.UnpSize;
  D->HostOS=X.HostOS;
  D->FileCRC=X.FileCRC;
  D->FileTime=X.FileTime;
  D->UnpVer=X.UnpVer;
  D->Method=X.Method;
  D->FileAttr=X.FileAttr;
  D->CmtSize=0;
  D->CmtState=0;

  return Code;
}

int PASCAL RARReadHeaderEx(HANDLE hArcData,struct RARHeaderDataEx *D)
{
  DataSet *Data=(DataSet *)hArcData;
  try
  {
    if ((Data->HeaderSize=(int)Data->Arc.SearchBlock(HEAD_FILE))<=0)
    {
      if (Data->Arc.Volume && Data->Arc.GetHeaderType()==HEAD_ENDARC &&
          Data->Arc.EndArcHead.NextVolume)
        if (MergeArchive(Data->Arc,NULL,false,'L'))
        {
          Data->Arc.Seek(Data->Arc.CurBlockPos,SEEK_SET);
          return RARReadHeaderEx(hArcData,D);
        }
        else
          return ERAR_EOPEN;

      if (Data->Arc.BrokenHeader)
        return ERAR_BAD_DATA;

      if (Data->Arc.FailedHeaderDecryption)
        return ERAR_BAD_PASSWORD;

      return ERAR_END_ARCHIVE;
    }
    FileHeader *hd=&Data->Arc.FileHead;
    if (Data->OpenMode==RAR_OM_LIST && hd->SplitBefore)
    {
      int Code=RARProcessFile(hArcData,RAR_SKIP,NULL,NULL);
      if (Code==0)
        return RARReadHeaderEx(hArcData,D);
      else
        return Code;
    }
    wcsncpyz(D->ArcNameW,Data->Arc.FileName.c_str(),ASIZE(D->ArcNameW));
    WideToChar(D->ArcNameW,D->ArcName,ASIZE(D->ArcName));
    if (D->ArcNameEx!=nullptr)
      wcsncpyz(D->ArcNameEx,Data->Arc.FileName.c_str(),D->ArcNameExSize);

    wcsncpyz(D->FileNameW,hd->FileName.c_str(),ASIZE(D->FileNameW));
    WideToChar(D->FileNameW,D->FileName,ASIZE(D->FileName));
#ifdef _WIN_ALL
    CharToOemA(D->FileName,D->FileName);
#endif
    if (D->FileNameEx!=nullptr)
      wcsncpyz(D->FileNameEx,hd->FileName.c_str(),D->FileNameExSize);

    D->Flags=0;
    if (hd->SplitBefore)
      D->Flags|=RHDF_SPLITBEFORE;
    if (hd->SplitAfter)
      D->Flags|=RHDF_SPLITAFTER;
    if (hd->Encrypted)
      D->Flags|=RHDF_ENCRYPTED;
    if (hd->Solid)
      D->Flags|=RHDF_SOLID;
    if (hd->Dir)
      D->Flags|=RHDF_DIRECTORY;

    D->PackSize=uint(hd->PackSize & 0xffffffff);
    D->PackSizeHigh=uint(hd->PackSize>>32);
    D->UnpSize=uint(hd->UnpSize & 0xffffffff);
    D->UnpSizeHigh=uint(hd->UnpSize>>32);
    D->HostOS=hd->HSType==HSYS_WINDOWS ? HOST_WIN32:HOST_UNIX;
    D->UnpVer=Data->Arc.FileHead.UnpVer;
    D->FileCRC=hd->FileHash.CRC32;
    D->FileTime=hd->mtime.GetDos();

    uint64 MRaw=hd->mtime.GetWin();
    D->MtimeLow=(uint)MRaw;
    D->MtimeHigh=(uint)(MRaw>>32);
    uint64 CRaw=hd->ctime.GetWin();
    D->CtimeLow=(uint)CRaw;
    D->CtimeHigh=(uint)(CRaw>>32);
    uint64 ARaw=hd->atime.GetWin();
    D->AtimeLow=(uint)ARaw;
    D->AtimeHigh=(uint)(ARaw>>32);

    D->Method=hd->Method+0x30;
    D->FileAttr=hd->FileAttr;
    D->CmtSize=0;
    D->CmtState=0;

    D->DictSize=uint(hd->WinSize/1024);

    switch (hd->FileHash.Type)
    {
      case HASH_RAR14:
      case HASH_CRC32:
        D->HashType=RAR_HASH_CRC32;
        break;
      case HASH_BLAKE2:
        D->HashType=RAR_HASH_BLAKE2;
        memcpy(D->Hash,hd->FileHash.Digest,BLAKE2_DIGEST_SIZE);
        break;
      default:
        D->HashType=RAR_HASH_NONE;
        break;
    }

    D->RedirType=hd->RedirType;

    if (hd->RedirType!=FSREDIR_NONE && D->RedirName!=NULL &&
        D->RedirNameSize>0 && D->RedirNameSize<100000)
      wcsncpyz(D->RedirName,hd->RedirName.c_str(),D->RedirNameSize);
    D->DirTarget=hd->DirTarget;
  }
  catch (RAR_EXIT ErrCode)
  {
    return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrCode);
  }
  return ERAR_SUCCESS;
}

int PASCAL ProcessFile(HANDLE hArcData,int Operation,char *DestPath,char *DestName,wchar *DestPathW,wchar *DestNameW)
{
  DataSet *Data=(DataSet *)hArcData;
  try
  {
    Data->Cmd.DllError=0;
    if (Data->OpenMode==RAR_OM_LIST || Data->OpenMode==RAR_OM_LIST_INCSPLIT ||
        Operation==RAR_SKIP && !Data->Arc.Solid)
    {
      if (Data->Arc.Volume && Data->Arc.GetHeaderType()==HEAD_FILE &&
          Data->Arc.FileHead.SplitAfter)
        if (MergeArchive(Data->Arc,NULL,false,'L'))
        {
          Data->Arc.Seek(Data->Arc.CurBlockPos,SEEK_SET);
          return ERAR_SUCCESS;
        }
        else
          return ERAR_EOPEN;
      Data->Arc.SeekToNext();
    }
    else
    {
      Data->Cmd.DllOpMode=Operation;

      Data->Cmd.ExtrPath.clear();
      Data->Cmd.DllDestName.clear();

      if (DestPath!=NULL)
      {
        std::string ExtrPathA=DestPath;
#ifdef _WIN_ALL

        OemToExt(ExtrPathA,ExtrPathA);
#endif
        CharToWide(ExtrPathA,Data->Cmd.ExtrPath);
        AddEndSlash(Data->Cmd.ExtrPath);
      }
      if (DestName!=NULL)
      {
        std::string DestNameA=DestName;
#ifdef _WIN_ALL

        OemToExt(DestNameA,DestNameA);
#endif
        CharToWide(DestNameA,Data->Cmd.DllDestName);
      }

      if (DestPathW!=NULL)
      {
        Data->Cmd.ExtrPath=DestPathW;
        AddEndSlash(Data->Cmd.ExtrPath);
      }

      if (DestNameW!=NULL)
        Data->Cmd.DllDestName=DestNameW;

      Data->Cmd.Command=Operation==RAR_EXTRACT ? L"X":L"T";
      Data->Cmd.Test=Operation!=RAR_EXTRACT;
      bool Repeat=false;
      Data->Extract.ExtractCurrentFile(Data->Arc,Data->HeaderSize,Repeat);

      while (Data->Arc.IsOpened() && Data->Arc.ReadHeader()!=0 &&
             Data->Arc.GetHeaderType()==HEAD_SERVICE)
      {
        Data->Extract.ExtractCurrentFile(Data->Arc,Data->HeaderSize,Repeat);
        Data->Arc.SeekToNext();
      }
      Data->Arc.Seek(Data->Arc.CurBlockPos,SEEK_SET);
    }
  }
  catch (std::bad_alloc&)
  {
    return ERAR_NO_MEMORY;
  }
  catch (RAR_EXIT ErrCode)
  {
    return Data->Cmd.DllError!=0 ? Data->Cmd.DllError : RarErrorToDll(ErrCode);
  }
  return Data->Cmd.DllError;
}

int PASCAL RARProcessFile(HANDLE hArcData,int Operation,char *DestPath,char *DestName)
{
  return ProcessFile(hArcData,Operation,DestPath,DestName,NULL,NULL);
}

int PASCAL RARProcessFileW(HANDLE hArcData,int Operation,wchar *DestPath,wchar *DestName)
{
  return ProcessFile(hArcData,Operation,NULL,NULL,DestPath,DestName);
}

void PASCAL RARSetChangeVolProc(HANDLE hArcData,CHANGEVOLPROC ChangeVolProc)
{
  DataSet *Data=(DataSet *)hArcData;
  Data->Cmd.ChangeVolProc=ChangeVolProc;
}

void PASCAL RARSetCallback(HANDLE hArcData,UNRARCALLBACK Callback,LPARAM UserData)
{
  DataSet *Data=(DataSet *)hArcData;
  Data->Cmd.Callback=Callback;
  Data->Cmd.UserData=UserData;
}

void PASCAL RARSetProcessDataProc(HANDLE hArcData,PROCESSDATAPROC ProcessDataProc)
{
  DataSet *Data=(DataSet *)hArcData;
  Data->Cmd.ProcessDataProc=ProcessDataProc;
}

void PASCAL RARSetPassword(HANDLE hArcData,char *Password)
{
#ifndef RAR_NOCRYPT
  DataSet *Data=(DataSet *)hArcData;
  wchar PasswordW[MAXPASSWORD];
  CharToWide(Password,PasswordW,ASIZE(PasswordW));
  Data->Cmd.Password.Set(PasswordW);
  cleandata(PasswordW,sizeof(PasswordW));
#endif
}

int PASCAL RARGetDllVersion()
{
  return RAR_DLL_VERSION;
}

static int RarErrorToDll(RAR_EXIT ErrCode)
{
  switch(ErrCode)
  {
    case RARX_FATAL:
    case RARX_READ:
      return ERAR_EREAD;
    case RARX_CRC:
      return ERAR_BAD_DATA;
    case RARX_WRITE:
      return ERAR_EWRITE;
    case RARX_OPEN:
      return ERAR_EOPEN;
    case RARX_CREATE:
      return ERAR_ECREATE;
    case RARX_MEMORY:
      return ERAR_NO_MEMORY;
    case RARX_BADPWD:
      return ERAR_BAD_PASSWORD;
    case RARX_SUCCESS:
      return ERAR_SUCCESS;
    case RARX_BADARC:
      return ERAR_BAD_ARCHIVE;
    default:
      return ERAR_UNKNOWN;
  }
}

EncodeFileName::EncodeFileName()
{
  Flags=0;
  FlagBits=0;
  FlagsPos=0;
  DestSize=0;
}

void EncodeFileName::Decode(const char *Name,size_t NameSize,
                            const byte *EncName,size_t EncSize,
                            std::wstring &NameW)
{
  size_t EncPos=0,DecPos=0;
  byte HighByte=EncPos<EncSize ? EncName[EncPos++] : 0;
  while (EncPos<EncSize)
  {
    if (FlagBits==0)
    {
      Flags=EncName[EncPos++];
      FlagBits=8;
    }
    switch(Flags>>6)
    {
      case 0:
        if (EncPos>=EncSize)
          break;

        NameW.resize(DecPos+1);
        NameW[DecPos++]=EncName[EncPos++];
        break;
      case 1:
        if (EncPos>=EncSize)
          break;
        NameW.resize(DecPos+1);
        NameW[DecPos++]=EncName[EncPos++]+(HighByte<<8);
        break;
      case 2:
        if (EncPos+1>=EncSize)
          break;
        NameW.resize(DecPos+1);
        NameW[DecPos++]=EncName[EncPos]+(EncName[EncPos+1]<<8);
        EncPos+=2;
        break;
      case 3:
        {
          if (EncPos>=EncSize)
            break;
          int Length=EncName[EncPos++];
          if ((Length & 0x80)!=0)
          {
            if (EncPos>=EncSize)
              break;
            byte Correction=EncName[EncPos++];
            for (Length=(Length&0x7f)+2;Length>0 && DecPos<NameSize;Length--,DecPos++)
            {
              NameW.resize(DecPos+1);
              NameW[DecPos]=((Name[DecPos]+Correction)&0xff)+(HighByte<<8);
            }
          }
          else
            for (Length+=2;Length>0 && DecPos<NameSize;Length--,DecPos++)
            {
              NameW.resize(DecPos+1);
              NameW[DecPos]=Name[DecPos];
            }
        }
        break;
    }
    Flags<<=2;
    FlagBits-=2;
  }
}

ErrorHandler::ErrorHandler()
{
  Clean();
}

void ErrorHandler::Clean()
{
  ExitCode=RARX_SUCCESS;
  ErrCount=0;
  EnableBreak=true;
  Silent=false;
  UserBreak=false;
  MainExit=false;
  DisableShutdown=false;
  ReadErrIgnoreAll=false;
}

void ErrorHandler::MemoryError()
{
  MemoryErrorMsg();
  Exit(RARX_MEMORY);
}

void ErrorHandler::OpenError(const std::wstring &FileName)
{
#ifndef SILENT
  OpenErrorMsg(FileName);
  Exit(RARX_OPEN);
#endif
}

void ErrorHandler::CloseError(const std::wstring &FileName)
{
  if (!UserBreak)
  {
    uiMsg(UIERROR_FILECLOSE,FileName);
    SysErrMsg();
  }

  SetErrorCode(RARX_FATAL);
}

void ErrorHandler::ReadError(const std::wstring &FileName)
{
#ifndef SILENT
  ReadErrorMsg(FileName);
#endif
#if !defined(SILENT) || defined(RARDLL)
  Exit(RARX_READ);
#endif
}

void ErrorHandler::AskRepeatRead(const std::wstring &FileName,bool &Ignore,bool &Retry,bool &Quit)
{
  SetErrorCode(RARX_READ);
#if !defined(SILENT) && !defined(SFX_MODULE)
  if (!Silent)
  {
    uiMsg(UIERROR_FILEREAD,L"",FileName);
    SysErrMsg();
    if (ReadErrIgnoreAll)
      Ignore=true;
    else
    {
      bool All=false;
      uiAskRepeatRead(FileName,Ignore,All,Retry,Quit);
      if (All)
        ReadErrIgnoreAll=Ignore=true;
      if (Quit)
        DisableShutdown=true;
    }
    return;
  }
#endif
  Ignore=true;
}

void ErrorHandler::WriteError(const std::wstring &ArcName,const std::wstring &FileName)
{
#ifndef SILENT
  WriteErrorMsg(ArcName,FileName);
#endif
#if !defined(SILENT) || defined(RARDLL)
  Exit(RARX_WRITE);
#endif
}

#ifdef _WIN_ALL
void ErrorHandler::WriteErrorFAT(const std::wstring &FileName)
{
  SysErrMsg();
  uiMsg(UIERROR_NTFSREQUIRED,FileName);
#if !defined(SILENT) && !defined(SFX_MODULE) || defined(RARDLL)
  Exit(RARX_WRITE);
#endif
}
#endif

bool ErrorHandler::AskRepeatWrite(const std::wstring &FileName,bool DiskFull)
{
#ifndef SILENT
  if (!Silent)
  {

    SysErrMsg();
    bool Repeat=uiAskRepeatWrite(FileName,DiskFull);
    if (!Repeat)
      DisableShutdown=true;
    return Repeat;
  }
#endif
  return false;
}

void ErrorHandler::SeekError(const std::wstring &FileName)
{
  if (!UserBreak)
  {
    uiMsg(UIERROR_FILESEEK,FileName);
    SysErrMsg();
  }
#if !defined(SILENT) || defined(RARDLL)
  Exit(RARX_FATAL);
#endif
}

void ErrorHandler::GeneralErrMsg(const wchar *fmt,...)
{
#ifndef RARDLL
  va_list arglist;
  va_start(arglist,fmt);

  std::wstring Msg=vwstrprintf(fmt,arglist);
  uiMsg(UIERROR_GENERALERRMSG,Msg);
  SysErrMsg();

  va_end(arglist);
#endif
}

void ErrorHandler::MemoryErrorMsg()
{
  uiMsg(UIERROR_MEMORY);
  SetErrorCode(RARX_MEMORY);
}

void ErrorHandler::OpenErrorMsg(const std::wstring &FileName)
{
  OpenErrorMsg(L"",FileName);
}

void ErrorHandler::OpenErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILEOPEN,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_OPEN);

  Wait();
}

void ErrorHandler::CreateErrorMsg(const std::wstring &FileName)
{
  CreateErrorMsg(L"",FileName);
}

void ErrorHandler::CreateErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILECREATE,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_CREATE);
}

void ErrorHandler::ReadErrorMsg(const std::wstring &FileName)
{
  ReadErrorMsg(L"",FileName);
}

void ErrorHandler::ReadErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILEREAD,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_READ);
}

void ErrorHandler::WriteErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILEWRITE,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_WRITE);
}

void ErrorHandler::ArcBrokenMsg(const std::wstring &ArcName)
{
  uiMsg(UIERROR_ARCBROKEN,ArcName);
  SetErrorCode(RARX_CRC);
}

void ErrorHandler::ChecksumFailedMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_CHECKSUM,ArcName,FileName);
  SetErrorCode(RARX_CRC);
}

void ErrorHandler::UnknownMethodMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_UNKNOWNMETHOD,ArcName,FileName);
  ErrHandler.SetErrorCode(RARX_FATAL);
}

void ErrorHandler::Exit(RAR_EXIT ExitCode)
{
  uiAlarm(UIALARM_ERROR);
  Throw(ExitCode);
}

void ErrorHandler::SetErrorCode(RAR_EXIT Code)
{
  switch(Code)
  {
    case RARX_WARNING:
    case RARX_USERBREAK:
      if (ExitCode==RARX_SUCCESS)
        ExitCode=Code;
      break;
    case RARX_CRC:
      if (ExitCode!=RARX_BADPWD)
        ExitCode=Code;
      break;
    case RARX_FATAL:
      if (ExitCode==RARX_SUCCESS || ExitCode==RARX_WARNING)
        ExitCode=RARX_FATAL;
      break;
    default:
      ExitCode=Code;
      break;
  }
  ErrCount++;
}

#ifdef _WIN_ALL
BOOL __stdcall ProcessSignal(DWORD SigType)
#else
#if defined(__sun)
extern "C"
#endif
void _stdfunction ProcessSignal(int SigType)
#endif
{
#ifdef _WIN_ALL

  if (SigType==CTRL_LOGOFF_EVENT)
    return TRUE;
#endif

  ErrHandler.UserBreak=true;
  ErrHandler.SetDisableShutdown();
  mprintf(St(MBreak));

#ifdef _WIN_ALL

  for (uint I=0;!ErrHandler.MainExit && I<50;I++)
    Sleep(100);
#if defined(USE_RC) && !defined(SFX_MODULE) && !defined(RARDLL)
  ExtRes.UnloadDLL();
#endif
  exit(RARX_USERBREAK);
#endif

#ifdef _UNIX
  static uint BreakCount=0;

  if (++BreakCount>1)
    exit(RARX_USERBREAK);

#endif

#if defined(_WIN_ALL) && !defined(_MSC_VER)

  return TRUE;
#endif
}

void ErrorHandler::SetSignalHandlers(bool Enable)
{
  EnableBreak=Enable;
#ifdef _WIN_ALL
  SetConsoleCtrlHandler(Enable ? ProcessSignal:NULL,TRUE);
#else
  signal(SIGINT,Enable ? ProcessSignal:SIG_IGN);
  signal(SIGTERM,Enable ? ProcessSignal:SIG_IGN);
#endif
}

void ErrorHandler::Throw(RAR_EXIT Code)
{
  if (Code==RARX_USERBREAK && !EnableBreak)
    return;
#if !defined(SILENT)
  if (Code!=RARX_SUCCESS)
    if (Code==RARX_USERERROR)
      mprintf(L"\n");
    else
      mprintf(L"\n%s\n",St(MProgAborted));
#endif
  SetErrorCode(Code);
  throw Code;
}

bool ErrorHandler::GetSysErrMsg(std::wstring &Msg)
{
#ifndef SILENT
#ifdef _WIN_ALL
  int ErrType=GetLastError();
  if (ErrType!=0)
  {
    wchar *Buf=nullptr;
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|
          FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_ALLOCATE_BUFFER,
          NULL,ErrType,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
          (LPTSTR)&Buf,0,NULL)!=0)
    {
      Msg=Buf;
      LocalFree(Buf);
      return true;
    }
  }
#endif

#ifdef _UNIX
  if (errno!=0)
  {
    char *err=strerror(errno);
    if (err!=NULL)
    {
      CharToWide(err,Msg);
      return true;
    }
  }
#endif
#endif
  return false;
}

void ErrorHandler::SysErrMsg()
{
#ifndef SILENT
  std::wstring Msg;
  if (!GetSysErrMsg(Msg))
    return;
#ifdef _WIN_ALL

  size_t Pos=0;
  while (Pos!=std::wstring::npos)
  {
    while (Msg[Pos]=='\r' || Msg[Pos]=='\n')
      Pos++;
    if (Pos==Msg.size())
      break;
    size_t EndPos=Msg.find_first_of(L"\r\n",Pos);
    std::wstring CurMsg=Msg.substr(Pos,EndPos==std::wstring::npos ? EndPos:EndPos-Pos);
    uiMsg(UIERROR_SYSERRMSG,CurMsg);
    Pos=EndPos;
  }
#endif

#ifdef _UNIX
  uiMsg(UIERROR_SYSERRMSG,Msg);
#endif

#endif
}

int ErrorHandler::GetSystemErrorCode()
{
#ifdef _WIN_ALL
  return GetLastError();
#else
  return errno;
#endif
}

void ErrorHandler::SetSystemErrorCode(int Code)
{
#ifdef _WIN_ALL
  SetLastError(Code);
#else
  errno=Code;
#endif
}

bool ExtractHardlink(CommandData *Cmd,const std::wstring &NameNew,const std::wstring &NameExisting)
{
  if (!FileExist(NameExisting))
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    uiMsg(UIERROR_NOLINKTARGET);
    ErrHandler.SetErrorCode(RARX_CREATE);
    return false;
  }
  CreatePath(NameNew,true,Cmd->DisableNames);

#ifdef _WIN_ALL
  bool Success=CreateHardLink(NameNew.c_str(),NameExisting.c_str(),NULL)!=0;
  if (!Success)
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#elif defined(_UNIX)
  std::string NameExistingA,NameNewA;
  WideToChar(NameExisting,NameExistingA);
  WideToChar(NameNew,NameNewA);
  bool Success=link(NameExistingA.c_str(),NameNewA.c_str())==0;
  if (!Success)
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#else
  return false;
#endif
}

#ifdef _WIN_ALL

static bool IsNtfsProhibitedStream(const std::wstring &StreamName)
{

  uint ColonCount=0;
  for (wchar Ch:StreamName)
    if (Ch==':' && ++ColonCount>1)
      return true;
  return false;

}
#endif

#if !defined(SFX_MODULE) && defined(_WIN_ALL)
void ExtractStreams20(Archive &Arc,const std::wstring &FileName)
{
  if (Arc.BrokenHeader)
  {
    uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  if (Arc.StreamHead.Method<0x31 || Arc.StreamHead.Method>0x35 || Arc.StreamHead.UnpVer>VER_PACK)
  {
    uiMsg(UIERROR_STREAMUNKNOWN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }

  std::wstring StreamName;
  CharToWide(Arc.StreamHead.StreamName,StreamName);

  if (StreamName[0]!=':')
  {
    uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  std::wstring FullName=FileName.size()==1 ? L".\\"+FileName:FileName;
  FullName+=StreamName;

#ifdef PROPAGATE_MOTW

  if (Arc.Motw.IsNameConflicting(StreamName))
    return;

#endif

  if (IsNtfsProhibitedStream(StreamName))
    return;

  FindData FD;
  bool HostFound=FindFile::FastFind(FileName,&FD);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr & ~FILE_ATTRIBUTE_READONLY);

  File CurFile;
  if (CurFile.WCreate(FullName))
  {
    ComprDataIO DataIO;
    Unpack Unpack(&DataIO);
    Unpack.Init(0x10000,false);

    DataIO.SetPackedSizeToRead(Arc.StreamHead.DataSize);
    DataIO.EnableShowProgress(false);
    DataIO.SetFiles(&Arc,&CurFile);
    DataIO.UnpHash.Init(HASH_CRC32,1);
    Unpack.SetDestSize(Arc.StreamHead.UnpSize);
    Unpack.DoUnpack(Arc.StreamHead.UnpVer,false);

    if (Arc.StreamHead.StreamCRC!=DataIO.UnpHash.GetCRC32())
    {
      uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,StreamName);
      ErrHandler.SetErrorCode(RARX_CRC);
    }
    else
      CurFile.Close();
  }

  File HostFile;
  if (HostFound && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&FD.ftCreationTime,&FD.ftLastAccessTime,
                &FD.ftLastWriteTime);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr);
}
#endif

#ifdef _WIN_ALL
void ExtractStreams(Archive &Arc,const std::wstring &FileName,bool TestMode)
{
  std::wstring StreamName=GetStreamNameNTFS(Arc);
  if (StreamName[0]!=':')
  {
    uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  if (TestMode)
  {
    File CurFile;
    Arc.ReadSubData(nullptr,&CurFile,true);
    return;
  }

  std::wstring FullName=FileName.size()==1 ? L".\\"+FileName:FileName;
  FullName+=StreamName;

#ifdef PROPAGATE_MOTW

  std::string ParsedMotw;
  if (Arc.Motw.IsNameConflicting(StreamName))
  {

    std::vector<byte> FileMotw;
    if (!Arc.ReadSubData(&FileMotw,nullptr,false))
      return;
    ParsedMotw.assign(FileMotw.begin(),FileMotw.end());

    if (!Arc.Motw.IsFileStreamMoreSecure(ParsedMotw))
      return;
  }

#endif

  if (IsNtfsProhibitedStream(StreamName))
    return;

  FindData FD;
  bool HostFound=FindFile::FastFind(FileName,&FD);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr & ~FILE_ATTRIBUTE_READONLY);
  File CurFile;

  if (CurFile.WCreate(FullName))
  {
#ifdef PROPAGATE_MOTW
    if (!ParsedMotw.empty())
    {

      CurFile.Write(ParsedMotw.data(),ParsedMotw.size());
      CurFile.Close();
    }
    else
#endif
    if (Arc.ReadSubData(nullptr,&CurFile,false))
      CurFile.Close();
  }

  File HostFile;
  if (HostFound && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&FD.ftCreationTime,&FD.ftLastAccessTime,
                &FD.ftLastWriteTime);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr);
}
#endif

std::wstring GetStreamNameNTFS(Archive &Arc)
{
  std::wstring Dest;
  if (Arc.Format==RARFMT15)
    Dest=RawToWide(Arc.SubHead.SubData);
  else
  {
    std::string Src(Arc.SubHead.SubData.begin(),Arc.SubHead.SubData.end());
    UtfToWide(Src.data(),Dest);
  }
  return Dest;
}

#ifdef _WIN_ALL
static void SetACLPrivileges();

static bool ReadSacl=false;

#ifndef SFX_MODULE
void ExtractACL20(Archive &Arc,const std::wstring &FileName)
{
  SetACLPrivileges();

  if (Arc.BrokenHeader)
  {
    uiMsg(UIERROR_ACLBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  if (Arc.EAHead.Method<0x31 || Arc.EAHead.Method>0x35 || Arc.EAHead.UnpVer>VER_PACK)
  {
    uiMsg(UIERROR_ACLUNKNOWN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }

  ComprDataIO DataIO;
  Unpack Unpack(&DataIO);
  Unpack.Init(0x10000,false);

  std::vector<byte> UnpData(Arc.EAHead.UnpSize);
  DataIO.SetUnpackToMemory(&UnpData[0],Arc.EAHead.UnpSize);
  DataIO.SetPackedSizeToRead(Arc.EAHead.DataSize);
  DataIO.EnableShowProgress(false);
  DataIO.SetFiles(&Arc,NULL);
  DataIO.UnpHash.Init(HASH_CRC32,1);
  Unpack.SetDestSize(Arc.EAHead.UnpSize);
  Unpack.DoUnpack(Arc.EAHead.UnpVer,false);

  if (Arc.EAHead.EACRC!=DataIO.UnpHash.GetCRC32())
  {
    uiMsg(UIERROR_ACLBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  SECURITY_INFORMATION  si=OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|
                           DACL_SECURITY_INFORMATION;
  if (ReadSacl)
    si|=SACL_SECURITY_INFORMATION;
  SECURITY_DESCRIPTOR *sd=(SECURITY_DESCRIPTOR *)&UnpData[0];

  int SetCode=SetFileSecurity(FileName.c_str(),si,sd);

  if (!SetCode)
  {
    uiMsg(UIERROR_ACLSET,Arc.FileName,FileName);
    DWORD LastError=GetLastError();
    ErrHandler.SysErrMsg();
    if (LastError==ERROR_ACCESS_DENIED && !IsUserAdmin())
      uiMsg(UIERROR_NEEDADMIN);
    ErrHandler.SetErrorCode(RARX_WARNING);
  }
}
#endif

void ExtractACL(Archive &Arc,const std::wstring &FileName)
{
  std::vector<byte> SubData;
  if (!Arc.ReadSubData(&SubData,NULL,false))
    return;

  SetACLPrivileges();

  SECURITY_INFORMATION si=OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|
                          DACL_SECURITY_INFORMATION;
  if (ReadSacl)
    si|=SACL_SECURITY_INFORMATION;
  SECURITY_DESCRIPTOR *sd=(SECURITY_DESCRIPTOR *)&SubData[0];

  int SetCode=SetFileSecurity(FileName.c_str(),si,sd);
  if (!SetCode)
  {
    std::wstring LongName;
    if (GetWinLongPath(FileName,LongName))
      SetCode=SetFileSecurity(LongName.c_str(),si,sd);
  }

  if (!SetCode)
  {
    uiMsg(UIERROR_ACLSET,Arc.FileName,FileName);
    DWORD LastError=GetLastError();
    ErrHandler.SysErrMsg();
    if (LastError==ERROR_ACCESS_DENIED && !IsUserAdmin())
      uiMsg(UIERROR_NEEDADMIN);
    ErrHandler.SetErrorCode(RARX_WARNING);
  }
}

void SetACLPrivileges()
{
  static bool InitDone=false;
  if (InitDone)
    return;

  if (SetPrivilege(SE_SECURITY_NAME))
    ReadSacl=true;
  SetPrivilege(SE_RESTORE_NAME);

  InitDone=true;
}

#define SYMLINK_FLAG_RELATIVE 1

typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG  Flags;
      WCHAR  PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR  PathBuffer[1];
    } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

bool CreateReparsePoint(CommandData *Cmd,const wchar *Name,FileHeader *hd)
{
  static bool PrivSet=false;
  if (!PrivSet)
  {
    SetPrivilege(SE_RESTORE_NAME);

    SetPrivilege(SE_CREATE_SYMBOLIC_LINK_NAME);
    PrivSet=true;
  }

  const std::wstring &SubstName=hd->RedirName;
  size_t SubstLength=SubstName.size();

  const DWORD BufSize=sizeof(REPARSE_DATA_BUFFER)+((DWORD)SubstLength+1)*2*sizeof(wchar);

  std::vector<byte> Buf(BufSize);
  REPARSE_DATA_BUFFER *rdb=(REPARSE_DATA_BUFFER *)Buf.data();

  bool WinPrefix=SubstName.rfind(L"\\??\\",0)!=std::wstring::npos;
  std::wstring PrintName=WinPrefix ? SubstName.substr(4):SubstName;

  if (WinPrefix && PrintName.rfind(L"UNC\\",0)!=std::wstring::npos)
    PrintName=L"\\"+PrintName.substr(3);

  size_t PrintLength=PrintName.size();

  bool AbsPath=WinPrefix;

  if (!Cmd->AbsoluteLinks && (AbsPath || IsFullPath(hd->RedirName) ||
      !IsRelativeSymlinkSafe(Cmd,hd->FileName,Name,hd->RedirName)))
  {
    uiMsg(UIERROR_SKIPUNSAFELINK,hd->FileName,hd->RedirName);
    ErrHandler.SetErrorCode(RARX_WARNING);
    return false;
  }

  CreatePath(Name,true,Cmd->DisableNames);

  if (FileExist(Name))
    if (IsDir(GetFileAttr(Name)))
      DelDir(Name);
    else
      DelFile(Name);

  if (hd->Dir || hd->DirTarget)
  {
    if (!CreateDir(Name))
    {
      uiMsg(UIERROR_DIRCREATE,L"",Name);
      ErrHandler.SetErrorCode(RARX_CREATE);
      return false;
    }
  }
  else
  {
    HANDLE hFile=CreateFile(Name,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
      ErrHandler.CreateErrorMsg(Name);
      return false;
    }
    CloseHandle(hFile);
  }

  if (hd->RedirType==FSREDIR_JUNCTION)
  {
    rdb->ReparseTag=IO_REPARSE_TAG_MOUNT_POINT;
    rdb->ReparseDataLength=USHORT(
      sizeof(rdb->MountPointReparseBuffer.SubstituteNameOffset)+
      sizeof(rdb->MountPointReparseBuffer.SubstituteNameLength)+
      sizeof(rdb->MountPointReparseBuffer.PrintNameOffset)+
      sizeof(rdb->MountPointReparseBuffer.PrintNameLength)+
      (SubstLength+1)*sizeof(WCHAR)+(PrintLength+1)*sizeof(WCHAR));
    rdb->Reserved=0;

    rdb->MountPointReparseBuffer.SubstituteNameOffset=0;
    rdb->MountPointReparseBuffer.SubstituteNameLength=USHORT(SubstLength*sizeof(WCHAR));
    wcscpy(rdb->MountPointReparseBuffer.PathBuffer,SubstName.data());

    rdb->MountPointReparseBuffer.PrintNameOffset=USHORT((SubstLength+1)*sizeof(WCHAR));
    rdb->MountPointReparseBuffer.PrintNameLength=USHORT(PrintLength*sizeof(WCHAR));
    wcscpy(rdb->MountPointReparseBuffer.PathBuffer+SubstLength+1,PrintName.data());
  }
  else
    if (hd->RedirType==FSREDIR_WINSYMLINK || hd->RedirType==FSREDIR_UNIXSYMLINK)
    {
      rdb->ReparseTag=IO_REPARSE_TAG_SYMLINK;
      rdb->ReparseDataLength=USHORT(
        sizeof(rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset)+
        sizeof(rdb->SymbolicLinkReparseBuffer.SubstituteNameLength)+
        sizeof(rdb->SymbolicLinkReparseBuffer.PrintNameOffset)+
        sizeof(rdb->SymbolicLinkReparseBuffer.PrintNameLength)+
        sizeof(rdb->SymbolicLinkReparseBuffer.Flags)+
        (SubstLength+1)*sizeof(WCHAR)+(PrintLength+1)*sizeof(WCHAR));
      rdb->Reserved=0;

      rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset=0;
      rdb->SymbolicLinkReparseBuffer.SubstituteNameLength=USHORT(SubstLength*sizeof(WCHAR));
      wcscpy(rdb->SymbolicLinkReparseBuffer.PathBuffer,SubstName.data());

      rdb->SymbolicLinkReparseBuffer.PrintNameOffset=USHORT((SubstLength+1)*sizeof(WCHAR));
      rdb->SymbolicLinkReparseBuffer.PrintNameLength=USHORT(PrintLength*sizeof(WCHAR));
      wcscpy(rdb->SymbolicLinkReparseBuffer.PathBuffer+SubstLength+1,PrintName.data());

      rdb->SymbolicLinkReparseBuffer.Flags=AbsPath ? 0:SYMLINK_FLAG_RELATIVE;
    }
    else
      return false;

  HANDLE hFile=CreateFile(Name,GENERIC_READ|GENERIC_WRITE,0,NULL,
               OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|
               FILE_FLAG_BACKUP_SEMANTICS,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
  {
    ErrHandler.CreateErrorMsg(Name);
    ErrHandler.SetErrorCode(RARX_CREATE);
    return false;
  }

  DWORD Returned;
  if (!DeviceIoControl(hFile,FSCTL_SET_REPARSE_POINT,rdb,
      FIELD_OFFSET(REPARSE_DATA_BUFFER,GenericReparseBuffer)+
      rdb->ReparseDataLength,NULL,0,&Returned,NULL))
  {
    CloseHandle(hFile);
    uiMsg(UIERROR_SLINKCREATE,L"",Name);

    DWORD LastError=GetLastError();
    if ((LastError==ERROR_ACCESS_DENIED || LastError==ERROR_PRIVILEGE_NOT_HELD) &&
        !IsUserAdmin())
      uiMsg(UIERROR_NEEDADMIN);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);

    if (hd->Dir)
      RemoveDirectory(Name);
    else
      DeleteFile(Name);
    return false;
  }
  File LinkFile;
  LinkFile.SetHandle(hFile);
  LinkFile.SetOpenFileTime(
    Cmd->xmtime==EXTTIME_NONE ? NULL:&hd->mtime,
    Cmd->xctime==EXTTIME_NONE ? NULL:&hd->ctime,
    Cmd->xatime==EXTTIME_NONE ? NULL:&hd->atime);
  LinkFile.Close();
  if (!Cmd->IgnoreGeneralAttr)
    SetFileAttr(Name,hd->FileAttr);
  return true;
}

#endif

#ifdef _UNIX

void ExtractUnixOwner30(Archive &Arc,const wchar *FileName)
{

  if (memchr(Arc.SubHead.SubData.data(),0,Arc.SubHead.SubData.size())==NULL)
    return;

  char *OwnerName=(char *)Arc.SubHead.SubData.data();
  int OwnerSize=strlen(OwnerName)+1;
  int GroupSize=Arc.SubHead.SubData.size()-OwnerSize;
  char *GroupName=(char *)&Arc.SubHead.SubData[OwnerSize];
  std::string GroupStr(GroupName,GroupName+GroupSize);

  struct passwd *pw;
  if ((pw=getpwnam(OwnerName))==NULL)
  {
    uiMsg(UIERROR_UOWNERGETOWNERID,Arc.FileName,GetWide(OwnerName));
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }
  uid_t OwnerID=pw->pw_uid;

  struct group *gr;
  if ((gr=getgrnam(GroupStr.c_str()))==NULL)
  {
    uiMsg(UIERROR_UOWNERGETGROUPID,Arc.FileName,GetWide(GroupName));
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }
  uint Attr=GetFileAttr(FileName);
  gid_t GroupID=gr->gr_gid;

  std::string NameA;
  WideToChar(FileName,NameA);

#if defined(SAVE_LINKS) && !defined(_APPLE)
  if (lchown(NameA.c_str(),OwnerID,GroupID)!=0)
#else
  if (chown(NameA.c_str(),OwnerID,GroupID)!=0)
#endif
  {
    uiMsg(UIERROR_UOWNERSET,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  SetFileAttr(FileName,Attr);
}

void SetUnixOwner(Archive &Arc,const std::wstring &FileName)
{

  FileHeader &hd=Arc.FileHead;
  if (*hd.UnixOwnerName!=0)
  {
    struct passwd *pw;
    if ((pw=getpwnam(hd.UnixOwnerName))==NULL)
    {
      if (!hd.UnixOwnerNumeric)
      {
        uiMsg(UIERROR_UOWNERGETOWNERID,Arc.FileName,GetWide(hd.UnixOwnerName));
        ErrHandler.SetErrorCode(RARX_WARNING);
        return;
      }
    }
    else
      hd.UnixOwnerID=pw->pw_uid;
  }
  if (*hd.UnixGroupName!=0)
  {
    struct group *gr;
    if ((gr=getgrnam(hd.UnixGroupName))==NULL)
    {
      if (!hd.UnixGroupNumeric)
      {
        uiMsg(UIERROR_UOWNERGETGROUPID,Arc.FileName,GetWide(hd.UnixGroupName));
        ErrHandler.SetErrorCode(RARX_WARNING);
        return;
      }
    }
    else
      hd.UnixGroupID=gr->gr_gid;
  }

  std::string NameA;
  WideToChar(FileName,NameA);

#if defined(SAVE_LINKS) && !defined(_APPLE)
  if (lchown(NameA.c_str(),hd.UnixOwnerID,hd.UnixGroupID)!=0)
#else
  if (chown(NameA.c_str(),hd.UnixOwnerID,hd.UnixGroupID)!=0)
#endif
  {
    uiMsg(UIERROR_UOWNERSET,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
}

#ifdef SAVE_LINKS

static bool UnixSymlink(CommandData *Cmd,const std::string &Target,const wchar *LinkName,RarTime *ftm,RarTime *fta)
{
  CreatePath(LinkName,true,Cmd->DisableNames);

  DelFile(LinkName);

  std::string LinkNameA;
  WideToChar(LinkName,LinkNameA);
  if (symlink(Target.c_str(),LinkNameA.c_str())==-1)
  {
    if (errno==EEXIST)
      uiMsg(UIERROR_ULINKEXIST,LinkName);
    else
    {
      uiMsg(UIERROR_SLINKCREATE,L"",LinkName);
      ErrHandler.SetErrorCode(RARX_WARNING);
    }
    return false;
  }
#ifdef USE_LUTIMES
#ifdef UNIX_TIME_NS
  timespec times[2];
  times[0].tv_sec=fta->GetUnix();
  times[0].tv_nsec=fta->IsSet() ? long(fta->GetUnixNS()%1000000000) : UTIME_NOW;
  times[1].tv_sec=ftm->GetUnix();
  times[1].tv_nsec=ftm->IsSet() ? long(ftm->GetUnixNS()%1000000000) : UTIME_NOW;
  utimensat(AT_FDCWD,LinkNameA.c_str(),times,AT_SYMLINK_NOFOLLOW);
#else
  struct timeval tv[2];
  tv[0].tv_sec=fta->GetUnix();
  tv[0].tv_usec=long(fta->GetUnixNS()%1000000000/1000);
  tv[1].tv_sec=ftm->GetUnix();
  tv[1].tv_usec=long(ftm->GetUnixNS()%1000000000/1000);
  lutimes(LinkNameA.c_str(),tv);
#endif
#endif

  return true;
}

static bool IsFullPath(const char *PathA)
{
  return *PathA==CPATHDIVIDER;
}

static bool SafeCharToWide(const std::string &Src,std::wstring &Dest)
{
  if (!CharToWide(Src,Dest) || Dest.empty())
    return false;
  uint SrcChars=0,DestChars=0;
  for (uint I=0;Src[I]!=0;I++)
    if (Src[I]=='/' || Src[I]=='.')
      SrcChars++;
  for (uint I=0;Dest[I]!=0;I++)
    if (Dest[I]=='/' || Dest[I]=='.')
      DestChars++;
  return SrcChars==DestChars;
}

static bool ExtractUnixLink30(CommandData *Cmd,ComprDataIO &DataIO,Archive &Arc,
                              const wchar *LinkName,bool &UpLink)
{
  if (IsLink(Arc.FileHead.FileAttr))
  {
    size_t DataSize=(size_t)Arc.FileHead.PackSize;
    if (DataSize>MAXPATHSIZE)
      return false;
    std::vector<char> TargetBuf(DataSize+1);
    if ((size_t)DataIO.UnpRead((byte*)TargetBuf.data(),DataSize)!=DataSize)
      return false;
    std::string Target(TargetBuf.data(),TargetBuf.size());

    DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,1);
    DataIO.UnpHash.Update(Target.data(),strlen(Target.data()));
    DataIO.UnpHash.Result(&Arc.FileHead.FileHash);

    if (!DataIO.UnpHash.Cmp(&Arc.FileHead.FileHash,Arc.FileHead.UseHashKey ? Arc.FileHead.HashKey:NULL))
      return true;

    std::wstring TargetW;
    if (!SafeCharToWide(Target.data(),TargetW))
      return false;
    TruncateAtZero(TargetW);

    if (!Cmd->AbsoluteLinks && (IsFullPath(TargetW) ||
        !IsRelativeSymlinkSafe(Cmd,Arc.FileHead.FileName.c_str(),LinkName,TargetW.c_str())))
    {
      uiMsg(UIERROR_SKIPUNSAFELINK,Arc.FileHead.FileName,TargetW);
      ErrHandler.SetErrorCode(RARX_WARNING);
      return false;
    }
    UpLink=Target.find("..")!=std::string::npos;
    return UnixSymlink(Cmd,Target,LinkName,&Arc.FileHead.mtime,&Arc.FileHead.atime);
  }
  return false;
}

static bool ExtractUnixLink50(CommandData *Cmd,const wchar *Name,FileHeader *hd)
{
  std::string Target;
  WideToChar(hd->RedirName,Target);
  if (hd->RedirType==FSREDIR_WINSYMLINK || hd->RedirType==FSREDIR_JUNCTION)
  {

    if (Target.rfind("\\??\\",0)!=std::string::npos ||
        Target.rfind("/\?\?/",0)!=std::string::npos)
    {
#if 0
      uiMsg(UIERROR_SLINKCREATE,nullptr,L"\"" + hd->FileName + L"\" -> \"" + hd->RedirName + L"\"");
      ErrHandler.SetErrorCode(RARX_WARNING);
      return false;
#endif

      Target=Target.substr(4);
    }
    DosSlashToUnix(Target,Target);
  }

  std::wstring TargetW;
  if (!SafeCharToWide(Target,TargetW))
    return false;

  if (!Cmd->AbsoluteLinks && (IsFullPath(TargetW) ||
      !IsRelativeSymlinkSafe(Cmd,hd->FileName.c_str(),Name,TargetW.c_str())))
  {
    uiMsg(UIERROR_SKIPUNSAFELINK,hd->FileName,TargetW);
    ErrHandler.SetErrorCode(RARX_WARNING);
    return false;
  }
  return UnixSymlink(Cmd,Target,Name,&hd->mtime,&hd->atime);
}

#endif
#endif

#ifndef SFX_MODULE
void SetExtraInfo20(CommandData *Cmd,Archive &Arc,const std::wstring &Name)
{
#ifdef _WIN_ALL
  if (Cmd->Test)
    return;
  switch(Arc.SubBlockHead.SubType)
  {
    case NTACL_HEAD:
      if (Cmd->ProcessOwners)
        ExtractACL20(Arc,Name);
      break;
    case STREAM_HEAD:
      ExtractStreams20(Arc,Name);
      break;
  }
#endif
}
#endif

void SetExtraInfo(CommandData *Cmd,Archive &Arc,const std::wstring &Name)
{
#ifdef _UNIX
  if (!Cmd->Test && Cmd->ProcessOwners && Arc.Format==RARFMT15 &&
      Arc.SubHead.CmpName(SUBHEAD_TYPE_UOWNER))
    ExtractUnixOwner30(Arc,Name.c_str());
#endif
#ifdef _WIN_ALL
  if (!Cmd->Test && Cmd->ProcessOwners && Arc.SubHead.CmpName(SUBHEAD_TYPE_ACL))
    ExtractACL(Arc,Name);
  if (Arc.SubHead.CmpName(SUBHEAD_TYPE_STREAM))
    ExtractStreams(Arc,Name,Cmd->Test);
#endif
}

void SetFileHeaderExtra(CommandData *Cmd,Archive &Arc,const std::wstring &Name)
{
#ifdef _UNIX
   if (Cmd->ProcessOwners && Arc.Format==RARFMT50 && Arc.FileHead.UnixOwnerSet)
     SetUnixOwner(Arc,Name);
#endif
}

static int CalcAllowedDepth(const std::wstring &Name)
{
  int AllowedDepth=0;
  for (size_t I=0;I<Name.size();I++)
    if (IsPathDiv(Name[I]))
    {
      bool Dot=Name[I+1]=='.' && (IsPathDiv(Name[I+2]) || Name[I+2]==0);
      bool Dot2=Name[I+1]=='.' && Name[I+2]=='.' && (IsPathDiv(Name[I+3]) || Name[I+3]==0);
      if (!Dot && !Dot2)
        AllowedDepth++;
      else
        if (Dot2)
          AllowedDepth--;
    }
  return AllowedDepth < 0 ? 0 : AllowedDepth;
}

static bool LinkInPath(std::wstring Path)
{
  if (Path.empty())
    return false;
  for (size_t I=Path.size()-1;I>0;I--)
    if (IsPathDiv(Path[I]))
    {
      Path.erase(I);
      FindData FD;
      if (FindFile::FastFind(Path,&FD,true) && (FD.IsLink || !FD.IsDir))
        return true;
    }
  return false;
}

bool IsRelativeSymlinkSafe(CommandData *Cmd,const std::wstring &SrcName,std::wstring PrepSrcName,const std::wstring &TargetName)
{

  if (IsFullRootPath(SrcName) || IsFullRootPath(TargetName))
    return false;

  int UpLevels=0;
  for (uint Pos=0;Pos<TargetName.size();Pos++)
  {
    bool Dot2=TargetName[Pos]=='.' && TargetName[Pos+1]=='.' &&
              (IsPathDiv(TargetName[Pos+2]) || TargetName[Pos+2]==0) &&
              (Pos==0 || IsPathDiv(TargetName[Pos-1]));
    if (Dot2)
      UpLevels++;
  }

  if (UpLevels>0 && LinkInPath(PrepSrcName))
    return false;

  int AllowedDepth=CalcAllowedDepth(SrcName);

  size_t ExtrPathLength=Cmd->ExtrPath.size();
  if (ExtrPathLength>0 && PrepSrcName.compare(0,ExtrPathLength,Cmd->ExtrPath)==0)
  {
    while (IsPathDiv(PrepSrcName[ExtrPathLength]))
      ExtrPathLength++;
    PrepSrcName.erase(0,ExtrPathLength);
  }
  int PrepAllowedDepth=CalcAllowedDepth(PrepSrcName);

  return AllowedDepth>=UpLevels && PrepAllowedDepth>=UpLevels;
}

bool ExtractSymlink(CommandData *Cmd,ComprDataIO &DataIO,Archive &Arc,const std::wstring &LinkName,bool &UpLink)
{

  UpLink=true;
#if defined(SAVE_LINKS) && defined(_UNIX) || defined(_WIN_ALL)
  if (Arc.Format==RARFMT50)
    UpLink=Arc.FileHead.RedirName.find(L"..")!=std::wstring::npos;
#endif

#if defined(SAVE_LINKS) && defined(_UNIX)

  if (Arc.Format==RARFMT15)
    return ExtractUnixLink30(Cmd,DataIO,Arc,LinkName.c_str(),UpLink);
  if (Arc.Format==RARFMT50)
    return ExtractUnixLink50(Cmd,LinkName.c_str(),&Arc.FileHead);
#elif defined(_WIN_ALL)

  if (Arc.Format==RARFMT50)
    return CreateReparsePoint(Cmd,LinkName.c_str(),&Arc.FileHead);
#endif
  return false;
}

CmdExtract::CmdExtract(CommandData *Cmd)
{
  CmdExtract::Cmd=Cmd;

  ArcAnalyzed=false;
  Analyze={};

  TotalFileCount=0;

  ConvertSymlinkPaths=true;

  Unp=new Unpack(&DataIO);
#ifdef RAR_SMP
  Unp->SetThreads(Cmd->Threads);
#endif
  Unp->AllowLargePages(Cmd->UseLargePages);
}

CmdExtract::~CmdExtract()
{
  FreeAnalyzeData();
  delete Unp;
}

void CmdExtract::FreeAnalyzeData()
{
  for (size_t I=0;I<RefList.size();I++)
  {

    if (!RefList[I].TmpName.empty())
      DelFile(RefList[I].TmpName);
  }
  RefList.clear();

  Analyze={};
}

void CmdExtract::DoExtract()
{
#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
  Fat32=NotFat32=false;
#endif
  SuppressNoFilesMessage=false;
  DataIO.SetCurrentCommand(Cmd->Command[0]);

  if (Cmd->UseStdin.empty())
  {
    FindData FD;
    while (Cmd->GetArcName(ArcName))
      if (FindFile::FastFind(ArcName,&FD))
        DataIO.TotalArcSize+=FD.Size;
  }

  Cmd->ArcNames.Rewind();
  while (Cmd->GetArcName(ArcName))
  {
    if (Cmd->ManualPassword)
      Cmd->Password.Clean();

    ReconstructDone=false;
    UseExactVolName=false;
    while (true)
    {
      EXTRACT_ARC_CODE Code=ExtractArchive();
      if (Code!=EXTRACT_ARC_REPEAT)
        break;
    }
    DataIO.ProcessedArcSize+=DataIO.LastArcSize;
  }

  if (Cmd->ManualPassword)
    Cmd->Password.Clean();

  if (TotalFileCount==0 && Cmd->Command[0]!='I' &&
      ErrHandler.GetErrorCode()!=RARX_BADPWD)
  {
    if (!SuppressNoFilesMessage)
      uiMsg(UIERROR_NOFILESTOEXTRACT,ArcName);

    if (ErrHandler.GetErrorCode()==RARX_SUCCESS)
      ErrHandler.SetErrorCode(RARX_NOFILES);
  }
  else
    if (!Cmd->DisableDone)
      if (Cmd->Command[0]=='I')
        mprintf(St(MDone));
      else
        if (ErrHandler.GetErrorCount()==0)
          mprintf(St(MExtrAllOk));
        else
          mprintf(St(MExtrTotalErr),ErrHandler.GetErrorCount());
}

void CmdExtract::ExtractArchiveInit(Archive &Arc)
{
  if (Cmd->Command[0]=='T' || Cmd->Command[0]=='I')
    Cmd->Test=true;

#ifdef PROPAGATE_MOTW

  if (!Cmd->Test && Cmd->MotwList.ItemsCount()>0)
    Arc.Motw.ReadZoneIdStream(Arc.FileName,Cmd->MotwAllFields);
#endif

  DataIO.AdjustTotalArcSize(&Arc);

  FileCount=0;
  MatchedArgs=0;
#ifndef SFX_MODULE
  FirstFile=true;
#endif

  GlobalPassword=Cmd->Password.IsSet() || uiIsGlobalPasswordSet();

  DataIO.UnpVolume=false;

  PrevProcessed=false;
  AllMatchesExact=true;
  AnySolidDataUnpackedWell=false;

  ArcAnalyzed=false;

  StartTime.SetCurrentTime();

  LastCheckedSymlink.clear();
}

EXTRACT_ARC_CODE CmdExtract::ExtractArchive()
{
  Archive Arc(Cmd);
  if (!Cmd->UseStdin.empty())
  {
    Arc.SetHandleType(FILE_HANDLESTD);
#ifdef USE_QOPEN
    Arc.SetProhibitQOpen(true);
#endif
  }
  else
  {

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    if (Cmd->Command[0]=='T' || Cmd->Test)
      ResetFileCache(ArcName);
#endif
    if (!Arc.WOpen(ArcName))
      return EXTRACT_ARC_NEXT;
  }

  if (!Arc.IsArchive(true))
  {
#if !defined(SFX_MODULE) && !defined(RARDLL)
    if (CmpExt(ArcName,L"rev"))
    {
      std::wstring FirstVolName;
      VolNameToFirstName(ArcName,FirstVolName,true);

      if (wcsicomp(ArcName,FirstVolName)!=0 && FileExist(FirstVolName) &&
          Cmd->ArcNames.Search(FirstVolName,false))
        return EXTRACT_ARC_NEXT;
      RecVolumesTest(Cmd,NULL,ArcName);
      TotalFileCount++;
      return EXTRACT_ARC_NEXT;
    }
#endif

    bool RarExt=false;
#ifndef SFX_MODULE
    RarExt=CmpExt(ArcName,L"rar");
#endif

    if (RarExt)
      uiMsg(UIERROR_BADARCHIVE,ArcName);
    else
      mprintf(St(MNotRAR),ArcName.c_str());

    if (RarExt)
      ErrHandler.SetErrorCode(RARX_BADARC);
    return EXTRACT_ARC_NEXT;
  }

  if (Arc.FailedHeaderDecryption)
    return EXTRACT_ARC_NEXT;

#ifndef SFX_MODULE
  if (Arc.Volume && !Arc.FirstVolume && !UseExactVolName)
  {
    std::wstring FirstVolName;
    VolNameToFirstName(ArcName,FirstVolName,Arc.NewNumbering);

    if (wcsicomp(ArcName,FirstVolName)!=0 && FileExist(FirstVolName) &&
        Cmd->ArcNames.Search(FirstVolName,false))
      return EXTRACT_ARC_NEXT;
  }
#endif

  Arc.ViewComment();

  int64 VolumeSetSize=0;

#ifndef SFX_MODULE
  if (!ArcAnalyzed && Cmd->UseStdin.empty())
  {
    AnalyzeArchive(Arc.FileName,Arc.Volume,Arc.NewNumbering);
    ArcAnalyzed=true;
  }
#endif

  if (Arc.Volume)
  {
#ifndef SFX_MODULE

    if (!Analyze.StartName.empty())
    {
      ArcName=Analyze.StartName;
      Analyze.StartName.clear();

      UseExactVolName=true;
      return EXTRACT_ARC_REPEAT;
    }
#endif

    std::wstring NextName=Arc.FileName;

    while (true)
    {

      NextVolumeName(NextName,!Arc.NewNumbering);
      FindData FD;
      if (FindFile::FastFind(NextName,&FD))
        VolumeSetSize+=FD.Size;
      else
        break;
    }
    DataIO.TotalArcSize+=VolumeSetSize;
  }

  ExtractArchiveInit(Arc);

  if (Cmd->Command[0]=='I')
  {
    Cmd->DisablePercentage=true;
  }
  else
    uiStartArchiveExtract(!Cmd->Test,ArcName);

#ifndef SFX_MODULE
  if (Analyze.StartPos!=0)
  {
    Arc.Seek(Analyze.StartPos,SEEK_SET);
    Analyze.StartPos=0;
  }
#endif

  while (1)
  {
    size_t Size=Arc.ReadHeader();

    bool Repeat=false;
    if (!ExtractCurrentFile(Arc,Size,Repeat))
      if (Repeat)
      {

        FindData NewArc;
        if (FindFile::FastFind(ArcName,&NewArc))
          DataIO.TotalArcSize=NewArc.Size;
        return EXTRACT_ARC_REPEAT;
      }
      else
        break;
  }

#if !defined(SFX_MODULE) && !defined(RARDLL)
  if (Cmd->Test && Arc.Volume)
    RecVolumesTest(Cmd,&Arc,ArcName);
#endif

  return EXTRACT_ARC_NEXT;
}

bool CmdExtract::ExtractCurrentFile(Archive &Arc,size_t HeaderSize,bool &Repeat)
{
  wchar Command=Cmd->Command[0];
  if (HeaderSize==0)
    if (DataIO.UnpVolume)
    {
#ifdef NOVOLUME
      return false;
#else

      if (!MergeArchive(Arc,&DataIO,false,Command))
      {
        ErrHandler.SetErrorCode(RARX_WARNING);
        return false;
      }
#endif
    }
    else
      return false;

  HEADER_TYPE HeaderType=Arc.GetHeaderType();
  if (HeaderType==HEAD_FILE)
  {

    if (Analyze.EndPos!=0 && Analyze.EndPos==Arc.CurBlockPos &&
        (Analyze.EndName.empty() || Analyze.EndName==Arc.FileName))
      return false;
  }
  else
  {
#ifndef SFX_MODULE
    if (Arc.Format==RARFMT15 && HeaderType==HEAD3_OLDSERVICE && PrevProcessed)
      SetExtraInfo20(Cmd,Arc,DestFileName);
#endif
    if (HeaderType==HEAD_SERVICE && PrevProcessed)
      SetExtraInfo(Cmd,Arc,DestFileName);
    if (HeaderType==HEAD_ENDARC)
      if (Arc.EndArcHead.NextVolume)
      {
#ifdef NOVOLUME
        return false;
#else
        if (!MergeArchive(Arc,&DataIO,false,Command))
        {
          ErrHandler.SetErrorCode(RARX_WARNING);
          return false;
        }
        Arc.Seek(Arc.CurBlockPos,SEEK_SET);
        return true;
#endif
      }
      else
        return false;
    Arc.SeekToNext();
    return true;
  }
  PrevProcessed=false;

  if (Arc.FileHead.PackSize<0)
    Arc.FileHead.PackSize=0;
  if (Arc.FileHead.UnpSize<0)
    Arc.FileHead.UnpSize=0;

  if (!Cmd->Recurse && MatchedArgs>=Cmd->FileArgs.ItemsCount() && AllMatchesExact)
    return false;

  int MatchType=MATCH_WILDSUBPATH;

  bool EqualNames=false;
  std::wstring MatchedArg;
  bool MatchFound=Cmd->IsProcessFile(Arc.FileHead,&EqualNames,MatchType,0,&MatchedArg)!=0;
#ifndef SFX_MODULE
  if (Cmd->ExclPath==EXCL_BASEPATH)
  {
    Cmd->ArcPath=MatchedArg;
    GetPathWithSep(Cmd->ArcPath,Cmd->ArcPath);
    if (IsWildcard(Cmd->ArcPath))
      Cmd->ArcPath.clear();
  }
#endif
  if (MatchFound && !EqualNames)
    AllMatchesExact=false;

  Arc.ConvertAttributes();

#if !defined(SFX_MODULE) && !defined(RARDLL)
  if (Arc.FileHead.SplitBefore && FirstFile && !UseExactVolName)
  {
    std::wstring StartVolName;
    GetFirstVolIfFullSet(ArcName,Arc.NewNumbering,StartVolName);

    if (StartVolName!=ArcName && FileExist(StartVolName))
    {
      ArcName=StartVolName;
      Cmd->ArcName=ArcName;

      Repeat=true;
      return false;
    }
#ifndef RARDLL
    if (!ReconstructDone)
    {
      ReconstructDone=true;
      if (RecVolumesRestore(Cmd,Arc.FileName,true))
      {
        Repeat=true;
        return false;
      }
    }
#endif
  }
#endif

  std::wstring ArcFileName;
  ConvertPath(&Arc.FileHead.FileName,&ArcFileName);

  if (Arc.FileHead.Version)
  {
    if (Cmd->VersionControl!=1 && !EqualNames)
    {
      if (Cmd->VersionControl==0)
        MatchFound=false;
      int Version=ParseVersionFileName(ArcFileName,false);
      if (Cmd->VersionControl-1==Version)
        ParseVersionFileName(ArcFileName,true);
      else
        MatchFound=false;
    }
  }
  else
    if (!Arc.IsArcDir() && Cmd->VersionControl>1)
      MatchFound=false;

  DataIO.UnpVolume=Arc.FileHead.SplitAfter;
  DataIO.NextVolumeMissing=false;

  Arc.Seek(Arc.NextBlockPos-Arc.FileHead.PackSize,SEEK_SET);

  bool ExtrFile=false;
  bool SkipSolid=false;

#ifndef SFX_MODULE
  if (FirstFile && (MatchFound || Arc.Solid) && Arc.FileHead.SplitBefore)
  {
    if (MatchFound)
    {
      uiMsg(UIERROR_NEEDPREVVOL,Arc.FileName,ArcFileName);
#ifdef RARDLL
      Cmd->DllError=ERAR_BAD_DATA;
#endif
      ErrHandler.SetErrorCode(RARX_OPEN);
    }
    MatchFound=false;
  }

  FirstFile=false;
#endif

  bool RefTarget=false;
  if (!MatchFound)
    for (size_t I=0;I<RefList.size();I++)
      if (ArcFileName == RefList[I].RefName)
      {
        ExtractRef &MatchedRef=RefList[I];

        if (!Cmd->Test)
        {

          DestFileName=!Cmd->TempPath.empty() ? Cmd->TempPath:Cmd->ExtrPath;
          AddEndSlash(DestFileName);
          DestFileName+=L"__tmp_reference_source_";
          MkTemp(DestFileName,nullptr);
          MatchedRef.TmpName=DestFileName;
        }
        RefTarget=true;
        break;
      }

  if (Arc.FileHead.Encrypted && Cmd->SkipEncrypted)
    if (Arc.Solid)
      return false;
    else
      MatchFound=false;

  if (MatchFound || RefTarget || (SkipSolid=Arc.Solid)!=false)
  {

    if (!uiStartFileExtract(ArcFileName,!Cmd->Test,Cmd->Test && Command!='I',SkipSolid))
      return false;

    if (!RefTarget)
      ExtrPrepareName(Arc,ArcFileName,DestFileName);

    ExtrFile=!SkipSolid && !DestFileName.empty() && !Arc.FileHead.SplitBefore;

    if ((Cmd->FreshFiles || Cmd->UpdateFiles) && (Command=='E' || Command=='X'))
    {
      FindData FD;
      if (FindFile::FastFind(DestFileName,&FD))
      {
        if (FD.mtime >= Arc.FileHead.mtime)
        {

          if (!FD.IsDir || FD.mtime<StartTime)
            ExtrFile=false;
        }
      }
      else
        if (Cmd->FreshFiles)
          ExtrFile=false;
    }

    if (!CheckUnpVer(Arc,ArcFileName))
    {
      ErrHandler.SetErrorCode(RARX_FATAL);
#ifdef RARDLL
      Cmd->DllError=ERAR_UNKNOWN_FORMAT;
#endif
      Arc.SeekToNext();
      return !Arc.Solid;
    }

#ifndef RAR_NOCRYPT
    if (Arc.FileHead.Encrypted)
    {
      RarCheckPassword CheckPwd;
      if (Arc.Format==RARFMT50 && Arc.FileHead.UsePswCheck && !Arc.BrokenHeader)
        CheckPwd.Set(Arc.FileHead.Salt,Arc.FileHead.InitV,Arc.FileHead.Lg2Count,Arc.FileHead.PswCheck);

      while (true)
      {

#ifdef RARDLL
        if (!ExtrDllGetPassword())
        {
          Cmd->DllError=ERAR_MISSING_PASSWORD;
          return false;
        }
#else
        if (!ExtrGetPassword(Arc,ArcFileName,CheckPwd.IsSet() ? &CheckPwd:NULL))
        {
          SuppressNoFilesMessage=true;
          return false;
        }
#endif

        SecPassword FilePassword=Cmd->Password;
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
        ConvertDosPassword(Arc,FilePassword);
#endif

        byte PswCheck[SIZE_PSWCHECK];
        bool EncSet=DataIO.SetEncryption(false,Arc.FileHead.CryptMethod,
               &FilePassword,Arc.FileHead.SaltSet ? Arc.FileHead.Salt:nullptr,
               Arc.FileHead.InitV,Arc.FileHead.Lg2Count,
               Arc.FileHead.HashKey,PswCheck);

        if (EncSet && Arc.FileHead.UsePswCheck && !Arc.BrokenHeader &&
            memcmp(Arc.FileHead.PswCheck,PswCheck,SIZE_PSWCHECK)!=0)
        {
          if (GlobalPassword)
          {

            uiMsg(UIERROR_BADPSW,Arc.FileName,ArcFileName);
          }
          else
          {

            uiMsg(UIWAIT_BADPSW,Arc.FileName,ArcFileName);
            Cmd->Password.Clean();

#ifndef RARDLL
            continue;
#endif
          }
#ifdef RARDLL

          if (Cmd->DllError!=ERAR_EOPEN)
            Cmd->DllError=ERAR_BAD_PASSWORD;
#endif
          ErrHandler.SetErrorCode(RARX_BADPWD);
          ExtrFile=false;
        }
        break;
      }
    }
    else
      DataIO.SetEncryption(false,CRYPT_NONE,NULL,NULL,NULL,0,NULL,NULL);
#endif

    bool CurConvertSymlinkPaths=ConvertSymlinkPaths;

#ifdef RARDLL
    if (!Cmd->DllDestName.empty())
    {
      DestFileName=Cmd->DllDestName;

      CurConvertSymlinkPaths=false;
    }
#endif

    if (ExtrFile && Command!='P' && !Cmd->Test && !Cmd->AbsoluteLinks &&
        CurConvertSymlinkPaths)
      ExtrFile=LinksToDirs(DestFileName,Cmd->ExtrPath,LastCheckedSymlink);

    File CurFile;

    bool LinkEntry=Arc.FileHead.RedirType!=FSREDIR_NONE;
    if (LinkEntry && (Arc.FileHead.RedirType!=FSREDIR_FILECOPY))
    {
      if (Cmd->SkipSymLinks && (Arc.FileHead.RedirType==FSREDIR_UNIXSYMLINK ||
          Arc.FileHead.RedirType==FSREDIR_WINSYMLINK || Arc.FileHead.RedirType==FSREDIR_JUNCTION))
        ExtrFile=false;

      if (ExtrFile && Command!='P' && !Cmd->Test)
      {

        bool UserReject=false;
        if (FileExist(DestFileName))
          FileCreate(Cmd,NULL,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime);
        if (UserReject)
          ExtrFile=false;
      }
    }
    else
      if (Arc.IsArcDir())
      {
        if (!ExtrFile || Command=='P' || Command=='I' || Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
          return true;
        TotalFileCount++;
        ExtrCreateDir(Arc,ArcFileName);

        return true;
      }
      else
        if (ExtrFile)
        {

          if (!CheckWinLimit(Arc,ArcFileName))
            return false;

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
          bool Compressed=Cmd->SetCompressedAttr &&
               (Arc.FileHead.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0;
          bool WriteOnly=!Compressed;
#else
          bool WriteOnly=true;
#endif

          ExtrFile=ExtrCreateFile(Arc,CurFile,WriteOnly);

#if defined(_WIN_ALL) && !defined(SFX_MODULE)

          if (ExtrFile && Compressed)
            SetFileCompression(CurFile.GetHandle(),true);

#endif

        }

    if (!ExtrFile && Arc.Solid)
    {
      SkipSolid=true;
      ExtrFile=true;

      if (!uiStartFileExtract(ArcFileName,false,false,true))
        return false;

      if (!CheckWinLimit(Arc,ArcFileName))
        return false;
    }
    if (ExtrFile)
    {

      if (Cmd->Test)
        PrevProcessed=true;

      bool TestMode=Cmd->Test || SkipSolid;

      if (!SkipSolid)
      {
        if (!TestMode && Command!='P' && CurFile.IsDevice())
        {
          uiMsg(UIERROR_INVALIDNAME,Arc.FileName,DestFileName);
          ErrHandler.WriteError(Arc.FileName,DestFileName);
        }
        TotalFileCount++;
      }
      FileCount++;
      if (Command!='I' && !Cmd->DisableNames)
        if (SkipSolid)
          mprintf(St(MExtrSkipFile),ArcFileName.c_str());
        else
          switch(Cmd->Test ? 'T':Command)
          {
            case 'T':
              mprintf(St(MExtrTestFile),ArcFileName.c_str());
              break;
#ifndef SFX_MODULE
            case 'P':
              mprintf(St(MExtrPrinting),ArcFileName.c_str());
              break;
#endif
            case 'X':
            case 'E':
              mprintf(St(MExtrFile),DestFileName.c_str());
              break;
          }
      if (!Cmd->DisablePercentage && !Cmd->DisableNames)
        mprintf(L"     ");
      if (Cmd->DisableNames)
        uiEolAfterMsg();

      DataIO.CurUnpRead=0;
      DataIO.CurUnpWrite=0;
      DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
      DataIO.PackedDataHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
      DataIO.SetPackedSizeToRead(Arc.FileHead.PackSize);
      DataIO.SetFiles(&Arc,&CurFile);
      DataIO.SetTestMode(TestMode);
      DataIO.SetSkipUnpCRC(SkipSolid);

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
      if (!TestMode && !Arc.BrokenHeader &&
          Arc.FileHead.UnpSize>0xffffffff && (Fat32 || !NotFat32))
      {
        if (!Fat32)
          NotFat32=!(Fat32=IsFAT(Cmd->ExtrPath));
        if (Fat32)
          uiMsg(UIMSG_FAT32SIZE);
      }
#endif

      uint64 Preallocated=0;
      if (!TestMode && !Arc.BrokenHeader && Arc.FileHead.UnpSize>1000000 &&
          Arc.FileHead.PackSize*1024>Arc.FileHead.UnpSize && Arc.IsSeekable() &&
          (Arc.FileHead.UnpSize<100000000 || Arc.FileLength()>Arc.FileHead.PackSize))
      {
        CurFile.Prealloc(Arc.FileHead.UnpSize);
        Preallocated=Arc.FileHead.UnpSize;
      }
      CurFile.SetAllowDelete(!Cmd->KeepBroken);

      bool FileCreateMode=!TestMode && !SkipSolid && Command!='P';
      bool ShowChecksum=true;

      bool LinkSuccess=true;
      if (LinkEntry)
      {
        FILE_SYSTEM_REDIRECT Type=Arc.FileHead.RedirType;

        if (Type==FSREDIR_HARDLINK || Type==FSREDIR_FILECOPY)
        {
          std::wstring RedirName;

          SlashToNative(Arc.FileHead.RedirName,RedirName);

          ConvertPath(&RedirName,&RedirName);

          std::wstring NameExisting;
          ExtrPrepareName(Arc,RedirName,NameExisting);
          if (FileCreateMode && !NameExisting.empty())
            if (Type==FSREDIR_HARDLINK)
              LinkSuccess=ExtractHardlink(Cmd,DestFileName,NameExisting);
            else
              LinkSuccess=ExtractFileCopy(CurFile,Arc.FileName,RedirName,DestFileName,NameExisting,Arc.FileHead.UnpSize);
        }
        else
          if (Type==FSREDIR_UNIXSYMLINK || Type==FSREDIR_WINSYMLINK || Type==FSREDIR_JUNCTION)
          {
            if (FileCreateMode)
            {
              bool UpLink;
              LinkSuccess=ExtractSymlink(Cmd,DataIO,Arc,DestFileName,UpLink);

              ConvertSymlinkPaths|=LinkSuccess && UpLink;

              LastCheckedSymlink.clear();
            }
          }
          else
          {
            uiMsg(UIERROR_UNKNOWNEXTRA,Arc.FileName,ArcFileName);
            LinkSuccess=false;
          }

          if (!LinkSuccess || Arc.Format==RARFMT15 && !FileCreateMode)
          {

            ShowChecksum=false;
          }
          PrevProcessed=FileCreateMode && LinkSuccess;
      }
      else
        if (!Arc.FileHead.SplitBefore)
          if (Arc.FileHead.Method==0)
            UnstoreFile(DataIO,Arc.FileHead.UnpSize);
          else
          {
            try
            {
              Unp->Init(Arc.FileHead.WinSize,Arc.FileHead.Solid);
            }
            catch (std::bad_alloc)
            {
              if (Arc.FileHead.WinSize>=0x40000000)
                uiMsg(UIERROR_EXTRDICTOUTMEM,Arc.FileName,uint(Arc.FileHead.WinSize/0x40000000+(Arc.FileHead.WinSize%0x40000000!=0 ? 1 : 0)));
              throw;
            }

            Unp->SetDestSize(Arc.FileHead.UnpSize);
#ifndef SFX_MODULE

            if (Arc.Format!=RARFMT50 && Arc.FileHead.UnpVer<=15)
              Unp->DoUnpack(15,FileCount>1 && Arc.Solid);
            else
#endif
              Unp->DoUnpack(Arc.FileHead.UnpVer,Arc.FileHead.Solid);
          }

      Arc.SeekToNext();

      bool ValidCRC=!Arc.FileHead.SplitAfter && DataIO.UnpHash.Cmp(&Arc.FileHead.FileHash,Arc.FileHead.UseHashKey ? Arc.FileHead.HashKey:NULL);

      if (!Arc.FileHead.Solid)
        AnySolidDataUnpackedWell=false;
      else
        if (Arc.FileHead.Method!=0 && Arc.FileHead.UnpSize>0 && ValidCRC)
          AnySolidDataUnpackedWell=true;

      bool BrokenFile=false;

      if (!SkipSolid && ShowChecksum)
      {
        if (ValidCRC)
        {
          if (Command!='P' && Command!='I' && !Cmd->DisableNames)
            mprintf(L"%s%s ",Cmd->DisablePercentage ? L" ":L"\b\b\b\b\b ",
              Arc.FileHead.FileHash.Type==HASH_NONE ? L"  ?":St(MOk));
        }
        else
        {
          if (Arc.FileHead.Encrypted && (!Arc.FileHead.UsePswCheck ||
              Arc.BrokenHeader) && !AnySolidDataUnpackedWell)
            uiMsg(UIERROR_CHECKSUMENC,Arc.FileName,ArcFileName);
          else
            uiMsg(UIERROR_CHECKSUM,Arc.FileName,ArcFileName);
          BrokenFile=true;
          ErrHandler.SetErrorCode(RARX_CRC);
#ifdef RARDLL

          if (Cmd->DllError!=ERAR_EOPEN && Cmd->DllError!=ERAR_BAD_PASSWORD)
            Cmd->DllError=ERAR_BAD_DATA;
#endif
        }
      }
      else
      {

        if (SkipSolid)
          mprintf(L"\b\b\b\b\b     ");
      }
      if (!TestMode && (Command=='X' || Command=='E') &&
          (!LinkEntry || LinkSuccess) && (!BrokenFile || Cmd->KeepBroken))
      {

        bool SetAll=!LinkEntry || Arc.FileHead.RedirType==FSREDIR_FILECOPY;

        bool SetTimeAndSize=SetAll;

        bool SetAttr=SetAll || Arc.FileHead.RedirType==FSREDIR_HARDLINK;

        bool SetExtra=SetAll || Arc.FileHead.RedirType==FSREDIR_UNIXSYMLINK;

        if (SetTimeAndSize)
        {

          if (Preallocated>0 && (BrokenFile || DataIO.CurUnpWrite!=Preallocated))
            CurFile.Truncate();

#ifdef PROPAGATE_MOTW
          Arc.Motw.CreateZoneIdStream(DestFileName,Cmd->MotwList);
#endif

          CurFile.SetOpenFileTime(
            Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
            Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.FileHead.ctime,
            Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
          CurFile.Close();
        }

        if (SetExtra)
          SetFileHeaderExtra(Cmd,Arc,DestFileName);

        if (SetTimeAndSize)
          CurFile.SetCloseFileTime(
            Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
            Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);

        if (SetAttr)
        {
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
          if (Cmd->ClearArc)
            Arc.FileHead.FileAttr&=~FILE_ATTRIBUTE_ARCHIVE;
#endif
          if (!Cmd->IgnoreGeneralAttr && !SetFileAttr(DestFileName,Arc.FileHead.FileAttr))
          {
            uiMsg(UIERROR_FILEATTR,Arc.FileName,DestFileName);

            ErrHandler.SysErrMsg();
          }
        }

        PrevProcessed=true;
      }
    }
  }

  if (MatchFound)
    MatchedArgs++;
  if (DataIO.NextVolumeMissing)
    return false;
  if (!ExtrFile)
    if (!Arc.Solid)
      Arc.SeekToNext();
    else
      if (!SkipSolid)
        return false;
  return true;
}

void CmdExtract::UnstoreFile(ComprDataIO &DataIO,int64 DestUnpSize)
{
  std::vector<byte> Buffer(File::CopyBufferSize());
  while (true)
  {
    int ReadSize=DataIO.UnpRead(Buffer.data(),Buffer.size());
    if (ReadSize<=0)
      break;
    int WriteSize=ReadSize<DestUnpSize ? ReadSize:(int)DestUnpSize;
    if (WriteSize>0)
    {
      DataIO.UnpWrite(Buffer.data(),WriteSize);
      DestUnpSize-=WriteSize;
    }
  }
}

bool CmdExtract::ExtractFileCopy(File &New,const std::wstring &ArcName,const std::wstring &RedirName,const std::wstring &NameNew,const std::wstring &NameExisting,int64 UnpSize)
{
  File Existing;
  if (!Existing.Open(NameExisting))
  {
    std::wstring TmpExisting=NameExisting;

    bool OpenFailed=true;

    for (size_t I=0;I<RefList.size();I++)
      if (RedirName==RefList[I].RefName && !RefList[I].TmpName.empty())
      {

        bool RefMove=RefList[I].RefCount-- == 1;
        TmpExisting=RefList[I].TmpName;
        if (RefMove)
        {
          New.Delete();

          bool MoveFailed=!RenameFile(TmpExisting,NameNew);
          if (MoveFailed)
          {

            if (!New.WCreate(NameNew,FMF_WRITE|FMF_SHAREREAD))
              return false;
            RefMove=false;
          }
          else
          {

            if (New.Open(NameNew))
              New.Seek(0,SEEK_END);

            RefList[I].TmpName.clear();
            return true;
          }
        }
        if (!RefMove)
          OpenFailed=!Existing.Open(TmpExisting);
        break;
      }

    if (OpenFailed)
    {
      ErrHandler.OpenErrorMsg(TmpExisting);
      uiMsg(UIERROR_FILECOPY,ArcName,TmpExisting,NameNew);
      uiMsg(UIERROR_FILECOPYHINT,ArcName);
#ifdef RARDLL
      Cmd->DllError=ERAR_EREFERENCE;
#endif
      return false;
    }
  }

  std::vector<byte> Buffer(0x100000);
  int64 CopySize=0;

  while (true)
  {
    Wait();
    int ReadSize=Existing.Read(Buffer.data(),Buffer.size());
    if (ReadSize==0)
      break;

    uiExtractProgress(CopySize,UnpSize,0,0);

    New.Write(Buffer.data(),ReadSize);
    CopySize+=ReadSize;
  }

  return true;
}

void CmdExtract::ExtrPrepareName(Archive &Arc,const std::wstring &ArcFileName,std::wstring &DestName)
{
  if (Cmd->Test)
  {

    DestName=ArcFileName;
    return;
  }

  DestName=Cmd->ExtrPath;

  if (!Cmd->ExtrPath.empty())
  {
    wchar LastChar=GetLastChar(Cmd->ExtrPath);

    if (!IsPathDiv(LastChar) && !IsDriveDiv(LastChar))
    {

      AddEndSlash(DestName);
    }
  }

#ifndef SFX_MODULE
  if (Cmd->AppendArcNameToPath!=APPENDARCNAME_NONE)
  {
    switch(Cmd->AppendArcNameToPath)
    {
      case APPENDARCNAME_DESTPATH:
        DestName+=PointToName(Arc.FirstVolumeName);
        RemoveExt(DestName);
        break;
      case APPENDARCNAME_OWNSUBDIR:
        DestName=Arc.FirstVolumeName;
        RemoveExt(DestName);
        break;
      case APPENDARCNAME_OWNDIR:
        DestName=Arc.FirstVolumeName;
        RemoveNameFromPath(DestName);
        break;
    }
    AddEndSlash(DestName);
  }
#endif

  std::wstring CurName=ArcFileName;
#ifndef SFX_MODULE
  std::wstring &ArcPath=!Cmd->ExclArcPath.empty() ? Cmd->ExclArcPath:Cmd->ArcPath;
  size_t ArcPathLength=ArcPath.size();
  if (ArcPathLength>0)
  {
    size_t NameLength=CurName.size();
    if (NameLength>=ArcPathLength && wcsnicompc(ArcPath,CurName,ArcPathLength)==0 &&
        (IsPathDiv(ArcPath[ArcPathLength-1]) ||
         IsPathDiv(CurName[ArcPathLength]) || CurName[ArcPathLength]==0))
    {
      size_t Pos=Min(ArcPathLength,NameLength);
      while (Pos<CurName.size() && IsPathDiv(CurName[Pos]))
        Pos++;
      CurName.erase(0,Pos);
      if (CurName.empty())
      {
        DestName.clear();
        return;
      }
    }
  }
#endif

  wchar Command=Cmd->Command[0];

  bool AbsPaths=Cmd->ExclPath==EXCL_ABSPATH && Command=='X' && IsDriveDiv(':');

  if (AbsPaths)
  {

    wchar DiskLetter=toupperw(CurName[0]);
    if (CurName[1]=='_' && IsPathDiv(CurName[2]) && DiskLetter>='A' && DiskLetter<='Z')
      DestName=CurName.substr(0,1) + L':' + CurName.substr(2);
    else
      if (CurName[0]=='_' && CurName[1]=='_')
        DestName=std::wstring(2,CPATHDIVIDER) + CurName.substr(2);
      else
        AbsPaths=false;
  }

  if (Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
    CurName=PointToName(CurName);
  if (!AbsPaths)
    DestName+=CurName;

#ifdef _WIN_ALL

  if (!Cmd->AllowIncompatNames)
    MakeNameCompatible(DestName);
#endif
}

#ifdef RARDLL
bool CmdExtract::ExtrDllGetPassword()
{
  if (!Cmd->Password.IsSet())
  {
    if (Cmd->Callback!=NULL)
    {
      wchar PasswordW[MAXPASSWORD];
      *PasswordW=0;
      if (Cmd->Callback(UCM_NEEDPASSWORDW,Cmd->UserData,(LPARAM)PasswordW,ASIZE(PasswordW))==-1)
        *PasswordW=0;
      if (*PasswordW==0)
      {
        char PasswordA[MAXPASSWORD];
        *PasswordA=0;
        if (Cmd->Callback(UCM_NEEDPASSWORD,Cmd->UserData,(LPARAM)PasswordA,ASIZE(PasswordA))==-1)
          *PasswordA=0;
        CharToWide(PasswordA,PasswordW,ASIZE(PasswordW));
        cleandata(PasswordA,sizeof(PasswordA));
      }
      Cmd->Password.Set(PasswordW);
      cleandata(PasswordW,sizeof(PasswordW));
      Cmd->ManualPassword=true;
    }
    if (!Cmd->Password.IsSet())
      return false;
  }
  return true;
}
#endif

#ifndef RARDLL
bool CmdExtract::ExtrGetPassword(Archive &Arc,const std::wstring &ArcFileName,RarCheckPassword *CheckPwd)
{
  if (!Cmd->Password.IsSet())
  {
    if (!uiGetPassword(UIPASSWORD_FILE,ArcFileName,&Cmd->Password,CheckPwd))
    {

      uiMsg(UIERROR_INCERRCOUNT);
      return false;
    }
    Cmd->ManualPassword=true;
  }
#if !defined(SILENT)
  else
    if (!GlobalPassword && !Arc.FileHead.Solid)
    {
      eprintf(St(MUseCurPsw),ArcFileName.c_str());
      switch(Cmd->AllYes ? 1 : Ask(St(MYesNoAll)))
      {
        case -1:
          ErrHandler.Exit(RARX_USERBREAK);
        case 2:
          if (!uiGetPassword(UIPASSWORD_FILE,ArcFileName,&Cmd->Password,CheckPwd))
            return false;
          break;
        case 3:
          GlobalPassword=true;
          break;
      }
    }
#endif
  return true;
}
#endif

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
void CmdExtract::ConvertDosPassword(Archive &Arc,SecPassword &DestPwd)
{
  if (Arc.Format==RARFMT15 && Arc.FileHead.HostOS==HOST_MSDOS)
  {

    wchar PlainPsw[MAXPASSWORD];
    Cmd->Password.Get(PlainPsw,ASIZE(PlainPsw));
    char PswA[MAXPASSWORD];
    CharToOemBuffW(PlainPsw,PswA,ASIZE(PswA));
    PswA[ASIZE(PswA)-1]=0;
    CharToWide(PswA,PlainPsw,ASIZE(PlainPsw));
    DestPwd.Set(PlainPsw);
    cleandata(PlainPsw,sizeof(PlainPsw));
    cleandata(PswA,sizeof(PswA));
  }
}
#endif

void CmdExtract::ExtrCreateDir(Archive &Arc,const std::wstring &ArcFileName)
{
  if (Cmd->Test)
  {
    if (!Cmd->DisableNames)
    {
      mprintf(St(MExtrTestFile),ArcFileName.c_str());
      mprintf(L" %s",St(MOk));
    }
    return;
  }

  MKDIR_CODE MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
  bool DirExist=false;
  if (MDCode!=MKDIR_SUCCESS)
  {
    DirExist=FileExist(DestFileName);
    if (DirExist && !IsDir(GetFileAttr(DestFileName)))
    {

      bool UserReject;
      FileCreate(Cmd,NULL,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime);
      DirExist=false;
    }
    if (!DirExist)
    {
      CreatePath(DestFileName,true,Cmd->DisableNames);
      MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
      if (MDCode!=MKDIR_SUCCESS && !IsNameUsable(DestFileName))
      {
        uiMsg(UIMSG_CORRECTINGNAME,Arc.FileName);
        std::wstring OrigName=DestFileName;
        MakeNameUsable(DestFileName,true);
#ifndef SFX_MODULE
        uiMsg(UIERROR_RENAMING,Arc.FileName,OrigName,DestFileName);
#endif
        DirExist=FileExist(DestFileName) && IsDir(GetFileAttr(DestFileName));
        if (!DirExist && (Cmd->AbsoluteLinks || !ConvertSymlinkPaths ||
            LinksToDirs(DestFileName,Cmd->ExtrPath,LastCheckedSymlink)))
        {
          CreatePath(DestFileName,true,Cmd->DisableNames);
          MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
        }
      }
    }
  }
  if (MDCode==MKDIR_SUCCESS)
  {
    if (!Cmd->DisableNames)
    {
      mprintf(St(MCreatDir),DestFileName.c_str());
      mprintf(L" %s",St(MOk));
    }
    PrevProcessed=true;
  }
  else
    if (DirExist)
    {
      if (!Cmd->IgnoreGeneralAttr)
        SetFileAttr(DestFileName,Arc.FileHead.FileAttr);
      PrevProcessed=true;
    }
    else
    {
      uiMsg(UIERROR_DIRCREATE,Arc.FileName,DestFileName);
      ErrHandler.SysErrMsg();
#ifdef RARDLL
      Cmd->DllError=ERAR_ECREATE;
#endif
      ErrHandler.SetErrorCode(RARX_CREATE);
    }
  if (PrevProcessed)
  {
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    if (Cmd->SetCompressedAttr &&
        (Arc.FileHead.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0 && WinNT()!=WNT_NONE)
      SetFileCompression(DestFileName,true);
#endif
    SetFileHeaderExtra(Cmd,Arc,DestFileName);
    SetDirTime(DestFileName,
      Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
      Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.FileHead.ctime,
      Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
  }
}

bool CmdExtract::ExtrCreateFile(Archive &Arc,File &CurFile,bool WriteOnly)
{
  bool Success=true;
  wchar Command=Cmd->Command[0];
#if !defined(SFX_MODULE)
  if (Command=='P')
    CurFile.SetHandleType(FILE_HANDLESTD);
#endif
  if ((Command=='E' || Command=='X') && !Cmd->Test)
  {
    bool UserReject;
    if (!FileCreate(Cmd,&CurFile,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime,WriteOnly))
    {
      Success=false;
      if (!UserReject)
      {
        ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
        if (FileExist(DestFileName) && IsDir(GetFileAttr(DestFileName)))
          uiMsg(UIERROR_DIRNAMEEXISTS);

#ifdef RARDLL
        Cmd->DllError=ERAR_ECREATE;
#endif
        if (!IsNameUsable(DestFileName))
        {
          uiMsg(UIMSG_CORRECTINGNAME,Arc.FileName);

          std::wstring OrigName=DestFileName;

          MakeNameUsable(DestFileName,true);

          if (Cmd->AbsoluteLinks || !ConvertSymlinkPaths ||
              LinksToDirs(DestFileName,Cmd->ExtrPath,LastCheckedSymlink))
          {
            CreatePath(DestFileName,true,Cmd->DisableNames);
            if (FileCreate(Cmd,&CurFile,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime,true))
            {
#ifndef SFX_MODULE
              uiMsg(UIERROR_RENAMING,Arc.FileName,OrigName,DestFileName);
#endif
              Success=true;
            }
            else
              ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
          }
        }
      }
    }
  }
  return Success;
}

bool CmdExtract::CheckUnpVer(Archive &Arc,const std::wstring &ArcFileName)
{
  bool WrongVer;
  if (Arc.Format==RARFMT50)
    WrongVer=Arc.FileHead.UnpVer>VER_UNPACK7;
  else
  {
#ifdef SFX_MODULE
    WrongVer=Arc.FileHead.UnpVer!=VER_UNPACK;
#else
    WrongVer=Arc.FileHead.UnpVer<13 || Arc.FileHead.UnpVer>VER_UNPACK;
#endif
  }

  if (Arc.FileHead.Method==0)
    WrongVer=false;

  if (Arc.FileHead.CryptMethod==CRYPT_UNKNOWN)
    WrongVer=true;

  if (WrongVer)
  {
    ErrHandler.UnknownMethodMsg(Arc.FileName,ArcFileName);

    if (!Arc.BrokenHeader)
      uiMsg(UIERROR_NEWERRAR,Arc.FileName);
  }
  return !WrongVer;
}

#ifndef SFX_MODULE

void CmdExtract::AnalyzeArchive(const std::wstring &ArcName,bool Volume,bool NewNumbering)
{
  FreeAnalyzeData();

  wchar *ArgName=Cmd->FileArgs.GetString();
  Cmd->FileArgs.Rewind();
  if (ArgName!=NULL && (wcscmp(ArgName,L"*")==0 || wcscmp(ArgName,L"*.*")==0))
    return;

  std::wstring NextName;
  if (Volume)
    GetFirstVolIfFullSet(ArcName,NewNumbering,NextName);
  else
    NextName=ArcName;

  bool MatchFound=false;
  bool PrevMatched=false;
  bool OpenNext=false;

  bool FirstVolume=true;

  bool FirstFile=true;

  while (true)
  {
    Archive Arc(Cmd);
    if (!Arc.Open(NextName) || !Arc.IsArchive(false))
    {
      if (OpenNext)
      {

        Analyze.EndName.clear();
        Analyze.EndPos=0;
      }
      break;
    }

    OpenNext=false;
    while (Arc.ReadHeader()>0)
    {
      Wait();

      HEADER_TYPE HeaderType=Arc.GetHeaderType();
      if (HeaderType==HEAD_ENDARC)
      {
        OpenNext|=Arc.EndArcHead.NextVolume;
        break;
      }
      if (HeaderType==HEAD_FILE)
      {
        if ((Arc.Format==RARFMT14 || Arc.Format==RARFMT15) && Arc.FileHead.UnpVer<=15)
        {

          OpenNext=false;
          break;
        }

        if (!Arc.FileHead.SplitBefore)
        {
          if (!MatchFound && !Arc.FileHead.Solid && !Arc.FileHead.Dir &&
              Arc.FileHead.RedirType==FSREDIR_NONE && Arc.FileHead.Method!=0)
          {

            if (!FirstVolume)
              Analyze.StartName=NextName;

            if (!FirstFile)
              Analyze.StartPos=Arc.CurBlockPos;
          }

          if (Cmd->IsProcessFile(Arc.FileHead,NULL,MATCH_WILDSUBPATH,0,NULL)!=0)
          {
            MatchFound = true;
            PrevMatched = true;

            Analyze.EndPos=0;

            if (Arc.FileHead.RedirType==FSREDIR_FILECOPY)
            {
              bool AlreadyAdded=false;
              for (size_t I=0;I<RefList.size();I++)
                if (Arc.FileHead.RedirName==RefList[I].RefName)
                {

                  RefList[I].RefCount++;
                  AlreadyAdded=true;
                  break;
                }

              size_t MaxListSize=1000000;

              if (!AlreadyAdded && RefList.size()<MaxListSize)
              {
                ExtractRef Ref{};
                Ref.RefName=Arc.FileHead.RedirName;
                Ref.RefCount=1;
                RefList.push_back(Ref);
              }
            }
          }
          else
          {
            if (PrevMatched)
            {

              if (!FirstVolume)
                Analyze.EndName=NextName;
              Analyze.EndPos=Arc.CurBlockPos;
            }
            PrevMatched=false;
          }
        }

        FirstFile=false;
        if (Arc.FileHead.SplitAfter)
        {
          OpenNext=true;
          break;
        }
      }
      Arc.SeekToNext();
    }
    Arc.Close();

    if (Volume && OpenNext)
    {
      NextVolumeName(NextName,!Arc.NewNumbering);
      FirstVolume=false;

      FirstFile=false;
    }
    else
      break;
  }

  if (RefList.size()!=0)
    Analyze={};
}
#endif

#ifndef SFX_MODULE

void CmdExtract::GetFirstVolIfFullSet(const std::wstring &SrcName,bool NewNumbering,std::wstring &DestName)
{
  std::wstring FirstVolName;
  VolNameToFirstName(SrcName,FirstVolName,NewNumbering);
  std::wstring NextName=FirstVolName;
  std::wstring ResultName=SrcName;
  while (true)
  {
    if (SrcName==NextName)
    {
      ResultName=FirstVolName;
      break;
    }
    if (!FileExist(NextName))
      break;
    NextVolumeName(NextName,!NewNumbering);
  }
  DestName=ResultName;
}
#endif

bool CmdExtract::CheckWinLimit(Archive &Arc,std::wstring &ArcFileName)
{
  if (Arc.FileHead.WinSize<=Cmd->WinSizeLimit || Arc.FileHead.WinSize<=Cmd->WinSize)
    return true;
  if (uiDictLimit(Cmd,ArcFileName,Arc.FileHead.WinSize,Max(Cmd->WinSizeLimit,Cmd->WinSize)))
  {

    Cmd->WinSizeLimit=Arc.FileHead.WinSize;
  }
  else
  {
    ErrHandler.SetErrorCode(RARX_FATAL);
#ifdef RARDLL
    Cmd->DllError=ERAR_LARGE_DICT;
#endif
    Arc.SeekToNext();
    return false;
  }
  return true;
}

bool FileCreate(CommandData *Cmd,File *NewFile,std::wstring &Name,
                bool *UserReject,int64 FileSize,RarTime *FileTime,bool WriteOnly)
{
  if (UserReject!=NULL)
    *UserReject=false;
#ifdef _WIN_ALL
  bool ShortNameChanged=false;
#endif
  while (FileExist(Name))
  {
#if defined(_WIN_ALL)
    if (!ShortNameChanged)
    {

      ShortNameChanged=true;

      if (UpdateExistingShortName(Name))
        continue;
    }

    ShortNameChanged=false;
#endif
    UIASKREP_RESULT Choice=uiAskReplaceEx(Cmd,Name,FileSize,FileTime,(NewFile==NULL ? UIASKREP_F_NORENAME:0));

    if (Choice==UIASKREP_R_REPLACE)
      break;
    if (Choice==UIASKREP_R_SKIP)
    {
      if (UserReject!=NULL)
        *UserReject=true;
      return false;
    }
    if (Choice==UIASKREP_R_CANCEL)
      ErrHandler.Exit(RARX_USERBREAK);
  }

  uint FileMode=WriteOnly ? FMF_WRITE|FMF_SHAREREAD:FMF_UPDATE|FMF_SHAREREAD;
  if (NewFile!=NULL && NewFile->Create(Name,FileMode))
    return true;

  CreatePath(Name,true,Cmd->DisableNames);
  return NewFile!=NULL ? NewFile->Create(Name,FileMode):DelFile(Name);
}

#if defined(_WIN_ALL)

bool UpdateExistingShortName(const std::wstring &Name)
{
  DWORD Res=GetLongPathName(Name.c_str(),NULL,0);
  if (Res==0)
    return false;
  std::vector<wchar> LongPathBuf(Res);
  Res=GetLongPathName(Name.c_str(),LongPathBuf.data(),(DWORD)LongPathBuf.size());
  if (Res==0 || Res>=LongPathBuf.size())
    return false;
  Res=GetShortPathName(Name.c_str(),NULL,0);
  if (Res==0)
    return false;
  std::vector<wchar> ShortPathBuf(Res);
  Res=GetShortPathName(Name.c_str(),ShortPathBuf.data(),(DWORD)ShortPathBuf.size());
  if (Res==0 || Res>=ShortPathBuf.size())
    return false;
  std::wstring LongPathName=LongPathBuf.data();
  std::wstring ShortPathName=ShortPathBuf.data();

  std::wstring LongName=PointToName(LongPathName);
  std::wstring ShortName=PointToName(ShortPathName);

  if (ShortName.empty() || wcsicomp(LongName,ShortName)==0 ||
      wcsicomp(PointToName(Name),ShortName)!=0)
    return false;

  std::wstring NewName;
  for (uint I=0;I<10000 && NewName.empty();I+=123)
  {

    NewName=Name;

    SetName(NewName,std::wstring(L"rtmp") + std::to_wstring(I));

    if (FileExist(NewName))
      NewName.clear();
  }

  if (NewName.empty())
    return false;

  std::wstring FullName=Name;
  SetName(FullName,LongName);

  if (!MoveFile(FullName.c_str(),NewName.c_str()))
    return false;

  File KeepShortFile;
  bool Created=false;
  if (!FileExist(Name))
    Created=KeepShortFile.Create(Name,FMF_WRITE|FMF_SHAREREAD);

  MoveFile(NewName.c_str(),FullName.c_str());

  if (Created)
  {

    KeepShortFile.Close();
    KeepShortFile.Delete();
  }

  return true;
}
#endif

File::File()
{
  hFile=FILE_BAD_HANDLE;
  NewFile=false;
  LastWrite=false;
  HandleType=FILE_HANDLENORMAL;
  LineInput=false;
  SkipClose=false;
  ErrorType=FILE_SUCCESS;
  OpenShared=false;
  AllowDelete=true;
  AllowExceptions=true;
  PreserveAtime=false;
#ifdef _WIN_ALL
  CreateMode=FMF_UNDEFINED;
#endif
  ReadErrorMode=FREM_ASK;
  TruncatedAfterReadError=false;
  CurFilePos=0;
}

File::~File()
{
  if (hFile!=FILE_BAD_HANDLE && !SkipClose)
    if (NewFile)
      Delete();
    else
      Close();
}

void File::operator = (File &SrcFile)
{
  hFile=SrcFile.hFile;
  NewFile=SrcFile.NewFile;
  LastWrite=SrcFile.LastWrite;
  HandleType=SrcFile.HandleType;
  TruncatedAfterReadError=SrcFile.TruncatedAfterReadError;
  FileName=SrcFile.FileName;
  SrcFile.SkipClose=true;
}

bool File::Open(const std::wstring &Name,uint Mode)
{
  ErrorType=FILE_SUCCESS;
  FileHandle hNewFile;
  bool OpenShared=File::OpenShared || (Mode & FMF_OPENSHARED)!=0;
  bool UpdateMode=(Mode & FMF_UPDATE)!=0;
  bool WriteMode=(Mode & FMF_WRITE)!=0;
#ifdef _WIN_ALL
  uint Access=WriteMode ? GENERIC_WRITE:GENERIC_READ;
  if (UpdateMode)
    Access|=GENERIC_WRITE;
  uint ShareMode=(Mode & FMF_OPENEXCLUSIVE) ? 0 : FILE_SHARE_READ;
  if (OpenShared)
    ShareMode|=FILE_SHARE_WRITE;
  uint Flags=FILE_FLAG_SEQUENTIAL_SCAN;
  FindData FD;
  if (PreserveAtime)
    Access|=FILE_WRITE_ATTRIBUTES;
  hNewFile=CreateFile(Name.c_str(),Access,ShareMode,NULL,OPEN_EXISTING,Flags,NULL);

  DWORD LastError;
  if (hNewFile==FILE_BAD_HANDLE)
  {
    LastError=GetLastError();

    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
    {
      hNewFile=CreateFile(LongName.c_str(),Access,ShareMode,NULL,OPEN_EXISTING,Flags,NULL);

      if (GetLastError()==ERROR_FILE_NOT_FOUND)
        LastError=ERROR_FILE_NOT_FOUND;
    }
  }
  if (hNewFile==FILE_BAD_HANDLE && LastError==ERROR_FILE_NOT_FOUND)
    ErrorType=FILE_NOTFOUND;
  if (PreserveAtime && hNewFile!=FILE_BAD_HANDLE)
  {
    FILETIME ft={0xffffffff,0xffffffff};
    SetFileTime(hNewFile,NULL,&ft,NULL);
  }

#else
  int flags=UpdateMode ? O_RDWR:(WriteMode ? O_WRONLY:O_RDONLY);
#ifdef O_BINARY
  flags|=O_BINARY;
#if defined(_AIX) && defined(_LARGE_FILE_API)
  flags|=O_LARGEFILE;
#endif
#endif

#if defined(O_NOATIME)
  if (PreserveAtime)
    flags|=O_NOATIME;
#endif
  std::string NameA;
  WideToChar(Name,NameA);

  int handle=open(NameA.c_str(),flags);
#ifdef LOCK_EX

#ifdef _OSF_SOURCE
  extern "C" int flock(int, int);
#endif
  if (!OpenShared && UpdateMode && handle>=0 && flock(handle,LOCK_EX|LOCK_NB)==-1)
  {
    close(handle);
    return false;
  }

#endif
  if (handle==-1)
    hNewFile=FILE_BAD_HANDLE;
  else
  {
#ifdef FILE_USE_OPEN
    hNewFile=handle;
#else
    hNewFile=fdopen(handle,UpdateMode ? UPDATEBINARY:READBINARY);
#endif
  }
  if (hNewFile==FILE_BAD_HANDLE && errno==ENOENT)
    ErrorType=FILE_NOTFOUND;
#endif
  NewFile=false;
  HandleType=FILE_HANDLENORMAL;
  SkipClose=false;
  bool Success=hNewFile!=FILE_BAD_HANDLE;
  if (Success)
  {
    hFile=hNewFile;
    FileName=Name;
    TruncatedAfterReadError=false;
  }
  return Success;
}

#if !defined(SFX_MODULE)
void File::TOpen(const std::wstring &Name)
{
  if (!WOpen(Name))
    ErrHandler.Exit(RARX_OPEN);
}
#endif

bool File::WOpen(const std::wstring &Name)
{
  if (Open(Name))
    return true;
  ErrHandler.OpenErrorMsg(Name);
  return false;
}

bool File::Create(const std::wstring &Name,uint Mode)
{

  bool WriteMode=(Mode & FMF_WRITE)!=0;
  bool ShareRead=(Mode & FMF_SHAREREAD)!=0 || File::OpenShared;
#ifdef _WIN_ALL
  CreateMode=Mode;
  uint Access=WriteMode ? GENERIC_WRITE:GENERIC_READ|GENERIC_WRITE;
  DWORD ShareMode=ShareRead ? FILE_SHARE_READ:0;

  wchar LastChar=GetLastChar(Name);
  bool Special=LastChar=='.' || LastChar==' ';

  if (Special && (Mode & FMF_STANDARDNAMES)==0)
    hFile=FILE_BAD_HANDLE;
  else
    hFile=CreateFile(Name.c_str(),Access,ShareMode,NULL,CREATE_ALWAYS,0,NULL);

  if (hFile==FILE_BAD_HANDLE)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      hFile=CreateFile(LongName.c_str(),Access,ShareMode,NULL,CREATE_ALWAYS,0,NULL);
  }

#else
#ifdef FILE_USE_OPEN
  std::string NameA;
  WideToChar(Name,NameA);
  hFile=open(NameA.c_str(),(O_CREAT|O_TRUNC) | (WriteMode ? O_WRONLY : O_RDWR),0666);
#else
  hFile=fopen(NameA.c_str(),WriteMode ? WRITEBINARY:CREATEBINARY);
#endif
#endif
  NewFile=true;
  HandleType=FILE_HANDLENORMAL;
  SkipClose=false;
  FileName=Name;
  return hFile!=FILE_BAD_HANDLE;
}

#if !defined(SFX_MODULE)
void File::TCreate(const std::wstring &Name,uint Mode)
{
  if (!WCreate(Name,Mode))
    ErrHandler.Exit(RARX_FATAL);
}
#endif

bool File::WCreate(const std::wstring &Name,uint Mode)
{
  if (Create(Name,Mode))
    return true;
  ErrHandler.CreateErrorMsg(Name);
  return false;
}

bool File::Close()
{
  bool Success=true;

  if (hFile!=FILE_BAD_HANDLE)
  {
    if (!SkipClose)
    {
#ifdef _WIN_ALL

      if (HandleType==FILE_HANDLENORMAL)
        Success=CloseHandle(hFile)!=FALSE;
#else
#ifdef FILE_USE_OPEN
      Success=close(hFile)!=-1;
#else
      Success=fclose(hFile)!=EOF;
#endif
#endif
    }
    hFile=FILE_BAD_HANDLE;
  }
  HandleType=FILE_HANDLENORMAL;
  if (!Success && AllowExceptions)
    ErrHandler.CloseError(FileName);
  return Success;
}

bool File::Delete()
{
  if (HandleType!=FILE_HANDLENORMAL)
    return false;
  if (hFile!=FILE_BAD_HANDLE)
    Close();
  if (!AllowDelete)
    return false;
  return DelFile(FileName);
}

bool File::Rename(const std::wstring &NewName)
{

  bool Success=(NewName==FileName);

  if (!Success)
    Success=RenameFile(FileName,NewName);

  if (Success)
    FileName=NewName;

  return Success;
}

bool File::Write(const void *Data,size_t Size)
{
  if (Size==0)
    return true;
  if (HandleType==FILE_HANDLESTD)
  {
#ifdef _WIN_ALL
    hFile=GetStdHandle(STD_OUTPUT_HANDLE);
#else

    if (hFile==FILE_BAD_HANDLE)
    {
#ifdef FILE_USE_OPEN
      hFile=dup(STDOUT_FILENO);
#else
      hFile=fdopen(dup(STDOUT_FILENO),"w");
#endif
    }
#endif
  }
  bool Success;
  while (1)
  {
    Success=false;
#ifdef _WIN_ALL
    DWORD Written=0;
    if (HandleType!=FILE_HANDLENORMAL)
    {

      const size_t MaxSize=0x4000;
      for (size_t I=0;I<Size;I+=MaxSize)
      {
        Success=WriteFile(hFile,(byte *)Data+I,(DWORD)Min(Size-I,MaxSize),&Written,NULL)!=FALSE;
        if (!Success)
          break;
      }
    }
    else
      Success=WriteFile(hFile,Data,(DWORD)Size,&Written,NULL)!=FALSE;
#else
#ifdef FILE_USE_OPEN
    ssize_t Written=write(hFile,Data,Size);
    Success=Written==Size;
#else
    int Written=fwrite(Data,1,Size,hFile);
    Success=Written==Size && !ferror(hFile);
#endif
#endif
    if (!Success && AllowExceptions && HandleType==FILE_HANDLENORMAL)
    {
#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(RARDLL)
      int ErrCode=GetLastError();
      int64 FilePos=Tell();
      uint64 FreeSize=GetFreeDisk(FileName);
      SetLastError(ErrCode);
      if (FreeSize>Size && FilePos-Size<=0xffffffff && FilePos+Size>0xffffffff)
        ErrHandler.WriteErrorFAT(FileName);
#endif
      if (ErrHandler.AskRepeatWrite(FileName,false))
      {
#if !defined(_WIN_ALL) && !defined(FILE_USE_OPEN)
        clearerr(hFile);
#endif
        if (Written<Size && Written>0)
          Seek(Tell()-Written,SEEK_SET);
        continue;
      }
      ErrHandler.WriteError(L"",FileName);
    }
    break;
  }
  LastWrite=true;
  return Success;
}

int File::Read(void *Data,size_t Size)
{
  if (TruncatedAfterReadError)
    return 0;

  int64 FilePos=0;

  if (ReadErrorMode==FREM_IGNORE)
    FilePos=Tell();
  int TotalRead=0;
  while (true)
  {
    int ReadSize=DirectRead(Data,Size);

    if (ReadSize==-1)
    {
      ErrorType=FILE_READERROR;
      if (AllowExceptions)
        if (ReadErrorMode==FREM_IGNORE)
        {
          ReadSize=0;
          for (size_t I=0;I<Size;I+=512)
          {
            Seek(FilePos+I,SEEK_SET);
            size_t SizeToRead=Min(Size-I,512);
            int ReadCode=DirectRead(Data,SizeToRead);
            ReadSize+=(ReadCode==-1) ? 512:ReadCode;
            if (ReadSize!=-1)
              TotalRead+=ReadSize;
          }
        }
        else
        {
          bool Ignore=false,Retry=false,Quit=false;
          if (ReadErrorMode==FREM_ASK && HandleType==FILE_HANDLENORMAL && IsOpened())
          {
            ErrHandler.AskRepeatRead(FileName,Ignore,Retry,Quit);
            if (Retry)
              continue;
          }
          if (Ignore || ReadErrorMode==FREM_TRUNCATE)
          {
            TruncatedAfterReadError=true;
            return 0;
          }
          ErrHandler.ReadError(FileName);
        }
    }
    TotalRead+=ReadSize;

    if (HandleType==FILE_HANDLESTD && !LineInput && ReadSize>0 && (uint)ReadSize<Size)
    {

      Data=(byte*)Data+ReadSize;
      Size-=ReadSize;
      continue;
    }
    break;
  }
  if (TotalRead>0)
    CurFilePos+=TotalRead;
  return TotalRead;
}

int File::DirectRead(void *Data,size_t Size)
{
#ifdef _WIN_ALL
  const size_t MaxDeviceRead=20000;
  const size_t MaxLockedRead=32768;
#endif
  if (HandleType==FILE_HANDLESTD)
  {
#ifdef _WIN_ALL

    hFile=GetStdHandle(STD_INPUT_HANDLE);
#else
#ifdef FILE_USE_OPEN
    hFile=STDIN_FILENO;
#else
    hFile=stdin;
#endif
#endif
  }
#ifdef _WIN_ALL

  DWORD Read;
  if (!ReadFile(hFile,Data,(DWORD)Size,&Read,NULL))
  {
    if (IsDevice() && Size>MaxDeviceRead)
      return DirectRead(Data,MaxDeviceRead);
    if (HandleType==FILE_HANDLESTD && GetLastError()==ERROR_BROKEN_PIPE)
      return 0;

    if (HandleType==FILE_HANDLENORMAL && Size>MaxLockedRead &&
        GetLastError()==ERROR_LOCK_VIOLATION)
      return DirectRead(Data,MaxLockedRead);

    return -1;
  }
  return Read;
#else
#ifdef FILE_USE_OPEN
  ssize_t ReadSize=read(hFile,Data,Size);
  if (ReadSize==-1)
    return -1;
  return (int)ReadSize;
#else
  if (LastWrite)
  {
    fflush(hFile);
    LastWrite=false;
  }
  clearerr(hFile);
  size_t ReadSize=fread(Data,1,Size,hFile);
  if (ferror(hFile))
    return -1;
  return (int)ReadSize;
#endif
#endif
}

void File::Seek(int64 Offset,int Method)
{
  if (!RawSeek(Offset,Method) && AllowExceptions)
    ErrHandler.SeekError(FileName);
}

bool File::RawSeek(int64 Offset,int Method)
{
  if (hFile==FILE_BAD_HANDLE)
    return true;
  if (!IsSeekable())
  {

    byte Buf[4096];
    if (Method==SEEK_CUR || Method==SEEK_SET && Offset>=CurFilePos)
    {
      uint64 SkipSize=Method==SEEK_CUR ? Offset:Offset-CurFilePos;
      while (SkipSize>0)
      {
        int ReadSize=Read(Buf,(size_t)Min(SkipSize,ASIZE(Buf)));
        if (ReadSize<=0)
          return false;
        SkipSize-=ReadSize;
        CurFilePos+=ReadSize;
      }
      return true;
    }

    if (Method==SEEK_END)
    {
      int ReadSize;
      while ((ReadSize=Read(Buf,ASIZE(Buf)))>0)
        CurFilePos+=ReadSize;
      return true;
    }

    return false;
  }
  if (Offset<0 && Method!=SEEK_SET)
  {
    Offset=(Method==SEEK_CUR ? Tell():FileLength())+Offset;
    Method=SEEK_SET;
  }
#ifdef _WIN_ALL
  LONG HighDist=(LONG)(Offset>>32);
  if (SetFilePointer(hFile,(LONG)Offset,&HighDist,Method)==0xffffffff &&
      GetLastError()!=NO_ERROR)
    return false;
#else
  LastWrite=false;
#ifdef FILE_USE_OPEN
  if (lseek(hFile,(off_t)Offset,Method)==-1)
    return false;
#elif defined(_LARGEFILE_SOURCE) && !defined(_OSF_SOURCE) && !defined(__VMS)
  if (fseeko(hFile,Offset,Method)!=0)
    return false;
#else
  if (fseek(hFile,(long)Offset,Method)!=0)
    return false;
#endif
#endif
  return true;
}

int64 File::Tell()
{
  if (hFile==FILE_BAD_HANDLE)
    if (AllowExceptions)
      ErrHandler.SeekError(FileName);
    else
      return -1;
  if (!IsSeekable())
    return CurFilePos;
#ifdef _WIN_ALL
  LONG HighDist=0;
  uint LowDist=SetFilePointer(hFile,0,&HighDist,FILE_CURRENT);
  if (LowDist==0xffffffff && GetLastError()!=NO_ERROR)
    if (AllowExceptions)
      ErrHandler.SeekError(FileName);
    else
      return -1;
  return INT32TO64(HighDist,LowDist);
#else
#ifdef FILE_USE_OPEN
  return lseek(hFile,0,SEEK_CUR);
#elif defined(_LARGEFILE_SOURCE) && !defined(_OSF_SOURCE)
  return ftello(hFile);
#else
  return ftell(hFile);
#endif
#endif
}

void File::Prealloc(int64 Size)
{
#ifdef _WIN_ALL
  if (RawSeek(Size,SEEK_SET))
  {
    Truncate();
    Seek(0,SEEK_SET);
  }
#endif

#if defined(_UNIX) && defined(USE_FALLOCATE)

  int fd = GetFD();
  if (fd >= 0)
    fallocate(fd, 0, 0, Size);
#endif
}

byte File::GetByte()
{
  byte Byte=0;
  Read(&Byte,1);
  return Byte;
}

void File::PutByte(byte Byte)
{
  Write(&Byte,1);
}

bool File::Truncate()
{
#ifdef _WIN_ALL
  return SetEndOfFile(hFile)!=FALSE;
#else
  return ftruncate(GetFD(),(off_t)Tell())==0;
#endif
}

void File::Flush()
{
#ifdef _WIN_ALL
  FlushFileBuffers(hFile);
#else
#ifndef FILE_USE_OPEN
  fflush(hFile);
#endif
  fsync(GetFD());
#endif
}

void File::SetOpenFileTime(RarTime *ftm,RarTime *ftc,RarTime *fta)
{
#ifdef _WIN_ALL

  if (CreateMode!=FMF_UNDEFINED && (CreateMode & FMF_WRITE)==0)
    FlushFileBuffers(hFile);

  bool sm=ftm!=NULL && ftm->IsSet();
  bool sc=ftc!=NULL && ftc->IsSet();
  bool sa=fta!=NULL && fta->IsSet();
  FILETIME fm,fc,fa;
  if (sm)
    ftm->GetWinFT(&fm);
  if (sc)
    ftc->GetWinFT(&fc);
  if (sa)
    fta->GetWinFT(&fa);
  SetFileTime(hFile,sc ? &fc:NULL,sa ? &fa:NULL,sm ? &fm:NULL);
#endif
}

void File::SetCloseFileTime(RarTime *ftm,RarTime *fta)
{

#ifdef _UNIX
  SetCloseFileTimeByName(FileName,ftm,fta);
#endif
}

void File::SetCloseFileTimeByName(const std::wstring &Name,RarTime *ftm,RarTime *fta)
{
#ifdef _UNIX
  bool setm=ftm!=NULL && ftm->IsSet();
  bool seta=fta!=NULL && fta->IsSet();
  if (setm || seta)
  {
    std::string NameA;
    WideToChar(Name,NameA);

#ifdef UNIX_TIME_NS
    timespec times[2];
    times[0].tv_sec=seta ? fta->GetUnix() : 0;
    times[0].tv_nsec=seta ? long(fta->GetUnixNS()%1000000000) : UTIME_NOW;
    times[1].tv_sec=setm ? ftm->GetUnix() : 0;
    times[1].tv_nsec=setm ? long(ftm->GetUnixNS()%1000000000) : UTIME_NOW;
    utimensat(AT_FDCWD,NameA.c_str(),times,0);
#else
    utimbuf ut;
    if (setm)
      ut.modtime=ftm->GetUnix();
    else
      ut.modtime=fta->GetUnix();
    if (seta)
      ut.actime=fta->GetUnix();
    else
      ut.actime=ut.modtime;
    utime(NameA.c_str(),&ut);
#endif
  }
#endif
}

#ifdef _UNIX
void File::StatToRarTime(struct stat &st,RarTime *ftm,RarTime *ftc,RarTime *fta)
{
#ifdef UNIX_TIME_NS
#if defined(_APPLE)
  if (ftm!=NULL) ftm->SetUnixNS(st.st_mtimespec.tv_sec*(uint64)1000000000+st.st_mtimespec.tv_nsec);
  if (ftc!=NULL) ftc->SetUnixNS(st.st_ctimespec.tv_sec*(uint64)1000000000+st.st_ctimespec.tv_nsec);
  if (fta!=NULL) fta->SetUnixNS(st.st_atimespec.tv_sec*(uint64)1000000000+st.st_atimespec.tv_nsec);
#else
  if (ftm!=NULL) ftm->SetUnixNS(st.st_mtim.tv_sec*(uint64)1000000000+st.st_mtim.tv_nsec);
  if (ftc!=NULL) ftc->SetUnixNS(st.st_ctim.tv_sec*(uint64)1000000000+st.st_ctim.tv_nsec);
  if (fta!=NULL) fta->SetUnixNS(st.st_atim.tv_sec*(uint64)1000000000+st.st_atim.tv_nsec);
#endif
#else
  if (ftm!=NULL) ftm->SetUnix(st.st_mtime);
  if (ftc!=NULL) ftc->SetUnix(st.st_ctime);
  if (fta!=NULL) fta->SetUnix(st.st_atime);
#endif
}
#endif

void File::GetOpenFileTime(RarTime *ftm,RarTime *ftc,RarTime *fta)
{
#ifdef _WIN_ALL
  FILETIME ctime,atime,mtime;
  GetFileTime(hFile,&ctime,&atime,&mtime);
  if (ftm!=NULL) ftm->SetWinFT(&mtime);
  if (ftc!=NULL) ftc->SetWinFT(&ctime);
  if (fta!=NULL) fta->SetWinFT(&atime);
#elif defined(_UNIX)
  struct stat st;
  fstat(GetFD(),&st);
  StatToRarTime(st,ftm,ftc,fta);
#endif
}

int64 File::FileLength()
{
  int64 SavePos=Tell();
  Seek(0,SEEK_END);
  int64 Length=Tell();
  Seek(SavePos,SEEK_SET);
  return Length;
}

bool File::IsDevice()
{
  if (hFile==FILE_BAD_HANDLE)
    return false;
#ifdef _WIN_ALL
  uint Type=GetFileType(hFile);
  return Type==FILE_TYPE_CHAR || Type==FILE_TYPE_PIPE;
#else
  return isatty(GetFD());
#endif
}

#ifndef SFX_MODULE
int64 File::Copy(File &Dest,int64 Length)
{
  bool CopyAll=(Length==INT64NDF);

  size_t BufSize=File::CopyBufferSize();
  if (!CopyAll && Length<(int64)BufSize)
    BufSize=(size_t)Length;

  std::vector<byte> Buffer(BufSize);
  int64 CopySize=0;

  while (CopyAll || Length>0)
  {
    Wait();
    size_t SizeToRead=(!CopyAll && Length<(int64)Buffer.size()) ? (size_t)Length:Buffer.size();
    byte *Buf=Buffer.data();
    int ReadSize=Read(Buf,SizeToRead);
    if (ReadSize==0)
      break;
    size_t WriteSize=ReadSize;
#ifdef _WIN_ALL

    if (CopySize==0 && WriteSize>=4096)
    {
      const size_t FirstWrite=1024;
      Dest.Write(Buf,FirstWrite);
      Buf+=FirstWrite;
      WriteSize-=FirstWrite;
    }
#endif
    Dest.Write(Buf,WriteSize);
    CopySize+=ReadSize;
    if (!CopyAll)
      Length-=ReadSize;
  }
  return CopySize;
}
#endif

MKDIR_CODE MakeDir(const std::wstring &Name,bool SetAttr,uint Attr)
{
#ifdef _WIN_ALL

  wchar LastChar=GetLastChar(Name);
  bool Special=LastChar=='.' || LastChar==' ';
  BOOL RetCode=Special ? FALSE : CreateDirectory(Name.c_str(),NULL);
  if (RetCode==0 && !FileExist(Name))
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      RetCode=CreateDirectory(LongName.c_str(),NULL);
  }
  if (RetCode!=0)
  {
    if (SetAttr)
      SetFileAttr(Name,Attr);
    return MKDIR_SUCCESS;
  }
  int ErrCode=GetLastError();
  if (ErrCode==ERROR_FILE_NOT_FOUND || ErrCode==ERROR_PATH_NOT_FOUND)
    return MKDIR_BADPATH;
  return MKDIR_ERROR;
#elif defined(_UNIX)
  std::string NameA;
  WideToChar(Name,NameA);
  mode_t uattr=SetAttr ? (mode_t)Attr:0777;
  int ErrCode=mkdir(NameA.c_str(),uattr);
  if (ErrCode==-1)
    return errno==ENOENT ? MKDIR_BADPATH:MKDIR_ERROR;
  return MKDIR_SUCCESS;
#else
  return MKDIR_ERROR;
#endif
}

bool CreateDir(const std::wstring &Name)
{
  return MakeDir(Name,false,0)==MKDIR_SUCCESS;
}

bool CreatePath(const std::wstring &Path,bool SkipLastName,bool Silent)
{
  if (Path.empty())
    return false;

#ifdef _WIN_ALL
  uint DirAttr=0;
#else
  uint DirAttr=0777;
#endif

  bool Success=true;

  for (size_t I=0;I<Path.size();I++)
  {

    if (IsPathDiv(Path[I]) && I>0)
    {
#ifdef _WIN_ALL

      if (I==2 && Path[1]==':')
        continue;
#endif
      std::wstring DirName=Path.substr(0,I);
      Success=MakeDir(DirName,true,DirAttr)==MKDIR_SUCCESS;
      if (Success && !Silent)
      {
        mprintf(St(MCreatDir),DirName.c_str());
        mprintf(L" %s",St(MOk));
      }
    }
  }
  if (!SkipLastName && !IsPathDiv(GetLastChar(Path)))
    Success=MakeDir(Path,true,DirAttr)==MKDIR_SUCCESS;
  return Success;
}

void SetDirTime(const std::wstring &Name,RarTime *ftm,RarTime *ftc,RarTime *fta)
{
#if defined(_WIN_ALL)
  bool sm=ftm!=NULL && ftm->IsSet();
  bool sc=ftc!=NULL && ftc->IsSet();
  bool sa=fta!=NULL && fta->IsSet();

  uint DirAttr=GetFileAttr(Name);
  bool ResetAttr=(DirAttr!=0xffffffff && (DirAttr & FILE_ATTRIBUTE_READONLY)!=0);
  if (ResetAttr)
    SetFileAttr(Name,0);

  HANDLE hFile=CreateFile(Name.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                          NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      hFile=CreateFile(LongName.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                       NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
  }

  if (hFile==INVALID_HANDLE_VALUE)
    return;
  FILETIME fm,fc,fa;
  if (sm)
    ftm->GetWinFT(&fm);
  if (sc)
    ftc->GetWinFT(&fc);
  if (sa)
    fta->GetWinFT(&fa);
  SetFileTime(hFile,sc ? &fc:NULL,sa ? &fa:NULL,sm ? &fm:NULL);
  CloseHandle(hFile);
  if (ResetAttr)
    SetFileAttr(Name,DirAttr);
#endif
#ifdef _UNIX
  File::SetCloseFileTimeByName(Name,ftm,fta);
#endif
}

bool IsRemovable(const std::wstring &Name)
{
#if defined(_WIN_ALL)
  std::wstring Root;
  GetPathRoot(Name,Root);
  int Type=GetDriveType(Root.empty() ? nullptr : Root.c_str());
  return Type==DRIVE_REMOVABLE || Type==DRIVE_CDROM;
#else
  return false;
#endif
}

#ifndef SFX_MODULE
int64 GetFreeDisk(const std::wstring &Name)
{
#ifdef _WIN_ALL
  std::wstring Root;
  GetPathWithSep(Name,Root);

  ULARGE_INTEGER uiTotalSize,uiTotalFree,uiUserFree;
  uiUserFree.u.LowPart=uiUserFree.u.HighPart=0;
  if (GetDiskFreeSpaceEx(Root.empty() ? NULL:Root.c_str(),&uiUserFree,&uiTotalSize,&uiTotalFree) &&
      uiUserFree.u.HighPart<=uiTotalFree.u.HighPart)
    return INT32TO64(uiUserFree.u.HighPart,uiUserFree.u.LowPart);
  return 0;
#elif defined(_UNIX)
  std::wstring Root;
  GetPathWithSep(Name,Root);
  std::string RootA;
  WideToChar(Root,RootA);
  struct statvfs sfs;
  if (statvfs(RootA.empty() ? ".":RootA.c_str(),&sfs)!=0)
    return 0;
  int64 FreeSize=sfs.f_bsize;
  FreeSize=FreeSize*sfs.f_bavail;
  return FreeSize;
#else
  return 0;
#endif
}
#endif

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)

bool IsFAT(const std::wstring &Name)
{
  std::wstring Root;
  GetPathRoot(Name,Root);
  wchar FileSystem[MAX_PATH+1];

  if (GetVolumeInformation(Root.empty() ? NULL:Root.c_str(),NULL,0,NULL,NULL,NULL,FileSystem,ASIZE(FileSystem)))
    return wcscmp(FileSystem,L"FAT")==0 || wcscmp(FileSystem,L"FAT32")==0;
  return false;
}
#endif

bool FileExist(const std::wstring &Name)
{
#ifdef _WIN_ALL
  return GetFileAttr(Name)!=0xffffffff;
#elif defined(ENABLE_ACCESS)
  std::string NameA;
  WideToChar(Name,NameA);
  return access(NameA.c_str(),0)==0;
#else
  FindData FD;
  return FindFile::FastFind(Name,&FD);
#endif
}

bool WildFileExist(const std::wstring &Name)
{
  if (IsWildcard(Name))
  {
    FindFile Find;
    Find.SetMask(Name);
    FindData fd;
    return Find.Next(&fd);
  }
  return FileExist(Name);
}

bool IsDir(uint Attr)
{
#ifdef _WIN_ALL
  return Attr!=0xffffffff && (Attr & FILE_ATTRIBUTE_DIRECTORY)!=0;
#endif
#if defined(_UNIX)
  return (Attr & 0xF000)==0x4000;
#endif
}

bool IsUnreadable(uint Attr)
{
#if defined(_UNIX) && defined(S_ISFIFO) && defined(S_ISSOCK) && defined(S_ISCHR)
  return S_ISFIFO(Attr) || S_ISSOCK(Attr) || S_ISCHR(Attr);
#else
  return false;
#endif
}

bool IsLink(uint Attr)
{
#ifdef _UNIX
  return (Attr & 0xF000)==0xA000;
#elif defined(_WIN_ALL)
  return (Attr & FILE_ATTRIBUTE_REPARSE_POINT)!=0;
#else
  return false;
#endif
}

bool IsDeleteAllowed(uint FileAttr)
{
#ifdef _WIN_ALL
  return (FileAttr & (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN))==0;
#else
  return (FileAttr & (S_IRUSR|S_IWUSR))==(S_IRUSR|S_IWUSR);
#endif
}

void PrepareToDelete(const std::wstring &Name)
{
#ifdef _WIN_ALL
  SetFileAttr(Name,0);
#endif
#ifdef _UNIX
  std::string NameA;
  WideToChar(Name,NameA);
  chmod(NameA.c_str(),S_IRUSR|S_IWUSR|S_IXUSR);
#endif
}

uint GetFileAttr(const std::wstring &Name)
{
#ifdef _WIN_ALL
  DWORD Attr=GetFileAttributes(Name.c_str());
  if (Attr==0xffffffff)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Attr=GetFileAttributes(LongName.c_str());
  }
  return Attr;
#else
  std::string NameA;
  WideToChar(Name,NameA);
  struct stat st;
  if (stat(NameA.c_str(),&st)!=0)
    return 0;
  return st.st_mode;
#endif
}

bool SetFileAttr(const std::wstring &Name,uint Attr)
{
#ifdef _WIN_ALL
  bool Success=SetFileAttributes(Name.c_str(),Attr)!=0;
  if (!Success)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Success=SetFileAttributes(LongName.c_str(),Attr)!=0;
  }
  return Success;
#elif defined(_UNIX)
  std::string NameA;
  WideToChar(Name,NameA);
  return chmod(NameA.c_str(),(mode_t)Attr)==0;
#else
  return false;
#endif
}

bool MkTemp(std::wstring &Name,const wchar *Ext)
{
  RarTime CurTime;
  CurTime.SetCurrentTime();

  uint Random=(uint)(CurTime.GetWin()/100000);

  uint PID=0;
#ifdef _WIN_ALL
  PID=(uint)GetCurrentProcessId();
#elif defined(_UNIX)
  PID=(uint)getpid();
#endif

  for (uint Attempt=0;;Attempt++)
  {
    uint RandomExt=Random%50000+Attempt;
    if (Attempt==1000)
      return false;

    if (Ext==nullptr)
      Ext=L".rartemp";

    std::wstring NewName=Name + std::to_wstring(PID) + L"." + std::to_wstring(RandomExt) + Ext;
    if (!FileExist(NewName))
    {
      Name=NewName;
      break;
    }
  }
  return true;
}

#if !defined(SFX_MODULE)
void CalcFileSum(File *SrcFile,uint *CRC32,byte *Blake2,uint Threads,int64 Size,uint Flags)
{
  int64 SavePos=SrcFile->Tell();
#ifndef SILENT
  int64 FileLength=Size==INT64NDF ? SrcFile->FileLength() : Size;
#endif

  if ((Flags & (CALCFSUM_SHOWTEXT|CALCFSUM_SHOWPERCENT))!=0)
    uiMsg(UIEVENT_FILESUMSTART);

  if ((Flags & CALCFSUM_CURPOS)==0)
    SrcFile->Seek(0,SEEK_SET);

  const size_t BufSize=0x100000;
  std::vector<byte> Data(BufSize);

  DataHash HashCRC,HashBlake2;
  HashCRC.Init(HASH_CRC32,Threads);
  HashBlake2.Init(HASH_BLAKE2,Threads);

  int64 BlockCount=0;
  int64 TotalRead=0;
  while (true)
  {
    size_t SizeToRead;
    if (Size==INT64NDF)
      SizeToRead=BufSize;
    else
      SizeToRead=(size_t)Min((int64)BufSize,Size);
    int ReadSize=SrcFile->Read(Data.data(),SizeToRead);
    if (ReadSize==0)
      break;
    TotalRead+=ReadSize;

    if ((++BlockCount & 0xf)==0)
    {
#ifndef SILENT
      if ((Flags & CALCFSUM_SHOWPROGRESS)!=0)
      {

        uiExtractProgress(TotalRead,FileLength,0,0);
      }
      else
      {
        if ((Flags & CALCFSUM_SHOWPERCENT)!=0)
          uiMsg(UIEVENT_FILESUMPROGRESS,ToPercent(TotalRead,FileLength));
      }
#endif
      Wait();
    }

    if (CRC32!=NULL)
      HashCRC.Update(Data.data(),ReadSize);
    if (Blake2!=NULL)
      HashBlake2.Update(Data.data(),ReadSize);

    if (Size!=INT64NDF)
      Size-=ReadSize;
  }
  SrcFile->Seek(SavePos,SEEK_SET);

  if ((Flags & CALCFSUM_SHOWPERCENT)!=0)
    uiMsg(UIEVENT_FILESUMEND);

  if (CRC32!=NULL)
    *CRC32=HashCRC.GetCRC32();
  if (Blake2!=NULL)
  {
    HashValue Result;
    HashBlake2.Result(&Result);
    memcpy(Blake2,Result.Digest,sizeof(Result.Digest));
  }
}
#endif

bool RenameFile(const std::wstring &SrcName,const std::wstring &DestName)
{
#ifdef _WIN_ALL
  bool Success=MoveFile(SrcName.c_str(),DestName.c_str())!=0;
  if (!Success)
  {
    std::wstring LongName1,LongName2;
    if (GetWinLongPath(SrcName,LongName1) && GetWinLongPath(DestName,LongName2))
      Success=MoveFile(LongName1.c_str(),LongName2.c_str())!=0;
  }
  return Success;
#else
  std::string SrcNameA,DestNameA;
  WideToChar(SrcName,SrcNameA);
  WideToChar(DestName,DestNameA);
  bool Success=rename(SrcNameA.c_str(),DestNameA.c_str())==0;
  return Success;
#endif
}

bool DelFile(const std::wstring &Name)
{
#ifdef _WIN_ALL
  bool Success=DeleteFile(Name.c_str())!=0;
  if (!Success)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Success=DeleteFile(LongName.c_str())!=0;
  }
  return Success;
#else
  std::string NameA;
  WideToChar(Name,NameA);
  bool Success=remove(NameA.c_str())==0;
  return Success;
#endif
}

bool DelDir(const std::wstring &Name)
{
#ifdef _WIN_ALL
  bool Success=RemoveDirectory(Name.c_str())!=0;
  if (!Success)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Success=RemoveDirectory(LongName.c_str())!=0;
  }
  return Success;
#else
  std::string NameA;
  WideToChar(Name,NameA);
  bool Success=rmdir(NameA.c_str())==0;
  return Success;
#endif
}

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool SetFileCompression(const std::wstring &Name,bool State)
{
  HANDLE hFile=CreateFile(Name.c_str(),FILE_READ_DATA|FILE_WRITE_DATA,
                 FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
                 FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      hFile=CreateFile(LongName.c_str(),FILE_READ_DATA|FILE_WRITE_DATA,
                 FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
                 FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if (hFile==INVALID_HANDLE_VALUE)
      return false;
  }
  bool Success=SetFileCompression(hFile,State);
  CloseHandle(hFile);
  return Success;
}

bool SetFileCompression(HANDLE hFile,bool State)
{
  SHORT NewState=State ? COMPRESSION_FORMAT_DEFAULT:COMPRESSION_FORMAT_NONE;
  DWORD Result;
  int RetCode=DeviceIoControl(hFile,FSCTL_SET_COMPRESSION,&NewState,
                              sizeof(NewState),NULL,0,&Result,NULL);
  return RetCode!=0;
}

void ResetFileCache(const std::wstring &Name)
{

  HANDLE hSrc=CreateFile(Name.c_str(),GENERIC_READ,
                         FILE_SHARE_READ|FILE_SHARE_WRITE,
                         NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
  if (hSrc!=INVALID_HANDLE_VALUE)
    CloseHandle(hSrc);
}
#endif

bool LinksToDirs(const std::wstring &SrcName,const std::wstring &SkipPart,std::wstring &LastChecked)
{

  std::wstring Path=SrcName;

  size_t SkipLength=SkipPart.size();

  if (SkipLength>0 && Path.rfind(SkipPart,0)!=0)
    SkipLength=0;

  for (size_t I=0;I<Path.size() && I<LastChecked.size() && Path[I]==LastChecked[I];I++)
    if (IsPathDiv(Path[I]) && I>SkipLength)
      SkipLength=I;

  while (SkipLength<Path.size() && IsPathDiv(Path[SkipLength]))
    SkipLength++;

  if (Path.size()>0)
    for (size_t I=Path.size()-1;I>SkipLength;I--)
      if (IsPathDiv(Path[I]))
      {
        Path.erase(I);
        FindData FD;
        if (FindFile::FastFind(Path,&FD,true) && FD.IsLink)
        {
#ifdef _WIN_ALL

          if (!DelDir(Path))
#else
          if (!DelFile(Path))
#endif
          {
            ErrHandler.CreateErrorMsg(SrcName);
            return false;
          }
        }
      }
  LastChecked=SrcName;

  return true;
}

bool ReadTextFile(
  const std::wstring &Name,
  StringList *List,
  bool Config,
  bool AbortOnError,
  RAR_CHARSET SrcCharset,
  bool Unquote,
  bool SkipComments,
  bool ExpandEnvStr)
{
  std::wstring FileName;

  if (Config)
    GetConfigName(Name,FileName,true,false);
  else
    FileName=Name;

  File SrcFile;
  if (!FileName.empty())
  {
    bool OpenCode=AbortOnError ? SrcFile.WOpen(FileName):SrcFile.Open(FileName,0);

    if (!OpenCode)
    {
      if (AbortOnError)
        ErrHandler.Exit(RARX_OPEN);
      return false;
    }
  }
  else
    SrcFile.SetHandleType(FILE_HANDLESTD);

  size_t DataSize=0,ReadSize;
  const int ReadBlock=4096;

  std::vector<byte> Data(ReadBlock);
  while ((ReadSize=SrcFile.Read(&Data[DataSize],ReadBlock))!=0)
  {
    DataSize+=ReadSize;
    Data.resize(DataSize+ReadBlock);
  }

  Data.resize(DataSize);

  int LittleEndian=DataSize>=2 && Data[0]==255 && Data[1]==254 ? 1:0;
  int BigEndian=DataSize>=2 && Data[0]==254 && Data[1]==255 ? 1:0;
  bool Utf8=DataSize>=3 && Data[0]==0xef && Data[1]==0xbb && Data[2]==0xbf;

  if (SrcCharset==RCH_DEFAULT)
    SrcCharset=DetectTextEncoding(Data.data(),DataSize);

  std::vector<wchar> DataW(ReadBlock);

  if (SrcCharset==RCH_DEFAULT || SrcCharset==RCH_OEM || SrcCharset==RCH_ANSI)
  {
    Data.push_back(0);
#if defined(_WIN_ALL)
    if (SrcCharset==RCH_OEM)
      OemToCharA((char *)Data.data(),(char *)Data.data());
#endif
    DataW.resize(Data.size());
    CharToWide((char *)Data.data(),DataW.data(),DataW.size());
  }

  if (SrcCharset==RCH_UNICODE)
  {
    size_t Start=2;
    if (!LittleEndian && !BigEndian)
    {
      Start=0;
      LittleEndian=1;
    }

    DataW.resize(Data.size()/2+1);
    size_t End=Data.size() & ~1;
    for (size_t I=Start;I<End;I+=2)
      DataW[(I-Start)/2]=Data[I+BigEndian]+Data[I+LittleEndian]*256;
    DataW[(End-Start)/2]=0;
  }

  if (SrcCharset==RCH_UTF8)
  {
    Data.push_back(0);
    DataW.resize(Data.size());
    UtfToWide((const char *)(Data.data()+(Utf8 ? 3:0)),DataW.data(),DataW.size());
  }

  wchar *CurStr=DataW.data();

  while (*CurStr!=0)
  {
    wchar *NextStr=CurStr,*CmtPtr=NULL;
    while (*NextStr!='\r' && *NextStr!='\n' && *NextStr!=0)
    {
      if (SkipComments && NextStr[0]=='/' && NextStr[1]=='/')
      {
        *NextStr=0;
        CmtPtr=NextStr;
      }
      NextStr++;
    }
    bool Done=*NextStr==0;

    *NextStr=0;
    for (wchar *SpacePtr=(CmtPtr!=NULL ? CmtPtr:NextStr)-1;SpacePtr>=CurStr;SpacePtr--)
    {
      if (*SpacePtr!=' ' && *SpacePtr!='\t')
        break;
      *SpacePtr=0;
    }

    if (Unquote && *CurStr=='\"')
    {
      size_t Length=wcslen(CurStr);
      if (CurStr[Length-1]=='\"')
      {
        CurStr[Length-1]=0;
        CurStr++;
      }
    }

    bool Expanded=false;
#if defined(_WIN_ALL)
    if (ExpandEnvStr && *CurStr=='%')
    {
      std::wstring ExpName=CurStr;
      ExpandEnvironmentStr(ExpName);
      if (!ExpName.empty())
        List->AddString(ExpName);
      Expanded=true;
    }
#endif
    if (!Expanded && *CurStr!=0)
      List->AddString(CurStr);

    if (Done)
      break;
    CurStr=NextStr+1;
    while (*CurStr=='\r' || *CurStr=='\n')
      CurStr++;
  }
  return true;
}

RAR_CHARSET DetectTextEncoding(const byte *Data,size_t DataSize)
{
  if (DataSize>3 && Data[0]==0xef && Data[1]==0xbb && Data[2]==0xbf &&
      IsTextUtf8(Data+3,DataSize-3))
    return RCH_UTF8;

  bool LittleEndian=DataSize>2 && Data[0]==255 && Data[1]==254;
  bool BigEndian=DataSize>2 && Data[0]==254 && Data[1]==255;

  if (LittleEndian || BigEndian)
    for (size_t I=LittleEndian ? 3 : 2;I<DataSize;I+=2)
      if (Data[I]<32 && Data[I]!='\r' && Data[I]!='\n')
        return RCH_UNICODE;

  return RCH_DEFAULT;
}

FindFile::FindFile()
{
  FirstCall=true;
#ifdef _WIN_ALL
  hFind=INVALID_HANDLE_VALUE;
#else
  dirp=NULL;
#endif
}

FindFile::~FindFile()
{
#ifdef _WIN_ALL
  if (hFind!=INVALID_HANDLE_VALUE)
    FindClose(hFind);
#else
  if (dirp!=NULL)
    closedir(dirp);
#endif
}

void FindFile::SetMask(const std::wstring &Mask)
{
  FindMask=Mask;
  FirstCall=true;
}

bool FindFile::Next(FindData *fd,bool GetSymLink)
{
  fd->Error=false;
  if (FindMask.empty())
    return false;
#ifdef _WIN_ALL
  if (FirstCall)
  {
    if ((hFind=Win32Find(INVALID_HANDLE_VALUE,FindMask,fd))==INVALID_HANDLE_VALUE)
      return false;
  }
  else
    if (Win32Find(hFind,FindMask,fd)==INVALID_HANDLE_VALUE)
      return false;
#else
  if (FirstCall)
  {
    std::wstring DirName;
    DirName=FindMask;
    RemoveNameFromPath(DirName);
    if (DirName.empty())
      DirName=L".";
    std::string DirNameA;
    WideToChar(DirName,DirNameA);
    if ((dirp=opendir(DirNameA.c_str()))==NULL)
    {
      fd->Error=(errno!=ENOENT);
      return false;
    }
  }
  while (1)
  {
    std::wstring Name;
    struct dirent *ent=readdir(dirp);
    if (ent==NULL)
      return false;
    if (strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)
      continue;
    if (!CharToWide(std::string(ent->d_name),Name))
      uiMsg(UIERROR_INVALIDNAME,L"",Name);

    if (CmpName(FindMask,Name,MATCH_NAMES))
    {
      std::wstring FullName=FindMask;
      FullName.erase(GetNamePos(FullName));
      if (FullName.size()+Name.size()>=MAXPATHSIZE)
      {
        uiMsg(UIERROR_PATHTOOLONG,FullName,L"",Name);
        return false;
      }
      FullName+=Name;
      if (!FastFind(FullName,fd,GetSymLink))
      {
        ErrHandler.OpenErrorMsg(FullName);
        continue;
      }
      fd->Name=FullName;
      break;
    }
  }
#endif
  fd->Flags=0;
  fd->IsDir=IsDir(fd->FileAttr);
  fd->IsLink=IsLink(fd->FileAttr);

  FirstCall=false;
  std::wstring NameOnly=PointToName(fd->Name);
  if (NameOnly==L"." || NameOnly==L"..")
    return Next(fd);
  return true;
}

bool FindFile::FastFind(const std::wstring &FindMask,FindData *fd,bool GetSymLink)
{
  fd->Error=false;
#ifndef _UNIX
  if (IsWildcard(FindMask))
    return false;
#endif
#ifdef _WIN_ALL
  HANDLE hFind=Win32Find(INVALID_HANDLE_VALUE,FindMask,fd);
  if (hFind==INVALID_HANDLE_VALUE)
    return false;
  FindClose(hFind);
#elif defined(_UNIX)
  std::string FindMaskA;
  WideToChar(FindMask,FindMaskA);

  struct stat st;
  if (GetSymLink)
  {
#ifdef SAVE_LINKS
    if (lstat(FindMaskA.c_str(),&st)!=0)
#else
    if (stat(FindMaskA.c_str(),&st)!=0)
#endif
    {
      fd->Error=(errno!=ENOENT);
      return false;
    }
  }
  else
    if (stat(FindMaskA.c_str(),&st)!=0)
    {
      fd->Error=(errno!=ENOENT);
      return false;
    }
  fd->FileAttr=st.st_mode;
  fd->Size=st.st_size;

  File::StatToRarTime(st,&fd->mtime,&fd->ctime,&fd->atime);

  fd->Name=FindMask;
#endif
  fd->Flags=0;
  fd->IsDir=IsDir(fd->FileAttr);
  fd->IsLink=IsLink(fd->FileAttr);

  return true;
}

#ifdef _WIN_ALL
HANDLE FindFile::Win32Find(HANDLE hFind,const std::wstring &Mask,FindData *fd)
{
  WIN32_FIND_DATA FindData;
  if (hFind==INVALID_HANDLE_VALUE)
  {
    hFind=FindFirstFile(Mask.c_str(),&FindData);
    if (hFind==INVALID_HANDLE_VALUE)
    {
      std::wstring LongMask;
      if (GetWinLongPath(Mask,LongMask))
        hFind=FindFirstFile(LongMask.c_str(),&FindData);
    }
    if (hFind==INVALID_HANDLE_VALUE)
    {
      int SysErr=GetLastError();

      fd->Error=SysErr!=ERROR_FILE_NOT_FOUND &&
                SysErr!=ERROR_PATH_NOT_FOUND &&
                SysErr!=ERROR_NO_MORE_FILES;
    }
  }
  else
    if (!FindNextFile(hFind,&FindData))
    {
      hFind=INVALID_HANDLE_VALUE;
      fd->Error=GetLastError()!=ERROR_NO_MORE_FILES;
    }

  if (hFind!=INVALID_HANDLE_VALUE)
  {
    fd->Name=Mask;
    SetName(fd->Name,FindData.cFileName);
    fd->Size=INT32TO64(FindData.nFileSizeHigh,FindData.nFileSizeLow);
    fd->FileAttr=FindData.dwFileAttributes;
    fd->ftCreationTime=FindData.ftCreationTime;
    fd->ftLastAccessTime=FindData.ftLastAccessTime;
    fd->ftLastWriteTime=FindData.ftLastWriteTime;
    fd->mtime.SetWinFT(&FindData.ftLastWriteTime);
    fd->ctime.SetWinFT(&FindData.ftCreationTime);
    fd->atime.SetWinFT(&FindData.ftLastAccessTime);

  }
  fd->Flags=0;
  return hFind;
}
#endif

BitInput::BitInput(bool AllocBuffer)
{
  ExternalBuffer=false;
  if (AllocBuffer)
  {

    size_t BufSize=MAX_SIZE+8;
    InBuf=new byte[BufSize];

    memset(InBuf,0,BufSize);
  }
  else
    InBuf=nullptr;
}

BitInput::~BitInput()
{
  if (!ExternalBuffer)
    delete[] InBuf;
}

void BitInput::faddbits(uint Bits)
{

  addbits(Bits);
}

uint BitInput::fgetbits()
{

  return getbits();
}

void BitInput::SetExternalBuffer(byte *Buf)
{
  if (InBuf!=NULL && !ExternalBuffer)
    delete[] InBuf;
  InBuf=Buf;
  ExternalBuffer=true;
}

void HashValue::Init(HASH_TYPE Type)
{
  HashValue::Type=Type;

  if (Type==HASH_RAR14 || Type==HASH_CRC32)
    CRC32=0;
  if (Type==HASH_BLAKE2)
  {

    static byte EmptyHash[32]={
      0xdd, 0x0e, 0x89, 0x17, 0x76, 0x93, 0x3f, 0x43,
      0xc7, 0xd0, 0x32, 0xb0, 0x8a, 0x91, 0x7e, 0x25,
      0x74, 0x1f, 0x8a, 0xa9, 0xa1, 0x2c, 0x12, 0xe1,
      0xca, 0xc8, 0x80, 0x15, 0x00, 0xf2, 0xca, 0x4f
    };
    memcpy(Digest,EmptyHash,sizeof(Digest));
  }
}

bool HashValue::operator == (const HashValue &cmp) const
{
  if (Type==HASH_NONE || cmp.Type==HASH_NONE)
    return true;
  if (Type==HASH_RAR14 && cmp.Type==HASH_RAR14 ||
      Type==HASH_CRC32 && cmp.Type==HASH_CRC32)
    return CRC32==cmp.CRC32;
  if (Type==HASH_BLAKE2 && cmp.Type==HASH_BLAKE2)
    return memcmp(Digest,cmp.Digest,sizeof(Digest))==0;
  return false;
}

DataHash::DataHash()
{
  blake2ctx=NULL;
  HashType=HASH_NONE;
#ifdef RAR_SMP
  ThPool=NULL;
  MaxThreads=0;
#endif
}

DataHash::~DataHash()
{
#ifdef RAR_SMP
  delete ThPool;
#endif
  cleandata(&CurCRC32, sizeof(CurCRC32));
  if (blake2ctx!=NULL)
  {
    cleandata(blake2ctx, sizeof(blake2sp_state));
    delete blake2ctx;
  }
}

void DataHash::Init(HASH_TYPE Type,uint MaxThreads)
{
  if (blake2ctx==NULL)
    blake2ctx=new blake2sp_state;
  HashType=Type;
  if (Type==HASH_RAR14)
    CurCRC32=0;
  if (Type==HASH_CRC32)
    CurCRC32=0xffffffff;
  if (Type==HASH_BLAKE2)
    blake2sp_init(blake2ctx);
#ifdef RAR_SMP
  DataHash::MaxThreads=Min(MaxThreads,HASH_POOL_THREADS);
#endif
}

void DataHash::Update(const void *Data,size_t DataSize)
{
#ifndef SFX_MODULE
  if (HashType==HASH_RAR14)
    CurCRC32=Checksum14((ushort)CurCRC32,Data,DataSize);
#endif
  if (HashType==HASH_CRC32)
  {
#ifdef RAR_SMP
    UpdateCRC32MT(Data,DataSize);
#else
    CurCRC32=CRC32(CurCRC32,Data,DataSize);
#endif
  }

  if (HashType==HASH_BLAKE2)
  {
#ifdef RAR_SMP
    if (MaxThreads>1 && ThPool==nullptr)
      ThPool=new ThreadPool(HASH_POOL_THREADS);
    blake2ctx->ThPool=ThPool;
    blake2ctx->MaxThreads=MaxThreads;
#endif
    blake2sp_update( blake2ctx, (byte *)Data, DataSize);
  }
}

#ifdef RAR_SMP
THREAD_PROC(BuildCRC32Thread)
{
  DataHash::CRC32ThreadData *td=(DataHash::CRC32ThreadData *)Data;

  td->DataCRC=CRC32(0,td->Data,td->DataSize);
}

void DataHash::UpdateCRC32MT(const void *Data,size_t DataSize)
{
  const size_t MinBlock=0x4000;
  if (DataSize<2*MinBlock || MaxThreads<2)
  {
    CurCRC32=CRC32(CurCRC32,Data,DataSize);
    return;
  }

  if (ThPool==nullptr)
    ThPool=new ThreadPool(HASH_POOL_THREADS);

  size_t Threads=MaxThreads;
  size_t BlockSize=DataSize/Threads;
  if (BlockSize<MinBlock)
  {
    BlockSize=MinBlock;
    Threads=DataSize/BlockSize;
  }

  CRC32ThreadData td[MaxPoolThreads];

  for (size_t I=0;I<Threads;I++)
  {
    td[I].Data=(byte*)Data+I*BlockSize;
    td[I].DataSize=(I+1==Threads) ? DataSize-I*BlockSize : BlockSize;
#ifdef USE_THREADS
    ThPool->AddTask(BuildCRC32Thread,(void*)&td[I]);
#else
    BuildCRC32Thread((void*)&td[I]);
#endif
  }

#ifdef USE_THREADS
  ThPool->WaitDone();
#endif

  uint StdShift=gfExpCRC(uint(8*td[0].DataSize));
  for (size_t I=0;I<Threads;I++)
  {

    uint ShiftMult;
    if (td[I].DataSize==td[0].DataSize)
      ShiftMult=StdShift;
    else
      ShiftMult=gfExpCRC(uint(8*td[I].DataSize));

    CurCRC32=BitReverse32(gfMulCRC(BitReverse32(CurCRC32), ShiftMult));

    CurCRC32^=td[I].DataCRC;
  }
}
#endif

uint DataHash::BitReverse32(uint N)
{
  uint Reversed=0;
  for (uint I=0;I<32;I++,N>>=1)
    Reversed|=(N & 1)<<(31-I);
  return Reversed;
}

uint DataHash::gfMulCRC(uint A, uint B)
{

  const uint POLY=uint(0x104c11db7);

  uint R = 0 ;
  while (A != 0 && B != 0)
  {

    R ^= (B & 1)!=0 ? A : 0;

    A  = (A << 1) ^ ((A & 0x80000000)!=0 ? POLY : 0);

    B >>= 1;
  }
  return R;
}

uint DataHash::gfExpCRC(uint N)
{
  uint S = 2;
  uint R = 1;
  while (N > 1)
  {
    if ((N & 1)!=0)
      R = gfMulCRC(R, S);
    S = gfMulCRC(S, S);
    N >>= 1;
  }

  return gfMulCRC(R, S);
}

void DataHash::Result(HashValue *Result)
{
  Result->Type=HashType;
  if (HashType==HASH_RAR14)
    Result->CRC32=CurCRC32;
  if (HashType==HASH_CRC32)
    Result->CRC32=CurCRC32^0xffffffff;
  if (HashType==HASH_BLAKE2)
  {

    blake2sp_state res=*blake2ctx;
    blake2sp_final(&res,Result->Digest);
  }
}

uint DataHash::GetCRC32()
{
  return HashType==HASH_CRC32 ? CurCRC32^0xffffffff : 0;
}

bool DataHash::Cmp(HashValue *CmpValue,byte *Key)
{
  HashValue Final;
  Result(&Final);
#ifndef RAR_NOCRYPT
  if (Key!=nullptr)
    ConvertHashToMAC(&Final,Key);
#endif
  return Final==*CmpValue;
}

void FileHeader::Reset(size_t SubDataSize)
{
  SubData.resize(SubDataSize);
  BaseBlock::Reset();
  FileHash.Init(HASH_NONE);
  mtime.Reset();
  atime.Reset();
  ctime.Reset();
  SplitBefore=false;
  SplitAfter=false;

  UnknownUnpSize=0;

  SubFlags=0;

  CryptMethod=CRYPT_NONE;
  Encrypted=false;
  SaltSet=false;
  UsePswCheck=false;
  UseHashKey=false;
  Lg2Count=0;

  Solid=false;
  Dir=false;
  WinSize=0;
  Inherited=false;
  SubBlock=false;
  CommentInHeader=false;
  Version=false;
  LargeFile=false;

  RedirType=FSREDIR_NONE;
  DirTarget=false;
  UnixOwnerSet=false;
}

void MainHeader::Reset()
{
  *this={};
}

DWORD WinNT()
{
  static int dwPlatformId=-1;
  static DWORD dwMajorVersion,dwMinorVersion;
  if (dwPlatformId==-1)
  {
    OSVERSIONINFO WinVer;
    WinVer.dwOSVersionInfoSize=sizeof(WinVer);
    GetVersionEx(&WinVer);
    dwPlatformId=WinVer.dwPlatformId;
    dwMajorVersion=WinVer.dwMajorVersion;
    dwMinorVersion=WinVer.dwMinorVersion;

  }
  DWORD Result=0;
  if (dwPlatformId==VER_PLATFORM_WIN32_NT)
    Result=dwMajorVersion*0x100+dwMinorVersion;

  return Result;
}

#include <comdef.h>
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

static bool WMI_IsWindows10()
{
  IWbemLocator *pLoc = NULL;

  HRESULT hres = CoCreateInstance(CLSID_WbemLocator,0,CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator,(LPVOID *)&pLoc);

  if (FAILED(hres))
    return false;

  IWbemServices *pSvc = NULL;

  hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"),NULL,NULL,NULL,0,NULL,NULL,&pSvc);

  if (FAILED(hres))
  {
    pLoc->Release();
    return false;
  }

  hres = CoSetProxyBlanket(pSvc,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,NULL,
         RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE);

  if (FAILED(hres))
  {
    pSvc->Release();
    pLoc->Release();
    return false;
  }

  IEnumWbemClassObject *pEnumerator = NULL;
  hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_OperatingSystem"),
         WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

  if (FAILED(hres) || pEnumerator==NULL)
  {
    pSvc->Release();
    pLoc->Release();
    return false;
  }

  bool Win10=false;

  IWbemClassObject *pclsObj = NULL;
  ULONG uReturn = 0;
  pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
  if (pclsObj!=NULL && uReturn>0)
  {
    VARIANT vtProp;
    pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
    Win10|=wcsstr(vtProp.bstrVal,L"Windows 10")!=NULL;
    VariantClear(&vtProp);
    pclsObj->Release();
  }

  pSvc->Release();
  pLoc->Release();
  pEnumerator->Release();

  return Win10;
}

bool IsWindows11OrGreater()
{
  static bool IsSet=false,IsWin11=false;
  if (!IsSet)
  {
    OSVERSIONINFO WinVer;
    WinVer.dwOSVersionInfoSize=sizeof(WinVer);
    GetVersionEx(&WinVer);
    IsWin11=WinVer.dwMajorVersion>10 ||
          WinVer.dwMajorVersion==10 && WinVer.dwBuildNumber >= 22000 && !WMI_IsWindows10();
    IsSet=true;
  }
  return IsWin11;
}

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(RARDLL)
#define ALLOW_LARGE_PAGES
#endif

LargePageAlloc::LargePageAlloc()
{
  UseLargePages=false;
#ifdef ALLOW_LARGE_PAGES
  PageSize=0;
#endif
}

void LargePageAlloc::AllowLargePages(bool Allow)
{
#ifdef ALLOW_LARGE_PAGES
  if (Allow && PageSize==0)
  {
    HMODULE hKernel=GetModuleHandle(L"kernel32.dll");
    if (hKernel!=nullptr)
    {
      typedef SIZE_T (*GETLARGEPAGEMINIMUM)();
      GETLARGEPAGEMINIMUM pGetLargePageMinimum=(GETLARGEPAGEMINIMUM)GetProcAddress(hKernel, "GetLargePageMinimum");
      if (pGetLargePageMinimum!=nullptr)
        PageSize=pGetLargePageMinimum();
    }
    if (PageSize==0 || !SetPrivilege(SE_LOCK_MEMORY_NAME))
    {
      UseLargePages=false;
      return;
    }
  }

  UseLargePages=Allow;
#endif
}

bool LargePageAlloc::IsPrivilegeAssigned()
{
#ifdef ALLOW_LARGE_PAGES
  return SetPrivilege(SE_LOCK_MEMORY_NAME);
#else
  return true;
#endif
}

bool LargePageAlloc::AssignPrivilege()
{
#ifdef ALLOW_LARGE_PAGES
  HANDLE hToken = NULL;

  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    return false;

  DWORD BufSize=0;
  GetTokenInformation(hToken, TokenUser, NULL, 0, &BufSize);
  if (BufSize==0 || BufSize>1000000)
  {
    CloseHandle(hToken);
    return false;
  }

  TOKEN_USER *TokenInfo = (TOKEN_USER*)malloc(BufSize);

  if (GetTokenInformation(hToken,TokenUser,TokenInfo,BufSize,&BufSize)==0)
  {
    CloseHandle(hToken);
    return false;
  }

  LPWSTR ApiSidStr;
  ConvertSidToStringSid(TokenInfo->User.Sid, &ApiSidStr);

  std::wstring SidStr=ApiSidStr;
  LocalFree(ApiSidStr);
  CloseHandle(hToken);

  if (IsUserAdmin())
    AssignPrivilegeBySid(SidStr);
  else
  {

    std::wstring ExeName=GetModuleFileStr();
    std::wstring Param=std::wstring(L"-") + LOCKMEM_SWITCH + SidStr;

    SHELLEXECUTEINFO shExecInfo{};
    shExecInfo.cbSize = sizeof(shExecInfo);

    shExecInfo.hwnd = NULL;
    shExecInfo.lpVerb = L"runas";
    shExecInfo.lpFile = ExeName.c_str();
    shExecInfo.lpParameters = Param.c_str();
    shExecInfo.nShow = SW_SHOWNORMAL;
    BOOL Result=ShellExecuteEx(&shExecInfo);
  }
#endif

  return true;
}

bool LargePageAlloc::AssignPrivilegeBySid(const std::wstring &Sid)
{
#ifdef ALLOW_LARGE_PAGES
  LSA_HANDLE PolicyHandle;
  LSA_OBJECT_ATTRIBUTES ObjectAttributes{};

#ifndef STATUS_SUCCESS

  const uint STATUS_SUCCESS=0;
#endif

  if (LsaOpenPolicy(NULL,&ObjectAttributes,POLICY_CREATE_ACCOUNT|
                    POLICY_LOOKUP_NAMES,&PolicyHandle)!=STATUS_SUCCESS)
    return false;

  PSID UserSid;
  ConvertStringSidToSid(Sid.c_str(),&UserSid);

  LSA_UNICODE_STRING LsaString;
  LsaString.Buffer=(PWSTR)SE_LOCK_MEMORY_NAME;

  LsaString.Length=(USHORT)wcslen(LsaString.Buffer)*sizeof(LsaString.Buffer[0]);
  LsaString.MaximumLength=LsaString.Length;

  bool Success=LsaAddAccountRights(PolicyHandle,UserSid,&LsaString,1)==STATUS_SUCCESS;

  LocalFree(UserSid);
  LsaClose(PolicyHandle);

  mprintf(St(MPrivilegeAssigned));
  if (Ask(St(MYesNo)) == 1)
    Shutdown(POWERMODE_RESTART);

  return Success;
#else
  return true;
#endif
}

bool LargePageAlloc::AssignConfirmation()
{
#ifdef ALLOW_LARGE_PAGES
  mprintf(St(MLockInMemoryNeeded));
  return Ask(St(MYesNo)) == 1;
#else
  return false;
#endif
}

void* LargePageAlloc::new_large(size_t Size)
{
  void *Allocated=nullptr;

#ifdef ALLOW_LARGE_PAGES
  if (UseLargePages && Size>=PageSize)
  {

    SIZE_T AllocSize=Size%PageSize==0 ? Size:(Size/PageSize+1)*PageSize;
    Allocated=VirtualAlloc(nullptr,AllocSize,MEM_COMMIT|MEM_RESERVE|MEM_LARGE_PAGES,PAGE_READWRITE);
    if (Allocated!=nullptr)
      LargeAlloc.push_back(Allocated);
  }
#endif
  return Allocated;
}

bool LargePageAlloc::delete_large(void *Addr)
{
#ifdef ALLOW_LARGE_PAGES
  if (Addr!=nullptr)
    for (size_t I=0;I<LargeAlloc.size();I++)
      if (LargeAlloc[I]==Addr)
      {
        LargeAlloc[I]=nullptr;
        VirtualFree(Addr,0,MEM_RELEASE);
        return true;
      }
#endif
  return false;
}

static void ListFileHeader(Archive &Arc,FileHeader &hd,bool &TitleShown,bool Verbose,bool Technical,bool Bare,bool DisableNames);
static void ListFileAttr(uint A,HOST_SYSTEM_TYPE HostType,wchar *AttrStr,size_t AttrSize);
static void ListOldSubHeader(Archive &Arc);
static void ListNewSubHeader(CommandData *Cmd,Archive &Arc);

void ListArchive(CommandData *Cmd)
{
  int64 SumPackSize=0,SumUnpSize=0;
  uint ArcCount=0,SumFileCount=0;
  bool Technical=(Cmd->Command[1]=='T');
  bool ShowService=Technical && Cmd->Command[2]=='A';
  bool Bare=(Cmd->Command[1]=='B');
  bool Verbose=(Cmd->Command[0]=='V');

  std::wstring ArcName;
  while (Cmd->GetArcName(ArcName))
  {
    if (Cmd->ManualPassword)
      Cmd->Password.Clean();

    Archive Arc(Cmd);
    if (!Arc.WOpen(ArcName))
      continue;
    bool FileMatched=true;
    while (true)
    {
      int64 TotalPackSize=0,TotalUnpSize=0;
      uint FileCount=0;
      if (Arc.IsArchive(true))
      {
        bool TitleShown=false;
        if (!Bare)
        {
          Arc.ViewComment();
          mprintf(L"\n%s: %s",St(MListArchive),Arc.FileName.c_str());

          mprintf(L"\n%s: ",St(MListDetails));
          const wchar *Fmt=Arc.Format==RARFMT14 ? L"RAR 1.4":(Arc.Format==RARFMT15 ? L"RAR 1.5":L"RAR 5");
          mprintf(L"%s", Fmt);
          if (Arc.Solid)
            mprintf(L", %s", St(MListSolid));
          if (Arc.SFXSize>0)
            mprintf(L", %s", St(MListSFX));
          if (Arc.Volume)
            if (Arc.Format==RARFMT50)
            {

              mprintf(L", ");
              mprintf(St(MVolumeNumber),Arc.VolNumber+1);
            }
            else
              mprintf(L", %s", St(MListVolume));
          if (Arc.Protected)
            mprintf(L", %s", St(MListRR));
          if (Arc.Locked)
            mprintf(L", %s", St(MListLock));
          if (Arc.Encrypted)
            mprintf(L", %s", St(MListEncHead));

          if (!Arc.MainHead.OrigName.empty())
            mprintf(L"\n%s: %s",St(MOrigName),Arc.MainHead.OrigName.c_str());
          if (Arc.MainHead.OrigTime.IsSet())
          {
            wchar DateStr[50];
            Arc.MainHead.OrigTime.GetText(DateStr,ASIZE(DateStr),Technical);
            mprintf(L"\n%s: %s",St(MOriginalTime),DateStr);
          }

          mprintf(L"\n");
        }

        wchar VolNumText[50];
        *VolNumText=0;
        while (Arc.ReadHeader()>0)
        {
          Wait();
          HEADER_TYPE HeaderType=Arc.GetHeaderType();
          if (HeaderType==HEAD_ENDARC)
          {
#ifndef SFX_MODULE

            if (Arc.EndArcHead.StoreVolNumber && Arc.Format==RARFMT15)
              swprintf(VolNumText,ASIZE(VolNumText),L"%.10ls %u",St(MListVolume),Arc.VolNumber+1);
#endif
            if (Technical && ShowService)
            {
              mprintf(L"\n%12ls: %ls",St(MListService),L"EOF");
              if (*VolNumText!=0)
                mprintf(L"\n%12ls: %ls",St(MListFlags),VolNumText);
              mprintf(L"\n");
            }
            break;
          }
          switch(HeaderType)
          {
            case HEAD_FILE:
              FileMatched=Cmd->IsProcessFile(Arc.FileHead,NULL,MATCH_WILDSUBPATH,0,NULL)!=0;
              if (FileMatched)
              {
                ListFileHeader(Arc,Arc.FileHead,TitleShown,Verbose,Technical,Bare,Cmd->DisableNames);
                if (!Arc.FileHead.SplitBefore)
                {
                  TotalUnpSize+=Arc.FileHead.UnpSize;
                  FileCount++;
                }
                TotalPackSize+=Arc.FileHead.PackSize;
              }
              break;
            case HEAD_SERVICE:

              if (!Arc.SubHead.SubBlock || Cmd->DisableNames)
                FileMatched=Cmd->IsProcessFile(Arc.SubHead,NULL,MATCH_WILDSUBPATH,0,NULL)!=0;
              if (FileMatched && !Bare)
              {

                if (Technical && ShowService)
                  ListFileHeader(Arc,Arc.SubHead,TitleShown,Verbose,true,false,false);
              }
              break;
          }
          Arc.SeekToNext();
        }
        if (!Bare && !Technical)
          if (TitleShown)
          {
            wchar UnpSizeText[20];
            itoa(TotalUnpSize,UnpSizeText,ASIZE(UnpSizeText));

            wchar PackSizeText[20];
            itoa(TotalPackSize,PackSizeText,ASIZE(PackSizeText));

            if (Verbose)
            {
              mprintf(L"\n----------- ---------  -------- ----- ---------- -----  --------  ----");
              mprintf(L"\n%21ls %9ls %3d%%  %-27ls %u",UnpSizeText,
                      PackSizeText,ToPercentUnlim(TotalPackSize,TotalUnpSize),
                      VolNumText,FileCount);
            }
            else
            {
              mprintf(L"\n----------- ---------  ---------- -----  ----");
              mprintf(L"\n%21ls  %-16ls  %u",UnpSizeText,VolNumText,FileCount);
            }

            SumFileCount+=FileCount;
            SumUnpSize+=TotalUnpSize;
            SumPackSize+=TotalPackSize;
            mprintf(L"\n");
          }
          else
            mprintf(St(MListNoFiles));

        ArcCount++;

#ifndef NOVOLUME
        if (Cmd->VolSize==VOLSIZE_AUTO && (Arc.FileHead.SplitAfter ||
            Arc.GetHeaderType()==HEAD_ENDARC && Arc.EndArcHead.NextVolume) &&
            MergeArchive(Arc,NULL,false,Cmd->Command[0]))
          Arc.Seek(0,SEEK_SET);
        else
#endif
          break;
      }
      else
      {
        if (Cmd->ArcNames.ItemsCount()<2 && !Bare)
          mprintf(St(MNotRAR),Arc.FileName.c_str());
        break;
      }
    }
  }

  if (Cmd->ManualPassword)
    Cmd->Password.Clean();

  if (ArcCount>1 && !Bare && !Technical)
  {
    wchar UnpSizeText[20],PackSizeText[20];
    itoa(SumUnpSize,UnpSizeText,ASIZE(UnpSizeText));
    itoa(SumPackSize,PackSizeText,ASIZE(PackSizeText));

    if (Verbose)
      mprintf(L"%21ls %9ls %3d%% %28ls %u",UnpSizeText,PackSizeText,
              ToPercentUnlim(SumPackSize,SumUnpSize),L"",SumFileCount);
    else
      mprintf(L"%21ls %18s %lu",UnpSizeText,L"",SumFileCount);
  }
}

enum LISTCOL_TYPE {
  LCOL_NAME,LCOL_ATTR,LCOL_SIZE,LCOL_PACKED,LCOL_RATIO,LCOL_CSUM,LCOL_ENCR
};

void ListFileHeader(Archive &Arc,FileHeader &hd,bool &TitleShown,bool Verbose,bool Technical,bool Bare,bool DisableNames)
{
  if (!TitleShown && !Technical && !Bare)
  {
    if (Verbose)
    {
      mprintf(L"\n%ls",St(MListTitleV));
      if (!DisableNames)
        mprintf(L"\n----------- ---------  -------- ----- ---------- -----  --------  ----");
    }
    else
    {
      mprintf(L"\n%ls",St(MListTitleL));
      if (!DisableNames)
        mprintf(L"\n----------- ---------  ---------- -----  ----");
    }

    TitleShown=true;
  }
  if (DisableNames)
    return;

  const wchar *Name=hd.FileName.c_str();
  RARFORMAT Format=Arc.Format;

  if (Bare)
  {
    mprintf(L"%s\n",Name);
    return;
  }

  wchar UnpSizeText[30],PackSizeText[30];
  if (hd.UnpSize==INT64NDF)
    wcsncpyz(UnpSizeText,L"?",ASIZE(UnpSizeText));
  else
    itoa(hd.UnpSize,UnpSizeText,ASIZE(UnpSizeText));
  itoa(hd.PackSize,PackSizeText,ASIZE(PackSizeText));

  wchar AttrStr[30];
  if (hd.HeaderType==HEAD_SERVICE)
    swprintf(AttrStr,ASIZE(AttrStr),L"%cB",hd.Inherited ? 'I' : '.');
  else
    ListFileAttr(hd.FileAttr,hd.HSType,AttrStr,ASIZE(AttrStr));

  wchar RatioStr[10];

  if (hd.SplitBefore && hd.SplitAfter)
    wcsncpyz(RatioStr,L"<->",ASIZE(RatioStr));
  else
    if (hd.SplitBefore)
      wcsncpyz(RatioStr,L"<--",ASIZE(RatioStr));
    else
      if (hd.SplitAfter)
        wcsncpyz(RatioStr,L"-->",ASIZE(RatioStr));
      else
        swprintf(RatioStr,ASIZE(RatioStr),L"%u%%",ToPercentUnlim(hd.PackSize,hd.UnpSize));

  wchar DateStr[50];
  hd.mtime.GetText(DateStr,ASIZE(DateStr),Technical);

  if (Technical)
  {
    mprintf(L"\n%12s: %s",St(MListName),Name);

    bool FileBlock=hd.HeaderType==HEAD_FILE;

    if (!FileBlock && Arc.SubHead.CmpName(SUBHEAD_TYPE_STREAM))
    {
      mprintf(L"\n%12ls: %ls",St(MListType),St(MListStream));
      std::wstring StreamName=GetStreamNameNTFS(Arc);
      mprintf(L"\n%12ls: %ls",St(MListTarget),StreamName.c_str());
    }
    else
    {
      const wchar *Type=St(FileBlock ? (hd.Dir ? MListDir:MListFile):MListService);

      if (hd.RedirType!=FSREDIR_NONE)
        switch(hd.RedirType)
        {
          case FSREDIR_UNIXSYMLINK:
            Type=St(MListUSymlink); break;
          case FSREDIR_WINSYMLINK:
            Type=St(MListWSymlink); break;
          case FSREDIR_JUNCTION:
            Type=St(MListJunction); break;
          case FSREDIR_HARDLINK:
            Type=St(MListHardlink); break;
          case FSREDIR_FILECOPY:
            Type=St(MListCopy);     break;
        }
      mprintf(L"\n%12ls: %ls",St(MListType),Type);
      if (hd.RedirType!=FSREDIR_NONE)
        if (Format==RARFMT15)
        {
          std::string LinkTargetA;
          if (Arc.FileHead.Encrypted)
          {

            LinkTargetA="*<-?->";
          }
          else
          {
            size_t DataSize=(size_t)Min(hd.PackSize,MAXPATHSIZE);
            std::vector<char> Buf(DataSize+1);
            Arc.Read(Buf.data(),DataSize);
            Buf[DataSize] = 0;
            LinkTargetA=Buf.data();
          }
          std::wstring LinkTarget;
          CharToWide(LinkTargetA,LinkTarget);
          mprintf(L"\n%12ls: %ls",St(MListTarget),LinkTarget.c_str());
        }
        else
          mprintf(L"\n%12ls: %ls",St(MListTarget),hd.RedirName.c_str());
    }
    if (!hd.Dir)
    {
      mprintf(L"\n%12ls: %ls",St(MListSize),UnpSizeText);
      mprintf(L"\n%12ls: %ls",St(MListPacked),PackSizeText);
      mprintf(L"\n%12ls: %ls",St(MListRatio),RatioStr);

      if (!FileBlock && Arc.SubHead.CmpName(SUBHEAD_TYPE_RR))
      {

        int RecoveryPercent=Arc.GetRecoveryPercent();
        if (RecoveryPercent>0)
          mprintf(L"\n%12ls: %u%%",L"RR%", RecoveryPercent);
      }
    }
    bool WinTitles=false;
#ifdef _WIN_ALL
    WinTitles=true;
#endif
    if (hd.mtime.IsSet())
      mprintf(L"\n%12ls: %ls",St(WinTitles ? MListModified:MListMtime),DateStr);
    if (hd.ctime.IsSet())
    {
      hd.ctime.GetText(DateStr,ASIZE(DateStr),true);
      mprintf(L"\n%12ls: %ls",St(WinTitles ? MListCreated:MListCtime),DateStr);
    }
    if (hd.atime.IsSet())
    {
      hd.atime.GetText(DateStr,ASIZE(DateStr),true);
      mprintf(L"\n%12ls: %ls",St(WinTitles ? MListAccessed:MListAtime),DateStr);
    }
    mprintf(L"\n%12ls: %ls",St(MListAttr),AttrStr);
    if (hd.FileHash.Type==HASH_CRC32)
      mprintf(L"\n%12ls: %8.8X",
        hd.UseHashKey ? L"CRC32 MAC":hd.SplitAfter ? L"Pack-CRC32":L"CRC32",
        hd.FileHash.CRC32);
    if (hd.FileHash.Type==HASH_BLAKE2)
    {
      std::wstring BlakeStr;
      BinToHex(hd.FileHash.Digest,BLAKE2_DIGEST_SIZE,BlakeStr);
      mprintf(L"\n%12ls: %ls",
        hd.UseHashKey ? L"BLAKE2 MAC":hd.SplitAfter ? L"Pack-BLAKE2":L"BLAKE2",
        BlakeStr.c_str());
    }

    const wchar *HostOS=L"";
    if (Format==RARFMT50 && hd.HSType!=HSYS_UNKNOWN)
      HostOS=hd.HSType==HSYS_WINDOWS ? L"Windows":L"Unix";
    if (Format==RARFMT15)
    {
      static const wchar *RarOS[]={
        L"DOS",L"OS/2",L"Windows",L"Unix",L"Mac OS",L"BeOS",L"WinCE",L"",L"",L""
      };
      if (hd.HostOS<ASIZE(RarOS))
        HostOS=RarOS[hd.HostOS];
    }
    if (*HostOS!=0)
      mprintf(L"\n%12ls: %ls",St(MListHostOS),HostOS);

    std::wstring WinSize;
    if (!hd.Dir)
      if (hd.WinSize%1073741824==0)
        WinSize=L" -md=" + std::to_wstring(hd.WinSize/1073741824) + L"g";
      else
        if (hd.WinSize%1048576==0)
          WinSize=L" -md=" + std::to_wstring(hd.WinSize/1048576) + L"m";
        else
          if (hd.WinSize>=1024)
            WinSize=L" -md=" + std::to_wstring(hd.WinSize/1024) + L"k";
          else
            WinSize=L" -md=?";

    mprintf(L"\n%12ls: RAR %ls(v%d) -m%d%s",St(MListCompInfo),
            Format==RARFMT15 ? L"1.5":L"5.0",
            hd.UnpVer==VER_UNKNOWN ? 0 : hd.UnpVer,hd.Method,WinSize.c_str());

    if (hd.Solid || hd.Encrypted)
    {
      mprintf(L"\n%12ls: ",St(MListFlags));
      if (hd.Solid)
        mprintf(L"%ls ",St(MListSolid));
      if (hd.Encrypted)
        mprintf(L"%ls ",St(MListEnc));
    }

    if (hd.Version)
    {
      uint Version=ParseVersionFileName(hd.FileName,false);
      if (Version!=0)
        mprintf(L"\n%12ls: %u",St(MListFileVer),Version);
    }

    if (hd.UnixOwnerSet)
    {
      mprintf(L"\n%12ls: ",L"Unix owner");
      if (*hd.UnixOwnerName!=0)
        mprintf(L"%ls",GetWide(hd.UnixOwnerName).c_str());
      else
        if (hd.UnixOwnerNumeric)
          mprintf(L"#%d",hd.UnixOwnerID);
      mprintf(L":");
      if (*hd.UnixGroupName!=0)
        mprintf(L"%ls",GetWide(hd.UnixGroupName).c_str());
      else
        if (hd.UnixGroupNumeric)
          mprintf(L"#%d",hd.UnixGroupID);
    }

    mprintf(L"\n");
    return;
  }

  mprintf(L"\n%c%10ls %9ls ",hd.Encrypted ? '*' : ' ',AttrStr,UnpSizeText);

  if (Verbose)
    mprintf(L"%9ls %4ls ",PackSizeText,RatioStr);

  mprintf(L" %ls  ",DateStr);

  if (Verbose)
  {
    if (hd.FileHash.Type==HASH_CRC32)
      mprintf(L"%8.8X  ",hd.FileHash.CRC32);
    else
      if (hd.FileHash.Type==HASH_BLAKE2)
      {
        byte *S=hd.FileHash.Digest;
        mprintf(L"%02x%02x..%02x  ",S[0],S[1],S[31]);
      }
      else
        mprintf(hd.Dir ? L"          ":L"????????  ");
  }
  mprintf(L"%ls",Name);
}

void ListFileAttr(uint A,HOST_SYSTEM_TYPE HostType,wchar *AttrStr,size_t AttrSize)
{
  switch(HostType)
  {
    case HSYS_WINDOWS:
      swprintf(AttrStr,AttrSize,L"%c%c%c%c%c%c%c",
              (A & 0x2000)!=0 ? 'I' : '.',
              (A & 0x0800)!=0 ? 'C' : '.',
              (A & 0x0020)!=0 ? 'A' : '.',
              (A & 0x0010)!=0 ? 'D' : '.',
              (A & 0x0004)!=0 ? 'S' : '.',
              (A & 0x0002)!=0 ? 'H' : '.',
              (A & 0x0001)!=0 ? 'R' : '.');
      break;
    case HSYS_UNIX:
      switch (A & 0xF000)
      {
        case 0x4000:
          AttrStr[0]='d';
          break;
        case 0xA000:
          AttrStr[0]='l';
          break;
        default:
          AttrStr[0]='-';
          break;
      }
      swprintf(AttrStr+1,AttrSize-1,L"%c%c%c%c%c%c%c%c%c",
              (A & 0x0100) ? 'r' : '-',
              (A & 0x0080) ? 'w' : '-',
              (A & 0x0040) ? ((A & 0x0800)!=0 ? 's':'x'):((A & 0x0800)!=0 ? 'S':'-'),
              (A & 0x0020) ? 'r' : '-',
              (A & 0x0010) ? 'w' : '-',
              (A & 0x0008) ? ((A & 0x0400)!=0 ? 's':'x'):((A & 0x0400)!=0 ? 'S':'-'),
              (A & 0x0004) ? 'r' : '-',
              (A & 0x0002) ? 'w' : '-',
              (A & 0x0001) ? ((A & 0x200)!=0 ? 't' : 'x') : '-');
      break;
    case HSYS_UNKNOWN:
      wcsncpyz(AttrStr,L"?",AttrSize);
      break;
  }
}

static bool match(const wchar *pattern,const wchar *string,bool ForceCase);
static int mwcsicompc(const wchar *Str1,const wchar *Str2,bool ForceCase);
static int mwcsnicompc(const wchar *Str1,const wchar *Str2,size_t N,bool ForceCase);
static bool IsWildcard(const wchar *Str,size_t CheckSize);

inline uint touppercw(uint ch,bool ForceCase)
{
  if (ForceCase)
    return ch;
#if defined(_UNIX)
  return ch;
#else
  return toupperw(ch);
#endif
}

bool CmpName(const wchar *Wildcard,const wchar *Name,uint CmpMode)
{
  bool ForceCase=(CmpMode&MATCH_FORCECASESENSITIVE)!=0;

  CmpMode&=MATCH_MODEMASK;

  wchar *Name1=PointToName(Wildcard);
  wchar *Name2=PointToName(Name);

  if (CmpMode!=MATCH_NAMES)
  {
    size_t WildLength=wcslen(Wildcard);
    if (CmpMode!=MATCH_EXACT && CmpMode!=MATCH_EXACTPATH && CmpMode!=MATCH_ALLWILD &&
        mwcsnicompc(Wildcard,Name,WildLength,ForceCase)==0)
    {

      wchar NextCh=Name[WildLength];
      if (NextCh==L'\\' || NextCh==L'/' || NextCh==0)
        return true;
    }

    if (CmpMode==MATCH_SUBPATHONLY)
      return false;

    size_t Path1Size=Name1-Wildcard;
    size_t Path2Size=Name2-Name;

    if ((CmpMode==MATCH_EXACT || CmpMode==MATCH_EXACTPATH) &&
        (Path1Size!=Path2Size ||
        mwcsnicompc(Wildcard,Name,Path1Size,ForceCase)!=0))
      return false;
    if (CmpMode==MATCH_ALLWILD)
      return match(Wildcard,Name,ForceCase);
    if (CmpMode==MATCH_SUBPATH || CmpMode==MATCH_WILDSUBPATH)
      if (IsWildcard(Wildcard,Path1Size))
        return match(Wildcard,Name,ForceCase);
      else
        if (CmpMode==MATCH_SUBPATH || IsWildcard(Wildcard))
        {
          if (Path1Size>0 && mwcsnicompc(Wildcard,Name,Path1Size,ForceCase)!=0)
            return false;
        }
        else
          if (Path1Size!=Path2Size || mwcsnicompc(Wildcard,Name,Path1Size,ForceCase)!=0)
            return false;
  }

  if (CmpMode==MATCH_EXACT)
    return mwcsicompc(Name1,Name2,ForceCase)==0;

  return match(Name1,Name2,ForceCase);
}

bool match(const wchar *pattern,const wchar *string,bool ForceCase)
{
  for (;; ++string)
  {
    wchar stringc=touppercw(*string,ForceCase);
    wchar patternc=touppercw(*pattern++,ForceCase);
    switch (patternc)
    {
      case 0:
        return stringc==0;
      case '?':
        if (stringc == 0)
          return false;
        break;
      case '*':
        if (*pattern==0)
          return true;
        if (*pattern=='.')
        {
          if (pattern[1]=='*' && pattern[2]==0)
            return true;
          const wchar *dot=wcschr(string,'.');
          if (pattern[1]==0)
            return (dot==NULL || dot[1]==0);
          if (dot!=NULL)
          {
            string=dot;
            if (wcspbrk(pattern,L"*?")==NULL && wcschr(string+1,'.')==NULL)
              return mwcsicompc(pattern+1,string+1,ForceCase)==0;
          }
        }

        while (*string)
          if (match(pattern,string++,ForceCase))
            return true;
        return false;
      default:
        if (patternc != stringc)
        {

          if (patternc=='.' && (stringc==0 || stringc=='\\' || stringc=='.'))
            return match(pattern,string,ForceCase);
          else
            return false;
        }
        break;
    }
  }
}

int mwcsicompc(const wchar *Str1,const wchar *Str2,bool ForceCase)
{
  if (ForceCase)
    return wcscmp(Str1,Str2);
  return wcsicompc(Str1,Str2);
}

int mwcsnicompc(const wchar *Str1,const wchar *Str2,size_t N,bool ForceCase)
{
  if (ForceCase)
    return wcsncmp(Str1,Str2,N);
#if defined(_UNIX)
  return wcsncmp(Str1,Str2,N);
#else
  return wcsnicomp(Str1,Str2,N);
#endif
}

bool IsWildcard(const wchar *Str,size_t CheckSize)
{
  size_t CheckPos=0;
#ifdef _WIN_ALL

  if (Str[0]=='\\' && Str[1]=='\\' && Str[2]=='?' && Str[3]=='\\')
    CheckPos+=4;
#endif
  for (size_t I=CheckPos;I<CheckSize && Str[I]!=0;I++)
    if (Str[I]=='*' || Str[I]=='?')
      return true;
  return false;
}

MarkOfTheWeb::MarkOfTheWeb()
{
  ZoneIdValue=-1;
  AllFields=false;
}

void MarkOfTheWeb::Clear()
{
  ZoneIdValue=-1;
}

void MarkOfTheWeb::ReadZoneIdStream(const std::wstring &FileName,bool AllFields)
{
  MarkOfTheWeb::AllFields=AllFields;
  ZoneIdValue=-1;
  ZoneIdStream.clear();

  std::wstring StreamName=FileName+MOTW_STREAM_NAME;

  File SrcFile;
  if (SrcFile.Open(StreamName))
  {
    ZoneIdStream.resize(MOTW_STREAM_MAX_SIZE);
    int BufSize=SrcFile.Read(&ZoneIdStream[0],ZoneIdStream.size());
    ZoneIdStream.resize(BufSize<0 ? 0:BufSize);

    if (BufSize<=0)
      return;

    ZoneIdValue=ParseZoneIdStream(ZoneIdStream);
  }
}

int MarkOfTheWeb::ParseZoneIdStream(std::string &Stream)
{
  if (Stream.rfind("[ZoneTransfer]",0)==std::string::npos)
    return -1;

  std::string::size_type ZoneId=Stream.find("ZoneId=",0);
  if (ZoneId==std::string::npos || !IsDigit(Stream[ZoneId+7]))
    return -1;
  int ZoneIdValue=atoi(&Stream[ZoneId+7]);
  if (ZoneIdValue<0 || ZoneIdValue>4)
    return -1;

  if (!AllFields)
    Stream="[ZoneTransfer]\r\nZoneId=" + std::to_string(ZoneIdValue) + "\r\n";

  return ZoneIdValue;
}

void MarkOfTheWeb::CreateZoneIdStream(const std::wstring &Name,StringList &MotwList)
{
  if (ZoneIdValue==-1)
    return;

  size_t ExtPos=GetExtPos(Name);
  const wchar *Ext=ExtPos==std::wstring::npos ? L"":&Name[ExtPos+1];

  bool Matched=false;
  const wchar *CurMask;
  MotwList.Rewind();
  while ((CurMask=MotwList.GetString())!=nullptr)
  {

    bool FastCmp=CurMask[0]=='*' && CurMask[1]=='.' && wcspbrk(CurMask+2,L"*?")==NULL;
    if (FastCmp && wcsicomp(Ext,CurMask+2)==0 || !FastCmp && CmpName(CurMask,Name,MATCH_NAMES))
    {
      Matched=true;
      break;
    }
  }

  if (!Matched)
    return;

  std::wstring StreamName=Name+MOTW_STREAM_NAME;

  File StreamFile;
  if (StreamFile.Create(StreamName))
  {

    StreamFile.SetExceptions(false);
    if (StreamFile.Write(&ZoneIdStream[0],ZoneIdStream.size()))
      StreamFile.Close();
  }
}

bool MarkOfTheWeb::IsNameConflicting(const std::wstring &StreamName)
{

  return wcsicomp(StreamName,MOTW_STREAM_NAME)==0 && ZoneIdValue!=-1;
}

bool MarkOfTheWeb::IsFileStreamMoreSecure(std::string &FileStream)
{
  int StreamZone=ParseZoneIdStream(FileStream);
  return StreamZone>ZoneIdValue;
}

RAROptions::RAROptions()
{
  Init();
}

void RAROptions::Init()
{
  memset(this,0,sizeof(RAROptions));
  WinSize=0x2000000;
  WinSizeLimit=0x100000000;
  Overwrite=OVERWRITE_DEFAULT;
  Method=3;
  MsgStream=MSG_STDOUT;
  ConvertNames=NAMES_ORIGINALCASE;
  xmtime=EXTTIME_MAX;
  FileSizeLess=INT64NDF;
  FileSizeMore=INT64NDF;
  HashType=HASH_CRC32;
#ifdef RAR_SMP
  Threads=GetNumberOfThreads();
#endif
#ifdef USE_QOPEN
  QOpenMode=QOPEN_AUTO;
#endif
}

wchar* PointToName(const wchar *Path)
{
  for (int I=(int)wcslen(Path)-1;I>=0;I--)
    if (IsPathDiv(Path[I]))
      return (wchar*)&Path[I+1];
  return (wchar*)((*Path!=0 && IsDriveDiv(Path[1])) ? Path+2:Path);
}

std::wstring PointToName(const std::wstring &Path)
{
  return std::wstring(Path.substr(GetNamePos(Path)));
}

size_t GetNamePos(const std::wstring &Path)
{
  for (int I=(int)Path.size()-1;I>=0;I--)
    if (IsPathDiv(Path[I]))
      return I+1;
  return IsDriveLetter(Path) ? 2 : 0;
}

wchar* PointToLastChar(const wchar *Path)
{
  size_t Length=wcslen(Path);
  return (wchar*)(Length>0 ? Path+Length-1:Path);
}

wchar GetLastChar(const std::wstring &Path)
{
  return Path.empty() ? 0:Path.back();
}

size_t ConvertPath(const std::wstring *SrcPath,std::wstring *DestPath)
{
  const std::wstring &S=*SrcPath;
  size_t DestPos=0;

  for (size_t I=0;I<S.size();I++)
    if (IsPathDiv(S[I]) && S[I+1]=='.' && S[I+2]=='.' &&
        (IsPathDiv(S[I+3]) || S[I+3]==0))
      DestPos=S[I+3]==0 ? I+3 : I+4;

  while (DestPos<S.size())
  {
    size_t I=DestPos;
    if (I+1<S.size() && IsDriveDiv(S[I+1]))
      I+=2;

    if (IsPathDiv(S[I]) && IsPathDiv(S[I+1]))
    {
      uint SlashCount=0;
      for (size_t J=I+2;J<S.size();J++)
        if (IsPathDiv(S[J]) && ++SlashCount==2)
        {
          I=J+1;
          break;
        }
    }

    for (size_t J=I;J<S.size();J++)
      if (IsPathDiv(S[J]))
        I=J+1;
      else
        if (S[J]!='.')
          break;
    if (I==DestPos)
      break;
    DestPos=I;
  }

  if (DestPath!=nullptr)
    *DestPath=S.substr(DestPos);

  return DestPos;
}

void SetName(std::wstring &FullName,const std::wstring &Name)
{
  auto Pos=GetNamePos(FullName);
  FullName.replace(Pos,std::wstring::npos,Name);
}

void SetExt(std::wstring &Name,std::wstring NewExt)
{
  auto DotPos=GetExtPos(Name);
  if (DotPos!=std::wstring::npos)
    Name.erase(DotPos);
  Name+=L"."+NewExt;
}

void RemoveExt(std::wstring &Name)
{
  auto DotPos=GetExtPos(Name);
  if (DotPos!=std::wstring::npos)
    Name.erase(DotPos);
}

#ifndef SFX_MODULE
void SetSFXExt(std::wstring &SFXName)
{
#ifdef _WIN_ALL
  SetExt(SFXName,L"exe");
#elif defined(_UNIX)
  SetExt(SFXName,L"sfx");
#endif
}
#endif

wchar *GetExt(const wchar *Name)
{
  return Name==NULL ? NULL:wcsrchr(PointToName(Name),'.');
}

std::wstring GetExt(const std::wstring &Name)
{
  auto ExtPos=GetExtPos(Name);
  if (ExtPos==std::wstring::npos)
    ExtPos=Name.size();
  return Name.substr(ExtPos);
}

std::wstring::size_type GetExtPos(const std::wstring &Name)
{
  auto NamePos=GetNamePos(Name);
  auto DotPos=Name.rfind('.');
  return DotPos<NamePos ? std::wstring::npos : DotPos;
}

bool CmpExt(const std::wstring &Name,const std::wstring &Ext)
{
  size_t ExtPos=GetExtPos(Name);
  if (ExtPos==std::wstring::npos)
    return Ext.empty();

  return wcsicomp(&Name[ExtPos+1],Ext.data())==0;
}

bool IsWildcard(const std::wstring &Str)
{
  size_t StartPos=0;
#ifdef _WIN_ALL

  if (Str.rfind(L"\\\\?\\",0)==0)
    StartPos=4;
#endif
  return Str.find_first_of(L"*?",StartPos)!=std::wstring::npos;
}

bool IsPathDiv(int Ch)
{
#ifdef _WIN_ALL
  return Ch=='\\' || Ch=='/';
#else
  return Ch==CPATHDIVIDER;
#endif
}

bool IsDriveDiv(int Ch)
{
#ifdef _UNIX
  return false;
#else
  return Ch==':';
#endif
}

bool IsDriveLetter(const std::wstring &Path)
{
  if (Path.size()<2)
    return false;
  wchar Letter=etoupperw(Path[0]);
  return Letter>='A' && Letter<='Z' && IsDriveDiv(Path[1]);
}

int GetPathDisk(const std::wstring &Path)
{
  if (IsDriveLetter(Path))
    return etoupperw(Path[0])-'A';
  else
    return -1;
}

void AddEndSlash(std::wstring &Path)
{
  if (!Path.empty() && Path.back()!=CPATHDIVIDER)
    Path+=CPATHDIVIDER;
}

void MakeName(const std::wstring &Path,const std::wstring &Name,std::wstring &Pathname)
{

  std::wstring OutName=Path;

  if (!IsDriveLetter(Path) || Path.size()>2)
    AddEndSlash(OutName);
  OutName+=Name;
  Pathname=OutName;
}

void GetPathWithSep(const std::wstring &FullName,std::wstring &Path)
{
  if (std::addressof(FullName)!=std::addressof(Path))
    Path=FullName;
  Path.erase(GetNamePos(FullName));
}

void RemoveNameFromPath(std::wstring &Path)
{
  auto NamePos=GetNamePos(Path);
  if (NamePos>=2 && (!IsDriveDiv(Path[1]) || NamePos>=4))
    NamePos--;
  Path.erase(NamePos);
}

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool GetAppDataPath(std::wstring &Path,bool Create)
{
  LPMALLOC g_pMalloc;
  SHGetMalloc(&g_pMalloc);
  LPITEMIDLIST ppidl;
  Path.clear();
  bool Success=false;
  if (SHGetSpecialFolderLocation(NULL,CSIDL_APPDATA,&ppidl)==NOERROR &&
      SHGetPathStrFromIDList(ppidl,Path) && !Path.empty())
  {
    AddEndSlash(Path);
    Path+=L"WinRAR";
    Success=FileExist(Path);
    if (!Success && Create)
      Success=CreateDir(Path);
  }
  g_pMalloc->Free(ppidl);
  return Success;
}
#endif

#if defined(_WIN_ALL)
bool SHGetPathStrFromIDList(PCIDLIST_ABSOLUTE pidl,std::wstring &Path)
{
  std::vector<wchar> Buf(MAX_PATH);
  bool Success=SHGetPathFromIDList(pidl,Buf.data())!=FALSE;
  Path=Buf.data();
  return Success;
}
#endif

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
void GetRarDataPath(std::wstring &Path,bool Create)
{
  Path.clear();

  HKEY hKey;
  if (RegOpenKeyEx(HKEY_CURRENT_USER,L"Software\\WinRAR\\Paths",0,
                   KEY_QUERY_VALUE,&hKey)==ERROR_SUCCESS)
  {
    DWORD DataSize;
    LSTATUS Code=RegQueryValueEx(hKey,L"AppData",NULL,NULL,NULL,&DataSize);
    if (Code==ERROR_SUCCESS)
    {
      std::vector<wchar> PathBuf(DataSize/sizeof(wchar));
      RegQueryValueEx(hKey,L"AppData",0,NULL,(BYTE *)PathBuf.data(),&DataSize);
      Path=PathBuf.data();
      RegCloseKey(hKey);
    }
  }

  if (Path.empty() || !FileExist(Path))
    if (!GetAppDataPath(Path,Create))
    {
      Path=GetModuleFileStr();
      RemoveNameFromPath(Path);
    }
}
#endif

#ifndef SFX_MODULE
bool EnumConfigPaths(uint Number,std::wstring &Path,bool Create)
{
#ifdef _UNIX
  static const wchar *ConfPath[]={
    L"/etc", L"/etc/rar", L"/usr/lib", L"/usr/local/lib", L"/usr/local/etc"
  };
  if (Number==0)
  {
    char *EnvStr=getenv("HOME");
    if (EnvStr!=NULL)
      CharToWide(EnvStr,Path);
    else
      Path=ConfPath[0];
    return true;
  }
  Number--;
  if (Number>=ASIZE(ConfPath))
    return false;
  Path=ConfPath[Number];
  return true;
#elif defined(_WIN_ALL)
  if (Number>1)
    return false;
  if (Number==0)
    GetRarDataPath(Path,Create);
  else
  {
    Path=GetModuleFileStr();
    RemoveNameFromPath(Path);
  }
  return true;
#else
  return false;
#endif
}
#endif

#ifndef SFX_MODULE
void GetConfigName(const std::wstring &Name,std::wstring &FullName,bool CheckExist,bool Create)
{
  FullName.clear();
  for (uint I=0;;I++)
  {
    std::wstring ConfPath;
    if (!EnumConfigPaths(I,ConfPath,Create))
      break;
    MakeName(ConfPath,Name,FullName);
    if (!CheckExist || WildFileExist(FullName))
      break;
  }
}
#endif

size_t GetVolNumPos(const std::wstring &ArcName)
{

  size_t NamePos=GetNamePos(ArcName);

  if (NamePos==ArcName.size())
    return NamePos;

  size_t Pos=ArcName.size()-1;

  while (!IsDigit(ArcName[Pos]) && Pos>NamePos)
    Pos--;

  size_t NumPos=Pos;
  while (IsDigit(ArcName[NumPos]) && NumPos>NamePos)
    NumPos--;

  while (NumPos>NamePos && ArcName[NumPos]!='.')
  {
    if (IsDigit(ArcName[NumPos]))
    {

      auto DotPos=ArcName.find('.',NamePos);
      if (DotPos!=std::wstring::npos && DotPos<NumPos)
        Pos=NumPos;
      break;
    }
    NumPos--;
  }
  return Pos;
}

void NextVolumeName(std::wstring &ArcName,bool OldNumbering)
{
  auto DotPos=GetExtPos(ArcName);
  if (DotPos==std::wstring::npos)
  {
    ArcName+=L".rar";
    DotPos=GetExtPos(ArcName);
  }
  else
    if (DotPos+1==ArcName.size() || CmpExt(ArcName,L"exe") || CmpExt(ArcName,L"sfx"))
      SetExt(ArcName,L"rar");

  if (!OldNumbering)
  {
    size_t NumPos=GetVolNumPos(ArcName);

    while (++ArcName[NumPos]=='9'+1)
    {
      ArcName[NumPos]='0';
      if (NumPos==0)
        break;
      NumPos--;
      if (!IsDigit(ArcName[NumPos]))
      {

        ArcName.insert(NumPos+1,1,'1');
        break;
      }
    }
  }
  else
  {

    if (ArcName.size()-DotPos<3)
      ArcName.replace(DotPos+1,std::wstring::npos,L"rar");

    if (!IsDigit(ArcName[DotPos+2]) || !IsDigit(ArcName[DotPos+3]))
      ArcName.replace(DotPos+2,std::wstring::npos,L"00");
    else
    {
      auto NumPos=ArcName.size()-1;
      while (++ArcName[NumPos]=='9'+1)
        if (NumPos==0 || ArcName[NumPos-1]=='.')
        {
          ArcName[NumPos]='a';
          break;
        }
        else
          ArcName[NumPos--]='0';
    }
  }
}

bool IsNameUsable(const std::wstring &Name)
{

#ifdef _UNIX

  if (Name.find(':')!=std::wstring::npos)
    return false;
#else
  if (Name.find(':',2)!=std::wstring::npos)
    return false;
#endif
  for (size_t I=0;I<Name.size();I++)
  {
    if ((uint)Name[I]<32)
      return false;

#ifdef _UNIX

    if ((Name[I]==' ' || Name[I]=='.') && IsPathDiv(Name[I+1]))
      return false;
#endif
  }
  return !Name.empty() && Name.find_first_of(L"?*<>|\"")==std::wstring::npos;
}

void MakeNameUsable(std::wstring &Name,bool Extended)
{
  for (size_t I=0;I<Name.size();I++)
  {
    if (wcschr(Extended ? L"?*<>|\"":L"?*",Name[I])!=NULL ||
        Extended && (uint)Name[I]<32)
      Name[I]='_';
#ifdef _UNIX

    if (Extended)
    {

      if (Name[I]==':')
        Name[I]='_';

      if (IsPathDiv(Name[I+1]) && (Name[I]==' ' || Name[I]=='.' && I>0 &&
          !IsPathDiv(Name[I-1]) && (Name[I-1]!='.' || I>1 && !IsPathDiv(Name[I-2]))))
        Name[I]='_';
    }
#else
    if (I>1 && Name[I]==':')
      Name[I]='_';
#endif
  }
}

void UnixSlashToDos(const char *SrcName,char *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='/' ? '\\':SrcName[Copied];
  DestName[Copied]=0;
}

void UnixSlashToDos(const wchar *SrcName,wchar *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='/' ? '\\':SrcName[Copied];
  DestName[Copied]=0;
}

void UnixSlashToDos(const std::string &SrcName,std::string &DestName)
{

  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='/' ? '\\':SrcName[I];
}

void UnixSlashToDos(const std::wstring &SrcName,std::wstring &DestName)
{

  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='/' ? '\\':SrcName[I];
}

void DosSlashToUnix(const char *SrcName,char *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='\\' ? '/':SrcName[Copied];
  DestName[Copied]=0;
}

void DosSlashToUnix(const wchar *SrcName,wchar *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='\\' ? '/':SrcName[Copied];
  DestName[Copied]=0;
}

void DosSlashToUnix(const std::string &SrcName,std::string &DestName)
{

  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='\\' ? '/':SrcName[I];
}

void DosSlashToUnix(const std::wstring &SrcName,std::wstring &DestName)
{

  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='\\' ? '/':SrcName[I];
}

void ConvertNameToFull(const std::wstring &Src,std::wstring &Dest)
{
  if (Src.empty())
  {
    Dest.clear();
    return;
  }
#ifdef _WIN_ALL
  {
    DWORD Code=GetFullPathName(Src.c_str(),0,NULL,NULL);
    if (Code!=0)
    {
      std::vector<wchar> FullName(Code);
      Code=GetFullPathName(Src.c_str(),(DWORD)FullName.size(),FullName.data(),NULL);

      if (Code>0 && Code<=FullName.size())
      {
        Dest=FullName.data();
        return;
      }
    }

    std::wstring LongName;
    if (GetWinLongPath(Src,LongName))
    {
      Code=GetFullPathName(LongName.c_str(),0,NULL,NULL);
      if (Code!=0)
      {
        std::vector<wchar> FullName(Code);
        Code=GetFullPathName(LongName.c_str(),(DWORD)FullName.size(),FullName.data(),NULL);

        if (Code>0 && Code<=FullName.size())
        {
          Dest=FullName.data();
          return;
        }
      }
    }
    if (Src!=Dest)
      Dest=Src;
  }
#elif defined(_UNIX)
  if (IsFullPath(Src))
    Dest.clear();
  else
  {
    std::vector<char> CurDirA(MAXPATHSIZE);
    if (getcwd(CurDirA.data(),CurDirA.size())==NULL)
      CurDirA[0]=0;
    CharToWide(CurDirA.data(),Dest);
    AddEndSlash(Dest);
  }
  Dest+=Src;
#else
  Dest=Src;
#endif
}

bool IsFullPath(const std::wstring &Path)
{
#ifdef _WIN_ALL
  return Path.size()>=2 && Path[0]=='\\' && Path[1]=='\\' ||
         Path.size()>=3 && IsDriveLetter(Path) && IsPathDiv(Path[2]);
#else
  return Path.size()>=1 && IsPathDiv(Path[0]);
#endif
}

bool IsFullRootPath(const std::wstring &Path)
{
  return IsFullPath(Path) || IsPathDiv(Path[0]);
}

void GetPathRoot(const std::wstring &Path,std::wstring &Root)
{
  if (IsDriveLetter(Path))
    Root=Path.substr(0,2) + L"\\";
  else
    if (Path[0]=='\\' && Path[1]=='\\')
    {
      size_t Slash=Path.find('\\',2);
      if (Slash!=std::wstring::npos)
      {
        size_t Length;
        if ((Slash=Path.find('\\',Slash+1))!=std::wstring::npos)
          Length=Slash+1;
        else
          Length=Path.size();
        Root=Path.substr(0,Length);
      }
    }
    else
      Root.clear();
}

int ParseVersionFileName(std::wstring &Name,bool Truncate)
{
  int Version=0;
  auto VerPos=Name.rfind(';');
  if (VerPos!=std::wstring::npos && VerPos+1<Name.size())
  {
    Version=atoiw(&Name[VerPos+1]);
    if (Truncate)
      Name.erase(VerPos);
  }
  return Version;
}

#if !defined(SFX_MODULE)

size_t VolNameToFirstName(const std::wstring &VolName,std::wstring &FirstName,bool NewNumbering)
{

  std::wstring Name=VolName;
  size_t VolNumStart=0;
  if (NewNumbering)
  {
    wchar N='1';

    for (size_t Pos=GetVolNumPos(Name);Pos>0;Pos--)
      if (IsDigit(Name[Pos]))
      {
        Name[Pos]=N;
        N='0';
      }
      else
        if (N=='0')
        {
          VolNumStart=Pos+1;
          break;
        }
  }
  else
  {

    SetExt(Name,L"rar");
    VolNumStart=GetExtPos(Name);
  }
  if (!FileExist(Name))
  {

    std::wstring Mask=Name;
    SetExt(Mask,L"*");
    FindFile Find;
    Find.SetMask(Mask);
    FindData FD;
    while (Find.Next(&FD))
    {
      Archive Arc;
      if (Arc.Open(FD.Name,0) && Arc.IsArchive(true) && Arc.FirstVolume)
      {
        Name=FD.Name;
        break;
      }
    }
  }
  FirstName=Name;
  return VolNumStart;
}
#endif

#ifndef SFX_MODULE
static void GenArcName(std::wstring &ArcName,const std::wstring &GenerateMask,uint ArcNumber,bool &ArcNumPresent)
{
  size_t Pos=0;
  bool Prefix=false;
  if (GenerateMask[0]=='+')
  {
    Prefix=true;
    Pos++;
  }

  std::wstring Mask=!GenerateMask.empty() ? GenerateMask.substr(Pos):L"yyyymmddhhmmss";

  bool QuoteMode=false;
  uint MAsMinutes=0;
  for (uint I=0;I<Mask.size();I++)
  {
    if (Mask[I]=='{' || Mask[I]=='}')
    {
      QuoteMode=(Mask[I]=='{');
      continue;
    }
    if (QuoteMode)
      continue;
    int CurChar=toupperw(Mask[I]);
    if (CurChar=='H')
      MAsMinutes=2;
    if (CurChar=='D' || CurChar=='Y')
      MAsMinutes=0;

    if (MAsMinutes>0 && CurChar=='M')
    {

      Mask[I]='I';
      MAsMinutes--;
    }
    if (CurChar=='N')
    {
      uint Digits=GetDigits(ArcNumber);
      uint NCount=0;
      while (toupperw(Mask[I+NCount])=='N')
        NCount++;

      if (NCount<Digits)
        Mask.insert(I,Digits-NCount,L'N');
      I+=Max(Digits,NCount)-1;
      ArcNumPresent=true;
      continue;
    }
  }

  RarTime CurTime;
  CurTime.SetCurrentTime();
  RarLocalTime rlt;
  CurTime.GetLocal(&rlt);

  std::wstring Ext;
  auto ExtPos=GetExtPos(ArcName);
  if (ExtPos==std::wstring::npos)
    Ext=PointToName(ArcName).empty() ? L".rar":L"";
  else
  {
    Ext=ArcName.substr(ExtPos);
    ArcName.erase(ExtPos);
  }

  int WeekDay=rlt.wDay==0 ? 6:rlt.wDay-1;
  int StartWeekDay=rlt.yDay-WeekDay;
  if (StartWeekDay<0)
    if (StartWeekDay<=-4)
      StartWeekDay+=IsLeapYear(rlt.Year-1) ? 366:365;
    else
      StartWeekDay=0;
  int CurWeek=StartWeekDay/7+1;
  if (StartWeekDay%7>=4)
    CurWeek++;

  const size_t FieldSize=11;
  char Field[10][FieldSize];

  snprintf(Field[0],FieldSize,"%04u",rlt.Year);
  snprintf(Field[1],FieldSize,"%02u",rlt.Month);
  snprintf(Field[2],FieldSize,"%02u",rlt.Day);
  snprintf(Field[3],FieldSize,"%02u",rlt.Hour);
  snprintf(Field[4],FieldSize,"%02u",rlt.Minute);
  snprintf(Field[5],FieldSize,"%02u",rlt.Second);
  snprintf(Field[6],FieldSize,"%02u",(uint)CurWeek);
  snprintf(Field[7],FieldSize,"%u",(uint)WeekDay+1);
  snprintf(Field[8],FieldSize,"%03u",rlt.yDay+1);
  snprintf(Field[9],FieldSize,"%05u",ArcNumber);

  const wchar *MaskChars=L"YMDHISWAEN";

  int CField[sizeof(Field)/sizeof(Field[0])]{};

  QuoteMode=false;
  for (uint I=0;I<Mask.size();I++)
  {
    if (Mask[I]=='{' || Mask[I]=='}')
    {
      QuoteMode=(Mask[I]=='{');
      continue;
    }
    if (QuoteMode)
      continue;
    const wchar *ChPtr=wcschr(MaskChars,toupperw(Mask[I]));
    if (ChPtr!=NULL)
      CField[ChPtr-MaskChars]++;
   }

  wchar DateText[MAX_GENERATE_MASK];
  *DateText=0;
  QuoteMode=false;
  for (size_t I=0,J=0;I<Mask.size() && J<ASIZE(DateText)-1;I++)
  {
    if (Mask[I]=='{' || Mask[I]=='}')
    {
      QuoteMode=(Mask[I]=='{');
      continue;
    }
    const wchar *ChPtr=wcschr(MaskChars,toupperw(Mask[I]));
    if (ChPtr==NULL || QuoteMode)
    {
      DateText[J]=Mask[I];
#ifdef _WIN_ALL

      if (DateText[J]==':')
        DateText[J]='_';
#endif
    }
    else
    {
      size_t FieldPos=ChPtr-MaskChars;
      int CharPos=(int)strlen(Field[FieldPos])-CField[FieldPos]--;

      if (FieldPos==1 && CField[FieldPos]==2 &&
          toupperw(Mask[I+1])=='M' && toupperw(Mask[I+2])=='M')
      {
        wcsncpyz(DateText+J,GetMonthName(rlt.Month-1),ASIZE(DateText)-J);
        J=wcslen(DateText);
        I+=2;
        continue;
      }

      if (CharPos<0)
        DateText[J]=Mask[I];
      else
        DateText[J]=Field[FieldPos][CharPos];
    }
    DateText[++J]=0;
  }

  if (Prefix)
  {
    std::wstring NewName;
    GetPathWithSep(ArcName,NewName);
    NewName+=DateText;
    NewName+=PointToName(ArcName);
    ArcName=NewName;
  }
  else
    ArcName+=DateText;
  ArcName+=Ext;
}

void GenerateArchiveName(std::wstring &ArcName,const std::wstring &GenerateMask,bool Archiving)
{
  std::wstring NewName;

  uint ArcNumber=1;
  while (true)
  {
    NewName=ArcName;

    bool ArcNumPresent=false;

    GenArcName(NewName,GenerateMask,ArcNumber,ArcNumPresent);

    if (!ArcNumPresent)
      break;
    if (!FileExist(NewName))
    {
      if (!Archiving && ArcNumber>1)
      {

        NewName=ArcName;
        GenArcName(NewName,GenerateMask,ArcNumber-1,ArcNumPresent);
      }
      break;
    }
    ArcNumber++;
  }
  ArcName=NewName;
}
#endif

#ifdef _WIN_ALL

bool GetWinLongPath(const std::wstring &Src,std::wstring &Dest)
{
  if (Src.empty())
    return false;
  const std::wstring Prefix=L"\\\\?\\";

  bool FullPath=Src.size()>=3 && IsDriveLetter(Src) && IsPathDiv(Src[2]);
  if (IsFullPath(Src))
  {
    if (IsDriveLetter(Src))
    {
      Dest=Prefix+Src;
      return true;
    }
    else
      if (Src.size()>2 && Src[0]=='\\' && Src[1]=='\\')
      {
        Dest=Prefix+L"UNC"+Src.substr(1);
        return true;
      }

    return false;
  }
  else
  {
    std::wstring CurDir;
    if (!GetCurDir(CurDir))
      return false;

    if (IsPathDiv(Src[0]))
    {
      Dest=Prefix+CurDir[0]+L':'+Src;
      return true;
    }
    else
    {
      Dest=Prefix+CurDir;
      AddEndSlash(Dest);

      size_t Pos=0;
      if (Src[0]=='.' && IsPathDiv(Src[1]))
        Pos=2;

      Dest+=Src.substr(Pos);
      return true;
    }
  }
  return false;
}

void ConvertToPrecomposed(std::wstring &Name)
{
  if (WinNT()<WNT_VISTA)
    return;
  int Size=FoldString(MAP_PRECOMPOSED,Name.c_str(),-1,NULL,0);
  if (Size<=0)
    return;
  std::vector<wchar> FileName(Size);
  if (FoldString(MAP_PRECOMPOSED,Name.c_str(),-1,FileName.data(),(int)FileName.size())!=0)
    Name=FileName.data();
}

void MakeNameCompatible(std::wstring &Name)
{

  for (int I=0;I<(int)Name.size();I++)
    if (I+1==Name.size() || IsPathDiv(Name[I+1]))
      while (I>=0 && (Name[I]=='.' || Name[I]==' '))
      {
        if (I==0 && Name[I]==' ')
        {

          Name[I]='_';
          break;
        }
        if (Name[I]=='.')
        {

          if (I==0 || IsPathDiv(Name[I-1]) || I==2 && IsDriveLetter(Name))
            break;
          if (I>=1 && Name[I-1]=='.' && (I==1 || IsPathDiv(Name[I-2]) ||
              I==3 && IsDriveLetter(Name)))
            break;
        }
        Name.erase(I,1);
        I--;
      }

  for (size_t I=0;I<Name.size();I++)
    if (I==0 || I>0 && IsPathDiv(Name[I-1]))
    {
      static const wchar *Devices[]={L"CON",L"PRN",L"AUX",L"NUL",L"COM#",L"LPT#"};
      const wchar *s=&Name[I];
      bool MatchFound=false;
      for (uint J=0;J<ASIZE(Devices);J++)
        for (uint K=0;;K++)
          if (Devices[J][K]=='#')
          {
            if (!IsDigit(s[K]))
              break;
          }
          else
            if (Devices[J][K]==0)
            {

              MatchFound=s[K]==0 || s[K]=='.' && !IsWindows11OrGreater() || IsPathDiv(s[K]);
              break;
            }
            else
              if (Devices[J][K]!=toupperw(s[K]))
                break;
      if (MatchFound)
      {
        std::wstring OrigName=Name;
        Name.insert(I,1,'_');
#ifndef SFX_MODULE
        uiMsg(UIMSG_CORRECTINGNAME,nullptr);
        uiMsg(UIERROR_RENAMING,nullptr,OrigName,Name);
#endif
      }
    }
}
#endif

#ifdef _WIN_ALL
std::wstring GetModuleFileStr()
{
  HMODULE hModule=nullptr;

  std::vector<wchar> Path(256);
  while (Path.size()<=MAXPATHSIZE)
  {
    if (GetModuleFileName(hModule,Path.data(),(DWORD)Path.size())<Path.size())
      break;
    Path.resize(Path.size()*4);
  }
  return std::wstring(Path.data());
}

std::wstring GetProgramFile(const std::wstring &Name)
{
  std::wstring FullName=GetModuleFileStr();
  SetName(FullName,Name);
  return FullName;
}
#endif

#if defined(_WIN_ALL)
bool SetCurDir(const std::wstring &Dir)
{
  return SetCurrentDirectory(Dir.c_str())!=0;
}
#endif

#ifdef _WIN_ALL
bool GetCurDir(std::wstring &Dir)
{
  DWORD BufSize=GetCurrentDirectory(0,NULL);
  if (BufSize==0)
    return false;
  std::vector<wchar> Buf(BufSize);
  DWORD Code=GetCurrentDirectory((DWORD)Buf.size(),Buf.data());
  Dir=Buf.data();
  return Code!=0;
}
#endif

QuickOpen::QuickOpen()
{
  Buf=NULL;
  Init(NULL,false);
}

QuickOpen::~QuickOpen()
{
  Close();
  delete[] Buf;
}

void QuickOpen::Init(Archive *Arc,bool WriteMode)
{
  if (Arc!=NULL)
    Close();

  QuickOpen::Arc=Arc;
  QuickOpen::WriteMode=WriteMode;

  ListStart=NULL;
  ListEnd=NULL;

  if (Buf==NULL)
    Buf=new byte[MaxBufSize];

  CurBufSize=0;

  Loaded=false;
}

void QuickOpen::Close()
{
  QuickOpenItem *Item=ListStart;
  while (Item!=NULL)
  {
    QuickOpenItem *Next=Item->Next;
    delete[] Item->Header;
    delete Item;
    Item=Next;
  }
}

void QuickOpen::Load(uint64 BlockPos)
{
  if (!Loaded)
  {

    SeekPos=Arc->Tell();
    UnsyncSeekPos=false;

    int64 SavePos=SeekPos;
    Arc->Seek(BlockPos,SEEK_SET);

    Arc->SetProhibitQOpen(true);
    size_t ReadSize=Arc->ReadHeader();
    Arc->SetProhibitQOpen(false);

    if (ReadSize==0 || Arc->GetHeaderType()!=HEAD_SERVICE ||
        !Arc->SubHead.CmpName(SUBHEAD_TYPE_QOPEN))
    {
      Arc->Seek(SavePos,SEEK_SET);
      return;
    }
    QOHeaderPos=Arc->CurBlockPos;
    RawDataStart=Arc->Tell();
    RawDataSize=Arc->SubHead.UnpSize;
    Arc->Seek(SavePos,SEEK_SET);

    Loaded=true;
  }

  if (Arc->SubHead.Encrypted)
  {
    CommandData *Cmd=Arc->GetCommandData();
#ifndef RAR_NOCRYPT
    if (Cmd->Password.IsSet())
      Crypt.SetCryptKeys(false,CRYPT_RAR50,&Cmd->Password,Arc->SubHead.Salt,
                         Arc->SubHead.InitV,Arc->SubHead.Lg2Count,
                         Arc->SubHead.HashKey,Arc->SubHead.PswCheck);
    else
#endif
    {
      Loaded=false;
      return;
    }
  }

  RawDataPos=0;
  ReadBufSize=0;
  ReadBufPos=0;
  LastReadHeader.clear();
  LastReadHeaderPos=0;

  ReadBuffer();
}

bool QuickOpen::Read(void *Data,size_t Size,size_t &Result)
{
  if (!Loaded)
    return false;

  while (LastReadHeaderPos+LastReadHeader.size()<=SeekPos)
    if (!ReadNext())
      break;
  if (!Loaded)
  {

    if (UnsyncSeekPos)
      Arc->File::Seek(SeekPos,SEEK_SET);
    return false;
  }

  if (SeekPos>=LastReadHeaderPos && SeekPos+Size<=LastReadHeaderPos+LastReadHeader.size())
  {
    memcpy(Data,&LastReadHeader[size_t(SeekPos-LastReadHeaderPos)],Size);
    Result=Size;
    SeekPos+=Size;
    UnsyncSeekPos=true;
  }
  else
  {
    if (UnsyncSeekPos)
    {
      Arc->File::Seek(SeekPos,SEEK_SET);
      UnsyncSeekPos=false;
    }
    int ReadSize=Arc->File::Read(Data,Size);
    if (ReadSize<0)
    {
      Loaded=false;
      return false;
    }
    Result=ReadSize;
    SeekPos+=ReadSize;
  }

  return true;
}

bool QuickOpen::Seek(int64 Offset,int Method)
{
  if (!Loaded)
    return false;

  if (Method==SEEK_SET && (uint64)Offset<SeekPos && (uint64)Offset<LastReadHeaderPos)
    Load(QOHeaderPos);

  if (Method==SEEK_SET)
    SeekPos=Offset;
  if (Method==SEEK_CUR)
    SeekPos+=Offset;
  UnsyncSeekPos=true;

  if (Method==SEEK_END)
  {
    Arc->File::Seek(Offset,SEEK_END);
    SeekPos=Arc->File::Tell();
    UnsyncSeekPos=false;
  }
  return true;
}

bool QuickOpen::Tell(int64 *Pos)
{
  if (!Loaded)
    return false;
  *Pos=SeekPos;
  return true;
}

uint QuickOpen::ReadBuffer()
{
  int64 SavePos=Arc->Tell();
  Arc->File::Seek(RawDataStart+RawDataPos,SEEK_SET);
  size_t SizeToRead=(size_t)Min(RawDataSize-RawDataPos,MaxBufSize-ReadBufSize);
  if (Arc->SubHead.Encrypted)
    SizeToRead &= ~CRYPT_BLOCK_MASK;
  int ReadSize=0;
  if (SizeToRead!=0)
  {
    ReadSize=Arc->File::Read(Buf+ReadBufSize,SizeToRead);
    if (ReadSize<=0)
      ReadSize=0;
    else
    {
#ifndef RAR_NOCRYPT
      if (Arc->SubHead.Encrypted)
        Crypt.DecryptBlock(Buf+ReadBufSize,ReadSize & ~CRYPT_BLOCK_MASK);
#endif
      RawDataPos+=ReadSize;
      ReadBufSize+=ReadSize;
    }
  }
  Arc->Seek(SavePos,SEEK_SET);
  return ReadSize;
}

bool QuickOpen::ReadRaw(RawRead &Raw)
{
  if (MaxBufSize-ReadBufPos<0x100)
  {

    size_t DataLeft=ReadBufSize-ReadBufPos;
    memcpy(Buf,Buf+ReadBufPos,DataLeft);
    ReadBufPos=0;
    ReadBufSize=DataLeft;
    ReadBuffer();
  }
  const size_t FirstReadSize=7;
  if (ReadBufPos+FirstReadSize>ReadBufSize)
    return false;
  Raw.Read(Buf+ReadBufPos,FirstReadSize);
  ReadBufPos+=FirstReadSize;

  uint SavedCRC=Raw.Get4();
  uint SizeBytes=Raw.GetVSize(4);
  uint64 BlockSize=Raw.GetV();
  int SizeToRead=int(BlockSize);
  SizeToRead-=FirstReadSize-SizeBytes-4;
  if (SizeToRead<0 || SizeBytes==0 || BlockSize==0)
  {
    Loaded=false;
    return false;
  }

  while (SizeToRead>0)
  {
    size_t DataLeft=ReadBufSize-ReadBufPos;
    size_t CurSizeToRead=Min(DataLeft,(size_t)SizeToRead);
    Raw.Read(Buf+ReadBufPos,CurSizeToRead);
    ReadBufPos+=CurSizeToRead;
    SizeToRead-=int(CurSizeToRead);
    if (SizeToRead>0)
    {
      ReadBufPos=0;
      ReadBufSize=0;
      if (ReadBuffer()==0)
        return false;
    }
  }

  return SavedCRC==Raw.GetCRC50();
}

bool QuickOpen::ReadNext()
{
  RawRead Raw(NULL);
  if (!ReadRaw(Raw))
    return false;
  uint Flags=(uint)Raw.GetV();
  uint64 Offset=Raw.GetV();
  size_t HeaderSize=(size_t)Raw.GetV();
  if (HeaderSize>MAX_HEADER_SIZE_RAR5)
    return false;
  LastReadHeader.resize(HeaderSize);
  Raw.GetB(LastReadHeader.data(),HeaderSize);

  LastReadHeaderPos=QOHeaderPos-Offset;
  return true;
}

RarVM::RarVM()
{
  Mem=NULL;
}

RarVM::~RarVM()
{
  delete[] Mem;
}

void RarVM::Init()
{
  if (Mem==NULL)
    Mem=new byte[VM_MEMSIZE+4];
}

void RarVM::Execute(VM_PreparedProgram *Prg)
{
  memcpy(R,Prg->InitR,sizeof(Prg->InitR));
  Prg->FilteredData=NULL;
  if (Prg->Type!=VMSF_NONE)
  {
    bool Success=ExecuteStandardFilter(Prg->Type);
    uint BlockSize=Prg->InitR[4] & VM_MEMMASK;
    Prg->FilteredDataSize=BlockSize;
    if (Prg->Type==VMSF_DELTA || Prg->Type==VMSF_RGB || Prg->Type==VMSF_AUDIO)
      Prg->FilteredData=2*BlockSize>VM_MEMSIZE || !Success ? Mem:Mem+BlockSize;
    else
      Prg->FilteredData=Mem;
  }
}

void RarVM::Prepare(byte *Code,uint CodeSize,VM_PreparedProgram *Prg)
{

  byte XorSum=0;
  for (uint I=1;I<CodeSize;I++)
    XorSum^=Code[I];

  if (XorSum!=Code[0])
    return;

  struct StandardFilters
  {
    uint Length;
    uint CRC;
    VM_StandardFilters Type;
  } static StdList[]={
    53, 0xad576887, VMSF_E8,
    57, 0x3cd7e57e, VMSF_E8E9,
   120, 0x3769893f, VMSF_ITANIUM,
    29, 0x0e06077d, VMSF_DELTA,
   149, 0x1c2c5dc8, VMSF_RGB,
   216, 0xbc85e701, VMSF_AUDIO
  };
  uint CodeCRC=CRC32(0xffffffff,Code,CodeSize)^0xffffffff;
  for (uint I=0;I<ASIZE(StdList);I++)
    if (StdList[I].CRC==CodeCRC && StdList[I].Length==CodeSize)
    {
      Prg->Type=StdList[I].Type;
      break;
    }
}

uint RarVM::ReadData(BitInput &Inp)
{
  uint Data=Inp.fgetbits();
  switch(Data&0xc000)
  {
    case 0:
      Inp.faddbits(6);
      return (Data>>10)&0xf;
    case 0x4000:
      if ((Data&0x3c00)==0)
      {
        Data=0xffffff00|((Data>>2)&0xff);
        Inp.faddbits(14);
      }
      else
      {
        Data=(Data>>6)&0xff;
        Inp.faddbits(10);
      }
      return Data;
    case 0x8000:
      Inp.faddbits(2);
      Data=Inp.fgetbits();
      Inp.faddbits(16);
      return Data;
    default:
      Inp.faddbits(2);
      Data=(Inp.fgetbits()<<16);
      Inp.faddbits(16);
      Data|=Inp.fgetbits();
      Inp.faddbits(16);
      return Data;
  }
}

void RarVM::SetMemory(size_t Pos,byte *Data,size_t DataSize)
{
  if (Pos<VM_MEMSIZE && Data!=Mem+Pos)
  {

    size_t CopySize=Min(DataSize,VM_MEMSIZE-Pos);
    if (CopySize!=0)
      memmove(Mem+Pos,Data,CopySize);
  }
}

bool RarVM::ExecuteStandardFilter(VM_StandardFilters FilterType)
{
  switch(FilterType)
  {
    case VMSF_E8:
    case VMSF_E8E9:
      {
        byte *Data=Mem;
        uint DataSize=R[4],FileOffset=R[6];

        if (DataSize>VM_MEMSIZE || DataSize<4)
          return false;

        const uint FileSize=0x1000000;
        byte CmpByte2=FilterType==VMSF_E8E9 ? 0xe9:0xe8;
        for (uint CurPos=0;CurPos<DataSize-4;)
        {
          byte CurByte=*(Data++);
          CurPos++;
          if (CurByte==0xe8 || CurByte==CmpByte2)
          {
            uint Offset=CurPos+FileOffset;
            uint Addr=RawGet4(Data);

            if ((Addr & 0x80000000)!=0)
            {
              if (((Addr+Offset) & 0x80000000)==0)
                RawPut4(Addr+FileSize,Data);
            }
            else
              if (((Addr-FileSize) & 0x80000000)!=0)
                RawPut4(Addr-Offset,Data);
            Data+=4;
            CurPos+=4;
          }
        }
      }
      break;
    case VMSF_ITANIUM:
      {
        byte *Data=Mem;
        uint DataSize=R[4],FileOffset=R[6];

        if (DataSize>VM_MEMSIZE || DataSize<21)
          return false;

        uint CurPos=0;

        FileOffset>>=4;

        while (CurPos<DataSize-21)
        {
          int Byte=(Data[0]&0x1f)-0x10;
          if (Byte>=0)
          {
            static byte Masks[16]={4,4,6,6,0,0,7,7,4,4,0,0,4,4,0,0};
            byte CmdMask=Masks[Byte];
            if (CmdMask!=0)
              for (uint I=0;I<=2;I++)
                if (CmdMask & (1<<I))
                {
                  uint StartPos=I*41+5;
                  uint OpType=FilterItanium_GetBits(Data,StartPos+37,4);
                  if (OpType==5)
                  {
                    uint Offset=FilterItanium_GetBits(Data,StartPos+13,20);
                    FilterItanium_SetBits(Data,(Offset-FileOffset)&0xfffff,StartPos+13,20);
                  }
                }
          }
          Data+=16;
          CurPos+=16;
          FileOffset++;
        }
      }
      break;
    case VMSF_DELTA:
      {
        uint DataSize=R[4],Channels=R[0],SrcPos=0,Border=DataSize*2;
        if (DataSize>VM_MEMSIZE/2 || Channels>MAX3_UNPACK_CHANNELS || Channels==0)
          return false;

        for (uint CurChannel=0;CurChannel<Channels;CurChannel++)
        {
          byte PrevByte=0;
          for (uint DestPos=DataSize+CurChannel;DestPos<Border;DestPos+=Channels)
            Mem[DestPos]=(PrevByte-=Mem[SrcPos++]);
        }
      }
      break;
    case VMSF_RGB:
      {
        uint DataSize=R[4],Width=R[0]-3,PosR=R[1];
        if (DataSize>VM_MEMSIZE/2 || DataSize<3 || Width>DataSize || PosR>2)
          return false;
        byte *SrcData=Mem,*DestData=SrcData+DataSize;
        const uint Channels=3;
        for (uint CurChannel=0;CurChannel<Channels;CurChannel++)
        {
          uint PrevByte=0;

          for (uint I=CurChannel;I<DataSize;I+=Channels)
          {
            uint Predicted;
            if (I>=Width+3)
            {
              byte *UpperData=DestData+I-Width;
              uint UpperByte=*UpperData;
              uint UpperLeftByte=*(UpperData-3);
              Predicted=PrevByte+UpperByte-UpperLeftByte;
              int pa=abs((int)(Predicted-PrevByte));
              int pb=abs((int)(Predicted-UpperByte));
              int pc=abs((int)(Predicted-UpperLeftByte));
              if (pa<=pb && pa<=pc)
                Predicted=PrevByte;
              else
                if (pb<=pc)
                  Predicted=UpperByte;
                else
                  Predicted=UpperLeftByte;
            }
            else
              Predicted=PrevByte;
            PrevByte=DestData[I]=(byte)(Predicted-*(SrcData++));
          }
        }
        for (uint I=PosR,Border=DataSize-2;I<Border;I+=3)
        {
          byte G=DestData[I+1];
          DestData[I]+=G;
          DestData[I+2]+=G;
        }
      }
      break;
    case VMSF_AUDIO:
      {
        uint DataSize=R[4],Channels=R[0];
        byte *SrcData=Mem,*DestData=SrcData+DataSize;

        if (DataSize>VM_MEMSIZE/2 || Channels>128 || Channels==0)
          return false;
        for (uint CurChannel=0;CurChannel<Channels;CurChannel++)
        {
          uint PrevByte=0,PrevDelta=0,Dif[7];
          int D1=0,D2=0,D3;
          int K1=0,K2=0,K3=0;
          memset(Dif,0,sizeof(Dif));

          for (uint I=CurChannel,ByteCount=0;I<DataSize;I+=Channels,ByteCount++)
          {
            D3=D2;
            D2=PrevDelta-D1;
            D1=PrevDelta;

            uint Predicted=8*PrevByte+K1*D1+K2*D2+K3*D3;
            Predicted=(Predicted>>3) & 0xff;

            uint CurByte=*(SrcData++);

            Predicted-=CurByte;
            DestData[I]=Predicted;
            PrevDelta=(signed char)(Predicted-PrevByte);
            PrevByte=Predicted;

            int D=(signed char)CurByte;

            D=(uint)D<<3;

            Dif[0]+=abs(D);
            Dif[1]+=abs(D-D1);
            Dif[2]+=abs(D+D1);
            Dif[3]+=abs(D-D2);
            Dif[4]+=abs(D+D2);
            Dif[5]+=abs(D-D3);
            Dif[6]+=abs(D+D3);

            if ((ByteCount & 0x1f)==0)
            {
              uint MinDif=Dif[0],NumMinDif=0;
              Dif[0]=0;
              for (uint J=1;J<ASIZE(Dif);J++)
              {
                if (Dif[J]<MinDif)
                {
                  MinDif=Dif[J];
                  NumMinDif=J;
                }
                Dif[J]=0;
              }
              switch(NumMinDif)
              {
                case 1: if (K1>=-16) K1--; break;
                case 2: if (K1 < 16) K1++; break;
                case 3: if (K2>=-16) K2--; break;
                case 4: if (K2 < 16) K2++; break;
                case 5: if (K3>=-16) K3--; break;
                case 6: if (K3 < 16) K3++; break;
              }
            }
          }
        }
      }
      break;
  }
  return true;
}

uint RarVM::FilterItanium_GetBits(byte *Data,uint BitPos,uint BitCount)
{
  uint InAddr=BitPos/8;
  uint InBit=BitPos&7;
  uint BitField=(uint)Data[InAddr++];
  BitField|=(uint)Data[InAddr++] << 8;
  BitField|=(uint)Data[InAddr++] << 16;
  BitField|=(uint)Data[InAddr] << 24;
  BitField >>= InBit;
  return BitField & (0xffffffff>>(32-BitCount));
}

void RarVM::FilterItanium_SetBits(byte *Data,uint BitField,uint BitPos,uint BitCount)
{
  uint InAddr=BitPos/8;
  uint InBit=BitPos&7;
  uint AndMask=0xffffffff>>(32-BitCount);
  AndMask=~(AndMask<<InBit);

  BitField<<=InBit;

  for (uint I=0;I<4;I++)
  {
    Data[InAddr+I]&=AndMask;
    Data[InAddr+I]|=BitField;
    AndMask=(AndMask>>8)|0xff000000;
    BitField>>=8;
  }
}

RawRead::RawRead()
{
  RawRead::SrcFile=nullptr;
  Reset();
}

RawRead::RawRead(File *SrcFile)
{
  RawRead::SrcFile=SrcFile;
  Reset();
}

void RawRead::Reset()
{
  Data.clear();
  ReadPos=0;
  DataSize=0;
  Crypt=NULL;
}

size_t RawRead::Read(size_t Size)
{
  size_t ReadSize=0;
#if !defined(RAR_NOCRYPT)
  if (Crypt!=NULL)
  {

    size_t FullSize=Data.size();

    size_t DataLeft=FullSize-DataSize;

    if (Size>DataLeft)
    {
      size_t SizeToRead=Size-DataLeft;
      size_t AlignedReadSize=SizeToRead+((~SizeToRead+1) & CRYPT_BLOCK_MASK);
      Data.resize(FullSize+AlignedReadSize);
      ReadSize=SrcFile->Read(&Data[FullSize],AlignedReadSize);
      Crypt->DecryptBlock(&Data[FullSize],AlignedReadSize);
      DataSize+=ReadSize==0 ? 0:Size;
    }
    else
    {
      ReadSize=Size;
      DataSize+=Size;
    }
  }
  else
#endif
    if (Size!=0)
    {
      Data.resize(Data.size()+Size);
      ReadSize=SrcFile->Read(&Data[DataSize],Size);
      DataSize+=ReadSize;
    }
  return ReadSize;
}

void RawRead::Read(byte *SrcData,size_t Size)
{
  if (Size!=0)
  {
    Data.resize(Data.size()+Size);
    memcpy(&Data[DataSize],SrcData,Size);
    DataSize+=Size;
  }
}

byte RawRead::Get1()
{
  return ReadPos<DataSize ? Data[ReadPos++]:0;
}

ushort RawRead::Get2()
{
  if (ReadPos+1<DataSize)
  {
    ushort Result=Data[ReadPos]+(Data[ReadPos+1]<<8);
    ReadPos+=2;
    return Result;
  }
  return 0;
}

uint RawRead::Get4()
{
  if (ReadPos+3<DataSize)
  {
    uint Result=RawGet4(&Data[ReadPos]);
    ReadPos+=4;
    return Result;
  }
  return 0;
}

uint64 RawRead::Get8()
{
  uint Low=Get4(),High=Get4();
  return INT32TO64(High,Low);
}

uint64 RawRead::GetV()
{
  uint64 Result=0;

  for (uint Shift=0;ReadPos<DataSize && Shift<64;Shift+=7)
  {
    byte CurByte=Data[ReadPos++];
    Result+=uint64(CurByte & 0x7f)<<Shift;
    if ((CurByte & 0x80)==0)
      return Result;
  }
  return 0;
}

uint RawRead::GetVSize(size_t Pos)
{
  for (size_t CurPos=Pos;CurPos<DataSize;CurPos++)
    if ((Data[CurPos] & 0x80)==0)
      return int(CurPos-Pos+1);
  return 0;
}

size_t RawRead::GetB(void *Field,size_t Size)
{
  byte *F=(byte *)Field;
  size_t CopySize=Min(DataSize-ReadPos,Size);
  if (CopySize>0)
    memcpy(F,&Data[ReadPos],CopySize);
  if (Size>CopySize)
    memset(F+CopySize,0,Size-CopySize);
  ReadPos+=CopySize;
  return CopySize;
}

void RawRead::GetW(wchar *Field,size_t Size)
{
  if (ReadPos+2*Size-1<DataSize)
  {
    RawToWide(&Data[ReadPos],Field,Size);
    ReadPos+=sizeof(wchar)*Size;
  }
  else
    memset(Field,0,sizeof(wchar)*Size);
}

uint RawRead::GetCRC15(bool ProcessedOnly)
{
  if (DataSize<=2)
    return 0;
  uint HeaderCRC=CRC32(0xffffffff,&Data[2],(ProcessedOnly ? ReadPos:DataSize)-2);
  return ~HeaderCRC & 0xffff;
}

uint RawRead::GetCRC50()
{
  if (DataSize<=4)
    return 0xffffffff;
  return CRC32(0xffffffff,&Data[4],DataSize-4) ^ 0xffffffff;
}

uint64 RawGetV(const byte *Data,uint &ReadPos,uint DataSize,bool &Overflow)
{
  Overflow=false;
  uint64 Result=0;
  for (uint Shift=0;ReadPos<DataSize;Shift+=7)
  {
    byte CurByte=Data[ReadPos++];
    Result+=uint64(CurByte & 0x7f)<<Shift;
    if ((CurByte & 0x80)==0)
      return Result;
  }
  Overflow=true;
  return 0;
}

ComprDataIO::ComprDataIO()
{
#ifndef RAR_NOCRYPT
  Crypt=new CryptData;
  Decrypt=new CryptData;
#endif

  Init();
}

void ComprDataIO::Init()
{
  UnpackFromMemory=false;
  UnpackToMemory=false;
  UnpPackedSize=0;
  UnpPackedLeft=0;
  ShowProgress=true;
  TestMode=false;
  SkipUnpCRC=false;
  NoFileHeader=false;
  PackVolume=false;
  UnpVolume=false;
  NextVolumeMissing=false;
  SrcFile=NULL;
  DestFile=NULL;
  UnpWrAddr=NULL;
  UnpWrSize=0;
  Command=NULL;
  Encryption=false;
  Decryption=false;
  CurPackRead=CurPackWrite=CurUnpRead=CurUnpWrite=0;
  LastPercent=-1;
  SubHead=NULL;
  SubHeadPos=NULL;
  CurrentCommand=0;
  ProcessedArcSize=0;
  LastArcSize=0;
  TotalArcSize=0;
}

ComprDataIO::~ComprDataIO()
{
#ifndef RAR_NOCRYPT
  delete Crypt;
  delete Decrypt;
#endif
}

int ComprDataIO::UnpRead(byte *Addr,size_t Count)
{
#ifndef RAR_NOCRYPT

  if (Decryption)
    Count &= ~CRYPT_BLOCK_MASK;
#endif

  int ReadSize=0,TotalRead=0;
  byte *ReadAddr;
  ReadAddr=Addr;
  while (Count > 0)
  {
    Archive *SrcArc=(Archive *)SrcFile;

    if (UnpackFromMemory)
    {
      memcpy(Addr,UnpackFromMemoryAddr,UnpackFromMemorySize);
      ReadSize=(int)UnpackFromMemorySize;
      UnpackFromMemorySize=0;
    }
    else
    {
      size_t SizeToRead=((int64)Count>UnpPackedLeft) ? (size_t)UnpPackedLeft:Count;
      if (SizeToRead > 0)
      {
        if (UnpVolume && Decryption && (int64)Count>UnpPackedLeft)
        {

          size_t NewTotalRead = TotalRead + SizeToRead;
          size_t Adjust = NewTotalRead - (NewTotalRead  & ~CRYPT_BLOCK_MASK);
          size_t NewSizeToRead = SizeToRead - Adjust;
          if ((int)NewSizeToRead > 0)
            SizeToRead = NewSizeToRead;
        }

        if (!SrcFile->IsOpened())
          return -1;
        ReadSize=SrcFile->Read(ReadAddr,SizeToRead);
        FileHeader *hd=SubHead!=NULL ? SubHead:&SrcArc->FileHead;
        if (!NoFileHeader && hd->SplitAfter)
          PackedDataHash.Update(ReadAddr,ReadSize);
      }
    }
    CurUnpRead+=ReadSize;
    TotalRead+=ReadSize;
#ifndef NOVOLUME

    ReadAddr+=ReadSize;
    Count-=ReadSize;
#endif
    UnpPackedLeft-=ReadSize;

    if (UnpVolume && UnpPackedLeft == 0 &&
        (ReadSize==0 || Decryption && (TotalRead & CRYPT_BLOCK_MASK) != 0) )
    {
#ifndef NOVOLUME
      if (!MergeArchive(*SrcArc,this,true,CurrentCommand))
#endif
      {
        NextVolumeMissing=true;
        return -1;
      }
    }
    else
      break;
  }
  Archive *SrcArc=(Archive *)SrcFile;
  if (SrcArc!=NULL)
    ShowUnpRead(SrcArc->NextBlockPos-UnpPackedSize+CurUnpRead,TotalArcSize);
  if (ReadSize!=-1)
  {
    ReadSize=TotalRead;
#ifndef RAR_NOCRYPT
    if (Decryption)
      Decrypt->DecryptBlock(Addr,ReadSize);
#endif
  }
  Wait();
  return ReadSize;
}

void ComprDataIO::UnpWrite(byte *Addr,size_t Count)
{

#ifdef RARDLL
  CommandData *Cmd=((Archive *)SrcFile)->GetCommandData();
  if (Cmd->DllOpMode!=RAR_SKIP)
  {
    if (Cmd->Callback!=NULL &&
        Cmd->Callback(UCM_PROCESSDATA,Cmd->UserData,(LPARAM)Addr,Count)==-1)
      ErrHandler.Exit(RARX_USERBREAK);
    if (Cmd->ProcessDataProc!=NULL)
    {
      int RetCode=Cmd->ProcessDataProc(Addr,(int)Count);
      if (RetCode==0)
        ErrHandler.Exit(RARX_USERBREAK);
    }
  }
#endif

  UnpWrAddr=Addr;
  UnpWrSize=Count;
  if (UnpackToMemory)
  {
    if (Count <= UnpackToMemorySize)
    {
      memcpy(UnpackToMemoryAddr,Addr,Count);
      UnpackToMemoryAddr+=Count;
      UnpackToMemorySize-=Count;
    }
  }
  else
    if (!TestMode)
      DestFile->Write(Addr,Count);
  CurUnpWrite+=Count;
  if (!SkipUnpCRC)
    UnpHash.Update(Addr,Count);
  ShowUnpWrite();
  Wait();
}

void ComprDataIO::ShowUnpRead(int64 ArcPos,int64 ArcSize)
{
  if (ShowProgress && SrcFile!=NULL)
  {

    ArcPos+=ProcessedArcSize;

    Archive *SrcArc=(Archive *)SrcFile;
    CommandData *Cmd=SrcArc->GetCommandData();

    int CurPercent=ToPercent(ArcPos,ArcSize);
    if (!Cmd->DisablePercentage && CurPercent!=LastPercent)
    {
      uiExtractProgress(CurUnpWrite,SrcArc->FileHead.UnpSize,ArcPos,ArcSize);
      LastPercent=CurPercent;
    }
  }
}

void ComprDataIO::ShowUnpWrite()
{
}

void ComprDataIO::SetFiles(File *SrcFile,File *DestFile)
{
  if (SrcFile!=NULL)
    ComprDataIO::SrcFile=SrcFile;
  if (DestFile!=NULL)
    ComprDataIO::DestFile=DestFile;
  LastPercent=-1;
}

void ComprDataIO::GetUnpackedData(byte **Data,size_t *Size)
{
  *Data=UnpWrAddr;
  *Size=UnpWrSize;
}

bool ComprDataIO::SetEncryption(bool Encrypt,CRYPT_METHOD Method,
     SecPassword *Password,const byte *Salt,const byte *InitV,
     uint Lg2Cnt,byte *HashKey,byte *PswCheck)
{
#ifdef RAR_NOCRYPT
  return false;
#else
  if (Encrypt)
  {
    Encryption=Crypt->SetCryptKeys(true,Method,Password,Salt,InitV,Lg2Cnt,HashKey,PswCheck);
    return Encryption;
  }
  else
  {
    Decryption=Decrypt->SetCryptKeys(false,Method,Password,Salt,InitV,Lg2Cnt,HashKey,PswCheck);
    return Decryption;
  }
#endif
}

#if !defined(SFX_MODULE) && !defined(RAR_NOCRYPT)
void ComprDataIO::SetCmt13Encryption()
{
  Decryption=true;
  Decrypt->SetCmt13Encryption();
}
#endif

void ComprDataIO::SetUnpackToMemory(byte *Addr,uint Size)
{
  UnpackToMemory=true;
  UnpackToMemoryAddr=Addr;
  UnpackToMemorySize=Size;
}

void ComprDataIO::AdjustTotalArcSize(Archive *Arc)
{

  uint64 ArcLength=Arc->IsSeekable() ? Arc->FileLength() : 0;

  if (Arc->MainHead.QOpenOffset>0 && Arc->MainHead.QOpenOffset<ArcLength)
    LastArcSize=Arc->MainHead.QOpenOffset;
  else
    if (Arc->MainHead.RROffset>0 && Arc->MainHead.RROffset<ArcLength)
      LastArcSize=Arc->MainHead.RROffset;
    else
    {

      const uint EndBlock=23;

      if (ArcLength>EndBlock)
        LastArcSize=ArcLength-EndBlock;
    }

  TotalArcSize-=ArcLength-LastArcSize;
}

static const size_t TotalBufferSize=0x4000000;

class RSEncode
{
  private:
    RSCoder RSC;
  public:
    void EncodeBuf();
    void DecodeBuf();

    void Init(int RecVolNumber) {RSC.Init(RecVolNumber);}
    byte *Buf;
    byte *OutBuf;
    int BufStart;
    int BufEnd;
    int FileNumber;
    int RecVolNumber;
    size_t RecBufferSize;
    int *Erasures;
    int EraSize;
};

#ifdef RAR_SMP
THREAD_PROC(RSDecodeThread)
{
  RSEncode *rs=(RSEncode *)Data;
  rs->DecodeBuf();
}
#endif

RecVolumes3::RecVolumes3(CommandData *Cmd,bool TestOnly)
{
  memset(SrcFile,0,sizeof(SrcFile));
  if (TestOnly)
  {
#ifdef RAR_SMP
    RSThreadPool=NULL;
#endif
  }
  else
  {
    Buf.resize(TotalBufferSize);
#ifdef RAR_SMP
    RSThreadPool=new ThreadPool(Cmd->Threads);
#endif
  }
}

RecVolumes3::~RecVolumes3()
{
  for (size_t I=0;I<ASIZE(SrcFile);I++)
    delete SrcFile[I];
#ifdef RAR_SMP
  delete RSThreadPool;
#endif
}

static bool IsNewStyleRev(const std::wstring &Name)
{
  size_t ExtPos=GetExtPos(Name);
  if (ExtPos==std::wstring::npos || ExtPos==0)
    return true;
  int DigitGroup=0;
  for (ExtPos--;ExtPos>0;ExtPos--)
    if (!IsDigit(Name[ExtPos]))
      if (Name[ExtPos]=='_' && IsDigit(Name[ExtPos-1]))
        DigitGroup++;
      else
        break;
  return DigitGroup<2;
}

bool RecVolumes3::Restore(CommandData *Cmd,const std::wstring &Name,bool Silent)
{
  std::wstring ArcName=Name;
  bool NewStyle=false;
  bool RevName=CmpExt(ArcName,L"rev");
  if (RevName)
  {
    NewStyle=IsNewStyleRev(ArcName);

    size_t ExtPos=GetExtPos(ArcName);
    while (ExtPos>1 && (IsDigit(ArcName[ExtPos-1]) || ArcName[ExtPos-1]=='_'))
      ExtPos--;
    ArcName.replace(ExtPos,std::wstring::npos,L"*.*");

    FindFile Find;
    Find.SetMask(ArcName);
    FindData fd;
    while (Find.Next(&fd))
    {
      Archive Arc(Cmd);
      if (Arc.WOpen(fd.Name) && Arc.IsArchive(true))
      {
        ArcName=fd.Name;
        break;
      }
    }
  }

  Archive Arc(Cmd);
  if (!Arc.WCheckOpen(ArcName))
    return false;
  if (!Arc.Volume)
  {
    uiMsg(UIERROR_NOTVOLUME,ArcName);
    return false;
  }
  bool NewNumbering=Arc.NewNumbering;
  Arc.Close();

  size_t VolNumStart=VolNameToFirstName(ArcName,ArcName,NewNumbering);
  std::wstring RecVolMask=ArcName;
  RecVolMask.replace(VolNumStart,std::wstring::npos,L"*.rev");
  size_t BaseNamePartLength=VolNumStart;

  int64 RecFileSize=0;

  bool CalcCRCMessageDone=false;

  FindFile Find;
  Find.SetMask(RecVolMask);
  FindData RecData;
  int FileNumber=0,RecVolNumber=0,FoundRecVolumes=0,MissingVolumes=0;
  std::wstring PrevName;
  while (Find.Next(&RecData))
  {
    std::wstring CurName=RecData.Name;
    int P[3];
    if (!RevName && !NewStyle)
    {
      NewStyle=true;

      size_t DotPos=GetExtPos(CurName);
      if (DotPos!=std::wstring::npos)
      {
        uint LineCount=0;
        DotPos--;
        while (DotPos>0 && CurName[DotPos]!='.')
        {
          if (CurName[DotPos]=='_')
            LineCount++;
          DotPos--;
        }
        if (LineCount==2)
          NewStyle=false;
      }
    }
    if (NewStyle)
    {
      if (!CalcCRCMessageDone)
      {
        uiMsg(UIMSG_RECVOLCALCCHECKSUM);
        CalcCRCMessageDone=true;
      }

      uiMsg(UIMSG_STRING,CurName);

      File CurFile;
      CurFile.TOpen(CurName);
      CurFile.Seek(0,SEEK_END);
      int64 Length=CurFile.Tell();
      CurFile.Seek(Length-7,SEEK_SET);
      for (int I=0;I<3;I++)
        P[2-I]=CurFile.GetByte()+1;
      uint FileCRC=0;
      for (int I=0;I<4;I++)
        FileCRC|=CurFile.GetByte()<<(I*8);
      uint CalcCRC;
      CalcFileSum(&CurFile,&CalcCRC,NULL,Cmd->Threads,Length-4);
      if (FileCRC!=CalcCRC)
      {
        uiMsg(UIMSG_CHECKSUM,CurName);
        continue;
      }
    }
    else
    {
      size_t DotPos=GetExtPos(CurName);
      if (DotPos==std::wstring::npos)
        continue;
      bool WrongParam=false;
      for (size_t I=0;I<ASIZE(P);I++)
      {
        do
        {
          DotPos--;
        } while (IsDigit(CurName[DotPos]) && DotPos>=BaseNamePartLength);
        P[I]=atoiw(&CurName[DotPos+1]);
        if (P[I]==0 || P[I]>255)
          WrongParam=true;
      }
      if (WrongParam)
        continue;
    }
    if (P[0]<=0 || P[1]<=0 || P[2]<=0 || P[1]+P[2]>255 || P[0]+P[2]-1>255)
      continue;
    if (RecVolNumber!=0 && RecVolNumber!=P[1] || FileNumber!=0 && FileNumber!=P[2])
    {
      uiMsg(UIERROR_RECVOLDIFFSETS,CurName,PrevName);
      return false;
    }
    RecVolNumber=P[1];
    FileNumber=P[2];
    PrevName=CurName;
    File *NewFile=new File;
    NewFile->TOpen(CurName);

    int SrcPos=FileNumber+P[0]-1;
    if (SrcPos<0 || SrcPos>=ASIZE(SrcFile))
      continue;
    SrcFile[SrcPos]=NewFile;

    FoundRecVolumes++;

    if (RecFileSize==0)
      RecFileSize=NewFile->FileLength();
  }
  if (!Silent || FoundRecVolumes!=0)
    uiMsg(UIMSG_RECVOLFOUND,FoundRecVolumes);
  if (FoundRecVolumes==0)
    return false;

  bool WriteFlags[256]{};

  std::wstring LastVolName;

  for (int CurArcNum=0;CurArcNum<FileNumber;CurArcNum++)
  {
    Archive *NewFile=new Archive(Cmd);
    bool ValidVolume=FileExist(ArcName);
    if (ValidVolume)
    {
      NewFile->TOpen(ArcName);
      ValidVolume=NewFile->IsArchive(false);
      if (ValidVolume)
      {
        while (NewFile->ReadHeader()!=0)
        {
          if (NewFile->GetHeaderType()==HEAD_ENDARC)
          {
            uiMsg(UIMSG_STRING,ArcName);

            if (NewFile->EndArcHead.DataCRC)
            {
              uint CalcCRC;
              CalcFileSum(NewFile,&CalcCRC,NULL,Cmd->Threads,NewFile->CurBlockPos);
              if (NewFile->EndArcHead.ArcDataCRC!=CalcCRC)
              {
                ValidVolume=false;
                uiMsg(UIMSG_CHECKSUM,ArcName);
              }
            }
            break;
          }
          NewFile->SeekToNext();
        }
      }
      if (!ValidVolume)
      {
        NewFile->Close();
        std::wstring NewName=ArcName+L".bad";

        uiMsg(UIMSG_BADARCHIVE,ArcName);
        uiMsg(UIMSG_RENAMING,ArcName,NewName);
        RenameFile(ArcName,NewName);
      }
      NewFile->Seek(0,SEEK_SET);
    }
    if (!ValidVolume)
    {

      if (!NewFile->Create(ArcName,FMF_WRITE|FMF_SHAREREAD))
      {

        uiMsg(UIERROR_RECVOLFOUND,FoundRecVolumes);
        uiMsg(UIERROR_RECONSTRUCTING);
        ErrHandler.CreateErrorMsg(ArcName);
        return false;
      }

      WriteFlags[CurArcNum]=true;
      MissingVolumes++;

      if (CurArcNum==FileNumber-1)
        LastVolName=ArcName;

      uiMsg(UIMSG_MISSINGVOL,ArcName);
      uiMsg(UIEVENT_NEWARCHIVE,ArcName);
    }
    SrcFile[CurArcNum]=(File*)NewFile;
    NextVolumeName(ArcName,!NewNumbering);
  }

  uiMsg(UIMSG_RECVOLMISSING,MissingVolumes);

  if (MissingVolumes==0)
  {
    uiMsg(UIERROR_RECVOLALLEXIST);
    return false;
  }

  if (MissingVolumes>FoundRecVolumes)
  {
    uiMsg(UIERROR_RECVOLFOUND,FoundRecVolumes);
    uiMsg(UIERROR_RECVOLCANNOTFIX);
    return false;
  }

  uiMsg(UIMSG_RECONSTRUCTING);

  int TotalFiles=FileNumber+RecVolNumber;
  int Erasures[256],EraSize=0;

  for (int I=0;I<TotalFiles;I++)
    if (WriteFlags[I] || SrcFile[I]==NULL)
      Erasures[EraSize++]=I;

  int64 ProcessedSize=0;
  int LastPercent=-1;
  mprintf(L"     ");

  size_t RecBufferSize=TotalBufferSize/TotalFiles;

#ifdef RAR_SMP
  uint ThreadNumber=Cmd->Threads;
#else
  uint ThreadNumber=1;
#endif
  RSEncode *rse=new RSEncode[ThreadNumber];
  for (uint I=0;I<ThreadNumber;I++)
    rse[I].Init(RecVolNumber);

  while (true)
  {
    Wait();
    int MaxRead=0;
    for (int I=0;I<TotalFiles;I++)
      if (WriteFlags[I] || SrcFile[I]==NULL)
        memset(&Buf[I*RecBufferSize],0,RecBufferSize);
      else
      {
        int ReadSize=SrcFile[I]->Read(&Buf[I*RecBufferSize],RecBufferSize);
        if ((size_t)ReadSize!=RecBufferSize)
          memset(&Buf[I*RecBufferSize+ReadSize],0,RecBufferSize-ReadSize);
        if (ReadSize>MaxRead)
          MaxRead=ReadSize;
      }
    if (MaxRead==0)
      break;

    int CurPercent=ToPercent(ProcessedSize,RecFileSize);
    if (!Cmd->DisablePercentage && CurPercent!=LastPercent)
    {
      uiProcessProgress("RC",ProcessedSize,RecFileSize);
      LastPercent=CurPercent;
    }
    ProcessedSize+=MaxRead;

    int BlockStart=0;
    int BlockSize=MaxRead/ThreadNumber;
    if (BlockSize<0x100)
      BlockSize=MaxRead;

    for (uint CurThread=0;BlockStart<MaxRead;CurThread++)
    {

      if (CurThread==ThreadNumber-1)
        BlockSize=MaxRead-BlockStart;

      RSEncode *curenc=rse+CurThread;
      curenc->Buf=&Buf[0];
      curenc->BufStart=BlockStart;
      curenc->BufEnd=BlockStart+BlockSize;
      curenc->FileNumber=TotalFiles;
      curenc->RecBufferSize=RecBufferSize;
      curenc->Erasures=Erasures;
      curenc->EraSize=EraSize;

#ifdef RAR_SMP
      if (ThreadNumber>1)
        RSThreadPool->AddTask(RSDecodeThread,(void*)curenc);
      else
        curenc->DecodeBuf();
#else
      curenc->DecodeBuf();
#endif

      BlockStart+=BlockSize;
    }

#ifdef RAR_SMP
    RSThreadPool->WaitDone();
#endif

    for (int I=0;I<FileNumber;I++)
      if (WriteFlags[I])
        SrcFile[I]->Write(&Buf[I*RecBufferSize],MaxRead);
  }
  delete[] rse;

  for (int I=0;I<RecVolNumber+FileNumber;I++)
    if (SrcFile[I]!=NULL)
    {
      File *CurFile=SrcFile[I];
      if (NewStyle && WriteFlags[I])
      {
        int64 Length=CurFile->Tell();
        CurFile->Seek(Length-7,SEEK_SET);
        for (int J=0;J<7;J++)
          CurFile->PutByte(0);
      }
      CurFile->Close();
      SrcFile[I]=NULL;
    }
  if (!LastVolName.empty())
  {

    Archive Arc(Cmd);
    if (Arc.Open(LastVolName,FMF_UPDATE) && Arc.IsArchive(true) &&
        Arc.SearchBlock(HEAD_ENDARC))
    {
      Arc.Seek(Arc.NextBlockPos,SEEK_SET);
      char Buf[8192];
      int ReadSize=Arc.Read(Buf,sizeof(Buf));
      int ZeroCount=0;
      while (ZeroCount<ReadSize && Buf[ZeroCount]==0)
        ZeroCount++;
      if (ZeroCount==ReadSize)
      {
        Arc.Seek(Arc.NextBlockPos,SEEK_SET);
        Arc.Truncate();
      }
    }
  }
#if !defined(SILENT)
  if (!Cmd->DisablePercentage)
    mprintf(L"\b\b\b\b100%%");
  if (!Silent && !Cmd->DisableDone)
    mprintf(St(MDone));
#endif
  return true;
}

void RSEncode::DecodeBuf()
{
  for (int BufPos=BufStart;BufPos<BufEnd;BufPos++)
  {
    byte Data[256];
    for (int I=0;I<FileNumber;I++)
      Data[I]=Buf[I*RecBufferSize+BufPos];
    RSC.Decode(Data,FileNumber,Erasures,EraSize);
    for (int I=0;I<EraSize;I++)
      Buf[Erasures[I]*RecBufferSize+BufPos]=Data[Erasures[I]];
  }
}

void RecVolumes3::Test(CommandData *Cmd,const std::wstring &Name)
{
  if (!IsNewStyleRev(Name))
  {
    ErrHandler.UnknownMethodMsg(Name,Name);
    return;
  }

  std::wstring VolName=Name;

  while (FileExist(VolName))
  {
    File CurFile;
    if (!CurFile.Open(VolName))
    {
      ErrHandler.OpenErrorMsg(VolName);
      continue;
    }
    if (!uiStartFileExtract(VolName,false,true,false))
      return;
    mprintf(St(MExtrTestFile),VolName.c_str());
    mprintf(L"     ");
    CurFile.Seek(0,SEEK_END);
    int64 Length=CurFile.Tell();
    CurFile.Seek(Length-4,SEEK_SET);
    uint FileCRC=0;
    for (int I=0;I<4;I++)
      FileCRC|=CurFile.GetByte()<<(I*8);

    uint CalcCRC;
    CalcFileSum(&CurFile,&CalcCRC,NULL,1,Length-4,Cmd->DisablePercentage ? 0 : CALCFSUM_SHOWPROGRESS);
    if (FileCRC==CalcCRC)
    {
      mprintf(L"%s%s ",L"\b\b\b\b\b ",St(MOk));
    }
    else
    {
      uiMsg(UIERROR_CHECKSUM,VolName,VolName);
      ErrHandler.SetErrorCode(RARX_CRC);
    }

    NextVolumeName(VolName,false);
  }
}

static const uint MaxVolumes=65535;

#define MAX_REV_TO_DATA_RATIO 10

RecVolumes5::RecVolumes5(CommandData *Cmd,bool TestOnly)
{
  RealBuf=NULL;
  RealReadBuffer=NULL;

  DataCount=0;
  RecCount=0;
  TotalCount=0;
  RecBufferSize=0;

#ifdef RAR_SMP
  MaxUserThreads=Cmd->Threads;
#else
  MaxUserThreads=1;
#endif

  ThreadData=new RecRSThreadData[MaxUserThreads];
  for (uint I=0;I<MaxUserThreads;I++)
  {
    ThreadData[I].RecRSPtr=this;
    ThreadData[I].RS=NULL;
  }

  if (TestOnly)
  {
#ifdef RAR_SMP
    RecThreadPool=NULL;
#endif
  }
  else
  {
#ifdef RAR_SMP
    RecThreadPool=new ThreadPool(MaxUserThreads);
#endif
    RealBuf=new byte[TotalBufferSize+SSE_ALIGNMENT];
    Buf=(byte *)ALIGN_VALUE(RealBuf,SSE_ALIGNMENT);
  }
}

RecVolumes5::~RecVolumes5()
{
  delete[] RealBuf;
  delete[] RealReadBuffer;
  for (RecVolItem &Item : RecItems)
    delete Item.f;
  for (uint I=0;I<MaxUserThreads;I++)
    delete ThreadData[I].RS;
  delete[] ThreadData;
#ifdef RAR_SMP
  delete RecThreadPool;
#endif
}

#ifdef RAR_SMP
THREAD_PROC(RecThreadRS)
{
  RecRSThreadData *td=(RecRSThreadData *)Data;
  td->RecRSPtr->ProcessAreaRS(td);
}
#endif

void RecVolumes5::ProcessRS(CommandData *Cmd,uint DataNum,const byte *Data,uint MaxRead,bool Encode)
{

  uint ThreadNumber=MaxUserThreads;

  const uint MinThreadBlock=0x1000;
  ThreadNumber=Min(ThreadNumber,MaxRead/MinThreadBlock);

  if (ThreadNumber<1)
    ThreadNumber=1;
  uint ThreadDataSize=MaxRead/ThreadNumber;
  ThreadDataSize+=(ThreadDataSize&1);
#ifdef USE_SSE
  ThreadDataSize=ALIGN_VALUE(ThreadDataSize,SSE_ALIGNMENT);
#endif
  if (ThreadDataSize<MinThreadBlock)
    ThreadDataSize=MinThreadBlock;

  for (size_t I=0,CurPos=0;I<ThreadNumber && CurPos<MaxRead;I++)
  {
    RecRSThreadData *td=ThreadData+I;
    if (td->RS==NULL)
    {
      td->RS=new RSCoder16;
      td->RS->Init(DataCount,RecCount,Encode ? NULL:ValidFlags);
    }
    td->DataNum=DataNum;
    td->Data=Data;
    td->Encode=Encode;
    td->StartPos=CurPos;

    size_t EndPos=CurPos+ThreadDataSize;
    if (EndPos>MaxRead || I==ThreadNumber-1)
      EndPos=MaxRead;

    td->Size=EndPos-CurPos;

    CurPos=EndPos;

#ifdef RAR_SMP
    if (ThreadNumber>1)
      RecThreadPool->AddTask(RecThreadRS,(void*)td);
    else
      ProcessAreaRS(td);
#else
    ProcessAreaRS(td);
#endif
  }
#ifdef RAR_SMP
    RecThreadPool->WaitDone();
#endif
}

void RecVolumes5::ProcessAreaRS(RecRSThreadData *td)
{
  uint Count=td->Encode ? RecCount : MissingVolumes;
  for (uint I=0;I<Count;I++)
    td->RS->UpdateECC(td->DataNum, I, td->Data+td->StartPos, Buf+I*RecBufferSize+td->StartPos, td->Size);
}

bool RecVolumes5::Restore(CommandData *Cmd,const std::wstring &Name,bool Silent)
{
  std::wstring ArcName=Name;

  size_t NumPos=GetVolNumPos(ArcName);
  while (NumPos>0 && IsDigit(ArcName[NumPos-1]))
    NumPos--;
  if (NumPos<=GetNamePos(ArcName))
    return false;
  ArcName.replace(NumPos,std::wstring::npos,L"*.*");

  std::wstring FirstVolName;
  std::wstring LongestRevName;

  int64 RecFileSize=0;

  FindFile VolFind;
  VolFind.SetMask(ArcName);
  FindData fd;
  uint FoundRecVolumes=0;
  while (VolFind.Next(&fd))
  {
    Wait();

    Archive *Vol=new Archive(Cmd);
    int ItemPos=-1;
    if (!fd.IsDir && Vol->WOpen(fd.Name))
    {
      if (CmpExt(fd.Name,L"rev"))
      {
        uint RecNum=ReadHeader(Vol,FoundRecVolumes==0);
        if (RecNum!=0)
        {
          if (FoundRecVolumes==0)
            RecFileSize=Vol->FileLength();

          ItemPos=RecNum;
          FoundRecVolumes++;

          if (fd.Name.size()>LongestRevName.size())
            LongestRevName=fd.Name;
        }
      }
      else
        if (Vol->IsArchive(true) && (Vol->SFXSize>0 || CmpExt(fd.Name,L"rar")))
        {
          if (!Vol->Volume && !Vol->BrokenHeader)
          {
            uiMsg(UIERROR_NOTVOLUME,ArcName);
            return false;
          }

          Vol->QOpenUnload();

          Vol->Seek(0,SEEK_SET);

          size_t NumPos=GetVolNumPos(fd.Name);
          uint VolNum=0;
          for (uint K=1;(int)NumPos>=0 && IsDigit(fd.Name[NumPos]);K*=10,NumPos--)
            VolNum+=(fd.Name[NumPos]-'0')*K;
          if (VolNum==0 || VolNum>MaxVolumes)
            continue;
          size_t CurSize=RecItems.size();
          if (VolNum>CurSize)
          {
            RecItems.resize(VolNum);

          }
          ItemPos=VolNum-1;

          if (FirstVolName.empty())
            VolNameToFirstName(fd.Name,FirstVolName,true);
        }
    }
    if (ItemPos==-1)
      delete Vol;
    else
      if ((uint)ItemPos<RecItems.size())
      {

        RecVolItem *Item=&RecItems[ItemPos];
        Item->f=Vol;
        Item->New=false;
        Item->Name=fd.Name;
      }
  }

  if (!Silent || FoundRecVolumes!=0)
    uiMsg(UIMSG_RECVOLFOUND,FoundRecVolumes);
  if (FoundRecVolumes==0)
    return false;

  if (FirstVolName.empty())
  {
    SetExt(LongestRevName,L"rar");
    VolNameToFirstName(LongestRevName,FirstVolName,true);
  }

  uiMsg(UIMSG_RECVOLCALCCHECKSUM);

  MissingVolumes=0;
  for (uint I=0;I<TotalCount;I++)
  {
    RecVolItem *Item=&RecItems[I];
    if (Item->f!=NULL)
    {
      uiMsg(UIMSG_STRING,Item->Name);

      uint RevCRC;
      CalcFileSum(Item->f,&RevCRC,NULL,MaxUserThreads,INT64NDF,CALCFSUM_CURPOS);
      Item->Valid=RevCRC==Item->CRC;
      if (!Item->Valid)
      {
        uiMsg(UIMSG_CHECKSUM,Item->Name);

        if (I>=DataCount)
        {
          Item->f->Close();
          Item->f=NULL;
          FoundRecVolumes--;
        }
      }
    }
    if (I<DataCount && (Item->f==NULL || !Item->Valid))
      MissingVolumes++;
  }

  uiMsg(UIMSG_RECVOLMISSING,MissingVolumes);

  if (MissingVolumes==0)
  {
    uiMsg(UIERROR_RECVOLALLEXIST);
    return false;
  }

  if (MissingVolumes>FoundRecVolumes)
  {
    uiMsg(UIERROR_RECVOLFOUND,FoundRecVolumes);
    uiMsg(UIERROR_RECVOLCANNOTFIX);
    return false;
  }

  uiMsg(UIMSG_RECONSTRUCTING);

  uint64 MaxVolSize=0;
  for (uint I=0;I<DataCount;I++)
  {
    RecVolItem *Item=&RecItems[I];
    if (Item->FileSize>MaxVolSize)
      MaxVolSize=Item->FileSize;
    if (Item->f!=NULL && !Item->Valid)
    {
      Item->f->Close();

      std::wstring NewName;
      NewName=Item->Name+L".bad";

      uiMsg(UIMSG_BADARCHIVE,Item->Name);
      uiMsg(UIMSG_RENAMING,Item->Name,NewName);
      RenameFile(Item->Name,NewName);
      delete Item->f;
      Item->f=NULL;
    }

    if ((Item->New=(Item->f==NULL))==true)
    {
      Item->Name=FirstVolName;
      uiMsg(UIMSG_CREATING,Item->Name);
      uiMsg(UIEVENT_NEWARCHIVE,Item->Name);
      File *NewVol=new File;
      bool UserReject;
      if (!FileCreate(Cmd,NewVol,Item->Name,&UserReject))
      {
        if (!UserReject)
          ErrHandler.CreateErrorMsg(Item->Name);
        ErrHandler.Exit(UserReject ? RARX_USERBREAK:RARX_CREATE);
      }
      NewVol->Prealloc(Item->FileSize);
      Item->f=NewVol;
    }
    NextVolumeName(FirstVolName,false);
  }

  int64 ProcessedSize=0;
  int LastPercent=-1;
  mprintf(L"     ");

  MissingVolumes=0;

  ValidFlags=new bool[TotalCount];
  for (uint I=0;I<TotalCount;I++)
  {
    ValidFlags[I]=RecItems[I].f!=NULL && !RecItems[I].New;
    if (I<DataCount && !ValidFlags[I])
      MissingVolumes++;
  }

  RecBufferSize=TotalBufferSize/MissingVolumes;
  if ((RecBufferSize&1)==1)
    RecBufferSize--;
#ifdef USE_SSE
  RecBufferSize&=~(SSE_ALIGNMENT-1);
#endif

  RSCoder16 RS;
  if (!RS.Init(DataCount,RecCount,ValidFlags))
  {
    uiMsg(UIERROR_OPFAILED);
    delete[] ValidFlags;
    return false;
  }

  RealReadBuffer=new byte[RecBufferSize+SSE_ALIGNMENT];
  byte *ReadBuf=(byte *)ALIGN_VALUE(RealReadBuffer,SSE_ALIGNMENT);

  while (true)
  {
    Wait();

    int MaxRead=0;
    for (uint I=0,J=DataCount;I<DataCount;I++)
    {
      uint VolNum=I;
      if (!ValidFlags[I])
      {
        while (!ValidFlags[J])
          J++;
        VolNum=J++;
      }
      RecVolItem *Item=&RecItems[VolNum];

      byte *B=&ReadBuf[0];
      int ReadSize=0;
      if (Item->f!=NULL && !Item->New)
        ReadSize=Item->f->Read(B,RecBufferSize);
      if (ReadSize!=RecBufferSize)
        memset(B+ReadSize,0,RecBufferSize-ReadSize);
      if (ReadSize>MaxRead)
        MaxRead=ReadSize;

      uint DataToProcess=(uint)Min(RecBufferSize,MaxVolSize-ProcessedSize);
      ProcessRS(Cmd,I,B,DataToProcess,false);
    }
    if (MaxRead==0)
      break;

    for (uint I=0,J=0;I<DataCount;I++)
      if (!ValidFlags[I])
      {
        RecVolItem *Item=&RecItems[I];
        size_t WriteSize=(size_t)Min(MaxRead,Item->FileSize);
        Item->f->Write(Buf+(J++)*RecBufferSize,WriteSize);
        Item->FileSize-=WriteSize;
      }

    int CurPercent=ToPercent(ProcessedSize,RecFileSize);
    if (!Cmd->DisablePercentage && CurPercent!=LastPercent)
    {
      uiProcessProgress("RV",ProcessedSize,RecFileSize);
      LastPercent=CurPercent;
    }
    ProcessedSize+=MaxRead;
  }

  for (uint I=0;I<TotalCount;I++)
    if (RecItems[I].f!=NULL)
      RecItems[I].f->Close();

  delete[] ValidFlags;
#if !defined(SILENT)
  if (!Cmd->DisablePercentage)
    mprintf(L"\b\b\b\b100%%");
  if (!Silent && !Cmd->DisableDone)
    mprintf(St(MDone));
#endif
  return true;
}

uint RecVolumes5::ReadHeader(File *RecFile,bool FirstRev)
{
  const size_t FirstReadSize=REV5_SIGN_SIZE+8;
  byte ShortBuf[FirstReadSize];
  if (RecFile->Read(ShortBuf,FirstReadSize)!=FirstReadSize)
    return 0;
  if (memcmp(ShortBuf,REV5_SIGN,REV5_SIGN_SIZE)!=0)
    return 0;
  uint HeaderSize=RawGet4(ShortBuf+REV5_SIGN_SIZE+4);
  if (HeaderSize>0x100000 || HeaderSize<=5)
    return 0;
  uint BlockCRC=RawGet4(ShortBuf+REV5_SIGN_SIZE);

  RawRead Raw(RecFile);
  if (Raw.Read(HeaderSize)!=HeaderSize)
    return 0;

  uint CalcCRC=CRC32(0xffffffff,ShortBuf+REV5_SIGN_SIZE+4,4);
  if ((CRC32(CalcCRC,Raw.GetDataPtr(),HeaderSize)^0xffffffff)!=BlockCRC)
    return 0;

  if (Raw.Get1()!=1)
    return 0;
  DataCount=Raw.Get2();
  RecCount=Raw.Get2();
  TotalCount=DataCount+RecCount;
  uint RecNum=Raw.Get2();
  if (RecNum>=TotalCount || TotalCount>MaxVolumes)
    return 0;
  uint RevCRC=Raw.Get4();

  if (FirstRev)
  {

    size_t CurSize=RecItems.size();
    RecItems.resize(TotalCount);

    for (uint I=0;I<DataCount;I++)
    {
      RecItems[I].FileSize=Raw.Get8();
      RecItems[I].CRC=Raw.Get4();
    }
  }

  RecItems[RecNum].CRC=RevCRC;

  return RecNum;
}

void RecVolumes5::Test(CommandData *Cmd,const std::wstring &Name)
{
  std::wstring VolName=Name;

  uint FoundRecVolumes=0;
  while (FileExist(VolName))
  {
    File CurFile;
    if (!CurFile.Open(VolName))
    {
      ErrHandler.OpenErrorMsg(VolName);
      continue;
    }
    if (!uiStartFileExtract(VolName,false,true,false))
      return;
    mprintf(St(MExtrTestFile),VolName.c_str());
    mprintf(L"     ");
    bool Valid=false;
    uint RecNum=ReadHeader(&CurFile,FoundRecVolumes==0);
    if (RecNum!=0)
    {
      FoundRecVolumes++;

      uint RevCRC;
      CalcFileSum(&CurFile,&RevCRC,NULL,1,INT64NDF,CALCFSUM_CURPOS|(Cmd->DisablePercentage ? 0 : CALCFSUM_SHOWPROGRESS));
      Valid=RevCRC==RecItems[RecNum].CRC;
    }

    if (Valid)
    {
      mprintf(L"%s%s ",L"\b\b\b\b\b ",St(MOk));
    }
    else
    {
      uiMsg(UIERROR_CHECKSUM,VolName,VolName);
      ErrHandler.SetErrorCode(RARX_CRC);
    }

    NextVolumeName(VolName,false);
  }
}

bool RecVolumesRestore(CommandData *Cmd,const std::wstring &Name,bool Silent)
{
  Archive Arc(Cmd);
  if (!Arc.Open(Name))
  {
    if (!Silent)
      ErrHandler.OpenErrorMsg(Name);
    return false;
  }

  RARFORMAT Fmt=RARFMT15;
  if (Arc.IsArchive(true))
    Fmt=Arc.Format;
  else
  {
    byte Sign[REV5_SIGN_SIZE];
    Arc.Seek(0,SEEK_SET);
    if (Arc.Read(Sign,REV5_SIGN_SIZE)==REV5_SIGN_SIZE && memcmp(Sign,REV5_SIGN,REV5_SIGN_SIZE)==0)
      Fmt=RARFMT50;
  }
  Arc.Close();

  if (Fmt==RARFMT15)
  {
    RecVolumes3 RecVol(Cmd,false);
    return RecVol.Restore(Cmd,Name,Silent);
  }
  else
  {
    RecVolumes5 RecVol(Cmd,false);
    return RecVol.Restore(Cmd,Name,Silent);
  }
}

void RecVolumesTest(CommandData *Cmd,Archive *Arc,const std::wstring &Name)
{
  std::wstring RevName;
  if (Arc==NULL)
    RevName=Name;
  else
  {

    bool NewNumbering=Arc->NewNumbering;

    std::wstring RecVolMask;
    size_t VolNumStart=VolNameToFirstName(Name,RecVolMask,NewNumbering);
    RecVolMask.replace(VolNumStart, std::wstring::npos, L"*.rev");

    FindFile Find;
    Find.SetMask(RecVolMask);
    FindData RecData;

    while (Find.Next(&RecData))
    {
      size_t NumPos=GetVolNumPos(RecData.Name);
      if (RecData.Name[NumPos]!='1')
        continue;
      bool FirstVol=true;
      while (NumPos>0 && IsDigit(RecData.Name[--NumPos]))
        if (RecData.Name[NumPos]!='0')
        {
          FirstVol=false;
          break;
        }
      if (FirstVol)
      {
        RevName=RecData.Name;
        break;
      }
    }
    if (RevName.empty())
      return;
  }

  File RevFile;
  if (!RevFile.Open(RevName))
  {
    ErrHandler.OpenErrorMsg(RevName);
    return;
  }
  mprintf(L"\n");
  byte Sign[REV5_SIGN_SIZE];
  bool Rev5=RevFile.Read(Sign,REV5_SIGN_SIZE)==REV5_SIGN_SIZE && memcmp(Sign,REV5_SIGN,REV5_SIGN_SIZE)==0;
  RevFile.Close();
  if (Rev5)
  {
    RecVolumes5 RecVol(Cmd,true);
    RecVol.Test(Cmd,RevName);
  }
  else
  {
    RecVolumes3 RecVol(Cmd,true);
    RecVol.Test(Cmd,RevName);
  }
}

#ifdef USE_SSE
#include <wmmintrin.h>
#endif

static byte S[256]=
{
   99, 124, 119, 123, 242, 107, 111, 197,  48,   1, 103,  43, 254, 215, 171, 118,
  202, 130, 201, 125, 250,  89,  71, 240, 173, 212, 162, 175, 156, 164, 114, 192,
  183, 253, 147,  38,  54,  63, 247, 204,  52, 165, 229, 241, 113, 216,  49,  21,
    4, 199,  35, 195,  24, 150,   5, 154,   7,  18, 128, 226, 235,  39, 178, 117,
    9, 131,  44,  26,  27, 110,  90, 160,  82,  59, 214, 179,  41, 227,  47, 132,
   83, 209,   0, 237,  32, 252, 177,  91, 106, 203, 190,  57,  74,  76,  88, 207,
  208, 239, 170, 251,  67,  77,  51, 133,  69, 249,   2, 127,  80,  60, 159, 168,
   81, 163,  64, 143, 146, 157,  56, 245, 188, 182, 218,  33,  16, 255, 243, 210,
  205,  12,  19, 236,  95, 151,  68,  23, 196, 167, 126,  61, 100,  93,  25, 115,
   96, 129,  79, 220,  34,  42, 144, 136,  70, 238, 184,  20, 222,  94,  11, 219,
  224,  50,  58,  10,  73,   6,  36,  92, 194, 211, 172,  98, 145, 149, 228, 121,
  231, 200,  55, 109, 141, 213,  78, 169, 108,  86, 244, 234, 101, 122, 174,   8,
  186, 120,  37,  46,  28, 166, 180, 198, 232, 221, 116,  31,  75, 189, 139, 138,
  112,  62, 181, 102,  72,   3, 246,  14,  97,  53,  87, 185, 134, 193,  29, 158,
  225, 248, 152,  17, 105, 217, 142, 148, 155,  30, 135, 233, 206,  85,  40, 223,
  140, 161, 137,  13, 191, 230,  66, 104,  65, 153,  45,  15, 176,  84, 187,  22
};

static byte S5[256];

static byte rcon[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};

static byte T1[256][4],T2[256][4],T3[256][4],T4[256][4];
static byte T5[256][4],T6[256][4],T7[256][4],T8[256][4];
static byte U1[256][4],U2[256][4],U3[256][4],U4[256][4];

inline void Xor128(void *dest,const void *arg1,const void *arg2)
{
#ifdef ALLOW_MISALIGNED
  ((uint32*)dest)[0]=((uint32*)arg1)[0]^((uint32*)arg2)[0];
  ((uint32*)dest)[1]=((uint32*)arg1)[1]^((uint32*)arg2)[1];
  ((uint32*)dest)[2]=((uint32*)arg1)[2]^((uint32*)arg2)[2];
  ((uint32*)dest)[3]=((uint32*)arg1)[3]^((uint32*)arg2)[3];
#else
  for (int I=0;I<16;I++)
    ((byte*)dest)[I]=((byte*)arg1)[I]^((byte*)arg2)[I];
#endif
}

inline void Xor128(byte *dest,const byte *arg1,const byte *arg2,
                   const byte *arg3,const byte *arg4)
{
#ifdef ALLOW_MISALIGNED
  (*(uint32*)dest)=(*(uint32*)arg1)^(*(uint32*)arg2)^(*(uint32*)arg3)^(*(uint32*)arg4);
#else
  for (int I=0;I<4;I++)
    dest[I]=arg1[I]^arg2[I]^arg3[I]^arg4[I];
#endif
}

inline void Copy128(byte *dest,const byte *src)
{
#ifdef ALLOW_MISALIGNED
  ((uint32*)dest)[0]=((uint32*)src)[0];
  ((uint32*)dest)[1]=((uint32*)src)[1];
  ((uint32*)dest)[2]=((uint32*)src)[2];
  ((uint32*)dest)[3]=((uint32*)src)[3];
#else
  for (int I=0;I<16;I++)
    dest[I]=src[I];
#endif
}

Rijndael::Rijndael()
{
  if (S5[0]==0)
    GenerateTables();
  m_uRounds = 0;
  CBCMode = true;
#ifdef USE_SSE
  AES_NI=false;
#endif
#ifdef USE_NEON_AES
  AES_Neon=false;
#endif
}

void Rijndael::Init(bool Encrypt,const byte *key,uint keyLen,const byte * initVector)
{

#if defined(USE_SSE)

#ifdef _MSC_VER
  int CPUInfo[4];
  __cpuid(CPUInfo, 0);
  if (CPUInfo[0]>=1)
  {
    __cpuid(CPUInfo, 1);
    AES_NI=(CPUInfo[2] & 0x2000000)!=0;
  }
  else
    AES_NI=false;
#elif defined(__GNUC__)
  AES_NI=__builtin_cpu_supports("aes");
#endif

#elif defined(USE_NEON_AES)
  #ifdef _APPLE

    uint Value=0;
    size_t Size=sizeof(Value);
    int RetCode=sysctlbyname("hw.optional.arm.FEAT_AES",&Value,&Size,NULL,0);

    AES_Neon=RetCode!=0 || Value!=0;
  #else
    AES_Neon=(getauxval(AT_HWCAP) & HWCAP_AES)!=0;
  #endif
#endif

  uint uKeyLenInBytes=0;

  switch(keyLen)
  {
    case 128:
      uKeyLenInBytes = 16;
      m_uRounds = 10;
      break;
    case 192:
      uKeyLenInBytes = 24;
      m_uRounds = 12;
      break;
    case 256:
      uKeyLenInBytes = 32;
      m_uRounds = 14;
      break;
  }

  byte keyMatrix[_MAX_KEY_COLUMNS][4];

  for(uint i = 0; i < uKeyLenInBytes; i++)
    keyMatrix[i >> 2][i & 3] = key[i];

  if (initVector==NULL)
    memset(m_initVector, 0, sizeof(m_initVector));
  else
    for(int i = 0; i < MAX_IV_SIZE; i++)
      m_initVector[i] = initVector[i];

  keySched(keyMatrix);

  if(!Encrypt)
    keyEncToDec();
}

void Rijndael::blockEncrypt(const byte *input,size_t inputLen,byte *outBuffer)
{
  if (inputLen <= 0)
    return;

  size_t numBlocks = inputLen/16;
#if defined(USE_SSE)
  if (AES_NI)
  {
    blockEncryptSSE(input,numBlocks,outBuffer);
    return;
  }
#elif defined(USE_NEON_AES)
  if (AES_Neon)
  {
    blockEncryptNeon(input,numBlocks,outBuffer);
    return;
  }
#endif

  byte *prevBlock = m_initVector;
  for(size_t i = numBlocks;i > 0;i--)
  {
    byte block[16];
    if (CBCMode)
      Xor128(block,prevBlock,input);
    else
      Copy128(block,input);

    byte temp[4][4];

    Xor128(temp,block,m_expandedKey[0]);
    Xor128(outBuffer,   T1[temp[0][0]],T2[temp[1][1]],T3[temp[2][2]],T4[temp[3][3]]);
    Xor128(outBuffer+4, T1[temp[1][0]],T2[temp[2][1]],T3[temp[3][2]],T4[temp[0][3]]);
    Xor128(outBuffer+8, T1[temp[2][0]],T2[temp[3][1]],T3[temp[0][2]],T4[temp[1][3]]);
    Xor128(outBuffer+12,T1[temp[3][0]],T2[temp[0][1]],T3[temp[1][2]],T4[temp[2][3]]);

    for(int r = 1; r < m_uRounds-1; r++)
    {
      Xor128(temp,outBuffer,m_expandedKey[r]);
      Xor128(outBuffer,   T1[temp[0][0]],T2[temp[1][1]],T3[temp[2][2]],T4[temp[3][3]]);
      Xor128(outBuffer+4, T1[temp[1][0]],T2[temp[2][1]],T3[temp[3][2]],T4[temp[0][3]]);
      Xor128(outBuffer+8, T1[temp[2][0]],T2[temp[3][1]],T3[temp[0][2]],T4[temp[1][3]]);
      Xor128(outBuffer+12,T1[temp[3][0]],T2[temp[0][1]],T3[temp[1][2]],T4[temp[2][3]]);
    }
    Xor128(temp,outBuffer,m_expandedKey[m_uRounds-1]);
    outBuffer[ 0] = T1[temp[0][0]][1];
    outBuffer[ 1] = T1[temp[1][1]][1];
    outBuffer[ 2] = T1[temp[2][2]][1];
    outBuffer[ 3] = T1[temp[3][3]][1];
    outBuffer[ 4] = T1[temp[1][0]][1];
    outBuffer[ 5] = T1[temp[2][1]][1];
    outBuffer[ 6] = T1[temp[3][2]][1];
    outBuffer[ 7] = T1[temp[0][3]][1];
    outBuffer[ 8] = T1[temp[2][0]][1];
    outBuffer[ 9] = T1[temp[3][1]][1];
    outBuffer[10] = T1[temp[0][2]][1];
    outBuffer[11] = T1[temp[1][3]][1];
    outBuffer[12] = T1[temp[3][0]][1];
    outBuffer[13] = T1[temp[0][1]][1];
    outBuffer[14] = T1[temp[1][2]][1];
    outBuffer[15] = T1[temp[2][3]][1];
    Xor128(outBuffer,outBuffer,m_expandedKey[m_uRounds]);
    prevBlock=outBuffer;

    outBuffer += 16;
    input += 16;
  }
  Copy128(m_initVector,prevBlock);
}

#ifdef USE_SSE
void Rijndael::blockEncryptSSE(const byte *input,size_t numBlocks,byte *outBuffer)
{
  __m128i v = _mm_loadu_si128((__m128i*)m_initVector);
  __m128i *src=(__m128i*)input;
  __m128i *dest=(__m128i*)outBuffer;
  __m128i *rkey=(__m128i*)m_expandedKey;
  while (numBlocks > 0)
  {
    __m128i d = _mm_loadu_si128(src++);
    if (CBCMode)
      v = _mm_xor_si128(v, d);
    else
      v = d;
    __m128i r0 = _mm_loadu_si128(rkey);
    v = _mm_xor_si128(v, r0);

    for (int i=1; i<m_uRounds; i++)
    {
      __m128i ri = _mm_loadu_si128(rkey + i);
      v = _mm_aesenc_si128(v, ri);
    }

    __m128i rl = _mm_loadu_si128(rkey + m_uRounds);
    v = _mm_aesenclast_si128(v, rl);
    _mm_storeu_si128(dest++,v);
    numBlocks--;
  }
  _mm_storeu_si128((__m128i*)m_initVector,v);
}
#endif

#ifdef USE_NEON_AES
void Rijndael::blockEncryptNeon(const byte *input,size_t numBlocks,byte *outBuffer)
{
  byte *prevBlock = m_initVector;
  while (numBlocks > 0)
  {
    byte block[16];
    if (CBCMode)
      vst1q_u8(block, veorq_u8(vld1q_u8(prevBlock), vld1q_u8(input)));
    else
      vst1q_u8(block, vld1q_u8(input));

    uint8x16_t data = vld1q_u8(block);
    for (uint i = 0; i < m_uRounds-1; i++)
    {
      data = vaeseq_u8(data, vld1q_u8((byte *)m_expandedKey[i]));
      data = vaesmcq_u8(data);
    }
    data = vaeseq_u8(data, vld1q_u8((byte *)(m_expandedKey[m_uRounds-1])));
    data = veorq_u8(data, vld1q_u8((byte *)(m_expandedKey[m_uRounds])));
    vst1q_u8(outBuffer, data);

    prevBlock=outBuffer;

    outBuffer += 16;
    input += 16;
    numBlocks--;
  }
  vst1q_u8(m_initVector, vld1q_u8(prevBlock));
  return;
}
#endif

void Rijndael::blockDecrypt(const byte *input, size_t inputLen, byte *outBuffer)
{
  if (inputLen <= 0)
    return;

  size_t numBlocks=inputLen/16;
#if defined(USE_SSE)
  if (AES_NI)
  {
    blockDecryptSSE(input,numBlocks,outBuffer);
    return;
  }
#elif defined(USE_NEON_AES)
  if (AES_Neon)
  {
    blockDecryptNeon(input,numBlocks,outBuffer);
    return;
  }
#endif

  byte block[16], iv[4][4];
  memcpy(iv,m_initVector,16);

  for (size_t i = numBlocks; i > 0; i--)
  {
    byte temp[4][4];

    Xor128(temp,input,m_expandedKey[m_uRounds]);

    Xor128(block,   T5[temp[0][0]],T6[temp[3][1]],T7[temp[2][2]],T8[temp[1][3]]);
    Xor128(block+4, T5[temp[1][0]],T6[temp[0][1]],T7[temp[3][2]],T8[temp[2][3]]);
    Xor128(block+8, T5[temp[2][0]],T6[temp[1][1]],T7[temp[0][2]],T8[temp[3][3]]);
    Xor128(block+12,T5[temp[3][0]],T6[temp[2][1]],T7[temp[1][2]],T8[temp[0][3]]);

    for(int r = m_uRounds-1; r > 1; r--)
    {
      Xor128(temp,block,m_expandedKey[r]);
      Xor128(block,   T5[temp[0][0]],T6[temp[3][1]],T7[temp[2][2]],T8[temp[1][3]]);
      Xor128(block+4, T5[temp[1][0]],T6[temp[0][1]],T7[temp[3][2]],T8[temp[2][3]]);
      Xor128(block+8, T5[temp[2][0]],T6[temp[1][1]],T7[temp[0][2]],T8[temp[3][3]]);
      Xor128(block+12,T5[temp[3][0]],T6[temp[2][1]],T7[temp[1][2]],T8[temp[0][3]]);
    }

    Xor128(temp,block,m_expandedKey[1]);
    block[ 0] = S5[temp[0][0]];
    block[ 1] = S5[temp[3][1]];
    block[ 2] = S5[temp[2][2]];
    block[ 3] = S5[temp[1][3]];
    block[ 4] = S5[temp[1][0]];
    block[ 5] = S5[temp[0][1]];
    block[ 6] = S5[temp[3][2]];
    block[ 7] = S5[temp[2][3]];
    block[ 8] = S5[temp[2][0]];
    block[ 9] = S5[temp[1][1]];
    block[10] = S5[temp[0][2]];
    block[11] = S5[temp[3][3]];
    block[12] = S5[temp[3][0]];
    block[13] = S5[temp[2][1]];
    block[14] = S5[temp[1][2]];
    block[15] = S5[temp[0][3]];
    Xor128(block,block,m_expandedKey[0]);

    if (CBCMode)
      Xor128(block,block,iv);

    Copy128((byte*)iv,input);
    Copy128(outBuffer,block);

    input += 16;
    outBuffer += 16;
  }

  memcpy(m_initVector,iv,16);
}

#ifdef USE_SSE
void Rijndael::blockDecryptSSE(const byte *input, size_t numBlocks, byte *outBuffer)
{
  __m128i initVector = _mm_loadu_si128((__m128i*)m_initVector);
  __m128i *src=(__m128i*)input;
  __m128i *dest=(__m128i*)outBuffer;
  __m128i *rkey=(__m128i*)m_expandedKey;
  while (numBlocks > 0)
  {
    __m128i rl = _mm_loadu_si128(rkey + m_uRounds);
    __m128i d = _mm_loadu_si128(src++);
    __m128i v = _mm_xor_si128(rl, d);

    for (int i=m_uRounds-1; i>0; i--)
    {
      __m128i ri = _mm_loadu_si128(rkey + i);
      v = _mm_aesdec_si128(v, ri);
    }

    __m128i r0 = _mm_loadu_si128(rkey);
    v = _mm_aesdeclast_si128(v, r0);

    if (CBCMode)
      v = _mm_xor_si128(v, initVector);
    initVector = d;
    _mm_storeu_si128(dest++,v);
    numBlocks--;
  }
  _mm_storeu_si128((__m128i*)m_initVector,initVector);
}
#endif

#ifdef USE_NEON_AES
void Rijndael::blockDecryptNeon(const byte *input, size_t numBlocks, byte *outBuffer)
{
  byte iv[16];
  memcpy(iv,m_initVector,16);

  while (numBlocks > 0)
  {
    uint8x16_t data = vld1q_u8(input);

    for (int i=m_uRounds-1; i>0; i--)
    {
      data = vaesdq_u8(data, vld1q_u8((byte *)m_expandedKey[i+1]));
      data = vaesimcq_u8(data);
    }

    data = vaesdq_u8(data, vld1q_u8((byte *)m_expandedKey[1]));
    data = veorq_u8(data, vld1q_u8((byte *)m_expandedKey[0]));

    if (CBCMode)
      data = veorq_u8(data, vld1q_u8(iv));

    vst1q_u8(iv, vld1q_u8(input));
    vst1q_u8(outBuffer, data);

    input += 16;
    outBuffer += 16;
    numBlocks--;
  }

  memcpy(m_initVector,iv,16);
}
#endif

void Rijndael::keySched(byte key[_MAX_KEY_COLUMNS][4])
{
  int j,rconpointer = 0;

  int uKeyColumns = m_uRounds - 6;

  byte tempKey[_MAX_KEY_COLUMNS][4];

  memcpy(tempKey,key,sizeof(tempKey));

  int r = 0;
  int t = 0;

  for(j = 0;(j < uKeyColumns) && (r <= m_uRounds); )
  {
    for(;(j < uKeyColumns) && (t < 4); j++, t++)
      for (int k=0;k<4;k++)
        m_expandedKey[r][t][k]=tempKey[j][k];

    if(t == 4)
    {
      r++;
      t = 0;
    }
  }

  while(r <= m_uRounds)
  {
    tempKey[0][0] ^= S[tempKey[uKeyColumns-1][1]];
    tempKey[0][1] ^= S[tempKey[uKeyColumns-1][2]];
    tempKey[0][2] ^= S[tempKey[uKeyColumns-1][3]];
    tempKey[0][3] ^= S[tempKey[uKeyColumns-1][0]];
    tempKey[0][0] ^= rcon[rconpointer++];

    if (uKeyColumns != 8)
      for(j = 1; j < uKeyColumns; j++)
        for (int k=0;k<4;k++)
          tempKey[j][k] ^= tempKey[j-1][k];
    else
    {
      for(j = 1; j < uKeyColumns/2; j++)
        for (int k=0;k<4;k++)
          tempKey[j][k] ^= tempKey[j-1][k];

      tempKey[uKeyColumns/2][0] ^= S[tempKey[uKeyColumns/2 - 1][0]];
      tempKey[uKeyColumns/2][1] ^= S[tempKey[uKeyColumns/2 - 1][1]];
      tempKey[uKeyColumns/2][2] ^= S[tempKey[uKeyColumns/2 - 1][2]];
      tempKey[uKeyColumns/2][3] ^= S[tempKey[uKeyColumns/2 - 1][3]];
      for(j = uKeyColumns/2 + 1; j < uKeyColumns; j++)
        for (int k=0;k<4;k++)
          tempKey[j][k] ^= tempKey[j-1][k];
    }
    for(j = 0; (j < uKeyColumns) && (r <= m_uRounds); )
    {
      for(; (j < uKeyColumns) && (t < 4); j++, t++)
        for (int k=0;k<4;k++)
          m_expandedKey[r][t][k] = tempKey[j][k];
      if(t == 4)
      {
        r++;
        t = 0;
      }
    }
  }
}

void Rijndael::keyEncToDec()
{
  for(int r = 1; r < m_uRounds; r++)
  {
    byte n_expandedKey[4][4];
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
      {
        byte *w=m_expandedKey[r][j];
        n_expandedKey[j][i]=U1[w[0]][i]^U2[w[1]][i]^U3[w[2]][i]^U4[w[3]][i];
      }
    memcpy(m_expandedKey[r],n_expandedKey,sizeof(m_expandedKey[0]));
  }
}

static byte gmul(byte a, byte b)
{
  const byte poly=0x1b;
  byte result = 0;
  while (b>0)
  {
    if ((b & 1) != 0)
      result ^= a;
    a = (a & 0x80) ? (a<<1)^poly : a<<1;
    b >>= 1;
  }
  return result;
}

void Rijndael::GenerateTables()
{
  for (int I=0;I<256;I++)
    S5[S[I]]=I;

  for (int I=0;I<256;I++)
  {
    byte s=S[I];
    T1[I][1]=T1[I][2]=T2[I][2]=T2[I][3]=T3[I][0]=T3[I][3]=T4[I][0]=T4[I][1]=s;
    T1[I][0]=T2[I][1]=T3[I][2]=T4[I][3]=gmul(s,2);
    T1[I][3]=T2[I][0]=T3[I][1]=T4[I][2]=gmul(s,3);

    byte b=S5[I];
    U1[b][3]=U2[b][0]=U3[b][1]=U4[b][2]=T5[I][3]=T6[I][0]=T7[I][1]=T8[I][2]=gmul(b,0xb);
    U1[b][1]=U2[b][2]=U3[b][3]=U4[b][0]=T5[I][1]=T6[I][2]=T7[I][3]=T8[I][0]=gmul(b,0x9);
    U1[b][2]=U2[b][3]=U3[b][0]=U4[b][1]=T5[I][2]=T6[I][3]=T7[I][0]=T8[I][1]=gmul(b,0xd);
    U1[b][0]=U2[b][1]=U3[b][2]=U4[b][3]=T5[I][0]=T6[I][1]=T7[I][2]=T8[I][3]=gmul(b,0xe);
  }
}

#if 0
static void TestRijndael();
struct TestRij {TestRij() {TestRijndael();exit(0);}} GlobalTestRij;

void TestRijndael()
{
  byte IV[16]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
  byte PT[64]={
    0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
    0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51,
    0x30,0xc8,0x1c,0x46,0xa3,0x5c,0xe4,0x11,0xe5,0xfb,0xc1,0x19,0x1a,0x0a,0x52,0xef,
    0xf6,0x9f,0x24,0x45,0xdf,0x4f,0x9b,0x17,0xad,0x2b,0x41,0x7b,0xe6,0x6c,0x37,0x10,
  };

  byte Key128[16]={0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  byte Chk128[16]={0x3f,0xf1,0xca,0xa1,0x68,0x1f,0xac,0x09,0x12,0x0e,0xca,0x30,0x75,0x86,0xe1,0xa7};
  byte Key192[24]={0x8e,0x73,0xb0,0xf7,0xda,0x0e,0x64,0x52,0xc8,0x10,0xf3,0x2b,0x80,0x90,0x79,0xe5,0x62,0xf8,0xea,0xd2,0x52,0x2c,0x6b,0x7b};
  byte Chk192[16]={0x08,0xb0,0xe2,0x79,0x88,0x59,0x88,0x81,0xd9,0x20,0xa9,0xe6,0x4f,0x56,0x15,0xcd};
  byte Key256[32]={0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4};
  byte Chk256[16]={0xb2,0xeb,0x05,0xe2,0xc3,0x9b,0xe9,0xfc,0xda,0x6c,0x19,0x07,0x8c,0x6a,0x9d,0x1b};
  byte *Key[3]={Key128,Key192,Key256};
  byte *Chk[3]={Chk128,Chk192,Chk256};

  Rijndael rij;
  for (uint L=0;L<3;L++)
  {
    byte Out[16];
    std::wstring Str;

    uint KeyLength=128+L*64;
    rij.Init(true,Key[L],KeyLength,IV);
    for (uint I=0;I<sizeof(PT);I+=16)
      rij.blockEncrypt(PT+I,16,Out);
    BinToHex(Chk[L],16,Str);
    mprintf(L"\nAES-%d expected: %s",KeyLength,Str.c_str());
    BinToHex(Out,sizeof(Out),Str);
    mprintf(L"\nAES-%d result:   %s",KeyLength,Str.c_str());
    if (memcmp(Out,Chk[L],16)==0)
      mprintf(L" OK");
    else
    {
      mprintf(L" FAILED");
      getchar();
    }
  }
}
#endif

#define Clean(D,S)  {for (int I=0;I<(S);I++) (D)[I]=0;}

void RSCoder::Init(int ParSize)
{
  RSCoder::ParSize=ParSize;
  FirstBlockDone=false;
  gfInit();
  pnInit();
}

void RSCoder::gfInit()
{
  for (int I=0,J=1;I<MAXPAR;I++)
  {
    gfLog[J]=I;
    gfExp[I]=J;
    J<<=1;
    if (J > MAXPAR)
      J^=0x11D;
  }
  for (int I=MAXPAR;I<MAXPOL;I++)
    gfExp[I]=gfExp[I-MAXPAR];
}

inline int RSCoder::gfMult(int a,int b)
{
  return(a==0 || b == 0 ? 0:gfExp[gfLog[a]+gfLog[b]]);
}

void RSCoder::pnInit()
{
  int p2[MAXPAR+1];

  Clean(p2,ParSize);
  p2[0]=1;

  for (int I=1;I<=ParSize;I++)
  {
    int p1[MAXPAR+1];
    Clean(p1,ParSize);
    p1[0]=gfExp[I];
    p1[1]=1;

    pnMult(p1,p2,GXPol);

    for (int J=0;J<ParSize;J++)
      p2[J]=GXPol[J];
  }
}

void RSCoder::pnMult(int *p1,int *p2,int *r)
{
  Clean(r,ParSize);
  for (int I=0;I<ParSize;I++)
    if (p1[I]!=0)
      for(int J=0;J<ParSize-I;J++)
        r[I+J]^=gfMult(p1[I],p2[J]);
}

void RSCoder::Encode(byte *Data,int DataSize,byte *DestData)
{
  int ShiftReg[MAXPAR+1];

  Clean(ShiftReg,ParSize+1);
  for (int I=0;I<DataSize;I++)
  {
    int D=Data[I]^ShiftReg[ParSize-1];

    for (int J=ParSize-1;J>0;J--)
      ShiftReg[J]=ShiftReg[J-1]^gfMult(GXPol[J],D);
    ShiftReg[0]=gfMult(GXPol[0],D);
  }
  for (int I=0;I<ParSize;I++)
    DestData[I]=ShiftReg[ParSize-I-1];
}

bool RSCoder::Decode(byte *Data,int DataSize,int *EraLoc,int EraSize)
{
  int SynData[MAXPOL];

  bool AllZeroes=true;
  for (int I=0;I<ParSize;I++)
  {
    int Sum=0;
    for (int J=0;J<DataSize;J++)
      Sum=Data[J]^gfMult(gfExp[I+1],Sum);
    if ((SynData[I]=Sum)!=0)
      AllZeroes=false;
  }

  if (AllZeroes)
    return(true);

  if (!FirstBlockDone)
  {
    FirstBlockDone=true;

    Clean(ELPol,ParSize+1);
    ELPol[0]=1;

    for (int EraPos=0;EraPos<EraSize;EraPos++)
      for (int I=ParSize,M=gfExp[DataSize-EraLoc[EraPos]-1];I>0;I--)
        ELPol[I]^=gfMult(M,ELPol[I-1]);

    ErrCount=0;

    for (int Root=MAXPAR-DataSize;Root<MAXPAR+1;Root++)
    {
      int Sum=0;
      for (int B=0;B<ParSize+1;B++)
        Sum^=gfMult(gfExp[(B*Root)%MAXPAR],ELPol[B]);
      if (Sum==0)
      {
        ErrorLocs[ErrCount]=MAXPAR-Root;

        Dnm[ErrCount]=0;
        for (int I=1;I<ParSize+1;I+=2)
          Dnm[ErrCount]^= gfMult(ELPol[I],gfExp[Root*(I-1)%MAXPAR]);

        ErrCount++;
      }
    }
  }

  int EEPol[MAXPOL];
  pnMult(ELPol,SynData,EEPol);

  if ((ErrCount<=ParSize) && ErrCount>0)
    for (int I=0;I<ErrCount;I++)
    {
      int Loc=ErrorLocs[I],DLoc=MAXPAR-Loc,N=0;
      for (int J=0;J<ParSize;J++)
        N^=gfMult(EEPol[J],gfExp[DLoc*J%MAXPAR]);
      int DataPos=DataSize-Loc-1;

      if (DataPos>=0 && DataPos<DataSize)
        Data[DataPos]^=gfMult(N,gfExp[MAXPAR-gfLog[Dnm[I]]]);
    }
  return(ErrCount<=ParSize);
}

#undef Clean

RSCoder16::RSCoder16()
{
  Decoding=false;
  ND=NR=NE=0;
  ValidFlags=NULL;
  MX=NULL;
  DataLog=NULL;
  DataLogSize=0;

  gfInit();
}

RSCoder16::~RSCoder16()
{
  delete[] gfExp;
  delete[] gfLog;
  delete[] DataLog;
  delete[] MX;
  delete[] ValidFlags;
}

void RSCoder16::gfInit()
{
  gfExp=new uint[4*gfSize+1];
  gfLog=new uint[gfSize+1];

  for (uint L=0,E=1;L<gfSize;L++)
  {
    gfLog[E]=L;
    gfExp[L]=E;
    gfExp[L+gfSize]=E;
    E<<=1;
    if (E>gfSize)
      E^=0x1100B;
  }

  gfLog[0]= 2*gfSize;
  for (uint I=2*gfSize;I<=4*gfSize;I++)
    gfExp[I]=0;
}

uint RSCoder16::gfAdd(uint a,uint b)
{
  return a^b;
}

uint RSCoder16::gfMul(uint a,uint b)
{
  return gfExp[gfLog[a]+gfLog[b]];
}

uint RSCoder16::gfInv(uint a)
{
  return a==0 ? 0:gfExp[gfSize-gfLog[a]];
}

bool RSCoder16::Init(uint DataCount, uint RecCount, bool *ValidityFlags)
{
  ND = DataCount;
  NR = RecCount;
  NE = 0;

  Decoding=ValidityFlags!=NULL;
  if (Decoding)
  {
    delete[] ValidFlags;
    ValidFlags=new bool[ND + NR];

    for (uint I = 0; I < ND + NR; I++)
      ValidFlags[I]=ValidityFlags[I];
    for (uint I = 0; I < ND; I++)
      if (!ValidFlags[I])
        NE++;
    uint ValidECC=0;
    for (uint I = ND; I < ND + NR; I++)
      if (ValidFlags[I])
        ValidECC++;
    if (NE > ValidECC || NE == 0 || ValidECC == 0)
      return false;
  }

  if (ND + NR > gfSize ||  ND == 0 || NR == 0)
    return false;

  delete[] MX;
  if (Decoding)
  {
    MX=new uint[NE * ND];
    MakeDecoderMatrix();
    InvertDecoderMatrix();
  }
  else
  {
    MX=new uint[NR * ND];
    MakeEncoderMatrix();
  }
  return true;
}

void RSCoder16::MakeEncoderMatrix()
{

  for (uint I = 0; I < NR; I++)
    for (uint J = 0; J < ND; J++)
      MX[I * ND + J] = gfInv( gfAdd( (I+ND), J) );
}

void RSCoder16::MakeDecoderMatrix()
{

  for (uint Flag=0, R=ND, Dest=0; Flag < ND; Flag++)
    if (!ValidFlags[Flag])
    {
      while (!ValidFlags[R])
        R++;
      for (uint J = 0; J < ND; J++)
        MX[Dest*ND + J] = gfInv( gfAdd(R,J) );
      Dest++;
      R++;
    }
}

void RSCoder16::InvertDecoderMatrix()
{
  uint *MI=new uint[NE * ND];
  memset(MI, 0, ND * NE * sizeof(*MI));
  for (uint Kr = 0, Kf = 0; Kr < NE; Kr++, Kf++)
  {
    while (ValidFlags[Kf])
      Kf++;
    MI[Kr * ND + Kf] = 1;
  }

  for (uint Kr = 0, Kf = 0; Kf < ND; Kr++, Kf++)
  {
    while (ValidFlags[Kf] && Kf < ND)
    {

      for (uint I = 0; I < NE; I++)
        MI[I * ND + Kf] ^= MX[I * ND + Kf];
      Kf++;
    }

    if (Kf == ND)
      break;

    uint *MXk = MX + Kr * ND;
    uint *MIk = MI + Kr * ND;

    uint PInv = gfInv( MXk[Kf] );

    for (uint I = 0; I < ND; I++)
    {
      MXk[I] = gfMul( MXk[I], PInv );
      MIk[I] = gfMul( MIk[I], PInv );
    }

    for (uint I = 0; I < NE; I++)
      if (I != Kr)
      {

        uint *MXi = MX + I * ND;
        uint *MIi = MI + I * ND;
        uint Mik = MXi[Kf];
        for (uint J = 0; J < ND; J++)
        {
          MXi[J] ^= gfMul(MXk[J] , Mik);
          MIi[J] ^= gfMul(MIk[J] , Mik);
        }
      }
  }

  for (uint I = 0; I < NE * ND; I++)
    MX[I] = MI[I];

  delete[] MI;
}

#if 0

void RSCoder16::Process(const uint *Data, uint *Out)
{
  uint ProcData[gfSize];

  for (uint I = 0; I < ND; I++)
    ProcData[I]=Data[I];

  if (Decoding)
  {

    for (uint I=0, R=ND, Dest=0; I < ND; I++)
      if (!ValidFlags[I])
      {
        while (!ValidFlags[R])
          R++;
        ProcData[I]=Data[R];
        R++;
      }
  }

  uint H=Decoding ? NE : NR;
  for (uint I = 0; I < H; I++)
  {
    uint R = 0;

    uint *MXi=MX + I * ND;
    for (uint J = 0; J < ND; J++)
      R ^= gfMul(MXi[J], ProcData[J]);

    Out[I] = R;
  }
}
#endif

void RSCoder16::UpdateECC(uint DataNum, uint ECCNum, const byte *Data, byte *ECC, size_t BlockSize)
{
  if (DataNum==0)
    memset(ECC, 0, BlockSize);

  bool DirectAccess;
#ifdef LITTLE_ENDIAN

  DirectAccess=sizeof(ushort)==2;
#else
  DirectAccess=false;
#endif

#ifdef USE_SSE
  if (DirectAccess && SSE_UpdateECC(DataNum,ECCNum,Data,ECC,BlockSize))
    return;
#endif

  if (ECCNum==0)
  {
    if (DataLogSize!=BlockSize)
    {
      delete[] DataLog;
      DataLog=new uint[BlockSize];
      DataLogSize=BlockSize;

    }
    if (DirectAccess)
      for (size_t I=0; I<BlockSize; I+=2)
        DataLog[I] = gfLog[ *(ushort*)(Data+I) ];
    else
      for (size_t I=0; I<BlockSize; I+=2)
      {
        uint D=Data[I]+Data[I+1]*256;
        DataLog[I] = gfLog[ D ];
      }
  }

  uint ML = gfLog[ MX[ECCNum * ND + DataNum] ];

  if (DirectAccess)
    for (size_t I=0; I<BlockSize; I+=2)
      *(ushort*)(ECC+I) ^= gfExp[ ML + DataLog[I] ];
  else
    for (size_t I=0; I<BlockSize; I+=2)
    {
      uint R=gfExp[ ML + DataLog[I] ];
      ECC[I]^=byte(R);
      ECC[I+1]^=byte(R/256);
    }
}

#ifdef USE_SSE

bool RSCoder16::SSE_UpdateECC(uint DataNum, uint ECCNum, const byte *Data, byte *ECC, size_t BlockSize)
{

  if ((size_t(Data) & (SSE_ALIGNMENT-1))!=0 || (size_t(ECC) & (SSE_ALIGNMENT-1))!=0 ||
      _SSE_Version<SSE_SSSE3)
    return false;

  uint M=MX[ECCNum * ND + DataNum];

  __m128i T0L,T1L,T2L,T3L;
  __m128i T0H,T1H,T2H,T3H;

  for (uint I=0;I<16;I++)
  {
    ((byte *)&T0L)[I]=byte(gfMul(I,M));
    ((byte *)&T0H)[I]=byte(gfMul(I,M)>>8);
    ((byte *)&T1L)[I]=byte(gfMul(I<<4,M));
    ((byte *)&T1H)[I]=byte(gfMul(I<<4,M)>>8);
    ((byte *)&T2L)[I]=byte(gfMul(I<<8,M));
    ((byte *)&T2H)[I]=byte(gfMul(I<<8,M)>>8);
    ((byte *)&T3L)[I]=byte(gfMul(I<<12,M));
    ((byte *)&T3H)[I]=byte(gfMul(I<<12,M)>>8);
  }

  size_t Pos=0;

  __m128i LowByteMask=_mm_set1_epi16(0xff);
  __m128i Low4Mask=_mm_set1_epi8(0xf);
  __m128i High4Mask=_mm_slli_epi16(Low4Mask,4);

  for (; Pos+2*sizeof(__m128i)<=BlockSize; Pos+=2*sizeof(__m128i))
  {

    __m128i *D=(__m128i *)(Data+Pos);

    __m128i HighBytes0=_mm_srli_epi16(D[0],8);
    __m128i LowBytes0=_mm_and_si128(D[0],LowByteMask);
    __m128i HighBytes1=_mm_srli_epi16(D[1],8);
    __m128i LowBytes1=_mm_and_si128(D[1],LowByteMask);
    __m128i HighBytes=_mm_packus_epi16(HighBytes0,HighBytes1);
    __m128i LowBytes=_mm_packus_epi16(LowBytes0,LowBytes1);

    __m128i LowBytesLow4=_mm_and_si128(LowBytes,Low4Mask);
    __m128i LowBytesMultSum=_mm_shuffle_epi8(T0L,LowBytesLow4);
    __m128i HighBytesMultSum=_mm_shuffle_epi8(T0H,LowBytesLow4);

    __m128i LowBytesHigh4=_mm_and_si128(LowBytes,High4Mask);
            LowBytesHigh4=_mm_srli_epi16(LowBytesHigh4,4);
    __m128i LowBytesHigh4MultLow=_mm_shuffle_epi8(T1L,LowBytesHigh4);
    __m128i LowBytesHigh4MultHigh=_mm_shuffle_epi8(T1H,LowBytesHigh4);

    LowBytesMultSum=_mm_xor_si128(LowBytesMultSum,LowBytesHigh4MultLow);
    HighBytesMultSum=_mm_xor_si128(HighBytesMultSum,LowBytesHigh4MultHigh);

    __m128i HighBytesLow4=_mm_and_si128(HighBytes,Low4Mask);
    __m128i HighBytesLow4MultLow=_mm_shuffle_epi8(T2L,HighBytesLow4);
    __m128i HighBytesLow4MultHigh=_mm_shuffle_epi8(T2H,HighBytesLow4);

    LowBytesMultSum=_mm_xor_si128(LowBytesMultSum,HighBytesLow4MultLow);
    HighBytesMultSum=_mm_xor_si128(HighBytesMultSum,HighBytesLow4MultHigh);

    __m128i HighBytesHigh4=_mm_and_si128(HighBytes,High4Mask);
            HighBytesHigh4=_mm_srli_epi16(HighBytesHigh4,4);
    __m128i HighBytesHigh4MultLow=_mm_shuffle_epi8(T3L,HighBytesHigh4);
    __m128i HighBytesHigh4MultHigh=_mm_shuffle_epi8(T3H,HighBytesHigh4);

    LowBytesMultSum=_mm_xor_si128(LowBytesMultSum,HighBytesHigh4MultLow);
    HighBytesMultSum=_mm_xor_si128(HighBytesMultSum,HighBytesHigh4MultHigh);

    __m128i HighBytesHigh4Mult0=_mm_unpacklo_epi8(LowBytesMultSum,HighBytesMultSum);
    __m128i HighBytesHigh4Mult1=_mm_unpackhi_epi8(LowBytesMultSum,HighBytesMultSum);

    __m128i *StoreECC=(__m128i *)(ECC+Pos);

    StoreECC[0]=_mm_xor_si128(StoreECC[0],HighBytesHigh4Mult0);
    StoreECC[1]=_mm_xor_si128(StoreECC[1],HighBytesHigh4Mult1);
  }

  for (; Pos<BlockSize; Pos+=2)
    *(ushort*)(ECC+Pos) ^= gfMul( M, *(ushort*)(Data+Pos) );

  return true;
}
#endif

ScanTree::ScanTree(StringList *FileMasks,RECURSE_MODE Recurse,bool GetLinks,SCAN_DIRS GetDirs)
{
  ScanTree::FileMasks=FileMasks;
  ScanTree::Recurse=Recurse;
  ScanTree::GetLinks=GetLinks;
  ScanTree::GetDirs=GetDirs;

  ScanEntireDisk=false;
  FolderWildcards=false;

  FindStack.push_back(NULL);

  SetAllMaskDepth=0;
  Depth=0;
  Errors=0;
  Cmd=NULL;
  ErrDirList=NULL;
  ErrDirSpecPathLength=NULL;
}

ScanTree::~ScanTree()
{
  for (int I=Depth;I>=0;I--)
    if (FindStack[I]!=NULL)
      delete FindStack[I];
}

SCAN_CODE ScanTree::GetNext(FindData *FD)
{
  if (Depth<0)
    return SCAN_DONE;

#ifndef SILENT
  uint LoopCount=0;
#endif

  SCAN_CODE FindCode;
  while (1)
  {
    if (CurMask.empty() && !GetNextMask())
      return SCAN_DONE;

#ifndef SILENT

    if ((++LoopCount & 0x3ff)==0)
      Wait();
#endif

    FindCode=FindProc(FD);
    if (FindCode==SCAN_ERROR)
    {
      Errors++;
      continue;
    }
    if (FindCode==SCAN_NEXT)
      continue;
    if (FindCode==SCAN_SUCCESS && FD->IsDir && GetDirs==SCAN_SKIPDIRS)
      continue;
    if (FindCode==SCAN_DONE && GetNextMask())
      continue;
    if (FilterList.ItemsCount()>0 && FindCode==SCAN_SUCCESS)
      if (!CommandData::CheckArgs(&FilterList,FD->IsDir,FD->Name,false,MATCH_WILDSUBPATH))
        continue;
    break;
  }
  return FindCode;
}

bool ScanTree::ExpandFolderMask()
{
  bool WildcardFound=false;
  uint SlashPos=0;
  for (uint I=0;I<CurMask.size();I++)
  {
    if (CurMask[I]=='?' || CurMask[I]=='*')
      WildcardFound=true;
    if (WildcardFound && IsPathDiv(CurMask[I]))
    {

      SlashPos=I;
      break;
    }
  }

  std::wstring Mask=CurMask.substr(0,SlashPos);

  ExpandedFolderList.Reset();
  FindFile Find;
  Find.SetMask(Mask);
  FindData FD;
  while (Find.Next(&FD))
    if (FD.IsDir)
    {
      FD.Name+=CurMask.substr(SlashPos);

      std::wstring LastMask=PointToName(FD.Name);
      if (LastMask==L"*" || LastMask==L"*.*" || LastMask.empty())
        RemoveNameFromPath(FD.Name);

      ExpandedFolderList.AddString(FD.Name);
    }
  if (ExpandedFolderList.ItemsCount()==0)
    return false;

  ExpandedFolderList.GetString(CurMask);
  return true;
}

bool ScanTree::GetFilteredMask()
{

  if (ExpandedFolderList.ItemsCount()>0 && ExpandedFolderList.GetString(CurMask))
    return true;

  FolderWildcards=false;
  FilterList.Reset();
  if (!FileMasks->GetString(CurMask))
    return false;

  bool WildcardFound=false;
  uint FolderWildcardCount=0;
  uint SlashPos=0;
  uint StartPos=0;
#ifdef _WIN_ALL
  if (CurMask.rfind(L"\\\\?\\",0)==0)
    StartPos=4;
#endif
  for (uint I=StartPos;I<CurMask.size();I++)
  {
    if (CurMask[I]=='?' || CurMask[I]=='*')
      WildcardFound=true;
    if (IsPathDiv(CurMask[I]) || IsDriveDiv(CurMask[I]))
    {
      if (WildcardFound)
      {

        FolderWildcardCount++;
        WildcardFound=false;
      }
      if (FolderWildcardCount==0)
        SlashPos=I;
    }
  }
  if (FolderWildcardCount==0)
    return true;
  FolderWildcards=true;

  if ((Recurse==RECURSE_NONE || Recurse==RECURSE_DISABLE) && FolderWildcardCount==1)
    return ExpandFolderMask();

  std::wstring Filter=L"*";
  AddEndSlash(Filter);

  std::wstring WildName=IsPathDiv(CurMask[SlashPos]) || IsDriveDiv(CurMask[SlashPos]) ? CurMask.substr(SlashPos+1) : CurMask.substr(SlashPos);
  Filter+=WildName;

  std::wstring LastMask=PointToName(Filter);
  if (LastMask==L"*" || LastMask==L"*.*")
    GetPathWithSep(Filter,Filter);

  FilterList.AddString(Filter);

  bool RelativeDrive=IsDriveDiv(CurMask[SlashPos]);
  if (RelativeDrive)
    SlashPos++;

  CurMask.erase(SlashPos);

  if (!RelativeDrive)
  {

    AddEndSlash(CurMask);
    CurMask+=MASKALL;
  }
  return true;
}

bool ScanTree::GetNextMask()
{
  if (!GetFilteredMask())
    return false;
#ifdef _WIN_ALL
  UnixSlashToDos(CurMask,CurMask);
#endif

  SpecPathLength=GetNamePos(CurMask);

  if (Recurse!=RECURSE_DISABLE)
    if (CurMask.size()>2 && CurMask[0]==CPATHDIVIDER && CurMask[1]==CPATHDIVIDER)
    {
      auto Slash=CurMask.find(CPATHDIVIDER,2);
      if (Slash!=std::wstring::npos)
      {
        Slash=CurMask.find(CPATHDIVIDER,Slash+1);

        ScanEntireDisk=Slash==std::wstring::npos ||
                       Slash!=std::wstring::npos && Slash+1==CurMask.size();

        if (Slash==std::wstring::npos)
          CurMask+=CPATHDIVIDER;
      }
    }
    else
      ScanEntireDisk=IsDriveLetter(CurMask) && IsPathDiv(CurMask[2]) && CurMask[3]==0;

  auto NamePos=GetNamePos(CurMask);
  std::wstring Name=CurMask.substr(NamePos);
  if (Name.empty())
    CurMask+=MASKALL;
  if (Name==L"." || Name==L"..")
  {
    AddEndSlash(CurMask);
    CurMask+=MASKALL;
  }
  Depth=0;

  OrigCurMask=CurMask;

  return true;
}

SCAN_CODE ScanTree::FindProc(FindData *FD)
{
  if (CurMask.empty())
    return SCAN_NEXT;
  bool FastFindFile=false;

  if (FindStack[Depth]==NULL)
  {
    bool Wildcards=IsWildcard(CurMask);

    bool FindCode=!Wildcards && FindFile::FastFind(CurMask,FD,GetLinks);

    bool IsDir=FindCode && FD->IsDir && (!GetLinks || !FD->IsLink);

    bool SearchAll=!IsDir && (Depth>0 || Recurse==RECURSE_ALWAYS ||
                   FolderWildcards && Recurse!=RECURSE_DISABLE ||
                   Wildcards && Recurse==RECURSE_WILDCARDS ||
                   ScanEntireDisk && Recurse!=RECURSE_DISABLE);
    if (Depth==0)
      SearchAllInRoot=SearchAll;
    if (SearchAll || Wildcards)
    {

      FindStack[Depth]=new FindFile;

      std::wstring SearchMask=CurMask;
      if (SearchAll)
        SetName(SearchMask,MASKALL);
      FindStack[Depth]->SetMask(SearchMask);
    }
    else
    {

      if (!FindCode || !IsDir || Recurse==RECURSE_DISABLE)
      {

        SCAN_CODE RetCode=SCAN_SUCCESS;

        if (!FindCode)
        {

          RetCode=FD->Error ? SCAN_ERROR:SCAN_NEXT;

          if (Cmd!=NULL && Cmd->ExclCheck(CurMask,false,true,true))
            RetCode=SCAN_NEXT;
          else
          {
            ErrHandler.OpenErrorMsg(ErrArcName,CurMask);

            ErrHandler.SetErrorCode(RARX_NOFILES);
          }
        }

        CurMask.clear();

        return RetCode;
      }

      FastFindFile=true;
    }
  }

  if (!FastFindFile && !FindStack[Depth]->Next(FD,GetLinks))
  {

    bool Error=FD->Error;
    if (Error)
      ScanError(Error);

    delete FindStack[Depth];
    FindStack[Depth--]=NULL;
    while (Depth>=0 && FindStack[Depth]==NULL)
      Depth--;
    if (Depth < 0)
    {

      if (Error)
        Errors++;
      return SCAN_DONE;
    }

    auto Slash=CurMask.rfind(CPATHDIVIDER);
    if (Slash!=std::wstring::npos)
    {
      std::wstring Mask;
      Mask=CurMask.substr(Slash);
      if (Depth<SetAllMaskDepth)
        Mask.replace(1, std::wstring::npos, PointToName(OrigCurMask));
      CurMask.erase(Slash);

      std::wstring DirName=CurMask;

      auto PrevSlash=CurMask.rfind(CPATHDIVIDER);
      if (PrevSlash==std::wstring::npos)
        CurMask=Mask.substr(1);
      else
      {
        CurMask.erase(PrevSlash);
        CurMask+=Mask;
      }

      if (GetDirs==SCAN_GETDIRSTWICE &&
          FindFile::FastFind(DirName,FD,GetLinks) && FD->IsDir)
      {
        FD->Flags|=FDDF_SECONDDIR;
        return Error ? SCAN_ERROR:SCAN_SUCCESS;
      }
    }
    return Error ? SCAN_ERROR:SCAN_NEXT;
  }

  if (FD->IsDir && (!GetLinks || !FD->IsLink))
  {

    if (!FastFindFile && Depth==0 && !SearchAllInRoot)
      return GetDirs==SCAN_GETCURDIRS ? SCAN_SUCCESS:SCAN_NEXT;

    if (Cmd!=NULL && (Cmd->ExclCheck(FD->Name,true,false,false) ||
        Cmd->ExclDirByAttr(FD->FileAttr)))
    {

      return FastFindFile ? SCAN_DONE:SCAN_NEXT;
    }

    std::wstring Mask=FastFindFile ? MASKALL:PointToName(CurMask);
    CurMask=FD->Name;

    if (CurMask.size()+Mask.size()+1>=MAXPATHSIZE || Depth>=MAXSCANDEPTH-1)
    {
      uiMsg(UIERROR_PATHTOOLONG,CurMask,SPATHDIVIDER,Mask);
      return SCAN_ERROR;
    }

    AddEndSlash(CurMask);
    CurMask+=Mask;

    Depth++;

    FindStack.resize(Depth+1);

    if (FastFindFile)
      SetAllMaskDepth=Depth;
  }
  if (!FastFindFile && !CmpName(CurMask,FD->Name,MATCH_NAMES))
    return SCAN_NEXT;

  return SCAN_SUCCESS;
}

void ScanTree::ScanError(bool &Error)
{
#ifdef _WIN_ALL
  if (Error)
  {

    auto Slash=GetNamePos(CurMask);
    if (Slash>1)
    {
      std::wstring Parent=CurMask.substr(0,Slash-1);
      DWORD Attr=GetFileAttr(Parent);
      if (Attr!=0xffffffff && (Attr & FILE_ATTRIBUTE_REPARSE_POINT)!=0)
        Error=false;
    }

    if (CurMask.find(L"System Volume Information\\")!=std::wstring::npos)
      Error=false;
  }
#endif

  if (Error && Cmd!=NULL && Cmd->ExclCheck(CurMask,false,true,true))
    Error=false;

  if (Error)
  {
    if (ErrDirList!=NULL)
      ErrDirList->AddString(CurMask);
    if (ErrDirSpecPathLength!=NULL)
      ErrDirSpecPathLength->push_back((uint)SpecPathLength);
    std::wstring FullName;

    ConvertNameToFull(CurMask,FullName);
    uiMsg(UIERROR_DIRSCAN,FullName);
    ErrHandler.SysErrMsg();
  }
}

#if defined(_WIN_ALL)
typedef BOOL (WINAPI *CRYPTPROTECTMEMORY)(LPVOID pData,DWORD cbData,DWORD dwFlags);
typedef BOOL (WINAPI *CRYPTUNPROTECTMEMORY)(LPVOID pData,DWORD cbData,DWORD dwFlags);

#ifndef CRYPTPROTECTMEMORY_BLOCK_SIZE
#define CRYPTPROTECTMEMORY_BLOCK_SIZE           16
#define CRYPTPROTECTMEMORY_SAME_PROCESS         0x00
#define CRYPTPROTECTMEMORY_CROSS_PROCESS        0x01
#endif

class CryptLoader
{
  private:
    HMODULE hCrypt;
    bool LoadCalled;
  public:
    CryptLoader()
    {
      hCrypt=NULL;
      pCryptProtectMemory=NULL;
      pCryptUnprotectMemory=NULL;
      LoadCalled=false;
    }
    ~CryptLoader()
    {
      if (hCrypt!=NULL)
        FreeLibrary(hCrypt);
      hCrypt=NULL;
      pCryptProtectMemory=NULL;
      pCryptUnprotectMemory=NULL;
    };
    void Load()
    {
      if (!LoadCalled)
      {
        hCrypt = LoadSysLibrary(L"Crypt32.dll");
        if (hCrypt != NULL)
        {

          pCryptProtectMemory = (CRYPTPROTECTMEMORY)GetProcAddress(hCrypt, "CryptProtectMemory");
          pCryptUnprotectMemory = (CRYPTUNPROTECTMEMORY)GetProcAddress(hCrypt, "CryptUnprotectMemory");
        }
        LoadCalled=true;
      }
    }

    CRYPTPROTECTMEMORY pCryptProtectMemory;
    CRYPTUNPROTECTMEMORY pCryptUnprotectMemory;
};

static CryptLoader GlobalCryptLoader;
#endif

SecPassword::SecPassword()
{
  Set(L"");
}

SecPassword::~SecPassword()
{
  Clean();
}

void SecPassword::Clean()
{
  PasswordSet=false;
  if (!Password.empty())
    cleandata(Password.data(),Password.size()*sizeof(Password[0]));
}

void cleandata(void *data,size_t size)
{
  if (data==nullptr || size==0)
    return;
#if defined(_WIN_ALL) && defined(_MSC_VER)
  SecureZeroMemory(data,size);
#else

  volatile byte *d = (volatile byte *)data;
  for (size_t i=0;i<size;i++)
    d[i]=0;
#endif
}

void SecPassword::Process(const wchar *Src,size_t SrcSize,wchar *Dst,size_t DstSize,bool Encode)
{

  memcpy(Dst,Src,Min(SrcSize,DstSize)*sizeof(*Dst));
  SecHideData(Dst,DstSize*sizeof(*Dst),Encode,false);
}

void SecPassword::Get(wchar *Psw,size_t MaxSize)
{
  if (PasswordSet)
  {
    Process(&Password[0],Password.size(),Psw,MaxSize,false);
    Psw[MaxSize-1]=0;
  }
  else
    *Psw=0;
}

void SecPassword::Get(std::wstring &Psw)
{
  wchar PswBuf[MAXPASSWORD];
  Get(PswBuf,ASIZE(PswBuf));
  Psw=PswBuf;
}

void SecPassword::Set(const wchar *Psw)
{

  Clean();

  if (*Psw!=0)
  {
    PasswordSet=true;
    Process(Psw,wcslen(Psw)+1,&Password[0],Password.size(),true);
  }
}

size_t SecPassword::Length()
{
  wchar Plain[MAXPASSWORD];
  Get(Plain,ASIZE(Plain));
  size_t Length=wcslen(Plain);
  cleandata(Plain,sizeof(Plain));
  return Length;
}

bool SecPassword::operator == (SecPassword &psw)
{

  wchar Plain1[MAXPASSWORD],Plain2[MAXPASSWORD];
  Get(Plain1,ASIZE(Plain1));
  psw.Get(Plain2,ASIZE(Plain2));
  bool Result=wcscmp(Plain1,Plain2)==0;
  cleandata(Plain1,sizeof(Plain1));
  cleandata(Plain2,sizeof(Plain2));
  return Result;
}

void SecHideData(void *Data,size_t DataSize,bool Encode,bool CrossProcess)
{

#if defined(_WIN_ALL)

  if (GlobalCryptLoader.pCryptProtectMemory==NULL)
    GlobalCryptLoader.Load();
  size_t Aligned=DataSize-DataSize%CRYPTPROTECTMEMORY_BLOCK_SIZE;
  DWORD Flags=CrossProcess ? CRYPTPROTECTMEMORY_CROSS_PROCESS : CRYPTPROTECTMEMORY_SAME_PROCESS;
  if (Encode)
  {
    if (GlobalCryptLoader.pCryptProtectMemory!=NULL)
    {
      if (!GlobalCryptLoader.pCryptProtectMemory(Data,DWORD(Aligned),Flags))
      {
        ErrHandler.GeneralErrMsg(L"CryptProtectMemory failed");
        ErrHandler.SysErrMsg();
        ErrHandler.Exit(RARX_FATAL);
      }
      return;
    }
  }
  else
  {
    if (GlobalCryptLoader.pCryptUnprotectMemory!=NULL)
    {
      if (!GlobalCryptLoader.pCryptUnprotectMemory(Data,DWORD(Aligned),Flags))
      {
        ErrHandler.GeneralErrMsg(L"CryptUnprotectMemory failed");
        ErrHandler.SysErrMsg();
        ErrHandler.Exit(RARX_FATAL);
      }
      return;
    }
  }
#endif

  uint Key;
#ifdef _WIN_ALL
  Key=GetCurrentProcessId();
#elif defined(_UNIX)
  Key=getpid();
#else
  Key=0;
#endif

  for (size_t I=0;I<DataSize;I++)
    *((byte *)Data+I)^=Key+I+75;
}

#ifndef SFX_MODULE
#define SHA1_UNROLL
#endif

#ifdef LITTLE_ENDIAN
#define blk0(i) (block->l[i] = ByteSwap32(block->l[i]))
#else
#define blk0(i) block->l[i]
#endif
#define blk(i) (block->l[i&15] = rotl32(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

#define R0(v,w,x,y,z,i) {z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rotl32(v,5);w=rotl32(w,30);}
#define R1(v,w,x,y,z,i) {z+=((w&(x^y))^y)+blk(i)+0x5A827999+rotl32(v,5);w=rotl32(w,30);}
#define R2(v,w,x,y,z,i) {z+=(w^x^y)+blk(i)+0x6ED9EBA1+rotl32(v,5);w=rotl32(w,30);}
#define R3(v,w,x,y,z,i) {z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rotl32(v,5);w=rotl32(w,30);}
#define R4(v,w,x,y,z,i) {z+=(w^x^y)+blk(i)+0xCA62C1D6+rotl32(v,5);w=rotl32(w,30);}

void SHA1Transform(uint32 state[5], uint32 workspace[16], const byte buffer[64], bool inplace)
{
  uint32 a, b, c, d, e;

  union CHAR64LONG16
  {
    unsigned char c[64];
    uint32 l[16];
  } *block;

  if (inplace)
    block = (CHAR64LONG16*)buffer;
  else
  {
    block = (CHAR64LONG16*)workspace;
    memcpy(block, buffer, 64);
  }

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];

#ifdef SHA1_UNROLL

  R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
  R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
  R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
  R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
#else
  for (uint I=0;;I+=5)
  {
    R0(a,b,c,d,e, I+0); if (I==15) break;
    R0(e,a,b,c,d, I+1); R0(d,e,a,b,c, I+2);
    R0(c,d,e,a,b, I+3); R0(b,c,d,e,a, I+4);
  }
  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
  for (uint I=20;I<=35;I+=5)
  {
    R2(a,b,c,d,e,I+0); R2(e,a,b,c,d,I+1); R2(d,e,a,b,c,I+2);
    R2(c,d,e,a,b,I+3); R2(b,c,d,e,a,I+4);
  }
  for (uint I=40;I<=55;I+=5)
  {
    R3(a,b,c,d,e,I+0); R3(e,a,b,c,d,I+1); R3(d,e,a,b,c,I+2);
    R3(c,d,e,a,b,I+3); R3(b,c,d,e,a,I+4);
  }
  for (uint I=60;I<=75;I+=5)
  {
    R4(a,b,c,d,e,I+0); R4(e,a,b,c,d,I+1); R4(d,e,a,b,c,I+2);
    R4(c,d,e,a,b,I+3); R4(b,c,d,e,a,I+4);
  }
#endif

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

void sha1_init(sha1_context* context)
{
  context->count = 0;

  context->state[0] = 0x67452301;
  context->state[1] = 0xEFCDAB89;
  context->state[2] = 0x98BADCFE;
  context->state[3] = 0x10325476;
  context->state[4] = 0xC3D2E1F0;
}

void sha1_process( sha1_context * context, const unsigned char * data, size_t len)
{
  size_t i, j = (size_t)(context->count & 63);
  context->count += len;

  if ((j + len) > 63)
  {
    memcpy(context->buffer+j, data, (i = 64-j));
    uint32 workspace[16];
    SHA1Transform(context->state, workspace, context->buffer, true);
    for ( ; i + 63 < len; i += 64)
      SHA1Transform(context->state, workspace, data+i, false);
    j = 0;
  }
  else
    i = 0;
  if (len > i)
    memcpy(context->buffer+j, data+i, len - i);
}

void sha1_process_rar29(sha1_context *context, const unsigned char *data, size_t len)
{
  size_t i, j = (size_t)(context->count & 63);
  context->count += len;

  if ((j + len) > 63)
  {
    memcpy(context->buffer+j, data, (i = 64-j));
    uint32 workspace[16];
    SHA1Transform(context->state, workspace, context->buffer, true);
    for ( ; i + 63 < len; i += 64)
    {
      SHA1Transform(context->state, workspace, data+i, false);
      for (uint k = 0; k < 16; k++)
        RawPut4(workspace[k],(void*)(data+i+k*4));
    }
    j = 0;
  }
  else
    i = 0;
  if (len > i)
    memcpy(context->buffer+j, data+i, len - i);
}

void sha1_done( sha1_context* context, uint32 digest[5])
{
  uint32 workspace[16];
  uint64 BitLength = context->count * 8;
  uint BufPos = (uint)context->count & 0x3f;
  context->buffer[BufPos++] = 0x80;

  if (BufPos!=56)
  {
    if (BufPos>56)
    {
      while (BufPos<64)
        context->buffer[BufPos++] = 0;
      BufPos=0;
    }
    if (BufPos==0)
      SHA1Transform(context->state, workspace, context->buffer, true);
    memset(context->buffer+BufPos,0,56-BufPos);
  }

  RawPutBE4((uint32)(BitLength>>32), context->buffer + 56);
  RawPutBE4((uint32)(BitLength), context->buffer + 60);

  SHA1Transform(context->state, workspace, context->buffer, true);

  for (uint i = 0; i < 5; i++)
    digest[i] = context->state[i];

  sha1_init(context);
}

static const uint32 K[64] =
{
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
  0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
  0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
  0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define Ch(x, y, z)  ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

#define Sg0(x) (rotr32(x, 2) ^ rotr32(x,13) ^ rotr32(x, 22))
#define Sg1(x) (rotr32(x, 6) ^ rotr32(x,11) ^ rotr32(x, 25))
#define sg0(x) (rotr32(x, 7) ^ rotr32(x,18) ^ (x >> 3))
#define sg1(x) (rotr32(x,17) ^ rotr32(x,19) ^ (x >> 10))

void sha256_init(sha256_context *ctx)
{
  ctx->H[0] = 0x6a09e667;
  ctx->H[1] = 0xbb67ae85;
  ctx->H[2] = 0x3c6ef372;
  ctx->H[3] = 0xa54ff53a;
  ctx->H[4] = 0x510e527f;
  ctx->H[5] = 0x9b05688c;
  ctx->H[6] = 0x1f83d9ab;
  ctx->H[7] = 0x5be0cd19;
  ctx->Count    = 0;
}

static void sha256_transform(sha256_context *ctx)
{
  uint32 W[64];
  uint32 v[8];

  for (uint I = 0; I < 16; I++)
    W[I] = RawGetBE4(ctx->Buffer + I * 4);
  for (uint I = 16; I < 64; I++)
    W[I] = sg1(W[I-2]) + W[I-7] + sg0(W[I-15]) + W[I-16];

  uint32 *H=ctx->H;
  v[0]=H[0]; v[1]=H[1]; v[2]=H[2]; v[3]=H[3];
  v[4]=H[4]; v[5]=H[5]; v[6]=H[6]; v[7]=H[7];

  for (uint I = 0; I < 64; I++)
  {
    uint32 T1 = v[7] + Sg1(v[4]) + Ch(v[4], v[5], v[6]) + K[I] + W[I];

    v[7] = v[6];
    v[6] = v[5];
    v[5] = v[4];
    v[4] = v[3] + T1;

    uint32 T2 = Sg0(v[0]) + Maj(v[0], v[1], v[2]);

    v[3] = v[2];
    v[2] = v[1];
    v[1] = v[0];
    v[0] = T1 + T2;
  }

  H[0]+=v[0]; H[1]+=v[1]; H[2]+=v[2]; H[3]+=v[3];
  H[4]+=v[4]; H[5]+=v[5]; H[6]+=v[6]; H[7]+=v[7];
}

void sha256_process(sha256_context *ctx, const void *Data, size_t Size)
{
  const byte *Src=(const byte *)Data;
  size_t BufPos = (uint)ctx->Count & 0x3f;
  ctx->Count+=Size;
  while (Size > 0)
  {
    size_t BufSpace=sizeof(ctx->Buffer)-BufPos;
    size_t CopySize=Size>BufSpace ? BufSpace:Size;

    memcpy(ctx->Buffer+BufPos,Src,CopySize);

    Src+=CopySize;
    BufPos+=CopySize;
    Size-=CopySize;
    if (BufPos == 64)
    {
      BufPos = 0;
      sha256_transform(ctx);
    }
  }
}

void sha256_done(sha256_context *ctx, byte *Digest)
{
  uint64 BitLength = ctx->Count * 8;
  uint BufPos = (uint)ctx->Count & 0x3f;
  ctx->Buffer[BufPos++] = 0x80;

  if (BufPos!=56)
  {
    if (BufPos>56)
    {
      while (BufPos<64)
        ctx->Buffer[BufPos++] = 0;
      BufPos=0;
    }
    if (BufPos==0)
      sha256_transform(ctx);
    memset(ctx->Buffer+BufPos,0,56-BufPos);
  }

  RawPutBE4((uint32)(BitLength>>32), ctx->Buffer + 56);
  RawPutBE4((uint32)(BitLength), ctx->Buffer + 60);

  sha256_transform(ctx);

  RawPutBE4(ctx->H[0], Digest +  0);
  RawPutBE4(ctx->H[1], Digest +  4);
  RawPutBE4(ctx->H[2], Digest +  8);
  RawPutBE4(ctx->H[3], Digest + 12);
  RawPutBE4(ctx->H[4], Digest + 16);
  RawPutBE4(ctx->H[5], Digest + 20);
  RawPutBE4(ctx->H[6], Digest + 24);
  RawPutBE4(ctx->H[7], Digest + 28);

  sha256_init(ctx);
}

void sha256_get(const void *Data, size_t Size, byte *Digest)
{
  sha256_context ctx;
  sha256_init(&ctx);
  sha256_process(&ctx, Data, Size);
  sha256_done(&ctx, Digest);
}

int ToPercent(int64 N1,int64 N2)
{
  if (N2<N1)
    return 100;
  return ToPercentUnlim(N1,N2);
}

int ToPercentUnlim(int64 N1,int64 N2)
{
  if (N2==0)
    return 0;
  return (int)(N1*100/N2);
}

const char *NullToEmpty(const char *Str)
{
  return Str==nullptr ? "":Str;
}

const wchar *NullToEmpty(const wchar *Str)
{
  return Str==nullptr ? L"":Str;
}

void OemToExt(const std::string &Src,std::string &Dest)
{
#ifdef _WIN_ALL
  if (std::addressof(Src)!=std::addressof(Dest))
    Dest=Src;

  OemToCharBuffA(&Dest[0],&Dest[0],(DWORD)Dest.size());

  std::string::size_type Pos=Dest.find('\0');
  if (Pos!=std::string::npos)
    Dest.erase(Pos);

#else
  if (std::addressof(Src)!=std::addressof(Dest))
    Dest=Src;
#endif
}

void ArcCharToWide(const char *Src,std::wstring &Dest,ACTW_ENCODING Encoding)
{
#if defined(_WIN_ALL)
  if (Encoding==ACTW_UTF8)
    UtfToWide(Src,Dest);
  else
  {
    std::string NameA;
    if (Encoding==ACTW_OEM)
    {
      OemToExt(Src,NameA);
      Src=NameA.data();
    }
    CharToWide(Src,Dest);
  }
#else
  if (Encoding==ACTW_UTF8)
    UtfToWide(Src,Dest);
  else
    CharToWide(Src,Dest);
#endif
  TruncateAtZero(Dest);
}

int stricomp(const char *s1,const char *s2)
{
#ifdef _WIN_ALL
  return CompareStringA(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,-1,s2,-1)-2;
#else
  while (toupper(*s1)==toupper(*s2))
  {
    if (*s1==0)
      return 0;
    s1++;
    s2++;
  }
  return s1 < s2 ? -1 : 1;
#endif
}

int strnicomp(const char *s1,const char *s2,size_t n)
{
#ifdef _WIN_ALL

  size_t l1=Min(strnlen(s1,n),n);
  size_t l2=Min(strnlen(s2,n),n);
  return CompareStringA(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,(int)l1,s2,(int)l2)-2;
#else
  if (n==0)
    return 0;
  while (toupper(*s1)==toupper(*s2))
  {
    if (*s1==0 || --n==0)
      return 0;
    s1++;
    s2++;
  }
  return s1 < s2 ? -1 : 1;
#endif
}

wchar* RemoveEOL(wchar *Str)
{
  for (int I=(int)wcslen(Str)-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n' || Str[I]==' ' || Str[I]=='\t');I--)
    Str[I]=0;
  return Str;
}

void RemoveEOL(std::wstring &Str)
{
  while (!Str.empty())
  {
    wchar c=Str.back();
    if (c=='\r' || c=='\n' || c==' ' || c=='\t')
      Str.pop_back();
    else
      break;
  }
}

wchar* RemoveLF(wchar *Str)
{
  for (int I=(int)wcslen(Str)-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n');I--)
    Str[I]=0;
  return Str;
}

void RemoveLF(std::wstring &Str)
{
  for (int I=(int)Str.size()-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n');I--)
    Str.erase(I);
}

#if defined(SFX_MODULE)

unsigned char etoupper(unsigned char c)
{
  return c>='a' && c<='z' ? c-'a'+'A' : c;
}
#endif

wchar etoupperw(wchar c)
{
  return c>='a' && c<='z' ? c-'a'+'A' : c;
}

bool IsDigit(int ch)
{
  return ch>='0' && ch<='9';
}

bool IsSpace(int ch)
{
  return ch==' ' || ch=='\t';
}

bool IsAlpha(int ch)
{
  return ch>='A' && ch<='Z' || ch>='a' && ch<='z';
}

void BinToHex(const byte *Bin,size_t BinSize,std::wstring &Hex)
{
  Hex.clear();
  for (uint I=0;I<BinSize;I++)
  {
    uint High=Bin[I] >> 4;
    uint Low=Bin[I] & 0xf;
    uint HighHex=High>9 ? 'a'+High-10 : '0'+High;
    uint LowHex=Low>9 ? 'a'+Low-10 : '0'+Low;
    Hex+=HighHex;
    Hex+=LowHex;
  }
}

#ifndef SFX_MODULE
uint GetDigits(uint Number)
{
  uint Digits=1;
  while (Number>=10)
  {
    Number/=10;
    Digits++;
  }
  return Digits;
}
#endif

bool LowAscii(const std::string &Str)
{
  for (char Ch : Str)
  {

    if ((byte)Ch>127)
      return false;
  }
  return true;
}

bool LowAscii(const std::wstring &Str)
{
  for (wchar Ch : Str)
  {

    if ((uint)Ch>127)
      return false;
  }
  return true;
}

int wcsicompc(const wchar *s1,const wchar *s2)
{
#if defined(_UNIX)
  return wcscmp(s1,s2);
#else
  return wcsicomp(s1,s2);
#endif
}

int wcsicompc(const std::wstring &s1,const std::wstring &s2)
{
  return wcsicompc(s1.c_str(),s2.c_str());
}

int wcsnicompc(const wchar *s1,const wchar *s2,size_t n)
{
#if defined(_UNIX)
  return wcsncmp(s1,s2,n);
#else
  return wcsnicomp(s1,s2,n);
#endif
}

int wcsnicompc(const std::wstring &s1,const std::wstring &s2,size_t n)
{
  return wcsnicompc(s1.c_str(),s2.c_str(),n);
}

void strncpyz(char *dest, const char *src, size_t maxlen)
{
  if (maxlen>0)
  {
    while (--maxlen>0 && *src!=0)
      *dest++=*src++;
    *dest=0;
  }
}

void wcsncpyz(wchar *dest, const wchar *src, size_t maxlen)
{
  if (maxlen>0)
  {
    while (--maxlen>0 && *src!=0)
      *dest++=*src++;
    *dest=0;
  }
}

void strncatz(char* dest, const char* src, size_t maxlen)
{
  size_t length = strlen(dest);
  if (maxlen > length)
    strncpyz(dest + length, src, maxlen - length);
}

void wcsncatz(wchar* dest, const wchar* src, size_t maxlen)
{
  size_t length = wcslen(dest);
  if (maxlen > length)
    wcsncpyz(dest + length, src, maxlen - length);
}

void itoa(int64 n,char *Str,size_t MaxSize)
{
  char NumStr[50];
  size_t Pos=0;

  int Neg=n < 0 ? 1 : 0;
  if (Neg)
    n=-n;

  do
  {
    if (Pos+1>=MaxSize-Neg)
      break;
    NumStr[Pos++]=char(n%10)+'0';
    n=n/10;
  } while (n!=0);

  if (Neg)
    NumStr[Pos++]='-';

  for (size_t I=0;I<Pos;I++)
    Str[I]=NumStr[Pos-I-1];
  Str[Pos]=0;
}

void itoa(int64 n,wchar *Str,size_t MaxSize)
{
  wchar NumStr[50];
  size_t Pos=0;

  int Neg=n < 0 ? 1 : 0;
  if (Neg)
    n=-n;

  do
  {
    if (Pos+1>=MaxSize-Neg)
      break;
    NumStr[Pos++]=wchar(n%10)+'0';
    n=n/10;
  } while (n!=0);

  if (Neg)
    NumStr[Pos++]='-';

  for (size_t I=0;I<Pos;I++)
    Str[I]=NumStr[Pos-I-1];
  Str[Pos]=0;
}

void fmtitoa(int64 n,wchar *Str,size_t MaxSize)
{
  static wchar ThSep=0;
#ifdef _WIN_ALL
  wchar Info[10];
  if (!ThSep!=0 && GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_STHOUSAND,Info,ASIZE(Info))>0)
    ThSep=*Info;
#elif defined(_UNIX)
  ThSep=*localeconv()->thousands_sep;
#endif
  if (ThSep==0)
    ThSep=' ';
  wchar RawText[30];
  itoa(n,RawText,ASIZE(RawText));
  uint S=0,D=0,L=wcslen(RawText)%3;
  while (RawText[S]!=0 && D+1<MaxSize)
  {
    if (S!=0 && (S+3-L)%3==0)
      Str[D++]=ThSep;
    Str[D++]=RawText[S++];
  }
  Str[D]=0;
}

std::wstring GetWide(const char *Src)
{
  std::wstring Str;
  CharToWide(Src,Str);
  return Str;
}

bool GetCmdParam(const std::wstring &CmdLine,std::wstring::size_type &Pos,std::wstring &Param)
{
  Param.clear();

  while (IsSpace(CmdLine[Pos]))
    Pos++;
  if (Pos==CmdLine.size())
    return false;

  bool Quote=false;
  while (Pos<CmdLine.size() && (Quote || !IsSpace(CmdLine[Pos])))
  {
    if (CmdLine[Pos]=='\"')
    {
      if (CmdLine[Pos+1]=='\"')
      {

        Param+='\"';
        Pos++;
      }
      else
        Quote=!Quote;
    }
    else
      Param+=CmdLine[Pos];
    Pos++;
  }
  return true;
}

#ifndef RARDLL

void PrintfPrepareFmt(const wchar *Org,std::wstring &Cvt)
{
  size_t Src=0;
  while (Org[Src]!=0)
  {
    if (Org[Src]=='%' && (Src==0 || Org[Src-1]!='%'))
    {
      size_t SPos=Src+1;

      while (IsDigit(Org[SPos]) || Org[SPos]=='-')
        SPos++;
      if (Org[SPos]=='s')
      {
        while (Src<SPos)
          Cvt.push_back(Org[Src++]);
        Cvt.push_back('l');
      }
    }
#ifdef _WIN_ALL

    if (Org[Src]=='\n' && (Src==0 || Org[Src-1]!='\r'))
      Cvt.push_back('\r');
#endif

    Cvt.push_back(Org[Src++]);
  }
}

std::wstring wstrprintf(const wchar *fmt,...)
{
  va_list arglist;
  va_start(arglist,fmt);
  std::wstring s=vwstrprintf(fmt,arglist);
  va_end(arglist);
  return s;
}

std::wstring vwstrprintf(const wchar *fmt,va_list arglist)
{
  std::wstring fmtw;
  PrintfPrepareFmt(fmt,fmtw);

  const size_t MaxAllocSize=0x10000;

  std::wstring Msg(256,L'\0');
  while (true)
  {
    va_list argscopy;
    va_copy(argscopy, arglist);
    int r=vswprintf(&Msg[0],Msg.size(),fmtw.c_str(),argscopy);
    va_end(argscopy);
    if (r>=0 || Msg.size()>MaxAllocSize)
      break;
    Msg.resize(Msg.size()*4);
  }
  std::wstring::size_type ZeroPos=Msg.find(L'\0');
  if (ZeroPos!=std::wstring::npos)
    Msg.resize(ZeroPos);

  return Msg;
}
#endif

#ifdef _WIN_ALL
bool ExpandEnvironmentStr(std::wstring &Str)
{
  DWORD ExpCode=ExpandEnvironmentStrings(Str.c_str(),nullptr,0);
  if (ExpCode==0)
    return false;
  std::vector<wchar> Buf(ExpCode);
  ExpCode=ExpandEnvironmentStrings(Str.c_str(),Buf.data(),(DWORD)Buf.size());
  if (ExpCode==0 || ExpCode>Buf.size())
    return false;
  Str=Buf.data();
  return true;
}
#endif

void TruncateAtZero(std::wstring &Str)
{
  std::wstring::size_type Pos=Str.find(L'\0');
  if (Pos!=std::wstring::npos)
    Str.erase(Pos);
}

void ReplaceEsc(std::wstring &Str)
{
  std::wstring::size_type Pos=0;
  while (true)
  {
    Pos=Str.find(L'\033',Pos);
    if (Pos==std::wstring::npos)
      break;
    Str[Pos]=L'\'';
    Str.insert(Pos+1,L"\\033'");
    Pos+=6;
  }
}

StringList::StringList()
{
  Reset();
}

void StringList::Reset()
{
  Rewind();
  StringData.clear();
  StringsCount=0;
  SavePosNumber=0;
}

void StringList::AddString(const wchar *Str)
{
  if (Str==NULL)
    Str=L"";

  size_t PrevSize=StringData.size();
  StringData.resize(PrevSize+wcslen(Str)+1);
  wcscpy(&StringData[PrevSize],Str);

  StringsCount++;
}

void StringList::AddString(const std::wstring &Str)
{
  AddString(Str.c_str());
}

bool StringList::GetString(wchar *Str,size_t MaxLength)
{
  wchar *StrPtr;
  if (!GetString(&StrPtr))
    return false;
  wcsncpyz(Str,StrPtr,MaxLength);
  return true;
}

bool StringList::GetString(std::wstring &Str)
{
  wchar *StrPtr;
  if (!GetString(&StrPtr))
    return false;
  Str=StrPtr;
  return true;
}

#ifndef SFX_MODULE
bool StringList::GetString(wchar *Str,size_t MaxLength,int StringNum)
{
  SavePosition();
  Rewind();
  bool RetCode=true;
  while (StringNum-- >=0)
    if (!GetString(Str,MaxLength))
    {
      RetCode=false;
      break;
    }
  RestorePosition();
  return RetCode;
}

bool StringList::GetString(std::wstring &Str,int StringNum)
{
  SavePosition();
  Rewind();
  bool RetCode=true;
  while (StringNum-- >=0)
    if (!GetString(Str))
    {
      RetCode=false;
      break;
    }
  RestorePosition();
  return RetCode;
}
#endif

wchar* StringList::GetString()
{
  wchar *Str;
  GetString(&Str);
  return Str;
}

bool StringList::GetString(wchar **Str)
{
  if (CurPos>=StringData.size())
  {
    if (Str!=NULL)
      *Str=NULL;
    return false;
  }

  wchar *CurStr=&StringData[CurPos];
  CurPos+=wcslen(CurStr)+1;
  if (Str!=NULL)
    *Str=CurStr;

  return true;
}

void StringList::Rewind()
{
  CurPos=0;
}

#ifndef SFX_MODULE
bool StringList::Search(const std::wstring &Str,bool CaseSensitive)
{
  SavePosition();
  Rewind();
  bool Found=false;
  wchar *CurStr;
  while (GetString(&CurStr))
  {
    if (CurStr!=NULL)
      if (CaseSensitive && Str!=CurStr || !CaseSensitive && wcsicomp(Str,CurStr)!=0)
        continue;
    Found=true;
    break;
  }
  RestorePosition();
  return Found;
}
#endif

#ifndef SFX_MODULE
void StringList::SavePosition()
{
  if (SavePosNumber<ASIZE(SaveCurPos))
  {
    SaveCurPos[SavePosNumber]=CurPos;
    SavePosNumber++;
  }
}
#endif

#ifndef SFX_MODULE
void StringList::RestorePosition()
{
  if (SavePosNumber>0)
  {
    SavePosNumber--;
    CurPos=SaveCurPos[SavePosNumber];
  }
}
#endif

static int SleepTime=0;

void InitSystemOptions(int SleepTime)
{
  ::SleepTime=SleepTime;
}

#if !defined(SFX_MODULE)
void SetPriority(int Priority)
{
#ifdef _WIN_ALL
  uint PriorityClass;
  int PriorityLevel;
  if (Priority<1 || Priority>15)
    return;

  if (Priority==1)
  {
    PriorityClass=IDLE_PRIORITY_CLASS;
    PriorityLevel=THREAD_PRIORITY_IDLE;

  }
  else
    if (Priority<7)
    {
      PriorityClass=IDLE_PRIORITY_CLASS;
      PriorityLevel=Priority-4;
    }
    else
      if (Priority==7)
      {
        PriorityClass=BELOW_NORMAL_PRIORITY_CLASS;
        PriorityLevel=THREAD_PRIORITY_ABOVE_NORMAL;
      }
      else
        if (Priority<10)
        {
          PriorityClass=NORMAL_PRIORITY_CLASS;
          PriorityLevel=Priority-7;
        }
        else
          if (Priority==10)
          {
            PriorityClass=ABOVE_NORMAL_PRIORITY_CLASS;
            PriorityLevel=THREAD_PRIORITY_NORMAL;
          }
          else
          {
            PriorityClass=HIGH_PRIORITY_CLASS;
            PriorityLevel=Priority-13;
          }
  SetPriorityClass(GetCurrentProcess(),PriorityClass);
  SetThreadPriority(GetCurrentThread(),PriorityLevel);

#ifdef RAR_SMP
  ThreadPool::SetPriority(PriorityLevel);
#endif

#endif
}
#endif

clock_t MonoClock()
{
  return clock();
}

void Wait()
{
  if (ErrHandler.UserBreak)
    ErrHandler.Exit(RARX_USERBREAK);
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
  if (SleepTime!=0)
  {
    static clock_t LastTime=MonoClock();
    if (MonoClock()-LastTime>10*CLOCKS_PER_SEC/1000)
    {
      Sleep(SleepTime);
      LastTime=MonoClock();
    }
  }
#endif
#if defined(_WIN_ALL)

  SetThreadExecutionState(ES_SYSTEM_REQUIRED);
#endif
}

#ifdef _WIN_ALL
bool SetPrivilege(LPCTSTR PrivName)
{
  bool Success=false;

  HANDLE hToken;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
  {
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (LookupPrivilegeValue(NULL,PrivName,&tp.Privileges[0].Luid) &&
        AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL) &&
        GetLastError() == ERROR_SUCCESS)
      Success=true;

    CloseHandle(hToken);
  }

  return Success;
}
#endif

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
void Shutdown(POWER_MODE Mode)
{
  SetPrivilege(SE_SHUTDOWN_NAME);
  if (Mode==POWERMODE_OFF)
    ExitWindowsEx(EWX_SHUTDOWN|EWX_FORCE,SHTDN_REASON_FLAG_PLANNED);
  if (Mode==POWERMODE_SLEEP)
    SetSuspendState(FALSE,FALSE,FALSE);
  if (Mode==POWERMODE_HIBERNATE)
    SetSuspendState(TRUE,FALSE,FALSE);
  if (Mode==POWERMODE_RESTART)
    ExitWindowsEx(EWX_REBOOT|EWX_FORCE,SHTDN_REASON_FLAG_PLANNED);
}

bool ShutdownCheckAnother(bool Open)
{
  const wchar *EventName=L"rar -ioff";
  static HANDLE hEvent=NULL;
  bool Result=false;
  if (Open)
    hEvent=CreateEvent(NULL,FALSE,FALSE,EventName);
  else
  {
    if (hEvent!=NULL)
      CloseHandle(hEvent);

    hEvent=CreateEvent(NULL,FALSE,FALSE,EventName);
    Result=GetLastError()==ERROR_ALREADY_EXISTS;
    if (hEvent!=NULL)
      CloseHandle(hEvent);
  }
  return Result;
}
#endif

#if defined(_WIN_ALL)

HMODULE WINAPI LoadSysLibrary(const wchar *Name)
{
  std::vector<wchar> SysDir(MAX_PATH);
  if (GetSystemDirectory(SysDir.data(),(UINT)SysDir.size())==0)
    return nullptr;
  std::wstring FullName;
  MakeName(SysDir.data(),Name,FullName);
  return LoadLibrary(FullName.c_str());
}

bool IsUserAdmin()
{
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  PSID AdministratorsGroup;
  BOOL b = AllocateAndInitializeSid(&NtAuthority,2,SECURITY_BUILTIN_DOMAIN_RID,
           DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup);
  if (b)
  {
    if (!CheckTokenMembership( NULL, AdministratorsGroup, &b))
      b = FALSE;
    FreeSid(AdministratorsGroup);
  }
  return b!=FALSE;
}

#endif

#ifdef USE_SSE
SSE_VERSION _SSE_Version=GetSSEVersion();

SSE_VERSION GetSSEVersion()
{
#ifdef _MSC_VER
  int CPUInfo[4];
  __cpuid(CPUInfo, 0);

  uint MaxSupported=CPUInfo[0];

  if (MaxSupported>=7)
  {
    __cpuid(CPUInfo, 7);
    if ((CPUInfo[1] & 0x20)!=0)
      return SSE_AVX2;
  }
  if (MaxSupported>=1)
  {
    __cpuid(CPUInfo, 1);
    if ((CPUInfo[2] & 0x80000)!=0)
      return SSE_SSE41;
    if ((CPUInfo[2] & 0x200)!=0)
      return SSE_SSSE3;
    if ((CPUInfo[3] & 0x4000000)!=0)
      return SSE_SSE2;
    if ((CPUInfo[3] & 0x2000000)!=0)
      return SSE_SSE;
  }
#elif defined(__GNUC__)
  if (__builtin_cpu_supports("avx2"))
    return SSE_AVX2;
  if (__builtin_cpu_supports("sse4.1"))
    return SSE_SSE41;
  if (__builtin_cpu_supports("ssse3"))
    return SSE_SSSE3;
  if (__builtin_cpu_supports("sse2"))
    return SSE_SSE2;
  if (__builtin_cpu_supports("sse"))
    return SSE_SSE;
#endif
  return SSE_NONE;
}
#endif

#ifdef RAR_SMP
static inline bool CriticalSectionCreate(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  InitializeCriticalSection(CritSection);
  return true;
#elif defined(_UNIX)
  return pthread_mutex_init(CritSection,NULL)==0;
#endif
}

static inline void CriticalSectionDelete(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  DeleteCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_destroy(CritSection);
#endif
}

static inline void CriticalSectionStart(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  EnterCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_lock(CritSection);
#endif
}

static inline void CriticalSectionEnd(CRITSECT_HANDLE *CritSection)
{
#ifdef _WIN_ALL
  LeaveCriticalSection(CritSection);
#elif defined(_UNIX)
  pthread_mutex_unlock(CritSection);
#endif
}

static THREAD_HANDLE ThreadCreate(NATIVE_THREAD_PTR Proc,void *Data)
{
#ifdef _UNIX

  pthread_t pt;
  int Code=pthread_create(&pt,NULL,Proc,Data);
  if (Code!=0)
  {
    wchar Msg[100];
    swprintf(Msg,ASIZE(Msg),L"\npthread_create failed, code %d\n",Code);
    ErrHandler.GeneralErrMsg(Msg);
    ErrHandler.SysErrMsg();
    ErrHandler.Exit(RARX_FATAL);
  }
  return pt;
#else
  DWORD ThreadId;
  HANDLE hThread=CreateThread(NULL,0x10000,Proc,Data,0,&ThreadId);
  if (hThread==NULL)
  {
    ErrHandler.GeneralErrMsg(L"CreateThread failed");
    ErrHandler.SysErrMsg();
    ErrHandler.Exit(RARX_FATAL);
  }
  return hThread;
#endif
}

static void ThreadClose(THREAD_HANDLE hThread)
{
#ifdef _UNIX
  pthread_join(hThread,NULL);
#else
  CloseHandle(hThread);
#endif
}

#ifdef _WIN_ALL
static void CWaitForSingleObject(HANDLE hHandle)
{
  DWORD rc=WaitForSingleObject(hHandle,INFINITE);
  if (rc==WAIT_FAILED)
  {
    ErrHandler.GeneralErrMsg(L"\nWaitForMultipleObjects error %d, GetLastError %d",rc,GetLastError());
    ErrHandler.Exit(RARX_FATAL);
  }
}
#endif

#ifdef _UNIX
static void cpthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  int rc=pthread_cond_wait(cond,mutex);
  if (rc!=0)
  {
    ErrHandler.GeneralErrMsg(L"\npthread_cond_wait error %d",rc);
    ErrHandler.Exit(RARX_FATAL);
  }
}
#endif

uint GetNumberOfCPU()
{
#ifndef RAR_SMP
  return 1;
#else
#ifdef _UNIX
#ifdef _SC_NPROCESSORS_ONLN
  uint Count=(uint)sysconf(_SC_NPROCESSORS_ONLN);
  return Count<1 ? 1:Count;
#elif defined(_APPLE)
  uint Count;
  size_t Size=sizeof(Count);
  return sysctlbyname("hw.ncpu",&Count,&Size,NULL,0)==0 ? Count:1;
#endif
#else

#ifdef WIN32_CPU_GROUPS

  HMODULE hKernel=GetModuleHandle(L"kernel32.dll");
  if (hKernel!=nullptr)
  {
    typedef DWORD (WINAPI *GETACTIVEPROCESSORCOUNT)(WORD GroupNumber);
    GETACTIVEPROCESSORCOUNT pGetActiveProcessorCount=(GETACTIVEPROCESSORCOUNT)GetProcAddress(hKernel,"GetActiveProcessorCount");
    typedef WORD (WINAPI *GETACTIVEPROCESSORGROUPCOUNT)();
    GETACTIVEPROCESSORGROUPCOUNT pGetActiveProcessorGroupCount=(GETACTIVEPROCESSORGROUPCOUNT)GetProcAddress(hKernel,"GetActiveProcessorGroupCount");
    if (pGetActiveProcessorCount!=nullptr && pGetActiveProcessorGroupCount!=nullptr &&
        pGetActiveProcessorGroupCount()>1)
    {

      DWORD Count=pGetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
      return Count;
    }
  }
#endif

  DWORD_PTR ProcessMask;
  DWORD_PTR SystemMask;

  if (!GetProcessAffinityMask(GetCurrentProcess(),&ProcessMask,&SystemMask))
    return 1;
  uint Count=0;
  for (DWORD_PTR Mask=1;Mask!=0;Mask<<=1)
    if ((ProcessMask & Mask)!=0)
      Count++;
  return Count<1 ? 1:Count;
#endif

#endif
}

uint GetNumberOfThreads()
{
  uint NumCPU=GetNumberOfCPU();
  if (NumCPU<1)
    return 1;
  if (NumCPU>MaxPoolThreads)
    return MaxPoolThreads;
  return NumCPU;
}

#ifdef _WIN_ALL
int ThreadPool::ThreadPriority=THREAD_PRIORITY_NORMAL;
#endif

ThreadPool::ThreadPool(uint MaxThreads)
{
  MaxAllowedThreads = MaxThreads;
  if (MaxAllowedThreads>MaxPoolThreads)
    MaxAllowedThreads=MaxPoolThreads;
  if (MaxAllowedThreads==0)
    MaxAllowedThreads=1;

  ThreadsCreatedCount=0;

  if (MaxAllowedThreads>ASIZE(TaskQueue))
    MaxAllowedThreads=ASIZE(TaskQueue);

  Closing=false;

  bool Success = CriticalSectionCreate(&CritSection);
#ifdef _WIN_ALL
  QueuedTasksCnt=CreateSemaphore(NULL,0,ASIZE(TaskQueue),NULL);
  NoneActive=CreateEvent(NULL,TRUE,TRUE,NULL);
  Success=Success && QueuedTasksCnt!=NULL && NoneActive!=NULL;
#elif defined(_UNIX)
  AnyActive = false;
  QueuedTasksCnt = 0;
  Success=Success && pthread_cond_init(&AnyActiveCond,NULL)==0 &&
          pthread_mutex_init(&AnyActiveMutex,NULL)==0 &&
          pthread_cond_init(&QueuedTasksCntCond,NULL)==0 &&
          pthread_mutex_init(&QueuedTasksCntMutex,NULL)==0;
#endif
  if (!Success)
  {
    ErrHandler.GeneralErrMsg(L"\nThread pool initialization failed.");
    ErrHandler.Exit(RARX_FATAL);
  }

  QueueTop = 0;
  QueueBottom = 0;
  ActiveThreads = 0;
}

ThreadPool::~ThreadPool()
{
  WaitDone();
  Closing=true;

#ifdef _WIN_ALL
  ReleaseSemaphore(QueuedTasksCnt,ASIZE(TaskQueue),NULL);
#elif defined(_UNIX)

  pthread_mutex_lock(&QueuedTasksCntMutex);
  QueuedTasksCnt+=ASIZE(TaskQueue);
  pthread_mutex_unlock(&QueuedTasksCntMutex);

  pthread_cond_broadcast(&QueuedTasksCntCond);
#endif

  for(uint I=0;I<ThreadsCreatedCount;I++)
  {
#ifdef _WIN_ALL

    CWaitForSingleObject(ThreadHandles[I]);
#endif

    ThreadClose(ThreadHandles[I]);
  }

  CriticalSectionDelete(&CritSection);
#ifdef _WIN_ALL
  CloseHandle(QueuedTasksCnt);
  CloseHandle(NoneActive);
#elif defined(_UNIX)
  pthread_cond_destroy(&AnyActiveCond);
  pthread_mutex_destroy(&AnyActiveMutex);
  pthread_cond_destroy(&QueuedTasksCntCond);
  pthread_mutex_destroy(&QueuedTasksCntMutex);
#endif
}

void ThreadPool::CreateThreads()
{
#ifdef WIN32_CPU_GROUPS

  uint GroupCount=0;
  uint CurGroupNumber=(uint)-1;
  uint CurGroupSize=0,CumulativeGroupSize=0;

  typedef DWORD (WINAPI *GETACTIVEPROCESSORCOUNT)(WORD GroupNumber);
  GETACTIVEPROCESSORCOUNT pGetActiveProcessorCount=nullptr;
  typedef BOOL (WINAPI *GETTHREADGROUPAFFINITY)(HANDLE hThread,PGROUP_AFFINITY GroupAffinity);
  GETTHREADGROUPAFFINITY pGetThreadGroupAffinity=nullptr;
  typedef BOOL (WINAPI *SETTHREADGROUPAFFINITY)(HANDLE hThread,const GROUP_AFFINITY *GroupAffinity,PGROUP_AFFINITY PreviousGroupAffinity);
  SETTHREADGROUPAFFINITY pSetThreadGroupAffinity=nullptr;

  if (!IsWindows11OrGreater())
  {
    HMODULE hKernel=GetModuleHandle(L"kernel32.dll");
    if (hKernel!=nullptr)
    {
      typedef WORD (WINAPI *GETACTIVEPROCESSORGROUPCOUNT)();
      GETACTIVEPROCESSORGROUPCOUNT pGetActiveProcessorGroupCount=(GETACTIVEPROCESSORGROUPCOUNT)GetProcAddress(hKernel,"GetActiveProcessorGroupCount");

      pGetActiveProcessorCount=(GETACTIVEPROCESSORCOUNT)GetProcAddress(hKernel,"GetActiveProcessorCount");
      pGetThreadGroupAffinity=(GETTHREADGROUPAFFINITY)GetProcAddress(hKernel,"GetThreadGroupAffinity");
      pSetThreadGroupAffinity=(SETTHREADGROUPAFFINITY)GetProcAddress(hKernel,"SetThreadGroupAffinity");

      if (pGetActiveProcessorCount!=nullptr && pGetActiveProcessorGroupCount!=nullptr &&
          pGetThreadGroupAffinity!=nullptr && pSetThreadGroupAffinity!=nullptr)
        GroupCount=pGetActiveProcessorGroupCount();
    }
  }
#endif

  for (uint I=0;I<MaxAllowedThreads;I++)
  {
    THREAD_HANDLE hThread=ThreadCreate(PoolThread, this);
    ThreadHandles[I] = hThread;
    ThreadsCreatedCount++;
#ifdef _WIN_ALL
#ifdef WIN32_CPU_GROUPS
    if (GroupCount>1)
    {
      if (I>=CumulativeGroupSize)
      {
        if (++CurGroupNumber>=GroupCount)
        {

          CurGroupNumber=0;
          CumulativeGroupSize=0;
        }

        CurGroupSize=pGetActiveProcessorCount(CurGroupNumber);

        CumulativeGroupSize+=CurGroupSize;
      }
      GROUP_AFFINITY GroupAffinity;
      pGetThreadGroupAffinity(hThread,&GroupAffinity);

      uint SrcGroupSize=pGetActiveProcessorCount(GroupAffinity.Group);

      KAFFINITY SrcGroupMask=(KAFFINITY)(SrcGroupSize==64 ? (uint64)0xffffffffffffffff:(uint64(1)<<SrcGroupSize)-1);

      if (SrcGroupSize!=0 && GroupAffinity.Mask==SrcGroupMask &&
          GroupAffinity.Group!=CurGroupNumber && SrcGroupSize<MaxAllowedThreads)
      {

        KAFFINITY CurGroupMask=(KAFFINITY)(CurGroupSize==64 ? (uint64)0xffffffffffffffff:(uint64(1)<<CurGroupSize)-1);
        GroupAffinity.Mask=CurGroupMask;
        GroupAffinity.Group=CurGroupNumber;

        pSetThreadGroupAffinity(hThread,&GroupAffinity,NULL);
      }
    }
#endif

    if (ThreadPool::ThreadPriority!=THREAD_PRIORITY_NORMAL)
      SetThreadPriority(ThreadHandles[I],ThreadPool::ThreadPriority);
#endif
  }
}

NATIVE_THREAD_TYPE ThreadPool::PoolThread(void *Param)
{
  ((ThreadPool*)Param)->PoolThreadLoop();
  return 0;
}

void ThreadPool::PoolThreadLoop()
{
  QueueEntry Task;
  while (GetQueuedTask(&Task))
  {
    Task.Proc(Task.Param);

    CriticalSectionStart(&CritSection);
    if (--ActiveThreads == 0)
    {
#ifdef _WIN_ALL
      SetEvent(NoneActive);
#elif defined(_UNIX)
      pthread_mutex_lock(&AnyActiveMutex);
      AnyActive=false;
      pthread_cond_signal(&AnyActiveCond);
      pthread_mutex_unlock(&AnyActiveMutex);
#endif
    }
    CriticalSectionEnd(&CritSection);
  }
}

bool ThreadPool::GetQueuedTask(QueueEntry *Task)
{
#ifdef _WIN_ALL
  CWaitForSingleObject(QueuedTasksCnt);
#elif defined(_UNIX)
  pthread_mutex_lock(&QueuedTasksCntMutex);
  while (QueuedTasksCnt==0)
    cpthread_cond_wait(&QueuedTasksCntCond,&QueuedTasksCntMutex);
  QueuedTasksCnt--;
  pthread_mutex_unlock(&QueuedTasksCntMutex);
#endif

  if (Closing)
    return false;

  CriticalSectionStart(&CritSection);

  *Task = TaskQueue[QueueBottom];
  QueueBottom = (QueueBottom + 1) % ASIZE(TaskQueue);

  CriticalSectionEnd(&CritSection);

  return true;
}

void ThreadPool::AddTask(PTHREAD_PROC Proc,void *Data)
{
  if (ThreadsCreatedCount == 0)
    CreateThreads();

  if (ActiveThreads>=ASIZE(TaskQueue))
    WaitDone();

  TaskQueue[QueueTop].Proc = Proc;
  TaskQueue[QueueTop].Param = Data;
  QueueTop = (QueueTop + 1) % ASIZE(TaskQueue);
  ActiveThreads++;
}

void ThreadPool::WaitDone()
{
  if (ActiveThreads==0)
    return;
#ifdef _WIN_ALL
  ResetEvent(NoneActive);
  ReleaseSemaphore(QueuedTasksCnt,ActiveThreads,NULL);
  CWaitForSingleObject(NoneActive);
#elif defined(_UNIX)
  AnyActive=true;

  pthread_mutex_lock(&QueuedTasksCntMutex);
  QueuedTasksCnt+=ActiveThreads;
  pthread_mutex_unlock(&QueuedTasksCntMutex);

  pthread_cond_broadcast(&QueuedTasksCntCond);

  pthread_mutex_lock(&AnyActiveMutex);
  while (AnyActive)
    cpthread_cond_wait(&AnyActiveCond,&AnyActiveMutex);
  pthread_mutex_unlock(&AnyActiveMutex);
#endif
}
#endif

void RarTime::GetLocal(RarLocalTime *lt)
{
#ifdef _WIN_ALL
  FILETIME ft;
  GetWinFT(&ft);
  FILETIME lft;

  if (WinNT() < WNT_VISTA)
  {

    FileTimeToLocalFileTime(&ft,&lft);
  }
  else
  {

    SYSTEMTIME st1,st2;
    FileTimeToSystemTime(&ft,&st1);
    SystemTimeToTzSpecificLocalTime(NULL,&st1,&st2);
    SystemTimeToFileTime(&st2,&lft);

    FILETIME rft;
    SystemTimeToFileTime(&st1,&rft);
    uint64 Corrected=INT32TO64(ft.dwHighDateTime,ft.dwLowDateTime)-
                     INT32TO64(rft.dwHighDateTime,rft.dwLowDateTime)+
                     INT32TO64(lft.dwHighDateTime,lft.dwLowDateTime);
    lft.dwLowDateTime=(DWORD)Corrected;
    lft.dwHighDateTime=(DWORD)(Corrected>>32);
  }

  SYSTEMTIME st;
  FileTimeToSystemTime(&lft,&st);
  lt->Year=st.wYear;
  lt->Month=st.wMonth;
  lt->Day=st.wDay;
  lt->Hour=st.wHour;
  lt->Minute=st.wMinute;
  lt->Second=st.wSecond;
  lt->wDay=st.wDayOfWeek;
  lt->yDay=lt->Day-1;

  static int mdays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  for (uint I=1;I<lt->Month && I<=ASIZE(mdays);I++)
    lt->yDay+=mdays[I-1];

  if (lt->Month>2 && IsLeapYear(lt->Year))
    lt->yDay++;
#else
  time_t ut=GetUnix();
  struct tm *t;
  t=localtime(&ut);

  lt->Year=t->tm_year+1900;
  lt->Month=t->tm_mon+1;
  lt->Day=t->tm_mday;
  lt->Hour=t->tm_hour;
  lt->Minute=t->tm_min;
  lt->Second=t->tm_sec;
  lt->wDay=t->tm_wday;
  lt->yDay=t->tm_yday;
#endif
  lt->Reminder=(itime % TICKS_PER_SECOND);
}

void RarTime::SetLocal(RarLocalTime *lt)
{
#ifdef _WIN_ALL
  SYSTEMTIME st;
  st.wYear=(WORD)lt->Year;
  st.wMonth=(WORD)lt->Month;
  st.wDay=(WORD)lt->Day;
  st.wHour=(WORD)lt->Hour;
  st.wMinute=(WORD)lt->Minute;
  st.wSecond=(WORD)lt->Second;
  st.wMilliseconds=0;
  st.wDayOfWeek=0;
  FILETIME lft;
  if (SystemTimeToFileTime(&st,&lft))
  {
    FILETIME ft;

    if (WinNT() < WNT_VISTA)
    {

      LocalFileTimeToFileTime(&lft,&ft);
    }
    else
    {

      SYSTEMTIME st1,st2;
      FileTimeToSystemTime(&lft,&st2);
      TzSpecificLocalTimeToSystemTime(NULL,&st2,&st1);
      SystemTimeToFileTime(&st1,&ft);

      FILETIME rft;
      SystemTimeToFileTime(&st2,&rft);
      uint64 Corrected=INT32TO64(lft.dwHighDateTime,lft.dwLowDateTime)-
                       INT32TO64(rft.dwHighDateTime,rft.dwLowDateTime)+
                       INT32TO64(ft.dwHighDateTime,ft.dwLowDateTime);
      ft.dwLowDateTime=(DWORD)Corrected;
      ft.dwHighDateTime=(DWORD)(Corrected>>32);
    }

    SetWinFT(&ft);
  }
  else
    Reset();
#else
  struct tm t;

  t.tm_sec=lt->Second;
  t.tm_min=lt->Minute;
  t.tm_hour=lt->Hour;
  t.tm_mday=lt->Day;
  t.tm_mon=lt->Month-1;
  t.tm_year=lt->Year-1900;
  t.tm_isdst=-1;
  SetUnix(mktime(&t));
#endif
  itime+=lt->Reminder;
}

#ifdef _WIN_ALL
void RarTime::GetWinFT(FILETIME *ft)
{
  _ULARGE_INTEGER ul;
  ul.QuadPart=GetWin();
  ft->dwLowDateTime=ul.LowPart;
  ft->dwHighDateTime=ul.HighPart;
}

void RarTime::SetWinFT(FILETIME *ft)
{
  _ULARGE_INTEGER ul = {ft->dwLowDateTime, ft->dwHighDateTime};
  SetWin(ul.QuadPart);
}
#endif

uint64 RarTime::GetWin()
{
  return itime/(TICKS_PER_SECOND/10000000);
}

void RarTime::SetWin(uint64 WinTime)
{
  itime=WinTime*(TICKS_PER_SECOND/10000000);
}

time_t RarTime::GetUnix()
{
  return time_t(GetUnixNS()/1000000000);
}

void RarTime::SetUnix(time_t ut)
{
  if (sizeof(ut)>4)
    SetUnixNS(uint64(ut)*1000000000);
  else
  {

    SetUnixNS(uint64(uint32(ut))*1000000000);
  }
}

uint64 RarTime::GetUnixNS()
{
  const uint64 ushift=11644473600000000000ULL;
  return itime*(1000000000/TICKS_PER_SECOND)-ushift;
}

void RarTime::SetUnixNS(uint64 ns)
{
  const uint64 ushift=11644473600000000000ULL;
  itime=(ns+ushift)/(1000000000/TICKS_PER_SECOND);
}

uint RarTime::GetDos()
{
  RarLocalTime lt;
  GetLocal(&lt);
  uint DosTime=(lt.Second/2)|(lt.Minute<<5)|(lt.Hour<<11)|
               (lt.Day<<16)|(lt.Month<<21)|((lt.Year-1980)<<25);
  return DosTime;
}

void RarTime::SetDos(uint DosTime)
{
  RarLocalTime lt;
  lt.Second=(DosTime & 0x1f)*2;
  lt.Minute=(DosTime>>5) & 0x3f;
  lt.Hour=(DosTime>>11) & 0x1f;
  lt.Day=(DosTime>>16) & 0x1f;
  lt.Month=(DosTime>>21) & 0x0f;
  lt.Year=(DosTime>>25)+1980;
  lt.Reminder=0;
  SetLocal(&lt);
}

void RarTime::GetText(wchar *DateStr,size_t MaxSize,bool FullMS)
{
  if (IsSet())
  {
    RarLocalTime lt;
    GetLocal(&lt);
    if (FullMS)
      swprintf(DateStr,MaxSize,L"%u-%02u-%02u %02u:%02u:%02u,%09u",lt.Year,lt.Month,lt.Day,lt.Hour,lt.Minute,lt.Second,lt.Reminder*(1000000000/TICKS_PER_SECOND));
    else
      swprintf(DateStr,MaxSize,L"%u-%02u-%02u %02u:%02u",lt.Year,lt.Month,lt.Day,lt.Hour,lt.Minute);
  }
  else
  {

    wcsncpyz(DateStr,L"\?\?\?\?-\?\?-\?\? \?\?:\?\?",MaxSize);
  }
}

#ifndef SFX_MODULE
void RarTime::SetIsoText(const wchar *TimeText)
{
  int Field[6];
  memset(Field,0,sizeof(Field));
  for (uint DigitCount=0;*TimeText!=0;TimeText++)
    if (IsDigit(*TimeText))
    {
      int FieldPos=DigitCount<4 ? 0:(DigitCount-4)/2+1;
      if (FieldPos<ASIZE(Field))
        Field[FieldPos]=Field[FieldPos]*10+*TimeText-'0';
      DigitCount++;
    }
  RarLocalTime lt;
  lt.Second=Field[5];
  lt.Minute=Field[4];
  lt.Hour=Field[3];
  lt.Day=Field[2]==0 ? 1:Field[2];
  lt.Month=Field[1]==0 ? 1:Field[1];
  lt.Year=Field[0];
  lt.Reminder=0;
  SetLocal(&lt);
}
#endif

#ifndef SFX_MODULE
void RarTime::SetAgeText(const wchar *TimeText)
{
  uint Seconds=0,Value=0;
  for (uint I=0;TimeText[I]!=0;I++)
  {
    wchar Ch=TimeText[I];
    if (IsDigit(Ch))
      Value=Value*10+Ch-'0';
    else
    {
      switch(etoupperw(Ch))
      {
        case 'D':
          Seconds+=Value*24*3600;
          break;
        case 'H':
          Seconds+=Value*3600;
          break;
        case 'M':
          Seconds+=Value*60;
          break;
        case 'S':
          Seconds+=Value;
          break;
      }
      Value=0;
    }
  }
  SetCurrentTime();
  itime-=uint64(Seconds)*TICKS_PER_SECOND;
}
#endif

void RarTime::SetCurrentTime()
{
#ifdef _WIN_ALL
  FILETIME ft;
  SYSTEMTIME st;
  GetSystemTime(&st);
  SystemTimeToFileTime(&st,&ft);
  SetWinFT(&ft);
#else
  time_t st;
  time(&st);
  SetUnix(st);
#endif
}

void RarTime::Adjust(int64 ns)
{
  ns/=1000000000/TICKS_PER_SECOND;
  itime+=(uint64)ns;
}

#ifndef SFX_MODULE
const wchar *GetMonthName(uint Month)
{
  return uiGetMonthName(Month);
}
#endif

bool IsLeapYear(uint Year)
{
  return (Year&3)==0 && (Year%100!=0 || Year%400==0);
}

static bool GetAutoRenamedName(std::wstring &Name);
static SOUND_NOTIFY_MODE uiSoundNotify;

void uiInit(SOUND_NOTIFY_MODE Sound)
{
  uiSoundNotify = Sound;
}

UIASKREP_RESULT uiAskReplaceEx(CommandData *Cmd,std::wstring &Name,int64 FileSize,RarTime *FileTime,uint Flags)
{
  if (Cmd->Overwrite==OVERWRITE_NONE)
    return UIASKREP_R_SKIP;

#if !defined(SFX_MODULE) && !defined(SILENT)

  if (Cmd->Overwrite==OVERWRITE_AUTORENAME && GetAutoRenamedName(Name))
    return UIASKREP_R_REPLACE;
#endif

  std::wstring NewName=Name;
  UIASKREP_RESULT Choice=Cmd->AllYes || Cmd->Overwrite==OVERWRITE_ALL ?
                  UIASKREP_R_REPLACE : uiAskReplace(NewName,FileSize,FileTime,Flags);

  if (Choice==UIASKREP_R_REPLACE || Choice==UIASKREP_R_REPLACEALL)
  {
    PrepareToDelete(Name);

    FindData FD;
    if (FindFile::FastFind(Name,&FD,true) && FD.IsLink)
      DelFile(Name);
  }

  if (Choice==UIASKREP_R_REPLACEALL)
  {
    Cmd->Overwrite=OVERWRITE_ALL;
    return UIASKREP_R_REPLACE;
  }
  if (Choice==UIASKREP_R_SKIPALL)
  {
    Cmd->Overwrite=OVERWRITE_NONE;
    return UIASKREP_R_SKIP;
  }
  if (Choice==UIASKREP_R_RENAME)
  {
    if (GetNamePos(NewName)==0)
      SetName(Name,NewName);
    else
      Name=NewName;
    if (FileExist(Name))
      return uiAskReplaceEx(Cmd,Name,FileSize,FileTime,Flags);
    return UIASKREP_R_REPLACE;
  }
#if !defined(SFX_MODULE) && !defined(SILENT)
  if (Choice==UIASKREP_R_RENAMEAUTO && GetAutoRenamedName(Name))
  {
    Cmd->Overwrite=OVERWRITE_AUTORENAME;
    return UIASKREP_R_REPLACE;
  }
#endif
  return Choice;
}

bool GetAutoRenamedName(std::wstring &Name)
{
  std::wstring Ext=GetExt(Name);
  for (uint FileVer=1;FileVer<1000000;FileVer++)
  {
    std::wstring NewName=Name;
    RemoveExt(NewName);
    wchar Ver[10];
    itoa(FileVer,Ver,ASIZE(Ver));
    NewName = NewName + L"(" + Ver + L")" + Ext;
    if (!FileExist(NewName))
    {
      Name=NewName;
      return true;
    }
  }
  return false;
}

#ifdef SILENT

UIASKREP_RESULT uiAskReplace(std::wstring &Name,int64 FileSize,RarTime *FileTime,uint Flags)
{
  return UIASKREP_R_REPLACE;
}

void uiStartArchiveExtract(bool Extract,const std::wstring &ArcName)
{
}

bool uiStartFileExtract(const std::wstring &FileName,bool Extract,bool Test,bool Skip)
{
  return true;
}

void uiExtractProgress(int64 CurFileSize,int64 TotalFileSize,int64 CurSize,int64 TotalSize)
{
}

void uiProcessProgress(const char *Command,int64 CurSize,int64 TotalSize)
{
}

void uiMsgStore::Msg()
{
}

bool uiGetPassword(UIPASSWORD_TYPE Type,const std::wstring &FileName,
                   SecPassword *Password,CheckPassword *CheckPwd)
{
  return false;
}

bool uiIsGlobalPasswordSet()
{
  return false;
}

void uiAlarm(UIALARM_TYPE Type)
{
}

bool uiIsAborted()
{
  return false;
}

void uiGiveTick()
{
}

bool uiDictLimit(CommandData *Cmd,const std::wstring &FileName,uint64 DictSize,uint64 MaxDictSize)
{
#ifdef RARDLL
  if (Cmd->Callback!=nullptr &&
      Cmd->Callback(UCM_LARGEDICT,Cmd->UserData,(LPARAM)(DictSize/1024),(LPARAM)(MaxDictSize/1024))==1)
    return true;
#endif
  return false;
}

#ifndef SFX_MODULE
const wchar *uiGetMonthName(uint Month)
{
  return L"";
}
#endif

void uiEolAfterMsg()
{
}

#else

static bool AnyMessageDisplayed=false;

UIASKREP_RESULT uiAskReplace(std::wstring &Name,int64 FileSize,RarTime *FileTime,uint Flags)
{
  wchar SizeText1[20],DateStr1[50],SizeText2[20],DateStr2[50];

  FindData ExistingFD={};
  FindFile::FastFind(Name,&ExistingFD);
  itoa(ExistingFD.Size,SizeText1,ASIZE(SizeText1));
  ExistingFD.mtime.GetText(DateStr1,ASIZE(DateStr1),false);

  if (FileSize==INT64NDF || FileTime==NULL)
  {
    eprintf(L"\n");
    eprintf(St(MAskOverwrite),Name.c_str());
  }
  else
  {
    itoa(FileSize,SizeText2,ASIZE(SizeText2));
    FileTime->GetText(DateStr2,ASIZE(DateStr2),false);
    if ((Flags & UIASKREP_F_EXCHSRCDEST)==0)
      eprintf(St(MAskReplace),Name.c_str(),SizeText1,DateStr1,SizeText2,DateStr2);
    else
      eprintf(St(MAskReplace),Name.c_str(),SizeText2,DateStr2,SizeText1,DateStr1);
  }

  bool AllowRename=(Flags & UIASKREP_F_NORENAME)==0;
  int Choice=0;
  do
  {
    Choice=Ask(St(AllowRename ? MYesNoAllRenQ : MYesNoAllQ));
  } while (Choice==0);
  switch(Choice)
  {
    case 1:
      return UIASKREP_R_REPLACE;
    case 2:
      return UIASKREP_R_SKIP;
    case 3:
      return UIASKREP_R_REPLACEALL;
    case 4:
      return UIASKREP_R_SKIPALL;
  }
  if (AllowRename && Choice==5)
  {
    mprintf(St(MAskNewName));
    getwstr(Name);
    return UIASKREP_R_RENAME;
  }
  return UIASKREP_R_CANCEL;
}

void uiStartArchiveExtract(bool Extract,const std::wstring &ArcName)
{
  mprintf(St(Extract ? MExtracting : MExtrTest), ArcName.c_str());
}

bool uiStartFileExtract(const std::wstring &FileName,bool Extract,bool Test,bool Skip)
{
  return true;
}

void uiExtractProgress(int64 CurFileSize,int64 TotalFileSize,int64 CurSize,int64 TotalSize)
{

  int CurPercent=TotalSize!=0 ? ToPercent(CurSize,TotalSize) : ToPercent(CurFileSize,TotalFileSize);
  mprintf(L"\b\b\b\b%3d%%",CurPercent);
}

void uiProcessProgress(const char *Command,int64 CurSize,int64 TotalSize)
{
  int CurPercent=ToPercent(CurSize,TotalSize);
  mprintf(L"\b\b\b\b%3d%%",CurPercent);
}

void uiMsgStore::Msg()
{

  AnyMessageDisplayed=(Code!=UIEVENT_NEWARCHIVE && Code!=UIEVENT_RRTESTINGEND);

  switch(Code)
  {
    case UIERROR_SYSERRMSG:
    case UIERROR_GENERALERRMSG:
      Log(NULL,L"\n%ls",Str[0]);
      break;
    case UIERROR_CHECKSUM:
      Log(Str[0],St(MCRCFailed),Str[1]);
      break;
    case UIERROR_CHECKSUMENC:
      Log(Str[0],St(MEncrBadCRC),Str[1]);
      break;
    case UIERROR_CHECKSUMPACKED:
      Log(Str[0],St(MDataBadCRC),Str[1],Str[0]);
      break;
    case UIERROR_BADPSW:
      Log(Str[0],St(MWrongFilePassword),Str[1]);
      break;
    case UIWAIT_BADPSW:
      Log(Str[0],St(MWrongPassword));
      break;
    case UIERROR_MEMORY:
      mprintf(L"\n");
      Log(NULL,St(MErrOutMem));
      break;
    case UIERROR_FILEOPEN:
      Log(Str[0],St(MCannotOpen),Str[1]);
      break;
    case UIERROR_FILECREATE:
      Log(Str[0],St(MCannotCreate),Str[1]);
      break;
    case UIERROR_FILECLOSE:
      Log(NULL,St(MErrFClose),Str[0]);
      break;
    case UIERROR_FILESEEK:
      Log(NULL,St(MErrSeek),Str[0]);
      break;
    case UIERROR_FILEREAD:
      mprintf(L"\n");
      Log(Str[0],St(MErrRead),Str[1]);
      break;
    case UIERROR_FILEWRITE:
      Log(Str[0],St(MErrWrite),Str[1]);
      break;
#ifndef SFX_MODULE
    case UIERROR_FILEDELETE:
      Log(Str[0],St(MCannotDelete),Str[1]);
      break;
    case UIERROR_RECYCLEFAILED:
      Log(Str[0],St(MRecycleFailed));
      break;
    case UIERROR_FILERENAME:
      Log(Str[0],St(MErrRename),Str[1],Str[2]);
      break;
#endif
    case UIERROR_FILEATTR:
      Log(Str[0],St(MErrChangeAttr),Str[1]);
      break;
    case UIERROR_FILECOPY:
      Log(Str[0],St(MCopyError),Str[1],Str[2]);
      break;
    case UIERROR_FILECOPYHINT:
      Log(Str[0],St(MCopyErrorHint));
      mprintf(L"     ");
      break;
    case UIERROR_DIRCREATE:
      Log(Str[0],St(MExtrErrMkDir),Str[1]);
      break;
    case UIERROR_SLINKCREATE:
      Log(Str[0],St(MErrCreateLnkS),Str[1]);
      break;
    case UIERROR_HLINKCREATE:
      Log(NULL,St(MErrCreateLnkH),Str[0]);
      break;
    case UIERROR_NOLINKTARGET:
      Log(NULL,St(MErrLnkTarget));
      mprintf(L"     ");
      break;
    case UIERROR_NEEDADMIN:
      Log(NULL,St(MNeedAdmin));
      break;
    case UIERROR_ARCBROKEN:
      mprintf(L"\n");
      Log(Str[0],St(MErrBrokenArc));
      break;
    case UIERROR_HEADERBROKEN:
      Log(Str[0],St(MHeaderBroken));
      break;
    case UIERROR_MHEADERBROKEN:
      Log(Str[0],St(MMainHeaderBroken));
      break;
    case UIERROR_FHEADERBROKEN:
      Log(Str[0],St(MLogFileHead),Str[1]);
      break;
    case UIERROR_SUBHEADERBROKEN:
      Log(Str[0],St(MSubHeadCorrupt));
      break;
    case UIERROR_SUBHEADERUNKNOWN:
      Log(Str[0],St(MSubHeadUnknown));
      break;
    case UIERROR_SUBHEADERDATABROKEN:
      Log(Str[0],St(MSubHeadDataCRC),Str[1]);
      break;
    case UIERROR_RRDAMAGED:
      Log(Str[0],St(MRRDamaged));
      break;
    case UIERROR_UNKNOWNMETHOD:
      Log(Str[0],St(MUnknownMeth),Str[1]);
      break;
    case UIERROR_UNKNOWNENCMETHOD:
      {
        wchar Msg[256];
        swprintf(Msg,ASIZE(Msg),St(MUnkEncMethod),Str[1]);
        Log(Str[0],L"%s: %s",Msg,Str[2]);
      }
      break;
#ifndef SFX_MODULE
   case UIERROR_RENAMING:
      Log(Str[0],St(MRenaming),Str[1],Str[2]);
      break;
    case UIERROR_NEWERRAR:
      Log(Str[0],St(MNewerRAR));
      break;
#endif
    case UIERROR_RECVOLDIFFSETS:
      Log(NULL,St(MRecVolDiffSets),Str[0],Str[1]);
      break;
    case UIERROR_RECVOLALLEXIST:
      mprintf(St(MRecVolAllExist));
      break;
    case UIERROR_RECONSTRUCTING:
      mprintf(St(MReconstructing));
      break;
    case UIERROR_RECVOLCANNOTFIX:
      mprintf(St(MRecVolCannotFix));
      break;
    case UIERROR_EXTRDICTOUTMEM:
      Log(Str[0],St(MExtrDictOutMem),Num[0]);
#ifdef _WIN_32
      Log(Str[0],St(MSuggest64bit));
#endif
      break;
    case UIERROR_UNEXPEOF:
      Log(Str[0],St(MLogUnexpEOF));
      break;
    case UIERROR_TRUNCSERVICE:
      {
        const wchar *Type=nullptr;
        if (wcscmp(Str[1],SUBHEAD_TYPE_QOPEN)==0)
          Type=St(MHeaderQO);
        else
          if (wcscmp(Str[1],SUBHEAD_TYPE_RR)==0)
            Type=St(MHeaderRR);
        if (Type!=nullptr)
          Log(Str[0],St(MTruncService),Type);
      }
      break;
    case UIERROR_BADARCHIVE:
      Log(Str[0],St(MBadArc),Str[0]);
      break;
    case UIERROR_CMTBROKEN:
      Log(Str[0],St(MLogCommBrk));
      break;
    case UIERROR_INVALIDNAME:
      Log(Str[0],St(MInvalidName),Str[1]);
      mprintf(L"\n");
      break;
#ifndef SFX_MODULE
    case UIERROR_OPFAILED:
      Log(NULL,St(MOpFailed));
      break;
    case UIERROR_NEWRARFORMAT:
      Log(Str[0],St(MNewRarFormat));
      break;
#endif
    case UIERROR_NOFILESTOEXTRACT:
      mprintf(St(MExtrNoFiles));
      break;
    case UIERROR_MISSINGVOL:
      Log(Str[0],St(MAbsNextVol),Str[0]);
      mprintf(L"     ");
      break;
#ifndef SFX_MODULE
    case UIERROR_NEEDPREVVOL:
      Log(Str[0],St(MUnpCannotMerge),Str[1]);
      break;
    case UIERROR_UNKNOWNEXTRA:
      Log(Str[0],St(MUnknownExtra),Str[1]);
      break;
    case UIERROR_CORRUPTEXTRA:
      Log(Str[0],St(MCorruptExtra),Str[1],Str[2]);
      break;
#endif
#if !defined(SFX_MODULE) && defined(_WIN_ALL)
    case UIERROR_NTFSREQUIRED:
      Log(NULL,St(MNTFSRequired),Str[0]);
      break;
#endif
#if !defined(SFX_MODULE) && defined(_WIN_ALL)
    case UIERROR_ACLBROKEN:
      Log(Str[0],St(MACLBroken),Str[1]);
      break;
    case UIERROR_ACLUNKNOWN:
      Log(Str[0],St(MACLUnknown),Str[1]);
      break;
    case UIERROR_ACLSET:
      Log(Str[0],St(MACLSetError),Str[1]);
      break;
    case UIERROR_STREAMBROKEN:
      Log(Str[0],St(MStreamBroken),Str[1]);
      break;
    case UIERROR_STREAMUNKNOWN:
      Log(Str[0],St(MStreamUnknown),Str[1]);
      break;
#endif
    case UIERROR_INCOMPATSWITCH:
      mprintf(St(MIncompatSwitch),Str[0],Num[0]);
      break;
    case UIERROR_PATHTOOLONG:
      Log(NULL,L"\n%ls%ls%ls",Str[0],Str[1],Str[2]);
      Log(NULL,St(MPathTooLong));
      break;
#ifndef SFX_MODULE
    case UIERROR_DIRSCAN:
      Log(NULL,St(MScanError),Str[0]);
      break;
#endif
    case UIERROR_UOWNERBROKEN:
      Log(Str[0],St(MOwnersBroken),Str[1]);
      break;
    case UIERROR_UOWNERGETOWNERID:
      Log(Str[0],St(MErrGetOwnerID),Str[1]);
      break;
    case UIERROR_UOWNERGETGROUPID:
      Log(Str[0],St(MErrGetGroupID),Str[1]);
      break;
    case UIERROR_UOWNERSET:
      Log(Str[0],St(MSetOwnersError),Str[1]);
      break;
    case UIERROR_ULINKREAD:
      Log(NULL,St(MErrLnkRead),Str[0]);
      break;
    case UIERROR_ULINKEXIST:
      Log(NULL,St(MSymLinkExists),Str[0]);
      break;
    case UIERROR_READERRTRUNCATED:
      Log(NULL,St(MErrReadTrunc),Str[0]);
      break;
    case UIERROR_READERRCOUNT:
      Log(NULL,St(MErrReadCount),Num[0]);
      break;
    case UIERROR_DIRNAMEEXISTS:
      Log(NULL,St(MDirNameExists));
      break;
    case UIERROR_TRUNCPSW:
      eprintf(St(MTruncPsw),Num[0]);
      eprintf(L"\n");
      break;
    case UIERROR_ADJUSTVALUE:
      Log(NULL,St(MAdjustValue),Str[0],Str[1]);
      break;
    case UIERROR_SKIPUNSAFELINK:
      Log(NULL,St(MSkipUnsafeLink),Str[0],Str[1]);
      break;

#ifndef SFX_MODULE
    case UIMSG_STRING:
      mprintf(L"\n%s",Str[0]);
      break;
#endif
    case UIMSG_CORRECTINGNAME:
      Log(Str[0],St(MCorrectingName));
      break;
    case UIMSG_BADARCHIVE:
      mprintf(St(MBadArc),Str[0]);
      break;
    case UIMSG_CREATING:
      mprintf(St(MCreating),Str[0]);
      break;
    case UIMSG_RENAMING:
      mprintf(St(MRenaming),Str[0],Str[1]);
      break;
    case UIMSG_RECVOLCALCCHECKSUM:
      mprintf(St(MCalcCRCAllVol));
      break;
    case UIMSG_RECVOLFOUND:
      mprintf(St(MRecVolFound),Num[0]);
      break;
    case UIMSG_RECVOLMISSING:
      mprintf(St(MRecVolMissing),Num[0]);
      break;
    case UIMSG_MISSINGVOL:
      mprintf(St(MAbsNextVol),Str[0]);
      break;
    case UIMSG_RECONSTRUCTING:
      mprintf(St(MReconstructing));
      break;
    case UIMSG_CHECKSUM:
      mprintf(St(MCRCFailed),Str[0]);
      break;
    case UIMSG_FAT32SIZE:
      mprintf(St(MFAT32Size));
      mprintf(L"     ");
      break;
    case UIMSG_SKIPENCARC:
      Log(NULL,St(MSkipEncArc),Str[0]);
      break;

    case UIEVENT_RRTESTINGSTART:
      mprintf(L"%s      ",St(MTestingRR));
      break;
  }
}

bool uiGetPassword(UIPASSWORD_TYPE Type,const std::wstring &FileName,
                   SecPassword *Password,CheckPassword *CheckPwd)
{

  return GetConsolePassword(Type,FileName,Password) && Password->IsSet();
}

bool uiIsGlobalPasswordSet()
{
  return false;
}

void uiAlarm(UIALARM_TYPE Type)
{
  if (uiSoundNotify==SOUND_NOTIFY_ON)
  {
    static clock_t LastTime=-10;
    if ((MonoClock()-LastTime)/CLOCKS_PER_SEC>5)
    {
#ifdef _WIN_ALL
      MessageBeep(-1);
#else
      putwchar('\007');
#endif
      LastTime=MonoClock();
    }
  }
}

bool uiAskNextVolume(std::wstring &VolName)
{
  eprintf(St(MAskNextVol),VolName.c_str());
  return Ask(St(MContinueQuit))!=2;
}

void uiAskRepeatRead(const std::wstring &FileName,bool &Ignore,bool &All,bool &Retry,bool &Quit)
{
  eprintf(St(MErrReadInfo));
  int Code=Ask(St(MIgnoreAllRetryQuit));

  Ignore=(Code==1);
  All=(Code==2);
  Quit=(Code==4);
  Retry=!Ignore && !All && !Quit;
}

bool uiAskRepeatWrite(const std::wstring &FileName,bool DiskFull)
{
  mprintf(L"\n");
  Log(NULL,St(DiskFull ? MNotEnoughDisk:MErrWrite),FileName.c_str());
  return Ask(St(MRetryAbort))==1;
}

bool uiDictLimit(CommandData *Cmd,const std::wstring &FileName,uint64 DictSize,uint64 MaxDictSize)
{
  mprintf(L"\n%s",FileName.c_str());
  const uint64 GB=1024*1024*1024;

  DictSize=DictSize/GB+(DictSize%GB!=0 ? 1:0);

  MaxDictSize/=GB;

  mprintf(St(MDictNotAllowed),(uint)DictSize,(uint)MaxDictSize,(uint)DictSize);
  mprintf(St(MDictExtrAnyway),(uint)DictSize,(uint)DictSize);
  mprintf(L"\n");
  return false;
}

#ifndef SFX_MODULE
const wchar *uiGetMonthName(uint Month)
{
  static MSGID MonthID[12]={
         MMonthJan,MMonthFeb,MMonthMar,MMonthApr,MMonthMay,MMonthJun,
         MMonthJul,MMonthAug,MMonthSep,MMonthOct,MMonthNov,MMonthDec
  };
  return St(MonthID[Month]);
}
#endif

void uiEolAfterMsg()
{
  if (AnyMessageDisplayed)
  {

    AnyMessageDisplayed=false;
    mprintf(L"\n");
  }
}

#endif

#define MBFUNCTIONS

#if defined(_UNIX) && defined(MBFUNCTIONS)

static bool WideToCharMap(const wchar *Src,char *Dest,size_t DestSize,bool &Success);
static void CharToWideMap(const char *Src,wchar *Dest,size_t DestSize,bool &Success);

static const uint MapAreaStart=0xE000;

static const uint MappedStringMark=0xFFFE;

#endif

bool WideToChar(const wchar *Src,char *Dest,size_t DestSize)
{
  bool RetCode=true;
  *Dest=0;

#ifdef _WIN_ALL
  if (WideCharToMultiByte(CP_ACP,0,Src,-1,Dest,(int)DestSize,NULL,NULL)==0)
    RetCode=false;

#elif defined(_APPLE)
  WideToUtf(Src,Dest,DestSize);

#elif defined(MBFUNCTIONS)
  if (!WideToCharMap(Src,Dest,DestSize,RetCode))
  {
    mbstate_t ps;
    memset (&ps, 0, sizeof(ps));
    const wchar *SrcParam=Src;

    size_t ResultingSize=wcsrtombs(Dest,&SrcParam,DestSize,&ps);

    if (ResultingSize==(size_t)-1 && errno==EILSEQ)
    {

      memset (&ps, 0, sizeof(ps));
      SrcParam=Src;
      memset(Dest,0,DestSize);
      ResultingSize=wcsrtombs(Dest,&SrcParam,DestSize,&ps);
    }

    if (ResultingSize==(size_t)-1)
      RetCode=false;
    if (ResultingSize==0 && *Src!=0)
      RetCode=false;
  }
#else
  for (int I=0;I<DestSize;I++)
  {
    Dest[I]=(char)Src[I];
    if (Src[I]==0)
      break;
  }
#endif
  if (DestSize>0)
    Dest[DestSize-1]=0;

  return RetCode;
}

bool CharToWide(const char *Src,wchar *Dest,size_t DestSize)
{
  bool RetCode=true;
  *Dest=0;

#ifdef _WIN_ALL
  if (MultiByteToWideChar(CP_ACP,0,Src,-1,Dest,(int)DestSize)==0)
    RetCode=false;

#elif defined(_APPLE)
  UtfToWide(Src,Dest,DestSize);

#elif defined(MBFUNCTIONS)
  mbstate_t ps;
  memset (&ps, 0, sizeof(ps));
  const char *SrcParam=Src;
  size_t ResultingSize=mbsrtowcs(Dest,&SrcParam,DestSize,&ps);
  if (ResultingSize==(size_t)-1)
    RetCode=false;
  if (ResultingSize==0 && *Src!=0)
    RetCode=false;

  if (RetCode==false && DestSize>1)
    CharToWideMap(Src,Dest,DestSize,RetCode);
#else
  for (int I=0;I<DestSize;I++)
  {
    Dest[I]=(wchar_t)Src[I];
    if (Src[I]==0)
      break;
  }
#endif
  if (DestSize>0)
    Dest[DestSize-1]=0;

  return RetCode;
}

bool WideToChar(const std::wstring &Src,std::string &Dest)
{

  std::vector<char> DestA(4*Src.size()+1);
  bool Result=WideToChar(Src.c_str(),DestA.data(),DestA.size());
  Dest=DestA.data();
  return Result;
}

bool CharToWide(const std::string &Src,std::wstring &Dest)
{

  std::vector<wchar> DestW(2*Src.size()+1);
  bool Result=CharToWide(Src.c_str(),DestW.data(),DestW.size());
  Dest=DestW.data();
  return Result;
}

#if defined(_UNIX) && defined(MBFUNCTIONS)

bool WideToCharMap(const wchar *Src,char *Dest,size_t DestSize,bool &Success)
{

  if (wcschr(Src,(wchar)MappedStringMark)==NULL)
    return false;

  memset(Dest,0,DestSize);

  Success=true;
  uint SrcPos=0,DestPos=0;
  while (Src[SrcPos]!=0 && DestPos<DestSize-MB_CUR_MAX)
  {
    if (uint(Src[SrcPos])==MappedStringMark)
    {
      SrcPos++;
      continue;
    }

    if (uint(Src[SrcPos])>=MapAreaStart+0x80 && uint(Src[SrcPos])<MapAreaStart+0x100)
      Dest[DestPos++]=char(uint(Src[SrcPos++])-MapAreaStart);
    else
    {
      mbstate_t ps;
      memset(&ps,0,sizeof(ps));
      if (wcrtomb(Dest+DestPos,Src[SrcPos],&ps)==(size_t)-1)
      {
        Dest[DestPos]='_';
        Success=false;
      }
      SrcPos++;
      memset(&ps,0,sizeof(ps));
      int Length=mbrlen(Dest+DestPos,MB_CUR_MAX,&ps);
      DestPos+=Max(Length,1);
    }
  }
  Dest[Min(DestPos,DestSize-1)]=0;
  return true;
}
#endif

#if defined(_UNIX) && defined(MBFUNCTIONS)

void CharToWideMap(const char *Src,wchar *Dest,size_t DestSize,bool &Success)
{

  Success=false;
  bool MarkAdded=false;
  uint SrcPos=0,DestPos=0;
  while (DestPos<DestSize)
  {
    if (Src[SrcPos]==0)
    {
      Success=true;
      break;
    }
    mbstate_t ps;
    memset(&ps,0,sizeof(ps));
    size_t res=mbrtowc(Dest+DestPos,Src+SrcPos,MB_CUR_MAX,&ps);
    if (res==(size_t)-1 || res==(size_t)-2)
    {

      if (byte(Src[SrcPos])>=0x80)
      {
        if (!MarkAdded)
        {
          Dest[DestPos++]=MappedStringMark;
          MarkAdded=true;
          if (DestPos>=DestSize)
            break;
        }
        Dest[DestPos++]=byte(Src[SrcPos++])+MapAreaStart;
      }
      else
        break;
    }
    else
    {
      memset(&ps,0,sizeof(ps));
      int Length=mbrlen(Src+SrcPos,MB_CUR_MAX,&ps);
      SrcPos+=Max(Length,1);
      DestPos++;
    }
  }
  Dest[Min(DestPos,DestSize-1)]=0;
}
#endif

byte* WideToRaw(const wchar *Src,size_t SrcSize,byte *Dest,size_t DestSize)
{
  for (size_t I=0;I<SrcSize && I*2+1<DestSize;I++,Src++)
  {
    Dest[I*2]=(byte)*Src;
    Dest[I*2+1]=(byte)(*Src>>8);
    if (*Src==0)
      break;
  }
  return Dest;
}

void WideToRaw(const std::wstring &Src,std::vector<byte> &Dest)
{
  for (wchar C : Src)
  {
    Dest.push_back((byte)C);
    Dest.push_back((byte)(C>>8));
  }

}

wchar* RawToWide(const byte *Src,wchar *Dest,size_t DestSize)
{
  for (size_t I=0;I<DestSize;I++)
    if ((Dest[I]=Src[I*2]+(Src[I*2+1]<<8))==0)
      break;
  return Dest;
}

std::wstring RawToWide(const std::vector<byte> &Src)
{
  std::wstring Dest;
  for (size_t I=0;I+1<Src.size();I+=2)
  {
    wchar c=Src[I]+(Src[I+1]<<8);
    Dest.push_back(c);
    if (c==0)
      break;
  }
  return Dest;
}

void WideToUtf(const wchar *Src,char *Dest,size_t DestSize)
{
  long dsize=(long)DestSize;
  dsize--;
  while (*Src!=0 && --dsize>=0)
  {
    uint c=*(Src++);
    if (c<0x80)
      *(Dest++)=c;
    else
      if (c<0x800 && --dsize>=0)
      {
        *(Dest++)=(0xc0|(c>>6));
        *(Dest++)=(0x80|(c&0x3f));
      }
      else
      {
        if (c>=0xd800 && c<=0xdbff && *Src>=0xdc00 && *Src<=0xdfff)
        {
          c=((c-0xd800)<<10)+(*Src-0xdc00)+0x10000;
          Src++;
        }
        if (c<0x10000 && (dsize-=2)>=0)
        {
          *(Dest++)=(0xe0|(c>>12));
          *(Dest++)=(0x80|((c>>6)&0x3f));
          *(Dest++)=(0x80|(c&0x3f));
        }
        else
          if (c < 0x200000 && (dsize-=3)>=0)
          {
            *(Dest++)=(0xf0|(c>>18));
            *(Dest++)=(0x80|((c>>12)&0x3f));
            *(Dest++)=(0x80|((c>>6)&0x3f));
            *(Dest++)=(0x80|(c&0x3f));
          }
      }
  }
  *Dest=0;
}

void WideToUtf(const std::wstring &Src,std::string &Dest)
{
  for (size_t I=0;I<Src.size() && Src[I]!=0;)
  {
    uint c=Src[I++];
    if (c<0x80)
      Dest.push_back(c);
    else
      if (c<0x800)
      {
        Dest.push_back(0xc0|(c>>6));
        Dest.push_back(0x80|(c&0x3f));
      }
      else
      {
        if (c>=0xd800 && c<=0xdbff && I<Src.size() && Src[I]>=0xdc00 && Src[I]<=0xdfff)
        {
          c=((c-0xd800)<<10)+(Src[I]-0xdc00)+0x10000;
          I++;
        }
        if (c<0x10000)
        {
          Dest.push_back(0xe0|(c>>12));
          Dest.push_back(0x80|((c>>6)&0x3f));
          Dest.push_back(0x80|(c&0x3f));
        }
        else
          if (c < 0x200000)
          {
            Dest.push_back(0xf0|(c>>18));
            Dest.push_back(0x80|((c>>12)&0x3f));
            Dest.push_back(0x80|((c>>6)&0x3f));
            Dest.push_back(0x80|(c&0x3f));
          }
      }
  }
}

size_t WideToUtfSize(const wchar *Src)
{
  size_t Size=0;
  for (;*Src!=0;Src++)
    if (*Src<0x80)
      Size++;
    else
      if (*Src<0x800)
        Size+=2;
      else
        if ((uint)*Src<0x10000)
        {
          if (Src[0]>=0xd800 && Src[0]<=0xdbff && Src[1]>=0xdc00 && Src[1]<=0xdfff)
          {
            Size+=4;
            Src++;
          }
          else
            Size+=3;
        }
        else
          if ((uint)*Src<0x200000)
            Size+=4;
  return Size+1;
}

bool UtfToWide(const char *Src,wchar *Dest,size_t DestSize)
{
  bool Success=true;
  long dsize=(long)DestSize;
  dsize--;
  while (*Src!=0)
  {
    uint c=byte(*(Src++)),d;
    if (c<0x80)
      d=c;
    else
      if ((c>>5)==6)
      {
        if ((*Src&0xc0)!=0x80)
        {
          Success=false;
          break;
        }
        d=((c&0x1f)<<6)|(*Src&0x3f);
        Src++;
      }
      else
        if ((c>>4)==14)
        {
          if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80)
          {
            Success=false;
            break;
          }
          d=((c&0xf)<<12)|((Src[0]&0x3f)<<6)|(Src[1]&0x3f);
          Src+=2;
        }
        else
          if ((c>>3)==30)
          {
            if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80 || (Src[2]&0xc0)!=0x80)
            {
              Success=false;
              break;
            }
            d=((c&7)<<18)|((Src[0]&0x3f)<<12)|((Src[1]&0x3f)<<6)|(Src[2]&0x3f);
            Src+=3;
          }
          else
          {
            Success=false;
            break;
          }
    if (--dsize<0)
      break;
    if (d>0xffff)
    {
      if (--dsize<0)
        break;
      if (d>0x10ffff)
      {
        Success=false;
        continue;
      }
      if (sizeof(*Dest)==2)
      {
        *(Dest++)=((d-0x10000)>>10)+0xd800;
        *(Dest++)=(d&0x3ff)+0xdc00;
      }
      else
        *(Dest++)=d;
    }
    else
      *(Dest++)=d;
  }
  *Dest=0;
  return Success;
}

bool UtfToWide(const char *Src,std::wstring &Dest)
{
  bool Success=true;
  Dest.clear();
  while (*Src!=0)
  {
    uint c=byte(*(Src++)),d;
    if (c<0x80)
      d=c;
    else
      if ((c>>5)==6)
      {
        if ((*Src&0xc0)!=0x80)
        {
          Success=false;
          break;
        }
        d=((c&0x1f)<<6)|(*Src&0x3f);
        Src++;
      }
      else
        if ((c>>4)==14)
        {
          if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80)
          {
            Success=false;
            break;
          }
          d=((c&0xf)<<12)|((Src[0]&0x3f)<<6)|(Src[1]&0x3f);
          Src+=2;
        }
        else
          if ((c>>3)==30)
          {
            if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80 || (Src[2]&0xc0)!=0x80)
            {
              Success=false;
              break;
            }
            d=((c&7)<<18)|((Src[0]&0x3f)<<12)|((Src[1]&0x3f)<<6)|(Src[2]&0x3f);
            Src+=3;
          }
          else
          {
            Success=false;
            break;
          }
    if (d>0xffff)
    {
      if (d>0x10ffff)
      {
        Success=false;
        continue;
      }
      if (sizeof(wchar_t)==2)
      {
        Dest.push_back( ((d-0x10000)>>10)+0xd800 );
        Dest.push_back( (d&0x3ff)+0xdc00 );
      }
      else
        Dest.push_back( d );
    }
    else
      Dest.push_back( d );
  }
  return Success;
}

bool IsTextUtf8(const byte *Src)
{
  return IsTextUtf8(Src,strlen((const char *)Src));
}

bool IsTextUtf8(const byte *Src,size_t SrcSize)
{
  while (SrcSize-- > 0)
  {
    byte C=*(Src++);
    int HighOne=0;
    for (byte Mask=0x80;Mask!=0 && (C & Mask)!=0;Mask>>=1)
      HighOne++;
    if (HighOne==1 || HighOne>6)
      return false;
    while (--HighOne > 0)
      if (SrcSize-- <= 0 || (*(Src++) & 0xc0)!=0x80)
        return false;
  }
  return true;
}

int wcsicomp(const wchar *s1,const wchar *s2)
{

  bool FastMode=true;
  while (true)
  {

    bool u1=*s1>='A' && *s1<='Z', l1=*s1>='a' && *s1<='z', d1=*s1>='0' && *s1<='9';
    bool u2=*s2>='A' && *s2<='Z', l2=*s2>='a' && *s2<='z', d2=*s2>='0' && *s2<='9';

    if (!u1 && !l1 && !d1 && *s1!=0 && !u2 && !l2 && !d2 && *s2!=0)
    {
      FastMode=false;
      break;
    }

    wchar c1 = l1 ? *s1-'a'+'A' : *s1;
    wchar c2 = l2 ? *s2-'a'+'A' : *s2;

    if (c1 != c2)
      return c1 < c2 ? -1 : 1;

    if (*s1==0)
      break;
    s1++;
    s2++;
  }
  if (FastMode)
    return 0;

#ifdef _WIN_ALL
  return CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,-1,s2,-1)-2;
#else
  while (true)
  {
    wchar u1 = towupper(*s1);
    wchar u2 = towupper(*s2);

    if (u1 != u2)
      return u1 < u2 ? -1 : 1;
    if (*s1==0)
      break;
    s1++;
    s2++;
  }
  return 0;
#endif
}

int wcsnicomp(const wchar *s1,const wchar *s2,size_t n)
{
#ifdef _WIN_ALL

  size_t sl1=wcslen(s1);
  size_t l1=Min(sl1+1,n);
  size_t sl2=wcslen(s2);
  size_t l2=Min(sl2+1,n);
  return CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,(int)l1,s2,(int)l2)-2;
#else
  if (n==0)
    return 0;
  while (true)
  {
    wchar u1 = towupper(*s1);
    wchar u2 = towupper(*s2);
    if (u1 != u2)
      return u1 < u2 ? -1 : 1;
    if (*s1==0 || --n==0)
      break;
    s1++;
    s2++;
  }
  return 0;
#endif
}

const wchar_t* wcscasestr(const wchar_t *str, const wchar_t *search)
{
  for (size_t i=0;str[i]!=0;i++)
    for (size_t j=0;;j++)
    {
      if (search[j]==0)
        return str+i;
      if (tolowerw(str[i+j])!=tolowerw(search[j]))
        break;
    }
  return nullptr;
}

std::wstring::size_type wcscasestr(const std::wstring &str, const std::wstring &search)
{
  const wchar *Found=wcscasestr(str.c_str(),search.c_str());
  return Found==nullptr ? std::wstring::npos : Found-str.c_str();
}

#ifndef SFX_MODULE
wchar* wcslower(wchar *s)
{
#ifdef _WIN_ALL

  CharLower(s);
#else
  for (wchar *c=s;*c!=0;c++)
    *c=towlower(*c);
#endif
  return s;
}

void wcslower(std::wstring &s)
{
  wcslower(&s[0]);
}

wchar* wcsupper(wchar *s)
{
#ifdef _WIN_ALL

  CharUpper(s);
#else
  for (wchar *c=s;*c!=0;c++)
    *c=towupper(*c);
#endif
  return s;
}

void wcsupper(std::wstring &s)
{
  wcsupper(&s[0]);
}
#endif

int toupperw(int ch)
{
#if defined(_WIN_ALL)

  return (int)(INT_PTR)CharUpper((wchar *)(INT_PTR)(ch&0xffff));
#else
  return towupper(ch);
#endif
}

int tolowerw(int ch)
{
#if defined(_WIN_ALL)

  return (int)(INT_PTR)CharLower((wchar *)(INT_PTR)(ch&0xffff));
#else
  return towlower(ch);
#endif
}

int atoiw(const std::wstring &s)
{
  return (int)atoilw(s);
}

int64 atoilw(const std::wstring &s)
{
  bool sign=false;
  size_t Pos=0;
  if (s[Pos]=='-')
  {
    Pos++;
    sign=true;
  }

  uint64 n=0;
  while (s[Pos]>='0' && s[Pos]<='9')
  {
    n=n*10+(s[Pos]-'0');
    Pos++;
  }

  return sign && int64(n)>=0 ? -int64(n) : int64(n);
}

#ifdef DBCS_SUPPORTED
SupportDBCS gdbcs;

SupportDBCS::SupportDBCS()
{
  Init();
}

void SupportDBCS::Init()
{
  CPINFO CPInfo;
  GetCPInfo(CP_ACP,&CPInfo);
  DBCSMode=CPInfo.MaxCharSize > 1;
  for (uint I=0;I<ASIZE(IsLeadByte);I++)
    IsLeadByte[I]=IsDBCSLeadByte(I)!=0;
}

char* SupportDBCS::charnext(const char *s)
{

  return (char *)(IsLeadByte[(byte)*s] && s[1]!=0 ? s+2:s+1);
}
#endif

inline byte RangeCoder::GetChar()
{
  return UnpackRead->GetChar();
}

void RangeCoder::InitDecoder(Unpack *UnpackRead)
{
  RangeCoder::UnpackRead=UnpackRead;

  low=code=0;
  range=0xffffffff;
  for (uint i = 0; i < 4; i++)
    code=(code << 8) | GetChar();
}

#define ARI_DEC_NORMALIZE(code,low,range,read)                           \
{                                                                        \
  while ((low^(low+range))<TOP || range<BOT && ((range=-(int)low&(BOT-1)),1)) \
  {                                                                      \
    code=(code << 8) | read->GetChar();                                  \
    range <<= 8;                                                         \
    low <<= 8;                                                           \
  }                                                                      \
}

inline int RangeCoder::GetCurrentCount()
{
  return (code-low)/(range /= SubRange.scale);
}

inline uint RangeCoder::GetCurrentShiftCount(uint SHIFT)
{
  return (code-low)/(range >>= SHIFT);
}

inline void RangeCoder::Decode()
{
  low += range*SubRange.LowCount;
  range *= SubRange.HighCount-SubRange.LowCount;
}

static const uint UNIT_SIZE=Max(sizeof(RARPPM_CONTEXT),sizeof(RARPPM_MEM_BLK));
static const uint FIXED_UNIT_SIZE=12;

SubAllocator::SubAllocator()
{
  Clean();
}

void SubAllocator::Clean()
{
  SubAllocatorSize=0;
}

inline void SubAllocator::InsertNode(void* p,int indx)
{
  ((RAR_NODE*) p)->next=FreeList[indx].next;
  FreeList[indx].next=(RAR_NODE*) p;
}

inline void* SubAllocator::RemoveNode(int indx)
{
  RAR_NODE* RetVal=FreeList[indx].next;
  FreeList[indx].next=RetVal->next;
  return RetVal;
}

inline uint SubAllocator::U2B(int NU)
{

  return UNIT_SIZE*NU;
}

inline RARPPM_MEM_BLK* SubAllocator::MBPtr(RARPPM_MEM_BLK *BasePtr,int Items)
{
  return((RARPPM_MEM_BLK*)( ((byte *)(BasePtr))+U2B(Items) ));
}

inline void SubAllocator::SplitBlock(void* pv,int OldIndx,int NewIndx)
{
  int i, UDiff=Indx2Units[OldIndx]-Indx2Units[NewIndx];
  byte* p=((byte*) pv)+U2B(Indx2Units[NewIndx]);
  if (Indx2Units[i=Units2Indx[UDiff-1]] != UDiff)
  {
    InsertNode(p,--i);
    p += U2B(i=Indx2Units[i]);
    UDiff -= i;
  }
  InsertNode(p,Units2Indx[UDiff-1]);
}

void SubAllocator::StopSubAllocator()
{
  if ( SubAllocatorSize )
  {
    SubAllocatorSize=0;
    free(HeapStart);
  }
}

bool SubAllocator::StartSubAllocator(int SASize)
{
  uint t=SASize << 20;
  if (SubAllocatorSize == t)
    return true;
  StopSubAllocator();

  uint AllocSize=t/FIXED_UNIT_SIZE*UNIT_SIZE+2*UNIT_SIZE;
  if ((HeapStart=(byte *)malloc(AllocSize)) == NULL)
  {
    ErrHandler.MemoryError();
    return false;
  }

  HeapEnd=HeapStart+AllocSize-UNIT_SIZE;

  SubAllocatorSize=t;
  return true;
}

void SubAllocator::InitSubAllocator()
{
  int i, k;
  memset(FreeList,0,sizeof(FreeList));
  pText=HeapStart;

  uint Size2=FIXED_UNIT_SIZE*(SubAllocatorSize/8/FIXED_UNIT_SIZE*7);

  uint RealSize2=Size2/FIXED_UNIT_SIZE*UNIT_SIZE;

  uint Size1=SubAllocatorSize-Size2;

  uint RealSize1=Size1/FIXED_UNIT_SIZE*UNIT_SIZE+UNIT_SIZE;

  LoUnit=UnitsStart=HeapStart+RealSize1;

  FakeUnitsStart=HeapStart+Size1;

  HiUnit=LoUnit+RealSize2;
  for (i=0,k=1;i < N1     ;i++,k += 1)
    Indx2Units[i]=k;
  for (k++;i < N1+N2      ;i++,k += 2)
    Indx2Units[i]=k;
  for (k++;i < N1+N2+N3   ;i++,k += 3)
    Indx2Units[i]=k;
  for (k++;i < N1+N2+N3+N4;i++,k += 4)
    Indx2Units[i]=k;
  for (GlueCount=k=i=0;k < 128;k++)
  {
    i += (Indx2Units[i] < k+1);
    Units2Indx[k]=i;
  }
}

inline void SubAllocator::GlueFreeBlocks()
{
  RARPPM_MEM_BLK s0, * p, * p1;
  int i, k, sz;
  if (LoUnit != HiUnit)
    *LoUnit=0;
  for (i=0, s0.next=s0.prev=&s0;i < N_INDEXES;i++)
    while ( FreeList[i].next )
    {
      p=(RARPPM_MEM_BLK*)RemoveNode(i);
      p->insertAt(&s0);
      p->Stamp=0xFFFF;
      p->NU=Indx2Units[i];
    }
  for (p=s0.next;p != &s0;p=p->next)
    while ((p1=MBPtr(p,p->NU))->Stamp == 0xFFFF && int(p->NU)+p1->NU < 0x10000)
    {
      p1->remove();
      p->NU += p1->NU;
    }
  while ((p=s0.next) != &s0)
  {
    for (p->remove(), sz=p->NU;sz > 128;sz -= 128, p=MBPtr(p,128))
      InsertNode(p,N_INDEXES-1);
    if (Indx2Units[i=Units2Indx[sz-1]] != sz)
    {
      k=sz-Indx2Units[--i];
      InsertNode(MBPtr(p,sz-k),k-1);
    }
    InsertNode(p,i);
  }
}

void* SubAllocator::AllocUnitsRare(int indx)
{
  if ( !GlueCount )
  {
    GlueCount = 255;
    GlueFreeBlocks();
    if ( FreeList[indx].next )
      return RemoveNode(indx);
  }
  int i=indx;
  do
  {
    if (++i == N_INDEXES)
    {
      GlueCount--;
      i=U2B(Indx2Units[indx]);
      int j=FIXED_UNIT_SIZE*Indx2Units[indx];
      if (FakeUnitsStart - pText > j)
      {
        FakeUnitsStart -= j;
        UnitsStart -= i;
        return UnitsStart;
      }
      return NULL;
    }
  } while ( !FreeList[i].next );
  void* RetVal=RemoveNode(i);
  SplitBlock(RetVal,i,indx);
  return RetVal;
}

inline void* SubAllocator::AllocUnits(int NU)
{
  int indx=Units2Indx[NU-1];
  if ( FreeList[indx].next )
    return RemoveNode(indx);
  void* RetVal=LoUnit;
  LoUnit += U2B(Indx2Units[indx]);
  if (LoUnit <= HiUnit)
    return RetVal;
  LoUnit -= U2B(Indx2Units[indx]);
  return AllocUnitsRare(indx);
}

void* SubAllocator::AllocContext()
{
  if (HiUnit != LoUnit)
    return (HiUnit -= UNIT_SIZE);
  if ( FreeList->next )
    return RemoveNode(0);
  return AllocUnitsRare(0);
}

void* SubAllocator::ExpandUnits(void* OldPtr,int OldNU)
{
  int i0=Units2Indx[OldNU-1], i1=Units2Indx[OldNU-1+1];
  if (i0 == i1)
    return OldPtr;
  void* ptr=AllocUnits(OldNU+1);
  if ( ptr )
  {
    memcpy(ptr,OldPtr,U2B(OldNU));
    InsertNode(OldPtr,i0);
  }
  return ptr;
}

void* SubAllocator::ShrinkUnits(void* OldPtr,int OldNU,int NewNU)
{
  int i0=Units2Indx[OldNU-1], i1=Units2Indx[NewNU-1];
  if (i0 == i1)
    return OldPtr;
  if ( FreeList[i1].next )
  {
    void* ptr=RemoveNode(i1);
    memcpy(ptr,OldPtr,U2B(NewNU));
    InsertNode(OldPtr,i0);
    return ptr;
  }
  else
  {
    SplitBlock(OldPtr,i0,i1);
    return OldPtr;
  }
}

void SubAllocator::FreeUnits(void* ptr,int OldNU)
{
  InsertNode(ptr,Units2Indx[OldNU-1]);
}

static const int MAX_O=64;
const uint TOP=1 << 24, BOT=1 << 15;

template <class T>
inline void _PPMD_SWAP(T& t1,T& t2) { T tmp=t1; t1=t2; t2=tmp; }

inline RARPPM_CONTEXT* RARPPM_CONTEXT::createChild(ModelPPM *Model,RARPPM_STATE* pStats,
                                             RARPPM_STATE& FirstState)
{
  RARPPM_CONTEXT* pc = (RARPPM_CONTEXT*) Model->SubAlloc.AllocContext();
  if ( pc )
  {
    pc->NumStats=1;
    pc->OneState=FirstState;
    pc->Suffix=this;
    pStats->Successor=pc;
  }
  return pc;
}

ModelPPM::ModelPPM()
{
  MinContext=NULL;
  MaxContext=NULL;
  MedContext=NULL;
}

void ModelPPM::RestartModelRare()
{
  int i, k, m;
  memset(CharMask,0,sizeof(CharMask));
  SubAlloc.InitSubAllocator();
  InitRL=-(MaxOrder < 12 ? MaxOrder:12)-1;
  MinContext = MaxContext = (RARPPM_CONTEXT*) SubAlloc.AllocContext();
  if (MinContext == NULL)
    throw std::bad_alloc();
  MinContext->Suffix=NULL;
  OrderFall=MaxOrder;
  MinContext->U.SummFreq=(MinContext->NumStats=256)+1;
  FoundState=MinContext->U.Stats=(RARPPM_STATE*)SubAlloc.AllocUnits(256/2);
  if (FoundState == NULL)
    throw std::bad_alloc();
  for (RunLength=InitRL, PrevSuccess=i=0;i < 256;i++)
  {
    MinContext->U.Stats[i].Symbol=i;
    MinContext->U.Stats[i].Freq=1;
    MinContext->U.Stats[i].Successor=NULL;
  }

  static const ushort InitBinEsc[]={
    0x3CDD,0x1F3F,0x59BF,0x48F3,0x64A1,0x5ABC,0x6632,0x6051
  };

  for (i=0;i < 128;i++)
    for (k=0;k < 8;k++)
      for (m=0;m < 64;m += 8)
        BinSumm[i][k+m]=BIN_SCALE-InitBinEsc[k]/(i+2);
  for (i=0;i < 25;i++)
    for (k=0;k < 16;k++)
      SEE2Cont[i][k].init(5*i+10);
}

void ModelPPM::StartModelRare(int MaxOrder)
{
  int i, k, m ,Step;
  EscCount=1;

  {
    ModelPPM::MaxOrder=MaxOrder;
    RestartModelRare();
    NS2BSIndx[0]=2*0;
    NS2BSIndx[1]=2*1;
    memset(NS2BSIndx+2,2*2,9);
    memset(NS2BSIndx+11,2*3,256-11);
    for (i=0;i < 3;i++)
      NS2Indx[i]=i;
    for (m=i, k=Step=1;i < 256;i++)
    {
      NS2Indx[i]=m;
      if ( !--k )
      {
        k = ++Step;
        m++;
      }
    }
    memset(HB2Flag,0,0x40);
    memset(HB2Flag+0x40,0x08,0x100-0x40);
    DummySEE2Cont.Shift=PERIOD_BITS;
  }
}

void RARPPM_CONTEXT::rescale(ModelPPM *Model)
{
  int OldNS=NumStats, i=NumStats-1, Adder, EscFreq;
  RARPPM_STATE* p1, * p;
  for (p=Model->FoundState;p != U.Stats;p--)
    _PPMD_SWAP(p[0],p[-1]);
  U.Stats->Freq += 4;
  U.SummFreq += 4;
  EscFreq=U.SummFreq-p->Freq;
  Adder=(Model->OrderFall != 0);
  U.SummFreq = (p->Freq=(p->Freq+Adder) >> 1);
  do
  {
    EscFreq -= (++p)->Freq;
    U.SummFreq += (p->Freq=(p->Freq+Adder) >> 1);
    if (p[0].Freq > p[-1].Freq)
    {
      RARPPM_STATE tmp=*(p1=p);
      do
      {
        p1[0]=p1[-1];
      } while (--p1 != U.Stats && tmp.Freq > p1[-1].Freq);
      *p1=tmp;
    }
  } while ( --i );
  if (p->Freq == 0)
  {
    do
    {
      i++;
    } while ((--p)->Freq == 0);
    EscFreq += i;
    if ((NumStats -= i) == 1)
    {
      RARPPM_STATE tmp=*U.Stats;
      do
      {
        tmp.Freq-=(tmp.Freq >> 1);
        EscFreq>>=1;
      } while (EscFreq > 1);
      Model->SubAlloc.FreeUnits(U.Stats,(OldNS+1) >> 1);
      *(Model->FoundState=&OneState)=tmp;  return;
    }
  }
  U.SummFreq += (EscFreq -= (EscFreq >> 1));
  int n0=(OldNS+1) >> 1, n1=(NumStats+1) >> 1;
  if (n0 != n1)
    U.Stats = (RARPPM_STATE*) Model->SubAlloc.ShrinkUnits(U.Stats,n0,n1);
  Model->FoundState=U.Stats;
}

inline RARPPM_CONTEXT* ModelPPM::CreateSuccessors(bool Skip,RARPPM_STATE* p1)
{
  RARPPM_STATE UpState;
  RARPPM_CONTEXT* pc=MinContext, * UpBranch=FoundState->Successor;
  RARPPM_STATE * p, * ps[MAX_O], ** pps=ps;
  if ( !Skip )
  {
    *pps++ = FoundState;
    if ( !pc->Suffix )
      goto NO_LOOP;
  }
  if ( p1 )
  {
    p=p1;
    pc=pc->Suffix;
    goto LOOP_ENTRY;
  }
  do
  {
    pc=pc->Suffix;
    if (pc->NumStats != 1)
    {
      if ((p=pc->U.Stats)->Symbol != FoundState->Symbol)
        do
        {
          p++;
        } while (p->Symbol != FoundState->Symbol);
    }
    else
      p=&(pc->OneState);
LOOP_ENTRY:
    if (p->Successor != UpBranch)
    {
      pc=p->Successor;
      break;

    }

    if (pps>=ps+ASIZE(ps))
      return NULL;

    *pps++ = p;
  } while ( pc->Suffix );
NO_LOOP:
  if (pps == ps)
    return pc;
  UpState.Symbol=*(byte*) UpBranch;
  UpState.Successor=(RARPPM_CONTEXT*) (((byte*) UpBranch)+1);
  if (pc->NumStats != 1)
  {
    if ((byte*) pc <= SubAlloc.pText)
      return(NULL);
    if ((p=pc->U.Stats)->Symbol != UpState.Symbol)
    do
    {
      p++;
    } while (p->Symbol != UpState.Symbol);
    uint cf=p->Freq-1;
    uint s0=pc->U.SummFreq-pc->NumStats-cf;
    UpState.Freq=1+((2*cf <= s0)?(5*cf > s0):((2*cf+3*s0-1)/(2*s0)));
  }
  else
    UpState.Freq=pc->OneState.Freq;
  do
  {
    pc = pc->createChild(this,*--pps,UpState);
    if ( !pc )
      return NULL;
  } while (pps != ps);
  return pc;
}

inline void ModelPPM::UpdateModel()
{
  RARPPM_STATE fs = *FoundState, *p = NULL;
  RARPPM_CONTEXT *pc, *Successor;
  uint ns1, ns, cf, sf, s0;
  if (fs.Freq < MAX_FREQ/4 && (pc=MinContext->Suffix) != NULL)
  {
    if (pc->NumStats != 1)
    {
      if ((p=pc->U.Stats)->Symbol != fs.Symbol)
      {
        do
        {
          p++;
        } while (p->Symbol != fs.Symbol);
        if (p[0].Freq >= p[-1].Freq)
        {
          _PPMD_SWAP(p[0],p[-1]);
          p--;
        }
      }
      if (p->Freq < MAX_FREQ-9)
      {
        p->Freq += 2;
        pc->U.SummFreq += 2;
      }
    }
    else
    {
      p=&(pc->OneState);
      p->Freq += (p->Freq < 32);
    }
  }
  if ( !OrderFall )
  {
    MinContext=MaxContext=FoundState->Successor=CreateSuccessors(TRUE,p);
    if ( !MinContext )
      goto RESTART_MODEL;
    return;
  }
  *SubAlloc.pText++ = fs.Symbol;
  Successor = (RARPPM_CONTEXT*) SubAlloc.pText;
  if (SubAlloc.pText >= SubAlloc.FakeUnitsStart)
    goto RESTART_MODEL;
  if ( fs.Successor )
  {
    if ((byte*) fs.Successor <= SubAlloc.pText &&
        (fs.Successor=CreateSuccessors(FALSE,p)) == NULL)
      goto RESTART_MODEL;
    if ( !--OrderFall )
    {
      Successor=fs.Successor;
      SubAlloc.pText -= (MaxContext != MinContext);
    }
  }
  else
  {
    FoundState->Successor=Successor;
    fs.Successor=MinContext;
  }
  s0=MinContext->U.SummFreq-(ns=MinContext->NumStats)-(fs.Freq-1);
  for (pc=MaxContext;pc != MinContext;pc=pc->Suffix)
  {
    if ((ns1=pc->NumStats) != 1)
    {
      if ((ns1 & 1) == 0)
      {
        pc->U.Stats=(RARPPM_STATE*) SubAlloc.ExpandUnits(pc->U.Stats,ns1 >> 1);
        if ( !pc->U.Stats )
          goto RESTART_MODEL;
      }
      pc->U.SummFreq += (2*ns1 < ns)+2*((4*ns1 <= ns) & (pc->U.SummFreq <= 8*ns1));
    }
    else
    {
      p=(RARPPM_STATE*) SubAlloc.AllocUnits(1);
      if ( !p )
        goto RESTART_MODEL;
      *p=pc->OneState;
      pc->U.Stats=p;
      if (p->Freq < MAX_FREQ/4-1)
        p->Freq += p->Freq;
      else
        p->Freq  = MAX_FREQ-4;
      pc->U.SummFreq=p->Freq+InitEsc+(ns > 3);
    }
    cf=2*fs.Freq*(pc->U.SummFreq+6);
    sf=s0+pc->U.SummFreq;
    if (cf < 6*sf)
    {
      cf=1+(cf > sf)+(cf >= 4*sf);
      pc->U.SummFreq += 3;
    }
    else
    {
      cf=4+(cf >= 9*sf)+(cf >= 12*sf)+(cf >= 15*sf);
      pc->U.SummFreq += (ushort)cf;
    }
    p=pc->U.Stats+ns1;
    p->Successor=Successor;
    p->Symbol = fs.Symbol;
    p->Freq = (byte)cf;
    pc->NumStats=(ushort)++ns1;
  }
  MaxContext=MinContext=fs.Successor;
  return;
RESTART_MODEL:
  RestartModelRare();
  EscCount=0;
}

static const byte ExpEscape[16]={ 25,14, 9, 7, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2 };
#define GET_MEAN(SUMM,SHIFT,ROUND) ((SUMM+(1 << (SHIFT-ROUND))) >> (SHIFT))

inline void RARPPM_CONTEXT::decodeBinSymbol(ModelPPM *Model)
{
  RARPPM_STATE& rs=OneState;
  Model->HiBitsFlag=Model->HB2Flag[Model->FoundState->Symbol];
  ushort& bs=Model->BinSumm[rs.Freq-1][Model->PrevSuccess+
           Model->NS2BSIndx[Suffix->NumStats-1]+
           Model->HiBitsFlag+2*Model->HB2Flag[rs.Symbol]+
           ((Model->RunLength >> 26) & 0x20)];
  if (Model->Coder.GetCurrentShiftCount(TOT_BITS) < bs)
  {
    Model->FoundState=&rs;
    rs.Freq += (rs.Freq < 128);
    Model->Coder.SubRange.LowCount=0;
    Model->Coder.SubRange.HighCount=bs;
    bs = GET_SHORT16(bs+INTERVAL-GET_MEAN(bs,PERIOD_BITS,2));
    Model->PrevSuccess=1;
    Model->RunLength++;
  }
  else
  {
    Model->Coder.SubRange.LowCount=bs;
    bs = GET_SHORT16(bs-GET_MEAN(bs,PERIOD_BITS,2));
    Model->Coder.SubRange.HighCount=BIN_SCALE;
    Model->InitEsc=ExpEscape[bs >> 10];
    Model->NumMasked=1;
    Model->CharMask[rs.Symbol]=Model->EscCount;
    Model->PrevSuccess=0;
    Model->FoundState=NULL;
  }
}

inline void RARPPM_CONTEXT::update1(ModelPPM *Model,RARPPM_STATE* p)
{
  (Model->FoundState=p)->Freq += 4;
  U.SummFreq += 4;
  if (p[0].Freq > p[-1].Freq)
  {
    _PPMD_SWAP(p[0],p[-1]);
    Model->FoundState=--p;
    if (p->Freq > MAX_FREQ)
      rescale(Model);
  }
}

inline bool RARPPM_CONTEXT::decodeSymbol1(ModelPPM *Model)
{
  Model->Coder.SubRange.scale=U.SummFreq;
  RARPPM_STATE* p=U.Stats;
  int i, HiCnt;
  int count=Model->Coder.GetCurrentCount();
  if (count>=(int)Model->Coder.SubRange.scale)
    return(false);
  if (count < (HiCnt=p->Freq))
  {
    Model->PrevSuccess=(2*(Model->Coder.SubRange.HighCount=HiCnt) > Model->Coder.SubRange.scale);
    Model->RunLength += Model->PrevSuccess;
    (Model->FoundState=p)->Freq=(HiCnt += 4);
    U.SummFreq += 4;
    if (HiCnt > MAX_FREQ)
      rescale(Model);
    Model->Coder.SubRange.LowCount=0;
    return(true);
  }
  else
    if (Model->FoundState==NULL)
      return(false);
  Model->PrevSuccess=0;
  i=NumStats-1;
  while ((HiCnt += (++p)->Freq) <= count)
    if (--i == 0)
    {
      Model->HiBitsFlag=Model->HB2Flag[Model->FoundState->Symbol];
      Model->Coder.SubRange.LowCount=HiCnt;
      Model->CharMask[p->Symbol]=Model->EscCount;
      i=(Model->NumMasked=NumStats)-1;
      Model->FoundState=NULL;
      do
      {
        Model->CharMask[(--p)->Symbol]=Model->EscCount;
      } while ( --i );
      Model->Coder.SubRange.HighCount=Model->Coder.SubRange.scale;
      return(true);
    }
  Model->Coder.SubRange.LowCount=(Model->Coder.SubRange.HighCount=HiCnt)-p->Freq;
  update1(Model,p);
  return(true);
}

inline void RARPPM_CONTEXT::update2(ModelPPM *Model,RARPPM_STATE* p)
{
  (Model->FoundState=p)->Freq += 4;
  U.SummFreq += 4;
  if (p->Freq > MAX_FREQ)
    rescale(Model);
  Model->EscCount++;
  Model->RunLength=Model->InitRL;
}

inline RARPPM_SEE2_CONTEXT* RARPPM_CONTEXT::makeEscFreq2(ModelPPM *Model,int Diff)
{
  RARPPM_SEE2_CONTEXT* psee2c;
  if (NumStats != 256)
  {
    psee2c=Model->SEE2Cont[Model->NS2Indx[Diff-1]]+
           (Diff < Suffix->NumStats-NumStats)+
           2*(U.SummFreq < 11*NumStats)+4*(Model->NumMasked > Diff)+
           Model->HiBitsFlag;
    Model->Coder.SubRange.scale=psee2c->getMean();
  }
  else
  {
    psee2c=&Model->DummySEE2Cont;
    Model->Coder.SubRange.scale=1;
  }
  return psee2c;
}

inline bool RARPPM_CONTEXT::decodeSymbol2(ModelPPM *Model)
{
  int count, HiCnt, i=NumStats-Model->NumMasked;
  RARPPM_SEE2_CONTEXT* psee2c=makeEscFreq2(Model,i);
  RARPPM_STATE* ps[256], ** pps=ps, * p=U.Stats-1;
  HiCnt=0;
  do
  {
    do
    {
      p++;
    } while (Model->CharMask[p->Symbol] == Model->EscCount);
    HiCnt += p->Freq;

    if (pps>=ps+ASIZE(ps))
      return false;

    *pps++ = p;
  } while ( --i );
  Model->Coder.SubRange.scale += HiCnt;
  count=Model->Coder.GetCurrentCount();
  if (count>=(int)Model->Coder.SubRange.scale)
    return(false);
  p=*(pps=ps);
  if (count < HiCnt)
  {
    HiCnt=0;
    while ((HiCnt += p->Freq) <= count)
    {
      pps++;
      if (pps>=ps+ASIZE(ps))
        return false;
      p=*pps;
    }
    Model->Coder.SubRange.LowCount = (Model->Coder.SubRange.HighCount=HiCnt)-p->Freq;
    psee2c->update();
    update2(Model,p);
  }
  else
  {
    Model->Coder.SubRange.LowCount=HiCnt;
    Model->Coder.SubRange.HighCount=Model->Coder.SubRange.scale;
    i=NumStats-Model->NumMasked;

    do
    {
      if (pps>=ps+ASIZE(ps))
        return false;
      Model->CharMask[(*pps)->Symbol]=Model->EscCount;
      pps++;
    } while ( --i );
    psee2c->Summ += (ushort)Model->Coder.SubRange.scale;
    Model->NumMasked = NumStats;
  }
  return true;
}

inline void ModelPPM::ClearMask()
{
  EscCount=1;
  memset(CharMask,0,sizeof(CharMask));
}

void ModelPPM::CleanUp()
{
  SubAlloc.StopSubAllocator();
  SubAlloc.StartSubAllocator(1);
  StartModelRare(2);
}

bool ModelPPM::DecodeInit(Unpack *UnpackRead,int &EscChar)
{
  int MaxOrder=UnpackRead->GetChar();
  bool Reset=(MaxOrder & 0x20)!=0;

  int MaxMB;
  if (Reset)
    MaxMB=UnpackRead->GetChar();
  else
    if (SubAlloc.GetAllocatedMemory()==0)
      return(false);
  if (MaxOrder & 0x40)
    EscChar=UnpackRead->GetChar();
  Coder.InitDecoder(UnpackRead);
  if (Reset)
  {
    MaxOrder=(MaxOrder & 0x1f)+1;
    if (MaxOrder>16)
      MaxOrder=16+(MaxOrder-16)*3;
    if (MaxOrder==1)
    {
      SubAlloc.StopSubAllocator();
      return(false);
    }
    SubAlloc.StartSubAllocator(MaxMB+1);
    StartModelRare(MaxOrder);
  }
  return(MinContext!=NULL);
}

int ModelPPM::DecodeChar()
{
  if ((byte*)MinContext <= SubAlloc.pText || (byte*)MinContext>SubAlloc.HeapEnd)
    return(-1);
  if (MinContext->NumStats != 1)
  {
    if ((byte*)MinContext->U.Stats <= SubAlloc.pText || (byte*)MinContext->U.Stats>SubAlloc.HeapEnd)
      return(-1);
    if (!MinContext->decodeSymbol1(this))
      return(-1);
  }
  else
    MinContext->decodeBinSymbol(this);
  Coder.Decode();
  while ( !FoundState )
  {
    ARI_DEC_NORMALIZE(Coder.code,Coder.low,Coder.range,Coder.UnpackRead);
    do
    {
      OrderFall++;
      MinContext=MinContext->Suffix;
      if ((byte*)MinContext <= SubAlloc.pText || (byte*)MinContext>SubAlloc.HeapEnd)
        return(-1);
    } while (MinContext->NumStats == NumMasked);
    if (!MinContext->decodeSymbol2(this))
      return(-1);
    Coder.Decode();
  }
  int Symbol=FoundState->Symbol;
  if (!OrderFall && (byte*) FoundState->Successor > SubAlloc.pText)
    MinContext=MaxContext=FoundState->Successor;
  else
  {
    UpdateModel();
    if (EscCount == 0)
      ClearMask();
  }
  ARI_DEC_NORMALIZE(Coder.code,Coder.low,Coder.range,Coder.UnpackRead);
  return(Symbol);
}

_forceinline void Unpack::InsertOldDist(size_t Distance)
{
  OldDist[3]=OldDist[2];
  OldDist[2]=OldDist[1];
  OldDist[1]=OldDist[0];
  OldDist[0]=Distance;
}

#if defined(LITTLE_ENDIAN) && defined(ALLOW_MISALIGNED)
#define UNPACK_COPY8
#endif

_forceinline void Unpack::CopyString(uint Length,size_t Distance)
{
  size_t SrcPtr=UnpPtr-Distance;

  if (Distance>UnpPtr)
  {

    SrcPtr+=MaxWinSize;

    if (Distance>MaxWinSize || !FirstWinDone)
    {

      while (Length-- > 0)
      {
        Window[UnpPtr]=0;
        UnpPtr=WrapUp(UnpPtr+1);
      }
      return;
    }
  }

  if (SrcPtr<MaxWinSize-MAX_INC_LZ_MATCH && UnpPtr<MaxWinSize-MAX_INC_LZ_MATCH)
  {

    byte *Src=Window+SrcPtr;
    byte *Dest=Window+UnpPtr;
    UnpPtr+=Length;

#ifdef UNPACK_COPY8
    if (Distance<Length)
#endif
      while (Length>=8)
      {
        Dest[0]=Src[0];
        Dest[1]=Src[1];
        Dest[2]=Src[2];
        Dest[3]=Src[3];
        Dest[4]=Src[4];
        Dest[5]=Src[5];
        Dest[6]=Src[6];
        Dest[7]=Src[7];

        Src+=8;
        Dest+=8;
        Length-=8;
      }
#ifdef UNPACK_COPY8
    else
      while (Length>=8)
      {

        RawPut8(RawGet8(Src),Dest);

        Src+=8;
        Dest+=8;
        Length-=8;
      }
#endif

    if (Length>0) { Dest[0]=Src[0];
    if (Length>1) { Dest[1]=Src[1];
    if (Length>2) { Dest[2]=Src[2];
    if (Length>3) { Dest[3]=Src[3];
    if (Length>4) { Dest[4]=Src[4];
    if (Length>5) { Dest[5]=Src[5];
    if (Length>6) { Dest[6]=Src[6]; } } } } } } }
  }
  else
    while (Length-- > 0)
    {
      Window[UnpPtr]=Window[WrapUp(SrcPtr++)];

      UnpPtr=WrapUp(UnpPtr+1);
    }
}

_forceinline uint Unpack::DecodeNumber(BitInput &Inp,DecodeTable *Dec)
{

  uint BitField=Inp.getbits() & 0xfffe;

  if (BitField<Dec->DecodeLen[Dec->QuickBits])
  {
    uint Code=BitField>>(16-Dec->QuickBits);
    Inp.addbits(Dec->QuickLen[Code]);
    return Dec->QuickNum[Code];
  }

  uint Bits=15;
  for (uint I=Dec->QuickBits+1;I<15;I++)
    if (BitField<Dec->DecodeLen[I])
    {
      Bits=I;
      break;
    }

  Inp.addbits(Bits);

  uint Dist=BitField-Dec->DecodeLen[Bits-1];

  Dist>>=(16-Bits);

  uint Pos=Dec->DecodePos[Bits]+Dist;

  if (Pos>=Dec->MaxNum)
    Pos=0;

  return Dec->DecodeNum[Pos];
}

_forceinline uint Unpack::SlotToLength(BitInput &Inp,uint Slot)
{
  uint LBits,Length=2;
  if (Slot<8)
  {
    LBits=0;
    Length+=Slot;
  }
  else
  {
    LBits=Slot/4-1;
    Length+=(4 | (Slot & 3)) << LBits;
  }

  if (LBits>0)
  {
    Length+=Inp.getbits()>>(16-LBits);
    Inp.addbits(LBits);
  }
  return Length;
}

#ifdef RAR_SMP

#define UNP_READ_SIZE_MT        0x400000
#define UNP_BLOCKS_PER_THREAD          2

struct UnpackThreadDataList
{
  UnpackThreadData *D;
  uint BlockCount;
};

THREAD_PROC(UnpackDecodeThread)
{
  UnpackThreadDataList *DL=(UnpackThreadDataList *)Data;
  for (uint I=0;I<DL->BlockCount;I++)
    DL->D->UnpackPtr->UnpackDecode(DL->D[I]);
}

void Unpack::InitMT()
{
  if (ReadBufMT==NULL)
  {

    const size_t Overflow=1024;

    ReadBufMT=new byte[UNP_READ_SIZE_MT+Overflow];
    memset(ReadBufMT,0,UNP_READ_SIZE_MT+Overflow);
  }
  if (UnpThreadData==NULL)
  {
    uint MaxItems=MaxUserThreads*UNP_BLOCKS_PER_THREAD;
    UnpThreadData=new UnpackThreadData[MaxItems];
    memset(UnpThreadData,0,sizeof(UnpackThreadData)*MaxItems);

    for (uint I=0;I<MaxItems;I++)
    {
      UnpackThreadData *CurData=UnpThreadData+I;
      if (CurData->Decoded==NULL)
      {

        CurData->DecodedAllocated=0x4100;

        CurData->Decoded=(UnpackDecodedItem *)malloc(CurData->DecodedAllocated*sizeof(UnpackDecodedItem));
        if (CurData->Decoded==NULL)
          ErrHandler.MemoryError();
      }
    }
  }
}

void Unpack::Unpack5MT(bool Solid)
{
  InitMT();
  UnpInitData(Solid);

  for (uint I=0;I<MaxUserThreads*UNP_BLOCKS_PER_THREAD;I++)
  {
    UnpackThreadData *CurData=UnpThreadData+I;
    CurData->LargeBlock=false;
    CurData->Incomplete=false;
  }

  UnpThreadData[0].BlockHeader=BlockHeader;
  UnpThreadData[0].BlockTables=BlockTables;
  uint LastBlockNum=0;

  int DataSize=0;
  int BlockStart=0;

  bool LargeBlock=false;

  bool Done=false;
  while (!Done)
  {

    const int TooSmallToProcess=1024;

    int ReadSize=UnpIO->UnpRead(ReadBufMT+DataSize,(UNP_READ_SIZE_MT-DataSize)&~0xf);
    if (ReadSize<0)
      break;
    DataSize+=ReadSize;
    if (DataSize==0)
      break;

    if (ReadSize>0 && DataSize<TooSmallToProcess)
      continue;

    while (BlockStart<DataSize && !Done)
    {
      uint BlockNumber=0,BlockNumberMT=0;
      while (BlockNumber<MaxUserThreads*UNP_BLOCKS_PER_THREAD)
      {
        UnpackThreadData *CurData=UnpThreadData+BlockNumber;
        LastBlockNum=BlockNumber;
        CurData->UnpackPtr=this;

        if (CurData->Incomplete)
          CurData->DataSize=DataSize;
        else
        {
          CurData->Inp.SetExternalBuffer(ReadBufMT+BlockStart);
          CurData->Inp.InitBitInput();
          CurData->DataSize=DataSize-BlockStart;
          if (CurData->DataSize==0)
            break;
          CurData->DamagedData=false;
          CurData->HeaderRead=false;
          CurData->TableRead=false;
        }

        CurData->NoDataLeft=(ReadSize==0);

        CurData->Incomplete=false;
        CurData->ThreadNumber=BlockNumber;

        if (!CurData->HeaderRead)
        {
          CurData->HeaderRead=true;
          if (!ReadBlockHeader(CurData->Inp,CurData->BlockHeader) ||
              !CurData->BlockHeader.TablePresent && !TablesRead5)
          {
            Done=true;
            break;
          }
          TablesRead5=true;
        }

        const int LargeBlockSize=0x20000;
        if (LargeBlock || CurData->BlockHeader.BlockSize>LargeBlockSize)
          LargeBlock=CurData->LargeBlock=true;
        else
          BlockNumberMT++;

        BlockStart+=CurData->BlockHeader.HeaderSize+CurData->BlockHeader.BlockSize;

        BlockNumber++;

        int DataLeft=DataSize-BlockStart;
        if (DataLeft>=0 && CurData->BlockHeader.LastBlockInFile)
          break;

        if (DataLeft<TooSmallToProcess)
          break;
      }

      UnpackThreadDataList UTDArray[MaxPoolThreads];
      uint UTDArrayPos=0;

      uint MaxBlockPerThread=BlockNumberMT/MaxUserThreads;
      if (BlockNumberMT%MaxUserThreads!=0)
        MaxBlockPerThread++;

      for (uint CurBlock=0;CurBlock<BlockNumberMT;CurBlock+=MaxBlockPerThread)
      {
        UnpackThreadDataList *UTD=UTDArray+UTDArrayPos++;
        UTD->D=UnpThreadData+CurBlock;
        UTD->BlockCount=Min(MaxBlockPerThread,BlockNumberMT-CurBlock);

#ifdef USE_THREADS
        if (BlockNumber==1)
          UnpackDecode(*UTD->D);
        else
          UnpThreadPool->AddTask(UnpackDecodeThread,(void*)UTD);
#else
        for (uint I=0;I<UTD->BlockCount;I++)
          UnpackDecode(UTD->D[I]);
#endif
      }

      if (BlockNumber==0)
        break;

#ifdef USE_THREADS
      UnpThreadPool->WaitDone();
#endif

      bool IncompleteThread=false;

      for (uint Block=0;Block<BlockNumber;Block++)
      {
        UnpackThreadData *CurData=UnpThreadData+Block;
        if (!CurData->LargeBlock && !ProcessDecoded(*CurData) ||
            CurData->LargeBlock && !UnpackLargeBlock(*CurData) ||
            CurData->DamagedData)
        {
          Done=true;
          break;
        }
        if (CurData->Incomplete)
        {
          int BufPos=int(CurData->Inp.InBuf+CurData->Inp.InAddr-ReadBufMT);
          if (DataSize<=BufPos)
          {
            Done=true;
            break;
          }
          IncompleteThread=true;
          memmove(ReadBufMT,ReadBufMT+BufPos,DataSize-BufPos);
          CurData->BlockHeader.BlockSize-=CurData->Inp.InAddr-CurData->BlockHeader.BlockStart;
          CurData->BlockHeader.HeaderSize=0;
          CurData->BlockHeader.BlockStart=0;
          CurData->Inp.InBuf=ReadBufMT;
          CurData->Inp.InAddr=0;

          if (Block!=0)
          {

            UnpackDecodedItem *Decoded=UnpThreadData[0].Decoded;
            uint DecodedAllocated=UnpThreadData[0].DecodedAllocated;
            UnpThreadData[0]=*CurData;
            UnpThreadData[0].Decoded=Decoded;
            UnpThreadData[0].DecodedAllocated=DecodedAllocated;
            CurData->Incomplete=false;
          }

          BlockStart=0;
          DataSize-=BufPos;
          break;
        }
        else
          if (CurData->BlockHeader.LastBlockInFile)
          {
            Done=true;
            break;
          }
      }

      if (IncompleteThread || Done)
        break;
      else
      {
        int DataLeft=DataSize-BlockStart;
        if (DataLeft<TooSmallToProcess)
        {
          if (DataLeft<0)
          {
            Done=true;
            break;
          }

          if (DataLeft>0)
            memmove(ReadBufMT,ReadBufMT+BlockStart,DataLeft);
          DataSize=DataLeft;
          BlockStart=0;
          break;
        }
      }
    }
  }
  UnpPtr=WrapUp(UnpPtr);
  UnpWriteBuf();

  BlockHeader=UnpThreadData[LastBlockNum].BlockHeader;
  BlockTables=UnpThreadData[LastBlockNum].BlockTables;
}

void Unpack::UnpackDecode(UnpackThreadData &D)
{
  if (!D.TableRead)
  {
    D.TableRead=true;
    if (!ReadTables(D.Inp,D.BlockHeader,D.BlockTables))
    {
      D.DamagedData=true;
      return;
    }
  }

  if (D.Inp.InAddr>D.BlockHeader.HeaderSize+D.BlockHeader.BlockSize)
  {
    D.DamagedData=true;
    return;
  }

  D.DecodedSize=0;
  int BlockBorder=D.BlockHeader.BlockStart+D.BlockHeader.BlockSize-1;

  int DataBorder=D.DataSize-16;
  int ReadBorder=Min(BlockBorder,DataBorder);

  while (true)
  {
    if (D.Inp.InAddr>=ReadBorder)
    {
      if (D.Inp.InAddr>BlockBorder || D.Inp.InAddr==BlockBorder &&
          D.Inp.InBit>=D.BlockHeader.BlockBitSize)
        break;

      if ((D.Inp.InAddr>=DataBorder) && !D.NoDataLeft || D.Inp.InAddr>=D.DataSize)
      {
        D.Incomplete=true;
        break;
      }
    }
    if (D.DecodedSize>D.DecodedAllocated-8)
    {
      D.DecodedAllocated=D.DecodedAllocated*2;
      void *Decoded=realloc(D.Decoded,D.DecodedAllocated*sizeof(UnpackDecodedItem));
      if (Decoded==NULL)
        ErrHandler.MemoryError();
      D.Decoded=(UnpackDecodedItem *)Decoded;
    }

    UnpackDecodedItem *CurItem=D.Decoded+D.DecodedSize++;

    uint MainSlot=DecodeNumber(D.Inp,&D.BlockTables.LD);
    if (MainSlot<256)
    {
      if (D.DecodedSize>1)
      {
        UnpackDecodedItem *PrevItem=CurItem-1;
        if (PrevItem->Type==UNPDT_LITERAL && PrevItem->Length<ASIZE(PrevItem->Literal)-1)
        {
          PrevItem->Length++;
          PrevItem->Literal[PrevItem->Length]=(byte)MainSlot;
          D.DecodedSize--;
          continue;
        }
      }
      CurItem->Type=UNPDT_LITERAL;
      CurItem->Literal[0]=(byte)MainSlot;
      CurItem->Length=0;
      continue;
    }
    if (MainSlot>=262)
    {
      uint Length=SlotToLength(D.Inp,MainSlot-262);

      size_t Distance=1;
      uint DBits,DistSlot=DecodeNumber(D.Inp,&D.BlockTables.DD);
      if (DistSlot<4)
      {
        DBits=0;
        Distance+=DistSlot;
      }
      else
      {
        DBits=DistSlot/2 - 1;
        Distance+=size_t(2 | (DistSlot & 1)) << DBits;
      }

      if (DBits>0)
      {
        if (DBits>=4)
        {
          if (DBits>4)
          {

            if (DBits>36)
              Distance+=( ( size_t(D.Inp.getbits64() ) >> (68-DBits) ) << 4 );
            else
              Distance+=( ( size_t(D.Inp.getbits32() ) >> (36-DBits) ) << 4 );
            D.Inp.addbits(DBits-4);
          }
          uint LowDist=DecodeNumber(D.Inp,&D.BlockTables.LDD);
          Distance+=LowDist;

          if (sizeof(Distance)==4 && DBits>=30)
            Distance=(size_t)-1;
        }
        else
        {
          Distance+=D.Inp.getbits()>>(16-DBits);
          D.Inp.addbits(DBits);
        }
      }

      if (Distance>0x100)
      {
        Length++;
        if (Distance>0x2000)
        {
          Length++;
          if (Distance>0x40000)
            Length++;
        }
      }

      CurItem->Type=UNPDT_MATCH;
      CurItem->Length=(ushort)Length;
      CurItem->Distance=Distance;
      continue;
    }
    if (MainSlot==256)
    {
      UnpackFilter Filter;
      ReadFilter(D.Inp,Filter);

      CurItem->Type=UNPDT_FILTER;
      CurItem->Length=Filter.Type;
      CurItem->Distance=Filter.BlockStart;

      CurItem=D.Decoded+D.DecodedSize++;

      CurItem->Type=UNPDT_FILTER;
      CurItem->Length=Filter.Channels;
      CurItem->Distance=Filter.BlockLength;

      continue;
    }
    if (MainSlot==257)
    {
      CurItem->Type=UNPDT_FULLREP;
      continue;
    }
    if (MainSlot<262)
    {
      CurItem->Type=UNPDT_REP;
      CurItem->Distance=MainSlot-258;
      uint LengthSlot=DecodeNumber(D.Inp,&D.BlockTables.RD);
      uint Length=SlotToLength(D.Inp,LengthSlot);
      CurItem->Length=(ushort)Length;
      continue;
    }
  }
}

bool Unpack::ProcessDecoded(UnpackThreadData &D)
{
  UnpackDecodedItem *Item=D.Decoded,*Border=D.Decoded+D.DecodedSize;
  while (Item<Border)
  {
    UnpPtr=WrapUp(UnpPtr);

    FirstWinDone|=(PrevPtr>UnpPtr);
    PrevPtr=UnpPtr;

    if (WrapDown(WriteBorder-UnpPtr)<=MAX_INC_LZ_MATCH && WriteBorder!=UnpPtr)
    {
      UnpWriteBuf();
      if (WrittenFileSize>DestUnpSize)
        return false;
    }

    if (Item->Type==UNPDT_LITERAL)
    {
#if defined(LITTLE_ENDIAN) && defined(ALLOW_MISALIGNED)
      if (Item->Length==7 && UnpPtr<MaxWinSize-8)
      {
        *(uint64 *)(Window+UnpPtr)=*(uint64 *)(Item->Literal);
         UnpPtr+=8;
      }
      else
#endif
        for (uint I=0;I<=Item->Length;I++)
          Window[WrapUp(UnpPtr++)]=Item->Literal[I];
    }
    else
      if (Item->Type==UNPDT_MATCH)
      {
        InsertOldDist(Item->Distance);
        LastLength=Item->Length;
        CopyString(Item->Length,Item->Distance);
      }
      else
        if (Item->Type==UNPDT_REP)
        {
          size_t Distance=OldDist[Item->Distance];
          for (size_t I=Item->Distance;I>0;I--)
            OldDist[I]=OldDist[I-1];
          OldDist[0]=Distance;
          LastLength=Item->Length;
          CopyString(Item->Length,Distance);
        }
        else
          if (Item->Type==UNPDT_FULLREP)
          {
            if (LastLength!=0)
              CopyString(LastLength,OldDist[0]);
          }
          else
            if (Item->Type==UNPDT_FILTER)
            {
              UnpackFilter Filter;

              Filter.Type=(byte)Item->Length;
              Filter.BlockStart=Item->Distance;

              Item++;

              Filter.Channels=(byte)Item->Length;
              Filter.BlockLength=(uint)Item->Distance;

              AddFilter(Filter);
            }
    Item++;
  }
  return true;
}

bool Unpack::UnpackLargeBlock(UnpackThreadData &D)
{
  if (!D.TableRead)
  {
    D.TableRead=true;
    if (!ReadTables(D.Inp,D.BlockHeader,D.BlockTables))
    {
      D.DamagedData=true;
      return false;
    }
  }

  if (D.Inp.InAddr>D.BlockHeader.HeaderSize+D.BlockHeader.BlockSize)
  {
    D.DamagedData=true;
    return false;
  }

  int BlockBorder=D.BlockHeader.BlockStart+D.BlockHeader.BlockSize-1;

  int DataBorder=D.DataSize-16;
  int ReadBorder=Min(BlockBorder,DataBorder);

  while (true)
  {
    UnpPtr=WrapUp(UnpPtr);

    FirstWinDone|=(PrevPtr>UnpPtr);
    PrevPtr=UnpPtr;

    if (D.Inp.InAddr>=ReadBorder)
    {
      if (D.Inp.InAddr>BlockBorder || D.Inp.InAddr==BlockBorder &&
          D.Inp.InBit>=D.BlockHeader.BlockBitSize)
        break;

      if ((D.Inp.InAddr>=DataBorder) && !D.NoDataLeft || D.Inp.InAddr>=D.DataSize)
      {
        D.Incomplete=true;
        break;
      }
    }
    if (WrapDown(WriteBorder-UnpPtr)<=MAX_INC_LZ_MATCH && WriteBorder!=UnpPtr)
    {
      UnpWriteBuf();
      if (WrittenFileSize>DestUnpSize)
        return false;
    }

    uint MainSlot=DecodeNumber(D.Inp,&D.BlockTables.LD);
    if (MainSlot<256)
    {
      Window[UnpPtr++]=(byte)MainSlot;
      continue;
    }
    if (MainSlot>=262)
    {
      uint Length=SlotToLength(D.Inp,MainSlot-262);

      size_t Distance=1;
      uint DBits,DistSlot=DecodeNumber(D.Inp,&D.BlockTables.DD);
      if (DistSlot<4)
      {
        DBits=0;
        Distance+=DistSlot;
      }
      else
      {
        DBits=DistSlot/2 - 1;
        Distance+=size_t(2 | (DistSlot & 1)) << DBits;
      }

      if (DBits>0)
      {
        if (DBits>=4)
        {
          if (DBits>4)
          {

            if (DBits>36)
              Distance+=( ( size_t(D.Inp.getbits64() ) >> (68-DBits) ) << 4 );
            else
              Distance+=( ( size_t(D.Inp.getbits32() ) >> (36-DBits) ) << 4 );
            D.Inp.addbits(DBits-4);
          }
          uint LowDist=DecodeNumber(D.Inp,&D.BlockTables.LDD);
          Distance+=LowDist;

          if (sizeof(Distance)==4 && DBits>=30)
            Distance=(size_t)-1;
        }
        else
        {
          Distance+=D.Inp.getbits32()>>(32-DBits);
          D.Inp.addbits(DBits);
        }
      }

      if (Distance>0x100)
      {
        Length++;
        if (Distance>0x2000)
        {
          Length++;
          if (Distance>0x40000)
            Length++;
        }
      }

      InsertOldDist(Distance);
      LastLength=Length;
      CopyString(Length,Distance);
      continue;
    }
    if (MainSlot==256)
    {
      UnpackFilter Filter;
      if (!ReadFilter(D.Inp,Filter) || !AddFilter(Filter))
        break;
      continue;
    }
    if (MainSlot==257)
    {
      if (LastLength!=0)
        CopyString(LastLength,OldDist[0]);
      continue;
    }
    if (MainSlot<262)
    {
      uint DistNum=MainSlot-258;
      size_t Distance=OldDist[DistNum];
      for (uint I=DistNum;I>0;I--)
        OldDist[I]=OldDist[I-1];
      OldDist[0]=Distance;

      uint LengthSlot=DecodeNumber(D.Inp,&D.BlockTables.RD);
      uint Length=SlotToLength(D.Inp,LengthSlot);
      LastLength=Length;
      CopyString(Length,Distance);
      continue;
    }
  }
  return true;
}

#endif
#ifndef SFX_MODULE
#define STARTL1  2
static uint DecL1[]={0x8000,0xa000,0xc000,0xd000,0xe000,0xea00,
                             0xee00,0xf000,0xf200,0xf200,0xffff};
static uint PosL1[]={0,0,0,2,3,5,7,11,16,20,24,32,32};

#define STARTL2  3
static uint DecL2[]={0xa000,0xc000,0xd000,0xe000,0xea00,0xee00,
                             0xf000,0xf200,0xf240,0xffff};
static uint PosL2[]={0,0,0,0,5,7,9,13,18,22,26,34,36};

#define STARTHF0  4
static uint DecHf0[]={0x8000,0xc000,0xe000,0xf200,0xf200,0xf200,
                              0xf200,0xf200,0xffff};
static uint PosHf0[]={0,0,0,0,0,8,16,24,33,33,33,33,33};

#define STARTHF1  5
static uint DecHf1[]={0x2000,0xc000,0xe000,0xf000,0xf200,0xf200,
                              0xf7e0,0xffff};
static uint PosHf1[]={0,0,0,0,0,0,4,44,60,76,80,80,127};

#define STARTHF2  5
static uint DecHf2[]={0x1000,0x2400,0x8000,0xc000,0xfa00,0xffff,
                              0xffff,0xffff};
static uint PosHf2[]={0,0,0,0,0,0,2,7,53,117,233,0,0};

#define STARTHF3  6
static uint DecHf3[]={0x800,0x2400,0xee00,0xfe80,0xffff,0xffff,
                              0xffff};
static uint PosHf3[]={0,0,0,0,0,0,0,2,16,218,251,0,0};

#define STARTHF4  8
static uint DecHf4[]={0xff00,0xffff,0xffff,0xffff,0xffff,0xffff};
static uint PosHf4[]={0,0,0,0,0,0,0,0,0,255,0,0,0};

void Unpack::Unpack15(bool Solid)
{
  UnpInitData(Solid);
  UnpInitData15(Solid);
  UnpReadBuf();
  if (!Solid)
  {
    InitHuff();
    UnpPtr=0;
  }
  else
    UnpPtr=WrPtr;
  --DestUnpSize;
  if (DestUnpSize>=0)
  {
    GetFlagsBuf();
    FlagsCnt=8;
  }

  while (DestUnpSize>=0)
  {
    UnpPtr&=MaxWinMask;

    FirstWinDone|=(PrevPtr>UnpPtr);
    PrevPtr=UnpPtr;

    if (Inp.InAddr>ReadTop-30 && !UnpReadBuf())
      break;
    if (((WrPtr-UnpPtr) & MaxWinMask)<270 && WrPtr!=UnpPtr)
      UnpWriteBuf20();
    if (StMode)
    {
      HuffDecode();
      continue;
    }

    if (--FlagsCnt < 0)
    {
      GetFlagsBuf();
      FlagsCnt=7;
    }

    if (FlagBuf & 0x80)
    {
      FlagBuf<<=1;
      if (Nlzb > Nhfb)
        LongLZ();
      else
        HuffDecode();
    }
    else
    {
      FlagBuf<<=1;
      if (--FlagsCnt < 0)
      {
        GetFlagsBuf();
        FlagsCnt=7;
      }
      if (FlagBuf & 0x80)
      {
        FlagBuf<<=1;
        if (Nlzb > Nhfb)
          HuffDecode();
        else
          LongLZ();
      }
      else
      {
        FlagBuf<<=1;
        ShortLZ();
      }
    }
  }
  UnpWriteBuf20();
}

#define GetShortLen1(pos) ((pos)==1 ? Buf60+3:ShortLen1[pos])
#define GetShortLen2(pos) ((pos)==3 ? Buf60+3:ShortLen2[pos])

void Unpack::ShortLZ()
{
  static uint ShortLen1[]={1,3,4,4,5,6,7,8,8,4,4,5,6,6,4,0};
  static uint ShortXor1[]={0,0xa0,0xd0,0xe0,0xf0,0xf8,0xfc,0xfe,
                                   0xff,0xc0,0x80,0x90,0x98,0x9c,0xb0};
  static uint ShortLen2[]={2,3,3,3,4,4,5,6,6,4,4,5,6,6,4,0};
  static uint ShortXor2[]={0,0x40,0x60,0xa0,0xd0,0xe0,0xf0,0xf8,
                                   0xfc,0xc0,0x80,0x90,0x98,0x9c,0xb0};

  uint Length,SaveLength;
  uint LastDistance;
  uint Distance;
  int DistancePlace;
  NumHuf=0;

  uint BitField=Inp.fgetbits();
  if (LCount==2)
  {
    Inp.faddbits(1);
    if (BitField >= 0x8000)
    {
      CopyString15(LastDist,LastLength);
      return;
    }
    BitField <<= 1;
    LCount=0;
  }

  BitField>>=8;

  if (AvrLn1<37)
  {
    for (Length=0;;Length++)
      if (((BitField^ShortXor1[Length]) & (~(0xff>>GetShortLen1(Length))))==0)
        break;
    Inp.faddbits(GetShortLen1(Length));
  }
  else
  {
    for (Length=0;;Length++)
      if (((BitField^ShortXor2[Length]) & (~(0xff>>GetShortLen2(Length))))==0)
        break;
    Inp.faddbits(GetShortLen2(Length));
  }

  if (Length >= 9)
  {
    if (Length == 9)
    {
      LCount++;
      CopyString15(LastDist,LastLength);
      return;
    }
    if (Length == 14)
    {
      LCount=0;
      Length=DecodeNum(Inp.fgetbits(),STARTL2,DecL2,PosL2)+5;
      Distance=(Inp.fgetbits()>>1) | 0x8000;
      Inp.faddbits(15);
      LastLength=Length;
      LastDist=Distance;
      CopyString15(Distance,Length);
      return;
    }

    LCount=0;
    SaveLength=Length;
    Distance=(uint)OldDist[(OldDistPtr-(Length-9)) & 3];
    Length=DecodeNum(Inp.fgetbits(),STARTL1,DecL1,PosL1)+2;
    if (Length==0x101 && SaveLength==10)
    {
      Buf60 ^= 1;
      return;
    }
    if (Distance > 256)
      Length++;
    if (Distance >= MaxDist3)
      Length++;

    OldDist[OldDistPtr++]=Distance;
    OldDistPtr = OldDistPtr & 3;
    LastLength=Length;
    LastDist=Distance;
    CopyString15(Distance,Length);
    return;
  }

  LCount=0;
  AvrLn1 += Length;
  AvrLn1 -= AvrLn1 >> 4;

  DistancePlace=DecodeNum(Inp.fgetbits(),STARTHF2,DecHf2,PosHf2) & 0xff;
  Distance=ChSetA[DistancePlace];
  if (--DistancePlace != -1)
  {
    LastDistance=ChSetA[DistancePlace];
    ChSetA[DistancePlace+1]=(ushort)LastDistance;
    ChSetA[DistancePlace]=(ushort)Distance;
  }
  Length+=2;
  OldDist[OldDistPtr++] = ++Distance;
  OldDistPtr = OldDistPtr & 3;
  LastLength=Length;
  LastDist=Distance;
  CopyString15(Distance,Length);
}

void Unpack::LongLZ()
{
  uint Length;
  uint Distance;
  uint DistancePlace,NewDistancePlace;
  uint OldAvr2,OldAvr3;

  NumHuf=0;
  Nlzb+=16;
  if (Nlzb > 0xff)
  {
    Nlzb=0x90;
    Nhfb >>= 1;
  }
  OldAvr2=AvrLn2;

  uint BitField=Inp.fgetbits();
  if (AvrLn2 >= 122)
    Length=DecodeNum(BitField,STARTL2,DecL2,PosL2);
  else
    if (AvrLn2 >= 64)
      Length=DecodeNum(BitField,STARTL1,DecL1,PosL1);
    else
      if (BitField < 0x100)
      {
        Length=BitField;
        Inp.faddbits(16);
      }
      else
      {
        for (Length=0;((BitField<<Length)&0x8000)==0;Length++)
          ;
        Inp.faddbits(Length+1);
      }

  AvrLn2 += Length;
  AvrLn2 -= AvrLn2 >> 5;

  BitField=Inp.fgetbits();
  if (AvrPlcB > 0x28ff)
    DistancePlace=DecodeNum(BitField,STARTHF2,DecHf2,PosHf2);
  else
    if (AvrPlcB > 0x6ff)
      DistancePlace=DecodeNum(BitField,STARTHF1,DecHf1,PosHf1);
    else
      DistancePlace=DecodeNum(BitField,STARTHF0,DecHf0,PosHf0);

  AvrPlcB += DistancePlace;
  AvrPlcB -= AvrPlcB >> 8;
  while (1)
  {
    Distance = ChSetB[DistancePlace & 0xff];
    NewDistancePlace = NToPlB[Distance++ & 0xff]++;
    if (!(Distance & 0xff))
      CorrHuff(ChSetB,NToPlB);
    else
      break;
  }

  ChSetB[DistancePlace & 0xff]=ChSetB[NewDistancePlace];
  ChSetB[NewDistancePlace]=(ushort)Distance;

  Distance=((Distance & 0xff00) | (Inp.fgetbits() >> 8)) >> 1;
  Inp.faddbits(7);

  OldAvr3=AvrLn3;
  if (Length!=1 && Length!=4)
    if (Length==0 && Distance <= MaxDist3)
    {
      AvrLn3++;
      AvrLn3 -= AvrLn3 >> 8;
    }
    else
      if (AvrLn3 > 0)
        AvrLn3--;
  Length+=3;
  if (Distance >= MaxDist3)
    Length++;
  if (Distance <= 256)
    Length+=8;
  if (OldAvr3 > 0xb0 || AvrPlc >= 0x2a00 && OldAvr2 < 0x40)
    MaxDist3=0x7f00;
  else
    MaxDist3=0x2001;
  OldDist[OldDistPtr++]=Distance;
  OldDistPtr = OldDistPtr & 3;
  LastLength=Length;
  LastDist=Distance;
  CopyString15(Distance,Length);
}

void Unpack::HuffDecode()
{
  uint CurByte,NewBytePlace;
  uint Length;
  uint Distance;
  int BytePlace;

  uint BitField=Inp.fgetbits();

  if (AvrPlc > 0x75ff)
    BytePlace=DecodeNum(BitField,STARTHF4,DecHf4,PosHf4);
  else
    if (AvrPlc > 0x5dff)
      BytePlace=DecodeNum(BitField,STARTHF3,DecHf3,PosHf3);
    else
      if (AvrPlc > 0x35ff)
        BytePlace=DecodeNum(BitField,STARTHF2,DecHf2,PosHf2);
      else
        if (AvrPlc > 0x0dff)
          BytePlace=DecodeNum(BitField,STARTHF1,DecHf1,PosHf1);
        else
          BytePlace=DecodeNum(BitField,STARTHF0,DecHf0,PosHf0);
  BytePlace&=0xff;
  if (StMode)
  {
    if (BytePlace==0 && BitField > 0xfff)
      BytePlace=0x100;
    if (--BytePlace==-1)
    {
      BitField=Inp.fgetbits();
      Inp.faddbits(1);
      if (BitField & 0x8000)
      {
        NumHuf=StMode=0;
        return;
      }
      else
      {
        Length = (BitField & 0x4000) ? 4 : 3;
        Inp.faddbits(1);
        Distance=DecodeNum(Inp.fgetbits(),STARTHF2,DecHf2,PosHf2);
        Distance = (Distance << 5) | (Inp.fgetbits() >> 11);
        Inp.faddbits(5);
        CopyString15(Distance,Length);
        return;
      }
    }
  }
  else
    if (NumHuf++ >= 16 && FlagsCnt==0)
      StMode=1;
  AvrPlc += BytePlace;
  AvrPlc -= AvrPlc >> 8;
  Nhfb+=16;
  if (Nhfb > 0xff)
  {
    Nhfb=0x90;
    Nlzb >>= 1;
  }

  Window[UnpPtr++]=(byte)(ChSet[BytePlace]>>8);
  --DestUnpSize;

  while (1)
  {
    CurByte=ChSet[BytePlace];
    NewBytePlace=NToPl[CurByte++ & 0xff]++;
    if ((CurByte & 0xff) > 0xa1)
      CorrHuff(ChSet,NToPl);
    else
      break;
  }

  ChSet[BytePlace]=ChSet[NewBytePlace];
  ChSet[NewBytePlace]=(ushort)CurByte;
}

void Unpack::GetFlagsBuf()
{
  uint Flags,NewFlagsPlace;
  uint FlagsPlace=DecodeNum(Inp.fgetbits(),STARTHF2,DecHf2,PosHf2);

  if (FlagsPlace>=sizeof(ChSetC)/sizeof(ChSetC[0]))
    return;

  while (1)
  {
    Flags=ChSetC[FlagsPlace];
    FlagBuf=Flags>>8;
    NewFlagsPlace=NToPlC[Flags++ & 0xff]++;
    if ((Flags & 0xff) != 0)
      break;
    CorrHuff(ChSetC,NToPlC);
  }

  ChSetC[FlagsPlace]=ChSetC[NewFlagsPlace];
  ChSetC[NewFlagsPlace]=(ushort)Flags;
}

void Unpack::UnpInitData15(bool Solid)
{
  if (!Solid)
  {
    AvrPlcB=AvrLn1=AvrLn2=AvrLn3=NumHuf=Buf60=0;
    AvrPlc=0x3500;
    MaxDist3=0x2001;
    Nhfb=Nlzb=0x80;
  }
  FlagsCnt=0;
  FlagBuf=0;
  StMode=0;
  LCount=0;
  ReadTop=0;
}

void Unpack::InitHuff()
{
  for (ushort I=0;I<256;I++)
  {
    ChSet[I]=ChSetB[I]=I<<8;
    ChSetA[I]=I;
    ChSetC[I]=((~I+1) & 0xff)<<8;
  }
  memset(NToPl,0,sizeof(NToPl));
  memset(NToPlB,0,sizeof(NToPlB));
  memset(NToPlC,0,sizeof(NToPlC));
  CorrHuff(ChSetB,NToPlB);
}

void Unpack::CorrHuff(ushort *CharSet,byte *NumToPlace)
{
  int I,J;
  for (I=7;I>=0;I--)
    for (J=0;J<32;J++,CharSet++)
      *CharSet=(*CharSet & ~0xff) | I;
  memset(NumToPlace,0,sizeof(NToPl));
  for (I=6;I>=0;I--)
    NumToPlace[I]=(7-I)*32;
}

void Unpack::CopyString15(uint Distance,uint Length)
{
  DestUnpSize-=Length;

  if (!FirstWinDone && Distance>UnpPtr || Distance>MaxWinSize || Distance==0)
    while (Length-- > 0)
    {
      Window[UnpPtr]=0;
      UnpPtr=(UnpPtr+1) & MaxWinMask;
    }
  else
    while (Length-- > 0)
    {
      Window[UnpPtr]=Window[(UnpPtr-Distance) & MaxWinMask];
      UnpPtr=(UnpPtr+1) & MaxWinMask;
    }
}

uint Unpack::DecodeNum(uint Num,uint StartPos,uint *DecTab,uint *PosTab)
{
  int I;
  for (Num&=0xfff0,I=0;DecTab[I]<=Num;I++)
    StartPos++;
  Inp.faddbits(StartPos);
  return(((Num-(I ? DecTab[I-1]:0))>>(16-StartPos))+PosTab[StartPos]);
}

void Unpack::CopyString20(uint Length,uint Distance)
{
  LastDist=Distance;
  OldDist[OldDistPtr++]=Distance;
  OldDistPtr = OldDistPtr & 3;
  LastLength=Length;
  DestUnpSize-=Length;
  CopyString(Length,Distance);
}

void Unpack::Unpack20(bool Solid)
{
  static unsigned char LDecode[]={0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,80,96,112,128,160,192,224};
  static unsigned char LBits[]=  {0,0,0,0,0,0,0,0,1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5};
  static uint DDecode[]={0,1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288,16384,24576,32768U,49152U,65536,98304,131072,196608,262144,327680,393216,458752,524288,589824,655360,720896,786432,851968,917504,983040};
  static unsigned char DBits[]=  {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13,    14,    14,   15,   15,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16};
  static unsigned char SDDecode[]={0,4,8,16,32,64,128,192};
  static unsigned char SDBits[]=  {2,2,3, 4, 5, 6,  6,  6};
  uint Bits;

  if (Suspended)
    UnpPtr=WrPtr;
  else
  {
    UnpInitData(Solid);
    if (!UnpReadBuf())
      return;
    if ((!Solid || !TablesRead2) && !ReadTables20())
      return;
    --DestUnpSize;
  }

  while (DestUnpSize>=0)
  {
    UnpPtr&=MaxWinMask;

    FirstWinDone|=(PrevPtr>UnpPtr);
    PrevPtr=UnpPtr;

    if (Inp.InAddr>ReadTop-30)
      if (!UnpReadBuf())
        break;
    if (((WrPtr-UnpPtr) & MaxWinMask)<270 && WrPtr!=UnpPtr)
    {
      UnpWriteBuf20();
      if (Suspended)
        return;
    }
    if (UnpAudioBlock)
    {
      uint AudioNumber=DecodeNumber(Inp,&MD[UnpCurChannel]);

      if (AudioNumber==256)
      {
        if (!ReadTables20())
          break;
        continue;
      }
      Window[UnpPtr++]=DecodeAudio((int)AudioNumber);
      if (++UnpCurChannel==UnpChannels)
        UnpCurChannel=0;
      --DestUnpSize;
      continue;
    }

    uint Number=DecodeNumber(Inp,&BlockTables.LD);
    if (Number<256)
    {
      Window[UnpPtr++]=(byte)Number;
      --DestUnpSize;
      continue;
    }
    if (Number>269)
    {
      uint Length=LDecode[Number-=270]+3;
      if ((Bits=LBits[Number])>0)
      {
        Length+=Inp.getbits()>>(16-Bits);
        Inp.addbits(Bits);
      }

      uint DistNumber=DecodeNumber(Inp,&BlockTables.DD);
      uint Distance=DDecode[DistNumber]+1;
      if ((Bits=DBits[DistNumber])>0)
      {
        Distance+=Inp.getbits()>>(16-Bits);
        Inp.addbits(Bits);
      }

      if (Distance>=0x2000)
      {
        Length++;
        if (Distance>=0x40000L)
          Length++;
      }

      CopyString20(Length,Distance);
      continue;
    }
    if (Number==269)
    {
      if (!ReadTables20())
        break;
      continue;
    }
    if (Number==256)
    {
      CopyString20(LastLength,LastDist);
      continue;
    }
    if (Number<261)
    {
      uint Distance=(uint)OldDist[(OldDistPtr-(Number-256)) & 3];
      uint LengthNumber=DecodeNumber(Inp,&BlockTables.RD);
      uint Length=LDecode[LengthNumber]+2;
      if ((Bits=LBits[LengthNumber])>0)
      {
        Length+=Inp.getbits()>>(16-Bits);
        Inp.addbits(Bits);
      }
      if (Distance>=0x101)
      {
        Length++;
        if (Distance>=0x2000)
        {
          Length++;
          if (Distance>=0x40000)
            Length++;
        }
      }
      CopyString20(Length,Distance);
      continue;
    }
    if (Number<270)
    {
      uint Distance=SDDecode[Number-=261]+1;
      if ((Bits=SDBits[Number])>0)
      {
        Distance+=Inp.getbits()>>(16-Bits);
        Inp.addbits(Bits);
      }
      CopyString20(2,Distance);
      continue;
   }
  }
  ReadLastTables();
  UnpWriteBuf20();
}

void Unpack::UnpWriteBuf20()
{
  if (UnpPtr!=WrPtr)
    UnpSomeRead=true;
  if (UnpPtr<WrPtr)
  {
    UnpIO->UnpWrite(&Window[WrPtr],-(int)WrPtr & MaxWinMask);
    UnpIO->UnpWrite(Window,UnpPtr);

  }
  else
    UnpIO->UnpWrite(&Window[WrPtr],UnpPtr-WrPtr);
  WrPtr=UnpPtr;
}

bool Unpack::ReadTables20()
{
  byte BitLength[BC20];
  byte Table[MC20*4];
  if (Inp.InAddr>ReadTop-25)
    if (!UnpReadBuf())
      return false;
  uint BitField=Inp.getbits();
  UnpAudioBlock=(BitField & 0x8000)!=0;

  if (!(BitField & 0x4000))
    memset(UnpOldTable20,0,sizeof(UnpOldTable20));
  Inp.addbits(2);

  uint TableSize;
  if (UnpAudioBlock)
  {
    UnpChannels=((BitField>>12) & 3)+1;
    if (UnpCurChannel>=UnpChannels)
      UnpCurChannel=0;
    Inp.addbits(2);
    TableSize=MC20*UnpChannels;
  }
  else
    TableSize=NC20+DC20+RC20;

  for (uint I=0;I<BC20;I++)
  {
    BitLength[I]=(byte)(Inp.getbits() >> 12);
    Inp.addbits(4);
  }
  MakeDecodeTables(BitLength,&BlockTables.BD,BC20);
  for (uint I=0;I<TableSize;)
  {
    if (Inp.InAddr>ReadTop-5)
      if (!UnpReadBuf())
        return false;
    uint Number=DecodeNumber(Inp,&BlockTables.BD);
    if (Number<16)
    {
      Table[I]=(Number+UnpOldTable20[I]) & 0xf;
      I++;
    }
    else
      if (Number==16)
      {
        uint N=(Inp.getbits() >> 14)+3;
        Inp.addbits(2);
        if (I==0)
          return false;
        else
          while (N-- > 0 && I<TableSize)
          {
            Table[I]=Table[I-1];
            I++;
          }
      }
      else
      {
        uint N;
        if (Number==17)
        {
          N=(Inp.getbits() >> 13)+3;
          Inp.addbits(3);
        }
        else
        {
          N=(Inp.getbits() >> 9)+11;
          Inp.addbits(7);
        }
        while (N-- > 0 && I<TableSize)
          Table[I++]=0;
      }
  }
  TablesRead2=true;
  if (Inp.InAddr>ReadTop)
    return true;
  if (UnpAudioBlock)
    for (uint I=0;I<UnpChannels;I++)
      MakeDecodeTables(&Table[I*MC20],&MD[I],MC20);
  else
  {
    MakeDecodeTables(&Table[0],&BlockTables.LD,NC20);
    MakeDecodeTables(&Table[NC20],&BlockTables.DD,DC20);
    MakeDecodeTables(&Table[NC20+DC20],&BlockTables.RD,RC20);
  }
  memcpy(UnpOldTable20,Table,TableSize);
  return true;
}

void Unpack::ReadLastTables()
{
  if (ReadTop>=Inp.InAddr+5)
    if (UnpAudioBlock)
    {
      if (DecodeNumber(Inp,&MD[UnpCurChannel])==256)
        ReadTables20();
    }
    else
      if (DecodeNumber(Inp,&BlockTables.LD)==269)
        ReadTables20();
}

void Unpack::UnpInitData20(int Solid)
{
  if (!Solid)
  {
    TablesRead2=false;
    UnpAudioBlock=false;
    UnpChannelDelta=0;
    UnpCurChannel=0;
    UnpChannels=1;

    memset(AudV,0,sizeof(AudV));
    memset(UnpOldTable20,0,sizeof(UnpOldTable20));
    memset(MD,0,sizeof(MD));
  }
}

byte Unpack::DecodeAudio(int Delta)
{
  struct AudioVariables *V=&AudV[UnpCurChannel];
  V->ByteCount++;
  V->D4=V->D3;
  V->D3=V->D2;
  V->D2=V->LastDelta-V->D1;
  V->D1=V->LastDelta;
  int PCh=8*V->LastChar+V->K1*V->D1+V->K2*V->D2+V->K3*V->D3+V->K4*V->D4+V->K5*UnpChannelDelta;
  PCh=(PCh>>3) & 0xFF;

  uint Ch=PCh-Delta;

  int D=(signed char)Delta;

  D=(uint)D<<3;

  V->Dif[0]+=abs(D);
  V->Dif[1]+=abs(D-V->D1);
  V->Dif[2]+=abs(D+V->D1);
  V->Dif[3]+=abs(D-V->D2);
  V->Dif[4]+=abs(D+V->D2);
  V->Dif[5]+=abs(D-V->D3);
  V->Dif[6]+=abs(D+V->D3);
  V->Dif[7]+=abs(D-V->D4);
  V->Dif[8]+=abs(D+V->D4);
  V->Dif[9]+=abs(D-UnpChannelDelta);
  V->Dif[10]+=abs(D+UnpChannelDelta);

  UnpChannelDelta=V->LastDelta=(signed char)(Ch-V->LastChar);
  V->LastChar=Ch;

  if ((V->ByteCount & 0x1F)==0)
  {
    uint MinDif=V->Dif[0],NumMinDif=0;
    V->Dif[0]=0;
    for (uint I=1;I<ASIZE(V->Dif);I++)
    {
      if (V->Dif[I]<MinDif)
      {
        MinDif=V->Dif[I];
        NumMinDif=I;
      }
      V->Dif[I]=0;
    }
    switch(NumMinDif)
    {
      case 1:
        if (V->K1>=-16)
          V->K1--;
        break;
      case 2:
        if (V->K1<16)
          V->K1++;
        break;
      case 3:
        if (V->K2>=-16)
          V->K2--;
        break;
      case 4:
        if (V->K2<16)
          V->K2++;
        break;
      case 5:
        if (V->K3>=-16)
          V->K3--;
        break;
      case 6:
        if (V->K3<16)
          V->K3++;
        break;
      case 7:
        if (V->K4>=-16)
          V->K4--;
        break;
      case 8:
        if (V->K4<16)
          V->K4++;
        break;
      case 9:
        if (V->K5>=-16)
          V->K5--;
        break;
      case 10:
        if (V->K5<16)
          V->K5++;
        break;
    }
  }
  return (byte)Ch;
}

inline int Unpack::SafePPMDecodeChar()
{
  int Ch=PPM.DecodeChar();
  if (Ch==-1)
  {
    PPM.CleanUp();
    UnpBlockType=BLOCK_LZ;
  }
  return(Ch);
}

void Unpack::Unpack29(bool Solid)
{
  static unsigned char LDecode[]={0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,80,96,112,128,160,192,224};
  static unsigned char LBits[]=  {0,0,0,0,0,0,0,0,1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5};
  static int DDecode[DC30];
  static byte DBits[DC30];
  static int DBitLengthCounts[]= {4,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,14,0,12};
  static unsigned char SDDecode[]={0,4,8,16,32,64,128,192};
  static unsigned char SDBits[]=  {2,2,3, 4, 5, 6,  6,  6};
  unsigned int Bits;

  if (DDecode[1]==0)
  {
    int Dist=0,BitLength=0,Slot=0;
    for (int I=0;I<ASIZE(DBitLengthCounts);I++,BitLength++)
      for (int J=0;J<DBitLengthCounts[I];J++,Slot++,Dist+=(1<<BitLength))
      {
        DDecode[Slot]=Dist;
        DBits[Slot]=BitLength;
      }
  }

  FileExtracted=true;

  if (!Suspended)
  {
    UnpInitData(Solid);
    if (!UnpReadBuf30())
      return;
    if ((!Solid || !TablesRead3) && !ReadTables30())
      return;
  }

  while (true)
  {
    UnpPtr&=MaxWinMask;

    FirstWinDone|=(PrevPtr>UnpPtr);
    PrevPtr=UnpPtr;

    if (Inp.InAddr>ReadBorder)
    {
      if (!UnpReadBuf30())
        break;
    }
    if (((WrPtr-UnpPtr) & MaxWinMask)<=MAX3_INC_LZ_MATCH && WrPtr!=UnpPtr)
    {
      UnpWriteBuf30();
      if (WrittenFileSize>DestUnpSize)
        return;
      if (Suspended)
      {
        FileExtracted=false;
        return;
      }
    }
    if (UnpBlockType==BLOCK_PPM)
    {

      int Ch=PPM.DecodeChar();
      if (Ch==-1)
      {
        PPM.CleanUp();
        UnpBlockType=BLOCK_LZ;
        break;
      }
      if (Ch==PPMEscChar)
      {
        int NextCh=SafePPMDecodeChar();
        if (NextCh==0)
        {
          if (!ReadTables30())
            break;
          continue;
        }
        if (NextCh==-1)
          break;
        if (NextCh==2)
          break;
        if (NextCh==3)
        {
          if (!ReadVMCodePPM())
            break;
          continue;
        }
        if (NextCh==4)
        {
          unsigned int Distance=0,Length;
          bool Failed=false;
          for (int I=0;I<4 && !Failed;I++)
          {
            int Ch=SafePPMDecodeChar();
            if (Ch==-1)
              Failed=true;
            else
              if (I==3)
                Length=(byte)Ch;
              else
                Distance=(Distance<<8)+(byte)Ch;
          }
          if (Failed)
            break;

          CopyString(Length+32,Distance+2);
          continue;
        }
        if (NextCh==5)
        {
          int Length=SafePPMDecodeChar();
          if (Length==-1)
            break;
          CopyString(Length+4,1);
          continue;
        }

      }
      Window[UnpPtr++]=Ch;
      continue;
    }

    uint Number=DecodeNumber(Inp,&BlockTables.LD);
    if (Number<256)
    {
      Window[UnpPtr++]=(byte)Number;
      continue;
    }
    if (Number>=271)
    {
      uint Length=LDecode[Number-=271]+3;
      if ((Bits=LBits[Number])>0)
      {
        Length+=Inp.getbits()>>(16-Bits);
        Inp.addbits(Bits);
      }

      uint DistNumber=DecodeNumber(Inp,&BlockTables.DD);
      uint Distance=DDecode[DistNumber]+1;
      if ((Bits=DBits[DistNumber])>0)
      {
        if (DistNumber>9)
        {
          if (Bits>4)
          {
            Distance+=((Inp.getbits()>>(20-Bits))<<4);
            Inp.addbits(Bits-4);
          }
          if (LowDistRepCount>0)
          {
            LowDistRepCount--;
            Distance+=PrevLowDist;
          }
          else
          {
            uint LowDist=DecodeNumber(Inp,&BlockTables.LDD);
            if (LowDist==16)
            {
              LowDistRepCount=LOW_DIST_REP_COUNT-1;
              Distance+=PrevLowDist;
            }
            else
            {
              Distance+=LowDist;
              PrevLowDist=LowDist;
            }
          }
        }
        else
        {
          Distance+=Inp.getbits()>>(16-Bits);
          Inp.addbits(Bits);
        }
      }

      if (Distance>=0x2000)
      {
        Length++;
        if (Distance>=0x40000)
          Length++;
      }

      InsertOldDist(Distance);
      LastLength=Length;
      CopyString(Length,Distance);
      continue;
    }
    if (Number==256)
    {
      if (!ReadEndOfBlock())
        break;
      continue;
    }
    if (Number==257)
    {
      if (!ReadVMCode())
        break;
      continue;
    }
    if (Number==258)
    {
      if (LastLength!=0)
        CopyString(LastLength,OldDist[0]);
      continue;
    }
    if (Number<263)
    {
      uint DistNum=Number-259;
      uint Distance=(uint)OldDist[DistNum];
      for (uint I=DistNum;I>0;I--)
        OldDist[I]=OldDist[I-1];
      OldDist[0]=Distance;

      uint LengthNumber=DecodeNumber(Inp,&BlockTables.RD);
      int Length=LDecode[LengthNumber]+2;
      if ((Bits=LBits[LengthNumber])>0)
      {
        Length+=Inp.getbits()>>(16-Bits);
        Inp.addbits(Bits);
      }
      LastLength=Length;
      CopyString(Length,Distance);
      continue;
    }
    if (Number<272)
    {
      uint Distance=SDDecode[Number-=263]+1;
      if ((Bits=SDBits[Number])>0)
      {
        Distance+=Inp.getbits()>>(16-Bits);
        Inp.addbits(Bits);
      }
      InsertOldDist(Distance);
      LastLength=2;
      CopyString(2,Distance);
      continue;
    }
  }
  UnpWriteBuf30();
}

bool Unpack::ReadEndOfBlock()
{
  uint BitField=Inp.getbits();
  bool NewTable,NewFile=false;

  if ((BitField & 0x8000)!=0)
  {
    NewTable=true;
    Inp.addbits(1);
  }
  else
  {
    NewFile=true;
    NewTable=(BitField & 0x4000)!=0;
    Inp.addbits(2);
  }
  TablesRead3=!NewTable;

  if (NewFile)
    return false;
  return ReadTables30();
}

bool Unpack::ReadVMCode()
{

  uint FirstByte=Inp.getbits()>>8;
  Inp.addbits(8);
  uint Length=(FirstByte & 7)+1;
  if (Length==7)
  {
    Length=(Inp.getbits()>>8)+7;
    Inp.addbits(8);
  }
  else
    if (Length==8)
    {
      Length=Inp.getbits();
      Inp.addbits(16);
    }
  if (Length==0)
    return false;
  std::vector<byte> VMCode(Length);
  for (uint I=0;I<Length;I++)
  {

    if (Inp.InAddr>=ReadTop-1 && !UnpReadBuf30() && I<Length-1)
      return false;
    VMCode[I]=Inp.getbits()>>8;
    Inp.addbits(8);
  }
  return AddVMCode(FirstByte,VMCode.data(),Length);
}

bool Unpack::ReadVMCodePPM()
{
  uint FirstByte=SafePPMDecodeChar();
  if ((int)FirstByte==-1)
    return false;
  uint Length=(FirstByte & 7)+1;
  if (Length==7)
  {
    int B1=SafePPMDecodeChar();
    if (B1==-1)
      return false;
    Length=B1+7;
  }
  else
    if (Length==8)
    {
      int B1=SafePPMDecodeChar();
      if (B1==-1)
        return false;
      int B2=SafePPMDecodeChar();
      if (B2==-1)
        return false;
      Length=B1*256+B2;
    }
  if (Length==0)
    return false;
  std::vector<byte> VMCode(Length);
  for (uint I=0;I<Length;I++)
  {
    int Ch=SafePPMDecodeChar();
    if (Ch==-1)
      return false;
    VMCode[I]=Ch;
  }
  return AddVMCode(FirstByte,VMCode.data(),Length);
}

bool Unpack::AddVMCode(uint FirstByte,byte *Code,uint CodeSize)
{
  VMCodeInp.InitBitInput();
  memcpy(VMCodeInp.InBuf,Code,Min(BitInput::MAX_SIZE,CodeSize));
  VM.Init();

  uint FiltPos;
  if ((FirstByte & 0x80)!=0)
  {
    FiltPos=RarVM::ReadData(VMCodeInp);
    if (FiltPos==0)
      InitFilters30(false);
    else
      FiltPos--;
  }
  else
    FiltPos=LastFilter;

  if (FiltPos>Filters30.size() || FiltPos>OldFilterLengths.size())
    return false;
  LastFilter=FiltPos;
  bool NewFilter=(FiltPos==Filters30.size());

  UnpackFilter30 *StackFilter=new UnpackFilter30;

  UnpackFilter30 *Filter;
  if (NewFilter)
  {
    if (FiltPos>MAX3_UNPACK_FILTERS)
    {

      delete StackFilter;
      return false;
    }

    StackFilter->ParentFilter=(uint)Filters30.size();
    Filter=new UnpackFilter30;
    Filters30.push_back(Filter);

    OldFilterLengths.push_back(0);
  }
  else
  {
    Filter=Filters30[FiltPos];
    StackFilter->ParentFilter=FiltPos;
  }

  uint EmptyCount=0;
  for (uint I=0;I<PrgStack.size();I++)
  {
    PrgStack[I-EmptyCount]=PrgStack[I];
    if (PrgStack[I]==NULL)
      EmptyCount++;
    if (EmptyCount>0)
      PrgStack[I]=NULL;
  }
  if (EmptyCount==0)
  {
    if (PrgStack.size()>MAX3_UNPACK_FILTERS)
    {
      delete StackFilter;
      return false;
    }
    PrgStack.resize(PrgStack.size()+1);
    EmptyCount=1;
  }
  size_t StackPos=PrgStack.size()-EmptyCount;
  PrgStack[StackPos]=StackFilter;

  uint BlockStart=RarVM::ReadData(VMCodeInp);
  if ((FirstByte & 0x40)!=0)
    BlockStart+=258;
  StackFilter->BlockStart=(uint)((BlockStart+UnpPtr)&MaxWinMask);
  if ((FirstByte & 0x20)!=0)
  {
    StackFilter->BlockLength=RarVM::ReadData(VMCodeInp);

    OldFilterLengths[FiltPos]=StackFilter->BlockLength;
  }
  else
  {

    StackFilter->BlockLength=FiltPos<OldFilterLengths.size() ? OldFilterLengths[FiltPos]:0;
  }

  StackFilter->NextWindow=WrPtr!=UnpPtr && ((WrPtr-UnpPtr)&MaxWinMask)<=BlockStart;

  memset(StackFilter->Prg.InitR,0,sizeof(StackFilter->Prg.InitR));
  StackFilter->Prg.InitR[4]=StackFilter->BlockLength;

  if ((FirstByte & 0x10)!=0)
  {
    uint InitMask=VMCodeInp.fgetbits()>>9;
    VMCodeInp.faddbits(7);
    for (uint I=0;I<7;I++)
      if (InitMask & (1<<I))
        StackFilter->Prg.InitR[I]=RarVM::ReadData(VMCodeInp);
  }

  if (NewFilter)
  {
    uint VMCodeSize=RarVM::ReadData(VMCodeInp);
    if (VMCodeSize>=0x10000 || VMCodeSize==0 || VMCodeInp.InAddr+VMCodeSize>CodeSize)
      return false;
    std::vector<byte> VMCode(VMCodeSize);
    for (uint I=0;I<VMCodeSize;I++)
    {
      if (VMCodeInp.Overflow(3))
        return false;
      VMCode[I]=VMCodeInp.fgetbits()>>8;
      VMCodeInp.faddbits(8);
    }
    VM.Prepare(VMCode.data(),VMCodeSize,&Filter->Prg);
  }
  StackFilter->Prg.Type=Filter->Prg.Type;

  return true;
}

bool Unpack::UnpReadBuf30()
{
  int DataSize=ReadTop-Inp.InAddr;
  if (DataSize<0)
    return false;
  if (Inp.InAddr>BitInput::MAX_SIZE/2)
  {

    if (DataSize>0)
      memmove(Inp.InBuf,Inp.InBuf+Inp.InAddr,DataSize);
    Inp.InAddr=0;
    ReadTop=DataSize;
  }
  else
    DataSize=ReadTop;
  int ReadCode=UnpIO->UnpRead(Inp.InBuf+DataSize,BitInput::MAX_SIZE-DataSize);
  if (ReadCode>0)
    ReadTop+=ReadCode;
  ReadBorder=ReadTop-30;
  return ReadCode!=-1;
}

void Unpack::UnpWriteBuf30()
{
  uint WrittenBorder=(uint)WrPtr;
  uint WriteSize=(uint)((UnpPtr-WrittenBorder)&MaxWinMask);
  for (size_t I=0;I<PrgStack.size();I++)
  {

    UnpackFilter30 *flt=PrgStack[I];
    if (flt==NULL)
      continue;
    if (flt->NextWindow)
    {
      flt->NextWindow=false;
      continue;
    }
    unsigned int BlockStart=flt->BlockStart;
    unsigned int BlockLength=flt->BlockLength;
    if (((BlockStart-WrittenBorder)&MaxWinMask)<WriteSize)
    {
      if (WrittenBorder!=BlockStart)
      {
        UnpWriteArea(WrittenBorder,BlockStart);
        WrittenBorder=BlockStart;
        WriteSize=(uint)((UnpPtr-WrittenBorder)&MaxWinMask);
      }
      if (BlockLength<=WriteSize)
      {
        uint BlockEnd=(BlockStart+BlockLength)&MaxWinMask;
        if (BlockStart<BlockEnd || BlockEnd==0)
          VM.SetMemory(0,Window+BlockStart,BlockLength);
        else
        {
          uint FirstPartLength=uint(MaxWinSize-BlockStart);
          VM.SetMemory(0,Window+BlockStart,FirstPartLength);
          VM.SetMemory(FirstPartLength,Window,BlockEnd);
        }

        VM_PreparedProgram *ParentPrg=&Filters30[flt->ParentFilter]->Prg;
        VM_PreparedProgram *Prg=&flt->Prg;

        ExecuteCode(Prg);

        byte *FilteredData=Prg->FilteredData;
        unsigned int FilteredDataSize=Prg->FilteredDataSize;

        delete PrgStack[I];
        PrgStack[I]=nullptr;
        while (I+1<PrgStack.size())
        {
          UnpackFilter30 *NextFilter=PrgStack[I+1];

          if (NextFilter==NULL || NextFilter->BlockStart!=BlockStart ||
              NextFilter->BlockLength!=FilteredDataSize || NextFilter->NextWindow)
            break;

          VM.SetMemory(0,FilteredData,FilteredDataSize);

          VM_PreparedProgram *ParentPrg=&Filters30[NextFilter->ParentFilter]->Prg;
          VM_PreparedProgram *NextPrg=&NextFilter->Prg;

          ExecuteCode(NextPrg);

          FilteredData=NextPrg->FilteredData;
          FilteredDataSize=NextPrg->FilteredDataSize;
          I++;
          delete PrgStack[I];
          PrgStack[I]=nullptr;
        }
        UnpIO->UnpWrite(FilteredData,FilteredDataSize);
        UnpSomeRead=true;
        WrittenFileSize+=FilteredDataSize;
        WrittenBorder=BlockEnd;
        WriteSize=uint((UnpPtr-WrittenBorder)&MaxWinMask);
      }
      else
      {

        for (size_t J=I;J<PrgStack.size();J++)
        {
          UnpackFilter30 *flt=PrgStack[J];
          if (flt!=nullptr && flt->NextWindow)
            flt->NextWindow=false;
        }
        WrPtr=WrittenBorder;
        return;
      }
    }
  }

  UnpWriteArea(WrittenBorder,UnpPtr);
  WrPtr=UnpPtr;
}

void Unpack::ExecuteCode(VM_PreparedProgram *Prg)
{
  Prg->InitR[6]=(uint)WrittenFileSize;
  VM.Execute(Prg);
}

bool Unpack::ReadTables30()
{
  byte BitLength[BC];
  byte Table[HUFF_TABLE_SIZE30];
  if (Inp.InAddr>ReadTop-25)
    if (!UnpReadBuf30())
      return(false);
  Inp.faddbits((8-Inp.InBit)&7);
  uint BitField=Inp.fgetbits();
  if (BitField & 0x8000)
  {
    UnpBlockType=BLOCK_PPM;
    return(PPM.DecodeInit(this,PPMEscChar));
  }
  UnpBlockType=BLOCK_LZ;

  PrevLowDist=0;
  LowDistRepCount=0;

  if (!(BitField & 0x4000))
    memset(UnpOldTable,0,sizeof(UnpOldTable));
  Inp.faddbits(2);

  for (uint I=0;I<BC;I++)
  {
    uint Length=(byte)(Inp.fgetbits() >> 12);
    Inp.faddbits(4);
    if (Length==15)
    {
      uint ZeroCount=(byte)(Inp.fgetbits() >> 12);
      Inp.faddbits(4);
      if (ZeroCount==0)
        BitLength[I]=15;
      else
      {
        ZeroCount+=2;
        while (ZeroCount-- > 0 && I<ASIZE(BitLength))
          BitLength[I++]=0;
        I--;
      }
    }
    else
      BitLength[I]=Length;
  }
  MakeDecodeTables(BitLength,&BlockTables.BD,BC30);

  const uint TableSize=HUFF_TABLE_SIZE30;
  for (uint I=0;I<TableSize;)
  {
    if (Inp.InAddr>ReadTop-5)
      if (!UnpReadBuf30())
        return(false);
    uint Number=DecodeNumber(Inp,&BlockTables.BD);
    if (Number<16)
    {
      Table[I]=(Number+UnpOldTable[I]) & 0xf;
      I++;
    }
    else
      if (Number<18)
      {
        uint N;
        if (Number==16)
        {
          N=(Inp.fgetbits() >> 13)+3;
          Inp.faddbits(3);
        }
        else
        {
          N=(Inp.fgetbits() >> 9)+11;
          Inp.faddbits(7);
        }
        if (I==0)
          return false;
        else
          while (N-- > 0 && I<TableSize)
          {
            Table[I]=Table[I-1];
            I++;
          }
      }
      else
      {
        uint N;
        if (Number==18)
        {
          N=(Inp.fgetbits() >> 13)+3;
          Inp.faddbits(3);
        }
        else
        {
          N=(Inp.fgetbits() >> 9)+11;
          Inp.faddbits(7);
        }
        while (N-- > 0 && I<TableSize)
          Table[I++]=0;
      }
  }
  TablesRead3=true;
  if (Inp.InAddr>ReadTop)
    return false;
  MakeDecodeTables(&Table[0],&BlockTables.LD,NC30);
  MakeDecodeTables(&Table[NC30],&BlockTables.DD,DC30);
  MakeDecodeTables(&Table[NC30+DC30],&BlockTables.LDD,LDC30);
  MakeDecodeTables(&Table[NC30+DC30+LDC30],&BlockTables.RD,RC30);
  memcpy(UnpOldTable,Table,sizeof(UnpOldTable));
  return true;
}

void Unpack::UnpInitData30(bool Solid)
{
  if (!Solid)
  {
    TablesRead3=false;
    memset(UnpOldTable,0,sizeof(UnpOldTable));
    PPMEscChar=2;
    UnpBlockType=BLOCK_LZ;
  }
  InitFilters30(Solid);
}

void Unpack::InitFilters30(bool Solid)
{
  if (!Solid)
  {
    OldFilterLengths.clear();
    LastFilter=0;

    for (size_t I=0;I<Filters30.size();I++)
      delete Filters30[I];
    Filters30.clear();
  }
  for (size_t I=0;I<PrgStack.size();I++)
    delete PrgStack[I];
  PrgStack.clear();
}

#endif
void Unpack::Unpack5(bool Solid)
{
  FileExtracted=true;

  if (!Suspended)
  {
    UnpInitData(Solid);
    if (!UnpReadBuf())
      return;

    if (!ReadBlockHeader(Inp,BlockHeader) ||
        !ReadTables(Inp,BlockHeader,BlockTables) || !TablesRead5)
      return;
  }

  while (true)
  {
    UnpPtr=WrapUp(UnpPtr);

    FirstWinDone|=(PrevPtr>UnpPtr);
    PrevPtr=UnpPtr;

    if (Inp.InAddr>=ReadBorder)
    {
      bool FileDone=false;

      while (Inp.InAddr>BlockHeader.BlockStart+BlockHeader.BlockSize-1 ||
             Inp.InAddr==BlockHeader.BlockStart+BlockHeader.BlockSize-1 &&
             Inp.InBit>=BlockHeader.BlockBitSize)
      {
        if (BlockHeader.LastBlockInFile)
        {
          FileDone=true;
          break;
        }
        if (!ReadBlockHeader(Inp,BlockHeader) || !ReadTables(Inp,BlockHeader,BlockTables))
          return;
      }
      if (FileDone || !UnpReadBuf())
        break;
    }

    if (WrapDown(WriteBorder-UnpPtr)<=MAX_INC_LZ_MATCH && WriteBorder!=UnpPtr)
    {
      UnpWriteBuf();
      if (WrittenFileSize>DestUnpSize)
        return;
      if (Suspended)
      {
        FileExtracted=false;
        return;
      }
    }

    uint MainSlot=DecodeNumber(Inp,&BlockTables.LD);
    if (MainSlot<256)
    {
      if (Fragmented)
        FragWindow[UnpPtr++]=(byte)MainSlot;
      else
        Window[UnpPtr++]=(byte)MainSlot;
      continue;
    }
    if (MainSlot>=262)
    {
      uint Length=SlotToLength(Inp,MainSlot-262);

      size_t Distance=1;
      uint DBits,DistSlot=DecodeNumber(Inp,&BlockTables.DD);
      if (DistSlot<4)
      {
        DBits=0;
        Distance+=DistSlot;
      }
      else
      {
        DBits=DistSlot/2 - 1;
        Distance+=size_t(2 | (DistSlot & 1)) << DBits;
      }

      if (DBits>0)
      {
        if (DBits>=4)
        {
          if (DBits>4)
          {

            if (DBits>36)
              Distance+=( ( size_t(Inp.getbits64() ) >> (68-DBits) ) << 4 );
            else
              Distance+=( ( size_t(Inp.getbits32() ) >> (36-DBits) ) << 4 );
            Inp.addbits(DBits-4);
          }
          uint LowDist=DecodeNumber(Inp,&BlockTables.LDD);
          Distance+=LowDist;

          if (sizeof(Distance)==4 && DBits>=30)
            Distance=(size_t)-1;
        }
        else
        {
          Distance+=Inp.getbits()>>(16-DBits);
          Inp.addbits(DBits);
        }
      }

      if (Distance>0x100)
      {
        Length++;
        if (Distance>0x2000)
        {
          Length++;
          if (Distance>0x40000)
            Length++;
        }
      }

      InsertOldDist(Distance);
      LastLength=Length;
      if (Fragmented)
        FragWindow.CopyString(Length,Distance,UnpPtr,FirstWinDone,MaxWinSize);
      else
        CopyString(Length,Distance);
      continue;
    }
    if (MainSlot==256)
    {
      UnpackFilter Filter;
      if (!ReadFilter(Inp,Filter) || !AddFilter(Filter))
        break;
      continue;
    }
    if (MainSlot==257)
    {
      if (LastLength!=0)
        if (Fragmented)
          FragWindow.CopyString(LastLength,OldDist[0],UnpPtr,FirstWinDone,MaxWinSize);
        else
          CopyString(LastLength,OldDist[0]);
      continue;
    }
    if (MainSlot<262)
    {
      uint DistNum=MainSlot-258;
      size_t Distance=OldDist[DistNum];
      for (uint I=DistNum;I>0;I--)
        OldDist[I]=OldDist[I-1];
      OldDist[0]=Distance;

      uint LengthSlot=DecodeNumber(Inp,&BlockTables.RD);
      uint Length=SlotToLength(Inp,LengthSlot);
      LastLength=Length;
      if (Fragmented)
        FragWindow.CopyString(Length,Distance,UnpPtr,FirstWinDone,MaxWinSize);
      else
        CopyString(Length,Distance);
      continue;
    }
  }
  UnpWriteBuf();
}

uint Unpack::ReadFilterData(BitInput &Inp)
{
  uint ByteCount=(Inp.fgetbits()>>14)+1;
  Inp.addbits(2);

  uint Data=0;
  for (uint I=0;I<ByteCount;I++)
  {
    Data+=(Inp.fgetbits()>>8)<<(I*8);
    Inp.addbits(8);
  }
  return Data;
}

bool Unpack::ReadFilter(BitInput &Inp,UnpackFilter &Filter)
{
  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-16)
    if (!UnpReadBuf())
      return false;

  Filter.BlockStart=ReadFilterData(Inp);
  Filter.BlockLength=ReadFilterData(Inp);
  if (Filter.BlockLength>MAX_FILTER_BLOCK_SIZE)
    Filter.BlockLength=0;

  Filter.Type=Inp.fgetbits()>>13;
  Inp.faddbits(3);

  if (Filter.Type==FILTER_DELTA)
  {
    Filter.Channels=(Inp.fgetbits()>>11)+1;
    Inp.faddbits(5);
  }

  return true;
}

bool Unpack::AddFilter(UnpackFilter &Filter)
{
  if (Filters.size()>=MAX_UNPACK_FILTERS)
  {
    UnpWriteBuf();
    if (Filters.size()>=MAX_UNPACK_FILTERS)
      InitFilters();
  }

  Filter.NextWindow=WrPtr!=UnpPtr && WrapDown(WrPtr-UnpPtr)<=Filter.BlockStart;

  Filter.BlockStart=(Filter.BlockStart+UnpPtr)%MaxWinSize;
  Filters.push_back(Filter);
  return true;
}

bool Unpack::UnpReadBuf()
{
  int DataSize=ReadTop-Inp.InAddr;
  if (DataSize<0)
    return false;
  BlockHeader.BlockSize-=Inp.InAddr-BlockHeader.BlockStart;
  if (Inp.InAddr>BitInput::MAX_SIZE/2)
  {

    if (DataSize>0)
      memmove(Inp.InBuf,Inp.InBuf+Inp.InAddr,DataSize);
    Inp.InAddr=0;
    ReadTop=DataSize;
  }
  else
    DataSize=ReadTop;
  int ReadCode=0;
  if (BitInput::MAX_SIZE!=DataSize)
    ReadCode=UnpIO->UnpRead(Inp.InBuf+DataSize,BitInput::MAX_SIZE-DataSize);
  if (ReadCode>0)
    ReadTop+=ReadCode;
  ReadBorder=ReadTop-30;
  BlockHeader.BlockStart=Inp.InAddr;
  if (BlockHeader.BlockSize!=-1)
  {

    ReadBorder=Min(ReadBorder,BlockHeader.BlockStart+BlockHeader.BlockSize-1);
  }
  return ReadCode!=-1;
}

void Unpack::UnpWriteBuf()
{
  size_t WrittenBorder=WrPtr;
  size_t FullWriteSize=WrapDown(UnpPtr-WrittenBorder);
  size_t WriteSizeLeft=FullWriteSize;
  bool NotAllFiltersProcessed=false;
  for (size_t I=0;I<Filters.size();I++)
  {

    UnpackFilter *flt=&Filters[I];
    if (flt->Type==FILTER_NONE)
      continue;
    if (flt->NextWindow)
    {

      if (WrapDown(flt->BlockStart-WrPtr)<=FullWriteSize)
        flt->NextWindow=false;
      continue;
    }
    size_t BlockStart=flt->BlockStart;
    uint BlockLength=flt->BlockLength;
    if (WrapDown(BlockStart-WrittenBorder)<WriteSizeLeft)
    {
      if (WrittenBorder!=BlockStart)
      {
        UnpWriteArea(WrittenBorder,BlockStart);
        WrittenBorder=BlockStart;
        WriteSizeLeft=WrapDown(UnpPtr-WrittenBorder);
      }
      if (BlockLength<=WriteSizeLeft)
      {
        if (BlockLength>0)
        {
          size_t BlockEnd=WrapUp(BlockStart+BlockLength);

          FilterSrcMemory.resize(BlockLength);
          byte *Mem=FilterSrcMemory.data();
          if (BlockStart<BlockEnd || BlockEnd==0)
          {
            if (Fragmented)
              FragWindow.CopyData(Mem,BlockStart,BlockLength);
            else
              memcpy(Mem,Window+BlockStart,BlockLength);
          }
          else
          {
            size_t FirstPartLength=size_t(MaxWinSize-BlockStart);
            if (Fragmented)
            {
              FragWindow.CopyData(Mem,BlockStart,FirstPartLength);
              FragWindow.CopyData(Mem+FirstPartLength,0,BlockEnd);
            }
            else
            {
              memcpy(Mem,Window+BlockStart,FirstPartLength);
              memcpy(Mem+FirstPartLength,Window,BlockEnd);
            }
          }

          byte *OutMem=ApplyFilter(Mem,BlockLength,flt);

          Filters[I].Type=FILTER_NONE;

          if (OutMem!=NULL)
            UnpIO->UnpWrite(OutMem,BlockLength);

          UnpSomeRead=true;
          WrittenFileSize+=BlockLength;
          WrittenBorder=BlockEnd;
          WriteSizeLeft=WrapDown(UnpPtr-WrittenBorder);
        }
      }
      else
      {

        WrPtr=WrittenBorder;

        for (size_t J=I;J<Filters.size();J++)
        {
          UnpackFilter *flt=&Filters[J];
          if (flt->Type!=FILTER_NONE)
            flt->NextWindow=false;
        }

        NotAllFiltersProcessed=true;
        break;
      }
    }
  }

  size_t EmptyCount=0;
  for (size_t I=0;I<Filters.size();I++)
  {
    if (EmptyCount>0)
      Filters[I-EmptyCount]=Filters[I];
    if (Filters[I].Type==FILTER_NONE)
      EmptyCount++;
  }
  if (EmptyCount>0)
    Filters.resize(Filters.size()-EmptyCount);

  if (!NotAllFiltersProcessed)
  {

    UnpWriteArea(WrittenBorder,UnpPtr);
    WrPtr=UnpPtr;
  }

  WriteBorder=WrapUp(UnpPtr+Min(MaxWinSize,UNPACK_MAX_WRITE));

  if (WriteBorder==UnpPtr ||
      WrPtr!=UnpPtr && WrapDown(WrPtr-UnpPtr)<WrapDown(WriteBorder-UnpPtr))
    WriteBorder=WrPtr;
}

byte* Unpack::ApplyFilter(byte *Data,uint DataSize,UnpackFilter *Flt)
{
  byte *SrcData=Data;
  switch(Flt->Type)
  {
    case FILTER_E8:
    case FILTER_E8E9:
      {
        uint FileOffset=(uint)WrittenFileSize;

        const uint FileSize=0x1000000;
        byte CmpByte2=Flt->Type==FILTER_E8E9 ? 0xe9:0xe8;

        for (uint CurPos=0;CurPos+4<DataSize;)
        {
          byte CurByte=*(Data++);
          CurPos++;
          if (CurByte==0xe8 || CurByte==CmpByte2)
          {
            uint Offset=(CurPos+FileOffset)%FileSize;
            uint Addr=RawGet4(Data);

            if ((Addr & 0x80000000)!=0)
            {
              if (((Addr+Offset) & 0x80000000)==0)
                RawPut4(Addr+FileSize,Data);
            }
            else
              if (((Addr-FileSize) & 0x80000000)!=0)
                RawPut4(Addr-Offset,Data);

            Data+=4;
            CurPos+=4;
          }
        }
      }
      return SrcData;
    case FILTER_ARM:

      {
        uint FileOffset=(uint)WrittenFileSize;

        for (uint CurPos=0;CurPos+3<DataSize;CurPos+=4)
        {
          byte *D=Data+CurPos;
          if (D[3]==0xeb)
          {
            uint Offset=D[0]+uint(D[1])*0x100+uint(D[2])*0x10000;
            Offset-=(FileOffset+CurPos)/4;
            D[0]=(byte)Offset;
            D[1]=(byte)(Offset>>8);
            D[2]=(byte)(Offset>>16);
          }
        }
      }
      return SrcData;
    case FILTER_DELTA:
      {

        uint Channels=Flt->Channels,SrcPos=0;

        FilterDstMemory.resize(DataSize);
        byte *DstData=FilterDstMemory.data();

        for (uint CurChannel=0;CurChannel<Channels;CurChannel++)
        {
          byte PrevByte=0;
          for (uint DestPos=CurChannel;DestPos<DataSize;DestPos+=Channels)
            DstData[DestPos]=(PrevByte-=Data[SrcPos++]);
        }
        return DstData;
      }

  }
  return NULL;
}

void Unpack::UnpWriteArea(size_t StartPtr,size_t EndPtr)
{
  if (EndPtr!=StartPtr)
    UnpSomeRead=true;

  if (Fragmented)
  {
    size_t SizeToWrite=WrapDown(EndPtr-StartPtr);
    while (SizeToWrite>0)
    {
      size_t BlockSize=FragWindow.GetBlockSize(StartPtr,SizeToWrite);
      UnpWriteData(&FragWindow[StartPtr],BlockSize);
      SizeToWrite-=BlockSize;
      StartPtr=WrapUp(StartPtr+BlockSize);
    }
  }
  else
    if (EndPtr<StartPtr)
    {
      UnpWriteData(Window+StartPtr,MaxWinSize-StartPtr);
      UnpWriteData(Window,EndPtr);
    }
    else
      UnpWriteData(Window+StartPtr,EndPtr-StartPtr);
}

void Unpack::UnpWriteData(byte *Data,size_t Size)
{
  if (WrittenFileSize>=DestUnpSize)
    return;
  size_t WriteSize=Size;
  int64 LeftToWrite=DestUnpSize-WrittenFileSize;
  if ((int64)WriteSize>LeftToWrite)
    WriteSize=(size_t)LeftToWrite;
  UnpIO->UnpWrite(Data,WriteSize);
  WrittenFileSize+=Size;
}

void Unpack::UnpInitData50(bool Solid)
{
  if (!Solid)
    TablesRead5=false;
}

bool Unpack::ReadBlockHeader(BitInput &Inp,UnpackBlockHeader &Header)
{
  Header.HeaderSize=0;

  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-7)
    if (!UnpReadBuf())
      return false;
  Inp.faddbits((8-Inp.InBit)&7);

  byte BlockFlags=byte(Inp.fgetbits()>>8);
  Inp.faddbits(8);
  uint ByteCount=((BlockFlags>>3)&3)+1;

  if (ByteCount==4)
    return false;

  Header.HeaderSize=2+ByteCount;

  Header.BlockBitSize=(BlockFlags&7)+1;

  byte SavedCheckSum=Inp.fgetbits()>>8;
  Inp.faddbits(8);

  int BlockSize=0;
  for (uint I=0;I<ByteCount;I++)
  {
    BlockSize+=(Inp.fgetbits()>>8)<<(I*8);
    Inp.addbits(8);
  }

  Header.BlockSize=BlockSize;
  byte CheckSum=byte(0x5a^BlockFlags^BlockSize^(BlockSize>>8)^(BlockSize>>16));

  if (CheckSum!=SavedCheckSum)
    return false;

  Header.BlockStart=Inp.InAddr;

  ReadBorder=Min(ReadBorder,Header.BlockStart+Header.BlockSize-1);

  Header.LastBlockInFile=(BlockFlags & 0x40)!=0;
  Header.TablePresent=(BlockFlags & 0x80)!=0;

  return true;
}

bool Unpack::ReadTables(BitInput &Inp,UnpackBlockHeader &Header,UnpackBlockTables &Tables)
{
  if (!Header.TablePresent)
    return true;

  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-25)
    if (!UnpReadBuf())
      return false;

  byte BitLength[BC];
  for (uint I=0;I<BC;I++)
  {
    uint Length=(byte)(Inp.fgetbits() >> 12);
    Inp.faddbits(4);
    if (Length==15)
    {
      uint ZeroCount=(byte)(Inp.fgetbits() >> 12);
      Inp.faddbits(4);
      if (ZeroCount==0)
        BitLength[I]=15;
      else
      {
        ZeroCount+=2;
        while (ZeroCount-- > 0 && I<ASIZE(BitLength))
          BitLength[I++]=0;
        I--;
      }
    }
    else
      BitLength[I]=Length;
  }

  MakeDecodeTables(BitLength,&Tables.BD,BC);

  byte Table[HUFF_TABLE_SIZEX];
  const uint TableSize=ExtraDist ? HUFF_TABLE_SIZEX:HUFF_TABLE_SIZEB;
  for (uint I=0;I<TableSize;)
  {
    if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-5)
      if (!UnpReadBuf())
        return false;
    uint Number=DecodeNumber(Inp,&Tables.BD);
    if (Number<16)
    {
      Table[I]=Number;
      I++;
    }
    else
      if (Number<18)
      {
        uint N;
        if (Number==16)
        {
          N=(Inp.fgetbits() >> 13)+3;
          Inp.faddbits(3);
        }
        else
        {
          N=(Inp.fgetbits() >> 9)+11;
          Inp.faddbits(7);
        }
        if (I==0)
        {

          return false;
        }
        else
          while (N-- > 0 && I<TableSize)
          {
            Table[I]=Table[I-1];
            I++;
          }
      }
      else
      {
        uint N;
        if (Number==18)
        {
          N=(Inp.fgetbits() >> 13)+3;
          Inp.faddbits(3);
        }
        else
        {
          N=(Inp.fgetbits() >> 9)+11;
          Inp.faddbits(7);
        }
        while (N-- > 0 && I<TableSize)
          Table[I++]=0;
      }
  }
  TablesRead5=true;
  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop)
    return false;
  MakeDecodeTables(&Table[0],&Tables.LD,NC);
  uint DCodes=ExtraDist ? DCX : DCB;
  MakeDecodeTables(&Table[NC],&Tables.DD,DCodes);
  MakeDecodeTables(&Table[NC+DCodes],&Tables.LDD,LDC);
  MakeDecodeTables(&Table[NC+DCodes+LDC],&Tables.RD,RC);
  return true;
}

void Unpack::InitFilters()
{
  Filters.clear();
}

FragmentedWindow::FragmentedWindow()
{
  memset(Mem,0,sizeof(Mem));
  memset(MemSize,0,sizeof(MemSize));
  LastAllocated=0;
}

FragmentedWindow::~FragmentedWindow()
{
  Reset();
}

void FragmentedWindow::Reset()
{
  LastAllocated=0;
  for (uint I=0;I<ASIZE(Mem);I++)
    if (Mem[I]!=NULL)
    {
      free(Mem[I]);
      Mem[I]=NULL;
    }
}

void FragmentedWindow::Init(size_t WinSize)
{
  Reset();

  uint BlockNum=0;
  size_t TotalSize=0;
  while (TotalSize<WinSize && BlockNum<ASIZE(Mem))
  {
    size_t Size=WinSize-TotalSize;

    size_t MinSize=Max(Size/(ASIZE(Mem)-BlockNum), 0x400000);

    byte *NewMem=NULL;
    while (Size>=MinSize)
    {
      NewMem=(byte *)malloc(Size);
      if (NewMem!=NULL)
        break;
      Size-=Size/32;
    }
    if (NewMem==NULL)
      throw std::bad_alloc();

    memset(NewMem,0,Size);

    Mem[BlockNum]=NewMem;
    TotalSize+=Size;
    MemSize[BlockNum]=TotalSize;
    BlockNum++;
  }
  if (TotalSize<WinSize)
    throw std::bad_alloc();
  LastAllocated=WinSize;
}

byte& FragmentedWindow::operator [](size_t Item)
{
  if (Item<MemSize[0])
    return Mem[0][Item];
  for (uint I=1;I<ASIZE(MemSize);I++)
    if (Item<MemSize[I])
      return Mem[I][Item-MemSize[I-1]];
  return Mem[0][0];
}

void FragmentedWindow::CopyString(uint Length,size_t Distance,size_t &UnpPtr,bool FirstWinDone,size_t MaxWinSize)
{
  size_t SrcPtr=UnpPtr-Distance;
  if (Distance>UnpPtr)
  {
    SrcPtr+=MaxWinSize;

    if (Distance>MaxWinSize || !FirstWinDone)
    {
      while (Length-- > 0)
      {
        (*this)[UnpPtr]=0;
        if (++UnpPtr>=MaxWinSize)
          UnpPtr-=MaxWinSize;
      }
      return;
    }
  }

  while (Length-- > 0)
  {
    (*this)[UnpPtr]=(*this)[SrcPtr];
    if (++SrcPtr>=MaxWinSize)
      SrcPtr-=MaxWinSize;
    if (++UnpPtr>=MaxWinSize)
      UnpPtr-=MaxWinSize;
  }
}

void FragmentedWindow::CopyData(byte *Dest,size_t WinPos,size_t Size)
{
  for (size_t I=0;I<Size;I++)
    Dest[I]=(*this)[WinPos+I];
}

size_t FragmentedWindow::GetBlockSize(size_t StartPos,size_t RequiredSize)
{
  for (uint I=0;I<ASIZE(MemSize);I++)
    if (StartPos<MemSize[I])
      return Min(MemSize[I]-StartPos,RequiredSize);
  return 0;
}

Unpack::Unpack(ComprDataIO *DataIO)
:Inp(true),VMCodeInp(true)
{
  UnpIO=DataIO;
  Window=NULL;
  Fragmented=false;
  Suspended=false;
  UnpSomeRead=false;
  ExtraDist=false;
#ifdef RAR_SMP
  MaxUserThreads=1;
  UnpThreadPool=NULL;
  ReadBufMT=NULL;
  UnpThreadData=NULL;
#endif
  AllocWinSize=0;
  MaxWinSize=0;
  MaxWinMask=0;

  UnpInitData(false);
#ifndef SFX_MODULE

  UnpInitData15(false);
  InitHuff();
#endif
}

Unpack::~Unpack()
{
#ifndef SFX_MODULE
  InitFilters30(false);
#endif

  Alloc.delete_l<byte>(Window);
#ifdef RAR_SMP
  delete UnpThreadPool;
  delete[] ReadBufMT;
  delete[] UnpThreadData;
#endif
}

#ifdef RAR_SMP
void Unpack::SetThreads(uint Threads)
{

  MaxUserThreads=Min(Threads,8);
  UnpThreadPool=new ThreadPool(MaxUserThreads);
}
#endif

void Unpack::Init(uint64 WinSize,bool Solid)
{

  const size_t MinAllocSize=0x40000;
  if (WinSize<MinAllocSize)
    WinSize=MinAllocSize;

  if (WinSize>Min(0x10000000000ULL,UNPACK_MAX_DICT))
    throw std::bad_alloc();

  if (WinSize>0x80000000 && sizeof(size_t)<=4)
    throw std::bad_alloc();

  if (!Solid || Window==nullptr)
  {
    MaxWinSize=(size_t)WinSize;
    MaxWinMask=MaxWinSize-1;
  }

  if (WinSize<=AllocWinSize)
    return;

  if (Solid && (Window!=NULL || Fragmented && WinSize>FragWindow.GetWinSize()))
    throw std::bad_alloc();

  Alloc.delete_l<byte>(Window);
  Window=nullptr;

  try
  {
    if (!Fragmented)
      Window=Alloc.new_l<byte>((size_t)WinSize,false);
  }
  catch (std::bad_alloc)
  {
  }

  if (Window==nullptr)
    if (WinSize<0x1000000 || sizeof(size_t)>4)
      throw std::bad_alloc();
    else
    {
      if (WinSize>FragWindow.GetWinSize())
        FragWindow.Init((size_t)WinSize);
      Fragmented=true;
    }

  if (!Fragmented)
  {

    AllocWinSize=WinSize;
  }
}

void Unpack::DoUnpack(uint Method,bool Solid)
{

  switch(Method)
  {
#ifndef SFX_MODULE
    case 15:
      if (!Fragmented)
        Unpack15(Solid);
      break;
    case 20:
    case 26:
      if (!Fragmented)
        Unpack20(Solid);
      break;
    case 29:
      if (!Fragmented)
        Unpack29(Solid);
      break;
#endif
    case VER_PACK5:
    case VER_PACK7:
      ExtraDist=(Method==VER_PACK7);
#ifdef RAR_SMP
      if (MaxUserThreads>1)
      {

          if (!Fragmented)
          {
            Unpack5MT(Solid);
            break;
          }
      }
#endif
      Unpack5(Solid);
      break;
  }
}

void Unpack::UnpInitData(bool Solid)
{
  if (!Solid)
  {
    OldDist[0]=OldDist[1]=OldDist[2]=OldDist[3]=(size_t)-1;

    OldDistPtr=0;

    LastDist=(uint)-1;
    LastLength=0;

    memset(&BlockTables,0,sizeof(BlockTables));
    UnpPtr=WrPtr=0;
    PrevPtr=0;
    FirstWinDone=false;
    WriteBorder=Min(MaxWinSize,UNPACK_MAX_WRITE);
  }

  InitFilters();

  Inp.InitBitInput();
  WrittenFileSize=0;
  ReadTop=0;
  ReadBorder=0;

  memset(&BlockHeader,0,sizeof(BlockHeader));
  BlockHeader.BlockSize=-1;
#ifndef SFX_MODULE
  UnpInitData20(Solid);
  UnpInitData30(Solid);
#endif
  UnpInitData50(Solid);
}

void Unpack::MakeDecodeTables(byte *LengthTable,DecodeTable *Dec,uint Size)
{

  Dec->MaxNum=Size;

  uint LengthCount[16];
  memset(LengthCount,0,sizeof(LengthCount));
  for (size_t I=0;I<Size;I++)
    LengthCount[LengthTable[I] & 0xf]++;

  LengthCount[0]=0;

  memset(Dec->DecodeNum,0,Size*sizeof(*Dec->DecodeNum));

  Dec->DecodePos[0]=0;

  Dec->DecodeLen[0]=0;

  uint UpperLimit=0;

  for (size_t I=1;I<16;I++)
  {

    UpperLimit+=LengthCount[I];

    uint LeftAligned=UpperLimit<<(16-I);

    UpperLimit*=2;

    Dec->DecodeLen[I]=(uint)LeftAligned;

    Dec->DecodePos[I]=Dec->DecodePos[I-1]+LengthCount[I-1];
  }

  uint CopyDecodePos[ASIZE(Dec->DecodePos)];
  memcpy(CopyDecodePos,Dec->DecodePos,sizeof(CopyDecodePos));

  for (uint I=0;I<Size;I++)
  {

    byte CurBitLength=LengthTable[I] & 0xf;

    if (CurBitLength!=0)
    {

      uint LastPos=CopyDecodePos[CurBitLength];

      Dec->DecodeNum[LastPos]=(ushort)I;

      CopyDecodePos[CurBitLength]++;
    }
  }

  switch (Size)
  {
    case NC:
    case NC20:
    case NC30:
      Dec->QuickBits=MAX_QUICK_DECODE_BITS;
      break;
    default:
      Dec->QuickBits=MAX_QUICK_DECODE_BITS>3 ? MAX_QUICK_DECODE_BITS-3 : 0;
      break;
  }

  uint QuickDataSize=1<<Dec->QuickBits;

  uint CurBitLength=1;

  for (uint Code=0;Code<QuickDataSize;Code++)
  {

    uint BitField=Code<<(16-Dec->QuickBits);

    while (CurBitLength<ASIZE(Dec->DecodeLen) && BitField>=Dec->DecodeLen[CurBitLength])
      CurBitLength++;

    Dec->QuickLen[Code]=CurBitLength;

    uint Dist=BitField-Dec->DecodeLen[CurBitLength-1];

    Dist>>=(16-CurBitLength);

    uint Pos;
    if (CurBitLength<ASIZE(Dec->DecodePos) &&
        (Pos=Dec->DecodePos[CurBitLength]+Dist)<Size)
    {

      Dec->QuickNum[Code]=Dec->DecodeNum[Pos];
    }
    else
    {

      Dec->QuickNum[Code]=0;
    }
  }
}

#ifdef RARDLL
bool DllVolChange(CommandData *Cmd,std::wstring &NextName);
static bool DllVolNotify(CommandData *Cmd,const std::wstring &NextName);
#endif

bool MergeArchive(Archive &Arc,ComprDataIO *DataIO,bool ShowFileName,wchar Command)
{
  CommandData *Cmd=Arc.GetCommandData();

  HEADER_TYPE HeaderType=Arc.GetHeaderType();
  FileHeader *hd=HeaderType==HEAD_SERVICE ? &Arc.SubHead:&Arc.FileHead;
  bool SplitHeader=(HeaderType==HEAD_FILE || HeaderType==HEAD_SERVICE) &&
                   hd->SplitAfter;

  if (DataIO!=NULL && SplitHeader)
  {
    bool PackedHashPresent=Arc.Format==RARFMT50 ||
         hd->UnpVer>=20 && hd->FileHash.CRC32!=0xffffffff;
    if (PackedHashPresent &&
        !DataIO->PackedDataHash.Cmp(&hd->FileHash,hd->UseHashKey ? hd->HashKey:NULL))
      uiMsg(UIERROR_CHECKSUMPACKED, Arc.FileName, hd->FileName);
  }

  bool PrevVolEncrypted=Arc.Encrypted;

  int64 PosBeforeClose=Arc.Tell();

  if (DataIO!=NULL)
    DataIO->ProcessedArcSize+=DataIO->LastArcSize;

  Arc.Close();

  std::wstring NextName=Arc.FileName;
  NextVolumeName(NextName,!Arc.NewNumbering);

#if !defined(SFX_MODULE) && !defined(RARDLL)
  bool RecoveryDone=false;
#endif
  bool OldSchemeTested=false;

  bool FailedOpen=false;
#if !defined(SILENT)

  if (Cmd->VolumePause && !uiAskNextVolume(NextName))
    FailedOpen=true;
#endif

  uint OpenMode = Cmd->OpenShared ? FMF_OPENSHARED : 0;

  if (!FailedOpen)
    while (!Arc.Open(NextName,OpenMode))
    {

      if (DataIO!=NULL)
        DataIO->TotalArcSize=0;

      if (!OldSchemeTested)
      {

        std::wstring AltNextName=Arc.FileName;
        NextVolumeName(AltNextName,true);
        OldSchemeTested=true;
        if (Arc.Open(AltNextName,OpenMode))
        {
          NextName=AltNextName;
          break;
        }
      }
#ifdef RARDLL
      if (!DllVolChange(Cmd,NextName))
      {
        FailedOpen=true;
        break;
      }
#else

#ifndef SFX_MODULE
      if (!RecoveryDone)
      {
        RecVolumesRestore(Cmd,Arc.FileName,true);
        RecoveryDone=true;
        continue;
      }
#endif

      if (!Cmd->VolumePause && !IsRemovable(NextName))
      {
        FailedOpen=true;
        break;
      }
#ifndef SILENT
      if (Cmd->AllYes || !uiAskNextVolume(NextName))
#endif
      {
        FailedOpen=true;
        break;
      }

#endif
    }

  if (FailedOpen)
  {
    uiMsg(UIERROR_MISSINGVOL,NextName);
    Arc.Open(Arc.FileName,OpenMode);
    Arc.Seek(PosBeforeClose,SEEK_SET);
    return false;
  }

  if (Command=='T' || Command=='X' || Command=='E')
    mprintf(St(Command=='T' ? MTestVol:MExtrVol),Arc.FileName.c_str());

  Arc.CheckArc(true);
#ifdef RARDLL
  if (!DllVolNotify(Cmd,NextName))
    return false;
#endif

  if (Arc.Encrypted!=PrevVolEncrypted)
  {

    uiMsg(UIERROR_BADARCHIVE,Arc.FileName);
    ErrHandler.Exit(RARX_BADARC);
  }

  if (SplitHeader)
    Arc.SearchBlock(HeaderType);
  else
    Arc.ReadHeader();
  if (Arc.GetHeaderType()==HEAD_FILE)
  {
    Arc.ConvertAttributes();
    Arc.Seek(Arc.NextBlockPos-Arc.FileHead.PackSize,SEEK_SET);
  }
  if (ShowFileName && !Cmd->DisableNames)
  {
    mprintf(St(MExtrPoints),Arc.FileHead.FileName.c_str());
    if (!Cmd->DisablePercentage)
      mprintf(L"     ");
  }
  if (DataIO!=NULL)
  {
    if (HeaderType==HEAD_ENDARC)
      DataIO->UnpVolume=false;
    else
    {
      DataIO->UnpVolume=hd->SplitAfter;
      DataIO->SetPackedSizeToRead(hd->PackSize);
    }

    DataIO->AdjustTotalArcSize(&Arc);

    DataIO->CurUnpRead=0;

    DataIO->PackedDataHash.Init(hd->FileHash.Type,Cmd->Threads);
  }
  return true;
}

#ifdef RARDLL
bool DllVolChange(CommandData *Cmd,std::wstring &NextName)
{
  bool DllVolChanged=false,DllVolAborted=false;

  if (Cmd->Callback!=NULL)
  {
    std::wstring OrgNextName=NextName;

    std::vector<wchar> NameBuf(MAXPATHSIZE);
    std::copy(NextName.data(), NextName.data() + NextName.size() + 1, NameBuf.begin());

    if (Cmd->Callback(UCM_CHANGEVOLUMEW,Cmd->UserData,(LPARAM)NameBuf.data(),RAR_VOL_ASK)==-1)
      DllVolAborted=true;
    else
    {
      NextName=NameBuf.data();
      if (OrgNextName!=NextName)
        DllVolChanged=true;
      else
      {
        std::string NextNameA;
        WideToChar(NextName,NextNameA);
        std::string OrgNextNameA=NextNameA;

        std::vector<char> NameBufA(MAXPATHSIZE);
        std::copy(NextNameA.data(), NextNameA.data() + NextNameA.size() + 1, NameBufA.begin());

        if (Cmd->Callback(UCM_CHANGEVOLUME,Cmd->UserData,(LPARAM)NameBufA.data(),RAR_VOL_ASK)==-1)
          DllVolAborted=true;
        else
        {
          NextNameA=NameBufA.data();
          if (OrgNextNameA!=NextNameA)
          {

            CharToWide(NextNameA,NextName);
            DllVolChanged=true;
          }
        }
      }
    }
  }
  if (!DllVolChanged && Cmd->ChangeVolProc!=NULL)
  {
    std::string NextNameA;
    WideToChar(NextName,NextNameA);

    std::vector<char> NameBufA(MAXPATHSIZE);
    std::copy(NextNameA.data(), NextNameA.data() + NextNameA.size() + 1, NameBufA.begin());

    int RetCode=Cmd->ChangeVolProc(NameBufA.data(),RAR_VOL_ASK);
    if (RetCode==0)
      DllVolAborted=true;
    else
    {
      NextNameA=NameBufA.data();
      CharToWide(NextNameA,NextName);
    }
  }

  if (DllVolAborted || Cmd->Callback==NULL && Cmd->ChangeVolProc==NULL)
  {
    Cmd->DllError=ERAR_EOPEN;
    return false;
  }
  return true;
}
#endif

#ifdef RARDLL
static bool DllVolNotify(CommandData *Cmd,const std::wstring &NextName)
{
  std::string NextNameA;
  WideToChar(NextName,NextNameA);

  if (Cmd->Callback!=NULL)
  {
    if (Cmd->Callback(UCM_CHANGEVOLUMEW,Cmd->UserData,(LPARAM)NextName.data(),RAR_VOL_NOTIFY)==-1)
      return false;
    if (Cmd->Callback(UCM_CHANGEVOLUME,Cmd->UserData,(LPARAM)NextNameA.data(),RAR_VOL_NOTIFY)==-1)
      return false;
  }
  if (Cmd->ChangeVolProc!=NULL)
  {
    int RetCode=Cmd->ChangeVolProc((char *)NextNameA.data(),RAR_VOL_NOTIFY);
    if (RetCode==0)
      return false;
  }
  return true;
}
#endif
