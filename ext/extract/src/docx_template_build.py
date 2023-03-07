#! /usr/bin/env python3

'''
Creates C code for creating docx/odt files using internal template docx/odt
content.

Args:

    --pretty <directory>
        Prettyfies all .xml files within <directory> using 'xmllint --format'.

    -f
        Force touch of output file, even if unchanged.

    -i <in-path>
        Set template docx/odt file to extract from.

    -n docx | odt
        Infix to use in generated identifier names.

    -o <out-path>
        Set name of output files.

        We write to <out-path>.c and <out-path>.h.
'''

import io
import os
import re
import sys
import textwrap


def system(command):
    '''
    Like os.system() but raises exception if command fails.
    '''
    e = os.system(command)
    if e:
        print(f'command failed: {command}')
        assert 0

def read(path, encoding):
    '''
    Returns contents of file.
    '''
    with open(path, 'rb') as f:
        raw = f.read()
        if encoding:
            return raw.decode(encoding)
        return raw

def write(text, path, encoding):
    '''
    Writes text to file.
    '''
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, 'wb') as f:
        f.write(text.encode(encoding))

def write_if_diff(text, path, encoding, force):
    '''
    Does nothing if <force> is false and file named <path> already contains
    <text>. Otherwise writes <text> to file named <path>.
    '''
    if not force:
        if os.path.isfile(path):
            old = read(path, encoding)
            if old == text:
                return
        print(f'Updating path={path} because contents have changed')
    write(text, path, encoding)

def check_path_safe(path):
    '''
    Raises exception unless path consists only of characters and sequences that
    are known to be safe for shell commands.
    '''
    if '..' in path:
        raise Exception(f'Path is unsafe because contains "..": {path!r}')
    for c in path:
        if not c.isalnum() and c not in '/._-':
            #print(f'unsafe character {c} in: {path}')
            raise Exception(f'Path is unsafe because contains "{c}": {path!r}')

def path_safe(path):
    '''
    Returns True if path is safe else False.
    '''
    try:
        check_path_safe(path)
    except Exception:
        return False
    else:
        return True

assert not path_safe('foo;rm -rf *')
assert not path_safe('..')
assert path_safe('foo/bar.x')


def main():

    path_in = None
    path_out = None
    infix = None
    force = False

    args = iter(sys.argv[1:])
    while 1:
        try: arg = next(args)
        except StopIteration: break
        if arg == '-h' or arg == '--help':
            print(__doc__)
            return
        elif arg == '--pretty':
            d = next(args)
            for dirpath, dirnames, filenames in os.walk(d):
                for filename in filenames:
                    if not filename.endswith('.xml'):
                        continue
                    path = os.path.join(dirpath, filename)
                    system(f'xmllint --format {path} > {path}-')
                    system(f'mv {path}- {path}')
        elif arg == '-f':
            force = True
        elif arg == '-i':
            path_in = next(args)
        elif arg == '-n':
            infix = next(args)
        elif arg == '-o':
            path_out = next(args)
        else:
            assert 0, f'unrecognised arg: {arg}'

    if not path_in:
        return

    if not path_in:
        raise Exception('Need to specify -i <in-path>')
    if not infix:
        raise Exception('Need to specify -n <name>')
    if not path_out:
        raise Exception('Need to specify -o <out-path>')

    check_path_safe(path_in)
    check_path_safe(path_out)
    path_temp = f'{path_in}.dir'
    os.system(f'rm -r "{path_temp}" 2>/dev/null')
    system(f'unzip -q -d {path_temp} {path_in}')

    out_c = io.StringIO()
    out_c.write(f'/* THIS IS AUTO-GENERATED CODE, DO NOT EDIT. */\n')
    out_c.write(f'\n')
    out_c.write(f'#include "{os.path.basename(path_out)}.h"\n')
    out_c.write(f'\n')


    out_c.write(f'const {infix}_template_item_t {infix}_template_items[] =\n')
    out_c.write(f'{{\n')

    num_items = 0
    for dirpath, dirnames, filenames in os.walk(path_temp):
        dirnames.sort()

        if 0:
            # Write code to create directory item in zip. This isn't recognised by zipinfo, and doesn't
            # make Word like the file.
            #
            name = dirpath[ len(path_temp)+1: ]
            if name:
                if not name.endswith('/'):
                    name += '/'
                    out_c3.write(f'        if (extract_zip_write_file(zip, NULL, 0, "{infix}")) goto end;\n')

        for filename in sorted(filenames):
            num_items += 1
            path = os.path.join(dirpath, filename)
            #print(f'looking at path={path}')
            name = path[ len(path_temp)+1: ]
            out_c.write(f'    {{\n')
            out_c.write(f'        "{name}",\n')
            if filename.endswith('.xml') or filename.endswith('.rels'):
                text = read(os.path.join(dirpath, filename), 'utf-8')
                #print(f'first line is: %r' % text.split("\n")[0])
                text = text.replace('"', '\\"')

                # Looks like .docx template files use \r\n when we interpret them as
                # utf-8, so we preserve this in the generated strings.
                #
                # .odt seems to have first line ending with '\n', not '\r\n'.
                #
                text = text.replace('\r', '\\r')
                text = text.replace('\n', '\\n"\n                "')

                # Split on '<' to avoid overly-long lines, which break windows
                # compiler.
                #
                text = re.sub('([<][^/])', '"\n                "\\1', text)

                # Remove name of document creator.
                #
                for tag in 'dc:creator', 'cp:lastModifiedBy':
                    text = re.sub(f'[<]{tag}[>][^<]*[<]/{tag}[>]', f'<{tag}></{tag}>', text)

                out_c.write(f'        "')
                # Represent non-ascii utf-8 bytes as C escape sequences.
                for c in text:
                    if ord( c) <= 127:
                        out_c.write( c)
                    else:
                        for cc in c.encode( 'utf-8'):
                            out_c.write( f'\\x{cc:02x}')
                out_c.write(f'"\n')
            else:
                data = read(os.path.join(dirpath, filename), encoding=None)
                out_c.write(f'        "')
                i = 0
                for byte in data:
                    i += 1
                    if i % 16 == 0:
                        out_c.write(f'"\n        "')
                    out_c.write(f'\\x{byte:02x}')
                out_c.write(f'"\n')

            out_c.write(f'    }},\n')
            out_c.write(f'\n')

    out_c.write(f'}};\n')
    out_c.write(f'\n')
    out_c.write(f'int {infix}_template_items_num = {num_items};\n')

    out_c = out_c.getvalue()
    write_if_diff(out_c, f'{path_out}.c', 'utf-8', force)

    out_h = io.StringIO()
    out_h.write(f'#ifndef EXTRACT_{infix.upper()}_TEMPLATE_H\n')
    out_h.write(f'#define EXTRACT_{infix.upper()}_TEMPLATE_H\n')
    out_h.write(f'\n')
    out_h.write(f'/* THIS IS AUTO-GENERATED CODE, DO NOT EDIT. */\n')
    out_h.write(f'\n')
    out_h.write(f'\n')
    out_h.write(f'typedef struct\n')
    out_h.write(f'{{\n')
    out_h.write(f'    const char* name; /* Name of item in {infix} archive. */\n')
    out_h.write(f'    const char* text; /* Contents of item in {infix} archive. */\n')
    out_h.write(f'}} {infix}_template_item_t;\n')
    out_h.write(f'\n')
    out_h.write(f'extern const {infix}_template_item_t {infix}_template_items[];\n')
    out_h.write(f'extern int {infix}_template_items_num;\n')
    out_h.write(f'\n')
    out_h.write(f'\n')
    out_h.write(f'#endif\n')
    write_if_diff(out_h.getvalue(), f'{path_out}.h', 'utf-8', force)
    #os.system(f'rm -r "{path_temp}"')

if __name__ == '__main__':
    main()
