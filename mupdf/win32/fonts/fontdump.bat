@echo off

echo Converting BASE14 fonts
%1 %2\font_base14.c ..\..\fonts\Dingbats.cff ..\..\fonts\NimbusMonL-Bold.cff ..\..\fonts\NimbusMonL-BoldObli.cff ..\..\fonts\NimbusMonL-Regu.cff ..\..\fonts\NimbusMonL-ReguObli.cff ..\..\fonts\NimbusRomNo9L-Medi.cff ..\..\fonts\NimbusRomNo9L-MediItal.cff ..\..\fonts\NimbusRomNo9L-Regu.cff ..\..\fonts\NimbusRomNo9L-ReguItal.cff ..\..\fonts\NimbusSanL-Bold.cff ..\..\fonts\NimbusSanL-BoldItal.cff ..\..\fonts\NimbusSanL-Regu.cff ..\..\fonts\NimbusSanL-ReguItal.cff ..\..\fonts\StandardSymL.cff ..\..\fonts\URWChanceryL-MediItal.cff

echo Converting CJK fonts
%1 %2\font_cjk.c ..\..\fonts\droid\DroidSansFallback.ttf
