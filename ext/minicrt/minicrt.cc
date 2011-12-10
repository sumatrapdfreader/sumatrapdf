/*
This is a single-file implementation of Visual Studio crt
made in order to reduce the size of executables.

This is preliminary, I might change my mind or never
finish this code, but this is the plan:
* implementation is in a single file, to make it easy to integrate
  in Sumatra and other projects
* I'll start from scratch i.e. start with no code at all and
  add the necessary functions one by one, guided by what
  linker tells us is missing. This is to learn what is the absolutely
  minimum to include and review all the code that is included
* I'll reuse as much as possible from msvcrt.dll
* I'll use the code already written in omaha's minicrt project
  (but only after reviewing each function)
* the code that comes in *.obj files will have to be written
* other places that I might steal the code from:
 - http://llvm.org/svn/llvm-project/compiler-rt/trunk/
 - http://www.jbox.dk/sanos/source/

More info:
* http://kobyk.wordpress.com/2007/07/20/dynamically-linking-with-msvcrtdll-using-visual-c-2005/

First order of business: figure out how to tell it
to use functions from msvcrt.dll defined in msvcrt.def
(e.g. rand, srand, _time64, memcmp etc.)
*/

/*
TODO: here's the current score for EbookTest app:

1>Touch.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>HtmlWindow.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>et.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>UtilTests.obj : error LNK2019: unresolved external symbol __RTC_CheckEsp referenced in function "void __cdecl GeomTest(void)" (?GeomTest@@YAXXZ)
1>WinUtil.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>DialogSizer.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>Transactions.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>BencUtil.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>FileUtil.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>Http.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>StrUtil.obj : error LNK2001: unresolved external symbol __RTC_CheckEsp
1>Touch.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>HtmlWindow.obj : error LNK2019: unresolved external symbol __RTC_Shutdown referenced in function "public: virtual long __stdcall HW_IInternetProtocol::Start(wchar_t const *,struct IInternetProtocolSink *,struct IInternetBindInfo *,unsigned long,unsigned long)" (?Start@HW_IInternetProtocol@@UAGJPB_WPAUIInternetProtocolSink@@PAUIInternetBindInfo@@KK@Z)
1>et.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>UtilTests.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>WinUtil.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>DialogSizer.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>Transactions.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>BencUtil.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>FileUtil.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>Http.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>StrUtil.obj : error LNK2001: unresolved external symbol __RTC_Shutdown
1>Touch.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>HtmlWindow.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>et.obj : error LNK2019: unresolved external symbol __RTC_InitBase referenced in function _WinMain@16
1>UtilTests.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>WinUtil.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>DialogSizer.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>Transactions.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>BencUtil.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>FileUtil.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>Http.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>StrUtil.obj : error LNK2001: unresolved external symbol __RTC_InitBase
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>HtmlWindow.obj : error LNK2019: unresolved external symbol @_RTC_CheckStackVars@8 referenced in function "void __cdecl UnregisterInternetProtocolFactory(void)" (?UnregisterInternetProtocolFactory@@YAXXZ)
1>et.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>UtilTests.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>WinUtil.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>DialogSizer.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>Transactions.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>BencUtil.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>FileUtil.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>Http.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>StrUtil.obj : error LNK2001: unresolved external symbol @_RTC_CheckStackVars@8
1>HtmlWindow.obj : error LNK2019: unresolved external symbol __wassert referenced in function "public: virtual long __stdcall HtmlMoniker::ParseDisplayName(struct IBindCtx *,struct IMoniker *,wchar_t *,unsigned long *,struct IMoniker * *)" (?ParseDisplayName@HtmlMoniker@@UAGJPAUIBindCtx@@PAUIMoniker@@PA_WPAKPAPAU3@@Z)
1>UtilTests.obj : error LNK2001: unresolved external symbol __wassert
1>WinUtil.obj : error LNK2001: unresolved external symbol __wassert
1>Transactions.obj : error LNK2001: unresolved external symbol __wassert
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol __wassert
1>BencUtil.obj : error LNK2001: unresolved external symbol __wassert
1>FileUtil.obj : error LNK2001: unresolved external symbol __wassert
1>Http.obj : error LNK2001: unresolved external symbol __wassert
1>StrUtil.obj : error LNK2001: unresolved external symbol __wassert
1>BencUtil.obj : error LNK2001: unresolved external symbol __purecall
1>UtilTests.obj : error LNK2019: unresolved external symbol __purecall referenced in function "void __cdecl TStrTest(void)" (?TStrTest@@YAXXZ)
1>et.obj : error LNK2019: unresolved external symbol "void __cdecl operator delete(void *)" (??3@YAXPAX@Z) referenced in function "int __cdecl RegisterWinClass(struct HINSTANCE__ *)" (?RegisterWinClass@@YAHPAUHINSTANCE__@@@Z)
1>BencUtil.obj : error LNK2001: unresolved external symbol "void __cdecl operator delete(void *)" (??3@YAXPAX@Z)
1>UtilTests.obj : error LNK2001: unresolved external symbol "void __cdecl operator delete(void *)" (??3@YAXPAX@Z)
1>DialogSizer.obj : error LNK2001: unresolved external symbol "void __cdecl operator delete(void *)" (??3@YAXPAX@Z)
1>HtmlWindow.obj : error LNK2001: unresolved external symbol "void __cdecl operator delete(void *)" (??3@YAXPAX@Z)
1>HtmlWindow.obj : error LNK2001: unresolved external symbol __free_dbg
1>UtilTests.obj : error LNK2019: unresolved external symbol __free_dbg referenced in function "void __cdecl TStrTest(void)" (?TStrTest@@YAXXZ)
1>WinUtil.obj : error LNK2001: unresolved external symbol __free_dbg
1>DialogSizer.obj : error LNK2001: unresolved external symbol __free_dbg
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol __free_dbg
1>BencUtil.obj : error LNK2001: unresolved external symbol __free_dbg
1>FileUtil.obj : error LNK2001: unresolved external symbol __free_dbg
1>Http.obj : error LNK2001: unresolved external symbol __free_dbg
1>StrUtil.obj : error LNK2001: unresolved external symbol __free_dbg
1>HtmlWindow.obj : error LNK2001: unresolved external symbol _strlen
1>BencUtil.obj : error LNK2019: unresolved external symbol _strlen referenced in function "unsigned int __cdecl str::Len(char const *)" (?Len@str@@YAIPBD@Z)
1>FileUtil.obj : error LNK2001: unresolved external symbol _strlen
1>StrUtil.obj : error LNK2001: unresolved external symbol _strlen
1>UtilTests.obj : error LNK2001: unresolved external symbol _strlen
1>HtmlWindow.obj : error LNK2001: unresolved external symbol "void * __cdecl operator new(unsigned int,int,char const *,int)" (??2@YAPAXIHPBDH@Z)
1>et.obj : error LNK2001: unresolved external symbol "void * __cdecl operator new(unsigned int,int,char const *,int)" (??2@YAPAXIHPBDH@Z)
1>BencUtil.obj : error LNK2019: unresolved external symbol "void * __cdecl operator new(unsigned int,int,char const *,int)" (??2@YAPAXIHPBDH@Z) referenced in function "public: static class BencString * __cdecl BencString::Decode(char const *,unsigned int *)" (?Decode@BencString@@SAPAV1@PBDPAI@Z)
1>Http.obj : error LNK2001: unresolved external symbol "void * __cdecl operator new(unsigned int,int,char const *,int)" (??2@YAPAXIHPBDH@Z)
1>UtilTests.obj : error LNK2001: unresolved external symbol "void * __cdecl operator new(unsigned int,int,char const *,int)" (??2@YAPAXIHPBDH@Z)
1>DialogSizer.obj : error LNK2001: unresolved external symbol "void * __cdecl operator new(unsigned int,int,char const *,int)" (??2@YAPAXIHPBDH@Z)
1>BencUtil.obj : error LNK2019: unresolved external symbol _memchr referenced in function "public: static class BencString * __cdecl BencString::Decode(char const *,unsigned int *)" (?Decode@BencString@@SAPAV1@PBDPAI@Z)
1>BencUtil.obj : error LNK2019: unresolved external symbol __allmul referenced in function "char const * __cdecl ParseBencInt(char const *,__int64 &)" (?ParseBencInt@@YAPBDPBDAA_J@Z)
1>UtilTests.obj : error LNK2001: unresolved external symbol __allmul
1>et.obj : error LNK2019: unresolved external symbol ___security_cookie referenced in function "public: virtual class Gdiplus::Image * __thiscall Gdiplus::Image::Clone(void)" (?Clone@Image@Gdiplus@@UAEPAV12@XZ)
1>UtilTests.obj : error LNK2001: unresolved external symbol ___security_cookie
1>WinUtil.obj : error LNK2001: unresolved external symbol ___security_cookie
1>DialogSizer.obj : error LNK2001: unresolved external symbol ___security_cookie
1>HtmlWindow.obj : error LNK2001: unresolved external symbol ___security_cookie
1>BencUtil.obj : error LNK2001: unresolved external symbol ___security_cookie
1>FileUtil.obj : error LNK2001: unresolved external symbol ___security_cookie
1>Http.obj : error LNK2001: unresolved external symbol ___security_cookie
1>StrUtil.obj : error LNK2001: unresolved external symbol ___security_cookie
1>et.obj : error LNK2019: unresolved external symbol @__security_check_cookie@4 referenced in function "public: static void * __cdecl Gdiplus::GdiplusBase::operator new(unsigned int)" (??2GdiplusBase@Gdiplus@@SAPAXI@Z)
1>UtilTests.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>WinUtil.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>DialogSizer.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>HtmlWindow.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>BencUtil.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>FileUtil.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>Http.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>StrUtil.obj : error LNK2001: unresolved external symbol @__security_check_cookie@4
1>BencUtil.obj : error LNK2019: unresolved external symbol _bsearch referenced in function "private: class BencObj * __thiscall BencDict::GetObj(char const *)const " (?GetObj@BencDict@@ABEPAVBencObj@@PBD@Z)
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol _bsearch
1>BencUtil.obj : error LNK2019: unresolved external symbol _strcmp referenced in function "int __cdecl keycmp(void const *,void const *)" (?keycmp@@YAHPBX0@Z)
1>StrUtil.obj : error LNK2001: unresolved external symbol _strcmp
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol _strcmp
1>BencUtil.obj : error LNK2019: unresolved external symbol __strdup_dbg referenced in function "char * __cdecl str::Dup(char const *)" (?Dup@str@@YAPADPBD@Z)
1>StrUtil.obj : error LNK2001: unresolved external symbol __strdup_dbg
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol __strdup_dbg
1>WinUtil.obj : error LNK2001: unresolved external symbol _memcpy
1>DialogSizer.obj : error LNK2001: unresolved external symbol _memcpy
1>HtmlWindow.obj : error LNK2001: unresolved external symbol _memcpy
1>BencUtil.obj : error LNK2019: unresolved external symbol _memcpy referenced in function "void * __cdecl memdup(void *,unsigned int)" (?memdup@@YAPAXPAXI@Z)
1>Http.obj : error LNK2001: unresolved external symbol _memcpy
1>StrUtil.obj : error LNK2001: unresolved external symbol _memcpy
1>UtilTests.obj : error LNK2001: unresolved external symbol _memcpy
1>DialogSizer.obj : error LNK2001: unresolved external symbol __malloc_dbg
1>BencUtil.obj : error LNK2019: unresolved external symbol __malloc_dbg referenced in function "void * __cdecl memdup(void *,unsigned int)" (?memdup@@YAPAXPAXI@Z)
1>StrUtil.obj : error LNK2001: unresolved external symbol __malloc_dbg
1>UtilTests.obj : error LNK2001: unresolved external symbol __malloc_dbg
1>WinUtil.obj : error LNK2001: unresolved external symbol __malloc_dbg
1>UtilTests.obj : error LNK2019: unresolved external symbol __calloc_dbg referenced in function "void __cdecl StrVecTest(void)" (?StrVecTest@@YAXXZ)
1>WinUtil.obj : error LNK2001: unresolved external symbol __calloc_dbg
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol __calloc_dbg
1>HtmlWindow.obj : error LNK2001: unresolved external symbol __calloc_dbg
1>BencUtil.obj : error LNK2001: unresolved external symbol __calloc_dbg
1>FileUtil.obj : error LNK2001: unresolved external symbol __calloc_dbg
1>Http.obj : error LNK2001: unresolved external symbol __calloc_dbg
1>StrUtil.obj : error LNK2001: unresolved external symbol __calloc_dbg
1>HtmlWindow.obj : error LNK2019: unresolved external symbol _abort referenced in function "public: virtual void * __thiscall HtmlMoniker::`scalar deleting destructor'(unsigned int)" (??_GHtmlMoniker@@UAEPAXI@Z)
1>BencUtil.obj : error LNK2001: unresolved external symbol _abort
1>Http.obj : error LNK2001: unresolved external symbol _abort
1>UtilTests.obj : error LNK2001: unresolved external symbol _abort
1>WinUtil.obj : error LNK2001: unresolved external symbol _abort
1>HtmlWindow.obj : error LNK2001: unresolved external symbol _memmove
1>BencUtil.obj : error LNK2019: unresolved external symbol _memmove referenced in function "protected: class BencObj * * __thiscall Vec<class BencObj *>::MakeSpaceAt(unsigned int,unsigned int)" (?MakeSpaceAt@?$Vec@PAVBencObj@@@@IAEPAPAVBencObj@@II@Z)
1>StrUtil.obj : error LNK2001: unresolved external symbol _memmove
1>UtilTests.obj : error LNK2001: unresolved external symbol _memmove
1>WinUtil.obj : error LNK2001: unresolved external symbol _memmove
1>WinUtil.obj : error LNK2019: unresolved external symbol _memset referenced in function "wchar_t * __cdecl win::menu::ToSafeString(wchar_t const *)" (?ToSafeString@menu@win@@YAPA_WPB_W@Z)
1>HtmlWindow.obj : error LNK2001: unresolved external symbol _memset
1>et.obj : error LNK2001: unresolved external symbol _memset
1>BencUtil.obj : error LNK2001: unresolved external symbol _memset
1>Http.obj : error LNK2001: unresolved external symbol _memset
1>UtilTests.obj : error LNK2001: unresolved external symbol _memset
1>WinUtil.obj : error LNK2001: unresolved external symbol _memset
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol _wcslen
1>HtmlWindow.obj : error LNK2019: unresolved external symbol _wcslen referenced in function "void __cdecl RegisterInternetProtocolFactory(void)" (?RegisterInternetProtocolFactory@@YAXXZ)
1>FileUtil.obj : error LNK2001: unresolved external symbol _wcslen
1>StrUtil.obj : error LNK2001: unresolved external symbol _wcslen
1>UtilTests.obj : error LNK2001: unresolved external symbol _wcslen
1>WinUtil.obj : error LNK2001: unresolved external symbol _wcslen
1>WinUtil.obj : error LNK2001: unresolved external symbol __wcsdup_dbg
1>HtmlWindow.obj : error LNK2019: unresolved external symbol __wcsdup_dbg referenced in function "protected: void __thiscall HtmlWindow::EnsureAboutBlankShown(void)" (?EnsureAboutBlankShown@HtmlWindow@@IAEXXZ)
1>FileUtil.obj : error LNK2001: unresolved external symbol __wcsdup_dbg
1>Http.obj : error LNK2001: unresolved external symbol __wcsdup_dbg
1>StrUtil.obj : error LNK2001: unresolved external symbol __wcsdup_dbg
1>UtilTests.obj : error LNK2001: unresolved external symbol __wcsdup_dbg
1>FileUtil.obj : error LNK2019: unresolved external symbol _towupper referenced in function "bool __cdecl path::IsOnRemovableDrive(wchar_t const *)" (?IsOnRemovableDrive@path@@YA_NPB_W@Z)
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol _wcschr
1>FileUtil.obj : error LNK2019: unresolved external symbol _wcschr referenced in function "wchar_t const * __cdecl str::FindChar(wchar_t const *,wchar_t)" (?FindChar@str@@YAPB_WPB_W_W@Z)
1>StrUtil.obj : error LNK2001: unresolved external symbol _wcschr
1>UtilTests.obj : error LNK2001: unresolved external symbol _wcschr
1>WinUtil.obj : error LNK2001: unresolved external symbol _wcschr
1>FileUtil.obj : error LNK2019: unresolved external symbol _towlower referenced in function "bool __cdecl path::MatchWildcardsRec(wchar_t const *,wchar_t const *)" (?MatchWildcardsRec@path@@YA_NPB_W0@Z)
1>StrUtil.obj : error LNK2001: unresolved external symbol _towlower
1>FileUtil.obj : error LNK2019: unresolved external symbol _memcmp referenced in function "bool __cdecl file::StartsWith(wchar_t const *,char const *,unsigned int)" (?StartsWith@file@@YA_NPB_WPBDI@Z)
1>HtmlWindow.obj : error LNK2001: unresolved external symbol _memcmp
1>StrUtil.obj : error LNK2019: unresolved external symbol _wcscmp referenced in function "bool __cdecl str::Eq(wchar_t const *,wchar_t const *)" (?Eq@str@@YA_NPB_W0@Z)
1>UtilTests.obj : error LNK2001: unresolved external symbol _wcscmp
1>StrUtil.obj : error LNK2019: unresolved external symbol __stricmp referenced in function "bool __cdecl str::EqI(char const *,char const *)" (?EqI@str@@YA_NPBD0@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol __wcsicmp referenced in function "bool __cdecl str::EqI(wchar_t const *,wchar_t const *)" (?EqI@str@@YA_NPB_W0@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _iswspace referenced in function "bool __cdecl str::EqIS(wchar_t const *,wchar_t const *)" (?EqIS@str@@YA_NPB_W0@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _strncmp referenced in function "bool __cdecl str::EqN(char const *,char const *,unsigned int)" (?EqN@str@@YA_NPBD0I@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _wcsncmp referenced in function "bool __cdecl str::EqN(wchar_t const *,wchar_t const *,unsigned int)" (?EqN@str@@YA_NPB_W0I@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol __strnicmp referenced in function "bool __cdecl str::StartsWithI(char const *,char const *)" (?StartsWithI@str@@YA_NPBD0@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol __wcsnicmp referenced in function "bool __cdecl str::StartsWithI(wchar_t const *,wchar_t const *)" (?StartsWithI@str@@YA_NPB_W0@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _tolower referenced in function "void __cdecl str::ToLower(char *)" (?ToLower@str@@YAXPAD@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol __vsnprintf referenced in function "char * __cdecl str::FmtV(char const *,char *)" (?FmtV@str@@YAPADPBDPAD@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol __vsnwprintf referenced in function "wchar_t * __cdecl str::FmtV(wchar_t const *,char *)" (?FmtV@str@@YAPA_WPB_WPAD@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _strchr referenced in function "char const * __cdecl str::FindChar(char const *,char)" (?FindChar@str@@YAPBDPBDD@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _strncpy referenced in function "unsigned int __cdecl str::BufSet(char *,unsigned int,char const *)" (?BufSet@str@@YAIPADIPBD@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _wcsncpy referenced in function "unsigned int __cdecl str::BufSet(wchar_t *,unsigned int,wchar_t const *)" (?BufSet@str@@YAIPA_WIPB_W@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _sprintf referenced in function "char * __cdecl str::MemToHex(unsigned char const *,int)" (?MemToHex@str@@YAPADPBEH@Z)
1>StrUtil.obj : error LNK2019: unresolved external symbol _sscanf referenced in function "bool __cdecl str::HexToMem(char const *,unsigned char *,int)" (?HexToMem@str@@YA_NPBDPAEH@Z)
1>HtmlWindow.obj : error LNK2001: unresolved external symbol __fltused
1>et.obj : error LNK2001: unresolved external symbol __fltused
1>StrUtil.obj : error LNK2001: unresolved external symbol __fltused
1>UtilTests.obj : error LNK2019: unresolved external symbol __fltused referenced in function "void __cdecl TStrTest(void)" (?TStrTest@@YAXXZ)
1>WinUtil.obj : error LNK2001: unresolved external symbol __fltused
1>DialogSizer.obj : error LNK2001: unresolved external symbol __fltused
1>StrUtil.obj : error LNK2019: unresolved external symbol _iswalnum referenced in function "int __cdecl str::CmpNatural(wchar_t const *,wchar_t const *)" (?CmpNatural@str@@YAHPB_W0@Z)
1>TrivialHtmlParser.obj : error LNK2001: unresolved external symbol _iswalnum
1>StrUtil.obj : error LNK2019: unresolved external symbol _wcstod referenced in function "wchar_t const * __cdecl str::Parse(wchar_t const *,wchar_t const *,...)" (?Parse@str@@YAPB_WPB_W0ZZ)
1>StrUtil.obj : error LNK2019: unresolved external symbol _wcstol referenced in function "wchar_t const * __cdecl str::Parse(wchar_t const *,wchar_t const *,...)" (?Parse@str@@YAPB_WPB_W0ZZ)
1>StrUtil.obj : error LNK2019: unresolved external symbol _wcstoul referenced in function "wchar_t const * __cdecl str::Parse(wchar_t const *,wchar_t const *,...)" (?Parse@str@@YAPB_WPB_W0ZZ)
1>UtilTests.obj : error LNK2019: unresolved external symbol _iswdigit referenced in function "void __cdecl TStrTest(void)" (?TStrTest@@YAXXZ)
1>UtilTests.obj : error LNK2019: unresolved external symbol _wcsstr referenced in function "wchar_t const * __cdecl str::Find(wchar_t const *,wchar_t const *)" (?Find@str@@YAPB_WPB_W0@Z)
1>WinUtil.obj : error LNK2001: unresolved external symbol _wcsstr
1>UtilTests.obj : error LNK2019: unresolved external symbol _rand referenced in function "void __cdecl VecTest(void)" (?VecTest@@YAXXZ)
1>UtilTests.obj : error LNK2019: unresolved external symbol _srand referenced in function "void __cdecl VecTest(void)" (?VecTest@@YAXXZ)
1>UtilTests.obj : error LNK2019: unresolved external symbol __time64 referenced in function _time
1>UtilTests.obj : error LNK2019: unresolved external symbol __alldiv referenced in function "unsigned int __cdecl VecTestAppendFmt(void)" (?VecTestAppendFmt@@YAIXZ)
1>WinUtil.obj : error LNK2001: unresolved external symbol __alldiv
1>et.obj : error LNK2001: unresolved external symbol __alldiv
1>UtilTests.obj : error LNK2019: unresolved external symbol _qsort referenced in function "public: void __thiscall Vec<wchar_t *>::Sort(int (__cdecl*)(void const *,void const *))" (?Sort@?$Vec@PA_W@@QAEXP6AHPBX0@Z@Z)
1>et.obj : error LNK2001: unresolved external symbol _floor
1>UtilTests.obj : error LNK2019: unresolved external symbol _floor referenced in function "public: class Point<int> __thiscall Point<double>::Convert<int>(void)const " (??$Convert@H@?$Point@N@@QBE?AV?$Point@H@@XZ)
1>WinUtil.obj : error LNK2001: unresolved external symbol _floor
1>DialogSizer.obj : error LNK2001: unresolved external symbol _floor
1>HtmlWindow.obj : error LNK2001: unresolved external symbol _floor
1>et.obj : error LNK2001: unresolved external symbol __ftol2_sse
1>UtilTests.obj : error LNK2019: unresolved external symbol __ftol2_sse referenced in function "public: class Point<int> __thiscall Point<double>::Convert<int>(void)const " (??$Convert@H@?$Point@N@@QBE?AV?$Point@H@@XZ)
1>WinUtil.obj : error LNK2001: unresolved external symbol __ftol2_sse
1>DialogSizer.obj : error LNK2001: unresolved external symbol __ftol2_sse
1>HtmlWindow.obj : error LNK2001: unresolved external symbol __ftol2_sse
1>WinUtil.obj : error LNK2019: unresolved external symbol _setvbuf referenced in function "void __cdecl RedirectIOToConsole(void)" (?RedirectIOToConsole@@YAXXZ)
1>WinUtil.obj : error LNK2019: unresolved external symbol ___iob_func referenced in function "void __cdecl RedirectIOToConsole(void)" (?RedirectIOToConsole@@YAXXZ)
1>WinUtil.obj : error LNK2019: unresolved external symbol __fdopen referenced in function "void __cdecl RedirectIOToConsole(void)" (?RedirectIOToConsole@@YAXXZ)
1>WinUtil.obj : error LNK2019: unresolved external symbol __open_osfhandle referenced in function "void __cdecl RedirectIOToConsole(void)" (?RedirectIOToConsole@@YAXXZ)
1>WinUtil.obj : error LNK2019: unresolved external symbol _ceil referenced in function _ceilf
1>HtmlWindow.obj : error LNK2019: unresolved external symbol _wcsrchr referenced in function "wchar_t const * __cdecl str::FindCharLast(wchar_t const *,wchar_t)" (?FindCharLast@str@@YAPB_WPB_W_W@Z)
1>HtmlWindow.obj : error LNK2019: unresolved external symbol _atexit referenced in function "void __cdecl `dynamic initializer for 'gHtmlWindows''(void)" (??__EgHtmlWindows@@YAXXZ)
1>LINK : error LNK2001: unresolved external symbol _WinMainCRTStartup
1>obj-dbg\et.exe : fatal error LNK1120: 67 unresolved externals
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef UNICODE
#undef UNICODE
#endif

#ifdef _UNICODE
#undef _UNICODE
#endif

#include <windows.h>

extern "C" {
/* TODO: use its own heap ? */
/* TODO: use malloc/free/calloc from msvcrt.dll and 
   redirect _malloc_dbg/_free_dbg/_calloc_dbg to malloc/free/callco
   from msvcrt.dll ? */
void * __cdecl malloc(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size );
}

void __cdecl free(void * p) {
    HeapFree(GetProcessHeap(), 0, p);
}

void * __cdecl realloc(void * p, size_t size) {
    if (p)
        return HeapReAlloc(GetProcessHeap(), 0, p, size);
    else    // 'p' is 0, and HeapReAlloc doesn't act like realloc() here
        return HeapAlloc(GetProcessHeap(), 0, size);
}

void * __cdecl calloc(size_t nitems, size_t size) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, nitems * size);
}

void * __cdecl _recalloc(void * p, size_t nitems, size_t size) {
    return realloc(p, nitems * size);
}

void * __cdecl _malloc_dbg(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void __cdecl _free_dbg(void * p) {
    HeapFree(GetProcessHeap(), 0, p);
}

void * __cdecl _calloc_dbg(size_t nitems, size_t size) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, nitems * size);
}

}