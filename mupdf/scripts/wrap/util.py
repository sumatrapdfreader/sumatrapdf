'''
Utility fns.
'''

import os

import jlib


def snake_to_camel( name, initial):
    '''
    Converts foo_bar to FooBar or fooBar.

    >>> snake_to_camel( 'foo_bar', True)
    FooBar
    >>> snake_to_camel( 'foo_bar_q__a', False)
    fooBarQ_A
    '''
    items = name.split( '_')
    ret = ''
    for i, item in enumerate( items):
        if not item:
            item = '_'
        elif i or initial:
            item = item[0].upper() + item[1:]
        ret += item
    return ret


def clip( text, prefixes, suffixes=''):
    '''
    Returns <text> with prefix(s) and suffix(s) removed if present.
    '''
    if isinstance( prefixes, str):
        prefixes = prefixes,
    if isinstance( suffixes, str):
        suffixes = suffixes,
    for prefix in prefixes:
        if text.startswith( prefix):
            text = text[ len( prefix):]
            break
    for suffix in suffixes:
        if suffix and text.endswith( suffix):
            text = text[ :-len( suffix)]
            break
    return text


class Rename:
    '''
    Rename functions that generate camelCase class names and lower-case
    function names with underscores.

    Using camel case in function names seems to result in gcc errors when
    compiling the code created by swig -python. e.g. in _wrap_vthrow_fn()

    mupdfcpp_swig.cpp: In function PyObject* _wrap_vthrow_fn(PyObject*, PyObject*)
    mupdfcpp_swig.cpp:88571:15: error: invalid array assignment
           arg3 = *temp;
    '''
    def function_raw( self, name):
        '''
        Name used by wrapper function when calling C function <name>.
        '''
        return f'::{name}'
    def function( self, name):
        '''
        Name of wrapper function that calls C function <name>.
        '''
        if name.startswith( 'pdf_'):
            return 'p' + name
        ret = f'{clip( name, "fz_")}'
        if ret in ('stdin', 'stdout', 'stderr'):
            #log( 'appending underscore to {ret=}')
            ret += '_'
        return ret
    def function_class_aware( self, name):
        return f'm{name}'
    def function_call( self, name):
        '''
        Name used by class methods when calling wrapper function - we call our
        wrapper function in the mupdf:: namespace.
        '''
        return f'mupdf::{self.function(name)}'
    def class_( self, structname):
        '''
        Name of class that wraps <structname>.
        '''
        structname = clip( structname, 'struct ')
        if structname.startswith( 'fz_'):
            return snake_to_camel( clip( structname, 'fz_'), initial=True)
        elif structname.startswith( 'pdf_'):
            # Retain Pdf prefix.
            return snake_to_camel( structname, initial=True)
    def internal( self, name):
        return f'internal_{name}'
    def method( self, structname, fnname):
        if structname.startswith( 'fz_'):
            ret = clip( fnname, ('fz_', 'pdf_'))
            if ret in ('stdin', 'stdout', 'stderr'):
                jlib.log( 'appending underscore to {ret=}')
                ret += '_'
            return ret
        if structname.startswith( 'pdf_'):
            return clip( fnname, ('fz_', 'pdf_'))
        assert 0, f'unrecognised structname={structname}'

rename = Rename()


def update_file_regress( text, filename, check_regression):
    '''
    Behaves like jlib.update_file(), but if check_regression is true and
    <filename> already exists with different content from <text>, we show a
    diff and raise an exception.
    '''
    text_old = jlib.update_file( text, filename, check_regression)
    if text_old:
        jlib.log( 'jlib.update_file() => {len(text_old)=}. {filename=} {check_regression}')
    if check_regression:
        if text_old is not None:
            # Existing content differs and <check_regression> is true.
            with open( f'{filename}-2', 'w') as f:
                f.write( text)
            jlib.log( 'Output would have changed: {filename}')
            jlib.system( f'diff -u {filename} {filename}-2', verbose=True, raise_errors=False, prefix=f'diff {os.path.relpath(filename)}: ', out='log')
            return Exception( f'Output would have changed: {filename}')
        else:
            jlib.log( 'Generated file unchanged: {filename}')
