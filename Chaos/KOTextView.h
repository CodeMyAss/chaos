#import <Cocoa/Cocoa.h>

@interface KOTextView : NSView

@property (readonly) CGFloat charWidth;
@property (readonly) CGFloat charHeight;

@property (readonly) int rows;
@property (readonly) int cols;

- (void) useFont:(NSFont*)font;
- (void) useGridSize:(NSSize)size;

- (NSSize) realViewSize;

- (void) setChar:(NSString*)c x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg;
- (void) setStr:(NSString*)str x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg;

@end
