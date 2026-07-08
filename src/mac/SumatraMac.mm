#import <Cocoa/Cocoa.h>

#include "mac/SumatraMacEngine.h"

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

static void InstallMainMenu() {
    NSMenu* mainMenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    NSMenuItem* appMenuItem = [[[NSMenuItem alloc] initWithTitle:@""
                                                          action:nil
                                                   keyEquivalent:@""] autorelease];
    [mainMenu addItem:appMenuItem];

    NSMenu* appMenu = [[[NSMenu alloc] initWithTitle:@"SumatraPDF"] autorelease];
    NSString* quitTitle = [@"Quit " stringByAppendingString:[[NSProcessInfo processInfo] processName]];
    NSMenuItem* quitItem = [[[NSMenuItem alloc] initWithTitle:quitTitle
                                                       action:@selector(terminate:)
                                                keyEquivalent:@"q"] autorelease];
    [appMenu addItem:quitItem];
    [appMenuItem setSubmenu:appMenu];
    [NSApp setMainMenu:mainMenu];
}

@interface SumatraDocumentView : NSView
@property(nonatomic) CGImageRef image;
@property(nonatomic, copy) NSString* message;
@end

@implementation SumatraDocumentView

- (BOOL)isFlipped {
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
        NSString* text = _message ?: @"Pass a document path on the command line.";
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
    CGFloat margin = 24.0;
    CGFloat scale = MIN((bounds.size.width - 2 * margin) / imageW, (bounds.size.height - 2 * margin) / imageH);
    if (!isfinite(scale) || scale <= 0) {
        scale = 1;
    }
    scale = MIN(scale, 1.0);

    CGSize drawSize = CGSizeMake(floor(imageW * scale), floor(imageH * scale));
    CGRect drawRect = CGRectMake(floor((bounds.size.width - drawSize.width) / 2.0),
                                 floor((bounds.size.height - drawSize.height) / 2.0), drawSize.width,
                                 drawSize.height);

    [[NSColor colorWithCalibratedWhite:0.92 alpha:1.0] setFill];
    NSRectFill(NSRectFromCGRect(CGRectInset(drawRect, -1, -1)));

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, bounds.size.height);
    CGContextScaleCTM(ctx, 1, -1);
    CGRect cgDrawRect = CGRectMake(drawRect.origin.x, bounds.size.height - drawRect.origin.y - drawRect.size.height,
                                   drawRect.size.width, drawRect.size.height);
    CGContextDrawImage(ctx, cgDrawRect, _image);
    CGContextRestoreGState(ctx);
}

@end

@interface SumatraAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, retain) NSWindow* window;
@property(nonatomic, retain) SumatraDocumentView* documentView;
@property(nonatomic) void* document;
@end

@implementation SumatraAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;

    NSRect frame = NSMakeRect(0, 0, 900, 1100);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                       NSWindowStyleMaskResizable;
    _window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:style
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    [_window setTitle:@"SumatraPDF"];

    _documentView = [[SumatraDocumentView alloc] initWithFrame:frame];
    [_documentView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_window setContentView:_documentView];

    NSArray<NSString*>* args = [[NSProcessInfo processInfo] arguments];
    if ([args count] < 2) {
        [_documentView setMessage:@"Pass a document path on the command line."];
    } else {
        [self openPath:[args objectAtIndex:1]];
    }

    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)openPath:(NSString*)path {
    path = ResolveDocumentPath(path);
    MacRenderedPage page = {};
    bool ok = MacOpenDocument([path fileSystemRepresentation], &page);
    if (!ok) {
        NSString* message = page.error ? [NSString stringWithUTF8String:page.error] : @"Could not open the document.";
        [_documentView setMessage:message];
        MacFreeRenderedPage(&page);
        return;
    }

    CGImageRef image = CreateImageFromRenderedPage(&page);
    _document = page.document;
    page.document = nullptr;
    MacFreeRenderedPage(&page);

    if (!image) {
        [_documentView setMessage:@"Could not render the first page."];
        return;
    }

    [_documentView setImage:image];
    CGImageRelease(image);
    [_window setTitle:[path lastPathComponent]];
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    (void)notification;
    if (_document) {
        MacCloseDocument(_document);
        _document = nullptr;
    }
    MacShutdown();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)dealloc {
    [_documentView release];
    [_window release];
    [super dealloc];
}

@end

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    InstallMainMenu();

    SumatraAppDelegate* delegate = [[SumatraAppDelegate alloc] init];
    [app setDelegate:delegate];
    [app run];

    [delegate release];
    [pool release];
    return 0;
}
