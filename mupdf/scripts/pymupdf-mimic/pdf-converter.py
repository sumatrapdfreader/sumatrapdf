#! /usr/bin/env python3

import mupdf

import os
import sys

path = sys.argv[1]
document = mupdf.Document(path)

if document.pdf_specifics().m_internal:
    raise Exception('document is PDF already')
path_out = f'{path}.pdf'
print(f'Converting {path!r} to {path_out!r}')

def convert_to_pdf(document, page_from=0, page_to=-1, rotate=0):
    num_pages = document.count_pages()
    page_from = max(page_from, 0)
    page_from = min(page_from, num_pages-1)
    if page_to < 0:
        page_to = num_pages-1
    page_to = min(page_to, num_pages-1)

    page_delta = 1
    if page_to < page_from:
        p = page_to
        page_to = page_from
        page_from = p
        page_delta = -1

    document_out = mupdf.PdfDocument()

    while rotate < 0: rotate += 360
    while rotate >= 360: rotate -= 360
    if rotate % 90 != 0: rotate = 0

    for p in range(page_from, page_to + page_delta, page_delta):
        page = document.load_page(p)
        rect = page.bound_page()
        dev, resources, contents = document_out.page_write(rect)
        page.run_page(dev, mupdf.Matrix(), mupdf.Cookie())
        pdf_obj = document_out.add_page(rect, rotate, resources, contents)
        document_out.insert_page(-1, pdf_obj)

    write_options = mupdf.PdfWriteOptions()
    write_options.do_garbage         = 4;
    write_options.do_compress        = 1;
    write_options.do_compress_images = 1;
    write_options.do_compress_fonts  = 1;
    write_options.do_sanitize        = 1;
    write_options.do_incremental     = 0;
    write_options.do_ascii           = 0;
    write_options.do_decompress      = 0;
    write_options.do_linear          = 0;
    write_options.do_clean           = 1;
    write_options.do_pretty          = 0;
    buffer_ = mupdf.Buffer(8192)
    output = mupdf.Output(buffer_)
    document_out.write_document(output, write_options)
    size, data = buffer_.buffer_extract_raw()
    print(f'buffer_.buffer_extract() returned: {size, data}')
    return data, size

data, size = convert_to_pdf(document)

stream = mupdf.Stream(data, size)
document2 = mupdf.Document("pdf", stream)

# Fixme: we don't yet copy the TOC.

# Fixme: we don't yet copy links.

document3 = document2.pdf_specifics()
opts = mupdf.PdfWriteOptions()
opts.do_garbage = 4
opts.do_compress = 1
document3.save_document(path+ '.pdf', opts)
