'''
Things for generating Python-specific output.
'''

import jlib

from . import cpp
from . import parse
from . import rename
from . import state
from . import util


def make_outparam_helper_python(
        tu,
        cursor,
        fnname,
        fnname_wrapper,
        generated,
        main_name,
        ):
    # Write python wrapper.
    return_void = cursor.result_type.spelling == 'void'
    generated.swig_python.write('')
    generated.swig_python.write(f'def {main_name}(')
    sep = ''
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            continue
        generated.swig_python.write(f'{sep}{arg.name_python}')
        sep = ', '
    generated.swig_python.write('):\n')
    generated.swig_python.write(f'    """\n')
    generated.swig_python.write(f'    Wrapper for out-params of {cursor.spelling}().\n')
    sep = ''
    generated.swig_python.write(f'    Returns: ')
    sep = ''
    if not return_void:
        generated.swig_python.write( f'{cursor.result_type.spelling}')
        sep = ', '
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            generated.swig_python.write(f'{sep}{cpp.declaration_text(arg.cursor.type.get_pointee(), arg.name_python)}')
            sep = ', '
    generated.swig_python.write(f'\n')
    generated.swig_python.write(f'    """\n')
    generated.swig_python.write(f'    outparams = {main_name}_outparams()\n')
    generated.swig_python.write(f'    ret = {main_name}_outparams_fn(')
    sep = ''
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            continue
        generated.swig_python.write(f'{sep}{arg.name_python}')
        sep = ', '
    generated.swig_python.write(f'{sep}outparams)\n')
    generated.swig_python.write(f'    return ')
    sep = ''
    if not return_void:
        generated.swig_python.write(f'ret')
        sep = ', '
    for arg in parse.get_args( tu, cursor):
        if arg.out_param:
            generated.swig_python.write(f'{sep}outparams.{arg.name_python}')
            sep = ', '
    generated.swig_python.write('\n')
    generated.swig_python.write('\n')


def cppyy_add_outparams_wrapper(
        tu,
        fn_name,
        fn_cursor,
        state_,
        generated,
        ):

    parse.find_wrappable_function_with_arg0_type_cache_populate( tu)

    def get_ctype_name( arg):
        type_name = state.get_name_canonical( arg.cursor.type.get_pointee()).spelling
        if type_name in (
                'char',
                'double',
                'float',
                'int',
                'long',
                'short',
                ):
            return f'ctypes.c_{type_name}'
        elif type_name == 'unsigned long':  return 'ctypes.c_ulong'
        elif type_name == 'unsigned short': return 'ctypes.c_ushort'
        elif type_name == 'unsigned int':   return 'ctypes.c_uint'
        else:
            return None
    num_out_params = 0
    arg0 = None
    for arg in parse.get_args( tu, fn_cursor):
        if arg0 is None:
            arg0 = arg
        if arg.out_param:
            if not get_ctype_name( arg):
                #jlib.log( 'Not creating cppyy out-param wrapper for {fn_name}() because cannot handle {arg.cursor.type.spelling=}')
                return
            num_out_params += 1
    if num_out_params:
        return_void = fn_cursor.result_type.spelling == 'void'
        text = ''
        text += f'# Patch mupdf.m{fn_name} to return out-params directly.\n'
        text += f'mupdf_m{fn_name}_original = cppyy.gbl.mupdf.m{fn_name}\n'
        text += f'def mupdf_m{fn_name}( '
        sep = ''
        for arg in parse.get_args( tu, fn_cursor):
            if arg.out_param:
                pass
            else:
                text += f'{sep}{arg.name_python}'
                sep = ', '
        text += f'):\n'
        for arg in parse.get_args( tu, fn_cursor):
            if arg.out_param:
                ctype_name = get_ctype_name( arg)
                text += f'    {arg.name_python} = {ctype_name}()\n'
        text += f'    ret = mupdf_m{fn_name}_original( '
        sep = ''
        for arg in parse.get_args( tu, fn_cursor):
            if arg.out_param:
                text += f'{sep}ctypes.pointer( {arg.name_python})'
            else:
                text += f'{sep}{arg.name_python}'
            sep = ', '
        text += f')\n'
        sep = ' '
        text += f'    return'
        if not return_void:
            text += ' ret'
            sep = ', '
        for arg in parse.get_args( tu, fn_cursor):
            if arg.out_param:
                text += f'{sep}{arg.name_python}.value'
                sep = ', '
        text += f'\n'
        text += f'cppyy.gbl.mupdf.m{fn_name} = mupdf_m{fn_name}\n'

        # Look for class method that will use mupdf.m<fn_name>.
        #Generated
        struct_name = parse.find_class_for_wrappable_function( fn_name)
        if struct_name:
            class_name = rename.class_( struct_name)
            method_name = rename.method( struct_name, fn_name)
            text += f'# Also patch Python version of {fn_name}() in class wrapper for {struct_name} method {class_name}::{method_name}()\n'
            text += f'cppyy.gbl.mupdf.{class_name}.{method_name}_original = cppyy.gbl.mupdf.{class_name}.{method_name}\n'
            text += f'cppyy.gbl.mupdf.{class_name}.{method_name} = mupdf_m{fn_name}\n'
        else:
            pass
            #jlib.log( 'Not a method of a class: {fn_name=}')

        text += f'\n'

        generated.cppyy_extra += text

    if 0:
        jlib.log( 'parse.fnname_to_method_structname [{len(parse.fnname_to_method_structname)}]:')
        for fn_name, struct_name in parse.fnname_to_method_structname.items():
            jlib.log( '    {fn_name}: {struct_name}')
