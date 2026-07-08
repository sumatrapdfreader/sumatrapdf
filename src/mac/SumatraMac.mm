#import <Cocoa/Cocoa.h>

#include "mac/SumatraMacEngine.h"

// The website / manual URL opened from the Help menu.
static NSString* const kWebsiteURL = @"https://www.sumatrapdfreader.org";

static void ReleaseCopiedPixels(void* info, const void*, size_t) {
    free(info);
}

static CGImageRef CreateImageFromRenderedPage(const MacRenderedPage* page) {
    if (!page || !page->data || page->width <= 0 || page->height <= 0 || page->stride <= 0) {
        return nullptr;
    }

    size_t nBytes = (size_t)page->stride * (size_t)page->height;
    void* pixels = malloc(nBytes);
    if (!pixels) {
        return nullptr;
    }
    memcpy(pixels, page->data, nBytes);

    CGDataProviderRef provider = CGDataProviderCreateWithData(pixels, pixels, nBytes, ReleaseCopiedPixels);
    if (!provider) {
        free(pixels);
        return nullptr;
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little |
                              (page->premultiplied ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaFirst);
    CGImageRef image = CGImageCreate((size_t)page->width, (size_t)page->height, 8, 32, (size_t)page->stride,
                                     colorSpace, bitmapInfo, provider, nullptr, false,
                                     kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    return image;
}

static NSString* ExistingPath(NSArray<NSString*>* candidates) {
    NSFileManager* fileManager = [NSFileManager defaultManager];
    for (NSString* candidate in candidates) {
        if ([fileManager fileExistsAtPath:candidate]) {
            return candidate;
        }
    }
    return [candidates count] ? [candidates objectAtIndex:0] : @"";
}

static NSString* ResolveDocumentPath(NSString* path) {
    path = [path stringByStandardizingPath];
    if ([path isAbsolutePath]) {
        return path;
    }

    NSMutableArray<NSString*>* candidates = [NSMutableArray arrayWithObject:path];

    NSString* pwd = [[[NSProcessInfo processInfo] environment] objectForKey:@"PWD"];
    if ([pwd length] > 0) {
        [candidates addObject:[[pwd stringByAppendingPathComponent:path] stringByStandardizingPath]];
    }

    NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
    NSString* repoRoot = [[[[bundlePath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent]
        stringByDeletingLastPathComponent] stringByStandardizingPath];
    [candidates addObject:[repoRoot stringByAppendingPathComponent:path]];

    return ExistingPath(candidates);
}

// zoom presets
static const CGFloat kZoomMin = 0.125;
static const CGFloat kZoomMax = 8.0;
static const CGFloat kZoomStep = 1.25;

@class SumatraAppDelegate;

// A view that draws a single rendered page. In fit modes it scales the image to
// its bounds; when zoomed it is sized to the image's pixels and scrolls inside
// an NSScrollView. It owns keyboard navigation for the document.
@interface SumatraDocumentView : NSView
@property(nonatomic) CGImageRef image;
@property(nonatomic, copy) NSString* message;
@property(nonatomic) BOOL scaleToFit;
@property(nonatomic, assign) SumatraAppDelegate* owner;
@end

@implementation SumatraDocumentView

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)dealloc {
    if (_image) {
        CGImageRelease(_image);
    }
    [_message release];
    [super dealloc];
}

- (void)setImage:(CGImageRef)image {
    if (_image == image) {
        return;
    }
    if (_image) {
        CGImageRelease(_image);
    }
    _image = image ? CGImageRetain(image) : nullptr;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithCalibratedWhite:0.18 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    if (!_image) {
        NSDictionary* attrs = @{
            NSFontAttributeName : [NSFont systemFontOfSize:15],
            NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.88 alpha:1.0],
        };
        NSString* text = _message ?: @"Open a document with File → Open (⌘O).";
        NSSize size = [text sizeWithAttributes:attrs];
        NSRect bounds = [self bounds];
        NSPoint p = NSMakePoint(MAX(24.0, (bounds.size.width - size.width) / 2.0),
                                MAX(24.0, (bounds.size.height - size.height) / 2.0));
        [text drawAtPoint:p withAttributes:attrs];
        return;
    }

    NSRect bounds = [self bounds];
    CGFloat imageW = (CGFloat)CGImageGetWidth(_image);
    CGFloat imageH = (CGFloat)CGImageGetHeight(_image);

    NSRect drawRect;
    if (_scaleToFit) {
        CGFloat margin = 8.0;
        CGFloat scale = MIN((bounds.size.width - 2 * margin) / imageW, (bounds.size.height - 2 * margin) / imageH);
        if (!isfinite(scale) || scale <= 0) {
            scale = 1;
        }
        scale = MIN(scale, 1.0);
        CGSize drawSize = CGSizeMake(floor(imageW * scale), floor(imageH * scale));
        drawRect = NSMakeRect(floor((bounds.size.width - drawSize.width) / 2.0),
                              floor((bounds.size.height - drawSize.height) / 2.0), drawSize.width, drawSize.height);
    } else {
        // sized to the image (1:1); center if the view is larger than the image
        drawRect = NSMakeRect(floor(MAX(0, (bounds.size.width - imageW) / 2.0)),
                              floor(MAX(0, (bounds.size.height - imageH) / 2.0)), imageW, imageH);
    }

    [[NSColor colorWithCalibratedWhite:0.92 alpha:1.0] setFill];
    NSRectFill(NSRectFromCGRect(CGRectInset(NSRectToCGRect(drawRect), -1, -1)));

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, bounds.size.height);
    CGContextScaleCTM(ctx, 1, -1);
    CGRect cgDrawRect = CGRectMake(drawRect.origin.x, bounds.size.height - drawRect.origin.y - drawRect.size.height,
                                   drawRect.size.width, drawRect.size.height);
    CGContextDrawImage(ctx, cgDrawRect, _image);
    CGContextRestoreGState(ctx);
}

// Page navigation and zoom via the keyboard. Menu items provide the ⌘-modified
// equivalents; bare keys are handled here so arrows/space/page keys just work.
- (void)keyDown:(NSEvent*)event {
    NSString* chars = [event charactersIgnoringModifiers];
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;
    SumatraAppDelegate* owner = _owner;
    if (!owner) {
        [super keyDown:event];
        return;
    }

    switch (c) {
        case NSRightArrowFunctionKey:
        case NSDownArrowFunctionKey:
        case NSPageDownFunctionKey:
        case ' ':
            [owner goToNextPage:nil];
            return;
        case NSLeftArrowFunctionKey:
        case NSUpArrowFunctionKey:
        case NSPageUpFunctionKey:
            [owner goToPrevPage:nil];
            return;
        case NSHomeFunctionKey:
            [owner goToFirstPage:nil];
            return;
        case NSEndFunctionKey:
            [owner goToLastPage:nil];
            return;
        case '+':
        case '=':
            [owner zoomIn:nil];
            return;
        case '-':
        case '_':
            [owner zoomOut:nil];
            return;
        case '0':
            [owner zoomActualSize:nil];
            return;
        default:
            break;
    }
    [super keyDown:event];
}

@end

@interface SumatraAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, retain) NSWindow* window;
@property(nonatomic, retain) NSScrollView* scrollView;
@property(nonatomic, retain) SumatraDocumentView* documentView;
@property(nonatomic) void* document;
@property(nonatomic, copy) NSString* documentPath;
@property(nonatomic) int pageCount;
@property(nonatomic) int currentPage; // 1-based
@property(nonatomic) int rotation;    // 0/90/180/270
@property(nonatomic) CGFloat zoom;    // render zoom; 0 means "fit to window"
@end

@implementation SumatraAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    _zoom = 0; // fit page
    _rotation = 0;

    NSRect frame = NSMakeRect(0, 0, 900, 1100);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                       NSWindowStyleMaskResizable;
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"SumatraPDF"];

    _scrollView = [[NSScrollView alloc] initWithFrame:frame];
    [_scrollView setHasVerticalScroller:YES];
    [_scrollView setHasHorizontalScroller:YES];
    [_scrollView setAutohidesScrollers:YES];
    [_scrollView setBorderType:NSNoBorder];
    [_scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_scrollView setBackgroundColor:[NSColor colorWithCalibratedWhite:0.18 alpha:1.0]];

    _documentView = [[SumatraDocumentView alloc] initWithFrame:frame];
    [_documentView setOwner:self];
    [_documentView setScaleToFit:YES];
    [_scrollView setDocumentView:_documentView];

    [_window setContentView:_scrollView];
    [_window makeFirstResponder:_documentView];

    NSArray<NSString*>* args = [[NSProcessInfo processInfo] arguments];
    if ([args count] >= 2) {
        [self openPath:[args objectAtIndex:1]];
    }

    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)hasDocument {
    return _document != nullptr;
}

- (void)closeDocument {
    if (_document) {
        MacCloseDocument(_document);
        _document = nullptr;
    }
    _pageCount = 0;
    _currentPage = 0;
    _rotation = 0;
    _zoom = 0;
}

- (void)openPath:(NSString*)path {
    path = ResolveDocumentPath(path);
    char* error = nullptr;
    void* doc = MacOpenDocument([path fileSystemRepresentation], &error);
    if (!doc) {
        NSString* message = error ? [NSString stringWithUTF8String:error] : @"Could not open the document.";
        free(error);
        [_documentView setImage:nullptr];
        [_documentView setMessage:message];
        return;
    }

    [self closeDocument];
    _document = doc;
    self.documentPath = path;
    _pageCount = MacPageCount(doc);
    _currentPage = 1;
    _rotation = 0;
    _zoom = 0; // fit
    [self renderCurrentPage];
}

// Computes the render zoom for the current fit mode, given the page's mediabox
// (in points) and the space available in the scroll view's clip area.
- (CGFloat)fitZoomForWidthOnly:(BOOL)widthOnly {
    double wPts = 0, hPts = 0;
    if (!MacPageSize(_document, _currentPage, &wPts, &hPts) || wPts <= 0 || hPts <= 0) {
        return 1.0;
    }
    // rotation swaps width/height
    if (_rotation == 90 || _rotation == 270) {
        double t = wPts;
        wPts = hPts;
        hPts = t;
    }
    // at zoom 1.0 the engine renders roughly points * 96/72 pixels
    const double pxPerPt = 96.0 / 72.0;
    NSSize clip = [[_scrollView contentView] bounds].size;
    double margin = 16.0;
    double availW = MAX(1.0, clip.width - margin);
    double zoomW = availW / (wPts * pxPerPt);
    if (widthOnly) {
        return (CGFloat)zoomW;
    }
    double availH = MAX(1.0, clip.height - margin);
    double zoomH = availH / (hPts * pxPerPt);
    return (CGFloat)MIN(zoomW, zoomH);
}

- (void)renderCurrentPage {
    if (!_document) {
        return;
    }

    BOOL fitMode = (_zoom <= 0);
    CGFloat renderZoom = fitMode ? [self fitZoomForWidthOnly:NO] : _zoom;
    if (renderZoom <= 0) {
        renderZoom = 1.0;
    }

    MacRenderedPage page = {};
    bool ok = MacRenderPage(_document, _currentPage, (float)renderZoom, _rotation, &page);
    if (!ok) {
        MacFreeRenderedPage(&page);
        [_documentView setImage:nullptr];
        [_documentView setMessage:@"Could not render the page."];
        return;
    }

    CGImageRef image = CreateImageFromRenderedPage(&page);
    MacFreeRenderedPage(&page);
    if (!image) {
        [_documentView setImage:nullptr];
        [_documentView setMessage:@"Could not render the page."];
        return;
    }

    [_documentView setScaleToFit:fitMode];
    if (fitMode) {
        [_documentView setFrame:[[_scrollView contentView] bounds]];
        [_documentView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    } else {
        [_documentView setAutoresizingMask:NSViewNotSizable];
        [_documentView setFrameSize:NSMakeSize(CGImageGetWidth(image), CGImageGetHeight(image))];
    }
    [_documentView setImage:image];
    CGImageRelease(image);

    [self updateTitle];
}

- (void)updateTitle {
    NSString* name = _documentPath ? [_documentPath lastPathComponent] : @"SumatraPDF";
    if (_document && _pageCount > 0) {
        [_window setTitle:[NSString stringWithFormat:@"%@  —  page %d / %d", name, _currentPage, _pageCount]];
    } else {
        [_window setTitle:name];
    }
}

- (void)goToPage:(int)pageNo {
    if (!_document) {
        return;
    }
    if (pageNo < 1) {
        pageNo = 1;
    }
    if (pageNo > _pageCount) {
        pageNo = _pageCount;
    }
    if (pageNo == _currentPage) {
        return;
    }
    _currentPage = pageNo;
    [self renderCurrentPage];
    [[_scrollView contentView] scrollToPoint:NSZeroPoint];
    [_scrollView reflectScrolledClipView:[_scrollView contentView]];
}

#pragma mark - Menu / key actions

- (IBAction)openDocument:(id)sender {
    (void)sender;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setAllowedFileTypes:@[
        @"pdf", @"xps", @"oxps", @"epub", @"mobi", @"fb2", @"cbz", @"cbr", @"cb7", @"cbt", @"djvu", @"chm",
        @"png", @"jpg", @"jpeg", @"gif", @"tif", @"tiff", @"tga", @"bmp", @"webp", @"jxl", @"heic", @"avif"
    ]];
    if ([panel runModal] == NSModalResponseOK) {
        NSURL* url = [[panel URLs] firstObject];
        if (url) {
            [self openPath:[url path]];
        }
    }
}

- (IBAction)performClose:(id)sender {
    (void)sender;
    [self closeDocument];
    self.documentPath = nil;
    [_documentView setScaleToFit:YES];
    [_documentView setFrame:[[_scrollView contentView] bounds]];
    [_documentView setImage:nullptr];
    [_documentView setMessage:nil];
    [self updateTitle];
}

- (IBAction)showInFolder:(id)sender {
    (void)sender;
    if (!_documentPath) {
        return;
    }
    NSURL* url = [NSURL fileURLWithPath:_documentPath];
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
}

- (IBAction)goToNextPage:(id)sender {
    (void)sender;
    [self goToPage:_currentPage + 1];
}

- (IBAction)goToPrevPage:(id)sender {
    (void)sender;
    [self goToPage:_currentPage - 1];
}

- (IBAction)goToFirstPage:(id)sender {
    (void)sender;
    [self goToPage:1];
}

- (IBAction)goToLastPage:(id)sender {
    (void)sender;
    [self goToPage:_pageCount];
}

- (IBAction)goToPageDialog:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:[NSString stringWithFormat:@"Go to page (1 - %d):", _pageCount]];
    [alert addButtonWithTitle:@"Go"];
    [alert addButtonWithTitle:@"Cancel"];
    NSTextField* input = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)] autorelease];
    [input setStringValue:[NSString stringWithFormat:@"%d", _currentPage]];
    [alert setAccessoryView:input];
    if ([alert runModal] == NSAlertFirstButtonReturn) {
        int pageNo = [input intValue];
        if (pageNo >= 1 && pageNo <= _pageCount) {
            [self goToPage:pageNo];
        }
    }
}

- (IBAction)rotateLeft:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _rotation = (_rotation + 270) % 360;
    [self renderCurrentPage];
}

- (IBAction)rotateRight:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _rotation = (_rotation + 90) % 360;
    [self renderCurrentPage];
}

- (IBAction)toggleFullScreen:(id)sender {
    (void)sender;
    [_window toggleFullScreen:nil];
}

- (IBAction)zoomFitPage:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _zoom = 0; // fit
    [self renderCurrentPage];
}

- (IBAction)zoomFitWidth:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _zoom = [self fitZoomForWidthOnly:YES];
    [self renderCurrentPage];
}

- (IBAction)zoomActualSize:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _zoom = 1.0;
    [self renderCurrentPage];
}

- (void)applyZoomFactor:(CGFloat)factor {
    if (!_document) {
        return;
    }
    CGFloat base = (_zoom <= 0) ? [self fitZoomForWidthOnly:NO] : _zoom;
    CGFloat z = base * factor;
    z = MAX(kZoomMin, MIN(kZoomMax, z));
    _zoom = z;
    [self renderCurrentPage];
}

- (IBAction)zoomIn:(id)sender {
    (void)sender;
    [self applyZoomFactor:kZoomStep];
}

- (IBAction)zoomOut:(id)sender {
    (void)sender;
    [self applyZoomFactor:1.0 / kZoomStep];
}

- (IBAction)openWebsite:(id)sender {
    (void)sender;
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:kWebsiteURL]];
}

// Placeholder for menu items whose feature isn't ported yet: kept in the menu
// for structure but disabled (see validateMenuItem:).
- (IBAction)unavailableFeature:(id)sender {
    (void)sender;
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    SEL action = [item action];
    if (action == @selector(unavailableFeature:)) {
        return NO;
    }
    // actions that need an open document
    if (action == @selector(performClose:) || action == @selector(showInFolder:) ||
        action == @selector(goToNextPage:) || action == @selector(goToPrevPage:) ||
        action == @selector(goToFirstPage:) || action == @selector(goToLastPage:) ||
        action == @selector(goToPageDialog:) || action == @selector(rotateLeft:) ||
        action == @selector(rotateRight:) || action == @selector(zoomFitPage:) ||
        action == @selector(zoomFitWidth:) || action == @selector(zoomActualSize:) ||
        action == @selector(zoomIn:) || action == @selector(zoomOut:)) {
        return [self hasDocument];
    }
    if (action == @selector(goToNextPage:)) {
        return [self hasDocument] && _currentPage < _pageCount;
    }
    return YES;
}

#pragma mark - App lifecycle

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (_document && _zoom <= 0) {
        [self renderCurrentPage];
    }
}

- (BOOL)application:(NSApplication*)sender openFile:(NSString*)filename {
    (void)sender;
    [self openPath:filename];
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    (void)notification;
    [self closeDocument];
    MacShutdown();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)dealloc {
    [_documentView release];
    [_scrollView release];
    [_window release];
    [_documentPath release];
    [super dealloc];
}

@end

#pragma mark - Menu construction

static NSMenuItem* AddItem(NSMenu* menu, NSString* title, SEL action, id target, NSString* keyEquiv,
                           NSUInteger modifiers) {
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:keyEquiv] autorelease];
    if (target) {
        [item setTarget:target];
    }
    if ([keyEquiv length] > 0) {
        [item setKeyEquivalentModifierMask:modifiers];
    }
    [menu addItem:item];
    return item;
}

// A disabled placeholder item: present for structure, greyed out.
static void AddPlaceholder(NSMenu* menu, NSString* title, id target) {
    AddItem(menu, title, @selector(unavailableFeature:), target, @"", 0);
}

static NSString* ArrowKey(unichar c) {
    return [NSString stringWithCharacters:&c length:1];
}

static void InstallMainMenu(SumatraAppDelegate* delegate) {
    NSMenu* mainMenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];

    // Application menu
    NSMenuItem* appMenuItem = [[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:appMenuItem];
    NSMenu* appMenu = [[[NSMenu alloc] initWithTitle:@"SumatraPDF"] autorelease];
    AddItem(appMenu, @"About SumatraPDF", @selector(orderFrontStandardAboutPanel:), nil, @"", 0);
    [appMenu addItem:[NSMenuItem separatorItem]];
    AddItem(appMenu, @"Hide SumatraPDF", @selector(hide:), nil, @"h", NSEventModifierFlagCommand);
    AddItem(appMenu, @"Hide Others", @selector(hideOtherApplications:), nil, @"h",
            NSEventModifierFlagCommand | NSEventModifierFlagOption);
    AddItem(appMenu, @"Show All", @selector(unhideAllApplications:), nil, @"", 0);
    [appMenu addItem:[NSMenuItem separatorItem]];
    AddItem(appMenu, @"Quit SumatraPDF", @selector(terminate:), nil, @"q", NSEventModifierFlagCommand);
    [appMenuItem setSubmenu:appMenu];

    // File menu
    NSMenuItem* fileItem = [[[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:fileItem];
    NSMenu* fileMenu = [[[NSMenu alloc] initWithTitle:@"File"] autorelease];
    AddPlaceholder(fileMenu, @"New Window", delegate);
    AddItem(fileMenu, @"Open…", @selector(openDocument:), delegate, @"o", NSEventModifierFlagCommand);
    AddItem(fileMenu, @"Close", @selector(performClose:), delegate, @"w", NSEventModifierFlagCommand);
    AddItem(fileMenu, @"Show in Folder", @selector(showInFolder:), delegate, @"", 0);
    AddPlaceholder(fileMenu, @"Open Next File in Folder", delegate);
    AddPlaceholder(fileMenu, @"Open Previous File in Folder", delegate);
    [fileMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(fileMenu, @"Save As…", delegate);
    AddPlaceholder(fileMenu, @"Rename…", delegate);
    AddPlaceholder(fileMenu, @"Print…", delegate);
    [fileMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(fileMenu, @"Properties", delegate);
    [fileItem setSubmenu:fileMenu];

    // View menu
    NSMenuItem* viewItem = [[[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:viewItem];
    NSMenu* viewMenu = [[[NSMenu alloc] initWithTitle:@"View"] autorelease];
    AddPlaceholder(viewMenu, @"Single Page", delegate);
    AddPlaceholder(viewMenu, @"Facing", delegate);
    AddPlaceholder(viewMenu, @"Book View", delegate);
    AddPlaceholder(viewMenu, @"Show Pages Continuously", delegate);
    AddPlaceholder(viewMenu, @"Manga Mode", delegate);
    [viewMenu addItem:[NSMenuItem separatorItem]];
    AddItem(viewMenu, @"Rotate Left", @selector(rotateLeft:), delegate, @"[", NSEventModifierFlagCommand);
    AddItem(viewMenu, @"Rotate Right", @selector(rotateRight:), delegate, @"]", NSEventModifierFlagCommand);
    [viewMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(viewMenu, @"Presentation", delegate);
    AddItem(viewMenu, @"Enter Full Screen", @selector(toggleFullScreen:), delegate, @"f",
            NSEventModifierFlagCommand | NSEventModifierFlagControl);
    [viewMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(viewMenu, @"Show Bookmarks", delegate);
    AddPlaceholder(viewMenu, @"Show Toolbar", delegate);
    [viewItem setSubmenu:viewMenu];

    // Go To menu
    NSMenuItem* goItem = [[[NSMenuItem alloc] initWithTitle:@"Go To" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:goItem];
    NSMenu* goMenu = [[[NSMenu alloc] initWithTitle:@"Go To"] autorelease];
    AddItem(goMenu, @"Next Page", @selector(goToNextPage:), delegate, ArrowKey(NSRightArrowFunctionKey),
            NSEventModifierFlagCommand | NSEventModifierFlagFunction);
    AddItem(goMenu, @"Previous Page", @selector(goToPrevPage:), delegate, ArrowKey(NSLeftArrowFunctionKey),
            NSEventModifierFlagCommand | NSEventModifierFlagFunction);
    AddItem(goMenu, @"First Page", @selector(goToFirstPage:), delegate, ArrowKey(NSUpArrowFunctionKey),
            NSEventModifierFlagCommand | NSEventModifierFlagFunction);
    AddItem(goMenu, @"Last Page", @selector(goToLastPage:), delegate, ArrowKey(NSDownArrowFunctionKey),
            NSEventModifierFlagCommand | NSEventModifierFlagFunction);
    AddItem(goMenu, @"Page…", @selector(goToPageDialog:), delegate, @"g", NSEventModifierFlagCommand);
    [goMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(goMenu, @"Back", delegate);
    AddPlaceholder(goMenu, @"Forward", delegate);
    [goMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(goMenu, @"Find…", delegate);
    [goItem setSubmenu:goMenu];

    // Zoom menu
    NSMenuItem* zoomItem = [[[NSMenuItem alloc] initWithTitle:@"Zoom" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:zoomItem];
    NSMenu* zoomMenu = [[[NSMenu alloc] initWithTitle:@"Zoom"] autorelease];
    AddItem(zoomMenu, @"Fit Page", @selector(zoomFitPage:), delegate, @"9", NSEventModifierFlagCommand);
    AddItem(zoomMenu, @"Actual Size", @selector(zoomActualSize:), delegate, @"0", NSEventModifierFlagCommand);
    AddItem(zoomMenu, @"Fit Width", @selector(zoomFitWidth:), delegate, @"", 0);
    AddPlaceholder(zoomMenu, @"Fit by Orientation", delegate);
    AddPlaceholder(zoomMenu, @"Fit Content", delegate);
    AddPlaceholder(zoomMenu, @"Custom Zoom…", delegate);
    [zoomMenu addItem:[NSMenuItem separatorItem]];
    AddItem(zoomMenu, @"Zoom In", @selector(zoomIn:), delegate, @"+", NSEventModifierFlagCommand);
    AddItem(zoomMenu, @"Zoom Out", @selector(zoomOut:), delegate, @"-", NSEventModifierFlagCommand);
    [zoomItem setSubmenu:zoomMenu];

    // Window menu (standard)
    NSMenuItem* windowItem = [[[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:windowItem];
    NSMenu* windowMenu = [[[NSMenu alloc] initWithTitle:@"Window"] autorelease];
    AddItem(windowMenu, @"Minimize", @selector(performMiniaturize:), nil, @"m", NSEventModifierFlagCommand);
    AddItem(windowMenu, @"Zoom", @selector(performZoom:), nil, @"", 0);
    [windowItem setSubmenu:windowMenu];
    [NSApp setWindowsMenu:windowMenu];

    // Help menu
    NSMenuItem* helpItem = [[[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:helpItem];
    NSMenu* helpMenu = [[[NSMenu alloc] initWithTitle:@"Help"] autorelease];
    AddItem(helpMenu, @"SumatraPDF Website", @selector(openWebsite:), delegate, @"", 0);
    AddPlaceholder(helpMenu, @"Manual", delegate);
    [helpItem setSubmenu:helpMenu];
    [NSApp setHelpMenu:helpMenu];

    [NSApp setMainMenu:mainMenu];
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];

    SumatraAppDelegate* delegate = [[SumatraAppDelegate alloc] init];
    [app setDelegate:delegate];
    InstallMainMenu(delegate);
    [app run];

    [delegate release];
    [pool release];
    return 0;
}
