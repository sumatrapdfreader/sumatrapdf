/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef _QWORD_DEFINED
#define _QWORD_DEFINED
typedef unsigned __int64 QWORD, *LPQWORD;
#endif

#ifndef MAKEQWORD
#define MAKEQWORD(a, b)     ((QWORD)(((QWORD)((DWORD)(a))) << 32 | ((DWORD)(b))))
#define LODWORD(l)          ((DWORD)((l) & 0xFFFFFFFF))
#define HIDWORD(l)          ((DWORD)(((QWORD)(l) >> 32) & 0xFFFFFFFF))
#endif

// Define the Gesture structures here because they
// are not available in all versions of Windows
// These defines can be found in WinUser.h
#ifndef GF_BEGIN  // needs WINVER >= 0x0601

DECLARE_HANDLE(HGESTUREINFO);

/*
 * Gesture flags - GESTUREINFO.dwFlags
 */
#define GF_BEGIN                        0x00000001
#define GF_INERTIA                      0x00000002
#define GF_END                          0x00000004

/*
 * Gesture configuration structure
 *   - Used in SetGestureConfig and GetGestureConfig
 *   - Note that any setting not included in either GESTURECONFIG.dwWant or
 *     GESTURECONFIG.dwBlock will use the parent window's preferences or
 *     system defaults.
 */
typedef struct tagGESTURECONFIG {
    DWORD dwID;                     // gesture ID
    DWORD dwWant;                   // settings related to gesture ID that are to be turned on
    DWORD dwBlock;                  // settings related to gesture ID that are to be turned off
} GESTURECONFIG, *PGESTURECONFIG;

/*
 * Gesture information structure
 *   - Pass the HGESTUREINFO received in the WM_GESTURE message lParam into the
 *     GetGestureInfo function to retrieve this information.
 *   - If cbExtraArgs is non-zero, pass the HGESTUREINFO received in the WM_GESTURE
 *     message lParam into the GetGestureExtraArgs function to retrieve extended
 *     argument information.
 */
typedef struct tagGESTUREINFO {
    UINT cbSize;                    // size, in bytes, of this structure (including variable length Args field)
    DWORD dwFlags;                  // see GF_* flags
    DWORD dwID;                     // gesture ID, see GID_* defines
    HWND hwndTarget;                // handle to window targeted by this gesture
    POINTS ptsLocation;             // current location of this gesture
    DWORD dwInstanceID;             // internally used
    DWORD dwSequenceID;             // internally used
    ULONGLONG ullArguments;         // arguments for gestures whose arguments fit in 8 BYTES
    UINT cbExtraArgs;               // size, in bytes, of extra arguments, if any, that accompany this gesture
} GESTUREINFO, *PGESTUREINFO;
typedef GESTUREINFO const * PCGESTUREINFO;

/*
 * Gesture argument helpers
 *   - Angle should be a double in the range of -2pi to +2pi
 *   - Argument should be an unsigned 16-bit value
 */
#define GID_ROTATE_ANGLE_TO_ARGUMENT(_arg_)     ((USHORT)((((_arg_) + 2.0 * M_PI) / (4.0 * M_PI)) * 65535.0))
#define GID_ROTATE_ANGLE_FROM_ARGUMENT(_arg_)   ((((double)(_arg_) / 65535.0) * 4.0 * M_PI) - 2.0 * M_PI)

/*
 * Gesture configuration flags
 */
#define GC_ALLGESTURES                              0x00000001
#define GC_ZOOM                                     0x00000001
#define GC_PAN                                      0x00000001
#define GC_PAN_WITH_SINGLE_FINGER_VERTICALLY        0x00000002
#define GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY      0x00000004
#define GC_PAN_WITH_GUTTER                          0x00000008
#define GC_PAN_WITH_INERTIA                         0x00000010
#define GC_ROTATE                                   0x00000001
#define GC_TWOFINGERTAP                             0x00000001
#define GC_PRESSANDTAP                              0x00000001

/*
 * Gesture IDs
 */
#define GID_BEGIN                       1
#define GID_END                         2
#define GID_ZOOM                        3
#define GID_PAN                         4
#define GID_ROTATE                      5
#define GID_TWOFINGERTAP                6
#define GID_PRESSANDTAP                 7

// Window events
#define WM_GESTURE                         0x0119

#endif // HGESTUREINFO

// Touch Gesture API, only available in Windows 7 so those are wrappers for
// dynamically resolved dll APIs.
namespace Touch {

bool SupportsGestures();

BOOL GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
BOOL CloseGestureInfoHandle(HGESTUREINFO hGestureInfo);
BOOL SetGestureConfig(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);

}
