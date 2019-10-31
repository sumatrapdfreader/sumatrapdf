package example;

import com.artifex.mupdf.fitz.*;

import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

import java.io.File;
import java.io.FilenameFilter;
import java.lang.reflect.Field;
import java.util.Vector;
import java.util.Date;

public class Viewer extends Frame implements WindowListener, ActionListener, ItemListener, TextListener
{
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
	protected Button fontIncButton, fontDecButton;
	protected Label fontSizeLabel;

	protected TextField searchField;
	protected Button searchPrevButton, searchNextButton;
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

	protected static BufferedImage imageFromPage(Page page, Matrix ctm) {
		Rect bbox = page.getBounds().transform(ctm);
		Pixmap pixmap = new Pixmap(ColorSpace.DeviceBGR, bbox, true);
		pixmap.clear(255);

		DrawDevice dev = new DrawDevice(pixmap);
		page.run(dev, ctm, null);
		dev.close();
		dev.destroy();

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

	public Viewer(Document doc_) {
		this.doc = doc_;

		pixelScale = getRetinaScale();
		setTitle("MuPDF: " + doc.getMetaData(Document.META_INFO_TITLE));
		doc.layout(layoutWidth, layoutHeight, layoutEm);
		pageCount = doc.countPages();

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

				if (doc.isReflowable()) {
					toolbar = new Panel(new FlowLayout(FlowLayout.LEFT));
					{
						fontDecButton = new Button("Font-");
						fontDecButton.addActionListener(this);
						fontIncButton = new Button("Font+");
						fontIncButton.addActionListener(this);
						fontSizeLabel = new Label(String.valueOf(layoutEm));

						toolbar.add(fontDecButton);
						toolbar.add(fontSizeLabel);
						toolbar.add(fontIncButton);
					}
					c.gridy += 1;
					toolpane.add(toolbar, c);
				}

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
			pageScroll.add(pageHolder);
		}
		this.add(pageScroll, BorderLayout.CENTER);

		addWindowListener(this);

		updateOutline();
		updatePageCanvas();

		pack();
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
		Outline[] outline;
		try {
			outline = doc.loadOutline();
		} catch (Exception ex) {
			outline = null;
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
		BufferedImage image = imageFromPage(doc.loadPage(pageNumber), pageCTM);
		pageCanvas.setImage(image);

		Dimension size = pageHolder.getPreferredSize();
		size.width += 40;
		size.height += 40;
		pageScroll.setPreferredSize(size);
		pageCanvas.invalidate();
		validate();
	}

	protected void doSearch(int direction) {
		int searchPage;
		if (searchHitPage == -1)
			searchPage = pageNumber;
		else
			searchPage = pageNumber + direction;
		searchHitPage = -1;
		String needle = searchField.getText();
		while (searchPage >= 0 && searchPage < pageCount) {
			Page page = doc.loadPage(searchPage);
			searchHits = page.search(needle);
			page.destroy();
			if (searchHits != null && searchHits.length > 0) {
				searchHitPage = searchPage;
				pageNumber = searchPage;
				break;
			}
			searchPage += direction;
		}
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
			doSearch(1);
		if (source == searchNextButton)
			doSearch(1);
		if (source == searchPrevButton)
			doSearch(-1);

		if (source == fontIncButton && doc.isReflowable()) {
			layoutEm += 1;
			if (layoutEm > 36)
				layoutEm = 36;
			fontSizeLabel.setText(String.valueOf(layoutEm));
		}

		if (source == fontDecButton && doc.isReflowable()) {
			layoutEm -= 1;
			if (layoutEm < 6)
				layoutEm = 6;
			fontSizeLabel.setText(String.valueOf(layoutEm));
		}

		if (source == zoomOutButton) {
			zoomLevel -= 1;
			if (zoomLevel < 0)
				zoomLevel = 0;
			zoomChoice.select(zoomLevel);
		}

		if (source == zoomInButton) {
			zoomLevel += 1;
			if (zoomLevel >= zoomList.length)
				zoomLevel = zoomList.length - 1;
			zoomChoice.select(zoomLevel);
		}

		if (layoutEm != oldLayoutEm) {
			long mark = doc.makeBookmark(doc.locationFromPageNumber(pageNumber));
			doc.layout(layoutWidth, layoutHeight, layoutEm);
			updateOutline();
			pageCount = doc.countPages();
			pageLabel.setText("/ " + pageCount);
			pageNumber = doc.pageNumberFromLocation(doc.findBookmark(mark));
		}

		if (zoomLevel != oldZoomLevel || pageNumber != oldPageNumber || layoutEm != oldLayoutEm || searchHits != oldSearchHits)
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
		String acceleratorPath = documentPath.substring(1);
		acceleratorPath = acceleratorPath.replace(File.separatorChar, '%');
		acceleratorPath = acceleratorPath.replace('\\', '%');
		acceleratorPath = acceleratorPath.replace(':', '%');

		String tmpdir = System.getProperty("java.io.tmpdir");
		return new StringBuffer(tmpdir).append(File.separatorChar).append(acceleratorPath).append(".accel").toString();
	}

	protected static boolean acceleratorValid(File documentFile, File acceleratorFile) {
		long documentModified = documentFile.lastModified();
		long acceleratorModified = acceleratorFile.lastModified();

		return acceleratorModified != 0 && acceleratorModified > documentModified;
	}

	public static void main(String[] args) {
		File selectedFile;

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
			selectedFile = new File(fileDialog.getDirectory(), fileDialog.getFile());
			fileDialog.dispose();
		} else {
			selectedFile = new File(args[0]);
		}

		try {
			String documentPath = selectedFile.getAbsolutePath();
			String acceleratorPath = getAcceleratorPath(documentPath);

			Document doc;
			if (acceleratorValid(selectedFile, new File(acceleratorPath)))
				doc = Document.openDocument(documentPath, acceleratorPath);
			else
				doc = Document.openDocument(documentPath);

			if (doc.needsPassword()) {
				String pwd;
				do {
					pwd = passwordDialog(null);
					if (pwd == null)
						System.exit(1);
				} while (!doc.authenticatePassword(pwd));
			}

			doc.countPages();
			doc.saveAccelerator(acceleratorPath);

			Viewer app = new Viewer(doc);
			app.setVisible(true);
			return;
		} catch (Exception e) {
			messageBox(null, "MuPDF Error", "Cannot open \"" + selectedFile + "\": " + e.getMessage() + ".");
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
