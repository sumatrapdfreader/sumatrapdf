/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define ZOOM_FIT_PAGE       -1.f
#define ZOOM_FIT_WIDTH      -2.f
#define ZOOM_FIT_CONTENT    -3.f
#define ZOOM_ACTUAL_SIZE    100.0f
#define ZOOM_MAX            6400.f /* max zoom in % */
#define ZOOM_MIN            8.33f  /* min zoom in % */
#define INVALID_ZOOM        -99.0f

extern GlobalPrefs *gGlobalPrefs;

DisplayState *NewDisplayState(const WCHAR *filePath);
void DeleteDisplayState(DisplayState *ds);

Favorite *NewFavorite(int pageNo, const WCHAR *name, const WCHAR *pageLabel);
void DeleteFavorite(Favorite *fav);

GlobalPrefs *NewGlobalPrefs(const char *data);
char *SerializeGlobalPrefs(GlobalPrefs *gp, const char *prevData, size_t *sizeOut);
void DeleteGlobalPrefs(GlobalPrefs *gp);

namespace prefs {
namespace conv {

const WCHAR *   FromDisplayMode(DisplayMode mode);
DisplayMode     ToDisplayMode(const WCHAR *s, DisplayMode defVal);
void            FromZoom(char **dst, float zoom, DisplayState *stateForIssue2140=NULL);
float           ToZoom(const char *s, float defVal);

};
};

// convenience helpers
inline bool IsSingle(DisplayMode mode) {
    return DM_SINGLE_PAGE == mode || DM_CONTINUOUS == mode;
}
inline bool IsContinuous(DisplayMode mode) {
    return DM_CONTINUOUS == mode || DM_CONTINUOUS_FACING == mode || DM_CONTINUOUS_BOOK_VIEW == mode;
}
inline bool IsFacing(DisplayMode mode) {
    return DM_FACING == mode || DM_CONTINUOUS_FACING == mode;
}
inline bool IsBookView(DisplayMode mode) {
    return DM_BOOK_VIEW == mode || DM_CONTINUOUS_BOOK_VIEW == mode;
}

inline bool IsValidZoom(float zoomLevel) {
    return (ZOOM_MIN - 0.01f <= zoomLevel && zoomLevel <= ZOOM_MAX + 0.01f) ||
           ZOOM_FIT_PAGE == zoomLevel || ZOOM_FIT_WIDTH == zoomLevel || ZOOM_FIT_CONTENT == zoomLevel;
}
