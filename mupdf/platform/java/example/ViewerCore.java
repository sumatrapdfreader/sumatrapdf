// Copyright (C) 2004-2025 Artifex Software, Inc.
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

package example;

import com.artifex.mupdf.fitz.*;

import java.io.File;
import java.util.Vector;

public class ViewerCore {

	protected Worker worker;
	protected Callback callback;

	protected String documentPath;
	protected String acceleratorPath;

	protected Document doc;
	protected Page page;

	protected Location currentPage;
	protected Location searchHitPage;
	protected String searchNeedle;

	protected OutlineItem[] outline;

	protected boolean cancelSearch;

	public ViewerCore(Worker worker, Callback callback, String documentPath) {
		this.worker = worker;
		this.callback = callback;
		this.documentPath = documentPath;
	}

	public void openDocument(final OnException onException) {
		worker.add(new Worker.Task() {
			Document doc = null;
			String acceleratorPath = null;
			boolean needsPassword = false;
			protected String getAcceleratorPath(String documentPath) {
				try {
					String accelerator = new File(documentPath).
						getCanonicalFile().
						getPath().
						replace(File.separatorChar, '%').
						replace('\\', '%').
						replace('/', '%').
						replace(':', '%') + ".accel";
					String tmpdir = System.getProperty("java.io.tmpdir");
					return new File(tmpdir, accelerator).getCanonicalFile().getPath();
				} catch (Exception e) {
					return null;
				}
			}
			protected boolean acceleratorValid(String documentPath, String acceleratorPath) {
				try {
					long documentModified = new File(documentPath).lastModified();
					long acceleratorModified = new File(acceleratorPath).lastModified();
					return acceleratorModified != 0 && acceleratorModified > documentModified;
				} catch (Exception e) {
					return false;
				}
			}
			public void work() {
				String acceleratorPath = getAcceleratorPath(documentPath);
				if (!acceleratorValid(documentPath, acceleratorPath))
					acceleratorPath = null;
				if (acceleratorPath != null)
					doc = Document.openDocument(documentPath, acceleratorPath);
				else
					doc = Document.openDocument(documentPath);
				needsPassword = doc.needsPassword();
			}
			public void run() {
				ViewerCore.this.doc = doc;
				ViewerCore.this.acceleratorPath = acceleratorPath;
				if (needsPassword) {
					String password = callback.askPassword();
					if (password != null)
						checkPassword(password, onException);
				}
				else
					loadDocument(onException);
			}
			public void exception(Throwable t) {
				ViewerCore.this.acceleratorPath = null;
				ViewerCore.this.doc = null;
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void reloadDocument(final OnException onException) {
		acceleratorPath = null;
		doc = null;
		page = null;
		searchHitPage = null;
		searchNeedle = null;
		outline = null;
		cancelSearch = false;
		openDocument(onException);
	}

	protected void checkPassword(final String password, final OnException onException) {
		worker.add(new Worker.Task() {
			boolean passwordOK = false;
			public void work() {
				passwordOK = doc.authenticatePassword(password);
			}
			public void run() {
				if (!passwordOK) {
					String password = callback.askPassword();
					if (password != null)
						checkPassword(password, onException);
				}
				else
					loadDocument(onException);
			}
			public void exception(Throwable t) {
				ViewerCore.this.documentPath = null;
				ViewerCore.this.acceleratorPath = null;
				ViewerCore.this.doc = null;
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void refreshMetadata(final OnException onException) {
		worker.add(new Worker.Task() {
			String title = "";
			String author = "";
			String format = "";
			String encryption = "";
			boolean print = false;
			boolean edit = false;
			boolean copy = false;
			boolean annotate = false;
			boolean form = false;
			boolean accessibility = false;
			boolean assemble = false;
			boolean printHq = false;
			boolean isPDF = false;
			boolean reflowable = false;
			boolean linear = false;
			int updates = 0;
			int history = 0;
			public void work() {
				title = doc.getMetaData(Document.META_INFO_TITLE);
				author = doc.getMetaData(Document.META_INFO_AUTHOR);
				format = doc.getMetaData(Document.META_FORMAT);
				encryption = doc.getMetaData(Document.META_ENCRYPTION);
				print = doc.hasPermission(Document.PERMISSION_PRINT);
				copy = doc.hasPermission(Document.PERMISSION_COPY);
				edit = doc.hasPermission(Document.PERMISSION_EDIT);
				annotate = doc.hasPermission(Document.PERMISSION_ANNOTATE);
				form = doc.hasPermission(Document.PERMISSION_FORM);
				accessibility = doc.hasPermission(Document.PERMISSION_ACCESSIBILITY);
				assemble = doc.hasPermission(Document.PERMISSION_ASSEMBLE);
				printHq = doc.hasPermission(Document.PERMISSION_PRINT_HQ);
				reflowable = doc.isReflowable();
				isPDF = doc.isPDF();
				if (isPDF) {
					PDFDocument pdf = (PDFDocument) doc;
					linear = pdf.wasLinearized();
					updates = pdf.countVersions();
					history = pdf.validateChangeHistory();
				}
			}
			public void run() {
				callback.onMetadataChange(title, author, format, encryption);
				callback.onPermissionsChange(print, copy, edit, annotate, form, accessibility, assemble, printHq);
				callback.onReflowableChange(reflowable);
				if (isPDF) {
					callback.onLinearizedChange(linear);
					callback.onUpdatesChange(updates, history);
				}
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	protected void loadDocument(final OnException onException) {
		worker.add(new Worker.Task() {
			int chapterCount = 0;
			int pageCount = 0;
			public void work() {
				chapterCount = doc.countChapters();
				pageCount = doc.countPages();
				if (acceleratorPath != null)
					doc.saveAccelerator(acceleratorPath);
			}
			public void run() {
				callback.onChapterCountChange(chapterCount);
				callback.onPageCountChange(pageCount);
				loadOutline(onException);
				refreshMetadata(onException);
				if (currentPage != null)
					gotoLocation(currentPage, onException);
				else
					gotoFirstPage(onException);
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void relayoutDocument(final int width, final int height, final int em, final OnException onException) {
		worker.add(new Worker.Task() {
			int chapterCount = 0;
			int pageCount = 0;
			Location newPage;
			public void work() {
				long mark = doc.makeBookmark(ViewerCore.this.currentPage);
				doc.layout(width, height, em);
				chapterCount = doc.countChapters();
				pageCount = doc.countPages();
				newPage = doc.findBookmark(mark);
			}
			public void run() {
				callback.onLayoutChange(width, height, em);
				callback.onChapterCountChange(chapterCount);
				callback.onPageCountChange(pageCount);
				gotoLocation(newPage, onException);
				loadOutline(onException);
			}
			public void exception(Throwable t) {
				callback.onLayoutChange(450, 600, 12);
				callback.onChapterCountChange(0);
				callback.onPageCountChange(0);
				gotoLocation(null, onException);
				if (onException != null)
					onException.run(t);
			}
		});
	}

	protected void loadOutline(final OnException onException) {
		worker.add(new Worker.Task() {
			OutlineItem[] outline = null;
			protected void flattenOutline(Outline[] rawOutline, String indent, Vector<OutlineItem> v) {
				for (Outline node : rawOutline) {
					if (node.title != null) {
						Location loc = doc.resolveLink(node);
						String title = indent + node.title;
						v.add(new OutlineItem(title, node.uri, loc));
					}
					if (node.down != null)
						flattenOutline(node.down, indent + "    ", v);
				}
			}
			public void work() {
				Outline[] rawOutline = doc.loadOutline();
				if (rawOutline != null) {
					Vector<OutlineItem> v = new Vector<OutlineItem>();
					flattenOutline(rawOutline, "", v);

					outline = new OutlineItem[v.size()];
					v.toArray(outline);
				}
			}
			public void run() {
				ViewerCore.this.outline = outline;
				callback.onOutlineChange(outline);
			}
			public void exception(Throwable t) {
				callback.onOutlineChange(null);
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void gotoLocation(final Location location, final OnException onException) {
		worker.add(new Worker.Task() {
			int itemIndex = -1;
			Page page = null;
			Rect bbox = null;
			int chapterNumber = 0;
			int pageNumber = 0;
			public void work() {
				if (location == null)
					return;

				chapterNumber = location.chapter;
				pageNumber = doc.pageNumberFromLocation(location);

				if (outline != null) {
					for (int i = 0; i < outline.length; ++i) {
						Location loc = outline[i].location;
						int outlinePageNumber = doc.pageNumberFromLocation(loc);
						if (outlinePageNumber <= pageNumber)
							itemIndex = i;
					}
				}

				page = doc.loadPage(location);
				bbox = page.getBounds();
			}
			public void run() {
				ViewerCore.this.page = page;
				ViewerCore.this.currentPage = location;
				if (currentPage == null || !currentPage.equals(searchHitPage))
					ViewerCore.this.searchHitPage = null;
				callback.onOutlineItemChange(itemIndex);
				callback.onPageChange(location, chapterNumber, pageNumber, bbox);
			}
			public void exception(Throwable t) {
				ViewerCore.this.page = null;
				ViewerCore.this.currentPage = null;
				ViewerCore.this.searchHitPage = null;
				callback.onOutlineItemChange(-1);
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void flipPages(final int flips, final OnException onException) {
		worker.add(new Worker.Task() {
			Location location = currentPage;
			public void work() {
				int page = doc.pageNumberFromLocation(location);
				int newPage = page + flips;
				location = doc.locationFromPageNumber(newPage);
			}
			public void run() {
				gotoLocation(location, onException);
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void gotoFirstPage(final OnException onException) {
		worker.add(new Worker.Task() {
			Location location;
			public void work() {
				location = doc.locationFromPageNumber(0);
			}
			public void run() {
				gotoLocation(location, onException);
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void gotoLastPage(final OnException onException) {
		worker.add(new Worker.Task() {
			Location location;
			public void work() {
				location = doc.lastPage();
			}
			public void run() {
				gotoLocation(location, onException);
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void gotoPage(final int pageNumber, final OnException onException) {
		worker.add(new Worker.Task() {
			Location location;
			public void work() {
				location = doc.locationFromPageNumber(pageNumber);
			}
			public void run() {
				gotoLocation(location, onException);
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void renderPage(final Matrix ctm, final Rect bbox, final boolean icc, final int antialias, final boolean invert, final boolean tint, final int tintBlack, final int tintWhite, final Cookie cookie, final OnException onException) {
		worker.add(new Worker.Task() {
			Pixmap pixmap = null;
			Rect[] links = null;
			String[] linkURIs = null;
			Quad[][] hits = null;
			public void work() {
				Link[] pageLinks = page.getLinks();
				if (pageLinks == null)
				{
					links = new Rect[0];
					linkURIs = new String[0];
				}
				else
				{
					int i = 0;
					links = new Rect[pageLinks.length];
					linkURIs = new String[pageLinks.length];
					for (Link link: pageLinks)
					{
						links[i] = link.getBounds().transform(ctm);
						linkURIs[i] = link.getURI();
						i++;
					}
				}

				if (currentPage.equals(searchHitPage))
					hits = page.search(searchNeedle);
				if (hits == null)
					hits = new Quad[0][];
				for (Quad[] hit : hits)
					for (Quad q : hit)
						q.transform(ctm);

				pixmap = new Pixmap(ColorSpace.DeviceBGR, bbox, true);
				pixmap.clear(255);

				if (icc)
					Context.enableICC();
				else
					Context.disableICC();
				Context.setAntiAliasLevel(antialias);

				DrawDevice dev = new DrawDevice(pixmap);
				page.run(dev, ctm, cookie);
				dev.close();
				dev.destroy();

				if (invert) {
					pixmap.invertLuminance();
					pixmap.gamma(1 / 1.4f);
				}

				if (tint)
					pixmap.tint(tintBlack, tintWhite);
			}
			public void run() {
				callback.onPageContentsChange(pixmap, links, linkURIs, hits);
			}
			public void exception(Throwable t) {
				callback.onPageContentsChange(null, new Rect[0], new String[0], new Quad[0][]);
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void search(final String needle, final int direction, final OnException onException) {
		cancelSearch = false;
		if (doc == null)
			return;
		worker.add(new Worker.Task() {
			Location startPage, finalPage;
			public void work() {
				if (!currentPage.equals(searchHitPage))
					startPage = currentPage;
				else if (direction >= 0)
					startPage = doc.nextPage(currentPage);
				else
					startPage = doc.previousPage(currentPage);

				if (direction >= 0)
					finalPage = doc.lastPage();
				else
					finalPage = doc.locationFromPageNumber(0);
			}
			public void run() {
				callback.onSearchStart(startPage, finalPage, direction, needle);
				ViewerCore.this.searchNeedle = needle;
				runSearch(startPage, finalPage, direction, needle, onException);
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void cancelSearch(final OnException onException) {
		cancelSearch = true;
	}

	public void runSearch(final Location startPage, final Location finalPage, final int direction, final String needle, final OnException onException) {
		worker.add(new Worker.Task() {
			Location searchPage = startPage;
			Location hitPage = null;
			Quad[][] hits = new Quad[0][];
			boolean done = false;
			boolean cancelled = false;
			public void work() {
				long executionUntil = System.currentTimeMillis() + 100;
				do {
					if (cancelSearch) {
						cancelled = true;
						continue;
					}

					Page page = doc.loadPage(searchPage);
					hits = page.search(needle);
					if (hits != null && hits.length > 0) {
						hitPage = searchPage;
						done = true;
						continue;
					}
					page.destroy();

					if (searchPage.equals(finalPage)) {
						done = true;
					}
					else if (direction >= 0)
						searchPage = doc.nextPage(searchPage);
					else
						searchPage = doc.previousPage(searchPage);
				} while (!cancelled && !done && System.currentTimeMillis() < executionUntil);
			}
			public void run() {
				if (cancelled) {
					ViewerCore.this.searchNeedle = null;
					ViewerCore.this.searchHitPage = null;
					callback.onSearchCancelled();
				} else if (done) {
					ViewerCore.this.searchHitPage = hitPage;
					callback.onSearchStop(needle, hitPage);
				} else {
					callback.onSearchPage(searchPage, needle);
					worker.add(this);
				}
			}
			public void exception(Throwable t) {
				callback.onSearchStop(needle, null);
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void save(final String path, final String options, final DocumentWriter.OCRListener ocrListener, final OnException onException) {
		worker.add(new Worker.Task() {
			public void work() {
				PDFDocument pdf = (PDFDocument) doc;
				pdf.save(path, options);
			}
			public void run() {
				callback.onSaveComplete();
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public void save(final SeekableOutputStream stream, final String options, final DocumentWriter.OCRListener ocrListener, final OnException onException) {
		worker.add(new Worker.Task() {
			public void work() {
				PDFDocument pdf = (PDFDocument) doc;
				DocumentWriter wri = null;
				int pageCount = doc.countPages();
				try {
					wri = new DocumentWriter(stream, "ocr", "");
					wri.addOCRListener(ocrListener);

					for (int i = 0; i < pageCount; i++)
					{
						Page page = pdf.loadPage(i);
						Rect bounds = page.getBounds();
						Device dev = wri.beginPage(bounds);
						page.run(dev, new Matrix());
						wri.endPage();
					}

					wri.close();
				} finally {
					if (wri != null) wri.destroy();
				}
			}
			public void run() {
				callback.onSaveComplete();
			}
			public void exception(Throwable t) {
				if (onException != null)
					onException.run(t);
			}
		});
	}

	public interface Callback {
		public String askPassword();

		public void onChapterCountChange(int chapters);
		public void onLayoutChange(int width, int height, int em);
		public void onLinearizedChange(boolean linearized);
		public void onMetadataChange(String title, String author, String format, String encryption);
		public void onOutlineChange(OutlineItem[] outline);
		public void onOutlineItemChange(int index);
		public void onPageChange(Location page, int chapterNumber, int pageNumber, Rect bbox);
		public void onPageContentsChange(Pixmap pixmap, Rect[] links, String[] linkURIs, Quad[][] searchHits);
		public void onPageCountChange(int pages);
		public void onPermissionsChange(boolean print, boolean copy, boolean edit, boolean annotate, boolean form, boolean accessibility, boolean assemble, boolean printHq);
		public void onReflowableChange(boolean reflowable);
		public void onSaveComplete();
		public void onSearchCancelled();
		public void onSearchPage(Location page, String needle);
		public void onSearchStart(Location startPage, Location finalPage, int direction, String needle);
		public void onSearchStop(String needle, Location page);
		public void onUpdatesChange(int update, int history);
	}

	public interface OnException {
		public void run(Throwable t);
	}

	protected static class OutlineItem {
		protected String title;
		protected String uri;
		protected Location location;
		public OutlineItem(String title, String uri, Location location) {
			this.title = title;
			this.uri = uri;
			this.location = location;
		}
		public String toString() {
			return title;
		}
	}
}
