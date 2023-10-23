// Copyright (C) 2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

/**
 * Multi-threaded rendering of all pages in a document to PNG images.
 *
 * First look at Example.java and make sure you understand it.
 *
 * MuPDF can also be used in a more complex way; where the MuPDF
 * library is called concurrently from multiple threads within a
 * single application. The MuPDF JNI library internally uses a set of
 * mutexes (critical section objects or pthreads depending on
 * platform) to handle locking around operations that would conflict
 * if performed concurrently in multiple threads.
 *
 * The following simple rules should be followed to ensure that
 * multi-threaded operations run smoothly:
 *
 * * The Document can only be accessed by one thread at a time, but
 *   once DisplayLists are created from that Document, multiple
 *   threads can operate on those.
 *
 * * Each Device may only be accessed by one thread at a time.
 *
 * This example will create one main thread for reading pages from the
 * document, and one thread per page for rendering. After rendering
 * the main thread will wait for each rendering thread to complete
 * before writing that thread's rendered image to a PNG image. There
 * is nothing in MuPDF requiring a rendering thread to only render a
 * single page, this is just a design decision taken for this example.
 *
 * To build this example in a source tree:
 * make -C platform/java examples
 *
 * To render all page from a document and output PNGs, run:
 * java -classpath build/java/debug -Djava.library.path=build/java/debug \
 *         example.MultiThreaded document.pdf
 *
 * Caution! As all pages are rendered simultaneously, please choose a
 * file with just a few pages to avoid stressing your machine too
 * much. Also you may run in to a limitation on the number of threads
 * depending on your environment.
 */

package example;

/* Import all MuPDF java classes. */
import com.artifex.mupdf.fitz.*;

class MultiThreaded
{
	public static void main(String args[])
	{
		/* Parse arguments. */
		if (args.length < 1)
		{
			System.err.println("usage: MultiThreaded input-file");
			System.err.println("\tinput-file: path of PDF, XPS, CBZ or EPUB document to open");
			return;
		}

		String filename = args[0];

		/* Open the document on the main thread.
		 * You may not reference doc in any other thread.
		 */
		Document doc;
		try {
			doc = Document.openDocument(filename);
		} catch (RuntimeException ex) {
			System.err.println("cannot open document: " + ex.getMessage());
			return;
		}

		/* Count the pages on the main thread. */
		int pageCount;
		try {
			pageCount = doc.countPages();
		} catch (RuntimeException ex) {
			System.err.println("cannot count document pages: " + ex.getMessage());
			return;
		}

		/* Create a thread, an output pixmap and an exception placeholder per page. */
		final Thread[] threads = new Thread[pageCount];
		final Pixmap[] pixmaps = new Pixmap[pageCount];
		final Exception[] exceptions = new Exception[pageCount];

		for (int i = 0; i < pageCount; ++i)
		{
			final int pageNumber = i;
			try {
				/* Load page and convert it to a display list on
				 * the main thread. Note that this cannot be done
				 * in worker threads since doc should only be used
				 * by the main thread.
				 */
				Page page = doc.loadPage(pageNumber);

				/* Determine the bounding box for the page */
				final Rect bounds = page.getBounds();

				/* Convert the page into a display list. The display
				 * list can be used by any other thread as it is not
				 * bound to doc.
				 */
				final DisplayList displayList = page.toDisplayList();

				/* Pass display list, page size and destination pixmap to each
				 * thread for rendering. Also pass page number for debug printing.
				 * Everything inside run() takes place in each worker thread.
				 */
				threads[pageNumber] = new Thread() {
					public void run() {
						try {
							System.out.println(pageNumber + ": creating pixmap");

							/* Create a white destination pixmap with correct dimensions. */
							pixmaps[pageNumber] = new Pixmap(ColorSpace.DeviceRGB, bounds);
							pixmaps[pageNumber].clear(0xff);

							System.out.println(pageNumber + ": rendering display list to pixmap");

							/* Run the display list through a DrawDevice which
							 * will render the requested area of the page to the
							 * given pixmap.
							 */
							DrawDevice dev = new DrawDevice(pixmaps[pageNumber]);
							displayList.run(dev, Matrix.Identity(), bounds, null);
							dev.close();

						} catch (RuntimeException ex) {
							pixmaps[pageNumber] = null;
							exceptions[pageNumber] = ex;
						}
					}
				};

				threads[pageNumber].start();

			} catch (RuntimeException ex) {
				System.err.println(pageNumber + ": cannot load page, skipping render: " + ex.getMessage());
				exceptions[pageNumber] = ex;
			}
		}

		/* Wait for threads to finish in reverse order. */
		System.out.println("joining " + pageCount + " threads");
		for (int i = 0; i < pageCount; ++i)
		{
			if (threads[i] == null) {
				System.err.println(i + ": skipping save, page loading failed: " + exceptions[i].toString());
				continue;
			}

			try {
				threads[i].join();
			} catch (InterruptedException ex) {
				System.err.println(i + ": interrupted while waiting for rendering result, skipping all remaining pages: " + ex.getMessage());
				break;
			}

			if (pixmaps[i] == null) {
				System.err.println(i + ": skipping save, page rendering failed: " + exceptions[i].toString());
				continue;
			}

			/* Save destination pixmap from each thread to a PNG. */
			String pngfilename = String.format("out-%04d.png", i);
			System.out.println(i + ": saving rendered pixmap as " + pngfilename);
			pixmaps[i].saveAsPNG(pngfilename);
		}

		System.out.println("finally!");
	}
}
