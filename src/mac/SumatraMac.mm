#import <Cocoa/Cocoa.h>

#include <fcntl.h>
#include <unistd.h>

#include "mac/SumatraMacEngine.h"
#include "mac/MacPrefs.h"
#include "gui/mac/KeyboardHelpMacBridge.h"

// The website / manual URL opened from the Help menu.
static NSString* const kWebsiteURL = @"https://www.sumatrapdfreader.org";
static const CGFloat kCocoaPointsPerInch = 72.0;

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
static const CGFloat kMacZoomFitPage = -1.0;
static const CGFloat kMacZoomFitWidth = -2.0;

static NSString* const kToolbarOpen = @"sumatra.toolbar.open";
static NSString* const kToolbarTabs = @"sumatra.toolbar.tabs";
static NSString* const kToolbarPrevPage = @"sumatra.toolbar.prev-page";
static NSString* const kToolbarNextPage = @"sumatra.toolbar.next-page";
static NSString* const kToolbarPageStatus = @"sumatra.toolbar.page-status";
static NSString* const kToolbarZoomOut = @"sumatra.toolbar.zoom-out";
static NSString* const kToolbarZoomActual = @"sumatra.toolbar.zoom-actual";
static NSString* const kToolbarZoomIn = @"sumatra.toolbar.zoom-in";
static NSString* const kToolbarFitPage = @"sumatra.toolbar.fit-page";
static NSString* const kToolbarFitWidth = @"sumatra.toolbar.fit-width";
static NSString* const kToolbarRotateLeft = @"sumatra.toolbar.rotate-left";
static NSString* const kToolbarRotateRight = @"sumatra.toolbar.rotate-right";

@class SumatraAppDelegate;

@protocol SumatraDocumentViewOwner
- (void*)documentHandle;
- (int)documentRotation;
- (void)activateLinkAtPage:(int)pageNo x:(double)x y:(double)y zoom:(double)zoom;
- (void)selectionChanged;
- (IBAction)goToNextPage:(id)sender;
- (IBAction)goToPrevPage:(id)sender;
- (IBAction)goToFirstPage:(id)sender;
- (IBAction)goToLastPage:(id)sender;
- (IBAction)zoomIn:(id)sender;
- (IBAction)zoomOut:(id)sender;
- (IBAction)zoomActualSize:(id)sender;
- (IBAction)showKeyboardShortcuts:(id)sender;
@end

@interface SumatraPageImage : NSObject
@property(nonatomic) int pageNo;
@property(nonatomic) NSRect frame;
@property(nonatomic) double layoutZoom;
@property(nonatomic) CGImageRef image;
@property(nonatomic, retain) NSArray* highlights;
@end

@implementation SumatraPageImage

- (void)dealloc {
    if (_image) {
        CGImageRelease(_image);
    }
    [_highlights release];
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
}

@end

@interface SumatraTabState : NSObject
@property(nonatomic) void* document;
@property(nonatomic, copy) NSString* path;
@property(nonatomic) int pageCount;
@property(nonatomic) int currentPage;
@property(nonatomic) int rotation;
@property(nonatomic) CGFloat zoom;
@property(nonatomic) BOOL continuous;
@property(nonatomic) NSPoint scrollOrigin;
@end

@implementation SumatraTabState

- (void)dealloc {
    [_path release];
    [super dealloc];
}

@end

// A view that draws a single rendered page. In fit modes it scales the image to
// its bounds; when zoomed it is sized to the image's pixels and scrolls inside
// an NSScrollView. It owns keyboard navigation for the document.
@interface SumatraDocumentView : NSView
@property(nonatomic) CGImageRef image;
@property(nonatomic) NSSize imageSize;
@property(nonatomic, retain) NSArray* pages;
@property(nonatomic, copy) NSString* message;
@property(nonatomic) BOOL scaleToFit;
@property(nonatomic) BOOL selectingText;
@property(nonatomic, assign) id<SumatraDocumentViewOwner> owner;
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
    [_pages release];
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

- (void)setPages:(NSArray*)pages {
    if (_pages == pages) {
        return;
    }
    [_pages release];
    _pages = [pages retain];
    [self setNeedsDisplay:YES];
}

- (void)drawPageImage:(CGImageRef)image inRect:(NSRect)drawRect bounds:(NSRect)bounds {
    if (!image) {
        return;
    }
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, bounds.size.height);
    CGContextScaleCTM(ctx, 1, -1);
    CGRect cgDrawRect = CGRectMake(drawRect.origin.x, bounds.size.height - drawRect.origin.y - drawRect.size.height,
                                   drawRect.size.width, drawRect.size.height);
    CGContextDrawImage(ctx, cgDrawRect, image);
    CGContextRestoreGState(ctx);
}

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor colorWithCalibratedWhite:0.18 alpha:1.0] setFill];
    NSRectFill(dirtyRect);

    if ([_pages count] > 0) {
        NSRect bounds = [self bounds];
        for (SumatraPageImage* page in _pages) {
            NSRect drawRect = [page frame];
            if (!NSIntersectsRect(drawRect, dirtyRect)) {
                continue;
            }
            [[NSColor colorWithCalibratedWhite:0.92 alpha:1.0] setFill];
            NSRectFill(NSRectFromCGRect(CGRectInset(NSRectToCGRect(drawRect), -1, -1)));
            if ([page image]) {
                [self drawPageImage:[page image] inRect:drawRect bounds:bounds];
            } else {
                NSDictionary* attrs = @{
                    NSFontAttributeName : [NSFont systemFontOfSize:13],
                    NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.45 alpha:1.0],
                };
                NSString* text = [NSString stringWithFormat:@"Page %d", [page pageNo]];
                NSSize size = [text sizeWithAttributes:attrs];
                NSPoint p = NSMakePoint(NSMidX(drawRect) - size.width / 2.0, NSMidY(drawRect) - size.height / 2.0);
                [text drawAtPoint:p withAttributes:attrs];
            }
            if ([[page highlights] count] > 0) {
                [[NSColor colorWithCalibratedRed:1.0 green:0.82 blue:0.1 alpha:0.45] setFill];
                for (NSValue* value in [page highlights]) {
                    NSRectFillUsingOperation([value rectValue], NSCompositingOperationSourceOver);
                }
            }
        }
        return;
    }

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
    NSSize imageSize = _imageSize;
    if (imageSize.width <= 0 || imageSize.height <= 0) {
        imageSize = NSMakeSize(imageW, imageH);
    }

    NSRect drawRect;
    if (_scaleToFit) {
        CGFloat margin = 8.0;
        CGFloat scale =
            MIN((bounds.size.width - (2 * margin)) / imageSize.width, (bounds.size.height - (2 * margin)) / imageSize.height);
        if (!isfinite(scale) || scale <= 0) {
            scale = 1;
        }
        scale = MIN(scale, 1.0);
        CGSize drawSize = CGSizeMake(floor(imageSize.width * scale), floor(imageSize.height * scale));
        drawRect = NSMakeRect(floor((bounds.size.width - drawSize.width) / 2.0),
                              floor((bounds.size.height - drawSize.height) / 2.0), drawSize.width, drawSize.height);
    } else {
        drawRect = NSMakeRect(floor(MAX(0, (bounds.size.width - imageSize.width) / 2.0)),
                              floor(MAX(0, (bounds.size.height - imageSize.height) / 2.0)), imageSize.width,
                              imageSize.height);
    }

    [[NSColor colorWithCalibratedWhite:0.92 alpha:1.0] setFill];
    NSRectFill(NSRectFromCGRect(CGRectInset(NSRectToCGRect(drawRect), -1, -1)));

    [self drawPageImage:_image inRect:drawRect bounds:bounds];
}

- (SumatraPageImage*)pageAtPoint:(NSPoint)point {
    for (SumatraPageImage* page in _pages) {
        if (NSPointInRect(point, [page frame])) {
            return page;
        }
    }
    return nil;
}

- (void)mouseDown:(NSEvent*)event {
    SumatraPageImage* page = [self pageAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];
    void* document = [_owner documentHandle];
    if (!page || !document) {
        [super mouseDown:event];
        return;
    }
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSRect frame = [page frame];
    double x = point.x - frame.origin.x;
    double y = point.y - frame.origin.y;
    MacLink link = {};
    if (MacLinkAtPoint(document, [page pageNo], x, y, [page layoutZoom], [_owner documentRotation], &link)) {
        MacFreeLink(&link);
        [_owner activateLinkAtPage:[page pageNo] x:x y:y zoom:[page layoutZoom]];
        return;
    }
    _selectingText = MacStartSelection(document, [page pageNo], x, y, [page layoutZoom], [_owner documentRotation]);
    if (_selectingText) {
        [_owner selectionChanged];
        return;
    }
    [super mouseDown:event];
}

- (void)mouseDragged:(NSEvent*)event {
    if (!_selectingText) {
        [super mouseDragged:event];
        return;
    }
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    SumatraPageImage* page = [self pageAtPoint:point];
    if (!page) {
        return;
    }
    NSRect frame = [page frame];
    if (MacUpdateSelection([_owner documentHandle], [page pageNo], point.x - frame.origin.x, point.y - frame.origin.y,
                           [page layoutZoom], [_owner documentRotation])) {
        [_owner selectionChanged];
    }
}

- (void)mouseUp:(NSEvent*)event {
    if (_selectingText) {
        _selectingText = NO;
        [_owner selectionChanged];
        return;
    }
    [super mouseUp:event];
}

// Page navigation and zoom via the keyboard. Menu items provide the ⌘-modified
// equivalents; bare keys are handled here so arrows/space/page keys just work.
- (void)keyDown:(NSEvent*)event {
    NSString* chars = [event charactersIgnoringModifiers];
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;
    id<SumatraDocumentViewOwner> owner = _owner;
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
        case '?':
            [owner showKeyboardShortcuts:nil];
            return;
        default:
            break;
    }
    [super keyDown:event];
}

@end

@interface SumatraPrintView : NSView
@property(nonatomic) void* document;
@property(nonatomic) int pageCount;
@property(nonatomic) int pageNo;
@property(nonatomic) int rotation;
@end

@implementation SumatraPrintView

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)knowsPageRange:(NSRangePointer)range {
    range->location = 1;
    range->length = (NSUInteger)_pageCount;
    return YES;
}

- (NSRect)rectForPage:(NSInteger)page {
    _pageNo = (int)page;
    double width = 612, height = 792;
    MacPageSize(_document, _pageNo, &width, &height);
    if (_rotation == 90 || _rotation == 270) {
        double value = width;
        width = height;
        height = value;
    }
    return NSMakeRect(0, 0, width, height);
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    double dpi = MacFileDPI(_document);
    float zoom = (float)(2.0 * kCocoaPointsPerInch / (dpi > 0 ? dpi : 96.0));
    MacRenderedPage page = {};
    if (!MacRenderPage(_document, _pageNo, zoom, _rotation, &page)) {
        return;
    }
    CGImageRef image = CreateImageFromRenderedPage(&page);
    MacFreeRenderedPage(&page);
    if (!image) {
        return;
    }
    NSRect bounds = [self bounds];
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, bounds.size.height);
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx, NSRectToCGRect(bounds), image);
    CGContextRestoreGState(ctx);
    CGImageRelease(image);
}

@end

@interface SumatraAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSToolbarDelegate,
                                         NSTextFieldDelegate, SumatraDocumentViewOwner>
@property(nonatomic, retain) NSWindow* window;
@property(nonatomic, retain) NSScrollView* scrollView;
@property(nonatomic, retain) SumatraDocumentView* documentView;
@property(nonatomic, retain) NSToolbar* toolbar;
@property(nonatomic, retain) NSTextField* pageLabel;
@property(nonatomic, copy) NSString* findText;
@property(nonatomic) void* commandPalette;
@property(nonatomic, retain) NSTextField* commandPaletteQuery;
@property(nonatomic, retain) NSPopUpButton* commandPaletteItems;
@property(nonatomic) dispatch_source_t fileWatcher;
@property(nonatomic) int watchedFile;
@property(nonatomic, retain) NSMutableArray* tabs;
@property(nonatomic, retain) NSMutableArray* closedPaths;
@property(nonatomic, retain) NSSegmentedControl* tabSelector;
@property(nonatomic) int activeTab;
@property(nonatomic) void* document;
@property(nonatomic, copy) NSString* documentPath;
@property(nonatomic) int pageCount;
@property(nonatomic) int currentPage; // 1-based
@property(nonatomic) int rotation;    // 0/90/180/270
@property(nonatomic) CGFloat zoom;    // display zoom; 1 means actual size, 0 means fit to window
@property(nonatomic) BOOL continuousView;
- (void)installToolbar;
- (void)updateToolbarStatus;
- (BOOL)canPerformAction:(SEL)action;
- (void)renderDocumentShowingErrors:(BOOL)showErrors;
- (void)pageRenderReady;
@end

static void PageRenderReady(void* context) {
    [(SumatraAppDelegate*)context pageRenderReady];
}

@implementation SumatraAppDelegate

static NSImage* ToolbarImage(NSString* symbolName, NSString* fallbackName) {
    NSImage* image = nil;
    if ([NSImage respondsToSelector:@selector(imageWithSystemSymbolName:accessibilityDescription:)]) {
        image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:nil];
    }
    if (!image && fallbackName) {
        image = [NSImage imageNamed:fallbackName];
    }
    [image setTemplate:YES];
    return image;
}

static NSArray<NSString*>* ToolbarDefaultItems() {
    return @[
        kToolbarOpen,
        NSToolbarSeparatorItemIdentifier,
        kToolbarTabs,
        NSToolbarSeparatorItemIdentifier,
        kToolbarPrevPage,
        kToolbarNextPage,
        kToolbarPageStatus,
        NSToolbarFlexibleSpaceItemIdentifier,
        kToolbarZoomOut,
        kToolbarZoomActual,
        kToolbarZoomIn,
        NSToolbarSeparatorItemIdentifier,
        kToolbarFitPage,
        kToolbarFitWidth,
        NSToolbarSeparatorItemIdentifier,
        kToolbarRotateLeft,
        kToolbarRotateRight,
    ];
}

static NSArray<NSString*>* ToolbarAllowedItems() {
    NSMutableArray<NSString*>* items = [NSMutableArray arrayWithArray:ToolbarDefaultItems()];
    [items addObject:NSToolbarSpaceItemIdentifier];
    return items;
}

- (NSToolbarItem*)toolbarItem:(NSString*)identifier label:(NSString*)label tooltip:(NSString*)tooltip image:(NSImage*)image
                       action:(SEL)action {
    NSToolbarItem* item = [[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
    [item setLabel:label];
    [item setPaletteLabel:label];
    [item setToolTip:tooltip];
    [item setImage:image];
    [item setTarget:self];
    [item setAction:action];
    return item;
}

- (NSToolbarItem*)pageStatusToolbarItem:(NSString*)identifier {
    NSToolbarItem* item = [[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
    [item setLabel:@"Page"];
    [item setPaletteLabel:@"Page"];
    [item setToolTip:@"Current page"];

    NSTextField* field = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 86, 24)] autorelease];
    [field setAlignment:NSTextAlignmentCenter];
    [field setBezeled:NO];
    [field setDrawsBackground:NO];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setFont:[NSFont monospacedDigitSystemFontOfSize:13 weight:NSFontWeightRegular]];
    [field setTextColor:[NSColor secondaryLabelColor]];
    self.pageLabel = field;

    [item setView:field];
    [item setMinSize:NSMakeSize(86, 24)];
    [item setMaxSize:NSMakeSize(112, 24)];
    [self updateToolbarStatus];
    return item;
}

- (NSToolbarItem*)tabsToolbarItem:(NSString*)identifier {
    NSToolbarItem* item = [[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
    [item setLabel:@"Documents"];
    [item setPaletteLabel:@"Documents"];
    NSSegmentedControl* control = [[[NSSegmentedControl alloc] initWithFrame:NSMakeRect(0, 0, 280, 28)] autorelease];
    [control setSegmentStyle:NSSegmentStyleTexturedRounded];
    [control setTrackingMode:NSSegmentSwitchTrackingSelectOne];
    [control setTarget:self];
    [control setAction:@selector(selectTab:)];
    self.tabSelector = control;
    [item setView:control];
    [item setMinSize:NSMakeSize(120, 28)];
    [item setMaxSize:NSMakeSize(420, 28)];
    return item;
}

- (void)installToolbar {
    NSToolbar* toolbar = [[[NSToolbar alloc] initWithIdentifier:@"sumatra.toolbar.main"] autorelease];
    [toolbar setDelegate:self];
    [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
    [toolbar setSizeMode:NSToolbarSizeModeRegular];
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:YES];
    self.toolbar = toolbar;
    [_window setToolbar:toolbar];
}

- (NSArray*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar {
    (void)toolbar;
    return ToolbarDefaultItems();
}

- (NSArray*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar {
    (void)toolbar;
    return ToolbarAllowedItems();
}

- (NSArray*)toolbarSelectableItemIdentifiers:(NSToolbar*)toolbar {
    (void)toolbar;
    return @[];
}

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar itemForItemIdentifier:(NSString*)identifier willBeInsertedIntoToolbar:(BOOL)flag {
    (void)toolbar;
    (void)flag;
    if ([identifier isEqualToString:kToolbarOpen]) {
        return [self toolbarItem:identifier
                           label:@"Open"
                         tooltip:@"Open a document"
                           image:ToolbarImage(@"doc", NSImageNameFolder)
                          action:@selector(openDocument:)];
    }
    if ([identifier isEqualToString:kToolbarTabs]) {
        return [self tabsToolbarItem:identifier];
    }
    if ([identifier isEqualToString:kToolbarPrevPage]) {
        return [self toolbarItem:identifier
                           label:@"Previous"
                         tooltip:@"Previous page"
                           image:ToolbarImage(@"chevron.left", NSImageNameGoLeftTemplate)
                          action:@selector(goToPrevPage:)];
    }
    if ([identifier isEqualToString:kToolbarNextPage]) {
        return [self toolbarItem:identifier
                           label:@"Next"
                         tooltip:@"Next page"
                           image:ToolbarImage(@"chevron.right", NSImageNameGoRightTemplate)
                          action:@selector(goToNextPage:)];
    }
    if ([identifier isEqualToString:kToolbarPageStatus]) {
        return [self pageStatusToolbarItem:identifier];
    }
    if ([identifier isEqualToString:kToolbarZoomOut]) {
        return [self toolbarItem:identifier
                           label:@"Out"
                         tooltip:@"Zoom out"
                           image:ToolbarImage(@"minus.magnifyingglass", nil)
                          action:@selector(zoomOut:)];
    }
    if ([identifier isEqualToString:kToolbarZoomActual]) {
        return [self toolbarItem:identifier
                           label:@"Actual"
                         tooltip:@"Actual size"
                           image:ToolbarImage(@"1.magnifyingglass", nil)
                          action:@selector(zoomActualSize:)];
    }
    if ([identifier isEqualToString:kToolbarZoomIn]) {
        return [self toolbarItem:identifier
                           label:@"In"
                         tooltip:@"Zoom in"
                           image:ToolbarImage(@"plus.magnifyingglass", nil)
                          action:@selector(zoomIn:)];
    }
    if ([identifier isEqualToString:kToolbarFitPage]) {
        return [self toolbarItem:identifier
                           label:@"Fit Page"
                         tooltip:@"Fit page"
                           image:ToolbarImage(@"rectangle.portrait", nil)
                          action:@selector(zoomFitPage:)];
    }
    if ([identifier isEqualToString:kToolbarFitWidth]) {
        return [self toolbarItem:identifier
                           label:@"Fit Width"
                         tooltip:@"Fit width"
                           image:ToolbarImage(@"arrow.left.and.right.square", nil)
                          action:@selector(zoomFitWidth:)];
    }
    if ([identifier isEqualToString:kToolbarRotateLeft]) {
        return [self toolbarItem:identifier
                           label:@"Left"
                         tooltip:@"Rotate left"
                           image:ToolbarImage(@"rotate.left", NSImageNameRefreshTemplate)
                          action:@selector(rotateLeft:)];
    }
    if ([identifier isEqualToString:kToolbarRotateRight]) {
        return [self toolbarItem:identifier
                           label:@"Right"
                         tooltip:@"Rotate right"
                           image:ToolbarImage(@"rotate.right", NSImageNameRefreshTemplate)
                          action:@selector(rotateRight:)];
    }
    return nil;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    NSArray<NSString*>* supportDirs =
        NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* supportDir = [supportDirs count] ? [supportDirs objectAtIndex:0] : NSTemporaryDirectory();
    NSString* settingsPath = [[supportDir stringByAppendingPathComponent:@"SumatraPDF"]
        stringByAppendingPathComponent:@"SumatraPDF-settings.txt"];
    MacPrefsInit([settingsPath fileSystemRepresentation]);
    _zoom = kMacZoomFitPage;
    _rotation = 0;
    _continuousView = YES;
    _watchedFile = -1;
    _activeTab = -1;
    _tabs = [[NSMutableArray alloc] init];
    _closedPaths = [[NSMutableArray alloc] init];

    NSRect frame = NSMakeRect(0, 0, 900, 1100);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                       NSWindowStyleMaskResizable;
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"SumatraPDF"];
    [_window setDelegate:self];

    [self installToolbar];

    _scrollView = [[NSScrollView alloc] initWithFrame:frame];
    [_scrollView setHasVerticalScroller:YES];
    [_scrollView setHasHorizontalScroller:YES];
    [_scrollView setAutohidesScrollers:YES];
    [_scrollView setBorderType:NSNoBorder];
    [_scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_scrollView setBackgroundColor:[NSColor colorWithCalibratedWhite:0.18 alpha:1.0]];
    [[_scrollView contentView] setPostsBoundsChangedNotifications:YES];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(scrollViewBoundsChanged:)
                                                 name:NSViewBoundsDidChangeNotification
                                               object:[_scrollView contentView]];

    _documentView = [[SumatraDocumentView alloc] initWithFrame:frame];
    [_documentView setOwner:self];
    [_documentView setScaleToFit:YES];
    [_scrollView setDocumentView:_documentView];

    [_window setContentView:_scrollView];
    [_window makeFirstResponder:_documentView];

    NSArray<NSString*>* args = [[NSProcessInfo processInfo] arguments];
    if ([args count] >= 2) {
        [self openPath:[args objectAtIndex:1]];
    } else {
        int sessionCount = MacPrefsSessionCount();
        int sessionActiveTab = MacPrefsSessionActiveTab();
        for (int i = 0; i < sessionCount; i++) {
            MacPrefsViewState savedState = {};
            char* savedPath = MacPrefsCopySessionTab(i, &savedState);
            NSString* path = savedPath ? [NSString stringWithUTF8String:savedPath] : nil;
            MacFreeString(savedPath);
            if (path) {
                [self openPath:path];
            }
        }
        if (sessionCount > 0) {
            [self activateTabAtIndex:sessionActiveTab];
        }
    }

    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)hasDocument {
    return _document != nullptr;
}

- (void)saveActiveTabState {
    if (_activeTab < 0 || _activeTab >= (int)[_tabs count]) {
        return;
    }
    SumatraTabState* tab = [_tabs objectAtIndex:(NSUInteger)_activeTab];
    [tab setCurrentPage:_currentPage];
    [tab setRotation:_rotation];
    [tab setZoom:_zoom];
    [tab setContinuous:_continuousView];
    [tab setScrollOrigin:[[_scrollView contentView] bounds].origin];
}

- (void)refreshTabSelector {
    NSInteger count = (NSInteger)[_tabs count];
    [_tabSelector setSegmentCount:count];
    for (NSInteger i = 0; i < count; i++) {
        SumatraTabState* tab = [_tabs objectAtIndex:(NSUInteger)i];
        [_tabSelector setLabel:[[tab path] lastPathComponent] forSegment:i];
        [_tabSelector setToolTip:[tab path] forSegment:i];
    }
    [_tabSelector setSelectedSegment:_activeTab];
}

- (void)showEmptyDocumentView {
    _document = nullptr;
    self.documentPath = nil;
    _pageCount = 0;
    _currentPage = 0;
    _rotation = 0;
    _zoom = kMacZoomFitPage;
    _continuousView = YES;
    [_documentView setScaleToFit:YES];
    [_documentView setFrame:[[_scrollView contentView] bounds]];
    [_documentView setImage:nullptr];
    [_documentView setPages:nil];
    [_documentView setImageSize:NSZeroSize];
    [_documentView setMessage:nil];
    [self updateTitle];
}

- (void)activateTabAtIndex:(int)index {
    if (index < 0 || index >= (int)[_tabs count]) {
        return;
    }
    [self saveActiveTabState];
    [self stopWatchingDocument];
    _activeTab = index;
    SumatraTabState* tab = [_tabs objectAtIndex:(NSUInteger)index];
    _document = [tab document];
    self.documentPath = [tab path];
    _pageCount = [tab pageCount];
    _currentPage = [tab currentPage];
    _rotation = [tab rotation];
    _zoom = [tab zoom];
    _continuousView = [tab continuous];
    [_documentView setImage:nullptr];
    [_documentView setPages:nil];
    [self renderCurrentPage];
    [[_scrollView contentView] scrollToPoint:[tab scrollOrigin]];
    [_scrollView reflectScrolledClipView:[_scrollView contentView]];
    if (_continuousView) {
        [self renderCurrentPage];
    }
    [self startWatchingDocument];
    [self refreshTabSelector];
    [_window makeFirstResponder:_documentView];
}

- (IBAction)selectTab:(id)sender {
    (void)sender;
    [self activateTabAtIndex:(int)[_tabSelector selectedSegment]];
}

- (int)tabIndexForPath:(NSString*)path {
    for (NSUInteger i = 0; i < [_tabs count]; i++) {
        if ([[[_tabs objectAtIndex:i] path] isEqualToString:path]) {
            return (int)i;
        }
    }
    return -1;
}

- (void)destroyTab:(SumatraTabState*)tab {
    MacPrefsViewState state = {};
    state.valid = true;
    state.continuous = [tab continuous];
    CGFloat zoom = [tab zoom];
    state.zoomVirtual = zoom > 0 ? zoom * 100.0 : zoom;
    state.rotation = [tab rotation];
    state.pageNo = [tab currentPage];
    MacPrefsSaveDocument([[tab path] fileSystemRepresentation], &state);
    MacCloseDocument([tab document]);
    [tab setDocument:nullptr];
}

- (void)closeAllTabs {
    [self saveActiveTabState];
    [self stopWatchingDocument];
    for (SumatraTabState* tab in _tabs) {
        [self destroyTab:tab];
    }
    [_tabs removeAllObjects];
    _activeTab = -1;
    [self refreshTabSelector];
    [self showEmptyDocumentView];
}

- (void)stopWatchingDocument {
    if (_fileWatcher) {
        dispatch_source_cancel(_fileWatcher);
        dispatch_release(_fileWatcher);
        _fileWatcher = nullptr;
        _watchedFile = -1;
    } else if (_watchedFile >= 0) {
        close(_watchedFile);
        _watchedFile = -1;
    }
}

- (void)startWatchingDocument {
    [self stopWatchingDocument];
    if (!_documentPath) {
        return;
    }
    _watchedFile = open([_documentPath fileSystemRepresentation], O_EVTONLY);
    if (_watchedFile < 0) {
        return;
    }
    _fileWatcher = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)_watchedFile,
                                          DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME,
                                          dispatch_get_main_queue());
    if (!_fileWatcher) {
        close(_watchedFile);
        _watchedFile = -1;
        return;
    }
    NSString* watchedPath = [[_documentPath copy] autorelease];
    int watchedFile = _watchedFile;
    dispatch_source_set_cancel_handler(_fileWatcher, ^{
      close(watchedFile);
    });
    dispatch_source_set_event_handler(_fileWatcher, ^{
      if (_document && [_documentPath isEqualToString:watchedPath] &&
          [[NSFileManager defaultManager] fileExistsAtPath:watchedPath]) {
          [self reloadActiveTab];
      }
    });
    dispatch_resume(_fileWatcher);
}

- (void)reloadActiveTab {
    if (_activeTab < 0 || _activeTab >= (int)[_tabs count] || !_documentPath) {
        return;
    }
    [self saveActiveTabState];
    char* error = nullptr;
    void* document = MacOpenDocument([_documentPath fileSystemRepresentation], PageRenderReady, self, &error);
    free(error);
    if (!document) {
        [self startWatchingDocument];
        return;
    }
    [self stopWatchingDocument];
    SumatraTabState* tab = [_tabs objectAtIndex:(NSUInteger)_activeTab];
    MacCloseDocument([tab document]);
    [tab setDocument:document];
    [tab setPageCount:MacPageCount(document)];
    [tab setCurrentPage:MAX(1, MIN([tab pageCount], [tab currentPage]))];
    _document = document;
    _pageCount = [tab pageCount];
    _currentPage = [tab currentPage];
    MacResetRenderer(_document);
    [self renderCurrentPage];
    [self startWatchingDocument];
}

- (void)showOpenError:(NSString*)message forPath:(NSString*)path {
    NSString* detail = path ? [path stringByAbbreviatingWithTildeInPath] : nil;
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setAlertStyle:NSAlertStyleWarning];
    [alert setMessageText:message ?: @"Could not open the document."];
    if ([detail length] > 0) {
        [alert setInformativeText:detail];
    }
    if (_window) {
        [alert beginSheetModalForWindow:_window completionHandler:nil];
    } else {
        [alert runModal];
    }
}

- (void)openPath:(NSString*)path {
    path = ResolveDocumentPath(path);
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        NSString* message = @"The selected file does not exist.";
        if (![self hasDocument]) {
            [_documentView setImage:nullptr];
            [_documentView setPages:nil];
            [_documentView setMessage:message];
        }
        [self showOpenError:message forPath:path];
        return;
    }

    int existing = [self tabIndexForPath:path];
    if (existing >= 0) {
        [self activateTabAtIndex:existing];
        return;
    }

    char* error = nullptr;
    void* doc = MacOpenDocument([path fileSystemRepresentation], PageRenderReady, self, &error);
    if (!doc) {
        NSString* message = error ? [NSString stringWithUTF8String:error] : @"Could not open the document.";
        free(error);
        if (![self hasDocument]) {
            [_documentView setImage:nullptr];
            [_documentView setPages:nil];
            [_documentView setMessage:message];
        }
        [self showOpenError:message forPath:path];
        return;
    }

    SumatraTabState* tab = [[[SumatraTabState alloc] init] autorelease];
    [tab setDocument:doc];
    [tab setPath:path];
    [tab setPageCount:MacPageCount(doc)];
    [tab setCurrentPage:1];
    [tab setRotation:0];
    [tab setZoom:kMacZoomFitPage];
    [tab setContinuous:YES];
    MacPrefsViewState state = {};
    if (MacPrefsOpenDocument([path fileSystemRepresentation], &state) && state.valid) {
        [tab setContinuous:state.continuous];
        [tab setRotation:state.rotation];
        [tab setCurrentPage:MAX(1, MIN([tab pageCount], state.pageNo))];
        [tab setZoom:state.zoomVirtual > 0 ? state.zoomVirtual / 100.0 : state.zoomVirtual];
    }
    [_tabs addObject:tab];
    [self activateTabAtIndex:(int)[_tabs count] - 1];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (CGFloat)backingScale {
    CGFloat scale = [_window backingScaleFactor];
    return scale > 0 ? scale : 1.0;
}

- (CGFloat)fileDPI {
    if (!_document) {
        return 96.0;
    }
    double dpi = MacFileDPI(_document);
    return dpi > 0 ? (CGFloat)dpi : 96.0;
}

- (CGFloat)actualSizeRenderZoom {
    return [self backingScale] * kCocoaPointsPerInch / [self fileDPI];
}

- (NSSize)pageSizeForDisplayZoom:(CGFloat)displayZoom {
    double wPts = 0, hPts = 0;
    if (!MacPageSize(_document, _currentPage, &wPts, &hPts) || wPts <= 0 || hPts <= 0) {
        return NSMakeSize(1, 1);
    }
    if (_rotation == 90 || _rotation == 270) {
        double t = wPts;
        wPts = hPts;
        hPts = t;
    }
    CGFloat scale = kCocoaPointsPerInch * displayZoom / [self fileDPI];
    return NSMakeSize((CGFloat)wPts * scale, (CGFloat)hPts * scale);
}

// Computes the user-visible zoom for fit modes. At display zoom 1.0, file DPI
// units are mapped to Cocoa points so "Actual Size" has physical scale.
- (CGFloat)fitZoomForWidthOnly:(BOOL)widthOnly {
    NSSize pageSize = [self pageSizeForDisplayZoom:1.0];
    if (pageSize.width <= 0 || pageSize.height <= 0) {
        return 1.0;
    }
    NSSize clip = [[_scrollView contentView] bounds].size;
    double margin = 16.0;
    double availW = MAX(1.0, clip.width - margin);
    double zoomW = availW / pageSize.width;
    if (widthOnly) {
        return (CGFloat)zoomW;
    }
    double availH = MAX(1.0, clip.height - margin);
    double zoomH = availH / pageSize.height;
    return (CGFloat)MIN(zoomW, zoomH);
}

- (CGFloat)layoutZoomVirtual {
    if (_zoom == kMacZoomFitWidth) {
        return kMacZoomFitWidth;
    }
    if (_zoom <= 0) {
        return kMacZoomFitPage;
    }
    return _zoom * 100.0;
}

- (CGImageRef)renderedImageForPage:(int)pageNo renderZoom:(double)renderZoom showErrors:(BOOL)showErrors {
    MacRenderedPage page = {};
    bool ok = MacCopyRenderedPage(_document, pageNo, (float)renderZoom, _rotation, &page);
    if (!ok) {
        MacFreeRenderedPage(&page);
        MacRequestPage(_document, pageNo, (float)renderZoom, _rotation, 0);
        return nullptr;
    }

    CGImageRef image = CreateImageFromRenderedPage(&page);
    MacFreeRenderedPage(&page);
    if (!image) {
        if (showErrors) {
            [self showOpenError:@"Could not render the page." forPath:_documentPath];
        }
        return nullptr;
    }

    return image;
}

- (BOOL)buildDocumentLayout:(MacDocumentLayout*)layout {
    NSRect visible = [[_scrollView contentView] bounds];
    MacLayoutParams params = {};
    params.continuous = _continuousView;
    params.startPage = _currentPage > 0 ? _currentPage : 1;
    params.viewX = (int)floor(visible.origin.x);
    params.viewY = (int)floor(visible.origin.y);
    params.viewWidth = (int)MAX(1.0, floor(visible.size.width));
    params.viewHeight = (int)MAX(1.0, floor(visible.size.height));
    params.zoomVirtual = [self layoutZoomVirtual];
    params.backingScale = [self backingScale];
    params.rotation = _rotation;
    return MacLayoutDocument(_document, &params, layout);
}

- (void)renderDocumentShowingErrors:(BOOL)showErrors {
    if (!_document) {
        return;
    }

    MacDocumentLayout layout = {};
    if (![self buildDocumentLayout:&layout]) {
        [_documentView setImage:nullptr];
        [_documentView setPages:nil];
        [_documentView setMessage:@"Could not render the page."];
        if (showErrors) {
            [self showOpenError:@"Could not render the page." forPath:_documentPath];
        }
        return;
    }

    NSMutableArray* pageViews = [NSMutableArray arrayWithCapacity:(NSUInteger)layout.pageCount];
    for (int i = 0; i < layout.pageCount; i++) {
        MacLayoutPage* page = &layout.pages[i];
        if (!page->shown) {
            continue;
        }
        SumatraPageImage* pageView = [[[SumatraPageImage alloc] init] autorelease];
        [pageView setPageNo:page->pageNo];
        [pageView setFrame:NSMakeRect(page->x, page->y, page->width, page->height)];
        [pageView setLayoutZoom:page->layoutZoom];
        if (page->visibleRatio > 0 || !_continuousView) {
            CGImageRef image =
                [self renderedImageForPage:page->pageNo renderZoom:page->renderZoom showErrors:showErrors];
            [pageView setImage:image];
            if (image) {
                CGImageRelease(image);
            }
        }
        int highlightCount = MacFindResultRectCount(_document, page->pageNo);
        int selectionCount = MacSelectionRectCount(_document, page->pageNo);
        if (highlightCount > 0 || selectionCount > 0) {
            NSMutableArray* highlights =
                [NSMutableArray arrayWithCapacity:(NSUInteger)(highlightCount + selectionCount)];
            for (int j = 0; j < highlightCount; j++) {
                MacDisplayRect rect = {};
                if (!MacFindResultRect(_document, page->pageNo, j, page->layoutZoom, _rotation, &rect)) {
                    continue;
                }
                NSRect highlight = NSMakeRect(page->x + rect.x, page->y + rect.y, rect.width, rect.height);
                [highlights addObject:[NSValue valueWithRect:highlight]];
            }
            for (int j = 0; j < selectionCount; j++) {
                MacDisplayRect rect = {};
                if (!MacSelectionRect(_document, page->pageNo, j, page->layoutZoom, _rotation, &rect)) {
                    continue;
                }
                NSRect highlight = NSMakeRect(page->x + rect.x, page->y + rect.y, rect.width, rect.height);
                [highlights addObject:[NSValue valueWithRect:highlight]];
            }
            [pageView setHighlights:highlights];
        }
        [pageViews addObject:pageView];
    }

    int nearbyPages[] = {layout.currentPage - 1, layout.currentPage + 1, layout.currentPage - 2,
                         layout.currentPage + 2};
    for (int pageNo : nearbyPages) {
        if (pageNo < 1 || pageNo > layout.pageCount) {
            continue;
        }
        MacLayoutPage* page = &layout.pages[pageNo - 1];
        int priority = abs(pageNo - layout.currentPage) == 1 ? 1 : 2;
        MacRequestPage(_document, pageNo, (float)page->renderZoom, _rotation, priority);
    }

    [_documentView setScaleToFit:NO];
    [_documentView setImage:nullptr];
    [_documentView setPages:pageViews];
    [_documentView setAutoresizingMask:NSViewNotSizable];
    [_documentView setFrameSize:NSMakeSize(layout.canvasWidth, layout.canvasHeight)];

    if (layout.currentPage != _currentPage && layout.currentPage >= 1) {
        _currentPage = layout.currentPage;
    }
    MacFreeDocumentLayout(&layout);

    [self updateTitle];
}

- (void)renderCurrentPage {
    [self renderDocumentShowingErrors:NO];
}

- (void)pageRenderReady {
    if (_document) {
        [self renderCurrentPage];
    }
}

- (void)updateTitle {
    NSString* name = _documentPath ? [_documentPath lastPathComponent] : @"SumatraPDF";
    if (_document && _pageCount > 0) {
        [_window setTitle:[NSString stringWithFormat:@"%@  —  page %d / %d", name, _currentPage, _pageCount]];
    } else {
        [_window setTitle:name];
    }
    [self updateToolbarStatus];
}

- (void)updateToolbarStatus {
    if (_pageLabel) {
        NSString* text = (_document && _pageCount > 0) ? [NSString stringWithFormat:@"%d / %d", _currentPage, _pageCount]
                                                       : @"No document";
        [_pageLabel setStringValue:text];
    }
    [_toolbar validateVisibleItems];
}

- (void*)documentHandle {
    return _document;
}

- (int)documentRotation {
    return _rotation;
}

- (void)selectionChanged {
    [self renderCurrentPage];
}

- (void)activateLinkAtPage:(int)pageNo x:(double)x y:(double)y zoom:(double)zoom {
    MacLink link = {};
    if (!MacLinkAtPoint(_document, pageNo, x, y, zoom, _rotation, &link)) {
        return;
    }
    if (link.kind == MacLinkKind::Page) {
        [self goToPage:link.pageNo];
    } else if (link.value) {
        NSString* value = [NSString stringWithUTF8String:link.value];
        if (link.kind == MacLinkKind::Url) {
            NSURL* url = [NSURL URLWithString:value];
            if (url) {
                [[NSWorkspace sharedWorkspace] openURL:url];
            }
        } else if (link.kind == MacLinkKind::File) {
            NSString* path = [value stringByStandardizingPath];
            if (![path isAbsolutePath]) {
                path = [[_documentPath stringByDeletingLastPathComponent] stringByAppendingPathComponent:path];
            }
            [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:path]];
        }
    }
    MacFreeLink(&link);
}

- (BOOL)canPerformAction:(SEL)action {
    if (!action || action == @selector(openDocument:) || action == @selector(toggleFullScreen:) ||
        action == @selector(openWebsite:)) {
        return YES;
    }
    if (action == @selector(openRecentDocument:)) {
        return MacPrefsRecentCount() > 0;
    }
    if (action == @selector(reopenClosedTab:)) {
        return [_closedPaths count] > 0;
    }
    if (action == @selector(selectNextTab:) || action == @selector(selectPreviousTab:)) {
        return [_tabs count] > 1;
    }
    if (action == @selector(toggleFavorite:)) {
        return [self hasDocument];
    }
    if (action == @selector(goToPrevPage:) || action == @selector(goToFirstPage:)) {
        return [self hasDocument] && _currentPage > 1;
    }
    if (action == @selector(copySelection:)) {
        return [self hasDocument] && MacHasSelection(_document);
    }
    if (action == @selector(goToNextPage:) || action == @selector(goToLastPage:)) {
        return [self hasDocument] && _currentPage < _pageCount;
    }
    if (action == @selector(performClose:) || action == @selector(showInFolder:) ||
        action == @selector(goToPageDialog:) || action == @selector(rotateLeft:) ||
        action == @selector(rotateRight:) || action == @selector(zoomFitPage:) ||
        action == @selector(zoomFitWidth:) || action == @selector(zoomActualSize:) ||
        action == @selector(zoomIn:) || action == @selector(zoomOut:) || action == @selector(setSinglePageView:) ||
        action == @selector(setContinuousPageView:) || action == @selector(findDocument:) ||
        action == @selector(findNext:) || action == @selector(findPrevious:) || action == @selector(showToc:) ||
        action == @selector(showProperties:) || action == @selector(selectAll:) || action == @selector(printDocument:)) {
        return [self hasDocument];
    }
    return YES;
}

- (BOOL)validateToolbarItem:(NSToolbarItem*)item {
    return [self canPerformAction:[item action]];
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

    if (_continuousView) {
        MacDocumentLayout layout = {};
        if ([self buildDocumentLayout:&layout]) {
            for (int i = 0; i < layout.pageCount; i++) {
                MacLayoutPage* page = &layout.pages[i];
                if (page->pageNo == pageNo) {
                    [[_scrollView contentView] scrollToPoint:NSMakePoint(0, page->y)];
                    break;
                }
            }
            MacFreeDocumentLayout(&layout);
        }
    } else {
        [[_scrollView contentView] scrollToPoint:NSZeroPoint];
    }
    [_scrollView reflectScrolledClipView:[_scrollView contentView]];
    [self renderCurrentPage];
}

#pragma mark - Menu / key actions

- (IBAction)openDocument:(id)sender {
    (void)sender;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setAllowsOtherFileTypes:YES];
    [panel setAllowedFileTypes:@[
        @"pdf", @"xps", @"oxps", @"epub", @"mobi", @"fb2", @"cbz", @"cbr", @"cb7", @"cbt", @"djvu", @"djv", @"chm",
        @"png", @"jpg", @"jpeg", @"gif", @"tif", @"tiff", @"tga", @"bmp", @"webp", @"jxl", @"heic", @"avif"
    ]];
    if ([panel runModal] == NSModalResponseOK) {
        NSURL* url = [[panel URLs] firstObject];
        if (url) {
            [self openPath:[url path]];
        }
    }
}

- (IBAction)openRecentDocument:(id)sender {
    (void)sender;
    int count = MacPrefsRecentCount();
    if (count == 0) {
        return;
    }
    NSPopUpButton* items = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 440, 28) pullsDown:NO] autorelease];
    NSMutableArray* paths = [NSMutableArray arrayWithCapacity:(NSUInteger)count];
    for (int i = 0; i < count; i++) {
        char* pathUtf8 = MacPrefsCopyRecentPath(i);
        NSString* path = pathUtf8 ? [NSString stringWithUTF8String:pathUtf8] : nil;
        MacFreeString(pathUtf8);
        if (path) {
            [paths addObject:path];
            [items addItemWithTitle:[path lastPathComponent]];
        }
    }
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"Open Recent"];
    [alert addButtonWithTitle:@"Open"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setAccessoryView:items];
    if ([alert runModal] == NSAlertFirstButtonReturn && [items indexOfSelectedItem] >= 0) {
        [self openPath:[paths objectAtIndex:(NSUInteger)[items indexOfSelectedItem]]];
    }
}

- (IBAction)performClose:(id)sender {
    (void)sender;
    MacPrefsSaveSession(nullptr, nullptr);
    if (_activeTab < 0 || _activeTab >= (int)[_tabs count]) {
        return;
    }
    [self saveActiveTabState];
    SumatraTabState* tab = [[[_tabs objectAtIndex:(NSUInteger)_activeTab] retain] autorelease];
    if ([tab path]) {
        [_closedPaths addObject:[tab path]];
        while ([_closedPaths count] > 10) {
            [_closedPaths removeObjectAtIndex:0];
        }
    }
    int nextTab = MIN(_activeTab, (int)[_tabs count] - 2);
    [self stopWatchingDocument];
    [self destroyTab:tab];
    [_tabs removeObjectAtIndex:(NSUInteger)_activeTab];
    _activeTab = -1;
    _document = nullptr;
    if ([_tabs count] > 0) {
        [self activateTabAtIndex:nextTab];
    } else {
        [self refreshTabSelector];
        [self showEmptyDocumentView];
    }
}

- (IBAction)reopenClosedTab:(id)sender {
    (void)sender;
    if ([_closedPaths count] == 0) {
        return;
    }
    NSString* path = [[[_closedPaths lastObject] retain] autorelease];
    [_closedPaths removeLastObject];
    [self openPath:path];
}

- (void)selectRelativeTab:(int)direction {
    int count = (int)[_tabs count];
    if (count < 2) {
        return;
    }
    [self activateTabAtIndex:(_activeTab + direction + count) % count];
}

- (IBAction)selectNextTab:(id)sender {
    (void)sender;
    [self selectRelativeTab:1];
}

- (IBAction)selectPreviousTab:(id)sender {
    (void)sender;
    [self selectRelativeTab:-1];
}

- (IBAction)toggleFavorite:(id)sender {
    (void)sender;
    if (!_documentPath || _currentPage < 1) {
        return;
    }
    const char* path = [_documentPath fileSystemRepresentation];
    if (MacPrefsHasFavorite(path, _currentPage)) {
        MacPrefsRemoveFavorite(path, _currentPage);
    } else {
        MacPrefsAddFavorite(path, _currentPage);
    }
}

- (IBAction)showFavorites:(id)sender {
    (void)sender;
    int count = MacPrefsFavoriteCount();
    if (count == 0) {
        NSAlert* alert = [[[NSAlert alloc] init] autorelease];
        [alert setMessageText:@"Favorites"];
        [alert setInformativeText:@"No favorite pages have been saved."];
        [alert beginSheetModalForWindow:_window completionHandler:nil];
        return;
    }
    NSPopUpButton* items = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 440, 28) pullsDown:NO] autorelease];
    NSMutableArray* paths = [NSMutableArray arrayWithCapacity:(NSUInteger)count];
    NSMutableArray* pages = [NSMutableArray arrayWithCapacity:(NSUInteger)count];
    for (int i = 0; i < count; i++) {
        char* pathUtf8 = MacPrefsCopyFavoritePath(i);
        NSString* path = pathUtf8 ? [NSString stringWithUTF8String:pathUtf8] : nil;
        MacFreeString(pathUtf8);
        int pageNo = MacPrefsFavoritePage(i);
        if (path) {
            [paths addObject:path];
            [pages addObject:[NSNumber numberWithInt:pageNo]];
            [items addItemWithTitle:[NSString stringWithFormat:@"%@ — page %d", [path lastPathComponent], pageNo]];
        }
    }
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"Favorites"];
    [alert addButtonWithTitle:@"Go"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setAccessoryView:items];
    if ([alert runModal] != NSAlertFirstButtonReturn || [items indexOfSelectedItem] < 0) {
        return;
    }
    NSUInteger index = (NSUInteger)[items indexOfSelectedItem];
    NSString* path = [paths objectAtIndex:index];
    int pageNo = [[pages objectAtIndex:index] intValue];
    if (![_documentPath isEqualToString:path]) {
        [self openPath:path];
    }
    [self goToPage:pageNo];
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

- (void)showSearchNotFound {
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setAlertStyle:NSAlertStyleInformational];
    [alert setMessageText:@"Text not found"];
    [alert setInformativeText:_findText ?: @""];
    [alert beginSheetModalForWindow:_window completionHandler:nil];
}

- (BOOL)findForward:(BOOL)forward restart:(BOOL)restart {
    if (!_document || [_findText length] == 0) {
        return NO;
    }
    bool found = MacFindText(_document, _currentPage, [_findText UTF8String], forward, restart);
    if (!found) {
        [self renderCurrentPage];
        [self showSearchNotFound];
        return NO;
    }
    int pageNo = MacFindResultPage(_document);
    if (pageNo > 0) {
        [self goToPage:pageNo];
        [self renderCurrentPage];
    }
    return YES;
}

- (IBAction)findDocument:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"Find in document"];
    [alert addButtonWithTitle:@"Find"];
    [alert addButtonWithTitle:@"Cancel"];
    NSTextField* input = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)] autorelease];
    [input setStringValue:_findText ?: @""];
    [alert setAccessoryView:input];
    if ([alert runModal] == NSAlertFirstButtonReturn && [[input stringValue] length] > 0) {
        self.findText = [input stringValue];
        [self findForward:YES restart:YES];
    }
}

- (IBAction)findNext:(id)sender {
    (void)sender;
    if ([_findText length] == 0) {
        [self findDocument:nil];
        return;
    }
    [self findForward:YES restart:NO];
}

- (IBAction)findPrevious:(id)sender {
    (void)sender;
    if ([_findText length] == 0) {
        [self findDocument:nil];
        return;
    }
    [self findForward:NO restart:NO];
}

- (IBAction)showToc:(id)sender {
    (void)sender;
    int count = MacTocItemCount(_document);
    if (count == 0) {
        NSAlert* alert = [[[NSAlert alloc] init] autorelease];
        [alert setMessageText:@"Table of Contents"];
        [alert setInformativeText:@"This document has no table of contents."];
        [alert beginSheetModalForWindow:_window completionHandler:nil];
        return;
    }

    NSPopUpButton* items = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 440, 28) pullsDown:NO] autorelease];
    int selectedIndex = 0;
    for (int i = 0; i < count; i++) {
        char* titleUtf8 = MacCopyTocItemTitle(_document, i);
        NSString* title = titleUtf8 ? [NSString stringWithUTF8String:titleUtf8] : @"";
        MacFreeString(titleUtf8);
        NSMutableString* indented = [NSMutableString string];
        for (int depth = MacTocItemDepth(_document, i); depth > 0; depth--) {
            [indented appendString:@"  "];
        }
        [indented appendString:title ?: @""];
        [items addItemWithTitle:indented];
        int pageNo = MacTocItemPage(_document, i);
        if (pageNo > 0 && pageNo <= _currentPage) {
            selectedIndex = i;
        }
    }
    [items selectItemAtIndex:selectedIndex];

    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"Table of Contents"];
    [alert addButtonWithTitle:@"Go"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setAccessoryView:items];
    if ([alert runModal] == NSAlertFirstButtonReturn) {
        int pageNo = MacTocItemPage(_document, (int)[items indexOfSelectedItem]);
        if (pageNo > 0) {
            [self goToPage:pageNo];
        }
    }
}

- (IBAction)showProperties:(id)sender {
    (void)sender;
    NSMutableString* text = [NSMutableString string];
    int count = MacPropertyCount(_document);
    for (int i = 0; i < count; i++) {
        char* nameUtf8 = MacCopyPropertyName(_document, i);
        char* valueUtf8 = MacCopyPropertyValue(_document, i);
        NSString* name = nameUtf8 ? [NSString stringWithUTF8String:nameUtf8] : @"";
        NSString* value = valueUtf8 ? [NSString stringWithUTF8String:valueUtf8] : @"";
        MacFreeString(nameUtf8);
        MacFreeString(valueUtf8);
        [text appendFormat:@"%@: %@\n", name ?: @"", value ?: @""];
    }
    if ([text length] == 0) {
        [text appendString:@"No document properties are available."];
    }

    NSTextView* textView = [[[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 520, 300)] autorelease];
    [textView setString:text];
    [textView setEditable:NO];
    [textView setSelectable:YES];
    [textView setFont:[NSFont systemFontOfSize:13]];
    NSScrollView* scroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 520, 300)] autorelease];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSBezelBorder];
    [scroll setDocumentView:textView];

    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"Document Properties"];
    [alert addButtonWithTitle:@"OK"];
    [alert setAccessoryView:scroll];
    [alert runModal];
}

- (IBAction)printDocument:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    SumatraPrintView* view = [[[SumatraPrintView alloc] initWithFrame:NSMakeRect(0, 0, 612, 792)] autorelease];
    [view setDocument:_document];
    [view setPageCount:_pageCount];
    [view setRotation:_rotation];
    NSPrintOperation* operation = [NSPrintOperation printOperationWithView:view printInfo:[NSPrintInfo sharedPrintInfo]];
    [operation setShowsPrintPanel:YES];
    [operation setShowsProgressPanel:YES];
    [operation runOperation];
}

- (IBAction)copySelection:(id)sender {
    (void)sender;
    char* textUtf8 = MacCopySelectionText(_document);
    if (!textUtf8) {
        return;
    }
    NSString* text = [NSString stringWithUTF8String:textUtf8];
    MacFreeString(textUtf8);
    if (!text) {
        return;
    }
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:text forType:NSPasteboardTypeString];
}

- (IBAction)selectAll:(id)sender {
    (void)sender;
    MacSelectAll(_document);
    [self renderCurrentPage];
}

- (void)rebuildCommandPaletteItems {
    [_commandPaletteItems removeAllItems];
    MacFilterCommandPalette(_commandPalette, [[_commandPaletteQuery stringValue] UTF8String]);
    int count = MacCommandPaletteCount(_commandPalette);
    for (int i = 0; i < count; i++) {
        char* textUtf8 = MacCopyCommandPaletteItem(_commandPalette, i);
        NSString* item = textUtf8 ? [NSString stringWithUTF8String:textUtf8] : @"";
        MacFreeString(textUtf8);
        [_commandPaletteItems addItemWithTitle:item ?: @""];
    }
    [_commandPaletteItems setEnabled:count > 0];
}

- (void)controlTextDidChange:(NSNotification*)notification {
    if ([notification object] == _commandPaletteQuery) {
        [self rebuildCommandPaletteItems];
    }
}

- (void)dispatchPaletteAction:(MacCommandAction)action {
    switch (action) {
        case MacCommandAction::Open:
            [self openDocument:nil];
            break;
        case MacCommandAction::Close:
            [self performClose:nil];
            break;
        case MacCommandAction::ReopenClosed:
            [self reopenClosedTab:nil];
            break;
        case MacCommandAction::NextTab:
            [self selectNextTab:nil];
            break;
        case MacCommandAction::PreviousTab:
            [self selectPreviousTab:nil];
            break;
        case MacCommandAction::Print:
            [self printDocument:nil];
            break;
        case MacCommandAction::ShowInFolder:
            [self showInFolder:nil];
            break;
        case MacCommandAction::Properties:
            [self showProperties:nil];
            break;
        case MacCommandAction::SinglePage:
            [self setSinglePageView:nil];
            break;
        case MacCommandAction::ToggleContinuous:
            if (_continuousView) {
                [self setSinglePageView:nil];
            } else {
                [self setContinuousPageView:nil];
            }
            break;
        case MacCommandAction::RotateLeft:
            [self rotateLeft:nil];
            break;
        case MacCommandAction::RotateRight:
            [self rotateRight:nil];
            break;
        case MacCommandAction::Fullscreen:
            [self toggleFullScreen:nil];
            break;
        case MacCommandAction::Copy:
            [self copySelection:nil];
            break;
        case MacCommandAction::SelectAll:
            [self selectAll:nil];
            break;
        case MacCommandAction::NextPage:
            [self goToNextPage:nil];
            break;
        case MacCommandAction::PreviousPage:
            [self goToPrevPage:nil];
            break;
        case MacCommandAction::FirstPage:
            [self goToFirstPage:nil];
            break;
        case MacCommandAction::LastPage:
            [self goToLastPage:nil];
            break;
        case MacCommandAction::GoToPage:
            [self goToPageDialog:nil];
            break;
        case MacCommandAction::Find:
            [self findDocument:nil];
            break;
        case MacCommandAction::FindNext:
            [self findNext:nil];
            break;
        case MacCommandAction::FindPrevious:
            [self findPrevious:nil];
            break;
        case MacCommandAction::FitPage:
            [self zoomFitPage:nil];
            break;
        case MacCommandAction::ActualSize:
            [self zoomActualSize:nil];
            break;
        case MacCommandAction::FitWidth:
            [self zoomFitWidth:nil];
            break;
        case MacCommandAction::ZoomIn:
            [self zoomIn:nil];
            break;
        case MacCommandAction::ZoomOut:
            [self zoomOut:nil];
            break;
        case MacCommandAction::Toc:
            [self showToc:nil];
            break;
        case MacCommandAction::KeyboardHelp:
            [self showKeyboardShortcuts:nil];
            break;
        case MacCommandAction::None:
            break;
    }
}

- (IBAction)showCommandPalette:(id)sender {
    (void)sender;
    _commandPalette = MacCreateCommandPalette();
    self.commandPaletteQuery = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 38, 460, 24)] autorelease];
    self.commandPaletteItems = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 460, 28) pullsDown:NO] autorelease];
    [_commandPaletteQuery setPlaceholderString:@"Type a command"];
    [_commandPaletteQuery setDelegate:self];
    NSView* accessory = [[[NSView alloc] initWithFrame:NSMakeRect(0, 0, 460, 64)] autorelease];
    [accessory addSubview:_commandPaletteQuery];
    [accessory addSubview:_commandPaletteItems];
    [self rebuildCommandPaletteItems];

    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"Command Palette"];
    [alert addButtonWithTitle:@"Run"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setAccessoryView:accessory];
    NSInteger result = [alert runModal];
    int index = (int)[_commandPaletteItems indexOfSelectedItem];
    int commandId = result == NSAlertFirstButtonReturn ? MacCommandPaletteItemCommand(_commandPalette, index) : 0;
    MacCommandAction action = MacCommandPaletteAction(commandId);
    [_commandPaletteQuery setDelegate:nil];
    self.commandPaletteQuery = nil;
    self.commandPaletteItems = nil;
    MacDestroyCommandPalette(_commandPalette);
    _commandPalette = nullptr;
    [self dispatchPaletteAction:action];
}

- (IBAction)rotateLeft:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _rotation = (_rotation + 270) % 360;
    MacResetRenderer(_document);
    [self renderCurrentPage];
}

- (IBAction)rotateRight:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _rotation = (_rotation + 90) % 360;
    MacResetRenderer(_document);
    [self renderCurrentPage];
}

- (IBAction)toggleFullScreen:(id)sender {
    (void)sender;
    [_window toggleFullScreen:nil];
}

- (IBAction)setSinglePageView:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _continuousView = NO;
    MacResetRenderer(_document);
    [self renderCurrentPage];
    [[_scrollView contentView] scrollToPoint:NSZeroPoint];
    [_scrollView reflectScrolledClipView:[_scrollView contentView]];
}

- (IBAction)setContinuousPageView:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _continuousView = YES;
    MacResetRenderer(_document);
    [self renderCurrentPage];
}

- (IBAction)zoomFitPage:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _zoom = kMacZoomFitPage;
    MacResetRenderer(_document);
    [self renderCurrentPage];
}

- (IBAction)zoomFitWidth:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _zoom = kMacZoomFitWidth;
    MacResetRenderer(_document);
    [self renderCurrentPage];
}

- (IBAction)zoomActualSize:(id)sender {
    (void)sender;
    if (!_document) {
        return;
    }
    _zoom = 1.0;
    MacResetRenderer(_document);
    [self renderCurrentPage];
}

- (void)applyZoomFactor:(CGFloat)factor {
    if (!_document) {
        return;
    }
    CGFloat base = (_zoom <= 0) ? [self fitZoomForWidthOnly:NO] : _zoom;
    if (_zoom == kMacZoomFitWidth) {
        base = [self fitZoomForWidthOnly:YES];
    }
    CGFloat z = base * factor;
    z = MAX(kZoomMin, MIN(kZoomMax, z));
    _zoom = z;
    MacResetRenderer(_document);
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

- (IBAction)showKeyboardShortcuts:(id)sender {
    (void)sender;
    bool fullscreen = ([_window styleMask] & NSWindowStyleMaskFullScreen) != 0;
    MacToggleKeyboardHelp(_window, fullscreen);
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
    if (action == @selector(setSinglePageView:)) {
        [item setState:(!_continuousView && [self hasDocument]) ? NSControlStateValueOn : NSControlStateValueOff];
    } else if (action == @selector(setContinuousPageView:)) {
        [item setState:(_continuousView && [self hasDocument]) ? NSControlStateValueOn : NSControlStateValueOff];
    } else if (action == @selector(toggleFavorite:)) {
        bool favorite = _documentPath && MacPrefsHasFavorite([_documentPath fileSystemRepresentation], _currentPage);
        [item setState:favorite ? NSControlStateValueOn : NSControlStateValueOff];
    }
    return [self canPerformAction:action];
}

#pragma mark - App lifecycle

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (_document) {
        [self renderCurrentPage];
    }
}

- (void)scrollViewBoundsChanged:(NSNotification*)notification {
    (void)notification;
    if (_document && _continuousView) {
        [self renderCurrentPage];
    }
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    (void)notification;
    if (_document) {
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
    [self saveActiveTabState];
    MacPrefsBeginSession();
    for (SumatraTabState* tab in _tabs) {
        MacPrefsViewState state = {};
        state.valid = true;
        state.continuous = [tab continuous];
        CGFloat zoom = [tab zoom];
        state.zoomVirtual = zoom > 0 ? zoom * 100.0 : zoom;
        state.rotation = [tab rotation];
        state.pageNo = [tab currentPage];
        MacPrefsAppendSession([[tab path] fileSystemRepresentation], &state);
    }
    MacPrefsFinishSession(_activeTab);
    [self closeAllTabs];
    MacPrefsShutdown();
    MacShutdown();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_window setDelegate:nil];
    [_toolbar setDelegate:nil];
    [_pageLabel release];
    [_toolbar release];
    [_documentView release];
    [_scrollView release];
    [_window release];
    [_documentPath release];
    [_findText release];
    [_commandPaletteQuery release];
    [_commandPaletteItems release];
    [self stopWatchingDocument];
    [_tabSelector release];
    [_closedPaths release];
    [_tabs release];
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
    AddItem(fileMenu, @"Open Recent…", @selector(openRecentDocument:), delegate, @"", 0);
    AddItem(fileMenu, @"Close", @selector(performClose:), delegate, @"w", NSEventModifierFlagCommand);
    AddItem(fileMenu, @"Reopen Closed Tab", @selector(reopenClosedTab:), delegate, @"t",
            NSEventModifierFlagCommand | NSEventModifierFlagShift);
    AddItem(fileMenu, @"Show in Folder", @selector(showInFolder:), delegate, @"", 0);
    AddPlaceholder(fileMenu, @"Open Next File in Folder", delegate);
    AddPlaceholder(fileMenu, @"Open Previous File in Folder", delegate);
    [fileMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(fileMenu, @"Save As…", delegate);
    AddPlaceholder(fileMenu, @"Rename…", delegate);
    AddItem(fileMenu, @"Print…", @selector(printDocument:), delegate, @"p", NSEventModifierFlagCommand);
    [fileMenu addItem:[NSMenuItem separatorItem]];
    AddItem(fileMenu, @"Properties", @selector(showProperties:), delegate, @"", 0);
    [fileItem setSubmenu:fileMenu];

    // Edit menu
    NSMenuItem* editItem = [[[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:editItem];
    NSMenu* editMenu = [[[NSMenu alloc] initWithTitle:@"Edit"] autorelease];
    AddItem(editMenu, @"Copy", @selector(copySelection:), delegate, @"c", NSEventModifierFlagCommand);
    AddItem(editMenu, @"Select All", @selector(selectAll:), delegate, @"a", NSEventModifierFlagCommand);
    [editMenu addItem:[NSMenuItem separatorItem]];
    AddItem(editMenu, @"Find…", @selector(findDocument:), delegate, @"f", NSEventModifierFlagCommand);
    AddItem(editMenu, @"Find Next", @selector(findNext:), delegate, @"g", NSEventModifierFlagCommand);
    AddItem(editMenu, @"Find Previous", @selector(findPrevious:), delegate, @"g",
            NSEventModifierFlagCommand | NSEventModifierFlagShift);
    [editItem setSubmenu:editMenu];

    // View menu
    NSMenuItem* viewItem = [[[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:viewItem];
    NSMenu* viewMenu = [[[NSMenu alloc] initWithTitle:@"View"] autorelease];
    AddItem(viewMenu, @"Single Page", @selector(setSinglePageView:), delegate, @"", 0);
    AddPlaceholder(viewMenu, @"Facing", delegate);
    AddPlaceholder(viewMenu, @"Book View", delegate);
    AddItem(viewMenu, @"Show Pages Continuously", @selector(setContinuousPageView:), delegate, @"", 0);
    AddPlaceholder(viewMenu, @"Manga Mode", delegate);
    [viewMenu addItem:[NSMenuItem separatorItem]];
    AddItem(viewMenu, @"Rotate Left", @selector(rotateLeft:), delegate, @"[", NSEventModifierFlagCommand);
    AddItem(viewMenu, @"Rotate Right", @selector(rotateRight:), delegate, @"]", NSEventModifierFlagCommand);
    [viewMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(viewMenu, @"Presentation", delegate);
    AddItem(viewMenu, @"Enter Full Screen", @selector(toggleFullScreen:), delegate, @"f",
            NSEventModifierFlagCommand | NSEventModifierFlagControl);
    [viewMenu addItem:[NSMenuItem separatorItem]];
    AddItem(viewMenu, @"Show Bookmarks", @selector(showToc:), delegate, @"", 0);
    AddItem(viewMenu, @"Add/Remove Favorite", @selector(toggleFavorite:), delegate, @"b", NSEventModifierFlagCommand);
    AddItem(viewMenu, @"Show Favorites…", @selector(showFavorites:), delegate, @"", 0);
    AddItem(viewMenu, @"Show Toolbar", @selector(toggleToolbarShown:), nil, @"", 0);
    AddItem(viewMenu, @"Command Palette…", @selector(showCommandPalette:), delegate, @"p",
            NSEventModifierFlagCommand | NSEventModifierFlagShift);
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
    AddItem(goMenu, @"Page…", @selector(goToPageDialog:), delegate, @"l", NSEventModifierFlagCommand);
    [goMenu addItem:[NSMenuItem separatorItem]];
    AddPlaceholder(goMenu, @"Back", delegate);
    AddPlaceholder(goMenu, @"Forward", delegate);
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
    [windowMenu addItem:[NSMenuItem separatorItem]];
    AddItem(windowMenu, @"Show Next Tab", @selector(selectNextTab:), delegate, @"]",
            NSEventModifierFlagCommand | NSEventModifierFlagShift);
    AddItem(windowMenu, @"Show Previous Tab", @selector(selectPreviousTab:), delegate, @"[",
            NSEventModifierFlagCommand | NSEventModifierFlagShift);
    [windowItem setSubmenu:windowMenu];
    [NSApp setWindowsMenu:windowMenu];

    // Help menu
    NSMenuItem* helpItem = [[[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""] autorelease];
    [mainMenu addItem:helpItem];
    NSMenu* helpMenu = [[[NSMenu alloc] initWithTitle:@"Help"] autorelease];
    AddItem(helpMenu, @"Keyboard Shortcuts", @selector(showKeyboardShortcuts:), delegate, @"", 0);
    [helpMenu addItem:[NSMenuItem separatorItem]];
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
