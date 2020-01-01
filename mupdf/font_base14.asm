; Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
; License: GPLv3

; 64-bit linkers don't expect any name decorations
%ifdef WIN64
%define EXPORT(name) name
%else
%define EXPORT(name) _ %+ name
%endif

section .data:

global EXPORT(binary_resources_fonts_)
EXPORT(pdf_font_Dingbats):
    incbin "resources/fonts/urw/Dingbats.cff"

global EXPORT(pdf_font_NimbusMono_Bold)
EXPORT(pdf_font_NimbusMono_Bold):
    incbin "resources/fonts/urw/NimbusMono-Bold.cff"

global EXPORT(pdf_font_NimbusMono_BoldOblique)
EXPORT(pdf_font_NimbusMono_BoldOblique):
    incbin "resources/fonts/urw/NimbusMono-BoldOblique.cff"

global EXPORT(pdf_font_NimbusMono_Oblique)
EXPORT(pdf_font_NimbusMono_Oblique):
    incbin "resources/fonts/urw/NimbusMono-Oblique.cff"

global EXPORT(pdf_font_NimbusMono_Regular)
EXPORT(pdf_font_NimbusMono_Regular):
    incbin "resources/fonts/urw/NimbusMono-Regular.cff"

global EXPORT(pdf_font_NimbusRomNo9L_Med)
EXPORT(pdf_font_NimbusRomNo9L_Med):
    incbin "resources/fonts/urw/NimbusRomNo9L-Med.cff"

global EXPORT(pdf_font_NimbusRomNo9L_MedIta)
EXPORT(pdf_font_NimbusRomNo9L_MedIta):
    incbin "resources/fonts/urw/NimbusRomNo9L-MedIta.cff"

global EXPORT(pdf_font_NimbusRomNo9L_Reg)
EXPORT(pdf_font_NimbusRomNo9L_Reg):
    incbin "resources/fonts/urw/NimbusRomNo9L-Reg.cff"

global EXPORT(pdf_font_NimbusRomNo9L_RegIta)
EXPORT(pdf_font_NimbusRomNo9L_RegIta):
    incbin "resources/fonts/urw/NimbusRomNo9L-RegIta.cff"

global EXPORT(pdf_font_NimbusSanL_Bol)
EXPORT(pdf_font_NimbusSanL_Bol):
    incbin "resources/fonts/urw/NimbusSanL-Bol.cff"

global EXPORT(pdf_font_NimbusSanL_BolIta)
EXPORT(pdf_font_NimbusSanL_BolIta):
    incbin "resources/fonts/urw/NimbusSanL-BolIta.cff"

global EXPORT(pdf_font_NimbusSanL_Reg)
EXPORT(pdf_font_NimbusSanL_Reg):
    incbin "resources/fonts/urw/NimbusSanL-Reg.cff"

global EXPORT(pdf_font_NimbusSanL_RegIta)
EXPORT(pdf_font_NimbusSanL_RegIta):
    incbin "resources/fonts/urw/NimbusSanL-RegIta.cff"

global EXPORT(pdf_font_StandardSymL)
EXPORT(pdf_font_StandardSymL):
    incbin "resources/fonts/urw/StandardSymL.cff"


