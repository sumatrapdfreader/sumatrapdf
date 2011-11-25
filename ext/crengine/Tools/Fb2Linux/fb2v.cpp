// Written by Ch. Tronche (http://tronche.lri.fr:8000/)
// Copyright by the author. This is unmaintained, no-warranty free software. 
// Please use freely. It is appreciated (but by no means mandatory) to
// acknowledge the author's contribution. Thank you.
// Started on Thu Jun 26 23:29:03 1997

//
// Xlib tutorial: 2nd program
// Make a window appear on the screen and draw a line inside.
// If you don't understand this program, go to
// http://tronche.lri.fr:8000/gui/x/xlib-tutorial/2nd-program-anatomy.html
//

#include "crengine.h"



#include <X11/Xlib.h> // Every Xlib program must include this
#include <X11/Xutil.h>
#include <X11/keysym.h> // Every Xlib program must include this
#include <assert.h>   // I include this to test return values the lazy way
#include <unistd.h>   // So we got the profile for 10 seconds
#include <stdlib.h>
#include <string.h>
#include "xutils.h"
#include "../../include/hyphman.h"

#define NIL (0)       // A name for the void pointer


lString8 readFileToString( const char * fname )
{
    lString8 buf;
    LVStreamRef stream = LVOpenFileStream(fname, LVOM_READ);
    if (!stream)
        return buf;
    int sz = stream->GetSize();
    if (sz>0)
    {
        buf.insert( 0, sz, ' ' );
        stream->Read( buf.modify(), sz, NULL );
    }
    return buf;
}


class MyXWindowApp
{
private:
    Display *dpy;
    Window w;
    int blackColor;
    int whiteColor;
    GC gc;
    LVDocView * textView;
public:
    MyXWindowApp(LVDocView * text_view)
	: textView(text_view)
    {
      // Open the display
      dpy = XOpenDisplay(NIL);
      assert(dpy);
      // Get some colors

      blackColor = BlackPixel(dpy, DefaultScreen(dpy));
      whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

      // Create the window
      w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 
                     400, 300, 0, whiteColor, whiteColor);

      // We want to get MapNotify events

      XSelectInput(dpy, w, 
          ExposureMask | ButtonPressMask 
          | StructureNotifyMask | SubstructureNotifyMask
          | KeyPressMask | ButtonPressMask);

      // "Map" the window (that is, make it appear on the screen)

      XMapWindow(dpy, w);

      // Create a "Graphics Context"

      gc = XCreateGC(dpy, w, 0, NIL);


      // Wait for the MapNotify event
    }


    void doCommand( LVDocCmd cmd, int param )
    {
        textView->doCommand( cmd, param );
        Paint();
    }

    void Resize(int dx, int dy)
    {
        if (textView->GetWidth()==dx && textView->GetHeight()==dy)
            return; // no resize
	if (dx<5 || dy<5 || dx>3000 || dy>3000)
        {
            return;
        }
        textView->Resize(dx, dy);
        Paint();
    }

    void OnKeyPress( int code )
    {
        switch( code )
        {
        case XK_KP_Add:
            {
        doCommand( DCMD_ZOOM_IN, 0 );
            }
            break;
        case XK_KP_Subtract:
            {
        doCommand( DCMD_ZOOM_OUT, 0 );
            }
            break;
        case XK_Up:
            {
		doCommand( DCMD_LINEUP, 1 );
            }
            break;
        case XK_Down:
            {
		doCommand( DCMD_LINEDOWN, 1 );
            }
            break;
        case XK_Page_Up:
            {
		doCommand( DCMD_PAGEUP, 1 );
            }
            break;
        case XK_Page_Down:
            {
		doCommand( DCMD_PAGEDOWN, 1 );
            }
            break;
        case XK_Home:
            {
		doCommand( DCMD_BEGIN, 0 );
            }
            break;
        case XK_End:
            {
		doCommand( DCMD_END, 0 );
            }
            break;

        }
    }

    void Paint()
    {
        unsigned pal[4]={0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000};
        LVDocImageRef pageImage = textView->getPageImage(0);
        LVDrawBuf * drawbuf = pageImage->getDrawBuf();
        DrawBuf2Drawable(dpy, w, gc, 0, 0, drawbuf, pal, 1);

/*      
      // Tell the GC we draw using the white color

      XSetForeground(dpy, gc, blackColor);
      XSetBackground(dpy, gc, whiteColor);

      // Draw the line
      
      XDrawLine(dpy, w, gc, 5, 5, dx-10, dy-10);

    MyXImage img( 16, 16 );
    img.fill(0x456789);
    XPutImage( dpy, w, gc, img.getXImage(), 0, 0, 3, 3, 16, 16 );
    img.fill(0xABCDEF);
    XPutImage( dpy, w, gc, img.getXImage(), 0, 0, dx-20, dy-20, 16, 16 );

      // Send the "DrawLine" request to the server
*/

        XFlush(dpy);
    }
    
    void EventLoop()
    {
      XEvent e;
      for(;;) {
        XNextEvent(dpy, &e);
        switch( e.type )
        {
        case Expose:
            {
                if (!e.xexpose.count)
                    Paint();
            }
            break;
        case ConfigureNotify:
            {
                if (e.xconfigure.window == w)
                    Resize( e.xconfigure.width, e.xconfigure.height );
                break;
            }
        case KeyPress:
            if (e.xkey.window == w)
            {
                char buffer[20];
                int bufsize = 20;
                KeySym keysym;
                XComposeStatus compose;
                int charcount = XLookupString(&e.xkey, buffer, bufsize, &keysym,
                    &compose);                
                //printf("keypress(0x%04x)\n", keysym);
                OnKeyPress( keysym );
            }
            break;
        case ButtonPress:
            if (e.xbutton.window == w)
            {
                //printf("buttonpress(0x%04x)\n", e.xbutton.button );
                switch( e.xbutton.button )
                {
                case Button4:
		    doCommand( DCMD_LINEUP, 3 );
                    break;
                case Button5:
		    doCommand( DCMD_LINEDOWN, 3 );
                    break;
                }
            }
            break;
        }

        if (e.type == ButtonRelease)
          break;
      }
    }
};

void initHyph(const char * fname)
{
    HyphMan hyphman;
    LVStreamRef stream = LVOpenFileStream( fname, LVOM_READ);
    if (!stream)
    {
        printf("Cannot load hyphenation file %s\n", fname);
        return;
    }
    HyphMan::Open( stream.get() );
}

int main( int argc, const char * argv[] )
{
    LVDocView text_view;

    CRLog::setStdoutLogger();
    CRLog::setLogLevel(CRLog::LL_DEBUG);
    char exedir[1024];
    strcpy( exedir, argv[0] );
    int lastslash=-1;
    for (int p=0; exedir[p]; p++)
    {
        if (exedir[p]=='\\' || exedir[p]=='/')
            lastslash = p;
    }
    if (lastslash>=0)
        exedir[lastslash+1] = 0;
    else
        exedir[0] = 0;

    printf("home dir: %s\n", exedir);

    lString8 fontDir(exedir);
    fontDir << "/fonts";


    // init bitmap font manager
    InitFontManager( fontDir );

#if (USE_FREETYPE==1)
        LVContainerRef dir = LVOpenDirectory( LocalToUnicode(fontDir).c_str() );
        if ( !dir.isNull() )
        for ( int i=0; i<dir->GetObjectCount(); i++ ) {
            const LVContainerItemInfo * item = dir->GetObjectInfo(i);
            lString16 fileName = item->GetName();
            if ( !item->IsContainer() && fileName.length()>4 && lString16(fileName, fileName.length()-4, 4)==L".ttf" ) {
                lString8 fn = UnicodeToLocal(fileName);
                printf("loading font: %s\n", fn.c_str());
                if ( !fontMan->RegisterFont(fn) ) {
                    printf("    failed\n");
                }
            }
        }
        //fontMan->RegisterFont(lString8("arial.ttf"));
#else
    // Load font definitions into font manager
    // fonts are in files font1.lbf, font2.lbf, ... font32.lbf
    #define MAX_FONT_FILE 32
    for (int i=0; i<MAX_FONT_FILE; i++)
    {
        char fn[1024];
        sprintf( fn, "font%d.lbf", i );
        printf("try load font: %s\n", fn);
        fontMan->RegisterFont( lString8(fn) );
    }
#endif    

    // init hyphenation manager
    char hyphfn[1024];
    sprintf(hyphfn, "%sRussian_EnUS_hyphen_(Alan).pdb", exedir );
    initHyph( hyphfn );
    

    //LVCHECKPOINT("WinMain start");

    // stylesheet can be placed to file fb2.css
    // if not found, default stylesheet will be used
    char cssfn[1024];
    sprintf( cssfn, "%sfb2.css", exedir);
    lString8 css = readFileToString( cssfn );
    if (css.length() > 0)
    {
        printf("Style sheet file found.\n");
        text_view.setStyleSheet( css );
    }


    if (!fontMan->GetFontCount())
    {
        //error
        printf("Fatal Error: Cannot open font file(s) font#.lbf \nCannot work without font\nUse FontConv utility to generate .lbf fonts from TTF\n" );
        return 1;
    }

    printf("%d fonts loaded.\n", fontMan->GetFontCount());

    if ( argc<2 )
    {
        printf("Usage: fb2v <filename>\n" );
        return 2;
    }

    lString8 cmdline( argv[1] );

    if ( !text_view.LoadDocument( cmdline.c_str() ))
    {
        //error
        char str[100];
        printf( "Fatal Error: Cannot open document file %s\n", cmdline.c_str() );
        return 1;
    }

    printf( "Book file %s opened ok.\n", cmdline.c_str() );


    MyXWindowApp app( &text_view );

    app.EventLoop();

    //sleep(10);
      
    return 0;
}

