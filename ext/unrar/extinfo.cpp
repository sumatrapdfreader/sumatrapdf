#include "rar.hpp"

#include "hardlinks.cpp"
#include "win32stm.cpp"

#ifdef _WIN_ALL
#include "win32acl.cpp"
#include "win32lnk.cpp"
#endif

#ifdef _UNIX
#include "uowners.cpp"
#ifdef SAVE_LINKS
#include "ulinks.cpp"
#endif
#endif



#ifndef SFX_MODULE
void SetExtraInfo20(CommandData *Cmd,Archive &Arc,wchar *Name)
{
  switch(Arc.SubBlockHead.SubType)
  {
#ifdef _UNIX
    case UO_HEAD:
      if (Cmd->ProcessOwners)
        ExtractUnixOwner20(Arc,Name);
      break;
#endif
#ifdef _WIN_ALL
    case NTACL_HEAD:
      if (Cmd->ProcessOwners)
        ExtractACL20(Arc,Name);
      break;
    case STREAM_HEAD:
      ExtractStreams20(Arc,Name);
      break;
#endif
  }
}
#endif


void SetExtraInfo(CommandData *Cmd,Archive &Arc,wchar *Name)
{
#ifdef _UNIX
  if (Cmd->ProcessOwners && Arc.Format==RARFMT15 &&
      Arc.SubHead.CmpName(SUBHEAD_TYPE_UOWNER))
    ExtractUnixOwner30(Arc,Name);
#endif
#ifdef _WIN_ALL
  if (Cmd->ProcessOwners && Arc.SubHead.CmpName(SUBHEAD_TYPE_ACL))
    ExtractACL(Arc,Name);
  if (Arc.SubHead.CmpName(SUBHEAD_TYPE_STREAM))
    ExtractStreams(Arc,Name);
#endif
}




bool ExtractSymlink(CommandData *Cmd,ComprDataIO &DataIO,Archive &Arc,const wchar *LinkName)
{
#if defined(SAVE_LINKS) && defined(_UNIX)
  // For RAR 3.x archives we process links even in test mode to skip link data.
  if (Arc.Format==RARFMT15)
    return ExtractUnixLink30(DataIO,Arc,LinkName);
  if (Arc.Format==RARFMT50)
    return ExtractUnixLink50(LinkName,&Arc.FileHead);
#elif defined _WIN_ALL
  // RAR 5.0 archives store link information in file header, so there is
  // no need to additionally test it if we do not create a file.
  if (Arc.Format==RARFMT50)
    return CreateReparsePoint(Cmd,LinkName,&Arc.FileHead);
#endif
  return false;
}
