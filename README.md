# Chaos

A hackable text editor for the 22nd century.

![chaos.png](https://raw.githubusercontent.com/sdegutis/chaos/master/chaos.png)

## How's it different?

### Written in itself

The entire text editor is written in Lua, even the cursor! So you can easily extend *every single aspect* of the text editor, with full control. The only part not written in Lua is the part that draws the characters super fast, which leads me to the next feature:

### Just a grid of monospace characters

It's just a grid of text. That's it. There's no fancy GUI to manipulate. This makes it:

- Fast as lightning
- Super low on memory
- Dead-simple to program for

It won't slow down your older desktop, and it won't fry your new laptop's CPU and GPU.

## Start hacking it

Chaos's API uses [Lua](http://phrogz.net/lua/LearningLua_FromJS.html).

When Chaos launches, it runs `~/.chaos/init.lua`.

Full API docs coming soon.
