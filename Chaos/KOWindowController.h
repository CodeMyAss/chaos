#import <Cocoa/Cocoa.h>
#import "KOTextView.h"

@interface KOWindowController : NSWindowController <NSWindowDelegate>

- (void) useFont:(NSFont*)font;
- (void) useGridSize:(NSSize)size;
- (NSFont*) font;

- (int) cols;
- (int) rows;

- (void) setStr:(NSString*)str x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg;
- (void) clear:(NSColor*)bg;

@property (copy) dispatch_block_t windowResizedHandler;

- (void) useKeyDownHandler:(KOKeyDownHandler)handler;

@end
