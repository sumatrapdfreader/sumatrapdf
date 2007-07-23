using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

namespace PDFViewer
{
    public class PdfViewer
    {
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_LoadPDF(IntPtr pdfWin,string pdfFile);
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_Print(IntPtr pdfWin);
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_PrintPDF(IntPtr pdfWin,string pdfFile, int showOptionWindow);
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_SetDisplayMode(IntPtr pdfWin,int displayMode);
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GoToNextPage(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GoToPreviousPage(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GoToFirstPage(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GoToLastPage(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GoToThisPage(IntPtr pdfWin,int pageNumber);
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GetNumberOfPages(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GetCurrentPage(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_ZoomIn(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_ZoomOut(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_SetZoom(IntPtr pdfWin,int zoomValue); 
        [DllImport("sumatrapdf.dll")] private static extern int Sumatra_GetCurrentZoom(IntPtr pdfWin); 
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_Resize(IntPtr pdfWin);
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_ClosePdf(IntPtr pdfWin);
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_ShowPrintDialog(IntPtr pdfWin);
        [DllImport("sumatrapdf.dll")] private static extern IntPtr Sumatra_Init(IntPtr parentHandle);
        [DllImport("sumatrapdf.dll")] private static extern void Sumatra_Exit();

        private string mFileName;
        private int mCurrentPage;
        private int mCurrentZoom;
        private IntPtr winPdf;
        private int mNumberOfPages;      
                
        public int NumberOfPages
        {
            get { return mNumberOfPages; }
            set { mNumberOfPages = value; }
        }

        public int CurrentPage
        {
            get { return mCurrentPage; }
            set { mCurrentPage = value; }
        }
        public int CurrentZoom
        {
            get { return mCurrentZoom; }
            set { mCurrentZoom = value; }
        }
        public string FileName
        {
            get { return mFileName; }
            set
            { 
                mFileName = value;
            }
        }

        public void Init(IntPtr displayHandle)
        {
            if (displayHandle.ToInt32() != 0)
            {
                winPdf = Sumatra_Init(displayHandle);
            }
        }
        public void Print()
        {
            Sumatra_Print(winPdf);
        }
        public void GoToFirst()
        {
            CurrentPage = Sumatra_GoToFirstPage(winPdf);
        }
        public void GoToLast()
        {
            CurrentPage = Sumatra_GoToLastPage(winPdf);
        }
        public void GoToNext()
        {
            CurrentPage = Sumatra_GoToNextPage(winPdf);
        }
        public void GoToPrevious()
        {
            CurrentPage = Sumatra_GoToPreviousPage(winPdf);
        }
        public void GoToPage(int pageNumber)
        {
            CurrentPage = Sumatra_GoToThisPage(winPdf, pageNumber);
        }
        public void ZoomIn()
        {
            CurrentZoom = Sumatra_ZoomIn(winPdf);
        }
        public void ZoomOut()
        {
            CurrentZoom = Sumatra_ZoomOut(winPdf);
        }
        public void SetZoom(int zoomLevel)
        {
            CurrentZoom = Sumatra_SetZoom(winPdf, zoomLevel);
        }
        public void SetDisplay(int displayMode)
        {
            Sumatra_SetDisplayMode(winPdf, displayMode);
        }
        public void Close()
        {
            Sumatra_ClosePdf(winPdf);
        }
        public void Exit()
        {
            Sumatra_Exit();
        }
        public void Resize()
        {
            Sumatra_Resize(winPdf);
        }

        public void Load(string pdfFile)
        {
            FileName = pdfFile;
            Sumatra_LoadPDF(winPdf, FileName);
            CurrentPage = Sumatra_GetCurrentPage(winPdf);
            CurrentZoom = Sumatra_GetCurrentZoom(winPdf);
            NumberOfPages = Sumatra_GetNumberOfPages(winPdf);
        }

    }
}
