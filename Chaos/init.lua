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
    
    window:setw("hello", 1, 1, fg, bg)
end

window:resized(function()
    redraw()
end)

redraw()
