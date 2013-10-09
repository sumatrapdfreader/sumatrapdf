local ffi = require'ffi'
ffi.cdef[[
typedef struct tagLOGFONTW
{
    LONG      height;
    LONG      width;
    LONG      escapement;
    LONG      orientation;
    LONG      lfWeight;
    bool      italic;
    bool      underline;
    bool      strikeout;
    BYTE      lfCharSet;
    BYTE      lfOutPrecision;
    BYTE      lfClipPrecision;
    BYTE      lfQuality;
    BYTE      lfPitchAndFamily;
    WCHAR     lfFaceName[32];
} LOGFONTW, *PLOGFONTW,  *NPLOGFONTW,  *LPLOGFONTW;
]]
