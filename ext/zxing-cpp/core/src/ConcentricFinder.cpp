/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ConcentricFinder.h"

#include "LogMatrix.h"
#include "RegressionLine.h"
#include "ZXAlgorithms.h"

namespace ZXing {

std::optional<PointF> AverageEdgePixels(BitMatrixCursorI cur, int range, int numOfEdges)
{
	PointF sum = {};
	for (int i = 0; i < numOfEdges; ++i) {
		if (!cur.isIn())
			return {};
		cur.stepToEdge(1, range);
		sum += centered(cur.p) + centered(cur.p + cur.back());
		log(cur.p + cur.back(), 2);
	}
	return sum / (2 * numOfEdges);
}

std::optional<PointF> CenterOfDoubleCross(const BitMatrix& image, PointI center, int range, int numOfEdges)
{
	PointF sum = {};
	for (auto d : {PointI{0, 1}, {1, 0}, {1, 1}, {1, -1}}) {
		auto avr1 = AverageEdgePixels({image, center,  d}, range, numOfEdges);
		auto avr2 = AverageEdgePixels({image, center, -d}, range, numOfEdges);
		if (!avr1 || !avr2)
			return {};
		sum += *avr1 + *avr2;
	}
	return sum / 8;
}

std::optional<PointF> CenterOfRing(const BitMatrix& image, PointI center, int range, int nth, bool requireCircle)
{
#if 0
	if (requireCircle) {
		// alternative implementation with the aim of discarding closed loops that are not all circle like (M > 5*m)
		auto points = CollectRingPoints(image, center, range, std::abs(nth), nth < 0);
		if (points.empty())
			return {};
		auto res = Reduce(points, PointF{}, std::plus{}) / Size(points);

		double m = range, M = 0;
		for (auto p : points)
			UpdateMinMax(m, M, maxAbsComponent(p - res));

		if (M > 5 * m)
			return {};

		return res;
	}
#endif
	// range is the approximate width/height of the nth ring, if nth>1 then it would be plausible to limit the search radius
	// to approximately range / 2 * sqrt(2) == range * 0.75 but it turned out to be too limiting with realworld/noisy data.
	int radius = range;
	bool inner = nth < 0;
	nth = std::abs(nth);
	log(center, 3);
	BitMatrixCursorI cur(image, center, {0, 1});
	if (!cur.stepToEdge(nth, radius, inner))
		return {};
	cur.turnRight(); // move clock wise and keep edge on the right/left depending on backup
	const auto edgeDir = inner ? Direction::LEFT : Direction::RIGHT;

	uint32_t neighbourMask = 0;
	auto start = cur.p;
	PointF sum = {};
	int n = 0;
	do {
		log(cur.p, 4);
		sum += centered(cur.p);
		++n;

		// find out if we come full circle around the center. 8 bits have to be set in the end.
		neighbourMask |= (1 << (4 + dot(bresenhamDirection(cur.p - center), PointI(1, 3))));

		if (!cur.stepAlongEdge(edgeDir))
			return {};

		// use L-inf norm, simply because it is a lot faster than L2-norm and sufficiently accurate
		if (maxAbsComponent(cur.p - center) > radius || center == cur.p || n > 4 * 2 * range)
			return {};
	} while (cur.p != start);

	if (requireCircle && neighbourMask != 0b111101111)
		return {};

	return sum / n;
}

std::optional<PointF> CenterOfRings(const BitMatrix& image, PointF center, int range, int numOfRings)
{
	int n = 1;
	PointF sum = center;
	for (int i = 2; i < numOfRings + 1; ++i) {
		auto c = CenterOfRing(image, PointI(center), range, i);
		if (!c) {
			if (n == 1)
				return {};
			else
				return sum / n;
		} else if (distance(*c, center) > range / numOfRings / 2) {
			return {};
		}

		sum += *c;
		n++;
	}
	return sum / n;
}

static std::vector<PointF> CollectRingPoints(const BitMatrix& image, PointF center, int range, int edgeIndex, bool backup)
{
	PointI centerI(center);
	int radius = range;
	BitMatrixCursorI cur(image, centerI, {0, 1});
	if (!cur.stepToEdge(edgeIndex, radius, backup))
		return {};
	cur.turnRight(); // move clock wise and keep edge on the right/left depending on backup
	const auto edgeDir = backup ? Direction::LEFT : Direction::RIGHT;

	uint32_t neighbourMask = 0;
	auto start = cur.p;
	std::vector<PointF> points;
	points.reserve(4 * range);

	do {
		log(cur.p, 4);
		points.push_back(centered(cur.p));

		// find out if we come full circle around the center. 8 bits have to be set in the end.
		neighbourMask |= (1 << (4 + dot(bresenhamDirection(cur.p - centerI), PointI(1, 3))));

		if (!cur.stepAlongEdge(edgeDir))
			return {};

		// use L-inf norm, simply because it is a lot faster than L2-norm and sufficiently accurate
		if (maxAbsComponent(cur.p - centerI) > radius || centerI == cur.p || Size(points) > 4 * 2 * range)
			return {};

	} while (cur.p != start);

	if (neighbourMask != 0b111101111)
		return {};

	return points;
}

static std::optional<QuadrilateralF> FitQadrilateralToPoints(PointF center, std::vector<PointF>& points)
{
	auto dist2Center = [c = center](auto a, auto b) { return distance(a, c) < distance(b, c); };
	// rotate points such that the first one is the furthest away from the center (hence, a corner)
	std::rotate(points.begin(), std::max_element(points.begin(), points.end(), dist2Center), points.end());

	std::array<const PointF*, 4> corners;
	corners[0] = &points[0];
	// find the oposite corner by looking for the farthest point near the oposite point
	corners[2] = std::max_element(&points[Size(points) * 3 / 8], &points[Size(points) * 5 / 8], dist2Center);

	// find the two in between corners by looking for the points farthest from the long diagonal
	auto dist2Diagonal = [l = RegressionLine(*corners[0], *corners[2])](auto a, auto b) { return l.distance(a) < l.distance(b); };
	corners[1] = std::max_element(&points[Size(points) * 1 / 8], &points[Size(points) * 3 / 8], dist2Diagonal);
	corners[3] = std::max_element(&points[Size(points) * 5 / 8], &points[Size(points) * 7 / 8], dist2Diagonal);

	std::array lines{RegressionLine{corners[0] + 1, corners[1]}, RegressionLine{corners[1] + 1, corners[2]},
					 RegressionLine{corners[2] + 1, corners[3]}, RegressionLine{corners[3] + 1, &points.back() + 1}};

	if (std::any_of(lines.begin(), lines.end(), [](auto line) { return !line.isValid(); }))
		return {};

	std::array<const PointF*, 4> beg = {corners[0] + 1, corners[1] + 1, corners[2] + 1, corners[3] + 1};
	std::array<const PointF*, 4> end = {corners[1], corners[2], corners[3], &points.back() + 1};

	// check if all points belonging to each line segment are sufficiently close to that line
	for (int i = 0; i < 4; ++i)
		for (const PointF* p = beg[i]; p != end[i]; ++p) {
			auto len = std::distance(beg[i], end[i]);
			if (len > 3 && lines[i].distance(*p) > std::max(1., std::min(8., len / 8.))) {
#ifdef PRINT_DEBUG
				printf("%d: %.2f > %.2f @ %.fx%.f\n", i, lines[i].distance(*p), std::distance(beg[i], end[i]) / 1., p->x, p->y);
#endif
				return {};
			}
		}

	QuadrilateralF res;
	for (int i = 0; i < 4; ++i)
		res[i] = intersect(lines[i], lines[(i + 1) % 4]);

	return res;
}

static bool QuadrilateralIsPlausibleSquare(const QuadrilateralF q, int lineIndex)
{
	double m, M;
	m = M = distance(q[0], q[3]);
	for (int i = 1; i < 4; ++i)
		UpdateMinMax(m, M, distance(q[i - 1], q[i]));

	return m >= lineIndex * 2 && m > M / 3;
}

static std::optional<QuadrilateralF> FitSquareToPoints(const BitMatrix& image, PointF center, int range, int lineIndex, bool backup)
{
	auto points = CollectRingPoints(image, center, range, lineIndex, backup);
	if (points.empty())
		return {};

	auto res = FitQadrilateralToPoints(center, points);
	if (!res || !QuadrilateralIsPlausibleSquare(*res, lineIndex - backup))
		return {};

	return res;
}

std::optional<QuadrilateralF> FindConcentricPatternCorners(const BitMatrix& image, PointF center, int range, int lineIndex)
{
	auto innerCorners = FitSquareToPoints(image, center, range, lineIndex, false);
	if (!innerCorners)
		return {};

	auto outerCorners = FitSquareToPoints(image, center, range, lineIndex + 1, true);
	if (!outerCorners)
		return {};

	auto res = Blend(*innerCorners, *outerCorners);

	for (auto p : *innerCorners)
		log(p, 3);

	for (auto p : *outerCorners)
		log(p, 3);

	for (auto p : res)
		log(p, 3);

	return res;
}

std::optional<PointF> FinetuneConcentricPatternCenter(const BitMatrix& image, PointF center, int range, int finderPatternSize)
{
	// make sure we have at least one path of white around the center
	if (auto res1 = CenterOfRing(image, PointI(center), range, 1); res1 && image.get(*res1)) {
		// and then either at least one more ring around that
		if (auto res2 = CenterOfRings(image, *res1, range, finderPatternSize / 2); res2 && image.get(*res2))
			return res2;
		// or the center can be approximated by a square
		if (FitSquareToPoints(image, *res1, range, 1, false))
			return res1;
		// TODO: this is currently only keeping #258 alive, evaluate if still worth it
		if (auto res2 = CenterOfDoubleCross(image, PointI(*res1), range, finderPatternSize / 2 + 1); res2 && image.get(*res2))
			return res2;
	}
	return {};
}

} // ZXing
