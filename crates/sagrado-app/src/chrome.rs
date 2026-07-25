//! Skinned window chrome: titlebar, window buttons and frame drawn from the
//! theme's window-frame textures (native decorations are disabled).

use eframe::egui::{
    self, Align2, Color32, CursorIcon, FontId, Rect, Sense, StrokeKind, Ui, Vec2, ViewportCommand,
};
use sagrado_theme::{Slot, Theme};

use crate::paint::{nine_slice, SkinTextures};

/// Height of the titlebar; matches the titlebar texture when present.
pub fn titlebar_height(theme: &Theme) -> f32 {
    theme
        .image(Slot::WindowTitleBarActive)
        .map(|img| img.size()[1] as f32)
        .unwrap_or(22.0)
}

pub fn titlebar(ui: &mut Ui, theme: &Theme, skin: &SkinTextures, title: &str) {
    let height = titlebar_height(theme);
    let rect = Rect::from_min_size(ui.max_rect().min, Vec2::new(ui.max_rect().width(), height));
    let focused = ui.input(|i| i.viewport().focused.unwrap_or(true));

    let bar_slot = if focused {
        Slot::WindowTitleBarActive
    } else {
        Slot::WindowTitleBarInactive
    };
    match (skin.get(bar_slot), theme.image(bar_slot)) {
        (Some(tex), Some(img)) => nine_slice(ui.painter(), tex, img, rect, Color32::WHITE),
        _ => {
            let c = theme.colors;
            let fill = if focused {
                c.primary_dark
            } else {
                c.primary_background
            };
            ui.painter().rect_filled(rect, 0.0, fill);
        }
    }
    ui.painter().text(
        rect.center(),
        Align2::CENTER_CENTER,
        title,
        FontId::proportional(13.0),
        theme.colors.text,
    );

    // Window buttons, placed from the texture's authored positions.
    let close = window_button(
        ui,
        theme,
        skin,
        rect,
        (
            Slot::WindowCloseNormal,
            Slot::WindowCloseHilited,
            Slot::WindowCloseInactive,
        ),
        focused,
    );
    if close.clicked() {
        ui.ctx().send_viewport_cmd(ViewportCommand::Close);
    }
    let collapse = window_button(
        ui,
        theme,
        skin,
        rect,
        (
            Slot::WindowCollapseNormal,
            Slot::WindowCollapseHilited,
            Slot::WindowCollapseInactive,
        ),
        focused,
    );
    if collapse.clicked() {
        ui.ctx().send_viewport_cmd(ViewportCommand::Minimized(true));
    }
    let zoom = window_button(
        ui,
        theme,
        skin,
        rect,
        (
            Slot::WindowZoomNormal,
            Slot::WindowZoomHilited,
            Slot::WindowZoomInactive,
        ),
        focused,
    );
    if zoom.clicked() {
        let maximized = ui.input(|i| i.viewport().maximized.unwrap_or(false));
        ui.ctx()
            .send_viewport_cmd(ViewportCommand::Maximized(!maximized));
    }
    window_button(
        ui,
        theme,
        skin,
        rect,
        (
            Slot::WindowRightNormal,
            Slot::WindowRightNormal,
            Slot::WindowRightInactive,
        ),
        focused,
    );

    // Drag anywhere else on the titlebar to move the window.
    let drag = ui.interact(rect, ui.id().with("titlebar_drag"), Sense::click_and_drag());
    if drag.drag_started_by(egui::PointerButton::Primary) {
        ui.ctx().send_viewport_cmd(ViewportCommand::StartDrag);
    }
}

/// Draw one titlebar button using the authored `positions` of its normal
/// image: `[left, top, right, bottom]` offsets within the titlebar.
/// A zero left with a nonzero right means the button is right-anchored.
fn window_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    bar: Rect,
    slots: (Slot, Slot, Slot),
    focused: bool,
) -> egui::Response {
    let Some(normal) = theme.image(slots.0) else {
        return ui.allocate_rect(Rect::NOTHING, Sense::hover());
    };
    let [w, h] = normal.size();
    let (w, h) = (w as f32, h as f32);
    let [l, t, r, _b] = normal.positions.map(f32::from);
    let x = if l > 0.0 {
        bar.left() + l
    } else {
        bar.right() - r - w
    };
    let y = if t > 0.0 {
        bar.top() + t
    } else {
        bar.top() + (bar.height() - h) / 2.0
    };
    let rect = Rect::from_min_size(egui::pos2(x, y), Vec2::new(w, h));

    let resp = ui.interact(rect, ui.id().with(slots.0.index()), Sense::click());
    let slot = if !focused {
        slots.2
    } else if resp.is_pointer_button_down_on() {
        slots.1
    } else {
        slots.0
    };
    if let (Some(tex), Some(img)) = (skin.get(slot), theme.image(slot)) {
        nine_slice(ui.painter(), tex, img, rect, Color32::WHITE);
    }
    resp.on_hover_cursor(CursorIcon::PointingHand)
}

/// 1px window outline drawn over everything, so the frameless window reads
/// as a framed KDX window.
pub fn window_frame(ui: &Ui, theme: &Theme) {
    ui.painter().rect(
        ui.max_rect(),
        0.0,
        Color32::TRANSPARENT,
        (1.0, theme.colors.text),
        StrokeKind::Inside,
    );
}
