'''
Utility fns.
'''

import os

import jlib

def clip( text, prefixes, suffixes=''):
    '''
    Returns <text> with prefix(s) and suffix(s) removed if present.
    '''
    if isinstance( prefixes, str):
        prefixes = prefixes,
    if isinstance( suffixes, str):
        suffixes = suffixes,
    while 1:
        for prefix in prefixes:
            if text.startswith( prefix):
                text = text[ len( prefix):]
                break
        else:
            break
    while 1:
        for suffix in suffixes:
            if suffix and text.endswith( suffix):
                text = text[ :-len( suffix)]
                break
        else:
            break
    return text


def update_file_regress( text, filename, check_regression):
    '''
    Behaves like jlib.fs_update(), but if check_regression is true and
    <filename> already exists with different content from <text>, we show a
    diff and raise an exception.
    '''
    text_old = jlib.fs_update( text, filename, check_regression)
    if text_old:
        jlib.log( 'jlib.fs_update() => {len(text_old)=}. {filename=} {check_regression}')
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
