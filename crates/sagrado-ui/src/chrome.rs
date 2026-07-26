//! Custom window chrome drawn from the theme's Window Frame images.
//!
//! The frame image's positions give the frame thickness on each side (the
//! top thickness is the title bar); the title-bar buttons carry their own
//! offsets in their positions, anchored from the window edges. Themes that
//! ship no window art (colour-only appearances such as Haxial's built-in
//! "Standard") fall back to a functional chrome drawn from theme colours, so
//! the window can still be dragged, resized, closed, minimised and maximised.

use eframe::egui::{
    self, Align2, Color32, CursorIcon, Rect, Sense, StrokeKind, Ui, Vec2, ViewportCommand,
};
use sagrado_theme::{Slot, Theme};

use crate::paint::{natural, nine_slice, SkinTextures};

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
                let (client, grip_rect) = fallback_chrome(ui, ctx, theme, title, rect, focused);
                add_contents(&mut content_ui(ui, client));
                resize_grip(ui, ctx, grip_rect);
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

            // Title-bar buttons, anchored by their position metadata. The hit
            // area and footprint come from the button's reference (focus/
            // normal) image; the per-state image is drawn centred in that
            // footprint so a differently sized "hilited" image (common in
            // themes such as Boilerplate) doesn't jump out of place.
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
                    natural(ui.painter(), tex, img, btn_rect, Color32::WHITE);
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
            let drag_rect = Rect::from_min_max(
                egui::pos2(rect.left() + occupied_left, rect.top()),
                egui::pos2(rect.right() - occupied_right, rect.top() + t),
            );
            title_drag(ui, ctx, drag_rect);

            // Resize button in the bottom-right corner. Paint it now, but run
            // its drag interaction *after* the content so it wins the corner
            // (egui gives the last-added widget priority where they overlap).
            let resize_slot = if focused {
                Slot::WindowResizeFocus
            } else {
                Slot::WindowResizeNormal
            };
            let grip_rect = theme
                .image(resize_slot)
                .or_else(|| theme.image(Slot::WindowResizeNormal))
                .map(|img| {
                    let [w, h] = img.size();
                    let (w, h) = (w as f32, h as f32);
                    let pos = egui::pos2(
                        rect.right() - img.pos_right().max(1.0) - w,
                        rect.bottom() - img.pos_bottom().max(1.0) - h,
                    );
                    if let Some(tex) = skin
                        .get(resize_slot)
                        .or_else(|| skin.get(Slot::WindowResizeNormal))
                    {
                        crate::paint::natural_at(ui.painter(), tex, img, pos, Color32::WHITE);
                    }
                    Rect::from_min_size(pos, Vec2::new(w, h))
                });

            let client = Rect::from_min_max(
                egui::pos2(rect.left() + l, rect.top() + t),
                egui::pos2(rect.right() - r, rect.bottom() - b),
            );
            add_contents(&mut content_ui(ui, client));

            if let Some(grip_rect) = grip_rect {
                resize_grip(ui, ctx, grip_rect);
            }
        });
}

fn content_ui(ui: &mut Ui, client: Rect) -> Ui {
    ui.new_child(
        egui::UiBuilder::new()
            .max_rect(client)
            .layout(egui::Layout::top_down(egui::Align::Min)),
    )
}

/// Begin an OS window move when the given strip is dragged with the primary
/// button.
fn title_drag(ui: &mut Ui, ctx: &egui::Context, drag_rect: Rect) {
    if drag_rect.width() <= 0.0 || drag_rect.height() <= 0.0 {
        return;
    }
    let resp = ui.interact(
        drag_rect,
        ui.id().with("title_drag"),
        Sense::click_and_drag(),
    );
    if resp.drag_started_by(egui::PointerButton::Primary) {
        ctx.send_viewport_cmd(ViewportCommand::StartDrag);
    }
}

/// Begin an OS resize (south-east) when the grip is dragged.
fn resize_grip(ui: &mut Ui, ctx: &egui::Context, grip_rect: Rect) {
    let resp = ui
        .interact(grip_rect, ui.id().with("resize"), Sense::drag())
        .on_hover_cursor(CursorIcon::ResizeSouthEast);
    if resp.drag_started() {
        ctx.send_viewport_cmd(ViewportCommand::BeginResize(
            egui::viewport::ResizeDirection::SouthEast,
        ));
    }
}

/// Chrome for colour-only appearances that ship no window art: a bevelled
/// title bar drawn from theme colours with working close / minimise /
/// maximise buttons, drag-to-move and a resize grip. Returns the client rect.
fn fallback_chrome(
    ui: &mut Ui,
    ctx: &egui::Context,
    theme: &Theme,
    title: &str,
    rect: Rect,
    focused: bool,
) -> (Rect, Rect) {
    let c = theme.colors;
    let title_h = 22.0;
    let border = 4.0;

    // Outer border + title bar.
    ui.painter().rect(
        rect,
        0.0,
        c.primary_background,
        (1.0, c.primary_frame),
        StrokeKind::Inside,
    );
    let title_bar = Rect::from_min_size(rect.min, Vec2::new(rect.width(), title_h));
    ui.painter().rect_filled(title_bar, 0.0, c.primary_dark);
    ui.painter().line_segment(
        [title_bar.left_bottom(), title_bar.right_bottom()],
        (1.0, c.primary_frame),
    );

    // Traffic-light style buttons at the left: close, minimise, maximise.
    let btn = 14.0;
    let gap = 6.0;
    let cy = title_bar.center().y;
    let mut x = rect.left() + 8.0;
    let title_fg = if focused {
        c.selection_text
    } else {
        c.disabled_text
    };

    let mut chrome_button = |ui: &mut Ui, id: &str, glyph: &str, fill: Color32| -> bool {
        let r = Rect::from_center_size(egui::pos2(x + btn / 2.0, cy), Vec2::splat(btn));
        let resp = ui.interact(r, ui.id().with(id), Sense::click());
        let bg = if resp.is_pointer_button_down_on() {
            c.primary_light
        } else {
            fill
        };
        ui.painter()
            .rect(r, 3.0, bg, (1.0, c.primary_frame), StrokeKind::Inside);
        ui.painter().text(
            r.center(),
            Align2::CENTER_CENTER,
            glyph,
            egui::FontId::proportional(11.0),
            c.primary_frame,
        );
        x += btn + gap;
        resp.clicked()
    };

    if chrome_button(ui, "fb_close", "×", theme.colors.alert) {
        ctx.send_viewport_cmd(ViewportCommand::Close);
    }
    if chrome_button(ui, "fb_min", "–", c.primary_light) {
        ctx.send_viewport_cmd(ViewportCommand::Minimized(true));
    }
    if chrome_button(ui, "fb_max", "+", c.primary_light) {
        let maximized = ctx.input(|i| i.viewport().maximized.unwrap_or(false));
        ctx.send_viewport_cmd(ViewportCommand::Maximized(!maximized));
    }

    // Title text, centred in the remaining strip.
    let text_area = Rect::from_min_max(egui::pos2(x + 4.0, title_bar.top()), title_bar.max);
    ui.painter().with_clip_rect(text_area).text(
        text_area.center(),
        Align2::CENTER_CENTER,
        title,
        crate::fonts::ui_font(),
        title_fg,
    );

    // Drag strip covers the title bar right of the buttons.
    title_drag(
        ui,
        ctx,
        Rect::from_min_max(egui::pos2(x, rect.top()), title_bar.right_bottom()),
    );

    // Resize grip: a small bevelled corner triangle. Painted here; the drag
    // interaction runs after the content so it wins the shared corner.
    let grip = 16.0;
    let grip_rect = Rect::from_min_max(
        egui::pos2(rect.right() - grip, rect.bottom() - grip),
        rect.max,
    );
    for i in 1..=3 {
        let o = i as f32 * 4.0;
        ui.painter().line_segment(
            [
                egui::pos2(grip_rect.right() - o, grip_rect.bottom() - 1.0),
                egui::pos2(grip_rect.right() - 1.0, grip_rect.bottom() - o),
            ],
            (1.0, c.primary_frame),
        );
    }

    let client = Rect::from_min_max(
        egui::pos2(rect.left() + border, rect.top() + title_h),
        egui::pos2(rect.right() - border, rect.bottom() - border),
    );
    (client, grip_rect)
}
