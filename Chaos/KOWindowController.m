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

- (void) setChar:(unichar)c x:(int)x y:(int)y fg:(NSColor*)fg bg:(NSColor*)bg {
    NSDictionary* attrs = @{NSForegroundColorAttributeName: fg,
                            NSBackgroundColorAttributeName: bg,
                            NSFontAttributeName: self.font};
    
    NSString* str = [NSString stringWithFormat:@"%C", c];
    
    int p = ([self windowSize].width + 1) * y + x;
    
    if (p >= [self.tv.str length])
        return; // nope.
    
    NSRange r = NSMakeRange(p, 1);
    [self.tv.str setAttributes:attrs range:r];
    [self.tv.str replaceCharactersInRange:r withString:str];
    
    [self.tv setNeedsDisplay:YES];
}

- (void) windowDidResize:(NSNotification *)notification {
    NSSize s = [self windowSize];
    
    [self.tv.str deleteCharactersInRange:NSMakeRange(0, self.tv.str.length)];
    
    NSDictionary* attrs = @{NSFontAttributeName: self.font};
    
    for (int y = 0; y < s.height; y++) {
        for (int x = 0; x < s.width; x++) {
            [[self.tv.str mutableString] appendString:@" "];
        }
        [[self.tv.str mutableString] appendString:@"\n"];
    }
    
    [self.tv.str setAttributes:attrs range:NSMakeRange(0, self.tv.str.length)];
    
    // TODO: this is called a few more times than necessary, so we should call the callback only once per actual size change (or something)
    
    if (self.windowResizedHandler)
        self.windowResizedHandler();
    
    [self.tv setNeedsDisplay:YES];
}

@end
