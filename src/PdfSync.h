// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source syncronizer based on .pdfsync file
// License: GPLv2

#pragma once
#include <windows.h>
#include <crtdbg.h>

#include "base_util.h"
#include "str_util.h"


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
        _ASSERT(i<m_size);
        return m_data[i];
    }
    void clear()
    {
        m_size = 0;
    }
    void push_back(_Ty v)
    {
        if(m_size>=m_allocsize) {
            m_allocsize += ALLOC_INCREMENT;
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize); 
        }
        m_data[m_size] = v;
        m_size++;
    }
    void resize(size_t s)
    {
        if(s>m_allocsize) {
            m_allocsize = s+ALLOC_INCREMENT-s%ALLOC_INCREMENT;
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize); 
        }
        m_size = s;
    }
    size_t size()
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
        for(size_t i=0; i<m_size; i++)
            m_data[i].~_Ty();
        delete m_data;
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
        _ASSERT(size()>0);
        resize(size()-1);
    }
    _Ty &top() {
        _ASSERT(size()>0);
        return (*this)[size()-1];
    }
};

#endif


// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE               800

// Minimal vertical distance
#define PDFSYNC_EPSILON_Y                    20

#define PDFSYNCERR_SUCCESS                   0 // the synchronization succeeded
#define PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED 1 // the sync file cannot be opened
#define PDFSYNCERR_INVALID_PAGE_NUMBER       2 // the given page number does not exist in the sync file
#define PDFSYNCERR_NO_SYNC_AT_LOCATION       3 // no synchronization found at this location

#define PDFSYNCERR_UNKNOWN_SOURCEFILE        4 // the source file is not present in the sync file
#define PDFSYNCERR_NORECORD_IN_SOURCEFILE    5 // there is not any record declaration for that particular source file
#define PDFSYNCERR_NORECORD_FOR_THATLINE     6 // no record found for the requested line

typedef struct {
    TCHAR filename[_MAX_PATH]; // source file name
#ifdef _DEBUG
    fpos_t openline_pos;    // start of the scope in the sync file
    fpos_t closeline_pos;   // end of the scope
#endif
    size_t first_recordsection; // index of the first record section of that file
    size_t last_recordsection;  // index of the last record section of that file
} src_file;


// a plines_section is a section of consecutive lines of the form "p ..."
typedef struct {
    fpos_t startpos; // position of the first "p ..." line
#if _DEBUG
    fpos_t endpos;
#endif
} plines_section;


// a section of consecutive records declarations in the syncfile ('l' lines)
typedef struct {
    int srcfile;           // index of the `scoping' source file 
    fpos_t startpos;       // start position in the sync file
    UINT firstrecord;      // number of the first record in the section
#if _DEBUG
    fpos_t endpos;         // end position in the sync file
    int highestrecord;      // highest record #
#endif
} record_section;


#define PDF_EXTENSION     ".PDF"
#define PDFSYNC_EXTENSION ".PDFSYNC"

class Pdfsync
{
public:
    Pdfsync(PCTSTR filename)
    {
        size_t n = _tcslen(filename);
        size_t u = _countof(PDF_EXTENSION)-1;
        if(n>u && _tcsicmp(filename+(n-u),PDF_EXTENSION) == 0 ) {
            _tcsncpy_s(this->syncfilename, filename, n-u);
            _tcscat_s(this->syncfilename, PDFSYNC_EXTENSION);
        }
        else {
            size_t u = _countof(PDFSYNC_EXTENSION)-1;
            _ASSERT(n>u && _tcsicmp(filename+(n-u),PDFSYNC_EXTENSION) == 0 );
        }
        this->index_discarded = true;
    }

    int rebuild_index();
    UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR filename, UINT cchFilename, UINT *line, UINT *col);
    UINT source_to_pdf(PCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y);

    void discard_index() { this->index_discarded = true;}
    bool is_index_discarded() { return this->index_discarded; }

    UINT prepare_commandline(PCTSTR pattern, PCTSTR filename, UINT line, UINT col, PTSTR cmdline, UINT cchCmdline);

private:
    int get_record_section(int record_index);
    int scan_and_build_index(FILE *fp);
    UINT source_to_record(PCTSTR srcfilename, UINT line, UINT col = -1);

private:
    vector<size_t> pdfsheet_index; // pdfsheet_index[i] contains the index in pline_sections of the first pline section for that sheet
    vector<plines_section> pline_sections;
    vector<record_section> record_sections;
    vector<src_file> srcfiles;
    TCHAR syncfilename[_MAX_PATH];
    bool index_discarded; // true if the index needs to be recomputed (needs to be set to true when a change to the pdfsync file is detected)
};

