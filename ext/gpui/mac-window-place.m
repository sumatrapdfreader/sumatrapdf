#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

static BOOL gDidPlaceWindow = NO;

static void ApplyCompareWindowFrame(NSWindow* window) {
    const char* half = getenv("GPUI_COMPARE_WINDOW_HALF");
    if (!half) {
        return;
    }

    NSScreen* screen = [NSScreen mainScreen];
    if (!screen) {
        return;
    }

    NSRect work = screen.visibleFrame;
    CGFloat leftWidth = floor(NSWidth(work) / 2.0);
    CGFloat height = floor(NSHeight(work) * 0.8);
    CGFloat x = NSMinX(work);
    CGFloat width = leftWidth;
    if (strcmp(half, "right") == 0) {
        x += leftWidth;
        width = NSWidth(work) - leftWidth;
    } else if (strcmp(half, "left") != 0) {
        return;
    }
    CGFloat y = NSMaxY(work) - height;
    [window setFrame:NSMakeRect(x, y, width, height) display:NO];
}

static void PlaceCompareWindow(NSWindow* window) {
    if (gDidPlaceWindow) {
        return;
    }

    gDidPlaceWindow = YES;
    ApplyCompareWindowFrame(window);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC),
                   dispatch_get_main_queue(), ^{
                       ApplyCompareWindowFrame(window);
                   });
}

@interface NSWindow (GpuiComparePlacement)
- (void)gpuiCompareMakeKeyAndOrderFront:(id)sender;
- (void)gpuiCompareOrderFront:(id)sender;
@end

@implementation NSWindow (GpuiComparePlacement)
- (void)gpuiCompareMakeKeyAndOrderFront:(id)sender {
    [self gpuiCompareMakeKeyAndOrderFront:sender];
    PlaceCompareWindow(self);
}

- (void)gpuiCompareOrderFront:(id)sender {
    [self gpuiCompareOrderFront:sender];
    PlaceCompareWindow(self);
}
@end

static void SwapWindowMethod(SEL original, SEL replacement) {
    Method originalMethod = class_getInstanceMethod([NSWindow class], original);
    Method replacementMethod = class_getInstanceMethod([NSWindow class], replacement);
    method_exchangeImplementations(originalMethod, replacementMethod);
}

__attribute__((constructor)) static void InstallCompareWindowPlacement(void) {
    SwapWindowMethod(@selector(makeKeyAndOrderFront:), @selector(gpuiCompareMakeKeyAndOrderFront:));
    SwapWindowMethod(@selector(orderFront:), @selector(gpuiCompareOrderFront:));
}
