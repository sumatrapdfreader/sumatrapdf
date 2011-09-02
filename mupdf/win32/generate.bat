@echo off

if not exist scripts/fontdump.c cd ..
if not exist generated mkdir generated

cl /nologo -Ifitz -Ipdf scripts/fontdump.c
cl /nologo -Ifitz -Ipdf scripts/cmapdump.c

if not exist fontdump.exe goto usage
if not exist cmapdump.exe goto usage

if not exist generated/font_base14.h fontdump.exe generated/font_base14.h fonts/Dingbats.cff fonts/NimbusMonL-Bold.cff fonts/NimbusMonL-BoldObli.cff fonts/NimbusMonL-Regu.cff fonts/NimbusMonL-ReguObli.cff fonts/NimbusRomNo9L-Medi.cff fonts/NimbusRomNo9L-MediItal.cff fonts/NimbusRomNo9L-Regu.cff fonts/NimbusRomNo9L-ReguItal.cff fonts/NimbusSanL-Bold.cff fonts/NimbusSanL-BoldItal.cff fonts/NimbusSanL-Regu.cff fonts/NimbusSanL-ReguItal.cff fonts/StandardSymL.cff

if not exist generated/font_droid.h fontdump.exe generated/font_droid.h fonts/droid/DroidSans.ttf fonts/droid/DroidSansMono.ttf

if not exist generated/font_cjk.h fontdump.exe generated/font_cjk.h fonts/droid/DroidSansFallback.ttf

if not exist generated/cmap_cns.h cmapdump.exe generated/cmap_cns.h cmaps/cns/Adobe-CNS1-UCS2 cmaps/cns/Adobe-CNS1-0 cmaps/cns/Adobe-CNS1-1 cmaps/cns/Adobe-CNS1-2 cmaps/cns/Adobe-CNS1-3 cmaps/cns/Adobe-CNS1-4 cmaps/cns/Adobe-CNS1-5 cmaps/cns/Adobe-CNS1-6 cmaps/cns/B5-H cmaps/cns/B5-V cmaps/cns/B5pc-H cmaps/cns/B5pc-V cmaps/cns/CNS-EUC-H cmaps/cns/CNS-EUC-V cmaps/cns/CNS1-H cmaps/cns/CNS1-V cmaps/cns/CNS2-H cmaps/cns/CNS2-V cmaps/cns/ETen-B5-H cmaps/cns/ETen-B5-V cmaps/cns/ETenms-B5-H cmaps/cns/ETenms-B5-V cmaps/cns/ETHK-B5-H cmaps/cns/ETHK-B5-V cmaps/cns/HKdla-B5-H cmaps/cns/HKdla-B5-V cmaps/cns/HKdlb-B5-H cmaps/cns/HKdlb-B5-V cmaps/cns/HKgccs-B5-H cmaps/cns/HKgccs-B5-V cmaps/cns/HKm314-B5-H cmaps/cns/HKm314-B5-V cmaps/cns/HKm471-B5-H cmaps/cns/HKm471-B5-V cmaps/cns/HKscs-B5-H cmaps/cns/HKscs-B5-V cmaps/cns/UniCNS-UCS2-H cmaps/cns/UniCNS-UCS2-V cmaps/cns/UniCNS-UTF16-H cmaps/cns/UniCNS-UTF16-V

if not exist generated/cmap_gb.h cmapdump.exe generated/cmap_gb.h cmaps/gb/Adobe-GB1-UCS2 cmaps/gb/Adobe-GB1-0 cmaps/gb/Adobe-GB1-1 cmaps/gb/Adobe-GB1-2 cmaps/gb/Adobe-GB1-3 cmaps/gb/Adobe-GB1-4 cmaps/gb/Adobe-GB1-5 cmaps/gb/GB-EUC-H cmaps/gb/GB-EUC-V cmaps/gb/GB-H cmaps/gb/GB-V cmaps/gb/GBK-EUC-H cmaps/gb/GBK-EUC-V cmaps/gb/GBK2K-H cmaps/gb/GBK2K-V cmaps/gb/GBKp-EUC-H cmaps/gb/GBKp-EUC-V cmaps/gb/GBpc-EUC-H cmaps/gb/GBpc-EUC-V cmaps/gb/GBT-EUC-H cmaps/gb/GBT-EUC-V cmaps/gb/GBT-H cmaps/gb/GBT-V cmaps/gb/GBTpc-EUC-H cmaps/gb/GBTpc-EUC-V cmaps/gb/UniGB-UCS2-H cmaps/gb/UniGB-UCS2-V cmaps/gb/UniGB-UTF16-H cmaps/gb/UniGB-UTF16-V

if not exist generated/cmap_japan.h cmapdump.exe generated/cmap_japan.h cmaps/japan/Adobe-Japan1-UCS2 cmaps/japan/78-EUC-H cmaps/japan/78-EUC-V cmaps/japan/78-H cmaps/japan/78-RKSJ-H cmaps/japan/78-RKSJ-V cmaps/japan/78-V cmaps/japan/78ms-RKSJ-H cmaps/japan/78ms-RKSJ-V cmaps/japan/83pv-RKSJ-H cmaps/japan/90ms-RKSJ-H cmaps/japan/90ms-RKSJ-V cmaps/japan/90msp-RKSJ-H cmaps/japan/90msp-RKSJ-V cmaps/japan/90pv-RKSJ-H cmaps/japan/90pv-RKSJ-V cmaps/japan/Add-H cmaps/japan/Add-RKSJ-H cmaps/japan/Add-RKSJ-V cmaps/japan/Add-V cmaps/japan/Adobe-Japan1-0 cmaps/japan/Adobe-Japan1-1 cmaps/japan/Adobe-Japan1-2 cmaps/japan/Adobe-Japan1-3 cmaps/japan/Adobe-Japan1-4 cmaps/japan/Adobe-Japan1-5 cmaps/japan/Adobe-Japan1-6 cmaps/japan/EUC-H cmaps/japan/EUC-V cmaps/japan/Ext-H cmaps/japan/Ext-RKSJ-H cmaps/japan/Ext-RKSJ-V cmaps/japan/Ext-V cmaps/japan/H cmaps/japan/Hankaku cmaps/japan/Hiragana cmaps/japan/Katakana cmaps/japan/NWP-H cmaps/japan/NWP-V cmaps/japan/RKSJ-H cmaps/japan/RKSJ-V cmaps/japan/Roman cmaps/japan/UniJIS-UCS2-H cmaps/japan/UniJIS-UCS2-HW-H cmaps/japan/UniJIS-UCS2-HW-V cmaps/japan/UniJIS-UCS2-V cmaps/japan/UniJISPro-UCS2-HW-V cmaps/japan/UniJISPro-UCS2-V cmaps/japan/V cmaps/japan/WP-Symbol cmaps/japan/Adobe-Japan2-0 cmaps/japan/Hojo-EUC-H cmaps/japan/Hojo-EUC-V cmaps/japan/Hojo-H cmaps/japan/Hojo-V cmaps/japan/UniHojo-UCS2-H cmaps/japan/UniHojo-UCS2-V cmaps/japan/UniHojo-UTF16-H cmaps/japan/UniHojo-UTF16-V cmaps/japan/UniJIS-UTF16-H cmaps/japan/UniJIS-UTF16-V

if not exist generated/cmap_korea.h cmapdump.exe generated/cmap_korea.h cmaps/korea/Adobe-Korea1-UCS2 cmaps/korea/Adobe-Korea1-0 cmaps/korea/Adobe-Korea1-1 cmaps/korea/Adobe-Korea1-2 cmaps/korea/KSC-EUC-H cmaps/korea/KSC-EUC-V cmaps/korea/KSC-H cmaps/korea/KSC-Johab-H cmaps/korea/KSC-Johab-V cmaps/korea/KSC-V cmaps/korea/KSCms-UHC-H cmaps/korea/KSCms-UHC-HW-H cmaps/korea/KSCms-UHC-HW-V cmaps/korea/KSCms-UHC-V cmaps/korea/KSCpc-EUC-H cmaps/korea/KSCpc-EUC-V cmaps/korea/UniKS-UCS2-H cmaps/korea/UniKS-UCS2-V cmaps/korea/UniKS-UTF16-H cmaps/korea/UniKS-UTF16-V

del cmapdump.obj fontdump.obj cmapdump.exe fontdump.exe

goto fin

:usage
echo ERROR: Run this script in the mupdf directory.
echo ERROR: Run this script in a Visual Studio command prompt.
pause

:fin
