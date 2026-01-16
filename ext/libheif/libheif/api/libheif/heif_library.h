/*
 * HEIF codec.
 * Copyright (c) 2017-2023 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBHEIF_HEIF_LIBRARY_H
#define LIBHEIF_HEIF_LIBRARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>


// API versions table
//
// release    dec.options   enc.options   heif_reader   heif_writer   depth.rep   col.profile
// ------------------------------------------------------------------------------------------
//  1.0            1           N/A           N/A           N/A           1           N/A
//  1.1            1           N/A           N/A            1            1           N/A
//  1.3            1            1             1             1            1           N/A
//  1.4            1            1             1             1            1            1
//  1.7            2            1             1             1            1            1
//  1.9.2          2            2             1             1            1            1
//  1.10           2            3             1             1            1            1
//  1.11           2            4             1             1            1            1
//  1.13           3            4             1             1            1            1
//  1.14           3            5             1             1            1            1
//  1.15           4            5             1             1            1            1
//  1.16           5            6             1             1            1            1
//  1.18           5            7             1             1            1            1
//  1.19           6            7             2             1            1            1
//  1.20           7            7             2             1            1            1

#if (defined(_WIN32) || defined __CYGWIN__) && !defined(LIBHEIF_STATIC_BUILD)
#ifdef LIBHEIF_EXPORTS
#define LIBHEIF_API __declspec(dllexport)
#else
#define LIBHEIF_API __declspec(dllimport)
#endif
#elif defined(HAVE_VISIBILITY) && HAVE_VISIBILITY
#ifdef LIBHEIF_EXPORTS
#define LIBHEIF_API __attribute__((__visibility__("default")))
#else
#define LIBHEIF_API
#endif
#else
#define LIBHEIF_API
#endif

/**
 * Build a 32 bit integer from a 4-character code.
 */
#define heif_fourcc(a, b, c, d) ((uint32_t)((a<<24) | (b<<16) | (c<<8) | d))

#include <libheif/heif_version.h>
#include <libheif/heif_error.h>


/* === version numbers === */

// Version string of linked libheif library.
LIBHEIF_API const char* heif_get_version(void);

// Numeric version of linked libheif library, encoded as 0xHHMMLL00 = hh.mm.ll, where hh, mm, ll is the decimal representation of HH, MM, LL.
// For example: 0x02150300 is version 2.21.3
LIBHEIF_API uint32_t heif_get_version_number(void);

// Numeric part "HH" from above. Returned as a decimal number.
LIBHEIF_API int heif_get_version_number_major(void);
// Numeric part "MM" from above. Returned as a decimal number.
LIBHEIF_API int heif_get_version_number_minor(void);
// Numeric part "LL" from above. Returned as a decimal number.
LIBHEIF_API int heif_get_version_number_maintenance(void);

// Helper macros to check for given versions of libheif at compile time.
#define LIBHEIF_MAKE_VERSION(h, m, l) ((h) << 24 | (m) << 16 | (l) << 8)
#define LIBHEIF_HAVE_VERSION(h, m, l) (LIBHEIF_NUMERIC_VERSION >= LIBHEIF_MAKE_VERSION(h, m, l))

typedef struct heif_context heif_context;
typedef struct heif_image_handle heif_image_handle;

typedef uint32_t heif_item_id;
typedef uint32_t heif_property_id;

/**
 * Free a string returned by libheif in various API functions.
 * You may pass NULL.
 */
LIBHEIF_API
void heif_string_release(const char*);


// ========================= library initialization ======================

typedef struct heif_init_params
{
  int version;

  // currently no parameters
} heif_init_params;


/**
 * Initialise library.
 *
 * You should call heif_init() when you start using libheif and heif_deinit() when you are finished.
 * These calls are reference counted. Each call to heif_init() should be matched by one call to heif_deinit().
 *
 * For backwards compatibility, it is not really necessary to call heif_init(), but some library memory objects
 * will never be freed if you do not call heif_init()/heif_deinit().
 *
 * heif_init() will load the external modules installed in the default plugin path. Thus, you need it when you
 * want to load external plugins from the default path.
 * Codec plugins that are compiled into the library directly (selected by the compile-time parameters of libheif)
 * will be available even without heif_init().
 *
 * Make sure that you do not have one part of your program use heif_init()/heif_deinit() and another part that does
 * not use it as the latter may try to use an uninitialized library. If in doubt, enclose everything with init/deinit.
 *
 * You may pass nullptr to get default parameters. Currently, no parameters are supported.
 */
LIBHEIF_API
heif_error heif_init(heif_init_params*);

/**
 * Deinitialise and clean up library.
 *
 * You should call heif_init() when you start using libheif and heif_deinit() when you are finished.
 * These calls are reference counted. Each call to heif_init() should be matched by one call to heif_deinit().
 *
 * Note: heif_deinit() must not be called after exit(), for example in a global C++ object's destructor.
 * If you do, global variables in libheif might have already been released when heif_deinit() is running,
 * leading to a crash.
 *
 * \sa heif_init()
 */
LIBHEIF_API
void heif_deinit(void);


// --- Codec plugins ---

// --- Plugins are currently only supported on Unix platforms.

enum heif_plugin_type
{
  heif_plugin_type_encoder,
  heif_plugin_type_decoder
};

typedef struct heif_plugin_info
{
  int version; // version of this info struct
  enum heif_plugin_type type;
  const void* plugin;
  void* internal_handle; // for internal use only
} heif_plugin_info;

LIBHEIF_API
heif_error heif_load_plugin(const char* filename, heif_plugin_info const** out_plugin);

LIBHEIF_API
heif_error heif_load_plugins(const char* directory,
                             const heif_plugin_info** out_plugins,
                             int* out_nPluginsLoaded,
                             int output_array_size);

LIBHEIF_API
heif_error heif_unload_plugin(const heif_plugin_info* plugin);

// Get a NULL terminated array of the plugin directories that are searched by libheif.
// This includes the paths specified in the environment variable LIBHEIF_PLUGIN_PATHS and the built-in path
// (if not overridden by the environment variable).
LIBHEIF_API
const char* const* heif_get_plugin_directories(void);

LIBHEIF_API
void heif_free_plugin_directories(const char* const*);


// --- register plugins

typedef struct heif_decoder_plugin heif_decoder_plugin;
typedef struct heif_encoder_plugin heif_encoder_plugin;

LIBHEIF_API
heif_error heif_register_decoder_plugin(const heif_decoder_plugin*);

LIBHEIF_API
heif_error heif_register_encoder_plugin(const heif_encoder_plugin*);


// DEPRECATED. Use heif_register_decoder_plugin(const struct heif_decoder_plugin*) instead.
LIBHEIF_API
heif_error heif_register_decoder(heif_context* heif, const heif_decoder_plugin*);

#ifdef __cplusplus
}
#endif

#endif
