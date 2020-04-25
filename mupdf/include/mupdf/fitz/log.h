#ifndef MUPDF_FITZ_LOG_H
#define MUPDF_FITZ_LOG_H

#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

/**
	The functions in this file offer simple logging abilities.

	The default logfile is "fitz_log.txt". This can overridden by
	defining an environment variable "FZ_LOG_FILE", or module
	specific environment variables "FZ_LOG_FILE_<module>" (e.g.
	"FZ_LOG_FILE_STORE").

	Enable the following define(s) to enable built in debug logging
	from within the appropriate module(s).
*/

/* #define ENABLE_STORE_LOGGING */


/**
	Output a line to the log.
*/
void fz_log(fz_context *ctx, const char *fmt, ...);

/**
	Output a line to the log for a given module.
*/
void fz_log_module(fz_context *ctx, const char *module, const char *fmt, ...);

/**
	Internal function to actually do the opening of the logfile.

	Caller should close/drop the output when finished with it.
*/
fz_output *fz_new_log_for_module(fz_context *ctx, const char *module);

#endif
