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
- `crates/sagrado-app` — egui application with fully skinned widgets and a
  KDX-style "Preview GUI Items" window with runtime appearance switching.

## Widgets

Phase 1 (this scaffold): push button, default button, tick (checkbox) button,
mutex (radio) button, horizontal & vertical scroll bars (classic arrows,
proportional thumb, hold-to-repeat), and the popup (drop-down) button with a
fully themed menu.

## Running

```sh
cargo run -p sagrado
```

Appearance files are loaded from `themes/Appearances/*.hap` (a few original
KDX appearances are included). Drop additional `.hap` files there and restart
to see them in the appearance picker.
