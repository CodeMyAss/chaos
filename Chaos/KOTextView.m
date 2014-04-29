#import "KOTextView.h"
#import <QuartzCore/QuartzCore.h>


@interface KOTextView ()

@property (readwrite) CGFloat charWidth;
@property (readwrite) CGFloat charHeight;

@property (readwrite) int rows;
@property (readwrite) int cols;

@property NSMutableAttributedString* buffer;

@property NSMutableDictionary* defaultAttrs;

@end

@implementation KOTextView

- (id) initWithFrame:(NSRect)frameRect {
    if (self = [super initWithFrame:frameRect]) {
        self.buffer = [[NSMutableAttributedString alloc] init];
        self.defaultAttrs = [NSMutableDictionary dictionary];
        
        NSMutableParagraphStyle* pstyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
        [pstyle setLineBreakMode:NSLineBreakByCharWrapping];
        
        self.defaultAttrs[NSParagraphStyleAttributeName] = pstyle;
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
    self.defaultAttrs[NSFontAttributeName] = font;
    
    NSAttributedString* as = [[NSAttributedString alloc] initWithString:@"x" attributes:self.defaultAttrs];
    CTFramesetterRef frameSetter = CTFramesetterCreateWithAttributedString((__bridge CFAttributedStringRef)as);
    CGSize suggestedSize = CTFramesetterSuggestFrameSizeWithConstraints(frameSetter, CFRangeMake(0, 1), NULL, CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX), NULL);
    CFRelease(frameSetter);
    
    self.charWidth = suggestedSize.width;
    self.charHeight = suggestedSize.height;
}

- (NSSize) realViewSize {
    return NSMakeSize(self.charWidth * self.cols,
                      self.charHeight * self.rows);
}

- (void) drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    
    NSRect bounds = [self bounds];
    bounds.size = [self realViewSize];
    
    CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString((__bridge CFAttributedStringRef)self.buffer);
    
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathAddRect(path, NULL, bounds);
    
    CTFrameRef textFrame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0,0), path, NULL);
    
    // we have to manually draw backgrounds :/
    
    [self.buffer enumerateAttribute:NSBackgroundColorAttributeName
                            inRange:NSMakeRange(0, [self.buffer length])
                            options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                         usingBlock:^(NSColor* color, NSRange range, BOOL *stop) {
                             if (color) {
                                 for (NSUInteger i = range.location; i < NSMaxRange(range); i++) {
                                     NSRect bgRect;
                                     bgRect.origin.x = (i % self.cols) * self.charWidth;
                                     bgRect.origin.y = NSMaxY(bounds) - (((i / self.cols) + 1) * self.charHeight);
                                     bgRect.size.width = self.charWidth;
                                     bgRect.size.height = self.charHeight;
                                     
                                     bgRect = NSIntegralRect(bgRect);
                                     
                                     [color setFill];
                                     [NSBezierPath fillRect:bgRect];
                                 }
                             }
                         }];
    
    // okay, now draw the actual text (just one line! ha!)
    
    CTFrameDraw(textFrame, ctx);
    
    CFRelease(framesetter);
    CFRelease(textFrame);
    CGPathRelease(path);
}

- (void) useGridSize:(NSSize)size {
    self.cols = size.width;
    self.rows = size.height;
    
    // whenever we resize the grid, (if change > 0) just pad to length with " " and re-font the newly added text
    
    NSUInteger newBufferLen = self.cols * self.rows;
    NSUInteger oldLen = [self.buffer length];
    NSInteger diff = newBufferLen - oldLen;
    
    if (diff > 0) {
        NSString* padding = [@"" stringByPaddingToLength:diff withString:@" " startingAtIndex:0];
        [[self.buffer mutableString] appendString:padding];
        [self.buffer setAttributes:self.defaultAttrs range:NSMakeRange(oldLen, diff)];
    }
}

- (void) setStr:(NSString*)str x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg {
    NSUInteger i = x + y * self.cols;
    NSRange r = NSMakeRange(i, [str length]);
    [self.buffer replaceCharactersInRange:r withString:str];
    if (fg) [self.buffer addAttribute:NSForegroundColorAttributeName value:fg range:r];
    if (bg) [self.buffer addAttribute:NSBackgroundColorAttributeName value:bg range:r];
    [self setNeedsDisplay:YES];
}

@end
