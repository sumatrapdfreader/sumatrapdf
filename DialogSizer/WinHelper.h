/*----------------------------------------------------------------------
Copyright (c) 1998 Gipsysoft. All Rights Reserved.
Please see the file "licence.txt" for licencing details.
File:	WinHelper.h
Owner:	russf@gipsysoft.com
Purpose:	Windows helper functions, classes, structures and macros
					that make life a little easier
					These should all be zero impact classes etc. that is they 
					should *not* have a cpp file associated with them.
----------------------------------------------------------------------*/
#ifndef WINHELPER_H
#define WINHELPER_H

namespace WinHelper
{
	//	Wrapper for the SIZE structure
	class CSize : public tagSIZE
	{
	public:
		inline CSize() {};
		inline explicit CSize( const SIZE &size ) { cx = size.cx; cy = size.cy; }
		inline explicit CSize( long nSizeX, long nSizeY ) { cx = nSizeX; cy = nSizeY; }
		inline CSize( const CSize& size ) { cx = size.cx; cy = size.cy; };
		inline void Set( long nSizeX, long nSizeY ) { cx = nSizeX; cy = nSizeY; }
		inline operator LPSIZE() { return this; };

		inline bool operator !=( const SIZE &size ) const { return cx != size.cx || cy != size.cy;}
		inline CSize & operator =( const SIZE &size ) { cx = size.cx; cy = size.cy; return *this; }
		inline void Empty() { cx = cy = 0; }
	};

	//	Wrapper for a RECT structure
	class CRect : public tagRECT
	{
	public:
		inline CRect() {}
		//	From a window
		inline CRect( HWND hwnd ) { ::GetWindowRect( hwnd, this ); }
		//	Initialisation constructor
		inline explicit CRect( const RECT& rhs ) { Set( rhs.left, rhs.top, rhs.right, rhs.bottom );}
		inline CRect(int xLeft, int yTop, int xRight, int yBottom) { Set( xLeft, yTop, xRight, yBottom ); }
		//	Get the width of the rectangle
		inline int Width() const { return right - left; }
		//	Get the height of the rectangle
		inline int Height() const { return bottom - top; }
		//	overloaded operator so you don't have to do &rc anymore
		inline operator LPCRECT() const { return this; };
		inline operator LPRECT() { return this; };
		//	Return the SIZE of the rectangle;
		inline CSize Size() const { CSize s( Width(), Height() ); return s; }
		//inline CSize Size() const { return CSize(Width(), Height()); }
		//	Return the top left of the rectangle
		inline POINT TopLeft() const { POINT pt = { left, top }; return pt; }	
		//	Return the bottom right of the rectangle
		inline POINT BottomRight() const { POINT pt = { right, bottom }; return pt; }	
		//	Set the rectangles left, top, right and bottom
		inline void Set( int xLeft, int yTop, int xRight, int yBottom) { top = yTop; bottom = yBottom; right = xRight; left = xLeft; }
		//	Return true if the rectangle contains all zeros
		inline bool IsEmpty() const { return left == 0 && right == 0 && top == 0 && bottom == 0 ? true : false; }
		//	Zero out our rectangle
		inline void Empty() { left = right = top = bottom = 0; }
		//	Set the size of the rect but leave the top left position untouched.
		inline void SetSize( const CSize &size ) { bottom = top + size.cy; right = left + size.cx; }
		inline void SetSize( const SIZE &size ) { bottom = top + size.cy; right = left + size.cx; }
		inline void SetSize( int cx, int cy ) { bottom = top + cy; right = left + cx; }
		//	Move the rectangle by an offset
		inline void Offset( int cx, int cy )
			{
				top+=cy;
				bottom+=cy;
				right+=cx;
				left+=cx;
			}
		//	Inflate the rectangle by the cx and cy, use negative to shrink the rectangle
		inline void Inflate( int cx, int cy )
			{
				top-=cy;
				bottom+=cy;
				right+=cx;
				left-=cx;
			}
		//	Assignment from a RECT
		inline CRect &operator = ( const RECT&rhs )
			{
				left = rhs.left; top = rhs.top;
				right = rhs.right; bottom = rhs.bottom;
				return *this;
			}

		//	Return true if the point passed is within the rectangle
		inline bool PtInRect( const POINT &pt ) const	{	return  ( pt.x >= left && pt.x < right && pt.y >=top && pt.y < bottom ); }
		//	Return true if the rectangle passed overlaps this rectangle
		inline bool Intersect( const RECT &rc ) const {	return ( rc.left < right && rc.right > left && rc.top < bottom && rc.bottom > top ); }
	};

	//	Wrapper for the POINT structure
	class CPoint : public tagPOINT
	{
	public:
		inline CPoint() {};
		inline CPoint( LPARAM lParam ) { x = LOWORD( lParam ); y = HIWORD(lParam); }
		inline CPoint( int nX, int nY ) { x = nX; y = nY; }
		inline CPoint( const POINT &pt ) { x = pt.x; y = pt.y; }
		inline bool operator == ( const CPoint &rhs ) const { return x == rhs.x && y == rhs.y; }
		inline bool operator != ( const CPoint &rhs ) const { return x != rhs.x || y != rhs.y; }
		inline operator LPPOINT () { return this; }
		void ConvertToClient( HWND hwnd ) { ScreenToClient( hwnd, this ); }
	};

	//	Wrapper for a brush
	class CBrush
	{
	public:
		CBrush( HBRUSH hbr ) : m_hbr( hbr ) {}
		CBrush( COLORREF cr ) : m_hbr( CreateSolidBrush( cr ) ) {}
		
		~CBrush() { DeleteObject( m_hbr ); }
		operator HBRUSH() { return m_hbr; }

		HBRUSH m_hbr;
	};

	//	Wrapper for a brush
	class CPen
	{
	public:
		CPen( HPEN h ) : m_pen( h ) {}
		CPen( COLORREF cr, int nWidth = 1, int nStyle = PS_SOLID ) : m_pen( CreatePen( nStyle, nWidth, cr ) ) {}
		
		~CPen() { DeleteObject( m_pen ); }
		operator HPEN() { return m_pen; }

		HPEN m_pen;
	};

	//	Wrapper to get and release a window DC
	class CWindowDC
	{
	public:
		CWindowDC( HWND hwnd )
			: m_hwnd( hwnd )
			, m_hdc( ::GetDC( hwnd ) )
			{}

		~CWindowDC() { ::ReleaseDC( m_hwnd, m_hdc ); }
		operator HDC() const { return m_hdc; }
	private:
		HWND m_hwnd;
		HDC m_hdc;
	};


	class CSaveDCObject
	{
	public:
		CSaveDCObject( HDC hdc, HGDIOBJ h )
			: m_hdc( hdc )
			, m_hOld( SelectObject( hdc, h ) )
			{}
		~CSaveDCObject()
		{
			SelectObject( m_hdc, m_hOld );
		}

	private:
		HDC m_hdc;
		HGDIOBJ m_hOld;
	};

	#define ZeroStructure( t ) ZeroMemory( &t, sizeof( t ) )
	#define countof( t )	(sizeof( (t) ) / sizeof( (t)[0] ) )
	#define UNREF(P) UNREFERENCED_PARAMETER(P)

	//	Wrapper for the Begin, Defer and End WindowPos functions. Nothing glamorous.
	class CDeferWindowPos
	{
	public:
		inline CDeferWindowPos( const int nWindows = 1 ) : m_hdlDef( ::BeginDeferWindowPos(nWindows) ) {}
		inline ~CDeferWindowPos() { ::EndDeferWindowPos(m_hdlDef); }
		inline HDWP DeferWindowPos( HWND hWnd, HWND hWndInsertAfter , int x, int y, int cx, int cy, UINT uFlags )
		{
			return ::DeferWindowPos( m_hdlDef, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags );
		}
		inline HDWP DeferWindowPos( HWND hWnd, HWND hWndInsertAfter, const CRect &rc, UINT uFlags )
		{
			return ::DeferWindowPos( m_hdlDef, hWnd, hWndInsertAfter, rc.left, rc.top, rc.Width(), rc.Height(), uFlags );
		}

	private:
		HDWP m_hdlDef;
	};

}

#endif
