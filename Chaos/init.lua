print(window:getsize())

window:resized(function()
    window:set("a", 5, 1, "000000", "ffffff")
end)
