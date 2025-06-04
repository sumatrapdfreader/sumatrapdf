// Basic PDF viewer using MuPDF C# bindings.
//

public class MuPDFGui : System.Windows.Forms.Form
{
    // We use static pixmap to ensure it isn't garbage-collected.
    mupdf.FzPixmap pixmap;

    private System.Windows.Forms.MainMenu menu;
    private System.Windows.Forms.MenuItem menu_item_file;

    /* Zooming works by incrementing self.zoom by +/- 1 then using
    magnification = 2**(self.zoom/self.zoom_multiple). */
    private int     zoom_multiple = 4;
    private double  zoom = 0;
    private int     page_number = 0;

    mupdf.FzDocument                document;
    mupdf.FzPage                    page;
    System.Drawing.Bitmap           bitmap;
    System.Windows.Forms.PictureBox picture_box;

    // Need STAThread here otherwise OpenFileDialog hangs.
    [System.STAThread]
    public static void Main()
    {
        System.Windows.Forms.Application.Run(new MuPDFGui());
    }

    public MuPDFGui()
    {

        menu_item_file = new System.Windows.Forms.MenuItem("File",
                new System.Windows.Forms.MenuItem[]
                {
                    new System.Windows.Forms.MenuItem("&Open...", new System.EventHandler(this.open)),
                    new System.Windows.Forms.MenuItem("&Show html", new System.EventHandler(this.show_html)),
                    new System.Windows.Forms.MenuItem("&Quit", new System.EventHandler(this.quit))
                }
                );
        menu = new System.Windows.Forms.MainMenu(new System.Windows.Forms.MenuItem [] {menu_item_file});
        this.Menu = menu;

        Resize += handle_resize;
        KeyDown += handle_key_down;

        this.picture_box = new System.Windows.Forms.PictureBox();
        this.picture_box.SizeMode = System.Windows.Forms.PictureBoxSizeMode.AutoSize;
        this.AutoScroll = true;

        Controls.Add(picture_box);

        this.open_file("zlib.3.pdf");
    }

    public void open(System.Object sender, System.EventArgs e)
    {
        var dialog = new System.Windows.Forms.OpenFileDialog();
        var result = dialog.ShowDialog();
        if (result == System.Windows.Forms.DialogResult.OK)
        {
            this.open_file(dialog.FileName);
        }
    }

    void open_file(string path)
    {
        try
        {
            this.document = new mupdf.FzDocument(path);
        }
        catch (System.Exception e)
        {
            System.Console.WriteLine("Failed to open: " + path + " because: " + e);
            return;
        }
        this.goto_page(0, 0);
    }

    public void show_html(System.Object sender, System.EventArgs e)
    {
        System.Console.WriteLine("ShowHtml() called");
        var buffer = this.page.fz_new_buffer_from_page_with_format(
                "docx",
                "html",
                new mupdf.FzMatrix(1, 0, 0, 1, 0, 0),
                new mupdf.FzCookie()
                );
        System.Console.WriteLine("buffer=" + buffer);
        var html_bytes = buffer.fz_buffer_extract();
        var html_string = System.Text.Encoding.UTF8.GetString(html_bytes, 0, html_bytes.Length);
        var web_browser = new System.Windows.Forms.WebBrowser();
        web_browser.DocumentText = html_string;
        web_browser.Show();
    }

    public void quit(System.Object sender, System.EventArgs e)
    {
        System.Console.WriteLine("Quit() called");
        System.Windows.Forms.Application.Exit();
    }

    // Shows page. If width and/or height are zero we use .Width and/or .Height.
    //
    // To preserve current page and/or zoom, use .page_number and/or .zoom.
    //
    public void goto_page(int page_number, double zoom)
    {
        if (page_number < 0 || page_number >= document.fz_count_pages())
        {
            return;
        }

        this.zoom = zoom;
        this.page_number = page_number;
        this.page = document.fz_load_page(page_number);

        var z = System.Math.Pow(2, this.zoom / this.zoom_multiple);

        /* For now we always use 'fit width' view semantics. */
        var page_rect = this.page.fz_bound_page();
        var vscroll_width = System.Windows.Forms.SystemInformation.VerticalScrollBarWidth;
        z *= (this.ClientSize.Width - vscroll_width) / (page_rect.x1 - page_rect.x0);

        if (System.Type.GetType("Mono.Runtime") != null)
        {
            /* Use pixmap data without copying. This does not work on
            Windows.

            It looks like it's important to use MuPDF Fixed_RGB with
            alpha=1, and C#'s Format32bppRgb. Other combinations,
            e.g. (Fixed_RGB with alpha=0) and Format24bppRgb, result in a
            blank display. */
            var stopwatch = new System.Diagnostics.Stopwatch();
            stopwatch.Reset();
            stopwatch.Start();
            this.pixmap = this.page.fz_new_pixmap_from_page_contents(
                    new mupdf.FzMatrix((float) z, 0, 0, (float) z, 0, 0),
                    new mupdf.FzColorspace(mupdf.FzColorspace.Fixed.Fixed_RGB),
                    1 /*alpha*/
                    );
            stopwatch.Stop();
            var t_pixmap = stopwatch.Elapsed;

            stopwatch.Reset();
            stopwatch.Start();
            this.bitmap = new System.Drawing.Bitmap(
                    this.pixmap.fz_pixmap_width(),
                    this.pixmap.fz_pixmap_height(),
                    this.pixmap.fz_pixmap_stride(),
                    System.Drawing.Imaging.PixelFormat.Format32bppRgb,
                    (System.IntPtr) this.pixmap.fz_pixmap_samples_int()
                    );
            stopwatch.Stop();
            var t_bitmap = stopwatch.Elapsed;

            stopwatch.Reset();
            stopwatch.Start();
            // This is slow for large pixmaps/bitmaps.
            //check(pixmap, bitmap, 4);
            stopwatch.Stop();
            var t_check = stopwatch.Elapsed;

            /*System.Console.WriteLine(""
                    + " t_pixmap=" + t_pixmap
                    + " t_bitmap=" + t_bitmap
                    + " t_check=" + t_check
                    );*/
        }
        else
        {
            /* Copy pixmap's pixels into bitmap. This works on both Linux
            (Mono) and Windows.

            Unlike above, it seems that we need to use MuPDF Fixed_RGB with
            alpha=0, and C#'s Format32bppRgb. Other combinations give a
            blank display (possibly with alpha=0 for each pixel). */
            this.pixmap = this.page.fz_new_pixmap_from_page_contents(
                    new mupdf.FzMatrix((float) z, 0, 0, (float) z, 0, 0),
                    new mupdf.FzColorspace(mupdf.FzColorspace.Fixed.Fixed_RGB),
                    0 /*alpha*/
                    );

            this.bitmap = new System.Drawing.Bitmap(
                    this.pixmap.fz_pixmap_width(),
                    this.pixmap.fz_pixmap_height(),
                    System.Drawing.Imaging.PixelFormat.Format32bppRgb
                    );
            long samples = pixmap.fz_pixmap_samples_int();
            int stride = pixmap.fz_pixmap_stride();
            for (int x=0; x<bitmap.Width; x+=1)
            {
                for (int y=0; y<bitmap.Height; y+=1)
                {
                    unsafe
                    {
                        byte* sample = (byte*) samples + stride * y + 3 * x;
                        var color = System.Drawing.Color.FromArgb(sample[0], sample[1], sample[2]);
                        this.bitmap.SetPixel( x, y, color);
                    }
                }
            }
            //check(pixmap, bitmap, 3);
        }
        this.picture_box.Image = this.bitmap;
    }

    private void handle_key_down(object sender, System.Windows.Forms.KeyEventArgs e)
    {
        //System.Console.WriteLine("HandleKeyDown: " + e.KeyCode);
        if (e.Shift && e.KeyCode == System.Windows.Forms.Keys.PageUp)
        {
            goto_page(this.page_number - 1, this.zoom);
        }
        else if (e.Shift && e.KeyCode == System.Windows.Forms.Keys.PageDown)
        {
            goto_page(this.page_number + 1, this.zoom);
        }
        else if (e.KeyCode == System.Windows.Forms.Keys.D0)
        {
            goto_page(this.page_number, 0);
        }
        else if (e.KeyCode == System.Windows.Forms.Keys.Add
                || e.KeyCode == System.Windows.Forms.Keys.Oemplus
                )
        {
            goto_page(this.page_number, this.zoom + 1);
        }
        else if (e.KeyCode == System.Windows.Forms.Keys.Subtract
                || e.KeyCode == System.Windows.Forms.Keys.OemMinus)
        {
            goto_page(this.page_number, this.zoom - 1);
        }
    }

    private void handle_resize(object sender, System.EventArgs e)
    {
        goto_page(page_number, zoom);
    }

    // Throws exception if pixmap and bitmap differ.
    void check(mupdf.FzPixmap pixmap, System.Drawing.Bitmap bitmap, int pixmap_bytes_per_pixel)
    {
        long samples = pixmap.fz_pixmap_samples_int();
        if (pixmap.fz_pixmap_width() != bitmap.Width || pixmap.fz_pixmap_height() != bitmap.Height)
        {
            throw new System.Exception("Inconsistent sizes:"
                    + " pixmap=(" + pixmap.fz_pixmap_width() + " " + pixmap.fz_pixmap_height()
                    + " bitmap=(" + bitmap.Width + " " + bitmap.Height
                    );
        }
        int stride = pixmap.fz_pixmap_stride();
        for (int x=0; x<bitmap.Width; x+=1)
        {
            for (int y=0; y<bitmap.Height; y+=1)
            {
                unsafe
                {
                    byte* sample = (byte*) samples + stride * y + pixmap_bytes_per_pixel * x;
                    System.Drawing.Color color = bitmap.GetPixel( x, y);
                    if (color.R != sample[0] || color.G != sample[1] || color.B != sample[2])
                    {
                        string pixmap_pixel_text = "";
                        for (int i=0; i<pixmap_bytes_per_pixel; ++i)
                        {
                            if (i > 0) pixmap_pixel_text += " ";
                            pixmap_pixel_text += sample[i];
                        }
                        throw new System.Exception("Pixels differ: (" + x + " " + y + "):"
                                + " pixmap: (" + pixmap_pixel_text + ")"
                                + " bitmap: " + color);
                    }
                }
            }
        }
    }
}
