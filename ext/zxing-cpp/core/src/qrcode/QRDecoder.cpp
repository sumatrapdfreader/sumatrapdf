/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRDecoder.h"

#include "BitMatrix.h"
#include "BitSource.h"
#include "CharacterSet.h"
#include "DecoderResult.h"
#include "GenericGF.h"
#include "QRBitMatrixParser.h"
#include "QRCodecMode.h"
#include "QRDataBlock.h"
#include "QRFormatInformation.h"
#include "QRVersion.h"
#include "ReedSolomonDecoder.h"
#include "StructuredAppend.h"
#include "ZXAlgorithms.h"
#include "ZXTestSupport.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ZXing::QRCode {

/**
* <p>Given data and error-correction codewords received, possibly corrupted by errors, attempts to
* correct the errors in-place using Reed-Solomon error correction.</p>
*
* @param codewordBytes data and error correction codewords
* @param numDataCodewords number of codewords that are data bytes
* @return false if error correction fails
*/
static bool CorrectErrors(ByteArray& codewordBytes, int numDataCodewords)
{
	// First read into an array of ints
	std::vector<int> codewordsInts(codewordBytes.begin(), codewordBytes.end());

	int numECCodewords = Size(codewordBytes) - numDataCodewords;
	if (!ReedSolomonDecode(GenericGF::QRCodeField256(), codewordsInts, numECCodewords))
		return false;

	// Copy back into array of bytes -- only need to worry about the bytes that were data
	// We don't care about errors in the error-correction codewords
	std::copy_n(codewordsInts.begin(), numDataCodewords, codewordBytes.begin());
	return true;
}


/**
* See specification GBT 18284-2000
*/
static void DecodeHanziSegment(BitSource& bits, int count, Content& result)
{
	// Each character will require 2 bytes, decode as GB2312
	// There is no ECI value for GB2312, use GB18030 which is a superset
	result.switchEncoding(CharacterSet::GB18030);
	result.reserve(2 * count);

	while (count > 0) {
		// Each 13 bits encodes a 2-byte character
		int twoBytes = bits.readBits(13);
		int assembledTwoBytes = ((twoBytes / 0x060) << 8) | (twoBytes % 0x060);
		if (assembledTwoBytes < 0x00A00) {
			// In the 0xA1A1 to 0xAAFE range
			assembledTwoBytes += 0x0A1A1;
		} else {
			// In the 0xB0A1 to 0xFAFE range
			assembledTwoBytes += 0x0A6A1;
		}
		result += narrow_cast<uint8_t>((assembledTwoBytes >> 8) & 0xFF);
		result += narrow_cast<uint8_t>(assembledTwoBytes & 0xFF);
		count--;
	}
}

static void DecodeKanjiSegment(BitSource& bits, int count, Content& result)
{
	// Each character will require 2 bytes. Read the characters as 2-byte pairs
	// and decode as Shift_JIS afterwards
	result.switchEncoding(CharacterSet::Shift_JIS);
	result.reserve(2 * count);

	while (count > 0) {
		// Each 13 bits encodes a 2-byte character
		int twoBytes = bits.readBits(13);
		int assembledTwoBytes = ((twoBytes / 0x0C0) << 8) | (twoBytes % 0x0C0);
		if (assembledTwoBytes < 0x01F00) {
			// In the 0x8140 to 0x9FFC range
			assembledTwoBytes += 0x08140;
		} else {
			// In the 0xE040 to 0xEBBF range
			assembledTwoBytes += 0x0C140;
		}
		result += narrow_cast<uint8_t>(assembledTwoBytes >> 8);
		result += narrow_cast<uint8_t>(assembledTwoBytes);
		count--;
	}
}

static void DecodeByteSegment(BitSource& bits, int count, Content& result)
{
	result.switchEncoding(CharacterSet::Unknown);
	result.reserve(count);

	for (int i = 0; i < count; i++)
		result += narrow_cast<uint8_t>(bits.readBits(8));
}

static char ToAlphaNumericChar(int value)
{
	/**
	* See ISO 18004:2006, 6.4.4 Table 5
	*/
	static const char ALPHANUMERIC_CHARS[] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
		'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
		'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
		' ', '$', '%', '*', '+', '-', '.', '/', ':'
	};

	if (value < 0 || value >= Size(ALPHANUMERIC_CHARS))
		throw std::out_of_range("ToAlphaNumericChar: out of range");

	return ALPHANUMERIC_CHARS[value];
}

static void DecodeAlphanumericSegment(BitSource& bits, int count, Content& result)
{
	// Read two characters at a time
	std::string buffer;
	while (count > 1) {
		int nextTwoCharsBits = bits.readBits(11);
		buffer += ToAlphaNumericChar(nextTwoCharsBits / 45);
		buffer += ToAlphaNumericChar(nextTwoCharsBits % 45);
		count -= 2;
	}
	if (count == 1) {
		// special case: one character left
		buffer += ToAlphaNumericChar(bits.readBits(6));
	}
	// See section 6.4.8.1, 6.4.8.2
	if (result.symbology.aiFlag != AIFlag::None) {
		// We need to massage the result a bit if in an FNC1 mode:
		for (auto i = buffer.begin(); i != buffer.end(); i++) {
			if (*i == '%') {
				if (i + 1 != buffer.end() && *(i + 1) == '%') {
					// %% is rendered as %
					i = buffer.erase(i);
				} else {
					// In alpha mode, % should be converted to FNC1 separator 0x1D
					*i = static_cast<char>(0x1D);
				}
			}
		}
	}

	result.switchEncoding(CharacterSet::ISO8859_1);
	result += buffer;
}

static void DecodeNumericSegment(BitSource& bits, int count, Content& result)
{
	result.switchEncoding(CharacterSet::ISO8859_1);
	result.reserve(count);

	while (count) {
		int n = std::min(count, 3);
		int nDigits = bits.readBits(1 + 3 * n); // read 4, 7 or 10 bits into 1, 2 or 3 digits
		result.append(ZXing::ToString(nDigits, n));
		count -= n;
	}
}

static ECI ParseECIValue(BitSource& bits)
{
	int firstByte = bits.readBits(8);
	if ((firstByte & 0x80) == 0) {
		// just one byte
		return ECI(firstByte & 0x7F);
	}
	if ((firstByte & 0xC0) == 0x80) {
		// two bytes
		int secondByte = bits.readBits(8);
		return ECI(((firstByte & 0x3F) << 8) | secondByte);
	}
	if ((firstByte & 0xE0) == 0xC0) {
		// three bytes
		int secondThirdBytes = bits.readBits(16);
		return ECI(((firstByte & 0x1F) << 16) | secondThirdBytes);
	}
	throw FormatError("ParseECIValue: invalid value");
}

/**
 * QR codes encode mode indicators and terminator codes into a constant bit length of 4.
 * Micro QR codes have terminator codes that vary in bit length but are always longer than
 * the mode indicators.
 * M1 - 0 length mode code, 3 bits terminator code
 * M2 - 1 bit mode code, 5 bits terminator code
 * M3 - 2 bit mode code, 7 bits terminator code
 * M4 - 3 bit mode code, 9 bits terminator code
 * IsTerminator peaks into the bit stream to see if the current position is at the start of
 * a terminator code.  If true, then the decoding can finish. If false, then the decoding
 * can read off the next mode code.
 *
 * See ISO 18004:2015, 7.4.1 Table 2
 *
 * @param bits the stream of bits that might have a terminator code
 * @param version the QR or micro QR code version
 */
bool IsEndOfStream(const BitSource& bits, const Version& version)
{
	const int bitsRequired = TerminatorBitsLength(version);
	const int bitsAvailable = std::min(bits.available(), bitsRequired);
	return bitsAvailable == 0 || bits.peakBits(bitsAvailable) == 0;
}

/**
* <p>QR Codes can encode text as bits in one of several modes, and can use multiple modes
* in one QR Code. This method decodes the bits back into text.</p>
*
* <p>See ISO 18004:2006, 6.4.3 - 6.4.7</p>
*/
ZXING_EXPORT_TEST_ONLY
DecoderResult DecodeBitStream(ByteArray&& bytes, const Version& version, ErrorCorrectionLevel ecLevel)
{
	BitSource bits(bytes);
	Content result;
	Error error;
	result.symbology = {'Q', version.isModel1() ? '0' : '1', 1};
	StructuredAppendInfo structuredAppend;
	const int modeBitLength = CodecModeBitsLength(version);

	if (version.isModel1())
		bits.readBits(4); // Model 1 is leading with 4 0-bits -> drop them

	try
	{
		while(!IsEndOfStream(bits, version)) {
			CodecMode mode;
			if (modeBitLength == 0)
				mode = CodecMode::NUMERIC; // MicroQRCode version 1 is always NUMERIC and modeBitLength is 0
			else
				mode = CodecModeForBits(bits.readBits(modeBitLength), version.type());

			switch (mode) {
			case CodecMode::FNC1_FIRST_POSITION:
//				if (!result.empty()) // uncomment to enforce specification
//					throw FormatError("GS1 Indicator (FNC1 in first position) at illegal position");
				result.symbology.modifier = '3';
				result.symbology.aiFlag = AIFlag::GS1; // In Alphanumeric mode undouble doubled '%' and treat single '%' as <GS>
				break;
			case CodecMode::FNC1_SECOND_POSITION:
				if (!result.empty())
					throw FormatError("AIM Application Indicator (FNC1 in second position) at illegal position");
				result.symbology.modifier = '5'; // As above
				// ISO/IEC 18004:2015 7.4.8.3 AIM Application Indicator (FNC1 in second position), "00-99" or "A-Za-z"
				if (int appInd = bits.readBits(8); appInd < 100) // "00-09"
					result += ZXing::ToString(appInd, 2);
				else if ((appInd >= 165 && appInd <= 190) || (appInd >= 197 && appInd <= 222)) // "A-Za-z"
					result += narrow_cast<uint8_t>(appInd - 100);
				else
					throw FormatError("Invalid AIM Application Indicator");
				result.symbology.aiFlag = AIFlag::AIM; // see also above
				break;
			case CodecMode::STRUCTURED_APPEND:
				// sequence number and parity is added later to the result metadata
				// Read next 4 bits of index, 4 bits of symbol count, and 8 bits of parity data, then continue
				structuredAppend.index = bits.readBits(4);
				structuredAppend.count = bits.readBits(4) + 1;
				structuredAppend.id    = std::to_string(bits.readBits(8));
				break;
			case CodecMode::ECI:
				if (version.isModel1())
					throw FormatError("QRCode Model 1 does not support ECI");
				// Count doesn't apply to ECI
				result.switchEncoding(ParseECIValue(bits));
				break;
			case CodecMode::HANZI: {
				// First handle Hanzi mode which does not start with character count
				// chinese mode contains a sub set indicator right after mode indicator
				if (int subset = bits.readBits(4); subset != 1) // GB2312_SUBSET is the only supported one right now
					throw FormatError("Unsupported HANZI subset");
				int count = bits.readBits(CharacterCountBits(mode, version));
				DecodeHanziSegment(bits, count, result);
				break;
			}
			default: {
				// "Normal" QR code modes:
				// How many characters will follow, encoded in this mode?
				int count = bits.readBits(CharacterCountBits(mode, version));
				switch (mode) {
				case CodecMode::NUMERIC:      DecodeNumericSegment(bits, count, result); break;
				case CodecMode::ALPHANUMERIC: DecodeAlphanumericSegment(bits, count, result); break;
				case CodecMode::BYTE:         DecodeByteSegment(bits, count, result); break;
				case CodecMode::KANJI:        DecodeKanjiSegment(bits, count, result); break;
				default:                      throw FormatError("Invalid CodecMode");
				}
				break;
			}
			}
		}
	} catch (std::out_of_range&) { // see BitSource::readBits
		error = FormatError("Truncated bit stream");
	} catch (Error e) {
		error = std::move(e);
	}

	return DecoderResult(std::move(result))
		.setError(std::move(error))
		.setEcLevel(ToString(ecLevel))
		.setVersionNumber(version.versionNumber())
		.setStructuredAppend(structuredAppend);
}

DecoderResult Decode(const BitMatrix& bits)
{
	if (!Version::HasValidSize(bits))
		return FormatError("Invalid symbol size");

	auto formatInfo = ReadFormatInformation(bits);
	if (!formatInfo.isValid())
		return FormatError("Invalid format information");

	const Version* pversion = ReadVersion(bits, formatInfo.type());
	if (!pversion)
		return FormatError("Invalid version");

	const Version& version = *pversion;

	// Read codewords
	ByteArray codewords = ReadCodewords(bits, version, formatInfo);
	if (codewords.empty())
		return FormatError("Failed to read codewords");

	// Separate into data blocks
	std::vector<DataBlock> dataBlocks = DataBlock::GetDataBlocks(codewords, version, formatInfo.ecLevel);
	if (dataBlocks.empty())
		return FormatError("Failed to get data blocks");

	// Count total number of data bytes
	const auto op = [](auto totalBytes, const auto& dataBlock){ return totalBytes + dataBlock.numDataCodewords();};
	const auto totalBytes = Reduce(dataBlocks, int{}, op);
	ByteArray resultBytes(totalBytes);
	auto resultIterator = resultBytes.begin();

	// Error-correct and copy data blocks together into a stream of bytes
	Error error;
	for (auto& dataBlock : dataBlocks)
	{
		ByteArray& codewordBytes = dataBlock.codewords();
		int numDataCodewords = dataBlock.numDataCodewords();

		if (!CorrectErrors(codewordBytes, numDataCodewords))
			error = ChecksumError();

		resultIterator = std::copy_n(codewordBytes.begin(), numDataCodewords, resultIterator);
	}

	// Decode the contents of that stream of bytes
	auto ret = DecodeBitStream(std::move(resultBytes), version, formatInfo.ecLevel)
		.setDataMask(formatInfo.mask)
		.setIsMirrored(formatInfo.isMirrored);
	if (error)
		ret.setError(error);
	return ret;
}

} // namespace ZXing::QRCode
