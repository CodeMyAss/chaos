#import "KOAppDelegate.h"
#import "KOWindowController.h"
#import "lua/lauxlib.h"
#import "lua/lualib.h"

int luaopen_window(lua_State* L);

@interface KOAppDelegate ()
@property lua_State* L;
@end

@implementation KOAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSString* resourcePath = [[NSBundle mainBundle] resourcePath];
    const char* core_dir = [[resourcePath stringByAppendingPathComponent:@"?.lua"] fileSystemRepresentation];
    const char* user_dir = [[@"~/.hydra/?.lua" stringByStandardizingPath] fileSystemRepresentation];
    
    lua_State* L = luaL_newstate();
    self.L = L;
    luaL_openlibs(L);
    
    luaopen_window(L);               // [window]
    lua_setglobal(L, "window");      // []
    
    lua_getglobal(L, "package");     // [package]
    lua_getfield(L, -1, "path");     // [package, path]
    lua_pushliteral(L, ";");         // [package, path, ";"]
    lua_pushstring(L, core_dir);     // [package, path, ";", coredir]
    lua_pushliteral(L, ";");         // [package, path, ";", coredir, ";"]
    lua_pushstring(L, user_dir);     // [package, path, ";", coredir, ";", userdir]
    lua_concat(L, 5);                // [package, newpath]
    lua_setfield(L, -2, "path");     // [package]
    
    lua_pop(L, 1);                   // []
    
    luaL_dostring(L, "require('init')");
}

@end
