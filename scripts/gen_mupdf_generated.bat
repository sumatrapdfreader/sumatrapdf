@echo off
if not exist mupdf\generated mkdir mupdf\generated

@rem set cmap=dbg\cmapdump.exe
set cmap=%1
@rem echo cmap=%cmap%

@echo off
setlocal disableDelayedExpansion

if exist mupdf\generated\gen_cmap_cns.h goto skip_cns

set "files="
for %%a in (mupdf\resources\cmaps\cns\*) do call set files=%%files%% "%%a"
echo %files%
%cmap% mupdf\generated\gen_cmap_cns.h %files%
echo "generated mupdf\generated\gen_cmap_cns.h"

:skip_cns
if exist mupdf\generated\gen_cmap_gb.h goto skip_gb

set "files="
for %%a in (mupdf\resources\cmaps\gb\*) do call set files=%%files%% "%%a"
echo %files%
%cmap% mupdf\generated\gen_cmap_gb.h %files%
echo "generated mupdf\generated\gen_cmap_gb.h"

:skip_gb
if exist mupdf\generated\gen_cmap_japan.h goto skip_japan

set "files="
for %%a in (mupdf\resources\cmaps\japan\*) do call set files=%%files%% "%%a"
echo %files%
%cmap% mupdf\generated\gen_cmap_japan.h %files%
echo "generated mupdf\generated\gen_cmap_japan.h"

:skip_japan
if exist mupdf\generated\gen_cmap_korea.h goto skip_korea

set "files="
for %%a in (mupdf\resources\cmaps\korea\*) do call set files=%%files%% "%%a"
echo %files%
%cmap% mupdf\generated\gen_cmap_korea.h %files%
echo "generated mupdf\generated\gen_cmap_korea.h"

:skip_korea
