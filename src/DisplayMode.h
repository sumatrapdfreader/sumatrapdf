/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef struct FileState DisplayState;

enum DisplayMode {
    // automatic means: the continuous form of single page, facing or
    // book view - depending on the document's desired PageLayout
    DM_AUTOMATIC = 0,
    DM_SINGLE_PAGE,
    DM_FACING,
    DM_BOOK_VIEW,
    DM_CONTINUOUS,
    DM_CONTINUOUS_FACING,
    DM_CONTINUOUS_BOOK_VIEW,
};

#define ZOOM_FIT_PAGE -1.f
#define ZOOM_FIT_WIDTH -2.f
#define ZOOM_FIT_CONTENT -3.f
#define ZOOM_ACTUAL_SIZE 100.0f
#define ZOOM_MAX 6400.f /* max zoom in % */
#define ZOOM_MIN 8.33f  /* min zoom in % */
#define INVALID_ZOOM -99.0f

constexpr int INVALID_PAGE_NO = -1;

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
    return (ZOOM_MIN - 0.01f <= zoomLevel && zoomLevel <= ZOOM_MAX + 0.01f) || ZOOM_FIT_PAGE == zoomLevel ||
           ZOOM_FIT_WIDTH == zoomLevel || ZOOM_FIT_CONTENT == zoomLevel;
}

namespace prefs {
namespace conv {

const char* FromDisplayMode(DisplayMode mode);
DisplayMode ToDisplayMode(const char* s, DisplayMode defVal);
void FromZoom(char** dst, float zoom, DisplayState* stateForIssue2140 = nullptr);
float ToZoom(const char* s, float defVal);

}; // namespace conv
}; // namespace prefs
