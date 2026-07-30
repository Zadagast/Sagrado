# Surface → `.hap` contract

**Process:** before creating a new widget, read
[`hap-first.md`](hap-first.md) — probe real `.hap` colours / Images / Icons
and write the shopping list here. Do not invent a parallel look.

When you add a KDX window (or kit control), pick colours from these groups —
do **not** hardcode Standard reds/greys. Resolve through the helpers in
`chrome.h` / `controls.h`, with `kit_theme()` (or `settings::active_theme()`)
as the theme source.

Full index names live in [`hap-color-table.md`](hap-color-table.md) and
`enum HapColor` in `native/src/hap.h`.

## Shared helpers

| Helper | HapColor group | Use for |
|---|---|---|
| `frame_palette` / `paint_chrome` | Window 54–78, Window Focus 79–103 | Ooze Gel frame, title gradient, traffic-light glyphs |
| Title labels | **Primary Label (5)** / Disable (7) | Window titles (art themes leave Window Label white) |
| `dialog_colors` / `draw_button` | Primary, TextBox, Focus, Button 29–35, Default Button 50–53 (+ **Focus Box** when Default is still Standard red); Push Button slots 25–26 when present | Dialog fills, fields, push buttons |
| `button_hilite_colors` | Button Hilite 36–42 | Open/pressed command rows |
| `ui_colors` + `frame_palette` | Menu 104–112; Window Focus ring (or **Focus Box** when Window Focus is still Standard red) | Popup menus / submenus |
| `list_colors` | List 15–20, Focus Box 9, Primary Frame 4 | Tracker/user lists, picker hilite |
| `header_colors` | Column Header 174–182 (+ Primary Label) | Column headers, active chat tabs |
| `scroll_colors` | ScrollBar 128–147 | Scroll tracks, arrows, thumbs |
| Primary Light/Dark/Frame | 1 / 3 / 4 | Sunken well bevels |
| `Theme::image` / `icon` | Images + Icons sections | Widget bitmaps (below) |

Process-wide: set `kit_theme_fn` at boot (`settings::active_theme` in KDX) so
kit surfaces that cannot take a `Theme*` (popup menus) still resolve colours.

## Widget art slots (Images section)

Prefer these bitmaps when present; fall back to the colour helpers above.

| Slot enum | Index | Use |
|---|---|---|
| `SlotColumnHeaderNormal` / `Hilited` | 150 / 151 | Tracker column headers; active chat / TextEdit tabs |
| `SlotHScrollDoubleArrows` | 162 | Horizontal scrollbar body (arrow pairs in caps) |
| `SlotHScrollIndicatorNormal` | 166 | H-scroll thumb |
| `SlotHScrollGripsNormal` | 169 | H-scroll thumb grips overlay |
| `SlotVScrollDoubleArrows` | 181 | Vertical scrollbar body |
| `SlotVScrollIndicatorNormal` | 185 | V-scroll thumb |
| `SlotVScrollGripsNormal` | 188 | V-scroll thumb grips overlay |
| `SlotPushButtonNormal` / `Hilited` | 25 / 26 | Push buttons |
| `SlotWindow*` | 223+ | Ooze Gel traffic lights / menu / resize |

Scroll bar art: 9-slice the DoubleArrows image into the bar rect; travel
bounds come from `positions` (fallback: `caps`). Overlay Indicator on the
thumb, then Grips centered when the thumb is tall/wide enough.

## Icons section

Loaded from `.hap` section 3 (`0x44` / `0x48`) into `Theme::icons`. Same
image-record format as the Images section. Indices are sparse; common
pairs are 16×16 at N and ~32×32 at N+1 (`HapIcon` in `hap.h`):

| HapIcon | Index | Typical use |
|---|---|---|
| `IconFileGeneric16/32` | 4 / 5 | Generic file mark |
| `IconUser16/32` | 44 / 45 | Settings Identity well; chat user list |
| `IconFolder16/32` | 64 / 65 | Folders (File Browser later) |

Helpers: `icon16(slot)`, `icon32(slot)`. Themes without an Icons section
keep program-art fallbacks (`kdx_art.h`).

## Per-window shopping list

### Any framed window
- Window / Window Focus + Transitions
- Primary Label for the title string
- Title-bar button slots when the theme supplies art (`SlotWindow*`)

### Dialogs (Settings, Host a Server, Find…)
- Primary Background + Label
- Text Box (bg/fg/hilite/caret) + Focus Box
- Button + Default Button
- Optional: List hilite for dropdown pickers

### Popup menus (Commands, category menus)
- Outer ring: Window Focus frame palette
- Panel: Menu Light / Background / Dark / Label
- Hot row: Menu Hilite Light / Background / Dark / Label
- Disabled: Menu Disable Label
- Optional art: `SlotWindowMenuNormal` / `SlotWindowMenuFocus`

### Tracker
- Client fill: Primary Background
- Pane ring: Focus Box (active) / Primary Frame (idle)
- Headers: `SlotColumnHeader*` art, else Column Header colours + Primary Label
- Body: List Background / Label / Hilite / Sort Column / Separator
- Scrollbars: `SlotVScroll*` / `SlotHScroll*` art, else ScrollBar colours

### Chat
- Client fill: Primary Background
- Chat log well: Text Box Background + Primary bevels
- User list well: List Background + List Separator between rows
- User marks: `IconUser16` from Icons section when present
- Entry: Text Box + Focus Box + Insertion Point
- Room tab: `SlotColumnHeaderHilited` art, else Column Header Hilite*
- Tool buttons: Button group
- Scrollbars: `SlotVScroll*` art, else ScrollBar colours

### Settings (Identity)
- Icon well: `IconUser32` (Icons section) centered in the 48×48 well; colour
  placeholder when the theme has no Icons section

### Launcher (main window)
- Chrome + Primary/Window Focus Label for the butterfly + “KDX”
- Command rows: Button group
- Open Commands row: Button Hilite group
- Footer counters: Primary Label
- Commands popup: Menu group (above)
- Minimize → **KDX Dock** (not OS iconify)

### KDX Dock
- Ordinary framed window (`"KDX Dock"`); no dedicated `.hap` Dock slots
- Frame / title: Window Focus + Primary Label
- Client: Primary Background
- Item buttons: Button group (+ program-art icons per window)
- Close restores every docked window
- Minimize / title double-click on other windows hide into this Dock

## Authoring note

Bitmap-only themes often leave Window / Column Header **label** entries at
default white. Always prefer **Primary Label** for readable titles and header
text, matching real KDX / AppearanceEdit authoring practice.
