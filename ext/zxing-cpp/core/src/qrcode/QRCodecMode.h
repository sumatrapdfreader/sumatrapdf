/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing::QRCode {

enum class Type;
class Version;

/**
* <p>See ISO 18004:2006, 6.4.1, Tables 2 and 3. This enum encapsulates the various modes in which
* data can be encoded to bits in the QR code standard.</p>
*/
enum class CodecMode
{
	TERMINATOR           = 0x00, // Not really a mode...
	NUMERIC              = 0x01,
	ALPHANUMERIC         = 0x02,
	STRUCTURED_APPEND    = 0x03,
	BYTE                 = 0x04,
	FNC1_FIRST_POSITION  = 0x05,
	ECI                  = 0x07, // character counts don't apply
	KANJI                = 0x08,
	FNC1_SECOND_POSITION = 0x09,
	HANZI                = 0x0D, // See GBT 18284-2000; "Hanzi" is a transliteration of this mode name.
};

/**
 * @param bits variable number of bits encoding a QR Code data mode
 * @param type type of QR Code
 * @return Mode encoded by these bits
 * @throws FormatError if bits do not correspond to a known mode
 */
CodecMode CodecModeForBits(int bits, Type type);

/**
 * @param version version in question
 * @return number of bits used, in this QR Code symbol {@link Version}, to encode the
 *         count of characters that will follow encoded in this Mode
 */
int CharacterCountBits(CodecMode mode, const Version& version);

/**
 * @param version version in question
 * @return number of bits used to encode a codec mode.
 */
int CodecModeBitsLength(const Version& version);

/**
 * @param version version in question
 * @return number of bits in the Terminator code.
 */
int TerminatorBitsLength(const Version& version);

} // namespace ZXing::QRCode
