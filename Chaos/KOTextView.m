#import "KOTextView.h"
#import <QuartzCore/QuartzCore.h>


@interface KOCell : NSObject
@property NSString* str;
@property NSColor* fg;
@property NSColor* bg;
@end
@implementation KOCell
@end


@interface KOTextView ()

@property (readwrite) CGFloat charWidth;
@property (readwrite) CGFloat charHeight;

@property (readwrite) int rows;
@property (readwrite) int cols;

@property NSMutableArray* grid;
@property NSMutableAttributedString* buffer;

@end

#define SDRectFor(x, y) NSMakeRect(x * self.charWidth, [self bounds].size.height - ((y + 1) * self.charHeight), self.charWidth, self.charHeight)

@implementation KOTextView

- (id) initWithFrame:(NSRect)frameRect {
    if (self = [super initWithFrame:frameRect]) {
        self.grid = [NSMutableArray array];
        
        NSMutableDictionary* defaultAttrs = [NSMutableDictionary dictionary];
        
        NSMutableParagraphStyle* pstyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
        [pstyle setLineBreakMode:NSLineBreakByCharWrapping];
        
        defaultAttrs[NSParagraphStyleAttributeName] = pstyle;
        
        self.buffer = [[NSMutableAttributedString alloc] initWithString:@" " attributes:defaultAttrs];
    }
    return self;
}

- (BOOL) acceptsFirstResponder { return YES; }

- (void) keyDown:(NSEvent *)theEvent {
    BOOL ctrl = ([theEvent modifierFlags] & NSControlKeyMask) != 0;
    BOOL cmd = ([theEvent modifierFlags] & NSCommandKeyMask) != 0;
    BOOL alt = ([theEvent modifierFlags] & NSAlternateKeyMask) != 0;
    
    NSString* str = [theEvent charactersIgnoringModifiers];
    
    if ([str characterAtIndex:0] == 127)
        str = @"delete";
    
    if ([str characterAtIndex:0] == 13)
        str = @"return";
    
//    NSLog(@"%d", [str characterAtIndex:0]);
    
    if (self.keyDownHandler)
        self.keyDownHandler(ctrl, alt, cmd, str);
}

- (void) useFont:(NSFont*)font {
    [self.buffer addAttribute:NSFontAttributeName value:font range:NSMakeRange(0, 1)];
    
    NSRect r = [self.buffer boundingRectWithSize:NSMakeSize(200, 200) options:0];
    self.charWidth = r.size.width;
    self.charHeight = r.size.height;
}

- (NSSize) realViewSize {
    return NSMakeSize(self.charWidth * self.cols, self.charHeight * self.rows);
}

- (void) drawRect:(NSRect)dirtyRect {
    const NSRect* redrawRects;
    NSInteger redrawCount;
    [self getRectsBeingDrawn:&redrawRects count:&redrawCount];
    
    CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    
    for (int y = 0; y < self.rows; y++) {
        for (int x = 0; x < self.cols; x++) {
            for (int r = 0; r < redrawCount; r++) {
                NSRect charRect = SDRectFor(x, y);
                
                if (NSIntersectsRect(charRect, redrawRects[r])) {
                    NSArray* row = [self.grid objectAtIndex:y];
                    KOCell* cell = [row objectAtIndex:x];
                    NSRange range = NSMakeRange(0, 1);
                    
                    [self.buffer replaceCharactersInRange: range withString: cell.str];
                    [self.buffer addAttribute:NSForegroundColorAttributeName value:cell.fg range:range];
                    [self.buffer addAttribute:NSBackgroundColorAttributeName value:cell.bg range:range];
                    
                    CGContextSetFillColorWithColor(ctx, [cell.bg CGColor]);
                    CGContextFillRect(ctx, charRect);
                    
                    CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)self.buffer);
                    CGContextSetTextPosition(ctx, charRect.origin.x, charRect.origin.y);
                    CTLineDraw(line, ctx);
                    CFRelease(line);
                }
            }
        }
    }
}

- (void) useGridSize:(NSSize)size {
    int numMoreCols = size.width  - self.cols;
    int numMoreRows = size.height - self.rows;
    
    self.cols = size.width;
    self.rows = size.height;
    
    if (numMoreCols > 0) {
        for (NSMutableArray* row in self.grid) {
            for (int i = 0; i < numMoreCols; i++) {
                KOCell* c = [[KOCell alloc] init];
                c.fg = [NSColor blackColor];
                c.bg = [NSColor whiteColor];
                c.str = @" ";
                [row addObject:c];
            }
        }
    }
    
    if (numMoreRows > 0) {
        for (int i = 0; i < numMoreRows; i++) {
            NSMutableArray* row = [NSMutableArray array];
            for (int x = 0; x < self.cols; x++) {
                KOCell* c = [[KOCell alloc] init];
                c.fg = [NSColor blackColor];
                c.bg = [NSColor whiteColor];
                c.str = @" ";
                [row addObject:c];
            }
            [self.grid addObject:row];
        }
    }
}

- (BOOL) preservesContentDuringLiveResize { return YES; }

- (void) setStr:(NSString*)str x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg {
    NSArray* row = [self.grid objectAtIndex:y];
    KOCell* cell = [row objectAtIndex:x];
    
    cell.str = str;
    if (fg) cell.fg = fg;
    if (bg) cell.bg = bg;
    
    [self setNeedsDisplayInRect: SDRectFor(x, y)];
}

- (void) clear:(NSColor*)bg {
    for (NSArray* row in self.grid) {
        for (KOCell* cell in row) {
            cell.str = @" ";
            cell.bg = bg;
        }
    }
    
    [self setNeedsDisplay:YES];
}

- (void) setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    
    if ([self inLiveResize]) {
        NSRect rects[4];
        NSInteger count;
        [self getRectsExposedDuringLiveResize:rects count:&count];
        while (count --> 0) {
            [self setNeedsDisplayInRect:rects[count]];
        }
    }
    else {
        [self setNeedsDisplay:YES];
    }
}

@end
