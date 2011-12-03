/** \file lvref.h
    \brief smart pointer with reference counting template

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.
    See LICENSE file for details.

*/

#ifndef __LVREF_H_INCLUDED__
#define __LVREF_H_INCLUDED__

#include "lvmemman.h"

/// Memory manager pool for ref counting
/**
    For fast and efficient allocation of ref counter structures
*/
#if (LDOM_USE_OWN_MEM_MAN==1)
extern ldomMemManStorage * pmsREF;
#endif

/// Reference counter structure
/**
    For internal usage in LVRef<> class
*/
class ref_count_rec_t {
public:
    int _refcount;
    void * _obj;
    static ref_count_rec_t null_ref;

    ref_count_rec_t( void * obj ) : _refcount(1), _obj(obj) { }
#if (LDOM_USE_OWN_MEM_MAN==1)
    void * operator new( size_t )
    {
        if (pmsREF == NULL)
        {
            pmsREF = new ldomMemManStorage(sizeof(ref_count_rec_t));
        }
        return pmsREF->alloc();
    }
    void operator delete( void * p )
    {
        pmsREF->free((ldomMemBlock *)p);
    }
#endif
};

/// sample ref counter implementation for LVFastRef
class LVRefCounter
{
    int refCount;
public:
    LVRefCounter() : refCount(0) { }
    void AddRef() { refCount++; }
    int Release() { return --refCount; }
    int getRefCount() { return refCount; }
};

/// Fast smart pointer with reference counting
/**
    Stores pointer to object and reference counter.
    Imitates usual pointer behavior, but deletes object 
    when there are no more references on it.
    On copy, increases reference counter.
    On destroy, decreases reference counter; deletes object if counter became 0.
    T should implement AddRef() and Release() methods.
    \param T class of stored object
 */
template <class T> class LVFastRef
{
private:
    T * _ptr;
    inline void Release()
    {
        if ( _ptr ) {
            if ( _ptr->Release()==0 ) {
                delete _ptr;
            }
            _ptr=NULL;
        }
    }
public:
    /// Default constructor.
    /** Initializes pointer to NULL */
    LVFastRef() : _ptr(NULL) { }

    /// Constructor by object pointer.
    /** Initializes pointer to given value 
    \param ptr is a pointer to object
     */
    explicit LVFastRef( T * ptr ) {
        _ptr = ptr;
        if ( _ptr )
            _ptr->AddRef();
    }

    /// Copy constructor.
    /** Creates copy of object pointer. Increments reference counter instead of real copy.
    \param ref is reference to copy
     */
    LVFastRef( const LVFastRef & ref )
    {
        _ptr = ref._ptr;
        if ( _ptr )
            _ptr->AddRef();
    }

    /// Destructor.
    /** Decrements reference counter; deletes object if counter became 0. */
    ~LVFastRef() { Release(); }

    /// Clears pointer.
    /** Sets object pointer to NULL. */
    void Clear() { Release(); }

    /// Copy operator.
    /** Duplicates a pointer from specified reference. 
    Increments counter instead of copying of object. 
    \param ref is reference to copy
     */
    LVFastRef & operator = ( const LVFastRef & ref )
    {
        if ( _ptr ) {
            if ( _ptr==ref._ptr )
                return *this;
            Release();
        }
        if ( ref._ptr )
            (_ptr = ref._ptr)->AddRef();
        return *this;
    }

    /// Object pointer assignment operator.
    /** Sets object pointer to the specified value. 
    Reference counter is being initialized to 1.
    \param obj pointer to object
     */
    LVFastRef & operator = ( T * obj )
    {
        if ( _ptr ) {
            if ( _ptr==obj )
                return *this;
            Release();
        }
        if ( obj )
            (_ptr = obj)->AddRef();
        return *this;
    }

    /// Returns stored pointer to object.
    /** Imitates usual pointer behavior. 
    Usual way to access object fields. 
     */
    T * operator -> () const { return _ptr; }

    /// Dereferences pointer to object.
    /** Imitates usual pointer behavior. */
    T & operator * () const { return *_ptr; }

    /// To check reference counter value.
    /** It might be useful in some cases. 
    \return reference counter value.
     */
    int getRefCount() const { return _ptr->getRefCount(); }

    /// Returns stored pointer to object.
    /** Usual way to get pointer value. 
    \return stored pointer to object.
     */
    T * get() const { return _ptr; }

    /// Checks whether pointer is NULL or not.
    /** \return true if pointer is NULL.
    \sa isNull() */
    bool operator ! () const { return !_ptr; }

    /// Checks whether pointer is NULL or not.
    /** \return true if pointer is NULL. 
    \sa operator !()
     */
    bool isNull() const { return (_ptr == NULL); }
};

/// Smart pointer with reference counting
/**
    Stores pointer to object and reference counter.
    Imitates usual pointer behavior, but deletes object 
    when there are no more references on it.
    On copy, increases reference counter
    On destroy, decreases reference counter; deletes object if counter became 0.
    Separate counter object is used, so no counter support is required for T.
    \param T class of stored object
*/
template <class T> class LVRef
{
private:
    ref_count_rec_t * _ptr;
    //========================================
    ref_count_rec_t * AddRef() const { ++_ptr->_refcount; return _ptr; }
    //========================================
    void Release()
    { 
        if (--_ptr->_refcount == 0) 
        {
            if ( _ptr->_obj )
                delete (reinterpret_cast<T*>(_ptr->_obj));
            delete _ptr;
        }
    }
    //========================================
public:

	/// creates reference to copy
	LVRef & clone()
	{
		if ( isNull() )
			return LVRef();
		return LVRef( new T( *_ptr ) );
	}

    /// Default constructor.
    /** Initializes pointer to NULL */
    LVRef() : _ptr(&ref_count_rec_t::null_ref) { ref_count_rec_t::null_ref._refcount++; }

    /// Constructor by object pointer.
    /** Initializes pointer to given value 
        \param ptr is a pointer to object
    */
    explicit LVRef( T * ptr ) {
        if (ptr)
        {
            _ptr = new ref_count_rec_t(ptr);
        }
        else
        {
            ref_count_rec_t::null_ref._refcount++;
            _ptr = &ref_count_rec_t::null_ref;
        }
    }

    /// Copy constructor.
    /** Creates copy of object pointer. Increments reference counter instead of real copy.
        \param ref is reference to copy
    */
    LVRef( const LVRef & ref ) { _ptr = ref.AddRef(); }

    /// Destructor.
    /** Decrements reference counter; deletes object if counter became 0. */
    ~LVRef() { Release(); }

    /// Clears pointer.
    /** Sets object pointer to NULL. */
    void Clear() { Release(); _ptr = &ref_count_rec_t::null_ref; ++_ptr->_refcount; }

    /// Copy operator.
    /** Duplicates a pointer from specified reference. 
        Increments counter instead of copying of object. 
        \param ref is reference to copy
    */
    LVRef & operator = ( const LVRef & ref )
    {
        if (!ref._ptr->_obj)
        {
            Clear();
        }
        else
        {
            if (_ptr!=ref._ptr)
            {
                Release();
                _ptr = ref.AddRef(); 
            }
        }
        return *this;
    }

    /// Object pointer assignment operator.
    /** Sets object pointer to the specified value. 
        Reference counter is being initialized to 1.
        \param obj pointer to object
    */
    LVRef & operator = ( T * obj )
    {
        if ( !obj )
        {
            Clear();
        }
        else
        {
            if (_ptr->_obj!=obj)
            {
                Release();
                _ptr = new ref_count_rec_t(obj);
            }
        }
        return *this;
    }

    /// Returns stored pointer to object.
    /** Imitates usual pointer behavior. 
        Usual way to access object fields. 
    */
    T * operator -> () const { return reinterpret_cast<T*>(_ptr->_obj); }

    /// Dereferences pointer to object.
    /** Imitates usual pointer behavior. */
    T & operator * () const { return *(reinterpret_cast<T*>(_ptr->_obj)); }

    /// To check reference counter value.
    /** It might be useful in some cases. 
        \return reference counter value.
    */
    int getRefCount() const { return _ptr->_refcount; }

    /// Returns stored pointer to object.
    /** Usual way to get pointer value. 
        \return stored pointer to object.
    */
    T * get() const { return reinterpret_cast<T*>(_ptr->_obj); }

    /// Checks whether pointer is NULL or not.
    /** \return true if pointer is NULL.
        \sa isNull() */
    bool operator ! () const { return !_ptr->_obj; }

    /// Checks whether pointer is NULL or not.
    /** \return true if pointer is NULL. 
        \sa operator !()
    */
    bool isNull() const { return _ptr->_obj == NULL; }
};

template <typename T >
class LVRefVec
{
    LVRef<T> * _array;
    int _size;
    int _count;
public:
    /// default constructor
    LVRefVec() : _array(NULL), _size(0), _count(0) {}
    /// creates array of given size
    LVRefVec( int len, LVRef<T> value )
    {
        _size = _count = len;
        _array = new LVRef<T>[_size];
        for (int i=0; i<_count; i++)
            _array[i] = value;
    }
    LVRefVec( const LVRefVec & v )
    {
        _size = _count = v._count;
        if ( _size ) {
            _array = new LVRef<T>[_size];
            for (int i=0; i<_count; i++)
                _array[i] = v._array[i];
        } else {
            _array = NULL;
        }
    }
    LVRefVec & operator = ( const LVRefVec & v )
    {
        clear();
        _size = _count = v._count;
        if ( _size ) {
            _array = new LVRef<T>[_size];
            for (int i=0; i<_count; i++)
                _array[i] = v._array[i];
        } else {
            _array = NULL;
        }
        return *this;
    }
    /// retrieves item from specified position
    LVRef<T> operator [] ( int pos ) const { return _array[pos]; }
    /// retrieves item reference from specified position
    LVRef<T> & operator [] ( int pos ) { return _array[pos]; }
    /// ensures that size of vector is not less than specified value
    void reserve( int size )
    {
        if ( size > _size )
        {
            LVRef<T> * newarray = new LVRef<T>[ size ];
            for ( int i=0; i<_size; i++ )
                newarray[ i ] = _array[ i ];
            if ( _array )
                delete [] _array;
            _array = newarray;
            _size = size;
        }
    }
    /// sets item by index (extends vector if necessary)
    void set( int index, LVRef<T> item )
    {
        reserve( index );
        _array[index] = item;
    }
    /// returns size of buffer
    int size() { return _size; }
    /// returns number of items in vector
    int length() { return _count; }
    /// returns true if there are no items in vector
    bool empty() { return _count==0; }
    /// clears all items
    void clear()
    {
        if (_array)
        {
            delete [] _array;
            _array = NULL;
        }
        _size = 0;
        _count = 0;
    }
    /// copies range to beginning of array
    void trim( int pos, int count, int reserved )
    {
        if ( pos<0 || count<=0 || pos+count > _count )
            throw;
        int i;
        int new_sz = count;
        if (new_sz < reserved)
            new_sz = reserved;
        T* new_array = (T*)malloc( new_sz * sizeof( T ) );
        if (_array)
        {
            for ( i=0; i<count; i++ )
            {
                new_array[i] = _array[ pos + i ];
            }
            free( _array );
        }
        _array = new_array;
        _count = count;
        _size = new_sz;
    }
    /// removes several items from vector
    void erase( int pos, int count )
    {
        if ( pos<0 || count<=0 || pos+count > _count )
            throw;
        int i;
        for (i=pos+count; i<_count; i++)
        {
            _array[i-count] = _array[i];
        }
        _count -= count;
    }

    /// adds new item to end of vector
    void add( LVRef<T> item )
    { 
        insert( -1, item );
    }

    void add( LVRefVec<T> & list )
    {
        for ( int i=0; i<list.length(); i++ )
            add( list[i] );
    }

    /// adds new item to end of vector
    void append( const LVRef<T> * items, int count )
    {
        reserve( _count + count );
        for (int i=0; i<count; i++)
            _array[ _count+i ] = items[i];
        _count += count;
    }
    
    LVRef<T> * addSpace( int count )
    {
        reserve( _count + count );
        LVRef<T> * ptr = _array + _count;
        _count += count;
        return ptr;
    }
    
    /// inserts new item to specified position
    void insert( int pos, LVRef<T> item )
    {
        if (pos<0 || pos>_count)
            pos = _count;
        if ( _count >= _size )
            reserve( _count * 3 / 2  + 8 );
        for (int i=_count; i>pos; --i)
            _array[i] = _array[i-1];
        _array[pos] = item;
        _count++;
    }
    /// returns array pointer
    LVRef<T> * ptr() { return _array; }
    /// destructor
    ~LVRefVec() { clear(); }
};

/// auto pointer
template <class T >
class LVAutoPtr {
    T * p;
    LVAutoPtr( const LVAutoPtr & v ) { } // no copy allowed
public:
    LVAutoPtr()
        : p(NULL)
{
    }
    explicit LVAutoPtr( T* ptr )
        : p(ptr)
    {
    }
    inline void clear()
    {
        if ( p )
            delete( p );
        p = NULL;
    }
    ~LVAutoPtr()
    {
        clear();
    }
    inline T * operator -> ()
    {
        return p;
    }
    inline T & operator [] (int index) { return p[index]; }
    inline T * get() { return p; }
    inline T & operator * ()
    {
        return *p;
    }
    inline LVAutoPtr & operator = ( T* ptr )
    {
        if ( p==ptr )
            return *this;
        if ( p )
            delete p;
        p = ptr;
        return *this;
    }
};


#endif
