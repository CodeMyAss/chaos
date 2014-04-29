#import <Cocoa/Cocoa.h>
#import "KOTextView.h"

@interface KOWindowController : NSWindowController <NSWindowDelegate>

- (void) useGridSize:(NSSize)size;

- (int) cols;
- (int) rows;

- (void) setStr:(NSString*)str x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg;

@property (copy) dispatch_block_t windowResizedHandler;

- (void) useKeyDownHandler:(KOKeyDownHandler)handler;

@end
