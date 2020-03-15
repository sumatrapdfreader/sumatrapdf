#include <stdlib.h>
#include <stdio.h>

#include "../lice.h"

void usage(const char *a, const char *b)
{
  if (a||b) printf("%s: %s\n",a,b);
  printf("Usage: imgs2gif [-w width] [-h height] [-d ms] [-skipempty] [-leaddelay ms] output.gif file1.jpg [file2.png ...]\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int delay=10;
  int size_w=0,size_h=0;
  int i;
  int rv=0;
  int leaddelay=0;
  bool skipempty=false;
  void *gifOut=NULL;
  const char *fn=NULL;

  LICE_MemBitmap lastfr, fr,fr2;
  int accum_lat=0;

  for (i=1;i<argc;i++)
  {
    if (argv[i][0]=='-')
    {
      if (fn||gifOut) usage("Flag after filename",argv[i]);

      if (!strcmp(argv[i],"-w")) 
      {
        if (++i >= argc) usage("Missing parameter",argv[i-1]);
        size_w = atoi(argv[i]);
      }
      else if (!strcmp(argv[i],"-h")) 
      {
        if (++i >= argc) usage("Missing parameter",argv[i-1]);
        size_h = atoi(argv[i]);
      }
      else if (!strcmp(argv[i],"-d")) 
      {
        if (++i >= argc) usage("Missing parameter",argv[i-1]);
        delay = atoi(argv[i]);
      }
      else if (!strcmp(argv[i],"-skipempty")) skipempty=true;
      else if (!strcmp(argv[i],"-leaddelay")) 
      {
        if (++i >= argc) usage("Missing parameter",argv[i-1]);
        leaddelay=atoi(argv[i]);
      }
      else usage("Unknown option",argv[i]);
    }
    else if (!fn) fn=argv[i];
    else
    {
      printf("Loading %s\n",argv[i]);
      if (!LICE_LoadImage(argv[i],&fr) || !fr.getWidth() || !fr.getHeight()) 
      {
        printf("Error loading image: %s\n",argv[i]);
        continue;
      }
     
      if (!size_w) size_w=fr.getWidth();
      if (!size_h) size_h=fr.getHeight();
      LICE_MemBitmap *usefr = &fr;
      if (fr.getWidth() != size_w || fr.getHeight() != size_h)
      {
        fr2.resize(size_w,size_h);
        LICE_ScaledBlit(usefr=&fr2,&fr,0,0,size_w,size_h,0,0,fr.getWidth(),fr.getHeight(),1.0f,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
      }
       
      if (!gifOut)
      {
        gifOut=LICE_WriteGIFBegin(fn,usefr,0,leaddelay?leaddelay:delay,false);
        if (!gifOut) usage("Error writing to file",fn);
      }
      else
      {
        accum_lat += delay;
        int diffcoords[4]={0,0,size_w,size_h};
        if (LICE_BitmapCmp(usefr,&lastfr,diffcoords))
        {
          LICE_SubBitmap bm(usefr,diffcoords[0],diffcoords[1], diffcoords[2],diffcoords[3]);
          LICE_WriteGIFFrame(gifOut,&bm,diffcoords[0],diffcoords[1], true, accum_lat);
          accum_lat=0;
        }
        if (skipempty) accum_lat=0;
      }
 
      LICE_Copy(&lastfr,usefr);
    }
  }
  printf("finishing up\n");
  if (gifOut) LICE_WriteGIFEnd(gifOut);
  
  return rv;
}

#ifndef _WIN32
INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2)
{
  return 0;
}
#endif

