section .data:

; TODO: alignment

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
    incbin "resources/fonts/urw/NimbusMonoPS-Bold.cff"

_binary_NimbusMonoPS_Regular_cff_size:
    dq $-_binary_NimbusMonoPS_Regular_cff

; ---------

global _binary_NimbusMonoPS_Italic_cff
global _binary_NimbusMonoPS_Italic_cff_size

_binary_NimbusMonoPS_Italic_cff:
    incbin "resources/fonts/urw/NimbusMonoPS-Italic.cff"

_binary_NimbusMonoPS_Italic_cff_size:
    dq $-_binary_NimbusMonoPS_Italic_cff
