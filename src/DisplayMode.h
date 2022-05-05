/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct FileState;

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

constexpr float kZoomFitPage = -1.f;
constexpr float kZoomFitWidth = -2.f;
constexpr float kZoomFitContent = -3.f;
constexpr float kZoomActualSize = 100.0f;
constexpr float kZoomMax = 6400.f; /* max zoom in % */
constexpr float kZoomMin = 8.33f;  /* min zoom in % */
constexpr float kInvalidZoom = -99.0f;

bool IsSingle(DisplayMode mode);
bool IsContinuous(DisplayMode mode);
bool IsFacing(DisplayMode mode);
bool IsBookView(DisplayMode mode);
bool IsValidZoom(float zoomLevel);

const char* DisplayModeToString(DisplayMode mode);
DisplayMode DisplayModeFromString(const char* s, DisplayMode defVal);
float ZoomFromString(const char* s, float defVal);
void ZoomToString(char** dst, float zoom, FileState* stateForIssue2140);
