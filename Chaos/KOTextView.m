#import "KOTextView.h"

@implementation KOTextView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    [self.str drawInRect:[self bounds]];
}

@end
