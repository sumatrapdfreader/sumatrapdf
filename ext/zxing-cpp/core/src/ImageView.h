/*
* Copyright 2019 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace ZXing {

enum class ImageFormat : uint32_t
{
	None = 0,
	Lum  = 0x01000000,
	LumA = 0x02000000,
	RGB  = 0x03000102,
	BGR  = 0x03020100,
	RGBA = 0x04000102,
	ARGB = 0x04010203,
	BGRA = 0x04020100,
	ABGR = 0x04030201,
	RGBX [[deprecated("use RGBA")]] = RGBA,
	XRGB [[deprecated("use ARGB")]] = ARGB,
	BGRX [[deprecated("use BGRA")]] = BGRA,
	XBGR [[deprecated("use ABGR")]] = ABGR,
};

constexpr inline int PixStride(ImageFormat format) { return (static_cast<uint32_t>(format) >> 3*8) & 0xFF; }
constexpr inline int RedIndex(ImageFormat format) { return (static_cast<uint32_t>(format) >> 2*8) & 0xFF; }
constexpr inline int GreenIndex(ImageFormat format) { return (static_cast<uint32_t>(format) >> 1*8) & 0xFF; }
constexpr inline int BlueIndex(ImageFormat format) { return (static_cast<uint32_t>(format) >> 0*8) & 0xFF; }

constexpr inline uint8_t RGBToLum(unsigned r, unsigned g, unsigned b)
{
	// .299R + 0.587G + 0.114B (YUV/YIQ for PAL and NTSC),
	// (306*R) >> 10 is approximately equal to R*0.299, and so on.
	// 0x200 >> 10 is 0.5, it implements rounding.
	return static_cast<uint8_t>((306 * r + 601 * g + 117 * b + 0x200) >> 10);
}

/**
 * Simple class that stores a non-owning const pointer to image data plus layout and format information.
 */
class ImageView
{
protected:
	const uint8_t* _data = nullptr;
	ImageFormat _format = ImageFormat::None;
	int _width = 0, _height = 0, _pixStride = 0, _rowStride = 0;

public:
	/** ImageView default constructor creates a 'null' image view
	 */
	ImageView() = default;

	/**
	 * ImageView constructor
	 *
	 * @param data  pointer to image buffer
	 * @param width  image width in pixels
	 * @param height  image height in pixels
	 * @param format  image/pixel format
	 * @param rowStride  optional row stride in bytes, default is width * pixStride
	 * @param pixStride  optional pixel stride in bytes, default is calculated from format
	 */
	ImageView(const uint8_t* data, int width, int height, ImageFormat format, int rowStride = 0, int pixStride = 0)
		: _data(data),
		  _format(format),
		  _width(width),
		  _height(height),
		  _pixStride(pixStride ? pixStride : PixStride(format)),
		  _rowStride(rowStride ? rowStride : width * _pixStride)
	{
		// TODO: [[deprecated]] this check is to prevent exising code from suddenly throwing, remove in 3.0
		if (_data == nullptr && _width == 0 && _height == 0 && rowStride == 0 && pixStride == 0) {
			fprintf(stderr, "zxing-cpp deprecation warning: ImageView(nullptr, ...) will throw in the future, use ImageView()\n");
			return;
		}

		if (_data == nullptr)
			throw std::invalid_argument("Can not construct an ImageView from a NULL pointer");

		if (_width <= 0 || _height <= 0)
			throw std::invalid_argument("Neither width nor height of ImageView can be less or equal to 0");
	}

	/**
	 * ImageView constructor with bounds checking
	 */
	ImageView(const uint8_t* data, int size, int width, int height, ImageFormat format, int rowStride = 0, int pixStride = 0)
		: ImageView(data, width, height, format, rowStride, pixStride)
	{
		if (_rowStride < 0 || _pixStride < 0 || size < _height * _rowStride)
			throw std::invalid_argument("ImageView parameters are inconsistent (out of bounds)");
	}

	int width() const { return _width; }
	int height() const { return _height; }
	int pixStride() const { return _pixStride; }
	int rowStride() const { return _rowStride; }
	ImageFormat format() const { return _format; }

	const uint8_t* data() const { return _data; }
	const uint8_t* data(int x, int y) const { return _data + y * _rowStride + x * _pixStride; }

	ImageView cropped(int left, int top, int width, int height) const
	{
		left   = std::clamp(left, 0, _width - 1);
		top    = std::clamp(top, 0, _height - 1);
		width  = width <= 0 ? (_width - left) : std::min(_width - left, width);
		height = height <= 0 ? (_height - top) : std::min(_height - top, height);
		return {data(left, top), width, height, _format, _rowStride, _pixStride};
	}

	ImageView rotated(int degree) const
	{
		switch ((degree + 360) % 360) {
		case 90:  return {data(0, _height - 1), _height, _width, _format, _pixStride, -_rowStride};
		case 180: return {data(_width - 1, _height - 1), _width, _height, _format, -_rowStride, -_pixStride};
		case 270: return {data(_width - 1, 0), _height, _width, _format, -_pixStride, _rowStride};
		}
		return *this;
	}

	ImageView subsampled(int scale) const
	{
		return {_data, _width / scale, _height / scale, _format, _rowStride * scale, _pixStride * scale};
	}

};

class Image : public ImageView
{
	std::unique_ptr<uint8_t[]> _memory;
	Image(std::unique_ptr<uint8_t[]>&& data, int w, int h, ImageFormat f) : ImageView(data.get(), w, h, f), _memory(std::move(data)) {}

public:
	Image() = default;
	Image(int w, int h, ImageFormat f = ImageFormat::Lum) : Image(std::make_unique<uint8_t[]>(w * h * PixStride(f)), w, h, f) {}
};

} // ZXing

