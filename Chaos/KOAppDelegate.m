#import "KOAppDelegate.h"

@implementation KOAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    
    NSDictionary* attrs = @{NSForegroundColorAttributeName: [NSColor yellowColor],
                            NSBackgroundColorAttributeName: [NSColor blueColor],
                            NSFontAttributeName: [NSFont fontWithName:@"Menlo" size:12]};
    
    NSSize s = [@"x" sizeWithAttributes:attrs];
    s.width = round(s.width);
    s.height = round(s.height);
    
    NSRect f = [self.window frame];
    f.size.width = s.width * 50;
    f.size.height = s.height * 10;
    [self.window setFrame:f display:YES];
    
    [self.window setResizeIncrements:s];
}

@end
