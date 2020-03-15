#import <Cocoa/Cocoa.h>


@interface SWELLApplication : NSApplication {

}
- (void)sendEvent:(NSEvent *)anEvent;

@end

@interface SWELLAppController : NSObject { 
}
-(IBAction)onSysMenuCommand:(id)sender;
@end