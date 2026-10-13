/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "Barcode.h"

#include "DecoderResult.h"
#include "DetectorResult.h"
#include "ZXAlgorithms.h"

#ifdef ZXING_EXPERIMENTAL_API
#include "BitMatrix.h"

#ifdef ZXING_USE_ZINT
#include <zint.h>
void zint_symbol_deleter::operator()(zint_symbol* p) const noexcept
{
	ZBarcode_Delete(p);
}
#else
struct zint_symbol {};
void zint_symbol_deleter::operator()(zint_symbol*) const noexcept {}
#endif

#endif

#include <cmath>
#include <list>
#include <map>
#include <utility>

namespace ZXing {

Result::Result(const std::string& text, int y, int xStart, int xStop, BarcodeFormat format, SymbologyIdentifier si, Error error, bool readerInit)
	: _content({ByteArray(text)}, si),
	  _error(error),
	  _position(Line(y, xStart, xStop)),
	  _format(format),
	  _readerInit(readerInit)
{}

Result::Result(DecoderResult&& decodeResult, DetectorResult&& detectorResult, BarcodeFormat format)
	: _content(std::move(decodeResult).content()),
	  _error(std::move(decodeResult).error()),
	  _position(std::move(detectorResult).position()),
	  _sai(decodeResult.structuredAppend()),
	  _format(format),
	  _lineCount(decodeResult.lineCount()),
	  _isMirrored(decodeResult.isMirrored()),
	  _readerInit(decodeResult.readerInit())
#ifdef ZXING_EXPERIMENTAL_API
	  , _symbol(std::make_shared<BitMatrix>(std::move(detectorResult).bits()))
#endif
{
	if (decodeResult.versionNumber())
		snprintf(_version, 4, "%d", decodeResult.versionNumber());
	snprintf(_ecLevel, 4, "%s", decodeResult.ecLevel().data());

	// TODO: add type opaque and code specific 'extra data'? (see DecoderResult::extra())
}

Result::Result(DecoderResult&& decodeResult, Position&& position, BarcodeFormat format)
	: Result(std::move(decodeResult), {{}, std::move(position)}, format)
{}

bool Result::isValid() const
{
	return format() != BarcodeFormat::None && !_content.bytes.empty() && !error();
}

const ByteArray& Result::bytes() const
{
	return _content.bytes;
}

ByteArray Result::bytesECI() const
{
	return _content.bytesECI();
}

std::string Result::text(TextMode mode) const
{
	return _content.text(mode);
}

std::string Result::text() const
{
	return text(_readerOpts.textMode());
}

std::string Result::ecLevel() const
{
	return _ecLevel;
}

ContentType Result::contentType() const
{
	return _content.type();
}

bool Result::hasECI() const
{
	return _content.hasECI;
}

int Result::orientation() const
{
	constexpr auto std_numbers_pi_v = 3.14159265358979323846; // TODO: c++20 <numbers>
	return narrow_cast<int>(std::lround(_position.orientation() * 180 / std_numbers_pi_v));
}

std::string Result::symbologyIdentifier() const
{
	return _content.symbology.toString();
}

int Result::sequenceSize() const
{
	return _sai.count;
}

int Result::sequenceIndex() const
{
	return _sai.index;
}

std::string Result::sequenceId() const
{
	return _sai.id;
}

std::string Result::version() const
{
	return _version;
}

Result& Result::setReaderOptions(const ReaderOptions& opts)
{
	if (opts.characterSet() != CharacterSet::Unknown)
		_content.defaultCharset = opts.characterSet();
	_readerOpts = opts;
	return *this;
}

#ifdef ZXING_EXPERIMENTAL_API
void Result::symbol(BitMatrix&& bits)
{
	bits.flipAll();
	_symbol = std::make_shared<BitMatrix>(std::move(bits));
}

ImageView Result::symbol() const
{
	return {_symbol->row(0).begin(), _symbol->width(), _symbol->height(), ImageFormat::Lum};
}

void Result::zint(unique_zint_symbol&& z)
{
	_zint = std::shared_ptr(std::move(z));
}
#endif

bool Result::operator==(const Result& o) const
{
	// handle case where both are MatrixCodes first
	if (!BarcodeFormats(BarcodeFormat::LinearCodes).testFlags(format() | o.format())) {
		if (format() != o.format() || (bytes() != o.bytes() && isValid() && o.isValid()))
			return false;

		// check for equal position if both are valid with equal bytes or at least one is in error
		return IsInside(Center(o.position()), position());
	}

	if (format() != o.format() || bytes() != o.bytes() || error() != o.error())
		return false;

	if (orientation() != o.orientation())
		return false;

	if (lineCount() > 1 && o.lineCount() > 1)
		return HaveIntersectingBoundingBoxes(o.position(), position());

	// the following code is only meant for this or other lineCount == 1
	assert(lineCount() == 1 || o.lineCount() == 1);

	// sl == single line, ml = multi line
	const auto& sl = lineCount() == 1 ? *this : o;
	const auto& ml = lineCount() == 1 ? o : *this;

	// If one line is less than half the length of the other away from the
	// latter, we consider it to belong to the same symbol.
	// Additionally, both need to have roughly the same length (see #367).
	auto dTop = maxAbsComponent(ml.position().topLeft() - sl.position().topLeft());
	auto dBot = maxAbsComponent(ml.position().bottomLeft() - sl.position().topLeft());
	auto slLength = maxAbsComponent(sl.position().topLeft() - sl.position().bottomRight());
	bool isHorizontal = sl.position().topLeft().y == sl.position().bottomRight().y;
	// Measure the multi line length in the same direction as the single line one (not diagonaly)
	// to make sure overly tall symbols don't get segmented (see #769).
	auto mlLength = isHorizontal ? std::abs(ml.position().topLeft().x - ml.position().bottomRight().x)
								 : std::abs(ml.position().topLeft().y - ml.position().bottomRight().y);

	return std::min(dTop, dBot) < slLength / 2 && std::abs(slLength - mlLength) < slLength / 5;
}

Barcode MergeStructuredAppendSequence(const Barcodes& barcodes)
{
	if (barcodes.empty())
		return {};

	std::list<Barcode> allBarcodes(barcodes.begin(), barcodes.end());
	allBarcodes.sort([](const Barcode& r1, const Barcode& r2) { return r1.sequenceIndex() < r2.sequenceIndex(); });

	Barcode res = allBarcodes.front();
	for (auto i = std::next(allBarcodes.begin()); i != allBarcodes.end(); ++i)
		res._content.append(i->_content);

	res._position = {};
	res._sai.index = -1;

	if (allBarcodes.back().sequenceSize() != Size(allBarcodes) ||
		!std::all_of(allBarcodes.begin(), allBarcodes.end(),
					 [&](Barcode& it) { return it.sequenceId() == allBarcodes.front().sequenceId(); }))
		res._error = FormatError("sequenceIDs not matching during structured append sequence merging");

	return res;
}

Barcodes MergeStructuredAppendSequences(const Barcodes& barcodes)
{
	std::map<std::string, Barcodes> sas;
	for (auto& barcode : barcodes) {
		if (barcode.isPartOfSequence())
			sas[barcode.sequenceId()].push_back(barcode);
	}

	Barcodes res;
	for (auto& [id, seq] : sas) {
		auto barcode = MergeStructuredAppendSequence(seq);
		if (barcode.isValid())
			res.push_back(std::move(barcode));
	}

	return res;
}

} // namespace ZXing
