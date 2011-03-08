/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef GeomUtil_h
#define GeomUtil_h

typedef struct RectI {
    int x, y;
    int dx, dy;
} RectI;

typedef struct RectD {
    double x,y;
    double dx,dy;
} RectD;

int    RectI_Intersect(RectI *r1, RectI *r2, RectI *rIntersectOut);
void   RectI_FromXY(RectI *rOut, int xs, int xe, int ys, int ye);
int    RectI_Inside(RectI *r, int x, int y);
RectI  RectI_Union(RectI a, RectI b);
int    RectD_FromXY(RectD *rOut, double xs, double xe,  double ys, double ye);
void   RectD_FromRectI(RectD *rOut, const RectI *rIn);
void   RectI_FromRectD(RectI *rOut, const RectD *rIn);
RECT   RECT_FromRectI(RectI *rIn);
RectI  RectI_FromRECT(RECT *rIn);
int    RectD_Inside(RectD *r, double x, double y);
void   u_RectI_Intersect(void);

class PointI {
public:
    PointI() { x = 0; y = 0; }
    PointI(int _x, int _y) { x = _x; y = _y; }
    void set(int _x, int _y) { x = _x; y = _y; }
    int x;
    int y;
};

class PointD {
public:
    PointD() { x = 0; y = 0; }
    PointD(double _x, double _y) { x = _x; y = _y; }
    void set(double _x, double _y) { x = _x; y = _y; }
    double x;
    double y;
};

class SizeI {
public:
    SizeI() { dx = 0; dy = 0; }
    SizeI(int _dx, int _dy) { dx = _dx; dy = _dy; }
    void set(int _dx, int _dy) { dx = _dx; dy = _dy; }
    int dx;
    int dy;
};

class SizeD {
public:
    SizeD(double dx, double dy) { m_dx = dx; m_dy = dy; }
    SizeD(int dx, int dy) { m_dx = (double)dx; m_dy = (double)dy; }
    SizeD(SizeI si) { m_dx = (double)si.dx; m_dy = (double)si.dy; }
    SizeD() { m_dx = 0; m_dy = 0; }
    int dxI() const { return (int)m_dx; }
    int dyI() const { return (int)m_dy; }
    double dx() const { return m_dx; }
    double dy() const { return m_dy; }
    void setDx(double dx) { m_dx = dx; }
    void setDy(double dy) { m_dy = dy; }
    SizeI size() { return SizeI((int)dx(), (int)dy()); }
private:
    double m_dx;
    double m_dy;
};

#endif
