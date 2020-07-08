package example;

import com.artifex.mupdf.fitz.*;

import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

import java.io.File;
import java.io.FilenameFilter;
import java.lang.reflect.Field;
import java.util.Vector;

public class Viewer extends Frame implements WindowListener, ActionListener, ItemListener, KeyListener, MouseWheelListener
{
	protected String documentPath;
	protected Document doc;

	protected ScrollPane pageScroll;
	protected Panel pageHolder;
	protected ImageCanvas pageCanvas;
	protected Matrix pageCTM;

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
	protected int searchDirection = 1;
	protected Location searchPage = null;
	protected Location searchHitPage = null;
	protected Quad[] searchHits = new Quad[0];
	protected EventQueue eq = null;
	protected SearchTask searchTask = null;

	protected Panel outlinePanel;
	protected List outlineList;
	protected Vector<Outline> flatOutline;

	protected int pageCount;
	protected int chapterCount;
	protected Location currentPage = null;

	protected int zoomLevel = 5;
	protected int layoutWidth = 450;
	protected int layoutHeight = 600;
	protected int layoutEm = 12;
	protected float pixelScale;
	protected boolean currentInvert = false;
	protected boolean currentICC = true;
	protected int currentAA = 8;
	protected boolean isFullscreen;
	protected boolean currentTint = false;
	protected int tintBlack = 0x303030;
	protected int tintWhite = 0xFFFFF0;
	protected int currentRotate = 0;
	protected boolean currentOutline = false;
	protected boolean currentLinks = false;
	protected Link[] links = null;
	protected Rect pageBounds;

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

	protected static final int[] zoomList = {
		18, 24, 36, 54, 72, 96, 120, 144, 180, 216, 288
	};

	protected static BufferedImage imageFromPixmap(Pixmap pixmap) {
		int w = pixmap.getWidth();
		int h = pixmap.getHeight();
		BufferedImage image = new BufferedImage(w, h, BufferedImage.TYPE_3BYTE_BGR);
		image.setRGB(0, 0, w, h, pixmap.getPixels(), 0, w);
		return image;
	}

	protected static BufferedImage imageFromPage(Page page, Matrix ctm, boolean invert, boolean icc, int aa, boolean tint, int tintBlack, int tintWhite, int rotate) {
		Matrix trm = new Matrix(ctm).rotate(rotate);
		Rect bbox = page.getBounds().transform(trm);
		Pixmap pixmap = new Pixmap(ColorSpace.DeviceBGR, bbox, true);
		pixmap.clear(255);

		if (icc)
			Context.enableICC();
		else
			Context.disableICC();
		Context.setAntiAliasLevel(aa);

		DrawDevice dev = new DrawDevice(pixmap);
		page.run(dev, trm, null);
		dev.close();
		dev.destroy();

		if (invert) {
			pixmap.invertLuminance();
			pixmap.gamma(1 / 1.4f);
		}

		if (tint) {
			pixmap.tint(tintBlack, tintWhite);
		}

		BufferedImage image = imageFromPixmap(pixmap);
		pixmap.destroy();
		return image;
	}

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

	protected static String passwordDialog(Frame owner) {
		final Dialog box = new Dialog(owner, "Password", true);
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

	protected class ImageCanvas extends Canvas
	{
		protected BufferedImage image;

		public void setImage(BufferedImage image_) {
			image = image_;
			repaint();
		}

		public Dimension getPreferredSize() {
			if (image == null)
				return new Dimension(600, 700);

			return new Dimension(image.getWidth(), image.getHeight());
		}

		public void paint(Graphics g) {
			float imageScale = 1 / pixelScale;
			final Graphics2D g2d = (Graphics2D)g.create(0, 0, image.getWidth(), image.getHeight());
			g2d.scale(imageScale, imageScale);
			g2d.drawImage(image, 0, 0, null);

			if (currentPage.equals(searchHitPage) && searchHits != null) {
				g2d.setColor(new Color(1, 0, 0, 0.4f));
				for (int i = 0; i < searchHits.length; ++i) {
					Rect hit = new Rect(searchHits[i]).transform(pageCTM);
					g2d.fillRect((int)hit.x0, (int)hit.y0, (int)(hit.x1-hit.x0), (int)(hit.y1-hit.y0));
				}
			}

			if (links != null) {
				g2d.setColor(new Color(0, 0, 1, 0.1f));
				for (int i = 0; i < links.length; ++i) {
					Rect hit = new Rect(links[i].bounds).transform(pageCTM);
					g2d.fillRect((int)hit.x0, (int)hit.y0, (int)(hit.x1-hit.x0), (int)(hit.y1-hit.y0));
				}
			}

			g2d.dispose();
		}

		public Dimension getMinimumSize() { return getPreferredSize(); }
		public Dimension getMaximumSize() { return getPreferredSize(); }
		public void update(Graphics g) { paint(g); }
	}

	public Viewer(String documentPath) {
		this.documentPath = documentPath;

		pixelScale = getRetinaScale();
		setTitle("MuPDF: ");
		pageCount = 0;
		chapterCount = 0;

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
					pageLabel = new Label("/ " + pageCount);

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
					for (int i = 0; i < zoomList.length; ++i)
						zoomChoice.add(String.valueOf(zoomList[i]));
					zoomChoice.select(zoomLevel);
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
			}
			rightPanel.add(toolpane, BorderLayout.NORTH);
		}
		this.add(rightPanel, BorderLayout.EAST);

		pageScroll = new ScrollPane(ScrollPane.SCROLLBARS_AS_NEEDED);
		{
			pageHolder = new Panel(new GridBagLayout());
			{
				pageHolder.setBackground(Color.gray);
				pageCanvas = new ImageCanvas();
				pageHolder.add(pageCanvas);
			}
			pageCanvas.addKeyListener(this);
			pageCanvas.addMouseWheelListener(this);
			pageScroll.add(pageHolder);
		}
		this.add(pageScroll, BorderLayout.CENTER);

		addWindowListener(this);

		Toolkit toolkit = Toolkit.getDefaultToolkit();
		eq = toolkit.getSystemEventQueue();

		pack();

		reload();
	}

	public void dispose() {
		cancelSearch();
		super.dispose();
	}

	public void reload() {
		clearSearch();

		pageCanvas.requestFocusInWindow();

		String acceleratorPath = getAcceleratorPath(documentPath);
		if (acceleratorValid(documentPath, acceleratorPath))
			doc = Document.openDocument(documentPath, acceleratorPath);
		else
			doc = Document.openDocument(documentPath);

		if (doc.needsPassword()) {
			String pwd;
			do {
				pwd = passwordDialog(null);
				if (pwd == null)
					dispose();
			} while (!doc.authenticatePassword(pwd));
		}

		doc.layout(layoutWidth, layoutHeight, layoutEm);
		currentPage = doc.locationFromPageNumber(0);
		pageCount = doc.countPages();
		chapterCount = doc.countChapters();
		doc.saveAccelerator(acceleratorPath);

		setTitle("MuPDF: " + doc.getMetaData(Document.META_INFO_TITLE));
		pageLabel.setText("/ " + pageCount);
		reflowPanel.setVisible(doc.isReflowable());

		updateOutline();
		updatePageCanvas();
		pack();
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

			case KeyEvent.VK_PAGE_UP: flipPage(-1, number); break;
			case KeyEvent.VK_PAGE_DOWN: flipPage(+1, number); break;
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
		case 'r': reload(); break;
		case 'q': dispose(); break;

		case 'f': toggleFullscreen(); break;

		case 'm': mark(number); break;
		case 't': jumpHistoryBack(number); break;
		case 'T': jumpHistoryForward(number); break;

		case '>': relayout(number > 0 ? number : layoutEm + 1); break;
		case '<': relayout(number > 0 ? number : layoutEm - 1); break;

		case 'I': toggleInvert(); break;
		case 'E': toggleICC(); break;
		case 'A': toggleAA(); break;
		case 'C': toggleTint(); break;
		case 'o': toggleOutline(); break;
		case 'L': toggleLinks(); break;
		case 'i': showInfo(); break;

		case '[': rotate(-90); break;
		case ']': rotate(+90); break;

		case '+': zoom(false, +1); break;
		case '-': zoom(false, -1); break;
		case 'z': zoom(true, number); break;

		case 'k': pan(0, pageCanvas != null ? pageCanvas.getHeight() / -10 : -10); break;
		case 'j': pan(0, pageCanvas != null ? pageCanvas.getWidth() / +10 : +10); break;
		case 'h': pan(pageCanvas != null ? pageCanvas.getHeight() / -10 : -10, 0); break;
		case 'l': pan(pageCanvas != null ? pageCanvas.getWidth() / +10 : +10, 0); break;

		case 'b': smartMove(-1, number); break;
		case ' ': smartMove(+1, number); break;

		case ',': flipPage(-1, number); break;
		case '.': flipPage(+1, number); break;

		case 'g': jumpToPage(number - 1); break;
		case 'G': jumpToPage(pageCount - 1); break;

		case '/': editSearchNeedle(+1); break;
		case '?': editSearchNeedle(-1); break;
		case 'N': search(searchField.getText(), -1); break;
		case 'n': search(searchField.getText(), +1); break;
		case '\u001b': clearSearch(); break;
		}

		if (c >= '0' && c <= '9')
			number = number * 10 + c - '0';
		else
			number = 0;
	}

	public void mouseWheelMoved(MouseWheelEvent e) {
		int mod = e.getModifiers();
		int rot = e.getWheelRotation();
		if (mod == MouseWheelEvent.CTRL_MASK) {
			if (rot < 0)
				zoom(false, +1);
			else
				zoom(false, -1);
		} else if (mod == MouseWheelEvent.SHIFT_MASK) {
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

	protected void selectOutlineItem(int pagenum) {
		int best = -1;

		if (flatOutline != null) {
			for (int i = 0; i < flatOutline.size(); ++i) {
				Outline node = flatOutline.elementAt(i);
				int linkPage = doc.pageNumberFromLocation(doc.resolveLink(node));
				 if (linkPage >= 0 && linkPage <= pagenum)
					 best = i;
			}
		}

		if (best >= 0) {
			outlineList.select(best);
			outlineList.makeVisible(best);
		} else {
			int selected = outlineList.getSelectedIndex();
			if (selected >= 0)
				outlineList.deselect(selected);
		}
	}

	protected void addOutline(Outline[] outline, String indent) {
		for (int i = 0; i < outline.length; ++i) {
			Outline node = outline[i];
			if (node.title != null) {
				flatOutline.add(node);
				outlineList.add(indent + node.title);
			}
			if (node.down != null)
				addOutline(node.down, indent + "    ");
		}
	}

	protected void updateOutline() {
		Outline[] outline = null;
		try {
			if (doc != null)
				outline = doc.loadOutline();
		} catch (Exception ex) {
		}
		outlineList.removeAll();
		if (outline != null) {
			flatOutline = new Vector<Outline>();
			addOutline(outline, "");
			currentOutline = true;
		} else  {
			currentOutline = false;
		}
		outlinePanel.setVisible(currentOutline);
	}

	protected void updatePageCanvas() {
		int pageNumber = doc.pageNumberFromLocation(currentPage);
		pageField.setText(String.valueOf(pageNumber + 1));

		pageCTM = new Matrix().scale(zoomList[zoomLevel] / 72.0f * pixelScale);
		if (doc != null) {
			Page page = doc.loadPage(pageNumber);

			pageBounds = page.getBounds();

			if (currentLinks)
				links = page.getLinks();
			else
				links = null;


			BufferedImage image = imageFromPage(page, pageCTM, currentInvert, currentICC, currentAA, currentTint, tintBlack, tintWhite, currentRotate);
			pageCanvas.setImage(image);

			selectOutlineItem(pageNumber);
		}

		Dimension size = pageHolder.getPreferredSize();
		size.width += 40;
		size.height += 40;
		pageScroll.setPreferredSize(size);
		pageCanvas.invalidate();
		validate();
	}

	protected void editSearchNeedle(int direction) {
		clearSearch();
		searchDirection = direction;
		searchField.requestFocusInWindow();
	}

	protected void cancelSearch() {
		if (searchTask != null) {
			searchTask.cancel();
			searchTask = null;
		}
	}

	protected void clearSearch() {
		cancelSearch();
		searchField.setText("");
		pageCanvas.requestFocusInWindow();
		validate();
	}

	protected void search(String needle, int direction) {
		boolean done = false;

		if (doc == null || needle == null || needle.length() <= 0)
		{
			pageCanvas.requestFocusInWindow();
			validate();
			return;
		}

		if (searchTask != null) {
			searchTask.cancel();
			searchTask = null;
		}

		if (currentPage.equals(searchHitPage)) {
			Location finalPage = searchDirection > 0 ? doc.lastPage() : doc.locationFromPageNumber(0);
			searchPage = searchDirection > 0 ? doc.nextPage(currentPage) : doc.previousPage(currentPage);
			if (searchPage.equals(finalPage))
				done = true;
		} else {
			searchPage = currentPage;
		}

		pageCanvas.requestFocusInWindow();
		validate();

		searchHits = null;
		searchHitPage = null;

		if (chapterCount > 1)
			searchField.setText("Searching " + (searchPage.chapter + 1) + "/" + chapterCount + "-" + searchPage.page + "/" + pageCount);
		else
			searchField.setText("Searching " + searchPage.page + "/" + pageCount);
		searchField.setEditable(false);
		searchField.setEnabled(false);
		searchPrevButton.setEnabled(false);
		searchNextButton.setEnabled(false);

		searchTask = new SearchTask(needle, direction);
		eq.invokeLater(searchTask);
	}

	protected class SearchTask implements Runnable {
		protected boolean cancel;
		protected String needle;
		protected int direction;
		protected Location finalPage;

		protected SearchTask(String needle, int direction) {
			this.needle = needle;
			this.direction = direction;
			this.finalPage = searchDirection > 0 ? doc.lastPage() : doc.locationFromPageNumber(0);
			this.cancel = false;
		}

		private synchronized void requestCancellation() {
			cancel = true;
		}

		protected void cancel() {
			requestCancellation();
			while (!isCancelled()) {
				try {
					wait();
				} catch (InterruptedException e) {
				}
			}
		}

		protected synchronized boolean isCancelled() {
			return cancel;
		}

		protected synchronized void cancelled() {
			notify();
		}

		public void run() {
			long executionUntil = System.currentTimeMillis() + 100;
			boolean done = false;

			while (!done && !isCancelled() && System.currentTimeMillis() < executionUntil) {
				searchHits = doc.search(searchPage.chapter, searchPage.page, needle);
				if (searchHits != null && searchHits.length > 0) {
					searchHitPage = new Location(searchPage, searchHits[0].ul_x, searchHits[0].ul_y);
					jumpToLocation(searchHitPage);
					done = true;
				}
				else if (finalPage.equals(searchPage))
					done = true;
				else if (searchDirection > 0)
					searchPage = doc.nextPage(searchPage);
				else
					searchPage = doc.previousPage(searchPage);
			}

			if (isCancelled()) {
				searchField.setText(needle);
				searchField.setEditable(true);
				searchField.setEnabled(true);
				searchHits = null;
				searchHitPage = null;
				searchPrevButton.setEnabled(true);
				searchNextButton.setEnabled(true);
				cancelled();
			} else if (done) {
				searchField.setText(needle);
				searchField.setEditable(true);
				searchField.setEnabled(true);
				updatePageCanvas();
				pageCanvas.requestFocusInWindow();
				searchPrevButton.setEnabled(true);
				searchNextButton.setEnabled(true);
			} else {
				if (chapterCount > 1)
					searchField.setText("Searching " + (searchPage.chapter + 1) + "/" + chapterCount + "-" + searchPage.page + "/" + pageCount);
				else
					searchField.setText("Searching " + searchPage.page + "/" + pageCount);

				eq.invokeLater(this);
			}
		}
	}

	protected void zoom(boolean absolute, int number) {
		int newZoomLevel;

		if (absolute) {
			if (number == 0)
				newZoomLevel = 5;
			else
				newZoomLevel = number - 1;
		} else {
			if (number == 0)
				newZoomLevel = 5;
			else
				newZoomLevel = zoomLevel + number;
		}

		if (newZoomLevel < 0)
			newZoomLevel = 0;
		if (newZoomLevel >= zoomList.length)
			newZoomLevel = zoomList.length - 1;

		if (newZoomLevel == zoomLevel)
			return;

		zoomLevel = newZoomLevel;
		zoomChoice.select(newZoomLevel);
		updatePageCanvas();
	}

	protected void toggleInvert() {
		currentInvert = !currentInvert;
		updatePageCanvas();
	}

	protected boolean flipPage(int direction, int pages) {
		cancelSearch();

		if (pages < 1)
			pages = 1;

		Location loc = currentPage;
		for (int i = 0; i < pages; i++) {
			if (direction > 0)
				loc = doc.nextPage(loc);
			else
				loc = doc.previousPage(loc);
		}

		if (currentPage.equals(loc))
			return false;

		currentPage = loc;
		updatePageCanvas();
		return true;
	}

	protected void toggleICC() {
		currentICC = !currentICC;
		updatePageCanvas();
	}

	protected void toggleAA() {
		currentAA = number != 0 ? number : (currentAA == 8 ? 0 : 8);
		updatePageCanvas();
	}

	protected void toggleTint() {
		currentTint = !currentTint;
		updatePageCanvas();
	}

	protected void toggleOutline() {
		if (outlineList.getItemCount() <= 0)
			return;

		currentOutline = !currentOutline;
		outlinePanel.setVisible(currentOutline);
		pack();
		validate();
	}

	protected void toggleLinks() {
		currentLinks = !currentLinks;
		updatePageCanvas();
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
			//"S - save file (only for PDF)",
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
			//"w - shrink wrap window",
			//"W - fit to width",
			//"H - fit to height",
			//"Z - fit to page",
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

		String title = doc.getMetaData(Document.META_INFO_TITLE);
		if (title != null) rows++;

		String author = doc.getMetaData(Document.META_INFO_AUTHOR);
		if (author != null) rows++;

		String format = doc.getMetaData(Document.META_FORMAT);
		if (format != null) rows++;

		String encryption = doc.getMetaData(Document.META_ENCRYPTION);
		if (encryption != null) rows++;

		buffer = new StringBuffer();
		if (doc.hasPermission(Document.PERMISSION_PRINT))
			buffer.append("print, ");
		if (doc.hasPermission(Document.PERMISSION_COPY))
			buffer.append("copy, ");
		if (doc.hasPermission(Document.PERMISSION_EDIT))
			buffer.append("edit, ");
		if (doc.hasPermission(Document.PERMISSION_ANNOTATE))
			buffer.append("annotate, ");
		if (buffer.length() > 2)
			buffer.delete(buffer.length() - 2, buffer.length());
		String permissions = buffer.length() > 0 ? buffer.toString() : null;
		if (permissions != null) rows++;

		buffer = new StringBuffer();
		if (doc.isPDF()) {
			PDFDocument pdf = (PDFDocument) doc;
			int updates = pdf.countVersions();
			boolean linear = pdf.wasLinearized();

			buffer.append("PDF ");
			if (linear)
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
		if (doc.isPDF()) {
			PDFDocument pdf = (PDFDocument) doc;
			int updates = pdf.countVersions();

			if (updates > 1) {
				int n = pdf.validateChangeHistory();
				if (n == 0)
					buffer.append("Change history seems valid.");
				else if (n == 1)
					buffer.append("Invalid changes made to the document in the last update.");
				else if (n == 2)
					buffer.append("Invalid changes made to the document in the penultimate update.");
				else {
					buffer.append("Invalid changes made to the document ");
					buffer.append(n);
					buffer.append(" updates ago.");
				}
			}
		}
		String validation = buffer.length() > 0 ? buffer.toString() : null;
		if (validation != null) rows++;

		buffer = new StringBuffer();
		int w = (int)(pageBounds.x1 - pageBounds.x0 + 0.5f);
		int h = (int)(pageBounds.y1 - pageBounds.y0 + 0.5f);
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
		buffer.append(doc.pageNumberFromLocation(currentPage) + 1);
		buffer.append(" / ");
		buffer.append(pageCount);
		String page = buffer.length() > 0 ? buffer.toString() : null;
		if (page != null) rows++;

		String icc = currentICC ? "on" : "off";
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
		infoPanel.add(new Label("ICC rendering: " + icc));

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

	protected void rotate(int rotate) {
		currentRotate += rotate;
		updatePageCanvas();
	}

	protected void mark(int number) {
		if (number == 0)
			pushHistory();
		else if (number > 0 && number < marks.length)
			marks[number] = saveMark();
	}

	protected void jumpHistoryBack(int number) {
		if (number == 0) {
			if (historyCount > 0)
				popHistory();
		} else if (number > 0 && number < marks.length) {
			int page = doc.pageNumberFromLocation(marks[number].loc);
			restoreMark(marks[number]);
			jumpToPage(page);
		}
	}

	protected void jumpHistoryForward(int number) {
		if (number == 0) {
			if (futureCount > 0) {
				popFuture();
			}
		}
	}

	protected Mark saveMark() {
		return new Mark(currentPage);
	}

	protected void restoreMark(Mark mark) {
		currentPage = mark.loc;
		updatePageCanvas();
	}

	protected void pushHistory() {
		if (historyCount > 0 && currentPage.equals(history[historyCount - 1].loc))
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
		Location here = currentPage;
		pushFuture();
		while (historyCount > 0 && currentPage.equals(here))
			restoreMark(history[--historyCount]);
	}

	protected void popFuture() {
		Location here = currentPage;
		pushHistory();
		while (futureCount > 0 && currentPage.equals(here))
			restoreMark(future[--futureCount]);
	}

	protected void jumpToLocation(Location loc) {
		clearFuture();
		pushHistory();

		currentPage = loc;

		int page = doc.pageNumberFromLocation(currentPage);
		pageField.setText(String.valueOf(page));
		updatePageCanvas();

		pushHistory();
	}

	protected boolean jumpToPage(int page) {
		cancelSearch();

		clearFuture();
		pushHistory();

		if (page < 0)
			page = 0;
		if (page >= pageCount)
			page = pageCount - 1;

		if (doc.pageNumberFromLocation(currentPage) == page) {
			pushHistory();
			return false;
		}

		currentPage = doc.locationFromPageNumber(page);

		pageField.setText(String.valueOf(page));
		updatePageCanvas();
		pageCanvas.requestFocusInWindow();

		pushHistory();
		return true;
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
				} else if (flipPage(+1, 1)) {
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
				} else if (flipPage(-1, 1)) {
					vadj.setValue(vadj.getMaximum());
					hadj.setValue(hadj.getMaximum());
				}
			}
		}
	}

	protected void relayout(int em) {
		if (em < 6)
			em = 6;
		if (em > 36)
			em = 36;

		if (em == layoutEm)
			return;

		fontSizeLabel.setText(String.valueOf(em));

		layoutEm = em;
		long mark = doc.makeBookmark(currentPage);
		doc.layout(layoutWidth, layoutHeight, layoutEm);
		updateOutline();
		pageCount = doc.countPages();
		pageLabel.setText("/ " + pageCount);
		currentPage = doc.findBookmark(mark);
		updatePageCanvas();
	}

	public void actionPerformed(ActionEvent event) {
		Object source = event.getSource();
		int oldLayoutEm = layoutEm;
		int oldZoomLevel = zoomLevel;

		if (source == firstButton)
			jumpToPage(0);
		if (source == lastButton)
			jumpToLocation(doc.lastPage());
		if (source == prevButton)
			jumpToLocation(doc.previousPage(currentPage));
		if (source == nextButton)
			jumpToLocation(doc.nextPage(currentPage));
		if (source == pageField)
			jumpToPage(Integer.parseInt(pageField.getText()) - 1);

		if (source == searchField)
			search(searchField.getText(), +1);
		if (source == searchNextButton)
			search(searchField.getText(), +1);
		if (source == searchPrevButton)
			search(searchField.getText(), -1);

		if (source == fontIncButton && doc != null && doc.isReflowable())
			relayout(layoutEm + 1);
		if (source == fontDecButton && doc != null && doc.isReflowable())
			relayout(layoutEm - 1);

		if (source == zoomOutButton)
			zoom(false, -1);
		if (source == zoomInButton)
			zoom(false, +1);
	}

	public void itemStateChanged(ItemEvent event) {
		Object source = event.getSource();
		if (source == zoomChoice) {
			int oldZoomLevel = zoomLevel;
			zoomLevel = zoomChoice.getSelectedIndex();
			if (zoomLevel != oldZoomLevel)
				updatePageCanvas();
		}
		if (source == outlineList) {
			int i = outlineList.getSelectedIndex();
			Outline node = flatOutline.elementAt(i);
			Location loc = doc.resolveLink(node);
			jumpToLocation(loc);
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

	protected static String getAcceleratorPath(String documentPath) {
		String acceleratorName = documentPath.substring(1);
		acceleratorName = acceleratorName.replace(File.separatorChar, '%');
		acceleratorName = acceleratorName.replace('\\', '%');
		acceleratorName = acceleratorName.replace(':', '%');
		String tmpdir = System.getProperty("java.io.tmpdir");
		return new StringBuffer(tmpdir).append(File.separatorChar).append(acceleratorName).append(".accel").toString();
	}

	protected static boolean acceleratorValid(String documentPath, String acceleratorPath) {
		long documentModified = new File(documentPath).lastModified();
		long acceleratorModified = new File(acceleratorPath).lastModified();
		return acceleratorModified != 0 && acceleratorModified > documentModified;
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
					if (scale instanceof Integer && ((Integer)scale).intValue() == 2) {
						return 2.0f;
					}
				}
			}
			catch (Exception ignore) {
			}
			return 1.0f;
		}

		// try Apple VM
		final Float scaleFactor = (Float)Toolkit.getDefaultToolkit().getDesktopProperty("apple.awt.contentScaleFactor");
		if (scaleFactor != null && scaleFactor.intValue() == 2) {
			return 2.0f;
		}

		return 1.0f;
	}
}
