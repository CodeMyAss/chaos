#import <Cocoa/Cocoa.h>
#import "KOTextView.h"

@interface KOWindowController : NSWindowController

- (void) useFont:(NSFont*)font;
- (void) resizeWindowTo:(NSSize)size;
- (NSSize) windowSize;
- (void) setChar:(char)c x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg;

@property (copy) dispatch_block_t windowResizedHandler;

@end
