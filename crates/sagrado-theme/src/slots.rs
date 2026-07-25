//! Widget image slots of the appearance engine.
//!
//! Slot indices match the image table of Haxial `.hap` files (528 entries),
//! so imported appearances map 1:1 onto Sagrado widgets. Names follow the
//! AppearanceEdit documentation. Only the slots currently rendered by the
//! engine are listed; unknown slots are still imported and kept by index.

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u16)]
pub enum Slot {
    PushButtonNormal = 25,
    PushButtonHilited = 26,
    PushButtonDisabled = 27,
    DefaultButtonNormal = 37,
    DefaultButtonHilited = 38,
    DefaultButtonDisabled = 39,
    TickBlankNormal = 57,
    TickBlankHilited = 58,
    TickBlankDisabled = 59,
    TickTickedNormal = 61,
    TickTickedHilited = 62,
    TickTickedDisabled = 63,
    MutexBlankNormal = 69,
    MutexBlankHilited = 70,
    MutexBlankDisabled = 71,
    MutexTickedNormal = 73,
    MutexTickedHilited = 74,
    MutexTickedDisabled = 75,
    HSliderBarNormal = 89,
    HSliderBarHilited = 90,
    HSliderBarDisabled = 91,
    HSliderIndicatorNormal = 93,
    HSliderIndicatorHilited = 94,
    HSliderIndicatorDisabled = 95,
    HorizSeparator = 105,
    VertSeparator = 106,
    Box = 107,
    FramedRaisedBox = 108,
    ProgressBar = 111,
    ProgressBarFill = 112,
    PopupButtonNormal = 130,
    PopupButtonHilited = 131,
    PopupButtonDisabled = 132,
    PopupSymbolNormal = 150,
    PopupSymbolHilited = 151,
    PopupSymbolDisabled = 152,
    HScrollDoubleArrows = 162,
    HScrollSingleArrows = 163,
    HScrollDisabled = 164,
    HScrollIndicatorNormal = 169,
    HScrollIndicatorHilited = 170,
    VScrollDoubleArrows = 181,
    VScrollSingleArrows = 182,
    VScrollDisabled = 183,
    VScrollIndicatorNormal = 188,
    VScrollIndicatorHilited = 189,
    ColumnHeaderNormal = 223,
    ColumnHeaderHilited = 224,
    ColumnHeaderDisabled = 225,
}

impl Slot {
    pub fn index(self) -> usize {
        self as u16 as usize
    }
}
