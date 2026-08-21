#! /usr/bin/env python3

'''
Basic PDF viewer using PyQt and MuPDF's Python bindings.

    Hot-keys in main window:
        +=  zooms in
        -_  zoom out
        0   reset zoom.
        Up/down, page-up/down   Scroll current page.
        Shift page-up/down      Move to next/prev page.

Command-line usage:

    -h
    --help
        Show this help.
    <path>
        Show specified PDF file.

Example usage:

    These examples build+install the MuPDF Python bindings into a Python
    virtual environment, which enables this script's 'import mupdf' to work
    without having to set PYTHONPATH.

    Linux:
        > python3 -m venv pylocal
        > . pylocal/bin/activate
        (pylocal) > pip install libclang pyqt5
        (pylocal) > cd .../mupdf
        (pylocal) > python setup.py install

        (pylocal) > python scripts/mupdfwrap_gui.py

    Windows (in a Cmd terminal):
        > py -m venv pylocal
        > pylocal\Scripts\activate
        (pylocal) > pip install libclang pyqt5
        (pylocal) > cd ...\mupdf
        (pylocal) > python setup.py install

        (pylocal) > python scripts\mupdfwrap_gui.py

    OpenBSD:
        # It seems that pip can't install py1t5 or libclang so instead we
        # install system packages and use --system-site-packages.]

        > sudo pkg_add py3-llvm py3-qt5
        > python3 -m venv --system-site-packages pylocal
        > . pylocal/bin/activate
        (pylocal) > cd .../mupdf
        (pylocal) > python setup.py install

        (pylocal) > python scripts/mupdfwrap_gui.py

'''

import os
import sys

import mupdf

import PyQt5
import PyQt5.Qt
import PyQt5.QtCore
import PyQt5.QtWidgets


class MainWindow(PyQt5.QtWidgets.QMainWindow):

    def __init__(self):
        super().__init__()

        # Set up default state. Zooming works by incrementing self.zoom by +/-
        # 1 then using magnification = 2**(self.zoom/self.zoom_multiple).
        #
        self.page_number = None
        self.zoom_multiple = 4
        self.zoom = 0

        # Create Qt widgets.
        #
        self.central_widget = PyQt5.QtWidgets.QLabel(self)
        self.scroll_area = PyQt5.QtWidgets.QScrollArea()
        self.scroll_area.setWidget(self.central_widget)
        self.scroll_area.setWidgetResizable(True)
        self.setCentralWidget(self.scroll_area)
        self.central_widget.setToolTip(
                '+=  zoom in.\n'
                '-_  zoom out.\n'
                '0   zoom reset.\n'
                'Shift-page-up  prev page.\n'
                'Shift-page-down  next page.\n'
                )

        # Create menus.
        #
        # Need to store menu actions in self, otherwise they appear to get
        # destructed and so don't appear in the menu.
        #
        self.menu_file_open = PyQt5.QtWidgets.QAction('&Open...')
        self.menu_file_open.setToolTip('Open a new PDF.')
        self.menu_file_open.triggered.connect(self.open_)
        self.menu_file_open.setShortcut(PyQt5.QtGui.QKeySequence("Ctrl+O"))

        self.menu_file_show_html = PyQt5.QtWidgets.QAction('&Show html')
        self.menu_file_show_html.setToolTip('Convert to HTML and show in separate window.')
        self.menu_file_show_html.triggered.connect(self.show_html)

        self.menu_file_quit = PyQt5.QtWidgets.QAction('&Quit')
        self.menu_file_quit.setToolTip('Exit the application.')
        self.menu_file_quit.triggered.connect(self.quit)
        self.menu_file_quit.setShortcut(PyQt5.QtGui.QKeySequence("Ctrl+Q"))

        menu_file = self.menuBar().addMenu('&File')
        menu_file.setToolTipsVisible(True)
        menu_file.addAction(self.menu_file_open)
        menu_file.addAction(self.menu_file_show_html)
        menu_file.addAction(self.menu_file_quit)

    def keyPressEvent(self, event):
        if self.page_number is None:
            #print(f'self.page_number is None')
            return
        #print(f'event.key()={event.key()}')
        # Qt Seems to intercept up/down and page-up/down itself.
        modifiers = PyQt5.QtWidgets.QApplication.keyboardModifiers()
        #print(f'modifiers={modifiers}')
        shift = (modifiers == PyQt5.QtCore.Qt.ShiftModifier)
        if 0:
            pass
        elif shift and event.key() == PyQt5.Qt.Qt.Key_PageUp:
            self.goto_page(page_number=self.page_number - 1)
        elif shift and event.key() == PyQt5.Qt.Qt.Key_PageDown:
            self.goto_page(page_number=self.page_number + 1)
        elif event.key() in (ord('='), ord('+')):
            self.goto_page(zoom=self.zoom + 1)
        elif event.key() in (ord('-'), ord('_')):
            self.goto_page(zoom=self.zoom - 1)
        elif event.key() == (ord('0')):
            self.goto_page(zoom=0)

    def resizeEvent(self, event):
        self.goto_page(self.page_number, self.zoom)

    def show_html(self):
        '''
        Convert to HTML using Extract, and show in new window using
        PyQt5.QtWebKitWidgets.QWebView.
        '''
        buffer_ = self.page.fz_new_buffer_from_page_with_format(
                format="docx",
                options="html",
                transform=mupdf.FzMatrix(1, 0, 0, 1, 0, 0),
                cookie=mupdf.FzCookie(),
                )
        html_content = buffer_.fz_buffer_extract().decode('utf8')
        # Show in a new window using Qt's QWebView.
        self.webview = PyQt5.QtWebKitWidgets.QWebView()
        self.webview.setHtml(html_content)
        self.webview.show()

    def open_(self):
        '''
        Opens new PDF file, using Qt file-chooser dialogue.
        '''
        path, _ = PyQt5.QtWidgets.QFileDialog.getOpenFileName(self, 'Open', filter='*.pdf')
        if path:
            self.open_path(path)

    def open_path(self, path):
        path = os.path.abspath(path)
        try:
            self.document = mupdf.FzDocument(path)
        except Exception as e:
            print(f'Failed to open path={path!r}: {e}')
            return
        self.setWindowTitle(path)
        self.goto_page(page_number=0, zoom=0)

    def quit(self):
        # fixme: should probably use qt to exit?
        sys.exit()

    def goto_page(self, page_number=None, zoom=None):
        '''
        Updates display to show specified page number and zoom level,
        defaulting to current values if None.

        Updates self.page_number and self.zoom if we are successful.
        '''
        # Recreate the bitmap that we are displaying. We should probably use a
        # mupdf.FzDisplayList to avoid processing the page each time we need to
        # change zoom etc.
        #
        # We can run out of memory for large zoom values; should probably only
        # create bitmap for the visible region (or maybe slightly larger than
        # the visible region to allow for some limited scrolling?).
        #
        if page_number is None:
            page_number = self.page_number
        if zoom is None:
            zoom = self.zoom
        if page_number is None or page_number < 0 or page_number >= self.document.fz_count_pages():
            return
        self.page = mupdf.FzPage(self.document, page_number)
        page_rect = self.page.fz_bound_page()
        z = 2**(zoom / self.zoom_multiple)

        # For now we always use 'fit width' view semantics.
        #
        # Using -2 here avoids always-present horizontal scrollbar; not sure
        # why...
        z *= (self.centralWidget().size().width() - 2) / (page_rect.x1 - page_rect.x0)

        # Need to preserve the pixmap after we return because the Qt image will
        # refer to it, so we use self.pixmap.
        try:
            self.pixmap = self.page.fz_new_pixmap_from_page_contents(
                    ctm=mupdf.FzMatrix(z, 0, 0, z, 0, 0),
                    cs=mupdf.FzColorspace(mupdf.FzColorspace.Fixed_RGB),
                    alpha=0,
                    )
        except Exception as e:
            print(f'self.page.fz_new_pixmap_from_page_contents() failed: {e}')
            return
        image = PyQt5.QtGui.QImage(
                int(self.pixmap.fz_pixmap_samples()),
                self.pixmap.fz_pixmap_width(),
                self.pixmap.fz_pixmap_height(),
                self.pixmap.fz_pixmap_stride(),
                PyQt5.QtGui.QImage.Format_RGB888,
                );
        qpixmap = PyQt5.QtGui.QPixmap.fromImage(image)
        self.central_widget.setPixmap(qpixmap)
        self.page_number = page_number
        self.zoom = zoom


def main():

    app = PyQt5.QtWidgets.QApplication([])
    main_window = MainWindow()

    args = iter(sys.argv[1:])
    while 1:
        try:
            arg = next(args)
        except StopIteration:
            break
        if arg.startswith('-'):
            if arg in ('-h', '--help'):
                print(__doc__)
                return
            elif arg == '--html':
                main_window.show_html()
            else:
                raise Exception(f'Unrecognised option {arg!r}')
        else:
            main_window.open_path(arg)

    main_window.show()
    app.exec_()

if __name__ == '__main__':
    main()
