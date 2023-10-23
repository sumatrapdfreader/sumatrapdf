'''
Things for generating C#-specific output.
'''
from . import cpp
from . import parse
from . import rename
from . import state
from . import util

import jlib

import textwrap


def make_outparam_helper_csharp(
        tu,
        cursor,
        fnname,
        fnname_wrapper,
        generated,
        main_name,
        ):
    '''
    Write C# code for a convenient tuple-returning wrapper for MuPDF
    function that has out-params. We use the C# wrapper for our generated
    {main_name}_outparams() function.

    We don't attempt to handle functions that take unsigned char* args
    because these generally indicate sized binary data and cannot be handled
    generically.
    '''
    def write(text):
        generated.swig_csharp.write(text)

    main_name = rename.ll_fn(cursor.mangled_name)
    return_void = cursor.result_type.spelling == 'void'
    if fnname == 'fz_buffer_extract':
        # Write custom wrapper that returns the binary data as a C# bytes
        # array, using the C# wrapper for buffer_extract_outparams_fn(fz_buffer
        # buf, buffer_extract_outparams outparams).
        #
        write(
                textwrap.dedent(
                f'''

                // Custom C# wrapper for fz_buffer_extract().
                public static class mupdf_{rename.class_('fz_buffer')}_extract
                {{
                    public static byte[] {rename.method('fz_buffer', 'fz_buffer_extract')}(this mupdf.{rename.class_('fz_buffer')} buffer)
                    {{
                        var outparams = new mupdf.{rename.ll_fn('fz_buffer_storage')}_outparams();
                        uint n = mupdf.mupdf.{rename.ll_fn('fz_buffer_storage')}_outparams_fn(buffer.m_internal, outparams);
                        var raw1 = mupdf.SWIGTYPE_p_unsigned_char.getCPtr(outparams.datap);
                        System.IntPtr raw2 = System.Runtime.InteropServices.HandleRef.ToIntPtr(raw1);
                        byte[] ret = new byte[n];
                        // Marshal.Copy() raises exception if <raw2> is null even if <n> is zero.
                        if (n == 0) return ret;
                        System.Runtime.InteropServices.Marshal.Copy(raw2, ret, 0, (int) n);
                        buffer.{rename.method( 'fz_buffer', 'fz_clear_buffer')}();
                        buffer.{rename.method( 'fz_buffer', 'fz_trim_buffer')}();
                        return ret;
                    }}
                }}
                ''')
                )
        return

    # We don't attempt to generate wrappers for fns that take or return
    # 'unsigned char*' - swig does not treat these as zero-terminated strings,
    # and they are generally binary data so cannot be handled generically.
    #
    if parse.is_pointer_to(cursor.result_type, 'unsigned char'):
        jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because it returns unsigned char*.', 1)
        return
    for arg in parse.get_args( tu, cursor):
        if parse.is_pointer_to(arg.cursor.type, 'unsigned char'):
            jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has unsigned char* arg.', 1)
            return
        if parse.is_pointer_to_pointer_to(arg.cursor.type, 'unsigned char'):
            jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has unsigned char** arg.', 1)
            return
        if arg.cursor.type.get_array_size() >= 0:
            jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has array arg.', 1)
            return
        if arg.cursor.type.kind == state.clang.cindex.TypeKind.POINTER:
            pointee = state.get_name_canonical( arg.cursor.type.get_pointee())
            if pointee.kind == state.clang.cindex.TypeKind.ENUM:
                jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has enum out-param arg.', 1)
                return
            if pointee.kind == state.clang.cindex.TypeKind.FUNCTIONPROTO:
                jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has fn-ptr arg.', 1)
                return
            if pointee.is_const_qualified():
                # Not an out-param.
                jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has pointer-to-const arg.', 1)
                return
            if arg.cursor.type.get_pointee().spelling == 'FILE':
                jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has FILE* arg.', 1)
                return
            if pointee.spelling == 'void':
                jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because has void* arg.', 1)
                return

    num_return_values = 0 if return_void else 1
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            num_return_values += 1
    assert num_return_values

    if num_return_values > 7:
        # On linux, mono-csc can fail with:
        #   System.NotImplementedException: tuples > 7
        #
        jlib.log(f'Cannot generate C# out-param wrapper for {cursor.mangled_name} because would require > 7-tuple.')
        return

    # Write C# wrapper.
    arg0, _ = parse.get_first_arg( tu, cursor)
    if not arg0.alt:
        return

    method_name = rename.method( arg0.alt.type.spelling, fnname)

    write(f'\n')
    write(f'// Out-params extension method for C# class {rename.class_(arg0.alt.type.spelling)} (wrapper for MuPDF struct {arg0.alt.type.spelling}),\n')
    write(f'// adding class method {method_name}() (wrapper for {fnname}())\n')
    write(f'// which returns out-params directly.\n')
    write(f'//\n')
    write(f'public static class mupdf_{main_name}_outparams_helper\n')
    write(f'{{\n')
    write(f'    public static ')

    def write_type(alt, type_):
        if alt:
            write(f'mupdf.{rename.class_(alt.type.spelling)}')
        elif parse.is_pointer_to(type_, 'char'):
            write( f'string')
        else:
            text = cpp.declaration_text(type_, '').strip()
            if text == 'int16_t':           text = 'short'
            elif text == 'int64_t':         text = 'long'
            elif text == 'size_t':          text = 'ulong'
            elif text == 'unsigned int':    text = 'uint'
            write(f'{text}')

    # Generate the returned tuple.
    #
    if num_return_values > 1:
        write('(')

    sep = ''

    # Returned param, if any.
    if not return_void:
        return_alt = None
        base_type_cursor, base_typename, extras = parse.get_extras( tu, cursor.result_type)
        if extras:
            if extras.opaque:
                # E.g. we don't have access to defintion of fz_separation,
                # but it is marked in classextras with opaque=true, so
                # there will be a wrapper class.
                return_alt = base_type_cursor
            elif base_type_cursor.kind == state.clang.cindex.CursorKind.STRUCT_DECL:
                return_alt = base_type_cursor
        write_type(return_alt, cursor.result_type)
        sep = ', '

    # Out-params.
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            write(sep)
            write_type(arg.alt, arg.cursor.type.get_pointee())
            if num_return_values > 1:
                write(f' {arg.name_csharp}')
            sep = ', '

    if num_return_values > 1:
        write(')')

    # Generate function name and params. If first arg is a wrapper class we
    # use C#'s 'this' keyword to make this a member function of the wrapper
    # class.
    #jlib.log('outputs fn {fnname=}: is member: {"yes" if arg0.alt else "no"}')
    write(f' ')
    write( method_name if arg0.alt else 'fn')
    write(f'(')
    if arg0.alt: write('this ')
    sep = ''
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            continue
        write(sep)
        if arg.alt:
            # E.g. 'Document doc'.
            write(f'mupdf.{rename.class_(arg.alt.type.spelling)} {arg.name_csharp}')
        elif parse.is_pointer_to(arg.cursor.type, 'char'):
            write(f'string {arg.name_csharp}')
        else:
            text = cpp.declaration_text(arg.cursor.type, arg.name_csharp).strip()
            text = util.clip(text, 'const ')
            text = text.replace('int16_t ', 'short ')
            text = text.replace('int64_t ', 'long ')
            text = text.replace('size_t ', 'uint ')
            text = text.replace('unsigned int ', 'uint ')
            write(text)
        sep = ', '
    write(f')\n')

    # Function body.
    #
    write(f'    {{\n')

    # Create local outparams struct.
    write(f'        var outparams = new mupdf.{main_name}_outparams();\n')
    write(f'        ')

    # Generate function call.
    #
    # The C# *_outparams_fn() generated by swig is inside namespace mupdf {
    # class mupdf { ... } }, so we access it using the rather clumsy prefix
    # 'mupdf.mupdf.'. It will have been generated from a C++ function
    # (generate by us) which is in top-level namespace mupdf, but swig
    # appears to generate the same code even if the C++ function is not in
    # a namespace.
    #
    if not return_void:
        write(f'var ret = ')
    write(f'mupdf.mupdf.{main_name}_outparams_fn(')
    sep = ''
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            continue
        write(f'{sep}{arg.name_csharp}')
        if arg.alt:
            extras = parse.get_fz_extras( tu, arg.alt.type.spelling)
            assert extras.pod != 'none' \
                    'Cannot pass wrapper for {type_.spelling} as arg because pod is "none" so we cannot recover struct.'
            write('.internal_()' if extras.pod == 'inline' else '.m_internal')
        sep = ', '
    write(f'{sep}outparams);\n')

    # Generate return of tuple.
    write(f'        return ')
    if num_return_values > 1:
        write(f'(')
    sep = ''
    if not return_void:
        if return_alt:
            write(f'new mupdf.{rename.class_(return_alt.type.spelling)}(ret)')
        else:
            write(f'ret')
        sep = ', '
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            write(f'{sep}')
            type_ = arg.cursor.type.get_pointee()
            if arg.alt:
                    write(f'new mupdf.{rename.class_(arg.alt.type.spelling)}(outparams.{arg.name_csharp})')
            elif 0 and parse.is_pointer_to(type_, 'char'):
                # This was intended to convert char* to string, but swig
                # will have already done that when making a C# version of
                # the C++ struct, and modern csc on Windows doesn't like
                # creating a string from a string for some reason.
                write(f'new string(outparams.{arg.name_csharp})')
            else:
                write(f'outparams.{arg.name_csharp}')
            sep = ', '
    if num_return_values > 1:
        write(')')
    write(';\n')
    write(f'    }}\n')
    write(f'}}\n')
