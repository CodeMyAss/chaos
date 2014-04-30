local asdf = "┌"

function Utf8to32(utf8str)
   assert(type(utf8str) == "string")
   local res, seq, val = {}, 0, nil
   for i = 1, #utf8str do
      local c = string.byte(utf8str, i)
      if seq == 0 then
         table.insert(res, val)
         seq = c < 0x80 and 1 or c < 0xE0 and 2 or c < 0xF0 and 3 or
            c < 0xF8 and 4 or --c < 0xFC and 5 or c < 0xFE and 6 or
            error("invalid UTF-8 character sequence")
         val = bit32.band(c, 2^(8-seq) - 1)
      else
         val = bit32.bor(bit32.lshift(val, 6), bit32.band(c, 0x3F))
      end
      seq = seq - 1
   end
   table.insert(res, val)
   table.insert(res, 0)
   return res
end

local function redraw()
    local w, h = window:getsize()

    local fg = "000000"
    local bg = "ffffff"

    window:clear(bg)

    window:setw("┌", 0, 0, fg, bg)
    window:setw("┐", w-1, 0, fg, bg)
    window:setw("└", 0, h-1, fg, bg)
    window:setw("┘", w-1, h-1, fg, bg)

    for x = 1, w-2 do
        window:setw("─", x, 0, fg, bg)
        window:setw("─", x, h-1, fg, bg)
    end

    for y = 1, h-2 do
        window:setw("│", 0, y, fg, bg)
        window:setw("│", w-1, y, fg, bg)
    end

    local f = io.lines("Chaos.app/Contents/Resources/init.lua")

    for y = 1, h-2 do
       local line = Utf8to32(f())
       if line then
          for x = 1, #line do
             if x == w - 1 then break end
             local c = line[x]
             if c ~= 13 and c ~= 0 then window:set(c, x, y, fg, bg) end
          end
       end
    end
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

    window:setw(t.key, x, y, fg, bg)
    x = x + 1
    if x == w - 1 then
        x = 1;
        y = y + 1
    end
end)

redraw()

print("ready")
