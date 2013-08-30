#include "rar.hpp"


CommandData::CommandData()
{
  Init();
}


void CommandData::Init()
{
  RAROptions::Init();

  *Command=0;
  *ArcName=0;
  FileLists=false;
  NoMoreSwitches=false;

  ListMode=RCLM_AUTO;

  BareOutput=false;


  FileArgs.Reset();
  ExclArgs.Reset();
  InclArgs.Reset();
  StoreArgs.Reset();
  ArcNames.Reset();
  NextVolSizes.Reset();
}


// Return the pointer to next position in the string and store dynamically
// allocated command line parameter in Par.
static const wchar *AllocCmdParam(const wchar *CmdLine,wchar **Par)
{
  const wchar *NextCmd=GetCmdParam(CmdLine,NULL,0);
  if (NextCmd==NULL)
    return NULL;
  size_t ParSize=NextCmd-CmdLine+2; // Parameter size including the trailing zero.
  *Par=(wchar *)malloc(ParSize*sizeof(wchar));
  if (*Par==NULL)
    return NULL;
  return GetCmdParam(CmdLine,*Par,ParSize);
}


#ifndef SFX_MODULE
void CommandData::ParseCommandLine(bool Preprocess,int argc, char *argv[])
{
#ifdef CUSTOM_CMDLINE_PARSER
  // In Windows we may prefer to implement our own command line parser
  // to avoid replacing \" by " in standard parser. Such replacing corrupts
  // destination paths like "dest path\" in extraction commands.
  const wchar *CmdLine=GetCommandLine();

  wchar *Par;
  for (bool FirstParam=true;;FirstParam=false)
  {
    if ((CmdLine=AllocCmdParam(CmdLine,&Par))==NULL)
      break;
    bool Code=true;
    if (!FirstParam) // First parameter is the executable name.
      if (Preprocess)
        Code=PreprocessSwitch(Par);
      else
        ParseArg(Par);
    free(Par);
    if (Preprocess && !Code)
      break;
  }
#else
  Array<wchar> Arg;
  for (int I=1;I<argc;I++)
  {
    Arg.Alloc(strlen(argv[I])+1);
    CharToWide(argv[I],&Arg[0],Arg.Size());
    if (Preprocess)
    {
      if (!PreprocessSwitch(&Arg[0]))
        break;
    }
    else
      ParseArg(&Arg[0]);
  }
#endif
  if (!Preprocess)
    ParseDone();
}
#endif


#ifndef SFX_MODULE
void CommandData::ParseArg(wchar *Arg)
{
  if (IsSwitch(*Arg) && !NoMoreSwitches)
    if (Arg[1]=='-')
      NoMoreSwitches=true;
    else
      ProcessSwitch(Arg+1);
  else
    if (*Command==0)
    {
      wcsncpy(Command,Arg,ASIZE(Command));


#ifndef GUI
      *Command=toupperw(*Command);
      // 'I' and 'S' commands can contain case sensitive strings after
      // the first character, so we must not modify their case.
      // 'S' can contain SFX name, which case is important in Unix.
      if (*Command!='I' && *Command!='S')
        wcsupper(Command);
#endif
    }
    else
      if (*ArcName==0)
        wcsncpyz(ArcName,Arg,ASIZE(ArcName));
      else
      {
        // Check if last character is the path separator.
        size_t Length=wcslen(Arg);
        wchar EndChar=Length==0 ? 0:Arg[Length-1];
        bool EndSeparator=IsDriveDiv(EndChar) || IsPathDiv(EndChar);

        wchar CmdChar=toupperw(*Command);
        bool Add=wcschr(L"AFUM",CmdChar)!=NULL;
        bool Extract=CmdChar=='X' || CmdChar=='E';
        if (EndSeparator && !Add)
          wcsncpyz(ExtrPath,Arg,ASIZE(ExtrPath));
        else
          if ((Add || CmdChar=='T') && (*Arg!='@' || ListMode==RCLM_REJECT_LISTS))
            FileArgs.AddString(Arg);
          else
          {
            FindData FileData;
            bool Found=FindFile::FastFind(Arg,&FileData);
            if ((!Found || ListMode==RCLM_ACCEPT_LISTS) && 
                ListMode!=RCLM_REJECT_LISTS && *Arg=='@' && !IsWildcard(Arg))
            {
              FileLists=true;

              RAR_CHARSET Charset=FilelistCharset;

#if defined(_WIN_ALL) && !defined(GUI)
              // for compatibility reasons we use OEM encoding
              // in Win32 console version by default

//              if (Charset==RCH_DEFAULT)
//                Charset=RCH_OEM;
#endif

              ReadTextFile(Arg+1,&FileArgs,false,true,Charset,true,true,true);

            }
            else
              if (Found && FileData.IsDir && Extract && *ExtrPath==0)
              {
                wcsncpyz(ExtrPath,Arg,ASIZE(ExtrPath));
                AddEndSlash(ExtrPath,ASIZE(ExtrPath));
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
    Test=false;        // Switch '-t' is senseless for 'X', 'E', 'P' commands.

  // Suppress the copyright message and final end of line for 'lb' and 'vb'.
  if ((CmdChar=='L' || CmdChar=='V') && Command[1]=='B')
    BareOutput=true;
}


#ifndef SFX_MODULE
void CommandData::ParseEnvVar()
{
  char *EnvStr=getenv("RAR");
  if (EnvStr!=NULL)
  {
    Array<wchar> EnvStrW(strlen(EnvStr)+1);
    CharToWide(EnvStr,&EnvStrW[0],EnvStrW.Size());
    ProcessSwitchesString(&EnvStrW[0]);
  }
}
#endif



#ifndef SFX_MODULE
// Preprocess those parameters, which must be processed before the rest of
// command line. Return 'false' to stop further processing.
bool CommandData::PreprocessSwitch(const wchar *Switch)
{
  if (IsSwitch(Switch[0]))
  {
    Switch++;
    char SwitchA[1024];
    WideToChar(Switch,SwitchA,ASIZE(SwitchA));
    if (wcsicomp(Switch,L"-")==0) // Switch "--".
      return false;
    if (wcsicomp(Switch,L"cfg-")==0)
      ConfigDisabled=true;
#ifndef GUI
    if (wcsnicomp(Switch,L"ilog",4)==0)
    {
      // Ensure that correct log file name is already set
      // if we need to report an error when processing the command line.
      ProcessSwitch(Switch);
      InitLogOptions(LogName,ErrlogCharset);
    }
#endif
    if (wcsnicomp(Switch,L"sc",2)==0)
    {
      // Process -sc before reading any file lists.
      ProcessSwitch(Switch);
#ifndef GUI
      if (*LogName!=0)
        InitLogOptions(LogName,ErrlogCharset);
#endif
    }
  }
  return true;
}
#endif


#if !defined(GUI) && !defined(SFX_MODULE)
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
    }
  }
}
#endif


#ifndef SFX_MODULE
void CommandData::ProcessSwitchesString(const wchar *Str)
{
  wchar *Par;
  while ((Str=AllocCmdParam(Str,&Par))!=NULL)
  {
    if (IsSwitch(*Par))
      ProcessSwitch(Par+1);
    free(Par);
  }
}
#endif


#if !defined(SFX_MODULE)
void CommandData::ProcessSwitch(const wchar *Switch)
{

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
          AppendArcNameToPath=true;
          break;
#ifndef SFX_MODULE
        case 'G':
          if (Switch[2]=='-' && Switch[3]==0)
            GenerateArcName=0;
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
        case 'N': // Reserved for archive name.
          break;
        case 'O':
          AddArcOnly=true;
          break;
        case 'P':
          wcscpy(ArcPath,Switch+2);
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
      if (Switch[2]==0)
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
        }
      break;
    case 'D':
      if (Switch[2]==0)
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
          }
          break;
        case 'E':
          ProcessEA=false;
          break;
        default:
          if (Switch[1]=='+')
          {
            InclFileAttr=GetExclAttr(Switch+2);
            InclAttrSet=true;
          }
          else
            ExclFileAttr=GetExclAttr(Switch+1);
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
            Password.Set(Switch+2);
            cleandata((void *)Switch,wcslen(Switch)*sizeof(Switch[0]));
          }
          else
            if (!Password.IsSet())
            {
              GetPassword(PASSWORD_GLOBAL,NULL,&Password);
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
        wcsncpyz(LogName,Switch[4]!=0 ? Switch+4:DefLogName,ASIZE(LogName));
        break;
      }
      if (wcsicomp(Switch+1,L"SND")==0)
      {
        Sound=true;
        break;
      }
      if (wcsicomp(Switch+1,L"ERR")==0)
      {
        MsgStream=MSG_STDERR;
        break;
      }
      if (wcsnicomp(Switch+1,L"EML",3)==0)
      {
        wcsncpyz(EmailTo,Switch[4]!=0 ? Switch+4:L"@",ASIZE(EmailTo));
        break;
      }
      if (wcsicomp(Switch+1,L"NUL")==0)
      {
        MsgStream=MSG_NULL;
        break;
      }
      if (toupperw(Switch[1])=='D')
      {
        for (uint I=2;Switch[I]!=0;I++)
          switch(toupperw(Switch[I]))
          {
            case 'Q':
              MsgStream=MSG_ERRONLY;
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
          }
        break;
      }
      if (wcsicomp(Switch+1,L"OFF")==0)
      {
        Shutdown=true;
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
                  case 'T': Type=FILTER_PPM;         break;
                  case 'E': Type=FILTER_E8;          break;
                  case 'D': Type=FILTER_DELTA;       break;
                  case 'A': Type=FILTER_AUDIO;       break;
                  case 'C': Type=FILTER_RGB;         break;
                  case 'I': Type=FILTER_ITANIUM;     break;
                  case 'R': Type=FILTER_ARM;         break;
                }
                if (*Str=='+' || *Str=='-')
                  State=*(Str++)=='+' ? FILTER_FORCE:FILTER_DISABLE;
                FilterModes[Type].State=State;
                FilterModes[Type].Param1=Param1;
                FilterModes[Type].Param2=Param2;
              }
            }
          break;
        case 'M':
          break;
        case 'D':
          break;
        case 'S':
          {
            wchar StoreNames[1024];
            wcsncpyz(StoreNames,(Switch[2]==0 ? DefaultStoreList:Switch+2),ASIZE(StoreNames));
            wchar *Names=StoreNames;
            while (*Names!=0)
            {
              wchar *End=wcschr(Names,';');
              if (End!=NULL)
                *End=0;
              if (*Names=='.')
                Names++;
              wchar Mask[NM];
              if (wcspbrk(Names,L"*?.")==NULL)
                swprintf(Mask,ASIZE(Mask),L"*.%s",Names);
              else
                wcsncpyz(Mask,Names,ASIZE(Mask));
              StoreArgs.AddString(Mask);
              if (End==NULL)
                break;
              Names=End+1;
            }
          }
          break;
#ifdef RAR_SMP
        case 'T':
          Threads=atoiw(Switch+2);
          if (Threads>MaxPoolThreads || Threads<1)
            BadSwitch(Switch);
          else
          {
          }
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
        {
          RAR_CHARSET Charset=FilelistCharset;

#if defined(_WIN_ALL) && !defined(GUI)
          // for compatibility reasons we use OEM encoding
          // in Win32 console version by default

//          if (Charset==RCH_DEFAULT)
//            Charset=RCH_OEM;
#endif

          ReadTextFile(Switch+2,Args,false,true,Charset,true,true,true);
        }
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
          break;
#endif
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
        GetPassword(PASSWORD_GLOBAL,NULL,&Password);
        eprintf(L"\n");
      }
      else
      {
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
          case 'L':
            if (IsDigit(Switch[2]))
              FileSizeLess=atoilw(Switch+2);
            break;
          case 'M':
            if (IsDigit(Switch[2]))
              FileSizeMore=atoilw(Switch+2);
            break;
          case 'C':
            {
              bool AlreadyBad=false; // Avoid reporting "bad switch" several times.

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
                default :
                  BadSwitch(Switch);
                  AlreadyBad=true;
                  break;
              };
              if (!AlreadyBad)
                if (Switch[3]==0)
                  CommentCharset=FilelistCharset=ErrlogCharset=rch;
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
                      default:
                        BadSwitch(Switch);
                        AlreadyBad=true;
                        break;
                    }
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
          FileTimeBefore.SetAgeText(Switch+2);
          break;
        case 'N':
          FileTimeAfter.SetAgeText(Switch+2);
          break;
        case 'B':
          FileTimeBefore.SetIsoText(Switch+2);
          break;
        case 'A':
          FileTimeAfter.SetIsoText(Switch+2);
          break;
        case 'S':
          {
            EXTTIME_MODE Mode=EXTTIME_HIGH3;
            bool CommonMode=Switch[2]>='0' && Switch[2]<='4';
            if (CommonMode)
              Mode=(EXTTIME_MODE)(Switch[2]-'0');
            if (Switch[2]=='-')
              Mode=EXTTIME_NONE;
            if (CommonMode || Switch[2]=='-' || Switch[2]=='+' || Switch[2]==0)
              xmtime=xctime=xatime=Mode;
            else
            {
              if (Switch[3]>='0' && Switch[3]<='4')
                Mode=(EXTTIME_MODE)(Switch[3]-'0');
              if (Switch[3]=='-')
                Mode=EXTTIME_NONE;
              switch(toupperw(Switch[2]))
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
              }
            }
          }
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
          VolSize=VOLSIZE_AUTO; // UnRAR -v switch for list command.
          break;
      }
      break;
    case 'W':
      wcsncpyz(TempPath,Switch+1,ASIZE(TempPath));
      AddEndSlash(TempPath,ASIZE(TempPath));
      break;
    case 'Y':
      AllYes=true;
      break;
    case 'Z':
      if (Switch[1]==0)
      {
#ifndef GUI // stdin is not supported by WinRAR.
        // If comment file is not specified, we read data from stdin.
        wcscpy(CommentFile,L"stdin");
#endif
      }
      else
        wcsncpyz(CommentFile,Switch+1,ASIZE(CommentFile));
      break;
#ifndef GUI
    case '?' :
      OutHelp(RARX_SUCCESS);
      break;
#endif
    default :
      BadSwitch(Switch);
      break;
  }
}
#endif


#ifndef SFX_MODULE
void CommandData::BadSwitch(const wchar *Switch)
{
  mprintf(St(MUnknownOption),Switch);
  ErrHandler.Exit(RARX_USERERROR);
}
#endif


#ifndef GUI
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
  wchar Version[50];
  int Beta=RARVER_BETA;
  if (Beta!=0)
    swprintf(Version,ASIZE(Version),L"%d.%02d %ls %d",RARVER_MAJOR,RARVER_MINOR,St(MBeta),RARVER_BETA);
  else
    swprintf(Version,ASIZE(Version),L"%d.%02d",RARVER_MAJOR,RARVER_MINOR);
#ifdef UNRAR
  mprintf(St(MUCopyright),Version,RARVER_YEAR);
#else
#endif
#endif
#endif
}
#endif


inline bool CmpMSGID(MSGID i1,MSGID i2)
{
#ifdef MSGID_INT
  return i1==i2;
#else
  // If MSGID is const char*, we cannot compare pointers only.
  // Pointers to different instances of same string can differ,
  // so we need to compare complete strings.
  return strcmp(i1,i2)==0;
#endif
}

void CommandData::OutHelp(RAR_EXIT ExitCode)
{
#if !defined(GUI) && !defined(SILENT)
  OutTitle();
  static MSGID Help[]={
#ifdef SFX_MODULE
    // Console SFX switches definition.
    MCHelpCmd,MSHelpCmdE,MSHelpCmdT,MSHelpCmdV
#elif defined(UNRAR)
    // UnRAR switches definition.
    MUNRARTitle1,MRARTitle2,MCHelpCmd,MCHelpCmdE,MCHelpCmdL,
    MCHelpCmdP,MCHelpCmdT,MCHelpCmdV,MCHelpCmdX,MCHelpSw,MCHelpSwm,
    MCHelpSwAT,MCHelpSwAC,MCHelpSwAD,MCHelpSwAG,MCHelpSwAI,MCHelpSwAP,
    MCHelpSwCm,MCHelpSwCFGm,MCHelpSwCL,MCHelpSwCU,
    MCHelpSwDH,MCHelpSwEP,MCHelpSwEP3,MCHelpSwF,MCHelpSwIDP,MCHelpSwIERR,
    MCHelpSwINUL,MCHelpSwIOFF,MCHelpSwKB,MCHelpSwN,MCHelpSwNa,MCHelpSwNal,
    MCHelpSwO,MCHelpSwOC,MCHelpSwOR,MCHelpSwOW,MCHelpSwP,
    MCHelpSwPm,MCHelpSwR,MCHelpSwRI,MCHelpSwSL,MCHelpSwSM,MCHelpSwTA,
    MCHelpSwTB,MCHelpSwTN,MCHelpSwTO,MCHelpSwTS,MCHelpSwU,MCHelpSwVUnr,
    MCHelpSwVER,MCHelpSwVP,MCHelpSwX,MCHelpSwXa,MCHelpSwXal,MCHelpSwY
#else
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
      MCHelpSwEP2,MCHelpSwOC,MCHelpSwDR,MCHelpSwRI
    };
    bool Found=false;
    for (int J=0;J<sizeof(Win32Only)/sizeof(Win32Only[0]);J++)
      if (CmpMSGID(Help[I],Win32Only[J]))
      {
        Found=true;
        break;
      }
    if (Found)
      continue;
#endif
#if !defined(_UNIX) && !defined(_WIN_ALL)
    if (CmpMSGID(Help[I],MCHelpSwOW))
      continue;
#endif
#if !defined(_WIN_ALL) && !defined(_EMX)
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
    if (CmpMSGID(Help[I],MCHelpSwEE))
    {
#if defined(_EMX) && !defined(_DJGPP)
      if (_osmode != OS2_MODE)
        continue;
#else
      continue;
#endif
    }
#endif
    mprintf(St(Help[I]));
  }
  mprintf(L"\n");
  ErrHandler.Exit(ExitCode);
#endif
}


// Return 'true' if we need to exclude the file from processing as result
// of -x switch. If CheckInclList is true, we also check the file against
// the include list created with -n switch.
bool CommandData::ExclCheck(const wchar *CheckName,bool Dir,bool CheckFullPath,bool CheckInclList)
{
  if (ExclCheckArgs(&ExclArgs,Dir,CheckName,CheckFullPath,MATCH_WILDSUBPATH))
    return true;
  if (!CheckInclList || InclArgs.ItemsCount()==0)
    return false;
  if (ExclCheckArgs(&InclArgs,Dir,CheckName,false,MATCH_WILDSUBPATH))
    return false;
  return true;
}


bool CommandData::ExclCheckArgs(StringList *Args,bool Dir,const wchar *CheckName,bool CheckFullPath,int MatchMode)
{
  wchar *Name=ConvertPath(CheckName,NULL);
  wchar FullName[NM];
  wchar CurMask[NM+1]; // We reserve the space to append "*" to mask.
  *FullName=0;
  Args->Rewind();
  while (Args->GetString(CurMask,ASIZE(CurMask)-1))
  {
    wchar *LastMaskChar=PointToLastChar(CurMask);
    bool DirMask=IsPathDiv(*LastMaskChar); // Mask for directories only.

    if (Dir)
    {
      // CheckName is a directory.
      if (DirMask)
      {
        // We process the directory and have the directory exclusion mask.
        // So let's convert "mask\" to "mask" and process it normally.
        
        *LastMaskChar=0;
      }
      else
      {
        // If mask has wildcards in name part and does not have the trailing
        // '\' character, we cannot use it for directories.
      
        if (IsWildcard(PointToName(CurMask)))
          continue;
      }
    }
    else
    {
      // If we process a file inside of directory excluded by "dirmask\".
      // we want to exclude such file too. So we convert "dirmask\" to
      // "dirmask\*". It is important for operations other than archiving.
      // When archiving, directory matched by "dirmask\" is excluded
      // from further scanning.

      if (DirMask)
        wcscat(CurMask,L"*");
    }

#ifndef SFX_MODULE
    if (CheckFullPath && IsFullPath(CurMask))
    {
      // We do not need to do the special "*\" processing here, because
      // unlike the "else" part of this "if", now we convert names to full
      // format, so they all include the path, which is matched by "*\"
      // correctly. Moreover, removing "*\" from mask would break
      // the comparison, because now all names have the path.

      if (*FullName==0)
        ConvertNameToFull(CheckName,FullName,ASIZE(FullName));
      if (CmpName(CurMask,FullName,MatchMode))
        return true;
    }
    else
#endif
    {
      wchar NewName[NM+2],*CurName=Name;
      if (CurMask[0]=='*' && IsPathDiv(CurMask[1]))
      {
        // We want "*\name" to match 'name' not only in subdirectories,
        // but also in the current directory. We convert the name
        // from 'name' to '.\name' to be matched by "*\" part even if it is
        // in current directory.
        NewName[0]='.';
        NewName[1]=CPATHDIVIDER;
        wcsncpyz(NewName+2,Name,ASIZE(NewName)-2);
        CurName=NewName;
      }

      if (CmpName(ConvertPath(CurMask,NULL),CurName,MatchMode))
        return true;
    }
  }
  return false;
}


#ifndef SFX_MODULE
// Now this function performs only one task and only in Windows version:
// it skips symlinks to directories if -e1024 switch is specified.
// Symlinks are skipped in ScanTree class, so their entire contents
// is skipped too. Without this function we would check the attribute
// only directly before archiving, so we would skip the symlink record,
// but not the contents of symlinked directory.
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




#ifndef SFX_MODULE
// Return 'true' if we need to exclude the file from processing.
bool CommandData::TimeCheck(RarTime &ft)
{
  if (FileTimeBefore.IsSet() && ft>=FileTimeBefore)
    return true;
  if (FileTimeAfter.IsSet() && ft<=FileTimeAfter)
    return true;
  return false;
}
#endif


#ifndef SFX_MODULE
// Return 'true' if we need to exclude the file from processing.
bool CommandData::SizeCheck(int64 Size)
{
  if (FileSizeLess!=INT64NDF && Size>=FileSizeLess)
    return(true);
  if (FileSizeMore!=INT64NDF && Size<=FileSizeMore)
    return(true);
  return(false);
}
#endif




int CommandData::IsProcessFile(FileHeader &FileHead,bool *ExactMatch,int MatchType)
{
  if (wcslen(FileHead.FileName)>=NM)
    return 0;
  bool Dir=FileHead.Dir;
  if (ExclCheck(FileHead.FileName,Dir,false,true))
    return 0;
#ifndef SFX_MODULE
  if (TimeCheck(FileHead.mtime))
    return 0;
  if ((FileHead.FileAttr & ExclFileAttr)!=0 || InclAttrSet && (FileHead.FileAttr & InclFileAttr)==0)
    return 0;
  if (!Dir && SizeCheck(FileHead.UnpSize))
    return 0;
#endif
  wchar *ArgName;
  FileArgs.Rewind();
  for (int StringCount=1;(ArgName=FileArgs.GetString())!=NULL;StringCount++)
    if (CmpName(ArgName,FileHead.FileName,MatchType))
    {
      if (ExactMatch!=NULL)
        *ExactMatch=wcsicompc(ArgName,FileHead.FileName)==0;
      return StringCount;
    }
  return 0;
}


#ifndef GUI
void CommandData::ProcessCommand()
{
#ifndef SFX_MODULE

  const wchar *SingleCharCommands=L"FUADPXETK";
  if (Command[0]!=0 && Command[1]!=0 && wcschr(SingleCharCommands,Command[0])!=NULL || *ArcName==0)
    OutHelp(*Command==0 ? RARX_SUCCESS:RARX_USERERROR); // Return 'success' for 'rar' without parameters.

#ifdef _UNIX
  if (GetExt(ArcName)==NULL && (!FileExist(ArcName) || IsDir(GetFileAttr(ArcName))))
    wcsncatz(ArcName,L".rar",ASIZE(ArcName));
#else
  if (GetExt(ArcName)==NULL)
    wcsncatz(ArcName,L".rar",ASIZE(ArcName));
#endif

  if (wcschr(L"AFUMD",*Command)==NULL)
  {
    if (GenerateArcName)
      GenerateArchiveName(ArcName,ASIZE(ArcName),GenerateMask,false);

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
    case 'I':
      {
        CmdExtract Extract(this);
        Extract.DoExtract(this);
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
    mprintf(L"\n");
}
#endif


void CommandData::AddArcName(const wchar *Name)
{
  ArcNames.AddString(Name);
}


bool CommandData::GetArcName(wchar *Name,int MaxSize)
{
  return ArcNames.GetString(Name,MaxSize);
}


bool CommandData::IsSwitch(int Ch)
{
#if defined(_WIN_ALL) || defined(_EMX)
  return(Ch=='-' || Ch=='/');
#else
  return(Ch=='-');
#endif
}


#ifndef SFX_MODULE
uint CommandData::GetExclAttr(const wchar *Str)
{
  if (IsDigit(*Str))
    return(wcstol(Str,NULL,0));

  uint Attr=0;
  while (*Str!=0)
  {
    switch(toupperw(*Str))
    {
#ifdef _UNIX
      case 'D':
        Attr|=S_IFDIR;
        break;
      case 'V':
        Attr|=S_IFCHR;
        break;
#elif defined(_WIN_ALL) || defined(_EMX)
      case 'R':
        Attr|=0x1;
        break;
      case 'H':
        Attr|=0x2;
        break;
      case 'S':
        Attr|=0x4;
        break;
      case 'D':
        Attr|=0x10;
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
bool CommandData::CheckWinSize()
{
  // Define 0x100000000 as macro to avoid troubles with older compilers.
  const uint64 MaxDictSize=INT32TO64(1,0);
  // Limit the dictionary size to 4 GB.
  for (uint64 I=0x10000;I<=MaxDictSize;I*=2)
    if (WinSize==I)
      return true;
  WinSize=0x400000;
  return false;
}
#endif


#ifndef SFX_MODULE
void CommandData::ReportWrongSwitches(RARFORMAT Format)
{
  if (Format==RARFMT15)
  {
    if (HashType!=HASH_CRC32)
    {
      mprintf(St(MIncompatSwitch),L"-ht",4);
    }
#ifdef _WIN_ALL
    if (SaveSymLinks)
    {
      mprintf(St(MIncompatSwitch),L"-ol",4);
    }
#endif
    if (SaveHardLinks)
    {
      mprintf(St(MIncompatSwitch),L"-oh",4);
    }

#ifdef _WIN_ALL


#endif
    if (QOpenMode!=QOPEN_AUTO)
    {
      mprintf(St(MIncompatSwitch),L"-qo",4);
    }
/*
    // We use 64 MB for both formats and reduce it for RAR 4.x later.
    if (WinSize>0x400000)
    {
      wchar SwMD[10];
      swprintf(SwMD,ASIZE(SwMD),L"-md%dm",WinSize/0x100000);
      mprintf(St(MIncompatSwitch),SwMD,4);
    }
*/
  }
  if (Format==RARFMT50)
  {
  }
}
#endif
