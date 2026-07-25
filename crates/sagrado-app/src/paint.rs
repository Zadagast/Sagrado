//! Texture upload and 9-slice rendering for skinned widgets.

use std::collections::HashMap;

use eframe::egui::{self, Color32, Rect, TextureHandle, TextureOptions, Vec2};
use sagrado_theme::{SkinImage, Slot, Theme};

/// GPU textures for the current theme's widget images, keyed by slot index.
#[derive(Default)]
pub struct SkinTextures {
    theme_name: Option<String>,
    textures: HashMap<usize, TextureHandle>,
}

impl SkinTextures {
    /// Upload the theme's images if the theme changed since the last frame.
    pub fn set_theme(&mut self, ctx: &egui::Context, theme: &Theme) {
        if self.theme_name.as_deref() == Some(theme.name.as_str()) {
            return;
        }
        self.textures.clear();
        for idx in theme.slot_indices() {
            let img = theme.image_by_index(idx).unwrap();
            let tex = ctx.load_texture(
                format!("skin:{}:{idx}", theme.name),
                img.image.clone(),
                TextureOptions::NEAREST,
            );
            self.textures.insert(idx, tex);
        }
        self.theme_name = Some(theme.name.clone());
    }

    pub fn get(&self, slot: Slot) -> Option<&TextureHandle> {
        self.textures.get(&slot.index())
    }
}

/// Paint `skin` into `rect` using 9-slice scaling: the four corner caps are
/// drawn unscaled, the edges are stretched along one axis, and the middle
/// fills the remainder — the Haxial Appearance Engine's "Caps" model.
pub fn nine_slice(
    painter: &egui::Painter,
    tex: &TextureHandle,
    skin: &SkinImage,
    rect: Rect,
    tint: Color32,
) {
    let [iw, ih] = skin.size();
    let (iw, ih) = (iw as f32, ih as f32);
    let [l, r, t, b] = skin.caps.map(f32::from);
    let (l, r) = (l.min(iw / 2.0), r.min(iw / 2.0));
    let (t, b) = (t.min(ih / 2.0), b.min(ih / 2.0));

    let xs_src = [0.0, l, iw - r, iw];
    let ys_src = [0.0, t, ih - b, ih];
    let xs_dst = [rect.left(), rect.left() + l, rect.right() - r, rect.right()];
    let ys_dst = [rect.top(), rect.top() + t, rect.bottom() - b, rect.bottom()];

    for row in 0..3 {
        for col in 0..3 {
            let src = Rect::from_min_max(
                egui::pos2(xs_src[col] / iw, ys_src[row] / ih),
                egui::pos2(xs_src[col + 1] / iw, ys_src[row + 1] / ih),
            );
            let dst = Rect::from_min_max(
                egui::pos2(xs_dst[col], ys_dst[row]),
                egui::pos2(xs_dst[col + 1], ys_dst[row + 1]),
            );
            if dst.width() > 0.0 && dst.height() > 0.0 && src.width() > 0.0 && src.height() > 0.0 {
                painter.image(tex.id(), dst, src, tint);
            }
        }
    }
}

/// Paint `skin` centered in `rect` at its natural size (for indicators,
/// symbols and other non-stretched images).
pub fn natural(
    painter: &egui::Painter,
    tex: &TextureHandle,
    skin: &SkinImage,
    rect: Rect,
    tint: Color32,
) {
    let [w, h] = skin.size();
    let size = Vec2::new(w as f32, h as f32);
    let dst = Rect::from_center_size(rect.center(), size);
    painter.image(
        tex.id(),
        dst,
        Rect::from_min_max(egui::pos2(0.0, 0.0), egui::pos2(1.0, 1.0)),
        tint,
    );
}
