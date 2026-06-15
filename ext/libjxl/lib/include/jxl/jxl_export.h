/* Static-build replacement for CMake's generate_export_header() output. */
#ifndef JXL_EXPORT_H
#define JXL_EXPORT_H
#define JXL_EXPORT
#define JXL_NO_EXPORT
#ifndef JXL_DEPRECATED
#  define JXL_DEPRECATED __declspec(deprecated)
#endif
#define JXL_DEPRECATED_EXPORT
#define JXL_DEPRECATED_NO_EXPORT
#endif /* JXL_EXPORT_H */
