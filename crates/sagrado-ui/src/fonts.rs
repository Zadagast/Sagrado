//! The KDX-style UI font: a chunky bold pixel face (Pixel Operator, CC0)
//! matching the look of the original Haxial system font. Designed for a 16px
//! em; use [`UI_SIZE`] (or multiples) so glyphs stay pixel-crisp.

use std::sync::Arc;

use eframe::egui::{self, FontData, FontDefinitions, FontFamily, FontId};

/// Native pixel size of the bundled font.
pub const UI_SIZE: f32 = 16.0;

/// The standard UI font for menus, buttons, titles and labels.
pub fn ui_font() -> FontId {
    FontId::proportional(UI_SIZE)
}

/// The editor font (bold pixel monospace).
pub fn mono_font() -> FontId {
    FontId::monospace(UI_SIZE)
}

/// Register the bundled pixel fonts as the default proportional and
/// monospace families. Call once at startup.
pub fn install(ctx: &egui::Context) {
    let mut fonts = FontDefinitions::default();
    fonts.font_data.insert(
        "pixel_operator_bold".to_owned(),
        Arc::new(FontData::from_static(include_bytes!(
            "../assets/PixelOperator-Bold.ttf"
        ))),
    );
    fonts.font_data.insert(
        "pixel_operator_mono_bold".to_owned(),
        Arc::new(FontData::from_static(include_bytes!(
            "../assets/PixelOperatorMono-Bold.ttf"
        ))),
    );
    fonts
        .families
        .get_mut(&FontFamily::Proportional)
        .unwrap()
        .insert(0, "pixel_operator_bold".to_owned());
    fonts
        .families
        .get_mut(&FontFamily::Monospace)
        .unwrap()
        .insert(0, "pixel_operator_mono_bold".to_owned());
    ctx.set_fonts(fonts);
}
