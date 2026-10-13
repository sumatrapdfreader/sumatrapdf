/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "DMDecoder.h"

#include "BitMatrix.h"
#include "BitSource.h"
#include "DMBitLayout.h"
#include "DMDataBlock.h"
#include "DMVersion.h"
#include "DecoderResult.h"
#include "GenericGF.h"
#include "ReedSolomonDecoder.h"
#include "ZXAlgorithms.h"
#include "ZXTestSupport.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ZXing::DataMatrix {

/**
* <p>Data Matrix Codes can encode text as bits in one of several modes, and can use multiple modes
* in one Data Matrix Code. This class decodes the bits back into text.</p>
*
* <p>See ISO 16022:2006, 5.2.1 - 5.2.9.2</p>
*
* @author bbrown@google.com (Brian Brown)
* @author Sean Owen
*/
namespace DecodedBitStreamParser {

/**
* See ISO 16022:2006, Annex C Table C.1
* The C40 Basic Character Set (*'s used for placeholders for the shift values)
*/
static const char C40_BASIC_SET_CHARS[] = {
	'*', '*', '*', ' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

static const char C40_SHIFT2_SET_CHARS[] = {
	'!', '"', '#', '$', '%', '&', '\'', '(', ')', '*',  '+', ',', '-', '.',
	'/', ':', ';', '<', '=', '>', '?',  '@', '[', '\\', ']', '^', '_', 29 // FNC1->29
};

/**
* See ISO 16022:2006, Annex C Table C.2
* The Text Basic Character Set (*'s used for placeholders for the shift values)
*/
static const char TEXT_BASIC_SET_CHARS[] = {
	'*', '*', '*', ' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

// Shift 2 for Text is the same encoding as C40
#define TEXT_SHIFT2_SET_CHARS C40_SHIFT2_SET_CHARS

static const char TEXT_SHIFT3_SET_CHARS[] = {
	'`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O',  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '{', '|', '}', '~', 127
};

struct Shift128
{
	bool set = false;
	char operator()(int val) { return static_cast<char>(val + std::exchange(set, false) * 128); }
};

/**
* See ISO 16022:2006, 5.4.1, Table 6
*/
static ECI ParseECIValue(BitSource& bits)
{
	int firstByte = bits.readBits(8);
	if (firstByte <= 127)
		return ECI(firstByte - 1);

	int secondByte = bits.readBits(8);
	if (firstByte <= 191)
		return ECI((firstByte - 128) * 254 + 127 + secondByte - 1);

	int thirdByte = bits.readBits(8);

	return ECI((firstByte - 192) * 64516 + 16383 + (secondByte - 1) * 254 + thirdByte - 1);
}

/**
* See ISO 16022:2006, 5.6
*/
static void ParseStructuredAppend(BitSource& bits, StructuredAppendInfo& sai)
{
	// 5.6.2 Table 8
	int symbolSequenceIndicator = bits.readBits(8);
	sai.index = symbolSequenceIndicator >> 4;
	sai.count = 17 - (symbolSequenceIndicator & 0x0F); // 2-16 permitted, 17 invalid

	if (sai.count == 17 || sai.count <= sai.index) // If info doesn't make sense
		sai.count = 0; // Choose to mark count as unknown

	int fileId1 = bits.readBits(8); // File identification 1
	int fileId2 = bits.readBits(8); // File identification 2

	// There's no conversion method or meaning given to the 2 file id codewords in Section 5.6.3, apart from
	// saying that each value should be 1-254. Choosing here to represent them as base 256.
	sai.id = std::to_string((fileId1 << 8) | fileId2);
}

std::optional<std::array<int, 3>> DecodeNextTriple(BitSource& bits)
{
	// Values are encoded in a 16-bit value as (1600 * C1) + (40 * C2) + C3 + 1
	// If there is less than 2 bytes left or the next byte is the unlatch codeword then the current segment has ended
	if (bits.available() < 16)
		return {};
	int firstByte = bits.readBits(8);
	if (firstByte == 254) // Unlatch codeword
		return {};

	int fullBitValue = (firstByte << 8) + bits.readBits(8) - 1;
	int a = fullBitValue / 1600;
	fullBitValue -= a * 1600;
	int b = fullBitValue / 40;
	int c = fullBitValue - b * 40;

	return {{a, b, c}};
}

enum class Mode {C40, TEXT};

/**
* See ISO 16022:2006, 5.2.5 and Annex C, Table C.1 (C40)
* See ISO 16022:2006, 5.2.6 and Annex C, Table C.2 (Text)
*/
static void DecodeC40OrTextSegment(BitSource& bits, Content& result, Mode mode)
{
	// TODO(bbrown): The Upper Shift with C40 doesn't work in the 4 value scenario all the time
	Shift128 upperShift;
	int shift = 0;

	const char* BASIC_SET_CHARS = mode == Mode::C40 ? C40_BASIC_SET_CHARS : TEXT_BASIC_SET_CHARS;
	const char* SHIFT_SET_CHARS = mode == Mode::C40 ? C40_SHIFT2_SET_CHARS : TEXT_SHIFT2_SET_CHARS;

	while (auto triple = DecodeNextTriple(bits)) {
		for (int cValue : *triple) {
			switch (std::exchange(shift, 0)) {
			case 0:
				if (cValue < 3)
					shift = cValue + 1;
				else if (cValue < 40) // Size(BASIC_SET_CHARS)
					result.push_back(upperShift(BASIC_SET_CHARS[cValue]));
				else
					throw FormatError("invalid value in C40 or Text segment");
				break;
			case 1: result.push_back(upperShift(cValue)); break;
			case 2:
				if (cValue < 28) // Size(SHIFT_SET_CHARS))
					result.push_back(upperShift(SHIFT_SET_CHARS[cValue]));
				else if (cValue == 30) // Upper Shift
					upperShift.set = true;
				else
					throw FormatError("invalid value in C40 or Text segment");
				break;
			case 3:
				if (mode == Mode::C40)
					result.push_back(upperShift(cValue + 96));
				else if (cValue < Size(TEXT_SHIFT3_SET_CHARS))
					result.push_back(upperShift(TEXT_SHIFT3_SET_CHARS[cValue]));
				else
					throw FormatError("invalid value in C40 or Text segment");
				break;
			default: throw FormatError("invalid value in C40 or Text segment"); ;
			}
		}
	}
}

/**
* See ISO 16022:2006, 5.2.7
*/
static void DecodeAnsiX12Segment(BitSource& bits, Content& result)
{
	while (auto triple = DecodeNextTriple(bits)) {
		for (int cValue : *triple) {
			// X12 segment terminator <CR>, separator *, sub-element separator >, space
			static const char segChars[4] = {'\r', '*', '>', ' '};
			if (cValue < 0)
				throw FormatError("invalid value in AnsiX12 segment");
			else if (cValue < 4)
				result.push_back(segChars[cValue]);
			else if (cValue < 14) // 0 - 9
				result.push_back((char)(cValue + 44));
			else if (cValue < 40) // A - Z
				result.push_back((char)(cValue + 51));
			else
				throw FormatError("invalid value in AnsiX12 segment");
		}
	}
}

/**
* See ISO 16022:2006, 5.2.8 and Annex C Table C.3
*/
static void DecodeEdifactSegment(BitSource& bits, Content& result)
{
	// If there are less than 3 bytes left then it will be encoded as ASCII
	while (bits.available() >= 24) {
		for (int i = 0; i < 4; i++) {
			char edifactValue = bits.readBits(6);

			// Check for the unlatch character
			if (edifactValue == 0x1F) {  // 011111
				// Read rest of byte, which should be 0, and stop
				if (bits.bitOffset())
					bits.readBits(8 - bits.bitOffset());
				return;
			}

			if ((edifactValue & 0x20) == 0) // no 1 in the leading (6th) bit
				edifactValue |= 0x40;       // Add a leading 01 to the 6 bit binary value
			result.push_back(edifactValue);
		}
	}
}

/**
* See ISO 16022:2006, Annex B, B.2
*/
static int Unrandomize255State(int randomizedBase256Codeword, int base256CodewordPosition)
{
	int pseudoRandomNumber = ((149 * base256CodewordPosition) % 255) + 1;
	int tempVariable = randomizedBase256Codeword - pseudoRandomNumber;
	return tempVariable >= 0 ? tempVariable : tempVariable + 256;
}

/**
* See ISO 16022:2006, 5.2.9 and Annex B, B.2
*/
static void DecodeBase256Segment(BitSource& bits, Content& result)
{
	// Figure out how long the Base 256 Segment is.
	int codewordPosition = 1 + bits.byteOffset(); // position is 1-indexed
	int d1 = Unrandomize255State(bits.readBits(8), codewordPosition++);
	int count;
	if (d1 == 0) // Read the remainder of the symbol
		count = bits.available() / 8;
	else if (d1 < 250)
		count = d1;
	else
		count = 250 * (d1 - 249) + Unrandomize255State(bits.readBits(8), codewordPosition++);

	// We're seeing NegativeArraySizeException errors from users.
	if (count < 0)
		throw FormatError("invalid count in Base256 segment");

	result.reserve(count);
	for (int i = 0; i < count; i++) {
		// readBits(8) may fail, have seen this particular error in the wild, such as at
		// http://www.bcgen.com/demo/IDAutomationStreamingDataMatrix.aspx?MODE=3&D=Fred&PFMT=3&PT=F&X=0.3&O=0&LM=0.2
		result += narrow_cast<uint8_t>(Unrandomize255State(bits.readBits(8), codewordPosition++));
	}
}

ZXING_EXPORT_TEST_ONLY
DecoderResult Decode(ByteArray&& bytes, const bool isDMRE)
{
	BitSource bits(bytes);
	Content result;
	Error error;
	result.symbology = {'d', '1', 3}; // ECC 200 (ISO 16022:2006 Annex N Table N.1)
	std::string resultTrailer;

	struct StructuredAppendInfo sai;
	bool readerInit = false;
	bool firstCodeword = true;
	bool done = false;
	int firstFNC1Position = 1;
	Shift128 upperShift;

	auto setError = [&error](Error&& e) {
		// return only the first error but keep on decoding if possible
		if (!error)
			error = std::move(e);
	};

	// See ISO 16022:2006, 5.2.3 and Annex C, Table C.2
	try {
		while (!done && bits.available() >= 8) {
			int oneByte = bits.readBits(8);
			switch (oneByte) {
			case 0: setError(FormatError("invalid 0 code word")); break;
			case 129: done = true; break; // Pad -> we are done, ignore the rest of the bits
			case 230: DecodeC40OrTextSegment(bits, result, Mode::C40); break;
			case 231: DecodeBase256Segment(bits, result); break;
			case 232: // FNC1
				// Only recognizing an FNC1 as first/second by codeword position (aka symbol character position), not
				// by decoded character position, i.e. not recognizing a C40/Text encoded FNC1 (which requires a latch
				// and a shift)
				if (bits.byteOffset() == firstFNC1Position)
					result.symbology.modifier = '2'; // GS1
				else if (bits.byteOffset() == firstFNC1Position + 1)
					result.symbology.modifier = '3'; // AIM, note no AIM Application Indicator format defined, ISO 16022:2006 11.2
				else
					result.push_back((char)29); // translate as ASCII 29 <GS>
				break;
			case 233: // Structured Append
				if (!firstCodeword) // Must be first ISO 16022:2006 5.6.1
					setError(FormatError("structured append tag must be first code word"));
				ParseStructuredAppend(bits, sai);
				firstFNC1Position = 5;
				break;
			case 234: // Reader Programming
				if (!firstCodeword) // Must be first ISO 16022:2006 5.2.4.9
					setError(FormatError("reader programming tag must be first code word"));
				readerInit = true;
				break;
			case 235: upperShift.set = true; break; // Upper Shift (shift to Extended ASCII)
			case 236: // ISO 15434 format "05" Macro
				result.append("[)>\x1E" "05\x1D");
				resultTrailer.insert(0, "\x1E\x04");
				break;
			case 237: // ISO 15434 format "06" Macro
				result.append("[)>\x1E" "06\x1D");
				resultTrailer.insert(0, "\x1E\x04");
				break;
			case 238: DecodeAnsiX12Segment(bits, result); break;
			case 239: DecodeC40OrTextSegment(bits, result, Mode::TEXT); break;
			case 240: DecodeEdifactSegment(bits, result); break;
			case 241: result.switchEncoding(ParseECIValue(bits)); break;
			default:
				if (oneByte <= 128) { // ASCII data (ASCII value + 1)
					result.push_back(upperShift(oneByte) - 1);
				} else if (oneByte <= 229) { // 2-digit data 00-99 (Numeric Value + 130)
					result.append(ToString(oneByte - 130, 2));
				} else if (oneByte >= 242) { // Not to be used in ASCII encodation
					// work around encoders that use unlatch to ASCII as last code word (ask upstream)
					if (oneByte == 254 && bits.available() == 0)
						break;
					setError(FormatError("invalid code word"));
					break;
				}
			}
			firstCodeword = false;
		}
	} catch (Error e) {
		setError(std::move(e));
	}

	result.append(resultTrailer);
	result.symbology.aiFlag = result.symbology.modifier == '2' ? AIFlag::GS1 : AIFlag::None;
	result.symbology.modifier += isDMRE * 6;

	return DecoderResult(std::move(result))
		.setError(std::move(error))
		.setStructuredAppend(sai)
		.setReaderInit(readerInit);
}

} // namespace DecodedBitStreamParser

/**
* <p>Given data and error-correction codewords received, possibly corrupted by errors, attempts to
* correct the errors in-place using Reed-Solomon error correction.</p>
*
* @param codewordBytes data and error correction codewords
* @param numDataCodewords number of codewords that are data bytes
* @return false if error correction fails
*/
static bool
CorrectErrors(ByteArray& codewordBytes, int numDataCodewords)
{
	// First read into an array of ints
	std::vector<int> codewordsInts(codewordBytes.begin(), codewordBytes.end());
	int numECCodewords = Size(codewordBytes) - numDataCodewords;

	if (!ReedSolomonDecode(GenericGF::DataMatrixField256(), codewordsInts, numECCodewords))
		return false;

	// Copy back into array of bytes -- only need to worry about the bytes that were data
	// We don't care about errors in the error-correction codewords
	std::copy_n(codewordsInts.begin(), numDataCodewords, codewordBytes.begin());

	return true;
}

static DecoderResult DoDecode(const BitMatrix& bits)
{
	// Construct a parser and read version, error-correction level
	const Version* version = VersionForDimensionsOf(bits);
	if (version == nullptr)
		return FormatError("Invalid matrix dimension");

	// Read codewords
	ByteArray codewords = CodewordsFromBitMatrix(bits, *version);
	if (codewords.empty())
		return FormatError("Invalid number of code words");

	bool fix259 = false; // see https://github.com/zxing-cpp/zxing-cpp/issues/259
retry:
	// Separate into data blocks
	std::vector<DataBlock> dataBlocks = GetDataBlocks(codewords, *version, fix259);
	if (dataBlocks.empty())
		return FormatError("Invalid number of data blocks");

	// Count total number of data bytes
	ByteArray resultBytes(TransformReduce(dataBlocks, 0, [](const auto& db) { return db.numDataCodewords; }));

	// Error-correct and copy data blocks together into a stream of bytes
	const int dataBlocksCount = Size(dataBlocks);
	for (int j = 0; j < dataBlocksCount; j++) {
		auto& [numDataCodewords, codewords] = dataBlocks[j];
		if (!CorrectErrors(codewords, numDataCodewords)) {
			if(version->versionNumber == 24 && !fix259) {
				fix259 = true;
				goto retry;
			}
			return ChecksumError();
		}

		for (int i = 0; i < numDataCodewords; i++) {
			// De-interlace data blocks.
			resultBytes[i * dataBlocksCount + j] = codewords[i];
		}
	}
#ifdef PRINT_DEBUG
	if (fix259)
		printf("-> needed retry with fix259 for 144x144 symbol\n");
#endif

	// Decode the contents of that stream of bytes
	return DecodedBitStreamParser::Decode(std::move(resultBytes), version->isDMRE())
		.setVersionNumber(version->versionNumber);
}

static BitMatrix FlippedL(const BitMatrix& bits)
{
	BitMatrix res(bits.height(), bits.width());
	for (int y = 0; y < res.height(); ++y)
		for (int x = 0; x < res.width(); ++x)
			res.set(x, y, bits.get(bits.width() - 1 - y, bits.height() - 1 - x));
	return res;
}

DecoderResult Decode(const BitMatrix& bits)
{
	auto res = DoDecode(bits);
	if (res.isValid())
		return res;

	//TODO:
	// * unify bit mirroring helper code with QRReader?
	// * rectangular symbols with the a size of 8 x Y are not supported a.t.m.
	if (auto mirroredRes = DoDecode(FlippedL(bits)); mirroredRes.error().type() != Error::Checksum) {
		mirroredRes.setIsMirrored(true);
		return mirroredRes;
	}

	return res;
}

} // namespace ZXing::DataMatrix
