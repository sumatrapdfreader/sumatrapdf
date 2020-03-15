#ifndef _WDL_FILEBROWSE_H_
#define _WDL_FILEBROWSE_H_

#ifdef _WIN32
#include <windows.h>
#else
#include "swell/swell.h"
#endif


bool WDL_ChooseDirectory(HWND parent, const char *text, const char *initialdir, char *fn, int fnsize, bool preservecwd);
bool WDL_ChooseFileForSave(HWND parent, 
                                      const char *text, 
                                      const char *initialdir, 
                                      const char *initialfile, 
                                      const char *extlist,
                                      const char *defext,
                                      bool preservecwd,
                                      char *fn, 
                                      int fnsize,
                                      const char *dlgid=NULL, 
                                      void *dlgProc=NULL, 
#ifdef _WIN32
                                      HINSTANCE hInstance=NULL
#else
                                      struct SWELL_DialogResourceIndex *reshead=NULL
#endif
                                      );


char *WDL_ChooseFileForOpen(HWND parent,
                                        const char *text, 
                                        const char *initialdir,  
                                        const char *initialfile, 
                                        const char *extlist,
                                        const char *defext,

                                        bool preservecwd,
                                        bool allowmul, 

                                        const char *dlgid=NULL, 
                                        void *dlgProc=NULL, 
#ifdef _WIN32
                                        HINSTANCE hInstance=NULL
#else
                                        struct SWELL_DialogResourceIndex *reshead=NULL
#endif
                                        );


// allowmul=1: multi-selection in same folder,
//   the double NULL terminated return value of WDL_ChooseFileForOpen2 is like:
//   "some/single/path\0bla1.txt\0bla2.txt\0bla3.txt\0\0"
// allowmul=2: multi-selection with multipath support (vista+)
//   the double NULL terminated return value of WDL_ChooseFileForOpen2 is like:
//   "\0/some/path1/bla1.txt\0/some/path2/bla2.txt\0/some/path3/bla3.txt\0\0"                                        
char *WDL_ChooseFileForOpen2(HWND parent,
                                        const char *text, 
                                        const char *initialdir,  
                                        const char *initialfile, 
                                        const char *extlist,
                                        const char *defext,

                                        bool preservecwd,
                                        int allowmul,

                                        const char *dlgid=NULL, 
                                        void *dlgProc=NULL, 
#ifdef _WIN32
                                        HINSTANCE hInstance=NULL
#else
                                        struct SWELL_DialogResourceIndex *reshead=NULL
#endif
                                        );


#endif