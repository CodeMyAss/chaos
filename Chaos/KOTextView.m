#import "KOTextView.h"

@implementation KOTextView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    NSDictionary* attrs = @{NSForegroundColorAttributeName: [NSColor yellowColor],
                            NSBackgroundColorAttributeName: [NSColor blueColor],
                            NSFontAttributeName: [NSFont fontWithName:@"Menlo" size:12]};
    
    NSSize s = [@"x" sizeWithAttributes:attrs];
    s.width = round(s.width);
    s.height = round(s.height);
    
    NSString* str = @"hello world this is a really long string";
    
    NSRect r = [self bounds];
    
    for (int yy = 0; yy < 10; yy++) {
        for (int i = 0; i < [str length]; i++) {
            unichar c = [str characterAtIndex:i];
            NSString* ts = [NSString stringWithFormat:@"%c", c];
            NSAttributedString* astr = [[NSAttributedString alloc] initWithString:ts attributes:attrs];
            
            CGFloat x = i * s.width;
            CGFloat y = (r.size.height - s.height) - (yy * s.height)
            + 3; // TODO: uhh...
            
            [astr drawAtPoint:NSMakePoint(x, y)];
        }
    }
}

@end
