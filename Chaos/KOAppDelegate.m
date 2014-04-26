#import "KOAppDelegate.h"

@interface KOAppDelegate ()

@property NSFont* font;
@property NSSize realCharSize;
@property NSSize integerCharSize;

@end

@implementation KOAppDelegate

- (void) useFont:(NSFont*)font {
    NSSize oldSize = [self windowSize];
    
    self.font = font;
    
    NSSize s = [@"x" sizeWithAttributes:@{NSFontAttributeName: self.font}];
    self.realCharSize = s;
    s.width = ceil(s.width);
    s.height = ceil(s.height);
    self.integerCharSize = s;
    
    [self.window setResizeIncrements:self.integerCharSize];
    
    [self resizeWindowTo:oldSize];
}

- (void) resizeWindowTo:(NSSize)size {
    if (![self.window isVisible])
        return;
    
    NSRect f = [self.window frame];
    NSRect oldF = f;
    f.size.width = self.integerCharSize.width * size.width;
    f.size.height = self.integerCharSize.height * size.height - 14; // uhh
    f.origin.y += NSMaxY(oldF) - NSMaxY(f);
    [self.window setFrame:f display:YES];
}

- (NSSize) windowSize {
    NSSize s = [[self.window contentView] frame].size;
    
    // we do floor() so that we dont have a char only half on screen
    CGFloat w = floor(s.width / self.realCharSize.width);
    CGFloat h = floor(s.height / self.realCharSize.height);
    
    return NSMakeSize(w, h);
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [self useFont:[NSFont fontWithName:@"Menlo" size:12]];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowDidResize:)
                                                 name:NSWindowDidResizeNotification
                                               object:self.window];
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self setChar:'_' x:1 y:1 fg:[NSColor yellowColor] bg:[NSColor blueColor]];
        [self setChar:'_' x:0 y:0 fg:[NSColor yellowColor] bg:[NSColor blueColor]];
        [self setChar:'_' x:1 y:0 fg:[NSColor yellowColor] bg:[NSColor blueColor]];
    });
    
    [self.window makeKeyAndOrderFront:nil];
    [self resizeWindowTo:NSMakeSize(50, 10)];
}

- (void) setChar:(char)c x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg {
    NSDictionary* attrs = @{NSForegroundColorAttributeName: fg,
                            NSBackgroundColorAttributeName: bg,
                            NSFontAttributeName: self.font};
    
    NSString* str = [NSString stringWithFormat:@"%c", c];
    
    NSSize s = [[self.window contentView] frame].size;
    
    CGFloat w = floor(s.width / self.realCharSize.width);
//    CGFloat h = floor(s.height / self.charsize.height);
    
    int p = (w + 1) * y + x;
    
//    [self.tv.str replaceCharactersInRange:NSMakeRange(p, 1) withAttributedString:as];
    [self.tv.str setAttributes:attrs range:NSMakeRange(p, 1)];
    [self.tv.str replaceCharactersInRange:NSMakeRange(p, 1) withString:str];
    
    [self.tv setNeedsDisplay:YES];
}

- (void) windowDidResize:(NSNotification *)notification {
    NSSize s = [self windowSize];
    
    [self.tv.str deleteCharactersInRange:NSMakeRange(0, self.tv.str.length)];
    
    NSDictionary* attrs = @{NSForegroundColorAttributeName: [NSColor whiteColor],
                            NSBackgroundColorAttributeName: [NSColor blackColor],
                            NSFontAttributeName: self.font};
    
    int c = 'a';
    
    NSMutableString* ms = [NSMutableString string];
    for (int y = 0; y < s.height; y++) {
        for (int x = 0; x < s.width; x++) {
            [ms appendFormat:@"%c", c++];
            if (c == 'z' + 1) c = 'a';
        }
        [ms appendString:@"\n"];
    }
    
    NSAttributedString* astr = [[NSAttributedString alloc] initWithString:ms
                                                               attributes:attrs];
    
    [self.tv.str appendAttributedString:astr];
    
    [self.tv setNeedsDisplay:YES];
}

@end
