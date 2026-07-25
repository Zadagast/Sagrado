# Sagrado

A modern, peer-to-peer successor to Haxial KDX, built in Rust + Slint.

The current focus is the **retro theming & layout engine**: a fully owner-drawn
widget kit whose look is defined entirely by data — named colors plus small
nine-slice bitmap "slots" — so one design language can be re-skinned into many
classic looks. No native OS widgets, identical rendering on Linux and Windows.

## Crates

- `sagrado-theme` — UI-agnostic theme format + loader + nine-slice/color renderer.
- `sagrado-ui` — Slint widget library rendered from the runtime theme.
- `sagrado-app` — preview gallery binary (KDX-style "Preview GUI Items").

## Themes

A theme is a folder with a readable `theme.toml` (metadata, named colors,
per-widget slots) and optional PNG images. See `themes/` for examples. Any image
slot may be omitted, in which case the widget is drawn from the color table.

## Running

```sh
cargo run -p sagrado-app     # needs a display (e.g. DISPLAY=:0 on Linux)
```

Build deps on Linux: `pkg-config`, `libfontconfig1-dev`.
