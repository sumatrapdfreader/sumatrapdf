package example;

import com.artifex.mupdf.fitz.*;

import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

import java.io.File;
import java.io.FilenameFilter;
import java.lang.reflect.Field;
import java.util.Vector;

public class Viewer extends Frame implements WindowListener, ActionListener, ItemListener, KeyListener, TextListener
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
	protected int searchDirection = 0;
	protected int searchHitPage = -1;
	protected Quad[] searchHits;

	protected List outlineList;
	protected Vector<Outline> flatOutline;

	protected int pageCount;
	protected int pageNumber = 0;
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

	protected int number = 0;

	protected class Mark {
		int pageNumber;

		protected Mark(int pageNumber) {
			this.pageNumber = pageNumber;
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

			if (searchHitPage == pageNumber && searchHits != null && searchHits.length > 0) {
				g2d.setColor(new Color(1, 0, 0, 0.4f));
				for (int i = 0; i < searchHits.length; ++i) {
					Rect hit = new Rect(searchHits[i]).transform(pageCTM);
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
					searchField.addTextListener(this);
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

			outlineList = new List();
			outlineList.addItemListener(this);
			rightPanel.add(outlineList, BorderLayout.CENTER);
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
			pageScroll.add(pageHolder);
		}
		this.add(pageScroll, BorderLayout.CENTER);

		addWindowListener(this);

		pack();

		reload();
	}

	public void reload() {
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
		pageCount = doc.countPages();
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
	}

	public void keyTyped(KeyEvent e) {
		if (e.getSource() == pageCanvas)
			canvasKeyTyped(e);
		else if (e.getSource() == searchField)
			searchFieldKeyTyped(e);
	}

	protected void searchFieldKeyTyped(KeyEvent e) {
		if (e.getExtendedKeyCodeForChar(e.getKeyChar()) == java.awt.event.KeyEvent.VK_ESCAPE) {
			pageCanvas.requestFocusInWindow();
		}
	}

	protected void canvasKeyTyped(KeyEvent e) {
		char c = e.getKeyChar();

		switch(c)
		{
		case 'r': reload(); break;
		case 'q': dispose(); break;

		case 'f': doFullscreen(); break;

		case 'm': doMark(number); break;
		case 't': doHistoryBack(number); break;
		case 'T': doHistoryForward(number); break;

		case '>': doRelayout(number > 0 ? number : layoutEm + 1); break;
		case '<': doRelayout(number > 0 ? number : layoutEm - 1); break;

		case 'I': doInvert(); break;
		case 'E': doICC(); break;
		case 'A': doAA(); break;
		case 'C': doTint(); break;

		case '[': doRotate(-90); break;
		case ']': doRotate(+90); break;

		case '+': doZoom(1); break;
		case '-': doZoom(-1); break;

		case 'k': doPan(0, -10); break;
		case 'j': doPan(0, 10); break;
		case 'h': doPan(-10, 0); break;
		case 'l': doPan(10, 0); break;

		case 'b': doSmartMove(-1, number); break;
		case ' ': doSmartMove(+1, number); break;

		case ',': doFlipPage(-1, number); break;
		case '.': doFlipPage(+1, number); break;

		case 'g': doJumpToPage(number - 1); break;
		case 'G': doJumpToPage(pageCount - 1); break;

		case '/': doPreSearch(1); break;
		case '?': doPreSearch(-1); break;
		case 'N': doSearch(-1); break;
		case 'n': doSearch(1); break;
		case '\u001b': clearSearch(); break;
		}

		if (c >= '0' && c <= '9')
			number = number * 10 + c - '0';
		else
			number = 0;
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
			outlineList.setVisible(true);
		} else  {
			outlineList.setVisible(false);
		}
	}

	protected void updatePageCanvas() {
		pageField.setText(String.valueOf(pageNumber + 1));

		pageCTM = new Matrix().scale(zoomList[zoomLevel] / 72.0f * pixelScale);
		if (doc != null) {
			BufferedImage image = imageFromPage(doc.loadPage(pageNumber), pageCTM, currentInvert, currentICC, currentAA, currentTint, tintBlack, tintWhite, currentRotate);
			pageCanvas.setImage(image);
		}

		Dimension size = pageHolder.getPreferredSize();
		size.width += 40;
		size.height += 40;
		pageScroll.setPreferredSize(size);
		pageCanvas.invalidate();
		validate();
	}

	protected void doPreSearch(int direction) {
		searchDirection = direction;
		searchField.setText("");
		searchField.requestFocusInWindow();
	}

	protected void doSearch(int direction) {
		searchDirection = direction;
		doSearch();
	}

	protected void clearSearch() {
		if (doc == null)
			return;

		searchField.setText("");
		searchField.validate();
	}

	protected void doSearch() {
		if (doc == null)
			return;

		int searchPage;
		if (searchDirection == 0)
			searchDirection = 1;
		if (searchHitPage == -1)
			searchPage = pageNumber;
		else
			searchPage = pageNumber + searchDirection;
		searchHitPage = -1;
		String needle = searchField.getText();
		while (searchPage >= 0 && searchPage < pageCount) {
			Page page = doc.loadPage(searchPage);
			searchHits = page.search(needle);
			page.destroy();
			if (searchHits != null && searchHits.length > 0) {
				searchHitPage = searchPage;
				pageNumber = searchPage;
				updatePageCanvas();
				break;
			}
			searchPage += searchDirection;
		}
	}

	protected void doZoom(int i) {
		int newZoomLevel = zoomLevel + i;
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

	protected void doInvert() {
		currentInvert = !currentInvert;
		updatePageCanvas();
	}

	protected boolean doFlipPage(int direction, int pages) {
		if (pages < 1)
			pages = 1;

		int page = pageNumber + direction * pages;
		if (page < 0)
			page = 0;
		if (page >= pageCount)
			page = pageCount - 1;

		if (page == pageNumber)
			return false;

		pageNumber = page;
		updatePageCanvas();
		return true;
	}

	protected void doICC() {
		currentICC = !currentICC;
		updatePageCanvas();
	}

	protected void doAA() {
		currentAA = number != 0 ? number : (currentAA == 8 ? 0 : 8);
		updatePageCanvas();
	}

	protected void doTint() {
		currentTint = !currentTint;
		updatePageCanvas();
	}

	protected void doFullscreen() {
		isFullscreen = !isFullscreen;

		if (isFullscreen)
			setExtendedState(Frame.MAXIMIZED_BOTH);
		else
			setExtendedState(Frame.NORMAL);
	}

	protected void doRotate(int rotate) {
		currentRotate += rotate;
		updatePageCanvas();
	}

	protected void doMark(int number) {
		if (number == 0)
			pushHistory();
		else if (number > 0 && number < marks.length)
			marks[number] = saveMark();
	}

	protected void doHistoryBack(int number) {
		if (number == 0) {
			if (historyCount > 0)
				popHistory();
		} else if (number > 0 && number < marks.length) {
			int mark = marks[number].pageNumber;
			restoreMark(marks[number]);
			doJumpToPage(mark);
		}
	}

	protected void doHistoryForward(int number) {
		if (number == 0) {
			if (futureCount > 0) {
				popFuture();
			}
		}
	}

	protected Mark saveMark() {
		return new Mark(pageNumber);
	}

	protected void restoreMark(Mark mark) {
		pageNumber = mark.pageNumber;
		updatePageCanvas();
	}

	protected void pushHistory() {
		int here = pageNumber;

		if (historyCount > 0 && pageNumber == history[historyCount - 1].pageNumber)
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
		int here = pageNumber;
		pushFuture();
		while (historyCount > 0 && pageNumber == here)
			restoreMark(history[--historyCount]);
	}

	protected void popFuture() {
		int here = pageNumber;
		pushHistory();
		while (futureCount > 0 && pageNumber == here)
			restoreMark(future[--futureCount]);
	}

	protected boolean doJumpToPage(int page) {
		clearFuture();
		pushHistory();

		if (page < 0)
			page = 0;
		if (page >= pageCount)
			page = pageCount - 1;

		if (page == pageNumber) {
			pushHistory();
			return false;
		}

		pageNumber = page;
		updatePageCanvas();

		pushHistory();
		return true;
	}

	protected void doPan(int panx, int pany) {
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

	protected void doSmartMove(int direction, int moves) {
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
				} else if (doFlipPage(+1, 1)) {
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
				} else if (doFlipPage(-1, 1)) {
					vadj.setValue(vadj.getMaximum());
					hadj.setValue(hadj.getMaximum());
				}
			}
		}
	}

	protected void doRelayout(int em) {
		if (em < 6)
			em = 6;
		if (em > 36)
			em = 36;

		if (em == layoutEm)
			return;

		fontSizeLabel.setText(String.valueOf(em));

		layoutEm = em;
		long mark = doc.makeBookmark(doc.locationFromPageNumber(pageNumber));
		doc.layout(layoutWidth, layoutHeight, layoutEm);
		updateOutline();
		pageCount = doc.countPages();
		pageLabel.setText("/ " + pageCount);
		pageNumber = doc.pageNumberFromLocation(doc.findBookmark(mark));
		updatePageCanvas();
	}

	public void textValueChanged(TextEvent event) {
		Object source = event.getSource();
		if (source == searchField) {
			searchHitPage = -1;
		}
	}

	public void actionPerformed(ActionEvent event) {
		Object source = event.getSource();
		int oldPageNumber = pageNumber;
		int oldLayoutEm = layoutEm;
		int oldZoomLevel = zoomLevel;
		Quad[] oldSearchHits = searchHits;

		if (source == firstButton)
			pageNumber = 0;

		if (source == lastButton)
			pageNumber = pageCount - 1;

		if (source == prevButton) {
			pageNumber = pageNumber - 1;
			if (pageNumber < 0)
				pageNumber = 0;
		}

		if (source == nextButton) {
			pageNumber = pageNumber + 1;
			if (pageNumber >= pageCount)
				pageNumber = pageCount - 1;
		}

		if (source == pageField) {
			pageNumber = Integer.parseInt(pageField.getText()) - 1;
			if (pageNumber < 0)
				pageNumber = 0;
			if (pageNumber >= pageCount)
				pageNumber = pageCount - 1;
			pageField.setText(String.valueOf(pageNumber));
		}

		if (source == searchField)
		{
			pageCanvas.requestFocusInWindow();
			doSearch();
		}
		if (source == searchNextButton)
		{
			pageCanvas.requestFocusInWindow();
			searchDirection = 1;
			doSearch();
		}
		if (source == searchPrevButton)
		{
			pageCanvas.requestFocusInWindow();
			searchDirection = -1;
			doSearch();
		}

		if (source == fontIncButton && doc != null && doc.isReflowable())
			doRelayout(layoutEm + 1);

		if (source == fontDecButton && doc != null && doc.isReflowable())
			doRelayout(layoutEm - 1);

		if (source == zoomOutButton)
			doZoom(-1);
		if (source == zoomInButton)
			doZoom(1);

		if (pageNumber != oldPageNumber || searchHits != oldSearchHits)
			updatePageCanvas();
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
			int linkPage = doc.pageNumberFromLocation(doc.resolveLink(node));
			if (linkPage >= 0) {
				if (linkPage != pageNumber) {
					pageNumber = linkPage;
					updatePageCanvas();
				}
			}
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
