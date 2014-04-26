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
    [self.str drawInRect:[self bounds]];
}

@end
