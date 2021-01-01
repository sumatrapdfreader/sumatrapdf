/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef struct FileState DisplayState;

enum class DisplayMode {
    // automatic means: the continuous form of single page, facing or
    // book view - depending on the document's desired PageLayout
    Automatic = 0,
    SinglePage,
    Facing,
    BookView,
    Continuous,
    ContinuousFacing,
    ContinuousBookView,
};

#define ZOOM_FIT_PAGE -1.f
#define ZOOM_FIT_WIDTH -2.f
#define ZOOM_FIT_CONTENT -3.f
#define ZOOM_ACTUAL_SIZE 100.0f
#define ZOOM_MAX 6400.f /* max zoom in % */
#define ZOOM_MIN 8.33f  /* min zoom in % */
#define INVALID_ZOOM -99.0f

constexpr int INVALID_PAGE_NO = -1;

bool IsSingle(DisplayMode mode);
bool IsContinuous(DisplayMode mode);
bool IsFacing(DisplayMode mode);
bool IsBookView(DisplayMode mode);
bool IsValidZoom(float zoomLevel);

const char* DisplayModeToString(DisplayMode mode);
DisplayMode DisplayModeFromString(const char* s, DisplayMode defVal);
float ZoomFromString(const char* s, float defVal);
void ZoomToString(char** dst, float zoom, DisplayState* stateForIssue2140 = nullptr);
