#import "KOWindowController.h"

@interface KOWindowController ()

@property IBOutlet KOTextView *tv;
@property NSFont* font;
@property NSSize realCharSize;
@property NSSize integerCharSize;

@end

@implementation KOWindowController

- (NSString*) windowNibName {
    return @"Window";
}

- (void)windowDidLoad {
    [super windowDidLoad];
    
    [self useFont:[NSFont fontWithName:@"Menlo" size:12]];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowDidResize:)
                                                 name:NSWindowDidResizeNotification
                                               object:self.window];
    
//    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
//        [self setChar:'_' x:1 y:1 fg:[NSColor yellowColor] bg:[NSColor blueColor]];
//        [self setChar:'_' x:0 y:0 fg:[NSColor yellowColor] bg:[NSColor blueColor]];
//        [self setChar:'_' x:1 y:0 fg:[NSColor yellowColor] bg:[NSColor blueColor]];
//    });
    
    [self.window makeKeyAndOrderFront:nil];
    [self resizeWindowTo:NSMakeSize(50, 10)];
}


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

- (void) setChar:(char)c x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg {
    NSDictionary* attrs = @{NSForegroundColorAttributeName: fg,
                            NSBackgroundColorAttributeName: bg,
                            NSFontAttributeName: self.font};
    
    NSString* str = [NSString stringWithFormat:@"%c", c];
    
    int p = ([self windowSize].width + 1) * y + x;
    NSLog(@"%f %d %d %d", [self windowSize].width, y, x, p);
    NSRange r = NSMakeRange(p, 1);
    
    [self.tv.str setAttributes:attrs range:r];
    [self.tv.str replaceCharactersInRange:r withString:str];
    
    [self.tv setNeedsDisplay:YES];
}

- (void) windowDidResize:(NSNotification *)notification {
    NSSize s = [self windowSize];
    
    [self.tv.str deleteCharactersInRange:NSMakeRange(0, self.tv.str.length)];
    
    NSDictionary* attrs = @{NSFontAttributeName: self.font};
    
    NSString* ms = [@"" stringByPaddingToLength:(s.width + 1) * s.height
                                     withString:@" " startingAtIndex:0];
    
    NSAttributedString* astr = [[NSAttributedString alloc] initWithString:ms
                                                               attributes:attrs];
    
    [self.tv.str appendAttributedString:astr];
    
    // TODO: this is called a few more times than necessary, so we should call the callback only once per actual size change (or something)
    
    if (self.windowResizedHandler)
        self.windowResizedHandler();
    
    [self.tv setNeedsDisplay:YES];
}

@end
