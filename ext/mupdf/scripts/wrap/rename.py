'''
Functions to create C++ names from MuPDF names.
'''

import os

import jlib

from . import util


def snake_to_camel( name, initial):
    '''
    Converts foo_bar to FooBar or fooBar.

    >>> snake_to_camel( 'foo_bar', True)
    FooBar
    >>> snake_to_camel( 'foo_bar_q__a', False)
    fooBarQ_A
    '''
    # libclang can treat size_t oddly, which we work around when parsing MuPDF
    # headers, so we should not be given size_t.
    #
    assert name != 'size_t'
    items = name.split( '_')
    ret = ''
    for i, item in enumerate( items):
        if not item:
            item = '_'
        elif i or initial:
            item = item[0].upper() + item[1:].lower()
        ret += item
    return ret


# Using camel case in function names seems to result in gcc errors when
# compiling the code created by swig -python. e.g. in _wrap_vthrow_fn()
#
# mupdfcpp_swig.cpp: In function PyObject* _wrap_vthrow_fn(PyObject*, PyObject*)
# mupdfcpp_swig.cpp:88571:15: error: invalid array assignment
#        arg3 = *temp;


def internal( name):
    '''
    Used for internal names, e.g. exception types.
    '''
    return f'internal_{name}'

def error_class( error_enum):
    '''
    Name of generated class for MuPDF `FZ_ERROR_*` enum.
    '''
    assert error_enum.startswith( 'FZ_ERROR_')
    return snake_to_camel( error_enum, initial=True)

def c_fn( fnname):
    '''
    Returns fully-qualified name of MuPDF C function `fnname()`.
    '''
    return f'::{fnname}'

def ll_fn( fnname):
    '''
    Returns name of low-level wrapper function for MuPDF C function `fnname()`,
    adding a `ctx` arg and converting MuPDF exceptions into C++ exceptions.
    '''
    assert not fnname.startswith( 'll_'), f'fnname={fnname}'
    return f'll_{fnname}'

    if name.startswith( 'pdf_'):
        return 'p' + name
    ret = f'{util.clip( name, "fz_")}'
    if ret in ('stdin', 'stdout', 'stderr'):
        #log( 'appending underscore to {ret=}')
        ret += '_'
    return ret

def namespace():
    return 'mupdf'

def namespace_ll_fn( fnname):
    '''
    Returns full-qualified name of low-level wrapper function for MuPDF C
    function `fnname()`, adding a `ctx` arg and converting MuPDF exceptions
    into C++ exceptions.
    '''
    return f'{namespace()}::{ll_fn(fnname)}'

def fn( fnname):
    '''
    Returns name of wrapper function for MuPDF C function `fnname()`, using
    wrapper classes for args and return value.
    '''
    return fnname

def namespace_fn( fnname):
    '''
    Returns fully-qualified name of wrapper function for MuPDF C function
    `fnname()`, using wrapper classes for args and return values.
    '''
    return f'{namespace()}::{fn(fnname)}'

def class_( structname):
    '''
    Returns name of class that wraps MuPDF struct `structname`.
    '''
    structname = util.clip( structname, 'struct ')

    # Note that we can't return `structname` here because this will end up with
    # SWIG complaining like:
    #
    #   Error: 'pdf_xref' is multiply defined in the generated target language module
    #
    # - because SWIG internally puts everything into a single namespace.
    #
    #return structname

    return snake_to_camel( structname, initial=True)
    if structname.startswith( 'fz_'):
        return snake_to_camel( util.clip( structname, 'fz_'), initial=True)
    elif structname.startswith( 'pdf_'):
        # Retain Pdf prefix.
        return snake_to_camel( structname, initial=True)

def namespace_class( structname):
    '''
    Returns fully-qualified name of class that wraps MuPDF struct `structname`.
    '''
    return f'{namespace()}::{class_(structname)}'

def method( structname, fnname):
    '''
    Returns name of class method that wraps MuPDF function `fnname()`.
    '''
    return fnname
    if structname:
        structname = structname.lstrip( 'struct ')
    assert structname is None or structname.startswith( ('fz_', 'pdf_'))
    ret = util.clip( fnname, ('fz_', 'pdf_'))
    if ret in ('stdin', 'stdout', 'stderr'):
        jlib.log( 'appending underscore to {ret=}')
        ret += '_'
    return ret
