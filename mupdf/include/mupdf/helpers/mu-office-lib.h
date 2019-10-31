/**
 * Mu Office Library
 *
 * This helper layer provides an API for loading, and displaying
 * a file. It is deliberately as identical as possible to the
 * smart-office-lib.h header file in Smart Office to facilitate
 * a product which can use both Smart Office and MuPDF.
 */

#ifndef MU_OFFICE_LIB_H
#define MU_OFFICE_LIB_H

/*
 * General use
 *
 * This library uses threads but is not thread safe for the caller. All
 * calls should be made from a single thread. Some calls allow the user
 * to arrange for their own functions to be called back from the library;
 * often the users functions will be called back from a different thread. If
 * a call into the library is required in response to information from a
 * callback, it is the responsibility of the user to arrange for those
 * library calls to be made on the same thread as for other library calls.
 * Calls back into the library from a callback are not permitted.
 *
 * There are two main modes of use. In interactive windowing systems, it
 * is usual for the app developers code to be called on the main UI thread.
 * It is from the UI thread that all library calls should be made. Such
 * systems usually provide a way to post a call onto the UI thread. That
 * is the best way to respond to a callback from the library.
 *
 * The other mode of use is for plain executables that might wish to
 * (say) generate images of all the pages of a document. This can
 * be achieved, without use of callbacks, using the few synchronous
 * calls. E.g., MuOfficeDoc_getNumPages will wait for background document
 * loading to complete before returning the total number of pages and
 * MuOfficeRender_destroy will wait for background rendering to complete
 * before returning.
 */

#include <stddef.h> /* For size_t */
#include "mupdf/fitz.h" /* For fz_context/fz_document/fz_page */

/** Error type returned from most MuOffice functions
 *
 * 0 means no error
 *
 * non-zero values mean an error occurred. The exact value is an indication
 * of what went wrong and should be included in bug reports or support
 * queries. Library users should not test this value except for 0,
 * non-zero and any explicitly documented values.
 */
typedef int MuError;

/** Errors returned to MuOfficeLoadingErrorFn
 *
 * Other values may also be returned.
 */
typedef enum MuOfficeDocErrorType
{
	MuOfficeDocErrorType_NoError = 0,
	MuOfficeDocErrorType_UnsupportedDocumentType = 1,
	MuOfficeDocErrorType_EmptyDocument = 2,
	MuOfficeDocErrorType_UnableToLoadDocument = 4,
	MuOfficeDocErrorType_UnsupportedEncryption = 5,
	MuOfficeDocErrorType_Aborted = 6,
	MuOfficeDocErrorType_OutOfMemory = 7,

	/* FIXME: Additional ones that should be backported to
	 * smart-office-lib.h */
	MuOfficeDocErrorType_IllegalArgument = 8,

	/** A password is required to open this document.
	 *
	 * The app should provide it using MuOffice_providePassword
	 * or if it doesn't want to proceed call MuOfficeDoc_destroy or
	 * MuOfficeDoc_abortLoad.
	 */
	MuOfficeDocErrorType_PasswordRequest = 0x1000
} MuOfficeDocErrorType;

/**
 *Structure holding the detail of the layout of a bitmap. b5g6r5 is assumed.
 */
typedef struct MuOfficeBitmap_s
{
	void *memptr;
	int   width;
	int   height;
	int   lineSkip;
} MuOfficeBitmap;

/**
 * Structure defining a point
 *
 *    x           x coord of point
 *    y           y coord of point
 */
typedef struct MuOfficePoint_s
{
	float x;
	float y;
} MuOfficePoint;

/**
 * Structure defining a rectangle
 *
 *    x           x coord of top left of area within the page
 *    y           y coord of top left of area within the page
 *    width       width of area
 *    height      height of area
 */
typedef struct MuOfficeBox_s
{
	float x;
	float y;
	float width;
	float height;
} MuOfficeBox;

typedef enum MuOfficePointType
{
	MuOfficePointType_MoveTo,
	MuOfficePointType_LineTo
} MuOfficePointType;

typedef struct MuOfficePathPoint
{
	float x;
	float y;
	MuOfficePointType type;
} MuOfficePathPoint;

/**
 * Structure defining what area of a page should be rendered and to what
 * area of the bitmap
 *
 *    origin            coordinates of the document origin within the bitmap
 *    renderArea        the part of the bitmap to which to render
 */
typedef struct MuOfficeRenderArea_s
{
	MuOfficePoint   origin;
	MuOfficeBox     renderArea;
} MuOfficeRenderArea;

typedef struct MuOfficeLib_s MuOfficeLib;
typedef struct MuOfficeDoc_s MuOfficeDoc;
typedef struct MuOfficePage_s MuOfficePage;
typedef struct MuOfficeRender_s MuOfficeRender;

/**
 * Allocator function used by some functions to get blocks of memory.
 *
 * @param cookie     data pointer passed in with the allocator.
 * @param size       the size of the required block.
 *
 * @returns as for malloc. (NULL implies OutOfMemory, or size == 0).
 * Otherwise a pointer to an allocated block.
 */
typedef void *(MuOfficeAllocFn)(void   *cookie,
				size_t  size);

/**
 * Callback function monitoring document loading
 *
 * Also called when the document is edited, either adding or
 * removing pages, with the pagesLoaded value decreasing
 * in the page-removal case.
 *
 * @param cookie        the data pointer that was originally passed
 *                      to MuOfficeLib_loadDocument.
 * @param pagesLoaded   the number of pages so far discovered.
 * @param complete      whether loading has completed. If this flag
 *                      is set, pagesLoaded is the actual number of
 *                      pages in the document.
 */
typedef void (MuOfficeLoadingProgressFn)(void *cookie,
					int   pagesLoaded,
					int   complete);

/**
 * Callback function used to monitor errors in the process of loading
 * a document.
 *
 * @param cookie        the data pointer that was originally passed
 *                      to MuOfficeLib_loadDocument.
 * @param error         the error being reported
 */
typedef void (MuOfficeLoadingErrorFn)(	void                 *cookie,
					MuOfficeDocErrorType  error);

/**
 * Callback function used to monitor page changes
 *
 * @param cookie        the data pointer that was originally passed
 *                      to MuOfficeDoc_getPage.
 * @param area          the area that has changed.
 */
typedef void (MuOfficePageUpdateFn)(	void                 *cookie,
					const MuOfficeBox *area);

/**
 * Callback function used to monitor a background render of a
 * document page. The function is called exactly once.
 *
 * @param cookie        the data pointer that was originally passed
 *                      to MuOfficeDoc_monitorRenderProgress.
 * @param error         error returned from the rendering process
 */
typedef void (MuOfficeRenderProgressFn)(void    *cookie,
					MuError  error);

/**
 * Document types
 *
 * Keep in sync with smart-office-lib.h
 */
typedef enum
{
	MuOfficeDocType_PDF,
	MuOfficeDocType_XPS,
	MuOfficeDocType_IMG
} MuOfficeDocType;

/**
 * The possible results of a save operation
 */
typedef enum MuOfficeSaveResult
{
	MuOfficeSave_Succeeded,
	MuOfficeSave_Error,
	MuOfficeSave_Cancelled
}
MuOfficeSaveResult;

/**
 * Callback function used to monitor save operations.
 *
 * @param cookie        the data pointer that was originally passed to
 *                      MuOfficeDoc_save.
 * @param result        the result of the save operation
 */
typedef void (MuOfficeSaveResultFn)(	void                 *cookie,
					MuOfficeSaveResult result);

/**
 * Create a MuOfficeLib instance.
 *
 * @param pMu       address of variable to
 *                  receive the created instance
 *
 * @return          error indication - 0 for success
 */
MuError MuOfficeLib_create(MuOfficeLib **pMu);

/**
 * Destroy a MuOfficeLib instance
 *
 * @param mu        the instance to destroy
 */
void MuOfficeLib_destroy(MuOfficeLib *mu);

/**
 * Find the type of a file given its filename extension.
 *
 * @param path      path to the file (in utf8)
 *
 * @return          a valid MuOfficeDocType value, or MuOfficeDocType_Other
 */
MuOfficeDocType MuOfficeLib_getDocTypeFromFileExtension(const char *path);

/**
 * Return a list of filename extensions supported by Mu Office library.
 *
 * @return    comma-delimited list of extensions, without the leading ".".
 *            The caller should free the returned pointer..
 */
char * MuOfficeLib_getSupportedFileExtensions(void);

/**
 * Load a document
 *
 * Call will return immediately, leaving the document loading
 * in the background
 *
 * @param mu         a MuOfficeLib instance
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
MuError MuOfficeLib_loadDocument(MuOfficeLib              *mu,
				const char                *path,
				MuOfficeLoadingProgressFn *progressFn,
				MuOfficeLoadingErrorFn    *errorFn,
				void                      *cookie,
				MuOfficeDoc              **pDoc);

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
MuError MuOfficeLib_run(MuOfficeLib *mu, void (*fn)(fz_context *ctx, void *arg), void *arg);

/**
 * Provide the password for a document
 *
 * This function should be called to provide a password with a document
 * error if MuOfficeError_PasswordRequired is received.
 *
 * If a password is requested again, this means the password was incorrect.
 *
 * @param doc         the document object
 * @param password    the password (UTF8 encoded)
 * @return            error indication - 0 for success
 */
int MuOfficeDoc_providePassword(MuOfficeDoc *doc, const char *password);

/**
 * Return the type of an open document
 *
 * @param doc         the document object
 *
 * @return            the document type
 */
MuOfficeDocType MuOfficeDoc_docType(MuOfficeDoc *doc);

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
MuError MuOfficeDoc_getNumPages(MuOfficeDoc *doc, int *pNumPages);

/**
 * Determine if the document has been modified
 *
 * @param doc         the document
 *
 * @return            modified flag
 */
int MuOfficeDoc_hasBeenModified(MuOfficeDoc *doc);

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
MuError MuOfficeDoc_save(MuOfficeDoc         *doc,
			const char           *path,
			MuOfficeSaveResultFn *resultFn,
			void                 *cookie);

/**
 * Stop a document loading. The document is not destroyed, but
 * no further content will be read from the file.
 *
 * @param doc       the MuOfficeDoc object
 */
void MuOfficeDoc_abortLoad(MuOfficeDoc *doc);

/**
 * Destroy a MuOfficeDoc object. Loading of the document is shutdown
 * and no further callbacks will be issued for the specified object.
 *
 * @param doc       the MuOfficeDoc object
 */
void MuOfficeDoc_destroy(MuOfficeDoc *doc);

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
				MuOfficePage        **pPage);

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
MuError MuOfficeDoc_run(MuOfficeDoc *doc, void (*fn)(fz_context *ctx, fz_document *doc, void *arg), void *arg);

/**
 * Destroy a page object
 *
 * Note this does not delete or remove the page from the document.
 * It simply destroys the page object which is merely a reference
 * to the page.
 *
 * @param page         the page object
 */
void MuOfficePage_destroy(MuOfficePage *page);

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
				float        *pHeight);

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
					float        *pXZoom,
					float        *pYZoom);

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
					int          *pHeight);

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
MuError MuOfficePage_run(MuOfficePage *page, void (*fn)(fz_context *ctx, fz_page *page, void *arg), void *arg);

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
				MuOfficeRender          **pRender);

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
void MuOfficeRender_destroy(MuOfficeRender *render);

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
void MuOfficeRender_abort(MuOfficeRender *render);

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
MuError MuOfficeRender_waitUntilComplete(MuOfficeRender *render);

#endif /* SMART_OFFICE_LIB_H */
