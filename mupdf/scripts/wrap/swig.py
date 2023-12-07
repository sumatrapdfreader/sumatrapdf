'''
Support for using SWIG to generate langauge bindings from the C++ bindings.
'''

import inspect
import io
import os
import re
import textwrap

import jlib

from . import cpp
from . import rename
from . import state
from . import util


def translate_ucdn_macros( build_dirs):
    '''
    Returns string containing UCDN_* macros represented as enums.
    '''
    out = io.StringIO()
    with open( f'{build_dirs.dir_mupdf}/include/mupdf/ucdn.h') as f:
        text = f.read()
    out.write( '\n')
    out.write( '\n')
    out.write( 'enum\n')
    out.write( '{\n')
    n = 0
    for m in re.finditer('\n#define (UCDN_[A-Z0-9_]+) +([^\n]+)', text):
        out.write(f'    {m.group(1)} = {m.group(2)},\n')
        n += 1
    out.write( '};\n')
    out.write( '\n')
    assert n
    return out.getvalue()


def build_swig(
        state_,
        build_dirs,
        generated,
        language='python',
        swig_command='swig',
        check_regress=False,
        force_rebuild=False,
        ):
    '''
    Builds python or C# wrappers for all mupdf_* functions and classes, by
    creating a .i file that #include's our generated C++ header files and
    running swig.

    build_dirs
        A BuildDirs instance.
    generated.
        A Generated instance.
    language
        The output language, must be 'python' or 'csharp'.
    swig
        Location of swig binary.
    check_regress
        If true, we fail with error if generated .i file already exists and
        differs from our new content.
    '''
    assert isinstance( state_, state.State)
    assert isinstance(build_dirs, state.BuildDirs), type(build_dirs)
    assert isinstance(generated, cpp.Generated), type(generated)
    assert language in ('python', 'csharp')
    # Find version of swig. (We use quotes around <swig> to make things work on
    # Windows.)
    t = jlib.system( f'"{swig_command}" -version', out='return', verbose=1)
    jlib.log('SWIG version info:\n========\n{t}\n========')
    m = re.search( 'SWIG Version ([0-9]+)[.]([0-9]+)[.]([0-9]+)', t)
    assert m
    swig_major = int( m.group(1))
    jlib.system( f'which "{swig_command}"', raise_errors=0)

    # Create a .i file for SWIG.
    #
    common = textwrap.dedent(f'''
            #include <stdexcept>

            #include "mupdf/functions.h"
            #include "mupdf/classes.h"
            #include "mupdf/classes2.h"
            #include "mupdf/internal.h"
            #include "mupdf/exceptions.h"
            #include "mupdf/extra.h"

            #ifdef NDEBUG
                static bool g_mupdf_trace_director = false;
                static bool g_mupdf_trace_exceptions = false;
            #else
                static bool g_mupdf_trace_director = mupdf::internal_env_flag("MUPDF_trace_director");
                static bool g_mupdf_trace_exceptions = mupdf::internal_env_flag("MUPDF_trace_exceptions");
            #endif

            '''
            )
    if language == 'csharp':
        common += textwrap.dedent(f'''
                /* This is required otherwise compiling the resulting C++ code
                fails with:
                    error: use of undeclared identifier 'SWIG_fail'

                But no idea whether it is the 'correct' thing to do; seems odd
                that SWIG doesn't define SWIG_fail itself.
                */
                #define SWIG_fail throw std::runtime_error( e.what());
                ''')

    if language == 'python':
        common += textwrap.dedent(f'''

                static std::string to_stdstring(PyObject* s)
                {{
                    PyObject* repr_str = PyUnicode_AsEncodedString(s, "utf-8", "~E~");
                    const char* repr_str_s = PyBytes_AS_STRING(repr_str);
                    std::string ret = repr_str_s;
                    Py_DECREF(repr_str);
                    Py_DECREF(s);
                    return ret;
                }}

                static std::string py_repr(PyObject* x)
                {{
                    if (!x) return "<C_nullptr>";
                    PyObject* s = PyObject_Repr(x);
                    return to_stdstring(s);
                }}

                static std::string py_str(PyObject* x)
                {{
                    if (!x) return "<C_nullptr>";
                    PyObject* s = PyObject_Str(x);
                    return to_stdstring(s);
                }}

                /* Returns a Python `bytes` containing a copy of a `fz_buffer`'s
                data. If <clear> is true we also clear and trim the buffer. */
                PyObject* ll_fz_buffer_to_bytes_internal(fz_buffer* buffer, int clear)
                {{
                    unsigned char* c = NULL;
                    size_t len = {rename.namespace_ll_fn('fz_buffer_storage')}(buffer, &c);
                    PyObject* ret = PyBytes_FromStringAndSize((const char*) c, (Py_ssize_t) len);
                    if (clear)
                    {{
                        /* We mimic the affects of fz_buffer_extract(), which
                        leaves the buffer with zero capacity. */
                        {rename.namespace_ll_fn('fz_clear_buffer')}(buffer);
                        {rename.namespace_ll_fn('fz_trim_buffer')}(buffer);
                    }}
                    return ret;
                }}

                /* Returns a Python `memoryview` for specified memory. */
                PyObject* python_memoryview_from_memory( void* data, size_t size, int writable)
                {{
                    return PyMemoryView_FromMemory(
                            (char*) data,
                            (Py_ssize_t) size,
                            writable ? PyBUF_WRITE : PyBUF_READ
                            );
                }}

                /* Returns a Python `memoryview` for a `fz_buffer`'s data. */
                PyObject* ll_fz_buffer_storage_memoryview(fz_buffer* buffer, int writable)
                {{
                    unsigned char* data = NULL;
                    size_t len = {rename.namespace_ll_fn('fz_buffer_storage')}(buffer, &data);
                    return python_memoryview_from_memory( data, len, writable);
                }}

                /* Creates Python bytes from copy of raw data. */
                PyObject* raw_to_python_bytes(const unsigned char* c, size_t len)
                {{
                    return PyBytes_FromStringAndSize((const char*) c, (Py_ssize_t) len);
                }}

                /* Creates Python bytes from copy of raw data. */
                PyObject* raw_to_python_bytes(const void* c, size_t len)
                {{
                    return PyBytes_FromStringAndSize((const char*) c, (Py_ssize_t) len);
                }}

                /* The SWIG wrapper for this function returns a SWIG proxy for
                a 'const unsigned char*' pointing to the raw data of a python
                bytes. This proxy can then be passed from Python to functions
                that take a 'const unsigned char*'.

                For example to create a MuPDF fz_buffer* from a copy of a
                Python bytes instance:
                    bs = b'qwerty'
                    buffer_ = mupdf.fz_new_buffer_from_copied_data(mupdf.python_buffer_data(bs), len(bs))
                */
                const unsigned char* python_buffer_data(
                        const unsigned char* PYTHON_BUFFER_DATA,
                        size_t PYTHON_BUFFER_SIZE
                        )
                {{
                    return PYTHON_BUFFER_DATA;
                }}

                unsigned char* python_mutable_buffer_data(
                        unsigned char* PYTHON_BUFFER_MUTABLE_DATA,
                        size_t PYTHON_BUFFER_MUTABLE_SIZE
                        )
                {{
                    return PYTHON_BUFFER_MUTABLE_DATA;
                }}

                /* Casts an integer to a pdf_obj*. Used to convert SWIG's int
                values for PDF_ENUM_NAME_* into {rename.class_('pdf_obj')}'s. */
                pdf_obj* obj_enum_to_obj(int n)
                {{
                    return (pdf_obj*) (intptr_t) n;
                }}

                /* SWIG-friendly alternative to {rename.ll_fn('pdf_set_annot_color')}(). */
                void {rename.ll_fn('pdf_set_annot_color2')}(pdf_annot *annot, int n, float color0, float color1, float color2, float color3)
                {{
                    float color[] = {{ color0, color1, color2, color3 }};
                    return {rename.namespace_ll_fn('pdf_set_annot_color')}(annot, n, color);
                }}


                /* SWIG-friendly alternative to {rename.ll_fn('pdf_set_annot_interior_color')}(). */
                void {rename.ll_fn('pdf_set_annot_interior_color2')}(pdf_annot *annot, int n, float color0, float color1, float color2, float color3)
                {{
                    float color[] = {{ color0, color1, color2, color3 }};
                    return {rename.namespace_ll_fn('pdf_set_annot_interior_color')}(annot, n, color);
                }}

                /* SWIG-friendly alternative to `fz_fill_text()`. */
                void ll_fz_fill_text2(
                        fz_device* dev,
                        const fz_text* text,
                        fz_matrix ctm,
                        fz_colorspace* colorspace,
                        float color0,
                        float color1,
                        float color2,
                        float color3,
                        float alpha,
                        fz_color_params color_params
                        )
                {{
                    float color[] = {{color0, color1, color2, color3}};
                    return {rename.namespace_ll_fn( 'fz_fill_text')}(dev, text, ctm, colorspace, color, alpha, color_params);
                }}

                std::vector<unsigned char> {rename.fn('fz_memrnd2')}(int length)
                {{
                    std::vector<unsigned char>  ret(length);
                    {rename.namespace_fn('fz_memrnd')}(&ret[0], length);
                    return ret;
                }}


                /* mupdfpy optimisation for copying pixmap. Copies first <n>
                bytes of each pixel from <src> to <pm>. <pm> and <src> must
                have same `.w` and `.h` */
                void ll_fz_pixmap_copy( fz_pixmap* pm, const fz_pixmap* src, int n)
                {{
                    assert( pm->w == src->w);
                    assert( pm->h == src->h);
                    assert( n <= pm->n);
                    assert( n <= src->n);

                    if (pm->n == src->n)
                    {{
                        // identical samples
                        assert( pm->stride == src->stride);
                        memcpy( pm->samples, src->samples, pm->w * pm->h * pm->n);
                    }}
                    else
                    {{
                        for ( int y=0; y<pm->h; ++y)
                        {{
                            for ( int x=0; x<pm->w; ++x)
                            {{
                                memcpy(
                                        pm->samples + pm->stride * y + pm->n * x,
                                        src->samples + src->stride * y + src->n * x,
                                        n
                                        );
                                if (pm->alpha)
                                {{
                                    src->samples[ src->stride * y + src->n * x] = 255;
                                }}
                            }}
                        }}
                    }}
                }}

                /* mupdfpy optimisation for copying raw data into pixmap. `samples` must
                have enough data to fill the pixmap. */
                void ll_fz_pixmap_copy_raw( fz_pixmap* pm, const void* samples)
                {{
                    memcpy(pm->samples, samples, pm->stride * pm->h);
                }}
                ''')

    common += textwrap.dedent(f'''
            /* SWIG-friendly alternative to fz_runetochar(). */
            std::vector<unsigned char> {rename.fn('fz_runetochar2')}(int rune)
            {{
                std::vector<unsigned char>  buffer(10);
                int n = {rename.namespace_ll_fn('fz_runetochar')}((char*) &buffer[0], rune);
                assert(n < sizeof(buffer));
                buffer.resize(n);
                return buffer;
            }}

            /* SWIG-friendly alternatives to fz_make_bookmark() and
            {rename.fn('fz_lookup_bookmark')}(), using long long instead of fz_bookmark
            because SWIG appears to treat fz_bookmark as an int despite it
            being a typedef for intptr_t, so ends up slicing. */
            long long unsigned {rename.ll_fn('fz_make_bookmark2')}(fz_document* doc, fz_location loc)
            {{
                fz_bookmark bm = {rename.namespace_ll_fn('fz_make_bookmark')}(doc, loc);
                return (long long unsigned) bm;
            }}

            fz_location {rename.ll_fn('fz_lookup_bookmark2')}(fz_document *doc, long long unsigned mark)
            {{
                return {rename.namespace_ll_fn('fz_lookup_bookmark')}(doc, (fz_bookmark) mark);
            }}
            {rename.namespace_class('fz_location')} {rename.fn('fz_lookup_bookmark2')}( {rename.namespace_class('fz_document')} doc, long long unsigned mark)
            {{
                return {rename.namespace_class('fz_location')}( {rename.ll_fn('fz_lookup_bookmark2')}(doc.m_internal, mark));
            }}

            struct {rename.fn('fz_convert_color2_v')}
            {{
                float v0;
                float v1;
                float v2;
                float v3;
            }};

            /* SWIG-friendly alternative for
            {rename.ll_fn('fz_convert_color')}(), taking `float* sv`. */
            void {rename.ll_fn('fz_convert_color2')}(
                    fz_colorspace *ss,
                    float* sv,
                    fz_colorspace *ds,
                    {rename.fn('fz_convert_color2_v')}* dv,
                    fz_colorspace *is,
                    fz_color_params params
                    )
            {{
                //float sv[] = {{ sv0, sv1, sv2, sv3}};
                {rename.namespace_ll_fn('fz_convert_color')}(ss, sv, ds, &dv->v0, is, params);
            }}

            /* SWIG-friendly alternative for
            {rename.ll_fn('fz_convert_color')}(), taking four explicit `float`
            values for `sv`. */
            void {rename.ll_fn('fz_convert_color2')}(
                    fz_colorspace *ss,
                    float sv0,
                    float sv1,
                    float sv2,
                    float sv3,
                    fz_colorspace *ds,
                    {rename.fn('fz_convert_color2_v')}* dv,
                    fz_colorspace *is,
                    fz_color_params params
                    )
            {{
                float sv[] = {{ sv0, sv1, sv2, sv3}};
                {rename.namespace_ll_fn('fz_convert_color')}(ss, sv, ds, &dv->v0, is, params);
            }}

            /* SWIG- Director class to allow fz_set_warning_callback() and
            fz_set_error_callback() to be used with Python callbacks. Note that
            we rename print() to _print() to match what SWIG does. */
            struct DiagnosticCallback
            {{
                /* `description` must be "error" or "warning". */
                DiagnosticCallback(const char* description)
                :
                m_description(description)
                {{
                    #ifndef NDEBUG
                    if (g_mupdf_trace_director)
                    {{
                        std::cerr
                                << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ":"
                                << " DiagnosticCallback[" << m_description << "]() constructor."
                                << "\\n";
                    }}
                    #endif
                    if (m_description == "warning")
                    {{
                        mupdf::ll_fz_set_warning_callback( s_print, this);
                    }}
                    else if (m_description == "error")
                    {{
                        mupdf::ll_fz_set_error_callback( s_print, this);
                    }}
                    else
                    {{
                        std::cerr
                                << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ":"
                                << " DiagnosticCallback() constructor"
                                << " Unrecognised description: " << m_description
                                << "\\n";
                        assert(0);
                    }}
                }}
                virtual void _print( const char* message)
                {{
                    #ifndef NDEBUG
                    if (g_mupdf_trace_director)
                    {{
                        std::cerr
                                << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ":"
                                << " DiagnosticCallback[" << m_description << "]::_print()"
                                << " called (no derived class?)" << " message: " << message
                                << "\\n";
                    }}
                    #endif
                }}
                virtual ~DiagnosticCallback()
                {{
                    #ifndef NDEBUG
                    if (g_mupdf_trace_director)
                    {{
                        std::cerr
                                << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ":"
                                << " ~DiagnosticCallback[" << m_description << "]() destructor called"
                                << " this=" << this
                                << "\\n";
                    }}
                    #endif
                }}
                static void s_print( void* self0, const char* message)
                {{
                    DiagnosticCallback* self = (DiagnosticCallback*) self0;
                    try
                    {{
                        return self->_print( message);
                    }}
                    catch (std::exception& e)
                    {{
                        /* It's important to swallow any exception from
                        self->_print() because fz_set_warning_callback() and
                        fz_set_error_callback() specifically require that
                        the callback does not throw. But we always output a
                        diagnostic. */
                        std::cerr
                                << "DiagnosticCallback[" << self->m_description << "]::s_print()"
                                << " ignoring exception from _print(): "
                                << e.what()
                                << "\\n";
                    }}
                }}
                std::string m_description;
            }};

            struct StoryPositionsCallback
            {{
                StoryPositionsCallback()
                {{
                    //printf( "StoryPositionsCallback() constructor\\n");
                }}

                virtual void call( const fz_story_element_position* position) = 0;

                static void s_call( fz_context* ctx, void* self0, const fz_story_element_position* position)
                {{
                    //printf( "StoryPositionsCallback::s_call()\\n");
                    (void) ctx;
                    StoryPositionsCallback* self = (StoryPositionsCallback*) self0;
                    self->call( position);
                }}

                virtual ~StoryPositionsCallback()
                {{
                    //printf( "StoryPositionsCallback() destructor\\n");
                }}
            }};

            void ll_fz_story_positions_director( fz_story *story, StoryPositionsCallback* cb)
            {{
                //printf( "ll_fz_story_positions_director()\\n");
                {rename.namespace_ll_fn('fz_story_positions')}(
                        story,
                        StoryPositionsCallback::s_call,
                        cb
                        );
            }}

            void Pixmap_set_alpha_helper(
                int balen,
                int n,
                int data_len,
                int zero_out,
                unsigned char* data,
                fz_pixmap* pix,
                int premultiply,
                int bground,
                const std::vector<int>& colors,
                const std::vector<int>& bgcolor
                )
            {{
                int i = 0;
                int j = 0;
                int k = 0;
                int data_fix = 255;
                while (i < balen) {{
                    unsigned char alpha = data[k];
                    if (zero_out) {{
                        for (j = i; j < i+n; j++) {{
                            if (pix->samples[j] != (unsigned char) colors[j - i]) {{
                                data_fix = 255;
                                break;
                            }} else {{
                                data_fix = 0;
                            }}
                        }}
                    }}
                    if (data_len) {{
                        if (data_fix == 0) {{
                            pix->samples[i+n] = 0;
                        }} else {{
                            pix->samples[i+n] = alpha;
                        }}
                        if (premultiply && !bground) {{
                            for (j = i; j < i+n; j++) {{
                                pix->samples[j] = fz_mul255(pix->samples[j], alpha);
                            }}
                        }} else if (bground) {{
                            for (j = i; j < i+n; j++) {{
                                int m = (unsigned char) bgcolor[j - i];
                                pix->samples[j] = m + fz_mul255((pix->samples[j] - m), alpha);
                            }}
                        }}
                    }} else {{
                        pix->samples[i+n] = data_fix;
                    }}
                    i += n+1;
                    k += 1;
                }}
            }}

            void page_merge_helper(
                    {rename.namespace_class('pdf_obj')}& old_annots,
                    {rename.namespace_class('pdf_graft_map')}& graft_map,
                    {rename.namespace_class('pdf_document')}& doc_des,
                    {rename.namespace_class('pdf_obj')}& new_annots,
                    int n
                    )
            {{
                for ( int i=0; i<n; ++i)
                {{
                    {rename.namespace_class('pdf_obj')} o = {rename.namespace_fn('pdf_array_get')}( old_annots, i);
                    if ({rename.namespace_fn('pdf_dict_gets')}( o, "IRT").m_internal)
                        continue;
                    {rename.namespace_class('pdf_obj')} subtype = {rename.namespace_fn('pdf_dict_get')}( o, PDF_NAME(Subtype));
                    if ( {rename.namespace_fn('pdf_name_eq')}( subtype, PDF_NAME(Link)))
                        continue;
                    if ( {rename.namespace_fn('pdf_name_eq')}( subtype, PDF_NAME(Popup)))
                        continue;
                    if ( {rename.namespace_fn('pdf_name_eq')}( subtype, PDF_NAME(Widget)))
                    {{
                        /* fixme: C++ API doesn't yet wrap fz_warn() - it
                        excludes all variadic fns. */
                        //mupdf::fz_warn( "skipping widget annotation");
                        continue;
                    }}
                    {rename.namespace_fn('pdf_dict_del')}( o, PDF_NAME(Popup));
                    {rename.namespace_fn('pdf_dict_del')}( o, PDF_NAME(P));
                    {rename.namespace_class('pdf_obj')} copy_o = {rename.namespace_fn('pdf_graft_mapped_object')}( graft_map, o);
                    {rename.namespace_class('pdf_obj')} annot = {rename.namespace_fn('pdf_new_indirect')}( doc_des, {rename.namespace_fn('pdf_to_num')}( copy_o), 0);
                    {rename.namespace_fn('pdf_array_push')}( new_annots, annot);
                }}
            }}
            ''')

    common += generated.swig_cpp
    common += translate_ucdn_macros( build_dirs)

    text = ''

    text += '%module(directors="1") mupdf\n'
    for i in generated.virtual_fnptrs:
        text += f'%feature("director") {i};\n'

    text += f'%feature("director") DiagnosticCallback;\n'
    text += f'%feature("director") StoryPositionsCallback;\n'

    text += textwrap.dedent(
    '''
    %feature("director:except")
    {
        if ($error != NULL)
        {
            /*
            This is how we can end up here:

            1. Python code calls a function in the Python `mupdf` module.
            2. - which calls SWIG C++ code.
            3. - which calls MuPDF C++ API wrapper function.
            4. - which calls MuPDF C code which calls an MuPDF struct's function pointer.
            5. - which calls MuPDF C++ API Director wrapper (e.g. mupdf::FzDevice2) virtual function.
            6. - which calls SWIG Director C++ code.
            7. - which calls Python derived class's method, which raises a Python exception.

            The exception propagates back up the above stack, being converted
            into different exception representations as it goes:

            6. SWIG Director C++ code (here). We raise a C++ exception.
            5. MuPDF C++ API Director wrapper converts the C++ exception into a MuPDF fz_try/catch exception.
            4. MuPDF C code allows the exception to propogate or catches and rethrows or throws a new fz_try/catch exception.
            3. MuPDF C++ API wrapper function converts the fz_try/catch exception into a C++ exception.
            2. SWIG C++ code converts the C++ exception into a Python exception.
            1. Python code receives the Python exception.

            So the exception changes from a Python exception, to a C++
            exception, to a fz_try/catch exception, to a C++ exception, and
            finally back into a Python exception.

            Each of these stages is necessary. In particular we cannot let the
            first C++ exception propogate directly through MuPDF C code without
            being a fz_try/catch exception, because it would mess up MuPDF C
            code's fz_try/catch exception stack.

            Unfortuntately MuPDF fz_try/catch exception strings are limited to
            256 characters so some or all of our detailed backtrace information
            is lost.
            */

            /* Get text description of the Python exception. */
            PyObject* etype;
            PyObject* obj;
            PyObject* trace;
            PyErr_Fetch( &etype, &obj, &trace);

            /* Looks like PyErr_GetExcInfo() fails here, returning NULL.*/
            /*
            PyErr_GetExcInfo( &etype, &obj, &trace);
            std::cerr << "PyErr_GetExcInfo(): etype: " << py_str(etype) << "\\n";
            std::cerr << "PyErr_GetExcInfo(): obj: " << py_str(obj) << "\\n";
            std::cerr << "PyErr_GetExcInfo(): trace: " << py_str(trace) << "\\n";
            */

            std::string message = "Director error: " + py_str(etype) + ": " + py_str(obj) + "\\n";

            if (g_mupdf_trace_director)
            {
                /* __FILE__ and __LINE__ are not useful here because SWIG makes
                them point to the generic .i code. */
                std::cerr << "========\\n";
                std::cerr << "g_mupdf_trace_director set: Converting Python error into C++ exception:" << "\\n";
                #ifndef _WIN32
                    std::cerr << "    function: " << __PRETTY_FUNCTION__ << "\\n";
                #endif
                std::cerr << "    etype: " << py_str(etype) << "\\n";
                std::cerr << "    obj:   " << py_str(obj) << "\\n";
                std::cerr << "    trace: " << py_str(trace) << "\\n";
                std::cerr << "========\\n";
            }

            PyObject* traceback = PyImport_ImportModule("traceback");
            if (traceback)
            {
                /* Use traceback.format_tb() to get backtrace. */
                if (0)
                {
                    message += "Traceback (from traceback.format_tb()):\\n";
                    PyObject* traceback_dict = PyModule_GetDict(traceback);
                    PyObject* format_tb = PyDict_GetItem(traceback_dict, PyString_FromString("format_tb"));
                    PyObject* ret = PyObject_CallFunctionObjArgs(format_tb, trace, NULL);
                    PyObject* iter = PyObject_GetIter(ret);
                    for(;;)
                    {
                        PyObject* item = PyIter_Next(iter);
                        if (!item) break;
                        message += py_str(item);
                        Py_DECREF(item);
                    }
                    /* `format_tb` and `traceback_dict` are borrowed references.
                    */
                    Py_XDECREF(iter);
                    Py_XDECREF(ret);
                    Py_XDECREF(traceback);
                }

                /* Use exception_info() (copied from mupdf/scripts/jlib.py) to get
                detailed backtrace. */
                if (1)
                {
                    PyObject* globals = PyEval_GetGlobals();
                    PyObject* exception_info = PyDict_GetItemString(globals, "exception_info");
                    PyObject* string_return = PyUnicode_FromString("return");
                    PyObject* ret = PyObject_CallFunctionObjArgs(
                            exception_info,
                            trace,
                            Py_None,
                            string_return,
                            NULL
                            );
                    Py_XDECREF(string_return);
                    message += py_str(ret);
                    Py_XDECREF(ret);
                }
            }
            else
            {
                message += "[No backtrace available.]\\n";
            }

            Py_XDECREF(etype);
            Py_XDECREF(obj);
            Py_XDECREF(trace);

            message += "Exception was from C++/Python callback:\\n";
            message += "    ";
            #ifdef _WIN32
                message += __FUNCTION__;
            #else
                message += __PRETTY_FUNCTION__;
            #endif
            message += "\\n";

            if (1 || g_mupdf_trace_director)
            {
                std::cerr << "========\\n";
                std::cerr << "Director exception handler, message is:\\n" << message << "\\n";
                std::cerr << "========\\n";
            }

            /* SWIG 4.1 documention talks about throwing a
            Swig::DirectorMethodException here, but this doesn't work for us
            because it sets Python's error state again, which makes the
            next SWIG call of a C/C++ function appear to fail.
            //throw Swig::DirectorMethodException();
            */
            throw std::runtime_error( message.c_str());
        }
    }
    ''')

    # Ignore all C MuPDF functions; SWIG will still look at the C++ API in
    # namespace mudf.
    for fnname in generated.c_functions:
        if fnname in ('pdf_annot_type', 'pdf_widget_type'):
            # These are also enums which we don't want to ignore. SWIGing the
            # functions is hopefully harmless.
            pass
        elif fnname in ('x0', 'y0', 'x1', 'y1'):
            # Windows appears to have functions called y0() and y1() e.g. in:
            #
            #  C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.18362.0\\ucrt\\corecrt_math.h
            #
            # If we use `%ignore` with these, e.g. `%ignore ::y0`, swig
            # unhelpfully seems to also ignore any member variables called `y0`
            # or `y1`.
            #
            jlib.log('Not ignoring {fnname=} because breaks wrapping of fz_rect.')
            pass
        else:
            text += f'%ignore ::{fnname};\n'

    # Attempt to move C structs out of the way to allow wrapper classes to have
    # the same name as the struct they wrap. Unfortunately this causes a small
    # number of obscure errors from SWIG.
    if 0:
        for name in generated.c_structs:
            text += f'%rename(lll_{name}) ::{name};\n'

    for i in (
            'fz_append_vprintf',
            'fz_error_stack_slot',
            'fz_format_string',
            'fz_vsnprintf',
            'fz_vthrow',
            'fz_vwarn',
            'fz_write_vprintf',
            'fz_vlog_error_printf',

            'fz_utf8_from_wchar',
            'fz_wchar_from_utf8',
            'fz_fopen_utf8',
            'fz_remove_utf8',
            'fz_argv_from_wargv',
            'fz_free_argv',
            'fz_stdods',
            ):
        text += f'%ignore {i};\n'
        text += f'%ignore {rename.method(None, i)};\n'

    text += textwrap.dedent(f'''
            // Not implemented in mupdf.so: fz_colorspace_name_process_colorants
            %ignore fz_colorspace_name_process_colorants;
            %ignore fz_argv_from_wargv;

            %ignore fz_open_file_w;

            %ignore {rename.ll_fn('fz_append_vprintf')};
            %ignore {rename.ll_fn('fz_error_stack_slot_s')};
            %ignore {rename.ll_fn('fz_format_string')};
            %ignore {rename.ll_fn('fz_vsnprintf')};
            %ignore {rename.ll_fn('fz_vthrow')};
            %ignore {rename.ll_fn('fz_vwarn')};
            %ignore {rename.ll_fn('fz_write_vprintf')};
            %ignore {rename.ll_fn('fz_vlog_error_printf')};
            %ignore {rename.ll_fn('fz_open_file_w')};

            // Ignore custom C++ variadic fns.
            %ignore {rename.ll_fn('pdf_dict_getlv')};
            %ignore {rename.ll_fn('pdf_dict_getl')};
            %ignore {rename.fn('pdf_dict_getlv')};
            %ignore {rename.fn('pdf_dict_getl')};

            // SWIG can't handle this because it uses a valist.
            %ignore {rename.ll_fn('Memento_vasprintf')};
            %ignore {rename.fn('Memento_vasprintf')};

            // These appear to be not present in Windows debug builds.
            %ignore fz_assert_lock_held;
            %ignore fz_assert_lock_not_held;
            %ignore fz_lock_debug_lock;
            %ignore fz_lock_debug_unlock;

            %ignore Memento_cpp_new;
            %ignore Memento_cpp_delete;
            %ignore Memento_cpp_new_array;
            %ignore Memento_cpp_delete_array;
            %ignore Memento_showHash;

            // asprintf() isn't available on Windows, so exclude Memento_asprintf because
            // it is #define-d to asprintf.
            %ignore {rename.ll_fn('Memento_asprintf')};
            %ignore {rename.fn('Memento_asprintf')};

            // Might prefer to #include mupdf/exceptions.h and make the
            // %exception block below handle all the different exception types,
            // but swig-3 cannot parse 'throw()' in mupdf/exceptions.h.
            //
            // So for now we just #include <stdexcept> and handle
            // std::exception only.

            %include "typemaps.i"
            %include "cpointer.i"

            // This appears to allow python to call fns taking an int64_t.
            %include "stdint.i"

            /*
            This is only documented for Ruby, but is mentioned for Python at
            https://sourceforge.net/p/swig/mailman/message/4867286/.

            It makes the Python wrapper for `FzErrorBase` inherit Python's
            `Exception` instead of `object`, which in turn means it can be
            caught in Python with `except Exception as e: ...` or similar.

            Note that while it will have the underlying C++ class's `what()`
            method, this is not used by the `__str__()` and `__repr__()`
            methods. Instead:

                `__str__()` appears to return a tuple of the constructor args
                that were originally used to create the exception object with
                `PyObject_CallObject(class_, args)`.

                `__repr__()` returns a SWIG-style string such as
                `<texcept.MyError; proxy of <Swig Object of type 'MyError *' at
                0xb61ebfabc00> >`.

            We explicitly overwrite `__str__()` to call `what()`.
            */
            %feature("exceptionclass")  FzErrorBase;

            %{{
            ''')

    text += common

    text += textwrap.dedent(f'''
            %}}

            %include exception.i
            %include std_string.i
            %include carrays.i
            %include cdata.i
            %include std_vector.i
            {"%include argcargv.i" if language=="python" else ""}

            %array_class(unsigned char, uchar_array);

            %include <cstring.i>

            namespace std
            {{
                %template(vectoruc) vector<unsigned char>;
                %template(vectori) vector<int>;
                %template(vectors) vector<std::string>;
                %template(vectorq) vector<{rename.namespace_class("fz_quad")}>;
                %template(vector_search_page2_hit) vector<fz_search_page2_hit>;
            }};

            // Make sure that operator++() gets converted to __next__().
            //
            // Note that swig already seems to do:
            //
            //     operator* => __ref__
            //     operator== => __eq__
            //     operator!= => __ne__
            //     operator-> => __deref__
            //
            // Just need to add this method to containers that already have
            // begin() and end():
            //     def __iter__( self):
            //         return CppIterator( self)
            //

            %rename(__increment__) *::operator++;

            // Create fns that give access to arrays of some basic types, e.g. bytes_getitem().
            //
            %array_functions(unsigned char, bytes);

            // Useful for fz_stroke_state::dash_list[].
            %array_functions(float, floats);
            ''')

    if language == 'python':
        text += generated.swig_python_exceptions.getvalue()

    text += textwrap.dedent(f'''
            // Ensure SWIG handles OUTPUT params.
            //
            %include "cpointer.i"
            ''')

    if swig_major < 4:
        text += textwrap.dedent(f'''
                // SWIG version is less than 4 so swig is not able to copy
                // across comments from header file into generated code. The
                // next best thing is to use autodoc to make swig at least show
                // some generic information about arg types.
                //
                %feature("autodoc", "3");
                ''')

    text += textwrap.dedent(f'''
            // Tell swig about pdf_clean_file()'s (int,argv)-style args:
            %apply (int ARGC, char **ARGV) {{ (int retainlen, char *retainlist[]) }}
            ''')

    if language == 'python':
        text += textwrap.dedent( '''
                %include pybuffer.i

                /* Convert Python buffer to (const unsigned char*, size_t) pair
                for python_buffer_data(). */
                %pybuffer_binary(
                        const unsigned char* PYTHON_BUFFER_DATA,
                        size_t PYTHON_BUFFER_SIZE
                        );
                /* Convert Python buffer to (unsigned char*, size_t) pair for
                python_mutable_bytes_data(). */
                %pybuffer_mutable_binary(
                        unsigned char* PYTHON_BUFFER_MUTABLE_DATA,
                        size_t PYTHON_BUFFER_MUTABLE_SIZE
                        );
                '''
                )

    text += common

    if language == 'python':

        text += textwrap.dedent(f'''
                %pointer_functions(int, pint);

                %pythoncode %{{

                import inspect
                import os
                import re
                import sys
                import traceback

                def log( text):
                    print( text, file=sys.stderr)

                g_mupdf_trace_director = (os.environ.get('MUPDF_trace_director') == '1')

                def fz_lookup_metadata(document, key):
                    """
                    Like fz_lookup_metadata2() but returns None on error
                    instead of raising exception.
                    """
                    try:
                        return fz_lookup_metadata2(document, key)
                    except Exception:
                        return
                {rename.class_('fz_document')}.{rename.method('fz_document', 'fz_lookup_metadata')} \
                        = fz_lookup_metadata

                def pdf_lookup_metadata(document, key):
                    """
                    Likepsd_lookup_metadata2() but returns None on error
                    instead of raising exception.
                    """
                    try:
                        return pdf_lookup_metadata2(document, key)
                    except Exception:
                        return
                {rename.class_('pdf_document')}.{rename.method('pdf_document', 'pdf_lookup_metadata')} \
                        = pdf_lookup_metadata

                ''')

        exception_info_text = inspect.getsource(jlib.exception_info)
        text += 'import inspect\n'
        text += 'import io\n'
        text += 'import os\n'
        text += 'import sys\n'
        text += 'import traceback\n'
        text += 'import types\n'
        text += exception_info_text

    if language == 'python':
        # Make some additions to the generated Python module.
        #
        # E.g. python wrappers for functions that take out-params should return
        # tuples.
        #
        text += generated.swig_python
        text += generated.swig_python_set_error_classes.getvalue()

        def set_class_method(struct, fn):
            return f'{rename.class_(struct)}.{rename.method(struct, fn)} = {fn}'

        text += textwrap.dedent(f'''

                # Wrap fz_parse_page_range() to fix SWIG bug where a NULL return
                # value seems to mess up the returned list - we end up with ret
                # containing two elements rather than three, e.g. [0, 2]. This
                # occurs with SWIG-3.0; maybe fixed in SWIG-4?
                #
                ll_fz_parse_page_range_orig = ll_fz_parse_page_range
                def ll_fz_parse_page_range(s, n):
                    ret = ll_fz_parse_page_range_orig(s, n)
                    if len(ret) == 2:
                        return None, 0, 0
                    else:
                        return ret[0], ret[1], ret[2]
                fz_parse_page_range = ll_fz_parse_page_range

                # Provide native python implementation of format_output_path() (->
                # fz_format_output_path).
                #
                def ll_fz_format_output_path( format, page):
                    m = re.search( '(%[0-9]*d)', format)
                    if m:
                        ret = format[ :m.start(1)] + str(page) + format[ m.end(1):]
                    else:
                        dot = format.rfind( '.')
                        if dot < 0:
                            dot = len( format)
                        ret = format[:dot] + str(page) + format[dot:]
                    return ret
                fz_format_output_path = ll_fz_format_output_path

                class IteratorWrap:
                    """
                    This is a Python iterator for containers that have C++-style
                    begin() and end() methods that return iterators.

                    Iterators must have the following methods:

                        __increment__(): move to next item in the container.
                        __ref__(): return reference to item in the container.

                    Must also be able to compare two iterators for equality.

                    """
                    def __init__( self, container):
                        self.container = container
                        self.pos = None
                        self.end = container.end()
                    def __iter__( self):
                        return self
                    def __next__( self):    # for python2.
                        if self.pos is None:
                            self.pos = self.container.begin()
                        else:
                            self.pos.__increment__()
                        if self.pos == self.end:
                            raise StopIteration()
                        return self.pos.__ref__()
                    def next( self):    # for python3.
                        return self.__next__()

                # The auto-generated Python class method
                # {rename.class_('fz_buffer')}.{rename.method('fz_buffer', 'fz_buffer_extract')}() returns (size, data).
                #
                # But these raw values aren't particularly useful to
                # Python code so we change the method to return a Python
                # bytes instance instead, using the special C function
                # buffer_extract_bytes() defined above.
                #
                # The raw values for a buffer are available via
                # fz_buffer_storage().

                def ll_fz_buffer_extract(buffer):
                    """
                    Returns buffer data as a Python bytes instance, leaving the
                    buffer empty.
                    """
                    assert isinstance( buffer, fz_buffer)
                    return ll_fz_buffer_to_bytes_internal(buffer, clear=1)
                def fz_buffer_extract(buffer):
                    """
                    Returns buffer data as a Python bytes instance, leaving the
                    buffer empty.
                    """
                    assert isinstance( buffer, FzBuffer)
                    return ll_fz_buffer_extract(buffer.m_internal)
                {set_class_method('fz_buffer', 'fz_buffer_extract')}

                def ll_fz_buffer_extract_copy( buffer):
                    """
                    Returns buffer data as a Python bytes instance, leaving the
                    buffer unchanged.
                    """
                    assert isinstance( buffer, fz_buffer)
                    return ll_fz_buffer_to_bytes_internal(buffer, clear=0)
                def fz_buffer_extract_copy( buffer):
                    """
                    Returns buffer data as a Python bytes instance, leaving the
                    buffer unchanged.
                    """
                    assert isinstance( buffer, FzBuffer)
                    return ll_fz_buffer_extract_copy(buffer.m_internal)
                {set_class_method('fz_buffer', 'fz_buffer_extract_copy')}

                # [ll_fz_buffer_storage_memoryview() is implemented in C.]
                def fz_buffer_storage_memoryview( buffer, writable=False):
                    """
                    Returns a read-only or writable Python `memoryview` onto
                    `fz_buffer` data. This relies on `buffer` existing and
                    not changing size while the `memoryview` is used.
                    """
                    assert isinstance( buffer, FzBuffer)
                    return ll_fz_buffer_storage_memoryview( buffer.m_internal, writable)
                {set_class_method('fz_buffer', 'fz_buffer_storage_memoryview')}

                # Overwrite wrappers for fz_new_buffer_from_copied_data() to
                # take Python buffer.
                #
                ll_fz_new_buffer_from_copied_data_orig = ll_fz_new_buffer_from_copied_data
                def ll_fz_new_buffer_from_copied_data(data):
                    """
                    Returns fz_buffer containing copy of `data`, which should
                    be a `bytes` or similar Python buffer instance.
                    """
                    buffer_ = ll_fz_new_buffer_from_copied_data_orig(python_buffer_data(data), len(data))
                    return buffer_
                def fz_new_buffer_from_copied_data(data):
                    """
                    Returns FzBuffer containing copy of `data`, which should be
                    a `bytes` or similar Python buffer instance.
                    """
                    return FzBuffer( ll_fz_new_buffer_from_copied_data( data))
                {set_class_method('fz_buffer', 'fz_new_buffer_from_copied_data')}

                def ll_pdf_dict_getl(obj, *tail):
                    """
                    Python implementation of ll_pdf_dict_getl(), because SWIG
                    doesn't handle variadic args. Each item in `tail` should be
                    `mupdf.pdf_obj`.
                    """
                    for key in tail:
                        if not obj:
                            break
                        obj = ll_pdf_dict_get(obj, key)
                    assert isinstance(obj, pdf_obj)
                    return obj
                def pdf_dict_getl(obj, *tail):
                    """
                    Python implementation of pdf_dict_getl(), because SWIG
                    doesn't handle variadic args. Each item in `tail` should be
                    a `mupdf.PdfObj`.
                    """
                    for key in tail:
                        if not obj.m_internal:
                            break
                        obj = pdf_dict_get(obj, key)
                    assert isinstance(obj, PdfObj)
                    return obj
                {set_class_method('pdf_obj', 'pdf_dict_getl')}

                def ll_pdf_dict_putl(obj, val, *tail):
                    """
                    Python implementation of ll_pdf_dict_putl() because SWIG
                    doesn't handle variadic args. Each item in `tail` should
                    be a SWIG wrapper for a `pdf_obj`.
                    """
                    if ll_pdf_is_indirect( obj):
                        obj = ll_pdf_resolve_indirect_chain( obj)
                    if not pdf_is_dict( obj):
                        raise Exception(f'not a dict: {{obj}}')
                    if not tail:
                        return
                    doc = ll_pdf_get_bound_document( obj)
                    for i, key in enumerate( tail[:-1]):
                        assert isinstance( key, PdfObj), f'Item {{i}} in `tail` should be a pdf_obj but is a {{type(key)}}.'
                        next_obj = ll_pdf_dict_get( obj, key)
                        if not next_obj:
                            # We have to create entries
                            next_obj = ll_pdf_new_dict( doc, 1)
                            ll_pdf_dict_put( obj, key, next_obj)
                        obj = next_obj
                    key = tail[-1]
                    ll_pdf_dict_put( obj, key, val)
                def pdf_dict_putl(obj, val, *tail):
                    """
                    Python implementation of pdf_dict_putl(fz_context *ctx,
                    pdf_obj *obj, pdf_obj *val, ...) because SWIG doesn't
                    handle variadic args. Each item in `tail` should
                    be a SWIG wrapper for a `PdfObj`.
                    """
                    if pdf_is_indirect( obj):
                        obj = pdf_resolve_indirect_chain( obj)
                    if not pdf_is_dict( obj):
                        raise Exception(f'not a dict: {{obj}}')
                    if not tail:
                        return
                    doc = pdf_get_bound_document( obj)
                    for i, key in enumerate( tail[:-1]):
                        assert isinstance( key, PdfObj), f'item {{i}} in `tail` should be a PdfObj but is a {{type(key)}}.'
                        next_obj = pdf_dict_get( obj, key)
                        if not next_obj.m_internal:
                            # We have to create entries
                            next_obj = pdf_new_dict( doc, 1)
                            pdf_dict_put( obj, key, next_obj)
                        obj = next_obj
                    key = tail[-1]
                    pdf_dict_put( obj, key, val)
                {set_class_method('pdf_obj', 'pdf_dict_putl')}

                def pdf_dict_putl_drop(obj, *tail):
                    raise Exception('mupdf.pdf_dict_putl_drop() is unsupported and unnecessary in Python because reference counting is automatic. Instead use mupdf.pdf_dict_putl().')
                {set_class_method('pdf_obj', 'pdf_dict_putl_drop')}

                def ll_pdf_set_annot_color(annot, color):
                    """
                    Low-level Python implementation of pdf_set_annot_color()
                    using ll_pdf_set_annot_color2().
                    """
                    if isinstance(color, float):
                        ll_pdf_set_annot_color2(annot, 1, color, 0, 0, 0)
                    elif len(color) == 1:
                        ll_pdf_set_annot_color2(annot, 1, color[0], 0, 0, 0)
                    elif len(color) == 2:
                        ll_pdf_set_annot_color2(annot, 2, color[0], color[1], 0, 0)
                    elif len(color) == 3:
                        ll_pdf_set_annot_color2(annot, 3, color[0], color[1], color[2], 0)
                    elif len(color) == 4:
                        ll_pdf_set_annot_color2(annot, 4, color[0], color[1], color[2], color[3])
                    else:
                        raise Exception( f'Unexpected color should be float or list of 1-4 floats: {{color}}')
                def pdf_set_annot_color(self, color):
                    return ll_pdf_set_annot_color(self.m_internal, color)
                {set_class_method('pdf_annot', 'pdf_set_annot_color')}

                def ll_pdf_set_annot_interior_color(annot, color):
                    """
                    Low-level Python version of pdf_set_annot_color() using
                    pdf_set_annot_color2().
                    """
                    if isinstance(color, float):
                        ll_pdf_set_annot_interior_color2(annot, 1, color, 0, 0, 0)
                    elif len(color) == 1:
                        ll_pdf_set_annot_interior_color2(annot, 1, color[0], 0, 0, 0)
                    elif len(color) == 2:
                        ll_pdf_set_annot_interior_color2(annot, 2, color[0], color[1], 0, 0)
                    elif len(color) == 3:
                        ll_pdf_set_annot_interior_color2(annot, 3, color[0], color[1], color[2], 0)
                    elif len(color) == 4:
                        ll_pdf_set_annot_interior_color2(annot, 4, color[0], color[1], color[2], color[3])
                    else:
                        raise Exception( f'Unexpected color should be float or list of 1-4 floats: {{color}}')
                def pdf_set_annot_interior_color(self, color):
                    """
                    Python version of pdf_set_annot_color() using
                    pdf_set_annot_color2().
                    """
                    return ll_pdf_set_annot_interior_color(self.m_internal, color)
                {set_class_method('pdf_annot', 'pdf_set_annot_interior_color')}

                def ll_fz_fill_text( dev, text, ctm, colorspace, color, alpha, color_params):
                    """
                    Low-level Python version of fz_fill_text() taking list/tuple for `color`.
                    """
                    color = tuple(color) + (0,) * (4-len(color))
                    assert len(color) == 4, f'color not len 4: len={{len(color)}}: {{color}}'
                    return ll_fz_fill_text2(dev, text, ctm, colorspace, *color, alpha, color_params)
                def fz_fill_text(dev, text, ctm, colorspace, color, alpha, color_params):
                    """
                    Python version of fz_fill_text() taking list/tuple for `color`.
                    """
                    return ll_fz_fill_text(
                            dev.m_internal,
                            text.m_internal,
                            ctm.internal(),
                            colorspace.m_internal,
                            color,
                            alpha,
                            color_params.internal(),
                            )
                {set_class_method('fz_device', 'fz_fill_text')}

                # Override mupdf_convert_color() to return (rgb0, rgb1, rgb2, rgb3).
                def ll_fz_convert_color( ss, sv, ds, is_, params):
                    """
                    Low-level Python version of fz_convert_color().

                    `sv` should be a float or list of 1-4 floats or a SWIG
                    representation of a float*.

                    Returns (dv0, dv1, dv2, dv3).
                    """
                    dv = fz_convert_color2_v()
                    if isinstance( sv, float):
                       ll_fz_convert_color2( ss, sv, 0.0, 0.0, 0.0, ds, dv, is_, params)
                    elif isinstance( sv, (tuple, list)):
                        sv2 = tuple(sv) + (0,) * (4-len(sv))
                        ll_fz_convert_color2( ss, *sv2, ds, dv, is_, params)
                    else:
                        # Assume `sv` is SWIG representation of a `float*`.
                        ll_fz_convert_color2( ss, sv, ds, dv, is_, params)
                    return dv.v0, dv.v1, dv.v2, dv.v3
                def fz_convert_color( ss, sv, ds, is_, params):
                    """
                    Python version of fz_convert_color().

                    `sv` should be a float or list of 1-4 floats or a SWIG
                    representation of a float*.

                    Returns (dv0, dv1, dv2, dv3).
                    """
                    return ll_fz_convert_color( ss.m_internal, sv, ds.m_internal, is_.m_internal, params.internal())
                {set_class_method('fz_colorspace', 'fz_convert_color')}

                # Override fz_set_warning_callback() and
                # fz_set_error_callback() to use Python classes derived from
                # our SWIG Director class DiagnosticCallback (defined in C), so
                # that fnptrs can call Python code.
                #

                # We store DiagnosticCallbackPython instances in these
                # globals to ensure they continue to exist after
                # set_diagnostic_callback() returns.
                #
                set_warning_callback_s = None
                set_error_callback_s = None

                # Override set_error_callback().
                class DiagnosticCallbackPython( DiagnosticCallback):
                    """
                    Overrides Director class DiagnosticCallback's virtual
                    `_print()` method in Python.
                    """
                    def __init__( self, description, printfn):
                        super().__init__( description)
                        self.printfn = printfn
                        if g_mupdf_trace_director:
                            log( f'DiagnosticCallbackPython[{{self.m_description}}].__init__() self={{self!r}} printfn={{printfn!r}}')
                    def __del__( self):
                        if g_mupdf_trace_director:
                            log( f'DiagnosticCallbackPython[{{self.m_description}}].__del__() destructor called.')
                    def _print( self, message):
                        if g_mupdf_trace_director:
                            log( f'DiagnosticCallbackPython[{{self.m_description}}]._print(): Calling self.printfn={{self.printfn!r}} with message={{message!r}}')
                        try:
                            self.printfn( message)
                        except Exception as e:
                            # This shouldn't happen, so always output a diagnostic.
                            log( f'DiagnosticCallbackPython[{{self.m_description}}]._print(): Warning: exception from self.printfn={{self.printfn!r}}: e={{e!r}}')
                            # Calling `raise` here serves to test
                            # `DiagnosticCallback()`'s swallowing of what will
                            # be a C++ exception. But we could swallow the
                            # exception here instead.
                            raise

                def set_diagnostic_callback( description, printfn):
                    if g_mupdf_trace_director:
                        log( f'set_diagnostic_callback() description={{description!r}} printfn={{printfn!r}}')
                    if printfn:
                        ret = DiagnosticCallbackPython( description, printfn)
                        return ret
                    else:
                        if g_mupdf_trace_director:
                            log( f'Calling ll_fz_set_{{description}}_callback() with (None, None)')
                        if description == 'error':
                            ll_fz_set_error_callback( None, None)
                        elif description == 'warning':
                            ll_fz_set_warning_callback( None, None)
                        else:
                            assert 0, f'Unrecognised description={{description!r}}'
                        return None

                def fz_set_error_callback( printfn):
                    global set_error_callback_s
                    set_error_callback_s = set_diagnostic_callback( 'error', printfn)

                def fz_set_warning_callback( printfn):
                    global set_warning_callback_s
                    set_warning_callback_s = set_diagnostic_callback( 'warning', printfn)

                # Direct access to fz_pixmap samples.
                def ll_fz_pixmap_samples_memoryview( pixmap):
                    """
                    Returns a writable Python `memoryview` for a `fz_pixmap`.
                    """
                    assert isinstance( pixmap, fz_pixmap)
                    ret = python_memoryview_from_memory(
                            ll_fz_pixmap_samples( pixmap),
                            ll_fz_pixmap_stride( pixmap) * ll_fz_pixmap_height( pixmap),
                            1, # writable
                            )
                    return ret
                def fz_pixmap_samples_memoryview( pixmap):
                    """
                    Returns a writable Python `memoryview` for a `FzPixmap`.
                    """
                    return ll_fz_pixmap_samples_memoryview( pixmap.m_internal)
                {set_class_method('fz_pixmap', 'fz_pixmap_samples_memoryview')}

                # Avoid potential unsafe use of variadic args by forcing a
                # single arg and escaping all '%' characters. (Passing ('%s',
                # text) does not work - results in "(null)" being output.)
                #
                ll_fz_warn_original = ll_fz_warn
                def ll_fz_warn( text):
                    assert isinstance( text, str), f'text={{text!r}} str={{str!r}}'
                    text = text.replace( '%', '%%')
                    return ll_fz_warn_original( text)
                fz_warn = ll_fz_warn

                # Force use of pdf_load_field_name2() instead of
                # pdf_load_field_name() because the latter returns a char*
                # buffer that must be freed by the caller.
                ll_pdf_load_field_name = ll_pdf_load_field_name2
                pdf_load_field_name = pdf_load_field_name2
                {set_class_method('pdf_obj', 'pdf_load_field_name')}

                # It's important that when we create class derived
                # from StoryPositionsCallback, we ensure that
                # StoryPositionsCallback's constructor is called. Otherwise
                # the new instance doesn't seem to be an instance of
                # StoryPositionsCallback.
                #
                class StoryPositionsCallback_python( StoryPositionsCallback):
                    def __init__( self, python_callback):
                        super().__init__()
                        self.python_callback = python_callback
                    def call( self, position):
                        self.python_callback( position)

                ll_fz_story_positions_orig = ll_fz_story_positions
                def ll_fz_story_positions( story, python_callback):
                    """
                    Custom replacement for `ll_fz_story_positions()` that takes
                    a Python callable `python_callback`.
                    """
                    #log( f'll_fz_story_positions() type(story)={{type(story)!r}} type(python_callback)={{type(python_callback)!r}}')
                    python_callback_instance = StoryPositionsCallback_python( python_callback)
                    ll_fz_story_positions_director( story, python_callback_instance)
                def fz_story_positions( story, python_callback):
                    #log( f'fz_story_positions() type(story)={{type(story)!r}} type(python_callback)={{type(python_callback)!r}}')
                    assert isinstance( story, FzStory)
                    assert callable( python_callback)
                    def python_callback2( position):
                        position2 = FzStoryElementPosition( position)
                        python_callback( position2)
                    ll_fz_story_positions( story.m_internal, python_callback2)
                {set_class_method('fz_story', 'fz_story_positions')}

                # Monkey-patch `FzDocumentWriter.__init__()` to set `self._out`
                # to any `FzOutput2` arg. This ensures that the Python part of
                # the derived `FzOutput2` instance is kept alive for use by the
                # `FzDocumentWriter`, otherwise Python can delete it, then get
                # a SEGV if C++ tries to call the derived Python methods.
                #
                # [We don't patch equivalent class-aware functions such
                # as `fz_new_pdf_writer_with_output()` because they are
                # not available to C++/Python, because FzDocumentWriter is
                # non-copyable.]
                #
                FzDocumentWriter__init__0 = FzDocumentWriter.__init__
                def FzDocumentWriter__init__1(self, *args):
                    out = None
                    for arg in args:
                        if isinstance( arg, FzOutput2):
                            assert not out, "More than one FzOutput2 passed to FzDocumentWriter.__init__()"
                            out = arg
                    if out:
                        self._out = out
                    return FzDocumentWriter__init__0(self, *args)
                FzDocumentWriter.__init__ = FzDocumentWriter__init__1
                ''')

        # Add __iter__() methods for all classes with begin() and end() methods.
        #
        for classname in generated.container_classnames:
            text += f'{classname}.__iter__ = lambda self: IteratorWrap( self)\n'

        # For all wrapper classes with a to_string() method, add a __str__()
        # method to the underlying struct's Python class, which calls
        # to_string_<structname>().
        #
        # E.g. this allows Python code to print a mupdf.fz_rect instance.
        #
        # [We could instead call our generated to_string() and rely on overloading,
        # but this will end up switching on the type in the SWIG code.]
        #
        for struct_name in generated.to_string_structnames:
            text += f'{struct_name}.__str__ = lambda s: to_string_{struct_name}(s)\n'
            text += f'{struct_name}.__repr__ = lambda s: to_string_{struct_name}(s)\n'

        # For all wrapper classes with a to_string() method, add a __str__() method
        # to the Python wrapper class, which calls the class's to_string() method.
        #
        # E.g. this allows Python code to print a mupdf.Rect instance.
        #
        for struct_name in generated.to_string_structnames:
            text += f'{rename.class_(struct_name)}.__str__ = lambda self: self.to_string()\n'
            text += f'{rename.class_(struct_name)}.__repr__ = lambda self: self.to_string()\n'

        text += '%}\n'

    text2_code = textwrap.dedent( '''
            ''')

    if text2_code.strip():
        text2 = textwrap.dedent( f'''
                %{{
                    #include "mupdf/fitz.h"
                    #include "mupdf/classes.h"
                    #include "mupdf/classes2.h"
                    #include <vector>

                    {text2_code}
                %}}

                %include std_vector.i

                namespace std
                {{
                    %template(vectori) vector<int>;
                }};

                {text2_code}
                ''')
    else:
        text2 = ''

    if 1:   # lgtm [py/constant-conditional-expression]
        # This is a horrible hack to avoid swig failing because
        # include/mupdf/pdf/object.h defines an enum which contains a #include.
        #
        # Would like to pre-process files in advance so that swig doesn't see
        # the #include, but this breaks swig in a different way - swig cannot
        # cope with some code in system headers.
        #
        # So instead we copy include/mupdf/pdf/object.h into
        # {build_dirs.dir_mupdf}/platform/python/include/mupdf/pdf/object.h,
        # manually expanding the #include using a Python .replace() call. Then
        # we specify {build_dirs.dir_mupdf}/platform/python/include as the
        # first include path so that our modified mupdf/pdf/object.h will get
        # included in preference to the original.
        #
        os.makedirs(f'{build_dirs.dir_mupdf}/platform/python/include/mupdf/pdf', exist_ok=True)
        with open( f'{build_dirs.dir_mupdf}/include/mupdf/pdf/object.h') as f:
            o = f.read()
        with open( f'{build_dirs.dir_mupdf}/include/mupdf/pdf/name-table.h') as f:
            name_table_h = f.read()
        oo = o.replace( '#include "mupdf/pdf/name-table.h"\n', name_table_h)
        assert oo != o
        jlib.fs_update( oo, f'{build_dirs.dir_mupdf}/platform/python/include/mupdf/pdf/object.h')

    swig_i      = f'{build_dirs.dir_mupdf}/platform/{language}/mupdfcpp_swig.i'
    include1    = f'{build_dirs.dir_mupdf}/include/'
    include2    = f'{build_dirs.dir_mupdf}/platform/c++/include'
    swig_cpp    = f'{build_dirs.dir_mupdf}/platform/{language}/mupdfcpp_swig.cpp'
    swig_py     = f'{build_dirs.dir_so}/mupdf.py'

    swig2_i     = f'{build_dirs.dir_mupdf}/platform/{language}/mupdfcpp2_swig.i'
    swig2_cpp   = f'{build_dirs.dir_mupdf}/platform/{language}/mupdfcpp2_swig.cpp'
    swig2_py    = f'{build_dirs.dir_so}/mupdf2.py'

    os.makedirs( f'{build_dirs.dir_mupdf}/platform/{language}', exist_ok=True)
    os.makedirs( f'{build_dirs.dir_so}', exist_ok=True)
    util.update_file_regress( text, swig_i, check_regress)
    if text2:
        util.update_file_regress( text2, swig2_i, check_regress)
    else:
        jlib.fs_update( '', swig2_i)

    # Disable some unhelpful SWIG warnings. Must not use -Wall as it overrides
    # all warning disables.
    disable_swig_warnings = [
            201,    # Warning 201: Unable to find 'stddef.h'
            314,    # Warning 314: 'print' is a python keyword, renaming to '_print'
            302,    # Warning 302: Identifier 'pdf_annot_type' redefined (ignored),
            312,    # Warning 312: Nested union not currently supported (ignored).
            321,    # Warning 321: 'max' conflicts with a built-in name in python
            322,    # Warning 322: Redundant redeclaration of 'pdf_annot',
            362,    # Warning 362: operator= ignored
            451,    # Warning 451: Setting a const char * variable may leak memory.
            503,    # Warning 503: Can't wrap 'operator <<' unless renamed to a valid identifier.
            512,    # Warning 512: Overloaded method mupdf::DrawOptions::internal() const ignored, using non-const method mupdf::DrawOptions::internal() instead.
            509,    # Warning 509: Overloaded method mupdf::FzAaContext::FzAaContext(::fz_aa_context const) effectively ignored,
            560,    # Warning 560: Unknown Doxygen command: d.
            ]

    disable_swig_warnings = [ '-' + str( x) for x in disable_swig_warnings]
    disable_swig_warnings = '-w' + ','.join( disable_swig_warnings)

    # Preserve any existing file `swig_cpp`, so that we can restore the
    # mtime if SWIG produces an unchanged file. This then avoids unnecessary
    # recompilation.
    #
    # 2022-11-16: Disabled this, because it can result in continuous
    # unnecessary rebuilds, e.g. if .cpp is older than a mupdf header.
    #
    swig_cpp_old = None
    if 0 and os.path.exists( swig_cpp):
        swig_cpp_old = f'{swig_cpp}-old'
        jlib.fs_copy( swig_cpp, swig_cpp_old)

    if language == 'python':
        # Maybe use '^' on windows as equivalent to unix '\\' for multiline
        # ending?
        def make_command( module, cpp, swig_i):
            cpp = os.path.relpath( cpp)
            swig_i = os.path.relpath( swig_i)
            # We need to predefine MUPDF_FITZ_HEAP_H to disable parsing of
            # include/mupdf/fitz/heap.h. Otherwise swig's preprocessor seems to
            # ignore #undef's in include/mupdf/fitz/heap-imp.h then complains
            # about redefinition of macros in include/mupdf/fitz/heap.h.
            command = (
                    textwrap.dedent(
                    f'''
                    "{swig_command}"
                        {"-D_WIN32" if state_.windows else ""}
                        -c++
                        {"-doxygen" if swig_major >= 4 else ""}
                        -python
                        -Wextra
                        {disable_swig_warnings}
                        -module {module}
                        -outdir {os.path.relpath(build_dirs.dir_so)}
                        -o {cpp}
                        -includeall
                        -I{os.path.relpath(build_dirs.dir_mupdf)}/platform/python/include
                        -I{os.path.relpath(include1)}
                        -I{os.path.relpath(include2)}
                        -ignoremissing
                        -DMUPDF_FITZ_HEAP_H
                        {swig_i}
                    ''').strip().replace( '\n', "" if state_.windows else " \\\n")
                    )
            return command

        def modify_py( rebuilt, swig_py, do_enums):
            if not rebuilt:
                return
            swig_py_leaf = os.path.basename( swig_py)
            assert swig_py_leaf.endswith( '.py')
            so = f'_{swig_py_leaf[:-3]}.so'
            swig_py_tmp = f'{swig_py}-'
            jlib.fs_remove( swig_py_tmp)
            os.rename( swig_py, swig_py_tmp)
            with open( swig_py_tmp) as f:
                swig_py_content = f.read()

            if state_.windows:
                jlib.log('Adding prefix to {swig_cpp=}')
                prefix = ''
                postfix = ''
                with open( swig_cpp) as f:
                    swig_py_content = prefix + swig_py_content + postfix

            if do_enums:
                # Change all our PDF_ENUM_NAME_* enums so that they are actually
                # PdfObj instances so that they can be used like any other PdfObj.
                #
                #jlib.log('{len(generated.c_enums)=}')
                for enum_type, enum_names in generated.c_enums.items():
                    for enum_name in enum_names:
                        if enum_name.startswith( 'PDF_ENUM_NAME_'):
                            swig_py_content += f'{enum_name} = {rename.class_("pdf_obj")}( obj_enum_to_obj( {enum_name}))\n'

            with open( swig_py_tmp, 'w') as f:
                f.write( swig_py_content)
            os.rename( swig_py_tmp, swig_py)

        if text2:
            # Make mupdf2, for mupdfpy optimisations.
            jlib.log( 'Running SWIG to generate mupdf2 .cpp')
            command = make_command( 'mupdf2', swig2_cpp, swig2_i)
            rebuilt = jlib.build(
                    (swig2_i, include1, include2),
                    (swig2_cpp, swig2_py),
                    command,
                    force_rebuild,
                    )
            modify_py( rebuilt, swig2_py, do_enums=False)
        else:
            jlib.fs_update( '', swig2_cpp)
            jlib.fs_remove( swig2_py)

        # Make main mupdf .so.
        command = make_command( 'mupdf', swig_cpp, swig_i)
        rebuilt = jlib.build(
                (swig_i, include1, include2),
                (swig_cpp, swig_py),
                command,
                force_rebuild,
                )
        modify_py( rebuilt, swig_py, do_enums=True)


    elif language == 'csharp':
        outdir = os.path.relpath(f'{build_dirs.dir_mupdf}/platform/csharp')
        os.makedirs(outdir, exist_ok=True)
        # Looks like swig comes up with 'mupdfcpp_swig_wrap.cxx' leafname.
        #
        # We include platform/python/include in order to pick up the modified
        # include/mupdf/pdf/object.h that we generate elsewhere.
        dllimport = 'mupdfcsharp.so'
        if state_.windows:
            # Would like to specify relative path to .dll with:
            #   dllimport = os.path.relpath( f'{build_dirs.dir_so}/mupdfcsharp.dll')
            # but Windows/.NET doesn't seem to support this, despite
            # https://stackoverflow.com/questions/31807289 "how can i add a
            # swig generated c dll reference to a c sharp project".
            #
            dllimport = 'mupdfcsharp.dll'
        command = (
                textwrap.dedent(
                f'''
                "{swig_command}"
                    {"-D_WIN32" if state_.windows else ""}
                    -c++
                    -csharp
                    -Wextra
                    {disable_swig_warnings}
                    -module mupdf
                    -namespace mupdf
                    -dllimport {dllimport}
                    -outdir {outdir}
                    -outfile mupdf.cs
                    -o {os.path.relpath(swig_cpp)}
                    -includeall
                    -I{os.path.relpath(build_dirs.dir_mupdf)}/platform/python/include
                    -I{os.path.relpath(include1)}
                    -I{os.path.relpath(include2)}
                    -ignoremissing
                    -DMUPDF_FITZ_HEAP_H
                    {os.path.relpath(swig_i)}
                ''').strip().replace( '\n', "" if state_.windows else "\\\n")
                )
        rebuilt = jlib.build(
                (swig_i, include1, include2),
                (f'{outdir}/mupdf.cs', os.path.relpath(swig_cpp)),
                command,
                force_rebuild,
                )
        # fixme: use <rebuilt> line with language=='python' to avoid multiple
        # modifications to unchanged mupdf.cs?
        #
        # For classes that have our to_string() method, override C#'s
        # ToString() to call to_string().
        with open(f'{outdir}/mupdf.cs') as f:
            cs = f.read()
        cs2 = re.sub(
                '(( *)public string to_string[(][)])',
                '\\2public override string ToString() { return to_string(); }\n\\1',
                cs,
                )
        jlib.log('{len(cs)=}')
        jlib.log('{len(cs2)=}')
        assert cs2 != cs, f'Failed to add toString() methods.'
        jlib.log('{len(generated.swig_csharp)=}')
        assert len(generated.swig_csharp)
        cs2 += generated.swig_csharp
        jlib.log( 'Updating cs2 => {build_dirs.dir_so}/mupdf.cs')
        jlib.fs_update(cs2, f'{build_dirs.dir_so}/mupdf.cs')
        #jlib.fs_copy(f'{outdir}/mupdf.cs', f'{build_dirs.dir_so}/mupdf.cs')
        jlib.log('{rebuilt=}')

    else:
        assert 0

    if swig_cpp_old:
        with open( swig_cpp_old) as f:
            swig_cpp_contents_old = f.read()
        with open(swig_cpp) as f:
            swig_cpp_contents_new = f.read()
        if swig_cpp_contents_new == swig_cpp_contents_old:
            # File <swig_cpp> unchanged, so restore the mtime to avoid
            # unnecessary recompilation.
            jlib.log( 'File contents unchanged, copying {swig_cpp_old=} => {swig_cpp=}')
            jlib.fs_rename( swig_cpp_old, swig_cpp)


def test_swig():
    '''
    For testing different swig .i constructs.
    '''
    test_i = textwrap.dedent('''
            %include argcargv.i

            %apply (int ARGC, char **ARGV) { (int retainlen, const char **retainlist) }
            %apply (int ARGC, char **ARGV) { (const char **retainlist, int retainlen) }
            %apply (int ARGC, char **ARGV) { (const char *retainlist[], int retainlen) }

            %clear double a, int ARGC, char **ARGV;
            %clear double a, int argc, char *argv[];
            %clear int ARGC, char **ARGV;
            %clear (double a, int ARGC, char **ARGV);
            %clear (double a, int argc, char *argv[]);
            %clear (int ARGC, char **ARGV);
            %clear int retainlen, const char **retainlist;

            int bar( int argc, char* argv[]);
            int foo( double a, int argc, char* argv[]);

            int qwe( double a, int argc, const char** argv);

            void ppdf_clean_file( char *infile, char *outfile, char *password, pdf_write_options *opts, int retainlen, const char **retainlist);
            void ppdf_clean_file2(char *infile, char *outfile, char *password, pdf_write_options *opts, const char **retainlist, int retainlen);
            void ppdf_clean_file3(char *infile, char *outfile, char *password, pdf_write_options *opts, const char *retainlist[], int retainlen);

            ''')
    jlib.fs_update( test_i, 'test.i')

    jlib.system( textwrap.dedent(
            '''
            swig
                -Wall
                -c++
                -python
                -module test
                -outdir .
                -o test.cpp
                test.i
            ''').replace( '\n', ' \\\n')
            )
