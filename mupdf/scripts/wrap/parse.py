'''
Support for accessing parse tree for MuPDF headers.
'''

import collections
import os
import sys
import time

import jlib

try:
    import clang.cindex
except ImportError as e:
    if '--venv' not in sys.argv:
        jlib.log( 'Warning, could not import clang: {e}')
    clang = None

from . import classes
from . import cpp
from . import state
from . import util


def get_extras(tu, type_):
    '''
    Returns (cursor, typename, extras):
        cursor: for base type.
        typename:
        extras: None or from classes.classextras.
    '''
    base_type = get_base_type( type_)
    base_type_cursor = base_type.get_declaration()
    base_typename = get_base_typename( base_type)
    extras = classes.classextras.get( tu, base_typename)
    return base_type_cursor, base_typename, extras

def fileline( cursor):
    '''
    Returns <file>:<line> from cursor.location.
    '''
    f = cursor.location.file
    filename = os.path.relpath( f.name) if f else ''
    return f'{filename}:{cursor.location.line}'


def prefix( name):
    if name.startswith( 'fz_'):
        return 'fz_'
    if name.startswith( 'pdf_'):
        return 'pdf_'
    assert 0, f'unrecognised prefix (not fz_ or pdf_) in name={name}'


def get_fz_extras( tu, fzname):
    '''
    Finds ClassExtra for <fzname>, coping if <fzname> starts with 'const ' or
    'struct '. Returns None if not found.
    '''
    fzname = util.clip( fzname, 'const ')
    fzname = util.clip( fzname, 'struct ')
    ce = classes.classextras.get( tu, fzname)
    return ce

def get_children(cursor):
    '''
    Like cursor.get_children() but recurses into cursors with
    clang.cindex.CursorKind.UNEXPOSED_DECL which picks up top-level items
    marked with `extern "C"`, and clang.cindex.CursorKind.LINKAGE_SPEC which
    picks up items inside `extern "C" {...}`.
    '''
    verbose = 0
    for cursor in cursor.get_children():
        #verbose = state.state_.show_details( cursor.spelling)
        #verbose = 1
        if cursor.kind == clang.cindex.CursorKind.UNEXPOSED_DECL:
            # Things tagged with `extern "C" appear to be within this
            # cursor.
            for cursor2 in cursor.get_children():
                if verbose and cursor.spelling:
                    jlib.log( '{cursor.spelling=}')
                yield cursor2
        elif cursor.kind == clang.cindex.CursorKind.LINKAGE_SPEC:
            # extern "C" {...}
            for cursor2 in cursor.get_children():
                if verbose and cursor.spelling:
                    jlib.log( '{cursor.spelling=}')
                yield cursor2
        else:
            if verbose and cursor.spelling:
                jlib.log( '{cursor.spelling=}')
            yield cursor

def get_members( type_or_cursor, include_empty=False):
    '''
    Yields cursor for each member. Uses whichever of
    clang.cindex.Cursor.get_children() or clang.cindex.Type.get_fields() works.

    Args:
        type_or_cursor:
            .
        include_empty:
            If false (the default), we first try
            clang.cindex.Cursor.get_children(), but ignore items for which
            .spelling==''. If resulting list is empty, we instead use
            clang.cindex.Type.get_fields().

            Otherwise, we return list of items from
            clang.cindex.Cursor.get_children(), regardless of whether they
            have .spelling==''. This allows finding of non-typedef enums, for
            example.
    '''
    if isinstance( type_or_cursor, clang.cindex.Type):
        cursor = type_or_cursor.get_declaration()
    elif isinstance( type_or_cursor, clang.cindex.Cursor):
        cursor = type_or_cursor
    else:
        assert 0
    if cursor.type.kind in (state.clang.cindex.TypeKind.TYPEDEF, state.clang.cindex.TypeKind.ELABORATED):
        cursor2 = cursor.underlying_typedef_type.get_declaration()
    else:
        cursor2 = cursor

    if 0:
        # Diagnostics to show the difference between
        # clang.cindex.Cursor.get_children() and
        # clang.cindex.Type.get_fields().
        #
        # For example it looks like clang.cindex.Cursor.get_children() can
        # return an extra item with .spelling=='' for 'union {...} u;'.
        #
        ret_cursor = list()
        ret_cursor_no_empty = list()
        ret_type = list()
        for cursor3 in cursor2.get_children():
            item = (cursor3.spelling, cursor3.location.file.name, cursor3.location.line)
            ret_cursor.append( item)
            if cursor3.spelling:
                ret_cursor_no_empty.append( item)
        for cursor3 in cursor.type.get_canonical().get_fields():
            ret_type.append( (cursor3.spelling, cursor3.location.file.name, cursor3.location.line))
        ret_cursor.sort()
        ret_type.sort()
        ret_cursor_no_empty.sort()
        if (not ret_cursor_no_empty) and ret_type:
            jlib.log( 'ret_type and not ret_cursor_no_empty:')
            for i in ret_type:
                jlib.log( '    ret_type: {i}')
        if 0 and ret_cursor != ret_type:
            jlib.log('get_children() != get_fields():')
            for i in ret_cursor:
                jlib.log( '    ret_cursor: {i}')
            for i in ret_type:
                jlib.log( '    ret_type: {i}')

    ret = list()
    for cursor3 in cursor2.get_children():
        if include_empty or cursor3.spelling:
            ret.append(cursor3)
    if not ret:
        type_ = cursor.type.get_canonical()
        for cursor3 in type_.get_fields():
            ret.append( cursor3)
    for i in ret:
        yield i

def get_field0( type_):
    '''
    Returns cursor for first field in <type_> or None if <type_> has no fields.
    '''
    verbose = state.state_.show_details( type_.spelling)
    for cursor in get_members(type_):
        return cursor

get_base_type_cache = dict()
def get_base_type( type_):
    '''
    Repeatedly dereferences pointer and returns the ultimate type.
    '''
    # Caching reduces time from to 0.24s to 0.1s.
    key = type_.spelling
    ret = get_base_type_cache.get( key)
    if ret is None:
        while 1:
            type_ = state.get_name_canonical( type_)
            if type_.kind != clang.cindex.TypeKind.POINTER:
                break
            type_ = type_.get_pointee()
        ret = type_
        get_base_type_cache[ key] = ret

    return ret

def get_base_typename( type_):
    '''
    Follows pointer to get ultimate type, and returns its name, with any
    leading 'struct ' or 'const ' removed.
    '''
    type_ = get_base_type( type_)
    ret = type_.spelling
    ret = util.clip( ret, 'const ')
    ret = util.clip( ret, 'struct ')
    return ret

def is_double_pointer( type_):
    '''
    Returns true if <type_> is double pointer.
    '''
    type_ = state.get_name_canonical( type_)
    if type_.kind == clang.cindex.TypeKind.POINTER:
        type_ = state.get_name_canonical( type_.get_pointee())
        if type_.kind == clang.cindex.TypeKind.POINTER:
            return True

has_refs_cache = dict()
def has_refs( tu, type_):
    '''
    Returns (offset, bits) if <type_> has a 'refs' member, otherwise False.
        offset:
            Byte offset of 'refs' or name of 'refs' for use with offsetof(),
            e.g. 'super.refs'.
        bits:
            Size of 'refs' in bits. Will be -1 if there is no simple .refs
            member (e.g. fz_xml).
    '''
    type0 = type_
    type_ = type_.get_canonical()

    key = type_.spelling
    key = util.clip(key, 'struct ')
    verbose = state.state_.show_details( key)
    ret = has_refs_cache.get( key, None)
    if ret is None:
        ret = False
        if verbose:
            jlib.log( 'Analysing {type0.spelling=} {type_.spelling=} {key=}')

        for prefix in (
                'fz_',
                'pdf_',
                ):
            if verbose:
                jlib.log( '{type_.spelling=} {prefix=}')
            if key.startswith( prefix):
                if verbose:
                    jlib.log( 'Type is a fz_ or pdf_ struct: {key=}')
                keep_name = f'{prefix}keep_{key[len(prefix):]}'
                keep_fn_cursor = state.state_.find_function( tu, keep_name, method=False)
                if verbose:
                    jlib.log( '{keep_name=} {keep_fn_cursor=}')
                if keep_fn_cursor:
                    if verbose:
                        jlib.log( 'There is a keep() fn for this type so it uses reference counting: {keep_name=}')
                    base_type_cursor = get_base_type( type_).get_declaration()
                    if base_type_cursor.is_definition():
                        if verbose:
                            jlib.log( 'Type definition is available so we look for .refs member: {key=} {type_.spelling=} {fileline(base_type_cursor)=}')
                        if verbose:
                            jlib.log('type_.get_fields()')
                            for cursor in get_members(type_):
                                jlib.log('    {cursor.spelling=}')
                            jlib.log('base_type_cursor.get_children()')
                            for cursor in base_type_cursor.get_children():
                                jlib.log('    {cursor.spelling=}')
                            jlib.log('.')
                        for cursor in get_members(type_):
                            name = cursor.spelling
                            type2 = state.get_name_canonical( cursor.type)
                            if verbose:
                                jlib.log( '{name=} {type2.spelling=}')
                            if name == 'refs' and type2.spelling == 'int':
                                ret = 'refs', 32
                                break
                            if name == 'storable' and type2.spelling in ('struct fz_storable', 'fz_storable'):
                                ret = 'storable.refs', 32
                                break
                    else:
                        if 0:
                            jlib.log('Definition is not available for {key=}'
                                    ' because {base_type_cursor.spelling=} .is_definition()'
                                    ' returns false.'
                                    ' base_type_cursor.location={fileline(base_type_cursor)}'
                                    )

                    if not ret:
                        if verbose:
                            jlib.log(
                                    '{type_.spelling=}: Cannot find .refs member or we only have forward'
                                    ' declaration, so have to hard-code the size and offset'
                                    ' of the refs member.'
                                    )
                        if base_type_cursor.is_definition():
                            if key == 'pdf_document':
                                ret = 'super.refs', 32
                            elif key == 'pdf_page':
                                ret = 'super.refs', 32
                            elif key == 'fz_pixmap':
                                ret = 'storable.refs', 32
                            elif key in (
                                    'fz_colorspace',
                                    'fz_image',
                                    ):
                                return 'key_storable.storable.refs', 32
                            elif key == 'pdf_cmap':
                                return 'storable.refs', 32
                        else:
                            #jlib.log( 'No definition available, i.e. forward decl only.')
                            if key == 'pdf_obj':
                                ret = 0, 16
                            elif key == 'fz_path':
                                ret = 0, 8
                            elif key in (
                                    'fz_separations',
                                    'fz_halftone',
                                    'pdf_annot',
                                    'pdf_graft_map',
                                    ):
                                # Forward decl, first member is 'int regs;'.
                                return 0, 32
                            elif key in (
                                    'fz_display_list',
                                    'fz_glyph',
                                    'fz_jbig2_globals',
                                    'pdf_function',
                                    ):
                                # Forward decl, first member is 'fz_storable storable;'.
                                return 0, 32
                            elif key == 'fz_xml':
                                # This only has a simple .refs member if the
                                # .up member is null, so we don't attempt to
                                # use it, by returning size=-1.
                                ret = 0, -1

                        if ret is None:
                            # Need to hard-code info for this type.
                            assert 0, jlib.expand_nv(
                                    '{key=} has {keep_name}() fn but is forward decl or we cannot find .refs,'
                                    ' and we have no hard-coded info about size and offset of .regs.'
                                    ' {type0.spelling=} {type_.spelling=} {base_type_cursor.spelling}'
                                    )
                    assert ret, (
                            f'{key} has {keep_name}() but have not found size/location of .refs member.'
                            f' {type_.spelling=}'
                            f' {base_type_cursor.spelling=}'
                            f': {fileline(base_type_cursor)}'
                            )

        if type_.spelling in (
                'struct fz_document',
                'struct fz_buffer',
                ):
            assert ret
        #jlib.log('Populating has_refs_cache with {key=} {ret=}')
        has_refs_cache[ key] = ret
    return ret

def get_value( item, name):
    '''
    Enhanced wrapper for getattr().

    We call ourselves recursively if name contains one or more '.'. If name
    ends with (), makes fn call to get value.
    '''
    if not name:
        return item
    dot = name.find( '.')
    if dot >= 0:
        item_sub = get_value( item, name[:dot])
        return get_value( item_sub, name[dot+1:])
    if name.endswith('()'):
        value = getattr( item, name[:-2])
        assert callable(value)
        return value()
    return getattr( item, name)

def get_list( item, *names):
    '''
    Uses get_value() to find values of specified fields in <item>.

    Returns list of (name,value) pairs.
    '''
    ret = []
    for name in names:
        value = get_value( item, name)
        ret.append((name, value))
    return ret

def get_text( item, prefix, sep, *names):
    '''
    Returns text describing <names> elements of <item>.
    '''
    ret = []
    for name, value in get_list( item, *names):
        ret.append( f'{name}={value}')
    return prefix + sep.join( ret)


def dump_ast( cursor, out=None, depth=0):
    cleanup = lambda: None
    if out is None:
        out = sys.stdout
    if isinstance(out, str):
        out = open(out, 'w')
        cleanup = lambda : out.close()
    try:
        indent = depth*4*' '
        for cursor2 in cursor.get_children():

            def or_none(f):
                try:
                    return f()
                except Exception:
                    return
            result = or_none( cursor2.type.get_result)
            type_ = cursor2.type
            type_canonical = or_none( cursor2.type.get_canonical)

            text = indent
            text += jlib.log_text(
                    '{cursor2.kind=}'
                    ' {cursor2.displayname=}'
                    ' {cursor2.spelling=}'
                    ' {cursor2.linkage=}'
                    ' {cursor2.is_definition()=}'
                    )
            if result:
                text += jlib.log_text(' {result.spelling=}')
            if type_:
                text += jlib.log_text(' {type_.spelling=}')
            if type_canonical:
                text += jlib.log_text(' {type_canonical.spelling=}')
            text += '\n'
            if callable(out):
                out( text)
            else:
                out.write(text)

            dump_ast( cursor2, out, depth+1)
    finally:
        cleanup()

def show_ast( filename, includes):
    jlib.log('Parsing {filename=}')
    index = clang.cindex.Index.create()
    args = []
    for include in includes:
        args += ['-I', include]
    tu = index.parse( filename,
            args = args,
            )
    dump_ast( tu.cursor)

class Arg:
    '''
    Information about a function argument.

        .cursor:
            Cursor for the argument.
        .name:
            Arg name, or an invented name if none was present.
        .separator:
            '' for first returned argument, ', ' for the rest.
        .alt:
            Cursor for underlying fz_ struct type if <arg> is a pointer to or
            ref/value of a fz_ struct type that we wrap. Else None.
        .out_param:
            True if this looks like an out-parameter, e.g. alt is set and
            double pointer, or arg is pointer other than to char.
        .name_python:
            Same as .name or .name+'_' if .name is a Python keyword.
        .name_csharp:
            Same as .name or .name+'_' if .name is a C# keyword.
    '''
    def __init__(self, cursor, name, separator, alt, out_param):
        self.cursor = cursor
        self.name = name
        self.separator = separator
        self.alt = alt
        self.out_param = out_param
        if name in ('in', 'is'):
            self.name_python = f'{name}_'
        else:
            self.name_python = name
        self.name_csharp = f'{name}_' if name in ('out', 'is', 'in', 'params') else name

    def __str__(self):
        return f'Arg(name={self.name} alt={"true" if self.alt else "false"} out_param={self.out_param})'


get_args_cache = dict()

# get_args() needs to know how to get something hashable from a clang.cindex.Cursor.
if clang:
    g_cursor_is_hashable = issubclass(clang.cindex.Cursor, collections.abc.Hashable)

def get_args( tu, cursor, include_fz_context=False, skip_first_alt=False, verbose=False):
    '''
    Yields Arg instance for each arg of the function at <cursor>.

    Args:
        tu:
            A clang.cindex.TranslationUnit instance.
        cursor:
            Clang cursor for the function.
        include_fz_context:
            If false, we skip args that are 'struct fz_context*'
        skip_first_alt:
            If true, we skip the first arg with .alt set.
        verbose:
            .
    '''
    # We are called a few times for each function, and the calculations we do
    # are slow, so we cache the returned items. E.g. this reduces total time of
    # --build 0 from 3.5s to 2.1s.
    #
    if verbose:
        jlib.log( '## Looking at args of {cursor.spelling=}')
    if g_cursor_is_hashable:
        key = tu, cursor, include_fz_context, skip_first_alt
    else:
        key = tu, cursor.location.file, cursor.location.line, include_fz_context, skip_first_alt
    ret = get_args_cache.get( key)
    if not verbose and state.state_.show_details(cursor.spelling):
        verbose = True
    if ret is None:
        if verbose:
            jlib.log( '## Looking at args of {cursor.spelling=}')
        ret = []
        i = 0
        i_alt = 0
        separator = ''
        for arg_cursor in cursor.get_arguments():
            if verbose:
                jlib.log('{arg_cursor.kind=} {arg_cursor.spelling=}')
            assert arg_cursor.kind == clang.cindex.CursorKind.PARM_DECL
            if not include_fz_context and is_pointer_to( arg_cursor.type, 'fz_context'):
                # Omit this arg because our generated mupdf_*() wrapping functions
                # use internalContextGet() to get a context.
                continue
            name = arg_cursor.spelling or f'arg_{i}'
            if 0 and name == 'stmofsp':
                verbose = True
            alt = None
            out_param = False
            base_type_cursor, base_typename, extras = get_extras( tu, arg_cursor.type)
            if verbose:
                jlib.log( 'Looking at arg. {extras=}')
            if extras:
                if verbose:
                    jlib.log( '{extras.opaque=} {base_type_cursor.kind=} {base_type_cursor.is_definition()=}')
                if extras.opaque:
                    # E.g. we don't have access to definition of fz_separation,
                    # but it is marked in classes.classextras with opaque=true,
                    # so there will be a wrapper class.
                    alt = base_type_cursor
                elif (1
                        and base_type_cursor.kind == clang.cindex.CursorKind.STRUCT_DECL
                        #and base_type_cursor.is_definition()
                        ):
                    alt = base_type_cursor
            if verbose:
                jlib.log( '{arg_cursor.type.spelling=} {base_typename=} {arg_cursor.type.kind=} {get_base_typename(arg_cursor.type)=}')
                jlib.log( '{get_base_type(arg_cursor.type).kind=}')
            if alt:
                if is_double_pointer( arg_cursor.type):
                    out_param = True
            elif get_base_typename( arg_cursor.type) in ('char', 'unsigned char', 'signed char', 'void', 'FILE'):
                if is_double_pointer( arg_cursor.type):
                    if verbose:
                        jlib.log( 'setting outparam: {cursor.spelling=} {arg_cursor.type=}')
                    if cursor.spelling == 'pdf_clean_file':
                        # Don't mark char** argv as out-param, which will also
                        # allow us to tell swig to convert python lists into
                        # (argc,char**) pair.
                        pass
                    else:
                        if verbose:
                            jlib.log('setting out_param to true')
                        out_param = True
            elif ( base_typename.startswith( ('fz_', 'pdf_'))
                    and get_base_type(arg_cursor.type).kind != clang.cindex.TypeKind.ENUM
                    ):
                # Pointer to fz_ struct is not usually an out-param.
                if verbose:
                    jlib.log(
                            'not out-param because pointer to struct:'
                            ' arg is: {arg_cursor.displayname=}'
                            ' {base_typename.spelling=}'
                            ' {extras}'
                            ' {arg_cursor.type.kind=}'
                            )
            elif arg_cursor.type.kind == clang.cindex.TypeKind.POINTER:
                pointee = arg_cursor.type.get_pointee()
                if verbose:
                    jlib.log( 'clang.cindex.TypeKind.POINTER')
                if state.get_name_canonical( pointee).kind == clang.cindex.TypeKind.FUNCTIONPROTO:
                    # Don't mark function-pointer args as out-params.
                    if verbose:
                        jlib.log( 'clang.cindex.TypeKind.FUNCTIONPROTO')
                elif pointee.is_const_qualified():
                    if verbose:
                        jlib.log( 'is_const_qualified()')
                elif pointee.spelling == 'FILE':
                    pass
                else:
                    if verbose:
                        jlib.log( 'setting out_param = True')
                    out_param = True
            if alt:
                i_alt += 1
            i += 1
            if alt and skip_first_alt and i_alt == 1:
                continue
            arg =  Arg(arg_cursor, name, separator, alt, out_param)
            ret.append(arg)
            if verbose:
                jlib.log( 'Appending {arg=}')
            separator = ', '

        get_args_cache[ key] = ret

    for arg in ret:
        yield arg


def fn_has_struct_args( tu, cursor):
    '''
    Returns true if fn at <cursor> takes any fz_* struct args.
    '''
    for arg in get_args( tu, cursor):
        if arg.alt:
            return True

def get_first_arg( tu, cursor):
    '''
    Returns (arg, n), where <arg> is from get_args() for first argument (or
    None if no arguments), and <n> is number of arguments.
    '''
    n = 0
    ret = None
    for arg in get_args( tu, cursor):
        if n == 0:
            ret = arg
        n += 1
    return ret, n


is_cache = dict()

def is_( type_, type2):
    key = type_.spelling, type2
    ret = is_cache.get( key)
    if ret is None:
        d = cpp.declaration_text( type_, '', top_level='')
        d = util.clip( d, 'const ')
        d = util.clip( d, 'struct ')
        d = d.strip()
        ret = (d == type2)
        is_cache[ key] = ret
    return ret

is_pointer_to_cache = dict()

def is_pointer_to( type_, destination, verbose=False):
    '''
    Returns true if <type> is a pointer to <destination>.

    We do this using text for <destination>, rather than a clang.cindex.Type
    or clang.cindex.Cursor, so that we can represent base types such as int or
    char without having clang parse system headers. This involves stripping any
    initial 'struct ' text.

    Also, clang's representation of mupdf's varying use of typedef, struct and
    forward-declarations is rather difficult to work with directly.

    type_:
        A clang.cindex.Type.
    destination:
        Text typename.
    '''
    # Use cache - reduces time from 0.6s to 0.2.
    #
    key = type_.spelling, destination
    ret = is_pointer_to_cache.get( key)
    if verbose or ret is None:
        assert isinstance( type_, clang.cindex.Type)
        if verbose: jlib.log( '{type_.spelling=}')
        ret = None
        destination = util.clip( destination, 'struct ')
        if type_.kind == clang.cindex.TypeKind.POINTER:
            pointee = type_.get_pointee()
            if verbose: jlib.log('{pointee.spelling=}')
            d = cpp.declaration_text( pointee, '', top_level='', verbose=verbose)
            d = util.clip( d, 'const ')
            d = util.clip( d, 'struct ')
            if verbose:
                jlib.log( '{destination=} {type_.get_pointee().kind=} {type_.get_pointee().spelling=} {state.get_name_canonical( type_.get_pointee()).spelling=}')
            ret = d.strip() == destination or d.strip() == f'const {destination}'
        is_pointer_to_cache[ key] = ret

    return ret

def is_pointer_to_pointer_to( type_, destination, verbose=False):
    if verbose:
        jlib.log( '{type_.spelling=}')
    if type_.kind != clang.cindex.TypeKind.POINTER:
        return False
    pointee = type_.get_pointee()
    return is_pointer_to( pointee, destination, verbose=verbose)


class MethodExcludeReason_VARIADIC:
    pass
class MethodExcludeReason_OMIT_CLASS:
    pass
class MethodExcludeReason_NO_EXTRAS:
    pass
class MethodExcludeReason_NO_RAW_CONSTRUCTOR:
    pass
class MethodExcludeReason_NOT_COPYABLE:
    pass
class MethodExcludeReason_NO_WRAPPER_CLASS:
    pass
class MethodExcludeReason_ENUM:
    pass
class MethodExcludeReason_FIRST_ARG_NOT_STRUCT:
    pass

# Maps from <structname> to list of functions satisfying conditions specified
# by find_wrappable_function_with_arg0_type() below.
#
find_wrappable_function_with_arg0_type_cache = None

# Maps from fnname to list of strings, each string being a description of why
# this fn is not suitable for wrapping by class method.
#
find_wrappable_function_with_arg0_type_excluded_cache = None

# Maps from function name to the class that has a method that wraps this
# function.
#
fnname_to_method_structname = dict()

def find_wrappable_function_with_arg0_type_cache_populate( tu):
    '''
    Populates caches with wrappable functions.
    '''
    global find_wrappable_function_with_arg0_type_cache
    global find_wrappable_function_with_arg0_type_excluded_cache

    if find_wrappable_function_with_arg0_type_cache:
        return

    t0 = time.time()

    find_wrappable_function_with_arg0_type_cache = dict()
    find_wrappable_function_with_arg0_type_excluded_cache = dict()

    for fnname, cursor in state.state_.find_functions_starting_with( tu, ('fz_', 'pdf_'), method=True):

        exclude_reasons = []

        if fnname.startswith( 'fz_drop_') or fnname.startswith( 'fz_keep_'):
            continue
        if fnname.startswith( 'pdf_drop_') or fnname.startswith( 'pdf_keep_'):
            continue

        if cursor.type.is_function_variadic():
            exclude_reasons.append(
                    (
                    MethodExcludeReason_VARIADIC,
                    'function is variadic',
                    ))

        # Look at resulttype.
        #
        result_type = cursor.type.get_result()
        if result_type.kind == clang.cindex.TypeKind.POINTER:
            result_type = result_type.get_pointee()
        result_type_name = state.get_name_canonical( result_type)
        result_type_name = util.clip( result_type.spelling, 'struct ')
        if result_type_name.startswith( ('fz_', 'pdf_')):
            if result_type.kind == clang.cindex.TypeKind.TYPEDEF:
                result_cursor = result_type.get_declaration()
                result_type = result_cursor.underlying_typedef_type

            if result_type.kind == state.clang.cindex.TypeKind.ELABORATED:
                result_type_extras = get_fz_extras( tu, result_type_name)
                if not result_type_extras:
                    exclude_reasons.append(
                            (
                            MethodExcludeReason_NO_EXTRAS,
                            f'no extras defined for result_type={result_type_name}.'
                            ))
                else:
                    if not result_type_extras.constructor_raw:
                        exclude_reasons.append(
                                (
                                MethodExcludeReason_NO_RAW_CONSTRUCTOR,
                                f'wrapper for result_type={result_type_name} does not have raw constructor.',
                                ))
                    if not result_type_extras.copyable:
                        exclude_reasons.append(
                                (
                                MethodExcludeReason_NOT_COPYABLE,
                                f'wrapper for result_type={result_type_name} is not copyable.',
                                ))

        # Look at args
        #
        i = 0
        arg0_cursor = None
        for arg in get_args( tu, cursor):

            base_typename = get_base_typename( arg.cursor.type)
            if not arg.alt and base_typename.startswith( ('fz_', 'pdf_')):
                t_canonical = state.get_name_canonical( arg.cursor.type)
                if t_canonical.kind == clang.cindex.TypeKind.ENUM:
                    # We don't (yet) wrap fz_* enums, but for now at least we
                    # still wrap functions that take fz_* enum parameters -
                    # callers will have to use the fz_* type.
                    #
                    # For example this is required by mutool_draw.py because
                    # mudraw.c calls fz_set_separation_behavior().
                    #
                    jlib.logx(
                            'not excluding {fnname=} with enum fz_ param:'
                            ' {arg.cursor.spelling=}'
                            ' {arg.cursor.type.kind}'
                            ' {state.get_name_canonical(arg.cursor.type).kind=}'
                            )
                elif t_canonical.kind == clang.cindex.TypeKind.POINTER:
                    pass
                else:
                    exclude_reasons.append(
                            (
                            MethodExcludeReason_NO_WRAPPER_CLASS,
                            f'no wrapper class for arg i={i}:'
                                f' {state.get_name_canonical( arg.cursor.type).spelling}'
                                f' {state.get_name_canonical(arg.cursor.type).kind}'
                                ,
                            ))
            if i == 0:
                if arg.alt:
                    arg0_cursor = arg.alt
                else:
                    exclude_reasons.append(
                            (
                            MethodExcludeReason_FIRST_ARG_NOT_STRUCT,
                            'first arg is not fz_* struct',
                            ))
            i += 1

        if exclude_reasons:
            find_wrappable_function_with_arg0_type_excluded_cache[ fnname] = exclude_reasons
            #if fnname == 'fz_load_outline':   # lgtm [py/unreachable-statement]
            if state.state_.show_details(fnname):
                jlib.log( 'Excluding {fnname=} from possible class methods because:')
                for i in exclude_reasons:
                    jlib.log( '    {i}')
        else:
            if i > 0:
                # <fnname> is ok to wrap.
                arg0 = state.get_name_canonical( arg0_cursor.type).spelling
                arg0 = util.clip( arg0, 'struct ')

                #jlib.log( '=== Adding to {arg0=}: {fnname=}. {len(fnname_to_method_structname)=}')

                items = find_wrappable_function_with_arg0_type_cache.setdefault( arg0, [])
                items.append( fnname)

                fnname_to_method_structname[ fnname] = arg0

    jlib.log1( f'populating find_wrappable_function_with_arg0_type_cache took {time.time()-t0:.2f}s')


def find_wrappable_function_with_arg0_type( tu, structname):
    '''
    Return list of fz_*() function names which could be wrapped as a method of
    our wrapper class for <structname>.

    The functions whose names we return, satisfy all of the following:

        First non-context param is <structname> (by reference, pointer or value).

        If return type is a fz_* struct (by reference, pointer or value), the
        corresponding wrapper class has a raw constructor.
    '''
    find_wrappable_function_with_arg0_type_cache_populate( tu)

    ret = find_wrappable_function_with_arg0_type_cache.get( structname, [])
    if state.state_.show_details(structname):
        jlib.log('{structname=}: {len(ret)=}:')
        for i in ret:
            jlib.log('    {i}')
    return ret
find_struct_cache = None


def find_class_for_wrappable_function( fn_name):
    '''
    If <fn_name>'s first arg is a struct and our wrapper class for this struct
    has a method that wraps <fn_name>, return name of wrapper class.

    Otherwise return None.
    '''
    return fnname_to_method_structname.get( fn_name)


def find_struct( tu, structname, require_definition=True):
    '''
    Finds definition of struct.

    fixme: actually finds definition of anything, doesn't have to be a struct.

    Args:
        tu:
            Translation unit.
        structname:
            Name of struct to find.
        require_definition:
            Only return cursor if it is for definition of structure.

    Returns cursor for definition or None.
    '''
    verbose = state.state_.show_details( structname)
    verbose = False
    if verbose:
        jlib.log( '{=structname}')
    structname = util.clip( structname, ('const ', 'struct '))   # Remove any 'struct ' prefix.
    if verbose:
        jlib.log( '{=structname}')
    global find_struct_cache
    if find_struct_cache is None:
        find_struct_cache = dict()
        for cursor in get_children( tu.cursor):
            already = find_struct_cache.get( cursor.spelling)
            if already is None:
                find_struct_cache[ cursor.spelling] = cursor
            elif cursor.is_definition() and not already.is_definition():
                find_struct_cache[ cursor.spelling] = cursor
    ret = find_struct_cache.get( structname)
    if verbose:
        jlib.log( '{=ret}')
    if not ret:
        return
    if verbose:
        jlib.log( '{=require_definition ret.is_definition()}')
    if require_definition and not ret.is_definition():
        return
    return ret


def find_name( cursor, name, nest=0):
    '''
    Returns cursor for specified name within <cursor>, or None if not found.

    name:
        Name to search for. Can contain '.' characters; we look for each
        element in turn, calling ourselves recursively.

    cursor:
        Item to search.
    '''
    assert cursor.spelling != ''
    if cursor.spelling == '':
        # Anonymous item; this seems to occur for (non-anonymous) unions.
        #
        # We recurse into children directly.
        #
        for c in get_members(cursor):
            ret = find_name_internal( c, name, nest+1)
            if ret:
                return ret

    d = name.find( '.')
    if d >= 0:
        head, tail = name[:d], name[d+1:]
        # Look for first element then for remaining.
        c = find_name( cursor, head, nest+1)
        if not c:
            return
        ret = find_name( c, tail, nest+2)
        return ret

    for c in get_members(cursor):
        if c.spelling == '':
            ret = find_name( c, name, nest+1)
            if ret:
                return ret
        if c.spelling == name:
            return c
