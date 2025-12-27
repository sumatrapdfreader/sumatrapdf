public class HelloWorld
{
    public static void Main(string[] args)
    {
        System.Console.WriteLine("MuPDF C# test starting.");

        // Check FZ_ENABLE_FB2.
        System.Console.WriteLine("FZ_VERSION=" + mupdf.mupdf.FZ_VERSION);
        System.Console.WriteLine("FZ_ENABLE_FB2=" + mupdf.mupdf.FZ_ENABLE_FB2);

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

        // Check we get exception from MuPDF.
        //
        int received_exception = 0;
        try
        {
            System.Console.WriteLine("Attempting to open non-existent file 'does not exist'");
            mupdf.FzDocument document2 = new mupdf.FzDocument("does not exist");
            System.Console.WriteLine("*** Error, did not get expected exception.");
        }
        catch (System.Exception e)
        {
            received_exception = 1;
            System.Console.WriteLine("Received expected exception: [type=" + e.GetType() + "] " + e.Message);
        }
        if (received_exception != 1)
        {
            throw new System.Exception("Did not receive expected exception");
        }

        // Check we can make MuPDF open filename containing 4-byte
        // unicode character. This file will have been created by
        // `scripts/wrap/__main__.py --test-csharp`.
        byte[] text_utf8 =
        {
                0xf0,
                0x90,
                0x90,
                0xb7,
        };
        string testfile2 = "zlib.3.pdf"
                + System.Text.Encoding.UTF8.GetString(text_utf8)
                + ".pdf";
        System.Console.WriteLine("Opening testfile2: " + testfile2);
        try
        {
            mupdf.FzDocument document2 = new mupdf.FzDocument(testfile2);
            System.Console.WriteLine("new mupdf.FzDocument succeeded");
        }
        catch (System.Exception e)
        {
            System.Console.WriteLine("Exception: " + e.Message);
            throw new System.Exception("Failed to open filename containing 4-byte unicode character");
        }

        System.Console.WriteLine("MuPDF C# test finished.");
    }
}
