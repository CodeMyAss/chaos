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

local file = {}
do
    for line in io.lines("Chaos.app/Contents/Resources/init.lua") do
        table.insert(file, line)
    end
end

local function redraw()
    local w, h = window:getsize()

    local fg = "00FF00"
    local bg = "222222"

    window:clear(bg)

    for y = 1, h do
       local line = Utf8to32(file[y])
       if line then
          for x = 1, #line do
             if x-1 == w then break end
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
    local name, size = window:getfont()

    if     t.key == "-" then window:usefont(name, size-1)
    elseif t.key == "=" then window:usefont(name, size+1)
    elseif t.key == "_" then window:resize(w-1, h-1)
    elseif t.key == "+" then window:resize(w+1, h+1)
    else
        local fg = "ffff00"
        local bg = "0000ff"

        window:setw(t.key, x, y, fg, bg)
        x = x + 1
        if x == w+1 then
            x = 1
            y = y + 1
        end
    end
end)

redraw()

print("ready")
