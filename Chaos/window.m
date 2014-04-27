#import "lua/lauxlib.h"
#import "KOWindowController.h"

static NSColor* SDColorFromHex(const char* hex) {
    NSScanner* scanner = [NSScanner scannerWithString: [NSString stringWithUTF8String: hex]];
    unsigned colorCode = 0;
    [scanner scanHexInt:&colorCode];
    return [NSColor colorWithCalibratedRed: (colorCode >> 16) & 0xFF / 0xFF
                                     green: (colorCode >> 8) & 0xFF / 0xFF
                                      blue: (colorCode) & 0xFF / 0xFF
                                     alpha: 1.0];
}

// args: [win, fn]
static int win_resized(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    
    int i = luaL_ref(L, LUA_REGISTRYINDEX);
    
    wc.windowResizedHandler = ^{
        lua_rawgeti(L, LUA_REGISTRYINDEX, i);
        lua_pcall(L, 0, 0, 0);
    };
    
    return 0;
}

// args: [win]
static int win_getsize(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    NSSize s = [wc windowSize];
    lua_pushnumber(L, s.width);
    lua_pushnumber(L, s.height);
    return 2;
}

// args: [win, char, x, y, fg, bg]
static int win_set(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    
    NSString* str = [NSString stringWithUTF8String: lua_tostring(L, 2)];
    unichar c = [str characterAtIndex:0];
    
    int x = lua_tonumber(L, 3);
    int y = lua_tonumber(L, 4);
    
    NSColor* fg = SDColorFromHex(lua_tostring(L, 5));
    NSColor* bg = SDColorFromHex(lua_tostring(L, 6));
    
    [wc setChar:c x:x y:y fg:fg bg:bg];
    
    return 0;
}

static const luaL_Reg winlib[] = {
    {"getsize", win_getsize},
    {"resized", win_resized},
    {"set", win_set},
    {NULL, NULL}
};

int luaopen_window(lua_State* L) {
    luaL_newlib(L, winlib);
    return 1;
}
