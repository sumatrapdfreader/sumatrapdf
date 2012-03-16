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

/* Globals */
fz_colorspace *colorspace;
fz_document *doc;
int pagenum = 1;
int resolution = 160;
float pageWidth = 100;
float pageHeight = 100;
fz_display_list *currentPageList;
fz_rect currentMediabox;
fz_context *ctx;
int currentPageNumber = -1;
fz_page *currentPage = NULL;
fz_bbox *hit_bbox = NULL;

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_openFile(JNIEnv * env, jobject thiz, jstring jfilename)
{
	const char *filename;
	int pages = 0;
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
	float zoom;
	fz_matrix ctm;
	fz_bbox bbox;
	fz_device *dev = NULL;

	fz_var(dev);

	if (currentPage != NULL && page != currentPageNumber)
	{
		fz_free_page(doc, currentPage);
		currentPage = NULL;
	}

	/* In the event of an error, ensure we give a non-empty page */
	pageWidth = 100;
	pageHeight = 100;

	currentPageNumber = page;
	LOGE("Goto page %d...", page);
	fz_try(ctx)
	{
		if (currentPageList != NULL)
		{
			fz_free_display_list(ctx, currentPageList);
			currentPageList = NULL;
		}
		pagenum = page;
		currentPage = fz_load_page(doc, pagenum);
		zoom = resolution / 72;
		currentMediabox = fz_bound_page(doc, currentPage);
		ctm = fz_scale(zoom, zoom);
		bbox = fz_round_rect(fz_transform_rect(ctm, currentMediabox));
		pageWidth = bbox.x1-bbox.x0;
		pageHeight = bbox.y1-bbox.y0;
	}
	fz_catch(ctx)
	{
		currentPageNumber = page;
		LOGE("cannot make displaylist from page %d", pagenum);
	}
	fz_free_device(dev);
	dev = NULL;
}

JNIEXPORT float JNICALL
Java_com_artifex_mupdf_MuPDFCore_getPageWidth(JNIEnv *env, jobject thiz)
{
	LOGE("PageWidth=%g", pageWidth);
	return pageWidth;
}

JNIEXPORT float JNICALL
Java_com_artifex_mupdf_MuPDFCore_getPageHeight(JNIEnv *env, jobject thiz)
{
	LOGE("PageHeight=%g", pageHeight);
	return pageHeight;
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
	LOGE("Rendering page=%dx%d patch=[%d,%d,%d,%d]",
			pageW, pageH, patchX, patchY, patchW, patchH);

	fz_try(ctx)
	{
		if (currentPageList == NULL)
		{
			/* Render to list */
			currentPageList = fz_new_display_list(ctx);
			dev = fz_new_list_device(ctx, currentPageList);
			fz_run_page(doc, currentPage, dev, fz_identity, NULL);
		}
		rect.x0 = patchX;
		rect.y0 = patchY;
		rect.x1 = patchX + patchW;
		rect.y1 = patchY + patchH;
		pix = fz_new_pixmap_with_bbox_and_data(ctx, colorspace, rect, pixels);
		if (currentPageList == NULL)
		{
			fz_clear_pixmap_with_value(ctx, pix, 0xd0);
			break;
		}
		fz_clear_pixmap_with_value(ctx, pix, 0xff);

		zoom = resolution / 72;
		ctm = fz_scale(zoom, zoom);
		bbox = fz_round_rect(fz_transform_rect(ctm,currentMediabox));
		/* Now, adjust ctm so that it would give the correct page width
		 * heights. */
		xscale = (float)pageW/(float)(bbox.x1-bbox.x0);
		yscale = (float)pageH/(float)(bbox.y1-bbox.y0);
		ctm = fz_concat(ctm, fz_scale(xscale, yscale));
		bbox = fz_round_rect(fz_transform_rect(ctm,currentMediabox));
		dev = fz_new_draw_device(ctx, pix);
#ifdef TIME_DISPLAY_LIST
		{
			clock_t time;
			int i;

			LOGE("Executing display list");
			time = clock();
			for (i=0; i<100;i++) {
#endif
				fz_run_display_list(currentPageList, dev, ctm, bbox, NULL);
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
		rect = fz_transform_rect(ctm, currentMediabox);
		sheet = fz_new_text_sheet(ctx);
		text = fz_new_text_page(ctx, rect);
		dev  = fz_new_text_device(ctx, sheet, text);
		fz_run_page(doc, currentPage, dev, ctm, NULL);
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
	fz_free(ctx, hit_bbox);
	hit_bbox = NULL;
	fz_free_display_list(ctx, currentPageList);
	currentPageList = NULL;
	if (currentPage != NULL)
	{
		fz_free_page(doc, currentPage);
		currentPage = NULL;
	}
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

	linkInfoClass = (*env)->FindClass(env, "com/artifex/mupdf/LinkInfo");
	if (linkInfoClass == NULL) return NULL;
	ctor = (*env)->GetMethodID(env, linkInfoClass, "<init>", "(FFFFI)V");
	if (ctor == NULL) return NULL;

	Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(env, thiz, pageNumber);
	if (currentPageNumber == -1 || currentPage == NULL)
		return NULL;

	zoom = resolution / 72;
	ctm = fz_scale(zoom, zoom);

	list = fz_load_links(doc, currentPage);
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

JNIEXPORT int JNICALL
Java_com_artifex_mupdf_MuPDFCore_getPageLink(JNIEnv * env, jobject thiz, int pageNumber, float x, float y)
{
	fz_matrix ctm;
	float zoom;
	fz_link *link;
	fz_point p;

	Java_com_artifex_mupdf_MuPDFCore_gotoPageInternal(env, thiz, pageNumber);
	if (currentPageNumber == -1 || currentPage == NULL)
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

	for (link = fz_load_links(doc, currentPage); link; link = link->next)
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
