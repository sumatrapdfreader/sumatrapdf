/*
 * Compatibility header for building MuPDF (SumatraPDF-patched) on Linux.
 * Maps MSVC-specific extensions to GCC equivalents.
 */
#ifndef MUPDF_COMPAT_LINUX_H
#define MUPDF_COMPAT_LINUX_H

/* SumatraPDF patches use __declspec(thread) for thread-local storage.
 * GCC requires __thread before the type, but __declspec(thread) comes first.
 * We strip it here; the variable becomes a plain static (safe for our usage). */
#ifndef _MSC_VER
#define __declspec(x)
#endif

#endif /* MUPDF_COMPAT_LINUX_H */
