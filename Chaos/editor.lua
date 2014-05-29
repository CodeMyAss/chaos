local win = window.new()
win:settitle("Untitled")

local fg = "00FF00"
local bg = "222222"

local contents = ""

local function printdoc()
   local w, h = win:getsize()

   local x = 1
   local y = 1

   for i = 1, #contents do
      if x == w + 1 then
         x = 1
         y = y + 1
      end

      if y == h + 1 then break end

      local c = contents:sub(i,i):byte()
      if c == string.byte("\n") then
         x = 1
         y = y + 1
      else
         print(c, x, y, fg, bg)
         win:set(c, x, y, fg, bg)
         x = x + 1
      end
   end

   -- draw cursor
   win:set(string.byte(" "), x, y, bg, fg)
end

local function redraw()
   win:clear(bg)
   printdoc()
end

win:resized(redraw)

win:keydown(function(t)
               if t.key == "return" then
                  contents = contents .. "\n"
               elseif t.key == "delete" then -- i.e. backspace
                  contents = contents:sub(0, -2)
               else
                  contents = contents .. t.key
               end
               redraw()
            end)

redraw()
