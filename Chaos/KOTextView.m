#import "KOTextView.h"

@implementation KOTextView

- (id) initWithFrame:(NSRect)frameRect {
    if (self = [super initWithFrame:frameRect]) {
        self.str = [[NSMutableAttributedString alloc] init];
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
//    [self.str drawInRect:[self bounds]];
//    return;
    
    NSString* line1 = @"joll_";
//    NSString* line1 = @"┌───┐";
    NSString* line2 = @"│   │";
    NSString* line3 = @"│   │";
    NSString* line4 = @"└───┘";
    
    NSDictionary* attrs = @{NSForegroundColorAttributeName: [NSColor blueColor],
                            NSBackgroundColorAttributeName: [NSColor yellowColor],
                            NSFontAttributeName: [NSFont fontWithName:@"Menlo" size:12]};
    
    NSAttributedString* aline1 = [[NSAttributedString alloc] initWithString:line1 attributes:attrs];
    NSAttributedString* aline2 = [[NSAttributedString alloc] initWithString:line2 attributes:attrs];
    NSAttributedString* aline3 = [[NSAttributedString alloc] initWithString:line3 attributes:attrs];
    NSAttributedString* aline4 = [[NSAttributedString alloc] initWithString:line4 attributes:attrs];
    
    NSRect r = [aline1 boundingRectWithSize:NSMakeSize(200, 200) options:0];
    
//    [aline1 drawAtPoint:NSMakePoint(5, [self bounds].size.height - 18)];
//    [aline2 drawAtPoint:NSMakePoint(5, [self bounds].size.height - 36 + 3)];
//    [aline3 drawAtPoint:NSMakePoint(5, [self bounds].size.height - 54 + 6)];
//    [aline4 drawAtPoint:NSMakePoint(5, [self bounds].size.height - 72 + 9)];
    
    
    
    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
    CGContextSetTextMatrix(context, CGAffineTransformIdentity);
    
    CTLineRef line;
    
    line = CTLineCreateWithAttributedString((CFAttributedStringRef)aline1);
    CGContextSetTextPosition(context, 10.0, [self bounds].size.height - 18);
    CTLineDraw(line, context);
    CFRelease(line);
    
    line = CTLineCreateWithAttributedString((CFAttributedStringRef)aline1);
    CGContextSetTextPosition(context, 10.0, [self bounds].size.height - 36 + 3);
    CTLineDraw(line, context);
    CFRelease(line);
    
    return;
    
    
    
    
    // Create a path to render text in
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathAddRect(path, NULL, self.bounds );
    
    // An attributed string containing the text to render
    NSAttributedString* attString = self.str;
    
    // create the framesetter and render text
    CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString((CFAttributedStringRef)attString);
    CTFrameRef frame = CTFramesetterCreateFrame(framesetter,
                                                CFRangeMake(0, [attString length]), path, NULL);
    
    CTFrameDraw(frame, context);
    
    // Clean up
    CFRelease(frame);
    CFRelease(path);
    CFRelease(framesetter);
}

@end
