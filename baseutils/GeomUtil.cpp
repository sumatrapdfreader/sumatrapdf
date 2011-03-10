/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "GeomUtil.h"

/* Return true if 'r1' and 'r2' intersect and optionally
      put the intersect area into 'rIntersectOut'.
   Return false if there is no intersection. */
int RectI_Intersect(RectI *r1, RectI *r2, RectI *rIntersectOut)
{
    assert(r1 && r2);
    if (!r1 || !r2)
        return 0;

    /* { } visualizes r1 and | | visualizes r2 */

    /* case of non-overlapping rectangles i.e.:
       { }   | | */
    if (r2->x > r1->x + r1->dx)
        return 0;
    /* | |   { } */
    if (r1->x > r2->x + r2->dx)
        return 0;
    /* partially overlapped i.e.:
       {  |  } |   or   |  {  |  }
       and one inside the other i.e.:
       {  | |  }   or   |  {  }  |

       In these cases, the intersection starts with the larger of the start coordinates
       and ends with the smaller of the end coordinates (these are only calculated,
       when the intersection rectangle is actually requested) */

    /* the logic for y is the same */
    if (r2->y > r1->y + r1->dy)
        return 0;
    if (r1->y > r2->y + r2->dy)
        return 0;

    if (rIntersectOut) {
        int xIntersectS = max(r1->x, r2->x);
        int xIntersectE = min(r1->x + r1->dx, r2->x + r2->dx);
        int yIntersectS = max(r1->y, r2->y);
        int yIntersectE = min(r1->y + r1->dy, r2->y + r2->dy);

        RectI_FromXY(rIntersectOut, xIntersectS, xIntersectE, yIntersectS, yIntersectE);
    }
    return 1;
}

void RectI_FromXY(RectI *rOut, int xs, int xe, int ys, int ye)
{
    assert(rOut);
    if (!rOut)
        return;
    assert(xs <= xe);
    assert(ys <= ye);
    rOut->x = xs;
    rOut->y = ys;
    rOut->dx = xe - xs;
    rOut->dy = ye - ys;
}

void RectD_FromRectI(RectD *rOut, const RectI *rIn)
{
    rOut->x = (double)rIn->x;
    rOut->y = (double)rIn->y;
    rOut->dx = (double)rIn->dx;
    rOut->dy = (double)rIn->dy;
}

int intFromDouble(double d) {
    double i1 = (int) d;
    double i2 = i1 + 1;

    if (d - i1 < i2 - d)
        return (int)i1;
    else
        return (int)i2;
}

void RectI_FromRectD(RectI *rOut, const RectD *rIn)
{
    rOut->x = intFromDouble(rIn->x);
    rOut->y = intFromDouble(rIn->y);
    rOut->dx = intFromDouble(rIn->dx);
    rOut->dy = intFromDouble(rIn->dy);
}

RECT RECT_FromRectI(RectI *rIn)
{
    RECT rOut;
    SetRect(&rOut, rIn->x, rIn->y, rIn->x + rIn->dx, rIn->y + rIn->dy);
    return rOut;
}

RectI RectI_FromRECT(RECT *rIn)
{
    RectI rOut = { rIn->left, rIn->top, rIn->right - rIn->left, rIn->bottom - rIn->top };
    return rOut;
}

int RectD_FromXY(RectD *rOut, double xs, double xe, double ys, double ye)
{
    assert(rOut);
    if (!rOut)
        return 0;
    if (xs > xe)
        swap_double(xs, xe);
    if (ys > ye)
        swap_double(ys, ye);

    rOut->x = xs;
    rOut->y = ys;
    rOut->dx = xe - xs;
    assert(rOut->dx >= 0.0);
    rOut->dy = ye - ys;
    assert(rOut->dy >= 0.0);
    return 1;
}

/* Return TRUE if point 'x'/'y' is inside rectangle 'r' */
int RectI_Inside(RectI *r, int x, int y)
{
    if (x < r->x)
        return FALSE;
    if (x > r->x + r->dx)
        return FALSE;
    if (y < r->y)
        return FALSE;
    if (y > r->y + r->dy)
        return FALSE;
    return TRUE;
}

int RectD_Inside(RectD *r, double x, double y)
{
    if (x < r->x)
        return FALSE;
    if (x > r->x + r->dx)
        return FALSE;
    if (y < r->y)
        return FALSE;
    if (y > r->y + r->dy)
        return FALSE;
    return TRUE;
}

RectI RectI_Union(RectI a, RectI b)
{
    RectI u;
    if (a.dy <= 0. && a.dx <= 0.) return b;
    if (b.dy <= 0. && b.dx <= 0.) return a;
    u.x = min(a.x, b.x);
    u.y = min(a.y, b.y);
    u.dx = max(a.x + a.dx, b.x + b.dy) - u.x;
    u.dy = max(a.y + a.dy, b.y + b.dy) - u.y;
    return u;
}


#ifndef NDEBUG
void RectI_AssertEqual(RectI *rIntersect, RectI *rExpected)
{
    assert(memcmp(rIntersect, rExpected, sizeof(RectI)) == 0);
}

void u_RectI_Intersect(void)
{
    int         i, dataLen;
    RectI       r1, r2, rIntersect, rExpected, rExpectedSwaped;
    int         doIntersect, doIntersectExpected;

    struct SRIData {
        int     x1s, x1e, y1s, y1e;
        int     x2s, x2e, y2s, y2e;
        int     intersect;
        int     i_xs, i_xe, i_ys, i_ye;
    } testData[] = {
        { 0,10, 0,10,   0,10, 0,10,  1,  0,10, 0,10 }, /* complete intersect */
        { 0,10, 0,10,  20,30,20,30,  0,  0, 0, 0, 0 }, /* no intersect */
        { 0,10, 0,10,   5,15, 0,10,  1,  5,10, 0,10 }, /* { | } | */
        { 0,10, 0,10,   5, 7, 0,10,  1,  5, 7, 0,10 }, /* { | | } */

        { 0,10, 0,10,   5, 7, 5, 7,  1,  5, 7, 5, 7 },
        { 0,10, 0,10,   5, 15,5,15,  1,  5,10, 5,10 },
    };
    dataLen = dimof(testData);
    for (i = 0; i < dataLen; i++) {
        struct SRIData *curr;
        curr = &(testData[i]);
        RectI_FromXY(&rExpected, curr->i_xs, curr->i_xe, curr->i_ys, curr->i_ye);
        RectI_FromXY(&rExpectedSwaped, curr->i_ys, curr->i_ye, curr->i_xs, curr->i_xe);

        RectI_FromXY(&r1, curr->x1s, curr->x1e, curr->y1s, curr->y1e);
        RectI_FromXY(&r2, curr->x2s, curr->x2e, curr->y2s, curr->y2e);
        doIntersectExpected = curr->intersect;

        doIntersect = RectI_Intersect(&r1, &r2, &rIntersect);
        assert(doIntersect == doIntersectExpected);
        if (doIntersect)
            RectI_AssertEqual(&rIntersect, &rExpected);

        /* if we swap rectangles, the results should be the same */
        RectI_FromXY(&r2, curr->x1s, curr->x1e, curr->y1s, curr->y1e);
        RectI_FromXY(&r1, curr->x2s, curr->x2e, curr->y2s, curr->y2e);
        doIntersect = RectI_Intersect(&r1, &r2, &rIntersect);
        assert(doIntersect == doIntersectExpected);
        if (doIntersect)
            RectI_AssertEqual(&rIntersect, &rExpected);

        /* if we swap x with y coordinates in a rectangle, results should be the same */
        RectI_FromXY(&r1, curr->y1s, curr->y1e, curr->x1s, curr->x1e);
        RectI_FromXY(&r2, curr->y2s, curr->y2e, curr->x2s, curr->x2e);
        doIntersect = RectI_Intersect(&r1, &r2, &rIntersect);
        assert(doIntersect == doIntersectExpected);
        if (doIntersect)
            RectI_AssertEqual(&rIntersect, &rExpectedSwaped);

        /* swap both rectangles and x with y, results should be the same */
        RectI_FromXY(&r2, curr->y1s, curr->y1e, curr->x1s, curr->x1e);
        RectI_FromXY(&r1, curr->y2s, curr->y2e, curr->x2s, curr->x2e);
        doIntersect = RectI_Intersect(&r1, &r2, &rIntersect);
        assert(doIntersect == doIntersectExpected);
        if (doIntersect)
            RectI_AssertEqual(&rIntersect, &rExpectedSwaped);
    }
}
#endif

