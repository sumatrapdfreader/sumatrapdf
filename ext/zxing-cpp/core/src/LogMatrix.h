/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitMatrix.h"
#include "Matrix.h"
#include "Point.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

namespace ZXing {

#ifdef PRINT_DEBUG

class LogMatrix
{
	using LogBuffer = Matrix<uint8_t>;
	LogBuffer _log;
	const BitMatrix* _image = nullptr;
	int _scale = 1;

public:
	void init(const BitMatrix* image, int scale = 1)
	{
		_image = image;
		_scale = scale;
		_log = LogBuffer(_image->width() * _scale, _image->height() * _scale);
	}

	void write(const char* fn)
	{
		assert(_image);
		FILE* f = fopen(fn, "wb");

		// Write PPM header, P5 == grey, P6 == rgb
		fprintf(f, "P6\n%d %d\n255\n", _log.width(), _log.height());

		// Write pixels
		for (int y = 0; y < _log.height(); ++y)
			for (int x = 0; x < _log.width(); ++x) {
				unsigned char pix[3];
				unsigned char &r = pix[0], &g = pix[1], &b = pix[2];
				r = g = b = _image->get(x / _scale, y / _scale) ? 0 : 255;
				if (_scale > 1 && x % _scale == _scale / 2 && y % _scale == _scale / 2)
					r = g = b = r ? 230 : 50;
				switch (_log.get(x, y)) {
				case 1: r = g = b = _scale > 1 ? 128 : (r ? 230 : 50); break;
				case 2: r = b = 50, g = 220; break;
				case 3: g = r = 100, b = 250; break;
				case 4: g = b = 100, r = 250; break;
				}
				fwrite(&pix, 3, 1, f);
			}
		fclose(f);
	}

	template <typename T>
	void operator()(const PointT<T>& p, int color = 1)
	{
		if (_image && _image->isIn(p))
			_log.set(static_cast<int>(p.x * _scale), static_cast<int>(p.y * _scale), color);
	}

	void operator()(const PointT<int>& p, int color)
	{
		operator()(centered(p), color);
	}

	template <typename T>
	void operator()(const std::vector<PointT<T>>& points, int color = 2)
	{
		for (auto p : points)
			operator()(p, color);
	}
};

extern LogMatrix log;

class LogMatrixWriter
{
	LogMatrix &log;
	std::string fn;

public:
	LogMatrixWriter(LogMatrix& log, const BitMatrix& image, int scale, std::string fn) : log(log), fn(fn)
	{
		log.init(&image, scale);
	}
	~LogMatrixWriter() { log.write(fn.c_str()); }
};

#else

template<typename T> void log(PointT<T>, int = 0) {}

#endif

} // namespace ZXing
