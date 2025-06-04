.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. meta::
    :description: MuPDF documentation
    :keywords: MuPDF, pdf, epub

Progressive Loading
========================================

The idea of progressive loading is that as you download a PDF file into a browser, you can display the pages as they become available.

This relies on the caller of MuPDF having fine control over the HTTP fetch and on the server supporting byte-range fetches.

MuPDF has made various extensions to its mechanisms for handling progressive loading.

Streams
----------------------------------------

At its lowest level MuPDF reads file data from a `fz_stream`, using the `fz_open_document_with_stream` call. (`fz_open_document` is implemented by calling this). We have extended the `fz_stream` slightly, giving the system a way to ask for meta information (or perform meta operations) on a stream.

Using this mechanism MuPDF can query:

- whether a stream is progressive or not (i.e. whether the entire stream is accessible immediately).
- what the length of a stream should ultimately be (which an HTTP fetcher should know from the Content-Length header).

When data is pulled from a progressive stream, if we attempt to read data that is not currently available, the stream should throw a `FZ_ERROR_TRYLATER` error. This particular error code will be interpreted by the caller as an indication that it should retry the parsing of the current objects at a later time.

When a MuPDF call is made on a progressive stream, such as `fz_open_document_with_stream`, or `fz_load_page`, the caller should be prepared to handle a `FZ_ERROR_TRYLATER` error as meaning that more data is required before it can continue. No indication is directly given as to exactly how much more data is required, but as the caller will be implementing the progressive `fz_stream` that it has passed into MuPDF to start with, it can reasonably be expected to figure out an estimate for itself.

Cookies
----------------------------------------

.. note::
        This is not related to an HTTP cookie.

Once a page has been loaded, if its contents are to be 'run' as normal (using e.g. `fz_run_page`) any error (such as failing to read a font, or an image, or even a content stream belonging to the page) will result in a rendering that aborts with an `FZ_ERROR_TRYLATER` error. The caller can catch this and display a placeholder instead.

If each pages data was entirely self-contained and sent in sequence this would perhaps be acceptable, with each page appearing one after the other. Unfortunately, the linearization procedure as laid down by Adobe does **not** do this: objects shared between multiple pages (other than the first) are not sent with the pages themselves, but rather **after** all the pages have been sent.

This means that a document that has a title page, then contents that share a font used on pages 2 onwards, will not be able to correctly display page 2 until after the font has arrived in the file, which will not be until all the page data has been sent.

To mitigate against this, MuPDF provides a way whereby callers can indicate that they are prepared to accept an 'incomplete' rendering of the file (perhaps with missing images, or with substitute fonts).

Callers prepared to tolerate such renderings should set the `incomplete_ok` flag in the cookie, then call `fz_run_page` etc. as normal. If a `FZ_ERROR_TRYLATER` error is thrown at any point during the page rendering, the error will be swallowed, the 'incomplete' field in the cookie will become non-zero and rendering will continue. When control returns to the caller the caller can check the value of the 'incomplete' field and know that the rendering it received is not authoritative.

Using HTTP
----------------------------------------

If the caller has control over the HTTP fetch, then it is possible to use byte range requests to fetch the document 'out of order'. This enables non-linearized files to be progressively displayed as they download, and fetches complete renderings of pages earlier than would otherwise be the case. This process requires no changes within MuPDF itself, but rather in the way the progressive stream learns from the attempts MuPDF makes to fetch data.

Consider, for example, an attempt to fetch a hypothetical file from a server.

- The initial HTTP request for the document is sent with a "Range:" header to pull down the first (say) 4k of the file.

- As soon as we get the header in from this initial request, we can respond to meta stream operations to give the length, and whether byte requests are accepted.

- If the header indicates that byte ranges are acceptable the stream proceeds to go into a loop fetching chunks of the file at a time (not necessarily in-order). Otherwise the server will ignore the Range: header, and just serve the whole file.

- If the header indicates a content-length, the stream returns that.

- MuPDF can then decide how to proceed based upon these flags.

On a byte request capable stream:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- MuPDF  will immediately seek to the end of the file to attempt to read the trailer. This will fail with a `FZ_ERROR_TRYLATER` due to the data not being here yet, but the stream code should remember that this data is required and it should be prioritized in the background fetch process.

- Repeated attempts to open the stream should eventually succeed therefore. As MuPDF jumps through the file trying to read first the xrefs, then the page tree objects, then the page contents themselves etc., the background fetching process will be driven by the attempts to read the file in the foreground.

Typically therefore the opening of a non-linearized file will be slower than a linearized one, as the xrefs/page trees for a non-linear file can be 20%+ of the file data. Once past this initial point however, pages and data can be pulled from the file almost as fast as with a linearized file.

On a non-byte request capable stream:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- MuPDF will immediately seek to the end of the file to attempt to read the trailer. This will fail with a `FZ_ERROR_TRYLATER` due to the data not being here yet. Subsequent retries will continue to fail until the whole file has arrived, whereupon the whole file will be instantly available.

This is the worst case situation - nothing at all can be displayed until the entire file has downloaded.

A typical structure for a fetcher process (see `curl-stream.c`, `mupdf-curl` in `platform/win32/mupdf-curl.vcxproj`) as an example) might therefore look like this:

- We consider the file as an (initially empty) buffer which we are filling by making requests. In order to ensure that we make maximum use of our download link, we ensure that whenever one request finishes, we immediately launch another. Further, to avoid the overheads for the request/response headers being too large, we may want to divide the file into 'chunks', perhaps 4 or 32k in size.

- We can then have a receiver process that sits there in a loop requesting chunks to fill this buffer. In the absence of any other impetus the receiver should request the next 'chunk' of data from the file that it does not yet have, following the last fill point. Initially we start the fill point at the beginning of the file, but this will move around based on the requests made of the progressive stream.

- Whenever MuPDF attempts to read from the stream, we check to see if we have data for this area of the file already. If we do, we can return it. If not, we remember this as the next "fill point" for our receiver process and throw a `FZ_ERROR_TRYLATER` error.
