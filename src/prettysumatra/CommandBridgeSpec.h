#pragma once

// PrettySumatra shell/core command names used by the hybrid host.
namespace prettysumatra {
namespace bridge {

constexpr const char* kOpenFile = "openFile";
constexpr const char* kGoToPage = "goToPage";
constexpr const char* kZoom = "zoom";
constexpr const char* kSetFitMode = "setFitMode";
constexpr const char* kSearch = "search";
constexpr const char* kToggleSidebar = "toggleSidebar";
constexpr const char* kSetViewMode = "setViewMode";
constexpr const char* kExecCommand = "execCommand";
constexpr const char* kToolbarReady = "toolbarReady";
constexpr const char* kAddAnnotation = "addAnnotation";
constexpr const char* kEditAnnotation = "editAnnotation";
constexpr const char* kDeleteAnnotation = "deleteAnnotation";
constexpr const char* kExportAnnotations = "exportAnnotations";
constexpr const char* kImportAnnotations = "importAnnotations";

// HomePage UI command names
constexpr const char* kHomePageReady = "homePageReady";
constexpr const char* kOpenRecent = "openRecent";
constexpr const char* kReopenLast = "reopenLast";
constexpr const char* kApplyTheme = "applyTheme";

} // namespace bridge
} // namespace prettysumatra
