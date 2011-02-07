package com.artifex.mupdf;
import android.graphics.*;

public class MuPDFCore
{
    /* load our native library */
    static {
        System.loadLibrary("mupdf");
    }

    /* Readable members */
    public int   pageNum;
    public int   numPages;
    public float pageWidth;
    public float pageHeight;

    /* The native functions */
    private static native int   openFile(String filename);
    private static native void  gotoPageInternal(int localActionPageNum);
    private static native float getPageWidth();
    private static native float getPageHeight();
    public  static native void  drawPage(Bitmap bitmap,
                                         int    pageW,
                                         int    pageH,
                                         int    patchX,
                                         int    patchY,
                                         int    patchW,
                                         int    patchH);

    public MuPDFCore(String filename) throws Exception
    {
        numPages = openFile(filename);
        if (numPages <= 0)
        {
            throw new Exception("Failed to open "+filename);
        }
        pageNum = 1;
    }

    /* Shim function */
    public void gotoPage(int page)
    {
        if (page > numPages)
            page = numPages;
        else if (page < 1)
            page = 1;
        gotoPageInternal(page);
        this.pageNum = page;
        this.pageWidth  = getPageWidth();
        this.pageHeight = getPageHeight();
    }
}
