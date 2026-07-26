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

/// Tiles thinner than this repeat imperceptibly, so we stretch instead of
/// emitting one quad per pixel.
const MIN_TILE_PX: f32 = 3.0;
/// Never emit more than this many tiles along one axis; beyond it, stretch.
/// A solid or near-solid material (the common case for window backgrounds and
/// title bars, whose center strip is ~1px) then costs a single quad instead of
/// hundreds of thousands.
const MAX_REPEATS: f32 = 256.0;

/// Repeat the source region `src` (in texel coordinates) across `dst`,
/// clipping partial tiles — the Appearance Engine repeats rather than
/// stretches the material between caps.
fn tile(
    painter: &egui::Painter,
    tex: &TextureHandle,
    src: Rect,
    tex_size: Vec2,
    dst: Rect,
    tint: Color32,
) {
    let tw = src.width();
    let th = src.height();
    if tw <= 0.0 || th <= 0.0 || dst.width() <= 0.0 || dst.height() <= 0.0 {
        return;
    }
    let uv = Rect::from_min_max(
        egui::pos2(src.left() / tex_size.x, src.top() / tex_size.y),
        egui::pos2(src.right() / tex_size.x, src.bottom() / tex_size.y),
    );
    // Fall back to stretching an axis when tiling it would be pathological
    // (tile ~1px, or so many repeats they can't be told apart from a stretch).
    let step_x = if tw < MIN_TILE_PX || dst.width() / tw > MAX_REPEATS {
        dst.width()
    } else {
        tw
    };
    let step_y = if th < MIN_TILE_PX || dst.height() / th > MAX_REPEATS {
        dst.height()
    } else {
        th
    };
    let p = painter.with_clip_rect(dst.intersect(painter.clip_rect()));
    let mut y = dst.top();
    while y < dst.bottom() {
        let mut x = dst.left();
        while x < dst.right() {
            p.image(
                tex.id(),
                Rect::from_min_size(egui::pos2(x, y), Vec2::new(step_x, step_y)),
                uv,
                tint,
            );
            x += step_x;
        }
        y += step_y;
    }
}

/// Paint `skin` into `rect` the way the Haxial Appearance Engine does: the
/// four corner caps are copied unscaled to the destination corners, the four
/// edge strips repeat along their axis, and the middle repeats in both axes.
pub fn nine_slice(
    painter: &egui::Painter,
    tex: &TextureHandle,
    skin: &SkinImage,
    rect: Rect,
    tint: Color32,
) {
    let [iw, ih] = skin.size();
    let (iw, ih) = (iw as f32, ih as f32);
    let [l, t, r, b] = skin.caps.map(f32::from);
    let (l, r) = (l.min(iw), r.min(iw - l));
    let (t, b) = (t.min(ih), b.min(ih - t));
    let tex_size = Vec2::new(iw, ih);

    let xs_src = [0.0, l, iw - r, iw];
    let ys_src = [0.0, t, ih - b, ih];
    let xs_dst = [
        rect.left(),
        rect.left() + l,
        (rect.right() - r).max(rect.left() + l),
        rect.right(),
    ];
    let ys_dst = [
        rect.top(),
        rect.top() + t,
        (rect.bottom() - b).max(rect.top() + t),
        rect.bottom(),
    ];

    for row in 0..3 {
        for col in 0..3 {
            let src = Rect::from_min_max(
                egui::pos2(xs_src[col], ys_src[row]),
                egui::pos2(xs_src[col + 1], ys_src[row + 1]),
            );
            let dst = Rect::from_min_max(
                egui::pos2(xs_dst[col], ys_dst[row]),
                egui::pos2(xs_dst[col + 1], ys_dst[row + 1]),
            );
            if dst.width() <= 0.0
                || dst.height() <= 0.0
                || src.width() <= 0.0
                || src.height() <= 0.0
            {
                continue;
            }
            if row != 1 && col != 1 {
                // Corner: copied once at its natural size.
                let uv = Rect::from_min_max(
                    egui::pos2(src.left() / iw, src.top() / ih),
                    egui::pos2(src.right() / iw, src.bottom() / ih),
                );
                painter.image(tex.id(), dst, uv, tint);
            } else {
                tile(painter, tex, src, tex_size, dst, tint);
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

/// Paint `skin` at its natural size with its top-left corner at `pos`.
pub fn natural_at(
    painter: &egui::Painter,
    tex: &TextureHandle,
    skin: &SkinImage,
    pos: egui::Pos2,
    tint: Color32,
) {
    let [w, h] = skin.size();
    let dst = Rect::from_min_size(pos, Vec2::new(w as f32, h as f32));
    painter.image(
        tex.id(),
        dst,
        Rect::from_min_max(egui::pos2(0.0, 0.0), egui::pos2(1.0, 1.0)),
        tint,
    );
}
