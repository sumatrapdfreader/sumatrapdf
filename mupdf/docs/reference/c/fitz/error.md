# Errors

MuPDF uses a `setjmp` based exception handling system. This is encapsulated by
the use of three macros: `fz_try`, `fz_always`, and `fz_catch`. When an error
is raised by `fz_throw`, or re-raised by `fz_rethrow`, execution will jump to
the enclosing always/catch block.

All functions you call should be guarded by a `fz_try` block to catch the
errors, or the program will call `exit()` on errors. You don't want that.

The `fz_always` block is optional. It is typically used to free memory or
release resources unconditionally, in both the case when the execution of the
try block succeeds, and when an error occurs.

	fz_try(ctx) {
		// Do stuff that may throw an exception.
	}
	fz_always(ctx) {
		// This (optional) block is always executed.
	}
	fz_catch(ctx) {
		// This block is only executed when recovering from an exception.
	}


Since the `fz_try` macro is based on `setjmp`, the same conditions that apply
to local variables in the presence of `setjmp` apply. Any locals written to
inside the try block may be restored to their pre-try state when an error
occurs. We provide a `fz_var()` macro to guard against this.

In the following example, if we don't guard `buf` with `fz_var`, then when an
error occurs the `buf` local variable might have be reset to its pre-try value
(`NULL`) and we would leak the memory.

	char *buf = NULL;

	fz_var(buf);

	fz_try(ctx) {
		buf = fz_malloc(ctx, 100);
		// Do stuff with buf that may throw an exception.
	}
	fz_always(ctx) {
		fz_free(ctx, buf);
	}
	fz_catch(ctx) {
		fz_rethrow(ctx);
	}

Carefully note that you should **never** return from within a `fz_try` or
`fz_always block`! Doing so will unbalance the exception stack, and things will
go catastrophically wrong. Instead, it is possible to break out of the `fz_try`
and `fz_always` block by using a break statement if you want to exit the block
early without throwing an exception.

Throwing a new exception can be done with `fz_throw`. Passing an exception
along after having cleaned up in the `fz_catch` block can be done with
`fz_rethrow`. `fz_throw` takes a `printf`-like formatting string.

    enum {
        FZ_ERROR_SYSTEM, // fatal out of memory or syscall error
        FZ_ERROR_LIBRARY, // unclassified error from third-party library
        FZ_ERROR_ARGUMENT, // invalid or out-of-range arguments to functions
        FZ_ERROR_LIMIT, // failed because of resource or other hard limits
        FZ_ERROR_UNSUPPORTED, // tried to use an unsupported feature
        FZ_ERROR_FORMAT, // syntax or format errors that are unrecoverable
        FZ_ERROR_SYNTAX, // syntax errors that should be diagnosed and ignored
    };

    void fz_throw(fz_context *ctx, int error_code, const char *fmt, ...);
    void fz_rethrow(fz_context *ctx);
