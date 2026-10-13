/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BarcodeFormat.h"
#include "ByteArray.h"
#include "Content.h"
#include "ReaderOptions.h"
#include "Error.h"
#include "ImageView.h"
#include "Quadrilateral.h"
#include "StructuredAppend.h"

#ifdef ZXING_EXPERIMENTAL_API
#include <memory>
namespace ZXing {
class BitMatrix;
}

extern "C" struct zint_symbol;
struct zint_symbol_deleter
{
	void operator()(zint_symbol* p) const noexcept;
};
using unique_zint_symbol = std::unique_ptr<zint_symbol, zint_symbol_deleter>;
#endif

#include <string>
#include <vector>

namespace ZXing {

class DecoderResult;
class DetectorResult;
class WriterOptions;
class Result; // TODO: 3.0 replace deprected symbol name

using Position = QuadrilateralI;
using Barcode = Result;
using Barcodes = std::vector<Barcode>;
using Results = std::vector<Result>;

/**
 * @brief The Barcode class encapsulates the result of decoding a barcode within an image.
 */
class Result
{
	void setIsInverted(bool v) { _isInverted = v; }
	Result& setReaderOptions(const ReaderOptions& opts);

	friend Barcode MergeStructuredAppendSequence(const Barcodes&);
	friend Barcodes ReadBarcodes(const ImageView&, const ReaderOptions&);
	friend Image WriteBarcodeToImage(const Barcode&, const WriterOptions&);
	friend void IncrementLineCount(Barcode&);

public:
	Result() = default;

	// linear symbology convenience constructor
	Result(const std::string& text, int y, int xStart, int xStop, BarcodeFormat format, SymbologyIdentifier si, Error error = {},
		   bool readerInit = false);

	Result(DecoderResult&& decodeResult, DetectorResult&& detectorResult, BarcodeFormat format);

	[[deprecated]] Result(DecoderResult&& decodeResult, Position&& position, BarcodeFormat format);

	bool isValid() const;

	const Error& error() const { return _error; }

	BarcodeFormat format() const { return _format; }

	/**
	 * @brief bytes is the raw / standard content without any modifications like character set conversions
	 */
	const ByteArray& bytes() const;

	/**
	 * @brief bytesECI is the raw / standard content following the ECI protocol
	 */
	ByteArray bytesECI() const;

	/**
	 * @brief text returns the bytes() content rendered to unicode/utf8 text accoring to specified TextMode
	 */
	std::string text(TextMode mode) const;

	/**
	 * @brief text returns the bytes() content rendered to unicode/utf8 text accoring to the TextMode set in the ReaderOptions
	 */
	std::string text() const;

	/**
	 * @brief ecLevel returns the error correction level of the symbol (empty string if not applicable)
	 */
	std::string ecLevel() const;

	/**
	 * @brief contentType gives a hint to the type of content found (Text/Binary/GS1/etc.)
	 */
	ContentType contentType() const;

	/**
	 * @brief hasECI specifies wheter or not an ECI tag was found
	 */
	bool hasECI() const;

	const Position& position() const { return _position; }
	void setPosition(Position pos) { _position = pos; }

	/**
	 * @brief orientation of barcode in degree, see also Position::orientation()
	 */
	int orientation() const;

	/**
	 * @brief isMirrored is the symbol mirrored (currently only supported by QRCode and DataMatrix)
	 */
	bool isMirrored() const { return _isMirrored; }

	/**
	 * @brief isInverted is the symbol inverted / has reveresed reflectance (see ReaderOptions::tryInvert)
	 */
	bool isInverted() const { return _isInverted; }

	/**
	 * @brief symbologyIdentifier Symbology identifier "]cm" where "c" is symbology code character, "m" the modifier.
	 */
	std::string symbologyIdentifier() const;

	/**
	 * @brief sequenceSize number of symbols in a structured append sequence.
	 *
	 * If this is not part of a structured append sequence, the returned value is -1.
	 * If it is a structured append symbol but the total number of symbols is unknown, the
	 * returned value is 0 (see PDF417 if optional "Segment Count" not given).
	 */
	int sequenceSize() const;

	/**
	 * @brief sequenceIndex the 0-based index of this symbol in a structured append sequence.
	 */
	int sequenceIndex() const;

	/**
	 * @brief sequenceId id to check if a set of symbols belongs to the same structured append sequence.
	 *
	 * If the symbology does not support this feature, the returned value is empty (see MaxiCode).
	 * For QR Code, this is the parity integer converted to a string.
	 * For PDF417 and DataMatrix, this is the "fileId".
	 */
	std::string sequenceId() const;

	bool isLastInSequence() const { return sequenceSize() == sequenceIndex() + 1; }
	bool isPartOfSequence() const { return sequenceSize() > -1 && sequenceIndex() > -1; }

	/**
	 * @brief readerInit Set if Reader Initialisation/Programming symbol.
	 */
	bool readerInit() const { return _readerInit; }

	/**
	 * @brief lineCount How many lines have been detected with this code (applies only to linear symbologies)
	 */
	int lineCount() const { return _lineCount; }

	/**
	 * @brief version QRCode / DataMatrix / Aztec version or size.
	 */
	std::string version() const;

#ifdef ZXING_EXPERIMENTAL_API
	void symbol(BitMatrix&& bits);
	ImageView symbol() const;
	void zint(unique_zint_symbol&& z);
	zint_symbol* zint() const { return _zint.get(); }
#endif

	bool operator==(const Result& o) const;

private:
	Content _content;
	Error _error;
	Position _position;
	ReaderOptions _readerOpts; // TODO: 3.0 switch order to prevent 4 padding bytes
	StructuredAppendInfo _sai;
	BarcodeFormat _format = BarcodeFormat::None;
	char _ecLevel[4] = {};
	char _version[4] = {};
	int _lineCount = 0;
	bool _isMirrored = false;
	bool _isInverted = false;
	bool _readerInit = false;
#ifdef ZXING_EXPERIMENTAL_API
	std::shared_ptr<BitMatrix> _symbol;
	std::shared_ptr<zint_symbol> _zint;
#endif
};

/**
 * @brief Merge a list of Barcodes from one Structured Append sequence to a single barcode
 */
Barcode MergeStructuredAppendSequence(const Barcodes& results);

/**
 * @brief Automatically merge all Structured Append sequences found in the given list of barcodes
 */
Barcodes MergeStructuredAppendSequences(const Barcodes& barcodes);

} // ZXing
