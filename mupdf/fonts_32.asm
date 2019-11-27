section .data:

; TODO: alignment (?)

; ---------

global __binary_Dingbats_cff
global __binary_Dingbats_cff_size

__binary_Dingbats_cff:
    incbin "resources/fonts/urw/Dingbats.cff"

__binary_Dingbats_cff_size:
    dq $ - __binary_Dingbats_cff

; ---------

global __binary_NimbusMonoPS_Regular_cff
global __binary_NimbusMonoPS_Regular_cff_size

__binary_NimbusMonoPS_Regular_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-Regular.cff"

__binary_NimbusMonoPS_Regular_cff_size:
    dq $ - __binary_NimbusMonoPS_Regular_cff

; ---------

global __binary_NimbusMonoPS_Italic_cff
global __binary_NimbusMonoPS_Italic_cff_size

__binary_NimbusMonoPS_Italic_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-Italic.cff"

__binary_NimbusMonoPS_Italic_cff_size:
    dq $ - __binary_NimbusMonoPS_Italic_cff

; ---------

global __binary_NimbusMonoPS_Bold_cff
global __binary_NimbusMonoPS_Bold_cff_size

__binary_NimbusMonoPS_Bold_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-Bold.cff"

__binary_NimbusMonoPS_Bold_cff_size:
    dq $ - __binary_NimbusMonoPS_Bold_cff

; ---------

global __binary_NimbusMonoPS_BoldItalic_cff
global __binary_NimbusMonoPS_BoldItalic_cff_size

__binary_NimbusMonoPS_BoldItalic_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-BoldItalic.cff"

__binary_NimbusMonoPS_BoldItalic_cff_size:
    dq $ - __binary_NimbusMonoPS_BoldItalic_cff

; ---------

global __binary_NimbusRoman_Regular_cff
global __binary_NimbusRoman_Regular_cff_size

__binary_NimbusRoman_Regular_cff:
    incbin "resources/fonts/urw/NimbusRoman-Regular.cff"

__binary_NimbusRoman_Regular_cff_size:
    dq $ - __binary_NimbusRoman_Regular_cff

; ---------

global __binary_NimbusRoman_Italic_cff
global __binary_NimbusRoman_Italic_cff_size

__binary_NimbusRoman_Italic_cff:
    incbin "resources/fonts/urw/NimbusRoman-Italic.cff"

__binary_NimbusRoman_Italic_cff_size:
    dq $ - __binary_NimbusRoman_Italic_cff

; ---------

global __binary_NimbusRoman_Bold_cff
global __binary_NimbusRoman_Bold_cff_size

__binary_NimbusRoman_Bold_cff:
    incbin "resources/fonts/urw/NimbusRoman-Bold.cff"

__binary_NimbusRoman_Bold_cff_size:
    dq $ - __binary_NimbusRoman_Bold_cff

; ---------

global __binary_NimbusRoman_BoldItalic_cff
global __binary_NimbusRoman_BoldItalic_cff_size

__binary_NimbusRoman_BoldItalic_cff:
    incbin "resources/fonts/urw/NimbusRoman-BoldItalic.cff"

__binary_NimbusRoman_BoldItalic_cff_size:
    dq $ - __binary_NimbusRoman_BoldItalic_cff

; ---------

global __binary_NimbusSans_Regular_cff
global __binary_NimbusSans_Regular_cff_size

__binary_NimbusSans_Regular_cff:
    incbin "resources/fonts/urw/NimbusSans-Regular.cff"

__binary_NimbusSans_Regular_cff_size:
    dq $ - __binary_NimbusSans_Regular_cff

; ---------

global __binary_NimbusSans_Italic_cff
global __binary_NimbusSans_Italic_cff_size

__binary_NimbusSans_Italic_cff:
    incbin "resources/fonts/urw/NimbusSans-Italic.cff"

__binary_NimbusSans_Italic_cff_size:
    dq $ - __binary_NimbusSans_Italic_cff

; ---------

global __binary_NimbusSans_Bold_cff
global __binary_NimbusSans_Bold_cff_size

__binary_NimbusSans_Bold_cff:
    incbin "resources/fonts/urw/NimbusSans-Bold.cff"

__binary_NimbusSans_Bold_cff_size:
    dq $ - __binary_NimbusSans_Bold_cff

; ---------

global __binary_NimbusSans_BoldItalic_cff
global __binary_NimbusSans_BoldItalic_cff_size

__binary_NimbusSans_BoldItalic_cff:
    incbin "resources/fonts/urw/NimbusSans-BoldItalic.cff"

__binary_NimbusSans_BoldItalic_cff_size:
    dq $ - __binary_NimbusSans_BoldItalic_cff


; ---------

global __binary_StandardSymbolsPS_cff
global __binary_StandardSymbolsPS_cff_size

__binary_StandardSymbolsPS_cff:
    incbin "resources/fonts/urw/StandardSymbolsPS.cff"

__binary_StandardSymbolsPS_cff_size:
    dq $ - __binary_StandardSymbolsPS_cff

; ---------

; global __binary_SourceHanSerif_Regular_ttc
; global __binary_SourceHanSerif_Regular_ttc_size

; __binary_SourceHanSerif_Regular_ttc:
;     incbin "resources/fonts/han/SourceHanSerif-Regular.ttc"

; __binary_SourceHanSerif_Regular_ttc_size:
;     dq $ - __binary_SourceHanSerif_Regular_ttc

; ---------

global __binary_DroidSansFallbackFull_ttf
global __binary_DroidSansFallbackFull_ttf_size

__binary_DroidSansFallbackFull_ttf:
    incbin "resources/fonts/droid/DroidSansFallbackFull.ttf"

__binary_DroidSansFallbackFull_ttf_size:
    dq $ - __binary_DroidSansFallbackFull_ttf
