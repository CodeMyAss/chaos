#import "KOTextView.h"

@implementation KOTextView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    NSDictionary* attrs = @{NSForegroundColorAttributeName: [NSColor blueColor],
                            NSBackgroundColorAttributeName: [NSColor lightGrayColor],
                            NSFontAttributeName: [NSFont fontWithName:@"Menlo" size:12]};
    
    NSString* str = @"\n"
"(sdegutis) [woo] (16:41:46) ~/projects\n"
"$ ls\n"
"billow                     cleancoders_infrastructure myed2\n"
"chaos                      datomic                    myeditor\n"
"chaos2                     dotfiles\n"
"cleancoders.com            hydra\n"
;
    
    NSRect r = [self bounds];
    r.size.width = CGFLOAT_MAX;
    NSAttributedString* astr = [[NSAttributedString alloc] initWithString:str attributes:attrs];
    
//    CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)astr);
    
    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
    
    CGContextSetTextMatrix(context, CGAffineTransformIdentity);
//    CGContextTranslateCTM(context, 0, self.bounds.size.height);
//    CGContextScaleCTM(context, 1.0, -1.0);
    
    // Create a path to render text in
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathAddRect(path, NULL, r );
    
    // An attributed string containing the text to render
    NSAttributedString* attString = astr;
    
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
