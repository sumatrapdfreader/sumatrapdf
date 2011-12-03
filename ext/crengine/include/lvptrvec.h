/** \file lvptrvec.h
    \brief pointer vector template

    Implements vector of pointers.

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __LVPTRVEC_H_INCLUDED__
#define __LVPTRVEC_H_INCLUDED__

#include <stdlib.h>
#include "lvmemman.h"

/** \brief template which implements vector of pointer

    Automatically deletes objects when vector items are destroyed.
*/
template < class T, bool ownItems=true >
class LVPtrVector
{
    T * * _list;
    int _size;
    int _count;
public:
    /// default constructor
    LVPtrVector() : _list(NULL), _size(0), _count(0) {}
    /// retrieves item from specified position
    T * operator [] ( int pos ) const { return _list[pos]; }
	/// returns pointer array
	T ** get() { return _list; }
    /// retrieves item from specified position
    T * get( int pos ) const { return _list[pos]; }
    /// retrieves item reference from specified position
    T * & operator [] ( int pos ) { return _list[pos]; }
    /// ensures that size of vector is not less than specified value
    void reserve( int size )
    {
        if ( size > _size )
        {
            _list = (T**)realloc( _list, size * sizeof( T* ));
            for (int i=_size; i<size; i++)
                _list[i] = NULL;
            _size = size;
        }
    }
    /// sets item by index (extends vector if necessary)
    void set( int index, T * item )
    {
        reserve( index+1 );
        while (length()<index)
            add(NULL);
        if ( ownItems && _list[index] )
            delete _list[index];
        _list[index] = item;
        if (_count<=index)
            _count = index + 1;
    }
    /// returns size of buffer
    int size() const { return _size; }
    /// returns number of items in vector
    int length() const { return _count; }
    /// returns true if there are no items in vector
    bool empty() const { return _count==0; }
    /// clears all items
    void clear()
    {
        if (_list)
        {
            if ( ownItems ) {
                for (int i=0; i<_count; ++i)
                    delete _list[i];
            }
            free( _list );
        }
        _list = NULL;
        _size = 0;
        _count = 0;
    }
    /// removes several items from vector
    void erase( int pos, int count )
    {
        if ( count<=0 )
            return;
        if ( pos<0 || pos+count > _count )
            crFatalError();
        int i;
        for (i=0; i<count; i++)
        {
            if (_list[pos+i])
            {
                if ( ownItems )
                    delete _list[pos+i];
                _list[pos+i] = NULL;
            }
        }
        for (i=pos+count; i<_count; i++)
        {
            _list[i-count] = _list[i];
            _list[i] = NULL;
        }
        _count -= count;
    }
    /// removes item from vector by index
    T * remove( int pos )
    {
        if ( pos < 0 || pos > _count )
            crFatalError();
        int i;
        T * item = _list[pos];
        for ( i=pos; i<_count-1; i++ )
        {
            _list[i] = _list[i+1];
            //_list[i+1] = NULL;
        }
        _count--;
        return item;
    }
    /// returns vector index of specified pointer, -1 if not found
    int indexOf( T * p )
    {
        for ( int i=0; i<_count; i++ ) {
            if ( _list[i] == p )
                return i;
        }
        return -1;
    }
    T * last()
    {
        if ( _count<=0 )
            return NULL;
        return _list[_count-1];
    }
    T * first()
    {
        if ( _count<=0 )
            return NULL;
        return _list[0];
    }
    /// removes item from vector by index
    T * remove( T * p )
    {
        int i;
        int pos = indexOf( p );
        if ( pos<0 )
            return NULL;
        T * item = _list[pos];
        for ( i=pos; i<_count-1; i++ )
        {
            _list[i] = _list[i+1];
        }
        _count--;
        return item;
    }
    /// adds new item to end of vector
    void add( T * item ) { insert( -1, item ); }
    /// inserts new item to specified position
    void insert( int pos, T * item )
    {
        if (pos<0 || pos>_count)
            pos = _count;
        if ( _count >= _size )
            reserve( _count * 3 / 2  + 8 );
        for (int i=_count; i>pos; --i)
            _list[i] = _list[i-1];
        _list[pos] = item;
        _count++;
    }
    /// move item to specified position, other items will be shifted
    void move( int indexTo, int indexFrom )
    {
        if ( indexTo==indexFrom )
            return;
        T * p = _list[indexFrom];
        if ( indexTo<indexFrom ) {
            for ( int i=indexFrom; i>indexTo; i--)
                _list[i] = _list[i-1];
        } else {
            for ( int i=indexFrom; i<indexTo; i++)
                _list[i] = _list[i+1];
        }
        _list[ indexTo ] = p;
    }
    /// copy constructor
    LVPtrVector( const LVPtrVector & v )
        : _list(NULL), _size(0), _count(0)
    {
        if ( v._count>0 ) {
            reserve( v._count );
            for ( int i=0; i<v._count; i++ )
                add( new T(*v[i]) );
        }
    }
    /// stack-like interface: pop top item from stack
    T * pop()
    {
        if ( empty() )
            return NULL;
        return remove( length() - 1 );
    }
    /// stack-like interface: pop top item from stack
    T * popHead()
    {
        if ( empty() )
            return NULL;
        return remove( 0 );
    }
    /// stack-like interface: push item to stack
    void push( T * item )
    {
        add( item );
    }
    /// stack-like interface: push item to stack
    void pushHead( T * item )
    {
        insert( 0, item );
    }
    /// stack-like interface: get top item w/o removing from stack
    T * peek()
    {
        if ( empty() )
            return NULL;
        return get( length() - 1 );
    }
    /// stack-like interface: get top item w/o removing from stack
    T * peekHead()
    {
        if ( empty() )
            return NULL;
        return get( 0 );
    }
    /// destructor
    ~LVPtrVector() { clear(); }
};

template<class _Ty > class LVMatrix {
protected:
    int numcols;
    int numrows;
    _Ty ** rows;
public:
    LVMatrix<_Ty> () : numcols(0), numrows(0), rows(NULL) {}
    void Clear() {
        if (rows) {
			if (numrows && numcols) {
				for (int i=0; i<numrows; i++)
					free( rows[i] );
			}
            free( rows );
		}
        rows = NULL;
        numrows = 0;
        numcols = 0;
    }
    ~LVMatrix<_Ty> () {
        Clear();
    }   

    _Ty * operator [] (int rowindex) { return rows[rowindex]; }

    void SetSize( int nrows, int ncols, _Ty fill_elem ) {
        if (!nrows || !ncols) {
            Clear();
            return;
        }
        if ( nrows<numrows ) {
            for (int i=nrows; i<numrows; i++)
                free( rows[i] );
            numrows = nrows;
        } else if (nrows>numrows) {
            rows = (_Ty**) realloc( rows, sizeof(_Ty)*nrows );
            for (int i=numrows; i<nrows; i++) {
                rows[i] = (_Ty*)malloc( sizeof(_Ty*) * ncols );
                for (int j=0; j<numcols; j++)
                    rows[i][j]=fill_elem;
            }
            numrows = nrows;
        }
        if (ncols>numcols) {
            for (int i=0; i<numrows; i++) {
                rows[i] = (_Ty*)realloc( rows[i], sizeof(_Ty) * ncols );
                for (int j=numcols; j<ncols; j++)
                    rows[i][j]=fill_elem;
            }
            numcols = ncols;
        }
    }
};

template <typename T1, typename T2> class LVPair
{
    T1 _first;
    T2 _second;
public:
    LVPair( const T1 & first, const T2 & second )
    : _first(first), _second(second) {
    }
    LVPair( const LVPair & v ) 
    : _first(v._first), _second(v._second) {
    }
    LVPair & operator = ( const LVPair & v ) 
    {
        _first = v._first;
        _second = v._second;
    }
    T1 & first() { return _first; }
    const T1 & first() const { return _first; }
    T2 & second() { return _second; }
    const T2 & second() const { return _second; }
    ~LVPair() { }
};

#endif
