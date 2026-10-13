/*
* Copyright 2024 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#ifdef ZXING_EXPERIMENTAL_API

#include "WriteBarcode.h"
#include "BitMatrix.h"

#if !defined(ZXING_READERS) && !defined(ZXING_WRITERS)
#include "Version.h"
#endif

#include <sstream>

#ifdef ZXING_USE_ZINT

#include <zint.h>

#else

struct zint_symbol {};

#endif // ZXING_USE_ZINT

namespace ZXing {

struct CreatorOptions::Data
{
	BarcodeFormat format;
	bool readerInit = false;
	bool forceSquareDataMatrix = false;
	std::string ecLevel;

	// symbol size (qrcode, datamatrix, etc), map from I, 'WxH'
	// structured_append (idx, cnt, ID)

	mutable unique_zint_symbol zint;

#ifndef __cpp_aggregate_paren_init
	Data(BarcodeFormat f) : format(f) {}
#endif
};

#define ZX_PROPERTY(TYPE, NAME) \
	TYPE CreatorOptions::NAME() const noexcept { return d->NAME; } \
	CreatorOptions& CreatorOptions::NAME(TYPE v)& { return d->NAME = std::move(v), *this; } \
	CreatorOptions&& CreatorOptions::NAME(TYPE v)&& { return d->NAME = std::move(v), std::move(*this); }

	ZX_PROPERTY(BarcodeFormat, format)
	ZX_PROPERTY(bool, readerInit)
	ZX_PROPERTY(bool, forceSquareDataMatrix)
	ZX_PROPERTY(std::string, ecLevel)

#undef ZX_PROPERTY

CreatorOptions::CreatorOptions(BarcodeFormat format) : d(std::make_unique<Data>(format)) {}
CreatorOptions::~CreatorOptions() = default;
CreatorOptions::CreatorOptions(CreatorOptions&&) = default;
CreatorOptions& CreatorOptions::operator=(CreatorOptions&&) = default;


struct WriterOptions::Data
{
	int scale = 0;
	int sizeHint = 0;
	int rotate = 0;
	bool withHRT = false;
	bool withQuietZones = true;
};

#define ZX_PROPERTY(TYPE, NAME) \
	TYPE WriterOptions::NAME() const noexcept { return d->NAME; } \
	WriterOptions& WriterOptions::NAME(TYPE v)& { return d->NAME = std::move(v), *this; } \
	WriterOptions&& WriterOptions::NAME(TYPE v)&& { return d->NAME = std::move(v), std::move(*this); }

ZX_PROPERTY(int, scale)
ZX_PROPERTY(int, sizeHint)
ZX_PROPERTY(int, rotate)
ZX_PROPERTY(bool, withHRT)
ZX_PROPERTY(bool, withQuietZones)

#undef ZX_PROPERTY

WriterOptions::WriterOptions() : d(std::make_unique<Data>()) {}
WriterOptions::~WriterOptions() = default;
WriterOptions::WriterOptions(WriterOptions&&) = default;
WriterOptions& WriterOptions::operator=(WriterOptions&&) = default;

static bool IsLinearCode(BarcodeFormat format)
{
	return BarcodeFormats(BarcodeFormat::LinearCodes).testFlag(format);
}

static std::string ToSVG(ImageView iv)
{
	if (!iv.data())
		return {};

	// see https://stackoverflow.com/questions/10789059/create-qr-code-in-vector-image/60638350#60638350

	std::ostringstream res;

	res << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" viewBox=\"0 0 " << iv.width() << " " << iv.height()
		<< "\" stroke=\"none\">\n"
		<< "<path d=\"";

	for (int y = 0; y < iv.height(); ++y)
		for (int x = 0; x < iv.width(); ++x)
			if (*iv.data(x, y) == 0)
				res << "M" << x << "," << y << "h1v1h-1z";

	res << "\"/>\n</svg>";

	return res.str();
}

static Image ToImage(BitMatrix bits, bool isLinearCode, const WriterOptions& opts)
{
	bits.flipAll();
	auto symbol = Inflate(std::move(bits), opts.sizeHint(),
						  isLinearCode ? std::clamp(opts.sizeHint() / 2, 50, 300) : opts.sizeHint(),
						  opts.withQuietZones() ? 10 : 0);
	auto bitmap = ToMatrix<uint8_t>(symbol);
	auto iv = Image(symbol.width(), symbol.height());
	std::memcpy(const_cast<uint8_t*>(iv.data()), bitmap.data(), iv.width() * iv.height());
	return iv;
}

} // namespace ZXing


#ifdef ZXING_WRITERS

#ifdef ZXING_USE_ZINT
#include "ECI.h"
#include "ReadBarcode.h"

#include <charconv>
#include <zint.h>

namespace ZXing {

struct BarcodeFormatZXing2Zint
{
	BarcodeFormat zxing;
	int zint;
};

static constexpr BarcodeFormatZXing2Zint barcodeFormatZXing2Zint[] = {
	{BarcodeFormat::Aztec, BARCODE_AZTEC},
	{BarcodeFormat::Codabar, BARCODE_CODABAR},
	{BarcodeFormat::Code39, BARCODE_CODE39},
	{BarcodeFormat::Code93, BARCODE_CODE93},
	{BarcodeFormat::Code128, BARCODE_CODE128},
	{BarcodeFormat::DataBar, BARCODE_DBAR_OMN},
	{BarcodeFormat::DataBarExpanded, BARCODE_DBAR_EXP},
	{BarcodeFormat::DataBarLimited, BARCODE_DBAR_LTD},
	{BarcodeFormat::DataMatrix, BARCODE_DATAMATRIX},
	{BarcodeFormat::DXFilmEdge, BARCODE_DXFILMEDGE},
	{BarcodeFormat::EAN8, BARCODE_EANX},
	{BarcodeFormat::EAN13, BARCODE_EANX},
	{BarcodeFormat::ITF, BARCODE_C25INTER},
	{BarcodeFormat::MaxiCode, BARCODE_MAXICODE},
	{BarcodeFormat::MicroQRCode, BARCODE_MICROQR},
	{BarcodeFormat::PDF417, BARCODE_PDF417},
	{BarcodeFormat::QRCode, BARCODE_QRCODE},
	{BarcodeFormat::RMQRCode, BARCODE_RMQR},
	{BarcodeFormat::UPCA, BARCODE_UPCA},
	{BarcodeFormat::UPCE, BARCODE_UPCE},
};

struct String2Int
{
	const char* str;
	int val;
};

static int ParseECLevel(int symbology, std::string_view s)
{
	constexpr std::string_view EC_LABELS_QR[4] = {"L", "M", "Q", "H"};

	int res = 0;
	if (Contains({BARCODE_QRCODE, BARCODE_MICROQR, BARCODE_RMQR}, symbology))
		if ((res = IndexOf(EC_LABELS_QR, s) != -1))
			return res + 1;

	if (std::from_chars(s.data(), s.data() + s.size() - (s.back() == '%'), res).ec != std::errc{})
		throw std::invalid_argument("Invalid ecLevel: '" + std::string(s) + "'");

	auto findClosestECLevel = [](const std::vector<int>& list, int val) {
		int mIdx = -2, mAbs = 100;
		for (int i = 0; i < Size(list); ++i)
		if (int abs = std::abs(val - list[i]); abs < mAbs) {
				mIdx = i;
				mAbs = abs;
		}
		return mIdx + 1;
	};

	if (s.back()=='%'){
		switch (symbology) {
		case BARCODE_QRCODE:
		case BARCODE_MICROQR:
		case BARCODE_RMQR:
			return findClosestECLevel({20, 37, 55, 65}, res);
		case BARCODE_AZTEC:
			return findClosestECLevel({10, 23, 26, 50}, res);
		case BARCODE_PDF417:
			// TODO: do something sensible with PDF417?
		default:
			return -1;
		}
	}

	return res;
};

zint_symbol* CreatorOptions::zint() const
{
	auto& zint = d->zint;

	if (!zint) {
#ifdef PRINT_DEBUG
		printf("zint version: %d, sizeof(zint_symbol): %ld\n", ZBarcode_Version(), sizeof(zint_symbol));
#endif
		zint.reset(ZBarcode_Create());

		auto i = FindIf(barcodeFormatZXing2Zint, [zxing = format()](auto& v) { return v.zxing == zxing; });
		if (i == std::end(barcodeFormatZXing2Zint))
			throw std::invalid_argument("unsupported barcode format: " + ToString(format()));
		zint->symbology = i->zint;

		zint->scale = 0.5f;

		if (!ecLevel().empty())
			zint->option_1 = ParseECLevel(zint->symbology, ecLevel());
	}

	return zint.get();
}

#define CHECK(ZINT_CALL) \
	if (int err = (ZINT_CALL); err >= ZINT_ERROR) \
		throw std::invalid_argument(zint->errtxt);

Barcode CreateBarcode(const void* data, int size, int mode, const CreatorOptions& opts)
{
	auto zint = opts.zint();

	zint->input_mode = mode;
	zint->output_options |= OUT_BUFFER_INTERMEDIATE | BARCODE_QUIET_ZONES;

	if (mode == DATA_MODE && ZBarcode_Cap(zint->symbology, ZINT_CAP_ECI))
		zint->eci = static_cast<int>(ECI::Binary);

	CHECK(ZBarcode_Encode_and_Buffer(zint, (uint8_t*)data, size, 0));

#ifdef PRINT_DEBUG
	printf("create symbol with size: %dx%d\n", zint->width, zint->rows);
#endif

#ifdef ZXING_READERS
	auto buffer = std::vector<uint8_t>(zint->bitmap_width * zint->bitmap_height);
	std::transform(zint->bitmap, zint->bitmap + zint->bitmap_width * zint->bitmap_height, buffer.data(),
				   [](unsigned char v) { return (v == '0') * 0xff; });

	auto res = ReadBarcode({buffer.data(), zint->bitmap_width, zint->bitmap_height, ImageFormat::Lum},
						   ReaderOptions().setFormats(opts.format()).setIsPure(true).setBinarizer(Binarizer::BoolCast));
#else
	//TODO: replace by proper construction from encoded data from within zint
	auto res = Barcode(std::string((const char*)data, size), 0, 0, 0, opts.format(), {});
#endif

	auto bits = BitMatrix(zint->bitmap_width, zint->bitmap_height);
	std::transform(zint->bitmap, zint->bitmap + zint->bitmap_width * zint->bitmap_height, bits.row(0).begin(),
				   [](unsigned char v) { return (v == '1') * BitMatrix::SET_V; });
	res.symbol(std::move(bits));
	res.zint(std::move(opts.d->zint));

	return res;
}

Barcode CreateBarcodeFromText(std::string_view contents, const CreatorOptions& opts)
{
	return CreateBarcode(contents.data(), contents.size(), UNICODE_MODE, opts);
}

#if __cplusplus > 201703L
Barcode CreateBarcodeFromText(std::u8string_view contents, const CreatorOptions& opts)
{
	return CreateBarcode(contents.data(), contents.size(), UNICODE_MODE, opts);
}
#endif

Barcode CreateBarcodeFromBytes(const void* data, int size, const CreatorOptions& opts)
{
	return CreateBarcode(data, size, DATA_MODE, opts);
}

// Writer ========================================================================

struct SetCommonWriterOptions
{
	zint_symbol* zint;

	SetCommonWriterOptions(zint_symbol* zint, const WriterOptions& opts) : zint(zint)
	{
		zint->show_hrt = opts.withHRT();

		zint->output_options &= ~OUT_BUFFER_INTERMEDIATE;
		zint->output_options |= opts.withQuietZones() ? BARCODE_QUIET_ZONES : BARCODE_NO_QUIET_ZONES;

		if (opts.scale())
			zint->scale = opts.scale() / 2.f;
		else if (opts.sizeHint()) {
			int size = std::max(zint->width, zint->rows);
			zint->scale = std::max(1, int(float(opts.sizeHint()) / size)) / 2.f;
		}
	}

	// reset the defaults such that consecutive write calls don't influence each other
	~SetCommonWriterOptions() { zint->scale = 0.5f; }
};

} // ZXing

#else // ZXING_USE_ZINT

#include "MultiFormatWriter.h"
#include "ReadBarcode.h"

namespace ZXing {

zint_symbol* CreatorOptions::zint() const { return nullptr; }

static Barcode CreateBarcode(BitMatrix&& bits, const CreatorOptions& opts)
{
	auto img = ToMatrix<uint8_t>(bits);

	auto res = ReadBarcode({img.data(), img.width(), img.height(), ImageFormat::Lum},
						   ReaderOptions().setFormats(opts.format()).setIsPure(true).setBinarizer(Binarizer::BoolCast));
	res.symbol(std::move(bits));
	return res;
}

Barcode CreateBarcodeFromText(std::string_view contents, const CreatorOptions& opts)
{
	auto writer = MultiFormatWriter(opts.format()).setMargin(0);
	if (!opts.ecLevel().empty())
		writer.setEccLevel(std::stoi(opts.ecLevel()));
	writer.setEncoding(CharacterSet::UTF8); // write UTF8 (ECI value 26) for maximum compatibility

	return CreateBarcode(writer.encode(std::string(contents), 0, IsLinearCode(opts.format()) ? 50 : 0), opts);
}

#if __cplusplus > 201703L
Barcode CreateBarcodeFromText(std::u8string_view contents, const CreatorOptions& opts)
{
	return CreateBarcodeFromText({reinterpret_cast<const char*>(contents.data()), contents.size()}, opts);
}
#endif

Barcode CreateBarcodeFromBytes(const void* data, int size, const CreatorOptions& opts)
{
	std::wstring bytes;
	for (uint8_t c : std::basic_string_view<uint8_t>((uint8_t*)data, size))
		bytes.push_back(c);

	auto writer = MultiFormatWriter(opts.format()).setMargin(0);
	if (!opts.ecLevel().empty())
		writer.setEccLevel(std::stoi(opts.ecLevel()));
	writer.setEncoding(CharacterSet::BINARY);

	return CreateBarcode(writer.encode(bytes, 0, IsLinearCode(opts.format()) ? 50 : 0), opts);
}

} // namespace ZXing

#endif // ZXING_USE_ZINT

#else // ZXING_WRITERS

namespace ZXing {

zint_symbol* CreatorOptions::zint() const { return nullptr; }

Barcode CreateBarcodeFromText(std::string_view, const CreatorOptions&)
{
	throw std::runtime_error("This build of zxing-cpp does not support creating barcodes.");
}

#if __cplusplus > 201703L
Barcode CreateBarcodeFromText(std::u8string_view, const CreatorOptions&)
{
	throw std::runtime_error("This build of zxing-cpp does not support creating barcodes.");
}
#endif

Barcode CreateBarcodeFromBytes(const void*, int, const CreatorOptions&)
{
	throw std::runtime_error("This build of zxing-cpp does not support creating barcodes.");
}

} // namespace ZXing

#endif // ZXING_WRITERS

namespace ZXing {

std::string WriteBarcodeToSVG(const Barcode& barcode, [[maybe_unused]] const WriterOptions& opts)
{
	auto zint = barcode.zint();

	if (!zint)
		return ToSVG(barcode.symbol());

#if defined(ZXING_WRITERS) && defined(ZXING_USE_ZINT)
	auto resetOnExit = SetCommonWriterOptions(zint, opts);

	zint->output_options |= BARCODE_MEMORY_FILE;// | EMBED_VECTOR_FONT;
	strcpy(zint->outfile, "null.svg");

	CHECK(ZBarcode_Print(zint, opts.rotate()));

	return std::string(reinterpret_cast<const char*>(zint->memfile), zint->memfile_size);
#else
	return {}; // unreachable code
#endif
}

Image WriteBarcodeToImage(const Barcode& barcode, [[maybe_unused]] const WriterOptions& opts)
{
	auto zint = barcode.zint();

	if (!zint)
		return ToImage(barcode._symbol->copy(), IsLinearCode(barcode.format()), opts);

#if defined(ZXING_WRITERS) && defined(ZXING_USE_ZINT)
	auto resetOnExit = SetCommonWriterOptions(zint, opts);

	CHECK(ZBarcode_Buffer(zint, opts.rotate()));

#ifdef PRINT_DEBUG
	printf("write symbol with size: %dx%d\n", zint->bitmap_width, zint->bitmap_height);
#endif
	auto iv = Image(zint->bitmap_width, zint->bitmap_height);
	auto* src = zint->bitmap;
	auto* dst = const_cast<uint8_t*>(iv.data());
	for(int y = 0; y < iv.height(); ++y)
		for(int x = 0, w = iv.width(); x < w; ++x, src += 3)
			*dst++ = RGBToLum(src[0], src[1], src[2]);

	return iv;
#else
	return {}; // unreachable code
#endif
}

std::string WriteBarcodeToUtf8(const Barcode& barcode, [[maybe_unused]] const WriterOptions& options)
{
	auto iv = barcode.symbol();
	if (!iv.data())
		return {};

	constexpr auto map = std::array{" ", "\xe2\x96\x80", "\xe2\x96\x84", "\xe2\x96\x88"};
	std::ostringstream res;
	bool inverted = false; // TODO: take from WriterOptions

	for (int y = 0; y < iv.height(); y += 2) {
		for (int x = 0; x < iv.width(); ++x) {
			int tp = bool(*iv.data(x, y)) ^ inverted;
			int bt = (iv.height() == 1 && tp) || (y + 1 < iv.height() && (bool(*iv.data(x, y + 1)) ^ inverted));
			res << map[tp | (bt << 1)];
		}
		res << '\n';
	}

	return res.str();
}

} // namespace ZXing

#endif // ZXING_EXPERIMENTAL_API
