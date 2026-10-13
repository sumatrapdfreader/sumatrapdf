/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "MCDecoder.h"

#include "ByteArray.h"
#include "CharacterSet.h"
#include "DecoderResult.h"
#include "GenericGF.h"
#include "MCBitMatrixParser.h"
#include "ReedSolomonDecoder.h"
#include "ZXTestSupport.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ZXing::MaxiCode {

static const int ALL = 0;
static const int EVEN = 1;
static const int ODD = 2;

static bool CorrectErrors(ByteArray& codewordBytes, int start, int dataCodewords, int ecCodewords, int mode)
{
	int codewords = dataCodewords + ecCodewords;

	// in EVEN or ODD mode only half the codewords
	int divisor = mode == ALL ? 1 : 2;

	// First read into an array of ints
	std::vector<int> codewordsInts(codewords / divisor, 0);
	for (int i = 0; i < codewords; i++) {
		if ((mode == ALL) || (i % 2 == (mode - 1)))
			codewordsInts[i / divisor] = codewordBytes[i + start];
	}

	if (!ReedSolomonDecode(GenericGF::MaxiCodeField64(), codewordsInts, ecCodewords / divisor))
		return false;

	// Copy back into array of bytes -- only need to worry about the bytes that were data
	// We don't care about errors in the error-correction codewords
	for (int i = 0; i < dataCodewords; i++) {
		if ((mode == ALL) || (i % 2 == (mode - 1)))
			codewordBytes[i + start] = narrow_cast<uint8_t>(codewordsInts[i / divisor]);
	}

	return true;
}

/**
* <p>MaxiCodes can encode text or structured information as bits in one of several modes,
* with multiple character sets in one code. This class decodes the bits back into text.</p>
*
* @author mike32767
* @author Manuel Kasten
*/
namespace DecodedBitStreamParser {

static const short SHI0 = 0x100;
static const short SHI1 = 0x101;
static const short SHI2 = 0x102;
static const short SHI3 = 0x103;
static const short SHI4 = 0x104;
static const short TWSA = 0x105;	// two shift A
static const short TRSA = 0x106;	// three shift A
static const short LCHA = 0x107;	// latch A
static const short LCHB = 0x108;	// latch B
static const short LOCK = 0x109;
static const short ECI  = 0x10A;
static const short NS   = 0x10B;
static const short PAD  = 0x10C;

static const char FS = 0x1C;
static const char GS = 0x1D;
static const char RS = 0x1E;

// clang-format off
const static std::array<short, 0x40> CHARSETS[] = {
	{ // set 0 (A)
		'\r',  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
		 'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z',  ECI,   FS,   GS,   RS,   NS,
		 ' ',  PAD,  '"',  '#',  '$',  '%',  '&', '\'',  '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
		 '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ':', SHI1, SHI2, SHI3, SHI4, LCHB,
	},
	{ // set 1 (B)
		 '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
		 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  ECI,   FS,   GS,   RS,   NS,
		 '{',  PAD,  '}',  '~', 0x7F,  ';',  '<',  '=',  '>',  '?',  '[', '\\',  ']',  '^',  '_',  ' ',
		 ',',  '.',  '/',  ':',  '@',  '!',  '|',  PAD, TWSA, TRSA,  PAD, SHI0, SHI2, SHI3, SHI4, LCHA,
	},
	{ // set 2 (C)
		0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
		0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,  ECI,   FS,   GS,   RS,   NS,	// Note that in original code in Java, NS is not there, which seems to be a bug
		0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xAA, 0xAC, 0xB1, 0xB2, 0xB3, 0xB5, 0xB9, 0xBA, 0xBC, 0xBD, 0xBE,
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, LCHA, 0x20, LOCK, SHI3, SHI4, LCHB,
	},
	{ // set 3 (D)
		0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
		0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,  ECI,   FS,   GS,   RS,   NS,
		0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0xA1, 0xA8, 0xAB, 0xAF, 0xB0, 0xB4, 0xB7, 0xB8, 0xBB, 0xBF, 0x8A,
		0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, LCHA, 0x20, SHI2, LOCK, SHI4, LCHB,
	},
	{ // set 4 (E)
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,  ECI,  PAD,  PAD, 0x1B,   NS,
		  FS,   GS,   RS, 0x1F, 0x9F, 0xA0, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA9, 0xAD, 0xAE, 0xB6,
		0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, LCHA, 0x20, SHI2, SHI3, LOCK, LCHB,
	},
};
// clang-format on

static int GetBit(int bit, const ByteArray& bytes)
{
	bit--;
	return (bytes[bit / 6] & (1 << (5 - (bit % 6)))) == 0 ? 0 : 1;
}

static unsigned int GetInt(const ByteArray& bytes, const ByteArray& x)
{
	int len = Size(x);
	unsigned int val = 0;
	for (int i = 0; i < len; i++)
		val += GetBit(x[i], bytes) << (len - i - 1);

	return val;
}

static unsigned int GetPostCode2Length(const ByteArray& bytes)
{
	return std::min(GetInt(bytes, {39, 40, 41, 42, 31, 32}), 9U);
}

static std::string GetPostCode2(const ByteArray& bytes)
{
	unsigned int val = GetInt(bytes,
				  {33, 34, 35, 36, 25, 26, 27, 28, 29, 30, 19, 20, 21, 22, 23, 24, 13, 14, 15, 16, 17, 18, 7, 8, 9, 10, 11, 12, 1, 2});
	unsigned int len = GetPostCode2Length(bytes);
	// Pad or truncate to length
	char buf[11]; // 30 bits 0x3FFFFFFF == 1073741823 (10 digits)
	snprintf(buf, sizeof(buf), "%0*d", len, val);
	buf[len] = '\0';
	return buf;
}

static std::string GetPostCode3(const ByteArray& bytes)
{
	return {
		(char) CHARSETS[0].at(GetInt(bytes, { 39, 40, 41, 42, 31, 32 })),
		(char) CHARSETS[0].at(GetInt(bytes, { 33, 34, 35, 36, 25, 26 })),
		(char) CHARSETS[0].at(GetInt(bytes, { 27, 28, 29, 30, 19, 20 })),
		(char) CHARSETS[0].at(GetInt(bytes, { 21, 22, 23, 24, 13, 14 })),
		(char) CHARSETS[0].at(GetInt(bytes, { 15, 16, 17, 18,  7,  8 })),
		(char) CHARSETS[0].at(GetInt(bytes, { 9,  10, 11, 12,  1,  2 })),
	};
}

static unsigned int GetCountry(const ByteArray& bytes)
{
	return std::min(GetInt(bytes, {53, 54, 43, 44, 45, 46, 47, 48, 37, 38}), 999U);
}

static unsigned int GetServiceClass(const ByteArray& bytes)
{
	return std::min(GetInt(bytes, {55, 56, 57, 58, 59, 60, 49, 50, 51, 52}), 999U);
}

/**
 * See ISO/IEC 16023:2000 Section 4.6 Table 3
 */
static ZXing::ECI ParseECIValue(const ByteArray& bytes, int& i)
{
	int firstByte = bytes[++i];
	if ((firstByte & 0x20) == 0)
		return ZXing::ECI(firstByte);

	int secondByte = bytes[++i];
	if ((firstByte & 0x10) == 0)
		return ZXing::ECI(((firstByte & 0x0F) << 6) | secondByte);

	int thirdByte = bytes[++i];
	if ((firstByte & 0x08) == 0)
		return ZXing::ECI(((firstByte & 0x07) << 12) | (secondByte << 6) | thirdByte);

	int fourthByte = bytes[++i];
	return ZXing::ECI(((firstByte & 0x03) << 18) | (secondByte << 12) | (thirdByte << 6) | fourthByte);
}

/**
 * See ISO/IEC 16023:2000 Section 4.9.1 Table 5
 */
static void ParseStructuredAppend(const ByteArray& bytes, int& i, StructuredAppendInfo& sai)
{
	int byte  = bytes[++i];
	sai.index = (byte >> 3) & 0x07;
	sai.count = (byte & 0x07) + 1;
	if (sai.count == 1 || sai.count <= sai.index) // If info doesn't make sense
		sai.count = 0;                            // Choose to mark count as unknown
	// No id
}

static void GetMessage(const ByteArray& bytes, int start, int len, Content& result, StructuredAppendInfo& sai)
{
	int shift = -1;
	int set = 0;
	int lastset = 0;

	for (int i = start; i < start + len; i++) {
		int c = CHARSETS[set].at(bytes[i]);
		switch (c) {
		case LCHA:
			set   = 0;
			shift = -1;
			break;
		case LCHB:
			set   = 1;
			shift = -1;
			break;
		case SHI0:
		case SHI1:
		case SHI2:
		case SHI3:
		case SHI4:
			lastset = set;
			set     = c - SHI0;
			shift   = 1;
			break;
		case TWSA:
			lastset = set;
			set     = 0;
			shift   = 2;
			break;
		case TRSA:
			lastset = set;
			set     = 0;
			shift   = 3;
			break;
		case NS:
			result.append(
				ToString((bytes[i + 1] << 24) + (bytes[i + 2] << 18) + (bytes[i + 3] << 12) + (bytes[i + 4] << 6) + bytes[i + 5], 9));
			i += 5;
			break;
		case LOCK: shift = -1; break;
		case ECI: result.switchEncoding(ParseECIValue(bytes, i)); break;
		case PAD:
			if (i == start)
				ParseStructuredAppend(bytes, i, sai);
			shift = -1;
			break;
		default: result.push_back(c);
		}

		if (shift-- == 0)
			set = lastset;
	}
}

ZXING_EXPORT_TEST_ONLY
DecoderResult Decode(ByteArray&& bytes, const int mode)
{
	Content result;
	result.symbology = {'U', (mode == 2 || mode == 3) ? '1' : '0', 2}; // TODO: No identifier defined for mode 6?
	result.defaultCharset = CharacterSet::ISO8859_1;
	StructuredAppendInfo sai;

	switch (mode) {
	case 2:
	case 3: {
		auto postcode = mode == 2 ? GetPostCode2(bytes) : GetPostCode3(bytes);
		auto country  = ToString(GetCountry(bytes), 3);
		auto service  = ToString(GetServiceClass(bytes), 3);
		GetMessage(bytes, 10, 84, result, sai);
		if (result.bytes.asString().compare(0, 7, "[)>\u001E01\u001D") == 0) // "[)>" + RS + "01" + GS
			result.insert(9, postcode + GS + country + GS + service + GS);
		else
			result.insert(0, postcode + GS + country + GS + service + GS);
		break;
	}
	case 4:
	case 6: GetMessage(bytes, 1, 93, result, sai); break;
	case 5: GetMessage(bytes, 1, 77, result, sai); break;
	}

	return DecoderResult(std::move(result))
		.setEcLevel(std::to_string(mode))
		.setStructuredAppend(sai)
		.setReaderInit(mode == 6);
}

} // DecodedBitStreamParser

DecoderResult Decode(const BitMatrix& bits)
{
	ByteArray codewords = BitMatrixParser::ReadCodewords(bits);

	if (!CorrectErrors(codewords, 0, 10, 10, ALL))
		return ChecksumError();

	int mode = codewords[0] & 0x0F;
	ByteArray datawords;
	switch (mode) {
	case 2: // Structured Carrier Message (numeric postcode)
	case 3: // Structured Carrier Message (alphanumeric postcode)
	case 4: // Standard Symbol
	case 6: // Reader Programming
		if (CorrectErrors(codewords, 20, 84, 40, EVEN) && CorrectErrors(codewords, 20, 84, 40, ODD))
			datawords.resize(94, 0);
		else
			return ChecksumError();
		break;
	case 5: // Full ECC
		if (CorrectErrors(codewords, 20, 68, 56, EVEN) && CorrectErrors(codewords, 20, 68, 56, ODD))
			datawords.resize(78, 0);
		else
			return ChecksumError();
		break;
	default: return FormatError("Invalid mode");
	}

	std::copy_n(codewords.begin(), 10, datawords.begin());
	std::copy_n(codewords.begin() + 20, datawords.size() - 10, datawords.begin() + 10);

	return DecodedBitStreamParser::Decode(std::move(datawords), mode);
}

} // namespace ZXing::MaxiCode
