#import <Cocoa/Cocoa.h>
#import "KOTextView.h"

@interface KOAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>

@property IBOutlet NSWindow *window;
@property IBOutlet KOTextView *tv;

@end
