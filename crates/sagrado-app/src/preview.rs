//! KDX-style "Preview GUI Items" panel: exercises every skinned widget so a
//! theme can be inspected at a glance.

use eframe::egui::{self, Align2, Color32, FontId, Rect, StrokeKind, Ui, Vec2};
use sagrado_theme::Theme;

use sagrado_ui::widgets;
use sagrado_ui::SkinTextures;

pub struct PreviewState {
    text: String,
    ticked: bool,
    tick_disabled: bool,
    mutex_choice: usize,
    popup_choice: usize,
    v_scroll: f32,
    h_scroll: f32,
    slider: f32,
    progress: f32,
}

impl Default for PreviewState {
    fn default() -> Self {
        Self {
            text: "text box".to_owned(),
            ticked: true,
            tick_disabled: false,
            mutex_choice: 0,
            popup_choice: 0,
            v_scroll: 0.0,
            h_scroll: 0.25,
            slider: 0.35,
            progress: 0.5,
        }
    }
}

const LIST_ITEMS: usize = 12;
const LIST_VISIBLE: usize = 4;

impl PreviewState {
    pub fn show(&mut self, ui: &mut Ui, theme: &Theme, skin: &SkinTextures) {
        let c = theme.colors;

        ui.horizontal(|ui| {
            ui.colored_label(c.text, "Primary Label");
            widgets::text_box(ui, theme, &mut self.text, 180.0);
        });
        ui.add_space(8.0);

        ui.columns(2, |cols| {
            cols[0].vertical(|ui| {
                self.scrolling_list(ui, theme, skin);
                ui.add_space(6.0);
                widgets::h_scrollbar(
                    ui,
                    theme,
                    skin,
                    &mut self.h_scroll,
                    190.0,
                    LIST_VISIBLE as f32 / LIST_ITEMS as f32,
                    true,
                );
                ui.add_space(10.0);
                widgets::separator(ui, theme, skin, 190.0);
                ui.add_space(6.0);
                widgets::progress_bar(ui, theme, skin, self.progress, 190.0);
                ui.add_space(6.0);
                if widgets::h_slider(ui, theme, skin, &mut self.slider, 190.0, true).changed() {
                    self.progress = self.slider;
                }
                widgets::h_slider(ui, theme, skin, &mut 0.6, 190.0, false);
            });
            cols[1].vertical(|ui| {
                if widgets::push_button(ui, theme, skin, "Button", true, false).clicked() {
                    self.progress = (self.progress + 0.1) % 1.0;
                }
                ui.add_space(4.0);
                widgets::push_button(ui, theme, skin, "Disabled", false, false);
                ui.add_space(10.0);
                widgets::push_button(ui, theme, skin, "Default", true, true);
                ui.add_space(10.0);
                widgets::tick_button(ui, theme, skin, &mut self.ticked, "Tick Button", true);
                widgets::tick_button(
                    ui,
                    theme,
                    skin,
                    &mut self.tick_disabled,
                    "Tick Disable",
                    false,
                );
                ui.add_space(8.0);
                for (i, label) in ["Mutex One", "Mutex Two"].iter().enumerate() {
                    if widgets::mutex_button(ui, theme, skin, self.mutex_choice == i, label, true)
                        .clicked()
                    {
                        self.mutex_choice = i;
                    }
                }
                ui.add_space(10.0);
                widgets::popup_button(
                    ui,
                    theme,
                    skin,
                    &mut self.popup_choice,
                    &["Popup Menu", "Second Item", "Third Item", "Fourth Item"],
                    130.0,
                    true,
                );
                ui.add_space(4.0);
                widgets::popup_button(ui, theme, skin, &mut 0, &["Disabled"], 130.0, false);
            });
        });

        ui.add_space(12.0);
        widgets::separator(ui, theme, skin, ui.available_width());
        ui.add_space(6.0);
        ui.colored_label(
            c.text,
            format!(
                "{} — {} colors, {} images",
                theme.name,
                theme.color_table.len(),
                theme.image_count(),
            ),
        );
        if !theme.description.is_empty() {
            ui.colored_label(c.primary_dark, &theme.description);
        }
        ui.add_space(4.0);
        self.color_swatches(ui, theme);
    }

    /// A fixed-height list of rows scrolled by the themed vertical scrollbar.
    fn scrolling_list(&mut self, ui: &mut Ui, theme: &Theme, skin: &SkinTextures) {
        let c = theme.colors;
        let row_h = 20.0;
        let box_h = LIST_VISIBLE as f32 * row_h + 2.0;
        ui.horizontal_top(|ui| {
            ui.spacing_mut().item_spacing.x = 0.0;
            let (rect, _) = ui.allocate_exact_size(Vec2::new(174.0, box_h), egui::Sense::hover());
            let p = ui.painter();
            p.rect(
                rect,
                0.0,
                c.text_box_background,
                (1.0, c.text),
                StrokeKind::Inside,
            );
            let first = (self.v_scroll * (LIST_ITEMS - LIST_VISIBLE) as f32).round() as usize;
            for row in 0..LIST_VISIBLE {
                let i = first + row;
                let y = rect.top() + 1.0 + row as f32 * row_h;
                let row_rect = Rect::from_min_size(
                    egui::pos2(rect.left() + 1.0, y),
                    Vec2::new(rect.width() - 2.0, row_h),
                );
                let selected = i == 0;
                if selected {
                    p.rect_filled(row_rect, 0.0, c.selection);
                }
                p.text(
                    egui::pos2(row_rect.left() + 5.0, row_rect.center().y),
                    Align2::LEFT_CENTER,
                    format!("List item {}", i + 1),
                    FontId::proportional(12.0),
                    if selected { c.selection_text } else { c.text },
                );
            }
            widgets::v_scrollbar(
                ui,
                theme,
                skin,
                &mut self.v_scroll,
                box_h,
                LIST_VISIBLE as f32 / LIST_ITEMS as f32,
                true,
            );
        });
    }

    fn color_swatches(&self, ui: &mut Ui, theme: &Theme) {
        let named: [(&str, Color32); 6] = [
            ("light", theme.colors.primary_light),
            ("background", theme.colors.primary_background),
            ("dark", theme.colors.primary_dark),
            ("text", theme.colors.text),
            ("selection", theme.colors.selection),
            ("alert", theme.colors.alert),
        ];
        ui.horizontal(|ui| {
            for (name, color) in named {
                let (rect, resp) = ui.allocate_exact_size(Vec2::splat(18.0), egui::Sense::hover());
                ui.painter().rect(
                    rect,
                    2.0,
                    color,
                    (1.0, theme.colors.text),
                    StrokeKind::Inside,
                );
                resp.on_hover_text(name);
            }
        });
    }
}
