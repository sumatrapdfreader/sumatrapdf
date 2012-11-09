#include <jni.h>
#include <time.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "fitz.h"
#include "mupdf.h"

#define LOG_TAG "libmupdf"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

/* Set to 1 to enable debug log traces. */
#define DEBUG 0

/* Enable to log rendering times (render each frame 100 times and time) */
#undef TIME_DISPLAY_LIST

#define MAX_SEARCH_HITS (500)
#define NUM_CACHE (3)

enum
{
	NONE,
	TEXT,
	LISTBOX,
	COMBOBOX
};

typedef struct
{
	int number;
	int width;
	int height;
	fz_rect media_box;
	fz_page *page;
	fz_page *hq_page;
	fz_display_list *page_list;
	fz_display_list *annot_list;
} page_cache;


/* Globals */
fz_colorspace *colorspace;
fz_document *doc;
int resolution = 160;
fz_context *ctx;
fz_bbox *hit_bbox = NULL;
int current;

page_cache pages[NUM_CACHE] = {{0}};

static void drop_page_cache(page_cache *pc)
{
	LOGI("Drop page %d", pc->number);
	fz_free_display_list(ctx, pc->page_list);
	pc->page_list = NULL;
	fz_free_display_list(ctx, pc->annot_list);
	pc->annot_list = NULL;
	fz_free_page(doc, pc->page);
	pc->page = NULL;
	fz_free_page(doc, pc->hq_page);
	pc->hq_page = NULL;
}

static void clear_hq_pages()
{
	int i;

	for (i = 0; i < NUM_CACHE; i++) {
		fz_free_page(doc, pages[i].hq_page);
		pages[i].hq_page = NULL;
	}
}

static void dump_annotation_display_lists()
{
	int i;

	for (i = 0; i < NUM_CACHE; i++) {
		fz_free_display_list(ctx, pages[i].annot_list);
		pages[i].annot_list = NULL;
	}
}

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_openFile(JNIEnv * env, jobject thiz, jstring jfilename)
{
	const char *filename;
	int result = 0;

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (filename == NULL)
	{
		LOGE("Failed to get filename");
		return 0;
	}

	/* 128 MB store for low memory devices. Tweak as necessary. */
	ctx = fz_new_context(NULL, NULL, 128 << 20);
	if (!ctx)
	{
		LOGE("Failed to initialise context");
		return 0;
	}

	doc = NULL;
	fz_try(ctx)
	{
		colorspace = fz_device_rgb;

		LOGE("Opening document...");
		fz_try(ctx)
		{
			doc = fz_open_document(ctx, (char *)filename);
		}
		fz_catch(ctx)
		{
			fz_throw(ctx, "Cannot open document: '%s'\n", filename);
		}
		LOGE("Done!");
		result = 1;
	}
	fz_catch(ctx)
	{
		LOGE("Failed: %s", ctx->error->message);
		fz_close_document(doc);
		doc = NULL;
		fz_free_context(ctx);
		ctx = NULL;
	}

	(*env)->ReleaseStringUTFChars(env, jfilename, filename);

	return result;
}

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_countPagesInternal(JNIEnv *env, jobject thiz)
{
	return fz_count_pages(doc);
}

JNIEXPORT void JNICALL
Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(JNIEnv *env, jobject thiz, int page)
{
	int i;
	int furthest;
	int furthest_dist = -1;
	float zoom;
	fz_matrix ctm;
	fz_bbox bbox;
	page_cache *pc;

	for (i = 0; i < NUM_CACHE; i++)
	{
		if (pages[i].page != NULL && pages[i].number == page)
		{
			/* The page is already cached */
			current = i;
			return;
		}

		if (pages[i].page == NULL)
		{
			/* cache record unused, and so a good one to use */
			furthest = i;
			furthest_dist = INT_MAX;
		}
		else
		{
			int dist = abs(pages[i].number - page);

			/* Further away - less likely to be needed again */
			if (dist > furthest_dist)
			{
				furthest_dist = dist;
				furthest = i;
			}
		}
	}

	current = furthest;
	pc = &pages[current];

	drop_page_cache(pc);

	/* In the event of an error, ensure we give a non-empty page */
	pc->width = 100;
	pc->height = 100;

	pc->number = page;
	LOGE("Goto page %d...", page);
	fz_try(ctx)
	{
		LOGI("Load page %d", pc->number);
		pc->page = fz_load_page(doc, pc->number);
		zoom = resolution / 72;
		pc->media_box = fz_bound_page(doc, pc->page);
		ctm = fz_scale(zoom, zoom);
		bbox = fz_round_rect(fz_transform_rect(ctm, pc->media_box));
		pc->width = bbox.x1-bbox.x0;
		pc->height = bbox.y1-bbox.y0;
	}
	fz_catch(ctx)
	{
		LOGE("cannot make displaylist from page %d", pc->number);
	}
}

JNIEXPORT float JNICALL
Java_com_artifex_mupdf_MuPDFCore_getPageWidth(JNIEnv *env, jobject thiz)
{
	LOGE("PageWidth=%g", pages[current].width);
	return pages[current].width;
}

JNIEXPORT float JNICALL
Java_com_artifex_mupdf_MuPDFCore_getPageHeight(JNIEnv *env, jobject thiz)
{
	LOGE("PageHeight=%g", pages[current].height);
	return pages[current].height;
}

JNIEXPORT jboolean JNICALL
Java_com_artifex_mupdf_MuPDFCore_javascriptSupported(JNIEnv *env, jobject thiz)
{
	return fz_javascript_supported();
}

JNIEXPORT jboolean JNICALL
Java_com_artifex_mupdf_MuPDFCore_drawPage(JNIEnv *env, jobject thiz, jobject bitmap,
		int pageW, int pageH, int patchX, int patchY, int patchW, int patchH)
{
	AndroidBitmapInfo info;
	void *pixels;
	int ret;
	fz_device *dev = NULL;
	float zoom;
	fz_matrix ctm;
	fz_bbox bbox;
	fz_pixmap *pix = NULL;
	float xscale, yscale;
	fz_bbox rect;
	page_cache *pc = &pages[current];
	int hq = (patchW < pageW || patchH < pageH);

	if (pc->page == NULL)
		return 0;

	fz_var(pix);
	fz_var(dev);

	LOGI("In native method\n");
	if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
		LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
		return 0;
	}

	LOGI("Checking format\n");
	if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
		LOGE("Bitmap format is not RGBA_8888 !");
		return 0;
	}

	LOGI("locking pixels\n");
	if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
		LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
		return 0;
	}

	/* Call mupdf to render display list to screen */
	LOGE("Rendering page(%d)=%dx%d patch=[%d,%d,%d,%d]",
			pc->number, pageW, pageH, patchX, patchY, patchW, patchH);

	fz_try(ctx)
	{
		fz_interactive *idoc = fz_interact(doc);

		// Call fz_update_page now to ensure future calls yield the
		// changes from the current state
		fz_update_page(idoc, pc->page);

		if (hq) {
			// This is a rendering of the hq patch. Ensure there's a second copy of the
			// page for use when updating this patch
			if (pc->hq_page) {
				if (idoc)
					fz_update_page(idoc, pc->hq_page);
			} else {
				// There is only ever one hq patch, so we need
				// cache only one page object for the sake of hq
				clear_hq_pages();
				pc->hq_page = fz_load_page(doc, pc->number);
			}
		}

		if (pc->page_list == NULL)
		{
			/* Render to list */
			pc->page_list = fz_new_display_list(ctx);
			dev = fz_new_list_device(ctx, pc->page_list);
			fz_run_page_contents(doc, pc->page, dev, fz_identity, NULL);
		}
		if (pc->annot_list == NULL)
		{
			fz_annot *annot;
			if (dev)
			{
				fz_free_device(dev);
				dev = NULL;
			}
			pc->annot_list = fz_new_display_list(ctx);
			dev = fz_new_list_device(ctx, pc->annot_list);
			for (annot = fz_first_annot(doc, pc->page); annot; annot = fz_next_annot(doc, annot))
				fz_run_annot(doc, pc->page, annot, dev, fz_identity, NULL);
		}
		rect.x0 = patchX;
		rect.y0 = patchY;
		rect.x1 = patchX + patchW;
		rect.y1 = patchY + patchH;
		pix = fz_new_pixmap_with_bbox_and_data(ctx, colorspace, rect, pixels);
		if (pc->page_list == NULL && pc->annot_list == NULL)
		{
			fz_clear_pixmap_with_value(ctx, pix, 0xd0);
			break;
		}
		fz_clear_pixmap_with_value(ctx, pix, 0xff);

		zoom = resolution / 72;
		ctm = fz_scale(zoom, zoom);
		bbox = fz_round_rect(fz_transform_rect(ctm, pc->media_box));
		/* Now, adjust ctm so that it would give the correct page width
		 * heights. */
		xscale = (float)pageW/(float)(bbox.x1-bbox.x0);
		yscale = (float)pageH/(float)(bbox.y1-bbox.y0);
		ctm = fz_concat(ctm, fz_scale(xscale, yscale));
		bbox = fz_round_rect(fz_transform_rect(ctm, pc->media_box));
		dev = fz_new_draw_device(ctx, pix);
#ifdef TIME_DISPLAY_LIST
		{
			clock_t time;
			int i;

			LOGE("Executing display list");
			time = clock();
			for (i=0; i<100;i++) {
#endif
				if (pc->page_list)
					fz_run_display_list(pc->page_list, dev, ctm, bbox, NULL);
				if (pc->annot_list)
					fz_run_display_list(pc->annot_list, dev, ctm, bbox, NULL);
#ifdef TIME_DISPLAY_LIST
			}
			time = clock() - time;
			LOGE("100 renders in %d (%d per sec)", time, CLOCKS_PER_SEC);
		}
#endif
		fz_free_device(dev);
		dev = NULL;
		fz_drop_pixmap(ctx, pix);
		LOGE("Rendered");
	}
	fz_catch(ctx)
	{
		fz_free_device(dev);
		LOGE("Render failed");
	}

	AndroidBitmap_unlockPixels(env, bitmap);

	return 1;
}

static char *widget_type_string(int t)
{
	switch(t)
	{
	case FZ_WIDGET_TYPE_PUSHBUTTON: return "pushbutton";
	case FZ_WIDGET_TYPE_CHECKBOX: return "checkbox";
	case FZ_WIDGET_TYPE_RADIOBUTTON: return "radiobutton";
	case FZ_WIDGET_TYPE_TEXT: return "text";
	case FZ_WIDGET_TYPE_LISTBOX: return "listbox";
	case FZ_WIDGET_TYPE_COMBOBOX: return "combobox";
	}
}
JNIEXPORT jboolean JNICALL
Java_com_artifex_mupdf_MuPDFCore_updatePageInternal(JNIEnv *env, jobject thiz, jobject bitmap, int page,
		int pageW, int pageH, int patchX, int patchY, int patchW, int patchH)
{
	AndroidBitmapInfo info;
	void *pixels;
	int ret;
	fz_device *dev = NULL;
	float zoom;
	fz_matrix ctm;
	fz_bbox bbox;
	fz_pixmap *pix = NULL;
	float xscale, yscale;
	fz_bbox rect;
	fz_interactive *idoc;
	page_cache *pc = NULL;
	int hq = (patchW < pageW || patchH < pageH);
	int i;

	for (i = 0; i < NUM_CACHE; i++)
	{
		if (pages[i].page != NULL && pages[i].number == page)
		{
			pc = &pages[i];
			break;
		}
	}

	if (pc == NULL || (hq && pc->hq_page == NULL))
	{
		Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(env, thiz, page);
		return Java_com_artifex_mupdf_MuPDFCore_drawPage(env, thiz, bitmap, pageW, pageH, patchX, patchY, patchW, patchH);
	}

	idoc = fz_interact(doc);

	fz_var(pix);
	fz_var(dev);

	LOGI("In native method\n");
	if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
		LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
		return 0;
	}

	LOGI("Checking format\n");
	if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
		LOGE("Bitmap format is not RGBA_8888 !");
		return 0;
	}

	LOGI("locking pixels\n");
	if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
		LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
		return 0;
	}

	/* Call mupdf to render display list to screen */
	LOGE("Rendering page(%d)=%dx%d patch=[%d,%d,%d,%d]",
			pc->number, pageW, pageH, patchX, patchY, patchW, patchH);

	fz_try(ctx)
	{
		fz_annot *annot;
		// Unimportant which page object we use for rendering but we
		// must use the correct one for calculating updates
		fz_page *page = hq ? pc->hq_page : pc->page;

		fz_update_page(idoc, page);

		if (pc->page_list == NULL)
		{
			/* Render to list */
			pc->page_list = fz_new_display_list(ctx);
			dev = fz_new_list_device(ctx, pc->page_list);
			fz_run_page_contents(doc, page, dev, fz_identity, NULL);
		}

		if (pc->annot_list == NULL) {
			if (dev) {
				fz_free_device(dev);
				dev = NULL;
			}
			pc->annot_list = fz_new_display_list(ctx);
			dev = fz_new_list_device(ctx, pc->annot_list);
			for (annot = fz_first_annot(doc, page); annot; annot = fz_next_annot(doc, annot))
				fz_run_annot(doc, page, annot, dev, fz_identity, NULL);
		}

		rect.x0 = patchX;
		rect.y0 = patchY;
		rect.x1 = patchX + patchW;
		rect.y1 = patchY + patchH;
		pix = fz_new_pixmap_with_bbox_and_data(ctx, colorspace, rect, pixels);

		zoom = resolution / 72;
		ctm = fz_scale(zoom, zoom);
		bbox = fz_round_rect(fz_transform_rect(ctm, pc->media_box));
		/* Now, adjust ctm so that it would give the correct page width
		 * heights. */
		xscale = (float)pageW/(float)(bbox.x1-bbox.x0);
		yscale = (float)pageH/(float)(bbox.y1-bbox.y0);
		ctm = fz_concat(ctm, fz_scale(xscale, yscale));
		bbox = fz_round_rect(fz_transform_rect(ctm, pc->media_box));

		LOGI("Start polling for updates");
		while ((annot = fz_poll_changed_annot(idoc, page)) != NULL)
		{
			fz_bbox abox = fz_round_rect(fz_transform_rect(ctm, fz_bound_annot(doc, annot)));
			abox = fz_intersect_bbox(abox, rect);

			LOGI("Update rectanglefor %s - (%d, %d, %d, %d)", widget_type_string(fz_widget_get_type((fz_widget*)annot)),
					abox.x0, abox.y0, abox.x1, abox.y1);
			if (!fz_is_empty_bbox(abox))
			{
				LOGI("And it isn't empty");
				fz_clear_pixmap_rect_with_value(ctx, pix, 0xff, abox);
				dev = fz_new_draw_device_with_bbox(ctx, pix, abox);
				if (pc->page_list)
					fz_run_display_list(pc->page_list, dev, ctm, abox, NULL);
				if (pc->annot_list)
					fz_run_display_list(pc->annot_list, dev, ctm, abox, NULL);
				fz_free_device(dev);
				dev = NULL;
			}
		}
		LOGI("Done polling for updates");

		LOGE("Rendered");
	}
	fz_catch(ctx)
	{
		fz_free_device(dev);
		LOGE("Render failed");
	}

	fz_drop_pixmap(ctx, pix);
	AndroidBitmap_unlockPixels(env, bitmap);

	return 1;
}

static fz_text_char textcharat(fz_text_page *page, int idx)
{
	static fz_text_char emptychar = { {0,0,0,0}, ' ' };
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int ofs = 0;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				if (idx < ofs + span->len)
					return span->text[idx - ofs];
				/* pseudo-newline */
				if (span + 1 == line->spans + line->len)
				{
					if (idx == ofs + span->len)
						return emptychar;
					ofs++;
				}
				ofs += span->len;
			}
		}
	}
	return emptychar;
}

static int
charat(fz_text_page *page, int idx)
{
	return textcharat(page, idx).c;
}

static fz_bbox
bboxcharat(fz_text_page *page, int idx)
{
	return fz_round_rect(textcharat(page, idx).bbox);
}

static int
textlen(fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int len = 0;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
				len += span->len;
			len++; /* pseudo-newline */
		}
	}
	return len;
}

static int
match(fz_text_page *page, const char *s, int n)
{
	int orig = n;
	int c;
	while (*s) {
		s += fz_chartorune(&c, (char *)s);
		if (c == ' ' && charat(page, n) == ' ') {
			while (charat(page, n) == ' ')
				n++;
		} else {
			if (tolower(c) != tolower(charat(page, n)))
				return 0;
			n++;
		}
	}
	return n - orig;
}

static int
countOutlineItems(fz_outline *outline)
{
	int count = 0;

	while (outline)
	{
		if (outline->dest.kind == FZ_LINK_GOTO
				&& outline->dest.ld.gotor.page >= 0
				&& outline->title)
			count++;

		count += countOutlineItems(outline->down);
		outline = outline->next;
	}

	return count;
}

static int
fillInOutlineItems(JNIEnv * env, jclass olClass, jmethodID ctor, jobjectArray arr, int pos, fz_outline *outline, int level)
{
	while (outline)
	{
		if (outline->dest.kind == FZ_LINK_GOTO)
		{
			int page = outline->dest.ld.gotor.page;
			if (page >= 0 && outline->title)
			{
				jobject ol;
				jstring title = (*env)->NewStringUTF(env, outline->title);
				if (title == NULL) return -1;
				ol = (*env)->NewObject(env, olClass, ctor, level, title, page);
				if (ol == NULL) return -1;
				(*env)->SetObjectArrayElement(env, arr, pos, ol);
				(*env)->DeleteLocalRef(env, ol);
				(*env)->DeleteLocalRef(env, title);
				pos++;
			}
		}
		pos = fillInOutlineItems(env, olClass, ctor, arr, pos, outline->down, level+1);
		if (pos < 0) return -1;
		outline = outline->next;
	}

	return pos;
}

JNIEXPORT jboolean JNICALL
Java_com_artifex_mupdf_MuPDFCore_needsPasswordInternal(JNIEnv * env, jobject thiz)
{
	return fz_needs_password(doc) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_artifex_mupdf_MuPDFCore_authenticatePasswordInternal(JNIEnv *env, jobject thiz, jstring password)
{
	const char *pw;
	int         result;
	pw = (*env)->GetStringUTFChars(env, password, NULL);
	if (pw == NULL)
		return JNI_FALSE;

	result = fz_authenticate_password(doc, (char *)pw);
	(*env)->ReleaseStringUTFChars(env, password, pw);
	return result;
}

JNIEXPORT jboolean JNICALL
Java_com_artifex_mupdf_MuPDFCore_hasOutlineInternal(JNIEnv * env, jobject thiz)
{
	fz_outline *outline = fz_load_outline(doc);
	return (outline == NULL) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_artifex_mupdf_MuPDFCore_getOutlineInternal(JNIEnv * env, jobject thiz)
{
	jclass        olClass;
	jmethodID     ctor;
	jobjectArray  arr;
	jobject       ol;
	fz_outline   *outline;
	int           nItems;

	olClass = (*env)->FindClass(env, "com/artifex/mupdf/OutlineItem");
	if (olClass == NULL) return NULL;
	ctor = (*env)->GetMethodID(env, olClass, "<init>", "(ILjava/lang/String;I)V");
	if (ctor == NULL) return NULL;

	outline = fz_load_outline(doc);
	nItems = countOutlineItems(outline);

	arr = (*env)->NewObjectArray(env,
					nItems,
					olClass,
					NULL);
	if (arr == NULL) return NULL;

	return fillInOutlineItems(env, olClass, ctor, arr, 0, outline, 0) > 0
			? arr
			:NULL;
}

JNIEXPORT jobjectArray JNICALL
Java_com_artifex_mupdf_MuPDFCore_searchPage(JNIEnv * env, jobject thiz, jstring jtext)
{
	jclass         rectClass;
	jmethodID      ctor;
	jobjectArray   arr;
	jobject        rect;
	fz_text_sheet *sheet = NULL;
	fz_text_page  *text = NULL;
	fz_device     *dev  = NULL;
	float          zoom;
	fz_matrix      ctm;
	int            pos;
	int            len;
	int            i, n;
	int            hit_count = 0;
	const char    *str;
	page_cache    *pc = &pages[current];

	rectClass = (*env)->FindClass(env, "android/graphics/RectF");
	if (rectClass == NULL) return NULL;
	ctor = (*env)->GetMethodID(env, rectClass, "<init>", "(FFFF)V");
	if (ctor == NULL) return NULL;
	str = (*env)->GetStringUTFChars(env, jtext, NULL);
	if (str == NULL) return NULL;

	fz_var(sheet);
	fz_var(text);
	fz_var(dev);

	fz_try(ctx)
	{
		fz_rect rect;

		if (hit_bbox == NULL)
			hit_bbox = fz_malloc_array(ctx, MAX_SEARCH_HITS, sizeof(*hit_bbox));

		zoom = resolution / 72;
		ctm = fz_scale(zoom, zoom);
		rect = fz_transform_rect(ctm, pc->media_box);
		sheet = fz_new_text_sheet(ctx);
		text = fz_new_text_page(ctx, rect);
		dev  = fz_new_text_device(ctx, sheet, text);
		fz_run_page(doc, pc->page, dev, ctm, NULL);
		fz_free_device(dev);
		dev = NULL;

		len = textlen(text);
		for (pos = 0; pos < len; pos++)
		{
			fz_bbox rr = fz_empty_bbox;
			n = match(text, str, pos);
			for (i = 0; i < n; i++)
				rr = fz_union_bbox(rr, bboxcharat(text, pos + i));

			if (!fz_is_empty_bbox(rr) && hit_count < MAX_SEARCH_HITS)
				hit_bbox[hit_count++] = rr;
		}
	}
	fz_always(ctx)
	{
		fz_free_text_page(ctx, text);
		fz_free_text_sheet(ctx, sheet);
		fz_free_device(dev);
	}
	fz_catch(ctx)
	{
		jclass cls;
		(*env)->ReleaseStringUTFChars(env, jtext, str);
		cls = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
		if (cls != NULL)
			(*env)->ThrowNew(env, cls, "Out of memory in MuPDFCore_searchPage");
		(*env)->DeleteLocalRef(env, cls);

		return NULL;
	}

	(*env)->ReleaseStringUTFChars(env, jtext, str);

	arr = (*env)->NewObjectArray(env,
					hit_count,
					rectClass,
					NULL);
	if (arr == NULL) return NULL;

	for (i = 0; i < hit_count; i++) {
		rect = (*env)->NewObject(env, rectClass, ctor,
				(float) (hit_bbox[i].x0),
				(float) (hit_bbox[i].y0),
				(float) (hit_bbox[i].x1),
				(float) (hit_bbox[i].y1));
		if (rect == NULL)
			return NULL;
		(*env)->SetObjectArrayElement(env, arr, i, rect);
		(*env)->DeleteLocalRef(env, rect);
	}

	return arr;
}

JNIEXPORT void JNICALL
Java_com_artifex_mupdf_MuPDFCore_destroying(JNIEnv * env, jobject thiz)
{
	int i;

	fz_free(ctx, hit_bbox);
	hit_bbox = NULL;

	for (i = 0; i < NUM_CACHE; i++)
		drop_page_cache(&pages[i]);

	fz_close_document(doc);
	doc = NULL;
}

JNIEXPORT jobjectArray JNICALL
Java_com_artifex_mupdf_MuPDFCore_getPageLinksInternal(JNIEnv * env, jobject thiz, int pageNumber)
{
	jclass       linkInfoClass;
	jmethodID    ctor;
	jobjectArray arr;
	jobject      linkInfo;
	fz_matrix    ctm;
	float        zoom;
	fz_link     *list;
	fz_link     *link;
	int          count;
	page_cache  *pc;

	linkInfoClass = (*env)->FindClass(env, "com/artifex/mupdf/LinkInfo");
	if (linkInfoClass == NULL) return NULL;
	ctor = (*env)->GetMethodID(env, linkInfoClass, "<init>", "(FFFFI)V");
	if (ctor == NULL) return NULL;

	Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(env, thiz, pageNumber);
	pc = &pages[current];
	if (pc->page == NULL || pc->number != pageNumber)
		return NULL;

	zoom = resolution / 72;
	ctm = fz_scale(zoom, zoom);

	list = fz_load_links(doc, pc->page);
	count = 0;
	for (link = list; link; link = link->next)
	{
		if (link->dest.kind == FZ_LINK_GOTO)
			count++ ;
	}

	arr = (*env)->NewObjectArray(env, count, linkInfoClass, NULL);
	if (arr == NULL) return NULL;

	count = 0;
	for (link = list; link; link = link->next)
	{
		if (link->dest.kind == FZ_LINK_GOTO)
		{
			fz_rect rect = fz_transform_rect(ctm, link->rect);

			linkInfo = (*env)->NewObject(env, linkInfoClass, ctor,
					(float)rect.x0, (float)rect.y0, (float)rect.x1, (float)rect.y1,
					link->dest.ld.gotor.page);
			if (linkInfo == NULL) return NULL;
			(*env)->SetObjectArrayElement(env, arr, count, linkInfo);
			(*env)->DeleteLocalRef(env, linkInfo);

			count ++;
		}
	}

	return arr;
}

JNIEXPORT jobjectArray JNICALL
Java_com_artifex_mupdf_MuPDFCore_getWidgetAreasInternal(JNIEnv * env, jobject thiz, int pageNumber)
{
	jclass       rectFClass;
	jmethodID    ctor;
	jobjectArray arr;
	jobject      rectF;
	fz_interactive *idoc;
	fz_widget *widget;
	fz_matrix    ctm;
	float        zoom;
	int          count;
	page_cache  *pc;

	rectFClass = (*env)->FindClass(env, "android/graphics/RectF");
	if (rectFClass == NULL) return NULL;
	ctor = (*env)->GetMethodID(env, rectFClass, "<init>", "(FFFF)V");
	if (ctor == NULL) return NULL;

	Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(env, thiz, pageNumber);
	pc = &pages[current];
	if (pc->number != pageNumber || pc->page == NULL)
		return NULL;

	idoc = fz_interact(doc);
	if (idoc == NULL)
		return NULL;

	zoom = resolution / 72;
	ctm = fz_scale(zoom, zoom);

	count = 0;
	for (widget = fz_first_widget(idoc, pc->page); widget; widget = fz_next_widget(idoc, widget))
		count ++;

	arr = (*env)->NewObjectArray(env, count, rectFClass, NULL);
	if (arr == NULL) return NULL;

	count = 0;
	for (widget = fz_first_widget(idoc, pc->page); widget; widget = fz_next_widget(idoc, widget))
	{
		fz_rect rect = fz_transform_rect(ctm, *fz_widget_bbox(widget));

		rectF = (*env)->NewObject(env, rectFClass, ctor,
				(float)rect.x0, (float)rect.y0, (float)rect.x1, (float)rect.y1);
		if (rectF == NULL) return NULL;
		(*env)->SetObjectArrayElement(env, arr, count, rectF);
		(*env)->DeleteLocalRef(env, rectF);

		count ++;
	}

	return arr;
}

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_getPageLink(JNIEnv * env, jobject thiz, int pageNumber, float x, float y)
{
	fz_matrix ctm;
	float zoom;
	fz_link *link;
	fz_point p;
	page_cache *pc;

	Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(env, thiz, pageNumber);
	pc = &pages[current];
	if (pc->number != pageNumber || pc->page == NULL)
		return -1;

	p.x = x;
	p.y = y;

	/* Ultimately we should probably return a pointer to a java structure
	 * with the link details in, but for now, page number will suffice.
	 */
	zoom = resolution / 72;
	ctm = fz_scale(zoom, zoom);
	ctm = fz_invert_matrix(ctm);

	p = fz_transform_point(ctm, p);

	for (link = fz_load_links(doc, pc->page); link; link = link->next)
	{
		if (p.x >= link->rect.x0 && p.x <= link->rect.x1)
			if (p.y >= link->rect.y0 && p.y <= link->rect.y1)
				break;
	}

	if (link == NULL)
		return -1;

	if (link->dest.kind == FZ_LINK_URI)
	{
		//gotouri(link->dest.ld.uri.uri);
		return -1;
	}
	else if (link->dest.kind == FZ_LINK_GOTO)
		return link->dest.ld.gotor.page;
	return -1;
}

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_passClickEventInternal(JNIEnv * env, jobject thiz, int pageNumber, float x, float y)
{
	fz_matrix ctm;
	fz_interactive *idoc = fz_interact(doc);
	float zoom;
	fz_point p;
	fz_ui_event event;
	int changed = 0;
	page_cache *pc;

	if (idoc == NULL)
		return 0;

	Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(env, thiz, pageNumber);
	pc = &pages[current];
	if (pc->number != pageNumber || pc->page == NULL)
		return 0;

	p.x = x;
	p.y = y;

	/* Ultimately we should probably return a pointer to a java structure
	 * with the link details in, but for now, page number will suffice.
	 */
	zoom = resolution / 72;
	ctm = fz_scale(zoom, zoom);
	ctm = fz_invert_matrix(ctm);

	p = fz_transform_point(ctm, p);

	fz_try(ctx)
	{
		event.etype = FZ_EVENT_TYPE_POINTER;
		event.event.pointer.pt = p;
		event.event.pointer.ptype = FZ_POINTER_DOWN;
		changed = fz_pass_event(idoc, pc->page, &event);
		event.event.pointer.ptype = FZ_POINTER_UP;
		changed |= fz_pass_event(idoc, pc->page, &event);
		if (changed) {
			dump_annotation_display_lists();
		}
	}
	fz_catch(ctx)
	{
		LOGE("passClickEvent: %s", ctx->error->message);
	}

	return changed;
}

JNIEXPORT jstring JNICALL
Java_com_artifex_mupdf_MuPDFCore_getFocusedWidgetTextInternal(JNIEnv * env, jobject thiz)
{
	char *text = "";

	fz_try(ctx)
	{
		fz_interactive *idoc = fz_interact(doc);

		if (idoc)
		{
			fz_widget *focus = fz_focused_widget(idoc);

			if (focus)
				text = fz_text_widget_text(idoc, focus);
		}
	}
	fz_catch(ctx)
	{
		LOGE("getFocusedWidgetText failed: %s", ctx->error->message);
	}

	return (*env)->NewStringUTF(env, text);
}

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_setFocusedWidgetTextInternal(JNIEnv * env, jobject thiz, jstring jtext)
{
	const char *text;
	int result = 0;

	text = (*env)->GetStringUTFChars(env, jtext, NULL);
	if (text == NULL)
	{
		LOGE("Failed to get text");
		return 0;
	}

	fz_try(ctx)
	{
		fz_interactive *idoc = fz_interact(doc);

		if (idoc)
		{
			fz_widget *focus = fz_focused_widget(idoc);

			if (focus)
			{
				result = fz_text_widget_set_text(idoc, focus, text);
				dump_annotation_display_lists();
			}
		}
	}
	fz_catch(ctx)
	{
		LOGE("setFocusedWidgetText failed: %s", ctx->error->message);
	}

	(*env)->ReleaseStringUTFChars(env, jtext, text);

	return result;
}

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_getFocusedWidgetTypeInternal(JNIEnv * env, jobject thiz)
{
	fz_interactive *idoc = fz_interact(doc);
	fz_widget *focus;

	if (idoc == NULL)
		return NONE;

	focus = fz_focused_widget(idoc);

	if (focus == NULL)
		return NONE;

	switch (fz_widget_get_type(focus))
	{
	case FZ_WIDGET_TYPE_TEXT: return TEXT;
	case FZ_WIDGET_TYPE_LISTBOX: return LISTBOX;
	case FZ_WIDGET_TYPE_COMBOBOX: return COMBOBOX;
	}

	return NONE;
}
