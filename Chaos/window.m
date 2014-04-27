#import "lua/lauxlib.h"
#import "KOWindowController.h"

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

static int win_getsize(lua_State *L) {
    void** ud = lua_touserdata(L, 1);
    KOWindowController* wc = (__bridge KOWindowController*)*ud;
    NSSize s = [wc windowSize];
    lua_pushnumber(L, s.width);
    lua_pushnumber(L, s.height);
    return 2;
}

static const luaL_Reg winlib[] = {
    {"getsize", win_getsize},
    {"resized", win_resized},
    {NULL, NULL}
};

int luaopen_window(lua_State* L) {
    luaL_newlib(L, winlib);
    return 1;
}
