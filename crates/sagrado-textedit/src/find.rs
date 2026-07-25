//! Find / Replace panel state and rendering.

use eframe::egui::{self, Ui};
use sagrado_theme::Theme;
use sagrado_ui::{widgets, SkinTextures};

#[derive(Default)]
pub struct FindState {
    pub open: bool,
    pub query: String,
    pub replace: String,
    pub case_sensitive: bool,
}

/// Draw the Find / Replace bar. Returns a command id (`find.next`,
/// `find.replace`, `find.replace_all`) when a button is pressed.
pub fn panel(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    state: &mut FindState,
) -> Option<String> {
    let c = theme.colors;
    let mut command = None;
    egui::Frame::new()
        .fill(c.primary_background)
        .stroke((1.0, c.primary_dark))
        .inner_margin(egui::Margin::same(6))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.colored_label(c.text, "Find");
                widgets::text_box(ui, theme, &mut state.query, 160.0);
                if widgets::push_button(ui, theme, skin, "Next", true, false).clicked() {
                    command = Some("find.next".to_owned());
                }
            });
            ui.horizontal(|ui| {
                ui.colored_label(c.text, "Repl");
                widgets::text_box(ui, theme, &mut state.replace, 160.0);
                if widgets::push_button(ui, theme, skin, "Replace", true, false).clicked() {
                    command = Some("find.replace".to_owned());
                }
                if widgets::push_button(ui, theme, skin, "All", true, false).clicked() {
                    command = Some("find.replace_all".to_owned());
                }
            });
            if widgets::tick_button(
                ui,
                theme,
                skin,
                &mut state.case_sensitive,
                "Case sensitive",
                true,
            )
            .changed()
            {
                // no-op; state already updated
            }
        });
    command
}
