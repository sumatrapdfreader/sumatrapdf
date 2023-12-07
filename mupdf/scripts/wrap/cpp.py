'''
Functions for generating source code for the C++ bindings.
'''

import io
import os
import pickle
import re
import textwrap

import jlib

from . import classes
from . import csharp
from . import parse
from . import python
from . import rename
from . import state
from . import util


def _make_top_level( text, top_level='::'):
    if text == 'string':
        # This is a hack; for some reason we often end up with `string` when it
        # it should be `std::string`.
        text = 'std::string'
    initial_prefix = ['']
    def handle_prefix( text, prefix):
        if text.startswith( prefix):
            initial_prefix[0] += prefix
            return text[ len(prefix):]
        return text
    text = handle_prefix( text, 'const ')
    text = handle_prefix( text, 'struct ')
    if text.startswith( ('fz_', 'pdf_')):
        text = f'{top_level}{text}'
    text = f'{initial_prefix[0]}{text}'
    return text


def declaration_text(
        type_,
        name,
        nest=0,
        name_is_simple=True,
        verbose=False,
        expand_typedef=True,
        top_level='::',
        ):
    '''
    Returns text for C++ declaration of <type_> called <name>.

    type:
        a clang.cindex.Type.
    name:
        name of type; can be empty.
    nest:
        for internal diagnostics.
    name_is_simple:
        true iff <name> is an identifier.

    If name_is_simple is false, we surround <name> with (...) if type is a
    function.
    '''
    # clang can give unhelpful spelling for anonymous structs.
    assert 'struct (unnamed at ' not in type_.spelling, f'type_.spelling={type_.spelling}'
    if verbose:
        jlib.log( '{nest=} {name=} {type_.spelling=} {type_.get_declaration().get_usr()=}')
        jlib.log( '{type_.kind=} {type_.get_array_size()=} {expand_typedef=}')

    array_n = type_.get_array_size()
    if verbose:
        jlib.log( '{array_n=}')
    if array_n >= 0 or type_.kind == state.clang.cindex.TypeKind.INCOMPLETEARRAY:
        if verbose: jlib.log( '{array_n=}')
        if array_n < 0:
            array_n = ''
        ret = declaration_text(
                type_.get_array_element_type(),
                f'{name}[{array_n}]',
                nest+1,
                name_is_simple,
                verbose=verbose,
                expand_typedef=expand_typedef,
                top_level=top_level,
                )
        if verbose:
            jlib.log( 'returning {ret=}')
        return ret

    pointee = type_.get_pointee()
    if pointee and pointee.spelling:
        if verbose: jlib.log( '{pointee.spelling=}')
        ret = declaration_text(
                pointee, f'*{name}',
                nest+1,
                name_is_simple=False,
                verbose=verbose,
                expand_typedef=expand_typedef,
                top_level=top_level,
                )
        if verbose:
            jlib.log( 'returning {ret=}')
        return ret

    if expand_typedef and type_.get_typedef_name():
        if verbose: jlib.log( '{type_.get_typedef_name()=}')
        const = 'const ' if type_.is_const_qualified() else ''
        ret = f'{const}{_make_top_level(type_.get_typedef_name(), top_level)} {name}'
        if verbose:
            jlib.log( 'returning {ret=}')
        return ret

    # On MacOS type `size_t` returns true from get_result() and is
    # state.clang.cindex.TypeKind.ELABORATED.
    #
    if ( type_.get_result().spelling
            and type_.kind not in
                (
                    state.clang.cindex.TypeKind.FUNCTIONNOPROTO,
                    state.clang.cindex.TypeKind.ELABORATED,
                )
            ):
        # <type> is a function. We call ourselves with type=type_.get_result()
        # and name=<name>(<args>).
        #
        assert type_.kind == state.clang.cindex.TypeKind.FUNCTIONPROTO, \
                f'{type_.spelling=} {type_.kind=}'
        ret = ''
        sep = ''
        for arg in type_.argument_types():
            ret += sep
            ret += declaration_text(
                    arg,
                    '',
                    nest+1,
                    top_level=top_level,
                    verbose=verbose,
                    expand_typedef=expand_typedef,
                    )
            sep = ', '
        if verbose: jlib.log( '{ret!r=}')
        if not name_is_simple:
            # If name isn't a simple identifier, put it inside braces, e.g.
            # this crudely allows function pointers to work.
            name = f'({name})'
        ret = f'{name}({ret})'
        if verbose: jlib.log( '{type_.get_result()=}')
        ret = declaration_text(
                type_.get_result(),
                ret,
                nest+1,
                name_is_simple=False,
                verbose=verbose,
                expand_typedef=expand_typedef,
                top_level=top_level,
                )
        if verbose:
            jlib.log( 'returning {ret=}')
        return ret

    ret = f'{_make_top_level(type_.spelling, top_level)} {name}'
    assert not 'struct (unnamed at ' in ret, f'Bad clang name for anonymous struct: {ret}'
    if verbose: jlib.log( 'returning {ret=}')
    return ret


def write_call_arg(
        tu,
        arg,
        classname,
        have_used_this,
        out_cpp,
        verbose=False,
        python=False,
        ):
    '''
    Write an arg of a function call, translating between raw and wrapping
    classes as appropriate.

    If the required type is a fz_ struct that we wrap, we assume that arg.name
    is a reference to an instance of the wrapper class. If the wrapper class
    is the same as <classname>, we use 'this->' instead of <name>. We also
    generate slightly different code depending on whether the wrapper class is
    pod or inline pod.

    arg:
        Arg from get_args().
    classname:
        Name of wrapper class available as 'this'.
    have_used_this:
        If true, we never use 'this->...'.
    out_cpp:
        .
    python:
        If true, we write python code, not C.

    Returns True if we have used 'this->...', else return <have_used_this>.
    '''
    assert isinstance( arg, parse.Arg)
    assert isinstance( arg.cursor, state.clang.cindex.Cursor)
    if not arg.alt:
        # Arg is a normal type; no conversion necessary.
        if python:
            out_cpp.write( arg.name_python)
        else:
            out_cpp.write( arg.name)
        return have_used_this

    if verbose:
        jlib.log( '{=arg.name arg.alt.spelling classname}')
    type_ = state.get_name_canonical( arg.cursor.type)
    ptr = '*'
    #log( '{=arg.name arg.alt.spelling classname type_.spelling}')
    if type_.kind == state.clang.cindex.TypeKind.POINTER:
        type_ = state.get_name_canonical( type_.get_pointee())
        ptr = ''
    #log( '{=arg.name arg.alt.spelling classname type_.spelling}')
    extras = parse.get_fz_extras( tu, type_.spelling)
    assert extras, f'No extras for type_.spelling={type_.spelling}'
    if verbose:
        jlib.log( 'param is fz: {type_.spelling=} {extras2.pod=}')
    assert extras.pod != 'none' \
            'Cannot pass wrapper for {type_.spelling} as arg because pod is "none" so we cannot recover struct.'
    if python:
        if extras.pod == 'inline':
            out_cpp.write( f'{arg.name_python}.internal()')
        elif extras.pod:
            out_cpp.write( f'{arg.name_python}.m_internal')
        else:
            out_cpp.write( f'{arg.name_python}.m_internal')

    elif extras.pod == 'inline':
        # We use the address of the first class member, casting it to a pointer
        # to the wrapped type. Not sure this is guaranteed safe, but should
        # work in practise.
        name_ = f'{arg.name}.'
        if not have_used_this and rename.class_(arg.alt.type.spelling) == classname:
            have_used_this = True
            name_ = 'this->'
        field0 = parse.get_field0(type_).spelling
        out_cpp.write( f'{ptr} {name_}internal()')
    else:
        if verbose:
            jlib.log( '{=arg state.get_name_canonical(arg.cursor.type).kind classname extras}')
        if extras.pod and state.get_name_canonical( arg.cursor.type).kind == state.clang.cindex.TypeKind.POINTER:
            out_cpp.write( '&')
        elif not extras.pod and state.get_name_canonical( arg.cursor.type).kind != state.clang.cindex.TypeKind.POINTER:
            out_cpp.write( '*')
        elif arg.out_param:
            out_cpp.write( '&')
        if not have_used_this and rename.class_(arg.alt.type.spelling) == classname:
            have_used_this = True
            out_cpp.write( 'this->')
        else:
            out_cpp.write( f'{arg.name}.')
        out_cpp.write( 'm_internal')

    return have_used_this


def make_fncall( tu, cursor, return_type, fncall, out, refcheck_if):
    '''
    Writes a low-level function call to <out>, using fz_context_s from
    internal_context_get() and with fz_try...fz_catch that converts to C++
    exceptions by calling throw_exception().

    return_type:
        Text return type of function, e.g. 'void' or 'double'.
    fncall:
        Text containing function call, e.g. 'function(a, b, 34)'.
    out:
        Stream to which we write generated code.
    '''
    uses_fz_context = False;

    # Setting this to False is a hack to elide all fz_try/fz_catch code. This
    # has a very small effect on mupdfpy test suite performance - e.g. reduce
    # time from 548.1s to 543.2s.
    #
    use_fz_try = True

    if cursor.spelling in (
            'pdf_specifics',
            ):
        # This fn takes a fz_context* but never throws, so we can omit
        # `fz_try()...fz_catch()`, which might give a small performance
        # improvement.
        use_fz_try = False
        uses_fz_context = True
    else:
        for arg in parse.get_args( tu, cursor, include_fz_context=True):
            if parse.is_pointer_to( arg.cursor.type, 'fz_context'):
                uses_fz_context = True
                break
    if uses_fz_context:
        context_get = rename.internal( 'context_get')
        throw_exception = rename.internal( 'throw_exception')
        out.write( f'    fz_context* auto_ctx = {context_get}();\n')

    # Output code that writes diagnostics to std::cerr if $MUPDF_trace is set.
    #
    def varname_enable():
        for t in 'fz_keep_', 'fz_drop_', 'pdf_keep_', 'pdf_drop_':
            if cursor.spelling.startswith( t):
                return 's_trace_keepdrop'
        return 's_trace > 1'

    out.write( f'    {refcheck_if}\n')
    out.write( f'    if ({varname_enable()}) {{\n')
    out.write( f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): calling {cursor.spelling}():";\n')
    for arg in parse.get_args( tu, cursor, include_fz_context=True):
        if parse.is_pointer_to( arg.cursor.type, 'fz_context'):
            out.write( f'        if ({varname_enable()}) std::cerr << " auto_ctx=" << auto_ctx;\n')
        elif arg.out_param:
            out.write( f'        if ({varname_enable()}) std::cerr << " {arg.name}=" << (void*) {arg.name};\n')
        elif arg.alt:
            # If not a pod, there will not be an operator<<, so just show
            # the address of this arg.
            #
            extras = parse.get_fz_extras( tu, arg.alt.type.spelling)
            assert extras.pod != 'none' \
                    'Cannot pass wrapper for {type_.spelling} as arg because pod is "none" so we cannot recover struct.'
            if extras.pod:
                out.write( f'        std::cerr << " {arg.name}=" << {arg.name};\n')
            elif arg.cursor.type.kind == state.clang.cindex.TypeKind.POINTER:
                out.write( f'        if ({varname_enable()}) std::cerr << " {arg.name}=" << {arg.name};\n')
            else:
                out.write( f'        std::cerr << " &{arg.name}=" << &{arg.name};\n')
        elif parse.is_pointer_to(arg.cursor.type, 'char') and state.get_name_canonical( arg.cursor.type.get_pointee()).is_const_qualified():
            # 'const char*' is assumed to be zero-terminated string. But we
            # need to protect against trying to write nullptr because this
            # appears to kill std::cerr on Linux.
            out.write( f'        if ({arg.name}) std::cerr << " {arg.name}=\'" << {arg.name} << "\'";\n')
            out.write( f'        else std::cerr << " {arg.name}:null";\n')
        elif parse.is_( arg.cursor.type, 'va_list'):
            out.write( f'        std::cerr << " {arg.name}:va_list";\n')
        elif (0
                or parse.is_( arg.cursor.type, 'signed char')
                or parse.is_( arg.cursor.type, 'unsigned char')
                ):
            # Typically used for raw data, so not safe to treat as text.
            out.write( f'        std::cerr << " {arg.name}=" << ((int) {arg.name});\n')
        elif (0
                or parse.is_pointer_to(arg.cursor.type, 'signed char')
                or parse.is_pointer_to(arg.cursor.type, 'unsigned char')
                ):
            # Typically used for raw data, so not safe to treat as text.
            out.write( f'        std::cerr << " {arg.name}=" << ((void*) {arg.name});\n')
        elif arg.cursor.type.kind == state.clang.cindex.TypeKind.POINTER:
            # Don't assume non-const 'char*' is a zero-terminated string.
            out.write( f'        if ({varname_enable()}) std::cerr << " {arg.name}=" << (void*) {arg.name};\n')
        else:
            out.write( f'        std::cerr << " {arg.name}=" << {arg.name};\n')
    out.write( f'        std::cerr << "\\n";\n')
    out.write( f'    }}\n')
    out.write( f'    #endif\n')

    if uses_fz_context:
        out.write( f'    {refcheck_if}\n')
        out.write( f'        long stack0;\n')
        out.write( f'        if (s_check_error_stack)\n')
        out.write( f'        {{\n')
        out.write( f'            stack0 = auto_ctx->error.top - auto_ctx->error.stack_base;\n')
        out.write( f'        }}\n')
        out.write( f'    #endif\n')

    # Now output the function call.
    #
    if return_type != 'void':
        out.write(  f'    {return_type} ret;\n')

    if cursor.spelling == 'fz_warn':
        out.write( '    va_list ap;\n')
        out.write( '    fz_var(ap);\n')

    indent = ''
    if uses_fz_context and use_fz_try:
        out.write(      f'    fz_try(auto_ctx) {{\n')
        indent = '    '

    if cursor.spelling == 'fz_warn':
        out.write( f'    {indent}va_start(ap, fmt);\n')
        out.write( f'    {indent}fz_vwarn(auto_ctx, fmt, ap);\n')
    else:
        if not uses_fz_context:
            out.write( f'    /* No fz_context* arg, so no need for fz_try()/fz_catch() to convert MuPDF exceptions into C++ exceptions. */\n')
        out.write(  f'    {indent}')
        if return_type != 'void':
            out.write(  f'ret = ')
        out.write(  f'{fncall};\n')

    if uses_fz_context and use_fz_try:
        out.write(      f'    }}\n')

    if cursor.spelling == 'fz_warn':
        if use_fz_try:
            out.write(      f'    fz_always(auto_ctx) {{\n')
            out.write(      f'        va_end(ap);\n')
            out.write(      f'    }}\n')
        else:
            out.write(      f'    va_end(ap);\n')

    if uses_fz_context and use_fz_try:
        out.write(      f'    fz_catch(auto_ctx) {{\n')
        out.write(      f'        {refcheck_if}\n')
        out.write(      f'        if (s_trace_exceptions) {{\n')
        out.write(      f'            std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): fz_catch() has caught exception.\\n";\n')
        out.write(      f'        }}\n')
        out.write(      f'        #endif\n')
        out.write(      f'        {throw_exception}(auto_ctx);\n')
        out.write(      f'    }}\n')

    if uses_fz_context:
        out.write( f'    {refcheck_if}\n')
        out.write( f'        if (s_check_error_stack)\n')
        out.write( f'        {{\n')
        out.write( f'            long stack1 = auto_ctx->error.top - auto_ctx->error.stack_base;\n')
        out.write( f'            if (stack1 != stack0)\n')
        out.write( f'                std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): MuPDF error stack size changed by {cursor.spelling}(): " << stack0 << " -> " << stack1 << "\\n";\n')
        out.write( f'        }}\n')
        out.write( f'    #endif\n')

    if return_type != 'void':
        out.write(  f'    return ret;\n')


def to_pickle( obj, path):
    '''
    Pickles <obj> to file <path>.
    '''
    with open( path, 'wb') as f:
        pickle.dump( obj, f)

def from_pickle( path):
    '''
    Returns contents of file <path> unpickled.
    '''
    with open( path, 'rb') as f:
        return pickle.load( f)

class Generated:
    '''
    Stores information generated when we parse headers using clang.
    '''
    def __init__( self):
        self.h_files = []
        self.cpp_files = []
        self.fn_usage_filename = None
        self.container_classnames = []
        self.to_string_structnames = []
        self.fn_usage = dict()
        self.output_param_fns = []
        self.c_functions = []
        self.c_globals = []
        self.c_enums = []
        self.c_structs = []
        self.swig_cpp = io.StringIO()
        self.swig_cpp_python = io.StringIO()
        self.swig_python = io.StringIO()
        self.swig_python_exceptions = io.StringIO()
        self.swig_python_set_error_classes = io.StringIO()
        self.swig_csharp = io.StringIO()
        self.virtual_fnptrs = []    # List of extra wrapper class names with virtual fnptrs.
        self.cppyy_extra = ''

    def save( self, dirpath):
        '''
        Saves state to .pickle file, to be loaded later via pickle.load().
        '''
        to_pickle( self, f'{dirpath}/generated.pickle')


def make_outparam_helper(
        tu,
        cursor,
        fnname,
        fnname_wrapper,
        generated,
        ):
    '''
    Create extra C++, Python and C# code to make tuple-returning wrapper of
    specified function.

    We write Python code to generated.swig_python and C++ code to
    generated.swig_cpp.
    '''
    verbose = False
    main_name = rename.ll_fn(cursor.spelling)
    generated.swig_cpp.write( '\n')

    # Write struct.
    generated.swig_cpp.write( 'namespace mupdf\n')
    generated.swig_cpp.write('{\n')
    generated.swig_cpp.write(f'    /* Out-params helper class for {cursor.spelling}(). */\n')
    generated.swig_cpp.write(f'    struct {main_name}_outparams\n')
    generated.swig_cpp.write(f'    {{\n')
    for arg in parse.get_args( tu, cursor):
        if not arg.out_param:
            continue
        decl = declaration_text( arg.cursor.type, arg.name, verbose=verbose)
        if verbose:
            jlib.log( '{decl=}')
        assert arg.cursor.type.kind == state.clang.cindex.TypeKind.POINTER

        # We use state.get_name_canonical() here because, for example, it
        # converts int64_t to 'long long', which seems to be handled better by
        # swig - swig maps int64_t to mupdf.SWIGTYPE_p_int64_t which can't be
        # treated or converted to an integer.
        #
        pointee = state.get_name_canonical( arg.cursor.type.get_pointee())
        generated.swig_cpp.write(f'        {declaration_text( pointee, arg.name)};\n')
    generated.swig_cpp.write(f'    }};\n')
    generated.swig_cpp.write('\n')

    # Write function definition.
    name_args = f'{main_name}_outparams_fn('
    sep = ''
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            continue
        name_args += sep
        name_args += declaration_text( arg.cursor.type, arg.name, verbose=verbose)
        sep = ', '
    name_args += f'{sep}{main_name}_outparams* outparams'
    name_args += ')'
    generated.swig_cpp.write(f'    /* Out-params function for {cursor.spelling}(). */\n')
    generated.swig_cpp.write(f'    {declaration_text( cursor.result_type, name_args)}\n')
    generated.swig_cpp.write( '    {\n')
    # Set all pointer fields to NULL.
    for arg in parse.get_args( tu, cursor):
        if not arg.out_param:
            continue
        if arg.cursor.type.get_pointee().kind == state.clang.cindex.TypeKind.POINTER:
            generated.swig_cpp.write(f'        outparams->{arg.name} = NULL;\n')
    # Make call. Note that *_outparams will have changed size_t to unsigned long or similar so
    # that SWIG can handle it. Would like to cast the addresses of the struct members to
    # things like (size_t*) but this cause problems with const so we use temporaries.
    for arg in parse.get_args( tu, cursor):
        if not arg.out_param:
            continue
        generated.swig_cpp.write(f'        {declaration_text(arg.cursor.type.get_pointee(), arg.name)};\n')
    return_void = (cursor.result_type.spelling == 'void')
    generated.swig_cpp.write(f'        ')
    if not return_void:
        generated.swig_cpp.write(f'{declaration_text(cursor.result_type, "ret")} = ')
    generated.swig_cpp.write(f'{rename.ll_fn(cursor.spelling)}(')
    sep = ''
    for arg in parse.get_args( tu, cursor):
        generated.swig_cpp.write(sep)
        if arg.out_param:
            #generated.swig_cpp.write(f'&outparams->{arg.name}')
            generated.swig_cpp.write(f'&{arg.name}')
        else:
            generated.swig_cpp.write(f'{arg.name}')
        sep = ', '
    generated.swig_cpp.write(');\n')
    for arg in parse.get_args( tu, cursor):
        if not arg.out_param:
            continue
        generated.swig_cpp.write(f'        outparams->{arg.name} = {arg.name};\n')
    if not return_void:
        generated.swig_cpp.write('        return ret;\n')
    generated.swig_cpp.write('    }\n')
    generated.swig_cpp.write('}\n')

    # Write Python wrapper.
    python.make_outparam_helper_python(tu, cursor, fnname, fnname_wrapper, generated, main_name)

    # Write C# wrapper.
    csharp.make_outparam_helper_csharp(tu, cursor, fnname, fnname_wrapper, generated, main_name)


def make_python_class_method_outparam_override(
        tu,
        cursor,
        fnname,
        generated,
        structname,
        classname,
        return_type,
        ):
    '''
    Writes Python code to `generated.swig_python` that monkey-patches Python
    function or method to make it call the underlying MuPDF function's Python
    wrapper, which will return out-params in a tuple.

    This is necessary because C++ doesn't support out-params so the C++ API
    supports wrapper class out-params by taking references to a dummy wrapper
    class instances, whose m_internal is then changed to point to the out-param
    struct (with suitable calls to keep/drop to manage the destruction of the
    dummy instance).

    In Python, we could create dummy wrapper class instances (e.g. passing
    nullptr to constructor) and return them, but instead we make our own call
    to the underlying MuPDF function and wrap the out-params into wrapper
    classes.
    '''
    out = generated.swig_python
    # Underlying fn.
    main_name = rename.ll_fn(cursor.spelling)

    if structname:
        name_new = f'{classname}_{rename.method(structname, cursor.spelling)}_outparams_fn'
    else:
        name_new = f'{rename.fn(cursor.spelling)}_outparams_fn'

    # Define an internal Python function that will become the class method.
    #
    out.write( f'def {name_new}(')
    if structname:
        out.write( ' self')
        comma = ', '
    else:
        comma = ''
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            continue
        if structname and parse.is_pointer_to( arg.cursor.type, structname):
            continue
        out.write(f'{comma}{arg.name_python}')
        comma = ', '
    out.write('):\n')
    out.write( '    """\n')
    if structname:
        out.write(f'    Helper for out-params of class method {structname}::{main_name}() [{cursor.spelling}()].\n')
    else:
        out.write(f'    Class-aware helper for out-params of {fnname}() [{cursor.spelling}()].\n')
    out.write( '    """\n')

    # ret, a, b, ... = foo::bar(self.m_internal, p, q, r, ...)
    out.write(f'    ')
    sep = ''
    if cursor.result_type.spelling != 'void':
        out.write( 'ret')
        sep = ', '
    for arg in parse.get_args( tu, cursor):
        if not arg.out_param:
            continue
        out.write( f'{sep}{arg.name_python}')
        sep = ', '
    out.write( f' = {main_name}(')
    sep = ''
    if structname:
        out.write( f' self.m_internal')
        sep = ', '
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            continue
        if structname and parse.is_pointer_to( arg.cursor.type, structname):
            continue
        out.write( sep)
        write_call_arg( tu, arg, classname, have_used_this=False, out_cpp=out, python=True)
        sep = ', '
    out.write( ')\n')

    # return ret, a, b.
    #
    # We convert returned items to wrapper classes if they are MuPDF types.
    #
    out.write( '    return ')
    sep = ''
    if cursor.result_type.spelling != 'void':
        if return_type:
            #out.write( f'{return_type}(ret)')
            # Return type is a class wrapper.
            return_ll_type = cursor.result_type
            do_keep = False
            if cursor.result_type.kind == state.clang.cindex.TypeKind.POINTER:
                return_ll_type = return_ll_type.get_pointee()
                if parse.has_refs( tu, return_ll_type):
                    return_ll_type = return_ll_type.spelling
                    return_ll_type = util.clip( return_ll_type, ('struct ', 'const '))
                    assert return_ll_type.startswith( ( 'fz_', 'pdf_'))
                    for prefix in ( 'fz_', 'pdf_'):
                        if return_ll_type.startswith( prefix):
                            break
                    else:
                        assert 0, f'Unexpected arg type: {return_ll_type}'
                    return_extra = classes.classextras.get( tu, return_ll_type)
                    if not function_name_implies_kept_references( fnname):
                        do_keep = True
                else:
                    if 'char' in return_ll_type.spelling:
                        jlib.log('### Function returns {cursor.result_type.spelling=} -> {return_ll_type.spelling=}: {fnname}. {function_name_implies_kept_references(fnname)=}')
            if do_keep:
                keepfn = f'{prefix}keep_{return_ll_type[ len(prefix):]}'
                keepfn = rename.ll_fn( keepfn)
                out.write( f'{return_type}( {keepfn}( ret))')
            else:
                out.write( f'{return_type}(ret)')
        else:
            out.write( 'ret')
        sep = ', '
    for arg in parse.get_args( tu, cursor):
        if not arg.out_param:
            continue
        if arg.alt:
            name = util.clip( arg.alt.type.spelling, ('struct ', 'const '))
            for prefix in ( 'fz_', 'pdf_'):
                if name.startswith( prefix):
                    break
            else:
                assert 0, f'Unexpected arg type: {name}'
            if function_name_implies_kept_references( fnname):
                out.write( f'{sep}{rename.class_(name)}( {arg.name_python})')
            else:
                keepfn = f'{prefix}keep_{name[ len(prefix):]}'
                keepfn = rename.ll_fn( keepfn)
                out.write( f'{sep}{rename.class_(name)}({keepfn}( {arg.name_python}))')
        else:
            out.write( f'{sep}{arg.name_python}')
        sep = ', '
    out.write('\n')
    out.write('\n')

    # foo.bar = foo_bar_outparams_fn
    if structname:
        out.write(f'{classname}.{rename.method(structname, cursor.spelling)} = {name_new}\n')
    else:
        out.write(f'{rename.fn( cursor.spelling)} = {name_new}\n')
    out.write('\n')
    out.write('\n')


def make_wrapper_comment(
        tu,
        cursor,
        fnname,
        fnname_wrapper,
        indent,
        is_method,
        is_low_level,
        ):
    ret = io.StringIO()
    def write(text):
        text = text.replace('\n', f'\n{indent}')
        ret.write( text)

    num_out_params = 0
    for arg in parse.get_args(
            tu,
            cursor,
            include_fz_context=False,
            skip_first_alt=is_method,
            ):
        if arg.out_param:
            num_out_params += 1

    if is_low_level:
        write( f'Low-level wrapper for `{rename.c_fn(cursor.spelling)}()`.')
    else:
        write( f'Class-aware wrapper for `{rename.c_fn(cursor.spelling)}()`.')
    if num_out_params:
        tuple_size = num_out_params
        if cursor.result_type.spelling != 'void':
            tuple_size += 1
        write( f'\n')
        write( f'\n')
        write( f'This {"method" if is_method else "function"} has out-params. Python/C# wrappers look like:\n')
        write( f'    `{fnname_wrapper}(')
        sep = ''
        for arg in parse.get_args( tu, cursor, include_fz_context=False, skip_first_alt=is_method):
            if arg.alt or not arg.out_param:
                write( f'{sep}{declaration_text( arg.cursor.type, arg.name)}')
                sep = ', '
        write(')` => ')
        if tuple_size > 1:
            write( '`(')
        sep = ''
        if cursor.result_type.spelling != 'void':
            write( f'{cursor.result_type.spelling}')
            sep = ', '
        for arg in parse.get_args( tu, cursor, include_fz_context=False, skip_first_alt=is_method):
            if not arg.alt and arg.out_param:
                write( f'{sep}{declaration_text( arg.cursor.type.get_pointee(), arg.name)}')
                sep = ', '
        if tuple_size > 1:
            write( ')`')
        write( f'\n')
    else:
        write( ' ')

    return ret.getvalue()


def function_wrapper(
        tu,
        cursor,
        fnname,
        fnname_wrapper,
        out_h,
        out_cpp,
        generated,
        refcheck_if,
        ):
    '''
    Writes low-level C++ wrapper fn, converting any fz_try..fz_catch exception
    into a C++ exception.

    cursor:
        Clang cursor for function to wrap.
    fnname:
        Name of wrapped function.
    fnname_wrapper:
        Name of function to create.
    out_h:
        Stream to which we write header output.
    out_cpp:
        Stream to which we write cpp output.
    generated:
        A Generated instance.
    refcheck_if:
        A '#if*' statement that determines whether extra checks are compiled
        in.

    Example generated function:

        fz_band_writer * mupdf_new_band_writer_of_size(fz_context *ctx, size_t size, fz_output *out)
        {
            fz_band_writer * ret;
            fz_try(ctx) {
                ret = fz_new_band_writer_of_size(ctx, size, out);
            }
            fz_catch(ctx) {
                mupdf_throw_exception(ctx);
            }
            return ret;
        }
    '''
    assert cursor.kind == state.clang.cindex.CursorKind.FUNCTION_DECL
    if cursor.type.is_function_variadic() and fnname != 'fz_warn':
        jlib.log( 'Not writing low-level wrapper because variadic: {fnname=}', 1)
        return

    verbose = state.state_.show_details( fnname)
    if verbose:
        jlib.log( 'Wrapping {fnname}')
    num_out_params = 0
    for arg in parse.get_args( tu, cursor, include_fz_context=True):
        if parse.is_pointer_to(arg.cursor.type, 'fz_context'):
            continue
        if arg.out_param:
            num_out_params += 1

    # Write first line: <result_type> <fnname_wrapper> (<args>...)
    #
    comment = make_wrapper_comment( tu, cursor, fnname, fnname_wrapper, indent='', is_method=False, is_low_level=True)
    comment = f'/** {comment}*/\n'
    for out in out_h, out_cpp:
        out.write( comment)

    # Copy any comment into .h file before declaration.
    if cursor.raw_comment:
        out_h.write( f'{cursor.raw_comment}')
        if not cursor.raw_comment.endswith( '\n'):
            out_h.write( '\n')

    # Write declaration and definition.
    name_args_h = f'{fnname_wrapper}('
    name_args_cpp = f'{fnname_wrapper}('
    comma = ''
    for arg in parse.get_args( tu, cursor, include_fz_context=True):
        if verbose:
            jlib.log( '{arg.cursor=} {arg.name=} {arg.separator=} {arg.alt=} {arg.out_param=}')
        if parse.is_pointer_to(arg.cursor.type, 'fz_context'):
            continue
        decl = declaration_text( arg.cursor.type, arg.name, verbose=verbose)
        if verbose:
            jlib.log( '{decl=}')
        name_args_h += f'{comma}{decl}'
        decl = declaration_text( arg.cursor.type, arg.name)
        name_args_cpp += f'{comma}{decl}'
        comma = ', '

    if cursor.type.is_function_variadic():
        name_args_h += f'{comma}...'
        name_args_cpp += f'{comma}...'

    name_args_h += ')'
    name_args_cpp += ')'
    declaration_h = declaration_text( cursor.result_type, name_args_h, verbose=verbose)
    declaration_cpp = declaration_text( cursor.result_type, name_args_cpp, verbose=verbose)
    out_h.write( f'FZ_FUNCTION {declaration_h};\n')
    out_h.write( '\n')

    # Write function definition.
    #
    out_cpp.write( f'FZ_FUNCTION {declaration_cpp}\n')
    out_cpp.write( '{\n')
    return_type = cursor.result_type.spelling
    fncall = ''
    fncall += f'{rename.c_fn(cursor.spelling)}('
    for arg in parse.get_args( tu, cursor, include_fz_context=True):
        if parse.is_pointer_to( arg.cursor.type, 'fz_context'):
            fncall += f'{arg.separator}auto_ctx'
        else:
            fncall += f'{arg.separator}{arg.name}'
    fncall += ')'
    make_fncall( tu, cursor, return_type, fncall, out_cpp, refcheck_if)
    out_cpp.write( '}\n')
    out_cpp.write( '\n')

    if num_out_params:
        make_outparam_helper(
                tu,
                cursor,
                fnname,
                fnname_wrapper,
                generated,
                )


def make_namespace_open( namespace, out):
    if namespace:
        out.write( '\n')
        out.write( f'namespace {namespace}\n')
        out.write( '{\n')


def make_namespace_close( namespace, out):
    if namespace:
        out.write( '\n')
        out.write( f'}} /* End of namespace {namespace}. */\n')


# libclang can't always find headers so we define our own `std::string`
# and `std::vector<>` that work well enough for the generation of the
# C++ API.
#
# We also define extra raw functions to aid SWIG-generated code. These
# are implemented in C++, and should be excluded from the generated
# windows_def file later on, otherwise we get link errors on Windows.
#
g_extra_declarations = textwrap.dedent(f'''

        #ifdef MUPDF_WRAP_LIBCLANG

            namespace std
            {{
                template<typename T>
                struct vector
                {{
                }};

                struct string
                {{
                }};
            }}

        #else

            #include <string>
            #include <vector>

        #endif

        #include "mupdf/fitz.h"
        #include "mupdf/pdf.h"

        /**
        C++ alternative to `fz_lookup_metadata()` that returns a `std::string`
        or calls `fz_throw()` if not found.
        */
        FZ_FUNCTION std::string fz_lookup_metadata2(fz_context* ctx, fz_document* doc, const char* key);

        /**
        C++ alternative to `pdf_lookup_metadata()` that returns a `std::string`
        or calls `fz_throw()` if not found.
        */
        FZ_FUNCTION std::string pdf_lookup_metadata2(fz_context* ctx, pdf_document* doc, const char* key);

        /**
        C++ alternative to `fz_md5_pixmap()` that returns the digest by value.
        */
        FZ_FUNCTION std::vector<unsigned char> fz_md5_pixmap2(fz_context* ctx, fz_pixmap* pixmap);

        /**
        C++ alternative to fz_md5_final() that returns the digest by value.
        */
        FZ_FUNCTION std::vector<unsigned char> fz_md5_final2(fz_md5* md5);

        /** */
        FZ_FUNCTION long long fz_pixmap_samples_int(fz_context* ctx, fz_pixmap* pixmap);

        /**
        Provides simple (but slow) access to pixmap data from Python and C#.
        */
        FZ_FUNCTION int fz_samples_get(fz_pixmap* pixmap, int offset);

        /**
        Provides simple (but slow) write access to pixmap data from Python and
        C#.
        */
        FZ_FUNCTION void fz_samples_set(fz_pixmap* pixmap, int offset, int value);

        /**
        C++ alternative to fz_highlight_selection() that returns quads in a
        std::vector.
        */
        FZ_FUNCTION std::vector<fz_quad> fz_highlight_selection2(fz_context* ctx, fz_stext_page* page, fz_point a, fz_point b, int max_quads);

        struct fz_search_page2_hit
        {{
            fz_quad quad;
            int mark;
        }};

        /**
        C++ alternative to fz_search_page() that returns information in a std::vector.
        */
        FZ_FUNCTION std::vector<fz_search_page2_hit> fz_search_page2(fz_context* ctx, fz_document *doc, int number, const char *needle, int hit_max);

        /**
        C++ alternative to fz_string_from_text_language() that returns information in a std::string.
        */
        FZ_FUNCTION std::string fz_string_from_text_language2(fz_text_language lang);

        /**
        C++ alternative to fz_get_glyph_name() that returns information in a std::string.
        */
        FZ_FUNCTION std::string fz_get_glyph_name2(fz_context *ctx, fz_font *font, int glyph);
        ''')

g_extra_definitions = textwrap.dedent(f'''
        FZ_FUNCTION std::string fz_lookup_metadata2( fz_context* ctx, fz_document* doc, const char* key)
        {{
            /* Find length first. */
            int e = fz_lookup_metadata(ctx, doc, key, NULL /*buf*/, 0 /*size*/);
            if (e < 0)
            {{
                fz_throw(ctx, FZ_ERROR_GENERIC, "key not found: %s", key);
            }}
            assert(e != 0);
            char* buf = (char*) fz_malloc(ctx, e);
            int e2 = fz_lookup_metadata(ctx, doc, key, buf, e);
            assert(e2 = e);
            std::string ret = buf;
            free(buf);
            return ret;
        }}

        FZ_FUNCTION std::string pdf_lookup_metadata2( fz_context* ctx, pdf_document* doc, const char* key)
        {{
            /* Find length first. */
            int e = pdf_lookup_metadata(ctx, doc, key, NULL /*buf*/, 0 /*size*/);
            if (e < 0)
            {{
                fz_throw(ctx, FZ_ERROR_GENERIC, "key not found: %s", key);
            }}
            assert(e != 0);
            char* buf = (char*) fz_malloc(ctx, e);
            int e2 = pdf_lookup_metadata(ctx, doc, key, buf, e);
            assert(e2 = e);
            std::string ret = buf;
            free(buf);
            return ret;
        }}

        FZ_FUNCTION std::vector<unsigned char> fz_md5_pixmap2(fz_context *ctx, fz_pixmap *pixmap)
        {{
            std::vector<unsigned char>  ret(16);
            fz_md5_pixmap( ctx, pixmap, &ret[0]);
            return ret;
        }}

        FZ_FUNCTION long long fz_pixmap_samples_int(fz_context *ctx, fz_pixmap *pixmap)
        {{
            long long ret = (intptr_t) pixmap->samples;
            return ret;
        }}

        FZ_FUNCTION int fz_samples_get(fz_pixmap *pixmap, int offset)
        {{
            return pixmap->samples[offset];
        }}

        FZ_FUNCTION void fz_samples_set(fz_pixmap *pixmap, int offset, int value)
        {{
            pixmap->samples[offset] = value;
        }}

        FZ_FUNCTION std::vector<unsigned char> fz_md5_final2(fz_md5* md5)
        {{
            std::vector<unsigned char>  ret(16);
            fz_md5_final( md5, &ret[0]);
            return ret;
        }}

        FZ_FUNCTION std::vector<fz_quad> fz_highlight_selection2(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, int max_quads)
        {{
            {{
                std::vector<fz_quad>    ret(max_quads);
                int n;
                fz_try(ctx)
                {{
                    n = fz_highlight_selection(ctx, page, a, b, &ret[0], max_quads);
                }}
                fz_catch(ctx)
                {{
                    n = -1;
                }}
                if (n >= 0)
                {{
                    ret.resize(n);
                    return ret;
                }}
            }}
            /* We are careful to only call `fz_throw()` after `ret`'s
            destructor has been called. */
            fz_throw(ctx, FZ_ERROR_GENERIC, "fz_highlight_selection() failed");
        }}

        FZ_FUNCTION std::vector<fz_search_page2_hit> fz_search_page2(fz_context *ctx, fz_document *doc, int number, const char *needle, int hit_max)
        {{
            std::vector<fz_quad>    quads(hit_max);
            std::vector<int>        marks(hit_max);
            int n = fz_search_page_number(ctx, doc, number, needle, &marks[0], &quads[0], hit_max);
            std::vector<fz_search_page2_hit>    ret(n);
            for (int i=0; i<n; ++i)
            {{
                ret[i].quad = quads[i];
                ret[i].mark = marks[i];
            }}
            return ret;
        }}

        FZ_FUNCTION std::string fz_string_from_text_language2(fz_text_language lang)
        {{
            char    str[8];
            fz_string_from_text_language(str, lang);
            return std::string(str);
        }}

        FZ_FUNCTION std::string fz_get_glyph_name2(fz_context *ctx, fz_font *font, int glyph)
        {{
            char name[32];
            fz_get_glyph_name(ctx, font, glyph, name, sizeof(name));
            return std::string(name);
        }}
        ''')

def make_extra( out_extra_h, out_extra_cpp):
    '''
    We write extra abstractions here.

    These are written in C++ but are at the same level of abstraction as MuPDF
    C functions, for example they take `fz_context` args. This is done so that
    we automatically generate wrappers as class methods as well as global
    functions.
    '''
    out_extra_h.write( g_extra_declarations)

    out_extra_cpp.write( textwrap.dedent('''
            #include "mupdf/extra.h"

            '''))
    out_extra_cpp.write( g_extra_definitions)


def make_internal_functions( namespace, out_h, out_cpp, refcheck_if):
    '''
    Writes internal support functions.

    out_h:
        Stream to which we write C++ header text.
    out_cpp:
        Stream to which we write C++ text.
    '''
    out_h.write(
            textwrap.dedent(
            f'''
            /** Internal use only. Looks at environmental variable <name>; returns 0 if unset else int value. */
            FZ_FUNCTION int {rename.internal('env_flag')}(const char* name);

            /** Internal use only. Looks at environmental variable <name>; returns 0 if unset else int value. */
            FZ_FUNCTION int {rename.internal('env_flag_check_unset')}( const char* if_, const char* name);

            /** Internal use only. Returns `fz_context*` for use by current thread. */
            FZ_FUNCTION fz_context* {rename.internal('context_get')}();
            '''
            ))

    out_cpp.write(
            textwrap.dedent(
            '''
            #include "mupdf/exceptions.h"
            #include "mupdf/internal.h"

            #include <iostream>
            #include <thread>
            #include <mutex>

            #include <string.h>

            '''))

    make_namespace_open( namespace, out_cpp)

    state_t = rename.internal( 'state')
    thread_state_t = rename.internal( 'thread_state')

    cpp_text = textwrap.dedent(
            f'''
            FZ_FUNCTION int {rename.internal('env_flag')}(const char* name)
            {{
                const char* s = getenv( name);
                if (!s) return 0;
                return atoi( s);
            }}

            FZ_FUNCTION int {rename.internal('env_flag_check_unset')}(const char* if_, const char* name)
            {{
                const char* s = getenv( name);
                if (s) std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "():"
                        << " Warning: ignoring environmental variable because"
                        << " '" << if_ << "' is false: " << name << "\\n";
                return false;
            }}

            {refcheck_if}
                static const int    s_trace = mupdf::internal_env_flag("MUPDF_trace");
            #else
                static const int    s_trace = mupdf::internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace");
            #endif

            struct {rename.internal("state")}
            {{
                /* Constructor. */
                {rename.internal("state")}()
                {{
                    m_locks.user = this;
                    m_locks.lock = lock;
                    m_locks.unlock = unlock;
                    m_ctx = nullptr;
                    bool multithreaded = true;
                    const char* s = getenv( "MUPDF_mt_ctx");
                    if ( s && !strcmp( s, "0")) multithreaded = false;
                    reinit( multithreaded);
                }}

                void reinit( bool multithreaded)
                {{
                    if (s_trace)
                    {{
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): "
                                << " calling fz_drop_context()\\n";
                    }}
                    fz_drop_context( m_ctx);
                    m_multithreaded = multithreaded;
                    if (s_trace)
                    {{
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): "
                                << " calling fz_new_context()\\n";
                    }}
                    m_ctx = fz_new_context(NULL /*alloc*/, (multithreaded) ? &m_locks : nullptr, FZ_STORE_DEFAULT);
                    if (s_trace)
                    {{
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): "
                                << " calling fz_register_document_handlers()\\n";
                    }}
                    fz_register_document_handlers(m_ctx);
                }}
                static void lock(void *user, int lock)
                {{
                    {rename.internal("state")}*    self = ({rename.internal("state")}*) user;
                    assert( self->m_multithreaded);
                    self->m_mutexes[lock].lock();
                }}
                static void unlock(void *user, int lock)
                {{
                    {rename.internal("state")}*    self = ({rename.internal("state")}*) user;
                    assert( self->m_multithreaded);
                    self->m_mutexes[lock].unlock();
                }}
                ~{rename.internal("state")}()
                {{
                    if (s_trace)
                    {{
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): "
                                << " calling fz_drop_context()\\n";
                    }}
                    fz_drop_context(m_ctx);
                }}

                bool                m_multithreaded;
                fz_context*         m_ctx;
                std::mutex          m_mutex;    /* Serialise access to m_ctx. fixme: not actually necessary. */

                /* Provide thread support to mupdf. */
                std::mutex          m_mutexes[FZ_LOCK_MAX];
                fz_locks_context    m_locks;
            }};

            static {rename.internal("state")}  s_state;

            struct {rename.internal("thread_state")}
            {{
                {rename.internal("thread_state")}()
                :
                m_ctx( nullptr),
                m_constructed( true)
                {{}}
                fz_context* get_context()
                {{
                    assert( s_state.m_multithreaded);

                    /* The following code checks that we are not being called after
                    we have been destructed. This can happen if global mupdf
                    wrapper class instances are defined - thread-local objects
                    are destructed /before/ globals. */
                    if (!m_constructed)
                    {{
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ":\\n"
                                << "*** Error - undefined behaviour.\\n"
                                << "***\\n"
                                << "*** Attempt to get thread-local fz_context after destruction\\n"
                                << "*** of thread-local fz_context support instance.\\n"
                                << "***\\n"
                                << "*** This is undefined behaviour.\\n"
                                << "***\\n"
                                << "*** This can happen if mupdf wrapper class instances are\\n"
                                << "*** created as globals, because in C++ global object\\n"
                                << "*** destructors are run after thread_local destructors.\\n"
                                << "***\\n"
                                ;
                    }}
                    assert( m_constructed);
                    if (!m_ctx)
                    {{
                        /* Make a context for this thread by cloning the global
                        context. */
                        /* fixme: we don't actually need to take a lock here. */
                        std::lock_guard<std::mutex> lock( s_state.m_mutex);
                        if (s_trace)
                        {{
                            std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): "
                                    << " calling fz_clone_context()\\n";
                        }}
                        m_ctx = fz_clone_context(s_state.m_ctx);
                    }}
                    return m_ctx;
                }}
                ~{rename.internal("thread_state")}()
                {{
                    if (m_ctx)
                    {{
                        assert( s_state.m_multithreaded);
                        if (s_trace)
                        {{
                            std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): "
                                    << " calling fz_drop_context()\\n";
                        }}
                        fz_drop_context( m_ctx);
                    }}

                    /* These two statements are an attempt to get useful
                    diagnostics in cases of undefined behaviour caused by the
                    use of global wrapper class instances, whose destructors
                    will be called /after/ destruction of this thread-local
                    internal_thread_state instance. See check of m_constructed in
                    get_context().

                    This probably only works in non-optimised builds -
                    optimisation will simply elide both these statements. */
                    m_ctx = nullptr;
                    m_constructed = false;
                }}
                fz_context* m_ctx;
                bool m_constructed;
            }};

            static thread_local {rename.internal("thread_state")}  s_thread_state;

            FZ_FUNCTION fz_context* {rename.internal("context_get")}()
            {{
                if (s_state.m_multithreaded)
                {{
                    return s_thread_state.get_context();
                }}
                else
                {{
                    /* This gives a small improvement in performance for
                    single-threaded use, e.g. from 552.4s to 548.1s. */
                    return s_state.m_ctx;
                }}
            }}

            FZ_FUNCTION void reinit_singlethreaded()
            {{
                if (0)
                {{
                    std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): Reinitialising as single-threaded.\\n";
                }}
                s_state.reinit( false /*multithreaded*/);
            }}
            ''')
    out_cpp.write( cpp_text)

    make_namespace_close( namespace, out_cpp)

    # Generate code that exposes C++ operator new/delete to Memento.
    #
    # Disabled because our generated code makes very few direct calls
    # to operator new, and Memento ends up catching lots of (presumably
    # false-positive) leaks in the Python interpreter, so isn't very useful.
    #
    if 0:
        out_cpp.write( textwrap.dedent(
                '''
                #ifdef MEMENTO

                void* operator new( size_t size)
                {
                    return Memento_cpp_new( size);
                }

                void  operator delete( void* pointer)
                {
                    Memento_cpp_delete( pointer);
                }

                void* operator new[]( size_t size)
                {
                    return Memento_cpp_new_array( size);
                }

                void  operator delete[]( void* pointer)
                {
                    Memento_cpp_delete_array( pointer);
                }

                #endif
                '''
                ))


def make_function_wrappers(
        tu,
        namespace,
        out_exceptions_h,
        out_exceptions_cpp,
        out_functions_h,
        out_functions_cpp,
        out_internal_h,
        out_internal_cpp,
        out_functions_h2,
        out_functions_cpp2,
        generated,
        refcheck_if,
        ):
    '''
    Generates C++ source code containing wrappers for all fz_*() functions.

    We also create a function throw_exception(fz_context* ctx) that throws a
    C++ exception appropriate for the error in ctx.

    If a function has first arg fz_context*, extra code is generated that
    converts fz_try..fz_catch exceptions into C++ exceptions by calling
    throw_exception().

    We remove any fz_context* argument and the implementation calls
    internal_get_context() to get a suitable thread-specific fz_context* to
    use.

    We generate a class for each exception type.

    Returned source is just the raw functions text, e.g. it does not contain
    required #include's.

    Args:
        tu:
            Clang translation unit.
        out_exceptions_h:
            Stream to which we write exception class definitions.
        out_exceptions_cpp:
            Stream to which we write exception class implementation.
        out_functions_h:
            Stream to which we write function declarations.
        out_functions_cpp:
            Stream to which we write function definitions.
        generated:
            A Generated instance.
    '''
    # Look for FZ_ERROR_* enums. We generate an exception class for each of
    # these.
    #
    error_name_prefix = 'FZ_ERROR_'
    fz_error_names = []
    fz_error_names_maxlen = 0   # Used for padding so generated code aligns.

    for cursor in parse.get_children(tu.cursor):
        if cursor.kind == state.clang.cindex.CursorKind.ENUM_DECL:
            #log( 'enum: {cursor.spelling=})
            for child in parse.get_members( cursor):
                #log( 'child:{ child.spelling=})
                if child.spelling.startswith( error_name_prefix):
                    name = child.spelling[ len(error_name_prefix):]
                    fz_error_names.append( name)
                    if len( name) > fz_error_names_maxlen:
                        fz_error_names_maxlen = len( name)

    def errors(include_error_base=False):
        '''
        Yields (enum, typename, padding) for each error.
        E.g.:
            enum=FZ_ERROR_SYSTEM
            typename=mupdf_error_memory
            padding='  '
        '''
        names = fz_error_names
        if include_error_base:
            names = ['BASE'] + names
        for name in names:
            enum = f'{error_name_prefix}{name}'
            typename = rename.error_class( enum)
            padding = (fz_error_names_maxlen - len(name)) * ' '
            yield enum, typename, padding

    # Declare base exception class and define its methods.
    #
    base_name = rename.error_class('FZ_ERROR_BASE')

    out_exceptions_h.write( textwrap.dedent(
            f'''
            /** Base class for exceptions. */
            struct {base_name} : std::exception
            {{
                int         m_code;
                std::string m_text;
                mutable std::string m_what;
                FZ_FUNCTION const char* what() const throw();
                FZ_FUNCTION {base_name}(int code, const char* text);
            }};
            '''))

    out_exceptions_cpp.write( textwrap.dedent(
            f'''
            FZ_FUNCTION {base_name}::{base_name}(int code, const char* text)
            :
            m_code(code),
            m_text(text)
            {{
                {refcheck_if}
                if (s_trace_exceptions)
                {{
                    std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): {base_name}: " << m_text << "\\n";
                }}
                #endif
            }};

            FZ_FUNCTION const char* {base_name}::what() const throw()
            {{
                m_what = "code=" + std::to_string(m_code) + ": " + m_text;
                return m_what.c_str();
            }};

            '''))

    # Generate SWIG Python code to allow conversion of our error class
    # exceptions into equivalent Python exceptions.
    error_classes_n = 0
    for enum, typename, padding in errors():
        error_classes_n += 1

    error_classes_n += 1    # Extra space for FzErrorBase.
    generated.swig_python_exceptions.write( textwrap.dedent( f'''

            void internal_set_error_classes(PyObject* classes);

            %{{
            /* A Python list of Error classes, [FzErrorNone, FzErrorMemory, FzErrorGeneric, ...]. */
            static PyObject* s_error_classes[{error_classes_n}] = {{}};

            /* Called on startup by mupdf.py, with a list of error classes
            to be copied into s_error_classes. This will allow us to create
            instances of these error classes in SWIG's `%exception ...`, so
            Python code will see exceptions as instances of Python error
            classes. */
            void internal_set_error_classes(PyObject* classes)
            {{
                assert(PyList_Check(classes));
                int n = PyList_Size(classes);
                assert(n == {error_classes_n});
                for (int i=0; i<n; ++i)
                {{
                    PyObject* class_ = PyList_GetItem(classes, i);
                    s_error_classes[i] = class_;
                }}
            }}

            /* Sets Python exception to a new mupdf.<name> object constructed
            with `text`. */
            void set_exception(PyObject* class_, int code, const std::string& text)
            {{
                PyObject* args = Py_BuildValue("(s)", text.c_str());
                PyObject* instance = PyObject_CallObject(class_, args);
                PyErr_SetObject(class_, instance);
                Py_XDECREF(instance);
                Py_XDECREF(args);
            }}

            /* Exception handler for swig-generated code. Uses internal
            `throw;` to recover the current C++ exception then uses
            `set_exception()` to set the current Python exception. Caller
            should do `SWIG_fail;` after we return. */
            void handle_exception()
            {{
                try
                {{
                    throw;
                }}
            '''
            ))

    # Declare exception class for each FZ_ERROR_*. Also append catch blocks for
    # each of these exception classes to `handle_exception()`.
    #
    for i, (enum, typename, padding) in enumerate(errors()):
        out_exceptions_h.write( textwrap.dedent(
                f'''
                /** For `{enum}`. */
                struct {typename} : {base_name}
                {{
                    FZ_FUNCTION {typename}(const char* message);
                }};

                '''))

        generated.swig_python_exceptions.write( textwrap.dedent( f'''
                /**/
                    catch (mupdf::{typename}& e)
                    {{
                        if (g_mupdf_trace_exceptions)
                        {{
                            std::cerr
                                    #ifndef _WIN32
                                    << __PRETTY_FUNCTION__ << ": "
                                    #endif
                                    << "Converting C++ std::exception mupdf::{typename} into Python exception: "
                                    << "    e.m_code: " << e.m_code << "\\n"
                                    << "    e.m_text: " << e.m_text << "\\n"
                                    << "    e.what():" << e.what() << "\\n"
                                    << "\\n";
                        }}
                        set_exception(s_error_classes[{i}], e.m_code, e.m_text);

                    }}'''))

    # Append less specific exception handling.
    generated.swig_python_exceptions.write( textwrap.dedent( f'''
                catch (mupdf::FzErrorBase& e)
                {{
                    if (g_mupdf_trace_exceptions)
                    {{
                        std::cerr
                                #ifndef _WIN32
                                << __PRETTY_FUNCTION__ << ": "
                                #endif
                                << "Converting C++ std::exception mupdf::{typename} into Python exception: "
                                << "    e.m_code: " << e.m_code << "\\n"
                                << "    e.m_text: " << e.m_text << "\\n"
                                << "    e.what():" << e.what() << "\\n"
                                << "\\n";
                    }}
                    set_exception(s_error_classes[{error_classes_n-1}], e.m_code, e.m_text);
                }}
                catch (std::exception& e)
                {{
                    if (g_mupdf_trace_exceptions)
                    {{
                        std::cerr
                                #ifndef _WIN32
                                << __PRETTY_FUNCTION__ << ": "
                                #endif
                                << "Converting C++ std::exception into Python exception: "
                                << e.what()
                                << "\\n";
                    }}
                    SWIG_Error(SWIG_RuntimeError, e.what());

                }}
                catch (...)
                {{
                    if (g_mupdf_trace_exceptions)
                    {{
                        std::cerr
                                #ifndef _WIN32
                                << __PRETTY_FUNCTION__ << ": "
                                #endif
                                << "Converting unknown C++ exception into Python exception."
                                << "\\n";
                    }}
                    SWIG_Error(SWIG_RuntimeError, "Unknown exception");
                }}
            }}

            %}}

            %exception
            {{
                try
                {{
                    $action
                }}
                catch (...)
                {{
                    handle_exception();
                    SWIG_fail;
                }}
            }}
            '''))

    generated.swig_python_set_error_classes.write( f'# Define __str()__ for each error/exception class, to use self.what().\n')
    for enum, typename, padding in errors(include_error_base=1):
        generated.swig_python_set_error_classes.write( f'{typename}.__str__ = lambda self: self.what()\n')

    generated.swig_python_set_error_classes.write( textwrap.dedent( f'''
            # This must be after the declaration of mupdf::FzError*
            # classes in mupdf/exceptions.h and declaration of
            # `internal_set_error_classes()`, otherwise generated code is
            # before the declaration of the Python class or similar. */
            internal_set_error_classes([
            '''))
    for enum, typename, padding in errors():
        generated.swig_python_set_error_classes.write(f'        {typename},\n')
    generated.swig_python_set_error_classes.write( textwrap.dedent( f'''
                    FzErrorBase,
                    ])
            '''))

    # Define constructor for each exception class.
    #
    for enum, typename, padding in errors():
        out_exceptions_cpp.write( textwrap.dedent(
                f'''
                FZ_FUNCTION {typename}::{typename}(const char* text)
                : {base_name}({enum}, text)
                {{
                    {refcheck_if}
                    if (s_trace_exceptions)
                    {{
                        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): {typename} constructor, text: " << m_text << "\\n";
                    }}
                    #endif
                }}

                '''))

    # Generate function that throws an appropriate exception from a fz_context.
    #
    throw_exception = rename.internal( 'throw_exception')
    out_exceptions_h.write( textwrap.dedent(
            f'''
            /** Throw exception appropriate for error in `ctx`. */
            FZ_FUNCTION void {throw_exception}(fz_context* ctx);

            '''))
    out_exceptions_cpp.write( textwrap.dedent(
            f'''
            FZ_FUNCTION void {throw_exception}(fz_context* ctx)
            {{
                int code;
                const char* text = fz_convert_error(ctx, &code);
                {refcheck_if}
                if (s_trace_exceptions)
                {{
                    std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): code=" << code << "\\n";
                }}
                #endif
                {refcheck_if}
                if (s_trace_exceptions)
                {{
                    std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "(): text=" << text << "\\n";
                }}
                #endif
            '''))
    for enum, typename, padding in errors():
        out_exceptions_cpp.write( f'    if (code == {enum}) {padding}throw {typename}{padding}(text);\n')
    out_exceptions_cpp.write( f'    throw {base_name}(code, text);\n')
    out_exceptions_cpp.write( f'}}\n')
    out_exceptions_cpp.write( '\n')

    make_internal_functions( namespace, out_internal_h, out_internal_cpp, refcheck_if)

    # Generate wrappers for each function that we find.
    #
    functions = []
    for fnname, cursor in state.state_.find_functions_starting_with( tu, ('fz_', 'pdf_'), method=False):
        assert fnname not in state.omit_fns
        #jlib.log( '{fnname=} {cursor.spelling=} {cursor.type.spelling=}')
        if ( cursor.type == state.clang.cindex.TypeKind.FUNCTIONPROTO
                and cursor.type.is_function_variadic()
                ):
            # We don't attempt to wrap variadic functions - would need to find
            # the equivalent function that takes a va_list.
            if 0:
                jlib.log( 'Variadic fn: {cursor.type.spelling=}')
            if fnname != 'fz_warn':
                continue
        if fnname == 'fz_push_try':
            # This is partof implementation of fz_try/catch so doesn't make
            # sense to provide a wrapper. Also it is OS-dependent so including
            # it makes our generated code OS-specific.
            continue

        functions.append( (fnname, cursor))

    jlib.log( '{len(functions)=}')

    # Sort by function-name to make output easier to read.
    functions.sort()
    for fnname, cursor in functions:
        if state.state_.show_details( fnname):
            jlib.log( 'Looking at {fnname}')
        fnname_wrapper = rename.ll_fn( fnname)
        function_wrapper(
                tu,
                cursor,
                fnname,
                fnname_wrapper,
                out_functions_h,
                out_functions_cpp,
                generated,
                refcheck_if,
                )
        if not fnname.startswith( ( 'fz_keep_', 'fz_drop_', 'pdf_keep_', 'pdf_drop_')):
            function_wrapper_class_aware(
                    tu,
                    register_fn_use=None,
                    struct_name=None,
                    class_name=None,
                    fn_cursor=cursor,
                    refcheck_if=refcheck_if,
                    fnname=fnname,
                    out_h=out_functions_h2,
                    out_cpp=out_functions_cpp2,
                    generated=generated,
                    )

        python.cppyy_add_outparams_wrapper( tu, fnname, cursor, state.state_, generated)

        if fnname == "pdf_load_field_name":  #(fz_context *ctx, pdf_obj *field);
            # Output wrapper that returns std::string instead of buffer that
            # caller needs to free.
            out_functions_h.write(
                    textwrap.dedent(
                    f'''
                    /** Alternative to `{rename.ll_fn('pdf_load_field_name')}()` that returns a std::string. */
                    FZ_FUNCTION std::string {rename.ll_fn('pdf_load_field_name2')}(pdf_obj* field);

                    '''))
            out_functions_cpp.write(
                    textwrap.dedent(
                    f'''
                    FZ_FUNCTION std::string {rename.ll_fn('pdf_load_field_name2')}(pdf_obj* field)
                    {{
                        char* buffer = {rename.ll_fn('pdf_load_field_name')}( field);
                        std::string ret( buffer);
                        {rename.ll_fn('fz_free')}( buffer);
                        return ret;
                    }}
                    '''))
            out_functions_h2.write(
                    textwrap.indent(
                        textwrap.dedent(
                        f'''
                        /** Alternative to `{rename.fn('pdf_load_field_name')}()` that returns a std::string. */
                        FZ_FUNCTION std::string {rename.fn('pdf_load_field_name2')}({rename.class_('pdf_obj')}& field);
                        '''),
                        '    ',
                        )
                    )
            out_functions_cpp2.write(
                    textwrap.dedent(
                    f'''
                    FZ_FUNCTION std::string {rename.fn('pdf_load_field_name2')}({rename.class_('pdf_obj')}& field)
                    {{
                        return {rename.ll_fn('pdf_load_field_name2')}( field.m_internal);
                    }}
                    '''))

    # Output custom wrappers for variadic pdf_dict_getl().
    #

    decl = f'''FZ_FUNCTION pdf_obj* {rename.ll_fn('pdf_dict_getlv')}( pdf_obj* dict, va_list keys)'''
    out_functions_h.write( textwrap.dedent( f'''
            /* Low-level wrapper for `pdf_dict_getl()`. `keys` must be null-terminated list of `pdf_obj*`'s. */
            {decl};
            '''))
    out_functions_cpp.write( textwrap.dedent( f'''
            {decl}
            {{
                pdf_obj *key;
                while (dict != NULL && (key = va_arg(keys, pdf_obj *)) != NULL)
                {{
                    dict = {rename.ll_fn('pdf_dict_get')}( dict, key);
                }}
                return dict;
            }}
            '''))

    decl = f'''FZ_FUNCTION pdf_obj* {rename.ll_fn('pdf_dict_getl')}( pdf_obj* dict, ...)'''
    out_functions_h.write( textwrap.dedent( f'''
            /* Low-level wrapper for `pdf_dict_getl()`. `...` must be null-terminated list of `pdf_obj*`'s. */
            {decl};
            '''))
    out_functions_cpp.write( textwrap.dedent( f'''
            {decl}
            {{
                va_list keys;
                va_start(keys, dict);
                try
                {{
                    dict = {rename.ll_fn('pdf_dict_getlv')}( dict, keys);
                }}
                catch( std::exception&)
                {{
                    va_end(keys);
                    throw;
                }}
                va_end(keys);
                return dict;
            }}
            '''))

    decl = f'''FZ_FUNCTION {rename.class_('pdf_obj')} {rename.fn('pdf_dict_getlv')}( {rename.class_('pdf_obj')}& dict, va_list keys)'''
    out_functions_h2.write(
            textwrap.indent(
                textwrap.dedent( f'''
                    /* Class-aware wrapper for `pdf_dict_getl()`. `keys` must be null-terminated list of
                    `pdf_obj*`'s, not `{rename.class_('pdf_obj')}*`'s, so that conventional
                    use with `PDF_NAME()` works. */
                    {decl};
                    '''),
                '    ',
                )
            )
    out_functions_cpp2.write( textwrap.dedent( f'''
            {decl}
            {{
                pdf_obj* ret = {rename.ll_fn('pdf_dict_getlv')}( dict.m_internal, keys);
                return {rename.class_('pdf_obj')}( {rename.ll_fn('pdf_keep_obj')}( ret));
            }}
            '''))

    decl = f'''FZ_FUNCTION {rename.class_('pdf_obj')} {rename.fn('pdf_dict_getl')}( {rename.class_('pdf_obj')}* dict, ...)'''
    out_functions_h2.write(
            textwrap.indent(
                textwrap.dedent( f'''
                    /* Class-aware wrapper for `pdf_dict_getl()`. `...` must be null-terminated list of
                    `pdf_obj*`'s, not `{rename.class_('pdf_obj')}*`'s, so that conventional
                    use with `PDF_NAME()` works. [We use pointer `dict` arg because variadic
                    args do not with with reference args.] */
                    {decl};
                    '''),
                '    ',
                ),
            )
    out_functions_cpp2.write( textwrap.dedent( f'''
            {decl}
            {{
                va_list keys;
                va_start(keys, dict);
                try
                {{
                    {rename.class_('pdf_obj')} ret = {rename.fn('pdf_dict_getlv')}( *dict, keys);
                    va_end( keys);
                    return ret;
                }}
                catch (std::exception&)
                {{
                    va_end( keys);
                    throw;
                }}
            }}
            '''))

    # Write custom functions to allow calling of fz_document_handler function
    # pointers.
    #
    # Would be good to extend function_wrapper() and
    # function_wrapper_class_aware() to work with fnptr type as well as actual
    # functions. But for now we specify things manually and don't support
    # passing wrapper classes.
    #
    def fnptr_wrapper(
            return_type,
            fnptr,
            fnptr_args, # Must include leading comma.
            fnptr_arg_names, # Must include leading comma.
            ):
        decl = f'''FZ_FUNCTION {return_type} {rename.ll_fn(fnptr)}_call({fnptr} fn{fnptr_args})'''
        out_functions_h.write(
                textwrap.indent(
                    textwrap.dedent( f'''
                        /* Helper for calling a {fnptr}. Provides a `fz_context` and coverts
                        fz_try..fz_catch exceptions into C++ exceptions. */
                        {decl};
                        '''),
                    '    ',
                    )
                )
        out_functions_cpp.write( textwrap.dedent( f'''
                {decl}
                {{
                    fz_context* ctx = mupdf::internal_context_get();
                    {return_type} ret;
                    fz_try(ctx)
                    {{
                        ret = fn( ctx{fnptr_arg_names});
                    }}
                    fz_catch(ctx)
                    {{
                        mupdf::internal_throw_exception( ctx);
                    }}
                    return ret;
                }}
                '''))
    fnptr_wrapper(
            'fz_document*',
            'fz_document_open_fn',
            ', const char* filename',
            ', filename',
            )
    fnptr_wrapper(
            'fz_document*',
            'fz_document_open_with_stream_fn',
            ', fz_stream* stream',
            ', stream',
            )
    fnptr_wrapper(
            'fz_document*',
            'fz_document_open_accel_fn',
            ', const char* filename, const char* accel',
            ', filename, accel',
            )
    fnptr_wrapper(
            'fz_document*',
            'fz_document_open_accel_with_stream_fn',
            ', fz_stream* stream, fz_stream* accel',
            ', stream, accel',
            )


def class_add_iterator( tu, struct_cursor, struct_name, classname, extras, refcheck_if):
    '''
    Add begin() and end() methods so that this generated class is iterable
    from C++ with:

        for (auto i: foo) {...}

    We modify <extras> to create an iterator class and add begin() and end()
    methods that each return an instance of the iterator class.
    '''
    it_begin, it_end = extras.iterator_next

    # Figure out type of what the iterator returns by looking at type of
    # <it_begin>.
    if it_begin:
        c = parse.find_name( struct_cursor, it_begin)
        assert c.type.kind == state.clang.cindex.TypeKind.POINTER
        it_internal_type = state.get_name_canonical( c.type.get_pointee()).spelling
        it_internal_type = util.clip( it_internal_type, 'struct ')
        it_type = rename.class_( it_internal_type)
    else:
        # The container is also the first item in the linked list.
        it_internal_type = struct_name
        it_type = classname

    # We add to extras.methods_extra().
    #
    check_refs = 1 if parse.has_refs( tu, struct_cursor.type) else 0
    extras.methods_extra.append(
            classes.ExtraMethod( f'{classname}Iterator', 'begin()',
                    f'''
                    {{
                        auto ret = {classname}Iterator({'m_internal->'+it_begin if it_begin else '*this'});
                        {refcheck_if}
                        #if {check_refs}
                        if (s_check_refs)
                        {{
                            s_{classname}_refs_check.check( this, __FILE__, __LINE__, __FUNCTION__);
                        }}
                        #endif
                        #endif
                        return ret;
                    }}
                    ''',
                    f'/* Used for iteration over linked list of {it_type} items starting at {it_internal_type}::{it_begin}. */',
                    ),
            )
    extras.methods_extra.append(
            classes.ExtraMethod( f'{classname}Iterator', 'end()',
                    f'''
                    {{
                        auto ret = {classname}Iterator(NULL);
                        {refcheck_if}
                        #if {check_refs}
                        if (s_check_refs)
                        {{
                            s_{classname}_refs_check.check( this, __FILE__, __LINE__, __FUNCTION__);
                        }}
                        #endif
                        #endif
                        return ret;
                    }}
                    ''',
                    f'/* Used for iteration over linked list of {it_type} items starting at {it_internal_type}::{it_begin}. */',
                    ),
            )

    extras.class_bottom += f'\n    typedef {classname}Iterator iterator;\n'

    extras.class_pre += f'\nstruct {classname}Iterator;\n'

    extras.class_post += f'''
            struct {classname}Iterator
            {{
                FZ_FUNCTION {classname}Iterator(const {it_type}& item);
                FZ_FUNCTION {classname}Iterator& operator++();
                FZ_FUNCTION bool operator==( const {classname}Iterator& rhs);
                FZ_FUNCTION bool operator!=( const {classname}Iterator& rhs);
                FZ_FUNCTION {it_type} operator*();
                FZ_FUNCTION {it_type}* operator->();
                private:
                {it_type} m_item;
            }};
            '''
    keep_text = ''
    if extras.copyable and extras.copyable != 'default':
        # Our operator++ needs to create it_type from m_item.m_internal->next,
        # so we need to call fz_keep_<it_type>().
        #
        # [Perhaps life would be simpler if our generated constructors always
        # called fz_keep_*() as necessary? In some circumstances this would
        # require us to call fz_drop_*() when constructing an instance, but
        # that might be simpler?]
        #
        base_name = util.clip( struct_name, ('fz_', 'pdf_'))
        if struct_name.startswith( 'fz_'):
            keep_name = f'fz_keep_{base_name}'
        elif struct_name.startswith( 'pdf_'):
            keep_name = f'pdf_keep_{base_name}'
        keep_name = rename.ll_fn(keep_name)
        keep_text = f'{keep_name}(m_item.m_internal->next);'

    extras.extra_cpp += f'''
            FZ_FUNCTION {classname}Iterator::{classname}Iterator(const {it_type}& item)
            : m_item( item)
            {{
            }}
            FZ_FUNCTION {classname}Iterator& {classname}Iterator::operator++()
            {{
                {keep_text}
                m_item = {it_type}(m_item.m_internal->next);
                return *this;
            }}
            FZ_FUNCTION bool {classname}Iterator::operator==( const {classname}Iterator& rhs)
            {{
                return m_item.m_internal == rhs.m_item.m_internal;
            }}
            FZ_FUNCTION bool {classname}Iterator::operator!=( const {classname}Iterator& rhs)
            {{
                return m_item.m_internal != rhs.m_item.m_internal;
            }}
            FZ_FUNCTION {it_type} {classname}Iterator::operator*()
            {{
                return m_item;
            }}
            FZ_FUNCTION {it_type}* {classname}Iterator::operator->()
            {{
                return &m_item;
            }}

            '''


def class_find_constructor_fns( tu, classname, struct_name, base_name, extras):
    '''
    Returns list of functions that could be used as constructors of the
    specified wrapper class.

    For example we look for functions that return a pointer to <struct_name> or
    return a POD <struct_name> by value.

    tu:
        .
    classname:
        Name of our wrapper class.
    struct_name:
        Name of underlying mupdf struct.
    base_name:
        Name of struct without 'fz_' prefix.
    extras:
        .
    '''
    assert struct_name == f'fz_{base_name}' or struct_name == f'pdf_{base_name}'
    verbose = state.state_.show_details( struct_name)
    constructor_fns = []
    if '-' not in extras.constructor_prefixes:
        # Add default constructor fn prefix.
        if struct_name.startswith( 'fz_'):
            extras.constructor_prefixes.insert( 0, f'fz_new_')
            extras.constructor_prefixes.insert( 0, f'pdf_new_')
        elif struct_name.startswith( 'pdf_'):
            extras.constructor_prefixes.insert( 0, f'pdf_new_')
    for fnprefix in extras.constructor_prefixes:
        if verbose:
            jlib.log('{struct_name=} {fnprefix=}')
        for fnname, cursor in state.state_.find_functions_starting_with( tu, fnprefix, method=True):
            # Check whether this has identical signature to any fn we've
            # already found.
            if verbose:
                jlib.log( '{struct_name=} {fnname=}')
            duplicate_type = None
            duplicate_name = False
            for f, c, is_duplicate in constructor_fns:
                if verbose:
                    jlib.log( '{struct_name=} {cursor.spelling=} {c.type.spelling=}')
                if f == fnname:
                    if verbose:
                        jlib.log('setting duplicate_name to true')
                    duplicate_name = True
                    break
                if c.type == cursor.type:
                    if verbose:
                        jlib.log( '{struct_name} wrapper: ignoring candidate constructor {fnname}() because prototype is indistinguishable from {f=}()')
                    duplicate_type = f
                    break
            if duplicate_name:
                continue
            ok = False

            arg, n = parse.get_first_arg( tu, cursor)
            if arg and n == 1 and parse.is_pointer_to( arg.cursor.type, struct_name):
                # This avoids generation of bogus copy constructor wrapping
                # function fz_new_pixmap_from_alpha_channel() introduced
                # 2021-05-07.
                #
                if verbose:
                    jlib.log('ignoring possible constructor because looks like copy constructor: {fnname}')
            elif fnname in extras.constructor_excludes:
                if verbose:
                    jlib.log('{fnname=} is in {extras.constructor_excludes=}')
            elif extras.pod and extras.pod != 'none' and state.get_name_canonical( cursor.result_type).spelling == f'{struct_name}':
                # Returns POD struct by value.
                ok = True
            elif not extras.pod and parse.is_pointer_to( cursor.result_type, f'{struct_name}'):
                # Returns pointer to struct.
                ok = True

            if ok:
                if duplicate_type and extras.copyable:
                    if verbose:
                        jlib.log1( 'adding static method wrapper for {fnname}')
                    extras.method_wrappers_static.append( fnname)
                else:
                    if duplicate_type:
                        if verbose:
                            jlib.log( 'not able to provide static factory fn {struct_name}::{fnname} because wrapper class is not copyable.')
                    if verbose:
                        jlib.log( 'adding constructor wrapper for {fnname}')
                    constructor_fns.append( (fnname, cursor, duplicate_type))
            else:
                if verbose:
                    jlib.log( 'ignoring possible constructor for {classname=} because does not return required type: {fnname=} -> {cursor.result_type.spelling=}')

    constructor_fns.sort()
    return constructor_fns


def class_find_destructor_fns( tu, struct_name, base_name):
    '''
    Returns list of functions that could be used by destructor - must be called
    'fz_drop_<typename>', must take a <struct>* arg, may take a fz_context*
    arg.
    '''
    if struct_name.startswith( 'fz_'):
        destructor_prefix = f'fz_drop_{base_name}'
    elif struct_name.startswith( 'pdf_'):
        destructor_prefix = f'pdf_drop_{base_name}'
    destructor_fns = []
    for fnname, cursor in state.state_.find_functions_starting_with( tu, destructor_prefix, method=True):
        arg_struct = False
        arg_context = False
        args_num = 0
        for arg in parse.get_args( tu, cursor):
            if not arg_struct and parse.is_pointer_to( arg.cursor.type, struct_name):
                arg_struct = True
            elif not arg_context and parse.is_pointer_to( arg.cursor.type, 'fz_context'):
                arg_context = True
            args_num += 1
        if arg_struct:
            if args_num == 1 or (args_num == 2 and arg_context):
                # No params other than <struct>* and fz_context* so this is
                # candidate destructor.
                #log( 'adding candidate destructor: {fnname}')
                destructor_fns.append( (fnname, cursor))

    destructor_fns.sort()
    return destructor_fns


def class_constructor_default(
        tu,
        struct_cursor,
        classname,
        extras,
        out_h,
        out_cpp,
        refcheck_if,
        ):
    '''
    Generates constructor that sets each member to default value.
    '''
    if extras.pod:
        comment = f'Default constructor, sets each member to default value.'
    else:
        comment = f'Default constructor, sets `m_internal` to null.'
    out_h.write( '\n')
    out_h.write( f'    /** {comment} */\n')
    out_h.write( f'    FZ_FUNCTION {classname}();\n')

    out_cpp.write( f'/** {comment} */\n')
    out_cpp.write( f'FZ_FUNCTION {classname}::{classname}()\n')
    if not extras.pod:
        out_cpp.write( f': m_internal(nullptr)\n')
    out_cpp.write( f'{{\n')
    if extras.pod == 'none':
        pass
    elif extras.pod:
        for c in parse.get_members(struct_cursor):
            if extras.pod == 'inline':
                c_name = f'this->{c.spelling}'
            else:
                c_name = f'this->m_internal.{c.spelling}'
            if c.type.kind == state.clang.cindex.TypeKind.CONSTANTARRAY:
                out_cpp.write( f'    memset(&{c_name}, 0, sizeof({c_name}));\n')
            else:
                out_cpp.write( f'    {c_name} = {{}};\n')
    else:
        if parse.has_refs( tu, struct_cursor.type):
            out_cpp.write(f'    {refcheck_if}\n')
            out_cpp.write( '    if (s_check_refs)\n')
            out_cpp.write( '    {\n')
            out_cpp.write(f'        s_{classname}_refs_check.add( this, __FILE__, __LINE__, __FUNCTION__);\n')
            out_cpp.write( '    }\n')
            out_cpp.write( '    #endif\n')
    out_cpp.write( f'}};\n')


def class_copy_constructor(
        tu,
        functions,
        struct_name,
        struct_cursor,
        base_name,
        classname,
        constructor_fns,
        out_h,
        out_cpp,
        refcheck_if,
        ):
    '''
    Generate a copy constructor and operator= by finding a suitable fz_keep_*()
    function.

    We raise an exception if we can't find one.
    '''
    if struct_name.startswith( 'fz_'):
        keep_name = f'fz_keep_{base_name}'
        drop_name = f'fz_drop_{base_name}'
    elif struct_name.startswith( 'pdf_'):
        keep_name = f'pdf_keep_{base_name}'
        drop_name = f'pdf_drop_{base_name}'
    for name in keep_name, drop_name:
        cursor = state.state_.find_function( tu, name, method=True)
        if not cursor:
            classextra = classes.classextras.get( tu, struct_name)
            if classextra.copyable:
                if 1 or state.state_.show_details( struct_name):
                    jlib.log( 'changing to non-copyable because no function {name}(): {struct_name}')
                classextra.copyable = False
            return
        if name == keep_name:
            pvoid = parse.is_pointer_to( cursor.result_type, 'void')
            assert ( pvoid
                    or parse.is_pointer_to( cursor.result_type, struct_name)
                    ), (
                    f'Function {name}(): result_type not void* or pointer to {struct_name}: {cursor.result_type.spelling}'
                    )
        arg, n = parse.get_first_arg( tu, cursor)
        assert n == 1, f'should take exactly one arg: {cursor.spelling}()'
        assert parse.is_pointer_to( arg.cursor.type, struct_name), (
                f'arg0 is not pointer to {struct_name}: {cursor.spelling}(): {arg.cursor.spelling} {arg.name}')

    for fnname, cursor, duplicate_type in constructor_fns:
        fnname2 = rename.ll_fn(fnname)
        if fnname2 == keep_name:
            jlib.log( 'not generating copy constructor with {keep_name=} because already used by a constructor.')
            break
    else:
        functions( keep_name)
        comment = f'Copy constructor using `{keep_name}()`.'
        out_h.write( '\n')
        out_h.write( f'    /** {comment} */\n')
        out_h.write( f'    FZ_FUNCTION {classname}(const {classname}& rhs);\n')
        out_h.write( '\n')

        cast = ''
        if pvoid:
            # Need to cast the void* to the correct type.
            cast = f'(::{struct_name}*) '

        out_cpp.write( f'/** {comment} */\n')
        out_cpp.write( f'FZ_FUNCTION {classname}::{classname}(const {classname}& rhs)\n')
        out_cpp.write( f': m_internal({cast}{rename.ll_fn(keep_name)}(rhs.m_internal))\n')
        out_cpp.write( '{\n')

        # Write trace code.
        out_cpp.write( f'    {refcheck_if}\n')
        out_cpp.write( f'    if (s_trace_keepdrop) {{\n')
        out_cpp.write( f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "():"\n')
        out_cpp.write( f'                << " have called {rename.ll_fn(keep_name)}(rhs.m_internal)\\n"\n')
        out_cpp.write( f'                ;\n')
        out_cpp.write( f'    }}\n')
        out_cpp.write( f'    #endif\n')

        if parse.has_refs( tu, struct_cursor.type):
            out_cpp.write(f'    {refcheck_if}\n')
            out_cpp.write( '    if (s_check_refs)\n')
            out_cpp.write( '    {\n')
            out_cpp.write(f'        s_{classname}_refs_check.add( this, __FILE__, __LINE__, __FUNCTION__);\n')
            out_cpp.write( '    }\n')
            out_cpp.write( '    #endif\n')
        out_cpp.write( '}\n')
        out_cpp.write( '\n')

    # Make operator=().
    #
    comment = f'operator= using `{keep_name}()` and `{drop_name}()`.'
    out_h.write( f'    /** {comment} */\n')
    out_h.write( f'    FZ_FUNCTION {classname}& operator=(const {classname}& rhs);\n')

    out_cpp.write( f'/* {comment} */\n')
    out_cpp.write( f'FZ_FUNCTION {classname}& {classname}::operator=(const {classname}& rhs)\n')
    out_cpp.write(  '{\n')
    out_cpp.write( f'    {refcheck_if}\n')
    out_cpp.write( f'    if (s_trace_keepdrop) {{\n')
    out_cpp.write( f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "():"\n')
    out_cpp.write( f'                << " calling {rename.ll_fn(drop_name)}(this->m_internal)"\n')
    out_cpp.write( f'                << " and {rename.ll_fn(keep_name)}(rhs.m_internal)\\n"\n')
    out_cpp.write( f'                ;\n')
    out_cpp.write( f'    }}\n')
    out_cpp.write( f'    #endif\n')
    out_cpp.write( f'    {rename.ll_fn(drop_name)}(this->m_internal);\n')
    out_cpp.write( f'    {rename.ll_fn(keep_name)}(rhs.m_internal);\n')
    if parse.has_refs( tu, struct_cursor.type):
        out_cpp.write(f'    {refcheck_if}\n')
        out_cpp.write( '    if (s_check_refs)\n')
        out_cpp.write( '    {\n')
        out_cpp.write(f'        s_{classname}_refs_check.remove( this, __FILE__, __LINE__, __FUNCTION__);\n')
        out_cpp.write( '    }\n')
        out_cpp.write( '    #endif\n')
    out_cpp.write( f'    this->m_internal = {cast}rhs.m_internal;\n')
    if parse.has_refs( tu, struct_cursor.type):
        out_cpp.write(f'    {refcheck_if}\n')
        out_cpp.write( '    if (s_check_refs)\n')
        out_cpp.write( '    {\n')
        out_cpp.write(f'        s_{classname}_refs_check.add( this, __FILE__, __LINE__, __FUNCTION__);\n')
        out_cpp.write( '    }\n')
        out_cpp.write( '    #endif\n')
    out_cpp.write( f'    return *this;\n')
    out_cpp.write(  '}\n')
    out_cpp.write(  '\n')

def function_name_implies_kept_references( fnname):
    '''
    Returns true if <fnname> implies the function would return kept
    reference(s).
    '''
    if fnname in (
            'pdf_page_write',
            'fz_decomp_image_from_stream',
            'fz_get_pixmap_from_image',
            ):
        return True
    for i in ('new', 'create', 'find', 'load', 'open', 'keep', 'read', 'add', 'parse', 'graft', 'copy', 'deep_copy'):
        if fnname.startswith(f'fz_{i}_') or fnname.startswith(f'pdf_{i}_'):
            if state.state_.show_details(fnname):
                jlib.log('Assuming that {fnname=} returns a kept reference.')
            return True
    return False


def function_wrapper_class_aware_body(
        tu,
        fnname,
        out_cpp,
        struct_name,
        class_name,
        class_static,
        class_constructor,
        extras,
        struct_cursor,
        fn_cursor,
        return_cursor,
        wrap_return,
        refcheck_if,
        ):
    '''
    Writes function or method body to <out_cpp> that calls a generated C++ wrapper
    function.

    fnname:
        .
    out_cpp:
        .
    struct_name:
        If false, we write a class-aware wrapping function body. Otherwise name
        of struct such as 'fz_rect' and we write method body for the struct's
        wrapper class.
    class_name:
    class_static:
        If true, this is a static class method.
    class_constructor:
        If true, this is a constructor.
    extras:
        .
    struct_cursor:
        .
    fn_cursor:
        Cursor for the underlying MuPDF function.
    return_cursor:
        If not None, the cursor for definition of returned type.
    wrap_return:
        If 'pointer', the underlying function returns a pointer to a struct
        that we wrap.

        If 'value' the underlying function returns, by value, a
        struct that we wrap, so we need to construct our wrapper from the
        address of this value.

        Otherwise we don't wrap the returned value.
    '''
    verbose = state.state_.show_details( fnname)
    out_cpp.write( f'{{\n')
    return_void = (fn_cursor.result_type.spelling == 'void')

    # Write trace code.
    out_cpp.write( f'    {refcheck_if}\n')
    out_cpp.write( f'    if (s_trace) {{\n')
    out_cpp.write( f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << "():"\n')
    out_cpp.write( f'                << " calling mupdf::{rename.ll_fn(fnname)}()\\n";\n')
    out_cpp.write( f'    }}\n')
    out_cpp.write( f'    #endif\n')

    if fn_cursor.type.is_function_variadic():
        assert fnname == 'fz_warn', f'{fnname=}'
        out_cpp.write( f'    va_list ap;\n')
        out_cpp.write( f'    va_start( ap, fmt);\n')
        out_cpp.write( f'    {rename.ll_fn("fz_vwarn")}( fmt, ap);\n')
        out_cpp.write( f'    va_end( ap);\n')

    elif class_constructor or not struct_name:
        # This code can generate a class method, but we choose to not use this,
        # instead method body simply calls the class-aware function (see below).
        def get_keep_drop(arg):
            name = util.clip( arg.alt.type.spelling, 'struct ')
            if name.startswith('fz_'):
                prefix = 'fz'
                name = name[3:]
            elif name.startswith('pdf_'):
                prefix = 'pdf'
                name = name[4:]
            else:
                assert 0
            return rename.ll_fn(f'{prefix}_keep_{name}'), rename.ll_fn(f'{prefix}_drop_{name}')

        # Handle wrapper-class out-params - need to drop .m_internal and set to
        # null.
        #
        # fixme: maybe instead simply call <arg.name>'s destructor directly?
        #
        for arg in parse.get_args( tu, fn_cursor):
            if arg.alt and arg.out_param:
                if parse.has_refs(tu, arg.alt.type):
                    keep_fn, drop_fn = get_keep_drop(arg)
                    out_cpp.write( f'    /* Out-param {arg.name}.m_internal will be overwritten. */\n')
                    out_cpp.write( f'    {drop_fn}({arg.name}.m_internal);\n')
                    out_cpp.write( f'    {arg.name}.m_internal = nullptr;\n')

        # Write function call.
        if class_constructor:
            if extras.pod:
                if extras.pod == 'inline':
                    out_cpp.write( f'    *(::{struct_name}*) &this->{parse.get_field0(struct_cursor.type).spelling} = ')
                else:
                    out_cpp.write( f'    this->m_internal = ')
                if fn_cursor.result_type.kind == state.clang.cindex.TypeKind.POINTER:
                    out_cpp.write( f'*')
            else:
                out_cpp.write( f'    this->m_internal = ')
                if fn_cursor.result_type.kind == state.clang.cindex.TypeKind.POINTER:
                    pass
                else:
                    assert 0, 'cannot handle underlying fn returning by value when not pod.'
            out_cpp.write( f'{rename.ll_fn(fnname)}(')
        elif wrap_return == 'value':
            out_cpp.write( f'    {_make_top_level(return_cursor.spelling)} temp = mupdf::{rename.ll_fn(fnname)}(')
        elif wrap_return == 'pointer':
            out_cpp.write( f'    {_make_top_level(return_cursor.spelling)}* temp = mupdf::{rename.ll_fn(fnname)}(')
        elif wrap_return == 'const pointer':
            out_cpp.write( f'    const {_make_top_level(return_cursor.spelling)}* temp = mupdf::{rename.ll_fn(fnname)}(')
        elif return_void:
            out_cpp.write( f'    mupdf::{rename.ll_fn(fnname)}(')
        else:
            out_cpp.write( f'    auto ret = mupdf::{rename.ll_fn(fnname)}(')

        have_used_this = False
        sep = ''
        for arg in parse.get_args( tu, fn_cursor):
            arg_classname = class_name
            if class_static or class_constructor:
                arg_classname = None
            out_cpp.write( sep)
            have_used_this = write_call_arg(
                    tu,
                    arg,
                    arg_classname,
                    have_used_this,
                    out_cpp,
                    state.state_.show_details(fnname),
                    )
            sep = ', '
        out_cpp.write( f');\n')

        if state.state_.show_details(fnname):
            jlib.log('{=wrap_return}')
        refcounted_return = False
        if wrap_return in ('pointer', 'const pointer') and parse.has_refs( tu, return_cursor.type):
            refcounted_return = True
            refcounted_return_struct_cursor = return_cursor
        elif class_constructor and parse.has_refs( tu, struct_cursor.type):
            refcounted_return = True
            refcounted_return_struct_cursor = struct_cursor

        if refcounted_return:
            # This MuPDF function returns pointer to a struct which uses reference
            # counting. If the function returns a borrowed reference, we need
            # to increment its reference count before passing it to our wrapper
            # class's constructor.
            #
            #jlib.log('Function returns pointer to {return_cursor=}')
            return_struct_name = util.clip( refcounted_return_struct_cursor.spelling, 'struct ')
            if return_struct_name.startswith('fz_'):
                prefix = 'fz_'
            elif return_struct_name.startswith('pdf_'):
                prefix = 'pdf_'
            else:
                prefix = None
            if state.state_.show_details(fnname):
                jlib.log('{=prefix}')
            if prefix:
                if function_name_implies_kept_references( fnname):
                    pass
                    #out_cpp.write( f'    /* We assume that {fnname} returns a kept reference. */\n')
                else:
                    if state.state_.show_details(fnname):
                        jlib.log('{=classname fnname constructor} Assuming that {fnname=} returns a borrowed reference.')
                    # This function returns a borrowed reference.
                    suffix = return_struct_name[ len(prefix):]
                    keep_fn = f'{prefix}keep_{suffix}'
                    #jlib.log('Function assumed to return borrowed reference: {fnname=} => {return_struct_name=} {keep_fn=}')
                    #out_cpp.write( f'    /* We assume that {fnname} returns a borrowed reference. */\n')
                    if class_constructor:
                        out_cpp.write( f'    {rename.ll_fn(keep_fn)}(this->m_internal);\n')
                    else:
                        out_cpp.write( f'    {rename.ll_fn(keep_fn)}(temp);\n')

        if wrap_return == 'value':
            out_cpp.write( f'    auto ret = {rename.class_(return_cursor.spelling)}(&temp);\n')
        elif wrap_return in ('pointer', 'const pointer'):
            out_cpp.write( f'    auto ret = {rename.class_(return_cursor.spelling)}(temp);\n')

        # Handle wrapper-class out-params - need to keep arg.m_internal if
        # fnname implies it will be a borrowed reference.
        for arg in parse.get_args( tu, fn_cursor):
            if arg.alt and arg.out_param:
                if parse.has_refs(tu, arg.alt.type):
                    if function_name_implies_kept_references( fnname):
                        out_cpp.write( f'    /* We assume that out-param {arg.name}.m_internal is a kept reference. */\n')
                    else:
                        keep_fn, drop_fn = get_keep_drop(arg)
                        out_cpp.write( f'    /* We assume that out-param {arg.name}.m_internal is a borrowed reference. */\n')
                        out_cpp.write( f'    {keep_fn}({arg.name}.m_internal);\n')
    else:
        # Class method simply calls the class-aware function, which will have
        # been generated elsewhere.
        out_cpp.write( '    ')
        if not return_void:
            out_cpp.write( 'auto ret = ')

        out_cpp.write( f'mupdf::{rename.fn(fnname)}(')
        sep = ''
        for i, arg in enumerate( parse.get_args( tu, fn_cursor)):
            out_cpp.write( sep)
            if i==0 and not class_static:
                out_cpp.write( '*this')
            else:
                out_cpp.write( f'{arg.name}')
            sep = ', '

        out_cpp.write( ');\n')

    if struct_name and not class_static:
        if parse.has_refs( tu, struct_cursor.type):
            # Write code that does runtime checking of reference counts.
            out_cpp.write( f'    {refcheck_if}\n')
            out_cpp.write( f'    if (s_check_refs)\n')
            out_cpp.write( f'    {{\n')
            if class_constructor:
                out_cpp.write( f'        s_{class_name}_refs_check.add( this, __FILE__, __LINE__, __FUNCTION__);\n')
            else:
                out_cpp.write( f'        s_{class_name}_refs_check.check( this, __FILE__, __LINE__, __FUNCTION__);\n')
            out_cpp.write( f'    }}\n')
            out_cpp.write( f'    #endif\n')

    if not return_void and not class_constructor:
        out_cpp.write( f'    return ret;\n')

    out_cpp.write( f'}}\n')
    out_cpp.write( f'\n')


def function_wrapper_class_aware(
        tu,
        register_fn_use,
        fnname,
        out_h,
        out_cpp,
        struct_name,
        class_name,
        fn_cursor,
        refcheck_if,
        class_static=False,
        class_constructor=False,
        extras=None,
        struct_cursor=None,
        duplicate_type=None,
        generated=None,
        debug=None,
        ):
    '''
    Writes a function or class method that calls <fnname>.

    Also appends python and C# code to generated.swig_python and
    generated.swig_csharp if <generated> is not None.

        tu
            .
        register_fn_use
            Callback to keep track of what fz_*() fns have been used.
        fnname
            Name of fz_*() fn to wrap, e.g. fz_concat.
        out_h
        out_cpp
            Where to write generated code.
        struct_name
            If false, we generate class-aware wrapping function. Otherwise name
            of struct such as 'fz_rect' and we create a method in the struct's
            wrapper class.
        class_name
            Ignored if struct_name is false.

            Name of wrapper class, e.g. 'Rect'.
        class_static
            Ignored if struct_name is false.

            If true, we generate a static method.

            Otherwise we generate a normal class method, where first arg that
            is type <struct_name> is omitted from the generated method's
            prototype; in the implementation we use <this>.
        class_constructor
            If true, we write a constructor.
        extras
            None or ClassExtras instance.
            Only used if <constructor> is true.
        struct_cursor
            None or cursor for the struct definition.
            Only used if <constructor> is true.
        duplicate_type:
            If true, we have already generated a method with the same args, so
            this generated method will be commented-out.
        generated:
            If not None and there are one or more out-params, we write
            python code to generated.swig_python that overrides the default
            SWIG-generated method to call our *_outparams_fn() alternative.
        debug
            Show extra diagnostics.
    '''
    verbose = state.state_.show_details( fnname)
    if fn_cursor and fn_cursor.type.is_function_variadic() and fnname != 'fz_warn':
        jlib.log( 'Not writing class-aware wrapper because variadic: {fnname=}', 1)
        return
    if verbose:
        jlib.log( 'Writing class-aware wrapper for {fnname=}')
    if struct_name:
        assert fnname not in state.omit_methods, jlib.log_text( '{=fnname}')
    if debug:
        jlib.log( '{class_name=} {fnname=}')
    assert fnname.startswith( ('fz_', 'pdf_'))
    if not fn_cursor:
        fn_cursor = state.state_.find_function( tu, fnname, method=True)
    if not fn_cursor:
        jlib.log( '*** ignoring {fnname=}')
        return

    if fnname.endswith('_drop'):
        # E.g. fz_concat_push_drop() is not safe (or necessary) for us because
        # we need to manage reference counts ourselves.
        #jlib.log('Ignoring because ends with "_drop": {fnname}')
        return

    if struct_name:
        methodname = rename.method( struct_name, fnname)
    else:
        methodname = rename.fn( fnname)

    if verbose:
        jlib.log( 'Writing class-aware wrapper for {fnname=}')
    # Construct prototype fnname(args).
    #
    if class_constructor:
        assert struct_name
        decl_h = f'{class_name}('
        decl_cpp = f'{class_name}('
    else:
        decl_h = f'{methodname}('
        decl_cpp = f'{methodname}('
    have_used_this = False
    num_out_params = 0
    num_class_wrapper_params = 0
    comma = ''
    this_is_const = False
    debug = state.state_.show_details( fnname)

    for arg in parse.get_args( tu, fn_cursor):
        if debug:
            jlib.log( 'Looking at {struct_name=} {fnname=} {fnname_wrapper} {arg=}', 1)
        decl_h += comma
        decl_cpp += comma
        if arg.out_param:
            num_out_params += 1
        if arg.alt:
            # This parameter is a pointer to a struct that we wrap.
            num_class_wrapper_params += 1
            arg_extras = classes.classextras.get( tu, arg.alt.type.spelling)
            assert arg_extras, jlib.log_text( '{=structname fnname arg.alt.type.spelling}')
            const = ''
            if not arg.out_param and (not arg_extras.pod or arg.cursor.type.kind != state.clang.cindex.TypeKind.POINTER):
                const = 'const '

            if (1
                    and struct_name
                    and not class_static
                    and not class_constructor
                    and rename.class_(util.clip( arg.alt.type.spelling, 'struct ')) == class_name
                    and not have_used_this
                    ):
                assert not arg.out_param
                # Omit this arg from the method's prototype - we'll use <this>
                # when calling the underlying fz_ function.
                have_used_this = True
                if not arg_extras.pod:
                    this_is_const = const
                continue

            if arg_extras.pod == 'none':
                jlib.log( 'Not wrapping because {arg=} wrapper has {extras.pod=}', 1)
                return
            text = f'{const}{rename.class_(arg.alt.type.spelling)}& {arg.name}'
            decl_h += text
            decl_cpp += text
        else:
            jlib.logx( '{arg.spelling=}')
            decl_text = declaration_text( arg.cursor.type, arg.name)
            decl_h += decl_text
            decl_cpp += decl_text
        comma = ', '

    if fn_cursor.type.is_function_variadic():
        decl_h += f'{comma}...'
        decl_cpp += f'{comma}...'

    decl_h += ')'
    decl_cpp += ')'
    if this_is_const:
        decl_h += ' const'
        decl_cpp += ' const'

    if verbose:
        jlib.log( '{=struct_name class_constructor}')
    if class_constructor:
        comment = f'Constructor using `{fnname}()`.'
    else:
        comment = make_wrapper_comment(
                tu,
                fn_cursor,
                fnname,
                methodname,
                indent='    ',
                is_method=bool(struct_name),
                is_low_level=False,
                )

    if struct_name and not class_static and not class_constructor:
        assert have_used_this, f'error: wrapper for {struct_name}: {fnname}() is not useful - does not have a {struct_name} arg.'

    if struct_name and not duplicate_type:
        register_fn_use( fnname)

    # If this is true, we explicitly construct a temporary from what the
    # wrapped function returns.
    #
    wrap_return = None

    warning_not_copyable = False
    warning_no_raw_constructor = False

    # Figure out return type for our generated function/method.
    #
    if verbose:
        jlib.log( 'Looking at return type...')
    return_cursor = None
    return_type = None
    return_extras = None
    if class_constructor:
        assert struct_name
        fn_h = f'{decl_h}'
        fn_cpp = f'{class_name}::{decl_cpp}'
    else:
        fn_h = declaration_text( fn_cursor.result_type, decl_h)
        if verbose:
            jlib.log( '{fn_cursor.result_type=}')
        if struct_name:
            fn_cpp = declaration_text( fn_cursor.result_type, f'{class_name}::{decl_cpp}')
        else:
            fn_cpp = declaration_text( fn_cursor.result_type, f'{decl_cpp}')

        # See whether we can convert return type to an instance of a wrapper
        # class.
        #
        if verbose:
            jlib.log( '{fn_cursor.result_type.kind=}')
        if fn_cursor.result_type.kind == state.clang.cindex.TypeKind.POINTER:
            # Function returns a pointer.
            t = state.get_name_canonical( fn_cursor.result_type.get_pointee())
            if verbose:
                jlib.log( '{t.spelling=}')
            return_cursor = parse.find_struct( tu, t.spelling, require_definition=False)
            if verbose:
                jlib.log( '{=t.spelling return_cursor}')
            if return_cursor:
                # Function returns a pointer to a struct.
                return_extras = classes.classextras.get( tu, return_cursor.spelling)
                if return_extras:
                    # Function returns a pointer to a struct for which we
                    # generate a class wrapper, so change return type to be an
                    # instance of the class wrapper.
                    return_type = rename.class_(return_cursor.spelling)
                    if verbose:
                        jlib.log( '{=return_type}')
                    if 0 and (state.state_.show_details(return_cursor.type.spelling) or state.state_.show_details(struct_name)):
                        jlib.log('{return_cursor.type.spelling=}'
                                ' {return_cursor.spelling=}'
                                ' {struct_name=} {return_extras.copyable=}'
                                ' {return_extras.constructor_raw=}'
                                )
                    fn_h = f'{return_type} {decl_h}'
                    if struct_name:
                        fn_cpp = f'{return_type} {class_name}::{decl_cpp}'
                    else:
                        fn_cpp = f'{return_type} {decl_cpp}'
                    if t.is_const_qualified():
                        wrap_return = 'const pointer'
                    else:
                        wrap_return = 'pointer'
            else:
                return_pointee = fn_cursor.result_type.get_pointee()
                if 'char' in return_pointee.spelling:
                    if function_name_implies_kept_references(fnname):
                        # For now we just output a diagnostic, but eventually
                        # we might make C++ wrappers return a std::string here,
                        # free()-ing the char* before returning.
                        jlib.log( 'Function name implies kept reference and returns char*:'
                                ' {fnname}(): {fn_cursor.result_type.spelling=}'
                                ' -> {return_pointee.spelling=}.'
                                )

            if verbose:
                jlib.log( '{=warning_not_copyable warning_no_raw_constructor}')
        else:
            # The fz_*() function returns by value. See whether we can convert
            # its return type to an instance of a wrapper class.
            #
            # If so, we will use constructor that takes pointer to the fz_
            # struct. C++ doesn't allow us to use address of temporary, so we
            # generate code like this:
            #
            #   fz_quad_s ret = mupdf_snap_selection(...);
            #   return Quad(&ret);
            #
            t = state.get_name_canonical( fn_cursor.result_type)

            # 2023-02-09: parse.find_struct() will actually find any definition,
            # and we now prefix Fitz headers with a typedef of size_t on Linux,
            # so we need to avoid calling parse.find_struct() unless `t` is for
            # a MuPDF type.
            #
            if t.spelling.startswith( ('fz_', 'pdf_')):
                return_cursor = parse.find_struct( tu, t.spelling)
                if return_cursor:
                    tt = state.get_name_canonical( return_cursor.type)
                    if tt.kind == state.clang.cindex.TypeKind.ENUM:
                        # For now, we return this type directly with no wrapping.
                        pass
                    else:
                        return_extras = classes.classextras.get( tu, return_cursor.spelling)
                        return_type = rename.class_(return_cursor.type.spelling)
                        fn_h = f'{return_type} {decl_h}'
                        if struct_name:
                            fn_cpp = f'{return_type} {class_name}::{decl_cpp}'
                        else:
                            fn_cpp = f'{return_type} {decl_cpp}'
                        wrap_return = 'value'

    if return_extras:
        if not return_extras.copyable:
            out_h.write(
                    textwrap.indent(
                        textwrap.dedent( f'''
                            /* Class-aware wrapper for `{fnname}()`
                            is not available because returned wrapper class for `{return_cursor.spelling}`
                            is non-copyable. */
                            '''
                            ),
                        '    ',
                        )
                    )
            if verbose:
                jlib.log( 'Not creating class-aware wrapper because returned wrapper class is non-copyable: {fnname=}.')
            return
        if not return_extras.constructor_raw:
            out_h.write(
                    textwrap.indent(
                        textwrap.dedent( f'''
                            /* Class-aware wrapper for `{fnname}()`
                            is not available because returned wrapper class for `{return_cursor.spelling}`
                            does not have raw constructor. */
                            '''
                            ),
                        '    ',
                        )
                    )
            if verbose:
                jlib.log( 'Not creating class-aware wrapper because returned wrapper class does not have raw constructor: {fnname=}.')
            return

    out_h.write( '\n')
    out_h.write( f'    /** {comment} */\n')

    # Copy any comment (indented) into class definition above method
    # declaration.
    if fn_cursor.raw_comment:
        for line in fn_cursor.raw_comment.split( '\n'):
            out_h.write( f'    {line}\n')

    if duplicate_type:
        out_h.write( f'    /* Disabled because same args as {duplicate_type}.\n')

    out_h.write( f'    FZ_FUNCTION {"static " if class_static else ""}{fn_h};\n')

    if duplicate_type:
        out_h.write( f'    */\n')

    if not struct_name:
        # Use extra spacing between non-class functions. Class methods are
        # grouped together.
        out_cpp.write( f'\n')

    out_cpp.write( f'/* {comment} */\n')
    if duplicate_type:
        out_cpp.write( f'/* Disabled because same args as {duplicate_type}.\n')

    out_cpp.write( f'FZ_FUNCTION {fn_cpp}\n')

    function_wrapper_class_aware_body(
            tu,
            fnname,
            out_cpp,
            struct_name,
            class_name,
            class_static,
            class_constructor,
            extras,
            struct_cursor,
            fn_cursor,
            return_cursor,
            wrap_return,
            refcheck_if,
            )

    if struct_name:
        if duplicate_type:
            out_cpp.write( f'*/\n')

    # fixme: the test of `struct_name` means that we don't generate outparam override for
    # class-aware fns which don't have any struct/class args, e.g. fz_lookup_cjk_font().
    #

    if generated and num_out_params:
        make_python_class_method_outparam_override(
                tu,
                fn_cursor,
                fnname,
                generated,
                struct_name,
                class_name,
                return_type,
                )


def class_custom_method(
        tu,
        register_fn_use,
        struct_cursor,
        classname,
        extramethod,
        out_h,
        out_cpp,
        refcheck_if,
        ):
    '''
    Writes custom method as specified by <extramethod>.

        tu
            .
        register_fn_use
            Callable taking single <fnname> arg.
        struct_cursor
            Cursor for definition of MuPDF struct.
        classname
            Name of wrapper class for <struct_cursor>.
        extramethod
            An ExtraMethod or ExtraConstructor instance.
        out_h
        out_cpp
            Where to write generated code.
    '''
    assert isinstance( extramethod, ( classes.ExtraMethod, classes.ExtraConstructor)), f'{type(extramethod)}'
    is_constructor = False
    is_destructor = False
    is_begin_end = False

    if extramethod.return_:
        name_args = extramethod.name_args
        return_space = f'{extramethod.return_} '
        comment = 'Custom method.'
        if name_args.startswith( 'begin(') or name_args.startswith( 'end('):
            is_begin_end = True
    elif extramethod.name_args == '~()':
        # Destructor.
        name_args = f'~{classname}{extramethod.name_args[1:]}'
        return_space = ''
        comment = 'Custom destructor.'
        is_destructor = True
    else:
        # Constructor.
        assert extramethod.name_args.startswith( '('), f'bad constructor/destructor in classname={classname}'
        name_args = f'{classname}{extramethod.name_args}'
        return_space = ''
        comment = 'Custom constructor.'
        is_constructor = True

    out_h.write( f'\n')
    if extramethod.comment:
        for i, line in enumerate( extramethod.comment.strip().split('\n')):
            line = line.replace( '/* ', '/** ')
            out_h.write( f'    {line}\n')
    else:
        out_h.write( f'    /** {comment} */\n')
    out_h.write( f'    FZ_FUNCTION {return_space}{name_args};\n')

    out_cpp.write( f'/** {comment} */\n')
    # Remove any default arg values from <name_args>.
    name_args_no_defaults = re.sub('= *[^(][^),]*', '', name_args)
    if name_args_no_defaults != name_args:
        jlib.log('have changed {name_args=} to {name_args_no_defaults=}', 1)
    out_cpp.write( f'FZ_FUNCTION {return_space}{classname}::{name_args_no_defaults}')

    body = textwrap.dedent(extramethod.body)
    if is_constructor and parse.has_refs( tu, struct_cursor.type):
        # Insert ref checking code into end of custom constructor body.
        end = body.rfind('}')
        assert end >= 0
        out_cpp.write( body[:end])
        out_cpp.write( f'    {refcheck_if}\n')
        out_cpp.write( f'    if (s_check_refs)\n')
        out_cpp.write( f'    {{\n')
        out_cpp.write( f'        s_{classname}_refs_check.add( this, __FILE__, __LINE__, __FUNCTION__);\n')
        out_cpp.write( f'    }}\n')
        out_cpp.write( f'    #endif\n')
        out_cpp.write( body[end:])
    else:
        out_cpp.write( body)

    out_cpp.write( f'\n')

    if 1:   # lgtm [py/constant-conditional-expression]
        # Register calls of all fz_* functions. Not necessarily helpful - we
        # might only be interested in calls of fz_* functions that are directly
        # available to uses of class.
        #
        for fnname in re.findall( '(mupdf::[a-zA-Z0-9_]+) *[(]', extramethod.body):
            fnname = util.clip( fnname, 'mupdf::')
            if not fnname.startswith( 'pdf_'):
                fnname = 'fz_' + fnname
            #log( 'registering use of {fnname} in extramethod {classname}::{name_args}')
            register_fn_use( fnname)

    return is_constructor, is_destructor, is_begin_end


def class_raw_constructor(
        tu,
        register_fn_use,
        classname,
        struct_cursor,
        struct_name,
        base_name,
        extras,
        constructor_fns,
        out_h,
        out_cpp,
        refcheck_if,
        ):
    '''
    Create a raw constructor - a constructor taking a pointer to underlying
    struct. This raw constructor assumes that it already owns the pointer so it
    does not call fz_keep_*(); the class's destructor will call fz_drop_*().
    '''
    #jlib.log( 'Creating raw constructor {classname=} {struct_name=} {extras.pod=} {extras.constructor_raw=} {fnname=}')
    comment = f'/** Constructor using raw copy of pre-existing `::{struct_name}`. */'
    if extras.pod:
        constructor_decl = f'{classname}(const ::{struct_name}* internal)'
    else:
        constructor_decl = f'{classname}(::{struct_name}* internal)'
    out_h.write( '\n')
    out_h.write( f'    {comment}\n')
    if extras.constructor_raw == 'default':
        out_h.write( f'    FZ_FUNCTION {classname}(::{struct_name}* internal=NULL);\n')
    else:
        out_h.write( f'    FZ_FUNCTION {constructor_decl};\n')

    if extras.constructor_raw != 'declaration_only':
        out_cpp.write( f'FZ_FUNCTION {classname}::{constructor_decl}\n')
        if extras.pod == 'inline':
            pass
        elif extras.pod:
            out_cpp.write( ': m_internal(*internal)\n')
        else:
            out_cpp.write( ': m_internal(internal)\n')
        out_cpp.write( '{\n')
        if extras.pod == 'inline':
            assert struct_cursor, f'cannot form raw constructor for inline pod {classname} without cursor for underlying {struct_name}'
            out_cpp.write( f'    assert( internal);\n')
            for c in parse.get_members(struct_cursor):
                if c.type.kind == state.clang.cindex.TypeKind.CONSTANTARRAY:
                    out_cpp.write( f'    memcpy(this->{c.spelling}, internal->{c.spelling}, sizeof(this->{c.spelling}));\n')
                else:
                    out_cpp.write( f'    this->{c.spelling} = internal->{c.spelling};\n')
        if parse.has_refs( tu, struct_cursor.type):
            out_cpp.write( f'    {refcheck_if}\n')
            out_cpp.write( f'    if (s_check_refs)\n')
            out_cpp.write( f'    {{\n')
            out_cpp.write( f'        s_{classname}_refs_check.add( this, __FILE__, __LINE__, __FUNCTION__);\n')
            out_cpp.write( f'    }}\n')
            out_cpp.write( f'    #endif\n')
        out_cpp.write( '}\n')
        out_cpp.write( '\n')

    if extras.pod == 'inline':
        # Write second constructor that takes underlying struct by value.
        #
        assert not parse.has_refs( tu, struct_cursor.type)
        constructor_decl = f'{classname}(const ::{struct_name} internal)'
        out_h.write( '\n')
        out_h.write( f'    {comment}\n')
        out_h.write( f'    FZ_FUNCTION {constructor_decl};\n')

        if extras.constructor_raw != 'declaration_only':
            out_cpp.write( f'FZ_FUNCTION {classname}::{constructor_decl}\n')
            out_cpp.write( '{\n')
            for c in parse.get_members(struct_cursor):
                if c.type.kind == state.clang.cindex.TypeKind.CONSTANTARRAY:
                    out_cpp.write( f'    memcpy(this->{c.spelling}, &internal.{c.spelling}, sizeof(this->{c.spelling}));\n')
                else:
                    out_cpp.write( f'    this->{c.spelling} = internal.{c.spelling};\n')
            out_cpp.write( '}\n')
            out_cpp.write( '\n')

            # Write accessor for inline state.state_.
            #
            for const in False, True:
                space_const = ' const' if const else ''
                const_space = 'const ' if const else ''
                out_h.write( '\n')
                out_h.write( f'    /** Access as underlying struct. */\n')
                out_h.write( f'    FZ_FUNCTION {const_space}::{struct_name}* internal(){space_const};\n')
                out_cpp.write( f'{comment}\n')
                out_cpp.write( f'FZ_FUNCTION {const_space}::{struct_name}* {classname}::internal(){space_const}\n')
                out_cpp.write( '{\n')
                field0 = parse.get_field0(struct_cursor.canonical).spelling
                out_cpp.write( f'    auto ret = ({const_space}::{struct_name}*) &this->{field0};\n')
                if parse.has_refs( tu, struct_cursor.type):
                    out_cpp.write( f'    {refcheck_if}\n')
                    out_cpp.write( f'    if (s_check_refs)\n')
                    out_cpp.write( f'    {{\n')
                    out_cpp.write( f'        s_{classname}_refs_check.add( this, __FILE__, __LINE__, __FUNCTION__);\n')
                    out_cpp.write( f'    }}\n')
                    out_cpp.write( f'    #endif\n')
                out_cpp.write( '    return ret;\n')
                out_cpp.write( '}\n')
                out_cpp.write( '\n')



def class_accessors(
        tu,
        register_fn_use,
        classname,
        struct_cursor,
        struct_name,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes accessor functions for member data.
    '''
    if not extras.pod:
        jlib.logx( 'creating accessor for non-pod class {classname=} wrapping {struct_name}')

    n = 0

    for cursor in parse.get_members(struct_cursor):
        n += 1
        #jlib.log( 'accessors: {cursor.spelling=} {cursor.type.spelling=}')

        # We set this to fz_keep_<type>() function to call, if we return a
        # wrapper class constructed from raw pointer to underlying fz_* struct.
        keep_function = None

        # Set <decl> to be prototype with %s where the name is, e.g. 'int
        # %s()'; later on we use python's % operator to replace the '%s'
        # with the name we want.
        #
        if cursor.type.kind == state.clang.cindex.TypeKind.POINTER:
            decl = 'const ' + declaration_text( cursor.type, '%s()')
            pointee_type = state.get_name_canonical( cursor.type.get_pointee()).spelling
            pointee_type = util.clip( pointee_type, 'const ')
            pointee_type = util.clip( pointee_type, 'struct ')
            #if 'fz_' in pointee_type:
            #    jlib.log( '{pointee_type=}')
            # We don't attempt to make accessors to function pointers.
            if state.get_name_canonical( cursor.type.get_pointee()).kind == state.clang.cindex.TypeKind.FUNCTIONPROTO:
                jlib.logx( 'ignoring {cursor.spelling=} because pointer to FUNCTIONPROTO')
                continue
            elif pointee_type.startswith( ('fz_', 'pdf_')):
                extras2 = parse.get_fz_extras( tu, pointee_type)
                if extras2:
                    # Make this accessor return an instance of the wrapping
                    # class by value.
                    #
                    classname2 = rename.class_( pointee_type)
                    decl = f'{classname2} %s()'

                    # If there's a fz_keep_() function, we must call it on the
                    # internal data before returning the wrapper class.
                    pointee_type_base = util.clip( pointee_type, ('fz_', 'pdf_'))
                    keep_function = f'{parse.prefix(pointee_type)}keep_{pointee_type_base}'
                    if state.state_.find_function( tu, keep_function, method=False):
                        jlib.logx( 'using {keep_function=}')
                    else:
                        jlib.log( 'cannot find {keep_function=}')
                        keep_function = None
        elif cursor.type.kind == state.clang.cindex.TypeKind.FUNCTIONPROTO:
            jlib.log( 'ignoring {cursor.spelling=} because FUNCTIONPROTO')
            continue
        else:
            if 0 and extras.pod:    # lgtm [py/unreachable-statement]
                # Return reference so caller can modify. Unfortunately SWIG
                # converts non-const references to pointers, so generated
                # python isn't useful.
                fn_args = '& %s()'
            else:
                fn_args = '%s()'
            if cursor.type.get_array_size() >= 0:
                if 0:   # lgtm [py/unreachable-statement]
                    # Return reference to the array; we need to put fn name
                    # and args inside (...) to allow the declaration syntax
                    # to work - we end up with things like:
                    #
                    #   char (& media_class())[64];
                    #
                    # Unfortunately SWIG doesn't seem to be able to cope
                    # with this.
                    decl = declaration_text( cursor.type, '(%s)' % fn_args)
                else:
                    # Return pointer to the first element of the array, so
                    # that SWIG can cope.
                    fn_args = '* %s()'
                    type_ = cursor.type.get_array_element_type()
                    decl = declaration_text( type_, fn_args)
            else:
                if ( cursor.type.kind == state.clang.cindex.TypeKind.TYPEDEF
                        and cursor.type.get_typedef_name() in ('uint8_t', 'int8_t')
                        ):
                    # Don't let accessor return uint8_t because SWIG thinks it
                    # is a char*, leading to memory errors. Instead return int.
                    #
                    jlib.logx('Changing from {cursor.type.get_typedef_name()=} {cursor.type=} to int')
                    decl = f'int {fn_args}'
                else:
                    decl = declaration_text( cursor.type, fn_args)

        # todo: if return type is uint8_t or int8_t, maybe return as <int>
        # so SWIG doesn't think it is a string? This would fix errors witht
        # fz_image::n and fz_image::bpc.
        out_h.write( f'    FZ_FUNCTION {decl % cursor.spelling};\n')
        out_cpp.write( 'FZ_FUNCTION %s\n' % (decl % ( f'{classname}::{cursor.spelling}')))
        out_cpp.write( '{\n')
        if keep_function:
            out_cpp.write( f'    {rename.ll_fn(keep_function)}(m_internal->{cursor.spelling});\n')
        if extras.pod:
            out_cpp.write( f'    return m_internal.{cursor.spelling};\n')
        else:
            out_cpp.write( f'    return m_internal->{cursor.spelling};\n')
        out_cpp.write( '}\n')
        out_cpp.write( '\n')
    assert n, f'No fields found for {struct_cursor.spelling}.'



def class_destructor(
        tu,
        register_fn_use,
        classname,
        extras,
        struct_cursor,
        destructor_fns,
        out_h,
        out_cpp,
        refcheck_if,
        ):
    if len(destructor_fns) > 1:
        # Use function with shortest name.
        if 0:   # lgtm [py/unreachable-statement]
            jlib.log( 'Multiple possible destructor fns for {classname=}')
            for fnname, cursor in destructor_fns:
                jlib.log( '    {fnname=} {cursor.spelling=}')
        shortest = None
        for i in destructor_fns:
            if shortest is None or len(i[0]) < len(shortest[0]):
                shortest = i
        #jlib.log( 'Using: {shortest[0]=}')
        destructor_fns = [shortest]
    if len(destructor_fns):
        fnname, cursor = destructor_fns[0]
        register_fn_use( cursor.spelling)
        out_h.write( f'    /** Destructor using {cursor.spelling}(). */\n')
        out_h.write( f'    FZ_FUNCTION ~{classname}();\n');

        out_cpp.write( f'FZ_FUNCTION {classname}::~{classname}()\n')
        out_cpp.write(  '{\n')
        out_cpp.write( f'    {rename.ll_fn(fnname)}(m_internal);\n')
        if parse.has_refs( tu, struct_cursor.type):
            out_cpp.write( f'    {refcheck_if}\n')
            out_cpp.write( f'    if (s_check_refs)\n')
            out_cpp.write(  '    {\n')
            out_cpp.write( f'        s_{classname}_refs_check.remove( this, __FILE__, __LINE__, __FUNCTION__);\n')
            out_cpp.write(  '    }\n')
            out_cpp.write( f'    #endif\n')
        out_cpp.write(  '}\n')
        out_cpp.write( '\n')
    else:
        out_h.write( '    /** We use default destructor. */\n')


def pod_class_members(
        tu,
        classname,
        struct_cursor,
        struct_name,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes code for wrapper class's to_string() member function.
    '''
    out_h.write( f'\n')
    out_h.write( f'    /** Returns string containing our members, labelled and inside (...), using operator<<. */\n')
    out_h.write( f'    FZ_FUNCTION std::string to_string();\n')

    out_h.write( f'\n')
    out_h.write( f'    /** Comparison method. */\n')
    out_h.write( f'    FZ_FUNCTION bool operator==(const {classname}& rhs);\n')

    out_h.write( f'\n')
    out_h.write( f'    /** Comparison method. */\n')
    out_h.write( f'    FZ_FUNCTION bool operator!=(const {classname}& rhs);\n')

    out_cpp.write( f'FZ_FUNCTION std::string {classname}::to_string()\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    std::ostringstream buffer;\n')
    out_cpp.write( f'    buffer << *this;\n')
    out_cpp.write( f'    return buffer.str();\n')
    out_cpp.write( f'}}\n')
    out_cpp.write( f'\n')

    out_cpp.write( f'FZ_FUNCTION bool {classname}::operator==(const {classname}& rhs)\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    return ::operator==( *this, rhs);\n')
    out_cpp.write( f'}}\n')
    out_cpp.write( f'\n')

    out_cpp.write( f'FZ_FUNCTION bool {classname}::operator!=(const {classname}& rhs)\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    return ::operator!=( *this, rhs);\n')
    out_cpp.write( f'}}\n')
    out_cpp.write( f'\n')


def struct_to_string_fns(
        tu,
        struct_cursor,
        struct_name,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes functions for text representation of struct/wrapper-class members.
    '''
    out_h.write( f'\n')
    out_h.write( f'/** Returns string containing a {struct_name}\'s members, labelled and inside (...), using operator<<. */\n')
    out_h.write( f'FZ_FUNCTION std::string to_string_{struct_name}(const ::{struct_name}& s);\n')

    out_h.write( f'\n')
    out_h.write( f'/** Returns string containing a {struct_name}\'s members, labelled and inside (...), using operator<<.\n')
    out_h.write( f'(Convenience overload). */\n')
    out_h.write( f'FZ_FUNCTION std::string to_string(const ::{struct_name}& s);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::string to_string_{struct_name}(const ::{struct_name}& s)\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    std::ostringstream buffer;\n')
    out_cpp.write( f'    buffer << s;\n')
    out_cpp.write( f'    return buffer.str();\n')
    out_cpp.write( f'}}\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::string to_string(const ::{struct_name}& s)\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    return to_string_{struct_name}(s);\n')
    out_cpp.write( f'}}\n')


def pod_struct_fns(
        tu,
        namespace,
        struct_cursor,
        struct_name,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes extra fns for POD structs - operator<<(), operator==(), operator!=().
    '''
    # Write operator<< functions for streaming text representation of C struct
    # members. We should be at top-level in out_h and out_cpp, i.e. not inside
    # 'namespace mupdf {...}'.
    out_h.write( f'\n')
    out_h.write( f'/** {struct_name}: writes members, labelled and inside (...), to a stream. */\n')
    out_h.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const ::{struct_name}& rhs);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const ::{struct_name}& rhs)\n')
    out_cpp.write( f'{{\n')
    i = 0
    out_cpp.write( f'    out\n')
    out_cpp.write( f'            << "("\n');
    for cursor in parse.get_members(struct_cursor):
        out_cpp.write( f'            << ');
        if i:
            out_cpp.write( f'" {cursor.spelling}="')
        else:
            out_cpp.write( f' "{cursor.spelling}="')
        out_cpp.write( f' << rhs.{cursor.spelling}\n')
        i += 1
    out_cpp.write( f'            << ")"\n');
    out_cpp.write( f'            ;\n')
    out_cpp.write( f'    return out;\n')
    out_cpp.write( f'}}\n')

    # Write comparison fns.
    out_h.write( f'\n')
    out_h.write( f'/** {struct_name}: comparison function. */\n')
    out_h.write( f'FZ_FUNCTION bool operator==( const ::{struct_name}& lhs, const ::{struct_name}& rhs);\n')
    out_h.write( f'\n')
    out_h.write( f'/** {struct_name}: comparison function. */\n')
    out_h.write( f'FZ_FUNCTION bool operator!=( const ::{struct_name}& lhs, const ::{struct_name}& rhs);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION bool operator==( const ::{struct_name}& lhs, const ::{struct_name}& rhs)\n')
    out_cpp.write( f'{{\n')
    for cursor in parse.get_members(struct_cursor):
        out_cpp.write( f'    if (lhs.{cursor.spelling} != rhs.{cursor.spelling}) return false;\n')
    out_cpp.write( f'    return true;\n')
    out_cpp.write( f'}}\n')
    out_cpp.write( f'FZ_FUNCTION bool operator!=( const ::{struct_name}& lhs, const ::{struct_name}& rhs)\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    return !(lhs == rhs);\n')
    out_cpp.write( f'}}\n')


def pod_class_fns(
        tu,
        classname,
        struct_cursor,
        struct_name,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes extra fns for wrappers for POD structs - operator<<(), operator==(),
    operator!=().
    '''
    # Write functions for text representation of wrapper-class members. These
    # functions make use of the corresponding struct functions created by
    # struct_to_string_fns().
    #
    assert extras.pod != 'none'
    classname = f'mupdf::{classname}'
    out_h.write( f'\n')
    out_h.write( f'/** {classname}: writes underlying {struct_name}\'s members, labelled and inside (...), to a stream. */\n')
    out_h.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const {classname}& rhs);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const {classname}& rhs)\n')
    out_cpp.write( f'{{\n')
    if extras.pod == 'inline':
        out_cpp.write( f'    return out << *rhs.internal();\n')
    elif extras.pod:
        out_cpp.write( f'    return out << rhs.m_internal;\n')
    else:
        out_cpp.write( f'    return out << " " << *rhs.m_internal;\n')
    out_cpp.write( f'}}\n')

    # Write comparison fns, using comparison of underlying MuPDF struct.
    out_h.write( f'\n')
    out_h.write( f'/** {classname}: comparison function. */\n')
    out_h.write( f'FZ_FUNCTION bool operator==( const {classname}& lhs, const {classname}& rhs);\n')
    out_h.write( f'\n')
    out_h.write( f'/** {classname}: comparison function. */\n')
    out_h.write( f'FZ_FUNCTION bool operator!=( const {classname}& lhs, const {classname}& rhs);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION bool operator==( const {classname}& lhs, const {classname}& rhs)\n')
    out_cpp.write( f'{{\n')
    if extras.pod == 'inline':
        out_cpp.write( f'    return *lhs.internal() == *rhs.internal();\n')
    else:
        out_cpp.write( f'    return lhs.m_internal == rhs.m_internal;\n')
    out_cpp.write( f'}}\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION bool operator!=( const {classname}& lhs, const {classname}& rhs)\n')
    out_cpp.write( f'{{\n')
    if extras.pod == 'inline':
        out_cpp.write( f'    return *lhs.internal() != *rhs.internal();\n')
    else:
        out_cpp.write( f'    return lhs.m_internal != rhs.m_internal;\n')
    out_cpp.write( f'}}\n')


def get_struct_fnptrs( cursor_struct, shallow_typedef_expansion=False, verbose=False):
    '''
    Yields (cursor, fnptr_type) for function-pointer members of struct defined
    at cusor, where <cursor> is the cursor of the member and <fntr_type> is the
    type.

    cursor_struct:
        Cursor for definition of struct; this can be a typedef.
    shallow_typedef_expansion:
        If true, the returned <fnptr_type> has any top-level typedefs resolved
        so will be a clang.cindex.TypeKind.FUNCTIONPROTO, but typedefs within
        the function args are not resolved, e.g. they can be size_t. This can
        be useful when generating code that will be compiled on different
        platforms with differing definitions of size_t.
    '''
    if verbose:
        jlib.log('Looking for fnptrs in {cursor_struct.spelling=}')
    for cursor in parse.get_members(cursor_struct):
        t = cursor.type
        if verbose:
            jlib.log('{t.kind=} {cursor.spelling=}')
        if t.kind == state.clang.cindex.TypeKind.POINTER:
            t = cursor.type.get_pointee()
            if t.kind in (state.clang.cindex.TypeKind.TYPEDEF, state.clang.cindex.TypeKind.ELABORATED):
                t_cursor = t.get_declaration()
                t = t_cursor.underlying_typedef_type
            if t.kind == state.clang.cindex.TypeKind.FUNCTIONPROTO:
                if shallow_typedef_expansion:
                    if verbose:
                        jlib.log('Not calling state.get_name_canonical() for {t.spelling=}. {cursor.spelling=}.')
                else:
                    tt = state.get_name_canonical( t)
                    if verbose:
                        jlib.log('{tt.spelling=}')
                    if (0
                            or 'struct (unnamed at ' in tt.spelling
                            or 'unnamed struct at ' in tt.spelling
                            ):

                        # This is clang giving an unhelpful name to an
                        # anonymous struct.
                        if verbose:
                            jlib.log( 'Avoiding clang anonymous struct placeholder: {tt.spelling=}')
                    else:
                        t = tt
                if verbose:
                    jlib.log('Yielding: {cursor.spelling=} {t.spelling=}')
                yield cursor, t


def class_wrapper_virtual_fnptrs(
        tu,
        struct_cursor,
        struct_name,
        classname,
        extras,
        out_h,
        out_cpp,
        out_h_end,
        generated,
        refcheck_if,
        ):
    '''
    Generate extra wrapper class for structs that contain function pointers,
    for use as a SWIG Director class so that the function pointers can be made
    to effectively point to Python or C# code.
    '''
    if not extras.virtual_fnptrs:
        return
    verbose = state.state_.show_details( struct_name)
    generated.virtual_fnptrs.append( f'{classname}2')

    self_ = extras.virtual_fnptrs.pop( 'self_')
    self_n = extras.virtual_fnptrs.pop( 'self_n', 1)
    alloc = extras.virtual_fnptrs.pop( 'alloc')
    free = extras.virtual_fnptrs.pop( 'free', None)
    assert not extras.virtual_fnptrs, f'Unused items in virtual_fnptrs: {extras.virtual_fnptrs}'

    # Class definition beginning.
    #
    out_h.write( '\n')
    out_h.write( f'/** Wrapper class for struct {struct_name} with virtual fns for each fnptr; this is for use as a SWIG Director class. */\n')
    out_h.write( f'struct {classname}2 : {classname}\n')
    out_h.write(  '{\n')

    out_cpp.write( f'/* Implementation of methods for `{classname}2`, virtual_fnptrs wrapper for `{struct_name}`). */\n')
    out_cpp.write( '\n')

    def get_fnptrs( shallow_typedef_expansion=False):
        for i in get_struct_fnptrs( struct_cursor, shallow_typedef_expansion, verbose=verbose):
            yield i

    # Constructor
    #
    out_h.write( '\n')
    out_h.write( '    /** == Constructor. */\n')
    out_h.write(f'    FZ_FUNCTION {classname}2();\n')
    out_cpp.write('\n')
    out_cpp.write(f'FZ_FUNCTION {classname}2::{classname}2()\n')
    out_cpp.write( '{\n')
    alloc = [''] + alloc.split('\n')
    alloc = '\n    '.join(alloc)
    out_cpp.write(f'{alloc}\n')
    out_cpp.write(f'    {refcheck_if}\n')
    out_cpp.write(f'    if (s_trace_director)\n')
    out_cpp.write( '    {\n')
    out_cpp.write(f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": {classname}2::{classname}2(): this=" << this << "\\n";\n')
    if not extras.pod:
        out_cpp.write(f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": {classname}2::{classname}2(): m_internal=" << m_internal << "\\n";\n')
        out_cpp.write(f'        {classname}2* self = {self_("m_internal")};\n')
        out_cpp.write(f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": {classname}2::{classname}2(): self=" << self << "\\n";\n')
    out_cpp.write('    }\n')
    out_cpp.write('    #endif\n')
    out_cpp.write( '}\n')

    if free:
        # Destructor
        out_h.write( '\n')
        out_h.write( '    /** == Destructor. */\n')
        out_h.write(f'    FZ_FUNCTION ~{classname}2();\n')
        out_cpp.write('\n')
        out_cpp.write(f'FZ_FUNCTION {classname}2::~{classname}2()\n')
        out_cpp.write( '{\n')
        out_cpp.write(f'    {refcheck_if}\n')
        out_cpp.write(f'    if (s_trace_director)\n')
        out_cpp.write( '    {\n')
        out_cpp.write(f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": ~{classname}2(): this=" << this << "\\n";\n')
        if not extras.pod:
            out_cpp.write( f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": ~{classname}2(): m_internal=" << m_internal << "\\n";\n')
        out_cpp.write( '    }\n')
        out_cpp.write(f'    #endif\n')
        out_cpp.write(f'    {free}\n')
        out_cpp.write( '}\n')

    def write(text):
        out_h.write(text)
        out_cpp.write(text)

    # Define static callback for each fnptr. It's important that these
    # functions do not resolve function parameter typedefs such as size_t to
    # the underlying types such as long int, because:
    #
    #   * Our generated code can be compiled on different machines where types
    #   such as size_t can be typedef-ed differently.
    #
    #   * Elsewhere, code that we generate will assign our static callback
    #   functions to MuPDF's function pointers (which use things like size_t).
    #
    #   * These assignments will fail if the types don't match exactly.
    #
    # For example fz_output has a member:
    #   fz_output_write_fn *write;
    #
    # This typedef is:
    #   void (fz_output_write_fn)(fz_context *ctx, void *state, const void *data, size_t n);
    #
    # We generate a static function called Output2_s_write() and we will be
    # setting a fz_output's write member to point to Output2_s_write(), which
    # only works if the types match exactly.
    #
    # So we need to resolve the outer 'typedef fz_output_write_fn', but not
    # the inner 'size_t' typedef for the <n> arg. This is slightly tricky with
    # clang-python - it provide a Type.get_canonical() method that resolves all
    # typedefs, but to resolve just one level of typedefs requires a bit more
    # work. See get_struct_fnptrs() for details.
    #
    # [Usually our generated code deliberately resolves typedefs such as size_t
    # to long int etc, because SWIG-generated code for size_t etc does not
    # always work properly due to SWIG having its own definitions of things
    # like size_t in Python/C#. But in this case the generated static function
    # is not seen by SWIG so it's ok to make it use size_t etc.]
    #
    for cursor, fnptr_type in get_fnptrs( shallow_typedef_expansion=True):

        # Write static callback.
        out_cpp.write(f'/* Static callback, calls self->{cursor.spelling}(). */\n')
        out_cpp.write(f'static {_make_top_level(fnptr_type.get_result().spelling)} {classname}2_s_{cursor.spelling}')
        out_cpp.write('(')
        sep = ''
        for i, arg_type in enumerate( fnptr_type.argument_types()):
            name = f'arg_{i}'
            out_cpp.write(sep)
            out_cpp.write( declaration_text( arg_type, name, expand_typedef=False))
            sep = ', '
        out_cpp.write(')')
        out_cpp.write('\n')
        out_cpp.write('{\n')
        self_expression = self_( f'arg_{self_n}')
        out_cpp.write(f'    {classname}2* self = {self_expression};\n')
        out_cpp.write(f'    {refcheck_if}\n')
        out_cpp.write(f'    if (s_trace_director)\n')
        out_cpp.write( '    {\n')
        out_cpp.write(f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": {classname}2_s_{cursor.spelling}(): arg_0=" << arg_0 << " arg_1=" << arg_1 << " self=" << self << "\\n";\n')
        out_cpp.write( '    }\n')
        out_cpp.write( '    #endif\n')
        out_cpp.write( '    try\n')
        out_cpp.write( '    {\n')
        out_cpp.write(f'        return self->{cursor.spelling}(')
        sep = ''
        for i, arg_type in enumerate( fnptr_type.argument_types()):
            if i == self_n:
                # This is the void* from which we found `self` so ignore
                # here. Note that we still pass the fz_context to the virtual
                # fn.
                continue
            name = f'arg_{i}'
            out_cpp.write( f'{sep}{name}')
            sep = ', '
        out_cpp.write(');\n')
        out_cpp.write('    }\n')

        # todo: catch our different exception types and map to FZ_ERROR_*.
        out_cpp.write( '    catch (std::exception& e)\n')
        out_cpp.write( '    {\n')
        out_cpp.write(f'        {refcheck_if}\n')
        out_cpp.write( '        if (s_trace_director)\n')
        out_cpp.write( '        {\n')
        out_cpp.write(f'            std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": {classname}2_s_{cursor.spelling}(): converting std::exception to fz_throw(): " << e.what() << "\\n";\n')
        out_cpp.write( '        }\n')
        out_cpp.write( '        #endif\n')
        out_cpp.write( '        fz_throw(arg_0, FZ_ERROR_GENERIC, "%s", e.what());\n')
        out_cpp.write( '    }\n')
        out_cpp.write('}\n')

    # Define use_virtual_<name>( bool use) method for each fnptr.
    #
    out_h.write(f'\n')
    # Using a Doxygen-style `/**` comment prefix here can break swig with
    # `Error: Syntax error in input(3).` if there are no following method
    # declarations.
    out_h.write(f'    /** These methods set the function pointers in *m_internal\n')
    out_h.write(f'    to point to internal callbacks that call our virtual methods. */\n')
    for cursor, fnptr_type in get_fnptrs():
        out_h.write(f'    FZ_FUNCTION void use_virtual_{cursor.spelling}( bool use=true);\n')
        out_cpp.write(f'FZ_FUNCTION void {classname}2::use_virtual_{cursor.spelling}( bool use)\n')
        out_cpp.write( '{\n')

        out_cpp.write(f'    {refcheck_if}\n')
        out_cpp.write(f'    if (s_trace_director)\n')
        out_cpp.write( '    {\n')
        out_cpp.write(f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": {classname}2::use_virtual_{cursor.spelling}(): this=" << this << " use=" << use << "\\n";\n')
        out_cpp.write( '    }\n')
        out_cpp.write( '    #endif\n')

        if extras.pod == 'inline':
            # Fnptr (in {classname}2) and virtual function (in {classname})
            # have same name, so we need qualify the fnptr with {classname} to
            # ensure we distinguish between the two.
            out_cpp.write(f'    {classname}::{cursor.spelling} = (use) ? {classname}2_s_{cursor.spelling} : nullptr;\n')
        elif extras.pod:
            out_cpp.write(f'    m_internal.{cursor.spelling} = (use) ? {classname}2_s_{cursor.spelling} : nullptr;\n')
        else:
            out_cpp.write(f'    m_internal->{cursor.spelling} = (use) ? {classname}2_s_{cursor.spelling} : nullptr;\n')
        out_cpp.write( '}\n')

    # Write virtual fn default implementations.
    #
    out_h.write(f'\n')

    # Using a Doxygen-style `/**` comment prefix here can break swig with
    # `Error: Syntax error in input(3).` if there are no following method
    # declarations.
    out_h.write(f'    /** Default virtual method implementations; these all throw an exception. */\n')
    for cursor, fnptr_type in get_fnptrs():

        out_h.write(f'    FZ_FUNCTION virtual {_make_top_level(fnptr_type.get_result().spelling)} {cursor.spelling}(')
        out_cpp.write(f'/* Default implementation of virtual method. */\n')
        out_cpp.write(f'FZ_FUNCTION {_make_top_level(fnptr_type.get_result().spelling)} {classname}2::{cursor.spelling}(')
        sep = ''
        for i, arg_type in enumerate( fnptr_type.argument_types()):
            if i == self_n:
                # This is the void* from which we found `self` so ignore
                # here. Note that we still pass the fz_context to the virtual
                # fn.
                continue
            name = f'arg_{i}'
            write(f'{sep}')
            decl_text = declaration_text(arg_type, name, verbose=0)
            write(decl_text)
            sep = ', '
        out_h.write( ');\n')
        out_cpp.write( ')\n')
        out_cpp.write( '{\n')
        out_cpp.write(f'    std::cerr << "Unexpected call of unimplemented virtual_fnptrs fn {classname}2::{cursor.spelling}(): this=" << this << ".\\n";\n')
        out_cpp.write(f'    throw std::runtime_error( "Unexpected call of unimplemented virtual_fnptrs fn {classname}2::{cursor.spelling}()");\n')
        out_cpp.write( '}\n')

    out_h.write(  '};\n')


def class_wrapper(
        tu,
        register_fn_use,
        struct_cursor,
        struct_name,
        classname,
        extras,
        out_h,
        out_cpp,
        out_h_end,
        out_cpp2,
        out_h2,
        generated,
        refcheck_if,
        ):
    '''
    Creates source for a class called <classname> that wraps <struct_name>,
    with methods that call selected fz_*() functions. Writes to out_h and
    out_cpp.

    Created source is just the per-class code, e.g. it does not contain
    #include's.

    Args:
        tu:
            Clang translation unit.
        struct_cursor:
            Cursor for struct to wrap.
        struct_name:
            Name of struct to wrap.
        classname:
            Name of wrapper class to create.
        out_h:
            Stream to which we write class definition.
        out_cpp:
            Stream to which we write method implementations.
        out_h_end:
            Stream for text that should be put at the end of the generated
            header text.
        generated:
            We write extra python and C# code to generated.out_swig_python and
            generated.out_swig_csharp for use in the swig .i file.

    Returns (is_container, has_to_string). <is_container> is true if generated
    class has custom begin() and end() methods; <has_to_string> is true if we
    have created a to_string() method.
    '''
    assert extras, f'extras is None for {struct_name}'
    if extras.iterator_next:
        class_add_iterator( tu, struct_cursor, struct_name, classname, extras, refcheck_if)

    if extras.class_pre:
        out_h.write( textwrap.dedent( extras.class_pre))

    base_name = util.clip( struct_name, ('fz_', 'pdf_'))

    constructor_fns = class_find_constructor_fns( tu, classname, struct_name, base_name, extras)
    for fnname in extras.constructors_wrappers:
        cursor = state.state_.find_function( tu, fnname, method=True)
        assert cursor, f'No cursor for constructor wrapper fnname={fnname}'
        constructor_fns.append( (fnname, cursor, None))

    destructor_fns = class_find_destructor_fns( tu, struct_name, base_name)

    # Class definition beginning.
    #
    out_h.write( '\n')
    if extras.copyable:
        out_h.write( f'/** Wrapper class for struct `{struct_name}`. */\n')
    else:
        out_h.write( f'/** Wrapper class for struct `{struct_name}`. Not copyable or assignable. */\n')
    if struct_cursor.raw_comment:
        out_h.write( f'{struct_cursor.raw_comment}')
        if not struct_cursor.raw_comment.endswith( '\n'):
            out_h.write( '\n')
    out_h.write( f'struct {classname}\n{{')

    out_cpp.write( '\n')
    out_cpp.write( f'/* Implementation of methods for {classname} (wrapper for {struct_name}). */\n')
    out_cpp.write( '\n')
    refs = parse.has_refs( tu, struct_cursor.type)
    if refs:
        refs_name, refs_size = refs
        out_cpp.write( f'{refcheck_if}\n')
        if isinstance(refs_name, int):
            # <refs_name> is offset of .refs in the struct.
            allow_int_this = ', true /*allow_int_this*/' if struct_name == 'pdf_obj' else ''
            out_cpp.write( f'static RefsCheck<::{struct_name}, {classname}{allow_int_this}> s_{classname}_refs_check({refs_name}, {refs_size});\n')
        else:
            # <refs_name> is name of .refs in the struct.
            out_cpp.write( f'static RefsCheck<::{struct_name}, {classname}> s_{classname}_refs_check(offsetof(::{struct_name}, {refs_name}), {refs_size});\n')
        out_cpp.write( f'#endif\n')
        out_cpp.write( '\n')

    # Trailing text in header, e.g. typedef for iterator.
    #
    if extras.class_top:
        # Strip leading blank line to avoid slightly odd-looking layout.
        text = util.clip( extras.class_top, '\n')
        text = textwrap.dedent( text)
        text = textwrap.indent( text, '    ')
        out_h.write( '\n')
        out_h.write( text)

    # Constructors
    #
    num_constructors = 0
    have_created_default_constructor = False

    if constructor_fns:
        out_h.write( '\n')
        out_h.write( '    /** == Constructors. */\n')
    num_constructors += len(constructor_fns)
    for fnname, cursor, duplicate_type in constructor_fns:
        # clang-6 appears not to be able to handle fn args that are themselves
        # function pointers, so for now we allow function_wrapper() to fail,
        # so we need to use temporary buffers, otherwise out_functions_h and
        # out_functions_cpp can get partial text written.
        #
        assert cursor, f'No cursor for constructor function. fnname={fnname} duplicate_type={duplicate_type}'
        temp_out_h = io.StringIO()
        temp_out_cpp = io.StringIO()
        if state.state_.show_details(fnname):
            jlib.log('Creating constructor for {=classname fnname}')
        if parse.get_first_arg( tu, cursor) == (None, 0):
            have_created_default_constructor = True
        try:
            function_wrapper_class_aware(
                    tu,
                    register_fn_use,
                    fnname,
                    temp_out_h,
                    temp_out_cpp,
                    struct_name,
                    classname,
                    cursor,
                    refcheck_if,
                    class_static=False,
                    class_constructor=True,
                    extras=extras,
                    struct_cursor=struct_cursor,
                    duplicate_type=duplicate_type,
                    )
        except Clang6FnArgsBug as e:
            jlib.log( 'Unable to wrap function {fnname} becase: {e}')
        else:
            out_h.write( temp_out_h.getvalue())
            out_cpp.write( temp_out_cpp.getvalue())

    # Custom constructors.
    #
    for extra_constructor in extras.constructors_extra:
        if extra_constructor.name_args == '()':
            have_created_default_constructor = True
        class_custom_method(
                tu,
                register_fn_use,
                struct_cursor,
                classname,
                extra_constructor,
                out_h,
                out_cpp,
                refcheck_if,
                )
        num_constructors += 1

    # Look for function that can be used by copy constructor and operator=.
    #
    if not extras.pod and extras.copyable and extras.copyable != 'default':
        class_copy_constructor(
                tu,
                register_fn_use,
                struct_name,
                struct_cursor,
                base_name,
                classname,
                constructor_fns,
                out_h,
                out_cpp,
                refcheck_if,
                )
    elif extras.copyable:
        out_h.write( '\n')
        out_h.write( '    /** We use default copy constructor and operator=. */\n')

    if extras.constructor_default:
        if have_created_default_constructor:
            if 0:
                jlib.log( 'Not creating default constructor because default custom constructor. {struct_name=}')
        elif extras.constructor_raw == 'default':
            if 0:
                jlib.log( 'Not creating default constructor because default raw constructor. {struct_name=}')
        else:
            class_constructor_default(
                    tu,
                    struct_cursor,
                    classname,
                    extras,
                    out_h,
                    out_cpp,
                    refcheck_if,
                    )
            num_constructors += 1

    # Auto-add all methods that take <struct_name> as first param, but
    # skip methods that are already wrapped in extras.method_wrappers or
    # extras.methods_extra etc.
    #
    for fnname in parse.find_wrappable_function_with_arg0_type( tu, struct_name):
        if state.state_.show_details(fnname):
            jlib.log('{struct_name=}: looking at potential method wrapping {fnname=}')
        if fnname in extras.method_wrappers:
            #log( 'auto-detected fn already in {struct_name} method_wrappers: {fnname}')
            # Omit this function, because there is an extra method with the
            # same name. (We could probably include both as they will generally
            # have different args so overloading will destinguish them, but
            # extra methods are usually defined to be used in preference.)
            pass
        elif fnname.startswith( 'fz_new_draw_device'):
            # fz_new_draw_device*() functions take first arg fz_matrix, but
            # aren't really appropriate for the fz_matrix wrapper class.
            #
            pass
        elif isinstance( fnname, list):
            assert 0
        else:
            for extramethod in extras.methods_extra:
                if not extramethod.overload:
                    if extramethod.name_args.startswith( f'{rename.method( struct_name, fnname)}('):
                        jlib.log( 'Omitting default method because same name as extramethod: {extramethod.name_args}')
                        break
            else:
                #log( 'adding to extras.method_wrappers: {fnname}')
                extras.method_wrappers.append( fnname)

    # Extra static methods.
    #
    if extras.method_wrappers_static:
        out_h.write( '\n')
        out_h.write( '    /* == Static methods. */\n')
    for fnname in extras.method_wrappers_static:
        function_wrapper_class_aware(
                tu,
                register_fn_use,
                fnname,
                out_h,
                out_cpp,
                struct_name,
                classname,
                fn_cursor=None,
                refcheck_if=refcheck_if,
                class_static=True,
                struct_cursor=struct_cursor,
                generated=generated,
                )

    # Extra methods that wrap fz_*() fns.
    #
    if extras.method_wrappers or extras.methods_extra:
        out_h.write( '\n')
        out_h.write( '    /* == Methods. */')
        out_h.write( '\n')
    extras.method_wrappers.sort()
    for fnname in extras.method_wrappers:
        function_wrapper_class_aware(
                tu,
                register_fn_use,
                fnname,
                out_h,
                out_cpp,
                struct_name,
                classname,
                None, #fn_cursor
                refcheck_if,
                struct_cursor=struct_cursor,
                generated=generated,
                debug=state.state_.show_details(fnname),
                )

    # Custom methods.
    #
    is_container = 0
    custom_destructor = False
    for extramethod in extras.methods_extra:
        is_constructor, is_destructor, is_begin_end = class_custom_method(
                tu,
                register_fn_use,
                struct_cursor,
                classname,
                extramethod,
                out_h,
                out_cpp,
                refcheck_if,
                )
        if is_constructor:
            num_constructors += 1
        if is_destructor:
            custom_destructor = True
        if is_begin_end:
            is_container += 1

    assert is_container==0 or is_container==2, f'struct_name={struct_name} is_container={is_container}'   # Should be begin()+end() or neither.
    if is_container:
        pass
        #jlib.log( 'Generated class has begin() and end(): {classname=}')

    if num_constructors == 0 or extras.constructor_raw:
        if state.state_.show_details(struct_name):
            jlib.log('calling class_raw_constructor(). {struct_name=}')
        class_raw_constructor(
                tu,
                register_fn_use,
                classname,
                struct_cursor,
                struct_name,
                base_name,
                extras,
                constructor_fns,
                out_h,
                out_cpp,
                refcheck_if,
                )

    # Accessor methods to POD data.
    #
    if extras.accessors and extras.pod == 'inline':
        jlib.log( 'ignoring {extras.accessors=} for {struct_name=} because {extras.pod=}.')
    elif extras.accessors:
        out_h.write( f'\n')
        out_h.write( f'    /* == Accessors to members of ::{struct_name} m_internal. */\n')
        out_h.write( '\n')
        class_accessors(
                tu,
                register_fn_use,
                classname,
                struct_cursor,
                struct_name,
                extras,
                out_h,
                out_cpp,
                )

    # Destructor.
    #
    if not custom_destructor:
        out_h.write( f'\n')
        class_destructor(
                tu,
                register_fn_use,
                classname,
                extras,
                struct_cursor,
                destructor_fns,
                out_h,
                out_cpp,
                refcheck_if,
                )

    # If class has '{structname}* m_internal;', provide access to m_iternal as
    # an integer, for use by python etc.
    if not extras.pod:
        class_custom_method(
                tu,
                register_fn_use,
                struct_cursor,
                classname,
                classes.ExtraMethod(
                    'long long',
                    'm_internal_value()',
                    '''
                    {
                        return (uintptr_t) m_internal;
                    }
                    ''',
                    '/** Return numerical value of .m_internal; helps with Python debugging. */',
                    ),
                out_h,
                out_cpp,
                refcheck_if,
                )
    # Class members.
    #
    out_h.write( '\n')
    out_h.write( '    /* == Member data. */\n')
    out_h.write( '\n')
    if extras.pod == 'none':
        pass
    elif extras.pod == 'inline':
        out_h.write( f'    /* These members are the same as the members of ::{struct_name}. */\n')
        for c in parse.get_members(struct_cursor):
            out_h.write( f'    {declaration_text(c.type, c.spelling)};\n')
    elif extras.pod:
        out_h.write( f'    ::{struct_cursor.spelling}  m_internal; /** Wrapped data is held by value. */\n')
    else:
        # Putting this double-asterix comment on same line as m_internal breaks
        # swig-4.02 with "Error: Syntax error in input(3).".
        out_h.write( f'    /** Pointer to wrapped data. */\n')
        out_h.write( f'    ::{struct_name}* m_internal;\n')

    # Make operator<< (std::ostream&, ...) for POD classes.
    #
    has_to_string = False
    if extras.pod and extras.pod != 'none':
        has_to_string = True
        pod_class_members(
                tu,
                classname,
                struct_cursor,
                struct_name,
                extras,
                out_h,
                out_cpp,
                )

    # Trailing text in header, e.g. typedef for iterator.
    #
    if extras.class_bottom:
        out_h.write( textwrap.indent( textwrap.dedent( extras.class_bottom), '    '))

    # Private copy constructor if not copyable.
    #
    if not extras.copyable:
        out_h.write(  '\n')
        out_h.write(  '    private:\n')
        out_h.write(  '\n')
        out_h.write(  '    /** This class is not copyable or assignable. */\n')
        out_h.write( f'    {classname}(const {classname}& rhs);\n')
        out_h.write( f'    {classname}& operator=(const {classname}& rhs);\n')

    # Class definition end.
    #
    out_h.write( '};\n')

    if extras.class_post:
        out_h_end.write( textwrap.dedent( extras.class_post))

    if extras.extra_cpp:
        out_cpp.write( f'/* .extra_cpp for {struct_name}. */\n')
        out_cpp.write( textwrap.dedent( extras.extra_cpp))

    class_wrapper_virtual_fnptrs(
            tu,
            struct_cursor,
            struct_name,
            classname,
            extras,
            out_h,
            out_cpp,
            out_h_end,
            generated,
            refcheck_if,
            )

    return is_container, has_to_string


def header_guard( name, out):
    '''
    Writes header guard for <name> to stream <out>.
    '''
    m = ''
    for c in name:
        m += c.upper() if c.isalnum() else '_'
    out.write( f'#ifndef {m}\n')
    out.write( f'#define {m}\n')
    out.write( '\n')


def tabify( filename, text):
    '''
    Returns <text> with leading multiples of 4 spaces converted to tab
    characters.
    '''
    ret = ''
    linenum = 0
    for line in text.split( '\n'):
        linenum += 1
        i = 0
        while 1:
            if i == len(line):
                break
            if line[i] != ' ':
                break
            i += 1
        if i % 4:
            if line[ int(i/4)*4:].startswith( ' *'):
                # This can happen in comments.
                pass
            else:
                jlib.log( '*** {filename}:{linenum}: {i=} {line!r=} indentation is not a multiple of 4')
        num_tabs = int(i / 4)
        ret += num_tabs * '\t' + line[ num_tabs*4:] + '\n'

    # We use [:-1] here because split() always returns extra last item '', so
    # we will have appended an extra '\n'.
    #
    return ret[:-1]


def refcount_check_code( out, refcheck_if):
    '''
    Writes reference count checking code to <out>.
    '''
    out.write( textwrap.dedent(
            f'''
            /* Support for checking that reference counts of underlying
            MuPDF structs are not smaller than the number of wrapper class
            instances. Enable at runtime by setting environmental variable
            MUPDF_check_refs to "1". */

            static const bool   s_check_refs = internal_env_flag("MUPDF_check_refs");

            /* For each MuPDF struct that has an 'int refs' member, we create
            a static instance of this class template with T set to our wrapper
            class, for example:

                static RefsCheck<fz_document, Document> s_Document_refs_check;

            Then if s_check_refs is true, each constructor function calls
            .add(), the destructor calls .remove() and other class functions
            call .check(). This ensures that we check reference counting after
            each class operation.

            If <allow_int_this> is true, we allow _this->m_internal to be
            an invalid pointer less than 4096, in which case we don't try
            to check refs. This is used for_pdf_obj because in Python the
            enums PDF_ENUM_NAME_* are converted to mupdf.PdfObj's containg
            .m_internal's which are the enum values cast to (for_pdf_obj*), so
            that they can be used directly.

            If m_size is -1, we don't attempt any checking; this is for fz_xml
            which is reference counted but does not have a simple .refs member.
            */
            {refcheck_if}
            template<typename Struct, typename ClassWrapper, bool allow_int_this=false>
            struct RefsCheck
            {{
                std::mutex              m_mutex;
                int                     m_offset;
                int                     m_size;
                std::map<Struct*, int>  m_this_to_num;

                RefsCheck(int offset, int size)
                : m_offset(offset), m_size(size)
                {{
                    assert(offset >= 0 && offset < 1000);
                    assert(m_size == 32 || m_size == 16 || m_size == 8 || m_size == -1);
                }}

                void change( const ClassWrapper* this_, const char* file, int line, const char* fn, int delta)
                {{
                    assert( s_check_refs);
                    if (m_size == -1)
                    {{
                        /* No well-defined .refs member for us to check, e.g. fz_xml. */
                        return;
                    }}
                    if (!this_->m_internal) return;
                    if (allow_int_this)
                    {{
                        #if 0   // Historic diagnostics, might still be useful.
                        std::cerr << __FILE__ << ":" << __LINE__
                                << " " << file << ":" << line << ":" << fn << ":"
                                << " this_->m_internal=" << this_->m_internal
                                << "\\n";
                        #endif
                        if ((intptr_t) this_->m_internal < 4096)
                        {{
                            #if 0   // Historic diagnostics, might still be useful.
                            std::cerr << __FILE__ << ":" << __LINE__
                                    << " " << file << ":" << line << ":" << fn << ":"
                                    << " Ignoring this_->m_internal=" << this_->m_internal
                                    << "\\n";
                            #endif
                            return;
                        }}
                    }}
                    std::lock_guard< std::mutex> lock( m_mutex);
                    /* Our lock doesn't make our access to
                    this_->m_internal->refs thead-safe - other threads
                    could be modifying it via fz_keep_<Struct>() or
                    fz_drop_<Struct>(). But hopefully our read will be atomic
                    in practise anyway? */
                    void* refs_ptr = (char*) this_->m_internal + m_offset;
                    int refs;
                    if (m_size == 32)   refs = *(int32_t*) refs_ptr;
                    if (m_size == 16)   refs = *(int16_t*) refs_ptr;
                    if (m_size ==  8)   refs = *(int8_t* ) refs_ptr;

                    int& n = m_this_to_num[ this_->m_internal];
                    int n_prev = n;
                    assert( n >= 0);
                    n += delta;
                    #if 0   // Historic diagnostics, might still be useful.
                    std::cerr << file << ":" << line << ":" << fn << "():"
                            // << " " << typeid(ClassWrapper).name() << ":"
                            << " this_=" << this_
                            << " this_->m_internal=" << this_->m_internal
                            << " refs=" << refs
                            << " n: " << n_prev << " => " << n
                            << "\\n";
                    #endif
                    if ( n < 0)
                    {{
                        #if 0   // Historic diagnostics, might still be useful.
                        std::cerr << file << ":" << line << ":" << fn << "():"
                                // << " " << typeid(ClassWrapper).name() << ":"
                                << " this_=" << this_
                                << " this_->m_internal=" << this_->m_internal
                                << " bad n: " << n_prev << " => " << n
                                << "\\n";
                        #endif
                        abort();
                    }}
                    if ( n && refs < n)
                    {{
                        #if 0   // Historic diagnostics, might still be useful.
                        std::cerr << file << ":" << line << ":" << fn << "():"
                                // << " " << typeid(ClassWrapper).name() << ":"
                                << " this_=" << this_
                                << " this_->m_internal=" << this_->m_internal
                                << " refs=" << refs
                                << " n: " << n_prev << " => " << n
                                << " refs mismatch (refs<n):"
                                << "\\n";
                        #endif
                        abort();
                    }}
                    if (n && ::abs( refs - n) > 1000)
                    {{
                        /* This traps case where n > 0 but underlying struct is
                        freed and .ref is set to bogus value by fz_free() or
                        similar. */
                        #if 0   // Historic diagnostics, might still be useful.
                        std::cerr << file << ":" << line << ":" << fn << "(): " << ": " << typeid(ClassWrapper).name()
                                << " bad change to refs."
                                << " this_=" << this_
                                << " refs=" << refs
                                << " n: " << n_prev << " => " << n
                                << "\\n";
                        #endif
                        abort();
                    }}
                    if (n == 0) m_this_to_num.erase( this_->m_internal);
                }}
                void add( const ClassWrapper* this_, const char* file, int line, const char* fn)
                {{
                    change( this_, file, line, fn, +1);
                }}
                void remove( const ClassWrapper* this_, const char* file, int line, const char* fn)
                {{
                    change( this_, file, line, fn, -1);
                }}
                void check( const ClassWrapper* this_, const char* file, int line, const char* fn)
                {{
                    change( this_, file, line, fn, 0);
                }}
            }};
            #endif

            '''
            ))

def cpp_source(
        dir_mupdf,
        namespace,
        base,
        header_git,
        generated,
        check_regress,
        clang_info_version,
        refcheck_if,
        debug,
        ):
    '''
    Generates all .h and .cpp files.

    Args:

        dir_mupdf:
            Location of mupdf checkout.
        namespace:
            C++ namespace to use.
        base:
            Directory in which all generated files are placed.
        header_git:
            If true we include git info in the file comment that is written
            into all generated files.
        generated:
            A Generated instance.
        check_regress:
            If true, we raise exception if generated content differs from what
            is in existing files.
        refcheck_if:
            `#if ... ' text for enabling reference-checking code. For example
            `#if 1` to always enable, `#ifndef NDEBUG` to only enable in debug
            builds, `#if 0` to always disable.
        debug:
            True if debug build.

    Updates <generated> and returns <tu> from clang..
    '''
    assert isinstance(generated, Generated)
    assert not dir_mupdf.endswith( '/')
    assert not base.endswith( '/')

    # Do initial setting up of generated files before parse, because we include extra.h in our parse input.

    doit = True
    if doit:
        class File:
            def __init__( self, filename, tabify=True):
                self.filename = filename
                self.tabify = tabify
                self.file = io.StringIO()
                self.line_begin = True
                self.regressions = True
                self.closed = False
            def write( self, text, fileline=False):
                # Do not allow writes after .close().
                assert not self.closed, f'File.write() called after .close(). {self.filename=}'
                if fileline:
                    # Generate #line <line> "<filename>" for our caller's
                    # location. This makes any compiler warnings refer to thei
                    # python code rather than the generated C++ code.
                    tb = traceback.extract_stack( None)
                    filename, line, function, source = tb[0]
                    if self.line_begin:
                        self.file.write( f'#line {line} "{filename}"\n')
                self.file.write( text)
                self.line_begin = text.endswith( '\n')
            def close( self):
                if self.closed:
                    # Allow multiple calls to .close().
                    return
                self.closed = True
                if self.filename:
                    # Overwrite if contents differ.
                    text = self.get()
                    if self.tabify:
                        text = tabify( self.filename, text)
                    cr = check_regress
                    jlib.log('calling util.update_file_regress() check_regress={cr}: {self.filename=}', 1)
                    e = util.update_file_regress( text, self.filename, check_regression=cr)
                    jlib.log('util.update_file_regress() returned => {e}', 1)
                    if e:
                        jlib.log('util.update_file_regress() => {e=}', 1)
                        self.regressions = True
                        jlib.log(f'File updated: {os.path.relpath(self.filename)}')
                    else:
                        jlib.log(f'File unchanged: {os.path.relpath(self.filename)}')
            def get( self):
                return self.file.getvalue()
    else:
        class File:
            def __init__( self, filename):
                pass
            def write( self, text, fileline=False):
                pass
            def close( self):
                pass

    class Outputs:
        '''
        A set of output files.

        For convenience, after outputs.add( 'foo', 'foo.c'), outputs.foo is a
        python stream that writes to 'foo.c'.
        '''
        def __init__( self):
            self.items = []

        def add( self, name, filename):
            '''
            Sets self.<name> to file opened for writing on <filename>.
            '''
            file = File( filename)
            self.items.append( (name, filename, file))
            setattr( self, name, file)

        def get( self):
            '''
            Returns list of (name, filename, file) tuples.
            '''
            return self.items

        def close( self):
            for name, filename, file in self.items:
                file.close()

    out_cpps = Outputs()
    out_hs = Outputs()
    for name in (
            'classes',
            'classes2',
            'exceptions',
            'functions',
            'internal',
            'extra',
            ):
        out_hs.add( name, f'{base}/include/mupdf/{name}.h')
        out_cpps.add( name, f'{base}/implementation/{name}.cpp')

    # Make text of header comment for all generated file.
    #
    header_text = textwrap.dedent(
            f'''
            /**
            This file was auto-generated by mupdfwrap.py.
            ''')

    if header_git:
        git_id = jlib.get_git_id( dir_mupdf, allow_none=True)
        if git_id:
            git_id = git_id.split('\n', 1)
            header_text += textwrap.dedent(
                    f'''
                    mupdf checkout:
                        {git_id[0]}'
                    ''')

    header_text += '*/\n'
    header_text += '\n'
    header_text = header_text[1:]   # Strip leading \n.
    for _, _, file in out_cpps.get() + out_hs.get():
        file.write( header_text)

    os.makedirs( f'{base}/include/mupdf', exist_ok=True)
    os.makedirs( f'{base}/implementation', exist_ok=True)

    num_regressions = 0
    # Create extra File that writes to internal buffer rather than an actual
    # file, which we will append to out_h.
    #
    out_h_classes_end = File( None)

    # Write multiple-inclusion guards into headers:
    #
    for name, filename, file in out_hs.get():
        prefix = f'{base}/include/'
        assert filename.startswith( prefix)
        name = filename[ len(prefix):]
        header_guard( name, file)

    # We need to write to out_hs.extra here before we do the parse
    # because out_hs.extra will be part of the input text passed to the
    # clang parser.
    #
    make_extra(out_hs.extra, out_cpps.extra)
    out_hs.extra.write( textwrap.dedent('''
            #endif
            '''))
    out_hs.extra.close()
    out_cpps.extra.close()

    # Now parse.
    #
    try:
        index = state.clang.cindex.Index.create()
    except Exception as e:
        raise Exception(f'libclang does not appear to be installed') from e

    header = f'{dir_mupdf}/include/mupdf/fitz.h'
    assert os.path.isfile( header), f'header={header}'

    # Get clang to parse mupdf/fitz.h and mupdf/pdf.h and mupdf/extra.h.
    #
    # It might be possible to use index.parse()'s <unsaved_files> arg to
    # specify these multiple files, but i couldn't get that to work.
    #
    # So instead we write some #include's to a temporary file and ask clang to
    # parse it.
    #
    temp_h = f'_mupdfwrap_temp.cpp'
    try:
        with open( temp_h, 'w') as f:
            if state.state_.linux or state.state_.macos:
                jlib.log('Prefixing Fitz headers with `typedef unsigned long size_t;`'
                        ' because size_t not available to clang on Linux/MacOS.')
                # On Linux, size_t is defined internally in gcc (e.g. not even
                # in /usr/include/stdint.h) and so not visible to clang.
                #
                # If we don't define it, clang complains about C99 not
                # supporting implicit int and appears to variously expand
                # size_t as different function pointers, e.g. `int (int *)` and
                # `int (*)(int *)`.
                #
                f.write( textwrap.dedent('''
                    /*
                    Workaround on Linux/MacOS. size_t is defined internally in
                    gcc (e.g. not even in /usr/include/stdint.h) and so not visible to clang.
                    */
                    typedef unsigned long size_t;
                    '''))
            if state.state_.macos:
                f.write( textwrap.dedent('''
                    /*
                    Workaround on MacOS: we need to define fixed-size int types
                    and FILE and va_list, similarly as with size_t above.
                    */
                    typedef signed char         int8_t;
                    typedef short               int16_t;
                    typedef int                 int32_t;
                    typedef long long           int64_t;
                    typedef unsigned char       uint8_t;
                    typedef unsigned short      uint16_t;
                    typedef unsigned int        uint32_t;
                    typedef unsigned long long  uint64_t;
                    typedef struct FILE FILE;
                    typedef struct va_list va_list;
                    '''))
            f.write( textwrap.dedent('''
                    #include "mupdf/extra.h"

                    #include "mupdf/fitz.h"
                    #include "mupdf/pdf.h"
                    '''))

        # libclang often doesn't have access to system headers so we define
        # MUPDF_WRAP_LIBCLANG so that extra.h can use dummy definition of
        # std::vector.
        #
        args = [
                '-I', f'{dir_mupdf}/include',
                '-I', f'{dir_mupdf}/platform/c++/include',
                '-D', 'MUPDF_WRAP_LIBCLANG',
                '-D', 'FZ_FUNCTION=',
                ]
        tu = index.parse(
                temp_h,
                args = args,
                options = 0
                        | state.clang.cindex.TranslationUnit.PARSE_INCOMPLETE
                        | state.clang.cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
                        ,
                )

        # Show warnings/errors from the parse. Failure to include stddef.h
        # appears to be harmless on Linux, but other failures seem to cause
        # more problems.
        #
        def show_clang_diagnostic( diagnostic, depth=0):
            for diagnostic2 in diagnostic.children:
                show_clang_diagnostic( diagnostic2, depth + 1)
            jlib.log( '{" "*4*depth}{diagnostic}')
        if tu.diagnostics:
            jlib.log( 'tu.diagnostics():')
            for diagnostic in tu.diagnostics:
                show_clang_diagnostic(diagnostic, 1)

    finally:
        if os.path.isfile( temp_h):
            os.remove( temp_h)

    # Write required #includes into .h files:
    #
    out_hs.exceptions.write( textwrap.dedent(
            '''
            #include <stdexcept>
            #include <string>

            #include "mupdf/fitz.h"

            '''))

    out_hs.internal.write( textwrap.dedent(
            '''
            #include <iostream>

            '''))

    out_hs.functions.write( textwrap.dedent(
            '''
            #include "mupdf/extra.h"

            #include "mupdf/fitz.h"
            #include "mupdf/pdf.h"

            #include <iostream>
            #include <string>
            #include <vector>

            '''))

    out_hs.classes.write( textwrap.dedent(
            '''
            #include "mupdf/fitz.h"
            #include "mupdf/functions.h"
            #include "mupdf/pdf.h"

            #include <string>
            #include <vector>

            '''))

    out_hs.classes2.write( textwrap.dedent(
            '''
            #include "classes.h"

            '''))

    # Write required #includes into .cpp files:
    #
    out_cpps.exceptions.write( textwrap.dedent(
            f'''
            #include "mupdf/exceptions.h"
            #include "mupdf/fitz.h"
            #include "mupdf/internal.h"

            #include <iostream>

            #include <string.h>

            {refcheck_if}
                static const bool   s_trace_exceptions = mupdf::internal_env_flag("MUPDF_trace_exceptions");
            #else
                static const bool   s_trace_exceptions_dummy = mupdf::internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace_exceptions");
            #endif
            '''))

    out_cpps.functions.write( textwrap.dedent(
            '''
            #include "mupdf/exceptions.h"
            #include "mupdf/functions.h"
            #include "mupdf/internal.h"
            #include "mupdf/extra.h"

            #include <assert.h>
            #include <sstream>

            #include <string.h>

            '''))

    out_cpps.classes.write(
            textwrap.dedent(
            f'''
            #include "mupdf/classes.h"
            #include "mupdf/classes2.h"
            #include "mupdf/exceptions.h"
            #include "mupdf/internal.h"

            #include "mupdf/fitz/geometry.h"

            #include <map>
            #include <mutex>
            #include <sstream>
            #include <string.h>
            #include <thread>

            #include <string.h>

            {refcheck_if}
                static const int    s_trace = mupdf::internal_env_flag("MUPDF_trace");
                static const bool   s_trace_keepdrop = mupdf::internal_env_flag("MUPDF_trace_keepdrop");
                static const bool   s_trace_director = mupdf::internal_env_flag("MUPDF_trace_director");
            #else
                static const int    s_trace = mupdf::internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace");
                static const bool   s_trace_keepdrop = mupdf::internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace_keepdrop");
                static const bool   s_trace_director = mupdf::internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace_director");
            #endif
            '''))

    out_cpps.classes2.write(
            textwrap.dedent(
            f'''
            #include "mupdf/classes2.h"
            #include "mupdf/exceptions.h"
            #include "mupdf/internal.h"

            #include "mupdf/fitz/geometry.h"

            #include <map>
            #include <mutex>
            #include <sstream>
            #include <string.h>
            #include <thread>

            #include <string.h>

            {refcheck_if}
                static const int    s_trace = mupdf::internal_env_flag("MUPDF_trace");
            #else
                static const int    s_trace = mupdf::internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace");
            #endif
            '''))

    namespace = 'mupdf'
    for _, _, file in out_cpps.get() + out_hs.get():
        if file in (out_cpps.internal, out_cpps.extra, out_hs.extra):
            continue
        make_namespace_open( namespace, file)

    # Write reference counting check code to out_cpps.classes.
    refcount_check_code( out_cpps.classes, refcheck_if)

    # Write declaration and definition for metadata_keys global.
    #
    out_hs.functions.write(
            textwrap.dedent(
            '''
            /*
            The keys that are defined for fz_lookup_metadata().
            */
            FZ_DATA extern const std::vector<std::string> metadata_keys;

            '''))
    out_cpps.functions.write(
            textwrap.dedent(
            f'''
            FZ_FUNCTION const std::vector<std::string> metadata_keys = {{
                    "format",
                    "encryption",
                    "info:Title",
                    "info:Author",
                    "info:Subject",
                    "info:Keywords",
                    "info:Creator",
                    "info:Producer",
                    "info:CreationDate",
                    "info:ModDate",
            }};

            {refcheck_if}
                static const int    s_trace = internal_env_flag("MUPDF_trace");
                static const bool   s_trace_keepdrop = internal_env_flag("MUPDF_trace_keepdrop");
                static const bool   s_trace_exceptions = internal_env_flag("MUPDF_trace_exceptions");
                static const bool   s_check_error_stack = internal_env_flag("MUPDF_check_error_stack");
            #else
                static const int    s_trace = internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace");
                static const bool   s_trace_keepdrop = internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace_keepdrop");
                static const bool   s_trace_exceptions = internal_env_flag_check_unset("{refcheck_if}", "MUPDF_trace_exceptions");
                static const bool   s_check_error_stack = internal_env_flag_check_unset("{refcheck_if}", "MUPDF_check_error_stack");
            #endif

            '''))

    # Write source code for exceptions and wrapper functions.
    #
    jlib.log( 'Creating wrapper functions...')
    make_function_wrappers(
            tu,
            namespace,
            out_hs.exceptions,
            out_cpps.exceptions,
            out_hs.functions,
            out_cpps.functions,
            out_hs.internal,
            out_cpps.internal,
            out_hs.classes2,
            out_cpps.classes2,
            generated,
            refcheck_if,
            )

    fn_usage = dict()
    functions_unrecognised = set()

    for fnname, cursor in state.state_.find_functions_starting_with( tu, '', method=True):
        fn_usage[ fnname] = [0, cursor]
        generated.c_functions.append(fnname)

    for structname, cursor in state.state_.structs[ tu].items():
        generated.c_structs.append( structname)

    windows_def = ''
    #windows_def += 'LIBRARY mupdfcpp\n'    # This breaks things.
    windows_def += 'EXPORTS\n'

    for name, cursor in state.state_.find_global_data_starting_with( tu, ('fz_', 'pdf_')):
        if state.state_.show_details(name):
            jlib.log('global: {name=}')
        generated.c_globals.append(name)
        windows_def += f'    {name} DATA\n'
    for fnname, cursor in state.state_.find_functions_starting_with( tu, ('fz_', 'pdf_', 'FT_'), method=False):
        if fnname == 'fz_is_infinite_irect':
            jlib.log( '{fnname=} {cursor.storage_class=}')
        if cursor.storage_class == state.clang.cindex.StorageClass.STATIC:
            # These fns do not work in windows.def, probably because they are
            # usually inline?
            #
            jlib.log('Not adding to windows_def because static: {fnname}()', 1)
        elif fnname in (
                'fz_lookup_metadata2',
                'fz_md5_pixmap2',
                'fz_pixmap_samples_int',
                'fz_samples_get',
                'fz_samples_set',
                'pdf_lookup_metadata2',
                'fz_md5_final2',
                'fz_highlight_selection2',
                'fz_search_page2',
                'fz_string_from_text_language2',
                'fz_get_glyph_name2',
                ):
            # These are excluded from windows_def because are C++ so
            # we'd need to use the mangled name in. Instead we mark them
            # with FZ_FUNCTION.
            pass
        else:
            windows_def += f'    {fnname}\n'
    # Add some internal fns that PyMuPDF requires.
    for fnname in (
            'FT_Get_First_Char',
            'FT_Get_Next_Char',
            'pdf_lookup_page_loc',
            'fz_scale_pixmap',
            'fz_pixmap_size',
            'fz_subsample_pixmap',
            'fz_copy_pixmap_rect',
            'fz_write_pixmap_as_jpeg',
            ):
        windows_def += f'    {fnname}\n'

    if debug:
        # In debug builds these are real fns, not macros, and we need to
        # make them exported.
        windows_def += f'    fz_lock_debug_lock\n'
        windows_def += f'    fz_lock_debug_unlock\n'

    jlib.fs_update( windows_def, f'{base}/windows_mupdf.def')

    def register_fn_use( name):
        assert name.startswith( ('fz_', 'pdf_'))
        if name in fn_usage:
            fn_usage[ name][0] += 1
        else:
            functions_unrecognised.add( name)

    # Write source code for wrapper classes.
    #
    jlib.log( 'Creating wrapper classes...')

    # Find all classes that we can create.
    #
    classes_ = []
    for cursor in parse.get_children(tu.cursor):
        if not cursor.spelling.startswith( ('fz_', 'pdf_')):
            continue
        if cursor.kind != state.clang.cindex.CursorKind.TYPEDEF_DECL:
            continue;
        type_ = state.get_name_canonical( cursor.underlying_typedef_type)
        if type_.kind not in (state.clang.cindex.TypeKind.RECORD, state.clang.cindex.TypeKind.ELABORATED):
            continue
        if type_.kind == state.clang.cindex.TypeKind.ELABORATED:
            jlib.log( 'state.clang.cindex.TypeKind.ELABORATED: {type_.spelling=}')

        if not cursor.is_definition():
            # Handle abstract type only if we have an ClassExtra for it.
            extras = classes.classextras.get( tu, cursor.spelling)
            if extras and extras.opaque:
                pass
                #log( 'Creating wrapper for opaque struct: {cursor.spelling=}')
            else:
                continue

        #struct_name = type_.spelling
        struct_name = cursor.spelling
        struct_name = util.clip( struct_name, 'struct ')
        if cursor.spelling != struct_name:
            jlib.log('{type_.spelling=} {struct_name=} {cursor.spelling=}')
        classname = rename.class_( struct_name)

        # For some reason after updating mupdf 2020-04-13, clang-python is
        # returning two locations for struct fz_buffer_s, both STRUCT_DECL. One
        # is 'typedef struct fz_buffer_s fz_buffer;', the other is the full
        # struct definition.
        #
        # No idea why this is happening. Using .canonical doesn't seem to
        # affect things.
        #
        for cl, cu, s in classes_:
            if cl == classname:
                jlib.logx( 'ignoring duplicate STRUCT_DECL for {struct_name=}')
                break
        else:
            classes_.append( (classname, cursor, struct_name))

    classes_.sort()

    # Write forward declarations - this is required because some class
    # methods take pointers to other classes.
    #
    out_hs.classes.write( '\n')
    out_hs.classes.write( '/* Forward declarations of all classes that we define. */\n')
    for classname, struct_cursor, struct_name in classes_:
        out_hs.classes.write( f'struct {classname};\n')
    out_hs.classes.write( '\n')

    # Create each class.
    #
    for classname, struct_cursor, struct_name in classes_:
        #jlib.log( 'creating wrapper {classname} for {cursor.spelling}')
        extras = classes.classextras.get( tu, struct_name)
        assert extras, f'struct_name={struct_name}'
        if extras.pod:
            struct_to_string_fns(
                    tu,
                    struct_cursor,
                    struct_name,
                    extras,
                    out_hs.functions,
                    out_cpps.functions,
                    )

        with jlib.LogPrefixScope( f'{struct_name}: '):
            is_container, has_to_string = class_wrapper(
                    tu,
                    register_fn_use,
                    struct_cursor,
                    struct_name,
                    classname,
                    extras,
                    out_hs.classes,
                    out_cpps.classes,
                    out_h_classes_end,
                    out_cpps.classes2,
                    out_hs.classes2,
                    generated,
                    refcheck_if,
                    )
        if is_container:
            generated.container_classnames.append( classname)
        if has_to_string:
            generated.to_string_structnames.append( struct_name)

    out_hs.functions.write( textwrap.dedent( '''
            /** Reinitializes the MuPDF context for single-threaded use, which
            is slightly faster when calling code is single threaded.

            This should be called before any other use of MuPDF.
            */
            FZ_FUNCTION void reinit_singlethreaded();

            '''))

    # Write close of namespace.
    out_hs.classes.write( out_h_classes_end.get())
    for _, _, file in out_cpps.get() + out_hs.get():
        if file in (out_cpps.internal, out_cpps.extra, out_hs.extra):
            continue
        make_namespace_close( namespace, file)

    # Write pod struct fns such as operator<<(), operator==() - these need to
    # be outside the namespace.
    #
    for classname, struct_cursor, struct_name in classes_:
        extras = classes.classextras.get( tu, struct_name)
        if extras.pod:
            # Make operator<<(), operator==(), operator!=() for POD struct.
            #
            pod_struct_fns(
                    tu,
                    namespace,
                    struct_cursor,
                    struct_name,
                    extras,
                    out_hs.functions,
                    out_cpps.functions,
                    )
            if extras.pod != 'none':
                # Make operator<<(), operator==(), operator!=() for POD class
                # wrappers.
                #
                pod_class_fns(
                        tu,
                        classname,
                        struct_cursor,
                        struct_name,
                        extras,
                        out_hs.classes,
                        out_cpps.classes,
                        )


    # Terminate multiple-inclusion guards in headers:
    #
    for name, _, file in out_hs.get():
        if name != 'extra':
            file.write( '\n#endif\n')

    out_hs.close()
    out_cpps.close()

    generated.h_files = [filename for _, filename, _ in out_hs.get()]
    generated.cpp_files = [filename for _, filename, _ in out_cpps.get()]
    if 0:   # lgtm [py/unreachable-statement]
        jlib.log( 'Have created:')
        for filename in filenames_h + filenames_cpp:
            jlib.log( '    {filename}')


    # Output usage information.
    #

    fn_usage_filename = f'{base}/fn_usage.txt'
    out_fn_usage = File( fn_usage_filename, tabify=False)
    functions_unused = 0
    functions_used = 0

    for fnname in sorted( fn_usage.keys()):
        n, cursor = fn_usage[ fnname]
        exclude_reasons = parse.find_wrappable_function_with_arg0_type_excluded_cache.get( fnname, [])
        if n:
            functions_used += 1
        else:
            functions_unused += 1
        if n and not exclude_reasons:
            continue

    out_fn_usage.write( f'Functions not wrapped by class methods:\n')
    out_fn_usage.write( '\n')

    for fnname in sorted( fn_usage.keys()):
        n, cursor = fn_usage[ fnname]
        exclude_reasons = parse.find_wrappable_function_with_arg0_type_excluded_cache.get( fnname, [])
        if not exclude_reasons:
            continue
        if n:
            continue
        num_interesting_reasons = 0
        for t, description in exclude_reasons:
            if t == parse.MethodExcludeReason_FIRST_ARG_NOT_STRUCT:
                continue
            if t == parse.MethodExcludeReason_VARIADIC:
                continue
            num_interesting_reasons += 1
        if num_interesting_reasons:
            try:
                out_fn_usage.write( f'    {declaration_text( cursor.type, cursor.spelling)}\n')
            except Clang6FnArgsBug as e:
                out_fn_usage.write( f'    {cursor.spelling} [full prototype not available due to known clang-6 issue]\n')
            for t, description in exclude_reasons:
                if t == parse.MethodExcludeReason_FIRST_ARG_NOT_STRUCT:
                    continue
                out_fn_usage.write( f'        {description}\n')
            out_fn_usage.write( '\n')

    out_fn_usage.write( f'\n')
    out_fn_usage.write( f'Functions used more than once:\n')
    for fnname in sorted( fn_usage.keys()):
        n, cursor = fn_usage[ fnname]
        if n > 1:
            out_fn_usage.write( f'    n={n}: {declaration_text( cursor.type, cursor.spelling)}\n')

    out_fn_usage.write( f'\n')
    out_fn_usage.write( f'Number of wrapped functions: {len(fn_usage)}\n')
    out_fn_usage.write( f'Number of wrapped functions used by wrapper classes: {functions_used}\n')
    out_fn_usage.write( f'Number of wrapped functions not used by wrapper classes: {functions_unused}\n')

    out_fn_usage.close()

    generated.c_enums = state.state_.enums[ tu]

    if num_regressions:
        raise Exception( f'There were {num_regressions} regressions')
    return tu


def test():
    '''
    Place to experiment with clang-python.
    '''
    text = ''
    if state.state_.linux:
        text += textwrap.dedent('''
            /*
            Workaround on Linux. size_t is defined internally in gcc. It isn't
            even in stdint.h.
            */
            typedef unsigned long size_t;
            ''')

    text += textwrap.dedent('''
            #include "mupdf/fitz.h"
            #include "mupdf/pdf.h"
            ''')
    path = 'wrap-test.c'
    jlib.fs_update( text, path)
    index = state.clang.cindex.Index.create()
    tu = index.parse( path, '-I /usr/include -I include'.split(' '))
    path2 = 'wrap-test.c.c'
    tu.save(path2)
    jlib.log( 'Have saved to: {path2}')
    parse.dump_ast( tu.cursor, 'ast')
    for diagnostic in tu.diagnostics:
        jlib.log('{diagnostic=}')
    for cursor in parse.get_members( tu.cursor):
        if 'cpp_test_' in cursor.spelling:
            parse.dump_ast(cursor, out=jlib.log)
