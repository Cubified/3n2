## 3n2

A very fast terminal file browser.  Inspired by [nnn](https://github.com/jarun/nnn) (d(n^3)/dn = 3n^2).

Currently still a work in progress.

### Demo

![Demo](https://github.com/Cubified/3n2/blob/main/demo.gif)

### Features

- Very fast -- benchmarks coming soon, but qualitatively it feels extremely snappy to use (on par if not faster than `nnn`)
   - All sorting is done with [quadsort](https://github.com/scandum/quadsort)
- Tiny -- roughly 18kb when stripped
- Full-buffered output -- all screen refreshes are done at once, meaning a choppy, line-by-line redraw is not possible
- Column preview mode -- view the contents of a file or directory in a dedicated column
- Dependency-free -- everything is accomplished with pure ANSI escape codes (no ncurses or similar)
- Truecolor -- supports compiling with different color schemes, all of which take advantage of truecolor (16 million/RGB color) graphics support

### Compiling and Running

`3n2` has no dependencies, meaning it can be compiled and run with:

     $ make
     $ ./3n2

### To-Do

- Viewport (scrolling)
- Hex viewer for binary files
- Caching
- Better search functionality
