//! Skinned widgets drawn entirely from theme textures and colors.

use eframe::egui::{self, Align2, Color32, FontId, Rect, Response, Sense, StrokeKind, Ui, Vec2};
use sagrado_theme::{Slot, Theme};

use crate::paint::{natural, nine_slice, SkinTextures};

fn state_slot(normal: Slot, hilited: Slot, disabled: Slot, resp: &Response, enabled: bool) -> Slot {
    if !enabled {
        disabled
    } else if resp.is_pointer_button_down_on() {
        hilited
    } else {
        normal
    }
}

fn fallback_bevel(ui: &Ui, rect: Rect, theme: &Theme, pressed: bool) {
    let c = theme.colors;
    let (top, bottom) = if pressed {
        (c.primary_dark, c.primary_light)
    } else {
        (c.primary_light, c.primary_dark)
    };
    let p = ui.painter();
    p.rect_filled(rect, 2.0, c.primary_background);
    p.line_segment([rect.left_top(), rect.right_top()], (1.0, top));
    p.line_segment([rect.left_top(), rect.left_bottom()], (1.0, top));
    p.line_segment([rect.left_bottom(), rect.right_bottom()], (1.0, bottom));
    p.line_segment([rect.right_top(), rect.right_bottom()], (1.0, bottom));
}

fn draw_states(
    ui: &Ui,
    rect: Rect,
    theme: &Theme,
    skin: &SkinTextures,
    slots: (Slot, Slot, Slot),
    resp: &Response,
    enabled: bool,
) {
    let slot = state_slot(slots.0, slots.1, slots.2, resp, enabled);
    match (skin.get(slot), theme.image(slot)) {
        (Some(tex), Some(img)) => nine_slice(ui.painter(), tex, img, rect, Color32::WHITE),
        _ => fallback_bevel(ui, rect, theme, resp.is_pointer_button_down_on()),
    }
}

pub fn push_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    text: &str,
    enabled: bool,
    default: bool,
) -> Response {
    let font = FontId::proportional(13.0);
    let galley_width = ui
        .painter()
        .layout_no_wrap(text.to_owned(), font.clone(), theme.colors.text)
        .size()
        .x;
    let size = Vec2::new(galley_width + 28.0, 22.0);
    let (rect, resp) = ui.allocate_exact_size(size, Sense::click());
    let slots = if default {
        (
            Slot::DefaultButtonNormal,
            Slot::DefaultButtonHilited,
            Slot::DefaultButtonDisabled,
        )
    } else {
        (
            Slot::PushButtonNormal,
            Slot::PushButtonHilited,
            Slot::PushButtonDisabled,
        )
    };
    draw_states(ui, rect, theme, skin, slots, &resp, enabled);
    let text_color = if enabled {
        theme.colors.text
    } else {
        theme.colors.primary_dark
    };
    ui.painter()
        .text(rect.center(), Align2::CENTER_CENTER, text, font, text_color);
    resp
}

pub fn tick_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    checked: &mut bool,
    label: &str,
    enabled: bool,
) -> Response {
    toggle_button(
        ui,
        theme,
        skin,
        checked,
        label,
        enabled,
        (
            Slot::TickBlankNormal,
            Slot::TickBlankHilited,
            Slot::TickBlankDisabled,
        ),
        (
            Slot::TickTickedNormal,
            Slot::TickTickedHilited,
            Slot::TickTickedDisabled,
        ),
        false,
    )
}

pub fn mutex_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    selected: bool,
    label: &str,
    enabled: bool,
) -> Response {
    let mut on = selected;
    toggle_button(
        ui,
        theme,
        skin,
        &mut on,
        label,
        enabled,
        (
            Slot::MutexBlankNormal,
            Slot::MutexBlankHilited,
            Slot::MutexBlankDisabled,
        ),
        (
            Slot::MutexTickedNormal,
            Slot::MutexTickedHilited,
            Slot::MutexTickedDisabled,
        ),
        true,
    )
}

#[allow(clippy::too_many_arguments)]
fn toggle_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    checked: &mut bool,
    label: &str,
    enabled: bool,
    blank: (Slot, Slot, Slot),
    ticked: (Slot, Slot, Slot),
    round: bool,
) -> Response {
    let font = FontId::proportional(13.0);
    let text_width = ui
        .painter()
        .layout_no_wrap(label.to_owned(), font.clone(), theme.colors.text)
        .size()
        .x;
    let box_size = 14.0;
    let (rect, mut resp) =
        ui.allocate_exact_size(Vec2::new(box_size + 6.0 + text_width, 18.0), Sense::click());
    if resp.clicked() && enabled {
        *checked = !*checked;
        resp.mark_changed();
    }
    let slots = if *checked { ticked } else { blank };
    let slot = state_slot(slots.0, slots.1, slots.2, &resp, enabled);
    let box_rect = Rect::from_center_size(
        egui::pos2(rect.left() + box_size / 2.0, rect.center().y),
        Vec2::splat(box_size),
    );
    match (skin.get(slot), theme.image(slot)) {
        (Some(tex), Some(img)) => natural(ui.painter(), tex, img, box_rect, Color32::WHITE),
        _ => {
            let p = ui.painter();
            let c = theme.colors;
            if round {
                p.circle(
                    box_rect.center(),
                    box_size / 2.0 - 1.0,
                    c.text_box_background,
                    (1.0, c.text),
                );
                if *checked {
                    p.circle_filled(box_rect.center(), box_size / 4.0 - 1.0, c.selection);
                }
            } else {
                p.rect(
                    box_rect.shrink(1.0),
                    0.0,
                    c.text_box_background,
                    (1.0, c.text),
                    StrokeKind::Inside,
                );
                if *checked {
                    p.text(
                        box_rect.center(),
                        Align2::CENTER_CENTER,
                        "✕",
                        FontId::proportional(11.0),
                        c.text,
                    );
                }
            }
        }
    }
    let text_color = if enabled {
        theme.colors.text
    } else {
        theme.colors.primary_dark
    };
    ui.painter().text(
        egui::pos2(rect.left() + box_size + 6.0, rect.center().y),
        Align2::LEFT_CENTER,
        label,
        font,
        text_color,
    );
    resp
}

pub fn h_slider(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    value: &mut f32,
    width: f32,
    enabled: bool,
) -> Response {
    let (rect, mut resp) = ui.allocate_exact_size(Vec2::new(width, 24.0), Sense::click_and_drag());
    if enabled && (resp.dragged() || resp.clicked()) {
        if let Some(pos) = resp.interact_pointer_pos() {
            *value = ((pos.x - rect.left()) / rect.width()).clamp(0.0, 1.0);
            resp.mark_changed();
        }
    }
    let bar_slot = if enabled {
        if resp.dragged() {
            Slot::HSliderBarHilited
        } else {
            Slot::HSliderBarNormal
        }
    } else {
        Slot::HSliderBarDisabled
    };
    let bar_rect = Rect::from_center_size(rect.center(), Vec2::new(rect.width(), 8.0));
    match (skin.get(bar_slot), theme.image(bar_slot)) {
        (Some(tex), Some(img)) => {
            let h = img.size()[1] as f32;
            let r = Rect::from_center_size(rect.center(), Vec2::new(rect.width(), h));
            nine_slice(ui.painter(), tex, img, r, Color32::WHITE);
        }
        _ => {
            ui.painter().rect(
                bar_rect,
                2.0,
                theme.colors.text_box_background,
                (1.0, theme.colors.primary_dark),
                StrokeKind::Inside,
            );
        }
    }
    let ind_slot = if enabled {
        if resp.dragged() {
            Slot::HSliderIndicatorHilited
        } else {
            Slot::HSliderIndicatorNormal
        }
    } else {
        Slot::HSliderIndicatorDisabled
    };
    let x = rect.left() + *value * rect.width();
    let ind_rect = Rect::from_center_size(egui::pos2(x, rect.center().y), Vec2::splat(16.0));
    match (skin.get(ind_slot), theme.image(ind_slot)) {
        (Some(tex), Some(img)) => natural(ui.painter(), tex, img, ind_rect, Color32::WHITE),
        _ => {
            ui.painter().circle(
                ind_rect.center(),
                6.0,
                theme.colors.primary_light,
                (1.0, theme.colors.text),
            );
        }
    }
    resp
}

pub fn progress_bar(ui: &mut Ui, theme: &Theme, skin: &SkinTextures, fraction: f32, width: f32) {
    let (rect, _) = ui.allocate_exact_size(Vec2::new(width, 16.0), Sense::hover());
    let c = theme.colors;
    match (
        skin.get(Slot::ProgressBar),
        theme.image(Slot::ProgressBar),
        skin.get(Slot::ProgressBarFill),
        theme.image(Slot::ProgressBarFill),
    ) {
        (Some(bar_tex), Some(bar_img), Some(fill_tex), Some(fill_img)) => {
            let h = bar_img.size()[1] as f32;
            let bar_rect = Rect::from_center_size(rect.center(), Vec2::new(rect.width(), h));
            nine_slice(ui.painter(), bar_tex, bar_img, bar_rect, Color32::WHITE);
            // Fill positions: [left, top, right, bottom] insets into the bar.
            let [l, t, r, b] = fill_img.positions.map(f32::from);
            let inner = Rect::from_min_max(
                egui::pos2(bar_rect.left() + l, bar_rect.top() + t),
                egui::pos2(bar_rect.right() - r, bar_rect.bottom() - b),
            );
            let fill_rect = Rect::from_min_max(
                inner.min,
                egui::pos2(
                    inner.left() + inner.width() * fraction.clamp(0.0, 1.0),
                    inner.bottom(),
                ),
            );
            if fill_rect.width() > 0.0 {
                nine_slice(ui.painter(), fill_tex, fill_img, fill_rect, Color32::WHITE);
            }
        }
        _ => {
            let bar_rect = Rect::from_center_size(rect.center(), Vec2::new(rect.width(), 12.0));
            ui.painter().rect(
                bar_rect,
                2.0,
                c.text_box_background,
                (1.0, c.primary_dark),
                StrokeKind::Inside,
            );
            let mut fill = bar_rect.shrink(1.0);
            fill.set_width(fill.width() * fraction.clamp(0.0, 1.0));
            ui.painter().rect_filled(fill, 1.0, c.selection);
        }
    }
}

pub fn column_header(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    text: &str,
    width: f32,
) -> Response {
    let _ = skin;
    let (rect, resp) = ui.allocate_exact_size(Vec2::new(width, 18.0), Sense::click());
    let c = theme.colors;
    let p = ui.painter();
    p.rect_filled(rect, 0.0, c.primary_background);
    p.line_segment([rect.left_top(), rect.right_top()], (1.0, c.primary_light));
    p.line_segment(
        [rect.left_top(), rect.left_bottom()],
        (1.0, c.primary_light),
    );
    p.line_segment(
        [rect.left_bottom(), rect.right_bottom()],
        (1.0, c.primary_dark),
    );
    p.line_segment(
        [rect.right_top(), rect.right_bottom()],
        (1.0, c.primary_dark),
    );
    ui.painter().text(
        egui::pos2(rect.left() + 6.0, rect.center().y),
        Align2::LEFT_CENTER,
        text,
        FontId::proportional(12.0),
        theme.colors.text,
    );
    resp
}

/// KDX-style popup button: a recessed text field with the theme's popup
/// arrows image right-anchored, opening a themed menu of options.
pub fn popup_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    id: egui::Id,
    options: &[String],
    selected: &mut usize,
    width: f32,
) -> Response {
    let c = theme.colors;
    let (rect, resp) = ui.allocate_exact_size(Vec2::new(width, 20.0), Sense::click());
    let p = ui.painter();
    p.rect_filled(rect, 0.0, c.text_box_background);
    p.rect(
        rect,
        0.0,
        Color32::TRANSPARENT,
        (1.0, c.primary_dark),
        StrokeKind::Inside,
    );

    // Popup arrows button, right-anchored per its authored positions.
    let slot = if resp.is_pointer_button_down_on() {
        Slot::PopupButtonHilited
    } else {
        Slot::PopupButtonNormal
    };
    let mut text_right = rect.right() - 6.0;
    if let (Some(tex), Some(img)) = (skin.get(slot), theme.image(slot)) {
        let [w, h] = img.size();
        let (w, h) = (w as f32, h as f32);
        let [l, _t, r, _b] = img.positions.map(f32::from);
        let x = if l > 0.0 {
            rect.left() + l
        } else {
            rect.right() - r - w
        };
        let arrows = Rect::from_min_size(
            egui::pos2(x, rect.top() + (rect.height() - h) / 2.0),
            Vec2::new(w, h),
        );
        nine_slice(p, tex, img, arrows, Color32::WHITE);
        text_right = arrows.left() - 4.0;
    }
    let clip = p.with_clip_rect(Rect::from_min_max(
        rect.min,
        egui::pos2(text_right, rect.bottom()),
    ));
    clip.text(
        egui::pos2(rect.left() + 6.0, rect.center().y),
        Align2::LEFT_CENTER,
        options.get(*selected).map(String::as_str).unwrap_or(""),
        FontId::proportional(13.0),
        c.text,
    );

    let popup_id = id.with("popup");
    if resp.clicked() {
        ui.memory_mut(|m| m.toggle_popup(popup_id));
    }
    egui::popup_below_widget(
        ui,
        popup_id,
        &resp,
        egui::PopupCloseBehavior::CloseOnClick,
        |ui: &mut Ui| {
            ui.set_min_width(width - 2.0);
            egui::Frame::new()
                .fill(c.text_box_background)
                .stroke((1.0, c.primary_dark))
                .show(ui, |ui| {
                    for (i, name) in options.iter().enumerate() {
                        let checked = i == *selected;
                        let item = ui.allocate_response(
                            Vec2::new(ui.available_width().max(width - 6.0), 18.0),
                            Sense::click(),
                        );
                        let (bg, fg) = if item.hovered() || checked {
                            (c.selection, c.selection_text)
                        } else {
                            (c.text_box_background, c.text)
                        };
                        ui.painter().rect_filled(item.rect, 0.0, bg);
                        ui.painter().text(
                            egui::pos2(item.rect.left() + 6.0, item.rect.center().y),
                            Align2::LEFT_CENTER,
                            name,
                            FontId::proportional(13.0),
                            fg,
                        );
                        if item.clicked() {
                            *selected = i;
                        }
                    }
                });
        },
    );
    resp
}

pub fn separator(ui: &mut Ui, theme: &Theme, skin: &SkinTextures, width: f32) {
    let (rect, _) = ui.allocate_exact_size(Vec2::new(width, 6.0), Sense::hover());
    match (
        skin.get(Slot::HorizSeparator),
        theme.image(Slot::HorizSeparator),
    ) {
        (Some(tex), Some(img)) => {
            let h = img.size()[1] as f32;
            let r = Rect::from_center_size(rect.center(), Vec2::new(rect.width(), h));
            nine_slice(ui.painter(), tex, img, r, Color32::WHITE);
        }
        _ => {
            ui.painter().line_segment(
                [rect.left_center(), rect.right_center()],
                (1.0, theme.colors.primary_dark),
            );
        }
    }
}

pub fn text_box(ui: &mut Ui, theme: &Theme, text: &mut String, width: f32) -> Response {
    let c = theme.colors;
    let frame = egui::Frame::new()
        .fill(c.text_box_background)
        .stroke((1.0, c.text))
        .inner_margin(egui::Margin::symmetric(4, 2));
    frame
        .show(ui, |ui| {
            let mut style = (**ui.style()).clone();
            style.visuals.extreme_bg_color = c.text_box_background;
            style.visuals.override_text_color = Some(c.text);
            style.visuals.selection.bg_fill = c.selection;
            style.visuals.selection.stroke = (1.0, c.selection_text).into();
            ui.set_style(style);
            ui.add(
                egui::TextEdit::singleline(text)
                    .desired_width(width)
                    .frame(false),
            )
        })
        .inner
}
