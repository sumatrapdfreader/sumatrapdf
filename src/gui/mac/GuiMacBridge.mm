/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "gui/mac/GuiMacBridge.h"

struct MacGuiWindowData;

struct MacGuiDrawContext {
    NSGraphicsContext* graphics;
    NSRect bounds;
    bool mirrored;
};

@interface SumatraPlatformView : NSView {
    MacGuiWindowData* _platformData;
    NSTrackingArea* _trackingArea;
    NSEvent* _activeMouseEvent;
}
- (id)initWithFrame:(NSRect)frame platformData:(MacGuiWindowData*)data;
- (NSEvent*)activeMouseEvent;
@end

@interface SumatraPlatformWindowDelegate : NSObject <NSWindowDelegate> {
    MacGuiWindowData* _platformData;
}
- (id)initWithPlatformData:(MacGuiWindowData*)data;
@end

struct MacGuiWindowData {
    NSWindow* window;
    SumatraPlatformView* view;
    SumatraPlatformWindowDelegate* delegate;
    NSWindow* parent;
    void* userData;
    MacGuiWindowCallbacks callbacks;
    bool destroying;
};

static const void* kPlatformWindowDataKey = &kPlatformWindowDataKey;

static NSString* StringFromUtf8(const char* s, int len) {
    if (!s || len <= 0) {
        return @"";
    }
    NSString* result = [[[NSString alloc] initWithBytes:s length:(NSUInteger)len encoding:NSUTF8StringEncoding]
        autorelease];
    return result ?: @"";
}

static NSColor* ColorFromPacked(uint32_t color, uint8_t alpha) {
    CGFloat r = (CGFloat)(color & 0xff) / 255.0;
    CGFloat g = (CGFloat)((color >> 8) & 0xff) / 255.0;
    CGFloat b = (CGFloat)((color >> 16) & 0xff) / 255.0;
    CGFloat a = (CGFloat)alpha / 255.0;
    return [NSColor colorWithCalibratedRed:r green:g blue:b alpha:a];
}

static NSRect RectFromMac(MacGuiDrawContext* context, MacGuiRect rect) {
    CGFloat x = rect.x;
    if (context && context->mirrored) {
        x = NSWidth(context->bounds) - rect.x - rect.dx;
    }
    return NSMakeRect(x, rect.y, rect.dx, rect.dy);
}

static CGFloat PrimaryScreenTop() {
    NSArray<NSScreen*>* screens = [NSScreen screens];
    NSScreen* primary = [screens count] ? [screens objectAtIndex:0] : [NSScreen mainScreen];
    return primary ? NSMaxY([primary frame]) : 0;
}

static MacGuiRect MacRectFromScreenRect(NSRect rect) {
    CGFloat y = PrimaryScreenTop() - NSMaxY(rect);
    return {(int)floor(NSMinX(rect)), (int)floor(y), (int)ceil(NSWidth(rect)), (int)ceil(NSHeight(rect))};
}

static NSRect ScreenRectFromMac(MacGuiRect rect) {
    CGFloat y = PrimaryScreenTop() - rect.y - rect.dy;
    return NSMakeRect(rect.x, y, rect.dx, rect.dy);
}

static NSEventModifierFlags DeviceIndependentModifiers(NSEvent* event) {
    return [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
}

static MacGuiPointerEvent PointerEvent(SumatraPlatformView* view, NSEvent* event, int type) {
    NSPoint p = [view convertPoint:[event locationInWindow] fromView:nil];
    NSEventModifierFlags modifiers = DeviceIndependentModifiers(event);
    MacGuiPointerEvent result{};
    result.type = type;
    result.x = (int)floor(p.x);
    result.y = (int)floor(p.y);
    result.button = (int)[event buttonNumber] + 1;
    result.isCtrl = (modifiers & NSEventModifierFlagControl) != 0;
    result.isShift = (modifiers & NSEventModifierFlagShift) != 0;
    result.isAlt = (modifiers & NSEventModifierFlagOption) != 0;
    return result;
}

@implementation SumatraPlatformView

- (id)initWithFrame:(NSRect)frame platformData:(MacGuiWindowData*)data {
    self = [super initWithFrame:frame];
    if (self) {
        _platformData = data;
    }
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)updateTrackingAreas {
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
        [_trackingArea release];
    }
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways |
                                    NSTrackingInVisibleRect;
    _trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:options owner:self userInfo:nil];
    [self addTrackingArea:_trackingArea];
    [super updateTrackingAreas];
}

- (void)dealloc {
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
        [_trackingArea release];
    }
    [super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!_platformData || !_platformData->callbacks.paint) {
        return;
    }
    NSRect bounds = [self bounds];
    MacGuiDrawContext context{[NSGraphicsContext currentContext], bounds, false};
    _platformData->callbacks.paint(_platformData->userData, &context, (int)ceil(NSWidth(bounds)),
                                   (int)ceil(NSHeight(bounds)));
}

- (void)sendPointerEvent:(NSEvent*)event type:(int)type {
    if (!_platformData || !_platformData->callbacks.pointer) {
        return;
    }
    MacGuiPointerEvent pointer = PointerEvent(self, event, type);
    _activeMouseEvent = event;
    _platformData->callbacks.pointer(_platformData->userData, &pointer);
    _activeMouseEvent = nil;
}

- (void)mouseMoved:(NSEvent*)event {
    [self sendPointerEvent:event type:0];
}

- (void)mouseDragged:(NSEvent*)event {
    [self sendPointerEvent:event type:0];
}

- (void)mouseExited:(NSEvent*)event {
    [self sendPointerEvent:event type:1];
}

- (void)mouseDown:(NSEvent*)event {
    [self sendPointerEvent:event type:2];
}

- (void)mouseUp:(NSEvent*)event {
    [self sendPointerEvent:event type:3];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self sendPointerEvent:event type:2];
}

- (void)rightMouseUp:(NSEvent*)event {
    [self sendPointerEvent:event type:3];
}

- (void)otherMouseDown:(NSEvent*)event {
    [self sendPointerEvent:event type:2];
}

- (void)otherMouseUp:(NSEvent*)event {
    [self sendPointerEvent:event type:3];
}

- (void)keyDown:(NSEvent*)event {
    if (!_platformData || !_platformData->callbacks.key) {
        [super keyDown:event];
        return;
    }
    NSString* characters = [event characters];
    NSEventModifierFlags modifiers = DeviceIndependentModifiers(event);
    MacGuiKeyEvent key{};
    key.codepoint = [characters length] ? [characters characterAtIndex:0] : 0;
    key.isCtrl = (modifiers & NSEventModifierFlagControl) != 0;
    key.isShift = (modifiers & NSEventModifierFlagShift) != 0;
    key.isAlt = (modifiers & NSEventModifierFlagOption) != 0;
    if (!_platformData->callbacks.key(_platformData->userData, &key)) {
        [super keyDown:event];
    }
}

- (NSEvent*)activeMouseEvent {
    return _activeMouseEvent;
}

@end

@implementation SumatraPlatformWindowDelegate

- (id)initWithPlatformData:(MacGuiWindowData*)data {
    self = [super init];
    if (self) {
        _platformData = data;
    }
    return self;
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if (_platformData && !_platformData->destroying && _platformData->callbacks.close) {
        _platformData->callbacks.close(_platformData->userData);
    }
    return NO;
}

@end

void* MacGuiFontCreate(const char* name, int nameLen, float sizePt, bool bold, bool italic) {
    NSString* family = StringFromUtf8(name, nameLen);
    NSFont* font = nil;
    if ([family isEqualToString:@"system-ui"] || ![family length]) {
        font = bold ? [NSFont boldSystemFontOfSize:sizePt] : [NSFont systemFontOfSize:sizePt];
    } else {
        font = [NSFont fontWithName:family size:sizePt];
    }
    if (!font) {
        font = [NSFont systemFontOfSize:sizePt];
    }
    NSFontManager* manager = [NSFontManager sharedFontManager];
    if (bold) {
        NSFont* converted = [manager convertFont:font toHaveTrait:NSBoldFontMask];
        if (converted) font = converted;
    }
    if (italic) {
        NSFont* converted = [manager convertFont:font toHaveTrait:NSItalicFontMask];
        if (converted) font = converted;
    }
    return [font retain];
}

void MacGuiFontMeasure(void* nativeFont, const char* text, int textLen, int maxDx, int* dx, int* dy) {
    NSFont* font = (NSFont*)nativeFont;
    NSString* string = StringFromUtf8(text, textLen);
    NSMutableParagraphStyle* paragraph = [[[NSMutableParagraphStyle alloc] init] autorelease];
    [paragraph setLineBreakMode:maxDx >= 0 ? NSLineBreakByWordWrapping : NSLineBreakByClipping];
    NSDictionary* attrs = @{NSFontAttributeName : font, NSParagraphStyleAttributeName : paragraph};
    CGFloat width = maxDx >= 0 ? maxDx : CGFLOAT_MAX;
    NSRect bounds = [string boundingRectWithSize:NSMakeSize(width, CGFLOAT_MAX)
                                         options:NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingUsesFontLeading
                                      attributes:attrs];
    if (dx) *dx = (int)ceil(NSWidth(bounds));
    if (dy) *dy = (int)ceil(NSHeight(bounds));
}

float MacGuiDefaultFontSize() {
    return (float)[NSFont systemFontSize];
}

void* MacGuiWindowCreate(void* parentHandle, const char* title, int titleLen, int dx, int dy, bool visible,
                         bool frameless, bool resizable, void* userData, const MacGuiWindowCallbacks* callbacks) {
    NSUInteger style = frameless ? NSWindowStyleMaskBorderless : NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
    if (resizable) {
        style |= NSWindowStyleMaskResizable;
    }
    NSRect frame = NSMakeRect(0, 0, MAX(dx, 1), MAX(dy, 1));
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
    if (!window) {
        return nullptr;
    }
    auto* data = new MacGuiWindowData();
    data->window = window;
    data->parent = (NSWindow*)parentHandle;
    data->userData = userData;
    if (callbacks) {
        data->callbacks = *callbacks;
    }
    data->view = [[SumatraPlatformView alloc] initWithFrame:NSMakeRect(0, 0, dx, dy) platformData:data];
    data->delegate = [[SumatraPlatformWindowDelegate alloc] initWithPlatformData:data];

    [window setTitle:StringFromUtf8(title, titleLen)];
    [window setReleasedWhenClosed:NO];
    [window setDelegate:data->delegate];
    [window setContentView:data->view];
    [window setAcceptsMouseMovedEvents:YES];
    if (frameless) {
        [window setHasShadow:YES];
    }
    if (data->parent) {
        [data->parent addChildWindow:window ordered:NSWindowAbove];
    }
    objc_setAssociatedObject(window, kPlatformWindowDataKey, [NSValue valueWithPointer:data],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    if (visible) {
        [window makeKeyAndOrderFront:nil];
    }
    return window;
}

static MacGuiWindowData* WindowData(void* handle) {
    NSWindow* window = (NSWindow*)handle;
    NSValue* value = window ? objc_getAssociatedObject(window, kPlatformWindowDataKey) : nil;
    return value ? (MacGuiWindowData*)[value pointerValue] : nullptr;
}

void MacGuiWindowDestroy(void* handle) {
    NSWindow* window = (NSWindow*)handle;
    MacGuiWindowData* data = WindowData(handle);
    if (!window || !data) {
        return;
    }
    data->destroying = true;
    objc_setAssociatedObject(window, kPlatformWindowDataKey, nil, OBJC_ASSOCIATION_ASSIGN);
    [window setDelegate:nil];
    if (data->parent) {
        [data->parent removeChildWindow:window];
    }
    [window orderOut:nil];
    [window close];
    [data->delegate release];
    [data->view release];
    [window release];
    delete data;
}

MacGuiRect MacGuiWindowClientRect(void* handle) {
    NSWindow* window = (NSWindow*)handle;
    NSRect bounds = window ? [[window contentView] bounds] : NSZeroRect;
    return {0, 0, (int)ceil(NSWidth(bounds)), (int)ceil(NSHeight(bounds))};
}

MacGuiRect MacGuiWindowScreenRect(void* handle) {
    NSWindow* window = (NSWindow*)handle;
    return window ? MacRectFromScreenRect([window frame]) : MacGuiRect{};
}

MacGuiRect MacGuiWindowWorkArea(void* handle) {
    NSWindow* window = (NSWindow*)handle;
    NSScreen* screen = window ? [window screen] : [NSScreen mainScreen];
    return screen ? MacRectFromScreenRect([screen visibleFrame]) : MacGuiRect{};
}

void MacGuiWindowSetBounds(void* handle, MacGuiRect rect) {
    NSWindow* window = (NSWindow*)handle;
    if (window) {
        [window setFrame:ScreenRectFromMac(rect) display:YES];
    }
}

void MacGuiWindowShow(void* handle, bool show) {
    NSWindow* window = (NSWindow*)handle;
    if (!window) return;
    if (show) {
        [window makeKeyAndOrderFront:nil];
    } else {
        [window orderOut:nil];
    }
}

void MacGuiWindowFocus(void* handle) {
    MacGuiWindowData* data = WindowData(handle);
    if (data) {
        [data->window makeKeyAndOrderFront:nil];
        [data->window makeFirstResponder:data->view];
    }
}

void MacGuiWindowInvalidate(void* handle) {
    MacGuiWindowData* data = WindowData(handle);
    if (data) {
        [data->view setNeedsDisplay:YES];
    }
}

void MacGuiWindowSetCursor(void* handle, int cursor) {
    if (!handle) return;
    NSCursor* native = [NSCursor arrowCursor];
    switch (cursor) {
        case 0:
            native = nil;
            break;
        case 2:
            native = [NSCursor IBeamCursor];
            break;
        case 3:
            native = [NSCursor pointingHandCursor];
            break;
        case 4:
            native = [NSCursor crosshairCursor];
            break;
        case 5:
            native = [NSCursor openHandCursor];
            break;
        case 6:
            native = [NSCursor resizeUpDownCursor];
            break;
        case 7:
            native = [NSCursor resizeLeftRightCursor];
            break;
        case 8:
            native = [NSCursor operationNotAllowedCursor];
            break;
        default:
            break;
    }
    if (native) {
        [native set];
    } else {
        [NSCursor hide];
    }
}

void MacGuiWindowBeginMove(void* handle) {
    MacGuiWindowData* data = WindowData(handle);
    NSEvent* event = data ? [data->view activeMouseEvent] : nil;
    if (event && [data->window respondsToSelector:@selector(performWindowDragWithEvent:)]) {
        [data->window performWindowDragWithEvent:event];
    }
}

bool MacGuiWindowIsMaximized(void* handle) {
    NSWindow* window = (NSWindow*)handle;
    return window && ([window isZoomed] || ([window styleMask] & NSWindowStyleMaskFullScreen));
}

void MacGuiWindowActivateIfForeground(void* handle) {
    NSWindow* window = (NSWindow*)handle;
    if (window && [NSApp isActive]) {
        [window makeKeyAndOrderFront:nil];
    }
}

void MacGuiPostTask(void (*fn)(void*), void* data) {
    dispatch_async_f(dispatch_get_main_queue(), data, fn);
}

void MacGuiFillRect(void* nativeContext, MacGuiRect rect, uint32_t color, uint8_t alpha) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    [ColorFromPacked(color, alpha) setFill];
    NSRectFill(RectFromMac(context, rect));
}

void MacGuiDrawRect(void* nativeContext, MacGuiRect rect, uint32_t color, int thickness, bool dashed) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    NSBezierPath* path = [NSBezierPath bezierPathWithRect:NSInsetRect(RectFromMac(context, rect), thickness / 2.0,
                                                                      thickness / 2.0)];
    [path setLineWidth:thickness];
    if (dashed) {
        CGFloat pattern[] = {2, 2};
        [path setLineDash:pattern count:2 phase:0];
    }
    [ColorFromPacked(color, 255) setStroke];
    [path stroke];
}

void MacGuiFillRoundedRect(void* nativeContext, MacGuiRect rect, int radius, uint32_t fill, bool hasFill,
                           uint32_t border, bool hasBorder) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:RectFromMac(context, rect) xRadius:radius yRadius:radius];
    if (hasFill) {
        [ColorFromPacked(fill, 255) setFill];
        [path fill];
    }
    if (hasBorder) {
        [ColorFromPacked(border, 255) setStroke];
        [path setLineWidth:1];
        [path stroke];
    }
}

void MacGuiFillEllipse(void* nativeContext, MacGuiRect rect, uint32_t color, uint8_t alpha) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    [ColorFromPacked(color, alpha) setFill];
    [[NSBezierPath bezierPathWithOvalInRect:RectFromMac(context, rect)] fill];
}

void MacGuiDrawLine(void* nativeContext, int x1, int y1, int x2, int y2, uint32_t color, float thickness,
                    uint8_t alpha) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    if (context && context->mirrored) {
        x1 = (int)NSWidth(context->bounds) - x1;
        x2 = (int)NSWidth(context->bounds) - x2;
    }
    NSBezierPath* path = [NSBezierPath bezierPath];
    [path moveToPoint:NSMakePoint(x1, y1)];
    [path lineToPoint:NSMakePoint(x2, y2)];
    [path setLineWidth:thickness];
    [ColorFromPacked(color, alpha) setStroke];
    [path stroke];
}

void MacGuiDrawText(void* nativeContext, const char* text, int textLen, MacGuiRect rect, uint32_t flags,
                    void* nativeFont, uint32_t color) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    NSString* string = StringFromUtf8(text, textLen);
    NSFont* font = (NSFont*)nativeFont;
    NSMutableParagraphStyle* paragraph = [[[NSMutableParagraphStyle alloc] init] autorelease];
    if (flags & MacGuiTextCenter) {
        [paragraph setAlignment:NSTextAlignmentCenter];
    } else if (flags & MacGuiTextRight) {
        [paragraph setAlignment:NSTextAlignmentRight];
    }
    if (flags & MacGuiTextPathEllipsis) {
        [paragraph setLineBreakMode:NSLineBreakByTruncatingMiddle];
    } else if (flags & MacGuiTextEllipsis) {
        [paragraph setLineBreakMode:NSLineBreakByTruncatingTail];
    } else if (flags & MacGuiTextWrap) {
        [paragraph setLineBreakMode:NSLineBreakByWordWrapping];
    } else {
        [paragraph setLineBreakMode:NSLineBreakByClipping];
    }
    NSDictionary* attrs = @{
        NSFontAttributeName : font,
        NSForegroundColorAttributeName : ColorFromPacked(color, 255),
        NSParagraphStyleAttributeName : paragraph,
    };
    NSRect drawRect = RectFromMac(context, rect);
    NSStringDrawingOptions options = NSStringDrawingUsesFontLeading;
    if ((flags & MacGuiTextWrap) || !(flags & MacGuiTextSingleLine)) {
        options |= NSStringDrawingUsesLineFragmentOrigin;
    }
    if (flags & (MacGuiTextEllipsis | MacGuiTextPathEllipsis)) {
        options |= NSStringDrawingTruncatesLastVisibleLine;
    }
    if (flags & MacGuiTextVCenter) {
        NSRect measured = [string boundingRectWithSize:drawRect.size options:options attributes:attrs];
        drawRect.origin.y += MAX(0, floor((NSHeight(drawRect) - NSHeight(measured)) / 2.0));
    }
    [string drawWithRect:drawRect options:options attributes:attrs];
}

void MacGuiDrawPixmap(void* nativeContext, const uint8_t* data, int width, int height, int stride,
                      MacGuiRect rect) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    if (!data || width <= 0 || height <= 0) return;
    CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, data, (size_t)stride * height, nullptr);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;
    CGImageRef image = CGImageCreate((size_t)width, (size_t)height, 8, 32, (size_t)stride, colorSpace, bitmapInfo,
                                     provider, nullptr, false, kCGRenderingIntentDefault);
    if (image) {
        NSImage* nativeImage = [[[NSImage alloc] initWithCGImage:image size:NSMakeSize(width, height)] autorelease];
        [nativeImage drawInRect:RectFromMac(context, rect)
                      fromRect:NSZeroRect
                     operation:NSCompositingOperationSourceOver
                      fraction:1.0
                respectFlipped:YES
                         hints:nil];
        CGImageRelease(image);
    }
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
}

void MacGuiPushClip(void* nativeContext, MacGuiRect rect) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(RectFromMac(context, rect));
}

void MacGuiPopClip(void*) {
    [NSGraphicsContext restoreGraphicsState];
}

bool MacGuiSetMirrored(void* nativeContext, bool mirrored) {
    auto* context = (MacGuiDrawContext*)nativeContext;
    bool previous = context ? context->mirrored : false;
    if (context) {
        context->mirrored = mirrored;
    }
    return previous;
}
