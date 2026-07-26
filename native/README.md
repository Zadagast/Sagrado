# Sagrado Native

A ground-up native Win32 rewrite of Sagrado, built the way Haxial built KDX:
the entire UI is rendered into a software framebuffer and blitted to the
window with a single GDI call (`SetDIBitsToDevice`). No native widgets, no
frameworks — every pixel is ours.

Runs natively on Windows and under Wine on Linux/macOS.

## Build (cross-compile from Linux)

Requires MinGW-w64 (`apt install g++-mingw-w64-i686`):

```
cd native
make        # produces build/SagradoTextEdit.exe (32-bit, statically linked)
make run    # runs it under Wine
```

On Windows, build with MinGW-w64 or open the sources in any C++17 compiler
and link `gdi32` + `user32`.

## Layout

- `src/canvas.h` — the software framebuffer: fills, lines, gradients, and the
  built-in KDX pixel font (rasterized from Pixel Operator Bold, CC0).
- `src/chrome.h` — the KDX Standard window chrome, drawn from pixel values
  measured off the real Haxial TextEdit (frame slab, title gradient,
  title-bar boxes, grow box, raised bars).
- `src/main.cpp` — Win32 shell: borderless window with native move/resize
  (`WM_NCCALCSIZE`/`WM_NCHITTEST`), message loop, blit.
