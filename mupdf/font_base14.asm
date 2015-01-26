; Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
; License: GPLv3

; 64-bit linkers don't expect any name decorations
%ifdef WIN64
%define EXPORT(name) name
%else
%define EXPORT(name) _ %+ name
%endif

section .data:

global EXPORT(pdf_font_Dingbats)
EXPORT(pdf_font_Dingbats):
    incbin "resources/fonts/urw/Dingbats.cff"

global EXPORT(pdf_font_NimbusMon_Bol)
EXPORT(pdf_font_NimbusMon_Bol):
    incbin "resources/fonts/urw/NimbusMon-Bol.cff"

global EXPORT(pdf_font_NimbusMon_BolObl)
EXPORT(pdf_font_NimbusMon_BolObl):
    incbin "resources/fonts/urw/NimbusMon-BolObl.cff"

global EXPORT(pdf_font_NimbusMon_Obl)
EXPORT(pdf_font_NimbusMon_Obl):
    incbin "resources/fonts/urw/NimbusMon-Obl.cff"

global EXPORT(pdf_font_NimbusMon_Reg)
EXPORT(pdf_font_NimbusMon_Reg):
    incbin "resources/fonts/urw/NimbusMon-Reg.cff"

global EXPORT(pdf_font_NimbusRom_Ita)
EXPORT(pdf_font_NimbusRom_Ita):
    incbin "resources/fonts/urw/NimbusRom-Ita.cff"

global EXPORT(pdf_font_NimbusRom_Med)
EXPORT(pdf_font_NimbusRom_Med):
    incbin "resources/fonts/urw/NimbusRom-Med.cff"

global EXPORT(pdf_font_NimbusRom_MedIta)
EXPORT(pdf_font_NimbusRom_MedIta):
    incbin "resources/fonts/urw/NimbusRom-MedIta.cff"

global EXPORT(pdf_font_NimbusRom_Reg)
EXPORT(pdf_font_NimbusRom_Reg):
    incbin "resources/fonts/urw/NimbusRom-Reg.cff"

global EXPORT(pdf_font_NimbusSan_Bol)
EXPORT(pdf_font_NimbusSan_Bol):
    incbin "resources/fonts/urw/NimbusSan-Bol.cff"

global EXPORT(pdf_font_NimbusSan_BolIta)
EXPORT(pdf_font_NimbusSan_BolIta):
    incbin "resources/fonts/urw/NimbusSan-BolIta.cff"

global EXPORT(pdf_font_NimbusSan_Ita)
EXPORT(pdf_font_NimbusSan_Ita):
    incbin "resources/fonts/urw/NimbusSan-Ita.cff"

global EXPORT(pdf_font_NimbusSan_Reg)
EXPORT(pdf_font_NimbusSan_Reg):
    incbin "resources/fonts/urw/NimbusSan-Reg.cff"

global EXPORT(pdf_font_StandardSymL)
EXPORT(pdf_font_StandardSymL):
    incbin "resources/fonts/urw/StandardSymL.cff"
