//! Custom window chrome drawn from the theme's Window Frame images.
//!
//! The frame image's positions give the frame thickness on each side (the
//! top thickness is the title bar); the title-bar buttons carry their own
//! offsets in their positions, anchored from the window edges.

use eframe::egui::{self, Align2, Color32, CursorIcon, Rect, Sense, Ui, Vec2, ViewportCommand};
use sagrado_theme::{Slot, Theme};

use crate::paint::{natural_at, nine_slice, SkinTextures};

/// Show a fully skinned window: themed frame, title bar with menu / minimize
/// / maximize / close buttons, drag-to-move, and a resize corner. The content
/// closure fills the client area.
pub fn window_frame(
    ctx: &egui::Context,
    theme: &Theme,
    skin: &SkinTextures,
    title: &str,
    add_contents: impl FnOnce(&mut Ui),
) {
    let focused = ctx.input(|i| i.viewport().focused.unwrap_or(true));
    let frame_slot = if focused {
        Slot::WindowFrameFocus
    } else {
        Slot::WindowFrameNormal
    };

    egui::CentralPanel::default()
        .frame(egui::Frame::new())
        .show(ctx, |ui| {
            let rect = ui.max_rect();
            let c = theme.colors;
            ui.painter().rect_filled(rect, 0.0, c.primary_background);

            let Some(frame_img) = theme.image(frame_slot) else {
                fallback_chrome(ui, theme, title, rect);
                let client = Rect::from_min_max(
                    egui::pos2(rect.left() + 4.0, rect.top() + 28.0),
                    rect.max - Vec2::splat(4.0),
                );
                add_contents(&mut content_ui(ui, client));
                return;
            };
            let (l, t, r, b) = (
                frame_img.pos_left(),
                frame_img.pos_top(),
                frame_img.pos_right(),
                frame_img.pos_bottom(),
            );

            if let Some(tex) = skin.get(frame_slot) {
                nine_slice(ui.painter(), tex, frame_img, rect, Color32::WHITE);
            }

            let title_bar = Rect::from_min_max(rect.min, egui::pos2(rect.right(), rect.top() + t));

            // Title-bar buttons, anchored by their position metadata.
            let mut occupied_left = l;
            let mut occupied_right = r;
            let mut button = |ui: &mut Ui,
                              normal: Slot,
                              focus: Slot,
                              hilited: Slot,
                              id: &str|
             -> Option<(Rect, bool)> {
                let img = theme.image(focus).or_else(|| theme.image(normal))?;
                let [w, h] = img.size();
                let (w, h) = (w as f32, h as f32);
                let x = if img.pos_left() > 0.0 {
                    rect.left() + img.pos_left()
                } else {
                    rect.right() - img.pos_right() - w
                };
                let y = rect.top() + img.pos_top().max(2.0);
                let btn_rect = Rect::from_min_size(egui::pos2(x, y), Vec2::new(w, h));
                let resp = ui.interact(btn_rect, ui.id().with(id), Sense::click());
                let slot = if resp.is_pointer_button_down_on() {
                    hilited
                } else if focused {
                    focus
                } else {
                    normal
                };
                let (slot, img) = match theme.image(slot) {
                    Some(i) => (slot, i),
                    None => (normal, theme.image(normal)?),
                };
                if let Some(tex) = skin.get(slot) {
                    natural_at(ui.painter(), tex, img, btn_rect.min, Color32::WHITE);
                }
                if img.pos_left() > 0.0 {
                    occupied_left = occupied_left.max(img.pos_left() + w);
                } else {
                    occupied_right = occupied_right.max(img.pos_right() + w);
                }
                Some((btn_rect, resp.clicked()))
            };

            if let Some((_, clicked)) = button(
                ui,
                Slot::WindowCloseNormal,
                Slot::WindowCloseFocus,
                Slot::WindowCloseHilited,
                "close",
            ) {
                if clicked {
                    ctx.send_viewport_cmd(ViewportCommand::Close);
                }
            }
            if let Some((_, clicked)) = button(
                ui,
                Slot::WindowMaximizeNormal,
                Slot::WindowMaximizeFocus,
                Slot::WindowMaximizeHilited,
                "maximize",
            ) {
                if clicked {
                    let maximized = ctx.input(|i| i.viewport().maximized.unwrap_or(false));
                    ctx.send_viewport_cmd(ViewportCommand::Maximized(!maximized));
                }
            }
            if let Some((_, clicked)) = button(
                ui,
                Slot::WindowMinimizeNormal,
                Slot::WindowMinimizeFocus,
                Slot::WindowMinimizeHilited,
                "minimize",
            ) {
                if clicked {
                    ctx.send_viewport_cmd(ViewportCommand::Minimized(true));
                }
            }
            let _ = button(
                ui,
                Slot::WindowMenuNormal,
                Slot::WindowMenuFocus,
                Slot::WindowMenuFocus,
                "window_menu",
            );

            // Title text between the buttons.
            let title_color = if focused { c.text } else { c.disabled_text };
            let text_area = Rect::from_min_max(
                egui::pos2(rect.left() + occupied_left + 4.0, title_bar.top()),
                egui::pos2(rect.right() - occupied_right - 4.0, title_bar.bottom()),
            );
            ui.painter().with_clip_rect(text_area).text(
                text_area.center(),
                Align2::CENTER_CENTER,
                title,
                crate::fonts::ui_font(),
                title_color,
            );

            // Drag the title bar to move the window.
            let drag_resp = ui.interact(
                Rect::from_min_max(
                    egui::pos2(rect.left() + occupied_left, rect.top()),
                    egui::pos2(rect.right() - occupied_right, rect.top() + t),
                ),
                ui.id().with("title_drag"),
                Sense::click_and_drag(),
            );
            if drag_resp.drag_started_by(egui::PointerButton::Primary) {
                ctx.send_viewport_cmd(ViewportCommand::StartDrag);
            }

            // Resize button in the bottom-right corner.
            let resize_slot = if focused {
                Slot::WindowResizeFocus
            } else {
                Slot::WindowResizeNormal
            };
            if let Some(img) = theme
                .image(resize_slot)
                .or_else(|| theme.image(Slot::WindowResizeNormal))
            {
                let [w, h] = img.size();
                let (w, h) = (w as f32, h as f32);
                let pos = egui::pos2(
                    rect.right() - img.pos_right().max(1.0) - w,
                    rect.bottom() - img.pos_bottom().max(1.0) - h,
                );
                let grip_rect = Rect::from_min_size(pos, Vec2::new(w, h));
                if let Some(tex) = skin
                    .get(resize_slot)
                    .or_else(|| skin.get(Slot::WindowResizeNormal))
                {
                    natural_at(ui.painter(), tex, img, pos, Color32::WHITE);
                }
                let resp = ui
                    .interact(grip_rect, ui.id().with("resize"), Sense::drag())
                    .on_hover_cursor(CursorIcon::ResizeSouthEast);
                if resp.drag_started() {
                    ctx.send_viewport_cmd(ViewportCommand::BeginResize(
                        egui::viewport::ResizeDirection::SouthEast,
                    ));
                }
            }

            let client = Rect::from_min_max(
                egui::pos2(rect.left() + l, rect.top() + t),
                egui::pos2(rect.right() - r, rect.bottom() - b),
            );
            add_contents(&mut content_ui(ui, client));
        });
}

fn content_ui(ui: &mut Ui, client: Rect) -> Ui {
    ui.new_child(
        egui::UiBuilder::new()
            .max_rect(client)
            .layout(egui::Layout::top_down(egui::Align::Min)),
    )
}

fn fallback_chrome(ui: &mut Ui, theme: &Theme, title: &str, rect: Rect) {
    let c = theme.colors;
    let title_bar = Rect::from_min_size(rect.min, Vec2::new(rect.width(), 24.0));
    ui.painter().rect_filled(title_bar, 0.0, c.primary_dark);
    ui.painter().text(
        title_bar.center(),
        Align2::CENTER_CENTER,
        title,
        crate::fonts::ui_font(),
        c.selection_text,
    );
}
