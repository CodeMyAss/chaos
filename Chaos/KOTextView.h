#import <Cocoa/Cocoa.h>

typedef void(^KOKeyDownHandler)(BOOL ctrl, BOOL alt, BOOL cmd, NSString* str);

@interface KOTextView : NSView

@property (readonly) CGFloat charWidth;
@property (readonly) CGFloat charHeight;

@property (readonly) int rows;
@property (readonly) int cols;

@property (copy) KOKeyDownHandler keyDownHandler;

- (void) useFont:(NSFont*)font;
- (void) useGridSize:(NSSize)size;

- (NSSize) realViewSize;

- (void) setStr:(NSString*)str x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg;

- (void) clear:(NSColor*)bg;

@end
