/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef _VSTRLIST_H_
#define _VSTRLIST_H_

#include "tstr_util.h"
#include <assert.h>

#ifdef USE_STL
#include <vector>
#include <stack>
using namespace std;
#else
#define ALLOC_INCREMENT  10
template <class _Ty>
class vector {
public:
    _Ty &operator[](size_t i) const
    {
        assert(i<m_size);
        return m_data[i];
    }
    _Ty &at(size_t i) const
    {
        assert(i<m_size);
        return m_data[i];
    }
    void clear()
    {
        m_size = 0;
    }
    void push_back(_Ty v)
    {
        if (m_size>=m_allocsize) {
            m_allocsize += ALLOC_INCREMENT;
            if (m_allocsize >= INT_MAX / sizeof(_Ty)) abort();
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize);
            if (!m_data) abort();
        }
        m_data[m_size] = v;
        m_size++;
    }
    void resize(size_t s)
    {
        if (s>m_allocsize) {
            m_allocsize = s+ALLOC_INCREMENT-s%ALLOC_INCREMENT;
            if (m_allocsize >= INT_MAX / sizeof(_Ty)) abort();
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize);
            if (!m_data) abort();
        }
        m_size = s;
    }
    void erase(size_t i)
    {
        if (i < m_size) {
            memcpy(m_data + i, m_data + i + 1, sizeof(_Ty) * (m_size - i - 1));
            m_size--;
        }
    }
    size_t size() const 
    {
        return m_size;
    }
    vector()
    {
        m_allocsize = ALLOC_INCREMENT;
        m_size = 0;
        m_data = (_Ty *)malloc(sizeof(_Ty) * m_allocsize);
    }
    ~vector()
    {
        free(m_data);
    }
private:
    _Ty *m_data;
    size_t m_allocsize, m_size;
};

template <class _Ty>
class stack : public vector<_Ty> {
public:
    void push(_Ty v) {
        push_back(v);
    }
    void pop() {
        assert(this->size()>0);
        erase(this->size()-1);
    }
    _Ty &top() {
        assert(this->size()>0);
        return at(this->size()-1);
    }
};

#endif

class VStrList : public vector<TCHAR *>
{
public:
    ~VStrList() { clearFree(); }

    TCHAR *join(TCHAR *joint=NULL)
    {
        size_t len = 0;
        size_t jointLen = joint ? tstr_len(joint) : 0;
        TCHAR *result, *tmp;

        size_t n = size();
        for (size_t i = n; i > 0; i--)
            len += tstr_len(at(i - 1)) + jointLen;
        if (len <= jointLen)
            return (TCHAR *)calloc(1, sizeof(TCHAR));
        len -= jointLen;

        result = (TCHAR *)calloc(len + 1, sizeof(TCHAR));
        if (!result)
            return NULL;

        assert(size() == n);
        tmp = result;
        TCHAR *end = result + len + 1;
        for (size_t i = 0; i < n; i++) {
            if (jointLen > 0 && i > 0) {
                tstr_copy(tmp, end - tmp, joint);
                tmp += jointLen;
            }
            tstr_copy(tmp, end - tmp, at(i));
            tmp += tstr_len(at(i));
        }

        return result;
    }

    int find(TCHAR *string)
    {
        size_t n = size();
        for (size_t i = 0; i < n; i++) {
            TCHAR *item = at(i);
            if (tstr_eq(string, item))
                return (int)i;
        }
        return -1;
    }

    void clearFree()
    {
        for (size_t i = 0; i < size(); i++)
            free(at(i));
        clear();
    }
};

#endif
