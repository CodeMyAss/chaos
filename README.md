### Chaos

The completely extensible text editor.

*Written in its own extension language.*

#### What is it?

Chaos.app is just a grid of characters, with a Lua API.

Why, you ask? Well, I wanted to write a text editor. I mean, a
super-flexible one. One that anyone can modify *entirely* without
having to recompile anything.

But for the sake of fighting Wirth's law, I figured I'd do a throwback
to 1978. So it's nothing fancy. There's no embedded WebKit. No fancy
animations or bleeding-edge UI. Just a grid of characters. Because
honestly, I'd take flexibility and efficiency over fanciness any day.

So Chaos just makes a grid, and loads init.lua (and eventually also a
user-defined config, probably in `~/.chaos/init.lua`).

This Lua file has access to a `window` instance. You can register your
Lua callbacks for the "key pressed" event (which has full key and
modifier info) and "window resized" event. You can clear the whole
screen with a given background color, and set a given grid cell to a
given char, fg color, and bg color. That's all.

Anyway, this is meant to be a lower-level API that you write a useful
thing on top of. It was basically born out of my frustration with the
ncurses API and the inability to use any color I want in the terminal.

#### TODO

- add copy/paste to API
- add convenient way to print whole string (write it in lua)
- add mouse functionality to API
