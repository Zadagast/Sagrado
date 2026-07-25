# Sagrado

A modern clone of Haxial KDX.

The first building block is the **Sagrado Appearance Engine**: a
re-implementation of KDX's skinnable theming system. Every widget is drawn
from theme-supplied 9-slice bitmap textures and a table of named colors — no
native toolkit widgets — so appearances render identically on every platform.
Original Haxial `.hap` appearance files load natively.

## Crates

- `crates/sagrado-theme` — theme model (204-color table + 9-slice widget
  textures keyed by slot index) and a native importer for Haxial `.hap`
  appearance files (format reverse-engineered; layout documented in
  `src/hap.rs`).
- `crates/sagrado-ui` — the reusable toolkit: skinned widgets, the 9-slice
  painter, custom window chrome, and the menu bar / document tab bar. Shared by
  every Sagrado application.
- `crates/sagrado-app` — egui application with fully skinned widgets and a
  KDX-style "Preview GUI Items" window with runtime appearance switching.
- `crates/sagrado-textedit` — **Sagrado TextEdit**, a fast, skinnable plain-text
  editor modeled on Haxial TextEdit: custom KDX chrome, a File/Tools/Favorites/
  Location/Appearance menu bar, document tabs, Find/Replace, line sorting,
  occurrence counting, and soft-wrap.

## Widgets

Phase 1 (this scaffold): push button, default button, tick (checkbox) button,
mutex (radio) button, horizontal & vertical scroll bars (classic arrows,
proportional thumb, hold-to-repeat), and the popup (drop-down) button with a
fully themed menu.

## Running

```sh
cargo run -p sagrado             # widget/theme preview
cargo run -p sagrado-textedit    # Sagrado TextEdit
```

Appearance files are loaded from `themes/Appearances/*.hap` (a few original
KDX appearances are included). Drop additional `.hap` files there and restart
to see them in the appearance picker.
