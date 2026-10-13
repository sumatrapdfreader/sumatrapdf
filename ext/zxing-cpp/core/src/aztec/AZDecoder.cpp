/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "AZDecoder.h"

#include "AZDetectorResult.h"
#include "BitArray.h"
#include "BitMatrix.h"
#include "DecoderResult.h"
#include "GenericGF.h"
#include "ReedSolomonDecoder.h"
#include "ZXTestSupport.h"
#include "ZXAlgorithms.h"

#include <cctype>
#include <cstring>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace ZXing::Aztec {

enum class Table
{
	UPPER,
	LOWER,
	MIXED,
	DIGIT,
	PUNCT,
	BINARY
};

static const char* UPPER_TABLE[] = {
	"CTRL_PS", " ", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P",
	"Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "CTRL_LL", "CTRL_ML", "CTRL_DL", "CTRL_BS"
};

static const char* LOWER_TABLE[] = {
	"CTRL_PS", " ", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p",
	"q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "CTRL_US", "CTRL_ML", "CTRL_DL", "CTRL_BS"
};

static const char* MIXED_TABLE[] = {
	"CTRL_PS", " ", "\1", "\2", "\3", "\4", "\5", "\6", "\7", "\b", "\t", "\n",
	"\13", "\f", "\r", "\33", "\34", "\35", "\36", "\37", "@", "\\", "^", "_",
	"`", "|", "~", "\177", "CTRL_LL", "CTRL_UL", "CTRL_PL", "CTRL_BS"
};

static const char* PUNCT_TABLE[] = {
	"FLGN", "\r", "\r\n", ". ", ", ", ": ", "!", "\"", "#", "$", "%", "&", "'", "(", ")",
	"*", "+", ",", "-", ".", "/", ":", ";", "<", "=", ">", "?", "[", "]", "{", "}", "CTRL_UL"
};

static const char* DIGIT_TABLE[] = {
	"CTRL_PS", " ", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ",", ".", "CTRL_UL", "CTRL_US"
};

static int TotalBitsInLayer(int layers, bool compact)
{
	return ((compact ? 88 : 112) + 16 * layers) * layers;
}

/**
* Gets the array of bits from an Aztec Code matrix
*
* @return the array of bits
*/
static BitArray ExtractBits(const DetectorResult& ddata)
{
	bool compact = ddata.isCompact();
	int layers = ddata.nbLayers();
	int baseMatrixSize = (compact ? 11 : 14) + layers * 4; // not including alignment lines
	std::vector<int> map(baseMatrixSize, 0);

	if (compact) {
		std::iota(map.begin(), map.end(), 0);
	} else {
		int matrixSize = baseMatrixSize + 1 + 2 * ((baseMatrixSize / 2 - 1) / 15);
		int origCenter = baseMatrixSize / 2;
		int center = matrixSize / 2;
		for (int i = 0; i < origCenter; i++) {
			int newOffset = i + i / 15;
			map[origCenter - i - 1] = center - newOffset - 1;
			map[origCenter + i] = center + newOffset + 1;
		}
	}
	auto& matrix = ddata.bits();
	BitArray rawbits(TotalBitsInLayer(layers, compact));
	for (int i = 0, rowOffset = 0; i < layers; i++) {
		int rowSize = (layers - i) * 4 + (compact ? 9 : 12);
		// The top-left most point of this layer is <low, low> (not including alignment lines)
		int low = i * 2;
		// The bottom-right most point of this layer is <high, high> (not including alignment lines)
		int high = baseMatrixSize - 1 - low;
		// We pull bits from the two 2 x rowSize columns and two rowSize x 2 rows
		for (int j = 0; j < rowSize; j++) {
			int colOffset = j * 2;
			for (int k = 0; k < 2; k++) {
				// left column
				rawbits.set(rowOffset + 0 * rowSize + colOffset + k, matrix.get(map[low + k], map[low + j]));
				// bottom row
				rawbits.set(rowOffset + 2 * rowSize + colOffset + k, matrix.get(map[low + j], map[high - k]));
				// right column
				rawbits.set(rowOffset + 4 * rowSize + colOffset + k, matrix.get(map[high - k], map[high - j]));
				// top row
				rawbits.set(rowOffset + 6 * rowSize + colOffset + k, matrix.get(map[high - j], map[low + k]));
			}
		}
		rowOffset += rowSize * 8;
	}
	return rawbits;
}

/**
* @brief Performs RS error correction on an array of bits.
*/
static BitArray CorrectBits(const DetectorResult& ddata, const BitArray& rawbits)
{
	const GenericGF* gf = nullptr;
	int codewordSize;

	if (ddata.nbLayers() <= 2) {
		codewordSize = 6;
		gf = &GenericGF::AztecData6();
	} else if (ddata.nbLayers() <= 8) {
		codewordSize = 8;
		gf = &GenericGF::AztecData8();
	} else if (ddata.nbLayers() <= 22) {
		codewordSize = 10;
		gf = &GenericGF::AztecData10();
	} else {
		codewordSize = 12;
		gf = &GenericGF::AztecData12();
	}

	int numCodewords = Size(rawbits) / codewordSize;
	int numDataCodewords = ddata.nbDatablocks();
	int numECCodewords = numCodewords - numDataCodewords;

	if (numCodewords < numDataCodewords)
		throw FormatError("Invalid number of code words");

	auto dataWords = ToInts<int>(rawbits, codewordSize, numCodewords, Size(rawbits) % codewordSize);

	if (!ReedSolomonDecode(*gf, dataWords, numECCodewords))
		throw ChecksumError();

	// drop the ECCodewords from the dataWords array
	dataWords.resize(numDataCodewords);

	// Now perform the unstuffing operation.
	BitArray correctedBits;
	// correctedBits.reserve(numDataCodewords * codewordSize - stuffedBits);
	for (int dataWord : dataWords) {
		if (dataWord == 0 || dataWord == (1 << codewordSize) - 1)
			return {};
		else if (dataWord == 1) // next codewordSize-1 bits are all zeros or all ones
			correctedBits.appendBits(0, codewordSize - 1);
		else if (dataWord == (1 << codewordSize) - 2)
			correctedBits.appendBits(0xffffffff, codewordSize - 1);
		else
			correctedBits.appendBits(dataWord, codewordSize);
	}

	return correctedBits;
}

/**
* gets the table corresponding to the char passed
*/
static Table GetTable(char t)
{
	switch (t) {
	case 'L': return Table::LOWER;
	case 'P': return Table::PUNCT;
	case 'M': return Table::MIXED;
	case 'D': return Table::DIGIT;
	case 'B': return Table::BINARY;
	case 'U':
	default: return Table::UPPER;
	}
}

/**
* Gets the character (or string) corresponding to the passed code in the given table
*
* @param table the table used
* @param code the code of the character
*/
static const char* GetCharacter(Table table, int code)
{
	switch (table) {
	case Table::UPPER: return UPPER_TABLE[code];
	case Table::LOWER: return LOWER_TABLE[code];
	case Table::MIXED: return MIXED_TABLE[code];
	case Table::PUNCT: return PUNCT_TABLE[code];
	case Table::DIGIT: return DIGIT_TABLE[code];
	case Table::BINARY: return nullptr; // should not happen
	}
	// silence gcc warning/error (this code can not be reached)
	return nullptr;
}

/**
* See ISO/IEC 24778:2008 Section 10.1
*/
static ECI ParseECIValue(BitArrayView& bits, const int flg)
{
	int eci = 0;
	for (int i = 0; i < flg; i++)
		eci = 10 * eci + bits.readBits(4) - 2;
	return ECI(eci);
}

/**
* See ISO/IEC 24778:2008 Section 8
*/
static StructuredAppendInfo ParseStructuredAppend(ByteArray& bytes)
{
	std::string text(bytes.begin(), bytes.end());
	StructuredAppendInfo sai;
	std::string::size_type i = 0;

	if (text[0] == ' ') { // Space-delimited id
		std::string::size_type sp = text.find(' ', 1);
		if (sp == std::string::npos)
			return {};

		sai.id = text.substr(1, sp - 1); // Strip space delimiters
		i = sp + 1;
	}
	if (i + 1 >= text.size() || !std::isupper(text[i]) || !std::isupper(text[i + 1]))
		return {};

	sai.index = text[i] - 'A';
	sai.count = text[i + 1] - 'A' + 1;

	if (sai.count == 1 || sai.count <= sai.index) // If info doesn't make sense
		sai.count = 0; // Choose to mark count as unknown

	text.erase(0, i + 2); // Remove
	bytes = ByteArray(text);

	return sai;
}

static void DecodeContent(const BitArray& bits, Content& res)
{
	Table latchTable = Table::UPPER; // table most recently latched to
	Table shiftTable = Table::UPPER; // table to use for the next read

	auto remBits = BitArrayView(bits);

	while (remBits.size() >= (shiftTable == Table::DIGIT ? 4 : 5)) { // see ISO/IEC 24778:2008 7.3.1.2 regarding padding bits
		if (shiftTable == Table::BINARY) {
			if (remBits.size() <= 6) // padding bits
				break;
			int length = remBits.readBits(5);
			if (length == 0)
				length = remBits.readBits(11) + 31;
			for (int i = 0; i < length; i++)
				res.push_back(remBits.readBits(8));
			// Go back to whatever mode we had been in
			shiftTable = latchTable;
		} else {
			int size = shiftTable == Table::DIGIT ? 4 : 5;
			int code = remBits.readBits(size);
			const char* str = GetCharacter(shiftTable, code);
			if (std::strncmp(str, "CTRL_", 5) == 0) {
				// Table changes
				// ISO/IEC 24778:2008 prescibes ending a shift sequence in the mode from which it was invoked.
				// That's including when that mode is a shift.
				// Our test case dlusbs.png for issue #642 exercises that.
				latchTable = shiftTable;  // Latch the current mode, so as to return to Upper after U/S B/S
				shiftTable = GetTable(str[5]);
				if (str[6] == 'L')
					latchTable = shiftTable;
			} else if (std::strcmp(str, "FLGN") == 0) {
				int flg = remBits.readBits(3);
				if (flg == 0) { // FNC1
					res.push_back(29); // May be removed at end if first/second FNC1
				} else if (flg <= 6) {
					// FLG(1) to FLG(6) ECI
					res.switchEncoding(ParseECIValue(remBits, flg));
				} else {
					// FLG(7) is invalid
				}
				shiftTable = latchTable;
			} else {
				res.append(str);
				// Go back to whatever mode we had been in
				shiftTable = latchTable;
			}
		}
	}
}

ZXING_EXPORT_TEST_ONLY
DecoderResult Decode(const BitArray& bits)
{
	Content res;
	res.symbology = {'z', '0', 3};

	try {
		DecodeContent(bits, res);
	} catch (const std::exception&) { // see BitArrayView::readBits
		return FormatError();
	}

	if (res.bytes.empty())
		return FormatError("Empty symbol content");

	// Check for Structured Append - need 4 5-bit words, beginning with ML UL, ending with index and count
	bool haveStructuredAppend = Size(bits) > 20 && ToInt(bits, 0, 5) == 29 // latch to MIXED (from UPPER)
								&& ToInt(bits, 5, 5) == 29;                // latch back to UPPER (from MIXED)

	StructuredAppendInfo sai = haveStructuredAppend ? ParseStructuredAppend(res.bytes) : StructuredAppendInfo();

	// As converting character set ECIs ourselves and ignoring/skipping non-character ECIs, not using
	// modifiers that indicate ECI protocol (ISO/IEC 24778:2008 Annex F Table F.1)
	if (res.bytes.size() > 1 && res.bytes[0] == 29) {
		res.symbology.modifier = '1'; // GS1
		res.symbology.aiFlag = AIFlag::GS1;
		res.erase(0, 1); // Remove FNC1
	} else if (res.bytes.size() > 2 && std::isupper(res.bytes[0]) && res.bytes[1] == 29) {
		// FNC1 following single uppercase letter (the AIM Application Indicator)
		res.symbology.modifier = '2'; // AIM
		res.symbology.aiFlag = AIFlag::AIM;
		res.erase(1, 1); // Remove FNC1,
						 // The AIM Application Indicator character "A"-"Z" is left in the stream (ISO/IEC 24778:2008 16.2)
	} else if (res.bytes.size() > 3 && std::isdigit(res.bytes[0]) && std::isdigit(res.bytes[1]) && res.bytes[2] == 29) {
		// FNC1 following 2 digits (the AIM Application Indicator)
		res.symbology.modifier = '2'; // AIM
		res.symbology.aiFlag = AIFlag::AIM;
		res.erase(2, 1); // Remove FNC1
						 // The AIM Application Indicator characters "00"-"99" are left in the stream (ISO/IEC 24778:2008 16.2)
	}

	if (sai.index != -1)
		res.symbology.modifier += 6; // TODO: this is wrong as long as we remove the sai info from the content in ParseStructuredAppend

	return DecoderResult(std::move(res)).setStructuredAppend(sai);
}

DecoderResult DecodeRune(const DetectorResult& detectorResult) {
	Content res;
	res.symbology = {'z', 'C', 0}; // Runes cannot have ECI

	// Bizarrely, this is what it says to do in the spec
	auto runeString = ToString(detectorResult.runeValue(), 3);
	res.append(runeString);

	return DecoderResult(std::move(res));
}

DecoderResult Decode(const DetectorResult& detectorResult)
{
	try {
		if (detectorResult.nbLayers() == 0) {
			// This is a rune - just return the rune value
			return DecodeRune(detectorResult);
		}
		auto bits = CorrectBits(detectorResult, ExtractBits(detectorResult));
		return Decode(bits);
	} catch (Error e) {
		return e;
	}
}

} // namespace ZXing::Aztec
