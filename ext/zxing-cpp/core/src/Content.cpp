/*
 * Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "Content.h"

#include "CharacterSet.h"
#include "ECI.h"
#include "HRI.h"
#include "TextDecoder.h"
#include "Utf.h"
#include "ZXAlgorithms.h"

#if !defined(ZXING_READERS) && !defined(ZXING_WRITERS)
#include "Version.h"
#endif

#include <cctype>

namespace ZXing {

std::string ToString(ContentType type)
{
	const char* t2s[] = {"Text", "Binary", "Mixed", "GS1", "ISO15434", "UnknownECI"};
	return t2s[static_cast<int>(type)];
}

template <typename FUNC>
void Content::ForEachECIBlock(FUNC func) const
{
	ECI defaultECI = hasECI ? ECI::ISO8859_1 : ECI::Unknown;
	if (encodings.empty())
		func(defaultECI, 0, Size(bytes));
	else if (encodings.front().pos != 0)
		func(defaultECI, 0, encodings.front().pos);

	for (int i = 0; i < Size(encodings); ++i) {
		auto [eci, start] = encodings[i];
		int end = i + 1 == Size(encodings) ? Size(bytes) : encodings[i + 1].pos;

		if (start != end)
			func(eci, start, end);
	}
}

void Content::switchEncoding(ECI eci, bool isECI)
{
	// remove all non-ECI entries on first ECI entry
	if (isECI && !hasECI)
		encodings.clear();
	if (isECI || !hasECI)
		encodings.push_back({eci, Size(bytes)});

	hasECI |= isECI;
}

Content::Content() {}

Content::Content(ByteArray&& bytes, SymbologyIdentifier si) : bytes(std::move(bytes)), symbology(si) {}

void Content::switchEncoding(CharacterSet cs)
{
	switchEncoding(ToECI(cs), false);
}

void Content::append(const Content& other)
{
	if (!hasECI && other.hasECI)
		encodings.clear();
	if (other.hasECI || !hasECI)
		for (auto& e : other.encodings)
			encodings.push_back({e.eci, Size(bytes) + e.pos});
	append(other.bytes);

	hasECI |= other.hasECI;
}

void Content::erase(int pos, int n)
{
	bytes.erase(bytes.begin() + pos, bytes.begin() + pos + n);
	for (auto& e : encodings)
		if (e.pos > pos)
			pos -= n;
}

void Content::insert(int pos, const std::string& str)
{
	bytes.insert(bytes.begin() + pos, str.begin(), str.end());
	for (auto& e : encodings)
		if (e.pos > pos)
			pos += Size(str);
}

bool Content::canProcess() const
{
	return std::all_of(encodings.begin(), encodings.end(), [](Encoding e) { return CanProcess(e.eci); });
}

std::string Content::render(bool withECI) const
{
	if (empty() || !canProcess())
		return {};

#ifdef ZXING_READERS
	std::string res;
	if (withECI)
		res = symbology.toString(true);
	ECI lastECI = ECI::Unknown;
	auto fallbackCS = defaultCharset;
	if (!hasECI && fallbackCS == CharacterSet::Unknown)
		fallbackCS = guessEncoding();

	ForEachECIBlock([&](ECI eci, int begin, int end) {
		// first determine how to decode the content (choose character set)
		//  * eci == ECI::Unknown implies !hasECI and we guess
		//  * if !IsText(eci) the ToCharcterSet(eci) will return Unknown and we decode as binary
		CharacterSet cs = eci == ECI::Unknown ? fallbackCS : ToCharacterSet(eci);

		if (withECI) {
			// then find the eci to report back in the ECI designator
			if (IsText(ToECI(cs))) // everything decoded as text is reported as utf8
				eci = ECI::UTF8;
			else if (eci == ECI::Unknown) // implies !hasECI and fallbackCS is Unknown or Binary
				eci = ECI::Binary;

			if (lastECI != eci)
				res += ToString(eci);
			lastECI = eci;

			std::string tmp;
			TextDecoder::Append(tmp, bytes.data() + begin, end - begin, cs);
			for (auto c : tmp) {
				res += c;
				if (c == '\\') // in the ECI protocol a '\' has to be doubled
					res += c;
			}
		} else {
			TextDecoder::Append(res, bytes.data() + begin, end - begin, cs);
		}
	});

	return res;
#else
	//TODO: replace by proper construction from encoded data from within zint
	return std::string(bytes.asString());
#endif
}

std::string Content::text(TextMode mode) const
{
	switch (mode) {
	case TextMode::Plain: return render(false);
	case TextMode::ECI: return render(true);
	case TextMode::HRI:
		switch (type()) {
#ifdef ZXING_READERS
		case ContentType::GS1: {
			auto plain = render(false);
			auto hri = HRIFromGS1(plain);
			return hri.empty() ? plain : hri;
		}
		case ContentType::ISO15434: return HRIFromISO15434(render(false));
		case ContentType::Text: return render(false);
#endif
		default: return text(TextMode::Escaped);
		}
	case TextMode::Hex: return ToHex(bytes);
	case TextMode::Escaped: return EscapeNonGraphical(render(false));
	}

	return {}; // silence compiler warning
}

std::wstring Content::utfW() const
{
	return FromUtf8(render(false));
}

ByteArray Content::bytesECI() const
{
	if (empty())
		return {};

	std::string res = symbology.toString(true);

	ForEachECIBlock([&](ECI eci, int begin, int end) {
		if (hasECI)
			res += ToString(eci);

		for (int i = begin; i != end; ++i) {
			char c = static_cast<char>(bytes[i]);
			res += c;
			if (c == '\\') // in the ECI protocol a '\' has to be doubled
				res += c;
		}
	});

	return ByteArray(res);
}

CharacterSet Content::guessEncoding() const
{
#ifdef ZXING_READERS
	// assemble all blocks with unknown encoding
	ByteArray input;
	ForEachECIBlock([&](ECI eci, int begin, int end) {
		if (eci == ECI::Unknown)
			input.insert(input.end(), bytes.begin() + begin, bytes.begin() + end);
	});

	if (input.empty())
		return CharacterSet::Unknown;

	return TextDecoder::GuessEncoding(input.data(), input.size(), CharacterSet::ISO8859_1);
#else
	return CharacterSet::Unknown;
#endif
}

ContentType Content::type() const
{
#ifdef ZXING_READERS
	if (empty())
		return ContentType::Text;

	if (!canProcess())
		return ContentType::UnknownECI;

	if (symbology.aiFlag == AIFlag::GS1)
		return ContentType::GS1;

	// check for the absolut minimum of a ISO 15434 conforming message ("[)>" + RS + digit + digit)
	if (bytes.size() > 6 && bytes.asString(0, 4) == "[)>\x1E" && std::isdigit(bytes[4]) && std::isdigit(bytes[5]))
		return ContentType::ISO15434;

	ECI fallback = ToECI(guessEncoding());
	std::vector<bool> binaryECIs;
	ForEachECIBlock([&](ECI eci, int begin, int end) {
		if (eci == ECI::Unknown)
			eci = fallback;
		binaryECIs.push_back((!IsText(eci)
							  || (ToInt(eci) > 0 && ToInt(eci) < 28 && ToInt(eci) != 25
								  && std::any_of(bytes.begin() + begin, bytes.begin() + end,
												 [](auto c) { return c < 0x20 && c != 0x9 && c != 0xa && c != 0xd; }))));
	});

	if (!Contains(binaryECIs, true))
		return ContentType::Text;
	if (!Contains(binaryECIs, false))
		return ContentType::Binary;

	return ContentType::Mixed;
#else
	//TODO: replace by proper construction from encoded data from within zint
	return ContentType::Text;
#endif
}

} // namespace ZXing
