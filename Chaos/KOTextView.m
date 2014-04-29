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
    
    if (self.keyDownHandler)
        self.keyDownHandler(ctrl, alt, cmd, [theEvent charactersIgnoringModifiers]);
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
    
    CFArrayRef lines = CTFrameGetLines(textFrame);
    
    CGPoint origins[CFArrayGetCount(lines)];//the origins of each line at the baseline
    CTFrameGetLineOrigins(textFrame, CFRangeMake(0, 0), origins);
    
    for (int lineIndex = 0; lineIndex < CFArrayGetCount(lines); lineIndex++) {
        CTLineRef line = CFArrayGetValueAtIndex(lines, lineIndex);
        CFArrayRef runs = CTLineGetGlyphRuns(line);
        
        for (int runIndex = 0; runIndex < CFArrayGetCount(runs); runIndex++) {
            CTRunRef run = CFArrayGetValueAtIndex(runs, runIndex);
            
            NSDictionary* attrs = (__bridge NSDictionary*)CTRunGetAttributes(run);
            NSColor* bgColor = [attrs objectForKey:NSBackgroundColorAttributeName];
            
            if (bgColor) {
                CGRect runBounds;
                
                CGFloat ascent;
                CGFloat descent;
                runBounds.size.width = CTRunGetTypographicBounds(run, CFRangeMake(0, 0), &ascent, &descent, NULL);
                runBounds.size.height = ascent + descent;
                
                CGFloat xOffset = CTLineGetOffsetForStringIndex(line, CTRunGetStringRange(run).location, NULL);
                
                runBounds.origin.x = origins[lineIndex].x + bounds.origin.x + xOffset;
                runBounds.origin.y = origins[lineIndex].y + bounds.origin.y;
                runBounds.origin.y -= descent;
                runBounds = NSIntegralRect(runBounds);
                
                CGContextSetFillColorWithColor(ctx, [bgColor CGColor]);
                CGContextFillRect(ctx, runBounds);
            }
        }
    }
    
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

- (void) setChar:(NSString*)c x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg {
    [self setStr:[c substringToIndex:1] x:x y:y fg:fg bg:bg];
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
