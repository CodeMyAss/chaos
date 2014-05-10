local window = window.new()

local fg = "00FF00"
local bg = "222222"

window:settitle("Developer Console")

local stdout = ""
local stdin = ""

local function printstr(x, y, str)
    local w, h = window:getsize()

    for i = 1, #str do
       if x == w + 1 then
          x = 1
          y = y + 1
       end

       if y == h + 1 then break end

       local c = str:sub(i,i):byte()
       if c == string.byte("\n") then
          x = 1
          y = y + 1
       else
          window:set(c, x, y, fg, bg)
          x = x + 1
       end
    end
end

local function clearbottom()
    local w, h = window:getsize()

    for x = 1, w do
       window:set(string.byte(" "), x, h, fg, bg)
    end
end

local function redraw()
    local w, h = window:getsize()

    window:clear(bg)
    printstr(1, 1, stdout)
    clearbottom()
    printstr(1, h, "> " .. stdin)
end

window:resized(redraw)

window:keydown(function(t)
    if t.key == "return" then
       local command = stdin
       stdin = ""

       local fn = load(command)
       local success, result = pcall(fn)
       result = tostring(result)
       if not success then result = "error: " .. result end

       stdout = stdout .. "> " .. command .. "\n" .. result .. "\n"
    elseif t.key == "delete" then -- i.e. backspace
       stdin = stdin:sub(0, -2)
    else
       stdin = stdin .. t.key
    end
    redraw()
end)

redraw()

print("ready")
