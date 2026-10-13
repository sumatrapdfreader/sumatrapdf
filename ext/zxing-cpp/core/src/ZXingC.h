/*
* Copyright 2023 siiky
* Copyright 2023 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#ifndef _ZXING_C_H
#define _ZXING_C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus

#include "ZXingCpp.h"

typedef ZXing::Barcode ZXing_Barcode;
typedef ZXing::Barcodes ZXing_Barcodes;
typedef ZXing::ImageView ZXing_ImageView;
typedef ZXing::Image ZXing_Image;
typedef ZXing::ReaderOptions ZXing_ReaderOptions;

#ifdef ZXING_EXPERIMENTAL_API
typedef ZXing::CreatorOptions ZXing_CreatorOptions;
typedef ZXing::WriterOptions ZXing_WriterOptions;
#endif

extern "C"
{
#else

typedef struct ZXing_Barcode ZXing_Barcode;
typedef struct ZXing_Barcodes ZXing_Barcodes;
typedef struct ZXing_ImageView ZXing_ImageView;
typedef struct ZXing_Image ZXing_Image;
typedef struct ZXing_ReaderOptions ZXing_ReaderOptions;
typedef struct ZXing_CreatorOptions ZXing_CreatorOptions;
typedef struct ZXing_WriterOptions ZXing_WriterOptions;

#endif

/*
 * ZXing/ImageView.h
 */

typedef enum {
	ZXing_ImageFormat_None = 0,
	ZXing_ImageFormat_Lum = 0x01000000,
	ZXing_ImageFormat_LumA = 0x02000000,
	ZXing_ImageFormat_RGB = 0x03000102,
	ZXing_ImageFormat_BGR = 0x03020100,
	ZXing_ImageFormat_RGBA = 0x04000102,
	ZXing_ImageFormat_ARGB = 0x04010203,
	ZXing_ImageFormat_BGRA = 0x04020100,
	ZXing_ImageFormat_ABGR = 0x04030201,
} ZXing_ImageFormat;

ZXing_ImageView* ZXing_ImageView_new(const uint8_t* data, int width, int height, ZXing_ImageFormat format, int rowStride,
									 int pixStride);
ZXing_ImageView* ZXing_ImageView_new_checked(const uint8_t* data, int size, int width, int height, ZXing_ImageFormat format,
											 int rowStride, int pixStride);
void ZXing_ImageView_delete(ZXing_ImageView* iv);

void ZXing_ImageView_crop(ZXing_ImageView* iv, int left, int top, int width, int height);
void ZXing_ImageView_rotate(ZXing_ImageView* iv, int degree);

void ZXing_Image_delete(ZXing_Image* img);

const uint8_t* ZXing_Image_data(const ZXing_Image* img);
int ZXing_Image_width(const ZXing_Image* img);
int ZXing_Image_height(const ZXing_Image* img);
ZXing_ImageFormat ZXing_Image_format(const ZXing_Image* img);

/*
 * ZXing/BarcodeFormat.h
 */

typedef enum
{
	ZXing_BarcodeFormat_None = 0,
	ZXing_BarcodeFormat_Aztec = (1 << 0),
	ZXing_BarcodeFormat_Codabar = (1 << 1),
	ZXing_BarcodeFormat_Code39 = (1 << 2),
	ZXing_BarcodeFormat_Code93 = (1 << 3),
	ZXing_BarcodeFormat_Code128 = (1 << 4),
	ZXing_BarcodeFormat_DataBar = (1 << 5),
	ZXing_BarcodeFormat_DataBarExpanded = (1 << 6),
	ZXing_BarcodeFormat_DataMatrix = (1 << 7),
	ZXing_BarcodeFormat_EAN8 = (1 << 8),
	ZXing_BarcodeFormat_EAN13 = (1 << 9),
	ZXing_BarcodeFormat_ITF = (1 << 10),
	ZXing_BarcodeFormat_MaxiCode = (1 << 11),
	ZXing_BarcodeFormat_PDF417 = (1 << 12),
	ZXing_BarcodeFormat_QRCode = (1 << 13),
	ZXing_BarcodeFormat_UPCA = (1 << 14),
	ZXing_BarcodeFormat_UPCE = (1 << 15),
	ZXing_BarcodeFormat_MicroQRCode = (1 << 16),
	ZXing_BarcodeFormat_RMQRCode = (1 << 17),
	ZXing_BarcodeFormat_DXFilmEdge = (1 << 18),
	ZXing_BarcodeFormat_DataBarLimited = (1 << 19),

	ZXing_BarcodeFormat_LinearCodes = ZXing_BarcodeFormat_Codabar | ZXing_BarcodeFormat_Code39 | ZXing_BarcodeFormat_Code93
									  | ZXing_BarcodeFormat_Code128 | ZXing_BarcodeFormat_EAN8 | ZXing_BarcodeFormat_EAN13
									  | ZXing_BarcodeFormat_ITF | ZXing_BarcodeFormat_DataBar | ZXing_BarcodeFormat_DataBarExpanded
									  | ZXing_BarcodeFormat_DataBarLimited | ZXing_BarcodeFormat_DXFilmEdge | ZXing_BarcodeFormat_UPCA
									  | ZXing_BarcodeFormat_UPCE,
	ZXing_BarcodeFormat_MatrixCodes = ZXing_BarcodeFormat_Aztec | ZXing_BarcodeFormat_DataMatrix | ZXing_BarcodeFormat_MaxiCode
									  | ZXing_BarcodeFormat_PDF417 | ZXing_BarcodeFormat_QRCode | ZXing_BarcodeFormat_MicroQRCode
									  | ZXing_BarcodeFormat_RMQRCode,
	ZXing_BarcodeFormat_Any = ZXing_BarcodeFormat_LinearCodes | ZXing_BarcodeFormat_MatrixCodes,

	ZXing_BarcodeFormat_Invalid = 0xFFFFFFFFu /* return value when BarcodeFormatsFromString() throws */
} ZXing_BarcodeFormat;

typedef ZXing_BarcodeFormat ZXing_BarcodeFormats;

ZXing_BarcodeFormats ZXing_BarcodeFormatsFromString(const char* str);
ZXing_BarcodeFormat ZXing_BarcodeFormatFromString(const char* str);
char* ZXing_BarcodeFormatToString(ZXing_BarcodeFormat format);

/*
 * ZXing/ZXingCpp.h
 */

#ifdef ZXING_EXPERIMENTAL_API

typedef enum {
	ZXing_Operation_Create,
	ZXing_Operation_Read,
	ZXing_Operation_CreateAndRead,
	ZXing_Operation_CreateOrRead,
} ZXing_Operation;

ZXing_BarcodeFormats ZXing_SupportedBarcodeFormats(ZXing_Operation op);

#endif

/*
 * ZXing/Barcode.h
 */

typedef enum
{
	ZXing_ContentType_Text,
	ZXing_ContentType_Binary,
	ZXing_ContentType_Mixed,
	ZXing_ContentType_GS1,
	ZXing_ContentType_ISO15434,
	ZXing_ContentType_UnknownECI
} ZXing_ContentType;

typedef enum
{
	ZXing_ErrorType_None,
	ZXing_ErrorType_Format,
	ZXing_ErrorType_Checksum,
	ZXing_ErrorType_Unsupported
} ZXing_ErrorType;

char* ZXing_ContentTypeToString(ZXing_ContentType type);

typedef struct ZXing_PointI
{
	int x, y;
} ZXing_PointI;

typedef struct ZXing_Position
{
	ZXing_PointI topLeft, topRight, bottomRight, bottomLeft;
} ZXing_Position;

char* ZXing_PositionToString(ZXing_Position position);

bool ZXing_Barcode_isValid(const ZXing_Barcode* barcode);
ZXing_ErrorType ZXing_Barcode_errorType(const ZXing_Barcode* barcode);
char* ZXing_Barcode_errorMsg(const ZXing_Barcode* barcode);
ZXing_BarcodeFormat ZXing_Barcode_format(const ZXing_Barcode* barcode);
ZXing_ContentType ZXing_Barcode_contentType(const ZXing_Barcode* barcode);
uint8_t* ZXing_Barcode_bytes(const ZXing_Barcode* barcode, int* len);
uint8_t* ZXing_Barcode_bytesECI(const ZXing_Barcode* barcode, int* len);
char* ZXing_Barcode_text(const ZXing_Barcode* barcode);
char* ZXing_Barcode_ecLevel(const ZXing_Barcode* barcode);
char* ZXing_Barcode_symbologyIdentifier(const ZXing_Barcode* barcode);
ZXing_Position ZXing_Barcode_position(const ZXing_Barcode* barcode);
int ZXing_Barcode_orientation(const ZXing_Barcode* barcode);
bool ZXing_Barcode_hasECI(const ZXing_Barcode* barcode);
bool ZXing_Barcode_isInverted(const ZXing_Barcode* barcode);
bool ZXing_Barcode_isMirrored(const ZXing_Barcode* barcode);
int ZXing_Barcode_lineCount(const ZXing_Barcode* barcode);

void ZXing_Barcode_delete(ZXing_Barcode* barcode);
void ZXing_Barcodes_delete(ZXing_Barcodes* barcodes);

int ZXing_Barcodes_size(const ZXing_Barcodes* barcodes);
const ZXing_Barcode* ZXing_Barcodes_at(const ZXing_Barcodes* barcodes, int i);
ZXing_Barcode* ZXing_Barcodes_move(ZXing_Barcodes* barcodes, int i);

/*
 * ZXing/ReaderOptions.h
 */

typedef enum
{
	ZXing_Binarizer_LocalAverage,
	ZXing_Binarizer_GlobalHistogram,
	ZXing_Binarizer_FixedThreshold,
	ZXing_Binarizer_BoolCast,
} ZXing_Binarizer;

typedef enum
{
	ZXing_EanAddOnSymbol_Ignore,
	ZXing_EanAddOnSymbol_Read,
	ZXing_EanAddOnSymbol_Require,
} ZXing_EanAddOnSymbol;

typedef enum
{
	ZXing_TextMode_Plain,
	ZXing_TextMode_ECI,
	ZXing_TextMode_HRI,
	ZXing_TextMode_Hex,
	ZXing_TextMode_Escaped,
} ZXing_TextMode;

ZXing_ReaderOptions* ZXing_ReaderOptions_new();
void ZXing_ReaderOptions_delete(ZXing_ReaderOptions* opts);

void ZXing_ReaderOptions_setTryHarder(ZXing_ReaderOptions* opts, bool tryHarder);
void ZXing_ReaderOptions_setTryRotate(ZXing_ReaderOptions* opts, bool tryRotate);
void ZXing_ReaderOptions_setTryInvert(ZXing_ReaderOptions* opts, bool tryInvert);
void ZXing_ReaderOptions_setTryDownscale(ZXing_ReaderOptions* opts, bool tryDownscale);
void ZXing_ReaderOptions_setIsPure(ZXing_ReaderOptions* opts, bool isPure);
void ZXing_ReaderOptions_setReturnErrors(ZXing_ReaderOptions* opts, bool returnErrors);
void ZXing_ReaderOptions_setFormats(ZXing_ReaderOptions* opts, ZXing_BarcodeFormats formats);
void ZXing_ReaderOptions_setBinarizer(ZXing_ReaderOptions* opts, ZXing_Binarizer binarizer);
void ZXing_ReaderOptions_setEanAddOnSymbol(ZXing_ReaderOptions* opts, ZXing_EanAddOnSymbol eanAddOnSymbol);
void ZXing_ReaderOptions_setTextMode(ZXing_ReaderOptions* opts, ZXing_TextMode textMode);
void ZXing_ReaderOptions_setMinLineCount(ZXing_ReaderOptions* opts, int n);
void ZXing_ReaderOptions_setMaxNumberOfSymbols(ZXing_ReaderOptions* opts, int n);

bool ZXing_ReaderOptions_getTryHarder(const ZXing_ReaderOptions* opts);
bool ZXing_ReaderOptions_getTryRotate(const ZXing_ReaderOptions* opts);
bool ZXing_ReaderOptions_getTryInvert(const ZXing_ReaderOptions* opts);
bool ZXing_ReaderOptions_getTryDownscale(const ZXing_ReaderOptions* opts);
bool ZXing_ReaderOptions_getIsPure(const ZXing_ReaderOptions* opts);
bool ZXing_ReaderOptions_getReturnErrors(const ZXing_ReaderOptions* opts);
ZXing_BarcodeFormats ZXing_ReaderOptions_getFormats(const ZXing_ReaderOptions* opts);
ZXing_Binarizer ZXing_ReaderOptions_getBinarizer(const ZXing_ReaderOptions* opts);
ZXing_EanAddOnSymbol ZXing_ReaderOptions_getEanAddOnSymbol(const ZXing_ReaderOptions* opts);
ZXing_TextMode ZXing_ReaderOptions_getTextMode(const ZXing_ReaderOptions* opts);
int ZXing_ReaderOptions_getMinLineCount(const ZXing_ReaderOptions* opts);
int ZXing_ReaderOptions_getMaxNumberOfSymbols(const ZXing_ReaderOptions* opts);

/*
 * ZXing/ReadBarcode.h
 */

/** Note: opts is optional, i.e. it can be NULL, which will imply default settings. */
ZXing_Barcodes* ZXing_ReadBarcodes(const ZXing_ImageView* iv, const ZXing_ReaderOptions* opts);

#ifdef ZXING_EXPERIMENTAL_API

/*
 * ZXing/WriteBarcode.h
 */

ZXing_CreatorOptions* ZXing_CreatorOptions_new(ZXing_BarcodeFormat format);
void ZXing_CreatorOptions_delete(ZXing_CreatorOptions* opts);

void ZXing_CreatorOptions_setFormat(ZXing_CreatorOptions* opts, ZXing_BarcodeFormat format);
ZXing_BarcodeFormat ZXing_CreatorOptions_getFormat(const ZXing_CreatorOptions* opts);

void ZXing_CreatorOptions_setReaderInit(ZXing_CreatorOptions* opts, bool readerInit);
bool ZXing_CreatorOptions_getReaderInit(const ZXing_CreatorOptions* opts);

void ZXing_CreatorOptions_setForceSquareDataMatrix(ZXing_CreatorOptions* opts, bool forceSquareDataMatrix);
bool ZXing_CreatorOptions_getForceSquareDataMatrix(const ZXing_CreatorOptions* opts);

void ZXing_CreatorOptions_setEcLevel(ZXing_CreatorOptions* opts, const char* ecLevel);
char* ZXing_CreatorOptions_getEcLevel(const ZXing_CreatorOptions* opts);


ZXing_WriterOptions* ZXing_WriterOptions_new();
void ZXing_WriterOptions_delete(ZXing_WriterOptions* opts);

void ZXing_WriterOptions_setScale(ZXing_WriterOptions* opts, int scale);
int ZXing_WriterOptions_getScale(const ZXing_WriterOptions* opts);

void ZXing_WriterOptions_setSizeHint(ZXing_WriterOptions* opts, int sizeHint);
int ZXing_WriterOptions_getSizeHint(const ZXing_WriterOptions* opts);

void ZXing_WriterOptions_setRotate(ZXing_WriterOptions* opts, int rotate);
int ZXing_WriterOptions_getRotate(const ZXing_WriterOptions* opts);

void ZXing_WriterOptions_setWithHRT(ZXing_WriterOptions* opts, bool withHRT);
bool ZXing_WriterOptions_getWithHRT(const ZXing_WriterOptions* opts);

void ZXing_WriterOptions_setWithQuietZones(ZXing_WriterOptions* opts, bool withQuietZones);
bool ZXing_WriterOptions_getWithQuietZones(const ZXing_WriterOptions* opts);


ZXing_Barcode* ZXing_CreateBarcodeFromText(const char* data, int size, const ZXing_CreatorOptions* opts);
ZXing_Barcode* ZXing_CreateBarcodeFromBytes(const void* data, int size, const ZXing_CreatorOptions* opts);

/** Note: opts is optional, i.e. it can be NULL, which will imply default settings. */
char* ZXing_WriteBarcodeToSVG(const ZXing_Barcode* barcode, const ZXing_WriterOptions* opts);
ZXing_Image* ZXing_WriteBarcodeToImage(const ZXing_Barcode* barcode, const ZXing_WriterOptions* opts);

#endif /* ZXING_EXPERIMENTAL_API */

/* ZXing_LastErrorMsg() returns NULL in case there is no last error and a copy of the string otherwise. */
char* ZXing_LastErrorMsg();

const char* ZXing_Version();

void ZXing_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* _ZXING_C_H */
