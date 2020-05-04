#!/usr/bin/env python3

'''
Intended to behaves exactly like mutool, but uses the mupdf python => C++ =>
mupdf.so wrappers.

The code is intended to be similar to the mutool C code, to simplify
comparison.
'''

import getopt
import os
import sys
import textwrap

import mupdf


def usage():
    print( textwrap.dedent('''
            usage: mutool.py <command> [options]
            \ttrace\t-- trace device calls
            \tconvert\t-- convert document
            '''))


# Things for draw.
#

import mutool_draw

draw = mutool_draw.draw



# Things for convert.
#


def convert_usage():
    print( textwrap.dedent(
            f'''
        mutool convert version {mupdf.FZ_VERSION}
        Usage: mutool convert [options] file [pages]
        \t-p -\tpassword

        \t-A -\tnumber of bits of antialiasing (0 to 8)
        \t-W -\tpage width for EPUB layout
        \t-H -\tpage height for EPUB layout
        \t-S -\tfont size for EPUB layout
        \t-U -\tfile name of user stylesheet for EPUB layout
        \t-X\tdisable document styles for EPUB layout

        \t-o -\toutput file name (%d for page number)
        \t-F -\toutput format (default inferred from output file name)
        \t\t\traster: cbz, png, pnm, pgm, ppm, pam, pbm, pkm.
        \t\t\tprint-raster: pcl, pclm, ps, pwg.
        \t\t\tvector: pdf, svg.
        \t\t\ttext: html, xhtml, text, stext.
        \t-O -\tcomma separated list of options for output format

        \tpages\tcomma separated list of page ranges (N=last page)

            '''
        ))
    print( mupdf.fz_draw_options_usage)
    print( mupdf.fz_pcl_write_options_usage)
    print( mupdf.fz_pclm_write_options_usage)
    print( mupdf.fz_pwg_write_options_usage)
    print( mupdf.fz_stext_options_usage)
    print( mupdf.fz_pdf_write_options_usage)
    print( mupdf.fz_svg_write_options_usage)
    sys.exit(1)


def convert_runpage( doc, number, out):
    page = mupdf.Page( doc, number - 1)
    mediabox = page.bound_page()
    dev = out.begin_page(mediabox)
    page.run( dev, mupdf.Matrix(mupdf.fz_identity), mupdf.Cookie())
    out.end_page()

def convert_runrange( doc, count, range_, out):
    start = None
    end = None
    while 1:
        range_, start, end = mupdf.parse_page_range( range_, count)
        if range_ is None:
            break
        step = +1 if end > start else -1
        for i in range( start, end, step):
            convert_runpage( doc, i, out)

def convert( argv):
    # input options
    password = ''
    alphabits = 8
    layout_w = mupdf.FZ_DEFAULT_LAYOUT_W
    layout_h = mupdf.FZ_DEFAULT_LAYOUT_H
    layout_em = mupdf.FZ_DEFAULT_LAYOUT_EM
    layout_css = None
    layout_use_doc_css = 1

    # output options
    output = None
    format_ = None
    options = ''

    items, argv = getopt.getopt( argv, 'p:A:W:H:S:U:Xo:F:O:')
    for option, value in items:
        if 0: pass
        elif option == '-p':    password = value
        elif option == '-A':    alphabits = int(value)
        elif option == '-W':    layout_w = float( value)
        elif option == '-H':    layout_h = float( value)
        elif option == '-S':    layout_em = float( value)
        elif option == '-U':    layout_css = value
        elif option == '-X':    layout_use_doc_css = 0
        elif option == '-o':    output = value
        elif option == '-F':    format_ = value
        elif option == '-O':    options = value
        else:   assert 0

    if not argv or (not format_ and not output):
        convert_usage()

    mupdf.set_aa_level( alphabits)
    if layout_css:
        buf = mupdf.Buffer( layout_css)
        mupdf.set_user_css( buf.string_from_buffer())

    mupdf.set_use_document_css(layout_use_doc_css)

    print( f'output={output!r} format_={format_!r} options={options!r}')
    if format_:
        out = mupdf.DocumentWriter( output, format_, options)
    else:
        out = mupdf.DocumentWriter( output, options)

    i = 0
    while 1:
        if i >= len( argv):
            break
        arg = argv[i]
        doc = mupdf.Document( arg)
        if doc.needs_password():
            if not doc.authenticate_password( password):
                raise Exception( f'cannot authenticate password: {arg}')
        doc.layout_document( layout_w, layout_h, layout_em)
        count = doc.count_pages()

        range_ = '1-N'
        if i + 1 < len(argv) and mupdf.is_page_range(ctx, argv[i+1]):
            i += 1
            range_ = argv[i]
        convert_runrange( doc, count, range_, out)
        i += 1




# Things for trace.
#

def trace_usage():
    print( textwrap.dedent('''
            Usage: mutool trace [options] file [pages]
            \t-p -\tpassword

            \t-W -\tpage width for EPUB layout
            \t-H -\tpage height for EPUB layout
            \t-S -\tfont size for EPUB layout
            \t-U -\tfile name of user stylesheet for EPUB layout
            \t-X\tdisable document styles for EPUB layout

            \t-d\tuse display list

            \tpages\tcomma separated list of page numbers and ranges
            '''))
    sys.exit( 1)

def trace_runpage( use_display_list, doc, number):
    page = mupdf.Page( doc, number-1)
    mediabox = page.bound_page()
    print( f'<page number="{number}" mediabox="{mediabox.x0} {mediabox.y0} {mediabox.x1} {mediabox.y1}">')
    output = mupdf.Output( mupdf.Output.Fixed_STDOUT)
    dev = mupdf.Device( output)
    if use_display_list:
        list_ = mupdf.DisplayList( page)
        list_.run_display_list( dev, mupdf.Matrix(mupdf.fz_identity), mupdf.Rect(mupdf.fz_infinite_rect), mupdf.Cookie())
    else:
        page.run( dev, mupdf.Matrix(mupdf.fz_identity), mupdf.Cookie())
    output.close_output()
    print( '</page>')

def trace_runrange( use_display_list, doc, count, range_):
    start = None
    end = None
    while 1:
        range_, start, end = mupdf.parse_page_range( range_, count)
        if range_ is None:
            break
        step = +1 if end > start else -1
        for i in range( start, end, step):
            trace_runpage( use_display_list, doc, i)

def trace( argv):

    password = ''
    layout_w = mupdf.FZ_DEFAULT_LAYOUT_W
    layout_h = mupdf.FZ_DEFAULT_LAYOUT_H
    layout_em = mupdf.FZ_DEFAULT_LAYOUT_EM
    layout_css = None
    layout_use_doc_css = 1

    use_display_list = 0

    argv_i = 0
    while 1:
        arg = argv[ argv_i]
        if arg == '-p':
            password = next( opt)
        elif arg == '-W':
            argv_i += 1
            layout_w = float( argv[argv_i])
        elif arg == '-H':
            argv_i += 1
            layout_h = float( argv[argv_i])
        elif arg == '-S':
            argv_i += 1
            layout_em = float( argv[argv_i])
        elif arg == '-U':
            argv_i += 1
            layout_css = argv[argv_i]
        elif arg == '-X':
            layout_use_doc_css = 0
        elif arg == '-d':
            use_display_list = 1
        else:
            break
        argv_i += 1

    if argv_i == len( argv):
        trace_usage()

    if layout_css:
        buffer_ = mupdf.Buffer( layout_css)
        mupdf.mupdf_set_user_css( buffer_.string_from_buffer())

    mupdf.set_use_document_css( layout_use_doc_css)

    for argv_i in range( argv_i, len( argv)):
        arg = argv[ argv_i]
        doc = mupdf.Document( arg)
        if doc.needs_password():
            doc.authenticate_password( password)
        doc.layout_document( layout_w, layout_h, layout_em)
        print( f'<document filename="{arg}">')
        count = doc.count_pages()
        if argv_i + 1 < len( argv) and mupdf.is_page_range( argv[ argv_i+1]):
            argv_i += 1
            trace_runrange( use_display_list, doc, count, argv[ argv_i])
        else:
            trace_runrange( use_display_list, doc, count, '1-N')
        print( '<document>')


#
#

def main2( argv):
    arg1 = argv[1]
    fn = getattr( sys.modules[__name__], arg1, None)
    if not fn:
        print( f'cannot find {arg1}')
        usage()
        sys.exit(1)

    fn( argv[2:])


def main():

    argv = sys.argv
    if len( sys.argv) < 2:

        # Use test args.
        for zlib_pdf in (
                os.path.expanduser( '~/mupdf/thirdparty/zlib/zlib.3.pdf'),
                os.path.expanduser( '~/artifex/mupdf/thirdparty/zlib/zlib.3.pdf'),
                ):
            if os.path.isfile( zlib_pdf):
                break
        else:
            raise Exception( 'cannot find zlib.3.pdf')
        for command in [
                f'trace {zlib_pdf}',
                f'convert -o zlib.3.pdf-%d.png {zlib_pdf}',
                f'draw -o zlib.3.pdf-%d.png -s tmf -v -y l -w 150 -R 30 -h 200 {zlib_pdf}',
                f'draw -o zlib.png -R 10 {zlib_pdf}',
                ]:
            if 0:
                # This breaks - looks like <colorspace> gets dropped and *m_internal is freed?
                main2( [None] + command.split())
            else:
                command = f'{argv[0]} {command}'
                print( 'running test command: %s' % command)
                e = os.system( f'{command}')
                assert not e, f'command failed: {command}'
    else:
        main2( sys.argv)


if __name__ == '__main__':
    main()
