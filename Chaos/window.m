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

// args: [win, fn(t)]
static int win_keydown(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    
    int i = luaL_ref(L, LUA_REGISTRYINDEX);
    
    [wc useKeyDownHandler:^(BOOL ctrl, BOOL alt, BOOL cmd, NSString *str) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, i);
        
        lua_newtable(L);
        lua_pushboolean(L, ctrl);
        lua_setfield(L, -2, "ctrl");
        lua_pushboolean(L, alt);
        lua_setfield(L, -2, "alt");
        lua_pushboolean(L, cmd);
        lua_setfield(L, -2, "cmd");
        lua_pushstring(L, [str UTF8String]);
        lua_setfield(L, -2, "key");
        
        lua_pcall(L, 1, 0, 0);
    }];
    
    return 0;
}

// args: [win]
static int win_getsize(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    lua_pushnumber(L, [wc cols]);
    lua_pushnumber(L, [wc rows]);
    return 2;
}

// args: [win, char, x, y, fg, bg]
static int win_set(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    
    NSString* c = [NSString stringWithUTF8String: lua_tostring(L, 2)];
    int x = lua_tonumber(L, 3);
    int y = lua_tonumber(L, 4);
    NSColor* fg = SDColorFromHex(lua_tostring(L, 5));
    NSColor* bg = SDColorFromHex(lua_tostring(L, 6));
    
    [wc setChar:c x:x y:y fg:fg bg:bg];
    
    return 0;
}

// args: [win, str, x, y, fg, bg]
static int win_setw(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    
    NSString* c = [NSString stringWithUTF8String: lua_tostring(L, 2)];
    int x = lua_tonumber(L, 3);
    int y = lua_tonumber(L, 4);
    NSColor* fg = SDColorFromHex(lua_tostring(L, 5));
    NSColor* bg = SDColorFromHex(lua_tostring(L, 6));
    
    [wc setStr:c x:x y:y fg:fg bg:bg];
    
    return 0;
}

static const luaL_Reg winlib[] = {
    // event handlers
    {"resized", win_resized},
    {"keydown", win_keydown},
    
    // methods
    {"getsize", win_getsize},
    {"set", win_set},
    {"setw", win_setw},
    {NULL, NULL}
};

int luaopen_window(lua_State* L) {
    luaL_newlib(L, winlib);
    return 1;
}
