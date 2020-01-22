#include <jni.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"
#include "mupdf/pdf.h"

#include "mupdf_native.h" /* javah generated prototypes */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_ANDROID
#include <android/log.h>
#include <android/bitmap.h>
#define LOG_TAG "libmupdf"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGT(...) __android_log_print(ANDROID_LOG_INFO,"alert",__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#undef LOGI
#undef LOGE
#define LOGI(...) do{printf(__VA_ARGS__);putchar('\n');}while(0)
#define LOGE(...) do{printf(__VA_ARGS__);putchar('\n');}while(0)
#endif

#define MY_JNI_VERSION JNI_VERSION_1_6

#define FUN(A) Java_com_artifex_mupdf_fitz_ ## A
#define PKG "com/artifex/mupdf/fitz/"

/* Do our best to avoid type casting warnings. */

#define CAST(type, var) (type)pointer_cast(var)

static inline void *pointer_cast(jlong l)
{
	return (void *)(intptr_t)l;
}

static inline jlong jlong_cast(const void *p)
{
	return (jlong)(intptr_t)p;
}

/* Our vm */
static JavaVM *jvm = NULL;

/* All the cached classes/mids/fids we need. */

static jclass cls_Buffer;
static jclass cls_ColorSpace;
static jclass cls_Cookie;
static jclass cls_Device;
static jclass cls_DisplayList;
static jclass cls_Document;
static jclass cls_DocumentWriter;
static jclass cls_FloatArray;
static jclass cls_Font;
static jclass cls_IOException;
static jclass cls_IllegalArgumentException;
static jclass cls_Image;
static jclass cls_IndexOutOfBoundsException;
static jclass cls_IntegerArray;
static jclass cls_Link;
static jclass cls_Location;
static jclass cls_Matrix;
static jclass cls_NativeDevice;
static jclass cls_NullPointerException;
static jclass cls_Object;
static jclass cls_OutOfMemoryError;
static jclass cls_Outline;
static jclass cls_PDFAnnotation;
static jclass cls_PDFDocument;
static jclass cls_PDFDocument_JsEventListener;
static jclass cls_PDFGraftMap;
static jclass cls_PDFObject;
static jclass cls_PDFPage;
static jclass cls_Page;
static jclass cls_Path;
static jclass cls_PathWalker;
static jclass cls_Pixmap;
static jclass cls_Point;
static jclass cls_Quad;
static jclass cls_Rect;
static jclass cls_RuntimeException;
static jclass cls_SeekableInputStream;
static jclass cls_SeekableOutputStream;
static jclass cls_SeekableStream;
static jclass cls_Shade;
static jclass cls_String;
static jclass cls_StrokeState;
static jclass cls_StructuredText;
static jclass cls_StructuredTextWalker;
static jclass cls_Text;
static jclass cls_TextBlock;
static jclass cls_TextChar;
static jclass cls_TextLine;
static jclass cls_TextWalker;
static jclass cls_TryLaterException;
static jclass cls_PDFWidget;

static jfieldID fid_Buffer_pointer;
static jfieldID fid_ColorSpace_pointer;
static jfieldID fid_Cookie_pointer;
static jfieldID fid_Device_pointer;
static jfieldID fid_DisplayList_pointer;
static jfieldID fid_DocumentWriter_pointer;
static jfieldID fid_Document_pointer;
static jfieldID fid_Font_pointer;
static jfieldID fid_Image_pointer;
static jfieldID fid_Matrix_a;
static jfieldID fid_Matrix_b;
static jfieldID fid_Matrix_c;
static jfieldID fid_Matrix_d;
static jfieldID fid_Matrix_e;
static jfieldID fid_Matrix_f;
static jfieldID fid_NativeDevice_nativeInfo;
static jfieldID fid_NativeDevice_nativeResource;
static jfieldID fid_PDFAnnotation_pointer;
static jfieldID fid_PDFDocument_pointer;
static jfieldID fid_PDFGraftMap_pointer;
static jfieldID fid_PDFObject_Null;
static jfieldID fid_PDFObject_pointer;
static jfieldID fid_PDFPage_pointer;
static jfieldID fid_Page_pointer;
static jfieldID fid_Path_pointer;
static jfieldID fid_Pixmap_pointer;
static jfieldID fid_Point_x;
static jfieldID fid_Point_y;
static jfieldID fid_Quad_ul_x;
static jfieldID fid_Quad_ul_y;
static jfieldID fid_Quad_ur_x;
static jfieldID fid_Quad_ur_y;
static jfieldID fid_Quad_ll_x;
static jfieldID fid_Quad_ll_y;
static jfieldID fid_Quad_lr_x;
static jfieldID fid_Quad_lr_y;
static jfieldID fid_Rect_x0;
static jfieldID fid_Rect_x1;
static jfieldID fid_Rect_y0;
static jfieldID fid_Rect_y1;
static jfieldID fid_Shade_pointer;
static jfieldID fid_StrokeState_pointer;
static jfieldID fid_StructuredText_pointer;
static jfieldID fid_TextBlock_bbox;
static jfieldID fid_TextBlock_lines;
static jfieldID fid_TextChar_quad;
static jfieldID fid_TextChar_c;
static jfieldID fid_TextLine_bbox;
static jfieldID fid_TextLine_chars;
static jfieldID fid_Text_pointer;
static jfieldID fid_PDFWidget_fieldFlags;
static jfieldID fid_PDFWidget_fieldType;
static jfieldID fid_PDFWidget_maxLen;
static jfieldID fid_PDFWidget_options;
static jfieldID fid_PDFWidget_pointer;
static jfieldID fid_PDFWidget_textFormat;

static jmethodID mid_ColorSpace_fromPointer;
static jmethodID mid_ColorSpace_init;
static jmethodID mid_Device_beginGroup;
static jmethodID mid_Device_beginLayer;
static jmethodID mid_Device_beginMask;
static jmethodID mid_Device_beginTile;
static jmethodID mid_Device_clipImageMask;
static jmethodID mid_Device_clipPath;
static jmethodID mid_Device_clipStrokePath;
static jmethodID mid_Device_clipStrokeText;
static jmethodID mid_Device_clipText;
static jmethodID mid_Device_endGroup;
static jmethodID mid_Device_endLayer;
static jmethodID mid_Device_endMask;
static jmethodID mid_Device_endTile;
static jmethodID mid_Device_fillImage;
static jmethodID mid_Device_fillImageMask;
static jmethodID mid_Device_fillPath;
static jmethodID mid_Device_fillShade;
static jmethodID mid_Device_fillText;
static jmethodID mid_Device_ignoreText;
static jmethodID mid_Device_init;
static jmethodID mid_Device_popClip;
static jmethodID mid_Device_strokePath;
static jmethodID mid_Device_strokeText;
static jmethodID mid_DisplayList_init;
static jmethodID mid_Document_init;
static jmethodID mid_Font_init;
static jmethodID mid_Image_init;
static jmethodID mid_Link_init;
static jmethodID mid_Location_init;
static jmethodID mid_Matrix_init;
static jmethodID mid_Object_toString;
static jmethodID mid_Outline_init;
static jmethodID mid_PDFAnnotation_init;
static jmethodID mid_PDFDocument_init;
static jmethodID mid_PDFDocument_JsEventListener_onAlert;
static jmethodID mid_PDFGraftMap_init;
static jmethodID mid_PDFObject_init;
static jmethodID mid_PDFPage_init;
static jmethodID mid_Page_init;
static jmethodID mid_PathWalker_closePath;
static jmethodID mid_PathWalker_curveTo;
static jmethodID mid_PathWalker_lineTo;
static jmethodID mid_PathWalker_moveTo;
static jmethodID mid_Path_init;
static jmethodID mid_Pixmap_init;
static jmethodID mid_Point_init;
static jmethodID mid_Quad_init;
static jmethodID mid_Rect_init;
static jmethodID mid_SeekableInputStream_read;
static jmethodID mid_SeekableOutputStream_write;
static jmethodID mid_SeekableStream_position;
static jmethodID mid_SeekableStream_seek;
static jmethodID mid_Shade_init;
static jmethodID mid_StrokeState_init;
static jmethodID mid_StructuredText_init;
static jmethodID mid_StructuredTextWalker_onImageBlock;
static jmethodID mid_StructuredTextWalker_beginTextBlock;
static jmethodID mid_StructuredTextWalker_endTextBlock;
static jmethodID mid_StructuredTextWalker_beginLine;
static jmethodID mid_StructuredTextWalker_endLine;
static jmethodID mid_StructuredTextWalker_onChar;
static jmethodID mid_TextBlock_init;
static jmethodID mid_TextChar_init;
static jmethodID mid_TextLine_init;
static jmethodID mid_TextWalker_showGlyph;
static jmethodID mid_Text_init;
static jmethodID mid_PDFWidget_init;

#ifdef _WIN32
static DWORD context_key;
#else
static pthread_key_t context_key;
#endif
static fz_context *base_context;

static int check_enums()
{
	int valid = 1;

	valid &= com_artifex_mupdf_fitz_Device_BLEND_NORMAL == FZ_BLEND_NORMAL;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_MULTIPLY == FZ_BLEND_MULTIPLY;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_SCREEN == FZ_BLEND_SCREEN;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_OVERLAY == FZ_BLEND_OVERLAY;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_DARKEN == FZ_BLEND_DARKEN;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_LIGHTEN == FZ_BLEND_LIGHTEN;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_COLOR_DODGE == FZ_BLEND_COLOR_DODGE;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_COLOR_BURN == FZ_BLEND_COLOR_BURN;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_HARD_LIGHT == FZ_BLEND_HARD_LIGHT;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_SOFT_LIGHT == FZ_BLEND_SOFT_LIGHT;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_DIFFERENCE == FZ_BLEND_DIFFERENCE;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_EXCLUSION == FZ_BLEND_EXCLUSION;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_HUE == FZ_BLEND_HUE;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_SATURATION == FZ_BLEND_SATURATION;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_COLOR == FZ_BLEND_COLOR;
	valid &= com_artifex_mupdf_fitz_Device_BLEND_LUMINOSITY == FZ_BLEND_LUMINOSITY;

	valid &= com_artifex_mupdf_fitz_Font_LATIN == PDF_SIMPLE_ENCODING_LATIN;
	valid &= com_artifex_mupdf_fitz_Font_GREEK == PDF_SIMPLE_ENCODING_GREEK;
	valid &= com_artifex_mupdf_fitz_Font_CYRILLIC == PDF_SIMPLE_ENCODING_CYRILLIC;

	valid &= com_artifex_mupdf_fitz_Font_ADOBE_CNS == FZ_ADOBE_CNS;
	valid &= com_artifex_mupdf_fitz_Font_ADOBE_GB == FZ_ADOBE_GB;
	valid &= com_artifex_mupdf_fitz_Font_ADOBE_JAPAN == FZ_ADOBE_JAPAN;
	valid &= com_artifex_mupdf_fitz_Font_ADOBE_KOREA == FZ_ADOBE_KOREA;

	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_NONE == PDF_ANNOT_LE_NONE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_SQUARE == PDF_ANNOT_LE_SQUARE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_CIRCLE == PDF_ANNOT_LE_CIRCLE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_DIAMOND == PDF_ANNOT_LE_DIAMOND;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_OPEN_ARROW == PDF_ANNOT_LE_OPEN_ARROW;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_CLOSED_ARROW == PDF_ANNOT_LE_CLOSED_ARROW;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_BUTT == PDF_ANNOT_LE_BUTT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_R_OPEN_ARROW == PDF_ANNOT_LE_R_OPEN_ARROW;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_R_CLOSED_ARROW == PDF_ANNOT_LE_R_CLOSED_ARROW;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_LINE_ENDING_SLASH == PDF_ANNOT_LE_SLASH;

	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_TEXT == PDF_ANNOT_TEXT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_LINK == PDF_ANNOT_LINK;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_FREE_TEXT == PDF_ANNOT_FREE_TEXT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_LINE == PDF_ANNOT_LINE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_SQUARE == PDF_ANNOT_SQUARE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_CIRCLE == PDF_ANNOT_CIRCLE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_POLYGON == PDF_ANNOT_POLYGON;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_POLY_LINE == PDF_ANNOT_POLY_LINE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_HIGHLIGHT == PDF_ANNOT_HIGHLIGHT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_UNDERLINE == PDF_ANNOT_UNDERLINE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_SQUIGGLY == PDF_ANNOT_SQUIGGLY;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_STRIKE_OUT == PDF_ANNOT_STRIKE_OUT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_REDACT == PDF_ANNOT_REDACT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_STAMP == PDF_ANNOT_STAMP;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_CARET == PDF_ANNOT_CARET;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_INK == PDF_ANNOT_INK;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_POPUP == PDF_ANNOT_POPUP;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_FILE_ATTACHMENT == PDF_ANNOT_FILE_ATTACHMENT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_SOUND == PDF_ANNOT_SOUND;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_MOVIE == PDF_ANNOT_MOVIE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_WIDGET == PDF_ANNOT_WIDGET;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_SCREEN == PDF_ANNOT_SCREEN;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_PRINTER_MARK == PDF_ANNOT_PRINTER_MARK;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_TRAP_NET == PDF_ANNOT_TRAP_NET;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_WATERMARK == PDF_ANNOT_WATERMARK;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_3D == PDF_ANNOT_3D;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_UNKNOWN == PDF_ANNOT_UNKNOWN;

	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_INVISIBLE == PDF_ANNOT_IS_INVISIBLE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_HIDDEN == PDF_ANNOT_IS_HIDDEN;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_PRINT == PDF_ANNOT_IS_PRINT;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_NO_ZOOM == PDF_ANNOT_IS_NO_ZOOM;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_NO_ROTATE == PDF_ANNOT_IS_NO_ROTATE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_NO_VIEW == PDF_ANNOT_IS_NO_VIEW;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_READ_ONLY == PDF_ANNOT_IS_READ_ONLY;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_LOCKED == PDF_ANNOT_IS_LOCKED;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_TOGGLE_NO_VIEW == PDF_ANNOT_IS_TOGGLE_NO_VIEW;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_IS_LOCKED_CONTENTS == PDF_ANNOT_IS_LOCKED_CONTENTS;

	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_CAP_BUTT == FZ_LINECAP_BUTT;
	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_CAP_ROUND == FZ_LINECAP_ROUND;
	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_CAP_SQUARE == FZ_LINECAP_SQUARE;
	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_CAP_TRIANGLE == FZ_LINECAP_TRIANGLE;

	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_JOIN_MITER == FZ_LINEJOIN_MITER;
	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_JOIN_ROUND == FZ_LINEJOIN_ROUND;
	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_JOIN_BEVEL == FZ_LINEJOIN_BEVEL;
	valid &= com_artifex_mupdf_fitz_StrokeState_LINE_JOIN_MITER_XPS == FZ_LINEJOIN_MITER_XPS;

	valid &= com_artifex_mupdf_fitz_StructuredText_SELECT_CHARS == FZ_SELECT_CHARS;
	valid &= com_artifex_mupdf_fitz_StructuredText_SELECT_WORDS == FZ_SELECT_WORDS;
	valid &= com_artifex_mupdf_fitz_StructuredText_SELECT_LINES == FZ_SELECT_LINES;

	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_UNKNOWN == PDF_WIDGET_TYPE_UNKNOWN;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_BUTTON == PDF_WIDGET_TYPE_BUTTON;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_CHECKBOX == PDF_WIDGET_TYPE_CHECKBOX;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_COMBOBOX == PDF_WIDGET_TYPE_COMBOBOX;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_LISTBOX == PDF_WIDGET_TYPE_LISTBOX;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_RADIOBUTTON == PDF_WIDGET_TYPE_RADIOBUTTON;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_SIGNATURE == PDF_WIDGET_TYPE_SIGNATURE;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TYPE_TEXT == PDF_WIDGET_TYPE_TEXT;

	valid &= com_artifex_mupdf_fitz_PDFWidget_TX_FORMAT_NONE == PDF_WIDGET_TX_FORMAT_NONE;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TX_FORMAT_NUMBER == PDF_WIDGET_TX_FORMAT_NUMBER;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TX_FORMAT_SPECIAL == PDF_WIDGET_TX_FORMAT_SPECIAL;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TX_FORMAT_DATE == PDF_WIDGET_TX_FORMAT_DATE;
	valid &= com_artifex_mupdf_fitz_PDFWidget_TX_FORMAT_TIME == PDF_WIDGET_TX_FORMAT_TIME;

	return valid ? 1 : 0;
}

/* Helper functions to set the java exception flag. */

static void jni_throw(JNIEnv *env, int type, const char *mess)
{
	if (type == FZ_ERROR_TRYLATER)
		(*env)->ThrowNew(env, cls_TryLaterException, mess);
	else
		(*env)->ThrowNew(env, cls_RuntimeException, mess);
}

static void jni_rethrow(JNIEnv *env, fz_context *ctx)
{
	jni_throw(env, fz_caught(ctx), fz_caught_message(ctx));
}

static void jni_throw_run(JNIEnv *env, const char *info)
{
	(*env)->ThrowNew(env, cls_RuntimeException, info);
}

static void jni_throw_oom(JNIEnv *env, const char *info)
{
	(*env)->ThrowNew(env, cls_OutOfMemoryError, info);
}

static void jni_throw_oob(JNIEnv *env, const char *info)
{
	(*env)->ThrowNew(env, cls_IndexOutOfBoundsException, info);
}

static void jni_throw_arg(JNIEnv *env, const char *info)
{
	(*env)->ThrowNew(env, cls_IllegalArgumentException, info);
}

static void jni_throw_io(JNIEnv *env, const char *info)
{
	(*env)->ThrowNew(env, cls_IOException, info);
}

static void jni_throw_null(JNIEnv *env, const char *info)
{
	(*env)->ThrowNew(env, cls_NullPointerException, info);
}

/* Convert a java exception and throw into fitz. */

static void fz_throw_java(fz_context *ctx, JNIEnv *env)
{
	jthrowable ex = (*env)->ExceptionOccurred(env);
	if (ex)
	{
		jobject msg;
		(*env)->ExceptionClear(env);
		msg = (*env)->CallObjectMethod(env, ex, mid_Object_toString);
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionClear(env);
		else if (msg)
		{
			const char *p = (*env)->GetStringUTFChars(env, msg, NULL);
			if (p)
			{
				char buf[256];
				fz_strlcpy(buf, p, sizeof buf);
				(*env)->ReleaseStringUTFChars(env, msg, p);
				fz_throw(ctx, FZ_ERROR_GENERIC, "%s", buf);
			}
		}
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown java error");
}

/* Load classes, field and method IDs. */

static const char *current_class_name = NULL;
static jclass current_class = NULL;

static jclass get_class(int *failed, JNIEnv *env, const char *name)
{
	jclass local;

	if (*failed) return NULL;

	current_class_name = name;
	local = (*env)->FindClass(env, name);
	if (!local)
	{
		LOGI("Failed to find class %s", name);
		*failed = 1;
		return NULL;
	}

	current_class = (*env)->NewGlobalRef(env, local);
	if (!current_class)
	{
		LOGI("Failed to make global ref for %s", name);
		*failed = 1;
		return NULL;
	}

	(*env)->DeleteLocalRef(env, local);

	return current_class;
}

static jfieldID get_field(int *failed, JNIEnv *env, const char *field, const char *sig)
{
	jfieldID fid;

	if (*failed || !current_class) return NULL;

	fid = (*env)->GetFieldID(env, current_class, field, sig);
	if (fid == 0)
	{
		LOGI("Failed to get field for %s %s %s", current_class_name, field, sig);
		*failed = 1;
	}

	return fid;
}

static jfieldID get_static_field(int *failed, JNIEnv *env, const char *field, const char *sig)
{
	jfieldID fid;

	if (*failed || !current_class) return NULL;

	fid = (*env)->GetStaticFieldID(env, current_class, field, sig);
	if (fid == 0)
	{
		LOGI("Failed to get static field for %s %s %s", current_class_name, field, sig);
		*failed = 1;
	}

	return fid;
}

static jmethodID get_method(int *failed, JNIEnv *env, const char *method, const char *sig)
{
	jmethodID mid;

	if (*failed || !current_class) return NULL;

	mid = (*env)->GetMethodID(env, current_class, method, sig);
	if (mid == 0)
	{
		LOGI("Failed to get method for %s %s %s", current_class_name, method, sig);
		*failed = 1;
	}

	return mid;
}

static jmethodID get_static_method(int *failed, JNIEnv *env, const char *method, const char *sig)
{
	jmethodID mid;

	if (*failed || !current_class) return NULL;

	mid = (*env)->GetStaticMethodID(env, current_class, method, sig);
	if (mid == 0)
	{
		LOGI("Failed to get static method for %s %s %s", current_class_name, method, sig);
		*failed = 1;
	}

	return mid;
}

static int find_fids(JNIEnv *env)
{
	int err = 0;
	int getvmErr;

	cls_Buffer = get_class(&err, env, PKG"Buffer");
	fid_Buffer_pointer = get_field(&err, env, "pointer", "J");

	cls_ColorSpace = get_class(&err, env, PKG"ColorSpace");
	fid_ColorSpace_pointer = get_field(&err, env, "pointer", "J");
	mid_ColorSpace_init = get_method(&err, env, "<init>", "(J)V");
	mid_ColorSpace_fromPointer = get_static_method(&err, env, "fromPointer", "(J)L"PKG"ColorSpace;");

	cls_Cookie = get_class(&err, env, PKG"Cookie");
	fid_Cookie_pointer = get_field(&err, env, "pointer", "J");

	cls_Device = get_class(&err, env, PKG"Device");
	fid_Device_pointer = get_field(&err, env, "pointer", "J");
	mid_Device_init = get_method(&err, env, "<init>", "(J)V");
	mid_Device_fillPath = get_method(&err, env, "fillPath", "(L"PKG"Path;ZL"PKG"Matrix;L"PKG"ColorSpace;[FFI)V");
	mid_Device_strokePath = get_method(&err, env, "strokePath", "(L"PKG"Path;L"PKG"StrokeState;L"PKG"Matrix;L"PKG"ColorSpace;[FFI)V");
	mid_Device_clipPath = get_method(&err, env, "clipPath", "(L"PKG"Path;ZL"PKG"Matrix;)V");
	mid_Device_clipStrokePath = get_method(&err, env, "clipStrokePath", "(L"PKG"Path;L"PKG"StrokeState;L"PKG"Matrix;)V");
	mid_Device_fillText = get_method(&err, env, "fillText", "(L"PKG"Text;L"PKG"Matrix;L"PKG"ColorSpace;[FFI)V");
	mid_Device_strokeText = get_method(&err, env, "strokeText", "(L"PKG"Text;L"PKG"StrokeState;L"PKG"Matrix;L"PKG"ColorSpace;[FFI)V");
	mid_Device_clipText = get_method(&err, env, "clipText", "(L"PKG"Text;L"PKG"Matrix;)V");
	mid_Device_clipStrokeText = get_method(&err, env, "clipStrokeText", "(L"PKG"Text;L"PKG"StrokeState;L"PKG"Matrix;)V");
	mid_Device_ignoreText = get_method(&err, env, "ignoreText", "(L"PKG"Text;L"PKG"Matrix;)V");
	mid_Device_fillShade = get_method(&err, env, "fillShade", "(L"PKG"Shade;L"PKG"Matrix;FI)V");
	mid_Device_fillImage = get_method(&err, env, "fillImage", "(L"PKG"Image;L"PKG"Matrix;FI)V");
	mid_Device_fillImageMask = get_method(&err, env, "fillImageMask", "(L"PKG"Image;L"PKG"Matrix;L"PKG"ColorSpace;[FFI)V");
	mid_Device_clipImageMask = get_method(&err, env, "clipImageMask", "(L"PKG"Image;L"PKG"Matrix;)V");
	mid_Device_popClip = get_method(&err, env, "popClip", "()V");
	mid_Device_beginLayer = get_method(&err, env, "beginLayer", "(Ljava/lang/String;)V");
	mid_Device_endLayer = get_method(&err, env, "endLayer", "()V");
	mid_Device_beginMask = get_method(&err, env, "beginMask", "(L"PKG"Rect;ZL"PKG"ColorSpace;[FI)V");
	mid_Device_endMask = get_method(&err, env, "endMask", "()V");
	mid_Device_beginGroup = get_method(&err, env, "beginGroup", "(L"PKG"Rect;L"PKG"ColorSpace;ZZIF)V");
	mid_Device_endGroup = get_method(&err, env, "endGroup", "()V");
	mid_Device_beginTile = get_method(&err, env, "beginTile", "(L"PKG"Rect;L"PKG"Rect;FFL"PKG"Matrix;I)I");
	mid_Device_endTile = get_method(&err, env, "endTile", "()V");

	cls_DisplayList = get_class(&err, env, PKG"DisplayList");
	fid_DisplayList_pointer = get_field(&err, env, "pointer", "J");
	mid_DisplayList_init = get_method(&err, env, "<init>", "(J)V");

	cls_Document = get_class(&err, env, PKG"Document");
	fid_Document_pointer = get_field(&err, env, "pointer", "J");
	mid_Document_init = get_method(&err, env, "<init>", "(J)V");

	cls_DocumentWriter = get_class(&err, env, PKG"DocumentWriter");
	fid_DocumentWriter_pointer = get_field(&err, env, "pointer", "J");

	cls_Font = get_class(&err, env, PKG"Font");
	fid_Font_pointer = get_field(&err, env, "pointer", "J");
	mid_Font_init = get_method(&err, env, "<init>", "(J)V");

	cls_Image = get_class(&err, env, PKG"Image");
	fid_Image_pointer = get_field(&err, env, "pointer", "J");
	mid_Image_init = get_method(&err, env, "<init>", "(J)V");

	cls_Link = get_class(&err, env, PKG"Link");
	mid_Link_init = get_method(&err, env, "<init>", "(L"PKG"Rect;Ljava/lang/String;)V");

	cls_Location = get_class(&err, env, PKG"Location");
	mid_Location_init = get_method(&err, env, "<init>", "(IIFF)V");

	cls_Matrix = get_class(&err, env, PKG"Matrix");
	fid_Matrix_a = get_field(&err, env, "a", "F");
	fid_Matrix_b = get_field(&err, env, "b", "F");
	fid_Matrix_c = get_field(&err, env, "c", "F");
	fid_Matrix_d = get_field(&err, env, "d", "F");
	fid_Matrix_e = get_field(&err, env, "e", "F");
	fid_Matrix_f = get_field(&err, env, "f", "F");
	mid_Matrix_init = get_method(&err, env, "<init>", "(FFFFFF)V");

	cls_NativeDevice = get_class(&err, env, PKG"NativeDevice");
	fid_NativeDevice_nativeResource = get_field(&err, env, "nativeResource", "Ljava/lang/Object;");
	fid_NativeDevice_nativeInfo = get_field(&err, env, "nativeInfo", "J");

	cls_Outline = get_class(&err, env, PKG"Outline");
	mid_Outline_init = get_method(&err, env, "<init>", "(Ljava/lang/String;Ljava/lang/String;[L"PKG"Outline;)V");

	cls_Page = get_class(&err, env, PKG"Page");
	fid_Page_pointer = get_field(&err, env, "pointer", "J");
	mid_Page_init = get_method(&err, env, "<init>", "(J)V");

	cls_Path = get_class(&err, env, PKG"Path");
	fid_Path_pointer = get_field(&err, env, "pointer", "J");
	mid_Path_init = get_method(&err, env, "<init>", "(J)V");

	cls_PathWalker = get_class(&err, env, PKG"PathWalker");
	mid_PathWalker_moveTo = get_method(&err, env, "moveTo", "(FF)V");
	mid_PathWalker_lineTo = get_method(&err, env, "lineTo", "(FF)V");
	mid_PathWalker_curveTo = get_method(&err, env, "curveTo", "(FFFFFF)V");
	mid_PathWalker_closePath = get_method(&err, env, "closePath", "()V");

	cls_PDFAnnotation = get_class(&err, env, PKG"PDFAnnotation");
	fid_PDFAnnotation_pointer = get_field(&err, env, "pointer", "J");
	mid_PDFAnnotation_init = get_method(&err, env, "<init>", "(J)V");

	cls_PDFDocument = get_class(&err, env, PKG"PDFDocument");
	fid_PDFDocument_pointer = get_field(&err, env, "pointer", "J");
	mid_PDFDocument_init = get_method(&err, env, "<init>", "(J)V");

	cls_PDFDocument_JsEventListener = get_class(&err, env, PKG"PDFDocument$JsEventListener");
	mid_PDFDocument_JsEventListener_onAlert = get_method(&err, env, "onAlert", "(Ljava/lang/String;)V");

	cls_PDFGraftMap = get_class(&err, env, PKG"PDFGraftMap");
	fid_PDFGraftMap_pointer = get_field(&err, env, "pointer", "J");
	mid_PDFGraftMap_init = get_method(&err, env, "<init>", "(J)V");

	cls_PDFObject = get_class(&err, env, PKG"PDFObject");
	fid_PDFObject_pointer = get_field(&err, env, "pointer", "J");
	fid_PDFObject_Null = get_static_field(&err, env, "Null", "L"PKG"PDFObject;");
	mid_PDFObject_init = get_method(&err, env, "<init>", "(J)V");

	cls_PDFPage = get_class(&err, env, PKG"PDFPage");
	fid_PDFPage_pointer = get_field(&err, env, "pointer", "J");
	mid_PDFPage_init = get_method(&err, env, "<init>", "(J)V");

	cls_Pixmap = get_class(&err, env, PKG"Pixmap");
	fid_Pixmap_pointer = get_field(&err, env, "pointer", "J");
	mid_Pixmap_init = get_method(&err, env, "<init>", "(J)V");

	cls_Point = get_class(&err, env, PKG"Point");
	mid_Point_init = get_method(&err, env, "<init>", "(FF)V");
	fid_Point_x = get_field(&err, env, "x", "F");
	fid_Point_y = get_field(&err, env, "y", "F");

	cls_Quad = get_class(&err, env, PKG"Quad");
	fid_Quad_ul_x = get_field(&err, env, "ul_x", "F");
	fid_Quad_ul_y = get_field(&err, env, "ul_y", "F");
	fid_Quad_ur_x = get_field(&err, env, "ur_x", "F");
	fid_Quad_ur_y = get_field(&err, env, "ur_y", "F");
	fid_Quad_ll_x = get_field(&err, env, "ll_x", "F");
	fid_Quad_ll_y = get_field(&err, env, "ll_y", "F");
	fid_Quad_lr_x = get_field(&err, env, "lr_x", "F");
	fid_Quad_lr_y = get_field(&err, env, "lr_y", "F");
	mid_Quad_init = get_method(&err, env, "<init>", "(FFFFFFFF)V");

	cls_Rect = get_class(&err, env, PKG"Rect");
	fid_Rect_x0 = get_field(&err, env, "x0", "F");
	fid_Rect_x1 = get_field(&err, env, "x1", "F");
	fid_Rect_y0 = get_field(&err, env, "y0", "F");
	fid_Rect_y1 = get_field(&err, env, "y1", "F");
	mid_Rect_init = get_method(&err, env, "<init>", "(FFFF)V");

	cls_SeekableInputStream = get_class(&err, env, PKG"SeekableInputStream");
	mid_SeekableInputStream_read = get_method(&err, env, "read", "([B)I");

	cls_SeekableOutputStream = get_class(&err, env, PKG"SeekableOutputStream");
	mid_SeekableOutputStream_write = get_method(&err, env, "write", "([BII)V");

	cls_SeekableStream = get_class(&err, env, PKG"SeekableStream");
	mid_SeekableStream_position = get_method(&err, env, "position", "()J");
	mid_SeekableStream_seek = get_method(&err, env, "seek", "(JI)J");

	cls_Shade = get_class(&err, env, PKG"Shade");
	fid_Shade_pointer = get_field(&err, env, "pointer", "J");
	mid_Shade_init = get_method(&err, env, "<init>", "(J)V");

	cls_String = get_class(&err, env,  "java/lang/String");

	cls_StrokeState = get_class(&err, env, PKG"StrokeState");
	fid_StrokeState_pointer = get_field(&err, env, "pointer", "J");
	mid_StrokeState_init = get_method(&err, env, "<init>", "(J)V");

	cls_StructuredText = get_class(&err, env, PKG"StructuredText");
	fid_StructuredText_pointer = get_field(&err, env, "pointer", "J");
	mid_StructuredText_init = get_method(&err, env, "<init>", "(J)V");

	cls_StructuredTextWalker = get_class(&err, env, PKG"StructuredTextWalker");
	mid_StructuredTextWalker_onImageBlock = get_method(&err, env, "onImageBlock", "(L"PKG"Rect;L"PKG"Matrix;L"PKG"Image;)V");
	mid_StructuredTextWalker_beginTextBlock = get_method(&err, env, "beginTextBlock", "(L"PKG"Rect;)V");
	mid_StructuredTextWalker_endTextBlock = get_method(&err, env, "endTextBlock", "()V");
	mid_StructuredTextWalker_beginLine = get_method(&err, env, "beginLine", "(L"PKG"Rect;I)V");
	mid_StructuredTextWalker_endLine = get_method(&err, env, "endLine", "()V");
	mid_StructuredTextWalker_onChar = get_method(&err, env, "onChar", "(IL"PKG"Point;L"PKG"Font;FL"PKG"Quad;)V");

	cls_Text = get_class(&err, env, PKG"Text");
	fid_Text_pointer = get_field(&err, env, "pointer", "J");
	mid_Text_init = get_method(&err, env, "<init>", "(J)V");

	cls_TextBlock = get_class(&err, env, PKG"StructuredText$TextBlock");
	mid_TextBlock_init = get_method(&err, env, "<init>", "(L"PKG"StructuredText;)V");
	fid_TextBlock_bbox = get_field(&err, env, "bbox", "L"PKG"Rect;");
	fid_TextBlock_lines = get_field(&err, env, "lines", "[L"PKG"StructuredText$TextLine;");

	cls_TextChar = get_class(&err, env, PKG"StructuredText$TextChar");
	mid_TextChar_init = get_method(&err, env, "<init>", "(L"PKG"StructuredText;)V");
	fid_TextChar_quad = get_field(&err, env, "quad", "L"PKG"Quad;");
	fid_TextChar_c = get_field(&err, env, "c", "I");

	cls_TextLine = get_class(&err, env, PKG"StructuredText$TextLine");
	mid_TextLine_init = get_method(&err, env, "<init>", "(L"PKG"StructuredText;)V");
	fid_TextLine_bbox = get_field(&err, env, "bbox", "L"PKG"Rect;");
	fid_TextLine_chars = get_field(&err, env, "chars", "[L"PKG"StructuredText$TextChar;");

	cls_TextWalker = get_class(&err, env, PKG"TextWalker");
	mid_TextWalker_showGlyph = get_method(&err, env, "showGlyph", "(L"PKG"Font;L"PKG"Matrix;IIZ)V");

	cls_PDFWidget = get_class(&err, env, PKG"PDFWidget");
	fid_PDFWidget_pointer = get_field(&err, env, "pointer", "J");
	fid_PDFWidget_fieldType = get_field(&err, env, "fieldType", "I");
	fid_PDFWidget_textFormat = get_field(&err, env, "textFormat", "I");
	fid_PDFWidget_maxLen = get_field(&err, env, "maxLen", "I");
	fid_PDFWidget_fieldFlags = get_field(&err, env, "fieldFlags", "I");
	fid_PDFWidget_options = get_field(&err, env, "options", "[Ljava/lang/String;");
	mid_PDFWidget_init = get_method(&err, env, "<init>", "(J)V");

	cls_TryLaterException = get_class(&err, env, PKG"TryLaterException");

	/* Standard Java classes */

	cls_FloatArray = get_class(&err, env, "[F");
	cls_IntegerArray = get_class(&err, env, "[I");

	cls_Object = get_class(&err, env, "java/lang/Object");
	mid_Object_toString = get_method(&err, env, "toString", "()Ljava/lang/String;");

	cls_IndexOutOfBoundsException = get_class(&err, env, "java/lang/IndexOutOfBoundsException");
	cls_IllegalArgumentException = get_class(&err, env, "java/lang/IllegalArgumentException");
	cls_IOException = get_class(&err, env, "java/io/IOException");
	cls_NullPointerException = get_class(&err, env, "java/lang/NullPointerException");
	cls_RuntimeException = get_class(&err, env, "java/lang/RuntimeException");

	cls_OutOfMemoryError = get_class(&err, env, "java/lang/OutOfMemoryError");

	/* Get and store the main JVM pointer. We need this in order to get
	 * JNIEnv pointers on callback threads. This is specifically
	 * guaranteed to be safe to store in a static var. */

	getvmErr = (*env)->GetJavaVM(env, &jvm);
	if (getvmErr < 0)
	{
		LOGE("mupdf_native.c find_fids() GetJavaVM failed with %d", getvmErr);
		err = 1;
	}

	return err;
}

/* When making callbacks from C to java, we may be called on threads
 * other than the foreground. As such, we have no JNIEnv. This function
 * handles getting us the required environment */
static JNIEnv *jni_attach_thread(fz_context *ctx, int *detach)
{
	JNIEnv *env = NULL;
	int state;

	*detach = 0;
	state = (*jvm)->GetEnv(jvm, (void*)&env, MY_JNI_VERSION);
	if (state == JNI_EDETACHED)
	{
		*detach = 1;
		state = (*jvm)->AttachCurrentThread(jvm, (void*)&env, NULL);
	}

	if (state != JNI_OK)
		return NULL;

	return env;
}

static void jni_detach_thread(int detach)
{
	if (!detach)
		return;
	(*jvm)->DetachCurrentThread(jvm);
}

static void lose_fids(JNIEnv *env)
{
	(*env)->DeleteGlobalRef(env, cls_Buffer);
	(*env)->DeleteGlobalRef(env, cls_ColorSpace);
	(*env)->DeleteGlobalRef(env, cls_Cookie);
	(*env)->DeleteGlobalRef(env, cls_Device);
	(*env)->DeleteGlobalRef(env, cls_DisplayList);
	(*env)->DeleteGlobalRef(env, cls_Document);
	(*env)->DeleteGlobalRef(env, cls_DocumentWriter);
	(*env)->DeleteGlobalRef(env, cls_FloatArray);
	(*env)->DeleteGlobalRef(env, cls_Font);
	(*env)->DeleteGlobalRef(env, cls_IllegalArgumentException);
	(*env)->DeleteGlobalRef(env, cls_Image);
	(*env)->DeleteGlobalRef(env, cls_IndexOutOfBoundsException);
	(*env)->DeleteGlobalRef(env, cls_IntegerArray);
	(*env)->DeleteGlobalRef(env, cls_IOException);
	(*env)->DeleteGlobalRef(env, cls_Link);
	(*env)->DeleteGlobalRef(env, cls_Location);
	(*env)->DeleteGlobalRef(env, cls_Matrix);
	(*env)->DeleteGlobalRef(env, cls_NativeDevice);
	(*env)->DeleteGlobalRef(env, cls_NullPointerException);
	(*env)->DeleteGlobalRef(env, cls_Object);
	(*env)->DeleteGlobalRef(env, cls_Outline);
	(*env)->DeleteGlobalRef(env, cls_OutOfMemoryError);
	(*env)->DeleteGlobalRef(env, cls_Page);
	(*env)->DeleteGlobalRef(env, cls_Path);
	(*env)->DeleteGlobalRef(env, cls_PathWalker);
	(*env)->DeleteGlobalRef(env, cls_PDFAnnotation);
	(*env)->DeleteGlobalRef(env, cls_PDFDocument);
	(*env)->DeleteGlobalRef(env, cls_PDFDocument_JsEventListener);
	(*env)->DeleteGlobalRef(env, cls_PDFPage);
	(*env)->DeleteGlobalRef(env, cls_PDFGraftMap);
	(*env)->DeleteGlobalRef(env, cls_PDFObject);
	(*env)->DeleteGlobalRef(env, cls_Pixmap);
	(*env)->DeleteGlobalRef(env, cls_Point);
	(*env)->DeleteGlobalRef(env, cls_Quad);
	(*env)->DeleteGlobalRef(env, cls_Rect);
	(*env)->DeleteGlobalRef(env, cls_RuntimeException);
	(*env)->DeleteGlobalRef(env, cls_SeekableStream);
	(*env)->DeleteGlobalRef(env, cls_SeekableInputStream);
	(*env)->DeleteGlobalRef(env, cls_SeekableOutputStream);
	(*env)->DeleteGlobalRef(env, cls_Shade);
	(*env)->DeleteGlobalRef(env, cls_String);
	(*env)->DeleteGlobalRef(env, cls_StrokeState);
	(*env)->DeleteGlobalRef(env, cls_StructuredText);
	(*env)->DeleteGlobalRef(env, cls_StructuredTextWalker);
	(*env)->DeleteGlobalRef(env, cls_Text);
	(*env)->DeleteGlobalRef(env, cls_TextBlock);
	(*env)->DeleteGlobalRef(env, cls_TextChar);
	(*env)->DeleteGlobalRef(env, cls_TextLine);
	(*env)->DeleteGlobalRef(env, cls_TextWalker);
	(*env)->DeleteGlobalRef(env, cls_TryLaterException);
	(*env)->DeleteGlobalRef(env, cls_PDFWidget);
}

#ifdef HAVE_ANDROID

static fz_font *load_noto(fz_context *ctx, const char *a, const char *b, const char *c, int idx)
{
	char buf[500];
	fz_font *font = NULL;
	fz_try(ctx)
	{
		fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.ttf", a, b, c);
		if (!fz_file_exists(ctx, buf))
			fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.otf", a, b, c);
		if (!fz_file_exists(ctx, buf))
			fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.ttc", a, b, c);
		if (fz_file_exists(ctx, buf))
			font = fz_new_font_from_file(ctx, NULL, buf, idx, 0);
	}
	fz_catch(ctx)
		return NULL;
	return font;
}

static fz_font *load_noto_cjk(fz_context *ctx, int lang)
{
	fz_font *font = load_noto(ctx, "NotoSerif", "CJK", "-Regular", lang);
	if (!font) font = load_noto(ctx, "NotoSans", "CJK", "-Regular", lang);
	if (!font) font = load_noto(ctx, "DroidSans", "Fallback", "", 0);
	return font;
}

static fz_font *load_noto_arabic(fz_context *ctx)
{
	fz_font *font = load_noto(ctx, "Noto", "Naskh", "-Regular", 0);
	if (!font) font = load_noto(ctx, "Noto", "NaskhArabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "Droid", "Naskh", "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSerif", "Arabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSans", "Arabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "DroidSans", "Arabic", "-Regular", 0);
	return font;
}

static fz_font *load_noto_try(fz_context *ctx, const char *stem)
{
	fz_font *font = load_noto(ctx, "NotoSerif", stem, "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSans", stem, "-Regular", 0);
	if (!font) font = load_noto(ctx, "DroidSans", stem, "-Regular", 0);
	return font;
}

enum { JP, KR, SC, TC };

fz_font *load_droid_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic)
{
	switch (script)
	{
	default:
	case UCDN_SCRIPT_COMMON:
	case UCDN_SCRIPT_INHERITED:
	case UCDN_SCRIPT_UNKNOWN:
		return NULL;

	case UCDN_SCRIPT_HANGUL: return load_noto_cjk(ctx, KR);
	case UCDN_SCRIPT_HIRAGANA: return load_noto_cjk(ctx, JP);
	case UCDN_SCRIPT_KATAKANA: return load_noto_cjk(ctx, JP);
	case UCDN_SCRIPT_BOPOMOFO: return load_noto_cjk(ctx, TC);
	case UCDN_SCRIPT_HAN:
		switch (language) {
		case FZ_LANG_ja: return load_noto_cjk(ctx, JP);
		case FZ_LANG_ko: return load_noto_cjk(ctx, KR);
		case FZ_LANG_zh_Hans: return load_noto_cjk(ctx, SC);
		default:
		case FZ_LANG_zh_Hant: return load_noto_cjk(ctx, TC);
		}

	case UCDN_SCRIPT_LATIN:
	case UCDN_SCRIPT_GREEK:
	case UCDN_SCRIPT_CYRILLIC:
		return load_noto_try(ctx, "");
	case UCDN_SCRIPT_ARABIC:
		return load_noto_arabic(ctx);

	case UCDN_SCRIPT_ARMENIAN: return load_noto_try(ctx, "Armenian");
	case UCDN_SCRIPT_HEBREW: return load_noto_try(ctx, "Hebrew");
	case UCDN_SCRIPT_SYRIAC: return load_noto_try(ctx, "Syriac");
	case UCDN_SCRIPT_THAANA: return load_noto_try(ctx, "Thaana");
	case UCDN_SCRIPT_DEVANAGARI: return load_noto_try(ctx, "Devanagari");
	case UCDN_SCRIPT_BENGALI: return load_noto_try(ctx, "Bengali");
	case UCDN_SCRIPT_GURMUKHI: return load_noto_try(ctx, "Gurmukhi");
	case UCDN_SCRIPT_GUJARATI: return load_noto_try(ctx, "Gujarati");
	case UCDN_SCRIPT_ORIYA: return load_noto_try(ctx, "Oriya");
	case UCDN_SCRIPT_TAMIL: return load_noto_try(ctx, "Tamil");
	case UCDN_SCRIPT_TELUGU: return load_noto_try(ctx, "Telugu");
	case UCDN_SCRIPT_KANNADA: return load_noto_try(ctx, "Kannada");
	case UCDN_SCRIPT_MALAYALAM: return load_noto_try(ctx, "Malayalam");
	case UCDN_SCRIPT_SINHALA: return load_noto_try(ctx, "Sinhala");
	case UCDN_SCRIPT_THAI: return load_noto_try(ctx, "Thai");
	case UCDN_SCRIPT_LAO: return load_noto_try(ctx, "Lao");
	case UCDN_SCRIPT_TIBETAN: return load_noto_try(ctx, "Tibetan");
	case UCDN_SCRIPT_MYANMAR: return load_noto_try(ctx, "Myanmar");
	case UCDN_SCRIPT_GEORGIAN: return load_noto_try(ctx, "Georgian");
	case UCDN_SCRIPT_ETHIOPIC: return load_noto_try(ctx, "Ethiopic");
	case UCDN_SCRIPT_CHEROKEE: return load_noto_try(ctx, "Cherokee");
	case UCDN_SCRIPT_CANADIAN_ABORIGINAL: return load_noto_try(ctx, "CanadianAboriginal");
	case UCDN_SCRIPT_OGHAM: return load_noto_try(ctx, "Ogham");
	case UCDN_SCRIPT_RUNIC: return load_noto_try(ctx, "Runic");
	case UCDN_SCRIPT_KHMER: return load_noto_try(ctx, "Khmer");
	case UCDN_SCRIPT_MONGOLIAN: return load_noto_try(ctx, "Mongolian");
	case UCDN_SCRIPT_YI: return load_noto_try(ctx, "Yi");
	case UCDN_SCRIPT_OLD_ITALIC: return load_noto_try(ctx, "OldItalic");
	case UCDN_SCRIPT_GOTHIC: return load_noto_try(ctx, "Gothic");
	case UCDN_SCRIPT_DESERET: return load_noto_try(ctx, "Deseret");
	case UCDN_SCRIPT_TAGALOG: return load_noto_try(ctx, "Tagalog");
	case UCDN_SCRIPT_HANUNOO: return load_noto_try(ctx, "Hanunoo");
	case UCDN_SCRIPT_BUHID: return load_noto_try(ctx, "Buhid");
	case UCDN_SCRIPT_TAGBANWA: return load_noto_try(ctx, "Tagbanwa");
	case UCDN_SCRIPT_LIMBU: return load_noto_try(ctx, "Limbu");
	case UCDN_SCRIPT_TAI_LE: return load_noto_try(ctx, "TaiLe");
	case UCDN_SCRIPT_LINEAR_B: return load_noto_try(ctx, "LinearB");
	case UCDN_SCRIPT_UGARITIC: return load_noto_try(ctx, "Ugaritic");
	case UCDN_SCRIPT_SHAVIAN: return load_noto_try(ctx, "Shavian");
	case UCDN_SCRIPT_OSMANYA: return load_noto_try(ctx, "Osmanya");
	case UCDN_SCRIPT_CYPRIOT: return load_noto_try(ctx, "Cypriot");
	case UCDN_SCRIPT_BUGINESE: return load_noto_try(ctx, "Buginese");
	case UCDN_SCRIPT_COPTIC: return load_noto_try(ctx, "Coptic");
	case UCDN_SCRIPT_NEW_TAI_LUE: return load_noto_try(ctx, "NewTaiLue");
	case UCDN_SCRIPT_GLAGOLITIC: return load_noto_try(ctx, "Glagolitic");
	case UCDN_SCRIPT_TIFINAGH: return load_noto_try(ctx, "Tifinagh");
	case UCDN_SCRIPT_SYLOTI_NAGRI: return load_noto_try(ctx, "SylotiNagri");
	case UCDN_SCRIPT_OLD_PERSIAN: return load_noto_try(ctx, "OldPersian");
	case UCDN_SCRIPT_KHAROSHTHI: return load_noto_try(ctx, "Kharoshthi");
	case UCDN_SCRIPT_BALINESE: return load_noto_try(ctx, "Balinese");
	case UCDN_SCRIPT_CUNEIFORM: return load_noto_try(ctx, "Cuneiform");
	case UCDN_SCRIPT_PHOENICIAN: return load_noto_try(ctx, "Phoenician");
	case UCDN_SCRIPT_PHAGS_PA: return load_noto_try(ctx, "PhagsPa");
	case UCDN_SCRIPT_NKO: return load_noto_try(ctx, "NKo");
	case UCDN_SCRIPT_SUNDANESE: return load_noto_try(ctx, "Sundanese");
	case UCDN_SCRIPT_LEPCHA: return load_noto_try(ctx, "Lepcha");
	case UCDN_SCRIPT_OL_CHIKI: return load_noto_try(ctx, "OlChiki");
	case UCDN_SCRIPT_VAI: return load_noto_try(ctx, "Vai");
	case UCDN_SCRIPT_SAURASHTRA: return load_noto_try(ctx, "Saurashtra");
	case UCDN_SCRIPT_KAYAH_LI: return load_noto_try(ctx, "KayahLi");
	case UCDN_SCRIPT_REJANG: return load_noto_try(ctx, "Rejang");
	case UCDN_SCRIPT_LYCIAN: return load_noto_try(ctx, "Lycian");
	case UCDN_SCRIPT_CARIAN: return load_noto_try(ctx, "Carian");
	case UCDN_SCRIPT_LYDIAN: return load_noto_try(ctx, "Lydian");
	case UCDN_SCRIPT_CHAM: return load_noto_try(ctx, "Cham");
	case UCDN_SCRIPT_TAI_THAM: return load_noto_try(ctx, "TaiTham");
	case UCDN_SCRIPT_TAI_VIET: return load_noto_try(ctx, "TaiViet");
	case UCDN_SCRIPT_AVESTAN: return load_noto_try(ctx, "Avestan");
	case UCDN_SCRIPT_EGYPTIAN_HIEROGLYPHS: return load_noto_try(ctx, "EgyptianHieroglyphs");
	case UCDN_SCRIPT_SAMARITAN: return load_noto_try(ctx, "Samaritan");
	case UCDN_SCRIPT_LISU: return load_noto_try(ctx, "Lisu");
	case UCDN_SCRIPT_BAMUM: return load_noto_try(ctx, "Bamum");
	case UCDN_SCRIPT_JAVANESE: return load_noto_try(ctx, "Javanese");
	case UCDN_SCRIPT_MEETEI_MAYEK: return load_noto_try(ctx, "MeeteiMayek");
	case UCDN_SCRIPT_IMPERIAL_ARAMAIC: return load_noto_try(ctx, "ImperialAramaic");
	case UCDN_SCRIPT_OLD_SOUTH_ARABIAN: return load_noto_try(ctx, "OldSouthArabian");
	case UCDN_SCRIPT_INSCRIPTIONAL_PARTHIAN: return load_noto_try(ctx, "InscriptionalParthian");
	case UCDN_SCRIPT_INSCRIPTIONAL_PAHLAVI: return load_noto_try(ctx, "InscriptionalPahlavi");
	case UCDN_SCRIPT_OLD_TURKIC: return load_noto_try(ctx, "OldTurkic");
	case UCDN_SCRIPT_KAITHI: return load_noto_try(ctx, "Kaithi");
	case UCDN_SCRIPT_BATAK: return load_noto_try(ctx, "Batak");
	case UCDN_SCRIPT_BRAHMI: return load_noto_try(ctx, "Brahmi");
	case UCDN_SCRIPT_MANDAIC: return load_noto_try(ctx, "Mandaic");
	case UCDN_SCRIPT_CHAKMA: return load_noto_try(ctx, "Chakma");
	case UCDN_SCRIPT_MIAO: return load_noto_try(ctx, "Miao");
	case UCDN_SCRIPT_MEROITIC_CURSIVE: return load_noto_try(ctx, "Meroitic");
	case UCDN_SCRIPT_MEROITIC_HIEROGLYPHS: return load_noto_try(ctx, "Meroitic");
	case UCDN_SCRIPT_SHARADA: return load_noto_try(ctx, "Sharada");
	case UCDN_SCRIPT_SORA_SOMPENG: return load_noto_try(ctx, "SoraSompeng");
	case UCDN_SCRIPT_TAKRI: return load_noto_try(ctx, "Takri");
	case UCDN_SCRIPT_BASSA_VAH: return load_noto_try(ctx, "BassaVah");
	case UCDN_SCRIPT_CAUCASIAN_ALBANIAN: return load_noto_try(ctx, "CaucasianAlbanian");
	case UCDN_SCRIPT_DUPLOYAN: return load_noto_try(ctx, "Duployan");
	case UCDN_SCRIPT_ELBASAN: return load_noto_try(ctx, "Elbasan");
	case UCDN_SCRIPT_GRANTHA: return load_noto_try(ctx, "Grantha");
	case UCDN_SCRIPT_KHOJKI: return load_noto_try(ctx, "Khojki");
	case UCDN_SCRIPT_KHUDAWADI: return load_noto_try(ctx, "Khudawadi");
	case UCDN_SCRIPT_LINEAR_A: return load_noto_try(ctx, "LinearA");
	case UCDN_SCRIPT_MAHAJANI: return load_noto_try(ctx, "Mahajani");
	case UCDN_SCRIPT_MANICHAEAN: return load_noto_try(ctx, "Manichaean");
	case UCDN_SCRIPT_MENDE_KIKAKUI: return load_noto_try(ctx, "MendeKikakui");
	case UCDN_SCRIPT_MODI: return load_noto_try(ctx, "Modi");
	case UCDN_SCRIPT_MRO: return load_noto_try(ctx, "Mro");
	case UCDN_SCRIPT_NABATAEAN: return load_noto_try(ctx, "Nabataean");
	case UCDN_SCRIPT_OLD_NORTH_ARABIAN: return load_noto_try(ctx, "OldNorthArabian");
	case UCDN_SCRIPT_OLD_PERMIC: return load_noto_try(ctx, "OldPermic");
	case UCDN_SCRIPT_PAHAWH_HMONG: return load_noto_try(ctx, "PahawhHmong");
	case UCDN_SCRIPT_PALMYRENE: return load_noto_try(ctx, "Palmyrene");
	case UCDN_SCRIPT_PAU_CIN_HAU: return load_noto_try(ctx, "PauCinHau");
	case UCDN_SCRIPT_PSALTER_PAHLAVI: return load_noto_try(ctx, "PsalterPahlavi");
	case UCDN_SCRIPT_SIDDHAM: return load_noto_try(ctx, "Siddham");
	case UCDN_SCRIPT_TIRHUTA: return load_noto_try(ctx, "Tirhuta");
	case UCDN_SCRIPT_WARANG_CITI: return load_noto_try(ctx, "WarangCiti");
	case UCDN_SCRIPT_AHOM: return load_noto_try(ctx, "Ahom");
	case UCDN_SCRIPT_ANATOLIAN_HIEROGLYPHS: return load_noto_try(ctx, "AnatolianHieroglyphs");
	case UCDN_SCRIPT_HATRAN: return load_noto_try(ctx, "Hatran");
	case UCDN_SCRIPT_MULTANI: return load_noto_try(ctx, "Multani");
	case UCDN_SCRIPT_OLD_HUNGARIAN: return load_noto_try(ctx, "OldHungarian");
	case UCDN_SCRIPT_SIGNWRITING: return load_noto_try(ctx, "Signwriting");
	case UCDN_SCRIPT_ADLAM: return load_noto_try(ctx, "Adlam");
	case UCDN_SCRIPT_BHAIKSUKI: return load_noto_try(ctx, "Bhaiksuki");
	case UCDN_SCRIPT_MARCHEN: return load_noto_try(ctx, "Marchen");
	case UCDN_SCRIPT_NEWA: return load_noto_try(ctx, "Newa");
	case UCDN_SCRIPT_OSAGE: return load_noto_try(ctx, "Osage");
	case UCDN_SCRIPT_TANGUT: return load_noto_try(ctx, "Tangut");
	case UCDN_SCRIPT_MASARAM_GONDI: return load_noto_try(ctx, "MasaramGondi");
	case UCDN_SCRIPT_NUSHU: return load_noto_try(ctx, "Nushu");
	case UCDN_SCRIPT_SOYOMBO: return load_noto_try(ctx, "Soyombo");
	case UCDN_SCRIPT_ZANABAZAR_SQUARE: return load_noto_try(ctx, "ZanabazarSquare");
	case UCDN_SCRIPT_DOGRA: return load_noto_try(ctx, "Dogra");
	case UCDN_SCRIPT_GUNJALA_GONDI: return load_noto_try(ctx, "GunjalaGondi");
	case UCDN_SCRIPT_HANIFI_ROHINGYA: return load_noto_try(ctx, "HanifiRohingya");
	case UCDN_SCRIPT_MAKASAR: return load_noto_try(ctx, "Makasar");
	case UCDN_SCRIPT_MEDEFAIDRIN: return load_noto_try(ctx, "Medefaidrin");
	case UCDN_SCRIPT_OLD_SOGDIAN: return load_noto_try(ctx, "OldSogdian");
	case UCDN_SCRIPT_SOGDIAN: return load_noto_try(ctx, "Sogdian");
	case UCDN_SCRIPT_ELYMAIC: return load_noto_try(ctx, "Elymaic");
	case UCDN_SCRIPT_NANDINAGARI: return load_noto_try(ctx, "Nandinagari");
	case UCDN_SCRIPT_NYIAKENG_PUACHUE_HMONG: return load_noto_try(ctx, "NyiakengPuachueHmong");
	case UCDN_SCRIPT_WANCHO: return load_noto_try(ctx, "Wancho");
	}
	return NULL;
}

fz_font *load_droid_cjk_font(fz_context *ctx, const char *name, int ros, int serif)
{
	switch (ros) {
	case FZ_ADOBE_CNS: return load_noto_cjk(ctx, TC);
	case FZ_ADOBE_GB: return load_noto_cjk(ctx, SC);
	case FZ_ADOBE_JAPAN: return load_noto_cjk(ctx, JP);
	case FZ_ADOBE_KOREA: return load_noto_cjk(ctx, KR);
	}
	return NULL;
}

fz_font *load_droid_font(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics)
{
	return NULL;
}

#endif

/* Put the fz_context in thread-local storage */

#ifdef _WIN32
static CRITICAL_SECTION mutexes[FZ_LOCK_MAX];
#else
static pthread_mutex_t mutexes[FZ_LOCK_MAX];
#endif

static void lock(void *user, int lock)
{
#ifdef _WIN32
	EnterCriticalSection(&mutexes[lock]);
#else
	(void)pthread_mutex_lock(&mutexes[lock]);
#endif
}

static void unlock(void *user, int lock)
{
#ifdef _WIN32
	LeaveCriticalSection(&mutexes[lock]);
#else
	(void)pthread_mutex_unlock(&mutexes[lock]);
#endif
}

static const fz_locks_context locks =
{
	NULL, /* user */
	lock,
	unlock
};

static void fin_base_context(JNIEnv *env)
{
	int i;

	for (i = 0; i < FZ_LOCK_MAX; i++)
#ifdef _WIN32
		DeleteCriticalSection(&mutexes[i]);
#else
		(void)pthread_mutex_destroy(&mutexes[i]);
#endif

	fz_drop_context(base_context);
	base_context = NULL;
}

#ifndef _WIN32
static void drop_tls_context(void *arg)
{
	fz_context *ctx = (fz_context *)arg;

	fz_drop_context(ctx);
}
#endif

static int init_base_context(JNIEnv *env)
{
	int i;

#ifdef _WIN32
	/* No destructor on windows. We will leak contexts.
	 * There is no easy way around this, but this page:
	 * http://stackoverflow.com/questions/3241732/is-there-anyway-to-dynamically-free-thread-local-storage-in-the-win32-apis/3245082#3245082
	 * suggests a workaround that we can implement if we
	 * need to. */
	context_key = TlsAlloc();
	if (context_key == TLS_OUT_OF_INDEXES)
		return -1;
#else
	pthread_key_create(&context_key, drop_tls_context);
#endif

	for (i = 0; i < FZ_LOCK_MAX; i++)
#ifdef _WIN32
		InitializeCriticalSection(&mutexes[i]);
#else
		(void)pthread_mutex_init(&mutexes[i], NULL);
#endif

	base_context = fz_new_context(NULL, &locks, FZ_STORE_DEFAULT);
	if (!base_context)
		return -1;

	fz_register_document_handlers(base_context);

#ifdef HAVE_ANDROID
	fz_install_load_system_font_funcs(base_context,
		load_droid_font,
		load_droid_cjk_font,
		load_droid_fallback_font);
#endif

	return 0;
}

static fz_context *get_context(JNIEnv *env)
{
	fz_context *ctx = (fz_context *)
#ifdef _WIN32
		TlsGetValue(context_key);
#else
		pthread_getspecific(context_key);
#endif

	if (ctx)
		return ctx;

	ctx = fz_clone_context(base_context);
	if (!ctx) { jni_throw_oom(env, "failed to clone fz_context"); return NULL; }

#ifdef _WIN32
	TlsSetValue(context_key, ctx);
#else
	pthread_setspecific(context_key, ctx);
#endif
	return ctx;
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv *env;

	if ((*vm)->GetEnv(vm, (void **)&env, MY_JNI_VERSION) != JNI_OK)
		return -1;

	return MY_JNI_VERSION;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
	JNIEnv *env;

	if ((*vm)->GetEnv(vm, (void **)&env, MY_JNI_VERSION) != JNI_OK)
		return; /* If this fails, we're really in trouble! */

	fz_drop_context(base_context);
	base_context = NULL;
	lose_fids(env);
}

JNIEXPORT jint JNICALL
FUN(Context_initNative)(JNIEnv *env, jclass cls)
{
	if (!check_enums())
		return -1;

	/* Must init the context before find_finds, because the act of
	 * finding the fids can cause classes to load. This causes
	 * statics to be setup, which can in turn call JNI code, which
	 * requires the context. (For example see ColorSpace) */
	if (init_base_context(env) < 0)
		return -1;

	if (find_fids(env) != 0)
	{
		fin_base_context(env);
		return -1;
	}

	return 0;
}

/* Conversion functions: C to Java. These all throw fitz exceptions. */

static inline jobject to_ColorSpace(fz_context *ctx, JNIEnv *env, fz_colorspace *cs)
{
	jobject jcs;

	if (!ctx || !cs) return NULL;

	fz_keep_colorspace(ctx, cs);
	jcs = (*env)->CallStaticObjectMethod(env, cls_ColorSpace, mid_ColorSpace_fromPointer, jlong_cast(cs));
	if (!jcs)
		fz_drop_colorspace(ctx, cs);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jcs;
}

static inline jobject to_Image(fz_context *ctx, JNIEnv *env, fz_image *img)
{
	jobject jimg;

	if (!ctx || !img) return NULL;

	fz_keep_image(ctx, img);
	jimg = (*env)->NewObject(env, cls_Image, mid_Image_init, jlong_cast(img));
	if (!jimg)
		fz_drop_image(ctx, img);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jimg;
}

static inline jobject to_Matrix(fz_context *ctx, JNIEnv *env, fz_matrix mat)
{
	jobject jctm;

	if (!ctx) return NULL;

	jctm = (*env)->NewObject(env, cls_Matrix, mid_Matrix_init, mat.a, mat.b, mat.c, mat.d, mat.e, mat.f);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jctm;
}

static inline jobject to_Path(fz_context *ctx, JNIEnv *env, const fz_path *path)
{
	jobject jpath;

	if (!ctx || !path) return NULL;

	fz_keep_path(ctx, path);
	jpath = (*env)->NewObject(env, cls_Path, mid_Path_init, jlong_cast(path));
	if (!jpath)
		fz_drop_path(ctx, path);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jpath;
}

static inline jobject to_Rect(fz_context *ctx, JNIEnv *env, fz_rect rect)
{
	jobject jrect;

	if (!ctx) return NULL;

	jrect = (*env)->NewObject(env, cls_Rect, mid_Rect_init, rect.x0, rect.y0, rect.x1, rect.y1);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jrect;
}

static inline jobject to_Shade(fz_context *ctx, JNIEnv *env, fz_shade *shd)
{
	jobject jshd;

	if (!ctx || !shd) return NULL;

	fz_keep_shade(ctx, shd);
	jshd = (*env)->NewObject(env, cls_Shade, mid_Shade_init, jlong_cast(shd));
	if (!jshd)
		fz_drop_shade(ctx, shd);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jshd;
}

static inline jobject to_StrokeState(fz_context *ctx, JNIEnv *env, const fz_stroke_state *state)
{
	jobject jstate;

	if (!ctx || !state) return NULL;

	fz_keep_stroke_state(ctx, state);
	jstate = (*env)->NewObject(env, cls_StrokeState, mid_StrokeState_init, jlong_cast(state));
	if (!jstate)
		fz_drop_stroke_state(ctx, state);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jstate;
}

static inline jobject to_Text(fz_context *ctx, JNIEnv *env, const fz_text *text)
{
	jobject jtext;

	if (!ctx) return NULL;

	fz_keep_text(ctx, text);
	jtext = (*env)->NewObject(env, cls_Text, mid_Text_init, jlong_cast(text));
	if (!jtext)
		fz_drop_text(ctx, text);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return jtext;
}

static inline jfloatArray to_jfloatArray(fz_context *ctx, JNIEnv *env, const float *color, jint n)
{
	jfloatArray arr;

	if (!ctx) return NULL;

	arr = (*env)->NewFloatArray(env, n);
	if (!arr)
		fz_throw_java(ctx, env);

	(*env)->SetFloatArrayRegion(env, arr, 0, n, color);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return arr;
}

/* Conversion functions: C to Java. None of these throw fitz exceptions. */

static inline jint to_ColorParams_safe(fz_context *ctx, JNIEnv *env, fz_color_params cp)
{
	if (!ctx)
		return 0;

	return (((int) (!!cp.bp)<<5) | ((int) (!!cp.op)<<6) | ((int) (!!cp.opm)<<7) | (cp.ri & 31));
}

static inline jobject to_ColorSpace_safe(fz_context *ctx, JNIEnv *env, fz_colorspace *cs)
{
	jobject jcs;

	if (!ctx || !cs) return NULL;

	fz_keep_colorspace(ctx, cs);
	jcs = (*env)->CallStaticObjectMethod(env, cls_ColorSpace, mid_ColorSpace_fromPointer, jlong_cast(cs));
	if (!jcs) fz_drop_colorspace(ctx, cs);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return jcs;
}

static inline jobject to_Font_safe(fz_context *ctx, JNIEnv *env, fz_font *font)
{
	jobject jfont;

	if (!ctx || !font) return NULL;

	fz_keep_font(ctx, font);
	jfont = (*env)->NewObject(env, cls_Font, mid_Font_init, jlong_cast(font));
	if (!jfont)
		fz_drop_font(ctx, font);

	return jfont;
}

static inline jobject to_Image_safe(fz_context *ctx, JNIEnv *env, fz_image *img)
{
	jobject jimg;

	if (!ctx || !img) return NULL;

	fz_keep_image(ctx, img);
	jimg = (*env)->NewObject(env, cls_Image, mid_Image_init, jlong_cast(img));
	if (!jimg)
		fz_drop_image(ctx, img);

	return jimg;
}

static inline jobject to_Matrix_safe(fz_context *ctx, JNIEnv *env, fz_matrix mat)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Matrix, mid_Matrix_init, mat.a, mat.b, mat.c, mat.d, mat.e, mat.f);
}

static inline jobject to_Outline_safe(fz_context *ctx, JNIEnv *env, fz_document *doc, fz_outline *outline)
{
	jobject joutline = NULL;
	jobject jarr = NULL;
	jsize jindex = 0;
	jsize count = 0;
	fz_outline *counter = outline;

	if (!ctx || !outline) return NULL;

	while (counter)
	{
		count++;
		counter = counter->next;
	}

	jarr = (*env)->NewObjectArray(env, count, cls_Outline, NULL);
	if (!jarr) return NULL;

	while (outline)
	{
		jstring jtitle = NULL;
		jstring juri = NULL;
		jobject jdown = NULL;

		if (outline->title)
		{
			jtitle = (*env)->NewStringUTF(env, outline->title);
			if (!jtitle) return NULL;
		}

		if (outline->uri)
		{
			juri = (*env)->NewStringUTF(env, outline->uri);
			if (!juri) return NULL;
		}

		if (outline->down)
		{
			jdown = to_Outline_safe(ctx, env, doc, outline->down);
			if (!jdown) return NULL;
		}

		joutline = (*env)->NewObject(env, cls_Outline, mid_Outline_init, jtitle, juri, jdown);
		if (!joutline) return NULL;

		if (jdown)
			(*env)->DeleteLocalRef(env, jdown);
		if (juri)
			(*env)->DeleteLocalRef(env, juri);
		if (jtitle)
			(*env)->DeleteLocalRef(env, jtitle);

		(*env)->SetObjectArrayElement(env, jarr, jindex++, joutline);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, joutline);
		outline = outline->next;
	}

	return jarr;
}

static inline jobject to_PDFAnnotation_safe(fz_context *ctx, JNIEnv *env, pdf_annot *annot)
{
	jobject jannot;

	if (!ctx || !annot) return NULL;

	pdf_keep_annot(ctx, annot);
	jannot = (*env)->NewObject(env, cls_PDFAnnotation, mid_PDFAnnotation_init, jlong_cast(annot));
	if (!jannot)
		pdf_drop_annot(ctx, annot);

	return jannot;
}

static inline jobject to_PDFObject_safe(fz_context *ctx, JNIEnv *env, jobject pdf, pdf_obj *obj)
{
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	if (obj == NULL)
		return (*env)->GetStaticObjectField(env, cls_PDFObject, fid_PDFObject_Null);

	pdf_keep_obj(ctx, obj);
	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), pdf);
	if (!jobj)
		pdf_drop_obj(ctx, obj);

	return jobj;
}

static inline jobject to_Point_safe(fz_context *ctx, JNIEnv *env, fz_point point)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Point, mid_Point_init, point.x, point.y);
}

static inline jobject to_Quad_safe(fz_context *ctx, JNIEnv *env, fz_quad quad)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Quad, mid_Quad_init,
		quad.ul.x, quad.ul.y,
		quad.ur.x, quad.ur.y,
		quad.ll.x, quad.ll.y,
		quad.lr.x, quad.lr.y);
}

static inline jobjectArray to_jQuadArray_safe(fz_context *ctx, JNIEnv *env, const fz_quad *quads, jint n)
{
	jobjectArray arr;
	int i;

	if (!ctx || !quads) return NULL;

	arr = (*env)->NewObjectArray(env, n, cls_Quad, NULL);
	if (!arr) return NULL;

	for (i = 0; i < n; i++)
	{
		jobject jquad = to_Quad_safe(ctx, env, quads[i]);
		if (!jquad) return NULL;

		(*env)->SetObjectArrayElement(env, arr, i, jquad);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, jquad);
	}

	return arr;
}

static inline jobject to_Rect_safe(fz_context *ctx, JNIEnv *env, fz_rect rect)
{
	if (!ctx) return NULL;

	return (*env)->NewObject(env, cls_Rect, mid_Rect_init, rect.x0, rect.y0, rect.x1, rect.y1);
}

static inline jobjectArray to_StringArray_safe(fz_context *ctx, JNIEnv *env, const char **strings, int n)
{
	jobjectArray arr;
	int i;

	if (!ctx || !strings) return NULL;

	arr = (*env)->NewObjectArray(env, n, cls_String, NULL);
	if (!arr) return NULL;

	for (i = 0; i < n; i++)
	{
		jobject jstring;

		jstring = (*env)->NewStringUTF(env, strings[i]);
		if (!jstring) return NULL;

		(*env)->SetObjectArrayElement(env, arr, i, jstring);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, jstring);
	}

	return arr;
}

/* Conversion functions: C to Java. Take ownership of fitz object. None of these throw fitz exceptions. */

static inline jobject to_Document_safe_own(fz_context *ctx, JNIEnv *env, fz_document *doc)
{
	jobject obj;
	pdf_document *pdf;

	if (!ctx || !doc) return NULL;

	pdf = pdf_document_from_fz_document(ctx, doc);
	if (pdf)
		obj = (*env)->NewObject(env, cls_PDFDocument, mid_PDFDocument_init, jlong_cast(pdf));
	else
		obj = (*env)->NewObject(env, cls_Document, mid_Document_init, jlong_cast(doc));
	if (!obj)
		fz_drop_document(ctx, doc);

	return obj;
}

static inline jobject to_Device_safe_own(fz_context *ctx, JNIEnv *env, fz_device *device)
{
	jobject jdev;

	if (!ctx || !device) return NULL;

	jdev = (*env)->NewObject(env, cls_DisplayList, mid_Device_init, jlong_cast(device));
	if (!jdev)
		fz_drop_device(ctx, device);

	return jdev;
}

static inline jobject to_DisplayList_safe_own(fz_context *ctx, JNIEnv *env, fz_display_list *list)
{
	jobject jlist;

	if (!ctx || !list) return NULL;

	jlist = (*env)->NewObject(env, cls_DisplayList, mid_DisplayList_init, jlong_cast(list));
	if (!jlist)
		fz_drop_display_list(ctx, list);

	return jlist;
}

static inline jobject to_Page_safe_own(fz_context *ctx, JNIEnv *env, fz_page *page)
{
	jobject jobj;
	pdf_page *ppage;

	if (!ctx || !page) return NULL;

	ppage = pdf_page_from_fz_page(ctx, page);
	if (ppage)
		jobj = (*env)->NewObject(env, cls_PDFPage, mid_PDFPage_init, jlong_cast(page));
	else
		jobj = (*env)->NewObject(env, cls_Page, mid_Page_init, jlong_cast(page));
	if (!jobj)
		fz_drop_page(ctx, page);

	return jobj;
}

static inline jobject to_PDFAnnotation_safe_own(fz_context *ctx, JNIEnv *env, pdf_annot *annot)
{
	jobject jannot;

	if (!ctx || !annot) return NULL;

	jannot = (*env)->NewObject(env, cls_PDFAnnotation, mid_PDFAnnotation_init, jlong_cast(annot));
	if (!jannot)
		pdf_drop_annot(ctx, annot);

	return jannot;
}

static inline jobject to_PDFGraftMap_safe_own(fz_context *ctx, JNIEnv *env, jobject pdf, pdf_graft_map *map)
{
	jobject jmap;

	if (!ctx || !map || !pdf) return NULL;

	jmap = (*env)->NewObject(env, cls_PDFGraftMap, mid_PDFGraftMap_init, jlong_cast(map), pdf);
	if (!jmap)
		pdf_drop_graft_map(ctx, map);

	return jmap;
}

static inline jobject to_PDFObject_safe_own(fz_context *ctx, JNIEnv *env, jobject pdf, pdf_obj *obj)
{
	jobject jobj;

	if (!ctx || !obj || !pdf) return NULL;

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), pdf);
	if (!jobj)
		pdf_drop_obj(ctx, obj);

	return jobj;
}

static inline jobject to_PDFWidget(fz_context *ctx, JNIEnv *env, pdf_widget *widget)
{
	jobject jwidget;
	int nopts;
	const char **opts = NULL;
	jobjectArray jopts = NULL;

	fz_var(opts);

	pdf_keep_annot(ctx, widget);
	jwidget = (*env)->NewObject(env, cls_PDFWidget, mid_PDFWidget_init, jlong_cast(widget));
	if (!jwidget)
	{
		pdf_drop_annot(ctx, widget);
		return NULL;
	}

	fz_try(ctx)
	{
		int fieldType = pdf_widget_type(ctx, widget);
		int fieldFlags = pdf_field_flags(ctx, widget->obj);
		(*env)->SetIntField(env, jwidget, fid_PDFWidget_fieldType, fieldType);
		(*env)->SetIntField(env, jwidget, fid_PDFWidget_fieldFlags, fieldFlags);
		if (fieldType == PDF_WIDGET_TYPE_TEXT)
		{
			(*env)->SetIntField(env, jwidget, fid_PDFWidget_maxLen, pdf_text_widget_max_len(ctx, widget));
			(*env)->SetIntField(env, jwidget, fid_PDFWidget_textFormat, pdf_text_widget_format(ctx, widget));
		}
		if (fieldType == PDF_WIDGET_TYPE_COMBOBOX || fieldType == PDF_WIDGET_TYPE_LISTBOX)
		{
			nopts = pdf_choice_widget_options(ctx, widget, 0, NULL);
			if (nopts > 0)
			{
				opts = Memento_label(fz_malloc(ctx, nopts * sizeof(*opts)), "to_PDFWidget");
				pdf_choice_widget_options(ctx, widget, 0, opts);
				jopts = to_StringArray_safe(ctx, env, opts, nopts);
				if (jopts)
					(*env)->SetObjectField(env, jwidget, fid_PDFWidget_options, jopts);
			}
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, opts);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return jwidget;
}

static inline jobject to_Pixmap_safe_own(fz_context *ctx, JNIEnv *env, fz_pixmap *pixmap)
{
	jobject jobj;

	if (!ctx || !pixmap) return NULL;

	jobj = (*env)->NewObject(env, cls_Pixmap, mid_Pixmap_init, jlong_cast(pixmap));
	if (!jobj)
		fz_drop_pixmap(ctx, pixmap);

	return jobj;
}

static inline jobject to_StructuredText_safe_own(fz_context *ctx, JNIEnv *env, fz_stext_page *text)
{
	jobject jtext;

	if (!ctx || !text) return NULL;

	jtext = (*env)->NewObject(env, cls_StructuredText, mid_StructuredText_init, jlong_cast(text));
	if (!jtext)
		fz_drop_stext_page(ctx, text);

	return jtext;
}

/* Conversion functions: Java to C. These all throw java exceptions. */

static inline fz_buffer *from_Buffer(JNIEnv *env, jobject jobj)
{
	fz_buffer *buffer;
	if (!jobj) return NULL;
	buffer = CAST(fz_buffer *, (*env)->GetLongField(env, jobj, fid_Buffer_pointer));
	if (!buffer) jni_throw_null(env, "cannot use already destroyed Buffer");
	return buffer;
}

static inline fz_colorspace *from_ColorSpace(JNIEnv *env, jobject jobj)
{
	fz_colorspace *cs;
	if (!jobj) return NULL;
	cs = CAST(fz_colorspace *, (*env)->GetLongField(env, jobj, fid_ColorSpace_pointer));
	if (!cs) jni_throw_null(env, "cannot use already destroyed ColorSpace");
	return cs;
}

static inline fz_cookie *from_Cookie(JNIEnv *env, jobject jobj)
{
	fz_cookie *cookie;
	if (!jobj) return NULL;
	cookie = CAST(fz_cookie *, (*env)->GetLongField(env, jobj, fid_Cookie_pointer));
	if (!cookie) jni_throw_null(env, "cannot use already destroyed Cookie");
	return cookie;
}

static fz_device *from_Device(JNIEnv *env, jobject jobj)
{
	fz_device *dev;
	if (!jobj) return NULL;
	dev = CAST(fz_device *, (*env)->GetLongField(env, jobj, fid_Device_pointer));
	if (!dev) jni_throw_null(env, "cannot use already destroyed Device");
	return dev;
}

static inline fz_display_list *from_DisplayList(JNIEnv *env, jobject jobj)
{
	fz_display_list *list;
	if (!jobj) return NULL;
	list = CAST(fz_display_list *, (*env)->GetLongField(env, jobj, fid_DisplayList_pointer));
	if (!list) jni_throw_null(env, "cannot use already destroyed DisplayList");
	return list;
}

static inline fz_document *from_Document(JNIEnv *env, jobject jobj)
{
	fz_document *doc;
	if (!jobj) return NULL;
	doc = CAST(fz_document *, (*env)->GetLongField(env, jobj, fid_Document_pointer));
	if (!doc) jni_throw_null(env, "cannot use already destroyed Document");
	return doc;
}

static inline fz_document_writer *from_DocumentWriter(JNIEnv *env, jobject jobj)
{
	fz_document_writer *wri;
	if (!jobj) return NULL;
	wri = CAST(fz_document_writer *, (*env)->GetLongField(env, jobj, fid_DocumentWriter_pointer));
	if (!wri) jni_throw_null(env, "cannot use already destroyed DocumentWriter");
	return wri;
}

static inline fz_font *from_Font(JNIEnv *env, jobject jobj)
{
	fz_font *font;
	if (!jobj) return NULL;
	font = CAST(fz_font *, (*env)->GetLongField(env, jobj, fid_Font_pointer));
	if (!font) jni_throw_null(env, "cannot use already destroyed Font");
	return font;
}

static inline fz_image *from_Image(JNIEnv *env, jobject jobj)
{
	fz_image *image;
	if (!jobj) return NULL;
	image = CAST(fz_image *, (*env)->GetLongField(env, jobj, fid_Image_pointer));
	if (!image) jni_throw_null(env, "cannot use already destroyed Image");
	return image;
}

static inline fz_page *from_Page(JNIEnv *env, jobject jobj)
{
	fz_page *page;
	if (!jobj) return NULL;
	page = CAST(fz_page *, (*env)->GetLongField(env, jobj, fid_Page_pointer));
	if (!page) jni_throw_null(env, "cannot use already destroyed Page");
	return page;
}

static inline fz_path *from_Path(JNIEnv *env, jobject jobj)
{
	fz_path *path;
	if (!jobj) return NULL;
	path = CAST(fz_path *, (*env)->GetLongField(env, jobj, fid_Path_pointer));
	if (!path) jni_throw_null(env, "cannot use already destroyed Path");
	return path;
}

static inline pdf_annot *from_PDFAnnotation(JNIEnv *env, jobject jobj)
{
	pdf_annot *annot;
	if (!jobj) return NULL;
	annot = CAST(pdf_annot *, (*env)->GetLongField(env, jobj, fid_PDFAnnotation_pointer));
	if (!annot) jni_throw_null(env, "cannot use already destroyed PDFAnnotation");
	return annot;
}

static inline pdf_document *from_PDFDocument(JNIEnv *env, jobject jobj)
{
	pdf_document *pdf;
	if (!jobj) return NULL;
	pdf = CAST(pdf_document *, (*env)->GetLongField(env, jobj, fid_PDFDocument_pointer));
	if (!pdf) jni_throw_null(env, "cannot use already destroyed PDFDocument");
	return pdf;
}

static inline pdf_graft_map *from_PDFGraftMap(JNIEnv *env, jobject jobj)
{
	pdf_graft_map *map;
	if (!jobj) return NULL;
	map = CAST(pdf_graft_map *, (*env)->GetLongField(env, jobj, fid_PDFGraftMap_pointer));
	if (!map) jni_throw_null(env, "cannot use already destroyed PDFGraftMap");
	return map;
}

static inline pdf_obj *from_PDFObject(JNIEnv *env, jobject jobj)
{
	pdf_obj *obj;
	if (!jobj) return NULL;
	obj = CAST(pdf_obj *, (*env)->GetLongField(env, jobj, fid_PDFObject_pointer));
	return obj;
}

static inline pdf_page *from_PDFPage(JNIEnv *env, jobject jobj)
{
	pdf_page *page;
	if (!jobj) return NULL;
	page = CAST(pdf_page *, (*env)->GetLongField(env, jobj, fid_PDFPage_pointer));
	if (!page) jni_throw_null(env, "cannot use already destroyed PDFPage");
	return page;
}

static inline fz_pixmap *from_Pixmap(JNIEnv *env, jobject jobj)
{
	fz_pixmap *pixmap;
	if (!jobj) return NULL;
	pixmap = CAST(fz_pixmap *, (*env)->GetLongField(env, jobj, fid_Pixmap_pointer));
	if (!pixmap) jni_throw_null(env, "cannot use already destroyed Pixmap");
	return pixmap;
}

static inline fz_shade *from_Shade(JNIEnv *env, jobject jobj)
{
	fz_shade *shd;
	if (!jobj) return NULL;
	shd = CAST(fz_shade *, (*env)->GetLongField(env, jobj, fid_Shade_pointer));
	if (!shd) jni_throw_null(env, "cannot use already destroyed Shade");
	return shd;
}

static inline fz_stroke_state *from_StrokeState(JNIEnv *env, jobject jobj)
{
	fz_stroke_state *stroke;
	if (!jobj) return NULL;
	stroke = CAST(fz_stroke_state *, (*env)->GetLongField(env, jobj, fid_StrokeState_pointer));
	if (!stroke) jni_throw_null(env, "cannot use already destroyed StrokeState");
	return stroke;
}

static inline fz_stext_page *from_StructuredText(JNIEnv *env, jobject jobj)
{
	fz_stext_page *stext;
	if (!jobj) return NULL;
	stext = CAST(fz_stext_page *, (*env)->GetLongField(env, jobj, fid_StructuredText_pointer));
	if (!stext) jni_throw_null(env, "cannot use already destroyed StructuredText");
	return stext;
}

static inline fz_text *from_Text(JNIEnv *env, jobject jobj)
{
	fz_text *text;
	if (!jobj) return NULL;
	text = CAST(fz_text *, (*env)->GetLongField(env, jobj, fid_Text_pointer));
	if (!text) jni_throw_null(env, "cannot use already destroyed Text");
	return text;
}

static inline int from_jfloatArray(JNIEnv *env, float *color, jint n, jfloatArray jcolor)
{
	jsize len;

	if (!jcolor)
		len = 0;
	else
	{
		len = (*env)->GetArrayLength(env, jcolor);
		if (len > n)
			len = n;
		(*env)->GetFloatArrayRegion(env, jcolor, 0, len, color);
		if ((*env)->ExceptionCheck(env)) return 0;
	}

	if (len < n)
		memset(color+len, 0, (n - len) * sizeof(float));

	return 1;
}

static inline fz_matrix from_Matrix(JNIEnv *env, jobject jmat)
{
	fz_matrix mat;

	if (!jmat)
		return fz_identity;

	mat.a = (*env)->GetFloatField(env, jmat, fid_Matrix_a);
	mat.b = (*env)->GetFloatField(env, jmat, fid_Matrix_b);
	mat.c = (*env)->GetFloatField(env, jmat, fid_Matrix_c);
	mat.d = (*env)->GetFloatField(env, jmat, fid_Matrix_d);
	mat.e = (*env)->GetFloatField(env, jmat, fid_Matrix_e);
	mat.f = (*env)->GetFloatField(env, jmat, fid_Matrix_f);

	return mat;
}

static inline fz_point from_Point(JNIEnv *env, jobject jpt)
{
	fz_point pt;

	if (!jpt)
	{
		pt.x = pt.y = 0;
		return pt;
	}

	pt.x = (*env)->GetFloatField(env, jpt, fid_Point_x);
	pt.y = (*env)->GetFloatField(env, jpt, fid_Point_y);

	return pt;
}

static inline fz_rect from_Rect(JNIEnv *env, jobject jrect)
{
	fz_rect rect;

	if (!jrect)
		return fz_empty_rect;

	rect.x0 = (*env)->GetFloatField(env, jrect, fid_Rect_x0);
	rect.x1 = (*env)->GetFloatField(env, jrect, fid_Rect_x1);
	rect.y0 = (*env)->GetFloatField(env, jrect, fid_Rect_y0);
	rect.y1 = (*env)->GetFloatField(env, jrect, fid_Rect_y1);

	return rect;
}

static inline fz_quad from_Quad(JNIEnv *env, jobject jquad)
{
	fz_quad quad;

	if (!jquad)
		return fz_make_quad(0, 0, 0, 0, 0, 0, 0, 0);

	quad.ul.x = (*env)->GetFloatField(env, jquad, fid_Quad_ul_x);
	quad.ul.y = (*env)->GetFloatField(env, jquad, fid_Quad_ul_y);
	quad.ur.x = (*env)->GetFloatField(env, jquad, fid_Quad_ur_x);
	quad.ur.y = (*env)->GetFloatField(env, jquad, fid_Quad_ur_y);
	quad.ll.x = (*env)->GetFloatField(env, jquad, fid_Quad_ll_x);
	quad.ll.y = (*env)->GetFloatField(env, jquad, fid_Quad_ll_y);
	quad.lr.x = (*env)->GetFloatField(env, jquad, fid_Quad_lr_x);
	quad.lr.y = (*env)->GetFloatField(env, jquad, fid_Quad_lr_y);

	return quad;
}

/* Conversion functions: Java to C. None of these throw java exceptions. */

static inline fz_buffer *from_Buffer_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_buffer *, (*env)->GetLongField(env, jobj, fid_Buffer_pointer));
}

static inline fz_color_params from_ColorParams_safe(JNIEnv *env, jint params)
{
	fz_color_params p;

	p.bp = (params>>5) & 1;
	p.op = (params>>6) & 1;
	p.opm = (params>>7) & 1;
	p.ri = (params & 31);

	return p;
}

static inline fz_colorspace *from_ColorSpace_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_colorspace *, (*env)->GetLongField(env, jobj, fid_ColorSpace_pointer));
}

static inline fz_cookie *from_Cookie_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_cookie *, (*env)->GetLongField(env, jobj, fid_Cookie_pointer));
}

static fz_device *from_Device_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_device *, (*env)->GetLongField(env, jobj, fid_Device_pointer));
}

static inline fz_display_list *from_DisplayList_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_display_list *, (*env)->GetLongField(env, jobj, fid_DisplayList_pointer));
}

static inline fz_document *from_Document_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_document *, (*env)->GetLongField(env, jobj, fid_Document_pointer));
}

static inline fz_document_writer *from_DocumentWriter_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_document_writer *, (*env)->GetLongField(env, jobj, fid_DocumentWriter_pointer));
}

static inline fz_font *from_Font_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_font *, (*env)->GetLongField(env, jobj, fid_Font_pointer));
}

static inline fz_image *from_Image_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_image *, (*env)->GetLongField(env, jobj, fid_Image_pointer));
}

static inline fz_page *from_Page_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_page *, (*env)->GetLongField(env, jobj, fid_Page_pointer));
}

static inline fz_path *from_Path_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_path *, (*env)->GetLongField(env, jobj, fid_Path_pointer));
}

static inline pdf_annot *from_PDFAnnotation_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_annot *, (*env)->GetLongField(env, jobj, fid_PDFAnnotation_pointer));
}

static inline pdf_document *from_PDFDocument_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_document *, (*env)->GetLongField(env, jobj, fid_PDFDocument_pointer));
}

static inline pdf_graft_map *from_PDFGraftMap_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_graft_map *, (*env)->GetLongField(env, jobj, fid_PDFGraftMap_pointer));
}

static inline pdf_obj *from_PDFObject_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_obj *, (*env)->GetLongField(env, jobj, fid_PDFObject_pointer));
}

static inline pdf_widget *from_PDFWidget_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(pdf_widget *, (*env)->GetLongField(env, jobj, fid_PDFWidget_pointer));
}

static inline fz_pixmap *from_Pixmap_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_pixmap *, (*env)->GetLongField(env, jobj, fid_Pixmap_pointer));
}

static inline fz_shade *from_Shade_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_shade *, (*env)->GetLongField(env, jobj, fid_Shade_pointer));
}

static inline fz_stroke_state *from_StrokeState_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_stroke_state *, (*env)->GetLongField(env, jobj, fid_StrokeState_pointer));
}

static inline fz_stext_page *from_StructuredText_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_stext_page *, (*env)->GetLongField(env, jobj, fid_StructuredText_pointer));
}

static inline fz_text *from_Text_safe(JNIEnv *env, jobject jobj)
{
	if (!jobj) return NULL;
	return CAST(fz_text *, (*env)->GetLongField(env, jobj, fid_Text_pointer));
}

/* Callbacks to implement fz_stream and fz_output using Java classes */

typedef struct
{
	jobject stream;
	jbyteArray array;
	jbyte buffer[8192];
}
SeekableStreamState;

static int SeekableInputStream_next(fz_context *ctx, fz_stream *stm, size_t max)
{
	SeekableStreamState *state = stm->state;
	JNIEnv *env;
	int detach;
	int n, ch;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableInputStream_next");

	n = (*env)->CallIntMethod(env, state->stream, mid_SeekableInputStream_read, state->array);
	if ((*env)->ExceptionCheck(env)) {
		jni_detach_thread(detach);
		fz_throw_java(ctx, env);
	}

	if (n > 0)
	{
		(*env)->GetByteArrayRegion(env, state->array, 0, n, state->buffer);

		/* update stm->pos so fz_tell knows the current position */
		stm->rp = (unsigned char *)state->buffer;
		stm->wp = stm->rp + n;
		stm->pos += n;

		ch = *stm->rp++;
	}
	else if (n < 0)
	{
		ch = EOF;
	}
	else
	{
		jni_detach_thread(detach);
		fz_throw(ctx, FZ_ERROR_GENERIC, "no bytes read");
	}

	jni_detach_thread(detach);
	return ch;
}

static void SeekableOutputStream_write(fz_context *ctx, void *streamState_, const void *buffer_, size_t count)
{
	SeekableStreamState *state = streamState_;
	const jbyte *buffer = buffer_;
	JNIEnv *env;
	int detach;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_write");

	while (count > 0)
	{
		size_t n = fz_minz(count, sizeof(state->buffer));

		(*env)->SetByteArrayRegion(env, state->array, 0, n, buffer);
		buffer += n;
		count -= n;

		(*env)->CallVoidMethod(env, state->stream, mid_SeekableOutputStream_write, state->array, 0, n);
		if ((*env)->ExceptionCheck(env)) {
			jni_detach_thread(detach);
			fz_throw_java(ctx, env);
		}
	}

	jni_detach_thread(detach);
}

static int64_t SeekableOutputStream_tell(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	JNIEnv *env;
	int detach;
	int64_t pos = 0;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_tell");

	pos = (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_position);
	if ((*env)->ExceptionCheck(env)) {
		jni_detach_thread(detach);
		fz_throw_java(ctx, env);
	}

	jni_detach_thread(detach);

	return pos;
}

static void SeekableInputStream_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	SeekableStreamState *state = stm->state;
	JNIEnv *env;
	int detach;
	int64_t pos;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableInputStream_seek");

	pos = (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_seek, offset, whence);
	if ((*env)->ExceptionCheck(env)) {
		jni_detach_thread(detach);
		fz_throw_java(ctx, env);
	}

	stm->pos = pos;
	stm->rp = stm->wp = (unsigned char *)state->buffer;

	jni_detach_thread(detach);
}

static void SeekableOutputStream_seek(fz_context *ctx, void *streamState_, int64_t offset, int whence)
{
	SeekableStreamState *state = streamState_;
	JNIEnv *env;
	int detach;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_seek");

	(void) (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_seek, offset, whence);
	if ((*env)->ExceptionCheck(env)) {
		jni_detach_thread(detach);
		fz_throw_java(ctx, env);
	}

	jni_detach_thread(detach);
}

static void SeekableInputStream_drop(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	JNIEnv *env;
	int detach;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL) {
		fz_warn(ctx, "cannot attach to JVM in SeekableInputStream_drop; leaking input stream");
		return;
	}

	(*env)->DeleteGlobalRef(env, state->stream);
	(*env)->DeleteGlobalRef(env, state->array);

	fz_free(ctx, state);

	jni_detach_thread(detach);
}

static void SeekableOutputStream_drop(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	JNIEnv *env;
	int detach;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL) {
		fz_warn(ctx, "cannot attach to JVM in SeekableOutputStream_drop; leaking output stream");
		return;
	}

	(*env)->DeleteGlobalRef(env, state->stream);
	(*env)->DeleteGlobalRef(env, state->array);

	fz_free(ctx, state);

	jni_detach_thread(detach);
}

/*
	Devices can either be implemented in C, or in Java.
	We therefore have to think about 4 possible call combinations.

	1) C -> C:
	The standard mupdf case. No special worries here.
	2) C -> Java:
	This can only happen when we call run on a page/annotation/
	displaylist. We need to ensure that the java Device has an
	appropriate fz_java_device generated for it, which is done by the
	Device constructor. The 'run' calls take care to lock/unlock for us.
	3) Java -> C:
	The C device will have a java shim (a subclass of NativeDevice).
	All calls will go through the device methods in NativeDevice,
	which converts the java objects to C ones, and lock/unlock
	any underlying objects as required.
	4) Java -> Java:
	No special worries.
 */

typedef struct
{
	fz_device super;
	JNIEnv *env;
	jobject self;
}
fz_java_device;

static void
fz_java_device_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jfloatArray jcolor = to_jfloatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillPath, jpath, (jboolean)even_odd, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *state, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jfloatArray jcolor = to_jfloatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_strokePath, jpath, jstate, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipPath, jpath, (jboolean)even_odd, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *state, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jpath = to_Path(ctx, env, path);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipStrokePath, jpath, jstate, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jcolor = to_jfloatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillText, jtext, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *state, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jcolor = to_jfloatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_strokeText, jtext, jstate, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipText, jtext, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *state, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jstate = to_StrokeState(ctx, env, state);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipStrokeText, jtext, jstate, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jtext = to_Text(ctx, env, text);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_ignoreText, jtext, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shd, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jshd = to_Shade(ctx, env, shd);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillShade, jshd, jctm, alpha);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_image(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jimg = to_Image(ctx, env, img);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillImage, jimg, jctm, alpha);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jimg = to_Image(ctx, env, img);
	jobject jctm = to_Matrix(ctx, env, ctm);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jcolor = to_jfloatArray(ctx, env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_fillImageMask, jimg, jctm, jcs, jcolor, alpha, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, fz_rect scissor)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jimg = to_Image(ctx, env, img);
	jobject jctm = to_Matrix(ctx, env, ctm);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_clipImageMask, jimg, jctm);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_pop_clip(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_popClip);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_layer(fz_context *ctx, fz_device *dev, const char *name)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jstring jname;

	jname = (*env)->NewStringUTF(env, name);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginLayer, jname);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_layer(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endLayer);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_mask(fz_context *ctx, fz_device *dev, fz_rect rect, int luminosity, fz_colorspace *cs, const float *bc, fz_color_params cs_params)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jrect = to_Rect(ctx, env, rect);
	jobject jcs = to_ColorSpace(ctx, env, cs);
	jfloatArray jbc = to_jfloatArray(ctx, env, bc, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS);
	int jcp = to_ColorParams_safe(ctx, env, cs_params);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginMask, jrect, (jint)luminosity, jcs, jbc, jcp);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_mask(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endMask);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_begin_group(fz_context *ctx, fz_device *dev, fz_rect rect, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jrect = to_Rect(ctx, env, rect);
	jobject jcs = to_ColorSpace(ctx, env, cs);

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_beginGroup, jrect, jcs, (jboolean)isolated, (jboolean)knockout, (jint)blendmode, alpha);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_end_group(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endGroup);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static int
fz_java_device_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;
	jobject jarea = to_Rect(ctx, env, area);
	jobject jview = to_Rect(ctx, env, view);
	jobject jctm = to_Matrix(ctx, env, ctm);
	int res;

	res = (*env)->CallIntMethod(env, jdev->self, mid_Device_beginTile, jarea, jview, xstep, ystep, jctm, (jint)id);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);

	return res;
}

static void
fz_java_device_end_tile(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->CallVoidMethod(env, jdev->self, mid_Device_endTile);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
fz_java_device_drop(fz_context *ctx, fz_device *dev)
{
	fz_java_device *jdev = (fz_java_device *)dev;
	JNIEnv *env = jdev->env;

	(*env)->DeleteGlobalRef(env, jdev->self);
}

static fz_device *fz_new_java_device(fz_context *ctx, JNIEnv *env, jobject self)
{
	fz_java_device *dev = NULL;
	jobject jself;

	jself = (*env)->NewGlobalRef(env, self);
	if (!jself) return NULL;

	fz_try(ctx)
	{
		dev = fz_new_derived_device(ctx, fz_java_device);
		dev->env = env;
		dev->self = jself;

		dev->super.drop_device = fz_java_device_drop;

		dev->super.fill_path = fz_java_device_fill_path;
		dev->super.stroke_path = fz_java_device_stroke_path;
		dev->super.clip_path = fz_java_device_clip_path;
		dev->super.clip_stroke_path = fz_java_device_clip_stroke_path;

		dev->super.fill_text = fz_java_device_fill_text;
		dev->super.stroke_text = fz_java_device_stroke_text;
		dev->super.clip_text = fz_java_device_clip_text;
		dev->super.clip_stroke_text = fz_java_device_clip_stroke_text;
		dev->super.ignore_text = fz_java_device_ignore_text;

		dev->super.fill_shade = fz_java_device_fill_shade;
		dev->super.fill_image = fz_java_device_fill_image;
		dev->super.fill_image_mask = fz_java_device_fill_image_mask;
		dev->super.clip_image_mask = fz_java_device_clip_image_mask;

		dev->super.pop_clip = fz_java_device_pop_clip;

		dev->super.begin_mask = fz_java_device_begin_mask;
		dev->super.end_mask = fz_java_device_end_mask;
		dev->super.begin_group = fz_java_device_begin_group;
		dev->super.end_group = fz_java_device_end_group;

		dev->super.begin_tile = fz_java_device_begin_tile;
		dev->super.end_tile = fz_java_device_end_tile;

		dev->super.begin_layer = fz_java_device_begin_layer;
		dev->super.end_layer = fz_java_device_end_layer;
	}
	fz_catch(ctx)
	{
		fz_drop_device(ctx, &dev->super);
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (fz_device*)dev;
}

JNIEXPORT jlong JNICALL
FUN(Device_newNative)(JNIEnv *env, jclass self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		dev = fz_new_java_device(ctx, env, self);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(dev);
}

JNIEXPORT void JNICALL
FUN(Device_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device_safe(env, self);

	if (!ctx || !dev) return;

	fz_drop_device(ctx, dev);
}

/* Device Interface */

typedef struct NativeDeviceInfo NativeDeviceInfo;

typedef int (NativeDeviceLockFn)(JNIEnv *env, NativeDeviceInfo *info);
typedef void (NativeDeviceUnlockFn)(JNIEnv *env, NativeDeviceInfo *info);

struct NativeDeviceInfo
{
	/* Some devices (like the AndroidDrawDevice, or DrawDevice) need
	 * to lock/unlock the java object around device calls. We have functions
	 * here to do that. Other devices (like the DisplayList device) need
	 * no such locking, so these are NULL. */
	NativeDeviceLockFn *lock; /* Function to lock */
	NativeDeviceUnlockFn *unlock; /* Function to unlock */
	jobject object; /* The java object that needs to be locked. */

	/* Conceptually, we support drawing onto a 'plane' of pixels.
	 * The plane is width/height in size. The page is positioned on this
	 * at xOffset,yOffset. We want to redraw the given patch of this.
	 *
	 * The samples pointer in pixmap is updated on every lock/unlock, to
	 * cope with the object moving in memory.
	 */
	fz_pixmap *pixmap;
	int xOffset;
	int yOffset;
	int width;
	int height;
};

static NativeDeviceInfo *lockNativeDevice(JNIEnv *env, jobject self, int *err)
{
	NativeDeviceInfo *info = NULL;

	*err = 0;
	if (!(*env)->IsInstanceOf(env, self, cls_NativeDevice))
		return NULL;

	info = CAST(NativeDeviceInfo *, (*env)->GetLongField(env, self, fid_NativeDevice_nativeInfo));
	if (!info)
	{
		/* Some devices (like the Displaylist device) need no locking, so have no info. */
		return NULL;
	}
	info->object = (*env)->GetObjectField(env, self, fid_NativeDevice_nativeResource);

	if (info->lock(env, info))
	{
		*err = 1;
		return NULL;
	}

	return info;
}

static void unlockNativeDevice(JNIEnv *env, NativeDeviceInfo *info)
{
	if (info)
		info->unlock(env, info);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	NativeDeviceInfo *ninfo;

	if (!ctx) return;

	FUN(Device_finalize)(env, self); /* Call super.finalize() */

	ninfo = CAST(NativeDeviceInfo *, (*env)->GetLongField(env, self, fid_NativeDevice_nativeInfo));
	if (ninfo)
	{
		fz_drop_pixmap(ctx, ninfo->pixmap);
		fz_free(ctx, ninfo);
	}
}

JNIEXPORT void JNICALL
FUN(NativeDevice_close)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_close_device(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillPath)(JNIEnv *env, jobject self, jobject jpath, jboolean even_odd, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	int err;

	if (!ctx || !dev) return;
	if (!path) { jni_throw_arg(env, "path must not be null"); return; }
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_path(ctx, dev, path, even_odd, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_strokePath)(JNIEnv *env, jobject self, jobject jpath, jobject jstroke, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!path) { jni_throw_arg(env, "path must not be null"); return; }
	if (!stroke) { jni_throw_arg(env, "stroke must not be null"); return; }
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_stroke_path(ctx, dev, path, stroke, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipPath)(JNIEnv *env, jobject self, jobject jpath, jboolean even_odd, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!path) { jni_throw_arg(env, "path must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_path(ctx, dev, path, even_odd, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipStrokePath)(JNIEnv *env, jobject self, jobject jpath, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!path) { jni_throw_arg(env, "path must not be null"); return; }
	if (!stroke) { jni_throw_arg(env, "stroke must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_stroke_path(ctx, dev, path, stroke, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillText)(JNIEnv *env, jobject self, jobject jtext, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) { jni_throw_arg(env, "text must not be null"); return; }
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_text(ctx, dev, text, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_strokeText)(JNIEnv *env, jobject self, jobject jtext, jobject jstroke, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) { jni_throw_arg(env, "text must not be null"); return; }
	if (!stroke) { jni_throw_arg(env, "stroke must not be null"); return; }
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_stroke_text(ctx, dev, text, stroke, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipText)(JNIEnv *env, jobject self, jobject jtext, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) { jni_throw_arg(env, "text must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_text(ctx, dev, text, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipStrokeText)(JNIEnv *env, jobject self, jobject jtext, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) { jni_throw_arg(env, "text must not be null"); return; }
	if (!stroke) { jni_throw_arg(env, "stroke must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_stroke_text(ctx, dev, text, stroke, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_ignoreText)(JNIEnv *env, jobject self, jobject jtext, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) { jni_throw_arg(env, "text must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_ignore_text(ctx, dev, text, ctm);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillShade)(JNIEnv *env, jobject self, jobject jshd, jobject jctm, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_shade *shd = from_Shade(env, jshd);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!shd) { jni_throw_arg(env, "shade must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_shade(ctx, dev, shd, ctm, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillImage)(JNIEnv *env, jobject self, jobject jimg, jobject jctm, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_image *img = from_Image(env, jimg);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!img) { jni_throw_arg(env, "image must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_image(ctx, dev, img, ctm, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillImageMask)(JNIEnv *env, jobject self, jobject jimg, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_image *img = from_Image(env, jimg);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!img) { jni_throw_arg(env, "image must not be null"); return; }
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_image_mask(ctx, dev, img, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipImageMask)(JNIEnv *env, jobject self, jobject jimg, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_image *img = from_Image(env, jimg);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!img) { jni_throw_arg(env, "image must not be null"); return; }

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_image_mask(ctx, dev, img, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_popClip)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_pop_clip(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_beginLayer)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	const char *name;
	int err;

	if (!ctx || !dev) return;

	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_begin_layer(ctx, dev, name);
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jname, name);
		unlockNativeDevice(env, info);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endLayer)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_layer(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_beginMask)(JNIEnv *env, jobject self, jobject jrect, jboolean luminosity, jobject jcs, jfloatArray jcolor, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_rect rect = from_Rect(env, jrect);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_begin_mask(ctx, dev, rect, luminosity, cs, color, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endMask)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_mask(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_beginGroup)(JNIEnv *env, jobject self, jobject jrect, jobject jcs, jboolean isolated, jboolean knockout, jint blendmode, jfloat alpha)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_rect rect = from_Rect(env, jrect);
	fz_colorspace *cs = from_ColorSpace(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_begin_group(ctx, dev, rect, cs, isolated, knockout, blendmode, alpha);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endGroup)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_group(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(NativeDevice_beginTile)(JNIEnv *env, jobject self, jobject jarea, jobject jview, jfloat xstep, jfloat ystep, jobject jctm, jint id)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_rect area = from_Rect(env, jarea);
	fz_rect view = from_Rect(env, jview);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int i = 0;
	int err;

	if (!ctx || !dev) return 0;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return 0;
	fz_try(ctx)
		i = fz_begin_tile_id(ctx, dev, area, view, xstep, ystep, ctm, id);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return i;
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endTile)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_tile(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(DrawDevice_newNative)(JNIEnv *env, jclass self, jobject jpixmap)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, jpixmap);
	fz_device *device = NULL;

	if (!ctx) return 0;
	if (!pixmap) { jni_throw_arg(env, "pixmap must not be null"); return 0; }

	fz_try(ctx)
		device = fz_new_draw_device(ctx, fz_identity, pixmap);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(device);
}

JNIEXPORT jlong JNICALL
FUN(DisplayListDevice_newNative)(JNIEnv *env, jclass self, jobject jlist)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, jlist);
	fz_device *device = NULL;

	if (!ctx) return 0;

	fz_var(device);

	fz_try(ctx)
		device = fz_new_list_device(ctx, list);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(device);
}

#ifdef HAVE_ANDROID

static jlong
newNativeAndroidDrawDevice(JNIEnv *env, jobject self, fz_context *ctx, jobject obj, jint width, jint height, NativeDeviceLockFn *lock, NativeDeviceUnlockFn *unlock, jint xOrigin, jint yOrigin, jint patchX0, jint patchY0, jint patchX1, jint patchY1)
{
	fz_device *device = NULL;
	fz_pixmap *pixmap = NULL;
	unsigned char dummy;
	NativeDeviceInfo *ninfo = NULL;
	NativeDeviceInfo *info;
	fz_irect bbox;
	int err;

	if (!ctx) return 0;

	/* Ensure patch fits inside bitmap. */
	if (patchX0 < 0) patchX0 = 0;
	if (patchY0 < 0) patchY0 = 0;
	if (patchX1 > width) patchX1 = width;
	if (patchY1 > height) patchY1 = height;

	bbox.x0 = xOrigin + patchX0;
	bbox.y0 = yOrigin + patchY0;
	bbox.x1 = xOrigin + patchX1;
	bbox.y1 = yOrigin + patchY1;

	fz_var(pixmap);
	fz_var(ninfo);

	fz_try(ctx)
	{
		pixmap = fz_new_pixmap_with_bbox_and_data(ctx, fz_device_rgb(ctx), bbox, NULL, 1, &dummy);
		pixmap->stride = width * sizeof(int32_t);
		ninfo = Memento_label(fz_malloc(ctx, sizeof(*ninfo)), "newNativeAndroidDrawDevice");
		ninfo->pixmap = pixmap;
		ninfo->lock = lock;
		ninfo->unlock = unlock;
		ninfo->xOffset = patchX0;
		ninfo->yOffset = patchY0;
		ninfo->width = width;
		ninfo->height = height;
		ninfo->object = obj;
		(*env)->SetLongField(env, self, fid_NativeDevice_nativeInfo, jlong_cast(ninfo));
		(*env)->SetObjectField(env, self, fid_NativeDevice_nativeResource, obj);
		info = lockNativeDevice(env,self,&err);
		if (!err)
		{
			fz_clear_pixmap_with_value(ctx, pixmap, 0xff);
			unlockNativeDevice(env,ninfo);
			device = fz_new_draw_device(ctx, fz_identity, pixmap);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pixmap);
		fz_free(ctx, ninfo);
		jni_rethrow(env, ctx);
		return 0;
	}

	/* lockNativeDevice will already have raised a JNI error if there was one. */
	if (err)
	{
		fz_drop_pixmap(ctx, pixmap);
		fz_free(ctx, ninfo);
		return 0;
	}

	return jlong_cast(device);
}

static int androidDrawDevice_lock(JNIEnv *env, NativeDeviceInfo *info)
{
	uint8_t *pixels;
	int ret;
	int phase = 0;
	fz_context *ctx = get_context(env);
	size_t size = info->width * info->height * 4;

	assert(info);
	assert(info->object);

	while (1) {
		ret = AndroidBitmap_lockPixels(env, info->object, (void **)&pixels);
		if (ret == ANDROID_BITMAP_RESULT_SUCCESS)
			break;
		if (!fz_store_scavenge_external(ctx, size, &phase))
			break; /* Failed to free any */
	}
	if (ret != ANDROID_BITMAP_RESULT_SUCCESS)
	{
		info->pixmap->samples = NULL;
		jni_throw(env, FZ_ERROR_GENERIC, "bitmap lock failed in DrawDevice call");
		return 1;
	}

	/* Now offset pixels to allow for the page offsets */
	pixels += sizeof(int32_t) * (info->xOffset + info->width * info->yOffset);

	info->pixmap->samples = pixels;

	return 0;
}

static void androidDrawDevice_unlock(JNIEnv *env, NativeDeviceInfo *info)
{
	assert(info);
	assert(info->object);

	info->pixmap->samples = NULL;
	if (AndroidBitmap_unlockPixels(env, info->object) != ANDROID_BITMAP_RESULT_SUCCESS)
		jni_throw(env, FZ_ERROR_GENERIC, "bitmap unlock failed in DrawDevice call");
}

JNIEXPORT jlong JNICALL
FUN(android_AndroidDrawDevice_newNative)(JNIEnv *env, jclass self, jobject jbitmap, jint xOrigin, jint yOrigin, jint pX0, jint pY0, jint pX1, jint pY1)
{
	fz_context *ctx = get_context(env);
	AndroidBitmapInfo info;
	jlong device = 0;
	int ret;

	if (!ctx) return 0;
	if (!jbitmap) { jni_throw_arg(env, "bitmap must not be null"); return 0; }

	if ((ret = AndroidBitmap_getInfo(env, jbitmap, &info)) != ANDROID_BITMAP_RESULT_SUCCESS)
		jni_throw(env, FZ_ERROR_GENERIC, "new DrawDevice failed to get bitmap info");
	if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888)
		jni_throw(env, FZ_ERROR_GENERIC, "new DrawDevice failed as bitmap format is not RGBA_8888");
	if (info.stride != info.width * 4)
		jni_throw(env, FZ_ERROR_GENERIC, "new DrawDevice failed as bitmap width != stride");

	fz_try(ctx)
		device = newNativeAndroidDrawDevice(env, self, ctx, jbitmap, info.width, info.height, androidDrawDevice_lock, androidDrawDevice_unlock, xOrigin, yOrigin, pX0, pY0, pX1, pY1);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return device;
}

JNIEXPORT jlong JNICALL
FUN(AndroidImage_newImageFromBitmap)(JNIEnv *env, jobject self, jobject jbitmap, jlong jmask)
{
	fz_context *ctx = get_context(env);
	fz_image *mask = CAST(fz_image *, jmask);
	fz_image *image = NULL;
	fz_pixmap *pixmap = NULL;
	AndroidBitmapInfo info;
	void *pixels;
	int ret;

	if (!ctx) return 0;
	if (!jbitmap) { jni_throw_arg(env, "bitmap must not be null"); return 0; }

	if (mask && mask->mask)
		jni_throw(env, FZ_ERROR_GENERIC, "new Image failed as mask cannot be masked");
	if ((ret = AndroidBitmap_getInfo(env, jbitmap, &info)) != ANDROID_BITMAP_RESULT_SUCCESS)
		jni_throw(env, FZ_ERROR_GENERIC, "new Image failed to get bitmap info");
	if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888)
		jni_throw(env, FZ_ERROR_GENERIC, "new Image failed as bitmap format is not RGBA_8888");
	if (info.stride != info.width)
		jni_throw(env, FZ_ERROR_GENERIC, "new Image failed as bitmap width != stride");

	fz_var(pixmap);

	fz_try(ctx)
	{
		int ret;
		int phase = 0;
		size_t size = info.width * info.height * 4;
		pixmap = fz_new_pixmap(ctx, fz_device_rgb(ctx), info.width, info.height, NULL, 1);
		while (1) {
			ret = AndroidBitmap_lockPixels(env, jbitmap, (void **)&pixels);
			if (ret == ANDROID_BITMAP_RESULT_SUCCESS)
				break;
			if (!fz_store_scavenge_external(ctx, size, &phase))
				break; /* Failed to free any */
		}
		if (ret != ANDROID_BITMAP_RESULT_SUCCESS)
			fz_throw(ctx, FZ_ERROR_GENERIC, "bitmap lock failed in new Image");
		memcpy(pixmap->samples, pixels, info.width * info.height * 4);
		if (AndroidBitmap_unlockPixels(env, jbitmap) != ANDROID_BITMAP_RESULT_SUCCESS)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Bitmap unlock failed in new Image");
		image = fz_new_image_from_pixmap(ctx, fz_keep_pixmap(ctx, pixmap), fz_keep_image(ctx, mask));
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pixmap);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(image);
}
#endif

/* ColorSpace Interface */

JNIEXPORT void JNICALL
FUN(ColorSpace_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace_safe(env, self);
	if (!ctx || !cs) return;
	fz_drop_colorspace(ctx, cs);
}

JNIEXPORT jint JNICALL
FUN(ColorSpace_getNumberOfComponents)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, self);
	if (!ctx) return 0;
	return fz_colorspace_n(ctx, cs);
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceGray)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_gray(ctx));
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceRGB)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_rgb(ctx));
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceBGR)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_bgr(ctx));
}

JNIEXPORT jlong JNICALL
FUN(ColorSpace_nativeDeviceCMYK)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return 0;
	return jlong_cast(fz_device_cmyk(ctx));
}

/* Font interface */

JNIEXPORT void JNICALL
FUN(Font_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font_safe(env, self);

	if (!ctx || !font) return;

	fz_drop_font(ctx, font);
}

JNIEXPORT jlong JNICALL
FUN(Font_newNative)(JNIEnv *env, jobject self, jstring jname, jint index)
{
	fz_context *ctx = get_context(env);
	const char *name = NULL;
	fz_font *font = NULL;

	if (!ctx) return 0;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return 0;
	}

	fz_try(ctx)
	{
		const unsigned char *data;
		int size;

		data = fz_lookup_base14_font(ctx, name, &size);
		if (data)
			font = fz_new_font_from_memory(ctx, name, data, size, index, 0);
		else
			font = fz_new_font_from_file(ctx, name, name, index, 0);
	}
	fz_always(ctx)
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(font);
}

JNIEXPORT jstring JNICALL
FUN(Font_getName)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);

	if (!ctx || !font) return NULL;

	return (*env)->NewStringUTF(env, fz_font_name(ctx, font));
}

JNIEXPORT jint JNICALL
FUN(Font_encodeCharacter)(JNIEnv *env, jobject self, jint unicode)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	jint glyph = 0;

	if (!ctx || !font) return 0;

	fz_try(ctx)
		glyph = fz_encode_character(ctx, font, unicode);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return glyph;
}

JNIEXPORT jfloat JNICALL
FUN(Font_advanceGlyph)(JNIEnv *env, jobject self, jint glyph, jboolean wmode)
{
	fz_context *ctx = get_context(env);
	fz_font *font = from_Font(env, self);
	float advance = 0;

	if (!ctx || !font) return 0;

	fz_try(ctx)
		advance = fz_advance_glyph(ctx, font, glyph, wmode);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return advance;
}

/* Pixmap Interface */

JNIEXPORT void JNICALL
FUN(Pixmap_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap_safe(env, self);

	if (!ctx || !pixmap) return;

	fz_drop_pixmap(ctx, pixmap);
}

JNIEXPORT jlong JNICALL
FUN(Pixmap_newNative)(JNIEnv *env, jobject self, jobject jcs, jint x, jint y, jint w, jint h, jboolean alpha)
{
	fz_context *ctx = get_context(env);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_pixmap *pixmap = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
	{
		pixmap = fz_new_pixmap(ctx, cs, w, h, NULL, alpha);
		pixmap->x = x;
		pixmap->y = y;
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(pixmap);
}

JNIEXPORT void JNICALL
FUN(Pixmap_clear)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_clear_pixmap(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_clearWithValue)(JNIEnv *env, jobject self, jint value)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_clear_pixmap_with_value(ctx, pixmap, value);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_saveAsPNG)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	const char *filename = "null";

	if (!ctx) return;
	if (!jfilename) { jni_throw_arg(env, "filename must not be null"); return; }

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_pixmap_as_png(ctx, pixmap, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getX)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->x : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getY)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->y : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getWidth)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->w : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getHeight)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->h : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getNumberOfComponents)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->n : 0;
}

JNIEXPORT jboolean JNICALL
FUN(Pixmap_getAlpha)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap && pixmap->alpha ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getStride)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->stride : 0;
}

JNIEXPORT jobject JNICALL
FUN(Pixmap_getColorSpace)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	fz_colorspace *cs;

	if (!ctx | !pixmap) return NULL;

	fz_try(ctx)
		cs = fz_pixmap_colorspace(ctx, pixmap);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_ColorSpace_safe(ctx, env, cs);
}

JNIEXPORT jbyteArray JNICALL
FUN(Pixmap_getSamples)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	int size = pixmap->h * pixmap->stride;
	jbyteArray arr;

	if (!ctx | !pixmap) return NULL;

	arr = (*env)->NewByteArray(env, size);
	if (!arr) return NULL;

	(*env)->SetByteArrayRegion(env, arr, 0, size, (const jbyte *)pixmap->samples);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT jbyte JNICALL
FUN(Pixmap_getSample)(JNIEnv *env, jobject self, jint x, jint y, jint k)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx | !pixmap) return 0;

	if (x < 0 || x >= pixmap->w) { jni_throw_oob(env, "x out of range"); return 0; }
	if (y < 0 || y >= pixmap->h) { jni_throw_oob(env, "y out of range"); return 0; }
	if (k < 0 || k >= pixmap->n) { jni_throw_oob(env, "k out of range"); return 0; }

	return pixmap->samples[(x + y * pixmap->w) * pixmap->n + k];
}

JNIEXPORT jintArray JNICALL
FUN(Pixmap_getPixels)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);
	int size = pixmap->w * pixmap->h;
	jintArray arr;

	if (!ctx | !pixmap) return NULL;

	if (pixmap->n != 4 || !pixmap->alpha)
	{
		jni_throw(env, FZ_ERROR_GENERIC, "invalid colorspace for getPixels (must be RGB/BGR with alpha)");
		return NULL;
	}

	if (size * 4 != pixmap->h * pixmap->stride)
	{
		jni_throw(env, FZ_ERROR_GENERIC, "invalid stride for getPixels");
		return NULL;
	}

	arr = (*env)->NewIntArray(env, size);
	if (!arr) return NULL;

	(*env)->SetIntArrayRegion(env, arr, 0, size, (const jint *)pixmap->samples);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getXResolution)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->xres : 0;
}

JNIEXPORT jint JNICALL
FUN(Pixmap_getYResolution)(JNIEnv *env, jobject self)
{
	fz_pixmap *pixmap = from_Pixmap(env, self);
	return pixmap ? pixmap->yres : 0;
}

JNIEXPORT void JNICALL
FUN(Pixmap_invert)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_invert_pixmap(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_invertLuminance)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_invert_pixmap_luminance(ctx, pixmap);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Pixmap_gamma)(JNIEnv *env, jobject self, jfloat gamma)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, self);

	if (!ctx || !pixmap) return;

	fz_try(ctx)
		fz_gamma_pixmap(ctx, pixmap, gamma);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

/* Path Interface */

JNIEXPORT void JNICALL
FUN(Path_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path_safe(env, self);

	if (!ctx || !path) return;

	fz_drop_path(ctx, path);
}

JNIEXPORT jlong JNICALL
FUN(Path_newNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		path = fz_new_path(ctx);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(path);
}

JNIEXPORT jobject JNICALL
FUN(Path_currentPoint)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	fz_point point;

	if (!ctx || !path) return NULL;

	fz_try(ctx)
		point = fz_currentpoint(ctx, path);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Point_safe(ctx, env, point);
}

JNIEXPORT void JNICALL
FUN(Path_moveTo)(JNIEnv *env, jobject self, jfloat x, jfloat y)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_moveto(ctx, path, x, y);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_lineTo)(JNIEnv *env, jobject self, jfloat x, jfloat y)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_lineto(ctx, path, x, y);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_curveTo)(JNIEnv *env, jobject self, jfloat cx1, jfloat cy1, jfloat cx2, jfloat cy2, jfloat ex, jfloat ey)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_curveto(ctx, path, cx1, cy1, cx2, cy2, ex, ey);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_curveToV)(JNIEnv *env, jobject self, jfloat cx, jfloat cy, jfloat ex, jfloat ey)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_curvetov(ctx, path, cx, cy, ex, ey);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_curveToY)(JNIEnv *env, jobject self, jfloat cx, jfloat cy, jfloat ex, jfloat ey)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_curvetoy(ctx, path, cx, cy, ex, ey);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_rect)(JNIEnv *env, jobject self, jint x1, jint y1, jint x2, jint y2)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_rectto(ctx, path, x1, y1, x2, y2);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_closePath)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_closepath(ctx, path);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Path_transform)(JNIEnv *env, jobject self, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);

	if (!ctx || !path) return;

	fz_try(ctx)
		fz_transform_path(ctx, path, ctm);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(Path_cloneNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_path *old_path = from_Path(env, self);
	fz_path *new_path = NULL;

	if (!ctx || !old_path) return 0;

	fz_try(ctx)
		new_path = fz_clone_path(ctx, old_path);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(new_path);
}

JNIEXPORT jobject JNICALL
FUN(Path_getBounds)(JNIEnv *env, jobject self, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_rect rect;

	if (!ctx || !path) return NULL;
	if (!stroke) { jni_throw_arg(env, "stroke must not be null"); return NULL; }

	fz_try(ctx)
		rect = fz_bound_path(ctx, path, stroke, ctm);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Rect_safe(ctx, env, rect);
}

typedef struct
{
	JNIEnv *env;
	jobject obj;
} path_walker_state;

static void
pathWalkMoveTo(fz_context *ctx, void *arg, float x, float y)
{
	path_walker_state *state = (path_walker_state *)arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_moveTo, x, y);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
pathWalkLineTo(fz_context *ctx, void *arg, float x, float y)
{
	path_walker_state *state = (path_walker_state *)arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_lineTo, x, y);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
pathWalkCurveTo(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	path_walker_state *state = (path_walker_state *)arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_curveTo, x1, y1, x2, y2, x3, y3);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static void
pathWalkClosePath(fz_context *ctx, void *arg)
{
	path_walker_state *state = (path_walker_state *) arg;
	JNIEnv *env = state->env;
	(*env)->CallVoidMethod(env, state->obj, mid_PathWalker_closePath);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java(ctx, env);
}

static const fz_path_walker java_path_walker =
{
	pathWalkMoveTo,
	pathWalkLineTo,
	pathWalkCurveTo,
	pathWalkClosePath,
	NULL,
	NULL,
	NULL,
	NULL
};

JNIEXPORT void JNICALL
FUN(Path_walk)(JNIEnv *env, jobject self, jobject obj)
{
	fz_context *ctx = get_context(env);
	fz_path *path = from_Path(env, self);
	path_walker_state state;

	if (!ctx || !path) return;
	if (!obj) { jni_throw_arg(env, "object must not be null"); return; }

	state.env = env;
	state.obj = obj;

	fz_try(ctx)
		fz_walk_path(ctx, path, &java_path_walker, &state);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

/* Rect interface */

JNIEXPORT void JNICALL
FUN(Rect_adjustForStroke)(JNIEnv *env, jobject self, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_rect rect = from_Rect(env, self);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);

	if (!ctx) return;
	if (!stroke) { jni_throw_arg(env, "stroke must not be null"); return; }

	fz_try(ctx)
		rect = fz_adjust_rect_for_stroke(ctx, rect, stroke, ctm);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return;
	}

	(*env)->SetFloatField(env, self, fid_Rect_x0, rect.x0);
	(*env)->SetFloatField(env, self, fid_Rect_x1, rect.x1);
	(*env)->SetFloatField(env, self, fid_Rect_y0, rect.y0);
	(*env)->SetFloatField(env, self, fid_Rect_y1, rect.y1);
}

/* StrokeState interface */

JNIEXPORT void JNICALL
FUN(StrokeState_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stroke_state *stroke = from_StrokeState_safe(env, self);

	if (!ctx || !stroke) return;

	fz_drop_stroke_state(ctx, stroke);
}

JNIEXPORT jlong JNICALL
FUN(StrokeState_newStrokeState)(JNIEnv *env, jobject self, jint startCap, jint dashCap, jint endCap, jint lineJoin, jfloat lineWidth, jfloat miterLimit, jfloat dashPhase, jfloatArray dash)
{
	fz_context *ctx = get_context(env);
	fz_stroke_state *stroke = NULL;
	jsize len = 0;

	if (!ctx) return 0;
	if (!dash) { jni_throw_arg(env, "dash must not be null"); return 0; }

	len = (*env)->GetArrayLength(env, dash);

	fz_try(ctx)
	{
		stroke = fz_new_stroke_state_with_dash_len(ctx, len);
		stroke->start_cap = startCap;
		stroke->dash_cap = dashCap;
		stroke->end_cap = endCap;
		stroke->linejoin = lineJoin;
		stroke->linewidth = lineWidth;
		stroke->miterlimit = miterLimit;
		stroke->dash_phase = dashPhase;
		stroke->dash_len = len;
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	(*env)->GetFloatArrayRegion(env, dash, 0, len, &stroke->dash_list[0]);
	if ((*env)->ExceptionCheck(env)) return 0;

	return jlong_cast(stroke);
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getStartCap)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->start_cap : 0;
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getDashCap)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->dash_cap : 0;
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getEndCap)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->end_cap : 0;
}

JNIEXPORT jint JNICALL
FUN(StrokeState_getLineJoin)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->linejoin : 0;
}

JNIEXPORT float JNICALL
FUN(StrokeState_getLineWidth)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->linewidth : 0;
}

JNIEXPORT float JNICALL
FUN(StrokeState_getMiterLimit)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->miterlimit : 0;
}

JNIEXPORT float JNICALL
FUN(StrokeState_getDashPhase)(JNIEnv *env, jobject self)
{
	fz_stroke_state *stroke = from_StrokeState(env, self);
	return stroke ? stroke->dash_phase : 0;
}

JNIEXPORT jfloatArray JNICALL
FUN(StrokeState_getDashes)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stroke_state *stroke = from_StrokeState(env, self);
	jfloatArray arr;

	if (!ctx || !stroke) return NULL;

	if (stroke->dash_len == 0)
		return NULL; /* there are no dashes, so return NULL instead of empty array */

	arr = (*env)->NewFloatArray(env, stroke->dash_len);
	if (!arr) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, stroke->dash_len, &stroke->dash_list[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

/* Text interface */

JNIEXPORT void JNICALL
FUN(Text_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_text *text = from_Text_safe(env, self);

	if (!ctx || !text) return;

	fz_drop_text(ctx, text);
}

JNIEXPORT jlong JNICALL
FUN(Text_newNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_text *text = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		text = fz_new_text(ctx);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(text);
}

JNIEXPORT jobject JNICALL
FUN(Text_getBounds)(JNIEnv *env, jobject self, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_text *text = from_Text(env, self);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_rect rect;

	if (!ctx || !text) return NULL;
	if (!stroke) { jni_throw_arg(env, "stroke must not be null"); return NULL; }

	fz_try(ctx)
		rect = fz_bound_text(ctx, text, stroke, ctm);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT void JNICALL
FUN(Text_showGlyph)(JNIEnv *env, jobject self, jobject jfont, jobject jtrm, jint glyph, jint unicode, jboolean wmode)
{
	fz_context *ctx = get_context(env);
	fz_text *text = from_Text(env, self);
	fz_font *font = from_Font(env, jfont);
	fz_matrix trm = from_Matrix(env, jtrm);

	if (!ctx || !text) return;
	if (!font) { jni_throw_arg(env, "font must not be null"); return; }

	fz_try(ctx)
		fz_show_glyph(ctx, text, font, trm, glyph, unicode, wmode, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Text_showString)(JNIEnv *env, jobject self, jobject jfont, jobject jtrm, jstring jstr, jboolean wmode)
{
	fz_context *ctx = get_context(env);
	fz_text *text = from_Text(env, self);
	fz_font *font = from_Font(env, jfont);
	fz_matrix trm = from_Matrix(env, jtrm);
	const char *str = NULL;

	if (!ctx || !text) return;
	if (!jfont) { jni_throw_arg(env, "font must not be null"); return; }
	if (!jstr) { jni_throw_arg(env, "string must not be null"); return; }

	str = (*env)->GetStringUTFChars(env, jstr, NULL);
	if (!str) return;

	fz_try(ctx)
		trm = fz_show_string(ctx, text, font, trm, str, wmode, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jstr, str);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return;
	}

	(*env)->SetFloatField(env, jtrm, fid_Matrix_e, trm.e);
	(*env)->SetFloatField(env, jtrm, fid_Matrix_f, trm.f);
}

JNIEXPORT void JNICALL
FUN(Text_walk)(JNIEnv *env, jobject self, jobject walker)
{
	fz_context *ctx = get_context(env);
	fz_text *text = from_Text(env, self);
	fz_text_span *span;
	fz_font *font = NULL;
	jobject jfont = NULL;
	jobject jtrm = NULL;
	int i;

	if (!ctx || !text) return;
	if (!walker) { jni_throw_arg(env, "walker must not be null"); return; }

	if (text->head == NULL)
		return; /* text has no spans to walk */

	for (span = text->head; span; span = span->next)
	{
		if (font != span->font)
		{
			if (jfont)
				(*env)->DeleteLocalRef(env, jfont);
			font = span->font;
			jfont = to_Font_safe(ctx, env, font);
			if (!jfont) return;
		}

		for (i = 0; i < span->len; ++i)
		{
			jtrm = (*env)->NewObject(env, cls_Matrix, mid_Matrix_init,
					span->trm.a, span->trm.b, span->trm.c, span->trm.d,
					span->items[i].x, span->items[i].y);
			if (!jtrm) return;

			(*env)->CallVoidMethod(env, walker, mid_TextWalker_showGlyph,
					jfont, jtrm,
					(jint)span->items[i].gid,
					(jint)span->items[i].ucs,
					(jint)span->wmode);
			if ((*env)->ExceptionCheck(env)) return;

			(*env)->DeleteLocalRef(env, jtrm);
		}
	}
}

/* Image interface */

JNIEXPORT void JNICALL
FUN(Image_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *image = from_Image_safe(env, self);

	if (!ctx || !image) return;

	fz_drop_image(ctx, image);
}

JNIEXPORT jlong JNICALL
FUN(Image_newNativeFromPixmap)(JNIEnv *env, jobject self, jobject jpixmap)
{
	fz_context *ctx = get_context(env);
	fz_pixmap *pixmap = from_Pixmap(env, jpixmap);
	fz_image *image = NULL;

	if (!ctx) return 0;
	if (!pixmap) { jni_throw_arg(env, "pixmap must not be null"); return 0; }

	fz_try(ctx)
		image = fz_new_image_from_pixmap(ctx, pixmap, NULL);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(image);
}

JNIEXPORT jlong JNICALL
FUN(Image_newNativeFromFile)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	const char *filename = "null";
	fz_image *image = NULL;

	if (!ctx) return 0;
	if (!jfilename) { jni_throw_arg(env, "filename must not be null"); return 0; }

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return 0;

	fz_try(ctx)
		image = fz_new_image_from_file(ctx, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(image);
}

JNIEXPORT jint JNICALL
FUN(Image_getWidth)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->w : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getHeight)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->h : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getNumberOfComponents)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->n : 0;
}

JNIEXPORT jobject JNICALL
FUN(Image_getColorSpace)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *image = from_Image(env, self);
	if (!ctx || !image) return NULL;
	return to_ColorSpace_safe(ctx, env, image->colorspace);
}

JNIEXPORT jint JNICALL
FUN(Image_getBitsPerComponent)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->bpc : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getXResolution)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->xres : 0;
}

JNIEXPORT jint JNICALL
FUN(Image_getYResolution)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image ? image->yres : 0;
}

JNIEXPORT jboolean JNICALL
FUN(Image_getImageMask)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image && image->imagemask ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(Image_getInterpolate)(JNIEnv *env, jobject self)
{
	fz_image *image = from_Image(env, self);
	return image && image->interpolate ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
FUN(Image_getMask)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *img = from_Image(env, self);

	if (!ctx || !img) return NULL;

	return to_Image_safe(ctx, env, img->mask);
}

JNIEXPORT jobject JNICALL
FUN(Image_toPixmap)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_image *img = from_Image(env, self);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !img) return NULL;

	fz_try(ctx)
		pixmap = fz_get_pixmap_from_image(ctx, img, NULL, NULL, NULL, NULL);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

/* Annotation Interface */

JNIEXPORT void JNICALL
FUN(PDFAnnotation_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation_safe(env, self);

	if (!ctx || !annot) return;

	pdf_drop_annot(ctx, annot);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_run)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie= from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !annot) return;
	if (!dev) { jni_throw_arg(env, "device must not be null"); return; }

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		pdf_run_annot(ctx, annot, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_toPixmap)(JNIEnv *env, jobject self, jobject jctm, jobject jcs, jboolean alpha)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pixmap = pdf_new_pixmap_from_annot(ctx, annot, ctm, cs, NULL, alpha);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getBounds)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		rect = pdf_bound_annot(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_toDisplayList)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_display_list *list = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		list = pdf_new_display_list_from_annot(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_DisplayList_safe_own(ctx, env, list);
}

/* Document interface */

JNIEXPORT void JNICALL
FUN(Document_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document_safe(env, self);

	if (!ctx || !doc) return;

	fz_drop_document(ctx, doc);

	/* This is a reasonable place to call Memento. */
	Memento_fin();
}

JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithStream)(JNIEnv *env, jclass cls, jstring jmagic, jobject jdocument, jobject jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	fz_stream *docstream = NULL;
	fz_stream *accstream = NULL;
	jobject jdoc = NULL;
	jobject jacc = NULL;
	jbyteArray docarray = NULL;
	jbyteArray accarray = NULL;
	SeekableStreamState *docstate = NULL;
	SeekableStreamState *accstate = NULL;
	const char *magic = NULL;

	fz_var(jdoc);
	fz_var(jacc);
	fz_var(docarray);
	fz_var(accarray);
	fz_var(docstream);
	fz_var(accstream);

	if (!ctx) return NULL;
	if (jmagic)
	{
		magic = (*env)->GetStringUTFChars(env, jmagic, NULL);
		if (!magic)
		{
			jni_throw_run(env, "cannot get characters in magic string");
			return NULL;
		}
	}
	if (jdocument)
	{
		jdoc = (*env)->NewGlobalRef(env, jdocument);
		if (!jdoc)
		{
			if (magic)
				(*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get reference to document stream");
			return NULL;
		}
	}
	if (jaccelerator)
	{
		jacc = (*env)->NewGlobalRef(env, jaccelerator);
		if (!jacc)
		{
			(*env)->DeleteGlobalRef(env, jdoc);
			if (magic)
				(*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get reference to accelerator stream");
			return NULL;
		}
	}

	docarray = (*env)->NewByteArray(env, sizeof docstate->buffer);
	if (docarray)
		docarray = (*env)->NewGlobalRef(env, docarray);
	if (!docarray)
	{
		(*env)->DeleteGlobalRef(env, jacc);
		(*env)->DeleteGlobalRef(env, jdoc);
		if (magic)
			(*env)->ReleaseStringUTFChars(env, jmagic, magic);
		jni_throw_run(env, "cannot create internal buffer for document stream");
		return NULL;
	}

	accarray = (*env)->NewByteArray(env, sizeof accstate->buffer);
	if (accarray)
		accarray = (*env)->NewGlobalRef(env, accarray);
	if (!accarray)
	{
		(*env)->DeleteGlobalRef(env, docarray);
		(*env)->DeleteGlobalRef(env, jacc);
		(*env)->DeleteGlobalRef(env, jdoc);
		if (magic)
			(*env)->ReleaseStringUTFChars(env, jmagic, magic);
		jni_throw_run(env, "cannot create internal buffer for accelerator stream");
		return NULL;
	}

	fz_try(ctx)
	{
		if (jdoc)
		{
			/* No exceptions can occur from here to stream owning docstate, so we must not free docstate. */
			docstate = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_docstate");
			docstate->stream = jdoc;
			docstate->array = docarray;

			/* Ownership transferred to docstate. */
			jdoc = NULL;
			docarray = NULL;

			/* Stream takes ownership of docstate. */
			docstream = fz_new_stream(ctx, docstate, SeekableInputStream_next, SeekableInputStream_drop);
			docstream->seek = SeekableInputStream_seek;
		}

		if (jacc)
		{
			/* No exceptions can occur from here to stream owning accstate, so we must not free accstate. */
			accstate = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_accstate");
			accstate->stream = jacc;
			accstate->array = accarray;

			/* Ownership transferred to accstate. */
			jacc = NULL;
			accarray = NULL;

			/* Stream takes ownership of accstate. */
			accstream = fz_new_stream(ctx, accstate, SeekableInputStream_next, SeekableInputStream_drop);
			accstream->seek = SeekableInputStream_seek;
		}

		doc = fz_open_accelerated_document_with_stream(ctx, magic, docstream, accstream);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, accstream);
		fz_drop_stream(ctx, docstream);
		if (magic)
			(*env)->ReleaseStringUTFChars(env, jmagic, magic);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, accarray);
		(*env)->DeleteGlobalRef(env, docarray);
		(*env)->DeleteGlobalRef(env, jacc);
		(*env)->DeleteGlobalRef(env, jdoc);
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Document_safe_own(ctx, env, doc);
}

JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithPath)(JNIEnv *env, jclass cls, jstring jfilename, jstring jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	const char *filename = NULL;
	const char *accelerator = NULL;

	if (!ctx) return NULL;
	if (jfilename)
	{
		filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
		if (!filename)
		{
			jni_throw_run(env, "cannot get characters in filename string");
			return NULL;
		}
	}
	if (jaccelerator)
	{
		accelerator = (*env)->GetStringUTFChars(env, jaccelerator, NULL);
		if (!accelerator)
		{
			jni_throw_run(env, "cannot get characters in accelerator filename string");
			return NULL;
		}
	}

	fz_try(ctx)
		doc = fz_open_accelerated_document(ctx, filename, accelerator);
	fz_always(ctx)
	{
		if (accelerator)
			(*env)->ReleaseStringUTFChars(env, jaccelerator, accelerator);
		if (filename)
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return to_Document_safe_own(ctx, env, doc);
}


JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithPathAndStream)(JNIEnv *env, jclass cls, jstring jfilename, jobject jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	const char *filename = NULL;
	fz_stream *docstream = NULL;
	fz_stream *accstream = NULL;
	jobject jacc = NULL;
	jbyteArray accarray = NULL;
	SeekableStreamState *accstate = NULL;

	fz_var(jacc);
	fz_var(accarray);
	fz_var(accstream);
	fz_var(docstream);

	if (!ctx) return NULL;
	if (jfilename)
	{
		filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
		if (!filename)
		{
			jni_throw_run(env, "cannot get characters in filename string");
			return NULL;
		}
	}
	if (jaccelerator)
	{
		jacc = (*env)->NewGlobalRef(env, jaccelerator);
		if (!jacc)
		{
			if (jfilename)
				(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			jni_throw_run(env, "cannot get reference to accelerator stream");
			return NULL;
		}
	}

	accarray = (*env)->NewByteArray(env, sizeof accstate->buffer);
	if (accarray)
		accarray = (*env)->NewGlobalRef(env, accarray);
	if (!accarray)
	{
		(*env)->DeleteGlobalRef(env, jacc);
		if (jfilename)
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
		jni_throw_run(env, "cannot get create internal buffer for accelerator stream");
		return NULL;
	}

	fz_try(ctx)
	{
		if (filename)
			docstream = fz_open_file(ctx, filename);

		if (jacc)
		{
			/* No exceptions can occur from here to stream owning accstate, so we must not free accstate. */
			accstate = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_accstate2");
			accstate->stream = jacc;
			accstate->array = accarray;

			/* Ownership transferred to accstate. */
			jacc = NULL;
			accarray = NULL;

			/* Stream takes ownership of accstate. */
			accstream = fz_new_stream(ctx, accstate, SeekableInputStream_next, SeekableInputStream_drop);
			accstream->seek = SeekableInputStream_seek;
		}

		doc = fz_open_accelerated_document_with_stream(ctx, filename, docstream, accstream);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, accstream);
		fz_drop_stream(ctx, docstream);
		if (filename)
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, accarray);
		(*env)->DeleteGlobalRef(env, jacc);
		jni_rethrow(env, ctx);
		return 0;
	}

	return to_Document_safe_own(ctx, env, doc);
}

JNIEXPORT jobject JNICALL
FUN(Document_openNativeWithBuffer)(JNIEnv *env, jclass cls, jstring jmagic, jobject jbuffer, jobject jaccelerator)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = NULL;
	const char *magic = NULL;
	fz_stream *docstream = NULL;
	fz_stream *accstream = NULL;
	fz_buffer *docbuf = NULL;
	fz_buffer *accbuf = NULL;
	jbyte *buffer = NULL;
	jbyte *accelerator = NULL;
	int n, m;

	fz_var(docbuf);
	fz_var(accbuf);
	fz_var(docstream);
	fz_var(accstream);

	if (!ctx) return NULL;
	if (jmagic)
	{
		magic = (*env)->GetStringUTFChars(env, jmagic, NULL);
		if (!magic)
		{
			jni_throw_run(env, "cannot get characters in magic string");
			return NULL;
		}
	}
	if (jbuffer)
	{
		n = (*env)->GetArrayLength(env, jbuffer);

		buffer = (*env)->GetByteArrayElements(env, jbuffer, NULL);
		if (!buffer) {
			if (magic)
				(*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get document bytes to read");
			return NULL;
		}
	}
	if (jaccelerator)
	{
		m = (*env)->GetArrayLength(env, jaccelerator);

		accelerator = (*env)->GetByteArrayElements(env, jaccelerator, NULL);
		if (!accelerator) {
			if (buffer)
				(*env)->ReleaseByteArrayElements(env, jbuffer, buffer, 0);
			if (magic)
				(*env)->ReleaseStringUTFChars(env, jmagic, magic);
			jni_throw_run(env, "cannot get accelerator bytes to read");
			return NULL;
		}
	}

	fz_try(ctx)
	{
		if (buffer)
		{
			docbuf = fz_new_buffer(ctx, n);
			fz_append_data(ctx, docbuf, buffer, n);
			docstream = fz_open_buffer(ctx, docbuf);
		}

		if (accelerator)
		{
			accbuf = fz_new_buffer(ctx, m);
			fz_append_data(ctx, accbuf, accelerator, m);
			accstream = fz_open_buffer(ctx, accbuf);
		}

		doc = fz_open_accelerated_document_with_stream(ctx, magic, docstream, accstream);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, accstream);
		fz_drop_buffer(ctx, accbuf);
		fz_drop_stream(ctx, docstream);
		fz_drop_buffer(ctx, docbuf);
		if (accelerator)
			(*env)->ReleaseByteArrayElements(env, jaccelerator, accelerator, 0);
		if (buffer)
			(*env)->ReleaseByteArrayElements(env, jbuffer, buffer, 0);
		if (magic)
			(*env)->ReleaseStringUTFChars(env, jmagic, magic);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Document_safe_own(ctx, env, doc);
}

JNIEXPORT jboolean JNICALL
FUN(Document_recognize)(JNIEnv *env, jclass self, jstring jmagic)
{
	fz_context *ctx = get_context(env);
	const char *magic = NULL;
	jboolean recognized = JNI_FALSE;

	if (!ctx) return JNI_FALSE;
	if (jmagic)
	{
		magic = (*env)->GetStringUTFChars(env, jmagic, NULL);
		if (!magic) return JNI_FALSE;
	}

	fz_try(ctx)
		recognized = fz_recognize_document(ctx, magic) != NULL;
	fz_always(ctx)
		if (magic)
			(*env)->ReleaseStringUTFChars(env, jmagic, magic);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return recognized;
}

JNIEXPORT void JNICALL
FUN(Document_saveAccelerator)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *filename = "null";

	if (!ctx || !doc) return;
	if (!jfilename) { jni_throw_arg(env, "filename must not be null"); return; }

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	fz_try(ctx)
		fz_save_accelerator(ctx, doc, filename);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Document_outputAccelerator)(JNIEnv *env, jobject self, jobject jstream)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	SeekableStreamState *state = NULL;
	jobject stream = NULL;
	jbyteArray array = NULL;
	fz_output *out;

	fz_var(state);
	fz_var(out);
	fz_var(stream);
	fz_var(array);

	stream = (*env)->NewGlobalRef(env, jstream);
	if (!stream)
		return;

	array = (*env)->NewByteArray(env, sizeof state->buffer);
	if (array)
		array = (*env)->NewGlobalRef(env, array);
	if (!array)
	{
		(*env)->DeleteGlobalRef(env, stream);
		return;
	}

	fz_try(ctx)
	{
		state = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreamState_outputAccelerator");
		state->stream = stream;
		state->array = array;

		out = fz_new_output(ctx, 8192, state, SeekableOutputStream_write, NULL, SeekableOutputStream_drop);
		out->seek = SeekableOutputStream_seek;
		out->tell = SeekableOutputStream_tell;

		/* these are now owned by 'out' */
		state = NULL;
		stream = NULL;
		array = NULL;

		fz_output_accelerator(ctx, doc, out);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, stream);
		(*env)->DeleteGlobalRef(env, array);
		fz_free(ctx, state);
		jni_rethrow(env, ctx);
	}
}

JNIEXPORT jboolean JNICALL
FUN(Document_needsPassword)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int okay = 0;

	if (!ctx || !doc) return JNI_FALSE;

	fz_try(ctx)
		okay = fz_needs_password(ctx, doc);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return okay ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(Document_authenticatePassword)(JNIEnv *env, jobject self, jstring jpassword)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *password = NULL;
	int okay = 0;

	if (!ctx || !doc) return JNI_FALSE;

	if (jpassword)
	{
		password = (*env)->GetStringUTFChars(env, jpassword, NULL);
		if (!password) return JNI_FALSE;
	}

	fz_try(ctx)
		okay = fz_authenticate_password(ctx, doc, password);
	fz_always(ctx)
		if (password)
			(*env)->ReleaseStringUTFChars(env, jpassword, password);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return okay ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
FUN(Document_countChapters)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int count = 0;

	if (!ctx || !doc) return 0;

	fz_try(ctx)
		count = fz_count_chapters(ctx, doc);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return count;
}

JNIEXPORT jint JNICALL
FUN(Document_countPages)(JNIEnv *env, jobject self, jint chapter)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int count = 0;

	if (!ctx || !doc) return 0;

	fz_try(ctx)
		count = fz_count_chapter_pages(ctx, doc, chapter);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return count;
}

JNIEXPORT jboolean JNICALL
FUN(Document_isReflowable)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	int is_reflowable = 0;

	if (!ctx || !doc) return JNI_FALSE;

	fz_try(ctx)
		is_reflowable = fz_is_document_reflowable(ctx, doc);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return is_reflowable ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
FUN(Document_layout)(JNIEnv *env, jobject self, jfloat w, jfloat h, jfloat em)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);

	if (!ctx || !doc) return;

	fz_try(ctx)
		fz_layout_document(ctx, doc, w, h, em);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(Document_loadPage)(JNIEnv *env, jobject self, jint chapter, jint number)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_page *page = NULL;

	if (!ctx || !doc) return NULL;

	fz_try(ctx)
		page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Page_safe_own(ctx, env, page);
}

JNIEXPORT jobject JNICALL
FUN(Document_getMetaData)(JNIEnv *env, jobject self, jstring jkey)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	const char *key = NULL;
	char info[256];

	if (!ctx || !doc) return NULL;
	if (!jkey) { jni_throw_arg(env, "key must not be null"); return NULL; }

	key = (*env)->GetStringUTFChars(env, jkey, NULL);
	if (!key) return 0;

	fz_try(ctx)
		fz_lookup_metadata(ctx, doc, key, info, sizeof info);
	fz_always(ctx)
		if (key)
			(*env)->ReleaseStringUTFChars(env, jkey, key);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (*env)->NewStringUTF(env, info);
}

JNIEXPORT jboolean JNICALL
FUN(Document_isUnencryptedPDF)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	pdf_document *idoc = pdf_specifics(ctx, doc);
	int cryptVer;

	if (!ctx || !doc) return JNI_FALSE;
	if (!idoc)
		return JNI_FALSE;

	cryptVer = pdf_crypt_version(ctx, idoc->crypt);
	return (cryptVer == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
FUN(Document_loadOutline)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_outline *outline = NULL;
	jobject joutline = NULL;

	if (!ctx || !doc) return NULL;

	fz_var(outline);

	fz_try(ctx)
		outline = fz_load_outline(ctx, doc);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	if (outline)
	{
		joutline = to_Outline_safe(ctx, env, doc, outline);
		if (!joutline)
			jni_throw(env, FZ_ERROR_GENERIC, "loadOutline failed");
		fz_drop_outline(ctx, outline);
	}

	return joutline;
}

JNIEXPORT jlong JNICALL
FUN(Document_makeBookmark)(JNIEnv *env, jobject self, jint chapter, jint page)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_bookmark mark = 0;

	fz_try(ctx)
		mark = fz_make_bookmark(ctx, doc, fz_make_location(chapter, page));
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return mark;
}

JNIEXPORT jobject JNICALL
FUN(Document_findBookmark)(JNIEnv *env, jobject self, jlong mark)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_location loc = { -1, -1 };

	fz_try(ctx)
		loc = fz_lookup_bookmark(ctx, doc, mark);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (*env)->NewObject(env, cls_Location, mid_Location_init, loc.chapter, loc.page, 0, 0);
}

JNIEXPORT jobject JNICALL
FUN(Document_resolveLink)(JNIEnv *env, jobject self, jstring juri)
{
	fz_context *ctx = get_context(env);
	fz_document *doc = from_Document(env, self);
	fz_location loc = { -1, -1 };
	float x = 0, y = 0;
	const char *uri = "";

	if (juri)
	{
		uri = (*env)->GetStringUTFChars(env, juri, NULL);
		if (!uri)
			return NULL;
	}

	fz_try(ctx)
		loc = fz_resolve_link(ctx, doc, uri, &x, &y);
	fz_always(ctx)
		if (juri)
			(*env)->ReleaseStringUTFChars(env, juri, uri);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (*env)->NewObject(env, cls_Location, mid_Location_init, loc.chapter, loc.page, x, y);
}

/* Page interface */

JNIEXPORT void JNICALL
FUN(Page_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page_safe(env, self);

	if (!ctx || !page) return;

	fz_drop_page(ctx, page);
}

JNIEXPORT jobject JNICALL
FUN(Page_toPixmap)(JNIEnv *env, jobject self, jobject jctm, jobject jcs, jboolean alpha, jboolean showExtra)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !page) return NULL;

	fz_try(ctx)
	{
		if (showExtra)
			pixmap = fz_new_pixmap_from_page(ctx, page, ctm, cs, alpha);
		else
			pixmap = fz_new_pixmap_from_page_contents(ctx, page, ctm, cs, alpha);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(Page_getBounds)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_rect rect;

	if (!ctx || !page) return NULL;

	fz_try(ctx)
		rect = fz_bound_page(ctx, page);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT void JNICALL
FUN(Page_run)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) { jni_throw_arg(env, "device must not be null"); return; }

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Page_runPageContents)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) { jni_throw_arg(env, "device must not be null"); return; }

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page_contents(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Page_runPageAnnots)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) { jni_throw_arg(env, "device must not be null"); return; }

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page_annots(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Page_runPageWidgets)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) { jni_throw_arg(env, "device must not be null"); return; }

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page_widgets(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFPage_getAnnotations)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_page *page = from_PDFPage(env, self);
	pdf_annot *annot = NULL;
	pdf_annot *annots = NULL;
	jobject jannots = NULL;
	int annot_count;
	int i;

	if (!ctx || !page) return NULL;

	/* count the annotations */
	fz_try(ctx)
	{
		annots = pdf_first_annot(ctx, page);

		annot = annots;
		for (annot_count = 0; annot; annot_count++)
			annot = pdf_next_annot(ctx, annot);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	/* no annotations, return NULL instead of empty array */
	if (annot_count == 0)
		return NULL;

	/* now run through actually creating the annotation objects */
	jannots = (*env)->NewObjectArray(env, annot_count, cls_PDFAnnotation, NULL);
	if (!jannots) return NULL;

	annot = annots;
	for (i = 0; annot && i < annot_count; i++)
	{
		jobject jannot = to_PDFAnnotation_safe(ctx, env, annot);
		if (!jannot) return NULL;

		(*env)->SetObjectArrayElement(env, jannots, i, jannot);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, jannot);

		fz_try(ctx)
			annot = pdf_next_annot(ctx, annot);
		fz_catch(ctx)
		{
			jni_rethrow(env, ctx);
			return NULL;
		}
	}

	return jannots;
}

JNIEXPORT jobjectArray JNICALL
FUN(PDFPage_getWidgetsNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_page *page = from_PDFPage(env, self);
	pdf_widget *widget;
	jobjectArray jwidgets = NULL;
	int count = 0;

	if (!ctx || !page)
		return NULL;

	fz_try(ctx)
	{
		for (widget = pdf_first_widget(ctx, page); widget; widget = pdf_next_widget(ctx, widget))
			count++;
	}
	fz_catch(ctx)
	{
		count = 0;
	}

	if (count == 0)
		return NULL;

	jwidgets = (*env)->NewObjectArray(env, count, cls_PDFWidget, NULL);
	if (!jwidgets)
		return NULL;

	fz_try(ctx)
	{
		int i = 0;

		for (widget = pdf_first_widget(ctx, page); widget; widget = pdf_next_widget(ctx, widget))
		{
			jobject jwidget;

			jwidget = to_PDFWidget(ctx, env, widget);
			if (!jwidget)
				fz_throw_java(ctx, env);

			(*env)->SetObjectArrayElement(env, jwidgets, i, jwidget);
			if ((*env)->ExceptionCheck(env))
				fz_throw_java(ctx, env);

			(*env)->DeleteLocalRef(env, jwidget);
			i++;
		}
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return jwidgets;
}

JNIEXPORT jobject JNICALL
FUN(Page_getLinks)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_link *link = NULL;
	fz_link *links = NULL;
	jobject jlinks = NULL;
	int link_count;
	int i;

	if (!ctx || !page) return NULL;

	fz_var(links);

	fz_try(ctx)
		links = fz_load_links(ctx, page);
	fz_catch(ctx)
	{
		fz_drop_link(ctx, links);
		jni_rethrow(env, ctx);
		return NULL;
	}

	/* count the links */
	link = links;
	for (link_count = 0; link; link_count++)
		link = link->next;

	/* no links, return NULL instead of empty array */
	if (link_count == 0)
	{
		fz_drop_link(ctx, links);
		return NULL;
	}

	/* now run through actually creating the link objects */
	jlinks = (*env)->NewObjectArray(env, link_count, cls_Link, NULL);
	if (!jlinks) return NULL;

	link = links;
	for (i = 0; link && i < link_count; i++)
	{
		jobject jbounds = NULL;
		jobject jlink = NULL;
		jobject juri = NULL;

		jbounds = to_Rect_safe(ctx, env, link->rect);
		if (!jbounds) return NULL;

		juri = (*env)->NewStringUTF(env, link->uri);
		if (!juri) return NULL;

		jlink = (*env)->NewObject(env, cls_Link, mid_Link_init, jbounds, juri);
		(*env)->DeleteLocalRef(env, jbounds);
		if (!jlink) return NULL;
		if (juri)
			(*env)->DeleteLocalRef(env, juri);

		(*env)->SetObjectArrayElement(env, jlinks, i, jlink);
		if ((*env)->ExceptionCheck(env)) return NULL;

		(*env)->DeleteLocalRef(env, jlink);
		link = link->next;
	}

	fz_drop_link(ctx, links);

	return jlinks;
}

JNIEXPORT jobject JNICALL
FUN(Page_search)(JNIEnv *env, jobject self, jstring jneedle)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_quad hits[256];
	const char *needle = NULL;
	int n = 0;

	if (!ctx || !page) return NULL;
	if (!jneedle) { jni_throw_arg(env, "needle must not be null"); return NULL; }

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return 0;

	fz_try(ctx)
		n = fz_search_page(ctx, page, needle, hits, nelem(hits));
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_jQuadArray_safe(ctx, env, hits, n);
}

JNIEXPORT jobject JNICALL
FUN(Page_toDisplayList)(JNIEnv *env, jobject self, jboolean showExtra)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_display_list *list = NULL;

	if (!ctx || !page) return NULL;

	fz_try(ctx)
		if (showExtra)
			list = fz_new_display_list_from_page(ctx, page);
		else
			list = fz_new_display_list_from_page_contents(ctx, page);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_DisplayList_safe_own(ctx, env, list);
}

JNIEXPORT jobject JNICALL
FUN(Page_toStructuredText)(JNIEnv *env, jobject self, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_stext_page *text = NULL;
	const char *options= NULL;
	fz_stext_options opts;

	if (!ctx || !page) return NULL;

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options) return NULL;
	}

	fz_try(ctx)
	{
		fz_parse_stext_options(ctx, &opts, options);
		text = fz_new_stext_page_from_page(ctx, page, &opts);
	}
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_StructuredText_safe_own(ctx, env, text);
}

JNIEXPORT jbyteArray JNICALL
FUN(Page_textAsHtml)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_stext_page *text = NULL;
	fz_device *dev = NULL;
	fz_matrix ctm;
	jbyteArray arr = NULL;
	fz_buffer *buf = NULL;
	fz_output *out = NULL;
	unsigned char *data;
	size_t len;

	if (!ctx || !page) return NULL;

	fz_var(text);
	fz_var(dev);
	fz_var(buf);
	fz_var(out);

	fz_try(ctx)
	{
		ctm = fz_identity;
		text = fz_new_stext_page(ctx, fz_bound_page(ctx, page));
		dev = fz_new_stext_device(ctx, text, NULL);
		fz_run_page(ctx, page, dev, ctm, NULL);
		fz_close_device(ctx, dev);

		buf = fz_new_buffer(ctx, 256);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_header_as_html(ctx, out);
		fz_print_stext_page_as_html(ctx, out, text, page->number);
		fz_print_stext_trailer_as_html(ctx, out);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
		fz_drop_device(ctx, dev);
		fz_drop_stext_page(ctx, text);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		jni_rethrow(env, ctx);
		return NULL;
	}

	len = fz_buffer_storage(ctx, buf, &data);
	arr = (*env)->NewByteArray(env, (jsize)len);
	if (arr)
	{
		(*env)->SetByteArrayRegion(env, arr, 0, (jsize)len, (jbyte *)data);
	}
	fz_drop_buffer(ctx, buf);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

/* Cookie interface */

JNIEXPORT void JNICALL
FUN(Cookie_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie_safe(env, self);

	if (!ctx || !cookie) return;

	fz_free(ctx, cookie);
}

JNIEXPORT jlong JNICALL
FUN(Cookie_newNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		cookie = fz_malloc_struct(ctx, fz_cookie);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(cookie);
}

JNIEXPORT void JNICALL
FUN(Cookie_abort)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_cookie *cookie = from_Cookie(env, self);

	if (!ctx || !cookie) return;

	cookie->abort = 1;
}

/* DisplayList interface */

JNIEXPORT jlong JNICALL
FUN(DisplayList_newNative)(JNIEnv *env, jobject self, jobject jmediabox)
{
	fz_context *ctx = get_context(env);
	fz_rect mediabox = from_Rect(env, jmediabox);

	fz_display_list *list = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		list = fz_new_display_list(ctx, mediabox);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(list);
}

JNIEXPORT void JNICALL
FUN(DisplayList_run)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jrect, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	fz_rect rect;
	int err;

	if (!ctx || !list) return;
	if (!dev) { jni_throw_arg(env, "device must not be null"); return; }

	/* Use a scissor rectangle if one is supplied */
	if (jrect)
		rect = from_Rect(env, jrect);
	else
		rect = fz_infinite_rect;

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_display_list(ctx, list, dev, ctm, rect, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DisplayList_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList_safe(env, self);

	if (!ctx || !list) return;

	fz_drop_display_list(ctx, list);
}

JNIEXPORT jobject JNICALL
FUN(DisplayList_toPixmap)(JNIEnv *env, jobject self, jobject jctm, jobject jcs, jboolean alpha)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !list) return NULL;

	fz_try(ctx)
		pixmap = fz_new_pixmap_from_display_list(ctx, list, ctm, cs, alpha);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(DisplayList_toStructuredText)(JNIEnv *env, jobject self, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_stext_page *text = NULL;
	const char *options= NULL;
	fz_stext_options opts;

	if (!ctx || !list) return NULL;

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options) return NULL;
	}

	fz_try(ctx)
	{
		fz_parse_stext_options(ctx, &opts, options);
		text = fz_new_stext_page_from_display_list(ctx, list, &opts);
	}
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_StructuredText_safe_own(ctx, env, text);
}

JNIEXPORT jobject JNICALL
FUN(DisplayList_search)(JNIEnv *env, jobject self, jstring jneedle)
{
	fz_context *ctx = get_context(env);
	fz_display_list *list = from_DisplayList(env, self);
	fz_quad hits[256];
	const char *needle = NULL;
	int n = 0;

	if (!ctx || !list) return NULL;
	if (!jneedle) { jni_throw_arg(env, "needle must not be null"); return NULL; }

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return NULL;

	fz_try(ctx)
		n = fz_search_display_list(ctx, list, needle, hits, nelem(hits));
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_jQuadArray_safe(ctx, env, hits, n);
}

/* Buffer interface */

JNIEXPORT void JNICALL
FUN(Buffer_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer_safe(env, self);

	if (!ctx || !buf) return;

	fz_drop_buffer(ctx, buf);
}

JNIEXPORT jlong JNICALL
FUN(Buffer_newNativeBuffer)(JNIEnv *env, jobject self, jint n)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		buf = fz_new_buffer(ctx, n);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(buf);
}

JNIEXPORT jint JNICALL
FUN(Buffer_getLength)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);

	if (!ctx || !buf) return -1;

	return (jint)fz_buffer_storage(ctx, buf, NULL);
}

JNIEXPORT jint JNICALL
FUN(Buffer_readByte)(JNIEnv *env, jobject self, jint jat)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	size_t at = (size_t) jat;
	size_t len;
	unsigned char *data;

	if (!ctx || !buf) return -1;
	if (jat < 0) { jni_throw_oob(env, "at is negative"); return -1; }

	len = fz_buffer_storage(ctx, buf, &data);
	if (at >= len)
		return -1;

	return data[at];
}

JNIEXPORT jint JNICALL
FUN(Buffer_readBytes)(JNIEnv *env, jobject self, jint jat, jobject jbs)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	size_t at = (size_t) jat;
	jbyte *bs = NULL;
	size_t len;
	size_t remaining_input = 0;
	size_t remaining_output = 0;
	unsigned char *data;

	if (!ctx || !buf) return -1;
	if (jat < 0) { jni_throw_oob(env, "at is negative"); return -1; }
	if (!jbs) { jni_throw_arg(env, "buffer must not be null"); return -1; }

	len = fz_buffer_storage(ctx, buf, &data);
	if (at >= len)
		return -1;

	remaining_input = len - at;
	remaining_output = (*env)->GetArrayLength(env, jbs);
	len = fz_minz(0, remaining_output);
	len = fz_minz(len, remaining_input);

	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) { jni_throw_io(env, "cannot get bytes to read"); return -1; }

	memcpy(bs, &data[at], len);
	(*env)->ReleaseByteArrayElements(env, jbs, bs, 0);

	return (jint)len;
}

JNIEXPORT jint JNICALL
FUN(Buffer_readBytesInto)(JNIEnv *env, jobject self, jint jat, jobject jbs, jint joff, jint jlen)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	size_t at = (size_t) jat;
	jbyte *bs = NULL;
	size_t off = (size_t) joff;
	size_t len = (size_t) jlen;
	size_t bslen = 0;
	size_t blen;
	unsigned char *data;

	if (!ctx || !buf) return -1;
	if (jat < 0) { jni_throw_oob(env, "at is negative"); return -1; }
	if (!jbs) { jni_throw_arg(env, "buffer must not be null"); return -1; }
	if (joff < 0) { jni_throw_oob(env, "offset is negative"); return -1; }
	if (jlen < 0) { jni_throw_oob(env, "length is negative"); return -1; }

	bslen = (*env)->GetArrayLength(env, jbs);
	if (len > bslen - off) { jni_throw_oob(env, "offset + length is outside of buffer"); return -1; }

	blen = fz_buffer_storage(ctx, buf, &data);
	if (at >= blen)
		return -1;

	len = fz_minz(len, blen - at);

	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) { jni_throw_io(env, "cannot get bytes to read"); return -1; }

	memcpy(&bs[off], &data[at], len);
	(*env)->ReleaseByteArrayElements(env, jbs, bs, 0);

	return (jint)len;
}

JNIEXPORT void JNICALL
FUN(Buffer_writeByte)(JNIEnv *env, jobject self, jbyte b)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);

	if (!ctx || !buf) return;

	fz_try(ctx)
		fz_append_byte(ctx, buf, b);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeBytes)(JNIEnv *env, jobject self, jobject jbs)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	jsize len = 0;
	jbyte *bs = NULL;

	if (!ctx || !buf) return;
	if (!jbs) { jni_throw_arg(env, "buffer must not be null"); return; }

	len = (*env)->GetArrayLength(env, jbs);
	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) { jni_throw_io(env, "cannot get bytes to write"); return; }

	fz_try(ctx)
		fz_append_data(ctx, buf, bs, len);
	fz_always(ctx)
		(*env)->ReleaseByteArrayElements(env, jbs, bs, JNI_ABORT);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeBytesFrom)(JNIEnv *env, jobject self, jobject jbs, jint joff, jint jlen)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	jbyte *bs = NULL;
	jsize off = (jsize) joff;
	jsize len = (jsize) jlen;
	jsize bslen = 0;

	if (!ctx || !buf) return;
	if (!jbs) { jni_throw_arg(env, "buffer must not be null"); return; }

	bslen = (*env)->GetArrayLength(env, jbs);
	if (joff < 0)
	{
		jni_throw_oob(env, "offset is negative");
		return;
	}
	if (jlen < 0)
	{
		jni_throw_oob(env, "length is negative");
		return;
	}
	if (off + len >= bslen)
	{
		jni_throw_oob(env, "offset + length is outside of buffer");
		return;
	}

	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) { jni_throw_io(env, "cannot get bytes to write"); return; }

	fz_try(ctx)
		fz_append_data(ctx, buf, &bs[off], len);
	fz_always(ctx)
		(*env)->ReleaseByteArrayElements(env, jbs, bs, JNI_ABORT);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeBuffer)(JNIEnv *env, jobject self, jobject jbuf)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	fz_buffer *cat = from_Buffer(env, jbuf);

	if (!ctx || !buf) return;
	if (!cat) { jni_throw_arg(env, "buffer must not be null"); return; }

	fz_try(ctx)
		fz_append_buffer(ctx, buf, cat);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeRune)(JNIEnv *env, jobject self, jint rune)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);

	if (!ctx || !buf) return;

	fz_try(ctx)
		fz_append_rune(ctx, buf, rune);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeLine)(JNIEnv *env, jobject self, jstring jline)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	const char *line = NULL;

	if (!ctx || !buf) return;
	if (!jline) { jni_throw_arg(env, "line must not be null"); return; }

	line = (*env)->GetStringUTFChars(env, jline, NULL);
	if (!line) return;

	fz_try(ctx)
	{
		fz_append_string(ctx, buf, line);
		fz_append_byte(ctx, buf, '\n');
	}
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jline, line);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Buffer_writeLines)(JNIEnv *env, jobject self, jobject jlines)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	int i = 0;
	jsize len = 0;

	if (!ctx || !buf) return;
	if (!jlines) { jni_throw_arg(env, "lines must not be null"); return; }

	len = (*env)->GetArrayLength(env, jlines);

	for (i = 0; i < len; ++i)
	{
		const char *line;
		jobject jline;

		jline = (*env)->GetObjectArrayElement(env, jlines, i);
		if ((*env)->ExceptionCheck(env)) return;

		if (!jline)
			continue;
		line = (*env)->GetStringUTFChars(env, jline, NULL);
		if (!line) return;

		fz_try(ctx)
		{
			fz_append_string(ctx, buf, line);
			fz_append_byte(ctx, buf, '\n');
		}
		fz_always(ctx)
			(*env)->ReleaseStringUTFChars(env, jline, line);
		fz_catch(ctx)
		{
			jni_rethrow(env, ctx);
			return;
		}
	}
}

JNIEXPORT void JNICALL
FUN(Buffer_save)(JNIEnv *env, jobject self, jstring jfilename)
{
	fz_context *ctx = get_context(env);
	fz_buffer *buf = from_Buffer(env, self);
	const char *filename = NULL;

	if (!ctx || !buf) return;
	if (jfilename)
	{
		filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
		if (!filename) return;
	}

	fz_try(ctx)
		fz_save_buffer(ctx, buf, filename);
	fz_always(ctx)
		if (filename)
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

/* DocumentWriter interface */

JNIEXPORT void JNICALL
FUN(DocumentWriter_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter_safe(env, self);

	if (!ctx || !wri) return;

	fz_drop_document_writer(ctx, wri);
}

JNIEXPORT jlong JNICALL
FUN(DocumentWriter_newNativeDocumentWriter)(JNIEnv *env, jobject self, jstring jfilename, jstring jformat, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);
	const char *filename = NULL;
	const char *format = NULL;
	const char *options = NULL;

	if (!ctx || !wri) return 0;
	if (!jfilename) { jni_throw_arg(env, "filename must not be null"); return 0; }

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return 0;

	if (jformat)
	{
		format = (*env)->GetStringUTFChars(env, jformat, NULL);
		if (!format)
		{
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			return 0;
		}
	}
	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options)
		{
			if (format)
				(*env)->ReleaseStringUTFChars(env, jformat, format);
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			return 0;
		}
	}

	fz_try(ctx)
		wri = fz_new_document_writer(ctx, filename, format, options);
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
		if (format)
			(*env)->ReleaseStringUTFChars(env, jformat, format);
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(wri);
}

JNIEXPORT jobject JNICALL
FUN(DocumentWriter_beginPage)(JNIEnv *env, jobject self, jobject jmediabox)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);
	fz_rect mediabox = from_Rect(env, jmediabox);
	fz_device *device = NULL;

	if (!ctx || !wri) return NULL;

	fz_try(ctx)
		device = fz_begin_page(ctx, wri, mediabox);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Device_safe_own(ctx, env, device);
}

JNIEXPORT void JNICALL
FUN(DocumentWriter_endPage)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);

	if (!ctx || !wri) return;

	fz_try(ctx)
		fz_end_page(ctx, wri);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(DocumentWriter_close)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_document_writer *wri = from_DocumentWriter(env, self);

	if (!ctx || !wri) return;

	fz_try(ctx)
		fz_close_document_writer(ctx, wri);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

/* StructuredText interface */

JNIEXPORT void JNICALL
FUN(StructuredText_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText_safe(env, self);

	if (!ctx || !text) return;

	fz_drop_stext_page(ctx, text);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_search)(JNIEnv *env, jobject self, jstring jneedle)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_quad hits[256];
	const char *needle = NULL;
	int n = 0;

	if (!ctx || !text) return NULL;
	if (!jneedle) { jni_throw_arg(env, "needle must not be null"); return NULL; }

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return NULL;

	fz_try(ctx)
		n = fz_search_stext_page(ctx, text, needle, hits, nelem(hits));
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_jQuadArray_safe(ctx, env, hits, n);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_highlight)(JNIEnv *env, jobject self, jobject jpt1, jobject jpt2)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_point pt1 = from_Point(env, jpt1);
	fz_point pt2 = from_Point(env, jpt2);
	fz_quad hits[1000];
	int n = 0;

	if (!ctx || !text) return NULL;

	fz_try(ctx)
		n = fz_highlight_selection(ctx, text, pt1, pt2, hits, nelem(hits));
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_jQuadArray_safe(ctx, env, hits, n);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_snapSelection)(JNIEnv *env, jobject self, jobject jpt1, jobject jpt2, jint mode)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_point pt1 = from_Point(env, jpt1);
	fz_point pt2 = from_Point(env, jpt2);
	fz_quad quad;

	if (!ctx || !text) return NULL;

	fz_try(ctx)
		quad = fz_snap_selection(ctx, text, &pt1, &pt2, mode);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	(*env)->SetFloatField(env, jpt1, fid_Point_x, pt1.x);
	(*env)->SetFloatField(env, jpt1, fid_Point_y, pt1.y);
	(*env)->SetFloatField(env, jpt2, fid_Point_x, pt2.x);
	(*env)->SetFloatField(env, jpt2, fid_Point_y, pt2.y);

	return to_Quad_safe(ctx, env, quad);
}

JNIEXPORT jobject JNICALL
FUN(StructuredText_copy)(JNIEnv *env, jobject self, jobject jpt1, jobject jpt2)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *text = from_StructuredText(env, self);
	fz_point pt1 = from_Point(env, jpt1);
	fz_point pt2 = from_Point(env, jpt2);
	jobject jstring = NULL;
	char *s = NULL;

	if (!ctx || !text) return NULL;

	fz_var(s);

	fz_try(ctx)
		s = fz_copy_selection(ctx, text, pt1, pt2, 0);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jstring = (*env)->NewStringUTF(env, s);
	fz_free(ctx, s);

	return jstring;
}

JNIEXPORT void JNICALL
FUN(StructuredText_walk)(JNIEnv *env, jobject self, jobject walker)
{
	fz_context *ctx = get_context(env);
	fz_stext_page *page = from_StructuredText(env, self);
	fz_stext_block *block = NULL;
	fz_stext_line *line = NULL;
	fz_stext_char *ch = NULL;
	jobject jbbox = NULL;
	jobject jtrm = NULL;
	jobject jimage = NULL;
	jobject jorigin = NULL;
	jobject jfont = NULL;
	jobject jquad = NULL;

	if (!ctx || !page) return;
	if (!walker) { jni_throw_arg(env, "walker must not be null"); return; }

	if (page->first_block == NULL)
		return; /* structured text has no blocks to walk */

	for (block = page->first_block; block; block = block->next)
	{
		jbbox = to_Rect_safe(ctx, env, block->bbox);
		if (!jbbox) return;

		if (block->type == FZ_STEXT_BLOCK_IMAGE)
		{
			jtrm = to_Matrix_safe(ctx, env, block->u.i.transform);
			if (!jtrm) return;

			jimage = to_Image_safe(ctx, env, block->u.i.image);
			if (!jimage) return;

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_onImageBlock, jbbox, jtrm, jimage);
			if ((*env)->ExceptionCheck(env)) return;

			(*env)->DeleteLocalRef(env, jbbox);
			(*env)->DeleteLocalRef(env, jimage);
			(*env)->DeleteLocalRef(env, jtrm);
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_beginTextBlock, jbbox);
			if ((*env)->ExceptionCheck(env)) return;

			(*env)->DeleteLocalRef(env, jbbox);

			for (line = block->u.t.first_line; line; line = line->next)
			{
				jbbox = to_Rect_safe(ctx, env, line->bbox);
				if (!jbbox) return;

				(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_beginLine, jbbox, line->wmode);
				if ((*env)->ExceptionCheck(env)) return;

				(*env)->DeleteLocalRef(env, jbbox);

				for (ch = line->first_char; ch; ch = ch->next)
				{
					jorigin = to_Point_safe(ctx, env, ch->origin);
					if (!jorigin) return;

					jfont = to_Font_safe(ctx, env, ch->font);
					if (!jfont) return;

					jquad = to_Quad_safe(ctx, env, ch->quad);
					if (!jquad) return;

					(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_onChar,
						ch->c, jorigin, jfont, ch->size, jquad);
					if ((*env)->ExceptionCheck(env)) return;

					(*env)->DeleteLocalRef(env, jquad);
					(*env)->DeleteLocalRef(env, jfont);
					(*env)->DeleteLocalRef(env, jorigin);
				}

				(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_endLine);
				if ((*env)->ExceptionCheck(env)) return;
			}

			(*env)->CallVoidMethod(env, walker, mid_StructuredTextWalker_endTextBlock);
			if ((*env)->ExceptionCheck(env)) return;
		}
	}
}

/* PDFDocument interface */

JNIEXPORT jlong JNICALL
FUN(PDFDocument_newNative)(JNIEnv *env, jclass cls)
{
	fz_context *ctx = get_context(env);
	pdf_document *doc = NULL;

	if (!ctx) return 0;

	fz_try(ctx)
		doc = pdf_create_document(ctx);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return jlong_cast(doc);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	void *data = NULL;

	if (!ctx || !pdf) return;

	data = pdf_get_doc_event_callback_data(ctx, pdf);
	if (data)
		(*env)->DeleteGlobalRef(env, data);

	fz_drop_document(ctx, &pdf->super);
}

JNIEXPORT jint JNICALL
FUN(PDFDocument_countObjects)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int count = 0;

	if (!ctx || !pdf) return 0;

	fz_try(ctx)
		count = pdf_xref_len(ctx, pdf);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return count;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newNull)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	obj = PDF_NULL;

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newBoolean)(JNIEnv *env, jobject self, jboolean b)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	obj = b ? PDF_TRUE : PDF_FALSE;

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newInteger)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_int(ctx, i);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newReal)(JNIEnv *env, jobject self, jfloat f)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_real(ctx, f);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newString)(JNIEnv *env, jobject self, jstring jstring)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	const char *s = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;
	if (!jstring) { jni_throw_arg(env, "string must not be null"); return NULL; }

	s = (*env)->GetStringUTFChars(env, jstring, NULL);
	if (!s) return NULL;

	fz_try(ctx)
		obj = pdf_new_text_string(ctx, s);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jstring, s);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newByteString)(JNIEnv *env, jobject self, jobject jbs)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jbyte *bs;
	size_t bslen;
	jobject jobj;

	if (!ctx || !pdf) return NULL;
	if (!jbs) { jni_throw_arg(env, "bs must not be null"); return NULL; }

	bslen = (*env)->GetArrayLength(env, jbs);

	fz_try(ctx)
		bs = Memento_label(fz_malloc(ctx, bslen), "PDFDocument_newByteString");
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	(*env)->GetByteArrayRegion(env, jbs, 0, bslen, bs);
	if ((*env)->ExceptionCheck(env)) {
		fz_free(ctx, bs);
		return NULL;
	}

	fz_try(ctx)
		obj = pdf_new_string(ctx, (char *) bs, bslen);
	fz_always(ctx)
		fz_free(ctx, bs);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newName)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	const char *name = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;
	if (!jname) { jni_throw_arg(env, "name must not be null"); return NULL; }

	name = (*env)->GetStringUTFChars(env, jname, NULL);
	if (!name) return NULL;

	fz_try(ctx)
		obj = pdf_new_name(ctx, name);
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newIndirect)(JNIEnv *env, jobject self, jint num, jint gen)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_indirect(ctx, pdf, num, gen);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newArray)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_array(ctx, pdf, 0);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newDictionary)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;
	jobject jobj;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_new_dict(ctx, pdf, 0);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, obj);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_findPage)(JNIEnv *env, jobject self, jint jat)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;

	if (!ctx || !pdf) return NULL;
	if (jat < 0 || jat >= pdf_count_pages(ctx, pdf)) { jni_throw_oob(env, "at is not a valid page"); return NULL; }

	fz_try(ctx)
		obj = pdf_lookup_page_obj(ctx, pdf, jat);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe(ctx, env, self, obj);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_getTrailer)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = NULL;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		obj = pdf_trailer(ctx, pdf);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe(ctx, env, self, obj);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);

	if (!ctx || !pdf) return NULL;
	if (!jobj) { jni_throw_arg(env, "object must not be null"); return NULL; }

	fz_try(ctx)
		obj = pdf_add_object_drop(ctx, pdf, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_createObject)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		ind = pdf_new_indirect(ctx, pdf, pdf_create_object(ctx, pdf), 0);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_deleteObject)(JNIEnv *env, jobject self, jint num)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);

	if (!ctx || !pdf) return;

	fz_try(ctx)
		pdf_delete_object(ctx, pdf, num);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_newPDFGraftMap)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_graft_map *map = NULL;

	if (!ctx || !pdf) return NULL;

	fz_try(ctx)
		map = pdf_new_graft_map(ctx, pdf);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFGraftMap_safe_own(ctx, env, self, map);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_graftObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, jobj);
	pdf_document *dst = from_PDFDocument(env, self);

	if (!ctx || !dst) return NULL;
	if (!dst) { jni_throw_arg(env, "dst must not be null"); return NULL; }

	fz_try(ctx)
		obj = pdf_graft_object(ctx, dst, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, obj);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addStreamBuffer)(JNIEnv *env, jobject self, jobject jbuf, jobject jobj, jboolean compressed)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);
	fz_buffer *buf = from_Buffer(env, jbuf);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!jbuf) { jni_throw_arg(env, "buffer must not be null"); return NULL; }

	fz_try(ctx)
		ind = pdf_add_stream(ctx, pdf, buf, obj, compressed);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addStreamString)(JNIEnv *env, jobject self, jstring jbuf, jobject jobj, jboolean compressed)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);
	fz_buffer *buf = NULL;
	const char *sbuf = NULL;
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!jbuf) { jni_throw_arg(env, "buffer must not be null"); return NULL; }

	sbuf = (*env)->GetStringUTFChars(env, jbuf, NULL);
	if (!sbuf) return NULL;

	fz_var(buf);

	fz_try(ctx)
	{
		buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)sbuf, strlen(sbuf));
		ind = pdf_add_stream(ctx, pdf, buf, obj, compressed);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		(*env)->ReleaseStringUTFChars(env, jbuf, sbuf);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addPageBuffer)(JNIEnv *env, jobject self, jobject jmediabox, jint rotate, jobject jresources, jobject jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_rect mediabox = from_Rect(env, jmediabox);
	pdf_obj *resources = from_PDFObject(env, jresources);
	fz_buffer *contents = from_Buffer(env, jcontents);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!resources) { jni_throw_arg(env, "resources must not be null"); return NULL; }
	if (!contents) { jni_throw_arg(env, "contents must not be null"); return NULL; }

	fz_try(ctx)
		ind = pdf_add_page(ctx, pdf, mediabox, rotate, resources, contents);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addPageString)(JNIEnv *env, jobject self, jobject jmediabox, jint rotate, jobject jresources, jstring jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_rect mediabox = from_Rect(env, jmediabox);
	pdf_obj *resources = from_PDFObject(env, jresources);
	const char *scontents = NULL;
	fz_buffer *contents = NULL;
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!resources) { jni_throw_arg(env, "resources must not be null"); return NULL; }
	if (!contents) { jni_throw_arg(env, "contents must not be null"); return NULL; }

	scontents = (*env)->GetStringUTFChars(env, jcontents, NULL);
	if (!scontents) return NULL;

	fz_var(contents);

	fz_try(ctx)
	{
		contents = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)scontents, strlen(scontents));
		ind = pdf_add_page(ctx, pdf, mediabox, rotate, resources, contents);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, contents);
		(*env)->ReleaseStringUTFChars(env, jcontents, scontents);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_insertPage)(JNIEnv *env, jobject self, jint jat, jobject jpage)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int at = jat;
	pdf_obj *page = from_PDFObject(env, jpage);

	if (!ctx || !pdf) return;
	if (jat < 0 || jat >= pdf_count_pages(ctx, pdf)) { jni_throw_oob(env, "at is not a valid page"); return; }
	if (!page) { jni_throw_arg(env, "page must not be null"); return; }

	fz_try(ctx)
		pdf_insert_page(ctx, pdf, at, page);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_deletePage)(JNIEnv *env, jobject self, jint jat)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	int at = jat;

	if (!ctx || !pdf) return;
	if (jat < 0 || jat >= pdf_count_pages(ctx, pdf)) { jni_throw_oob(env, "at is not a valid page"); return; }

	fz_try(ctx)
		pdf_delete_page(ctx, pdf, at);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addImage)(JNIEnv *env, jobject self, jobject jimage)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_image *image = from_Image(env, jimage);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!image) { jni_throw_arg(env, "image must not be null"); return NULL; }

	fz_try(ctx)
		ind = pdf_add_image(ctx, pdf, image);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addFont)(JNIEnv *env, jobject self, jobject jfont)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_font *font = from_Font(env, jfont);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!font) { jni_throw_arg(env, "font must not be null"); return NULL; }

	fz_try(ctx)
		ind = pdf_add_cid_font(ctx, pdf, font);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addCJKFont)(JNIEnv *env, jobject self, jobject jfont, jint ordering, jint wmode, jboolean serif)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_font *font = from_Font(env, jfont);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!font) { jni_throw_arg(env, "font must not be null"); return NULL; }

	fz_try(ctx)
		ind = pdf_add_cjk_font(ctx, pdf, font, ordering, wmode, serif);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT jobject JNICALL
FUN(PDFDocument_addSimpleFont)(JNIEnv *env, jobject self, jobject jfont, jint encoding)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	fz_font *font = from_Font(env, jfont);
	pdf_obj *ind = NULL;

	if (!ctx || !pdf) return NULL;
	if (!font) { jni_throw_arg(env, "font must not be null"); return NULL; }

	fz_try(ctx)
		ind = pdf_add_simple_font(ctx, pdf, font, encoding);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, ind);
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_hasUnsavedChanges)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	if (!ctx || !pdf) return JNI_FALSE;
	return pdf_has_unsaved_changes(ctx, pdf) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_canBeSavedIncrementally)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	if (!ctx || !pdf) return JNI_FALSE;
	return pdf_can_be_saved_incrementally(ctx, pdf) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_nativeSaveWithStream)(JNIEnv *env, jobject self, jobject jstream, jstring joptions)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	SeekableStreamState *state = NULL;
	jobject stream = NULL;
	jbyteArray array = NULL;
	fz_output *out = NULL;
	const char *options = NULL;
	pdf_write_options pwo;

	fz_var(state);
	fz_var(out);
	fz_var(stream);
	fz_var(array);

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options)
			return;
	}

	stream = (*env)->NewGlobalRef(env, jstream);
	if (!stream)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
		return;
	}

	array = (*env)->NewByteArray(env, sizeof state->buffer);
	if (array)
		array = (*env)->NewGlobalRef(env, array);
	if (!array)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
		(*env)->DeleteGlobalRef(env, stream);
		return;
	}

	fz_try(ctx)
	{
		if (jstream)
		{
			/* No exceptions can occur from here to stream owning state, so we must not free state. */
			state = Memento_label(fz_malloc(ctx, sizeof(SeekableStreamState)), "SeekableStreramState_state");
			state->stream = stream;
			state->array = array;

			/* Ownership transferred to state. */
			stream = NULL;
			array = NULL;

			/* Stream takes ownership of state. */
			out = fz_new_output(ctx, sizeof state->buffer, state, SeekableOutputStream_write, NULL, SeekableOutputStream_drop);
			out->seek = SeekableOutputStream_seek;
			out->tell = SeekableOutputStream_tell;

			/* these are now owned by 'out' */
			state = NULL;
		}

		pdf_parse_write_options(ctx, &pwo, options);
		pdf_write_document(ctx, pdf, out, &pwo);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
	}
	fz_catch(ctx)
	{
		(*env)->DeleteGlobalRef(env, array);
		(*env)->DeleteGlobalRef(env, stream);
		jni_rethrow(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFDocument_save)(JNIEnv *env, jobject self, jstring jfilename, jstring joptions)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument(env, self);
	const char *filename = NULL;
	const char *options = NULL;
	pdf_write_options pwo;

	if (!ctx || !pdf) return;
	if (!jfilename) { jni_throw_arg(env, "filename must not be null"); return; }

	filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	if (!filename) return;

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options)
		{
			(*env)->ReleaseStringUTFChars(env, jfilename, filename);
			return;
		}
	}

	fz_try(ctx)
	{
		pdf_parse_write_options(ctx, &pwo, options);
		pdf_save_document(ctx, pdf, filename, &pwo);
	}
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
		(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

/* PDFObject interface */

JNIEXPORT void JNICALL
FUN(PDFObject_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject_safe(env, self);

	if (!ctx || !obj) return;

	pdf_drop_obj(ctx, obj);
}

JNIEXPORT jint JNICALL
FUN(PDFObject_toIndirect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int num = 0;

	if (!ctx || !obj) return 0;

	fz_try(ctx)
		num = pdf_to_num(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return num;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isIndirect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_indirect(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isNull)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_null(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isBoolean)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_bool(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isInteger)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_int(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isReal)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_real(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isNumber)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_number(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isString)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_string(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isName)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_name(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isArray)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_array(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isDictionary)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_dict(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_isStream)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_is_stream(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jbyteArray JNICALL
FUN(PDFObject_readStream)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	fz_buffer *buf = NULL;
	jbyteArray arr = NULL;

	if (!ctx || !obj) return NULL;

	fz_var(buf);

	fz_try(ctx)
	{
		size_t len;
		unsigned char *data;

		buf = pdf_load_stream(ctx, obj);
		len = fz_buffer_storage(ctx, buf, &data);
		arr = (*env)->NewByteArray(env, (jsize)len);
		if (arr)
		{
			(*env)->SetByteArrayRegion(env, arr, 0, (jsize)len, (signed char *) &data[0]);
			if ((*env)->ExceptionCheck(env))
				arr = NULL;
		}
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return arr;
}

JNIEXPORT jbyteArray JNICALL
FUN(PDFObject_readRawStream)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	fz_buffer *buf = NULL;
	jbyteArray arr = NULL;

	if (!ctx || !obj) return NULL;

	fz_var(buf);

	fz_try(ctx)
	{
		unsigned char *data;
		size_t len;

		buf = pdf_load_raw_stream(ctx, obj);
		len = fz_buffer_storage(ctx, buf, &data);
		arr = (*env)->NewByteArray(env, (jsize)len);
		if (arr)
		{
			(*env)->SetByteArrayRegion(env, arr, 0, (jsize)len, (signed char *) &data[0]);
			if ((*env)->ExceptionCheck(env))
				arr = NULL;
		}
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return arr;
}

JNIEXPORT void JNICALL
FUN(PDFObject_writeObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *ref = from_PDFObject(env, self);
	pdf_document *pdf = pdf_get_bound_document(ctx, ref);
	pdf_obj *obj = from_PDFObject(env, jobj);

	if (!ctx || !obj) return;
	if (!pdf) { jni_throw_arg(env, "object not bound to document"); return; }
	if (!obj) { jni_throw_arg(env, "object must not be null"); return; }

	fz_try(ctx)
		pdf_update_object(ctx, pdf, pdf_to_num(ctx, ref), obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_writeStreamBuffer)(JNIEnv *env, jobject self, jobject jbuf)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	pdf_document *pdf = pdf_get_bound_document(ctx, obj);
	fz_buffer *buf = from_Buffer(env, jbuf);

	if (!ctx || !obj) return;
	if (!pdf) { jni_throw_arg(env, "object not bound to document"); return; }
	if (!buf) { jni_throw_arg(env, "buffer must not be null"); return; }

	fz_try(ctx)
		pdf_update_stream(ctx, pdf, obj, buf, 0);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_writeStreamString)(JNIEnv *env, jobject self, jstring jstr)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	pdf_document *pdf = pdf_get_bound_document(ctx, obj);
	const char *str = NULL;
	fz_buffer *buf = NULL;

	if (!ctx || !obj) return;
	if (!pdf) { jni_throw_arg(env, "object not bound to document"); return; }
	if (!jstr) { jni_throw_arg(env, "string must not be null"); return; }

	str = (*env)->GetStringUTFChars(env, jstr, NULL);
	if (!str) return;

	fz_var(buf);

	fz_try(ctx)
	{
		buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)str, strlen(str));
		pdf_update_stream(ctx, pdf, obj, buf, 0);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		(*env)->ReleaseStringUTFChars(env, jstr, str);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_writeRawStreamBuffer)(JNIEnv *env, jobject self, jobject jbuf)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	pdf_document *pdf = pdf_get_bound_document(ctx, obj);
	fz_buffer *buf = from_Buffer(env, jbuf);

	if (!ctx || !obj) return;
	if (!pdf) { jni_throw_arg(env, "object not bound to document"); return; }
	if (!buf) { jni_throw_arg(env, "buffer must not be null"); return; }

	fz_try(ctx)
		pdf_update_stream(ctx, pdf, obj, buf, 1);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_writeRawStreamString)(JNIEnv *env, jobject self, jstring jstr)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	pdf_document *pdf = pdf_get_bound_document(ctx, obj);
	const char *str = NULL;
	fz_buffer *buf = NULL;

	if (!ctx || !obj) return;
	if (!pdf) { jni_throw_arg(env, "object not bound to document"); return; }
	if (!jstr) { jni_throw_arg(env, "string must not be null"); return; }

	str = (*env)->GetStringUTFChars(env, jstr, NULL);
	if (!str) return;

	fz_var(buf);

	fz_try(ctx)
	{
		buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)str, strlen(str));
		pdf_update_stream(ctx, pdf, obj, buf, 1);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		(*env)->ReleaseStringUTFChars(env, jstr, str);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFObject_resolve)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	pdf_obj *ind = NULL;
	jobject jobj;

	if (!ctx || !obj) return NULL;

	fz_try(ctx)
		ind = pdf_resolve_indirect(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	pdf_keep_obj(ctx, ind);
	jobj = (*env)->NewObject(env, cls_PDFObject, mid_PDFObject_init, jlong_cast(obj), self);
	if (!jobj)
		pdf_drop_obj(ctx, ind);
	return jobj;
}

JNIEXPORT jobject JNICALL
FUN(PDFObject_getArray)(JNIEnv *env, jobject self, jint index)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);
	pdf_obj *val = NULL;

	if (!ctx || !arr) return NULL;

	fz_try(ctx)
		val = pdf_array_get(ctx, arr, index);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe(ctx, env, self, val);
}

JNIEXPORT jobject JNICALL
FUN(PDFObject_getDictionary)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	const char *name = NULL;
	pdf_obj *val = NULL;

	if (!ctx || !dict) return NULL;

	if (jname)
		name = (*env)->GetStringUTFChars(env, jname, NULL);

	if (name)
	{
		fz_try(ctx)
			val = pdf_dict_gets(ctx, dict, name);
		fz_always(ctx)
			(*env)->ReleaseStringUTFChars(env, jname, name);
		fz_catch(ctx)
		{
			jni_rethrow(env, ctx);
			return NULL;
		}
	}

	return to_PDFObject_safe(ctx, env, self, val);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putArrayBoolean)(JNIEnv *env, jobject self, jint index, jboolean b)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_put(ctx, arr, index, b ? PDF_TRUE : PDF_FALSE);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putArrayInteger)(JNIEnv *env, jobject self, jint index, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_put_drop(ctx, arr, index, pdf_new_int(ctx, i));
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putArrayFloat)(JNIEnv *env, jobject self, jint index, jfloat f)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_put_drop(ctx, arr, index, pdf_new_real(ctx, f));
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putArrayString)(JNIEnv *env, jobject self, jint index, jstring jstr)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);
	const char *str = NULL;

	if (!ctx || !arr) return;
	if (jstr)
	{
		str = (*env)->GetStringUTFChars(env, jstr, NULL);
		if (!str) return;
	}

	fz_try(ctx)
	{
		if (str)
			pdf_array_put_drop(ctx, arr, index, pdf_new_string(ctx, str, strlen(str)));
		else
			pdf_array_put(ctx, arr, index, PDF_NULL);
	}
	fz_always(ctx)
	{
		if (str)
			(*env)->ReleaseStringUTFChars(env, jstr, str);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putArrayPDFObject)(JNIEnv *env, jobject self, jint index, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);
	pdf_obj *obj = from_PDFObject(env, jobj);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_put(ctx, arr, index, obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryStringBoolean)(JNIEnv *env, jobject self, jstring jname, jboolean b)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	const char *name = NULL;
	pdf_obj *key = NULL;

	if (!ctx || !dict) return;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}

	fz_var(key);

	fz_try(ctx)
	{
		key = name ? pdf_new_name(ctx, name) : NULL;
		pdf_dict_put(ctx, dict, key, b ? PDF_TRUE : PDF_FALSE);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, key);
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryStringInteger)(JNIEnv *env, jobject self, jstring jname, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	const char *name = NULL;
	pdf_obj *key = NULL;

	if (!ctx || !dict) return;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}

	fz_var(key);

	fz_try(ctx)
	{
		key = name ? pdf_new_name(ctx, name) : NULL;
		pdf_dict_put_int(ctx, dict, key, i);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, key);
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryStringFloat)(JNIEnv *env, jobject self, jstring jname, jfloat f)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	const char *name = NULL;
	pdf_obj *key = NULL;

	if (!ctx || !dict) return;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}

	fz_var(key);

	fz_try(ctx)
	{
		key = name ? pdf_new_name(ctx, name) : NULL;
		pdf_dict_put_real(ctx, dict, key, f);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, key);
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryStringString)(JNIEnv *env, jobject self, jstring jname, jstring jstr)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	const char *name = NULL;
	const char *str = NULL;
	pdf_obj *key = NULL;

	if (!ctx || !dict) return;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}
	if (jstr)
	{
		str = (*env)->GetStringUTFChars(env, jstr, NULL);
		if (!str)
		{
			(*env)->ReleaseStringUTFChars(env, jname, str);
			return;
		}
	}

	fz_var(key);

	fz_try(ctx)
	{
		key = name ? pdf_new_name(ctx, name) : NULL;
		if (str)
			pdf_dict_put_string(ctx, dict, key, str, strlen(str));
		else
			pdf_dict_put(ctx, dict, key, PDF_NULL);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, key);
		if (str)
			(*env)->ReleaseStringUTFChars(env, jstr, str);
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryStringPDFObject)(JNIEnv *env, jobject self, jstring jname, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	pdf_obj *val = from_PDFObject(env, jobj);
	const char *name = NULL;
	pdf_obj *key = NULL;

	if (!ctx || !dict) return;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}

	fz_var(key);

	fz_try(ctx)
	{
		key = name ? pdf_new_name(ctx, name) : NULL;
		pdf_dict_put(ctx, dict, key, val);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, key);
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryPDFObjectBoolean)(JNIEnv *env, jobject self, jobject jname, jboolean b)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	pdf_obj *name = from_PDFObject(env, jname);

	if (!ctx || !dict) return;

	fz_try(ctx)
		pdf_dict_put(ctx, dict, name, b ? PDF_TRUE : PDF_FALSE);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryPDFObjectInteger)(JNIEnv *env, jobject self, jobject jname, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	pdf_obj *name = from_PDFObject(env, jname);

	if (!ctx || !dict) return;

	fz_try(ctx)
		pdf_dict_put_int(ctx, dict, name, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryPDFObjectFloat)(JNIEnv *env, jobject self, jobject jname, jfloat f)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	pdf_obj *name = from_PDFObject(env, jname);

	if (!ctx || !dict) return;

	fz_try(ctx)
		pdf_dict_put_real(ctx, dict, name, f);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryPDFObjectString)(JNIEnv *env, jobject self, jobject jname, jstring jstr)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	pdf_obj *name = from_PDFObject(env, jname);
	const char *str = NULL;

	if (!ctx || !dict) return;
	if (jstr)
	{
		str = (*env)->GetStringUTFChars(env, jstr, NULL);
		if (!str) return;
	}

	fz_try(ctx)
	{
		if (str)
			pdf_dict_put_string(ctx, dict, name, str, strlen(str));
		else
			pdf_dict_put(ctx, dict, name, PDF_NULL);
	}
	fz_always(ctx)
	{
		if (str)
			(*env)->ReleaseStringUTFChars(env, jstr, str);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_putDictionaryPDFObjectPDFObject)(JNIEnv *env, jobject self, jobject jname, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	pdf_obj *name = from_PDFObject(env, jname);
	pdf_obj *obj = from_PDFObject(env, jobj);

	if (!ctx || !dict) return;

	fz_try(ctx)
		pdf_dict_put(ctx, dict, name, obj);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_deleteArray)(JNIEnv *env, jobject self, jint index)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_delete(ctx, arr, index);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_deleteDictionaryString)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	const char *name = NULL;

	if (!ctx || !dict) return;
	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}

	fz_try(ctx)
	{
		pdf_dict_dels(ctx, dict, name);
	}
	fz_always(ctx)
	{
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_deleteDictionaryPDFObject)(JNIEnv *env, jobject self, jobject jname)
{
	fz_context *ctx = get_context(env);
	pdf_obj *dict = from_PDFObject(env, self);
	pdf_obj *name = from_PDFObject(env, jname);

	if (!ctx || !dict) return;

	fz_try(ctx)
		pdf_dict_del(ctx, dict, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFObject_asBoolean)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int b = 0;

	if (!ctx || !obj) return JNI_FALSE;

	fz_try(ctx)
		b = pdf_to_bool(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return JNI_FALSE;
	}

	return b ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
FUN(PDFObject_asInteger)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int i = 0;

	if (!ctx || !obj) return 0;

	fz_try(ctx)
		i = pdf_to_int(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return i;
}

JNIEXPORT jfloat JNICALL
FUN(PDFObject_asFloat)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	float f = 0;

	if (!ctx || !obj) return 0;

	fz_try(ctx)
		f = pdf_to_real(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return f;
}

JNIEXPORT jint JNICALL
FUN(PDFObject_asIndirect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	int ind = 0;

	if (!ctx || !obj) return 0;

	fz_try(ctx)
		ind = pdf_to_num(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return ind;
}

JNIEXPORT jstring JNICALL
FUN(PDFObject_asString)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	const char *str = NULL;

	if (!ctx || !obj) return NULL;

	fz_try(ctx)
		str = pdf_to_text_string(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (*env)->NewStringUTF(env, str);
}

JNIEXPORT jobject JNICALL
FUN(PDFObject_asByteString)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	const char *str = NULL;
	jobject jbs = NULL;
	jbyte *bs = NULL;
	size_t len;

	if (!ctx || !obj) return NULL;

	fz_try(ctx)
	{
		str = pdf_to_str_buf(ctx, obj);
		len = pdf_to_str_len(ctx, obj);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	jbs = (*env)->NewByteArray(env, len);
	if (!jbs) return NULL;
	bs = (*env)->GetByteArrayElements(env, jbs, NULL);
	if (!bs) return NULL;

	memcpy(bs, str, len);

	(*env)->ReleaseByteArrayElements(env, jbs, bs, 0);

	return jbs;
}

JNIEXPORT jstring JNICALL
FUN(PDFObject_asName)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, self);
	const char *str = NULL;

	if (!ctx || !obj) return NULL;

	fz_try(ctx)
		str = pdf_to_name(ctx, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (*env)->NewStringUTF(env, str);
}

JNIEXPORT jint JNICALL
FUN(PDFObject_size)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);
	int len;

	if (!ctx || !arr) return 0;

	fz_try(ctx)
		len = pdf_array_len(ctx, arr);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return len;
}

JNIEXPORT void JNICALL
FUN(PDFObject_pushBoolean)(JNIEnv *env, jobject self, jboolean b)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_push_bool(ctx, arr, b);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_pushInteger)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_push_int(ctx, arr, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_pushFloat)(JNIEnv *env, jobject self, jfloat f)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_push_real(ctx, arr, f);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_pushString)(JNIEnv *env, jobject self, jstring jstr)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);
	const char *str = NULL;

	if (!ctx || !arr) return;
	if (jstr)
	{
		str = (*env)->GetStringUTFChars(env, jstr, NULL);
		if (!str) return;
	}

	fz_try(ctx)
	{
		if (str)
			pdf_array_push_string(ctx, arr, str, strlen(str));
		else
			pdf_array_push(ctx, arr, PDF_NULL);
	}
	fz_always(ctx)
	{
		if (str)
			(*env)->ReleaseStringUTFChars(env, jstr, str);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFObject_pushPDFObject)(JNIEnv *env, jobject self, jobject jitem)
{
	fz_context *ctx = get_context(env);
	pdf_obj *arr = from_PDFObject(env, self);
	pdf_obj *item = from_PDFObject(env, jitem);

	if (!ctx || !arr) return;

	fz_try(ctx)
		pdf_array_push(ctx, arr, item);
	fz_always(ctx)
		pdf_drop_obj(ctx, item);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFObject_toString)(JNIEnv *env, jobject self, jboolean tight, jboolean ascii)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject_safe(env, self);
	jstring string = NULL;
	char *s = NULL;
	size_t n = 0;

	if (!ctx || !obj) return NULL;

	fz_var(s);

	fz_try(ctx)
	{
		s = pdf_sprint_obj(ctx, NULL, 0, &n, obj, tight, ascii);
		string = (*env)->NewStringUTF(env, s);
	}
	fz_always(ctx)
		fz_free(ctx, s);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return string;
}

/* Shade interface */

JNIEXPORT void JNICALL
FUN(Shade_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_shade *shd = from_Shade_safe(env, self);

	if (!ctx || !shd) return;

	fz_drop_shade(ctx, shd);
}

/* PDFGraftMap interface */

JNIEXPORT void JNICALL
FUN(PDFGraftMap_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_graft_map *map = from_PDFGraftMap_safe(env, self);

	if (!ctx || !map) return;

	pdf_drop_graft_map(ctx, map);
}

JNIEXPORT jobject JNICALL
FUN(PDFGraftMap_graftObject)(JNIEnv *env, jobject self, jobject jobj)
{
	fz_context *ctx = get_context(env);
	pdf_obj *obj = from_PDFObject(env, jobj);
	pdf_graft_map *map = from_PDFGraftMap(env, self);

	if (!ctx) return NULL;
	if (!map) { jni_throw_arg(env, "map must not be null"); return NULL; }

	fz_try(ctx)
		obj = pdf_graft_mapped_object(ctx, map, obj);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFObject_safe_own(ctx, env, self, obj);
}

/* PDFPage interface */

JNIEXPORT jobject JNICALL
FUN(PDFPage_createAnnotation)(JNIEnv *env, jobject self, jint subtype)
{
	fz_context *ctx = get_context(env);
	pdf_page *page = from_PDFPage(env, self);
	pdf_annot *annot = NULL;

	if (!ctx || !page) return NULL;

	fz_try(ctx)
		annot = pdf_create_annot(ctx, page, subtype);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_PDFAnnotation_safe_own(ctx, env, annot);
}

JNIEXPORT void JNICALL
FUN(PDFPage_deleteAnnotation)(JNIEnv *env, jobject self, jobject jannot)
{
	fz_context *ctx = get_context(env);
	pdf_page *page = from_PDFPage(env, self);
	pdf_annot *annot = from_PDFAnnotation(env, jannot);

	if (!ctx || !page) return;

	fz_try(ctx)
		pdf_delete_annot(ctx, page, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFPage_update)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_page *page = from_PDFPage(env, self);
	jboolean changed = JNI_FALSE;

	if (!ctx || !page) return JNI_FALSE;

	fz_try(ctx)
		changed = pdf_update_page(ctx, page);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return changed;
}

JNIEXPORT jboolean JNICALL
FUN(PDFPage_applyRedactions)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_page *page = from_PDFPage(env, self);
	jboolean redacted = JNI_FALSE;

	if (!ctx || !page) return JNI_FALSE;

	fz_try(ctx)
		redacted = pdf_redact_page(ctx, page->doc, page, NULL);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return redacted;
}
/* PDFAnnotation interface */

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getType)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jint subtype = 0;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		subtype = pdf_annot_type(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return subtype;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getFlags)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jint flags = 0;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		flags = pdf_annot_flags(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return flags;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setFlags)(JNIEnv *env, jobject self, jint flags)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_flags(ctx, annot, flags);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getContents)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *contents = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		contents = pdf_annot_contents(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (*env)->NewStringUTF(env, contents);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setContents)(JNIEnv *env, jobject self, jstring jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *contents = NULL;

	if (!ctx || !annot) return;
	if (jcontents)
	{
		contents = (*env)->GetStringUTFChars(env, jcontents, NULL);
		if (!contents) return;
	}

	fz_try(ctx)
		pdf_set_annot_contents(ctx, annot, contents);
	fz_always(ctx)
		if (contents)
			(*env)->ReleaseStringUTFChars(env, jcontents, contents);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getAuthor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *author = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		author = pdf_annot_author(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return (*env)->NewStringUTF(env, author);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setAuthor)(JNIEnv *env, jobject self, jstring jauthor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *author = NULL;

	if (!ctx || !annot) return;
	if (jauthor)
	{
		author = (*env)->GetStringUTFChars(env, jauthor, NULL);
		if (!author) return;
	}

	fz_try(ctx)
		pdf_set_annot_author(ctx, annot, author);
	fz_always(ctx)
		if (author)
			(*env)->ReleaseStringUTFChars(env, jauthor, author);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(PDFAnnotation_getModificationDateNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jlong t;

	if (!ctx || !annot) return -1;

	fz_try(ctx)
		t = pdf_annot_modification_date(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return -1;
	}

	return t * 1000;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setModificationDate)(JNIEnv *env, jobject self, jlong time)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_modification_date(ctx, annot, time / 1000);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return;
	}
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getRect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		rect = pdf_annot_rect(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setRect)(JNIEnv *env, jobject self, jobject jrect)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect = from_Rect(env, jrect);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_rect(ctx, annot, rect);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getBorder)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jfloat border;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		border = pdf_annot_border(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return border;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setBorder)(JNIEnv *env, jobject self, jfloat border)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_border(ctx, annot, border);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getColor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n;
	float color[4];
	jfloatArray arr;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_color(ctx, annot, &n, color);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	arr = (*env)->NewFloatArray(env, n);
	if (!arr) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, n, &color[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setColor)(JNIEnv *env, jobject self, jobject jcolor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	float color[4];
	int n;

	if (!ctx || !annot) return;
	if (!from_jfloatArray(env, color, nelem(color), jcolor)) return;
	n = (*env)->GetArrayLength(env, jcolor);

	fz_try(ctx)
		pdf_set_annot_color(ctx, annot, n, color);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getInteriorColor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n;
	float color[4];
	jfloatArray arr;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_interior_color(ctx, annot, &n, color);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	arr = (*env)->NewFloatArray(env, n);
	if (!arr) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, n, &color[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setInteriorColor)(JNIEnv *env, jobject self, jobject jcolor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	float color[4];
	int n;

	if (!ctx || !annot) return;
	if (!from_jfloatArray(env, color, nelem(color), jcolor)) return;
	n = (*env)->GetArrayLength(env, jcolor);

	fz_try(ctx)
		pdf_set_annot_interior_color(ctx, annot, n, color);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getQuadPointCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_quad_point_count(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getQuadPoint)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_quad q;

	fz_try(ctx)
		q = pdf_annot_quad_point(ctx, annot, i);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Quad_safe(ctx, env, q);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearQuadPoints)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_quad_points(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addQuadPoint)(JNIEnv *env, jobject self, jobject qobj)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_quad q = from_Quad(env, qobj);

	fz_try(ctx)
		pdf_add_annot_quad_point(ctx, annot, q);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getVertexCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_vertex_count(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getVertex)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point v;

	fz_try(ctx)
		v = pdf_annot_vertex(ctx, annot, i);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Point_safe(ctx, env, v);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearVertices)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_vertices(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addVertex)(JNIEnv *env, jobject self, float x, float y)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_vertex(ctx, annot, fz_make_point(x, y));
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getInkListCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_ink_list_count(ctx, annot);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return n;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getInkListStrokeCount)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_ink_list_stroke_count(ctx, annot, i);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return 0;
	}

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getInkListStrokeVertex)(JNIEnv *env, jobject self, jint i, jint k)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point v;

	fz_try(ctx)
		v = pdf_annot_ink_list_stroke_vertex(ctx, annot, i, k);
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	return to_Point_safe(ctx, env, v);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearInkList)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_ink_list(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addInkListStroke)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addInkListStrokeVertex)(JNIEnv *env, jobject self, float x, float y)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke_vertex(ctx, annot, fz_make_point(x, y));
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventEnter)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_enter(ctx, annot);
		annot->is_hot = 1;
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventExit)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_exit(ctx, annot);
		annot->is_hot = 0;
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventDown)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_down(ctx, annot);
		annot->is_active = 1;
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventUp)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		if (annot->is_hot && annot->is_active)
			pdf_annot_event_up(ctx, annot);
		annot->is_active = 0;
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventFocus)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_event_focus(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventBlur)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_event_blur(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_update)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean changed = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		changed = pdf_update_annot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return changed;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_isOpen)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean open = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		open = pdf_annot_is_open(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return open;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setIsOpen)(JNIEnv *env, jobject self, jboolean open)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_is_open(ctx, annot, open);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getIcon)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *name = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		name = pdf_annot_icon_name(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, name);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setIcon)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *name = NULL;

	if (!ctx || !annot) return;
	if (!jname) { jni_throw_arg(env, "icon name must not be null"); return; }

	name = (*env)->GetStringUTFChars(env, jname, NULL);
	if (!name) return;

	fz_try(ctx)
		pdf_set_annot_icon_name(ctx, annot, name);
	fz_always(ctx)
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

JNIEXPORT jintArray JNICALL
FUN(PDFAnnotation_getLineEndingStyles)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	enum pdf_line_ending s = 0, e = 0;
	int line_endings[2];
	jintArray jline_endings = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_line_ending_styles(ctx, annot, &s, &e);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	line_endings[0] = s;
	line_endings[1] = e;
	jline_endings = (*env)->NewIntArray(env, 2);
	(*env)->SetIntArrayRegion(env, jline_endings, 0, 2, &line_endings[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return jline_endings;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLineEndingStyles)(JNIEnv *env, jobject self, jint start_style, jint end_style)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_ending_styles(ctx, annot, start_style, end_style);
	fz_catch(ctx)
		jni_rethrow(env, ctx);
}

static void event_cb(fz_context *ctx, pdf_document *doc, pdf_doc_event *event, void *data)
{
	JNIEnv *env;
	int detach;
	jobject jlistener = (jobject)data;

	env = jni_attach_thread(ctx, &detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in event_cb");

	switch (event->type)
	{
	case PDF_DOCUMENT_EVENT_ALERT:
		{
			pdf_alert_event *alert;
			jstring jstring;

			alert = pdf_access_alert_event(ctx, event);

			jstring = (*env)->NewStringUTF(env, alert->message);
			if (!jstring)
			{
				jni_detach_thread(detach);
				fz_throw_java(ctx, env);
				return;
			}

			(*env)->CallVoidMethod(env, jlistener, mid_PDFDocument_JsEventListener_onAlert, jstring);
			if ((*env)->ExceptionCheck(env))
			{
				jni_detach_thread(detach);
				fz_throw_java(ctx, env);
			}
		}
		break;

	default:
		jni_detach_thread(detach);
		fz_throw(ctx, FZ_ERROR_GENERIC, "event not yet implemented");
		break;
	}

	jni_detach_thread(detach);
}

JNIEXPORT void JNICALL
FUN(PDFDocument_enableJs)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);

	if (!ctx || !pdf)
		return;

	fz_try(ctx)
	{
		pdf_enable_js(ctx, pdf);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFDocument_disableJs)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);

	if (!ctx || !pdf)
		return;

	fz_try(ctx)
	{
		pdf_disable_js(ctx, pdf);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}
}

JNIEXPORT jboolean JNICALL
FUN(PDFDocument_isJsSupported)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	jboolean supported = JNI_FALSE;

	if (!ctx || !pdf)
		return JNI_FALSE;

	fz_try(ctx)
	{
		supported = pdf_js_supported(ctx, pdf);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return supported;
}

JNIEXPORT void JNICALL
FUN(PDFDocument_setJsEventListener)(JNIEnv *env, jobject self, jobject listener)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);
	void *data = NULL;

	if (!ctx || !pdf)
		return;

	fz_try(ctx)
	{
		data = pdf_get_doc_event_callback_data(ctx, pdf);
		if (data)
			(*env)->DeleteGlobalRef(env, data);
		pdf_set_doc_event_callback(ctx, pdf, event_cb, (*env)->NewGlobalRef(env, listener));
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFDocument_calculate)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_document *pdf = from_PDFDocument_safe(env, self);

	if (!ctx || !pdf)
		return;

	fz_try(ctx)
	{
		if (pdf->recalculate)
			pdf_calculate_form(ctx, pdf);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}
}

JNIEXPORT void JNICALL
FUN(PDFWidget_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);

	if (!ctx || !widget)
		return;

	pdf_drop_annot(ctx, widget);
}

JNIEXPORT jstring JNICALL
FUN(PDFWidget_getValue)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	const char *text = NULL;
	jstring jval;

	if (!ctx || !widget)
		return NULL;

	fz_try(ctx)
	{
		text = pdf_field_value(ctx, widget->obj);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	jval = (*env)->NewStringUTF(env, text);
	return jval;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_setTextValue)(JNIEnv *env, jobject self, jstring jval)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	const char *val;
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget)
		return JNI_FALSE;

	val = (*env)->GetStringUTFChars(env, jval, NULL);

	fz_var(accepted);
	fz_try(ctx)
	{
		accepted = pdf_set_text_field_value(ctx, widget, val);
	}
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jval, val);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return accepted;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_setChoiceValue)(JNIEnv *env, jobject self, jstring jval)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	const char *val;
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget)
		return JNI_FALSE;

	val = (*env)->GetStringUTFChars(env, jval, NULL);

	fz_var(accepted);
	fz_try(ctx)
	{
		accepted = pdf_set_choice_field_value(ctx, widget, val);
	}
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jval, val);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return accepted;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_setValue)(JNIEnv *env, jobject self, jstring jval)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	const char *val;
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget)
		return JNI_FALSE;

	val = (*env)->GetStringUTFChars(env, jval, NULL);

	fz_var(accepted);
	fz_try(ctx)
	{
		accepted = pdf_set_field_value(ctx, widget->page->doc, widget->obj, (char *)val, widget->ignore_trigger_events);
	}
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jval, val);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return accepted;
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_toggle)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	jboolean accepted = JNI_FALSE;

	if (!ctx || !widget)
		return JNI_FALSE;

	fz_var(accepted);
	fz_try(ctx)
	{
		accepted = pdf_toggle_widget(ctx, widget);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return accepted;
}

JNIEXPORT void JNICALL
FUN(PDFWidget_setEditing)(JNIEnv *env, jobject self, jboolean val)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);

	if (!ctx || !widget)
		return;

	fz_try(ctx)
	{
		pdf_set_widget_editing_state(ctx, widget, val);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}
}

JNIEXPORT jboolean JNICALL
FUN(PDFWidget_isEditing)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	jboolean state = JNI_FALSE;

	if (!ctx || !widget)
		return JNI_FALSE;

	fz_var(state);
	fz_try(ctx)
	{
		state = pdf_get_widget_editing_state(ctx, widget);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return state;
}

JNIEXPORT jobject JNICALL
FUN(PDFWidget_textQuads)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_widget *widget = from_PDFWidget_safe(env, self);
	jobject jquad;
	jobjectArray array;
	int i, nchars;
	fz_stext_page *stext = NULL;

	if (!ctx || !widget)
		return NULL;

	fz_try(ctx)
	{
		fz_stext_options opts = {0};
		opts.flags = FZ_STEXT_INHIBIT_SPACES;
		stext = pdf_new_stext_page_from_annot(ctx, widget, &opts);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
		return NULL;
	}

	nchars = 0;
	for (fz_stext_block *block = stext->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			for (fz_stext_line *line = block->u.t.first_line; line; line = line->next)
			{
				for (fz_stext_char *ch = line->first_char; ch; ch = ch->next)
				{
					nchars++;
				}
			}
		}
	}

	array = (*env)->NewObjectArray(env, nchars, cls_Quad, NULL);
	if (!array) {
		fz_drop_stext_page(ctx, stext);
		return NULL;
	}

	i = 0;
	for (fz_stext_block *block = stext->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			for (fz_stext_line *line = block->u.t.first_line; line; line = line->next)
			{
				for (fz_stext_char *ch = line->first_char; ch; ch = ch->next)
				{
					jquad = to_Quad_safe(ctx, env, ch->quad);
					if (!jquad) {
						fz_drop_stext_page(ctx, stext);
						return NULL;
					}

					(*env)->SetObjectArrayElement(env, array, i, jquad);
					if ((*env)->ExceptionCheck(env)) {
						fz_drop_stext_page(ctx, stext);
						return NULL;
					}

					(*env)->DeleteLocalRef(env, jquad);
					i++;
				}
			}
		}
	}

	fz_drop_stext_page(ctx, stext);
	return array;
}
