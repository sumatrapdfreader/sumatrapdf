// Copyright (C) 2004-2023 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

/*
NOTE!
	The JNI specification states that New<PrimitiveType>Array() do not
	throw java exceptions, but many JVMs (e.g. Android's) treat them the
	same way as NewObjectArray which may throw e.g. OutOfMemoryError.
	So after calling these functions it is as important to call
	ExceptionCheck() to check for exceptions as for functions that
	are marked as throwing exceptions according to the JNI specification.
*/

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
#include <limits.h>

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

/* Our VM */
static JavaVM *jvm = NULL;

/* All the cached classes/mids/fids we need. */

static jclass cls_AbortException;
static jclass cls_AlertResult;
static jclass cls_Archive;
static jclass cls_ArrayOfQuad;
static jclass cls_Buffer;
static jclass cls_ColorSpace;
static jclass cls_Context;
static jclass cls_Context_Log;
static jclass cls_Context_Version;
static jclass cls_Cookie;
static jclass cls_DefaultAppearance;
static jclass cls_DefaultColorSpaces;
static jclass cls_Device;
static jclass cls_DisplayList;
static jclass cls_Document;
static jclass cls_DocumentWriter;
static jclass cls_DocumentWriter_OCRListener;
static jclass cls_DOM;
static jclass cls_DOMAttribute;
static jclass cls_FitzInputStream;
static jclass cls_FloatArray;
static jclass cls_Font;
static jclass cls_Story;
static jclass cls_IOException;
static jclass cls_IllegalArgumentException;
static jclass cls_Image;
static jclass cls_IndexOutOfBoundsException;
static jclass cls_IntegerArray;
static jclass cls_Link;
static jclass cls_LinkDestination;
static jclass cls_Location;
static jclass cls_Matrix;
static jclass cls_MultiArchive;
static jclass cls_NativeDevice;
static jclass cls_NullPointerException;
static jclass cls_Object;
static jclass cls_OutOfMemoryError;
static jclass cls_Outline;
static jclass cls_OutlineItem;
static jclass cls_OutlineIterator;
static jclass cls_PDFAnnotation;
static jclass cls_PDFDocument;
static jclass cls_PDFDocument_JsEventListener;
static jclass cls_PDFDocument_PDFEmbeddedFileParams;
static jclass cls_PDFGraftMap;
static jclass cls_PDFObject;
static jclass cls_PDFPage;
static jclass cls_PDFWidget;
static jclass cls_PKCS7DistinguishedName;
static jclass cls_PKCS7Signer;
static jclass cls_PKCS7Verifier;
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
static jclass cls_TextWidgetCharLayout;
static jclass cls_TextWidgetLayout;
static jclass cls_TextWidgetLineLayout;
static jclass cls_TreeArchive;
static jclass cls_TryLaterException;
static jclass cls_UnsupportedOperationException;

static jfieldID fid_AlertResult_buttonPressed;
static jfieldID fid_AlertResult_checkboxChecked;
static jfieldID fid_Archive_pointer;
static jfieldID fid_Buffer_pointer;
static jfieldID fid_ColorSpace_pointer;
static jfieldID fid_Context_Version_major;
static jfieldID fid_Context_Version_minor;
static jfieldID fid_Context_Version_patch;
static jfieldID fid_Context_Version_version;
static jfieldID fid_Context_lock;
static jfieldID fid_Context_log;
static jfieldID fid_Cookie_pointer;
static jfieldID fid_DefaultAppearance_color;
static jfieldID fid_DefaultAppearance_font;
static jfieldID fid_DefaultAppearance_size;
static jfieldID fid_DefaultColorSpaces_pointer;
static jfieldID fid_Device_pointer;
static jfieldID fid_DisplayList_pointer;
static jfieldID fid_DocumentWriter_ocrlistener;
static jfieldID fid_DocumentWriter_pointer;
static jfieldID fid_Document_pointer;
static jfieldID fid_DOM_pointer;
static jfieldID fid_DOMAttribute_attribute;
static jfieldID fid_DOMAttribute_value;
static jfieldID fid_FitzInputStream_closed;
static jfieldID fid_FitzInputStream_markpos;
static jfieldID fid_FitzInputStream_pointer;
static jfieldID fid_Font_pointer;
static jfieldID fid_Story_pointer;
static jfieldID fid_Image_pointer;
static jfieldID fid_Link_pointer;
static jfieldID fid_LinkDestination_chapter;
static jfieldID fid_LinkDestination_height;
static jfieldID fid_LinkDestination_page;
static jfieldID fid_LinkDestination_type;
static jfieldID fid_LinkDestination_width;
static jfieldID fid_LinkDestination_x;
static jfieldID fid_LinkDestination_y;
static jfieldID fid_LinkDestination_zoom;
static jfieldID fid_Matrix_a;
static jfieldID fid_Matrix_b;
static jfieldID fid_Matrix_c;
static jfieldID fid_Matrix_d;
static jfieldID fid_Matrix_e;
static jfieldID fid_Matrix_f;
static jfieldID fid_MultiArchive_pointer;
static jfieldID fid_NativeDevice_nativeInfo;
static jfieldID fid_NativeDevice_nativeResource;
static jfieldID fid_OutlineIterator_pointer;
static jfieldID fid_PDFAnnotation_pointer;
static jfieldID fid_PDFDocument_pointer;
static jfieldID fid_PDFGraftMap_pointer;
static jfieldID fid_PDFObject_Null;
static jfieldID fid_PDFObject_pointer;
static jfieldID fid_PDFPage_pointer;
static jfieldID fid_PDFWidget_fieldFlags;
static jfieldID fid_PDFWidget_fieldType;
static jfieldID fid_PDFWidget_maxLen;
static jfieldID fid_PDFWidget_options;
static jfieldID fid_PDFWidget_pointer;
static jfieldID fid_PDFWidget_textFormat;
static jfieldID fid_PKCS7DistinguishedName_c;
static jfieldID fid_PKCS7DistinguishedName_cn;
static jfieldID fid_PKCS7DistinguishedName_email;
static jfieldID fid_PKCS7DistinguishedName_o;
static jfieldID fid_PKCS7DistinguishedName_ou;
static jfieldID fid_PKCS7Signer_pointer;
static jfieldID fid_PKCS7Verifier_pointer;
static jfieldID fid_Page_pointer;
static jfieldID fid_Path_pointer;
static jfieldID fid_Pixmap_pointer;
static jfieldID fid_Point_x;
static jfieldID fid_Point_y;
static jfieldID fid_Quad_ll_x;
static jfieldID fid_Quad_ll_y;
static jfieldID fid_Quad_lr_x;
static jfieldID fid_Quad_lr_y;
static jfieldID fid_Quad_ul_x;
static jfieldID fid_Quad_ul_y;
static jfieldID fid_Quad_ur_x;
static jfieldID fid_Quad_ur_y;
static jfieldID fid_Rect_x0;
static jfieldID fid_Rect_x1;
static jfieldID fid_Rect_y0;
static jfieldID fid_Rect_y1;
static jfieldID fid_Shade_pointer;
static jfieldID fid_StrokeState_pointer;
static jfieldID fid_StructuredText_pointer;
static jfieldID fid_TextBlock_bbox;
static jfieldID fid_TextBlock_lines;
static jfieldID fid_TextChar_c;
static jfieldID fid_TextChar_quad;
static jfieldID fid_TextLine_bbox;
static jfieldID fid_TextLine_chars;
static jfieldID fid_TextWidgetCharLayout_advance;
static jfieldID fid_TextWidgetCharLayout_index;
static jfieldID fid_TextWidgetCharLayout_rect;
static jfieldID fid_TextWidgetCharLayout_x;
static jfieldID fid_TextWidgetLayout_invMatrix;
static jfieldID fid_TextWidgetLayout_lines;
static jfieldID fid_TextWidgetLayout_matrix;
static jfieldID fid_TextWidgetLineLayout_chars;
static jfieldID fid_TextWidgetLineLayout_fontSize;
static jfieldID fid_TextWidgetLineLayout_index;
static jfieldID fid_TextWidgetLineLayout_rect;
static jfieldID fid_TextWidgetLineLayout_x;
static jfieldID fid_TextWidgetLineLayout_y;
static jfieldID fid_Text_pointer;
static jfieldID fid_TreeArchive_pointer;

static jmethodID mid_Archive_init;
static jmethodID mid_Buffer_init;
static jmethodID mid_ColorSpace_fromPointer;
static jmethodID mid_ColorSpace_init;
static jmethodID mid_Context_Version_init;
static jmethodID mid_Context_Log_error;
static jmethodID mid_Context_Log_warning;
static jmethodID mid_DefaultAppearance_init;
static jmethodID mid_DefaultColorSpaces_init;
static jmethodID mid_DefaultColorSpaces_getDefaultGray;
static jmethodID mid_DefaultColorSpaces_getDefaultRGB;
static jmethodID mid_DefaultColorSpaces_getDefaultCMYK;
static jmethodID mid_DefaultColorSpaces_getOutputIntent;
static jmethodID mid_DefaultColorSpaces_setDefaultGray;
static jmethodID mid_DefaultColorSpaces_setDefaultRGB;
static jmethodID mid_DefaultColorSpaces_setDefaultCMYK;
static jmethodID mid_DefaultColorSpaces_setOutputIntent;
static jmethodID mid_Device_beginGroup;
static jmethodID mid_Device_beginLayer;
static jmethodID mid_Device_beginMask;
static jmethodID mid_Device_beginMetatext;
static jmethodID mid_Device_beginStructure;
static jmethodID mid_Device_beginTile;
static jmethodID mid_Device_clipImageMask;
static jmethodID mid_Device_clipPath;
static jmethodID mid_Device_clipStrokePath;
static jmethodID mid_Device_clipStrokeText;
static jmethodID mid_Device_clipText;
static jmethodID mid_Device_endGroup;
static jmethodID mid_Device_endLayer;
static jmethodID mid_Device_endMask;
static jmethodID mid_Device_endMetatext;
static jmethodID mid_Device_endStructure;
static jmethodID mid_Device_endTile;
static jmethodID mid_Device_fillImage;
static jmethodID mid_Device_fillImageMask;
static jmethodID mid_Device_fillPath;
static jmethodID mid_Device_fillShade;
static jmethodID mid_Device_fillText;
static jmethodID mid_Device_ignoreText;
static jmethodID mid_Device_init;
static jmethodID mid_Device_popClip;
static jmethodID mid_Device_renderFlags;
static jmethodID mid_Device_setDefaultColorSpaces;
static jmethodID mid_Device_strokePath;
static jmethodID mid_Device_strokeText;
static jmethodID mid_DisplayList_init;
static jmethodID mid_DocumentWriter_OCRListener_progress;
static jmethodID mid_Document_init;
static jmethodID mid_DOM_init;
static jmethodID mid_DOMAttribute_init;
static jmethodID mid_FitzInputStream_init;
static jmethodID mid_Font_init;
static jmethodID mid_Image_init;
static jmethodID mid_Link_init;
static jmethodID mid_Location_init;
static jmethodID mid_Matrix_init;
static jmethodID mid_MultiArchive_init;
static jmethodID mid_NativeDevice_init;
static jmethodID mid_Object_toString;
static jmethodID mid_Outline_init;
static jmethodID mid_OutlineItem_init;
static jmethodID mid_OutlineIterator_init;
static jmethodID mid_PDFAnnotation_init;
static jmethodID mid_LinkDestination_init;
static jmethodID mid_PDFDocument_JsEventListener_onAlert;
static jmethodID mid_PDFDocument_PDFEmbeddedFileParams_init;
static jmethodID mid_PDFDocument_init;
static jmethodID mid_PDFGraftMap_init;
static jmethodID mid_PDFObject_init;
static jmethodID mid_PDFPage_init;
static jmethodID mid_PDFWidget_init;
static jmethodID mid_PKCS7DistinguishedName_init;
static jmethodID mid_PKCS7Signer_maxDigest;
static jmethodID mid_PKCS7Signer_name;
static jmethodID mid_PKCS7Signer_sign;
static jmethodID mid_PKCS7Verifier_checkCertificate;
static jmethodID mid_PKCS7Verifier_checkDigest;
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
static jmethodID mid_SeekableOutputStream_truncate;
static jmethodID mid_SeekableOutputStream_write;
static jmethodID mid_SeekableStream_position;
static jmethodID mid_SeekableStream_seek;
static jmethodID mid_Shade_init;
static jmethodID mid_StrokeState_init;
static jmethodID mid_StructuredTextWalker_beginLine;
static jmethodID mid_StructuredTextWalker_beginTextBlock;
static jmethodID mid_StructuredTextWalker_endLine;
static jmethodID mid_StructuredTextWalker_endTextBlock;
static jmethodID mid_StructuredTextWalker_onChar;
static jmethodID mid_StructuredTextWalker_onImageBlock;
static jmethodID mid_StructuredText_init;
static jmethodID mid_TextBlock_init;
static jmethodID mid_TextChar_init;
static jmethodID mid_TextLine_init;
static jmethodID mid_TextWalker_showGlyph;
static jmethodID mid_TextWidgetCharLayout_init;
static jmethodID mid_TextWidgetLayout_init;
static jmethodID mid_TextWidgetLineLayout_init;
static jmethodID mid_Text_init;
static jmethodID mid_TreeArchive_init;

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

	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_MASK == FZ_DEVFLAG_MASK;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_COLOR == FZ_DEVFLAG_COLOR;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_UNCACHEABLE == FZ_DEVFLAG_UNCACHEABLE;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_FILLCOLOR_UNDEFINED == FZ_DEVFLAG_FILLCOLOR_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_STROKECOLOR_UNDEFINED == FZ_DEVFLAG_STROKECOLOR_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_STARTCAP_UNDEFINED == FZ_DEVFLAG_STARTCAP_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_DASHCAP_UNDEFINED == FZ_DEVFLAG_DASHCAP_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_ENDCAP_UNDEFINED == FZ_DEVFLAG_ENDCAP_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_LINEJOIN_UNDEFINED == FZ_DEVFLAG_LINEJOIN_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_MITERLIMIT_UNDEFINED == FZ_DEVFLAG_MITERLIMIT_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_LINEWIDTH_UNDEFINED == FZ_DEVFLAG_LINEWIDTH_UNDEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_BBOX_DEFINED == FZ_DEVFLAG_BBOX_DEFINED;
	valid &= com_artifex_mupdf_fitz_Device_DEVICE_FLAG_GRIDFIT_AS_TILED == FZ_DEVFLAG_GRIDFIT_AS_TILED;

	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_INVALID == FZ_STRUCTURE_INVALID;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_DOCUMENT == FZ_STRUCTURE_DOCUMENT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_PART == FZ_STRUCTURE_PART;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_ART == FZ_STRUCTURE_ART;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_SECT == FZ_STRUCTURE_SECT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_DIV == FZ_STRUCTURE_DIV;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_BLOCKQUOTE == FZ_STRUCTURE_BLOCKQUOTE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_CAPTION == FZ_STRUCTURE_CAPTION;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TOC == FZ_STRUCTURE_TOC;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TOCI == FZ_STRUCTURE_TOCI;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_INDEX == FZ_STRUCTURE_INDEX;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_NONSTRUCT == FZ_STRUCTURE_NONSTRUCT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_PRIVATE == FZ_STRUCTURE_PRIVATE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_DOCUMENTFRAGMENT == FZ_STRUCTURE_DOCUMENTFRAGMENT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_ASIDE == FZ_STRUCTURE_ASIDE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TITLE == FZ_STRUCTURE_TITLE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_FENOTE == FZ_STRUCTURE_FENOTE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_SUB == FZ_STRUCTURE_SUB;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_P == FZ_STRUCTURE_P;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_H == FZ_STRUCTURE_H;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_H1 == FZ_STRUCTURE_H1;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_H2 == FZ_STRUCTURE_H2;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_H3 == FZ_STRUCTURE_H3;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_H4 == FZ_STRUCTURE_H4;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_H5 == FZ_STRUCTURE_H5;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_H6 == FZ_STRUCTURE_H6;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_LIST == FZ_STRUCTURE_LIST;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_LISTITEM == FZ_STRUCTURE_LISTITEM;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_LABEL == FZ_STRUCTURE_LABEL;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_LISTBODY == FZ_STRUCTURE_LISTBODY;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TABLE == FZ_STRUCTURE_TABLE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TR == FZ_STRUCTURE_TR;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TH == FZ_STRUCTURE_TH;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TD == FZ_STRUCTURE_TD;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_THEAD == FZ_STRUCTURE_THEAD;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TBODY == FZ_STRUCTURE_TBODY;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_TFOOT == FZ_STRUCTURE_TFOOT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_SPAN == FZ_STRUCTURE_SPAN;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_QUOTE == FZ_STRUCTURE_QUOTE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_NOTE == FZ_STRUCTURE_NOTE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_REFERENCE == FZ_STRUCTURE_REFERENCE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_BIBENTRY == FZ_STRUCTURE_BIBENTRY;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_CODE == FZ_STRUCTURE_CODE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_LINK == FZ_STRUCTURE_LINK;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_ANNOT == FZ_STRUCTURE_ANNOT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_EM == FZ_STRUCTURE_EM;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_STRONG == FZ_STRUCTURE_STRONG;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_RUBY == FZ_STRUCTURE_RUBY;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_RB == FZ_STRUCTURE_RB;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_RT == FZ_STRUCTURE_RT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_RP == FZ_STRUCTURE_RP;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_WARICHU == FZ_STRUCTURE_WARICHU;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_WT == FZ_STRUCTURE_WT;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_WP == FZ_STRUCTURE_WP;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_FIGURE == FZ_STRUCTURE_FIGURE;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_FORMULA == FZ_STRUCTURE_FORMULA;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_FORM == FZ_STRUCTURE_FORM;
	valid &= com_artifex_mupdf_fitz_Device_STRUCTURE_ARTIFACT == FZ_STRUCTURE_ARTIFACT;

	valid &= com_artifex_mupdf_fitz_Device_METATEXT_ACTUALTEXT == FZ_METATEXT_ACTUALTEXT;
	valid &= com_artifex_mupdf_fitz_Device_METATEXT_ALT == FZ_METATEXT_ALT;
	valid &= com_artifex_mupdf_fitz_Device_METATEXT_ABBREVIATION == FZ_METATEXT_ABBREVIATION;
	valid &= com_artifex_mupdf_fitz_Device_METATEXT_TITLE == FZ_METATEXT_TITLE;

	valid &= com_artifex_mupdf_fitz_Font_SIMPLE_ENCODING_LATIN == PDF_SIMPLE_ENCODING_LATIN;
	valid &= com_artifex_mupdf_fitz_Font_SIMPLE_ENCODING_GREEK == PDF_SIMPLE_ENCODING_GREEK;
	valid &= com_artifex_mupdf_fitz_Font_SIMPLE_ENCODING_CYRILLIC == PDF_SIMPLE_ENCODING_CYRILLIC;

	valid &= com_artifex_mupdf_fitz_Font_ADOBE_CNS == FZ_ADOBE_CNS;
	valid &= com_artifex_mupdf_fitz_Font_ADOBE_GB == FZ_ADOBE_GB;
	valid &= com_artifex_mupdf_fitz_Font_ADOBE_JAPAN == FZ_ADOBE_JAPAN;
	valid &= com_artifex_mupdf_fitz_Font_ADOBE_KOREA == FZ_ADOBE_KOREA;

	valid &= com_artifex_mupdf_fitz_Page_MEDIA_BOX == FZ_MEDIA_BOX;
	valid &= com_artifex_mupdf_fitz_Page_CROP_BOX == FZ_CROP_BOX;
	valid &= com_artifex_mupdf_fitz_Page_ART_BOX == FZ_ART_BOX;
	valid &= com_artifex_mupdf_fitz_Page_TRIM_BOX == FZ_TRIM_BOX;
	valid &= com_artifex_mupdf_fitz_Page_BLEED_BOX == FZ_BLEED_BOX;
	valid &= com_artifex_mupdf_fitz_Page_UNKNOWN_BOX == FZ_UNKNOWN_BOX;

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

	valid &= com_artifex_mupdf_fitz_PDFAnnotation_BORDER_STYLE_SOLID == PDF_BORDER_STYLE_SOLID;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_BORDER_STYLE_DASHED == PDF_BORDER_STYLE_DASHED;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_BORDER_STYLE_BEVELED == PDF_BORDER_STYLE_BEVELED;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_BORDER_STYLE_INSET == PDF_BORDER_STYLE_INSET;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_BORDER_STYLE_UNDERLINE == PDF_BORDER_STYLE_UNDERLINE;

	valid &= com_artifex_mupdf_fitz_PDFAnnotation_BORDER_EFFECT_NONE == PDF_BORDER_EFFECT_NONE;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_BORDER_EFFECT_CLOUDY == PDF_BORDER_EFFECT_CLOUDY;

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
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_RICH_MEDIA == PDF_ANNOT_RICH_MEDIA;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_WIDGET == PDF_ANNOT_WIDGET;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_SCREEN == PDF_ANNOT_SCREEN;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_PRINTER_MARK == PDF_ANNOT_PRINTER_MARK;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_TRAP_NET == PDF_ANNOT_TRAP_NET;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_WATERMARK == PDF_ANNOT_WATERMARK;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_3D == PDF_ANNOT_3D;
	valid &= com_artifex_mupdf_fitz_PDFAnnotation_TYPE_PROJECTION == PDF_ANNOT_PROJECTION;
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

	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_UNSET == FZ_LANG_UNSET;
	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_ur == FZ_LANG_ur;
	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_urd == FZ_LANG_urd;
	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_ko == FZ_LANG_ko;
	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_ja == FZ_LANG_ja;
	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_zh == FZ_LANG_zh;
	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_zh_Hans == FZ_LANG_zh_Hans;
	valid &= com_artifex_mupdf_fitz_PDFDocument_LANGUAGE_zh_Hant == FZ_LANG_zh_Hant;

	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_FIT == FZ_LINK_DEST_FIT;
	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_FIT_B == FZ_LINK_DEST_FIT_B;
	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_FIT_H == FZ_LINK_DEST_FIT_H;
	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_FIT_BH == FZ_LINK_DEST_FIT_BH;
	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_FIT_V == FZ_LINK_DEST_FIT_V;
	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_FIT_BV == FZ_LINK_DEST_FIT_BV;
	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_FIT_R == FZ_LINK_DEST_FIT_R;
	valid &= com_artifex_mupdf_fitz_LinkDestination_LINK_DEST_XYZ == FZ_LINK_DEST_XYZ;

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

	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_FIELD_IS_READ_ONLY == PDF_FIELD_IS_READ_ONLY;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_FIELD_IS_REQUIRED == PDF_FIELD_IS_REQUIRED;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_FIELD_IS_NO_EXPORT == PDF_FIELD_IS_NO_EXPORT;

	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_TX_FIELD_IS_MULTILINE == PDF_TX_FIELD_IS_MULTILINE;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_TX_FIELD_IS_PASSWORD == PDF_TX_FIELD_IS_PASSWORD;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_TX_FIELD_IS_COMB == PDF_TX_FIELD_IS_COMB;

	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_BTN_FIELD_IS_NO_TOGGLE_TO_OFF == PDF_BTN_FIELD_IS_NO_TOGGLE_TO_OFF;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_BTN_FIELD_IS_RADIO == PDF_BTN_FIELD_IS_RADIO;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_BTN_FIELD_IS_PUSHBUTTON == PDF_BTN_FIELD_IS_PUSHBUTTON;

	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_CH_FIELD_IS_COMBO == PDF_CH_FIELD_IS_COMBO;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_CH_FIELD_IS_EDIT == PDF_CH_FIELD_IS_EDIT;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_CH_FIELD_IS_SORT == PDF_CH_FIELD_IS_SORT;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_CH_FIELD_IS_MULTI_SELECT == PDF_CH_FIELD_IS_MULTI_SELECT;

	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_SIGNATURE_SHOW_LABELS == PDF_SIGNATURE_SHOW_LABELS;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_SIGNATURE_SHOW_DN == PDF_SIGNATURE_SHOW_DN;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_SIGNATURE_SHOW_DATE == PDF_SIGNATURE_SHOW_DATE;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_SIGNATURE_SHOW_TEXT_NAME == PDF_SIGNATURE_SHOW_TEXT_NAME;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_SIGNATURE_SHOW_GRAPHIC_NAME == PDF_SIGNATURE_SHOW_GRAPHIC_NAME;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_SIGNATURE_SHOW_LOGO == PDF_SIGNATURE_SHOW_LOGO;
	valid &= com_artifex_mupdf_fitz_PDFWidget_PDF_SIGNATURE_DEFAULT_APPEARANCE == PDF_SIGNATURE_DEFAULT_APPEARANCE;

	valid &= com_artifex_mupdf_fitz_PDFPage_REDACT_IMAGE_NONE == PDF_REDACT_IMAGE_NONE;
	valid &= com_artifex_mupdf_fitz_PDFPage_REDACT_IMAGE_REMOVE == PDF_REDACT_IMAGE_REMOVE;
	valid &= com_artifex_mupdf_fitz_PDFPage_REDACT_IMAGE_PIXELS == PDF_REDACT_IMAGE_PIXELS;

	return valid ? 1 : 0;
}

/* Helper functions to set the java exception flag. */

static void jni_throw_imp(JNIEnv *env, jclass cls, const char *mess)
{
	(*env)->ThrowNew(env, cls, mess);
}

#define jni_throw_void(env, info) do { jni_throw_imp(env, info); return; }
#define jni_throw(env, info) do { jni_throw_imp(env, info); return 0; }

static void jni_rethrow_imp(JNIEnv *env, fz_context *ctx)
{
	if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
		jni_throw_imp(env, cls_TryLaterException, fz_caught_message(ctx));
	else
		jni_throw_imp(env, cls_RuntimeException, fz_caught_message(ctx));
}

#define jni_rethrow_void(env, ctx) do { jni_rethrow_imp(env, ctx); return; } while (0);
#define jni_rethrow(env, ctx) do { jni_rethrow_imp(env, ctx); return 0; } while (0);

static void jni_throw_run_imp(JNIEnv *env, const char *info)
{
	jni_throw_imp(env, cls_RuntimeException, info);
}

#define jni_throw_run_void(env, info) do { jni_throw_run_imp(env, info); return; } while (0);
#define jni_throw_run(env, info) do { jni_throw_run_imp(env, info); return 0; } while (0);

static void jni_throw_oom_imp(JNIEnv *env, const char *info)
{
	jni_throw_imp(env, cls_OutOfMemoryError, info);
}

#define jni_throw_oom_void(env, info) do { jni_throw_oom_imp(env, info); return; } while (0);
#define jni_throw_oom(env, info) do { jni_throw_oom_imp(env, info); return 0; } while (0);

static void jni_throw_oob_imp(JNIEnv *env, const char *info)
{
	jni_throw_imp(env, cls_IndexOutOfBoundsException, info);
}

#define jni_throw_oob_void(env, info) do { jni_throw_oob_imp(env, info); return; } while (0);
#define jni_throw_oob(env, info) do { jni_throw_oob_imp(env, info); return 0; } while (0);

static void jni_throw_arg_imp(JNIEnv *env, const char *info)
{
	jni_throw_imp(env, cls_IllegalArgumentException, info);
}

#define jni_throw_arg_void(env, info) do { jni_throw_arg_imp(env, info); return; } while (0);
#define jni_throw_arg(env, info) do { jni_throw_arg_imp(env, info); return 0; } while (0);

static void jni_throw_io_imp(JNIEnv *env, const char *info)
{
	jni_throw_imp(env, cls_IOException, info);
}

#define jni_throw_io_void(env, info) do { jni_throw_io_imp(env, info); return; } while (0);
#define jni_throw_io(env, info) do { jni_throw_io_imp(env, info); return 0; } while (0);

static void jni_throw_null_imp(JNIEnv *env, const char *info)
{
	jni_throw_imp(env, cls_NullPointerException, info);
}

#define jni_throw_null_void(env, info) do { jni_throw_null_imp(env, info); return; } while (0);
#define jni_throw_null(env, info) do { jni_throw_null_imp(env, info); return 0; } while (0);

static void jni_throw_uoe_imp(JNIEnv *env, const char *info)
{
	jni_throw_imp(env, cls_UnsupportedOperationException, info);
}

#define jni_throw_uoe_void(env, info) do { jni_throw_uoe_imp(env, info); return; } while (0);
#define jni_throw_uoe(env, info) do { jni_throw_uoe_imp(env, info); return 0; } while (0);

/* Convert a java exception and throw into fitz. */

static void fz_throw_java_and_detach_thread(fz_context *ctx, JNIEnv *env, jboolean detach)
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
				if (detach)
					(*jvm)->DetachCurrentThread(jvm);
				fz_throw(ctx, FZ_ERROR_GENERIC, "%s", buf);
			}
		}
	}
	if (detach)
		(*jvm)->DetachCurrentThread(jvm);
	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown java error");
}

#define fz_throw_java(ctx, env) fz_throw_java_and_detach_thread((ctx), (env), JNI_FALSE)

#define fz_throw_and_detach_thread(ctx, detach, code, ...) \
	do \
	{ \
		if (detach) \
			(*jvm)->DetachCurrentThread(jvm); \
		fz_throw((ctx), (code), __VA_ARGS__); \
	} while (0)

#define fz_rethrow_and_detach_thread(ctx, detach) \
	do \
	{ \
		if (detach) \
			(*jvm)->DetachCurrentThread(jvm); \
		fz_rethrow(ctx); \
	} while (0)

typedef struct {
	pdf_pkcs7_verifier base;
	jobject jverifier;
} java_pkcs7_verifier;

/* Load classes, field and method IDs. */

static const char *current_class_name = NULL;
static jclass current_class = NULL;

static jclass get_class(int *failed, JNIEnv *env, const char *name)
{
	jclass local;

	if (*failed) return NULL;

	current_class_name = name;
	local = (*env)->FindClass(env, name);
	if (!local || (*env)->ExceptionCheck(env))
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
	if (fid == 0 || (*env)->ExceptionCheck(env))
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
	if (fid == 0 || (*env)->ExceptionCheck(env))
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
	if (mid == 0 || (*env)->ExceptionCheck(env))
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
	if (mid == 0 || (*env)->ExceptionCheck(env))
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

	/* Get and store the main JVM pointer. We need this in order to get
	 * JNIEnv pointers on callback threads. This is specifically
	 * guaranteed to be safe to store in a static var. */

	getvmErr = (*env)->GetJavaVM(env, &jvm);
	if (getvmErr < 0)
	{
		LOGE("cannot get JVM interface (error %d)", getvmErr);
		return -1;
	}

	/* Look up Context first as it is required for logging. E.g. when
	 * classes' statics are being executed which may cause logging. */

	cls_Context = get_class(&err, env, PKG"Context");
	fid_Context_log = get_static_field(&err, env, "log", "L"PKG"Context$Log;");
	fid_Context_lock = get_static_field(&err, env, "lock", "Ljava/lang/Object;");

	cls_Context_Log = get_class(&err, env, PKG"Context$Log");
	mid_Context_Log_error = get_method(&err, env, "error", "(Ljava/lang/String;)V");
	mid_Context_Log_warning = get_method(&err, env, "warning", "(Ljava/lang/String;)V");

	cls_Context_Version = get_class(&err, env, PKG"Context$Version");
	fid_Context_Version_major = get_field(&err, env, "major", "I");
	fid_Context_Version_minor = get_field(&err, env, "minor", "I");
	fid_Context_Version_patch = get_field(&err, env, "patch", "I");
	fid_Context_Version_version = get_field(&err, env, "version", "Ljava/lang/String;");
	mid_Context_Version_init = get_method(&err, env, "<init>", "()V");

	/* MuPDF classes */

	cls_Archive = get_class(&err, env, PKG"Archive");
	mid_Archive_init = get_method(&err, env, "<init>", "(J)V");
	fid_Archive_pointer = get_field(&err, env, "pointer", "J");

	cls_Buffer = get_class(&err, env, PKG"Buffer");
	mid_Buffer_init = get_method(&err, env, "<init>", "(J)V");
	fid_Buffer_pointer = get_field(&err, env, "pointer", "J");

	cls_ColorSpace = get_class(&err, env, PKG"ColorSpace");
	fid_ColorSpace_pointer = get_field(&err, env, "pointer", "J");
	mid_ColorSpace_init = get_method(&err, env, "<init>", "(J)V");
	mid_ColorSpace_fromPointer = get_static_method(&err, env, "fromPointer", "(J)L"PKG"ColorSpace;");

	cls_Cookie = get_class(&err, env, PKG"Cookie");
	fid_Cookie_pointer = get_field(&err, env, "pointer", "J");

	cls_DefaultAppearance = get_class(&err, env, PKG"DefaultAppearance");
	fid_DefaultAppearance_color = get_field(&err, env, "color", "[F");
	fid_DefaultAppearance_font = get_field(&err, env, "font", "Ljava/lang/String;");
	fid_DefaultAppearance_size = get_field(&err, env, "size", "F");
	mid_DefaultAppearance_init = get_method(&err, env, "<init>", "()V");

	cls_DefaultColorSpaces = get_class(&err, env, PKG"DefaultColorSpaces");
	fid_DefaultColorSpaces_pointer = get_field(&err, env, "pointer", "J");
	mid_DefaultColorSpaces_init = get_method(&err, env, "<init>", "(J)V");
	mid_DefaultColorSpaces_setDefaultGray = get_method(&err, env, "setDefaultGray", "(L"PKG"ColorSpace;)V");
	mid_DefaultColorSpaces_setDefaultRGB = get_method(&err, env, "setDefaultRGB", "(L"PKG"ColorSpace;)V");
	mid_DefaultColorSpaces_setDefaultCMYK = get_method(&err, env, "setDefaultCMYK", "(L"PKG"ColorSpace;)V");
	mid_DefaultColorSpaces_setOutputIntent = get_method(&err, env, "setOutputIntent", "(L"PKG"ColorSpace;)V");
	mid_DefaultColorSpaces_getDefaultGray = get_method(&err, env, "getDefaultGray", "()L"PKG"ColorSpace;");
	mid_DefaultColorSpaces_getDefaultRGB = get_method(&err, env, "getDefaultRGB", "()L"PKG"ColorSpace;");
	mid_DefaultColorSpaces_getDefaultCMYK = get_method(&err, env, "getDefaultCMYK", "()L"PKG"ColorSpace;");
	mid_DefaultColorSpaces_getOutputIntent = get_method(&err, env, "getOutputIntent", "()L"PKG"ColorSpace;");

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
	mid_Device_renderFlags = get_method(&err, env, "renderFlags", "(II)V");
	mid_Device_setDefaultColorSpaces = get_method(&err, env, "setDefaultColorSpaces", "(L"PKG"DefaultColorSpaces;)V");
	mid_Device_beginStructure = get_method(&err, env, "beginStructure", "(ILjava/lang/String;I)V");
	mid_Device_endStructure = get_method(&err, env, "endStructure", "()V");
	mid_Device_beginMetatext = get_method(&err, env, "beginMetatext", "(ILjava/lang/String;)V");
	mid_Device_endMetatext = get_method(&err, env, "endMetatext", "()V");

	cls_DisplayList = get_class(&err, env, PKG"DisplayList");
	fid_DisplayList_pointer = get_field(&err, env, "pointer", "J");
	mid_DisplayList_init = get_method(&err, env, "<init>", "(J)V");

	cls_Document = get_class(&err, env, PKG"Document");
	fid_Document_pointer = get_field(&err, env, "pointer", "J");
	mid_Document_init = get_method(&err, env, "<init>", "(J)V");

	cls_DocumentWriter = get_class(&err, env, PKG"DocumentWriter");
	fid_DocumentWriter_pointer = get_field(&err, env, "pointer", "J");
	fid_DocumentWriter_ocrlistener = get_field(&err, env, "ocrlistener", "J");

	cls_DocumentWriter_OCRListener = get_class(&err, env, PKG"DocumentWriter$OCRListener");
	mid_DocumentWriter_OCRListener_progress = get_method(&err, env, "progress", "(II)Z");

	cls_DOM = get_class(&err, env, PKG"DOM");
	fid_DOM_pointer = get_field(&err, env, "pointer", "J");
	mid_DOM_init = get_method(&err, env, "<init>", "(J)V");

	cls_DOMAttribute = get_class(&err, env, PKG"DOM$DOMAttribute");
	fid_DOMAttribute_attribute = get_field(&err, env, "attribute", "Ljava/lang/String;");
	fid_DOMAttribute_value = get_field(&err, env, "value", "Ljava/lang/String;");
	mid_DOMAttribute_init = get_method(&err, env, "<init>", "()V");

	cls_FitzInputStream = get_class(&err, env, PKG"FitzInputStream");
	fid_FitzInputStream_pointer = get_field(&err, env, "pointer", "J");
	fid_FitzInputStream_markpos = get_field(&err, env, "markpos", "J");
	fid_FitzInputStream_closed = get_field(&err, env, "closed", "Z");
	mid_FitzInputStream_init = get_method(&err, env, "<init>", "(J)V");

	cls_Font = get_class(&err, env, PKG"Font");
	fid_Font_pointer = get_field(&err, env, "pointer", "J");
	mid_Font_init = get_method(&err, env, "<init>", "(J)V");

	cls_Story = get_class(&err, env, PKG"Story");
	fid_Story_pointer = get_field(&err, env, "pointer", "J");

	cls_Image = get_class(&err, env, PKG"Image");
	fid_Image_pointer = get_field(&err, env, "pointer", "J");
	mid_Image_init = get_method(&err, env, "<init>", "(J)V");

	cls_Link = get_class(&err, env, PKG"Link");
	fid_Link_pointer = get_field(&err, env, "pointer", "J");
	mid_Link_init = get_method(&err, env, "<init>", "(J)V");

	cls_Location = get_class(&err, env, PKG"Location");
	mid_Location_init = get_method(&err, env, "<init>", "(II)V");

	cls_Matrix = get_class(&err, env, PKG"Matrix");
	fid_Matrix_a = get_field(&err, env, "a", "F");
	fid_Matrix_b = get_field(&err, env, "b", "F");
	fid_Matrix_c = get_field(&err, env, "c", "F");
	fid_Matrix_d = get_field(&err, env, "d", "F");
	fid_Matrix_e = get_field(&err, env, "e", "F");
	fid_Matrix_f = get_field(&err, env, "f", "F");
	mid_Matrix_init = get_method(&err, env, "<init>", "(FFFFFF)V");

	cls_MultiArchive = get_class(&err, env, PKG"MultiArchive");
	mid_MultiArchive_init = get_method(&err, env, "<init>", "(J)V");
	fid_MultiArchive_pointer = get_field(&err, env, "pointer", "J");

	cls_NativeDevice = get_class(&err, env, PKG"NativeDevice");
	fid_NativeDevice_nativeResource = get_field(&err, env, "nativeResource", "Ljava/lang/Object;");
	fid_NativeDevice_nativeInfo = get_field(&err, env, "nativeInfo", "J");
	mid_NativeDevice_init = get_method(&err, env, "<init>", "(J)V");

	cls_Outline = get_class(&err, env, PKG"Outline");
	mid_Outline_init = get_method(&err, env, "<init>", "(Ljava/lang/String;Ljava/lang/String;[L"PKG"Outline;)V");

	cls_OutlineItem = get_class(&err, env, PKG"OutlineIterator$OutlineItem");
	mid_OutlineItem_init = get_method(&err, env, "<init>", "(Ljava/lang/String;Ljava/lang/String;Z)V");

	cls_OutlineIterator = get_class(&err, env, PKG"OutlineIterator");
	fid_OutlineIterator_pointer = get_field(&err, env, "pointer", "J");
	mid_OutlineIterator_init = get_method(&err, env, "<init>", "(J)V");

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

	cls_LinkDestination = get_class(&err, env, PKG"LinkDestination");
	mid_LinkDestination_init = get_method(&err, env, "<init>", "(IIIFFFFF)V");
	fid_LinkDestination_chapter = get_field(&err, env, "chapter", "I");
	fid_LinkDestination_page = get_field(&err, env, "page", "I");
	fid_LinkDestination_type = get_field(&err, env, "type", "I");
	fid_LinkDestination_x = get_field(&err, env, "x", "F");
	fid_LinkDestination_y = get_field(&err, env, "y", "F");
	fid_LinkDestination_width = get_field(&err, env, "width", "F");
	fid_LinkDestination_height = get_field(&err, env, "height", "F");
	fid_LinkDestination_zoom = get_field(&err, env, "zoom", "F");

	cls_PDFDocument_JsEventListener = get_class(&err, env, PKG"PDFDocument$JsEventListener");
	mid_PDFDocument_JsEventListener_onAlert = get_method(&err, env, "onAlert", "(L"PKG"PDFDocument;Ljava/lang/String;Ljava/lang/String;IIZLjava/lang/String;Z)L"PKG"PDFDocument$JsEventListener$AlertResult;");

	cls_PDFDocument_PDFEmbeddedFileParams = get_class(&err, env, PKG"PDFDocument$PDFEmbeddedFileParams");
	mid_PDFDocument_PDFEmbeddedFileParams_init = get_method(&err, env, "<init>", "(Ljava/lang/String;Ljava/lang/String;IJJ)V");

	cls_AlertResult = get_class(&err, env, PKG"PDFDocument$JsEventListener$AlertResult");
	fid_AlertResult_buttonPressed = get_field(&err, env, "buttonPressed", "I");
	fid_AlertResult_checkboxChecked = get_field(&err, env, "checkboxChecked", "Z");

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

	cls_PDFWidget = get_class(&err, env, PKG"PDFWidget");
	fid_PDFWidget_pointer = get_field(&err, env, "pointer", "J");
	fid_PDFWidget_fieldType = get_field(&err, env, "fieldType", "I");
	fid_PDFWidget_textFormat = get_field(&err, env, "textFormat", "I");
	fid_PDFWidget_maxLen = get_field(&err, env, "maxLen", "I");
	fid_PDFWidget_fieldFlags = get_field(&err, env, "fieldFlags", "I");
	fid_PDFWidget_options = get_field(&err, env, "options", "[Ljava/lang/String;");
	mid_PDFWidget_init = get_method(&err, env, "<init>", "(J)V");

	cls_PKCS7Signer = get_class(&err, env, PKG"PKCS7Signer");
	fid_PKCS7Signer_pointer = get_field(&err, env, "pointer", "J");
	mid_PKCS7Signer_name = get_method(&err, env, "name", "()L"PKG"PKCS7DistinguishedName;");
	mid_PKCS7Signer_sign = get_method(&err, env, "sign", "(L"PKG"FitzInputStream;)[B");
	mid_PKCS7Signer_maxDigest = get_method(&err, env, "maxDigest", "()I");

	cls_PKCS7Verifier = get_class(&err, env, PKG"PKCS7Verifier");
	fid_PKCS7Verifier_pointer = get_field(&err, env, "pointer", "J");
	mid_PKCS7Verifier_checkCertificate = get_method(&err, env, "checkCertificate", "([B)I");
	mid_PKCS7Verifier_checkDigest = get_method(&err, env, "checkDigest", "(L"PKG"FitzInputStream;[B)I");

	cls_PKCS7DistinguishedName = get_class(&err, env, PKG"PKCS7DistinguishedName");
	fid_PKCS7DistinguishedName_cn = get_field(&err, env, "cn", "Ljava/lang/String;");
	fid_PKCS7DistinguishedName_c = get_field(&err, env, "c", "Ljava/lang/String;");
	fid_PKCS7DistinguishedName_o = get_field(&err, env, "o", "Ljava/lang/String;");
	fid_PKCS7DistinguishedName_ou = get_field(&err, env, "ou", "Ljava/lang/String;");
	fid_PKCS7DistinguishedName_email = get_field(&err, env, "email", "Ljava/lang/String;");
	mid_PKCS7DistinguishedName_init = get_method(&err, env, "<init>", "()V");

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

	cls_ArrayOfQuad = get_class(&err, env, "[L"PKG"Quad;");

	cls_Rect = get_class(&err, env, PKG"Rect");
	fid_Rect_x0 = get_field(&err, env, "x0", "F");
	fid_Rect_x1 = get_field(&err, env, "x1", "F");
	fid_Rect_y0 = get_field(&err, env, "y0", "F");
	fid_Rect_y1 = get_field(&err, env, "y1", "F");
	mid_Rect_init = get_method(&err, env, "<init>", "(FFFF)V");

	cls_SeekableInputStream = get_class(&err, env, PKG"SeekableInputStream");
	mid_SeekableInputStream_read = get_method(&err, env, "read", "([B)I");

	cls_SeekableOutputStream = get_class(&err, env, PKG"SeekableOutputStream");
	mid_SeekableOutputStream_truncate = get_method(&err, env, "truncate", "()V");
	mid_SeekableOutputStream_write = get_method(&err, env, "write", "([BII)V");

	cls_SeekableStream = get_class(&err, env, PKG"SeekableStream");
	mid_SeekableStream_position = get_method(&err, env, "position", "()J");
	mid_SeekableStream_seek = get_method(&err, env, "seek", "(JI)J");

	cls_Shade = get_class(&err, env, PKG"Shade");
	fid_Shade_pointer = get_field(&err, env, "pointer", "J");
	mid_Shade_init = get_method(&err, env, "<init>", "(J)V");

	cls_String = get_class(&err, env, "java/lang/String");

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
	mid_StructuredTextWalker_beginLine = get_method(&err, env, "beginLine", "(L"PKG"Rect;IL"PKG"Point;)V");
	mid_StructuredTextWalker_endLine = get_method(&err, env, "endLine", "()V");
	mid_StructuredTextWalker_onChar = get_method(&err, env, "onChar", "(IL"PKG"Point;L"PKG"Font;FL"PKG"Quad;)V");

	cls_Text = get_class(&err, env, PKG"Text");
	fid_Text_pointer = get_field(&err, env, "pointer", "J");
	mid_Text_init = get_method(&err, env, "<init>", "(J)V");

	cls_TextBlock = get_class(&err, env, PKG"StructuredText$TextBlock");
	fid_TextBlock_bbox = get_field(&err, env, "bbox", "L"PKG"Rect;");
	fid_TextBlock_lines = get_field(&err, env, "lines", "[L"PKG"StructuredText$TextLine;");
	mid_TextBlock_init = get_method(&err, env, "<init>", "()V");

	cls_TextChar = get_class(&err, env, PKG"StructuredText$TextChar");
	fid_TextChar_quad = get_field(&err, env, "quad", "L"PKG"Quad;");
	fid_TextChar_c = get_field(&err, env, "c", "I");
	mid_TextChar_init = get_method(&err, env, "<init>", "()V");

	cls_TextLine = get_class(&err, env, PKG"StructuredText$TextLine");
	fid_TextLine_bbox = get_field(&err, env, "bbox", "L"PKG"Rect;");
	fid_TextLine_chars = get_field(&err, env, "chars", "[L"PKG"StructuredText$TextChar;");
	mid_TextLine_init = get_method(&err, env, "<init>", "()V");

	cls_TextWalker = get_class(&err, env, PKG"TextWalker");
	mid_TextWalker_showGlyph = get_method(&err, env, "showGlyph", "(L"PKG"Font;L"PKG"Matrix;IIZ)V");

	cls_TextWidgetLayout = get_class(&err, env, PKG"PDFWidget$TextWidgetLayout");
	fid_TextWidgetLayout_matrix = get_field(&err, env, "matrix", "L"PKG"Matrix;");
	fid_TextWidgetLayout_invMatrix = get_field(&err, env, "invMatrix", "L"PKG"Matrix;");
	fid_TextWidgetLayout_lines = get_field(&err, env, "lines", "[L"PKG"PDFWidget$TextWidgetLineLayout;");
	mid_TextWidgetLayout_init = get_method(&err, env, "<init>", "()V");

	cls_TextWidgetLineLayout = get_class(&err, env, PKG"PDFWidget$TextWidgetLineLayout");
	fid_TextWidgetLineLayout_x = get_field(&err, env, "x", "F");
	fid_TextWidgetLineLayout_y = get_field(&err, env, "y", "F");
	fid_TextWidgetLineLayout_fontSize = get_field(&err, env, "fontSize", "F");
	fid_TextWidgetLineLayout_index = get_field(&err, env, "index", "I");
	fid_TextWidgetLineLayout_rect = get_field(&err, env, "rect", "L"PKG"Rect;");
	fid_TextWidgetLineLayout_chars = get_field(&err, env, "chars", "[L"PKG"PDFWidget$TextWidgetCharLayout;");
	mid_TextWidgetLineLayout_init = get_method(&err, env, "<init>", "()V");

	cls_TextWidgetCharLayout = get_class(&err, env, PKG"PDFWidget$TextWidgetCharLayout");
	fid_TextWidgetCharLayout_x = get_field(&err, env, "x", "F");
	fid_TextWidgetCharLayout_advance = get_field(&err, env, "advance", "F");
	fid_TextWidgetCharLayout_index = get_field(&err, env, "index", "I");
	fid_TextWidgetCharLayout_rect = get_field(&err, env, "rect", "L"PKG"Rect;");
	mid_TextWidgetCharLayout_init = get_method(&err, env, "<init>", "()V");

	cls_TreeArchive = get_class(&err, env, PKG"TreeArchive");
	mid_TreeArchive_init = get_method(&err, env, "<init>", "(J)V");
	fid_TreeArchive_pointer = get_field(&err, env, "pointer", "J");

	cls_AbortException = get_class(&err, env, PKG"AbortException");
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
	cls_UnsupportedOperationException = get_class(&err, env, "java/lang/UnsupportedOperationException");

	cls_OutOfMemoryError = get_class(&err, env, "java/lang/OutOfMemoryError");

	if (err)
	{
		LOGE("one or more class, member or field IDs could not be found");
		return -1;
	}

	return 0;
}

/* When making callbacks from C to java, we may be called on threads
 * other than the foreground. As such, we have no JNIEnv. This function
 * handles getting us the required environment */
static JNIEnv *jni_attach_thread(jboolean *detach)
{
	JNIEnv *env = NULL;
	int state;

	*detach = JNI_FALSE;
	state = (*jvm)->GetEnv(jvm, (void *)&env, MY_JNI_VERSION);
	if (state == JNI_EDETACHED)
	{
		*detach = JNI_TRUE;
		state = (*jvm)->AttachCurrentThread(jvm, (void *)&env, NULL);
	}

	if (state != JNI_OK) return NULL;

	return env;
}

static void jni_detach_thread(jboolean detach)
{
	if (!detach) return;
	(*jvm)->DetachCurrentThread(jvm);
}

static void lose_fids(JNIEnv *env)
{
	(*env)->DeleteGlobalRef(env, cls_AbortException);
	(*env)->DeleteGlobalRef(env, cls_AlertResult);
	(*env)->DeleteGlobalRef(env, cls_Archive);
	(*env)->DeleteGlobalRef(env, cls_ArrayOfQuad);
	(*env)->DeleteGlobalRef(env, cls_Buffer);
	(*env)->DeleteGlobalRef(env, cls_ColorSpace);
	(*env)->DeleteGlobalRef(env, cls_Context);
	(*env)->DeleteGlobalRef(env, cls_Context_Log);
	(*env)->DeleteGlobalRef(env, cls_Context_Version);
	(*env)->DeleteGlobalRef(env, cls_Cookie);
	(*env)->DeleteGlobalRef(env, cls_DefaultAppearance);
	(*env)->DeleteGlobalRef(env, cls_DefaultColorSpaces);
	(*env)->DeleteGlobalRef(env, cls_Device);
	(*env)->DeleteGlobalRef(env, cls_DisplayList);
	(*env)->DeleteGlobalRef(env, cls_Document);
	(*env)->DeleteGlobalRef(env, cls_DocumentWriter);
	(*env)->DeleteGlobalRef(env, cls_DOM);
	(*env)->DeleteGlobalRef(env, cls_DOMAttribute);
	(*env)->DeleteGlobalRef(env, cls_FitzInputStream);
	(*env)->DeleteGlobalRef(env, cls_FloatArray);
	(*env)->DeleteGlobalRef(env, cls_Font);
	(*env)->DeleteGlobalRef(env, cls_Story);
	(*env)->DeleteGlobalRef(env, cls_IOException);
	(*env)->DeleteGlobalRef(env, cls_IllegalArgumentException);
	(*env)->DeleteGlobalRef(env, cls_Image);
	(*env)->DeleteGlobalRef(env, cls_IndexOutOfBoundsException);
	(*env)->DeleteGlobalRef(env, cls_IntegerArray);
	(*env)->DeleteGlobalRef(env, cls_Link);
	(*env)->DeleteGlobalRef(env, cls_LinkDestination);
	(*env)->DeleteGlobalRef(env, cls_Location);
	(*env)->DeleteGlobalRef(env, cls_Matrix);
	(*env)->DeleteGlobalRef(env, cls_MultiArchive);
	(*env)->DeleteGlobalRef(env, cls_NativeDevice);
	(*env)->DeleteGlobalRef(env, cls_NullPointerException);
	(*env)->DeleteGlobalRef(env, cls_Object);
	(*env)->DeleteGlobalRef(env, cls_OutOfMemoryError);
	(*env)->DeleteGlobalRef(env, cls_Outline);
	(*env)->DeleteGlobalRef(env, cls_OutlineItem);
	(*env)->DeleteGlobalRef(env, cls_OutlineIterator);
	(*env)->DeleteGlobalRef(env, cls_PDFAnnotation);
	(*env)->DeleteGlobalRef(env, cls_PDFDocument);
	(*env)->DeleteGlobalRef(env, cls_PDFDocument_JsEventListener);
	(*env)->DeleteGlobalRef(env, cls_PDFDocument_PDFEmbeddedFileParams);
	(*env)->DeleteGlobalRef(env, cls_PDFGraftMap);
	(*env)->DeleteGlobalRef(env, cls_PDFObject);
	(*env)->DeleteGlobalRef(env, cls_PDFPage);
	(*env)->DeleteGlobalRef(env, cls_PDFWidget);
	(*env)->DeleteGlobalRef(env, cls_PKCS7DistinguishedName);
	(*env)->DeleteGlobalRef(env, cls_PKCS7Signer);
	(*env)->DeleteGlobalRef(env, cls_PKCS7Verifier);
	(*env)->DeleteGlobalRef(env, cls_Page);
	(*env)->DeleteGlobalRef(env, cls_Path);
	(*env)->DeleteGlobalRef(env, cls_PathWalker);
	(*env)->DeleteGlobalRef(env, cls_Pixmap);
	(*env)->DeleteGlobalRef(env, cls_Point);
	(*env)->DeleteGlobalRef(env, cls_Quad);
	(*env)->DeleteGlobalRef(env, cls_Rect);
	(*env)->DeleteGlobalRef(env, cls_RuntimeException);
	(*env)->DeleteGlobalRef(env, cls_SeekableInputStream);
	(*env)->DeleteGlobalRef(env, cls_SeekableOutputStream);
	(*env)->DeleteGlobalRef(env, cls_SeekableStream);
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
	(*env)->DeleteGlobalRef(env, cls_TextWidgetCharLayout);
	(*env)->DeleteGlobalRef(env, cls_TextWidgetLayout);
	(*env)->DeleteGlobalRef(env, cls_TextWidgetLineLayout);
	(*env)->DeleteGlobalRef(env, cls_TreeArchive);
	(*env)->DeleteGlobalRef(env, cls_TryLaterException);
	(*env)->DeleteGlobalRef(env, cls_UnsupportedOperationException);
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv *env;
	jint ret;

	ret = (*vm)->GetEnv(vm, (void **)&env, MY_JNI_VERSION);
	if (ret != JNI_OK)
	{
		LOGE("cannot get JNI interface during load (error %d)", ret);
		return -1;
	}

	return MY_JNI_VERSION;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
	JNIEnv *env;
	jint ret;

	ret = (*vm)->GetEnv(vm, (void **)&env, MY_JNI_VERSION);
	if (ret != JNI_OK)
	{
		/* If this fails, we're really in trouble! */
		LOGE("cannot get JNI interface during unload (error %d)", ret);
		return;
	}

	fz_drop_context(base_context);
	base_context = NULL;
	lose_fids(env);
}

#ifdef HAVE_ANDROID
#include "jni/android/androidfonts.c"
#endif

#include "jni/wrap.c"

#include "jni/context.c"
#include "jni/device.c"
#include "jni/nativedevice.c"

#include "jni/archive.c"
#include "jni/buffer.c"
#include "jni/colorspace.c"
#include "jni/cookie.c"
#include "jni/defaultcolorspaces.c"
#include "jni/displaylist.c"
#include "jni/displaylistdevice.c"
#include "jni/document.c"
#include "jni/documentwriter.c"
#include "jni/dom.c"
#include "jni/drawdevice.c"
#include "jni/fitzinputstream.c"
#include "jni/font.c"
#include "jni/image.c"
#include "jni/link.c"
#include "jni/multiarchive.c"
#include "jni/outlineiterator.c"
#include "jni/page.c"
#include "jni/path.c"
#include "jni/pdfannotation.c"
#include "jni/pdfdocument.c"
#include "jni/pdfgraftmap.c"
#include "jni/pdfobject.c"
#include "jni/pdfpage.c"
#include "jni/pdfwidget.c"
#include "jni/pixmap.c"
#include "jni/pkcs7signer.c"
#include "jni/pkcs7verifier.c"
#include "jni/rect.c"
#include "jni/shade.c"
#include "jni/story.c"
#include "jni/strokestate.c"
#include "jni/structuredtext.c"
#include "jni/text.c"
#include "jni/treearchive.c"

#ifdef HAVE_ANDROID
#include "jni/android/androiddrawdevice.c"
#include "jni/android/androidimage.c"
#endif
