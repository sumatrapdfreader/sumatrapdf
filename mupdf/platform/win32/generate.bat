@echo off

if not exist scripts/fontdump.c cd ../..
if not exist scripts/fontdump.c goto usage
if not exist generated mkdir generated

cl /nologo -Iinclude scripts/fontdump.c setargv.obj
cl /nologo -Iinclude scripts/cmapdump.c setargv.obj
cl /nologo -Iinclude scripts/cquote.c setargv.obj
cl /nologo -Iinclude scripts/bin2hex.c setargv.obj

if not exist fontdump.exe goto usage
if not exist cmapdump.exe goto usage
if not exist cquote.exe goto usage
if not exist bin2hex.exe goto usage

if not exist generated/gen_font_base14.h fontdump.exe generated/gen_font_base14.h resources/fonts/*.cff
if not exist generated/gen_font_droid.h fontdump.exe generated/gen_font_droid.h resources/fonts/droid/DroidSans.ttf resources/fonts/droid/DroidSansMono.ttf
if not exist generated/gen_font_cjk.h fontdump.exe generated/gen_font_cjk.h resources/fonts/droid/DroidSansFallback.ttf
if not exist generated/gen_font_cjk_full.h fontdump.exe generated/gen_font_cjk_full.h resources/fonts/droid/DroidSansFallbackFull.ttf

if not exist generated/gen_cmap_cns.h cmapdump.exe generated/gen_cmap_cns.h resources\cmaps\cns\*
if not exist generated/gen_cmap_gb.h cmapdump.exe generated/gen_cmap_gb.h resources\cmaps\gb\*
if not exist generated/gen_cmap_japan.h cmapdump.exe generated/gen_cmap_japan.h resources\cmaps\japan\*
if not exist generated/gen_cmap_korea.h cmapdump.exe generated/gen_cmap_korea.h resources\cmaps\korea\*

if not exist generated/gen_adobe_ca.h bin2hex.exe generated/gen_adobe_ca.h resources/certs/AdobeCA.p7c
if not exist generated/gen_js_util.h cquote.exe generated/gen_js_util.h source/pdf/js/pdf-util.js

del cmapdump.obj fontdump.obj cquote.obj bin2hex.obj cmapdump.exe fontdump.exe cquote.exe bin2hex.exe

goto fin

:usage
echo ERROR: Run this script in the mupdf directory.
echo ERROR: Run this script in a Visual Studio command prompt.
pause

:fin
