#ifndef _RAR_PATHFN_
#define _RAR_PATHFN_

wchar* PointToName(const wchar *Path);
wchar* PointToLastChar(const wchar *Path);
wchar* ConvertPath(const wchar *SrcPath,wchar *DestPath);
void SetName(wchar *FullName,const wchar *Name,size_t MaxSize);
void SetExt(wchar *Name,const wchar *NewExt);
void SetSFXExt(wchar *SFXName);
wchar *GetExt(const wchar *Name);
bool CmpExt(const wchar *Name,const wchar *Ext);
bool IsWildcard(const wchar *Str);
bool IsPathDiv(int Ch);
bool IsDriveDiv(int Ch);
int GetPathDisk(const wchar *Path);
void AddEndSlash(wchar *Path,size_t MaxLength);
void MakeName(const wchar *Path,const wchar *Name,wchar *Pathname,size_t MaxSize);
void GetFilePath(const wchar *FullName,wchar *Path,size_t MaxLength);
void RemoveNameFromPath(wchar *Path);
void GetRarDataPath(wchar *Path,size_t MaxSize,bool Create);
bool EnumConfigPaths(uint Number,wchar *Path,size_t MaxSize,bool Create);
void GetConfigName(const wchar *Name,wchar *FullName,size_t MaxSize,bool CheckExist,bool Create);
wchar* GetVolNumPart(const wchar *ArcName);
void NextVolumeName(wchar *ArcName,uint MaxLength,bool OldNumbering);
bool IsNameUsable(const wchar *Name);
void MakeNameUsable(char *Name,bool Extended);
void MakeNameUsable(wchar *Name,bool Extended);
char* UnixSlashToDos(char *SrcName,char *DestName=NULL,size_t MaxLength=NM);
char* DosSlashToUnix(char *SrcName,char *DestName=NULL,size_t MaxLength=NM);
wchar* UnixSlashToDos(wchar *SrcName,wchar *DestName=NULL,size_t MaxLength=NM);
wchar* DosSlashToUnix(wchar *SrcName,wchar *DestName=NULL,size_t MaxLength=NM);
void ConvertNameToFull(const wchar *Src,wchar *Dest,size_t MaxSize);
bool IsFullPath(const wchar *Path);
bool IsDiskLetter(const wchar *Path);
void GetPathRoot(const wchar *Path,wchar *Root,size_t MaxSize);
int ParseVersionFileName(wchar *Name,bool Truncate);
wchar* VolNameToFirstName(const wchar *VolName,wchar *FirstName,bool NewNumbering);
wchar* GetWideName(const char *Name,const wchar *NameW,wchar *DestW,size_t DestSize);

#ifndef SFX_MODULE
void GenerateArchiveName(wchar *ArcName,size_t MaxSize,wchar *GenerateMask,bool Archiving);
#endif

#ifdef _WIN_ALL
bool GetWinLongPath(const wchar *Src,wchar *Dest,size_t MaxSize);
#endif

#endif
