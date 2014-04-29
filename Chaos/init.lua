local function redraw()
    local w, h = window:getsize()

    local fg = "000000"
    local bg = "ffffff"

    window:set("┌", 0, 0, fg, bg)
    window:set("┐", w-1, 0, fg, bg)
    window:set("└", 0, h-1, fg, bg)
    window:set("┘", w-1, h-1, fg, bg)
    
    for x = 1, w-2 do
        window:set("─", x, 0, fg, bg)
        window:set("─", x, h-1, fg, bg)
    end

    for y = 1, h-2 do
        window:set("│", 0, y, fg, bg)
        window:set("│", w-1, y, fg, bg)
    end
    
    for y = 1, h-2 do
        for x = 1, w-2 do
            window:set(" ", x, y, fg, bg)
        end
    end
    
    window:setw("hello world!", 1, 1, fg, bg)
end

local x = 1
local y = 2

window:resized(function()
    x = 1
    y = 2
    redraw()
end)

window:keydown(function(t)
    local w, h = window:getsize()

    local fg = "ffff00"
    local bg = "0000ff"

    window:set(t.key, x, y, fg, bg)
    x = x + 1
    if x == w - 1 then
        x = 1;
        y = y + 1
    end
end)

redraw()

print("ready")
