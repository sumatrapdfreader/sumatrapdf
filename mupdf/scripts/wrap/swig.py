'''
Support for using SWIG to generate langauge bindings from the C++ bindings.
'''

import io
import os
import re
import textwrap

import jlib

from . import cpp
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
                /* Support for extracting buffer data into a Python bytes. */
                PyObject* buffer_extract_bytes(fz_buffer* buffer)
                {{
                    unsigned char* c = NULL;
                    /* We mimic the affects of fz_buffer_extract(), which leaves
                    the buffer with zero capacity. */
                    size_t len = mupdf::buffer_storage(buffer, &c);
                    PyObject* ret = PyBytes_FromStringAndSize((const char*) c, (Py_ssize_t) len);
                    if (ret) {{
                        mupdf::clear_buffer(buffer);
                        mupdf::trim_buffer(buffer);
                    }}
                    return ret;
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
                    buffer_ = mupdf.new_buffer_from_copied_data(mupdf.python_bytes_data(bs), len(bs))
                */
                const unsigned char* python_bytes_data(const unsigned char* PYTHON_BYTES_DATA, size_t PYTHON_BYTES_SIZE)
                {{
                    return PYTHON_BYTES_DATA;
                }}

                /* Casts an integer to a pdf_obj*. Used to convert SWIG's int
                values for PDF_ENUM_NAME_* into PdfObj's. */
                pdf_obj* obj_enum_to_obj(int n)
                {{
                    return (pdf_obj*) (intptr_t) n;
                }}

                /* SWIG-friendly alternative to ppdf_set_annot_color(). */
                void ppdf_set_annot_color2(pdf_annot *annot, int n, float color0, float color1, float color2, float color3)
                {{
                    float color[] = {{ color0, color1, color2, color3 }};
                    return mupdf::ppdf_set_annot_color(annot, n, color);
                }}


                /* SWIG-friendly alternative to ppdf_set_annot_color(). */
                void ppdf_set_annot_interior_color2(pdf_annot *annot, int n, float color0, float color1, float color2, float color3)
                {{
                    float color[] = {{ color0, color1, color2, color3 }};
                    return mupdf::ppdf_set_annot_color(annot, n, color);
                }}

                /* SWIG-friendly alternative to mfz_fill_text(). */
                void mfz_fill_text2(
                        mupdf::Device& dev,
                        const mupdf::Text& text,
                        mupdf::Matrix& ctm,
                        const mupdf::Colorspace& colorspace,
                        float color0,
                        float color1,
                        float color2,
                        float color3,
                        float alpha,
                        mupdf::ColorParams& color_params
                        )
                {{
                    float color[] = {{color0, color1, color2, color3}};
                    return mfz_fill_text(dev, text, ctm, colorspace, color, alpha, color_params);
                }}

                std::vector<unsigned char> mfz_memrnd2(int length)
                {{
                    std::vector<unsigned char>  ret(length);
                    mupdf::mfz_memrnd(&ret[0], length);
                    return ret;
                }}
                ''')

    common += textwrap.dedent(f'''
            /* SWIG-friendly alternative to fz_runetochar(). */
            std::vector<unsigned char> runetochar2(int rune)
            {{
                std::vector<unsigned char>  buffer(10);
                int n = mupdf::runetochar((char*) &buffer[0], rune);
                assert(n < sizeof(buffer));
                buffer.resize(n);
                return buffer;
            }}

            /* SWIG-friendly alternatives to fz_make_bookmark() and
            fz_lookup_bookmark(), using long long instead of fz_bookmark
            because SWIG appears to treat fz_bookmark as an int despite it
            being a typedef for intptr_t, so ends up slicing. */
            long long unsigned make_bookmark2(fz_document* doc, fz_location loc)
            {{
                fz_bookmark bm = mupdf::make_bookmark(doc, loc);
                return (long long unsigned) bm;
            }}
            long long unsigned mfz_make_bookmark2(fz_document* doc, fz_location loc)
            {{
                return make_bookmark2(doc, loc);
            }}

            fz_location lookup_bookmark2(fz_document *doc, long long unsigned mark)
            {{
                return mupdf::lookup_bookmark(doc, (fz_bookmark) mark);
            }}
            fz_location mfz_lookup_bookmark2(fz_document *doc, long long unsigned mark)
            {{
                return lookup_bookmark2(doc, mark);
            }}

            struct convert_color2_dv
            {{
                float dv0;
                float dv1;
                float dv2;
                float dv3;
            }};

            /* SWIG-friendly alternative for fz_convert_color(). */
            void convert_color2(
                    fz_colorspace *ss,
                    const float *sv,
                    fz_colorspace *ds,
                    convert_color2_dv* dv,
                    fz_colorspace *is,
                    fz_color_params params
                    )
            {{
                mupdf::convert_color(ss, sv, ds, &dv->dv0, is, params);
            }}

            /* SWIG-friendly support for fz_set_warning_callback() and
            fz_set_error_callback(). */

            struct SetWarningCallback
            {{
                SetWarningCallback( void* user=NULL)
                {{
                    this->user = user;
                    mupdf::set_warning_callback( s_print, this);
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
                    mupdf::set_error_callback( s_print, this);
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
    for fnname in generated.c_functions:
        if fnname in ('pdf_annot_type', 'pdf_widget_type'):
            # These are also enums which we don't want to ignore. SWIGing the
            # functions is hopefully harmless.
            pass
        elif 0 and fnname == 'pdf_string_from_annot_type':  # causes duplicate symbol with classes2.cpp and python.
            pass
        else:
            text += f'%ignore {fnname};\n'

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

            %ignore {util.rename.function('fz_append_vprintf')};
            %ignore {util.rename.function('fz_error_stack_slot_s')};
            %ignore {util.rename.function('fz_format_string')};
            %ignore {util.rename.function('fz_vsnprintf')};
            %ignore {util.rename.function('fz_vthrow')};
            %ignore {util.rename.function('fz_vwarn')};
            %ignore {util.rename.function('fz_write_vprintf')};
            %ignore {util.rename.function('fz_vsnprintf')};
            %ignore {util.rename.function('fz_vthrow')};
            %ignore {util.rename.function('fz_vwarn')};
            %ignore {util.rename.function('fz_append_vprintf')};
            %ignore {util.rename.function('fz_write_vprintf')};
            %ignore {util.rename.function('fz_format_string')};
            %ignore {util.rename.function('fz_open_file_w')};

            // SWIG can't handle this because it uses a valist.
            %ignore {util.rename.function('Memento_vasprintf')};

            // asprintf() isn't available on windows, so exclude Memento_asprintf because
            // it is #define-d to asprintf.
            %ignore {util.rename.function('Memento_asprintf')};

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
                %template(vectorq) vector<mupdf::{util.rename.class_("fz_quad")}>;
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

            // Don't wrap raw fz_*() functions.
            %rename("$ignore", regexmatch$name="^fz_", %$isfunction, %$not %$ismember) "";
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

                def Document_lookup_metadata(self, key):
                    """
                    Python implementation override of Document.lookup_metadata().

                    Returns string or None if not found.
                    """
                    e = new_pint()
                    ret = lookup_metadata(self.m_internal, key, e)
                    e = pint_value(e)
                    if e < 0:
                        return None
                    return ret

                Document.lookup_metadata = Document_lookup_metadata

                def PdfDocument_lookup_metadata(self, key):
                    """
                    Python implementation override of PdfDocument.lookup_metadata().

                    Returns string or None if not found.
                    """
                    e = new_pint()
                    ret = ppdf_lookup_metadata(self.m_internal, key, e)
                    e = pint_value(e)
                    if e < 0:
                        return None
                    return ret

                PdfDocument.lookup_metadata = PdfDocument_lookup_metadata
                ''')

    if language == 'python':
        # Make some additions to the generated Python module.
        #
        # E.g. python wrappers for functions that take out-params should return
        # tuples.
        #
        text += generated.swig_python
        text += textwrap.dedent('''
                import re

                # Wrap parse_page_range() to fix SWIG bug where a NULL return
                # value seems to mess up the returned list - we end up with ret
                # containing two elements rather than three, e.g. [0, 2]. This
                # occurs with SWIG-3.0; maybe fixed in SWIG-4?
                #
                w_parse_page_range = parse_page_range
                def parse_page_range(s, n):
                    ret = w_parse_page_range(s, n)
                    if len(ret) == 2:
                        return None, 0, 0
                    else:
                        return ret[0], ret[1], ret[2]

                # Provide native python implementation of format_output_path() (->
                # fz_format_output_path).
                #
                def format_output_path( format, page):
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

                # The auto-generated Python class method Buffer.buffer_extract()
                # returns (size, data).
                #
                # But these raw values aren't particularly useful to Python code so
                # we change the method to return a Python bytes instance instead,
                # using the special C function buffer_storage_bytes() defined
                # above.
                #
                # We make the original method available as
                # Buffer.buffer_extract_raw(); this can be used to create a
                # mupdf.Stream by passing the raw values back to C++ with:
                #
                #   data, size = buffer_.buffer_extract_raw()
                #   stream = mupdf.Stream(data, size))
                #
                # We don't provide a similar wrapper for Buffer.buffer_storage()
                # because we can't create a Python bytes object that
                # points into the buffer's storage. We still provide
                # Buffer.buffer_storage_raw() just in case there is a need for
                # Python code that can pass the raw (data, size) back in to C.
                #

                Buffer.buffer_extract_raw = Buffer.buffer_extract

                def Buffer_buffer_extract(self):
                    """
                    Returns buffer data as a Python bytes instance, leaving the
                    buffer empty. Note that this will make a copy of the underlying
                    data.
                    """
                    return buffer_extract_bytes(self.m_internal)

                Buffer.buffer_extract = Buffer_buffer_extract

                Buffer.buffer_storage_raw = Buffer.buffer_storage
                #delattr(Buffer, 'buffer_storage')
                def Buffer_buffer_storage(self):
                    raise Exception("Buffer.buffer_storage() is not available; use Buffer.buffer_storage_raw() to get (size, data) where <data> is SWIG wrapper for buffer's 'unsigned char*' storage")
                Buffer.buffer_storage = Buffer_buffer_storage


                # Overwrite Buffer.new_buffer_from_copied_data() to take Python Bytes instance.
                #
                def Buffer_new_buffer_from_copied_data(bytes_):
                    buffer_ = new_buffer_from_copied_data(python_bytes_data(bytes_), len(bytes_))
                    return Buffer(buffer_)
                Buffer.new_buffer_from_copied_data = Buffer_new_buffer_from_copied_data


                def mpdf_dict_getl(obj, *tail):
                    """
                    Python implementation of pdf_dict_getl(fz_context *ctx,
                    pdf_obj *obj, ...), because SWIG doesn't handle variadic
                    args.
                    """
                    for key in tail:
                        if not obj.m_internal:
                            break
                        obj = obj.dict_get(key)
                    assert isinstance(obj, PdfObj)
                    return obj
                PdfObj.dict_getl = mpdf_dict_getl

                def mpdf_dict_putl(obj, val, *tail):
                    """
                    Python implementation of pdf_dict_putl(fz_context *ctx,
                    pdf_obj *obj, pdf_obj *val, ...) because SWIG doesn't
                    handle variadic args.
                    """
                    if obj.is_indirect():
                        obj = obj.resolve_indirect_chain()
                    if not obj.is_dict():
                        raise Exception(f'not a dict: {obj}')
                    if not tail:
                        return
                    doc = obj.get_bound_document()
                    for key in tail[:-1]:
                        next_obj = obj.dict_get(key)
                        if not next_obj.m_internal:
                            # We have to create entries
                            next_obj = doc.new_dict(1)
                            obj.dict_put(key, next_obj)
                        obj = next_obj
                    key = tail[-1]
                    obj.dict_put(key, val)
                PdfObj.dict_putl = mpdf_dict_putl

                def mpdf_dict_putl_drop(obj, *tail):
                    raise Exception('mupdf.PdfObj.dict_putl_drop() is unsupported and unnecessary in Python because reference counting is automatic. Instead use mupdf.PdfObj.dict_putl()')
                PdfObj.dict_putl_drop = mpdf_dict_putl_drop

                def ppdf_set_annot_color(annot, color):
                    """
                    Python implementation of pdf_set_annot_color() using
                    ppdf_set_annot_color2().
                    """
                    if isinstance(color, float):
                        ppdf_set_annot_color2(annot, 1, color, 0, 0, 0)
                    elif len(color) == 1:
                        ppdf_set_annot_color2(annot, 1, color[0], 0, 0, 0)
                    elif len(color) == 2:
                        ppdf_set_annot_color2(annot, 2, color[0], color[1], 0, 0)
                    elif len(color) == 3:
                        ppdf_set_annot_color2(annot, 3, color[0], color[1], color[2], 0)
                    elif len(color) == 4:
                        ppdf_set_annot_color2(annot, 4, color[0], color[1], color[2], color[3])
                    else:
                        raise Exception( f'Unexpected color should be float or list of 1-4 floats: {color}')

                # Override PdfAnnot.set_annot_color() to use the above.
                def mpdf_set_annot_color(self, color):
                    return ppdf_set_annot_color(self.m_internal, color)
                PdfAnnot.set_annot_color = mpdf_set_annot_color

                def ppdf_set_annot_interior_color(annot, color):
                    """
                    Python version of pdf_set_annot_color() using
                    ppdf_set_annot_color2().
                    """
                    if isinstance(color, float):
                        ppdf_set_annot_interior_color2(annot, 1, color, 0, 0, 0)
                    elif len(color) == 1:
                        ppdf_set_annot_interior_color2(annot, 1, color[0], 0, 0, 0)
                    elif len(color) == 2:
                        ppdf_set_annot_interior_color2(annot, 2, color[0], color[1], 0, 0)
                    elif len(color) == 3:
                        ppdf_set_annot_interior_color2(annot, 3, color[0], color[1], color[2], 0)
                    elif len(color) == 4:
                        ppdf_set_annot_interior_color2(annot, 4, color[0], color[1], color[2], color[3])
                    else:
                        raise Exception( f'Unexpected color should be float or list of 1-4 floats: {color}')

                # Override PdfAnnot.set_interiorannot_color() to use the above.
                def mpdf_set_annot_interior_color(self, color):
                    return ppdf_set_annot_interior_color(self.m_internal, color)
                PdfAnnot.set_annot_interior_color = mpdf_set_annot_interior_color

                # Override mfz_fill_text() to handle color as a Python tuple/list.
                def mfz_fill_text(dev, text, ctm, colorspace, color, alpha, color_params):
                    """
                    Python version of mfz_fill_text() using mfz_fill_text2().
                    """
                    color = tuple(color) + (0,) * (4-len(color))
                    assert len(color) == 4, f'color not len 4: len={len(color)}: {color}'
                    return mfz_fill_text2(dev, text, ctm, colorspace, *color, alpha, color_params)

                Device.fill_text = mfz_fill_text

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

                set_warning_callback = set_warning_callback2
                set_error_callback = set_error_callback2
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
            text += f'{util.rename.class_(struct_name)}.__str__ = lambda self: self.to_string()\n'

        text += '%}\n'

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

    os.makedirs( f'{build_dirs.dir_mupdf}/platform/{language}', exist_ok=True)
    os.makedirs( f'{build_dirs.dir_so}', exist_ok=True)
    util.update_file_regress( text, swig_i, check_regress)

    # Try to disable some unhelpful SWIG warnings;. unfortunately this doesn't
    # seem to have any effect.
    disable_swig_warnings = [
            201,    # Warning 201: Unable to find 'stddef.h'
            314,    # Warning 314: 'print' is a python keyword, renaming to '_print'
            312,    # Warning 312: Nested union not currently supported (ignored).
            321,    # Warning 321: 'max' conflicts with a built-in name in python
            362,    # Warning 362: operator= ignored
            451,    # Warning 451: Setting a const char * variable may leak memory.
            503,    # Warning 503: Can't wrap 'operator <<' unless renamed to a valid identifier.
            512,    # Warning 512: Overloaded method mupdf::DrawOptions::internal() const ignored, using non-const method mupdf::DrawOptions::internal() instead.
            ]
    disable_swig_warnings = map( str, disable_swig_warnings)
    disable_swig_warnings = '-w' + ','.join( disable_swig_warnings)

    if language == 'python':
        # Need -D_WIN32 on Windows because as of 2022-03-17, C++ code for
        # SWIG Directors support doesn't work on Windows so is inside #ifndef
        # _WIN32...#endif.
        #
        # Maybe use '^' on windows as equivalent to unix '\\' for multiline
        # ending?
        command = (
                textwrap.dedent(
                f'''
                "{swig_command}"
                    {"-D_WIN32" if state_.windows else ""}
                    -Wall
                    -c++
                    {"-doxygen" if swig_major >= 4 else ""}
                    -python
                    {disable_swig_warnings}
                    -module mupdf
                    -outdir {os.path.relpath(build_dirs.dir_so)}
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
                (swig_cpp, swig_py),
                command,
                force_rebuild,
                )
        jlib.log('{rebuilt=}')
        if rebuilt:
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
                        for leaf in ('libmupdf.so', 'libmupdfcpp.so', '_mupdf.so'):
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

            # Change all our PDF_ENUM_NAME_* enums so that they are actually
            # PdfObj instances so that they can be used like any other PdfObj.
            #
            jlib.log('{len(generated.c_enums)=}')
            for enum_type, enum_names in generated.c_enums.items():
                for enum_name in enum_names:
                    if enum_name.startswith( 'PDF_ENUM_NAME_'):
                        swig_py_content += f'{enum_name} = PdfObj( obj_enum_to_obj( {enum_name}))\n'

            with open( swig_py_tmp, 'w') as f:
                f.write( swig_py_content)
            os.rename( swig_py_tmp, swig_py)

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
                    -Wall
                    -c++
                    -csharp
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
        jlib.update_file(cs2, f'{build_dirs.dir_so}/mupdf.cs')
        #jlib.copy(f'{outdir}/mupdf.cs', f'{build_dirs.dir_so}/mupdf.cs')
        jlib.log('{rebuilt=}')

    else:
        assert 0


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
