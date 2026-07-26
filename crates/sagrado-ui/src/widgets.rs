//! Skinned widgets drawn entirely from theme textures and colors.

use eframe::egui::{self, Align2, Color32, Rect, Response, Sense, StrokeKind, Ui, Vec2};
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
    let font = crate::fonts::ui_font();
    let galley_width = ui
        .painter()
        .layout_no_wrap(text.to_owned(), font.clone(), theme.colors.text)
        .size()
        .x;
    // A default button is drawn 3 pixels larger on every side than a
    // regular button (Appearance Engine rule).
    let pad = if default { 3.0 } else { 0.0 };
    let size = Vec2::new(galley_width + 28.0 + 2.0 * pad, 22.0 + 2.0 * pad);
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
        theme.colors.disabled_text
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
    let font = crate::fonts::ui_font();
    let text_width = ui
        .painter()
        .layout_no_wrap(label.to_owned(), font.clone(), theme.colors.text)
        .size()
        .x;
    // Tick/mutex images have a maximum height of 18 pixels; draw them at
    // their natural size.
    let box_size = theme
        .image(blank.0)
        .map(|img| img.size()[0] as f32)
        .unwrap_or(14.0);
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
                        crate::fonts::ui_font(),
                        c.text,
                    );
                }
            }
        }
    }
    let text_color = if enabled {
        theme.colors.text
    } else {
        theme.colors.disabled_text
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

/// Zone of a scrollbar the pointer grabbed on press.
#[derive(Clone, Copy, PartialEq, Default)]
enum ScrollZone {
    /// Not grabbed.
    #[default]
    None,
    /// Dragging the thumb; carries the grab offset from the thumb start.
    Thumb(f32),
    Dec,
    Inc,
    PageDec,
    PageInc,
}

/// Classic vertical scroll bar: arrow buttons at both ends (click or hold to
/// scroll by lines), page-scrolling on the track, and a proportional
/// draggable thumb. `value` is 0..=1; `visible` is the fraction of the
/// content that fits in the viewport (controls the thumb length).
pub fn v_scrollbar(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    value: &mut f32,
    length: f32,
    visible: f32,
    enabled: bool,
) -> Response {
    let thickness = scrollbar_thickness(theme, false);
    let (rect, resp) =
        ui.allocate_exact_size(Vec2::new(thickness, length), Sense::click_and_drag());
    scrollbar_in(ui, theme, skin, rect, resp, value, visible, enabled, false)
}

/// Classic horizontal scroll bar; see [`v_scrollbar`].
pub fn h_scrollbar(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    value: &mut f32,
    length: f32,
    visible: f32,
    enabled: bool,
) -> Response {
    let thickness = scrollbar_thickness(theme, true);
    let (rect, resp) =
        ui.allocate_exact_size(Vec2::new(length, thickness), Sense::click_and_drag());
    scrollbar_in(ui, theme, skin, rect, resp, value, visible, enabled, true)
}

/// The thickness (16 px in original themes) of a scroll bar, taken from its
/// track art when present.
pub fn scrollbar_thickness(theme: &Theme, horizontal: bool) -> f32 {
    let slot = if horizontal {
        Slot::HScrollSingleArrows
    } else {
        Slot::VScrollSingleArrows
    };
    theme
        .image(slot)
        .map(|img| {
            let [w, h] = img.size();
            if horizontal {
                h as f32
            } else {
                w as f32
            }
        })
        .unwrap_or(16.0)
}

/// Draw a scroll bar occupying an explicit `rect` (with its `resp` already
/// obtained), so callers such as the text editor can lay the KDX bar into a
/// reserved gutter. Returns the response.
#[allow(clippy::too_many_arguments)]
pub fn v_scrollbar_in(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    rect: Rect,
    value: &mut f32,
    visible: f32,
    enabled: bool,
) -> Response {
    let resp = ui.interact(rect, ui.id().with("v_scroll_in"), Sense::click_and_drag());
    scrollbar_in(ui, theme, skin, rect, resp, value, visible, enabled, false)
}

/// Horizontal counterpart of [`v_scrollbar_in`].
#[allow(clippy::too_many_arguments)]
pub fn h_scrollbar_in(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    rect: Rect,
    value: &mut f32,
    visible: f32,
    enabled: bool,
) -> Response {
    let resp = ui.interact(rect, ui.id().with("h_scroll_in"), Sense::click_and_drag());
    scrollbar_in(ui, theme, skin, rect, resp, value, visible, enabled, true)
}

#[allow(clippy::too_many_arguments)]
fn scrollbar_in(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    rect: Rect,
    mut resp: Response,
    value: &mut f32,
    visible: f32,
    enabled: bool,
    horizontal: bool,
) -> Response {
    // KDX classically puts a double arrow (both directions) at each end of
    // the bar; prefer that art and fall back to the single-arrow track.
    let (double_slot, single_slot, disabled_slot, thumb_normal, thumb_hilited) = if horizontal {
        (
            Slot::HScrollDoubleArrows,
            Slot::HScrollSingleArrows,
            Slot::HScrollDisabled,
            Slot::HScrollIndicatorNormal,
            Slot::HScrollIndicatorHilited,
        )
    } else {
        (
            Slot::VScrollDoubleArrows,
            Slot::VScrollSingleArrows,
            Slot::VScrollDisabled,
            Slot::VScrollIndicatorNormal,
            Slot::VScrollIndicatorHilited,
        )
    };
    let track_slot = if !enabled {
        disabled_slot
    } else if theme.image(double_slot).is_some() {
        double_slot
    } else {
        single_slot
    };
    let track_img = theme.image(track_slot);
    let double_arrows = track_img.is_none() || track_slot == double_slot;

    // Scrollbar images are complete bars (16 px thick in original themes);
    // arrows never stretch, so their extents are the track's 9-slice caps
    // along the scroll axis (caps order: left, top, right, bottom).
    let (thickness, arrow_a, arrow_b) = match track_img {
        Some(img) => {
            let [w, h] = img.size();
            let caps = img.caps.map(f32::from);
            if horizontal {
                (h as f32, caps[0].max(4.0), caps[2].max(4.0))
            } else {
                (w as f32, caps[1].max(4.0), caps[3].max(4.0))
            }
        }
        None => (16.0, 26.0, 26.0),
    };

    let axis = |p: egui::Pos2| if horizontal { p.x } else { p.y };
    let span_min = axis(rect.min) + arrow_a;
    let span_max = axis(rect.max) - arrow_b;
    let span_len = (span_max - span_min).max(0.0);
    let visible = visible.clamp(0.01, 1.0);
    let thumb_len = (span_len * visible).clamp(12.0_f32.min(span_len), span_len);
    let travel = span_len - thumb_len;
    let thumb_start = span_min + *value * travel;

    // -- interaction --------------------------------------------------------
    let id = resp.id;
    let zone_id = id.with("zone");
    let repeat_id = id.with("repeat");
    let line_step = (visible * 0.25).max(0.02);
    let page_step = visible.max(0.05);

    if enabled && (resp.drag_started() || resp.clicked()) {
        if let Some(pos) = resp.interact_pointer_pos() {
            let p = axis(pos);
            let zone = if p < span_min {
                if double_arrows && p >= span_min - arrow_a / 2.0 {
                    ScrollZone::Inc
                } else {
                    ScrollZone::Dec
                }
            } else if p > span_max {
                if double_arrows && p <= span_max + arrow_b / 2.0 {
                    ScrollZone::Dec
                } else {
                    ScrollZone::Inc
                }
            } else if p >= thumb_start && p <= thumb_start + thumb_len {
                ScrollZone::Thumb(p - thumb_start)
            } else if p < thumb_start {
                ScrollZone::PageDec
            } else {
                ScrollZone::PageInc
            };
            ui.data_mut(|d| {
                d.insert_temp(zone_id, zone);
                d.insert_temp(repeat_id, f64::NEG_INFINITY);
            });
        }
    }

    let pointer_down = resp.is_pointer_button_down_on() || resp.dragged();
    let zone = ui.data(|d| d.get_temp::<ScrollZone>(zone_id));
    if enabled && pointer_down {
        if let Some(zone) = zone {
            let old = *value;
            match zone {
                ScrollZone::None => {}
                ScrollZone::Thumb(grab) => {
                    if travel > 0.0 {
                        if let Some(pos) = resp.interact_pointer_pos() {
                            *value = ((axis(pos) - grab - span_min) / travel).clamp(0.0, 1.0);
                        }
                    }
                }
                ScrollZone::Dec | ScrollZone::Inc | ScrollZone::PageDec | ScrollZone::PageInc => {
                    let now = ui.input(|i| i.time);
                    let last = ui
                        .data(|d| d.get_temp::<f64>(repeat_id))
                        .unwrap_or(f64::NEG_INFINITY);
                    if now - last >= 0.15 {
                        let step = match zone {
                            ScrollZone::Dec => -line_step,
                            ScrollZone::Inc => line_step,
                            ScrollZone::PageDec => -page_step,
                            ScrollZone::PageInc => page_step,
                            ScrollZone::None | ScrollZone::Thumb(_) => 0.0,
                        };
                        *value = (*value + step).clamp(0.0, 1.0);
                        ui.data_mut(|d| d.insert_temp(repeat_id, now));
                        ui.ctx().request_repaint();
                    } else {
                        ui.ctx().request_repaint();
                    }
                }
            }
            if *value != old {
                resp.mark_changed();
            }
        }
    } else {
        ui.data_mut(|d| d.remove_temp::<ScrollZone>(zone_id));
    }

    // -- painting -----------------------------------------------------------
    match (skin.get(track_slot), track_img) {
        (Some(tex), Some(img)) => nine_slice(ui.painter(), tex, img, rect, Color32::WHITE),
        _ => {
            // KDX Standard style: dark track with black border and grey
            // arrow triangles at both ends.
            let c = theme.colors;
            ui.painter().rect(
                rect,
                0.0,
                c.primary_background,
                (1.0, Color32::BLACK),
                StrokeKind::Inside,
            );
            let arrow_color = if enabled {
                c.disabled_text
            } else {
                c.primary_dark
            };
            let tri = |center: egui::Pos2, dir: Vec2| {
                let side = Vec2::new(-dir.y, dir.x);
                ui.painter().add(egui::Shape::convex_polygon(
                    vec![
                        center + dir * 3.5,
                        center - dir * 2.5 + side * 4.0,
                        center - dir * 2.5 - side * 4.0,
                    ],
                    arrow_color,
                    egui::Stroke::NONE,
                ));
            };
            // A dec+inc arrow pair at each end, like KDX Standard.
            let (dec_dir, inc_dir) = if horizontal {
                (Vec2::new(-1.0, 0.0), Vec2::new(1.0, 0.0))
            } else {
                (Vec2::new(0.0, -1.0), Vec2::new(0.0, 1.0))
            };
            let at = |d: f32| {
                if horizontal {
                    egui::pos2(rect.left() + d, rect.center().y)
                } else {
                    egui::pos2(rect.center().x, rect.top() + d)
                }
            };
            let len = if horizontal {
                rect.width()
            } else {
                rect.height()
            };
            tri(at(arrow_a * 0.25), dec_dir);
            tri(at(arrow_a * 0.75), inc_dir);
            tri(at(len - arrow_b * 0.75), dec_dir);
            tri(at(len - arrow_b * 0.25), inc_dir);
        }
    }

    if enabled {
        let dragging = matches!(zone, Some(ScrollZone::Thumb(_))) && pointer_down;
        let thumb_slot = if dragging {
            thumb_hilited
        } else {
            thumb_normal
        };
        let thumb_start = span_min + *value * travel;
        let thumb_rect = if horizontal {
            Rect::from_min_size(
                egui::pos2(thumb_start, rect.top()),
                Vec2::new(thumb_len, thickness),
            )
        } else {
            Rect::from_min_size(
                egui::pos2(rect.left(), thumb_start),
                Vec2::new(thickness, thumb_len),
            )
        };
        let grips_slot = if dragging {
            if horizontal {
                Slot::HScrollGripsHilited
            } else {
                Slot::VScrollGripsHilited
            }
        } else if horizontal {
            Slot::HScrollGripsNormal
        } else {
            Slot::VScrollGripsNormal
        };
        match (skin.get(thumb_slot), theme.image(thumb_slot)) {
            (Some(tex), Some(img)) => {
                nine_slice(ui.painter(), tex, img, thumb_rect, Color32::WHITE);
                if let (Some(gtex), Some(gimg)) = (skin.get(grips_slot), theme.image(grips_slot)) {
                    let [gw, gh] = gimg.size();
                    let fits = if horizontal {
                        thumb_rect.width() >= gw as f32 + 4.0
                    } else {
                        thumb_rect.height() >= gh as f32 + 4.0
                    };
                    if fits {
                        natural(ui.painter(), gtex, gimg, thumb_rect, Color32::WHITE);
                    }
                }
            }
            _ => {
                // KDX Standard style: grey thumb with black border and grip
                // lines across the middle.
                let c = theme.colors;
                ui.painter().rect(
                    thumb_rect.shrink(1.0),
                    0.0,
                    c.primary_dark,
                    (1.0, Color32::BLACK),
                    StrokeKind::Inside,
                );
                let mid = thumb_rect.center();
                for i in -1..=1 {
                    let o = i as f32 * 3.0;
                    let (a, b) = if horizontal {
                        (
                            egui::pos2(mid.x + o, thumb_rect.top() + 4.0),
                            egui::pos2(mid.x + o, thumb_rect.bottom() - 4.0),
                        )
                    } else {
                        (
                            egui::pos2(thumb_rect.left() + 4.0, mid.y + o),
                            egui::pos2(thumb_rect.right() - 4.0, mid.y + o),
                        )
                    };
                    ui.painter().line_segment([a, b], (1.0, c.disabled_text));
                }
            }
        }
    }
    resp
}

/// KDX popup button: a drop-down menu of mutually exclusive choices, drawn
/// from the theme's popup body and symbol textures with a themed menu list.
pub fn popup_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    selected: &mut usize,
    items: &[&str],
    width: f32,
    enabled: bool,
) -> Response {
    let font = crate::fonts::ui_font();
    let (rect, mut resp) = ui.allocate_exact_size(Vec2::new(width, 22.0), Sense::click());
    let popup_id = resp.id.with("menu");
    let open = ui.memory(|m| m.is_popup_open(popup_id));

    let body_slot = if !enabled {
        Slot::PopupButtonDisabled
    } else if open || resp.is_pointer_button_down_on() {
        Slot::PopupButtonHilited
    } else {
        Slot::PopupButtonNormal
    };
    let text_inset = match (skin.get(body_slot), theme.image(body_slot)) {
        (Some(tex), Some(img)) => {
            nine_slice(ui.painter(), tex, img, rect, Color32::WHITE);
            img.cap_left().max(8.0)
        }
        _ => {
            fallback_bevel(ui, rect, theme, open);
            8.0
        }
    };

    let symbol_slot = if !enabled {
        Slot::PopupSymbolDisabled
    } else if open || resp.is_pointer_button_down_on() {
        Slot::PopupSymbolHilited
    } else {
        Slot::PopupSymbolNormal
    };
    match (skin.get(symbol_slot), theme.image(symbol_slot)) {
        (Some(tex), Some(img)) => {
            // Symbol anchoring: a nonzero left position wins over right;
            // top wins over bottom; with no vertical position the symbol is
            // vertically centered.
            let [w, h] = img.size();
            let (w, h) = (w as f32, h as f32);
            let x = if img.pos_left() > 0.0 {
                rect.left() + img.pos_left()
            } else if img.pos_right() > 0.0 {
                rect.right() - img.pos_right() - w
            } else {
                rect.right() - 4.0 - w
            };
            let y = if img.pos_top() > 0.0 {
                rect.top() + img.pos_top()
            } else if img.pos_bottom() > 0.0 {
                rect.bottom() - img.pos_bottom() - h
            } else {
                rect.center().y - h / 2.0
            };
            crate::paint::natural_at(ui.painter(), tex, img, egui::pos2(x, y), Color32::WHITE);
        }
        _ => {
            ui.painter().text(
                egui::pos2(rect.right() - 12.0, rect.center().y),
                Align2::CENTER_CENTER,
                "▾",
                font.clone(),
                theme.colors.text,
            );
        }
    }

    let text_color = if enabled {
        theme.colors.text
    } else {
        theme.colors.disabled_text
    };
    if let Some(label) = items.get(*selected) {
        ui.painter().text(
            egui::pos2(rect.left() + text_inset, rect.center().y),
            Align2::LEFT_CENTER,
            label,
            font.clone(),
            text_color,
        );
    }

    if enabled && resp.clicked() {
        ui.memory_mut(|m| m.toggle_popup(popup_id));
    }

    if open {
        let c = theme.colors;
        let row_h = 20.0;
        let area = egui::Area::new(popup_id)
            .order(egui::Order::Foreground)
            .fixed_pos(rect.left_bottom())
            .show(ui.ctx(), |ui| {
                egui::Frame::new()
                    .fill(c.primary_background)
                    .stroke((1.0, c.text))
                    .inner_margin(egui::Margin::same(1))
                    .show(ui, |ui| {
                        ui.set_width(width - 2.0);
                        ui.spacing_mut().item_spacing.y = 0.0;
                        for (i, label) in items.iter().enumerate() {
                            let (row, row_resp) = ui.allocate_exact_size(
                                Vec2::new(ui.available_width(), row_h),
                                Sense::click(),
                            );
                            let hovered = row_resp.hovered();
                            if hovered {
                                ui.painter().rect_filled(row, 0.0, c.selection);
                            }
                            let color = if hovered { c.selection_text } else { c.text };
                            let mark = if i == *selected { "✓ " } else { "" };
                            ui.painter().text(
                                egui::pos2(row.left() + 6.0, row.center().y),
                                Align2::LEFT_CENTER,
                                format!("{mark}{label}"),
                                font.clone(),
                                color,
                            );
                            if row_resp.clicked() {
                                if *selected != i {
                                    *selected = i;
                                    resp.mark_changed();
                                }
                                ui.memory_mut(|m| m.close_popup());
                            }
                        }
                    });
            });
        if area.response.clicked_elsewhere() && !resp.clicked() {
            ui.memory_mut(|m| m.close_popup());
        }
    }
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
        (Some(tex), Some(img)) => {
            // The indicator's top position places it that many pixels above
            // the top of the bar.
            let w = img.size()[0] as f32;
            let bar_h = theme
                .image(bar_slot)
                .map(|b| b.size()[1] as f32)
                .unwrap_or(8.0);
            let bar_top = rect.center().y - bar_h / 2.0;
            let top = bar_top - img.pos_top();
            crate::paint::natural_at(
                ui.painter(),
                tex,
                img,
                egui::pos2(x - w / 2.0, top.max(rect.top())),
                Color32::WHITE,
            );
        }
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
