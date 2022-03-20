'''
Things for generating Python-specific output.
'''

from . import cpp
from . import parse


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
    generated.swig_python.write(f'    Wrapper for out-params of {cursor.mangled_name}().\n')
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
