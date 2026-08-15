# Progressive Loading

## What is progressive loading?

The idea of progressive loading is that as you download a PDF file into a browser, you can display the pages as they appear.

MuPDF can make use of 2 different mechanisms to achieve this. The first relies on the file being "linearized", the second relies on the caller of MuPDF having fine control over the http fetch and on the server supporting byte-range fetches.

For optimum performance a file should be both linearized and be available over a byte-range supporting link, but benefits can still be had with either one of these alone.

## Progressive download using "linearized" files

Adobe defines "linearized" PDFs as being ones that have both a specific layout of objects and a small amount of extra information to help avoid seeking within a file. The stated aim is to deliver the first page of a document in advance of the whole document downloading, whereupon subsequent pages will become available. Adobe also refers to these as "Optimized for fast web view" or "Web Optimized".

In fact, the standard outlines (poorly) a mechanism by which 'hints' can be included that enable the subsequent pages to be found within the file too. Unfortunately this is very poorly supported with many tools, and so the hints have to be treated with suspicion.

MuPDF will attempt to use hints if they are available, but will also use a linear search of the file to discover pages if not. This means that the first page will be displayed quickly, and then subsequent ones will appear with 'incomplete' renderings that improve over time as more and more resources are gradually delivered.

Essentially the file starts with a slightly modified header, and the first object in the file is a special one (the linearization object) that:

- indicates that the file is linearized.
- gives some useful information (like the number of pages in the file).

This object is then followed by all the objects required for the first page, then the "hint stream", then sets of object for each subsequent page in turn, then shared objects required for those pages, then various other random things.

> While page 1 is sent with all the objects that it uses, shared or otherwise, subsequent pages do not get shared resources until after all the unshared page objects have been sent.

## The Hint Stream

Adobe intended "Hint Stream" to be useful to facilitate the display of subsequent pages, but it has never used it. Consequently you can't trust people to write it properly - indeed Adobe outputs something that doesn't quite conform to the specification.

Consequently very few people actually use it. MuPDF will use it after sanity checking the values, and should cope with illegal/incorrect values.

## So how does MuPDF handle progressive loading?

MuPDF has made various extensions to its mechanisms for handling progressive loading.

### Progressive streams

At its lowest level MuPDF reads file data from a `fz_stream`, using the `fz_open_document_with_stream` call. (`fz_open_document` is implemented by calling this). We have extended the `fz_stream` slightly, giving the system a way to ask for meta information (or perform meta operations) on a stream.

Using this mechanism MuPDF can query:

- whether a stream is progressive or not (i.e. whether the entire stream is accessible immediately).
- what the length of a stream should ultimately be (which an http fetcher should know from the Content-Length header).

With this information MuPDF can decide whether to use its normal object reading code, or whether to make use of a linearized object. Knowing the length enables us to check with the length value given in the linearized object - if these differ, the assumption is that an incremental save has taken place, thus the file is no longer linearized.

When data is pulled from a progressive stream, if we attempt to read data that is not currently available, the stream should throw a `FZ_ERROR_TRYLATER` error. This particular error code will be interpreted by the caller as an indication that it should retry the parsing of the current objects at a later time.

When a MuPDF call is made on a progressive stream, such as `fz_open_document_with_stream`, or `fz_load_page`, the caller should be prepared to handle a `FZ_ERROR_TRYLATER` error as meaning that more data is required before it can continue. No indication is directly given as to exactly how much more data is required, but as the caller will be implementing the progressive `fz_stream` that it has passed into MuPDF to start with, it can reasonably be expected to figure out an estimate for itself.

### Progress Cookie

Once a page has been loaded, if its contents are to be 'run' as normal (using e.g. `fz_run_page`) any error (such as failing to read a font, or an image, or even a content stream belonging to the page) will result in a rendering that aborts with an `FZ_ERROR_TRYLATER` error. The caller can catch this and display a placeholder instead.

If each pages data was entirely self-contained and sent in sequence this would perhaps be acceptable, with each page appearing one after the other. Unfortunately, the linearization procedure as laid down by Adobe does **not** do this: objects shared between multiple pages (other than the first) are not sent with the pages themselves, but rather **after** all the pages have been sent.

This means that a document that has a title page, then contents that share a font used on pages 2 onwards, will not be able to correctly display page 2 until after the font has arrived in the file, which will not be until all the page data has been sent.

To mitigate against this, MuPDF provides a way whereby callers can indicate that they are prepared to accept an 'incomplete' rendering of the file (perhaps with missing images, or with substitute fonts).

Callers prepared to tolerate such renderings should set the `incomplete_ok` flag in the cookie, then call `fz_run_page` etc. as normal. If a `FZ_ERROR_TRYLATER` error is thrown at any point during the page rendering, the error will be swallowed, the 'incomplete' field in the cookie will become non-zero and rendering will continue. When control returns to the caller the caller can check the value of the 'incomplete' field and know that the rendering it received is not authoritative.

## Progressive loading using byte range requests

If the caller has control over the http fetch, then it is possible to use byte range requests to fetch the document 'out of order'. This enables non-linearized files to be progressively displayed as they download, and fetches complete renderings of pages earlier than would otherwise be the case. This process requires no changes within MuPDF itself, but rather in the way the progressive stream learns from the attempts MuPDF makes to fetch data.

Consider, for example, an attempt to fetch a hypothetical file from a server.

- The initial http request for the document is sent with a "Range:" header to pull down the first (say) 4k of the file.

- As soon as we get the header in from this initial request, we can respond to meta stream operations to give the length, and whether byte requests are accepted.

- If the header indicates that byte ranges are acceptable the stream proceeds to go into a loop fetching chunks of the file at a time (not necessarily in-order). Otherwise the server will ignore the Range: header, and just serve the whole file.

- If the header indicates a content-length, the stream returns that.

- MuPDF can then decide how to proceed based upon these flags and whether the file is linearized or not. (If the file contains a linearized object, and the content length matches, then the file is considered to be linear, otherwise it is not).

### If the file is linear:

- We proceed to read objects out of the file as it downloads. This will provide us the first page and all its resources. It will also enable us to read the hint streams (if present).

- Once we have read the hint streams, we unpack (and sanity check) them to give us a map of where in the file each object is predicted to live, and which objects are required for each page. If any of these values are out of range, we treat the file as if there were no hint streams.

- If we have hints, any attempt to load a subsequent page will cause MuPDF to attempt to read exactly the objects required. This will cause a sequence of seeks in the `fz_stream` followed by reads. If the stream does not have the data to satisfy that request yet, the stream code should remember the location that was fetched (and fetch that block in the background so that future retries will succeed) and should raise an `FZ_ERROR_TRYLATER` error.

> Typically therefore when we jump to a page in a linear file on a byte request capable link, we will quickly see a rough rendering, which will improve fairly fast as images and fonts arrive.

- Regardless of whether we have hints or byte requests, on every `fz_load_page` call MuPDF will attempt to process more of the stream (that is assumed to be being downloaded in the background). As linearized files are guaranteed to have pages in order, pages will gradually become available. In the absence of byte requests and hints however, we have no way of getting resources early, so the renderings for these pages will remain incomplete until much more of the file has arrived.

> Typically therefore when we jump to a page in a linear file on a non byte request capable link, we will see a rough rendering for that page as soon as data arrives for it (which will typically take much longer than would be the case with byte range capable downloads), and that will improve much more slowly as images and fonts may not appear until almost the whole file has arrived.

- When the whole file has arrived, then we will attempt to read the outlines for the file.

### For a non-linearized PDF on a byte request capable stream:

- MuPDF will immediately seek to the end of the file to attempt to read the trailer. This will fail with a `FZ_ERROR_TRYLATER` due to the data not being here yet, but the stream code should remember that this data is required and it should be prioritized in the background fetch process.

- Repeated attempts to open the stream should eventually succeed therefore. As MuPDF jumps through the file trying to read first the xrefs, then the page tree objects, then the page contents themselves etc., the background fetching process will be driven by the attempts to read the file in the foreground.

> Typically therefore the opening of a non-linearized file will be slower than a linearized one, as the xrefs/page trees for a non-linear file can be 20%+ of the file data. Once past this initial point however, pages and data can be pulled from the file almost as fast as with a linearized file.

### For a non-linearized PDF on a non-byte request capable stream:

- MuPDF will immediately seek to the end of the file to attempt to read the trailer. This will fail with a `FZ_ERROR_TRYLATER` due to the data not being here yet. Subsequent retries will continue to fail until the whole file has arrived, whereupon the whole file will be instantly available.

> This is the worst case situation - nothing at all can be displayed until the entire file has downloaded.

A typical structure for a fetcher process (see `curl-stream.c`, `mupdf-curl` in `platform/win32/mupdf-curl.vcxproj`) as an example) might therefore look like this:

- We consider the file as an (initially empty) buffer which we are filling by making requests. In order to ensure that we make maximum use of our download link, we ensure that whenever one request finishes, we immediately launch another. Further, to avoid the overheads for the request/response headers being too large, we may want to divide the file into 'chunks', perhaps 4 or 32k in size.

- We can then have a receiver process that sits there in a loop requesting chunks to fill this buffer. In the absence of any other impetus the receiver should request the next 'chunk' of data from the file that it does not yet have, following the last fill point. Initially we start the fill point at the beginning of the file, but this will move around based on the requests made of the progressive stream.

- Whenever MuPDF attempts to read from the stream, we check to see if we have data for this area of the file already. If we do, we can return it. If not, we remember this as the next "fill point" for our receiver process and throw a `FZ_ERROR_TRYLATER` error.
