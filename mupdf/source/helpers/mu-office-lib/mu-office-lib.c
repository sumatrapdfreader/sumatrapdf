/**
 * Mu Office Library
 *
 * Provided access to the core document, loading, displaying and
 * editing routines
 *
 * Intended for use with native UI
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "mupdf/helpers/mu-office-lib.h"
#include "mupdf/helpers/mu-threads.h"
#include "mupdf/memento.h"

#include <assert.h>

enum
{
	MuError_OK = 0,
	MuError_OOM = -1,
	MuError_BadNull = -2,
	MuError_Generic = -3,
	MuError_NotImplemented = -4,
	MuError_PasswordPending = -5,
};

enum {
	LAYOUT_W = 450,
	LAYOUT_H = 600,
	LAYOUT_EM = 12
};

#ifdef DISABLE_MUTHREADS
#error "mu-office-lib requires threading to be enabled"
#endif

/*
	If we are building as part of a smartoffice build, then we
	should appeal to Pal_Mem_etc to get memory. If not, then
	we should use malloc instead.

	FIXME: Allow for something other than malloc/calloc/realloc/
	free here.
*/
#ifndef SMARTOFFICE_BUILD
void *Pal_Mem_calloc(unsigned int num, size_t size)
{
	return calloc(num, size);
}

void *Pal_Mem_malloc(size_t size)
{
	return malloc(size);
}

void *Pal_Mem_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

void Pal_Mem_free(void *address)
{
	free(address);
}
#endif

/*
	All MuPDF's allocations are redirected through the
	following functions.
*/
static void *muoffice_malloc(void *arg, size_t size)
{
	return Pal_Mem_malloc(size);
}

static void *muoffice_realloc(void *arg, void *old, size_t size)
{
	return Pal_Mem_realloc(old, size);
}

static void muoffice_free(void *arg, void *ptr)
{
	Pal_Mem_free(ptr);
}

static fz_alloc_context muoffice_alloc =
{
	/* user */
	NULL,

	/* void *(*malloc)(void *, size_t); */
	muoffice_malloc,

	/* void *(*realloc)(void *, void *, size_t); */
	muoffice_realloc,

	/* void (*free)(void *, void *); */
	muoffice_free
};

/*
	All MuPDF's locking is done using the following functions
*/
static void muoffice_lock(void *user, int lock);

static void muoffice_unlock(void *user, int lock);

struct MuOfficeLib_s
{
	fz_context *ctx;
	mu_mutex mutexes[FZ_LOCK_MAX+1];
	fz_locks_context locks;
};

/*
	We add 1 extra lock which we use in this helper to protect
	against accessing the fz_document from multiple threads
	inadvertently when the caller is calling 'run' or
	'runBackground'.
*/
enum
{
	DOCLOCK = FZ_LOCK_MAX
};

static void muoffice_lock(void *user, int lock)
{
	MuOfficeLib *mu = (MuOfficeLib *)user;

	mu_lock_mutex(&mu->mutexes[lock]);
}

static void muoffice_unlock(void *user, int lock)
{
	MuOfficeLib *mu = (MuOfficeLib *)user;

	mu_unlock_mutex(&mu->mutexes[lock]);
}

static void muoffice_doc_lock(MuOfficeLib *mu)
{
	mu_lock_mutex(&mu->mutexes[DOCLOCK]);
}

static void muoffice_doc_unlock(MuOfficeLib *mu)
{
	mu_unlock_mutex(&mu->mutexes[DOCLOCK]);
}

static void fin_muoffice_locks(MuOfficeLib *mu)
{
	int i;

	for (i = 0; i < FZ_LOCK_MAX+1; i++)
		mu_destroy_mutex(&mu->mutexes[i]);
}

static fz_locks_context *init_muoffice_locks(MuOfficeLib *mu)
{
	int i;
	int failed = 0;

	for (i = 0; i < FZ_LOCK_MAX+1; i++)
		failed |= mu_create_mutex(&mu->mutexes[i]);

	if (failed)
	{
		fin_muoffice_locks(mu);
		return NULL;
	}

	mu->locks.user = mu;
	mu->locks.lock = muoffice_lock;
	mu->locks.unlock = muoffice_unlock;

	return &mu->locks;
}

MuError MuOfficeLib_create(MuOfficeLib **pMu)
{
	MuOfficeLib *mu;
	fz_locks_context *locks;

	if (pMu == NULL)
		return MuOfficeDocErrorType_IllegalArgument;

	mu = Pal_Mem_calloc(1, sizeof(MuOfficeLib));
	if (mu == NULL)
		return MuOfficeDocErrorType_OutOfMemory;

	locks = init_muoffice_locks(mu);
	if (locks == NULL)
		goto Fail;

	mu->ctx = fz_new_context(&muoffice_alloc, locks, FZ_STORE_DEFAULT);
	if (mu->ctx == NULL)
		goto Fail;

	fz_try(mu->ctx)
		fz_register_document_handlers(mu->ctx);
	fz_catch(mu->ctx)
		goto Fail;

	*pMu = mu;

	return MuOfficeDocErrorType_NoError;

Fail:
	if (mu)
	{
		fin_muoffice_locks(mu);
		Pal_Mem_free(mu);
	}
	return MuOfficeDocErrorType_OutOfMemory;
}

/**
 * Destroy a MuOfficeLib instance
 *
 * @param mu  the instance to destroy
 */
void MuOfficeLib_destroy(MuOfficeLib *mu)
{
	if (mu == NULL)
		return;

	fz_drop_context(mu->ctx);
	fin_muoffice_locks(mu);

	Pal_Mem_free(mu);
}

/**
 * Perform MuPDF native operations on a given MuOfficeLib
 * instance.
 *
 * The function is called with a fz_context value that can
 * be safely used (i.e. the context is cloned/dropped
 * appropriately around the call). The function should signal
 * errors by fz_throw-ing.
 *
 * @param mu           the MuOfficeLib instance.
 * @param fn           the function to call to run the operations.
 * @param arg          Opaque data pointer.
 *
 * @return             error indication - 0 for success
 */
MuError MuOfficeLib_run(MuOfficeLib *mu, void (*fn)(fz_context *ctx, void *arg), void *arg)
{
	fz_context *ctx;
	MuError err = MuError_OK;

	if (mu == NULL)
		return MuError_BadNull;
	if (fn == NULL)
		return err;

	ctx = fz_clone_context(mu->ctx);
	if (ctx == NULL)
		return MuError_OOM;

	fz_try(ctx)
		fn(ctx, arg);
	fz_catch(ctx)
		err = MuError_Generic;

	fz_drop_context(ctx);

	return err;
}

/**
 * Find the type of a file given its filename extension.
 *
 * @param path      path to the file (in utf8)
 *
 * @return          a valid MuOfficeDocType value, or MuOfficeDocType_Other
 */
MuOfficeDocType MuOfficeLib_getDocTypeFromFileExtension(const char *path)
{
	return /* FIXME */MuOfficeDocType_PDF;
}

/**
 * Return a list of file extensions supported by Mu Office library.
 *
 * @return    comma-delimited list of extensions, without the leading ".".
 *            The caller should free the returned pointer..
 */
char * MuOfficeLib_getSupportedFileExtensions(void)
{
	/* FIXME */
	return NULL;
}

struct MuOfficeDoc_s
{
	MuOfficeLib *mu;
	fz_context *ctx;
	MuOfficeLoadingProgressFn *progress;
	MuOfficeLoadingErrorFn *error;
	void *cookie;
	char *path;
	char *password;
	mu_semaphore password_sem;
	mu_thread thread;
	int needs_password;
	int aborted;
	fz_document *doc;

	MuOfficePage *pages;
};

struct MuOfficePage_s
{
	MuOfficePage *next;
	MuOfficeDoc *doc;
	int pageNum;
	void *cookie;
	MuOfficePageUpdateFn *updateFn;
	fz_page *page;
	fz_display_list *list;
};

struct MuOfficeRender_s
{
	MuOfficePage *page;
	float zoom;
	const MuOfficeBitmap *bitmap;
	int area_valid;
	MuOfficeRenderArea area;
	MuOfficeRenderProgressFn *progress;
	MuError error;
	mu_thread thread;
	void *cookie;
	fz_cookie mu_cookie;
};

static void load_worker(void *arg)
{
	MuOfficeDoc *doc = (MuOfficeDoc *)arg;
	int numPages = 0;
	fz_context *ctx = fz_clone_context(doc->ctx);
	int err = 0;

	if (ctx == NULL)
	{
		return;
	}

	muoffice_doc_lock(doc->mu);

	fz_try(ctx)
	{
		doc->doc = fz_open_document(ctx, doc->path);
		doc->needs_password = fz_needs_password(ctx, doc->doc);
	}
	fz_catch(ctx)
	{
		err = MuOfficeDocErrorType_UnsupportedDocumentType;
		goto fail;
	}

	fz_try(ctx)
	{
		if (doc->needs_password && doc->error)
		{
			do
			{
				doc->error(doc->cookie, MuOfficeDocErrorType_PasswordRequest);
				mu_wait_semaphore(&doc->password_sem);
				if (doc->aborted)
					break;
				doc->needs_password = (fz_authenticate_password(ctx, doc->doc, doc->password) != 0);
				Pal_Mem_free(doc->password);
				doc->password = NULL;
			}
			while (doc->needs_password);
		}

		fz_layout_document(ctx, doc->doc, LAYOUT_W, LAYOUT_H, LAYOUT_EM);

		numPages = fz_count_pages(ctx, doc->doc);
	}
	fz_catch(ctx)
		err = MuOfficeDocErrorType_UnableToLoadDocument;

fail:
	muoffice_doc_unlock(doc->mu);

	if (err)
		doc->error(doc->cookie, err);

	if (doc->progress)
		doc->progress(doc->cookie, numPages, 1);

	fz_drop_context(ctx);
}

/**
 * Load a document
 *
 * Call will return immediately, leaving the document loading
 * in the background
 *
 * @param so         a MuOfficeLib instance
 * @param path       path to the file to load (in utf8)
 * @param progressFn callback for monitoring progress
 * @param errorFn    callback for monitoring errors
 * @param cookie     a pointer to pass back to the callbacks
 * @param pDoc       address for return of a MuOfficeDoc object
 *
 * @return          error indication - 0 for success
 *
 * The progress callback may be called several times, with increasing
 * values of pagesLoaded. Unless MuOfficeDoc_destroy is called,
 * before loading completes, a call with "completed" set to true
 * is guaranteed.
 *
 * Once MuOfficeDoc_destroy is called there will be no
 * further callbacks.
 *
 * Alternatively, in a synchronous context, MuOfficeDoc_getNumPages
 * can be called to wait for loading to complete and return the total
 * number of pages. In this mode of use, progressFn can be NULL. 
 */
MuError MuOfficeLib_loadDocument(	MuOfficeLib               *mu,
					const char                *path,
					MuOfficeLoadingProgressFn *progressFn,
					MuOfficeLoadingErrorFn    *errorFn,
					void                      *cookie,
					MuOfficeDoc              **pDoc)
{
	MuOfficeDoc *doc;
	fz_context *ctx;

	if (mu == NULL || pDoc == NULL)
		return MuOfficeDocErrorType_IllegalArgument;

	*pDoc = NULL;

	doc = Pal_Mem_calloc(1, sizeof(*doc));
	if (doc == NULL)
		return MuOfficeDocErrorType_NoError;

	ctx = mu->ctx;
	doc->mu       = mu;
	doc->ctx      = fz_clone_context(ctx);
	doc->progress = progressFn;
	doc->error    = errorFn;
	doc->cookie   = cookie;
	doc->path     = fz_strdup(ctx, path);
	if (mu_create_semaphore(&doc->password_sem))
		goto fail;

	if (mu_create_thread(&doc->thread, load_worker, doc))
		goto fail;

	*pDoc = doc;

	return MuError_OK;
fail:
	mu_destroy_semaphore(&doc->password_sem);
	Pal_Mem_free(doc);

	return MuError_OOM;
}

/**
 * Provide the password for a document
 *
 * This function should be called to provide a password with a document
 * error of MuOfficeError_PasswordRequired is received.
 *
 * If a password is requested again, this means the password was incorrect.
 *
 * @param doc         the document object
 * @param password    the password (UTF8 encoded)
 * @return            error indication - 0 for success
 */
int MuOfficeDoc_providePassword(MuOfficeDoc *doc, const char *password)
{
	size_t len;

	if (doc->password)
		return MuError_PasswordPending;
	if (!password)
		password = "";

	len = strlen(password);
	doc->password = Pal_Mem_malloc(len+1);
	strcpy(doc->password, password);
	mu_trigger_semaphore(&doc->password_sem);

	return MuError_OK;
}

/**
 * Return the type of an open document
 *
 * @param doc         the document object
 *
 * @return            the document type
 */
MuOfficeDocType MuOfficeDoc_docType(MuOfficeDoc *doc)
{
	return /* FIXME */MuOfficeDocType_PDF;
}

static void
ensure_doc_loaded(MuOfficeDoc *doc)
{
	if (doc == NULL)
		return;

	mu_destroy_thread(&doc->thread);
}

/**
 * Return the number of pages of a document
 *
 * This function waits for document loading to complete before returning
 * the result. It may block the calling thread for a significant period of
 * time. To avoid blocking, this call should be avoided in favour of using
 * the MuOfficeLib_loadDocument callbacks to monitor loading.
 *
 * If background loading fails, the associated error will be returned
 * from this call.
 *
 * @param doc         the document
 * @param pNumPages   address for return of the number of pages
 *
 * @return            error indication - 0 for success
 */
MuError MuOfficeDoc_getNumPages(MuOfficeDoc *doc, int *pNumPages)
{
	fz_context *ctx;
	MuError err = MuError_OK;

	if (doc == NULL)
	{
		*pNumPages = 0;
		return MuError_BadNull;
	}

	ensure_doc_loaded(doc);

	ctx = doc->ctx;

	fz_try(ctx)
	{
		*pNumPages = fz_count_pages(ctx, doc->doc);
	}
	fz_catch(ctx)
	{
		err = MuError_Generic;
	}

	return err;
}

/**
 * Determine if the document has been modified
 *
 * @param doc         the document
 *
 * @return            modified flag
 */
int MuOfficeDoc_hasBeenModified(MuOfficeDoc *doc)
{
	fz_context *ctx;
	pdf_document *pdoc;
	int modified = 0;

	if (doc == NULL)
		return 0;

	ensure_doc_loaded(doc);

	ctx = doc->ctx;
	pdoc = pdf_specifics(ctx, doc->doc);

	if (pdoc == NULL)
		return 0;

	fz_try(ctx)
		modified = pdf_has_unsaved_changes(ctx, pdoc);
	fz_catch(ctx)
		modified = 0;

	return modified;
}

/**
 * Start a save operation
 *
 * @param doc         the document
 * @param path        path of the file to which to save
 * @param resultFn    callback used to report completion
 * @param cookie      a pointer to pass to the callback
 *
 * @return            error indication - 0 for success
 */
MuError MuOfficeDoc_save(	MuOfficeDoc          *doc,
				const char              *path,
				MuOfficeSaveResultFn *resultFn,
				void                    *cookie)
{
	return MuError_NotImplemented; /* FIXME */
}

/**
 * Stop a document loading. The document is not destroyed, but
 * no further content will be read from the file.
 *
 * @param doc       the MuOfficeDoc object
 */
void MuOfficeDoc_abortLoad(MuOfficeDoc *doc)
{
	fz_context *ctx;

	if (doc == NULL)
		return;

	ctx = doc->ctx;
	doc->aborted = 1;
	mu_trigger_semaphore(&doc->password_sem);
}

/**
 * Destroy a MuOfficeDoc object. Loading of the document is shutdown
 * and no further callbacks will be issued for the specified object.
 *
 * @param doc       the MuOfficeDoc object
 */
void MuOfficeDoc_destroy(MuOfficeDoc *doc)
{
	MuOfficeDoc_abortLoad(doc);
	mu_destroy_thread(&doc->thread);
	mu_destroy_semaphore(&doc->password_sem);

	fz_drop_document(doc->ctx, doc->doc);
	fz_drop_context(doc->ctx);
	Pal_Mem_free(doc->path);
	Pal_Mem_free(doc);
}

/**
 * Get a page of a document
 *
 * @param doc          the document object
 * @param pageNumber   the number of the page to load (lying in the
 *                     range 0 to one less than the number of pages)
 * @param updateFn     Function to be called back when the page updates
 * @param cookie       Opaque value to pass for any updates
 * @param pPage        Address for return of the page object
 *
 * @return             error indication - 0 for success
 */
MuError MuOfficeDoc_getPage(	MuOfficeDoc          *doc,
				int                   pageNumber,
				MuOfficePageUpdateFn *updateFn,
				void                 *cookie,
				MuOfficePage        **pPage)
{
	MuOfficePage *page;
	MuError err = MuError_OK;
	fz_context *ctx;

	if (!doc)
		return MuError_BadNull;
	if (!pPage)
		return MuError_OK;

	*pPage = NULL;

	ensure_doc_loaded(doc);
	ctx = doc->ctx;

	page = Pal_Mem_calloc(1, sizeof(*page));
	if (page == NULL)
		return MuError_OOM;

	muoffice_doc_lock(doc->mu);

	fz_try(ctx)
	{
		page->doc = doc;
		page->pageNum = pageNumber;
		page->cookie = cookie;
		page->updateFn = updateFn;
		page->page = fz_load_page(doc->ctx, doc->doc, pageNumber);
		page->next = doc->pages;
		doc->pages = page;
		*pPage = page;
	}
	fz_catch(ctx)
	{
		Pal_Mem_free(page);
		err = MuError_Generic;
	}

	muoffice_doc_unlock(doc->mu);

	return err;
}

/**
 * Perform MuPDF native operations on a given document.
 *
 * The function is called with fz_context and fz_document
 * values that can be safely used (i.e. the context is
 * cloned/dropped appropriately around the function, and
 * locking is used to ensure that no other threads are
 * simultaneously using the document). Functions can
 * signal errors by fz_throw-ing.
 *
 * Due to the locking, it is best to ensure that as little
 * time is taken here as possible (i.e. if you fetch some
 * data and then spend a long time processing it, it is
 * probably best to fetch the data using MuOfficeDoc_run
 * and then process it outside). This avoids potentially
 * blocking the UI.
 *
 * @param doc          the document object.
 * @param fn           the function to call with fz_context/fz_document
 *                     values.
 * @param arg          Opaque data pointer.
 *
 * @return             error indication - 0 for success
 */
MuError MuOfficeDoc_run(MuOfficeDoc *doc, void (*fn)(fz_context *ctx, fz_document *doc, void *arg), void *arg)
{
	fz_context *ctx;
	MuError err = MuError_OK;

	if (doc == NULL)
		return MuError_BadNull;
	if (fn == NULL)
		return err;

	ensure_doc_loaded(doc);

	ctx = fz_clone_context(doc->mu->ctx);
	if (ctx == NULL)
		return MuError_OOM;

	muoffice_doc_lock(doc->mu);

	fz_try(ctx)
		fn(ctx, doc->doc, arg);
	fz_catch(ctx)
		err = MuError_Generic;

	muoffice_doc_unlock(doc->mu);

	fz_drop_context(ctx);

	return err;
}

/**
 * Destroy a page object
 *
 * Note this does not delete or remove the page from the document.
 * It simply destroys the page object which is merely a reference
 * to the page.
 *
 * @param page         the page object
 */
void MuOfficePage_destroy(MuOfficePage *page)
{
	MuOfficeDoc *doc;
	MuOfficePage **ptr;

	if (!page)
		return;

	/* Unlink page from doc */
	doc = page->doc;
	ptr = &doc->pages;
	while (*ptr && *ptr != page)
		ptr = &(*ptr)->next;
	assert(*ptr);
	*ptr = page->next;

	fz_drop_page(doc->ctx, page->page);
	fz_drop_display_list(doc->ctx, page->list);
	fz_free(doc->ctx, page);
}

/**
 * Get the size of a page in pixels
 *
 * This returns the size of the page in pixels. Pages can be rendered
 * with a zoom factor. The returned value is the size of bitmap
 * appropriate for rendering with a zoom of 1.0 and corresponds to
 * 90 dpi. The returned values are not necessarily whole numbers.
 *
 * @param page         the page object
 * @param pWidth       address for return of the width
 * @param pHeight      address for return of the height
 *
 * @return             error indication - 0 for success
 */
MuError MuOfficePage_getSize(	MuOfficePage *page,
				float        *pWidth,
				float        *pHeight)
{
	MuOfficeDoc *doc;
	fz_rect rect;

	if (!page)
		return MuError_BadNull;
	doc = page->doc;
	if (!doc)
		return MuError_BadNull;

	rect = fz_bound_page(doc->ctx, page->page);

	/* MuPDF measures in points (72ths of an inch). This API wants
	 * 90ths of an inch, so adjust. */

	if (pWidth)
		*pWidth = 90 * (rect.x1 - rect.x0) / 72;
	if (pHeight)
		*pHeight = 90 * (rect.y1 - rect.y0) / 72;

	return MuError_OK;
}

/**
 * Return the zoom factors necessary to render at to a given
 * size in pixels. (deprecated)
 *
 * @param page         the page object
 * @param width        the desired width
 * @param height       the desired height
 * @param pXZoom       Address for return of zoom necessary to fit width
 * @param pYZoom       Address for return of zoom necessary to fit height
 *
 * @return             error indication - 0 for success
 */
MuError MuOfficePage_calculateZoom(	MuOfficePage *page,
					int           width,
					int           height,
					float	     *pXZoom,
					float        *pYZoom)
{
	MuOfficeDoc *doc;
	fz_rect rect;
	float w, h;

	if (!page)
		return MuError_BadNull;
	doc = page->doc;
	if (!doc)
		return MuError_BadNull;

	rect = fz_bound_page(doc->ctx, page->page);

	/* MuPDF measures in points (72ths of an inch). This API wants
	 * 90ths of an inch, so adjust. */
	w = 90 * (rect.x1 - rect.x0) / 72;
	h = 90 * (rect.y1 - rect.y0) / 72;

	if (pXZoom)
		*pXZoom = width/w;
	if (pYZoom)
		*pYZoom = height/h;

	return MuError_OK;
}

/**
 * Get the size of a page in pixels for a specified zoom factor
 * (deprecated)
 *
 * This returns the size of bitmap that should be used to display
 * the entire page at the given zoom factor. A zoom of 1.0
 * corresponds to 90 dpi.
 *
 * @param page         the page object
 * @param zoom         the zoom factor
 * @param pWidth       address for return of the width
 * @param pHeight      address for return of the height
 *
 * @return             error indication - 0 for success
 */
MuError MuOfficePage_getSizeForZoom(	MuOfficePage *page,
					float         zoom,
					int          *pWidth,
					int          *pHeight)
{
	MuOfficeDoc *doc;
	fz_rect rect;
	float w, h;

	if (!page)
		return MuError_BadNull;
	doc = page->doc;
	if (!doc)
		return MuError_BadNull;

	rect = fz_bound_page(doc->ctx, page->page);

	/* MuPDF measures in points (72ths of an inch). This API wants
	 * 90ths of an inch, so adjust. */
	w = 90 * (rect.x1 - rect.x0) / 72;
	h = 90 * (rect.y1 - rect.y0) / 72;

	if (pWidth)
		*pWidth = (int)(w * zoom + 0.5f);
	if (pHeight)
		*pHeight = (int)(h * zoom + 0.5f);

	return MuError_OK;
}

/**
 * Perform MuPDF native operations on a given page.
 *
 * The function is called with fz_context and fz_page
 * values that can be safely used (i.e. the context is
 * cloned/dropped appropriately around the function, and
 * locking is used to ensure that no other threads are
 * simultaneously using the document). Functions can
 * signal errors by fz_throw-ing.
 *
 * Due to the locking, it is best to ensure that as little
 * time is taken here as possible (i.e. if you fetch some
 * data and then spend a long time processing it, it is
 * probably best to fetch the data using MuOfficePage_run
 * and then process it outside). This avoids potentially
 * blocking the UI.
 *
 * @param page         the page object.
 * @param fn           the function to call with fz_context/fz_document
 *                     values.
 * @param arg          Opaque data pointer.
 *
 * @return             error indication - 0 for success
 */
MuError MuOfficePage_run(MuOfficePage *page, void (*fn)(fz_context *ctx, fz_page *page, void *arg), void *arg)
{
	fz_context *ctx;
	MuError err = MuError_OK;

	if (page == NULL)
		return MuError_BadNull;
	if (fn == NULL)
		return err;

	ctx = fz_clone_context(page->doc->mu->ctx);
	if (ctx == NULL)
		return MuError_OOM;

	muoffice_doc_lock(page->doc->mu);

	fz_try(ctx)
		fn(ctx, page->page, arg);
	fz_catch(ctx)
		err = MuError_Generic;

	muoffice_doc_unlock(page->doc->mu);

	fz_drop_context(ctx);

	return err;
}

static void render_worker(void *arg)
{
	MuOfficeRender *render = (MuOfficeRender *)arg;
	MuOfficePage *page = render->page;
	fz_context *ctx = fz_clone_context(page->doc->ctx);
	int err = 0;
	fz_pixmap *pixmap = NULL;
	fz_device *dev = NULL;
	float scalex;
	float scaley;
	fz_rect page_bounds;
	int locked = 0;

	if (ctx == NULL)
		return;

	fz_var(pixmap);
	fz_var(dev);
	fz_var(locked);

	fz_try(ctx)
	{
		if (page->list == NULL)
		{
			muoffice_doc_lock(page->doc->mu);
			locked = 1;
			page->list = fz_new_display_list_from_page(ctx, page->page);
			locked = 0;
			muoffice_doc_unlock(page->doc->mu);
		}
		/* Make a pixmap from the bitmap */
		if (!render->area_valid)
		{
			render->area.renderArea.x = 0;
			render->area.renderArea.y = 0;
			render->area.renderArea.width = render->bitmap->width;
			render->area.renderArea.height = render->bitmap->height;
		}
		pixmap = fz_new_pixmap_with_data(ctx,
						fz_device_rgb(ctx),
						render->area.renderArea.width,
						render->area.renderArea.height,
						NULL,
						1,
						render->bitmap->lineSkip,
						((unsigned char *)render->bitmap->memptr) +
							render->bitmap->lineSkip * ((int)render->area.renderArea.x + (int)render->area.origin.x) +
							4 * ((int)render->area.renderArea.y + (int)render->area.origin.y));
		/* Be a bit clever with the scaling to make sure we get
		 * integer width/heights. First calculate the target
		 * width/height. */
		page_bounds = fz_bound_page(ctx, render->page->page);
		scalex = (int)(90 * render->zoom * (page_bounds.x1 - page_bounds.x0) / 72 + 0.5f);
		scaley = (int)(90 * render->zoom * (page_bounds.y1 - page_bounds.y0) / 72 + 0.5f);
		/* Now calculate the actual scale factors required */
		scalex /= (page_bounds.x1 - page_bounds.x0);
		scaley /= (page_bounds.y1 - page_bounds.y0);
		/* Render the list */
		fz_clear_pixmap_with_value(ctx, pixmap, 0xFF);
		dev = fz_new_draw_device(ctx, fz_post_scale(fz_translate(-page_bounds.x0, -page_bounds.y0), scalex, scaley), pixmap);
		fz_run_display_list(ctx, page->list, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, pixmap);
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		if (locked)
			muoffice_doc_unlock(page->doc->mu);
		err = MuError_Generic;
		goto fail;
	}

fail:
	if (render->progress)
		render->progress(render->cookie, err);
	render->error = err;

	fz_drop_context(ctx);
}

/**
 * Schedule the rendering of an area of document page to
 * an area of a bitmap.
 *
 * The alignment between page and bitmap is defined by specifying
 * document's origin within the bitmap, possibly either positive or
 * negative. A render object is returned via which the process can
 * be monitored or terminated.
 *
 * The progress function is called exactly once per render in either
 * the success or failure case.
 *
 * Note that, since a render object represents a running thread that
 * needs access to the page, document, and library objects, it is important
 * to call MuOfficeRender_destroy, not only before using or deallocating
 * the bitmap, but also before calling MuOfficePage_destroy, etc..
 *
 * @param page              the page to render
 * @param zoom              the zoom factor
 * @param bitmap            the bitmap
 * @param area              area to render
 * @param progressFn        the progress callback function
 * @param cookie            a pointer to pass to the callback function
 * @param pRender           Address for return of the render object
 *
 * @return                  error indication - 0 for success
 */
MuError MuOfficePage_render(	MuOfficePage             *page,
				float                     zoom,
			const	MuOfficeBitmap           *bitmap,
			const	MuOfficeRenderArea       *area,
				MuOfficeRenderProgressFn *progressFn,
				void                     *cookie,
				MuOfficeRender          **pRender)
{
	MuOfficeRender *render;
	MuOfficeDoc *doc;
	fz_context *ctx;

	if (!pRender)
		return MuError_BadNull;
	*pRender = NULL;
	if (!page)
		return MuError_BadNull;
	doc = page->doc;
	ctx = doc->ctx;

	render = Pal_Mem_calloc(1, sizeof(*render));
	if (render == NULL)
		return MuError_OOM;

	render->page = page;
	render->zoom = zoom;
	render->bitmap = bitmap;
	if (area)
	{
		render->area = *area;
		render->area_valid = 1;
	}
	else
	{
		render->area_valid = 0;
	}
	render->progress = progressFn;
	render->cookie = cookie;

	if (mu_create_thread(&render->thread, render_worker, render))
	{
		Pal_Mem_free(render);
		return MuError_OOM;
	}

	*pRender = render;

	return MuError_OK;
}

/**
 * Destroy a render
 *
 * This call destroys a MuOfficeRender object, aborting any current
 * render.
 *
 * This call is intended to support an app dealing with a user quickly
 * flicking through document pages. A render may be scheduled but, before
 * completion, be found not to be needed. In that case the bitmap will
 * need to be reused, which requires any existing render to be aborted.
 * The call to MuOfficeRender_destroy will cut short the render and
 * allow the bitmap to be reused immediately.
 *
 * @note If an active render thread is destroyed, it will be aborted.
 * While fast, this is not an instant operation. For maximum
 * responsiveness, it is best to 'abort' as soon as you realise you
 * don't need the render, and to destroy when you get the callback.
 *
 * @param render           The render object
 */
void MuOfficeRender_destroy(MuOfficeRender *render)
{
	if (!render)
		return;

	MuOfficeRender_abort(render);
	mu_destroy_thread(&render->thread);
	Pal_Mem_free(render);
}

/**
 * Abort a render
 *
 * This call aborts any rendering currently underway. The 'render
 * complete' callback (if any) given when the render was created will
 * still be called. If a render has completed, this call will have no
 * effect.
 *
 * This call will not block to wait for the render thread to stop, but
 * will cause it to stop as soon as it can in the background.
 *
 * @note It is important not to start any new render to the same bitmap
 * until the callback comes in (or until waitUntilComplete returns), as
 * otherwise you can have multiple renders drawing to the same bitmap
 * with unpredictable consequences.
 *
 * @param render           The render object to abort
 */
void MuOfficeRender_abort(MuOfficeRender *render)
{
	if (render)
		render->mu_cookie.abort = 1;
}

/**
 * Wait for a render to complete.
 *
 * This call will not return until rendering is complete, so on return
 * the bitmap will contain the page image (assuming the render didn't
 * run into an error condition) and will not be used further by any
 * background processing. Any error during rendering will be returned
 * from this function.
 *
 * This call may block the calling thread for a significant period of
 * time. To avoid blocking, supply a progress-monitoring callback
 * function to MuOfficePage_render.
 *
 * @param render           The render object to destroy
 * @return render error condition - 0 for no error.
 */
MuError MuOfficeRender_waitUntilComplete(MuOfficeRender *render)
{
	if (!render)
		return MuError_OK;

	mu_destroy_thread(&render->thread);

	return render->error;
}
