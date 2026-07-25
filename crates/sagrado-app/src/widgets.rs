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
    p.rect(
        rect,
        0.0,
        c.text_box_background,
        (1.0, c.text),
        StrokeKind::Inside,
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
    let mut open = ui
        .memory(|m| m.data.get_temp::<bool>(popup_id))
        .unwrap_or(false);
    if resp.clicked() {
        open = !open;
    }
    if open {
        let item_h = 18.0;
        let menu_size = Vec2::new(width, options.len() as f32 * item_h + 2.0);
        let area = egui::Area::new(popup_id)
            .order(egui::Order::Foreground)
            .fixed_pos(rect.left_bottom() + Vec2::new(0.0, 1.0))
            .show(ui.ctx(), |ui| {
                let (menu_rect, _) = ui.allocate_exact_size(menu_size, Sense::hover());
                let p = ui.painter().clone();
                p.rect(
                    menu_rect,
                    0.0,
                    c.text_box_background,
                    (1.0, c.text),
                    StrokeKind::Inside,
                );
                for (i, name) in options.iter().enumerate() {
                    let item_rect = Rect::from_min_size(
                        egui::pos2(
                            menu_rect.left() + 1.0,
                            menu_rect.top() + 1.0 + i as f32 * item_h,
                        ),
                        Vec2::new(menu_rect.width() - 2.0, item_h),
                    );
                    let item = ui.interact(item_rect, popup_id.with(i), Sense::click());
                    let (bg, fg) = if item.hovered() || i == *selected {
                        (c.selection, c.selection_text)
                    } else {
                        (c.text_box_background, c.text)
                    };
                    p.rect_filled(item_rect.shrink(1.0), 0.0, bg);
                    p.text(
                        egui::pos2(item_rect.left() + 6.0, item_rect.center().y),
                        Align2::LEFT_CENTER,
                        name,
                        FontId::proportional(13.0),
                        fg,
                    );
                    if item.clicked() {
                        *selected = i;
                        open = false;
                    }
                }
            });
        if area.response.clicked_elsewhere() && !resp.clicked() {
            open = false;
        }
    }
    ui.memory_mut(|m| m.data.insert_temp(popup_id, open));
    resp
}

fn scroll_part(
    ui: &Ui,
    theme: &Theme,
    skin: &SkinTextures,
    slot: Slot,
    rect: Rect,
    stretch: bool,
) -> bool {
    match (skin.get(slot), theme.image(slot)) {
        (Some(tex), Some(img)) => {
            if stretch {
                nine_slice(ui.painter(), tex, img, rect, Color32::WHITE);
            } else {
                natural(ui.painter(), tex, img, rect, Color32::WHITE);
            }
            true
        }
        _ => false,
    }
}

/// KDX-style scrollbar drawn from the theme's track, arrow-button, thumb and
/// grip images. Returns the response; `value` is 0..=1.
pub fn scrollbar(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    length: f32,
    value: &mut f32,
    vertical: bool,
    enabled: bool,
) -> Response {
    let track_slot = if vertical {
        Slot::VScrollTrack
    } else {
        Slot::HScrollTrack
    };
    let thickness = theme
        .image(track_slot)
        .map(|img| {
            let [w, h] = img.size();
            if vertical {
                w as f32
            } else {
                h as f32
            }
        })
        .unwrap_or(15.0);
    let size = if vertical {
        Vec2::new(thickness, length)
    } else {
        Vec2::new(length, thickness)
    };
    let (rect, mut resp) = ui.allocate_exact_size(size, Sense::click_and_drag());

    // KDX places a decrement/increment arrow pair at both ends of the bar.
    let arrow = thickness;
    let (dec_rects, inc_rects, trough) = if vertical {
        (
            [
                Rect::from_min_size(rect.min, Vec2::new(thickness, arrow)),
                Rect::from_min_size(
                    egui::pos2(rect.left(), rect.bottom() - 2.0 * arrow),
                    Vec2::new(thickness, arrow),
                ),
            ],
            [
                Rect::from_min_size(
                    egui::pos2(rect.left(), rect.top() + arrow),
                    Vec2::new(thickness, arrow),
                ),
                Rect::from_min_size(
                    egui::pos2(rect.left(), rect.bottom() - arrow),
                    Vec2::new(thickness, arrow),
                ),
            ],
            Rect::from_min_max(
                egui::pos2(rect.left(), rect.top() + 2.0 * arrow),
                egui::pos2(rect.right(), rect.bottom() - 2.0 * arrow),
            ),
        )
    } else {
        (
            [
                Rect::from_min_size(rect.min, Vec2::new(arrow, thickness)),
                Rect::from_min_size(
                    egui::pos2(rect.right() - 2.0 * arrow, rect.top()),
                    Vec2::new(arrow, thickness),
                ),
            ],
            [
                Rect::from_min_size(
                    egui::pos2(rect.left() + arrow, rect.top()),
                    Vec2::new(arrow, thickness),
                ),
                Rect::from_min_size(
                    egui::pos2(rect.right() - arrow, rect.top()),
                    Vec2::new(arrow, thickness),
                ),
            ],
            Rect::from_min_max(
                egui::pos2(rect.left() + 2.0 * arrow, rect.top()),
                egui::pos2(rect.right() - 2.0 * arrow, rect.bottom()),
            ),
        )
    };

    let trough_len = if vertical {
        trough.height()
    } else {
        trough.width()
    };
    let thumb_len = (trough_len * 0.35).clamp(16.0, trough_len.max(16.0));
    let travel = (trough_len - thumb_len).max(0.0);

    let pointer = resp.interact_pointer_pos();
    let over = |r: Rect| pointer.is_some_and(|p| r.contains(p));
    let pressed = resp.is_pointer_button_down_on();

    if enabled && resp.clicked() {
        if dec_rects.iter().any(|r| over(*r)) {
            *value = (*value - 0.1).clamp(0.0, 1.0);
            resp.mark_changed();
        } else if inc_rects.iter().any(|r| over(*r)) {
            *value = (*value + 0.1).clamp(0.0, 1.0);
            resp.mark_changed();
        }
    }
    if enabled && resp.dragged() && travel > 0.0 {
        if let Some(p) = pointer {
            let along = if vertical {
                p.y - trough.top()
            } else {
                p.x - trough.left()
            };
            *value = ((along - thumb_len / 2.0) / travel).clamp(0.0, 1.0);
            resp.mark_changed();
        }
    }

    let c = theme.colors;
    if !scroll_part(ui, theme, skin, track_slot, trough, true) {
        ui.painter().rect(
            rect,
            0.0,
            c.primary_background,
            (1.0, c.primary_dark),
            StrokeKind::Inside,
        );
    }

    let (dec_slots, inc_slots) = if vertical {
        (
            (
                Slot::VScrollUpNormal,
                Slot::VScrollUpHilited,
                Slot::VScrollUpDisabled,
            ),
            (
                Slot::VScrollDownNormal,
                Slot::VScrollDownHilited,
                Slot::VScrollDownDisabled,
            ),
        )
    } else {
        (
            (
                Slot::HScrollLeftNormal,
                Slot::HScrollLeftHilited,
                Slot::HScrollLeftDisabled,
            ),
            (
                Slot::HScrollRightNormal,
                Slot::HScrollRightHilited,
                Slot::HScrollRightDisabled,
            ),
        )
    };
    for (slots, r) in [
        (dec_slots, dec_rects[0]),
        (dec_slots, dec_rects[1]),
        (inc_slots, inc_rects[0]),
        (inc_slots, inc_rects[1]),
    ] {
        let slot = if !enabled {
            slots.2
        } else if pressed && over(r) {
            slots.1
        } else {
            slots.0
        };
        if !scroll_part(ui, theme, skin, slot, r, true) {
            fallback_bevel(ui, r, theme, pressed && over(r));
        }
    }

    if enabled && travel >= 0.0 && trough_len >= thumb_len {
        let start = *value * travel;
        let thumb_rect = if vertical {
            Rect::from_min_size(
                egui::pos2(trough.left(), trough.top() + start),
                Vec2::new(thickness, thumb_len),
            )
        } else {
            Rect::from_min_size(
                egui::pos2(trough.left() + start, trough.top()),
                Vec2::new(thumb_len, thickness),
            )
        };
        let dragging = resp.dragged();
        let (thumb_slot, grip_slot) = if vertical {
            (
                if dragging {
                    Slot::VScrollThumbHilited
                } else {
                    Slot::VScrollThumbNormal
                },
                if dragging {
                    Slot::VScrollGripHilited
                } else {
                    Slot::VScrollGripNormal
                },
            )
        } else {
            (
                if dragging {
                    Slot::HScrollThumbHilited
                } else {
                    Slot::HScrollThumbNormal
                },
                if dragging {
                    Slot::HScrollGripHilited
                } else {
                    Slot::HScrollGripNormal
                },
            )
        };
        if !scroll_part(ui, theme, skin, thumb_slot, thumb_rect, true) {
            fallback_bevel(ui, thumb_rect, theme, false);
        }
        if let (Some(tex), Some(img)) = (skin.get(grip_slot), theme.image(grip_slot)) {
            let [w, h] = img.size();
            let grip_rect = Rect::from_center_size(
                thumb_rect.center(),
                Vec2::new(w as f32, h as f32).min(thumb_rect.size()),
            );
            natural(ui.painter(), tex, img, grip_rect, Color32::WHITE);
        }
    }
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
