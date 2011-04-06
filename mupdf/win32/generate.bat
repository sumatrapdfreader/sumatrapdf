@echo off

if not exist scripts/fontdump.c cd ..
if not exist generated mkdir generated

cl /nologo -Ifitz -Ipdf scripts/fontdump.c
cl /nologo -Ifitz -Ipdf scripts/cmapdump.c

if not exist fontdump.exe goto usage
if not exist cmapdump.exe goto usage

if not exist generated/font_base14.h fontdump.exe generated/font_base14.h fonts/Dingbats.cff fonts/NimbusMonL-Bold.cff fonts/NimbusMonL-BoldObli.cff fonts/NimbusMonL-Regu.cff fonts/NimbusMonL-ReguObli.cff fonts/NimbusRomNo9L-Medi.cff fonts/NimbusRomNo9L-MediItal.cff fonts/NimbusRomNo9L-Regu.cff fonts/NimbusRomNo9L-ReguItal.cff fonts/NimbusSanL-Bold.cff fonts/NimbusSanL-BoldItal.cff fonts/NimbusSanL-Regu.cff fonts/NimbusSanL-ReguItal.cff fonts/StandardSymL.cff

if not exist generated/font_cjk.h fontdump.exe generated/font_cjk.h fonts/droid/DroidSansFallback.ttf

if not exist generated/cmap_unicode.h cmapdump.exe generated/cmap_unicode.h cmaps/Adobe-CNS1-UCS2 cmaps/Adobe-GB1-UCS2 cmaps/Adobe-Japan1-UCS2 cmaps/Adobe-Korea1-UCS2

if not exist generated/cmap_gb.h cmapdump.exe generated/cmap_gb.h cmaps/Adobe-GB1-0 cmaps/Adobe-GB1-1 cmaps/Adobe-GB1-2 cmaps/Adobe-GB1-3 cmaps/Adobe-GB1-4 cmaps/Adobe-GB1-5 cmaps/GB-EUC-H cmaps/GB-EUC-V cmaps/GB-H cmaps/GB-V cmaps/GBK-EUC-H cmaps/GBK-EUC-V cmaps/GBK2K-H cmaps/GBK2K-V cmaps/GBKp-EUC-H cmaps/GBKp-EUC-V cmaps/GBpc-EUC-H cmaps/GBpc-EUC-V cmaps/GBT-EUC-H cmaps/GBT-EUC-V cmaps/GBT-H cmaps/GBT-V cmaps/GBTpc-EUC-H cmaps/GBTpc-EUC-V cmaps/UniGB-UCS2-H cmaps/UniGB-UCS2-V cmaps/UniGB-UTF16-H cmaps/UniGB-UTF16-V

if not exist generated/cmap_cns.h cmapdump.exe generated/cmap_cns.h cmaps/Adobe-CNS1-0 cmaps/Adobe-CNS1-1 cmaps/Adobe-CNS1-2 cmaps/Adobe-CNS1-3 cmaps/Adobe-CNS1-4 cmaps/Adobe-CNS1-5 cmaps/Adobe-CNS1-6 cmaps/B5-H cmaps/B5-V cmaps/B5pc-H cmaps/B5pc-V cmaps/CNS-EUC-H cmaps/CNS-EUC-V cmaps/CNS1-H cmaps/CNS1-V cmaps/CNS2-H cmaps/CNS2-V cmaps/ETen-B5-H cmaps/ETen-B5-V cmaps/ETenms-B5-H cmaps/ETenms-B5-V cmaps/ETHK-B5-H cmaps/ETHK-B5-V cmaps/HKdla-B5-H cmaps/HKdla-B5-V cmaps/HKdlb-B5-H cmaps/HKdlb-B5-V cmaps/HKgccs-B5-H cmaps/HKgccs-B5-V cmaps/HKm314-B5-H cmaps/HKm314-B5-V cmaps/HKm471-B5-H cmaps/HKm471-B5-V cmaps/HKscs-B5-H cmaps/HKscs-B5-V cmaps/UniCNS-UCS2-H cmaps/UniCNS-UCS2-V cmaps/UniCNS-UTF16-H cmaps/UniCNS-UTF16-V

if not exist generated/cmap_japan.h cmapdump.exe generated/cmap_japan.h cmaps/78-EUC-H cmaps/78-EUC-V cmaps/78-H cmaps/78-RKSJ-H cmaps/78-RKSJ-V cmaps/78-V cmaps/78ms-RKSJ-H cmaps/78ms-RKSJ-V cmaps/83pv-RKSJ-H cmaps/90ms-RKSJ-H cmaps/90ms-RKSJ-V cmaps/90msp-RKSJ-H cmaps/90msp-RKSJ-V cmaps/90pv-RKSJ-H cmaps/90pv-RKSJ-V cmaps/Add-H cmaps/Add-RKSJ-H cmaps/Add-RKSJ-V cmaps/Add-V cmaps/Adobe-Japan1-0 cmaps/Adobe-Japan1-1 cmaps/Adobe-Japan1-2 cmaps/Adobe-Japan1-3 cmaps/Adobe-Japan1-4 cmaps/Adobe-Japan1-5 cmaps/Adobe-Japan1-6 cmaps/EUC-H cmaps/EUC-V cmaps/Ext-H cmaps/Ext-RKSJ-H cmaps/Ext-RKSJ-V cmaps/Ext-V cmaps/H cmaps/Hankaku cmaps/Hiragana cmaps/Katakana cmaps/NWP-H cmaps/NWP-V cmaps/RKSJ-H cmaps/RKSJ-V cmaps/Roman cmaps/UniJIS-UCS2-H cmaps/UniJIS-UCS2-HW-H cmaps/UniJIS-UCS2-HW-V cmaps/UniJIS-UCS2-V cmaps/UniJISPro-UCS2-HW-V cmaps/UniJISPro-UCS2-V cmaps/V cmaps/WP-Symbol cmaps/Adobe-Japan2-0 cmaps/Hojo-EUC-H cmaps/Hojo-EUC-V cmaps/Hojo-H cmaps/Hojo-V cmaps/UniHojo-UCS2-H cmaps/UniHojo-UCS2-V cmaps/UniHojo-UTF16-H cmaps/UniHojo-UTF16-V cmaps/UniJIS-UTF16-H cmaps/UniJIS-UTF16-V

if not exist generated/cmap_korea.h cmapdump.exe generated/cmap_korea.h cmaps/Adobe-Korea1-0 cmaps/Adobe-Korea1-1 cmaps/Adobe-Korea1-2 cmaps/KSC-EUC-H cmaps/KSC-EUC-V cmaps/KSC-H cmaps/KSC-Johab-H cmaps/KSC-Johab-V cmaps/KSC-V cmaps/KSCms-UHC-H cmaps/KSCms-UHC-HW-H cmaps/KSCms-UHC-HW-V cmaps/KSCms-UHC-V cmaps/KSCpc-EUC-H cmaps/KSCpc-EUC-V cmaps/UniKS-UCS2-H cmaps/UniKS-UCS2-V cmaps/UniKS-UTF16-H cmaps/UniKS-UTF16-V

del cmapdump.obj fontdump.obj cmapdump.exe fontdump.exe

goto fin

:usage
echo ERROR: Run this script in the mupdf directory.
echo ERROR: Run this script in a Visual Studio command prompt.
pause

:fin
