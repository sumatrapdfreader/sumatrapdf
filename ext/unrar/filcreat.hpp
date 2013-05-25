#ifndef _RAR_FILECREATE_
#define _RAR_FILECREATE_

bool FileCreate(RAROptions *Cmd,File *NewFile,wchar *Name,size_t MaxNameSize,
                OVERWRITE_MODE Mode,bool *UserReject,int64 FileSize=INT64NDF,
                RarTime *FileTime=NULL,bool WriteOnly=false);

bool GetAutoRenamedName(wchar *Name,size_t MaxNameSize);

#if defined(_WIN_ALL) && !defined(_WIN_CE)
bool UpdateExistingShortName(const wchar *Name);
#endif

#endif
