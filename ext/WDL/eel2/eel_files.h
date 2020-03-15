#ifndef __EEL_FILES_H__
#define __EEL_FILES_H__

// should include eel_strings.h before this, probably
//#define EEL_FILE_OPEN(fn,mode) ((instance)opaque)->OpenFile(fn,mode)
//#define EEL_FILE_GETFP(fp) ((instance)opaque)->GetFileFP(fp)
//#define EEL_FILE_CLOSE(fpindex) ((instance)opaque)->CloseFile(fpindex)

static EEL_F NSEEL_CGEN_CALL _eel_fopen(void *opaque, EEL_F *fn_index, EEL_F *mode_index)
{
  EEL_STRING_MUTEXLOCK_SCOPE
  const char *fn = EEL_STRING_GET_FOR_INDEX(*fn_index,NULL);
  const char *mode = EEL_STRING_GET_FOR_INDEX(*mode_index,NULL);
  if (!fn || !mode) return 0;
  return (EEL_F) EEL_FILE_OPEN(fn,mode);
}
static EEL_F NSEEL_CGEN_CALL _eel_fclose(void *opaque, EEL_F *fpp) 
{ 
  EEL_F ret=EEL_FILE_CLOSE((int)*fpp); 
#ifdef EEL_STRING_DEBUGOUT
  if (ret < 0) EEL_STRING_DEBUGOUT("fclose(): file handle %f not valid",*fpp);
#endif
  return ret;
}
static EEL_F NSEEL_CGEN_CALL _eel_fgetc(void *opaque, EEL_F *fpp) 
{
  EEL_STRING_MUTEXLOCK_SCOPE
  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (fp) return (EEL_F)fgetc(fp);
#ifdef EEL_STRING_DEBUGOUT
  EEL_STRING_DEBUGOUT("fgetc(): file handle %f not valid",*fpp);
#endif
  return -1.0;
}

static EEL_F NSEEL_CGEN_CALL _eel_ftell(void *opaque, EEL_F *fpp) 
{
  EEL_STRING_MUTEXLOCK_SCOPE
  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (fp) return (EEL_F)ftell(fp);
#ifdef EEL_STRING_DEBUGOUT
  EEL_STRING_DEBUGOUT("ftell(): file handle %f not valid",*fpp);
#endif
  return -1.0;
}
static EEL_F NSEEL_CGEN_CALL _eel_fflush(void *opaque, EEL_F *fpp) 
{
  EEL_STRING_MUTEXLOCK_SCOPE
  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (fp) { fflush(fp); return 0.0; }
#ifdef EEL_STRING_DEBUGOUT
  EEL_STRING_DEBUGOUT("fflush(): file handle %f not valid",*fpp);
#endif
  return -1.0;
}

static EEL_F NSEEL_CGEN_CALL _eel_feof(void *opaque, EEL_F *fpp) 
{
  EEL_STRING_MUTEXLOCK_SCOPE
  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (fp) return feof(fp) ? 1.0 : 0.0;
#ifdef EEL_STRING_DEBUGOUT
  EEL_STRING_DEBUGOUT("feof(): file handle %f not valid",*fpp);
#endif
  return -1.0;
}
static EEL_F NSEEL_CGEN_CALL _eel_fseek(void *opaque, EEL_F *fpp, EEL_F *offset, EEL_F *wh) 
{
  EEL_STRING_MUTEXLOCK_SCOPE
  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (fp) return fseek(fp, (int) *offset, *wh<0 ? SEEK_SET : *wh > 0 ? SEEK_END : SEEK_CUR);
#ifdef EEL_STRING_DEBUGOUT
  EEL_STRING_DEBUGOUT("fseek(): file handle %f not valid",*fpp);
#endif
  return -1.0;
}
static EEL_F NSEEL_CGEN_CALL _eel_fgets(void *opaque, EEL_F *fpp, EEL_F *strOut)
{
  EEL_STRING_MUTEXLOCK_SCOPE
  EEL_STRING_STORAGECLASS *wr=NULL;
  EEL_STRING_GET_FOR_WRITE(*strOut, &wr);

  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (!fp)
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("fgets(): file handle %f not valid",*fpp);
#endif
    if (wr) wr->Set("");
    return 0.0;
  }
  char buf[16384];
  buf[0]=0;
  fgets(buf,sizeof(buf),fp);
  if (wr)
  {
    wr->Set(buf);
    return (EEL_F)wr->GetLength();
  }
  else
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("fgets: bad destination specifier passed %f, throwing away %d bytes",*strOut, (int)strlen(buf));
#endif
    return (int)strlen(buf);
  }
}
static EEL_F NSEEL_CGEN_CALL _eel_fread(void *opaque, EEL_F *fpp, EEL_F *strOut, EEL_F *flen)
{
  int use_len = (int) *flen;
  if (use_len < 1) return 0.0;

  EEL_STRING_MUTEXLOCK_SCOPE
  EEL_STRING_STORAGECLASS *wr=NULL;
  EEL_STRING_GET_FOR_WRITE(*strOut, &wr);
  if (!wr)
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("fread: bad destination specifier passed %f, not reading %d bytes",*strOut, use_len);
#endif
    return -1;
  }

  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (!fp)
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("fread(): file handle %f not valid",*fpp);
#endif
    if (wr) wr->Set("");
    return 0.0;
  }

  wr->SetLen(use_len);
  if (wr->GetLength() != use_len)
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("fread: error allocating storage for read of %d bytes", use_len);
#endif
    return -1.0;
  }
  use_len = (int)fread((char *)wr->Get(),1,use_len,fp);
  wr->SetLen(use_len > 0 ? use_len : 0, true);
  return (EEL_F) use_len;
}

static EEL_F NSEEL_CGEN_CALL _eel_fwrite(void *opaque, EEL_F *fpp, EEL_F *strOut, EEL_F *flen)
{
  EEL_STRING_MUTEXLOCK_SCOPE
  int use_len = (int) *flen;

  EEL_STRING_STORAGECLASS *wr=NULL;
  const char *str=EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
  if (!wr && !str)
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("fwrite: bad source specifier passed %f, not writing %d bytes",*strOut, use_len);
#endif
    return -1.0;
  }
  if (!wr)
  {
    const int ssl = (int)strlen(str);
    if (use_len < 1 || use_len > ssl) use_len = ssl;
  }
  else 
  {
    if (use_len < 1 || use_len > wr->GetLength()) use_len = wr->GetLength();
  }

  if (use_len < 1) return 0.0;

  FILE *fp = EEL_FILE_GETFP((int)*fpp);
  if (!fp) 
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("fwrite(): file handle %f not valid",*fpp);
#endif
    return 0.0;
  } 

  return (EEL_F) fwrite(str,1,use_len,fp);
}

static EEL_F NSEEL_CGEN_CALL _eel_fprintf(void *opaque, INT_PTR nparam, EEL_F **parm)
{
  if (opaque && nparam > 1)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    FILE *fp = EEL_FILE_GETFP((int)*(parm[0]));
    if (!fp) 
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("fprintf(): file handle %f not valid",parm[0][0]);
#endif
      return 0.0;
    }

    EEL_STRING_STORAGECLASS *wr_src=NULL;
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*(parm[1]),&wr_src);
    if (fmt)
    {
      char buf[16384];
      const int len = eel_format_strings(opaque,fmt,wr_src?(fmt+wr_src->GetLength()) : NULL, buf,(int)sizeof(buf),(int)nparam-2,parm+2);

      if (len >= 0)
      {
        return (EEL_F) fwrite(buf,1,len,fp);
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("fprintf: bad format string %s",fmt);
#endif
      }
    }
    else
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("fprintf: bad format specifier passed %f",*(parm[1]));
#endif
    }
  }
  return 0.0;
}

void EEL_file_register()
{
  NSEEL_addfunc_retval("fopen",2,NSEEL_PProc_THIS,&_eel_fopen);

  NSEEL_addfunc_retval("fread",3,NSEEL_PProc_THIS,&_eel_fread);
  NSEEL_addfunc_retval("fgets",2,NSEEL_PProc_THIS,&_eel_fgets);
  NSEEL_addfunc_retval("fgetc",1,NSEEL_PProc_THIS,&_eel_fgetc);

  NSEEL_addfunc_retval("fwrite",3,NSEEL_PProc_THIS,&_eel_fwrite);
  NSEEL_addfunc_varparm("fprintf",2,NSEEL_PProc_THIS,&_eel_fprintf);

  NSEEL_addfunc_retval("fseek",3,NSEEL_PProc_THIS,&_eel_fseek);
  NSEEL_addfunc_retval("ftell",1,NSEEL_PProc_THIS,&_eel_ftell);
  NSEEL_addfunc_retval("feof",1,NSEEL_PProc_THIS,&_eel_feof);
  NSEEL_addfunc_retval("fflush",1,NSEEL_PProc_THIS,&_eel_fflush);
  NSEEL_addfunc_retval("fclose",1,NSEEL_PProc_THIS,&_eel_fclose);
}

#ifdef EEL_WANT_DOCUMENTATION
static const char *eel_file_function_reference =
"fopen\t\"fn\",\"mode\"\tOpens a file \"fn\" with mode \"mode\". For read, use \"r\" or \"rb\", write \"w\" or \"wb\". Returns a positive integer on success.\0"
"fclose\tfp\tCloses a file previously opened with fopen().\0"
"fread\tfp,#str,length\tReads from file fp into #str, up to length bytes. Returns actual length read, or negative if error.\0"
"fgets\tfp,#str\tReads a line from file fp into #str. Returns length of #str read.\0"
"fgetc\tfp\tReads a character from file fp, returns -1 if EOF.\0"
"fwrite\tfp,#str,len\tWrites up to len characters of #str to file fp. If len is less than 1, the full contents of #str will be written. Returns the number of bytes written to file.\0"
"fprintf\tfp,\"format\"[,...]\tFormats a string and writes it to file fp. For more information on format specifiers, see sprintf(). Returns bytes written to file.\0"
"fseek\tfp,offset,whence\tSeeks file fp, offset bytes from whence reference. Whence negative specifies start of file, positive whence specifies end of file, and zero whence specifies current file position.\0"
"ftell\tfp\tRetunrs the current file position.\0"
"feof\tfp\tReturns nonzero if the file fp is at the end of file.\0"
"fflush\tfp\tIf file fp is open for writing, flushes out any buffered data to disk.\0"
;
#endif


#endif
