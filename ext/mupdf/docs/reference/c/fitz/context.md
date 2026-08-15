# Context

Almost all functions in the MuPDF library take a `fz_context` structure as
their first argument. The context is used for many things; primarily it holds
the exception stack for our `setjmp` based exception handling. It also holds
various caches and auxiliary contexts for font rendering and color management.

Here is the code to create a Fitz context. The first two arguments are used if
you need to use a custom memory allocator, and the third argument is a hint to
much memory the various caches should be allowed to grow. The limit is only a
soft limit. We may exceed it, but will start clearing out stale data to try to
stay below the limit when possible. Setting it to a lower value will prevent
the caches from growing out of hand if you are tight on memory.

	#include <mupdf/fitz.h>

	#include <stdio.h>
	#include <stdlib.h>

	main()
	{
		fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
		if (!ctx) {
			fprintf(stderr, "Failed to create a new Fitz context!\n");
			return EXIT_FAILURE;
		}

		... do stuff ...

		fz_drop_context(ctx);
		return EXIT_SUCCESS;
	}
