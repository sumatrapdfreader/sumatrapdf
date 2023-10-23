// Copyright (C) 2004-2022 Artifex Software, Inc.
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

import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

import java.io.File;
import java.io.FilenameFilter;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.reflect.Field;
import java.util.Vector;

public class Viewer extends Frame implements WindowListener, ActionListener, ItemListener, KeyListener, MouseWheelListener, Context.Log, ViewerCore.Callback
{
	protected Worker worker;
	protected ViewerCore doc;
	protected Location location;
	protected int chapterNumber;
	protected int pageNumber;
	protected ViewerCore.OutlineItem[] outline;

	protected int layoutWidth = 450;
	protected int layoutHeight = 600;
	protected int layoutEm = 12;

	protected String title;
	protected String author;
	protected String format;
	protected String encryption;
	protected boolean print;
	protected boolean copy;
	protected boolean edit;
	protected boolean annotate;
	protected boolean form;
	protected boolean accessibility;
	protected boolean assemble;
	protected boolean printHq;
	protected boolean reflowable;
	protected boolean linearized;
	protected int updates;
	protected int firstUpdate;
	protected int chapters;
	protected int pages;

	protected Pixmap pixmap;
	protected Rect bbox;
	protected Rect[] links;
	protected String[] linkURIs;
	protected Quad[][] hits;

	protected int pixmapWidth;
	protected int pixmapHeight;
	protected float pixelScale;
	protected int screenDPI;
	protected Dimension screenSize;

	protected int searchDirection = 1;
	protected String searchNeedle = null;

	protected final int MIN_ZOOM_DPI = 18;
	protected final int MAX_ZOOM_DPI = 288;
	protected int[] zoomList = {
		18, 24, 36, 54, 72, 96, 120, 144, 180, 216, 288
	};
	protected boolean customZoom = false;
	protected int currentDPI = 72;

	protected int rotate = 0;
	protected boolean icc = true;
	protected int antialias = 8;
	protected boolean invert = false;
	protected boolean tint = false;
	protected int tintBlack = 0x303030;
	protected int tintWhite = 0xFFFFF0;
	protected boolean showLinks = false;
	protected boolean isFullscreen = false;

	protected ScrollPane pageScroll;
	protected Panel pageHolder;
	protected PageCanvas pageCanvas;

	protected Button firstButton, prevButton, nextButton, lastButton;
	protected TextField pageField;
	protected Label pageLabel;
	protected Button zoomInButton, zoomOutButton;
	protected Choice zoomChoice;
	protected Panel reflowPanel;
	protected Button fontIncButton, fontDecButton;
	protected Label fontSizeLabel;

	protected TextField searchField;
	protected Button searchPrevButton, searchNextButton;
	protected Panel searchStatusPanel;
	protected Label searchStatus;

	protected Panel outlinePanel;
	protected List outlineList;

	protected OCRProgressmeter OCRmeter;
	protected RenderProgressmeter renderMeter;

	protected int number = 0;

	protected class Mark {
		Location loc;

		protected Mark(Location loc) {
			this.loc = loc;
		}
	}

	protected int historyCount = 0;
	protected Mark[] history = new Mark[256];
	protected int futureCount = 0;
	protected Mark[] future = new Mark[256];
	protected Mark[] marks = new Mark[10];

	protected static void messageBox(Frame owner, String title, String message) {
		final Dialog box = new Dialog(owner, title, true);
		box.add(new Label(message), BorderLayout.CENTER);
		Panel buttonPane = new Panel(new FlowLayout());
		Button okayButton = new Button("Okay");
		okayButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent event) {
				box.setVisible(false);
			}
		});
		buttonPane.add(okayButton);
		box.add(buttonPane, BorderLayout.SOUTH);
		box.pack();
		box.setVisible(true);
		box.dispose();
	}

	protected static String passwordDialog(Frame owner, String title) {
		final Dialog box = new Dialog(owner, title, true);
		final TextField textField = new TextField(20);
		textField.setEchoChar('*');
		Panel buttonPane = new Panel(new FlowLayout());
		Button cancelButton = new Button("Cancel");
		Button okayButton = new Button("Okay");
		cancelButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent event) {
				textField.setText("");
				box.setVisible(false);
			}
		});
		okayButton.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent event) {
				box.setVisible(false);
			}
		});
		box.add(new Label("Password:"), BorderLayout.NORTH);
		box.add(textField, BorderLayout.CENTER);
		buttonPane.add(cancelButton);
		buttonPane.add(okayButton);
		box.add(buttonPane, BorderLayout.SOUTH);
		box.pack();
		box.setVisible(true);
		box.dispose();
		String pwd = textField.getText();
		if (pwd.length() == 0)
			return null;
		return pwd;
	}

	public class LogDialog extends Dialog implements ActionListener, KeyListener{
		TextArea info = new TextArea("");
		Button okay = new Button("Okay");

		public LogDialog(Frame parent, String title, String message) {
			super(parent, title, true);

			setLayout(new GridLayout(2, 1));
			info.setText(message);
			info.setEnabled(false);
			add(info);

			Panel buttonPanel = new Panel(new FlowLayout(FlowLayout.CENTER));
			{
				okay.addActionListener(this);
				okay.addKeyListener(this);
				buttonPanel.add(okay);
			}
			add(buttonPanel);

			pack();
			setResizable(false);
			okay.requestFocusInWindow();
			setLocationRelativeTo(parent);
			setVisible(true);
		}

		public void actionPerformed(ActionEvent e) {
			if (e.getSource() == okay)
				dispose();
		}

		public void keyPressed(KeyEvent e) { }
		public void keyReleased(KeyEvent e) { }

		public void keyTyped(KeyEvent e) {
			if (e.getKeyChar() == '\u001b')
				dispose();
		}
	}

	public void exception(Throwable t) {
		StringWriter sw = new StringWriter();
		PrintWriter pw = new PrintWriter(sw);
		t.printStackTrace(pw);
		System.out.println("Exception: " + sw.toString());
		LogDialog ld = new LogDialog(this, "Exception!", "Exception: " + sw.toString());
	}

	public void error(String message) {
		LogDialog ld = new LogDialog(this, "Error!", "Error: " + message);
	}

	public void warning(String message) {
		LogDialog ld = new LogDialog(this, "Warning!", "Warning: " + message);
	}

	public Viewer(String documentPath) {
		pixelScale = getRetinaScale();
		screenDPI = getScreenDPI();
		screenSize = getScreenSize();

		currentDPI = zoomList[findNextLargerZoomLevel(screenDPI - 1)];

		setTitle("MuPDF: ");

		outlinePanel = new Panel(new BorderLayout());
		{
			outlineList = new List();
			outlineList.addItemListener(this);
			outlinePanel.add(outlineList, BorderLayout.CENTER);
		}
		this.add(outlinePanel, BorderLayout.WEST);
		outlinePanel.setMinimumSize(new Dimension(300, 300));
		outlinePanel.setPreferredSize(new Dimension(300, 300));
		outlinePanel.setVisible(false);

		Panel rightPanel = new Panel(new BorderLayout());
		{
			Panel toolpane = new Panel(new GridBagLayout());
			{
				GridBagConstraints c = new GridBagConstraints();
				c.fill = GridBagConstraints.HORIZONTAL;
				c.anchor = GridBagConstraints.WEST;

				Panel toolbar = new Panel(new FlowLayout(FlowLayout.LEFT));
				{
					firstButton = new Button("|<");
					firstButton.addActionListener(this);
					prevButton = new Button("<");
					prevButton.addActionListener(this);
					nextButton = new Button(">");
					nextButton.addActionListener(this);
					lastButton = new Button(">|");
					lastButton.addActionListener(this);
					pageField = new TextField(4);
					pageField.addActionListener(this);
					pageLabel = new Label("/ " + pages);

					toolbar.add(firstButton);
					toolbar.add(prevButton);
					toolbar.add(pageField);
					toolbar.add(pageLabel);
					toolbar.add(nextButton);
					toolbar.add(lastButton);
				}
				c.gridy = 0;
				toolpane.add(toolbar, c);

				toolbar = new Panel(new FlowLayout(FlowLayout.LEFT));
				{
					zoomOutButton = new Button("Zoom-");
					zoomOutButton.addActionListener(this);
					zoomInButton = new Button("Zoom+");
					zoomInButton.addActionListener(this);

					zoomChoice = new Choice();
					for (int i = 0; i < zoomList.length; ++i) {
						zoomChoice.add(String.valueOf(zoomList[i]));
						if (zoomList[i] == currentDPI)
							zoomChoice.select(i);
					}
					zoomChoice.addItemListener(this);

					toolbar.add(zoomOutButton);
					toolbar.add(zoomChoice);
					toolbar.add(zoomInButton);
				}
				c.gridy += 1;
				toolpane.add(toolbar, c);

				reflowPanel = new Panel(new FlowLayout(FlowLayout.LEFT));
				{
					fontDecButton = new Button("Font-");
					fontDecButton.addActionListener(this);
					fontIncButton = new Button("Font+");
					fontIncButton.addActionListener(this);
					fontSizeLabel = new Label(String.valueOf(layoutEm));

					reflowPanel.add(fontDecButton);
					reflowPanel.add(fontSizeLabel);
					reflowPanel.add(fontIncButton);
				}
				c.gridy += 1;
				toolpane.add(reflowPanel, c);

				toolbar = new Panel(new FlowLayout(FlowLayout.LEFT));
				{
					searchField = new TextField(20);
					searchField.addActionListener(this);
					searchPrevButton = new Button("<");
					searchPrevButton.addActionListener(this);
					searchNextButton = new Button(">");
					searchNextButton.addActionListener(this);

					toolbar.add(searchField);
					toolbar.add(searchPrevButton);
					toolbar.add(searchNextButton);
				}
				searchField.addKeyListener(this);
				c.gridy += 1;
				toolpane.add(toolbar, c);

				searchStatusPanel = new Panel(new FlowLayout(FlowLayout.LEFT));
				{
					searchStatus = new Label();

					searchStatusPanel.add(searchStatus);
				}
				c.gridy += 1;
				toolpane.add(searchStatusPanel, c);
			}
			rightPanel.add(toolpane, BorderLayout.NORTH);
		}
		this.add(rightPanel, BorderLayout.EAST);

		pageScroll = new ScrollPane(ScrollPane.SCROLLBARS_NEVER);
		{
			pageHolder = new Panel(new GridBagLayout());
			{
				pageHolder.setBackground(Color.gray);
				pageCanvas = new PageCanvas(pixelScale);
				pageHolder.add(pageCanvas);
			}
			pageCanvas.addKeyListener(this);
			pageCanvas.addMouseWheelListener(this);
			pageScroll.add(pageHolder);
		}
		this.add(pageScroll, BorderLayout.CENTER);

		addWindowListener(this);

		Toolkit toolkit = Toolkit.getDefaultToolkit();
		EventQueue eq = toolkit.getSystemEventQueue();
		worker = new Worker(eq);
		worker.start();

		pack();

		pageCanvas.requestFocusInWindow();

		doc = new ViewerCore(worker, this, documentPath);
		doc.openDocument(new ViewerCore.OnException() {
			public void run(Throwable t) {
				exception(t);
			}
		});
	}

	public void dispose() {
		doc.cancelSearch(null);
		doc.worker.stop();
		super.dispose();
	}

	public void keyPressed(KeyEvent e) {
	}

	public void keyReleased(KeyEvent e) {
		if (e.getSource() == pageCanvas) {
			int c = e.getKeyCode();

			switch(c)
			{
			case KeyEvent.VK_F1: showHelp(); break;

			case KeyEvent.VK_LEFT: pan(-10, 0); break;
			case KeyEvent.VK_RIGHT: pan(+10, 0); break;
			case KeyEvent.VK_UP: pan(0, -10); break;
			case KeyEvent.VK_DOWN: pan(0, +10); break;

			case KeyEvent.VK_PAGE_UP: doc.flipPages(-number, null); break;
			case KeyEvent.VK_PAGE_DOWN: doc.flipPages(+number, null); break;
			}
		}
	}

	public void keyTyped(KeyEvent e) {
		if (e.getSource() == pageCanvas)
			canvasKeyTyped(e);
		else if (e.getSource() == searchField)
			searchFieldKeyTyped(e);
	}

	protected void searchFieldKeyTyped(KeyEvent e) {
		if (e.getExtendedKeyCodeForChar(e.getKeyChar()) == java.awt.event.KeyEvent.VK_ESCAPE)
			clearSearch();

	}

	protected void canvasKeyTyped(KeyEvent e) {
		char c = e.getKeyChar();

		switch(c)
		{
		case 'r': doc.reloadDocument(null); break;
		case 'q': dispose(); break;
		case 'S': save(); break;

		case 'f': toggleFullscreen(); break;

		case 'm': mark(number); break;
		case 't': jumpHistoryBack(number); break;
		case 'T': jumpHistoryForward(number); break;

		case '>': relayout(number > 0 ? number : +1); break;
		case '<': relayout(number > 0 ? number : -1); break;

		case 'I': toggleInvert(); break;
		case 'E': toggleICC(); break;
		case 'A': toggleAntiAlias(); break;
		case 'C': toggleTint(); break;
		case 'o': toggleOutline(); break;
		case 'L': toggleLinks(); break;
		case 'i': showInfo(); break;

		case '[': rotate(-90); break;
		case ']': rotate(+90); break;

		case '+': zoomIn(); break;
		case '-': zoomOut(); break;
		case 'z': zoomToDPI(number); break;

		case 'w': shrinkWrap(); break;
		case 'W': fitWidth(); break;
		case 'H': fitHeight(); break;
		case 'Z': fitPage(); break;

		case 'k': pan(0, pageCanvas != null ? pageCanvas.getHeight() / -10 : -10); break;
		case 'j': pan(0, pageCanvas != null ? pageCanvas.getWidth() / +10 : +10); break;
		case 'h': pan(pageCanvas != null ? pageCanvas.getHeight() / -10 : -10, 0); break;
		case 'l': pan(pageCanvas != null ? pageCanvas.getWidth() / +10 : +10, 0); break;

		case 'b': smartMove(-1, number); break;
		case ' ': smartMove(+1, number); break;

		case ',': flipPages(-number); break;
		case '.': flipPages(+number); break;

		case 'g': gotoPage(number); break;
		case 'G': gotoLastPage(); break;

		case '/': editSearchNeedle(+1); break;
		case '?': editSearchNeedle(-1); break;
		case 'N': search(-1); break;
		case 'n': search(+1); break;
		case '\u001b': clearSearch(); break;
		}

		if (c >= '0' && c <= '9')
			number = number * 10 + c - '0';
		else
			number = 0;
	}

	public void mouseWheelMoved(MouseWheelEvent e) {
		int mod = e.getModifiersEx();
		int rot = e.getWheelRotation();
		if ((mod & MouseWheelEvent.CTRL_DOWN_MASK) != 0) {
			if (rot < 0)
				zoomIn();
			else
				zoomOut();
		} else if ((mod & MouseWheelEvent.SHIFT_DOWN_MASK) != 0) {
			if (rot < 0)
				pan(pageCanvas != null ? pageCanvas.getHeight() / -10 : -10, 0);
			else
				pan(pageCanvas != null ? pageCanvas.getHeight() / +10 : +10, 0);
		} else if (mod == 0) {
			if (rot < 0)
				pan(0, pageCanvas != null ? pageCanvas.getHeight() / -10 : -10);
			else
				pan(0, pageCanvas != null ? pageCanvas.getHeight() / +10 : +10);
		}
	}

	protected void editSearchNeedle(int direction) {
		clearSearch();
		searchDirection = direction;
		searchField.requestFocusInWindow();
	}

	protected void cancelSearch() {
		doc.cancelSearch(null);
	}

	protected void clearSearch() {
		cancelSearch();
		searchField.setText("");
		searchStatus.setText("");
		searchStatusPanel.validate();
		validate();
		hits = null;
		redraw();
	}

	public void search(int direction) {
		if (searchField.isEnabled()) {
			cancelSearch();
			searchField.setEnabled(false);
			searchNextButton.setEnabled(false);
			searchPrevButton.setEnabled(false);
			pageCanvas.requestFocusInWindow();
			doc.search(searchField.getText(), direction, new ViewerCore.OnException() {
					public void run(Throwable t) {
						exception(t);
					}
			});
		}
	}

	protected void render() {
		if (bbox == null)
			return;

		float width = bbox.x1 - bbox.x0;
		float height = bbox.y1 - bbox.y0;
		float scaleX = (float) Math.floor(width * (currentDPI/72.0f) * pixelScale + 0.5f) / width;
		float scaleY = (float) Math.floor(height * (currentDPI/72.0f) * pixelScale + 0.5f) / height;
		Matrix ctm = new Matrix().scale(scaleX, scaleY).rotate(rotate);

		Rect atOrigin = new Rect(bbox).transform(ctm);
		ctm.e -= atOrigin.x0;
		ctm.f -= atOrigin.y0;
		Rect bounds = new Rect(bbox).transform(ctm);

		Cookie cookie = new Cookie();

		renderMeter = new RenderProgressmeter(this, "Rendering...", cookie, 250);
		renderMeter.setLocationRelativeTo(this);
		pageCanvas.requestFocusInWindow();

		doc.renderPage(ctm, bounds, icc, antialias, invert, tint, tintBlack, tintWhite, cookie,
			new ViewerCore.OnException() {
				public void run(Throwable t) {
					if (!renderMeter.cancelled)
						exception(t);
				}
			}
		);
	}

	protected int findExactZoomLevel(int dpi) {
		for (int level = 0; level < zoomList.length - 1; level++)
			if (zoomList[level] == dpi)
				return level;
		return -1;
	}

	protected int findNextSmallerZoomLevel(int dpi) {
		for (int level = zoomList.length - 1; level >= 0; level--)
			if (zoomList[level] < dpi)
				return level;
		return 0;
	}

	protected int findNextLargerZoomLevel(int dpi) {
		for (int level = 0; level < zoomList.length - 1; level++)
			if (zoomList[level] > dpi)
				return level;
		return zoomList.length - 1;
	}

	protected void zoomToDPI(int newDPI) {
		if (newDPI == 0)
			newDPI = screenDPI;
		if (newDPI < MIN_ZOOM_DPI)
			newDPI = MIN_ZOOM_DPI;
		if (newDPI > MAX_ZOOM_DPI)
			newDPI = MAX_ZOOM_DPI;

		if (newDPI == currentDPI)
			return;

		int level = findExactZoomLevel(newDPI);
		if (level < 0) {
			if (customZoom)
				zoomChoice.remove(0);
			customZoom = true;
			zoomChoice.insert(String.valueOf(newDPI), 0);
			zoomChoice.select(0);
		} else {
			if (customZoom) {
				customZoom = false;
				zoomChoice.remove(0);
			}
			zoomChoice.select(level);
		}

		currentDPI = newDPI;
		render();
	}

	protected void zoomToLevel(int level) {
		if (level < 0)
			level = 0;
		if (level >= zoomList.length - 1)
			level = zoomList.length - 1;
		zoomToDPI(zoomList[level]);
	}

	protected void zoomIn() {
		zoomToLevel(findNextLargerZoomLevel(currentDPI));
	}

	protected void zoomOut() {
		zoomToLevel(findNextSmallerZoomLevel(currentDPI));
	}

	protected void zoomToScale(float newZoom) {
		zoomToDPI((int)(newZoom * 72));
	}

	protected void fit(float desired, float unscaled) {
		zoomToScale(desired / unscaled);
	}

	protected float unscaledWidth() {
		return bbox != null ? bbox.x1 - bbox.x0 : 0;
	}

	protected float unscaledHeight() {
		return bbox != null ? bbox.y1 - bbox.y0 : 0;
	}

	protected void fitWidth() {
		fit(pageScroll.getSize().width, unscaledWidth());
	}

	protected void fitHeight() {
		fit(pageScroll.getSize().height, unscaledHeight());
	}

	protected void fitPage() {
		Dimension size = pageScroll.getSize();
		float width = bbox != null ? bbox.x1 - bbox.x0 : 0;
		float height = bbox != null ? bbox.y1 - bbox.y0 : 0;
		float pageAspect = (width == 0 || height == 0) ? 0 : (width / height);
		float canvasAspect = (float) size.width / (float) size.height;
		if (pageAspect > canvasAspect)
			fitWidth();
		else
			fitHeight();
	}

	protected void rotate(int change) {
		int newRotate = rotate + change;
		while (newRotate < 0) newRotate += 360;
		while (newRotate >= 360) newRotate -= 360;

		if (newRotate - rotate == 0)
			return;
		rotate = newRotate;

		render();
	}

	protected void toggleAntiAlias() {
		int newAntialias = number != 0 ? number : (antialias == 8 ? 0 : 8);
		if (newAntialias - antialias == 0)
			return;

		antialias = newAntialias;
		render();
	}

	protected void toggleICC() {
		icc = !icc;
		render();
	}

	protected void toggleInvert() {
		invert = !invert;
		render();
	}

	protected void toggleTint() {
		tint = !tint;
		render();
	}

	protected void toggleOutline() {
		if (outlineList.getItemCount() <= 0)
			return;

		outlinePanel.setVisible(!outlinePanel.isVisible());
		pack();
		validate();
	}

	protected void toggleLinks() {
		showLinks = !showLinks;
		redraw();
	}

	protected boolean isShrinkWrapped(int oldPixmapWidth, int oldPixmapHeight) {
		Dimension size = pageScroll.getSize();
		if (oldPixmapWidth == 0 && oldPixmapHeight == 0)
			return true;
		if (oldPixmapWidth + 4 == size.width && oldPixmapHeight + 4 == size.height)
			return true;
		return false;
	}

	protected void shrinkWrap() {
		Dimension newSize = new Dimension(pixmapWidth, pixmapHeight);
		newSize.width += 4;
		newSize.height += 4;

		if (newSize.width > screenSize.width)
			newSize.width = screenSize.width;
		if (newSize.height > screenSize.height)
			newSize.height = screenSize.height;

		pageScroll.setPreferredSize(newSize);
		pack();
	}

	protected void redraw() {
		boolean wasShrinkWrapped = isShrinkWrapped(pixmapWidth, pixmapHeight);

		if (pixmap != null) {
			pixmapWidth = pixmap.getWidth();
			pixmapHeight = pixmap.getHeight();
			BufferedImage image = new BufferedImage(pixmapWidth, pixmapHeight, BufferedImage.TYPE_3BYTE_BGR);
			image.setRGB(0, 0, pixmapWidth, pixmapHeight, pixmap.getPixels(), 0, pixmapWidth);
			pageCanvas.setPage(image, showLinks ? links : null, hits);
		} else {
			pixmapWidth = 0;
			pixmapHeight = 0;
			pageCanvas.setPage(null, null, null);
		}

		if (wasShrinkWrapped)
			shrinkWrap();

		pageCanvas.invalidate();
		validate();
	}

	protected static String paperSizeName(int w, int h)
	{
		/* ISO A */
		if (w == 2384 && h == 3370) return "A0";
		if (w == 1684 && h == 2384) return "A1";
		if (w == 1191 && h == 1684) return "A2";
		if (w == 842 && h == 1191) return "A3";
		if (w == 595 && h == 842) return "A4";
		if (w == 420 && h == 595) return "A5";
		if (w == 297 && h == 420) return "A6";

		/* US */
		if (w == 612 && h == 792) return "Letter";
		if (w == 612 && h == 1008) return "Legal";
		if (w == 792 && h == 1224) return "Ledger";
		if (w == 1224 && h == 792) return "Tabloid";

		return null;
	}

	protected void showHelp() {
		final Dialog box = new Dialog(this, "Help", true);
		box.addWindowListener(new WindowListener() {
			public void windowActivated(WindowEvent event) { }
			public void windowDeactivated(WindowEvent event) { }
			public void windowIconified(WindowEvent event) { }
			public void windowDeiconified(WindowEvent event) { }
			public void windowOpened(WindowEvent event) { }
			public void windowClosed(WindowEvent event) { }
			public void windowClosing(WindowEvent event) {
				box.setVisible(false);
				pageCanvas.requestFocusInWindow();
			}
		});

		String help[] = {
			"The middle mouse button (scroll wheel button) pans the document view.",
			//"The right mouse button selects a region and copies the marked text to the clipboard.",
			//"",
			//"",
			"F1 - show this message",
			"i - show document information",
			"o - show document outline",
			//"a - show annotation editor",
			"L - highlight links",
			//"F - highlight form fields",
			"r - reload file",
			"S - save file",
			"q - quit",
			"",
			"< - decrease E-book font size",
			"> - increase E-book font size",
			"A - toggle anti-aliasing",
			"I - toggle inverted color mode",
			"C - toggle tinted color mode",
			"E - toggle ICC color management",
			//"e - toggle spot color emulation",
			"",
			"f - fullscreen window",
			"w - shrink wrap window",
			"W - fit to width",
			"H - fit to height",
			"Z - fit to page",
			"z - reset zoom",
			"[number] z - set zoom resolution in DPI",
			"plus - zoom in",
			"minus - zoom out",
			"[ - rotate counter-clockwise",
			"] - rotate clockwise",
			"arrow keys - scroll in small increments",
			"h, j, k, l - scroll in small increments",
			"",
			"b - smart move backward",
			"space - smart move forward",
			"comma or page up - go backward",
			"period or page down - go forward",
			"g - go to first page",
			"G - go to last page",
			"[number] g - go to page number",
			"",
			"m - save current location in history",
			"t - go backward in history",
			"T - go forward in history",
			"[number] m - save current location in numbered bookmark",
			"[number] t - go to numbered bookmark",
			"",
			"/ - search for text forward",
			"? - search for text backward",
			"n - repeat search",
			"N - repeat search in reverse direction",
		};

		Panel helpPanel = new Panel(new GridLayout(help.length, 1));

		for (int i = 0; i < help.length; i++)
			helpPanel.add(new Label(help[i]));

		Button button = new Button("OK");
		button.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent event) {
				box.setVisible(false);
			}
		});
		button.addKeyListener(new KeyListener() {
			public void keyPressed(KeyEvent e) { }
			public void keyReleased(KeyEvent e) {
				if (e.getKeyCode() == KeyEvent.VK_F1)
					box.setVisible(false);
			}
			public void keyTyped(KeyEvent e) {
				if (e.getKeyChar() == '\u001b' || e.getKeyChar() == '\r' || e.getKeyChar() == '\n')
					box.setVisible(false);
			}
		});

		Panel buttonPane = new Panel(new FlowLayout());
		buttonPane.add(button);

		box.add(helpPanel, BorderLayout.CENTER);
		box.add(buttonPane, BorderLayout.SOUTH);

		box.setResizable(false);
		box.pack();

		java.awt.Point winLoc = this.getLocation();
		Dimension winDim = this.getSize();
		int winCenterX = winLoc.x + winDim.width / 2;
		int winCenterY = winLoc.y + winDim.height / 2;

		Dimension diagDim = box.getSize();
		int x = winCenterX - diagDim.width / 2;
		int y = winCenterY - diagDim.height / 2;

		box.setLocation(x, y);

		button.requestFocusInWindow();
		box.setVisible(true);

		box.dispose();
	}

	protected void showInfo() {
		StringBuffer buffer;

		cancelSearch();

		final Dialog box = new Dialog(this, "Document info", true);
		box.addWindowListener(new WindowListener() {
			public void windowActivated(WindowEvent event) { }
			public void windowDeactivated(WindowEvent event) { }
			public void windowIconified(WindowEvent event) { }
			public void windowDeiconified(WindowEvent event) { }
			public void windowOpened(WindowEvent event) { }
			public void windowClosed(WindowEvent event) { }
			public void windowClosing(WindowEvent event) {
				box.setVisible(false);
				pageCanvas.requestFocusInWindow();
			}
		});

		Panel infoPanel = new Panel();
		int rows = 0;

		if (title != null) rows++;

		if (author != null) rows++;

		if (format != null) rows++;

		if (encryption != null) rows++;

		buffer = new StringBuffer();
		if (print)
			buffer.append("print, ");
		if (copy)
			buffer.append("copy, ");
		if (edit)
			buffer.append("edit, ");
		if (annotate)
			buffer.append("annotate, ");
		if (form)
			buffer.append("form, ");
		if (accessibility)
			buffer.append("accessibility, ");
		if (assemble)
			buffer.append("assemble, ");
		if (printHq)
			buffer.append("print-hq, ");
		if (buffer.length() > 2)
			buffer.delete(buffer.length() - 2, buffer.length());
		String permissions = buffer.length() > 0 ? buffer.toString() : null;
		if (permissions != null) rows++;

		buffer = new StringBuffer();
		if (doc.equals("PDF")) {
			buffer.append("PDF ");
			if (linearized)
				buffer.append("linearized ");
			buffer.append("document with ");
			buffer.append(updates);
			if (updates == 1)
				buffer.append(" update");
			else
				buffer.append(" updates");
		}
		String versions = buffer.length() > 0 ? buffer.toString() : null;
		if (versions != null) rows++;

		buffer = new StringBuffer();
		if (doc.equals("PDF")) {
			if (updates > 1) {
				if (firstUpdate == 0)
					buffer.append("Change firstUpdate seems valid.");
				else if (firstUpdate == 1)
					buffer.append("Invalid changes made to the document in the last update.");
				else if (firstUpdate == 2)
					buffer.append("Invalid changes made to the document in the penultimate update.");
				else {
					buffer.append("Invalid changes made to the document ");
					buffer.append(firstUpdate);
					buffer.append(" updates ago.");
				}
			}
		}
		String validation = buffer.length() > 0 ? buffer.toString() : null;
		if (validation != null) rows++;

		buffer = new StringBuffer();
		int w = 0;
		int h = 0;
		if (bbox != null) {
			w = (int)(bbox.x1 - bbox.x0 + 0.5f);
			h = (int)(bbox.y1 - bbox.y0 + 0.5f);
		}
		buffer.append(w);
		buffer.append(" x ");
		buffer.append(h);
		String name = paperSizeName(w, h);
		if (name == null)
			name = paperSizeName(h, w);
		if (name != null)
			buffer.append("(" + name + ")");
		String paperSize = buffer.length() > 0 ? buffer.toString() : null;
		if (paperSize != null) rows++;

		buffer = new StringBuffer();
		buffer.append(pageNumber + 1);
		buffer.append(" / ");
		buffer.append(pages);
		String page = buffer.length() > 0 ? buffer.toString() : null;
		if (page != null) rows++;

		String iccstring = icc ? "on" : "off";
		rows++;

		String antialiasstring = Integer.toString(antialias);
		rows++;

		infoPanel.setLayout(new GridLayout(rows, 1));

		if (title != null) infoPanel.add(new Label("Title: " + title));
		if (author != null) infoPanel.add(new Label("Author: " + author));
		if (format != null) infoPanel.add(new Label("Format: " + format));
		if (encryption != null) infoPanel.add(new Label("Encryption: " + encryption));
		if (permissions != null) infoPanel.add(new Label("Permissions: " + permissions));
		if (versions != null) infoPanel.add(new Label(versions));
		if (validation != null) infoPanel.add(new Label(validation));
		infoPanel.add(new Label("Size: " + paperSize));
		infoPanel.add(new Label("Page: " + page));
		infoPanel.add(new Label("ICC rendering: " + iccstring));
		infoPanel.add(new Label("Antialias rendering: " + antialiasstring));

		Button button = new Button("OK");
		button.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent event) {
				box.setVisible(false);
			}
		});
		button.addKeyListener(new KeyListener() {
			public void keyPressed(KeyEvent e) { }
			public void keyReleased(KeyEvent e) { }
			public void keyTyped(KeyEvent e) {
				if (e.getKeyChar() == '\u001b' || e.getKeyChar() == '\r' || e.getKeyChar() == '\n')
					box.setVisible(false);
			}
		});

		Panel buttonPane = new Panel(new FlowLayout());
		buttonPane.add(button);

		box.add(infoPanel, BorderLayout.CENTER);
		box.add(buttonPane, BorderLayout.SOUTH);

		button.requestFocusInWindow();

		box.setResizable(false);
		box.pack();

		java.awt.Point winLoc = this.getLocation();
		Dimension winDim = this.getSize();
		int winCenterX = winLoc.x + winDim.width / 2;
		int winCenterY = winLoc.y + winDim.height / 2;

		Dimension diagDim = box.getSize();
		int x = winCenterX - diagDim.width / 2;
		int y = winCenterY - diagDim.height / 2;

		box.setLocation(x, y);

		box.setVisible(true);
		box.dispose();
	}

	protected void toggleFullscreen() {
		isFullscreen = !isFullscreen;

		if (isFullscreen)
			setExtendedState(Frame.MAXIMIZED_BOTH);
		else
			setExtendedState(Frame.NORMAL);
	}

	protected void mark(int number) {
		cancelSearch();
		if (number == 0)
			pushHistory();
		else if (number > 0 && number < marks.length)
			marks[number] = saveMark();
	}

	protected void jumpHistoryBack(int number) {
		cancelSearch();
		if (number == 0) {
			if (historyCount > 0)
				popHistory();
		} else if (number > 0 && number < marks.length)
			restoreMark(marks[number]);
	}

	protected void jumpHistoryForward(int number) {
		cancelSearch();
		if (number == 0) {
			if (futureCount > 0) {
				popFuture();
			}
		}
	}

	protected Mark saveMark() {
		return new Mark(location);
	}

	protected void restoreMark(Mark mark) {
		if (mark != null) {
			doc.gotoLocation(mark.loc, null);
			pageCanvas.requestFocusInWindow();
		}
	}

	protected void pushHistory() {
		if (historyCount > 0 && location.equals(history[historyCount - 1].loc))
		{
			return;
		}

		if (historyCount + 1 >= history.length) {
			for (int i = 0; i < history.length - 1; i++)
				history[i] = history[i + 1];
			history[historyCount] = saveMark();
		} else {
			history[historyCount++] = saveMark();
		}
	}

	protected void pushFuture() {
		if (futureCount + 1 >= future.length) {
			for (int i = 0; i < future.length - 1; i++)
				future[i] = future[i + 1];
			future[futureCount] = saveMark();
		} else {
			future[futureCount++] = saveMark();
		}
	}

	protected void clearFuture() {
		futureCount = 0;
	}

	protected void popHistory() {
		Location here = location;
		pushFuture();
		while (historyCount > 0 && location.equals(here))
			restoreMark(history[--historyCount]);
	}

	protected void popFuture() {
		Location here = location;
		pushHistory();
		while (futureCount > 0 && location.equals(here))
			restoreMark(future[--futureCount]);
	}

	protected void pan(int panx, int pany) {
		Adjustable hadj = pageScroll.getHAdjustable();
		Adjustable vadj = pageScroll.getVAdjustable();
		int h = hadj.getValue();
		int v = vadj.getValue();
		int newh = h + panx;
		int newv = v + pany;

		if (newh < hadj.getMinimum())
			newh = hadj.getMinimum();
		if (newh > hadj.getMaximum() - hadj.getVisibleAmount())
			newh = hadj.getMaximum() - hadj.getVisibleAmount();
		if (newv < vadj.getMinimum())
			newv = vadj.getMinimum();
		if (newv > vadj.getMaximum() - vadj.getVisibleAmount())
			newv = vadj.getMaximum() - vadj.getVisibleAmount();

		if (newh == h && newv == v)
			return;

		if (newh != h)
			hadj.setValue(newh);
		if (newv != v)
			vadj.setValue(newv);
	}

	protected void smartMove(int direction, int moves) {
		cancelSearch();

		if (moves < 1)
			moves = 1;

		while (moves-- > 0)
		{
			Adjustable hadj = pageScroll.getHAdjustable();
			Adjustable vadj = pageScroll.getVAdjustable();
			int slop_x = hadj.getMaximum() / 20;
			int slop_y = vadj.getMaximum() / 20;

			if (direction > 0) {
				int remaining_x = hadj.getMaximum() - hadj.getValue() - hadj.getVisibleAmount();
				int remaining_y = vadj.getMaximum() - vadj.getValue() - vadj.getVisibleAmount();

				if (remaining_y > slop_y) {
					int value = vadj.getValue() + vadj.getVisibleAmount() * 9 / 10;
					if (value > vadj.getMaximum())
						value = vadj.getMaximum();
					vadj.setValue(value);
				} else if (remaining_x > slop_x) {
					vadj.setValue(vadj.getMinimum());
					int value = hadj.getValue() + hadj.getVisibleAmount() * 9 / 10;
					if (value > hadj.getMaximum())
						value = hadj.getMaximum();
					hadj.setValue(value);
				} else {
					doc.flipPages(+1, null);
					vadj.setValue(vadj.getMinimum());
					hadj.setValue(hadj.getMinimum());
				}
			} else {
				int remaining_x = Math.abs(hadj.getMinimum() - hadj.getValue());
				int remaining_y = Math.abs(vadj.getMinimum() - vadj.getValue());

				if (remaining_y > slop_y) {
					int value = vadj.getValue() - vadj.getVisibleAmount() * 9 / 10;
					if (value < vadj.getMinimum())
						value = vadj.getMinimum();
					vadj.setValue(value);
				} else if (remaining_x > slop_x) {
					vadj.setValue(vadj.getMaximum());
					int value = hadj.getValue() - hadj.getVisibleAmount() * 9 / 10;
					if (value < hadj.getMinimum())
						value = hadj.getMinimum();
					hadj.setValue(value);
				} else {
					doc.flipPages(-1, null);
					vadj.setValue(vadj.getMaximum());
					hadj.setValue(hadj.getMaximum());
				}
			}
		}
	}

	protected void flipPages(int number) {
		cancelSearch();
		doc.flipPages(number, null);
	}

	protected void gotoPage(int number) {
		cancelSearch();
		doc.gotoPage(number - 1, null);
	}

	protected void gotoLastPage() {
		cancelSearch();
		doc.gotoLastPage(null);
	}

	protected void relayout(int change) {
		int newEm = layoutEm + change;
		if (newEm < 6)
			newEm = 6;
		if (newEm > 36)
			newEm = 36;

		if (newEm == layoutEm)
			return;

		layoutEm = newEm;
		fontSizeLabel.setText(String.valueOf(layoutEm));
		doc.relayoutDocument(layoutWidth, layoutHeight, layoutEm, null);
	}

	public void actionPerformed(ActionEvent event) {
		Object source = event.getSource();

		if (source == firstButton)
			doc.gotoFirstPage(null);
		if (source == lastButton)
			doc.gotoLastPage(null);
		if (source == prevButton)
			doc.flipPages(-1, null);
		if (source == nextButton)
			doc.flipPages(+1, null);
		if (source == pageField) {
			doc.gotoPage(Integer.parseInt(pageField.getText()) - 1, null);
			pageCanvas.requestFocusInWindow();
		}

		if (source == searchField)
			search(searchDirection);
		if (source == searchNextButton)
			search(+1);
		if (source == searchPrevButton)
			search(-1);

		if (source == fontIncButton && doc != null && reflowable)
			relayout(+1);
		if (source == fontDecButton && doc != null && reflowable)
			relayout(-1);

		if (source == zoomOutButton) {
			zoomOut();
			pageCanvas.requestFocusInWindow();
		}
		if (source == zoomInButton) {
			zoomIn();
			pageCanvas.requestFocusInWindow();
		}
	}

	public void itemStateChanged(ItemEvent event) {
		Object source = event.getSource();
		if (source == zoomChoice) {
			zoomToLevel(zoomChoice.getSelectedIndex());
			pageCanvas.requestFocusInWindow();
		}
		if (source == outlineList) {
			int i = outlineList.getSelectedIndex();
			doc.gotoLocation(outline[i].location, null);
			pageCanvas.requestFocusInWindow();
		}
	}

	public void windowClosing(WindowEvent event) { dispose(); }
	public void windowActivated(WindowEvent event) { }
	public void windowDeactivated(WindowEvent event) { }
	public void windowIconified(WindowEvent event) { }
	public void windowDeiconified(WindowEvent event) { }
	public void windowOpened(WindowEvent event) { }
	public void windowClosed(WindowEvent event) { }

	public void save() {
		cancelSearch();

		SaveOptionsDialog dialog = new SaveOptionsDialog(this);
		dialog.populate();
		dialog.setLocationRelativeTo(this);
		dialog.setVisible(true);
		dialog.dispose();

		final String options = dialog.getOptions();
		if (options == null)
		{
			pageCanvas.requestFocusInWindow();
			return;
		}

		FileDialog fileDialog = new FileDialog(this, "MuPDF Save File", FileDialog.SAVE);
		fileDialog.setDirectory(System.getProperty("user.dir"));
		fileDialog.setFilenameFilter(new FilenameFilter() {
			public boolean accept(File dir, String name) {
				return Document.recognize(name);
			}
		});
		fileDialog.setFile(doc.documentPath);
		fileDialog.setVisible(true);
		fileDialog.dispose();

		if (fileDialog.getFile() == null)
		{
			pageCanvas.requestFocusInWindow();
			return;
		}

		final String selectedPath = new StringBuffer(fileDialog.getDirectory()).append(File.separatorChar).append(fileDialog.getFile()).toString();
		OCRmeter = new OCRProgressmeter(this, "Saving...", pages);
		OCRmeter.setLocationRelativeTo(this);
		OCRmeter.setVisible(true);
		pageCanvas.requestFocusInWindow();

		if (options.indexOf("ocr-language=") < 0)
			doc.save(selectedPath, options, OCRmeter, new ViewerCore.OnException() {
				public void run(Throwable t) {
					if (t instanceof IOException)
						exception(t);
					else if (t instanceof RuntimeException && !OCRmeter.cancelled)
						exception(t);
				}
			});
		else
		{
			try {
				FileStream fs = new FileStream(selectedPath, "rw");
				doc.save(fs, options, OCRmeter, new ViewerCore.OnException() {
					public void run(Throwable t) {
						if (t instanceof RuntimeException && !OCRmeter.cancelled)
							exception(t);
					}
				});
			} catch (IOException e) {
				exception(e);
			}
		}
	}

	public void onSaveComplete() {
		if (OCRmeter != null)
			OCRmeter.done();
	}

	class SaveOptionsDialog extends Dialog implements ActionListener, ItemListener, KeyListener {
		Checkbox snapShot = new Checkbox("Snapshot", false);
		Checkbox highSecurity = new Checkbox("High security", false);
		Choice resolution = new Choice();
		TextField language = new TextField("eng");
		Checkbox incremental = new Checkbox("Incremental", false);

		Checkbox prettyPrint = new Checkbox("Pretty print", false);
		Checkbox ascii = new Checkbox("Ascii", false);
		Checkbox decompress = new Checkbox("Decompress", false);
		Checkbox compress = new Checkbox("Compress", true);
		Checkbox compressImages = new Checkbox("Compress images", true);
		Checkbox compressFonts = new Checkbox("Compress fonts", true);

		Checkbox linearize = new Checkbox("Linearize", false);
		Checkbox garbageCollect = new Checkbox("Garbage collect", false);
		Checkbox cleanSyntax = new Checkbox("Clean syntax", false);
		Checkbox sanitizeSyntax = new Checkbox("Sanitize syntax", false);

		Choice encryption = new Choice();
		TextField userPassword = new TextField();
		TextField ownerPassword = new TextField();

		Button cancel = new Button("Cancel");
		Button save = new Button("Save");

		String options = null;

		public SaveOptionsDialog(Frame parent) {
			super(parent, "MuPDF Save Options", true);

			resolution.add("200dpi");
			resolution.add("300dpi");
			resolution.add("600dpi");
			resolution.add("1200dpi");

			encryption.add("Keep");
			encryption.add("None");
			encryption.add("RC4, 40bit");
			encryption.add("RC4, 128bit");
			encryption.add("AES, 128bit");
			encryption.add("AES, 256bit");

			snapShot.addItemListener(this);
			highSecurity.addItemListener(this);
			resolution.addItemListener(this);
			language.addActionListener(this);
			incremental.addItemListener(this);
			prettyPrint.addItemListener(this);
			ascii.addItemListener(this);
			decompress.addItemListener(this);
			compress.addItemListener(this);
			compressImages.addItemListener(this);
			compressFonts.addItemListener(this);
			linearize.addItemListener(this);
			garbageCollect.addItemListener(this);
			cleanSyntax.addItemListener(this);
			sanitizeSyntax.addItemListener(this);

			encryption.addItemListener(this);
			userPassword.addActionListener(this);
			ownerPassword.addActionListener(this);

			cancel.addActionListener(this);
			save.addActionListener(this);
			save.addKeyListener(this);

			calculateOptions();
		}

		void populate(Container container, GridBagConstraints c, Component component) {
			GridBagLayout gbl = (GridBagLayout) container.getLayout();
			gbl.setConstraints(component, c);
			container.add(component);
		}

		void populate() {
			GridBagConstraints c = new GridBagConstraints();

			c.fill = GridBagConstraints.BOTH;
			c.weightx = 1.0;
			c.gridwidth = GridBagConstraints.REMAINDER;

			GridBagLayout gbl = new GridBagLayout();
			setLayout(gbl);

			Panel left = new Panel();
			Panel right = new Panel();
			GridBagLayout lgbl = new GridBagLayout();
			GridBagLayout rgbl = new GridBagLayout();
			left.setLayout(lgbl);
			right.setLayout(rgbl);

			populate(left, c, snapShot);
			populate(left, c, highSecurity);
			populate(left, c, resolution);
			populate(left, c, language);
			populate(left, c, incremental);

			c.weighty = 1.5;
			populate(left, c, new Panel());
			c.weighty = 0.0;

			populate(left, c, prettyPrint);
			populate(left, c, ascii);
			populate(left, c, decompress);
			populate(left, c, compress);
			populate(left, c, compressImages);
			populate(left, c, compressFonts);

			populate(right, c, linearize);
			populate(right, c, garbageCollect);
			populate(right, c, cleanSyntax);
			populate(right, c, sanitizeSyntax);

			c.weighty = 1.5;
			populate(right, c, new Panel());
			c.weighty = 0.0;

			populate(right, c, new Label("Encryption"));
			populate(right, c, encryption);
			populate(right, c, new Label("User password"));
			populate(right, c, userPassword);
			populate(right, c, new Label("Owner password"));
			populate(right, c, ownerPassword);

			c.gridwidth = GridBagConstraints.REMAINDER;
			populate(this, c, new Panel());

			c.gridwidth = 1;
			populate(this, c, left);
			c.gridwidth = GridBagConstraints.REMAINDER;
			populate(this, c, right);

			c.gridwidth = GridBagConstraints.REMAINDER;
			populate(this, c, new Panel());

			c.gridwidth = 1;
			populate(this, c, cancel);
			c.gridwidth = GridBagConstraints.REMAINDER;
			populate(this, c, save);

			pack();
			setResizable(false);
			save.requestFocusInWindow();
		}

		public void keyPressed(KeyEvent e) { }
		public void keyReleased(KeyEvent e) { }

		public void keyTyped(KeyEvent e) {
			if (e.getKeyChar() == '\u001b')
				cancel();
			else if (e.getKeyChar() == '\n')
				save();
		}

		public void actionPerformed(ActionEvent e) {
			if (e.getSource() == cancel)
				cancel();
			else if (e.getSource() == save)
				save();
		}

		void cancel() {
			options = null;
			setVisible(false);
		}

		void save() {
			setVisible(false);
		}

		public void itemStateChanged(ItemEvent e) {
			calculateOptions();
		}

		void calculateOptions() {
			boolean isPDF = false;
			boolean canBeSavedIncrementally = false;
			boolean isRedacted = false;

			if (isPDF && !canBeSavedIncrementally)
				incremental.setState(false);

			if (highSecurity.getState()) {
				incremental.setState(false);
				prettyPrint.setState(false);
				ascii.setState(false);
				decompress.setState(false);
				compress.setState(true);
				compressImages.setState(false);
				compressFonts.setState(false);
				linearize.setState(false);
				garbageCollect.setState(false);
				cleanSyntax.setState(false);
				sanitizeSyntax.setState(false);
				encryption.select("None");
				userPassword.setText("");
				ownerPassword.setText("");
			} else if (incremental.getState()) {
				linearize.setState(false);
				garbageCollect.setState(false);
				cleanSyntax.setState(false);
				sanitizeSyntax.setState(false);
				encryption.select("Keep");
				userPassword.setText("");
				ownerPassword.setText("");
			}

			highSecurity.setEnabled(snapShot.getState() == false);
			resolution.setEnabled(snapShot.getState() == false && highSecurity.getState() == true);
			language.setEnabled(snapShot.getState() == false && highSecurity.getState() == true);
			incremental.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && isPDF && canBeSavedIncrementally);
			prettyPrint.setEnabled(snapShot.getState() == false && highSecurity.getState() == false);
			ascii.setEnabled(snapShot.getState() == false && highSecurity.getState() == false);
			decompress.setEnabled(snapShot.getState() == false && highSecurity.getState() == false);
			compress.setEnabled(snapShot.getState() == false && highSecurity.getState() == false);
			compressImages.setEnabled(snapShot.getState() == false && highSecurity.getState() == false);
			compressFonts.setEnabled(snapShot.getState() == false && highSecurity.getState() == false);
			linearize.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && incremental.getState() == false);
			garbageCollect.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && incremental.getState() == false);
			cleanSyntax.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && incremental.getState() == false);
			sanitizeSyntax.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && incremental.getState() == false);
			encryption.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && incremental.getState() == false);
			userPassword.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && incremental.getState() == false && encryption.getSelectedItem() != "Keep" && encryption.getSelectedItem() != "None");
			ownerPassword.setEnabled(snapShot.getState() == false && highSecurity.getState() == false && incremental.getState() == false && encryption.getSelectedItem() != "Keep" && encryption.getSelectedItem() != "None");

			if (incremental.getState()) {
				garbageCollect.setState(false);
				linearize.setState(false);
				cleanSyntax.setState(false);
				sanitizeSyntax.setState(false);
				encryption.select("Keep");
			}

			StringBuilder opts = new StringBuilder();
			if (highSecurity.getState()) {
				opts.append(",compression=flate");
				opts.append(",resolution=");
				opts.append(resolution.getSelectedItem());
				opts.append(",ocr-language=");
				opts.append(language.getText());
			} else {
				if (decompress.getState()) opts.append(",decompress=yes");
				if (compress.getState()) opts.append(",compress=yes");
				if (compressFonts.getState()) opts.append(",compress-fonts=yes");
				if (compressImages.getState()) opts.append(",compress-images=yes");
				if (ascii.getState()) opts.append(",ascii=yes");
				if (prettyPrint.getState()) opts.append(",pretty=yes");
				if (linearize.getState()) opts.append(",linearize=yes");
				if (cleanSyntax.getState()) opts.append(",clean=yes");
				if (sanitizeSyntax.getState()) opts.append(",sanitize=yes");
				if (encryption.getSelectedItem() == "None") opts.append(",decrypt=yes");
				if (encryption.getSelectedItem() == "Keep") opts.append(",decrypt=no");
				if (encryption.getSelectedItem() == "None") opts.append(",encrypt=no");
				if (encryption.getSelectedItem() == "Keep") opts.append(",encrypt=keep");
				if (encryption.getSelectedItem() == "RC4, 40bit") opts.append(",encrypt=rc4-40");
				if (encryption.getSelectedItem() == "RC4, 128bit") opts.append(",encrypt=rc4-128");
				if (encryption.getSelectedItem() == "AES, 128bit") opts.append(",encrypt=aes-128");
				if (encryption.getSelectedItem() == "AES, 256bit") opts.append(",encrypt=aes-256");
				if (userPassword.getText().length() > 0) {
					opts.append(",user-password=");
					opts.append(userPassword.getText());
				}
				if (ownerPassword.getText().length() > 0) {
					opts.append(",owner-password=");
					opts.append(ownerPassword.getText());
				}
				opts.append(",permissions=-1");
				if (garbageCollect.getState() && isPDF && isRedacted)
					opts.append(",garbage=yes");
				else
					opts.append(",garbage=compact");
			}

			if (opts.charAt(0) == ',')
				opts.deleteCharAt(0);

			options = opts.toString();
		}

		String getOptions() {
			return options;
		}
	}

	class Progressmeter extends Dialog implements ActionListener, KeyListener {
		Label info = new Label("", Label.CENTER);
		Button cancel = new Button("Cancel");
		boolean cancelled = false;
		boolean done = false;

		public Progressmeter(Frame parent, String title, boolean modal, String initialText) {
			super(parent, title, modal);

			setLayout(new GridLayout(2, 1));

			info.setText(initialText);
			add(info);

			cancel.addActionListener(this);
			cancel.addKeyListener(this);
			add(cancel);

			pack();
			setResizable(false);
			cancel.requestFocusInWindow();
		}

		public void actionPerformed(ActionEvent e) {
			if (e.getSource() == cancel)
				cancel();
		}

		public void keyPressed(KeyEvent e) { }
		public void keyReleased(KeyEvent e) { }

		public void keyTyped(KeyEvent e) {
			if (e.getKeyChar() == '\u001b')
				cancel();
		}

		public void cancel() {
			cancelled = true;
		}

		public void done() {
			done = true;
		}

		public boolean progress(String text) {
			info.setText(text);
			return cancelled || done;
		}
	}

	class OCRProgressmeter extends Progressmeter implements DocumentWriter.OCRListener {
		int pages;

		public OCRProgressmeter(Frame parent, String title, int pages) {
			super(parent, title, true, "Progress: Page 65535/65535: 100%");
			this.pages = pages;
			progress(-1, 0);
			setVisible(true);
		}

		public void done() {
			super.done();
			setVisible(false);
			dispose();
		}

		public boolean progress(int page, int percent) {
			StringBuilder text = new StringBuilder();

			if (page >= 0 || pages >= 0) {
				text.append("Page ");
				if (page >= 0)
					text.append(page + 1);
				else
					text.append("?");
			}
			if (pages >= 0) {
				text.append("/");
				text.append(pages);
				text.append(": ");
			}

			text.append(percent);
			text.append("%");

			return progress(text.toString());
		}
	}

	class RenderProgressmeter extends Progressmeter {
		Cookie cookie;

		public RenderProgressmeter(Frame parent, String title, Cookie cookie, final int update) {
			super(parent, title, false, "Progress: 100%");
			this.cookie = cookie;

			(new Thread() {
				public void run() {
					try {
						int slept = 0;
						while (!progress(slept))
						{
							sleep(update);
							slept += update;
						}
					} catch (InterruptedException e) {
					}
				}
			}).start();
		}

		public void cancel() {
			super.cancel();
			cookie.abort();
		}

		public boolean progress(int slept) {
			int progress = cookie.getProgress();
			int max = cookie.getProgressMax();

			if (max <= 0 && progress < 100)
				max = 100;
			else if (max <= 0 && progress > 100)
			{
				int v = progress;
				max = 10;
				while (v > 10)
				{
					v /= 10;
					max *= 10;
				}
			}

			if (progress >= max)
				done = true;

			int percent = (int) ((float) progress / max * 100.0f);

			StringBuilder text = new StringBuilder();
			text.append("Progress: ");
			text.append(percent);
			text.append("%");

			if (slept > 0)
				setVisible(true);

			if (progress(text.toString()))
			{
				setVisible(false);
				dispose();
				return true;
			}

			return false;
		}
	}

	public static void main(String[] args) {
		String selectedPath;

		if (args.length <= 0) {
			FileDialog fileDialog = new FileDialog((Frame)null, "MuPDF Open File", FileDialog.LOAD);
			fileDialog.setDirectory(System.getProperty("user.dir"));
			fileDialog.setFilenameFilter(new FilenameFilter() {
				public boolean accept(File dir, String name) {
					return Document.recognize(name);
				}
			});
			fileDialog.setVisible(true);
			if (fileDialog.getFile() == null)
				System.exit(0);
			selectedPath = new StringBuffer(fileDialog.getDirectory()).append(File.separatorChar).append(fileDialog.getFile()).toString();
			fileDialog.dispose();
		} else {
			selectedPath = args[0];
		}

		try {
			Viewer app = new Viewer(selectedPath);
			app.setVisible(true);
		} catch (Exception e) {
			messageBox(null, "MuPDF Error", "Cannot open \"" + selectedPath + "\": " + e.getMessage() + ".");
			System.exit(1);
		}
	}

	public float getRetinaScale() {
		// first try Oracle's VM (we should also test for 1.7.0_40 or higher)
		final String vendor = System.getProperty("java.vm.vendor");
		boolean isOracle = vendor != null && vendor.toLowerCase().contains("Oracle".toLowerCase());
		if (isOracle) {
			GraphicsEnvironment env = GraphicsEnvironment.getLocalGraphicsEnvironment();
			final GraphicsDevice device = env.getDefaultScreenDevice();
			try {
				Field field = device.getClass().getDeclaredField("scale");
				if (field != null) {
					field.setAccessible(true);
					Object scale = field.get(device);
					if (scale instanceof Integer && ((Integer)scale).intValue() == 2)
						return 2.0f;
				}
			}
			catch (Exception ignore) {
			}
			return 1.0f;
		}

		// try Apple VM
		final Float scaleFactor = (Float)Toolkit.getDefaultToolkit().getDesktopProperty("apple.awt.contentScaleFactor");
		if (scaleFactor != null && scaleFactor.intValue() == 2)
			return 2.0f;

		return 1.0f;
	}

	public int getScreenDPI() {
		try {
			return Toolkit.getDefaultToolkit().getScreenResolution();
		} catch (HeadlessException e) {
			return 72;
		}
	}

	public Dimension getScreenSize() {
		try {
			return Toolkit.getDefaultToolkit().getScreenSize();
		} catch (HeadlessException e) {
			return new Dimension(1920, 1080);
		}
	}

	protected static class OutlineItem {
		protected String title;
		protected String uri;
		protected int page;
		public OutlineItem(String title, String uri, int page) {
			this.title = title;
			this.uri = uri;
			this.page = page;
		}
		public String toString() {
			return title;
		}
	}

	public String askPassword() {
		return passwordDialog(null, "Password");
	}
	public void onChapterCountChange(int chapters) {
		this.chapters = chapters;
	}
	public void onPageCountChange(int pages) {
		this.pages = pages;
		pageLabel.setText("/ " + pages);
	}
	public void onPageChange(Location page, int chapterNumber, int pageNumber, Rect bbox) {
		this.location = page;
		this.chapterNumber = chapterNumber;
		this.pageNumber = pageNumber;
		this.bbox = bbox;
		if (pageNumber >= 0 && pageNumber < pages)
			pageField.setText(String.valueOf(pageNumber + 1));
		else
			pageField.setText("");
		render();
	}
	public void onReflowableChange(boolean reflowable) {
		this.reflowable = reflowable;
		fontIncButton.setEnabled(reflowable);
		fontDecButton.setEnabled(reflowable);
		fontSizeLabel.setEnabled(reflowable);
	}
	public void onLayoutChange(int width, int height, int em) {
	}
	public void onOutlineChange(ViewerCore.OutlineItem[] outline) {
		boolean hadOutline = this.outline != null;
		this.outline = outline;
		outlineList.removeAll();
		if (outline != null)
			for (int i = 0; i < outline.length; i++)
				outlineList.add(outline[i].title);
		if (hadOutline)
			outlinePanel.setVisible(outline != null);
	}
	public void onPageContentsChange(Pixmap pixmap, Rect[] links, String[] linkURIs, Quad[][] hits) {
		this.pixmap = pixmap;
		this.links = links;
		this.linkURIs = linkURIs;
		this.hits = hits;
		redraw();
		if (renderMeter != null)
			renderMeter.done();
	}
	public void onSearchStart(Location startPage, Location finalPage, int direction, String needle) {
		searchField.setEnabled(false);
	}
	public void onSearchPage(Location page, String needle) {
		String text;
		if (chapters > 1)
			text = "Searching " + (page.chapter + 1) + "/" + chapters + "-" + page.page + "/" + pages;
		else
			text = "Searching " + page.page + "/" + pages;
		searchStatus.setText(text);
		searchStatusPanel.validate();
	}
	public void onSearchStop(String needle, Location page) {
		searchField.setEnabled(true);
		searchNextButton.setEnabled(true);
		searchPrevButton.setEnabled(true);
		if (page != null) {
			doc.gotoLocation(page, null);
			searchStatus.setText("");
		} else if (needle != null)
			searchStatus.setText("Search text not found.");
		else
			searchStatus.setText("");
		searchStatusPanel.validate();
		pageCanvas.requestFocusInWindow();
	}
	public void onSearchCancelled() {
		searchField.setEnabled(true);
		searchNextButton.setEnabled(true);
		searchPrevButton.setEnabled(true);
		searchStatus.setText("");
		searchStatusPanel.validate();
	}
	public void onOutlineItemChange(int index) {
		if (index == -1) {
			int selected = outlineList.getSelectedIndex();
			if (selected >= 0)
				outlineList.deselect(selected);
		} else {
			outlineList.makeVisible(index);
			outlineList.select(index);
		}
	}
	public void onMetadataChange(String title, String author, String format, String encryption) {
		this.title = title;
		this.author = author;
		this.format = format;
		this.encryption = encryption;
	}
	public void onPermissionsChange(boolean print, boolean copy, boolean edit, boolean annotate, boolean form, boolean accessibility, boolean assemble, boolean printHq) {
		this.print = print;
		this.copy = copy;
		this.edit = edit;
		this.annotate = annotate;
		this.form = form;
		this.accessibility = accessibility;
		this.assemble = assemble;
		this.printHq = printHq;
	}
	public void onLinearizedChange(boolean linearized) {
		this.linearized = linearized;
	}
	public void onUpdatesChange(int updates, int firstUpdate) {
		this.updates = updates;
		this.firstUpdate = firstUpdate;
	}
}
