'''
Support for using SWIG to generate langauge bindings from the C++ bindings.
'''

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
    jlib.log( '{=build_dirs type(build_dirs)}')
    assert isinstance(build_dirs, state.BuildDirs), type(build_dirs)
    assert isinstance(generated, cpp.Generated), type(generated)
    assert language in ('python', 'csharp')
    # Find version of swig. (We use quotes around <swig> to make things work on
    # Windows.)
    try:
        t = jlib.system( f'"{swig_command}" -version', out='return')
    except Exception as e:
        if state_.windows:
            raise Exception( 'swig failed; on Windows swig can be auto-installed with: --swig-windows-auto') from e
        else:
            raise
    m = re.search( 'SWIG Version ([0-9]+)[.]([0-9]+)[.]([0-9]+)', t)
    assert m
    swig_major = int( m.group(1))

    # Create a .i file for SWIG.
    #
    common = f'''
            #include <stdexcept>

            #include "mupdf/functions.h"
            #include "mupdf/classes.h"
            #include "mupdf/classes2.h"
            #include "mupdf/internal.h"
            #include "mupdf/exceptions.h"
            '''
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
                /* Returns a Python `bytes` containging a copy of a `fz_buffer`'s
                data. If <clear> is true we also clear and trim the buffer. */
                PyObject* python_buffer_to_bytes(fz_buffer* buffer, int clear)
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
                PyObject* python_buffer_to_memoryview(fz_buffer* buffer, int writable)
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
                    buffer_ = mupdf.fz_new_buffer_from_copied_data(mupdf.python_bytes_data(bs), len(bs))
                */
                const unsigned char* python_bytes_data(const unsigned char* PYTHON_BYTES_DATA, size_t PYTHON_BYTES_SIZE)
                {{
                    return PYTHON_BYTES_DATA;
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

                /* Fixme: change this to override low-level wrapper function
                with rename.ll_fn() and patch up callers. */
                /* SWIG-friendly alternative to {rename.fn('fz_fill_text')}(). */
                void {rename.fn('fz_fill_text2')}(
                        {rename.namespace_class('fz_device')}& dev,
                        const {rename.namespace_class('fz_text')}& text,
                        {rename.namespace_class('fz_matrix')}& ctm,
                        const {rename.namespace_class('fz_colorspace')}& colorspace,
                        float color0,
                        float color1,
                        float color2,
                        float color3,
                        float alpha,
                        {rename.namespace_class('fz_color_params')}& color_params
                        )
                {{
                    float color[] = {{color0, color1, color2, color3}};
                    return {rename.namespace_fn('fz_fill_text')}(dev, text, ctm, colorspace, color, alpha, color_params);
                }}

                std::vector<unsigned char> {rename.fn('fz_memrnd2')}(int length)
                {{
                    std::vector<unsigned char>  ret(length);
                    {rename.namespace_fn('fz_memrnd')}(&ret[0], length);
                    return ret;
                }}


                /* mupdfpy optimisation for copying pixmap. Copies first <n>
                bytes of each pixel from <src> to <pm>. <pm> and <src> should
                have same .w and .h */
                void {rename.fn('mupdfpy_pixmap_copy')}( fz_pixmap* pm, const fz_pixmap* src, int n)
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

            /* SWIG-friendly support for fz_set_warning_callback() and
            fz_set_error_callback(). */

            struct SetWarningCallback
            {{
                SetWarningCallback( void* user=NULL)
                {{
                    this->user = user;
                    {rename.namespace_ll_fn('fz_set_warning_callback')}( s_print, this);
                }}
                virtual void print( const char* message)
                {{
                }}
                static void s_print( void* self0, const char* message)
                {{
                    SetWarningCallback* self = (SetWarningCallback*) self0;
                    return self->print( message);
                }}
                void* user;
            }};

            struct SetErrorCallback
            {{
                SetErrorCallback( void* user=NULL)
                {{
                    this->user = user;
                    {rename.namespace_ll_fn('fz_set_error_callback')}( s_print, this);
                }}
                virtual void print( const char* message)
                {{
                }}
                static void s_print( void* self0, const char* message)
                {{
                    SetErrorCallback* self = (SetErrorCallback*) self0;
                    return self->print( message);
                }}
                void* user;
            }};

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

    if state_.windows:
        # 2022-02-24: Director classes break Windows builds at the moment.
        pass
    else:
        text += '%module(directors="1") mupdf\n'
        for i in generated.virtual_fnptrs:
            text += f'%feature("director") {i};\n'

        text += f'%feature("director") SetWarningCallback;\n'
        text += f'%feature("director") SetErrorCallback;\n'

        text += textwrap.dedent(
                '''
                %feature("director:except")
                {
                  if ($error != NULL)
                  {
                    throw Swig::DirectorMethodException();
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
            ):
        text += f'%ignore {i};\n'
        text += f'%ignore m{i};\n'

    text += textwrap.dedent(f'''
            // Not implemented in mupdf.so: fz_colorspace_name_process_colorants
            %ignore fz_colorspace_name_process_colorants;

            %ignore fz_open_file_w;

            %ignore {rename.ll_fn('fz_append_vprintf')};
            %ignore {rename.ll_fn('fz_error_stack_slot_s')};
            %ignore {rename.ll_fn('fz_format_string')};
            %ignore {rename.ll_fn('fz_vsnprintf')};
            %ignore {rename.ll_fn('fz_vthrow')};
            %ignore {rename.ll_fn('fz_vwarn')};
            %ignore {rename.ll_fn('fz_write_vprintf')};
            %ignore {rename.ll_fn('fz_vsnprintf')};
            %ignore {rename.ll_fn('fz_vthrow')};
            %ignore {rename.ll_fn('fz_vwarn')};
            %ignore {rename.ll_fn('fz_append_vprintf')};
            %ignore {rename.ll_fn('fz_write_vprintf')};
            %ignore {rename.ll_fn('fz_format_string')};
            %ignore {rename.ll_fn('fz_open_file_w')};

            // Ignore custom C++ variadic fns.
            %ignore {rename.ll_fn('pdf_dict_getlv')};
            %ignore {rename.ll_fn('pdf_dict_getl')};
            %ignore {rename.fn('pdf_dict_getlv')};
            %ignore {rename.fn('pdf_dict_getl')};

            // SWIG can't handle this because it uses a valist.
            %ignore {rename.ll_fn('Memento_vasprintf')};

            // asprintf() isn't available on windows, so exclude Memento_asprintf because
            // it is #define-d to asprintf.
            %ignore {rename.ll_fn('Memento_asprintf')};

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

            %{{
            {common}
            %}}

            %include exception.i
            %include std_string.i
            %include carrays.i
            %include cdata.i
            %include std_vector.i
            {"%include argcargv.i" if language=="python" else ""}

            %array_class(unsigned char, uchar_array);

            %include <cstring.i>
            %cstring_output_allocate(char **OUTPUT, free($1));

            namespace std
            {{
                %template(vectoruc) vector<unsigned char>;
                %template(vectori) vector<int>;
                %template(vectors) vector<std::string>;
                %template(vectorq) vector<{rename.namespace_class("fz_quad")}>;
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


            %array_functions(unsigned char, bytes);
            ''')

    text += textwrap.dedent(f'''
            %exception {{
                try {{
                    $action
                }}
            ''')
    if not state_.windows:  # Directors not currently supported on Windows.
        text += textwrap.dedent(f'''
                catch (Swig::DirectorException &e) {{
                    SWIG_fail;
                }}
                ''')
    text += textwrap.dedent(f'''
            catch(std::exception& e) {{
                SWIG_exception(SWIG_RuntimeError, e.what());
            }}
            catch(...) {{
                    SWIG_exception(SWIG_RuntimeError, "Unknown exception");
                }}
            }}
            ''')

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

                /* Convert Python bytes to (const unsigned char*, size_t) pair
                for python_bytes_data(). */
                %pybuffer_binary(const unsigned char* PYTHON_BYTES_DATA, size_t PYTHON_BYTES_SIZE);
                '''
                )

    text += common

    if language == 'python':
        text += textwrap.dedent(f'''
                %pointer_functions(int, pint);

                %pythoncode %{{

                def fz_lookup_metadata_extra(self, key):
                    """
                    Python implementation override of {rename.class_('fz_document')}.lookup_metadata().

                    Returns string or None if not found.
                    """
                    e = new_pint()
                    ret = {rename.ll_fn('fz_lookup_metadata')}(self.m_internal, key, e)
                    e = pint_value(e)
                    if e < 0:
                        return None
                    return ret

                {rename.class_('fz_document')}.{rename.method('fz_document', 'fz_lookup_metadata')} = fz_lookup_metadata_extra
                {rename.fn('fz_lookup_metadata')} = fz_lookup_metadata_extra

                def pdf_lookup_metadata_extra(self, key):
                    """
                    Python implementation override of {rename.class_('pdf_document')}.{rename.method('pdf_document', 'pdf_lookup_metadata')}().

                    Returns string or None if not found.
                    """
                    e = new_pint()
                    ret = {rename.ll_fn('pdf_lookup_metadata')}(self.m_internal, key, e)
                    e = pint_value(e)
                    if e < 0:
                        return None
                    return ret

                {rename.class_('pdf_document')}.{rename.method('pdf_document', 'pdf_lookup_metadata')} = pdf_lookup_metadata_extra
                {rename.fn('pdf_lookup_metadata')} = pdf_lookup_metadata_extra
                ''')

    if language == 'python':
        # Make some additions to the generated Python module.
        #
        # E.g. python wrappers for functions that take out-params should return
        # tuples.
        #
        text += generated.swig_python
        text += textwrap.dedent(f'''
                import re

                # Wrap fz_parse_page_range() to fix SWIG bug where a NULL return
                # value seems to mess up the returned list - we end up with ret
                # containing two elements rather than three, e.g. [0, 2]. This
                # occurs with SWIG-3.0; maybe fixed in SWIG-4?
                #
                w_fz_parse_page_range = {rename.ll_fn('fz_parse_page_range')}
                def {rename.fn('fz_parse_page_range')}(s, n):
                    ret = w_fz_parse_page_range(s, n)
                    if len(ret) == 2:
                        return None, 0, 0
                    else:
                        return ret[0], ret[1], ret[2]

                # Provide native python implementation of format_output_path() (->
                # fz_format_output_path).
                #
                def {rename.fn('fz_format_output_path')}( format, page):
                    m = re.search( '(%[0-9]*d)', format)
                    if m:
                        ret = format[ :m.start(1)] + str(page) + format[ m.end(1):]
                    else:
                        dot = format.rfind( '.')
                        if dot < 0:
                            dot = len( format)
                        ret = format[:dot] + str(page) + format[dot:]
                    return ret

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

                def {rename.class_('fz_buffer')}_fz_buffer_extract(self):
                    """
                    Returns buffer data as a Python bytes instance, leaving the
                    buffer empty.
                    """
                    assert isinstance( self, {rename.class_('fz_buffer')})
                    return python_buffer_to_bytes(self.m_internal, clear=1)
                {rename.class_('fz_buffer')}.{rename.method('fz_buffer', 'fz_buffer_extract')} = {rename.class_('fz_buffer')}_fz_buffer_extract
                {rename.fn('fz_buffer_extract')} = {rename.class_('fz_buffer')}_fz_buffer_extract

                def {rename.class_('fz_buffer')}_fz_buffer_extract_copy( self):
                    """
                    Returns buffer data as a Python bytes instance, leaving the
                    buffer unchanged.
                    """
                    assert isinstance( self, {rename.class_('fz_buffer')})
                    return python_buffer_to_bytes(self.m_internal, clear=0)
                {rename.class_('fz_buffer')}.{rename.method('fz_buffer', 'fz_buffer_extract_copy')}  = {rename.class_('fz_buffer')}_fz_buffer_extract_copy
                {rename.fn('fz_buffer_extract_copy')} = {rename.class_('fz_buffer')}_fz_buffer_extract_copy

                def fz_buffer_storage_memoryview( buffer, writable=False):
                    """
                    Returns a read-only or writable Python `memoryview` onto
                    `fz_buffer` data. This relies on `buffer` existing and
                    not changing size while the `memoryview` is used.
                    """
                    return python_buffer_to_memoryview( buffer, writable)
                {rename.class_('fz_buffer')}.{rename.method('fz_buffer', 'fz_buffer_storage_memoryview')}  = fz_buffer_storage_memoryview

                # Overwrite wrappers for fz_new_buffer_from_copied_data() to
                # take Python `bytes` instance.
                #
                def {rename.fn('fz_new_buffer_from_copied_data')}(bytes_):
                    buffer_ = {rename.ll_fn('fz_new_buffer_from_copied_data')}(python_bytes_data(bytes_), len(bytes_))
                    return {rename.class_('fz_buffer')}(buffer_)
                {rename.class_('fz_buffer')}.{rename.method('fz_buffer', 'fz_new_buffer_from_copied_data')} = {rename.fn('fz_new_buffer_from_copied_data')}


                def {rename.fn('pdf_dict_getl')}(obj, *tail):
                    """
                    Python implementation of pdf_dict_getl(fz_context *ctx,
                    pdf_obj *obj, ...), because SWIG doesn't handle variadic
                    args.
                    """
                    for key in tail:
                        if not obj.m_internal:
                            break
                        obj = obj.pdf_dict_get(key)
                    assert isinstance(obj, {rename.class_('pdf_obj')})
                    return obj
                {rename.class_('pdf_obj')}.{rename.method('pdf_obj', 'pdf_dict_getl')} = {rename.fn('pdf_dict_getl')}

                def {rename.fn('pdf_dict_putl')}(obj, val, *tail):
                    """
                    Python implementation of pdf_dict_putl(fz_context *ctx,
                    pdf_obj *obj, pdf_obj *val, ...) because SWIG doesn't
                    handle variadic args.
                    """
                    if obj.{rename.method('pdf_obj', 'pdf_is_indirect')}():
                        obj = obj.{rename.method('pdf_obj', 'pdf_resolve_indirect_chain')}()
                    if not obj.{rename.method('pdf_obj', 'pdf_is_dict')}():
                        raise Exception(f'not a dict: {{obj}}')
                    if not tail:
                        return
                    doc = obj.{rename.method('pdf_obj', 'pdf_get_bound_document')}()
                    for key in tail[:-1]:
                        next_obj = obj.{rename.method('pdf_obj', 'pdf_dict_get')}(key)
                        if not next_obj.m_internal:
                            # We have to create entries
                            next_obj = doc.{rename.method('pdf_obj', 'pdf_new_dict')}(1)
                            obj.{rename.method('pdf_obj', 'pdf_dict_put')}(key, next_obj)
                        obj = next_obj
                    key = tail[-1]
                    obj.{rename.method('pdf_obj', 'pdf_dict_put')}(key, val)
                {rename.class_('pdf_obj')}.{rename.method('pdf_obj', 'pdf_dict_putl')} = {rename.fn('pdf_dict_putl')}

                def {rename.fn('pdf_dict_putl_drop')}(obj, *tail):
                    raise Exception('mupdf.{rename.fn('pdf_dict_putl_drop')}() is unsupported and unnecessary in Python because reference counting is automatic. Instead use mupdf.{rename.fn('pdf_dict_putl')}().')
                {rename.class_('pdf_obj')}.{rename.method('pdf_obj', 'pdf_dict_putl_drop')} = {rename.fn('pdf_dict_putl_drop')}

                def {rename.ll_fn('pdf_set_annot_color')}(annot, color):
                    """
                    Python implementation of pdf_set_annot_color() using
                    {rename.ll_fn('pdf_set_annot_interior_color2')}().
                    """
                    if isinstance(color, float):
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 1, color, 0, 0, 0)
                    elif len(color) == 1:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 1, color[0], 0, 0, 0)
                    elif len(color) == 2:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 2, color[0], color[1], 0, 0)
                    elif len(color) == 3:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 3, color[0], color[1], color[2], 0)
                    elif len(color) == 4:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 4, color[0], color[1], color[2], color[3])
                    else:
                        raise Exception( f'Unexpected color should be float or list of 1-4 floats: {{color}}')

                # Override {rename.fn('pdf_set_annot_color')}()s to use the above.
                def {rename.fn('pdf_set_annot_color')}(self, color):
                    return {rename.ll_fn('pdf_set_annot_color')}(self.m_internal, color)
                {rename.class_('pdf_annot')}.{rename.method('pdf_annot', 'pdf_set_annot_color')} = {rename.fn('pdf_set_annot_color')}

                def {rename.ll_fn('pdf_set_annot_interior_color')}(annot, color):
                    """
                    Python version of pdf_set_annot_color() using
                    pdf_set_annot_color2().
                    """
                    if isinstance(color, float):
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 1, color, 0, 0, 0)
                    elif len(color) == 1:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 1, color[0], 0, 0, 0)
                    elif len(color) == 2:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 2, color[0], color[1], 0, 0)
                    elif len(color) == 3:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 3, color[0], color[1], color[2], 0)
                    elif len(color) == 4:
                        {rename.ll_fn('pdf_set_annot_interior_color2')}(annot, 4, color[0], color[1], color[2], color[3])
                    else:
                        raise Exception( f'Unexpected color should be float or list of 1-4 floats: {{color}}')

                # Override {rename.fn('pdf_set_annot_interior_color')}() to use
                # the above.
                def {rename.fn('pdf_set_annot_interior_color')}(self, color):
                    return {rename.ll_fn('pdf_set_annot_interior_color')}(self.m_internal, color)
                {rename.class_('pdf_annot')}.{rename.method('pdf_annot', 'pdf_set_annot_interior_color')} = {rename.fn('pdf_set_annot_interior_color')}

                # Override {rename.fn('fz_fill_text')}() to handle color as a Python tuple/list.
                def {rename.fn('fz_fill_text')}(dev, text, ctm, colorspace, color, alpha, color_params):
                    """
                    Python version of {rename.fn('fz_fill_text')}() using {rename.fn('fz_fill_text2')}().
                    """
                    color = tuple(color) + (0,) * (4-len(color))
                    assert len(color) == 4, f'color not len 4: len={{len(color)}}: {{color}}'
                    return {rename.fn('fz_fill_text2')}(dev, text, ctm, colorspace, *color, alpha, color_params)

                {rename.class_('fz_device')}.{rename.method('fz_device', 'fz_fill_text')} = {rename.fn('fz_fill_text')}

                # Override mupdf_convert_color() to return (rgb0, rgb1, rgb2, rgb3).
                def {rename.ll_fn('fz_convert_color')}( ss, sv, ds, is_, params):
                    """
                    Python version of {rename.ll_fn('fz_convert_color')}.

                    `sv` should be a float or list of 1-4 floats or a SWIG
                    representation of a float*.

                    Returns (dv0, dv1, dv2, dv3).
                    """
                    dv = {rename.fn('fz_convert_color')}2_v()
                    if isinstance( sv, float):
                       {rename.ll_fn('fz_convert_color2')}( ss, sv, 0.0, 0.0, 0.0, ds, dv, is_, params)
                    elif isinstance( sv, (tuple, list)):
                        sv2 = tuple(sv) + (0,) * (4-len(sv))
                        {rename.ll_fn('fz_convert_color2')}( ss, *sv2, ds, dv, is_, params)
                    else:
                        # Assume `sv` is SWIG representation of a `float*`.
                        {rename.ll_fn('fz_convert_color2')}( ss, sv, ds, dv, is_, params)
                    return dv.v0, dv.v1, dv.v2, dv.v3

                def {rename.fn('fz_convert_color')}( ss, sv, ds, is_, params):
                    """
                    Python version of {rename.fn('fz_convert_color')}.
                    `sv` should be a float or list of 1-4 floats.
                    Returns (dv0, dv1, dv2, dv3).
                    """
                    return {rename.ll_fn('fz_convert_color')}( ss.m_internal, sv, ds.m_internal, is_.m_internal, params.internal())
                {rename.class_('fz_colorspace')}.{rename.method('fz_colorspace', 'fz_convert_color')} = {rename.fn('fz_convert_color')}

                # Override set_warning_callback() and set_error_callback() to
                # use Python classes derived from our SWIG Director classes
                # SetWarningCallback and SetErrorCallback (defined in C), so
                # that fnptrs can call Python code.
                #
                set_warning_callback_s = None
                set_error_callback_s = None

                def set_warning_callback2( printfn):
                    class Callback( SetWarningCallback):
                        def print( self, message):
                            printfn( message)
                    global set_warning_callback_s
                    set_warning_callback_s = Callback()

                # Override set_error_callback().
                def set_error_callback2( printfn):
                    class Callback( SetErrorCallback):
                        def print( self, message):
                            printfn( message)
                    global set_error_callback_s
                    set_error_callback_s = Callback()

                {rename.fn('fz_set_warning_callback')} = set_warning_callback2
                {rename.fn('fz_set_error_callback')} = set_error_callback2

                # Direct access to fz_pixmap samples.
                def {rename.fn('fz_pixmap_samples')}2( pixmap):
                    """
                    Returns a writable Python `memoryview` for a `fz_pixmap`.
                    """
                    assert isinstance( pixmap, {rename.class_('fz_pixmap')})
                    ret = python_memoryview_from_memory(
                            {rename.fn('fz_pixmap_samples')}( pixmap),
                            {rename.fn('fz_pixmap_stride')}( pixmap) * {rename.fn('fz_pixmap_height')}( pixmap),
                            1, # writable
                            )
                    return ret
                {rename.class_('fz_pixmap')}.{rename.method('fz_pixmap', 'pixmap_samples')}2 = {rename.fn('fz_pixmap_samples')}2

                # Avoid potential unsafe use of variadic args by forcing a
                # single arg and escaping all '%' characters. (Passing ('%s',
                # text) does not work - results in "(null)" being output.)
                #
                {rename.ll_fn('fz_warn')}_original = {rename.ll_fn('fz_warn')}
                def {rename.ll_fn('fz_warn')}( text):
                    assert isinstance( text, str)
                    text = text.replace( '%', '%%')
                    return {rename.ll_fn('fz_warn')}_original( text)
                #warn = mfz_warn

                # Force use of pdf_field_name2() instead of pdf_field_name()
                # because the latter returns a buffer that must be freed by the
                # caller.
                {rename.ll_fn('pdf_field_name')} = {rename.ll_fn('pdf_field_name2')}
                {rename.fn('pdf_field_name')} = {rename.fn('pdf_field_name2')}
                {rename.class_('pdf_obj')}.{rename.method('pdf_obj', 'pdf_field_name')} = {rename.class_('pdf_obj')}.{rename.method('pdf_obj', 'pdf_field_name2')}
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

        # For all wrapper classes with a to_string() method, add a __str__() method
        # to the Python wrapper class, which calls the class's to_string() method.
        #
        # E.g. this allows Python code to print a mupdf.Rect instance.
        #
        for struct_name in generated.to_string_structnames:
            text += f'{rename.class_(struct_name)}.__str__ = lambda self: self.to_string()\n'

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
        jlib.update_file( oo, f'{build_dirs.dir_mupdf}/platform/python/include/mupdf/pdf/object.h')

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
        jlib.update_file( '', swig2_i)

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
    swig_cpp_old = None
    if os.path.exists( swig_cpp):
        swig_cpp_old = f'{swig_cpp}-old'
        jlib.copy( swig_cpp, swig_cpp_old)

    if language == 'python':
        # Need -D_WIN32 on Windows because as of 2022-03-17, C++ code for
        # SWIG Directors support doesn't work on Windows so is inside #ifndef
        # _WIN32...#endif.
        #
        # Maybe use '^' on windows as equivalent to unix '\\' for multiline
        # ending?
        def make_command( module, cpp, swig_i):
            cpp = os.path.relpath( cpp)
            swig_i = os.path.relpath( swig_i)
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
            jlib.remove( swig_py_tmp)
            os.rename( swig_py, swig_py_tmp)
            with open( swig_py_tmp) as f:
                swig_py_content = f.read()

            if state_.openbsd:
                # Write Python code that will automatically load the required
                # .so's when mupdf.py is imported. Unfortunately this doesn't
                # work on Linux.
                prefix = textwrap.dedent(
                        f'''
                        import ctypes
                        import os
                        import importlib

                        # The required .so's are in the same directory as this
                        # Python file. On OpenBSD we can explicitly load these
                        # .so's here using ctypes.cdll.LoadLibrary(), which
                        # avoids the need for LD_LIBRARY_PATH to be defined.
                        #
                        # Unfortunately this doesn't work on Linux.
                        #
                        for leaf in ('libmupdf.so', 'libmupdfcpp.so', '{so}'):
                            path = os.path.abspath(f'{{__file__}}/../{{leaf}}')
                            #print(f'path={{path}}')
                            #print(f'exists={{os.path.exists(path)}}')
                            ctypes.cdll.LoadLibrary( path)
                            #print(f'have loaded {{path}}')
                        ''')
                swig_py_content = prefix + swig_py_content

            elif state_.windows:
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
            jlib.update_file( '', swig2_cpp)
            jlib.remove( swig2_py)

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
        jlib.update_file(cs2, f'{build_dirs.dir_so}/mupdf.cs')
        #jlib.copy(f'{outdir}/mupdf.cs', f'{build_dirs.dir_so}/mupdf.cs')
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
            jlib.rename( swig_cpp_old, swig_cpp)


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
    jlib.update_file( test_i, 'test.i')

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
