//{{NO_DEPENDENCIES}}
// Used by SumatraPDF.rc
//
// ng: orig's resource.h minus what this port does not load from the exe: the
// print dialog template (ng/SumatraDialogs.cpp builds it in memory; the ids stay)
// and the reloading cue (gpui draws it).
#define IDI_SUMATRAPDF 1
#define IDR_EMBEDDED_PAK 1
#define IDC_CURSORDRAG 132

#define IDC_SECTION_PRINT_RANGE 1050
#define IDC_PRINT_RANGE_ALL 1051
#define IDC_PRINT_RANGE_EVEN 1052
#define IDC_PRINT_RANGE_ODD 1053
#define IDC_SECTION_PRINT_SCALE 1060
#define IDC_PRINT_SCALE_SHRINK 1061
#define IDC_PRINT_SCALE_FIT 1062
#define IDC_PRINT_SCALE_NONE 1063
#define IDC_PRINT_SCALE_STRETCH 1064
#define IDC_PRINT_CENTER_HORIZONTALLY 1065
#define IDC_PRINT_PAPER_SOURCE_BY_SIZE 1066
#define IDC_PRINT_PER_PAGE_PAPER_SIZE 1067
#define IDC_PRINT_ROTATE_LABEL 1068
#define IDC_PRINT_ROTATE 1069
#define IDC_PRINT_AUTO_ROTATE 1071


// Document-type icons (resource id = registry DefaultIcon ,-N).
#define IDI_DOCTYPE_EPUB 3
#define IDI_DOCTYPE_CBX 4
#define IDI_DOCTYPE_CHM 5
#define IDI_DOCTYPE_DJVU 6
#define IDI_DOCTYPE_IMG 7
#define IDI_DOCTYPE_PDF 8
#define IDI_DOCTYPE_MOBI 9
