# Hap-first: look before you build

**Rule:** before inventing a new widget, window, or control look, open real
`.hap` files (and AppearanceEdit when useful) and write down what the theme
already supplies. Do not invent a parallel palette, bitmap set, or layout
grammar.

Colour names: [`hap-color-table.md`](hap-color-table.md).  
Surface shopping lists: [`hap-surfaces.md`](hap-surfaces.md).  
Enums / loaders: `native/src/hap.h`, helpers in `chrome.h` / `controls.h`.

Themes live in `themes/Appearances/*.hap`. Roughly a third ship Images art;
about a quarter ship an Icons section. Colour tables are almost always
present — art is optional, colours are not.

## Checklist (every new widget)

1. **Name the KDX control** you are cloning (list, scrollbar, button, tab,
   progress, slider, file row, …).
2. **Read the colour groups** for that control in AppearanceEdit / the
   colour table. Prefer an existing helper (`list_colors`, `scroll_colors`,
   `dialog_colors`, …) over new hardcoded RGB.
3. **Probe Images slots** for that control across a few art-heavy themes
   (e.g. Ashen, Function, Mjolnir, Terminal-*). Note which indices exist,
   sizes, and whether `caps` / `positions` encode travel or 9-slice bounds.
4. **Probe Icons** if the control shows file/user/folder marks. Indices are
   sparse; confirm 16×16 vs ~32×32 pairs before wiring.
5. **Fallbacks are required.** Many themes are colour-only. Paint path:
   art when present → colour plates from HapColor → last-resort Standard.
6. **Document the contract** in `hap-surfaces.md` (and add `Slot` / `HapIcon`
   / helper names in code) in the same change that adds the widget.
7. **Never invent a new colour role** that AppearanceEdit does not expose.
   If KDX had no swatch for it, reuse Primary / Focus Box / Button / List.

## What “looking at a .hap” means

| Section | Header | Ask |
|---|---|---|
| Colors | `0x3c` / `0x40` | Which HapColor indices does this control use? Still Standard red? |
| Images | `0x34` / `0x38` | Which slots, sizes, caps, positions? Normal vs Hilite vs Focus? |
| Icons | `0x44` / `0x48` | Which sparse indices? 16 vs 32? Enough coverage to depend on? |

Quick probe pattern (Python or a tiny Wine tool): load several `.hap`s,
count how many define the candidate slots, dump sizes. If coverage is thin
(e.g. scroll grips ~ half of art themes), keep a strong colour fallback.

## Known traps

- **Art themes leave labels white** in Window / Column Header groups. Title
  and header ink → **Primary Label (5)** / Disable (7).
- **Default Button / Window Focus** often stay Standard stock-red in art
  themes. Rings and default outlines → **Focus Box (9)** when those groups
  look untouched.
- **`SlotWindowMenu*`** is the title-bar Window Menu glyph, not the popup
  menu frame. Popup chrome is Menu colours (+ Focus Box / Window Focus ring).
- **Icons ≠ Images.** File/user marks are section 3; widget chrome is
  section 1. Do not overload one table for the other.
- **No Dock / no File Browser exclusives** in `.hap`. Those windows still
  compose from shared Window / Button / List / Icon pieces.

## Before File Browser (and the next big surface)

Map first, code second:

- List + Column Header colours/art
- ScrollBar colours/art (V + H)
- Icons: folder, generic file, and any type marks you can identify
- File Label 0–15 (`ColFileLabel0`…) for tinted name rows
- Push Button / Text Box / Focus Box for tool strip and path field

Update this note when a probe invents a new durable slot name or kills a
wrong assumption.
