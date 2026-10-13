/*
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef ZXING_EXPERIMENTAL_API

#include "Barcode.h"
#include "ImageView.h"

#include <memory>
#include <string_view>

extern "C" struct zint_symbol;

namespace ZXing {

class CreatorOptions
{
	struct Data;

	std::unique_ptr<Data> d;

	friend Barcode CreateBarcode(const void* data, int size, int mode, const CreatorOptions& options);

public:
	CreatorOptions(BarcodeFormat format);

	~CreatorOptions();
	CreatorOptions(CreatorOptions&&);
	CreatorOptions& operator=(CreatorOptions&&);

	zint_symbol* zint() const;

#define ZX_PROPERTY(TYPE, NAME) \
	TYPE NAME() const noexcept; \
	CreatorOptions& NAME(TYPE v)&; \
	CreatorOptions&& NAME(TYPE v)&&;

	ZX_PROPERTY(BarcodeFormat, format)
	ZX_PROPERTY(bool, readerInit)
	ZX_PROPERTY(bool, forceSquareDataMatrix)
	ZX_PROPERTY(std::string, ecLevel)

#undef ZX_PROPERTY
};

/**
 * Generate barcode from unicode text
 *
 * @param contents  UTF-8 string to encode into a barcode
 * @param options  CreatorOptions (including BarcodeFormat)
 * @return #Barcode  generated barcode
 */
Barcode CreateBarcodeFromText(std::string_view contents, const CreatorOptions& options);

/**
 * Generate barcode from raw binary data
 *
 * @param data  array of bytes to encode into a barcode
 * @param size  size of byte array
 * @param options  CreatorOptions (including BarcodeFormat)
 * @return #Barcode  generated barcode
 */
Barcode CreateBarcodeFromBytes(const void* data, int size, const CreatorOptions& options);

#if __cplusplus > 201703L
Barcode CreateBarcodeFromText(std::u8string_view contents, const CreatorOptions& options);

template <typename R>
requires std::ranges::contiguous_range<R> && std::ranges::sized_range<R> && (sizeof(std::ranges::range_value_t<R>) == 1)
Barcode CreateBarcodeFromBytes(const R& contents, const CreatorOptions& options)
{
	return CreateBarcodeFromBytes(std::ranges::data(contents), std::ranges::size(contents), options);
}
#else
template <typename R>
Barcode CreateBarcodeFromBytes(const R& contents, const CreatorOptions& options)
{
	return CreateBarcodeFromBytes(std::data(contents), std::size(contents), options);
}
#endif

// =================================================================================

class WriterOptions
{
	struct Data;

	std::unique_ptr<Data> d;

public:
	WriterOptions();
	~WriterOptions();
	WriterOptions(WriterOptions&&);
	WriterOptions& operator=(WriterOptions&&);

#define ZX_PROPERTY(TYPE, NAME) \
	TYPE NAME() const noexcept; \
	WriterOptions& NAME(TYPE v)&; \
	WriterOptions&& NAME(TYPE v)&&;

	ZX_PROPERTY(int, scale)
	ZX_PROPERTY(int, sizeHint)
	ZX_PROPERTY(int, rotate)
	ZX_PROPERTY(bool, withHRT)
	ZX_PROPERTY(bool, withQuietZones)

#undef ZX_PROPERTY
};


/**
 * Write barcode symbol to SVG
 *
 * @param barcode  Barcode to write
 * @param options  WriterOptions to parameterize rendering
 * @return std::string  SVG representation of barcode symbol
 */
std::string WriteBarcodeToSVG(const Barcode& barcode, const WriterOptions& options = {});

/**
 * Write barcode symbol to a utf8 string using graphical characters (e.g. '▀')
 *
 * @param barcode  Barcode to write
 * @param options  WriterOptions to parameterize rendering
 * @return std::string  Utf8 string representation of barcode symbol
 */
std::string WriteBarcodeToUtf8(const Barcode& barcode, const WriterOptions& options = {});

/**
 * Write barcode symbol to Image (Bitmap)
 *
 * @param barcode  Barcode to write
 * @param options  WriterOptions to parameterize rendering
 * @return Image  Bitmap reprentation of barcode symbol
 */
Image WriteBarcodeToImage(const Barcode& barcode, const WriterOptions& options = {});

} // ZXing

#endif // ZXING_EXPERIMENTAL_API
