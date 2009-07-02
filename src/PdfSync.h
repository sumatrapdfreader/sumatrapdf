// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source synchronizer based on .pdfsync file
// License: GPLv2

#ifndef _PDF_SYNC_H__
#define _PDF_SYNC_H__

#include <windows.h>
#include <assert.h>

#include "base_util.h"
#include "str_util.h"
#include "tstr_util.h"
#include "file_util.h"
#ifdef SYNCTEX_FEATURE
#include "synctex_parser.h"
#endif

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
    void clear()
    {
        m_size = 0;
    }
    void push_back(_Ty v)
    {
        if (m_size>=m_allocsize) {
            m_allocsize += ALLOC_INCREMENT;
            m_data = (_Ty *)realloc(m_data, sizeof(_Ty) * m_allocsize); 
        }
        m_data[m_size] = v;
        m_size++;
    }
    void resize(size_t s)
    {
        if (s>m_allocsize) {
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
        resize(this->size()-1);
    }
    _Ty &top() {
        assert(this->size()>0);
        return (*this)[this->size()-1];
    }
};

#endif

// size of the mark highlighting the location calculated by forward-search
#define MARK_SIZE                            10 

// maximum error in the source file line number when doing forward-search
#define EPSILON_LINE                         5  

// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE               800

// Minimal vertical distance
#define PDFSYNC_EPSILON_Y                    20

//////
// Error codes returned by the synchronization functions
enum {  PDFSYNCERR_SUCCESS,                   // the synchronization succeeded
        PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED, // the sync file cannot be opened
        PDFSYNCERR_INVALID_PAGE_NUMBER,       // the given page number does not exist in the sync file
        PDFSYNCERR_NO_SYNC_AT_LOCATION,       // no synchronization found at this location
        PDFSYNCERR_UNKNOWN_SOURCEFILE,        // the source file is not present in the sync file
        PDFSYNCERR_NORECORD_IN_SOURCEFILE,    // there is not any record declaration for that particular source file
        PDFSYNCERR_NORECORD_FOR_THATLINE,     // no record found for the requested line
        PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD,// a record is found for the given source line but there is not point in the PDF that corresponds to it
        PDFSYNCERR_OUTOFMEMORY,
        PDFSYNCERR_INVALID_ARGUMENT
};

typedef struct {
    char filename[_MAX_PATH]; // source file name
#ifndef NDEBUG
    fpos_t openline_pos;    // start of the scope in the sync file
    fpos_t closeline_pos;   // end of the scope
#endif
    size_t first_recordsection; // index of the first record section of that file
    size_t last_recordsection;  // index of the last record section of that file
} src_file;


// a plines_section is a section of consecutive lines of the form "p ..."
typedef struct {
    fpos_t startpos; // position of the first "p ..." line
#ifndef NDEBUG
    fpos_t endpos;
#endif
} plines_section;


// a section of consecutive records declarations in the syncfile ('l' lines)
typedef struct {
    size_t srcfile;           // index of the `scoping' source file 
    fpos_t startpos;       // start position in the sync file
    UINT firstrecord;      // number of the first record in the section
#ifndef NDEBUG
    fpos_t endpos;         // end position in the sync file
    int highestrecord;      // highest record #
#endif
} record_section;


#define PDF_EXTENSION     _T(".PDF")
#define PDFSYNC_EXTENSION _T(".PDFSYNC")

// System of point coordinates
typedef enum { TopLeft,    // origin at the top-left corner
               BottomLeft, // origin at the bottom-left corner
} CoordSystem;

class Synchronizer
{
public:
    Synchronizer(LPCTSTR _syncfilename) {
        this->index_discarded = true;
        this->coordsys = BottomLeft; // by default set the internal coordinate system to bottom-left
        this->dir = FilePathW_GetDir(_syncfilename);
    }
    ~Synchronizer() {
        if (dir)
            free(dir);
    }

    // conversion from one coordinate system to another
    void convert_coord_to_internal(UINT *x, UINT *y, UINT pageHeight, CoordSystem src)
    {
        if (src==this->coordsys)
            return;    
        *y = pageHeight - *y;
    }
    void convert_coord_from_internal(UINT *x, UINT *y, UINT pageHeight, CoordSystem dst)
    {
        if (dst==this->coordsys)
            return;    
        *y = pageHeight - *y;
    }
    
    // Inverse-search:
    //  - sheet: page number in the PDF (starting from 1)
    //  - x,y: user-specified PDF-coordinates. They must be given in the system used internally by the synchronizer.
    //  - maxy: contains the height of the page. this is necessary to convert into the coordinate-system used internally by the syncrhonizer.
    //    For an A4 paper it is approximately equal to maxy=842.
    //  - cchFilename: size of the buffer 'filename'
    // The result is returned in filename, line, col
    //  - filename: receives the name of the source file
    //  - line: receives the line number
    //  - col: receives the column number
    virtual UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR filename, UINT cchFilename, UINT *line, UINT *col) = 0;
    
    // Forward-search:
    // The result is returned in (page,x,y). The coordinates x,y are specified in the internal 
    // coordinate system.
    virtual UINT source_to_pdf(LPCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y) = 0;

    void discard_index() { this->index_discarded = true; }
    bool is_index_discarded() { return this->index_discarded; }
    int rebuild_index() { this->index_discarded = false; return 0; }

    UINT prepare_commandline(LPCTSTR pattern, LPCTSTR filename, UINT line, UINT col, PTSTR cmdline, UINT cchCmdline);

private:
    bool index_discarded; // true if the index needs to be recomputed (needs to be set to true when a change to the pdfsync file is detected)

protected:
    CoordSystem coordsys; // system used internally by the syncfile for the PDF coordinates
    PTSTR dir;            // directory where the syncfile lies
};

#ifdef SYNCTEX_FEATURE

#define SYNCTEX_EXTENSION       _T(".synctex")
#define SYNCTEXGZ_EXTENSION     _T(".synctex.gz")

// Synchronizer based on .synctex file generated with SyncTex
class SyncTex : public Synchronizer
{
public:
    SyncTex(LPCTSTR _syncfilename) : Synchronizer(_syncfilename)
    {
        size_t n = lstrlen(_syncfilename);
        size_t u1 = dimof(SYNCTEX_EXTENSION)-1,
               u2 = dimof(SYNCTEXGZ_EXTENSION)-1;
        assert((n>u1 && _wcsicmp(_syncfilename+(n-u1),SYNCTEX_EXTENSION)==0)
               ||(n>u2 && _wcsicmp(_syncfilename+(n-u2),SYNCTEXGZ_EXTENSION)==0));
        tstr_copy(this->syncfilename, dimof(this->syncfilename), _syncfilename);
        
        this->scanner = NULL;
        this->coordsys = TopLeft;
    }
    ~SyncTex()
    {
        if (scanner)
          synctex_scanner_free(scanner);

    }
    void discard_index() { Synchronizer::discard_index();}
    bool is_index_discarded() { return Synchronizer::is_index_discarded(); }

    UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col);
    UINT source_to_pdf(LPCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y);
    int rebuild_index();

private:
    TCHAR syncfilename[MAX_PATH];
    synctex_scanner_t scanner;
};
#endif



// Synchronizer based on .sync file generated with the pdfsync tex package
class Pdfsync : public Synchronizer
{
public:
    Pdfsync(LPCTSTR _syncfilename) : Synchronizer(_syncfilename)
    {
        size_t n = lstrlen(_syncfilename);
        size_t u = dimof(PDFSYNC_EXTENSION)-1;
        assert(n>u && wcsicmp(_syncfilename+(n-u), PDFSYNC_EXTENSION) == 0 );
        tstr_copy(this->syncfilename, dimof(this->syncfilename), _syncfilename);
        this->coordsys = BottomLeft;
    }

    int rebuild_index();
    UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col);
    UINT source_to_pdf(LPCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y);

private:
    int get_record_section(int record_index);
    int scan_and_build_index(FILE *fp);
    UINT source_to_record(FILE *fp, LPCTSTR srcfilename, UINT line, UINT col, vector<size_t> &records);
    FILE *opensyncfile();

private:
    vector<size_t> pdfsheet_index; // pdfsheet_index[i] contains the index in pline_sections of the first pline section for that sheet
    vector<plines_section> pline_sections;
    vector<record_section> record_sections;
    vector<src_file> srcfiles;
    TCHAR syncfilename[MAX_PATH];
};


// create a synchronizer for the given PDF file
Synchronizer *CreateSynchronizer(LPCTSTR pdffilename);

#define PDFSYNC_DDE_SERVICE   _T("SUMATRA")
#define PDFSYNC_DDE_TOPIC     _T("control")

// forward-search command
//  format: [ForwardSearch("<pdffilepath>","<sourcefilepath>",<line>,<column>[,<newwindow>, <setfocus>])]
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [ForwardSearch("c:\file.pdf","c:\folder\source.tex",298,0)]
#define DDECOMMAND_SYNC       _T("ForwardSearch")

// open file command
//  format: [Open("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [Open("c:\file.pdf", 1, 1)]
#define DDECOMMAND_OPEN       _T("Open")

// jump to named destination command
//  format: [GoToNamedDest("<pdffilepath>","<destination name>")]
//  eg: [GoToNamedDest("c:\file.pdf", "chapter.1")]. pdf file must be already opened
#define DDECOMMAND_GOTO       _T("GotoNamedDest")

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam);

#endif
