//! KDX-style "Preview GUI Items" panel: exercises every skinned widget so a
//! theme can be inspected at a glance.

use eframe::egui::{self, Align2, Color32, FontId, Rect, StrokeKind, Ui, Vec2};
use sagrado_theme::Theme;

use crate::paint::SkinTextures;
use crate::widgets;

pub struct PreviewState {
    text: String,
    ticked: bool,
    tick_disabled: bool,
    mutex_choice: usize,
    slider: f32,
    progress: f32,
    scroll_v: f32,
    scroll_h: f32,
}

impl Default for PreviewState {
    fn default() -> Self {
        Self {
            text: "text box".to_owned(),
            ticked: true,
            tick_disabled: false,
            mutex_choice: 0,
            slider: 0.35,
            progress: 0.5,
            scroll_v: 0.0,
            scroll_h: 0.3,
        }
    }
}

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
                self.sample_list(ui, theme, skin);
                widgets::scrollbar(ui, theme, skin, 190.0, &mut self.scroll_h, false, true);
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

    fn sample_list(&mut self, ui: &mut Ui, theme: &Theme, skin: &SkinTextures) {
        let c = theme.colors;
        ui.horizontal(|ui| {
            ui.spacing_mut().item_spacing.x = 0.0;
            widgets::column_header(ui, theme, skin, "Column 1", 110.0);
            widgets::column_header(ui, theme, skin, "Column 2", 80.0);
        });
        let rows = [
            ("an item", "123K"),
            ("an item", "123K"),
            ("an item", "123K"),
        ];
        let list_width = 175.0;
        let (rect, _) = ui.allocate_exact_size(Vec2::new(list_width, 66.0), egui::Sense::hover());
        let sb_rect = Rect::from_min_size(
            egui::pos2(rect.right(), rect.top()),
            Vec2::new(190.0 - list_width, 66.0),
        );
        let mut sb_ui = ui.new_child(
            egui::UiBuilder::new()
                .max_rect(sb_rect)
                .layout(egui::Layout::top_down(egui::Align::Min)),
        );
        widgets::scrollbar(
            &mut sb_ui,
            theme,
            skin,
            66.0,
            &mut self.scroll_v,
            true,
            true,
        );
        let p = ui.painter();
        p.rect(
            rect,
            0.0,
            c.text_box_background,
            (1.0, c.text),
            StrokeKind::Inside,
        );
        for (i, (a, b)) in rows.iter().enumerate() {
            let y = rect.top() + 4.0 + i as f32 * 20.0;
            if i == 0 {
                p.rect_filled(
                    Rect::from_min_size(
                        egui::pos2(rect.left() + 1.0, y - 3.0),
                        Vec2::new(rect.width() - 2.0, 20.0),
                    ),
                    0.0,
                    c.selection,
                );
                p.text(
                    egui::pos2(rect.left() + 6.0, y + 7.0),
                    Align2::LEFT_CENTER,
                    *a,
                    FontId::proportional(12.0),
                    c.selection_text,
                );
                p.text(
                    egui::pos2(rect.left() + 116.0, y + 7.0),
                    Align2::LEFT_CENTER,
                    *b,
                    FontId::proportional(12.0),
                    c.selection_text,
                );
            } else {
                p.text(
                    egui::pos2(rect.left() + 6.0, y + 7.0),
                    Align2::LEFT_CENTER,
                    *a,
                    FontId::proportional(12.0),
                    c.text,
                );
                p.text(
                    egui::pos2(rect.left() + 116.0, y + 7.0),
                    Align2::LEFT_CENTER,
                    *b,
                    FontId::proportional(12.0),
                    c.text,
                );
            }
        }
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
