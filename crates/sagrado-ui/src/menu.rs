//! Menu bar and document tab bar, drawn in the KDX style from theme colors.

use eframe::egui::{self, Align2, Rect, Sense, Ui, Vec2};
use sagrado_theme::Theme;

use crate::fonts;
use crate::paint::SkinTextures;

/// One entry in a pull-down menu.
pub enum Item {
    /// A selectable command identified by `id`, shown as `label`.
    Entry {
        id: String,
        label: String,
        enabled: bool,
        checked: bool,
    },
    /// A horizontal divider between groups of entries.
    Separator,
}

impl Item {
    pub fn new(id: impl Into<String>, label: impl Into<String>) -> Self {
        Item::Entry {
            id: id.into(),
            label: label.into(),
            enabled: true,
            checked: false,
        }
    }
    pub fn disabled(id: impl Into<String>, label: impl Into<String>) -> Self {
        Item::Entry {
            id: id.into(),
            label: label.into(),
            enabled: false,
            checked: false,
        }
    }
    pub fn checked(id: impl Into<String>, label: impl Into<String>, checked: bool) -> Self {
        Item::Entry {
            id: id.into(),
            label: label.into(),
            enabled: true,
            checked,
        }
    }
}

/// A titled pull-down menu in the menu bar.
pub struct Menu {
    pub title: String,
    pub items: Vec<Item>,
}

impl Menu {
    pub fn new(title: impl Into<String>, items: Vec<Item>) -> Self {
        Self {
            title: title.into(),
            items,
        }
    }
}

/// Draw a KDX-style menu bar and return the `id` of any command chosen this
/// frame. Menu titles highlight on hover and open a themed pull-down.
pub fn menu_bar(ui: &mut Ui, theme: &Theme, menus: &[Menu]) -> Option<String> {
    let c = theme.colors;
    let font = fonts::ui_font();
    let bar_h = 26.0;
    let (bar, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), bar_h), Sense::hover());
    ui.painter().rect_filled(bar, 0.0, c.primary_background);

    // Index of the currently-open menu, self-managed so we control closing.
    let state_id = ui.id().with("menubar_open");
    let mut open_menu: Option<usize> = ui.memory(|m| m.data.get_temp(state_id)).flatten();

    let mut chosen = None;
    let mut x = bar.left() + 10.0;
    // Compute title rects first so we can react to clicks after rendering.
    for (mi, menu) in menus.iter().enumerate() {
        let text_w = ui
            .painter()
            .layout_no_wrap(menu.title.clone(), font.clone(), c.text)
            .size()
            .x;
        let title_rect =
            Rect::from_min_size(egui::pos2(x, bar.top()), Vec2::new(text_w + 16.0, bar_h));
        let resp = ui.interact(title_rect, state_id.with(("title", mi)), Sense::click());
        if resp.clicked() {
            open_menu = if open_menu == Some(mi) {
                None
            } else {
                Some(mi)
            };
        } else if resp.hovered() && open_menu.is_some() {
            // Slide between menus while the bar is active.
            open_menu = Some(mi);
        }
        let active = open_menu == Some(mi);
        if active || resp.hovered() {
            ui.painter().rect_filled(title_rect, 0.0, c.selection);
        }
        let label_color = if active || resp.hovered() {
            c.selection_text
        } else {
            c.text
        };
        ui.painter().text(
            title_rect.center(),
            Align2::CENTER_CENTER,
            &menu.title,
            font.clone(),
            label_color,
        );

        if active {
            match show_dropdown(
                ui,
                theme,
                state_id.with(("pop", mi)),
                title_rect.left_bottom(),
                &menu.items,
            ) {
                DropdownResult::Chosen(id) => {
                    chosen = Some(id);
                    open_menu = None;
                }
                DropdownResult::ClickedOutside => open_menu = None,
                DropdownResult::None => {}
            }
        }
        x += title_rect.width() + 6.0;
    }

    ui.memory_mut(|m| m.data.insert_temp(state_id, open_menu));
    if open_menu.is_some() {
        ui.ctx().request_repaint();
    }
    chosen
}

enum DropdownResult {
    Chosen(String),
    ClickedOutside,
    None,
}

fn show_dropdown(
    ui: &mut Ui,
    theme: &Theme,
    popup_id: egui::Id,
    pos: egui::Pos2,
    items: &[Item],
) -> DropdownResult {
    let c = theme.colors;
    let font = fonts::ui_font();
    let row_h = 22.0;
    let mut chosen = None;
    let width = items
        .iter()
        .map(|it| match it {
            Item::Entry { label, .. } => {
                ui.painter()
                    .layout_no_wrap(label.clone(), font.clone(), c.text)
                    .size()
                    .x
                    + 36.0
            }
            Item::Separator => 0.0,
        })
        .fold(120.0_f32, f32::max);

    let area = egui::Area::new(popup_id)
        .order(egui::Order::Foreground)
        .fixed_pos(pos)
        .show(ui.ctx(), |ui| {
            egui::Frame::new()
                .fill(c.primary_background)
                .stroke((1.0, c.text))
                .inner_margin(egui::Margin::same(1))
                .show(ui, |ui| {
                    ui.set_width(width);
                    ui.spacing_mut().item_spacing.y = 0.0;
                    for item in items {
                        match item {
                            Item::Separator => {
                                let (r, _) = ui.allocate_exact_size(
                                    Vec2::new(ui.available_width(), 5.0),
                                    Sense::hover(),
                                );
                                ui.painter().line_segment(
                                    [
                                        egui::pos2(r.left() + 3.0, r.center().y),
                                        egui::pos2(r.right() - 3.0, r.center().y),
                                    ],
                                    (1.0, c.primary_dark),
                                );
                            }
                            Item::Entry {
                                id,
                                label,
                                enabled,
                                checked,
                            } => {
                                let (row, row_resp) = ui.allocate_exact_size(
                                    Vec2::new(ui.available_width(), row_h),
                                    if *enabled {
                                        Sense::click()
                                    } else {
                                        Sense::hover()
                                    },
                                );
                                let hovered = *enabled && row_resp.hovered();
                                if hovered {
                                    ui.painter().rect_filled(row, 0.0, c.selection);
                                }
                                let color = if !*enabled {
                                    c.disabled_text
                                } else if hovered {
                                    c.selection_text
                                } else {
                                    c.text
                                };
                                if *checked {
                                    ui.painter().text(
                                        egui::pos2(row.left() + 6.0, row.center().y),
                                        Align2::LEFT_CENTER,
                                        "✓",
                                        font.clone(),
                                        color,
                                    );
                                }
                                ui.painter().text(
                                    egui::pos2(row.left() + 22.0, row.center().y),
                                    Align2::LEFT_CENTER,
                                    label,
                                    font.clone(),
                                    color,
                                );
                                if row_resp.clicked() {
                                    chosen = Some(id.clone());
                                }
                            }
                        }
                    }
                });
        });
    if let Some(id) = chosen {
        return DropdownResult::Chosen(id);
    }
    // Close if a click landed outside both the dropdown and the menu bar.
    let clicked = ui.input(|i| i.pointer.any_click());
    if clicked && !area.response.hovered() {
        let over_bar = ui
            .input(|i| i.pointer.interact_pos())
            .map(|p| p.y < pos.y)
            .unwrap_or(false);
        if !over_bar {
            return DropdownResult::ClickedOutside;
        }
    }
    DropdownResult::None
}

/// What the user did to a document tab this frame.
pub enum TabEvent {
    Select(usize),
    Close(usize),
}

/// Draw a row of document tabs (icon + name), KDX-style. The selected tab is
/// filled with the selection color. Middle-click or the small ✕ closes a tab.
pub fn doc_tabs(
    ui: &mut Ui,
    theme: &Theme,
    _skin: &SkinTextures,
    tabs: &[String],
    selected: usize,
) -> Option<TabEvent> {
    let c = theme.colors;
    let font = fonts::ui_font();
    let tab_h = 30.0;
    let mut event = None;
    ui.horizontal(|ui| {
        ui.spacing_mut().item_spacing.x = 4.0;
        for (i, name) in tabs.iter().enumerate() {
            let text_w = ui
                .painter()
                .layout_no_wrap(name.clone(), font.clone(), c.text)
                .size()
                .x;
            let w = text_w + 44.0;
            let (rect, resp) = ui.allocate_exact_size(Vec2::new(w, tab_h), Sense::click());
            let active = i == selected;
            let p = ui.painter();
            let bg = if active {
                c.selection
            } else {
                c.primary_background
            };
            p.rect_filled(rect, 0.0, bg);
            // Raised bevel: light on top/left, dark on bottom/right.
            let (tl, br) = (c.primary_light, c.primary_dark);
            p.line_segment([rect.left_top(), rect.right_top()], (2.0, tl));
            p.line_segment([rect.left_top(), rect.left_bottom()], (2.0, tl));
            p.line_segment([rect.left_bottom(), rect.right_bottom()], (2.0, br));
            p.line_segment([rect.right_top(), rect.right_bottom()], (2.0, br));
            let fg = if active { c.selection_text } else { c.text };
            // Document glyph.
            p.text(
                egui::pos2(rect.left() + 8.0, rect.center().y),
                Align2::LEFT_CENTER,
                "▤",
                font.clone(),
                fg,
            );
            p.text(
                egui::pos2(rect.left() + 26.0, rect.center().y),
                Align2::LEFT_CENTER,
                name,
                font.clone(),
                fg,
            );
            if resp.middle_clicked() {
                event = Some(TabEvent::Close(i));
            } else if resp.clicked() {
                event = Some(TabEvent::Select(i));
            }
        }
    });
    event
}
