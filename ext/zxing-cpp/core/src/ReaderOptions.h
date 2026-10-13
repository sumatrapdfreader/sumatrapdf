/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BarcodeFormat.h"
#include "CharacterSet.h"

#include <string_view>
#include <utility>

namespace ZXing {

/**
 * @brief The Binarizer enum
 *
 * Specify which algorithm to use for the grayscale to binary transformation.
 * The difference is how to get to a threshold value T which results in a bit
 * value R = L <= T.
 */
enum class Binarizer : unsigned char // needs to be unsigned for the bitfield below to work, uint8_t fails as well
{
	LocalAverage,    ///< T = average of neighboring pixels for matrix and GlobalHistogram for linear (HybridBinarizer)
	GlobalHistogram, ///< T = valley between the 2 largest peaks in the histogram (per line in linear case)
	FixedThreshold,  ///< T = 127
	BoolCast,        ///< T = 0, fastest possible
};

enum class EanAddOnSymbol : unsigned char // see above
{
	Ignore,  ///< Ignore any Add-On symbol during read/scan
	Read,    ///< Read EAN-2/EAN-5 Add-On symbol if found
	Require, ///< Require EAN-2/EAN-5 Add-On symbol to be present
};

enum class TextMode : unsigned char // see above
{
	Plain,   ///< bytes() transcoded to unicode based on ECI info or guessed charset (the default mode prior to 2.0)
	ECI,     ///< standard content following the ECI protocol with every character set ECI segment transcoded to unicode
	HRI,     ///< Human Readable Interpretation (dependent on the ContentType)
	Hex,     ///< bytes() transcoded to ASCII string of HEX values
	Escaped, ///< Use the EscapeNonGraphical() function (e.g. ASCII 29 will be transcoded to "<GS>")
};

class ReaderOptions
{
	bool _tryHarder                : 1;
	bool _tryRotate                : 1;
	bool _tryInvert                : 1;
	bool _tryDownscale             : 1;
	bool _isPure                   : 1;
	bool _tryCode39ExtendedMode    : 1;
	bool _validateCode39CheckSum   : 1;
	bool _validateITFCheckSum      : 1;
	bool _returnCodabarStartEnd    : 1;
	bool _returnErrors             : 1;
	uint8_t _downscaleFactor       : 3;
	EanAddOnSymbol _eanAddOnSymbol : 2;
	Binarizer _binarizer           : 2;
	TextMode _textMode             : 3;
	CharacterSet _characterSet     : 6;
#ifdef ZXING_EXPERIMENTAL_API
	bool _tryDenoise               : 1;
#endif

	uint8_t _minLineCount        = 2;
	uint8_t _maxNumberOfSymbols  = 0xff;
	uint16_t _downscaleThreshold = 500;
	BarcodeFormats _formats      = BarcodeFormat::None;

public:
	// bitfields don't get default initialized to 0 before c++20
	ReaderOptions()
		: _tryHarder(1),
		  _tryRotate(1),
		  _tryInvert(1),
		  _tryDownscale(1),
		  _isPure(0),
		  _tryCode39ExtendedMode(1),
		  _validateCode39CheckSum(0),
		  _validateITFCheckSum(0),
		  _returnCodabarStartEnd(1),
		  _returnErrors(0),
		  _downscaleFactor(3),
		  _eanAddOnSymbol(EanAddOnSymbol::Ignore),
		  _binarizer(Binarizer::LocalAverage),
		  _textMode(TextMode::HRI),
		  _characterSet(CharacterSet::Unknown)
#ifdef ZXING_EXPERIMENTAL_API
		  ,
		  _tryDenoise(0)
#endif
	{}

#define ZX_PROPERTY(TYPE, GETTER, SETTER, ...) \
	TYPE GETTER() const noexcept { return _##GETTER; } \
	__VA_ARGS__ ReaderOptions& SETTER(TYPE v)& { return (void)(_##GETTER = std::move(v)), *this; } \
	__VA_ARGS__ ReaderOptions&& SETTER(TYPE v)&& { return (void)(_##GETTER = std::move(v)), std::move(*this); }

	/// Specify a set of BarcodeFormats that should be searched for, the default is all supported formats.
	ZX_PROPERTY(BarcodeFormats, formats, setFormats)

	/// Spend more time to try to find a barcode; optimize for accuracy, not speed.
	ZX_PROPERTY(bool, tryHarder, setTryHarder)

	/// Also try detecting code in 90, 180 and 270 degree rotated images.
	ZX_PROPERTY(bool, tryRotate, setTryRotate)

	/// Also try detecting inverted ("reversed reflectance") codes if the format allows for those.
	ZX_PROPERTY(bool, tryInvert, setTryInvert)

	/// Also try detecting code in downscaled images (depending on image size).
	ZX_PROPERTY(bool, tryDownscale, setTryDownscale)

#ifdef ZXING_EXPERIMENTAL_API
	/// Also try detecting code after denoising (currently morphological closing filter for 2D symbologies only).
	ZX_PROPERTY(bool, tryDenoise, setTryDenoise)
#endif

	/// Binarizer to use internally when using the ReadBarcode function
	ZX_PROPERTY(Binarizer, binarizer, setBinarizer)

	/// Set to true if the input contains nothing but a single perfectly aligned barcode (generated image)
	ZX_PROPERTY(bool, isPure, setIsPure)

	/// Image size ( min(width, height) ) threshold at which to start downscaled scanning
	// WARNING: this API is experimental and may change/disappear
	ZX_PROPERTY(uint16_t, downscaleThreshold, setDownscaleThreshold)

	/// Scale factor used during downscaling, meaningful values are 2, 3 and 4
	// WARNING: this API is experimental and may change/disappear
	ZX_PROPERTY(uint8_t, downscaleFactor, setDownscaleFactor)

	/// The number of scan lines in a linear barcode that have to be equal to accept the result, default is 2
	ZX_PROPERTY(uint8_t, minLineCount, setMinLineCount)

	/// The maximum number of symbols (barcodes) to detect / look for in the image with ReadBarcodes
	ZX_PROPERTY(uint8_t, maxNumberOfSymbols, setMaxNumberOfSymbols)

	/// Enable the heuristic to detect and decode "full ASCII"/extended Code39 symbols
	ZX_PROPERTY(bool, tryCode39ExtendedMode, setTryCode39ExtendedMode)

	/// Deprecated / does nothing. The Code39 symbol has a valid checksum iff symbologyIdentifier()[2] is an odd digit
	ZX_PROPERTY(bool, validateCode39CheckSum, setValidateCode39CheckSum, [[deprecated]])

	/// Deprecated / does nothing. The ITF symbol has a valid checksum iff symbologyIdentifier()[2] == '1'.
	ZX_PROPERTY(bool, validateITFCheckSum, setValidateITFCheckSum, [[deprecated]])

	/// Deprecated / does nothing. Codabar start/stop characters are always returned.
	ZX_PROPERTY(bool, returnCodabarStartEnd, setReturnCodabarStartEnd, [[deprecated]])

	/// If true, return the barcodes with errors as well (e.g. checksum errors, see @Barcode::error())
	ZX_PROPERTY(bool, returnErrors, setReturnErrors)

	/// Specify whether to ignore, read or require EAN-2/5 add-on symbols while scanning EAN/UPC codes
	ZX_PROPERTY(EanAddOnSymbol, eanAddOnSymbol, setEanAddOnSymbol)

	/// Specifies the TextMode that controls the return of the Barcode::text() function
	ZX_PROPERTY(TextMode, textMode, setTextMode)

	/// Specifies fallback character set to use instead of auto-detecting it (when applicable)
	ZX_PROPERTY(CharacterSet, characterSet, setCharacterSet)
	ReaderOptions& setCharacterSet(std::string_view v)& { return (void)(_characterSet = CharacterSetFromString(v)), *this; }
	ReaderOptions&& setCharacterSet(std::string_view v) && { return (void)(_characterSet = CharacterSetFromString(v)), std::move(*this); }

#undef ZX_PROPERTY

	bool hasFormat(BarcodeFormats f) const noexcept { return _formats.testFlags(f) || _formats.empty(); }
};

#ifndef HIDE_DECODE_HINTS_ALIAS
using DecodeHints [[deprecated]] = ReaderOptions;
#endif

} // ZXing
