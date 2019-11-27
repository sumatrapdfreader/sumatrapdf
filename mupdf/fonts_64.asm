section .data:

; TODO: alignment (?)

; ---------

global _binary_Dingbats_cff
global _binary_Dingbats_cff_size

_binary_Dingbats_cff:
    incbin "resources/fonts/urw/Dingbats.cff"

_binary_Dingbats_cff_size:
    dq $-_binary_Dingbats_cff

; ---------

global _binary_NimbusMonoPS_Regular_cff
global _binary_NimbusMonoPS_Regular_cff_size

_binary_NimbusMonoPS_Regular_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-Regular.cff"

_binary_NimbusMonoPS_Regular_cff_size:
    dq $-_binary_NimbusMonoPS_Regular_cff

; ---------

global _binary_NimbusMonoPS_Italic_cff
global _binary_NimbusMonoPS_Italic_cff_size

_binary_NimbusMonoPS_Italic_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-Italic.cff"

_binary_NimbusMonoPS_Italic_cff_size:
    dq $-_binary_NimbusMonoPS_Italic_cff

; ---------

global _binary_NimbusMonoPS_Bold_cff
global _binary_NimbusMonoPS_Bold_cff_size

_binary_NimbusMonoPS_Bold_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-Bold.cff"

_binary_NimbusMonoPS_Bold_cff_size:
    dq $-_binary_NimbusMonoPS_Bold_cff

; ---------

global _binary_NimbusMonoPS_BoldItalic_cff
global _binary_NimbusMonoPS_BoldItalic_cff_size

_binary_NimbusMonoPS_BoldItalic_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-BoldItalic.cff"

_binary_NimbusMonoPS_BoldItalic_cff_size:
    dq $-_binary_NimbusMonoPS_BoldItalic_cff

; ---------

global _binary_NimbusRoman_Regular_cff
global _binary_NimbusRoman_Regular_cff_size

_binary_NimbusRoman_Regular_cff:
    incbin "resources/fonts/urw/NimbusRoman-Regular.cff"

_binary_NimbusRoman_Regular_cff_size:
    dq $-_binary_NimbusRoman_Regular_cff

; ---------

global _binary_NimbusRoman_Italic_cff
global _binary_NimbusRoman_Italic_cff_size

_binary_NimbusRoman_Italic_cff:
    incbin "resources/fonts/urw/NimbusRoman-Italic.cff"

_binary_NimbusRoman_Italic_cff_size:
    dq $-_binary_NimbusRoman_Italic_cff

; ---------

global _binary_NimbusRoman_Bold_cff
global _binary_NimbusRoman_Bold_cff_size

_binary_NimbusRoman_Bold_cff:
    incbin "resources/fonts/urw/NimbusRoman-Bold.cff"

_binary_NimbusRoman_Bold_cff_size:
    dq $-_binary_NimbusRoman_Bold_cff

; ---------

global _binary_NimbusRoman_BoldItalic_cff
global _binary_NimbusRoman_BoldItalic_cff_size

_binary_NimbusRoman_BoldItalic_cff:
    incbin "resources/fonts/urw/NimbusRoman-BoldItalic.cff"

_binary_NimbusRoman_BoldItalic_cff_size:
    dq $-_binary_NimbusRoman_BoldItalic_cff

; ---------

global _binary_NimbusSans_Regular_cff
global _binary_NimbusSans_Regular_cff_size

_binary_NimbusSans_Regular_cff:
    incbin "resources/fonts/urw/NimbusSans-Regular.cff"

_binary_NimbusSans_Regular_cff_size:
    dq $-_binary_NimbusSans_Regular_cff

; ---------

global _binary_NimbusSans_Italic_cff
global _binary_NimbusSans_Italic_cff_size

_binary_NimbusSans_Italic_cff:
    incbin "resources/fonts/urw/NimbusSans-Italic.cff"

_binary_NimbusSans_Italic_cff_size:
    dq $-_binary_NimbusSans_Italic_cff

; ---------

global _binary_NimbusSans_Bold_cff
global _binary_NimbusSans_Bold_cff_size

_binary_NimbusSans_Bold_cff:
    incbin "resources/fonts/urw/NimbusSans-Bold.cff"

_binary_NimbusSans_Bold_cff_size:
    dq $-_binary_NimbusSans_Bold_cff

; ---------

global _binary_NimbusSans_BoldItalic_cff
global _binary_NimbusSans_BoldItalic_cff_size

_binary_NimbusSans_BoldItalic_cff:
    incbin "resources/fonts/urw/NimbusSans-BoldItalic.cff"

_binary_NimbusSans_BoldItalic_cff_size:
    dq $-_binary_NimbusSans_BoldItalic_cff


; ---------

global _binary_StandardSymbolsPS_cff
global _binary_StandardSymbolsPS_cff_size

_binary_StandardSymbolsPS_cff:
    incbin "resources/fonts/urw/StandardSymbolsPS.cff"

_binary_StandardSymbolsPS_cff_size:
    dq $-_binary_StandardSymbolsPS_cff

; ---------

; global _binary_SourceHanSerif_Regular_ttc
; global _binary_SourceHanSerif_Regular_ttc_size

; _binary_SourceHanSerif_Regular_ttc:
;     incbin "resources/fonts/han/SourceHanSerif-Regular.ttc"

; _binary_SourceHanSerif_Regular_ttc_size:
;     dq $ - _binary_SourceHanSerif_Regular_ttc

; ----------

global _binary_DroidSansFallbackFull_ttf
global _binary_DroidSansFallbackFull_ttf_size

_binary_DroidSansFallbackFull_ttf:
    incbin "resources/fonts/droid/DroidSansFallbackFull.ttf"

_binary_DroidSansFallbackFull_ttf_size:
    dq $ - _binary_DroidSansFallbackFull_ttf

