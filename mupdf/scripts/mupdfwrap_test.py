#!/usr/bin/env python3

'''
Simple tests of the Python MuPDF API.
'''

import mupdf
import os
import sys


_log_prefix = ''

def log(text):
    print(f'{_log_prefix}{text}')

def log_prefix_set(prefix):
    global _log_prefix
    _log_prefix = prefix


g_test_n = 0

g_mupdf_root = os.path.abspath('%s/../..' % __file__)


def test(path):
    '''
    Runs various mupdf operations on <path>, which is assumed to be a file that
    mupdf can open.
    '''
    log(f'testing path={path}')

    assert os.path.isfile(path)
    global g_test_n
    g_test_n += 1

    if 1:
        # Test operations using functions:
        #
        log('Testing functions.')

        log(f'    Opening: %s' % path)
        document = mupdf.open_document(path)
        log(f'    mupdf.needs_password(document)={mupdf.needs_password(document)}')
        log(f'    mupdf.count_pages(document)={mupdf.count_pages(document)}')
        log(f'    mupdf.document_output_intent(document)={mupdf.document_output_intent(document)}')

    # Test operations using classes:
    #
    log(f'Testing classes')

    document = mupdf.Document(path)
    log(f'Have created mupdf.Document for {path}')
    log(f'document.needs_password()={document.needs_password()}')
    log(f'document.count_pages()={document.count_pages()}')

    for k in (
            'format',
            'encryption',
            'info:Author',
            'info:Title',
            'info:Creator',
            'info:Producer',
            'qwerty',
            ):
        v = document.lookup_metadata(k)
        log(f'document.lookup_metadata() k={k} returned v={v!r}')
        if k == 'qwerty':
            assert v is None
        else:
            pass

    zoom = 10
    scale = mupdf.Matrix.scale(zoom/100., zoom/100.)
    page_number = 0
    log(f'Have created scale: a={scale.a} b={scale.b} c={scale.c} d={scale.d} e={scale.e} f={scale.f}')

    colorspace = mupdf.Colorspace(mupdf.Colorspace.Fixed_RGB)
    log(f'{colorspace.m_internal.key_storable.storable.refs}')
    pixmap = mupdf.Pixmap(document, page_number, scale, colorspace, 0)
    log(f'Have created pixmap: {pixmap.m_internal.w} {pixmap.m_internal.h} {pixmap.m_internal.stride} {pixmap.m_internal.n}')

    filename = f'mupdf_test-out1-{g_test_n}.png'
    pixmap.save_pixmap_as_png(filename)
    log(f'Have created {filename} using pixmap.save_pixmap_as_png().')

    # Print image data in ascii PPM format. Copied from
    # mupdf/docs/examples/example.c.
    #
    samples = pixmap.m_internal.samples
    stride = pixmap.m_internal.stride
    n = pixmap.m_internal.n
    filename = f'mupdf_test-out2-{g_test_n}.ppm'
    with open(filename, 'w') as f:
        f.write('P3\n')
        f.write('%s %s\n' % (pixmap.m_internal.w, pixmap.m_internal.h))
        f.write('255\n')
        for y in range(0, pixmap.m_internal.h):
            for x in range(pixmap.m_internal.w):
                if x:
                    f.write('  ')
                offset = y * stride + x * n
                f.write('%3d %3d %3d' % (
                        mupdf.bytes_getitem(samples, offset + 0),
                        mupdf.bytes_getitem(samples, offset + 1),
                        mupdf.bytes_getitem(samples, offset + 2),
                        ))
            f.write('\n')
    log(f'Have created {filename} by scanning pixmap.')

    # Generate .png and but create Pixmap from Page instead of from Document.
    #
    page = mupdf.Page(document, 0)
    separations = page.page_separations()
    log(f'page_separations() returned {"true" if separations else "false"}')
    pixmap = mupdf.Pixmap(page, scale, colorspace, 0)
    filename = f'mupdf_test-out3-{g_test_n}.png'
    pixmap.save_pixmap_as_png(filename)
    log(f'Have created {filename} using pixmap.save_pixmap_as_png()')

    # Show links
    log(f'Links.')
    page = mupdf.Page(document, 0)
    link = mupdf.load_links(page.m_internal);
    log(f'{link}')
    if link:
        for i in link:
            log(f'{i}')

    # Check we can iterate over Link's, by creating one manually.
    #
    link = mupdf.Link(mupdf.Rect(0, 0, 1, 1), None, "hello")
    log(f'items in <link> are:')
    for i in link:
        log(f'    {i.m_internal.refs} {i.m_internal.uri}')

    # Check iteration over Outlines.
    #
    log(f'Outlines.')
    outline = mupdf.Outline(document)
    log(f'outline.m_internal={outline.m_internal}')
    if outline.m_internal:
        log(f'{outline.uri()} {outline.page()} {outline.x()} {outline.y()} {outline.is_open()} {outline.title()}')
        log(f'items in outline tree are:')
    for o in outline:
        log(f'    {o.uri()} {o.page()} {o.x()} {o.y()} {o.is_open()} {o.title()}')

    # Check iteration over StextPage.
    #
    log(f'StextPage.')
    stext_options = mupdf.StextOptions(0)
    page_num = 40
    try:
        stext_page = mupdf.StextPage(document, page_num, stext_options)
    except Exception:
        log('no page_num={page_num}')
    else:
        device_stext = mupdf.Device(stext_page, stext_options)
        matrix = mupdf.Matrix()
        page = mupdf.Page(document, 0)
        cookie = mupdf.Cookie()
        page.run(device_stext, matrix, cookie)
        log(f'    stext_page is:')
        for block in stext_page:
            log(f'        block:')
            for line in block:
                line_text = ''
                for char in line:
                    line_text += chr(char.m_internal.c)
                log(f'            {line_text}')

        device_stext.close_device()

    # Check copy-constructor.
    log(f'Checking copy-constructor')
    document2 = mupdf.Document(document)
    del document
    page = mupdf.Page(document2, 0)
    scale = mupdf.Matrix()
    pixmap = mupdf.Pixmap(page, scale, colorspace, 0)
    pixmap.save_pixmap_as_png('mupdf_test-out3.png')

    stdout = mupdf.Output(mupdf.Output.Fixed_STDOUT)
    log(f'{type(stdout)} {stdout.m_internal.state}')

    mediabox = page.bound_page()
    out = mupdf.DocumentWriter(filename, 'png', '')
    dev = out.begin_page(mediabox)
    page.run(dev, mupdf.Matrix(mupdf.fz_identity), mupdf.Cookie())
    out.end_page()

    # Check out-params are converted into python return value.
    bitmap = mupdf.Bitmap(10, 20, 8, 72, 72)
    bitmap_details = bitmap.bitmap_details()
    log(f'{bitmap_details}')
    assert bitmap_details == [10, 20, 8, 12]

    log(f'finished test of %s' % path)


if __name__ == '__main__':

    paths = sys.argv[1:]
    if not paths:
        paths = [
                f'{g_mupdf_root}/thirdparty/lcms2/doc/LittleCMS2.10 API.pdf',
                f'{g_mupdf_root}/thirdparty/lcms2/doc/LittleCMS2.10 Plugin API.pdf',
                f'{g_mupdf_root}/thirdparty/lcms2/doc/LittleCMS2.10 tutorial.pdf',
                f'{g_mupdf_root}/thirdparty/lcms2/plugins/fast_float/doc/LittleCMS fast float extensions 1.0.pdf',
                f'{g_mupdf_root}/thirdparty/zlib/zlib.3.pdf',
                ]
    # Run test() on all the .pdf files in the mupdf repository.
    #
    for path in paths:

        log_prefix_set(f'{os.path.relpath(path, g_mupdf_root)}: ')
        try:
            test(path)
        finally:
            log_prefix_set('')

    log(f'finished')
