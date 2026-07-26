# The .hap 204-entry color table — complete semantic map

Every Haxial Appearance (.hap) file carries a fixed table of 204 colors
(section 2 of the file, 204 × big-endian u32 `0x00RRGGBB`).

## How this map was verified

The mapping below is not guessed. It was read back from Haxial's own
AppearanceEdit (1.24 and 1.4) running under Wine:

1. A probe .hap was crafted from a real theme with every color entry
   overwritten as `color[i] = i·0x010101` (i.e. grey level = file index).
2. The probe was opened in AppearanceEdit and its **Colors** panel scrolled
   end to end; every named swatch's pixel value was read programmatically
   from screenshots.
3. Each swatch's grey level therefore reveals exactly which file index that
   named color occupies — a direct name→index readout from the authoritative
   editor, cross-checked against the ordered name strings embedded in the
   AppearanceEdit executable and the archived AppearanceEdit documentation.

Group header rows (`◆ … Group ◆`) display the group's base entry and are not
separate table entries.

## The table

| Index | Name | Notes |
|---|---|---|
| 0 | (reserved) | never shown by AppearanceEdit |
| 1 | Primary Light | bevel highlight |
| 2 | Primary Background | panel/dialog fill |
| 3 | Primary Dark | bevel shadow |
| 4 | Primary Frame | |
| 5 | Primary Label | |
| 6 | Primary Disable Frame | |
| 7 | Primary Disable Label | |
| 8 | Important Label | |
| 9 | Focus Box | |
| 10 | Text Box Background | editor background |
| 11 | Text Box Foreground | editor text |
| 12 | Text Hilite Background | selection |
| 13 | Text Hilite Foreground | |
| 14 | Text Insertion Point | caret |
| 15 | List Background | |
| 16 | List Label | |
| 17 | List Hilite Background | |
| 18 | List Hilite Foreground | |
| 19 | List Sort Column Background | |
| 20 | List Separator | |
| 21–24 | Workspace Background 1–4 | |
| 25–28 | (reserved) | not exposed by any AppearanceEdit version |
| 29–35 | Button: Light 2, Light 1, Button, Dark 1, Dark 2, Frame, Label | |
| 36–42 | Button Hilite (same 7-slot layout) | |
| 43–49 | Button Disable (same 7-slot layout) | |
| 50–53 | Default Button: Light, Button, Dark, Frame | |
| 54–60 | Window (unfocused): Light 2, Light 1, Window, Dark 1, Dark 2, Frame, Label | 60 = unfocused title text |
| 61–78 | Window Transition 1–18 | unfocused title-bar gradient |
| 79–85 | Window Focus (same 7-slot layout) | 85 = focused title text |
| 86–103 | Window Focus Transition 1–18 | focused title-bar gradient |
| 104 | Menu Light | |
| 105 | Menu Background | |
| 106 | Menu Dark | |
| 107 | Menu Label | |
| 108 | Menu Hilite Light | |
| 109 | Menu Hilite Background | |
| 110 | Menu Hilite Dark | |
| 111 | Menu Hilite Label | |
| 112 | Menu Disable Label | |
| 113–122 | Progress Transition 1–10 | |
| 123 | Progress Bkgnd Light | |
| 124 | Progress Bkgnd | |
| 125 | Progress Bkgnd Dark | |
| 126 | Progress Frame | |
| 127 | Progress Label | |
| 128–132 | ScrollBar: Frame, Light, ScrollBar, Dark, Label | |
| 133–136 | ScrollBar Hilite: Light, Hilite, Dark, Label | |
| 137–139 | ScrollBar Indicator: Light, Indicator, Dark | thumb |
| 140–142 | ScrollBar Indicator Hilite: Light, Hilite, Dark | |
| 143–147 | ScrollBar Bkgnd: Light 2, Light 1, Bkgnd, Dark 1, Dark 2 | track |
| 148–152 | (reserved) | not exposed by any AppearanceEdit version |
| 153–157 | ScrollBar Disable: Light, Disable, Dark, Frame, Label | |
| 158–161 | Slider Indicator: Light, Indicator, Dark, Frame | |
| 162–165 | Slider Indicator Hilite: Light, Hilite, Dark, Frame | |
| 166–169 | Slider: Bar, Bar Frame, Bar Hilite, Bar Hilite Frame | |
| 170–173 | Slider Disable: Light, Disable, Dark, Frame | |
| 174–182 | Column Header: Frame, Light, Header, Dark, Label, Hilite Light, Hilite, Hilite Dark, Hilite Label | tab plates |
| 183–198 | File Label 0–15 | AppearanceEdit 1.4 calls these "List Label 1–15" (16 = base List Label); list-item label tints |
| 199–203 | (unused) | zero in every observed theme |

## Notes

- The UI list order in AppearanceEdit is not the file order everywhere:
  Progress Frame/Label (126/127) are listed before the Bkgnd trio (123–125),
  and the Slider Bar quad (166–169) is listed before the Indicator quads
  (158–165). The indices above are the *file* order.
- AppearanceEdit 1.4's "Icon Tints" rows are aliases of existing entries
  (they read back 38, 45, 109, 105), not extra table entries; its Graph
  colors are not stored in this 204-entry table at all.
- The native implementation exposes this registry as `enum HapColor` in
  `native/src/hap.h`.
