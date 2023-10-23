public class HelloWorld
{
    public static void Main(string[] args)
    {
        System.Console.WriteLine("MuPDF C# test starting.");

        // Check we can load a document.
        mupdf.FzDocument document = new mupdf.FzDocument("zlib.3.pdf");
        System.Console.WriteLine("document: " + document);
        System.Console.WriteLine("num chapters: " + document.fz_count_chapters());
        mupdf.FzPage page = document.fz_load_page(0);
        mupdf.FzRect rect = page.fz_bound_page();
        System.Console.WriteLine("rect: " + rect);
        if ("" + rect != rect.to_string())
        {
            throw new System.Exception("rect ToString() is broken: '" + rect + "' != '" + rect.to_string() + "'");
        }

        // Test conversion to html using docx device.
        var buffer = page.fz_new_buffer_from_page_with_format(
                "docx",
                "html",
                new mupdf.FzMatrix(1, 0, 0, 1, 0, 0),
                new mupdf.FzCookie()
                );
        var data = buffer.fz_buffer_extract();
        var s = System.Text.Encoding.UTF8.GetString(data, 0, data.Length);
        if (s.Length < 100) {
            throw new System.Exception("HTML text is too short");
        }
        System.Console.WriteLine("s=" + s);

        // Check that previous buffer.fz_buffer_extract() cleared the buffer.
        data = buffer.fz_buffer_extract();
        s = System.Text.Encoding.UTF8.GetString(data, 0, data.Length);
        if (s.Length > 0) {
            throw new System.Exception("Buffer was not cleared.");
        }

        // Check we can create pixmap from page.
        var pixmap = page.fz_new_pixmap_from_page_contents(
                new mupdf.FzMatrix(1, 0, 0, 1, 0, 0),
                new mupdf.FzColorspace(mupdf.FzColorspace.Fixed.Fixed_RGB),
                0 /*alpha*/
                );

        // Check returned tuple from bitmap.fz_bitmap_details().
        var w = 100;
        var h = 200;
        var n = 4;
        var xres = 300;
        var yres = 300;
        var bitmap = new mupdf.FzBitmap(w, h, n, xres, yres);
        (var w2, var h2, var n2, var stride) = bitmap.fz_bitmap_details();
        System.Console.WriteLine("bitmap.fz_bitmap_details() returned:"
                + " " + w2 + " " + h2 + " " + n2 + " " + stride);
        if (w2 != w || h2 != h) {
            throw new System.Exception("Unexpected tuple values from bitmap.fz_bitmap_details().");
        }
        System.Console.WriteLine("MuPDF C# test finished.");
    }
}
