/*
 * mu-office-test - Simple test for Muoffice.
 */

#include "mupdf/memento.h"
#include "mupdf/helpers/mu-office-lib.h"
#include <windows.h>
#include <stdio.h>
#include <assert.h>

static HANDLE loaded;

/* Forward definition */
static void save_png(const MuOfficeBitmap *bitmap, const char *filename);

static void
load_progress(void *cookie, int pages_loaded, int complete)
{
	assert((intptr_t)cookie == 1234);

	fprintf(stderr, "load_progress: pages_loaded=%d complete=%d\n", pages_loaded, complete);

	if (complete)
		(void)ReleaseSemaphore(loaded, 1, NULL);
}

static void
load_error(void *cookie, MuOfficeDocErrorType error)
{
	assert((intptr_t)cookie == 1234);

	fprintf(stderr, "load_error: error=%d\n", error);
}

static void render_progress(void *cookie, MuError error)
{
	assert((intptr_t)cookie == 5678);

	fprintf(stderr, "render_progress: error=%d\n", error);
	(void)ReleaseSemaphore(loaded, 1, NULL);
}

static int
test_async(MuOfficeLib *mu)
{
	MuOfficeDoc *doc;
	MuError err;
	int count;
	MuOfficePage *page;
	float w, h;
	MuOfficeBitmap bitmap;
	MuOfficeRenderArea area;
	MuOfficeRender *render;

	err = MuOfficeLib_loadDocument(mu,
					"../MyTests/pdf_reference17.pdf",
					load_progress,
					load_error,
					(void *)1234, /* Arbitrary */
					&doc);

	/* Wait for the load to complete */
	WaitForSingleObject(loaded, INFINITE);

	/* Test number of pages */
	err = MuOfficeDoc_getNumPages(doc, &count);
	if (err)
	{
		fprintf(stderr, "Failed to count pages: error=%d\n", err);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "%d Pages in document\n", count);

	/* Get a page */
	err = MuOfficeDoc_getPage(doc, 0, NULL, (void *)4321, &page);
	if (err)
	{
		fprintf(stderr, "Failed to get page: error=%d\n", err);
		return EXIT_FAILURE;
	}

	/* Get size of page */
	err = MuOfficePage_getSize(page, &w, &h);
	if (err)
	{
		fprintf(stderr, "Failed to get page size: error=%d\n", err);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "Page size = %g x %g\n", w, h);

	/* Allocate ourselves a bitmap */
	bitmap.width = (int)(w * 1.5f + 0.5f);
	bitmap.height = (int)(h * 1.5f + 0.5f);
	bitmap.lineSkip = bitmap.width * 4;
	bitmap.memptr = malloc(bitmap.lineSkip * bitmap.height);

	/* Set the area to render the whole bitmap */
	area.origin.x = 0;
	area.origin.y = 0;
	area.renderArea.x = 0;
	area.renderArea.y = 0;
	area.renderArea.width = bitmap.width;
	area.renderArea.height = bitmap.height;

	/* Render into the bitmap */
	err = MuOfficePage_render(page, 1.5f, &bitmap, &area, render_progress, (void *)5678, &render);
	if (err)
	{
		fprintf(stderr, "Page render failed: error=%d\n", err);
		return EXIT_FAILURE;
	}

	/* Wait for the render to complete */
	WaitForSingleObject(loaded, INFINITE);

	/* Kill the render */
	MuOfficeRender_destroy(render);

	/* Output the bitmap */
	save_png(&bitmap, "out.png");
	free(bitmap.memptr);

	MuOfficePage_destroy(page);

	MuOfficeDoc_destroy(doc);

	CloseHandle(loaded);
	loaded = NULL;

	return EXIT_SUCCESS;
}

static int
test_sync(MuOfficeLib *mu)
{
	MuOfficeDoc *doc;
	MuError err;
	int count;
	MuOfficePage *page;
	float w, h;
	MuOfficeBitmap bitmap;
	MuOfficeRenderArea area;
	MuOfficeRender *render;

	loaded = CreateSemaphore(NULL, 0, 1, NULL);

	err = MuOfficeLib_loadDocument(mu,
					"../MyTests/pdf_reference17.pdf",
					NULL,
					NULL,
					NULL,
					&doc);

	/* Test number of pages */
	err = MuOfficeDoc_getNumPages(doc, &count);
	if (err)
	{
		fprintf(stderr, "Failed to count pages: error=%d\n", err);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "%d Pages in document\n", count);

	/* Get a page */
	err = MuOfficeDoc_getPage(doc, 1, NULL, (void *)4321, &page);
	if (err)
	{
		fprintf(stderr, "Failed to get page: error=%d\n", err);
		return EXIT_FAILURE;
	}

	/* Get size of page */
	err = MuOfficePage_getSize(page, &w, &h);
	if (err)
	{
		fprintf(stderr, "Failed to get page size: error=%d\n", err);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "Page size = %g x %g\n", w, h);

	/* Allocate ourselves a bitmap */
	bitmap.width = (int)(w * 1.5f + 0.5f);
	bitmap.height = (int)(h * 1.5f + 0.5f);
	bitmap.lineSkip = bitmap.width * 4;
	bitmap.memptr = malloc(bitmap.lineSkip * bitmap.height);

	/* Set the area to render the whole bitmap */
	area.origin.x = 0;
	area.origin.y = 0;
	area.renderArea.x = 0;
	area.renderArea.y = 0;
	area.renderArea.width = bitmap.width;
	area.renderArea.height = bitmap.height;

	/* Render into the bitmap */
	err = MuOfficePage_render(page, 1.5f, &bitmap, &area, NULL, NULL, &render);
	if (err)
	{
		fprintf(stderr, "Page render failed: error=%d\n", err);
		return EXIT_FAILURE;
	}

	err = MuOfficeRender_waitUntilComplete(render);
	if (err)
	{
		fprintf(stderr, "Page render failed to complete: error=%d\n", err);
		return EXIT_FAILURE;
	}

	/* Kill the render */
	MuOfficeRender_destroy(render);

	/* Output the bitmap */
	save_png(&bitmap, "out1.png");
	free(bitmap.memptr);

	MuOfficePage_destroy(page);

	MuOfficeDoc_destroy(doc);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	MuOfficeLib *mu;
	MuError err;
	int ret;

	err = MuOfficeLib_create(&mu);
	if (err)
	{
		fprintf(stderr, "Failed to create lib instance: error=%d\n", err);
		return EXIT_FAILURE;
	}

	ret = test_async(mu);
	if (ret)
		return ret;

	ret = test_sync(mu);
	if (ret)
		return ret;

	MuOfficeLib_destroy(mu);

	return EXIT_SUCCESS;
}

/*
	Code beneath here calls MuPDF directly, purely because
	this is the easiest way to get PNG saving functionality.
	This is not part of the test, which is why this is put
	at the end.
*/

#include "mupdf/fitz.h"

static void
save_png(const MuOfficeBitmap *bitmap, const char *filename)
{
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	fz_pixmap *pix;

	if (ctx == NULL)
	{
		fprintf(stderr, "save_png failed!\n");
		exit(EXIT_FAILURE);
	}

	pix = fz_new_pixmap_with_data(ctx, fz_device_rgb(ctx), bitmap->width, bitmap->height, NULL, 1, bitmap->lineSkip, bitmap->memptr);

	fz_try(ctx)
	{
		fz_save_pixmap_as_png(ctx, pix, filename);
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
	{
		fprintf(stderr, "save_png failed!\n");
		fz_drop_context(ctx);
		exit(EXIT_FAILURE);
	}

	fz_drop_context(ctx);
}
