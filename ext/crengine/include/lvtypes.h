/** \file lvtypes.h
    \brief CREngine common types definition

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.
    See LICENSE file for details.
*/

#ifndef LVTYPES_H_INCLUDED
#define LVTYPES_H_INCLUDED

#include <stdlib.h>
#include "crsetup.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifdef _WIN32
typedef long lInt32;            ///< signed 32 bit int
typedef unsigned long lUInt32;  ///< unsigned 32 bit int
#else
typedef int lInt32;            ///< signed 32 bit int
typedef unsigned int lUInt32;  ///< unsigned 32 bit int
#endif

typedef short int lInt16;           ///< signed 16 bit int
typedef unsigned short int lUInt16; ///< unsigned 16 bit int

typedef signed char lInt8;          ///< signed 8 bit int
typedef unsigned char lUInt8;       ///< unsigned 8 bit int

typedef wchar_t lChar16;            ///< 16 bit char
typedef char lChar8;                ///< 8 bit char

#if defined(_WIN32) && !defined(CYGWIN)
typedef __int64 lInt64;             ///< signed 64 bit int
typedef unsigned __int64 lUInt64;   ///< unsigned 64 bit int
#else
typedef long long int lInt64;       ///< signed 64 bit int
typedef unsigned long long int lUInt64; ///< unsigned 64 bit int
#endif

/// platform-dependent path separator
#if defined(_WIN32) && !defined(__WINE__)
#define PATH_SEPARATOR_CHAR '\\'
#elif __SYMBIAN32__
#define PATH_SEPARATOR_CHAR '\\'
#else
#define PATH_SEPARATOR_CHAR '/'
#endif

/// point
class lvPoint {
public:
    int x;
    int y;
    lvPoint() : x(0), y(0) { }
    lvPoint(int nx, int ny) : x(nx), y(ny) { }
    lvPoint( const lvPoint & v ) : x(v.x), y(v.y) { }
    lvPoint & operator = ( const lvPoint & v ) { x = v.x; y = v.y; return *this; }
};

/// rectangle
class lvRect {
public:
    int left;
    int top;
    int right;
    int bottom;
    /// returns true if rectangle is empty
    bool isEmpty() const { return left>=right || bottom<=top; }
    lvRect() : left(0), top(0), right(0), bottom(0) { }
    lvRect( int x0, int y0, int x1, int y1) : left(x0), top(y0), right(x1), bottom(y1) { }
    lvPoint topLeft() const { return lvPoint( left, top ); }
    lvPoint bottomRight() const { return lvPoint( right, bottom ); }
    void setTopLeft( const lvPoint & pt ) { top=pt.y; left=pt.x; }
    void setBottomRight( const lvPoint & pt ) { bottom=pt.y; right=pt.x; }
    /// returns true if rectangles are equal
    bool operator ==( const lvRect & rc ) const
    {
        return rc.left == left && rc.right == right && rc.top == top && rc.bottom == bottom;
    }

    /// returns rectangle width
    int width() const { return right - left; }
    /// returns rectangle height
    int height() const { return bottom - top; }
    lvPoint size() const { return lvPoint(right-left, bottom - top); }
    void shrink( int delta ) { left+=delta; right-=delta; top+=delta; bottom-=delta; }
    void shrinkBy( const lvRect & rc ) { left+=rc.left; right-=rc.right; top+=rc.top; bottom-=rc.bottom; }
    void extend( int delta ) { shrink(-delta); }
    void extendBy( const lvRect & rc ) { left-=rc.left; right+=rc.right; top-=rc.top; bottom+=rc.bottom; }
    /// makes this rect to cover both this and specified rect (bounding box for two rectangles)
    void extend( lvRect rc )
    {
        if ( rc.isEmpty() )
            return;
        if ( isEmpty() ) {
            left = rc.left;
            top = rc.top;
            right = rc.right;
            bottom = rc.bottom;
            return;
        }
        if ( left > rc.left )
            left = rc.left;
        if ( top > rc.top )
            top = rc.top;
        if ( right < rc.right )
            right = rc.right;
        if ( bottom < rc.bottom )
            bottom = rc.bottom;
    }
    /// returns true if specified rectangle is fully covered by this rectangle
    bool isRectInside( lvRect rc ) const
    {
        if ( rc.isEmpty() || isEmpty() )
            return false;
        if ( rc.left < left || rc.right > right || rc.top < top || rc.bottom > bottom )
            return false;
        return true;
    }
    /// returns true if point is inside this rectangle
    bool isPointInside ( lvPoint & pt ) const 
    {
        return left<=pt.x && top<=pt.y && right>pt.x && bottom > pt.y;
    }
	void clear() { left=right=top=bottom=0; }
	
	bool intersect (const lvRect &rc) 
	{
		bool ret = true;
		if (rc.left <= left) {
			if (rc.right <= left) 
				ret = false;
		} else if (rc.left < right) {
			left = rc.left;
		} else 
			ret = false;
		if (ret) {
			if (rc.right < right)
				right = rc.right;
			if (rc.top <= top) {
				if (rc.bottom <= top) 
					ret = false;
			} else if (rc.top < bottom) {
				top = rc.top;
			} else 
				ret = false;
			if (ret && rc.bottom < bottom)
				bottom = rc.bottom;
		}
		if (!ret)
			clear();
		return ret;
	}
};

class lvColor
{
    lUInt32 value;
public:
    lvColor( lUInt32 cl ) : value(cl) { }
    lvColor(  lUInt32 r, lUInt32 g, lUInt32 b ) : value(((r&255)<<16) | ((g&255)<<8) | (b&255)) { }
    lvColor( lUInt32 r, lUInt32 g, lUInt32 b, lUInt32 a ) : value(((a&255)<<24) | ((r&255)<<16) | ((g&255)<<8) | (b&255)) { }
    operator lUInt32 () const { return value; }
    lUInt32 get() const { return value; }
    lUInt8 r() const { return (lUInt8)(value>>16)&255; }
    lUInt8 g() const { return (lUInt8)(value>>8)&255; }
    lUInt8 b() const { return (lUInt8)(value)&255; }
    lUInt8 a() const { return (lUInt8)(value>>24)&255; }
};

/// byte order convertor
class lvByteOrderConv {
    bool _lsf;
public:
    lvByteOrderConv()
    {
        union {
            lUInt16 word;
            lUInt8 bytes[2];
        } test;
        test.word = 1;
        _lsf = test.bytes[0]!=0;
    }
    /// reverse 32 bit word
    inline static lUInt32 rev( lUInt32 w )
    {
        return
            ((w&0xFF000000)>>24)|
            ((w&0x00FF0000)>>8) |
            ((w&0x0000FF00)<<8) |
            ((w&0x000000FF)<<24);
    }
    /// reverse 16bit word
    inline static lUInt16 rev( lUInt16 w )
    {
        return
            (lUInt16)(
            ((w&0xFF00)>>8)|
            ((w&0x00FF)<<8) );
    }
    /// make 32 bit word least-significant-first byte order (Intel)
    lUInt32 lsf( lUInt32 w )
    {
        return ( _lsf ) ? w : rev(w);
    }
    /// make 32 bit word most-significant-first byte order (PPC)
    lUInt32 msf( lUInt32 w )
    {
        return ( !_lsf ) ? w : rev(w);
    }
    /// make 16 bit word least-significant-first byte order (Intel)
    lUInt16 lsf( lUInt16 w )
    {
        return ( _lsf ) ? w : rev(w);
    }
    /// make 16 bit word most-significant-first byte order (PPC)
    lUInt16 msf( lUInt16 w )
    {
        return ( !_lsf ) ? w : rev(w);
    }
    void rev( lUInt32 * w )
    {
        *w = rev(*w);
    }
    void rev( lUInt16 * w )
    {
        *w = rev(*w);
    }
    void msf( lUInt32 * w )
    {
        if ( _lsf )
            *w = rev(*w);
    }
    void lsf( lUInt32 * w )
    {
        if ( !_lsf )
            *w = rev(*w);
    }
    void msf( lUInt32 * w, int len )
    {
        if ( _lsf ) {
            for ( int i=0; i<len; i++)
                w[i] = rev(w[i]);
        }
    }
    void lsf( lUInt32 * w, int len )
    {
        if ( !_lsf ) {
            for ( int i=0; i<len; i++)
                w[i] = rev(w[i]);
        }
    }
    void msf( lUInt16 * w, int len )
    {
        if ( _lsf ) {
            for ( int i=0; i<len; i++)
                w[i] = rev(w[i]);
        }
    }
    void lsf( lUInt16 * w, int len )
    {
        if ( !_lsf ) {
            for ( int i=0; i<len; i++)
                w[i] = rev(w[i]);
        }
    }
    void msf( lUInt16 * w )
    {
        if ( _lsf )
            *w = rev(*w);
    }
    void lsf( lUInt16 * w )
    {
        if ( !_lsf )
            *w = rev(*w);
    }
    bool lsf()
    {
        return (_lsf);
    }
    bool msf()
    {
        return (!_lsf);
    }
};

/// timer to interval expiration, in milliseconds
class CRTimerUtil {
    lInt64 _start;
    volatile lInt64 _interval;
public:
    static lInt64 getSystemTimeMillis() {
#ifdef _WIN32
        FILETIME ts;
        GetSystemTimeAsFileTime(&ts);
        return ((lInt64)ts.dwLowDateTime)/10000 + ((lInt64)ts.dwHighDateTime)*1000;
#else
        timeval ts;
        gettimeofday(&ts, 0);
        return ((lInt64)ts.tv_usec)/1000 + ((lInt64)ts.tv_sec)*1000;
#endif
    }

    /// create timer with infinite limit
    CRTimerUtil() {
        _start = getSystemTimeMillis();
        _interval = -1;
    }

    /// create timer with limited interval (milliseconds)
    CRTimerUtil(lInt64 expirationIntervalMillis) {
        _start = getSystemTimeMillis();
        _interval = expirationIntervalMillis;
    }

    void restart() {
        _start = getSystemTimeMillis();
    }

    void restart(lInt64 expirationIntervalMillis) {
        _start = getSystemTimeMillis();
        _interval = expirationIntervalMillis;
    }

    CRTimerUtil & operator = (const CRTimerUtil & t) {
    	_start = t._start;
    	_interval = t._interval;
    	return *this;
    }

    void cancel() {
    	_interval = 0;
    }

    /// returns true if timeout is infinite
    bool infinite() {
        return _interval==-1;
    }
    /// returns true if expirationIntervalMillis is expired
    bool expired() {
        if ( _interval==-1 )
            return false;
        return getSystemTimeMillis() - _start >= _interval;
    }
    /// return milliseconds elapsed since timer start
    lInt64 elapsed() {
        return getSystemTimeMillis() - _start;
    }
    int interval() {
        return (int)_interval;
    }
};


#endif//LVTYPES_H_INCLUDED
