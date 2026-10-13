/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Point.h"
#include "ZXAlgorithms.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef PRINT_DEBUG
#include <cstdio>
#endif

namespace ZXing {

class RegressionLine
{
protected:
	std::vector<PointF> _points;
	PointF _directionInward;
	PointF::value_t a = NAN, b = NAN, c = NAN;

	friend PointF intersect(const RegressionLine& l1, const RegressionLine& l2);

	template<typename T> bool evaluate(const PointT<T>* begin, const PointT<T>* end)
	{
		auto mean = Reduce(begin, end, PointF()) / std::distance(begin, end);
		PointF::value_t sumXX = 0, sumYY = 0, sumXY = 0;
		for (auto p = begin; p != end; ++p) {
			auto d = *p - mean;
			sumXX += d.x * d.x;
			sumYY += d.y * d.y;
			sumXY += d.x * d.y;
		}
		if (sumYY >= sumXX) {
			auto l = std::sqrt(sumYY * sumYY + sumXY * sumXY);
			a = +sumYY / l;
			b = -sumXY / l;
		} else {
			auto l = std::sqrt(sumXX * sumXX + sumXY * sumXY);
			a = +sumXY / l;
			b = -sumXX / l;
		}
		if (dot(_directionInward, normal()) < 0) {
			a = -a;
			b = -b;
		}
		c = dot(normal(), mean); // (a*mean.x + b*mean.y);
		return dot(_directionInward, normal()) > 0.5f; // angle between original and new direction is at most 60 degree
	}

	template <typename T> bool evaluate(const std::vector<PointT<T>>& points) { return evaluate(&points.front(), &points.back() + 1); }

	template <typename T> static auto distance(PointT<T> a, PointT<T> b) { return ZXing::distance(a, b); }

public:
	RegressionLine() { _points.reserve(16); } // arbitrary but plausible start size (tiny performance improvement)

	template<typename T> RegressionLine(PointT<T> a, PointT<T> b)
	{
		evaluate(std::vector{a, b});
	}

	template<typename T> RegressionLine(const PointT<T>* b, const PointT<T>* e)
	{
		evaluate(b, e);
	}

	const auto& points() const { return _points; }
	int length() const { return _points.size() >= 2 ? int(distance(_points.front(), _points.back())) : 0; }
	bool isValid() const { return !std::isnan(a); }
	PointF normal() const { return isValid() ? PointF(a, b) : _directionInward; }
	auto signedDistance(PointF p) const { return dot(normal(), p) - c; }
	template <typename T> auto distance(PointT<T> p) const { return std::abs(signedDistance(PointF(p))); }
	PointF project(PointF p) const { return p - signedDistance(p) * normal(); }
	PointF centroid() const { return Reduce(_points) / _points.size(); }

	void reset()
	{
		_points.clear();
		_directionInward = {};
		a = b = c = NAN;
	}

	void add(PointF p) {
		assert(_directionInward != PointF());
		_points.push_back(p);
		if (_points.size() == 1)
			c = dot(normal(), p);
	}

	void pop_back() { _points.pop_back(); }
	void pop_front()
	{
		std::rotate(_points.begin(), _points.begin() + 1, _points.end());
		_points.pop_back();
	}

	void setDirectionInward(PointF d) { _directionInward = normalized(d); }

	bool evaluate(double maxSignedDist = -1, bool updatePoints = false)
	{
		bool ret = evaluate(_points);
		if (maxSignedDist > 0) {
			auto points = _points;
			while (true) {
				auto old_points_size = points.size();
				// remove points that are further 'inside' than maxSignedDist or further 'outside' than 2 x maxSignedDist
#ifdef __cpp_lib_erase_if
				std::erase_if(points, [this, maxSignedDist](auto p) {
					auto sd = this->signedDistance(p);
					return sd > maxSignedDist || sd < -2 * maxSignedDist;
				});
#else
				auto end = std::remove_if(points.begin(), points.end(), [this, maxSignedDist](auto p) {
					auto sd = this->signedDistance(p);
					return sd > maxSignedDist || sd < -2 * maxSignedDist;
				});
				points.erase(end, points.end());
#endif
				// if we threw away too many points, something is off with the line to begin with
				if (points.size() < old_points_size / 2 || points.size() < 2)
					return false;
				if (old_points_size == points.size())
					break;
#ifdef PRINT_DEBUG
				printf("removed %zu points -> %zu remaining\n", old_points_size - points.size(), points.size());
				fflush(stdout);
#endif
				ret = evaluate(points);
			}

			if (updatePoints)
				_points = std::move(points);
		}
		return ret;
	}

	bool isHighRes() const
	{
		PointF min = _points.front(), max = _points.front();
		for (auto p : _points) {
			UpdateMinMax(min.x, max.x, p.x);
			UpdateMinMax(min.y, max.y, p.y);
		}
		auto diff  = max - min;
		auto len   = maxAbsComponent(diff);
		auto steps = std::min(std::abs(diff.x), std::abs(diff.y));
		// due to aliasing we get bad extrapolations if the line is short and too close to vertical/horizontal
		return steps > 2 || len > 50;
	}
};

inline PointF intersect(const RegressionLine& l1, const RegressionLine& l2)
{
	assert(l1.isValid() && l2.isValid());
	auto d = l1.a * l2.b - l1.b * l2.a;
	auto x = (l1.c * l2.b - l1.b * l2.c) / d;
	auto y = (l1.a * l2.c - l1.c * l2.a) / d;
	return {x, y};
}

} // ZXing

