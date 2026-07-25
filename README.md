# Sagrado

A modern, peer-to-peer successor to Haxial KDX and Hotline for Linux and Windows.

This repository currently contains the first building block: the **Sagrado
Appearance Engine**, a re-implementation of KDX's skinnable theming system.
Every widget is drawn from theme-supplied 9-slice bitmap textures and a table
of named colors — no native toolkit widgets — so appearances render
identically on every platform.

## Crates

- `crates/sagrado-theme` — theme model (colors + 9-slice widget textures) and
  an importer for original Haxial `.hap` appearance files (format
  reverse-engineered; see `src/hap.rs` for the layout).
- `crates/sagrado-app` — egui application with fully skinned widgets and a
  KDX-style "Preview GUI Items" window with runtime appearance switching.

## Running

```sh
cargo run -p sagrado
```

Appearance files are loaded from `themes/Appearances/*.hap` (a few original
KDX appearances are included). Drop additional `.hap` files there and restart
to see them in the appearance picker.
