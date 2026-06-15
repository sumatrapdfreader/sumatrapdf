/* Static-build replacement for CMake's generate_export_header() output. */
#ifndef JXL_CMS_EXPORT_H
#define JXL_CMS_EXPORT_H
#define JXL_CMS_EXPORT
#define JXL_CMS_NO_EXPORT
#ifndef JXL_CMS_DEPRECATED
#  define JXL_CMS_DEPRECATED __declspec(deprecated)
#endif
#define JXL_CMS_DEPRECATED_EXPORT
#define JXL_CMS_DEPRECATED_NO_EXPORT
#endif /* JXL_CMS_EXPORT_H */
