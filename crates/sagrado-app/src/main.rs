#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod preview;

use sagrado_ui::{chrome, paint};

use std::path::PathBuf;

use eframe::egui;
use sagrado_theme::Theme;

fn main() -> eframe::Result {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([460.0, 560.0])
            .with_decorations(false)
            .with_title("Sagrado — Preview GUI Items"),
        ..Default::default()
    };
    eframe::run_native(
        "Sagrado",
        options,
        Box::new(|cc| Ok(Box::new(App::new(cc)))),
    )
}

/// Directories searched for appearance files, in order.
fn appearance_dirs() -> Vec<PathBuf> {
    let mut dirs = Vec::new();
    if let Ok(cwd) = std::env::current_dir() {
        dirs.push(cwd.join("themes/Appearances"));
        dirs.push(cwd.join("Appearances"));
    }
    if let Some(exe_dir) = std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|d| d.to_owned()))
    {
        dirs.push(exe_dir.join("Appearances"));
    }
    dirs
}

fn load_appearances() -> Vec<Theme> {
    let mut themes = Vec::new();
    for dir in appearance_dirs() {
        let Ok(entries) = std::fs::read_dir(&dir) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().and_then(|e| e.to_str()) != Some("hap") {
                continue;
            }
            let name = path
                .file_stem()
                .map(|s| s.to_string_lossy().into_owned())
                .unwrap_or_default();
            match std::fs::read(&path)
                .map_err(|e| e.to_string())
                .and_then(|data| sagrado_theme::hap::parse(&data, &name).map_err(|e| e.to_string()))
            {
                Ok(theme) => themes.push(theme),
                Err(err) => eprintln!("skipping {}: {err}", path.display()),
            }
        }
        if !themes.is_empty() {
            break;
        }
    }
    themes.sort_by(|a, b| a.name.cmp(&b.name));
    themes
}

struct App {
    themes: Vec<Theme>,
    current: usize,
    skin: paint::SkinTextures,
    preview: preview::PreviewState,
}

impl App {
    fn new(cc: &eframe::CreationContext<'_>) -> Self {
        sagrado_ui::fonts::install(&cc.egui_ctx);
        let mut themes = load_appearances();
        if themes.is_empty() {
            themes.push(Theme::new("Built-in"));
        }
        Self {
            themes,
            current: 0,
            skin: paint::SkinTextures::default(),
            preview: preview::PreviewState::default(),
        }
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let theme = &self.themes[self.current];
        self.skin.set_theme(ctx, theme);

        let colors = theme.colors;
        let mut current = self.current;
        let themes = &self.themes;
        let skin = &self.skin;
        let preview = &mut self.preview;
        chrome::window_frame(ctx, theme, skin, "Preview GUI Items", |ui| {
            egui::Frame::new()
                .fill(colors.primary_dark)
                .inner_margin(egui::Margin::symmetric(12, 6))
                .show(ui, |ui| {
                    ui.set_width(ui.available_width());
                    ui.horizontal(|ui| {
                        ui.colored_label(colors.selection_text, "Appearance:");
                        egui::ComboBox::from_id_salt("theme_picker")
                            .selected_text(&themes[current].name)
                            .show_ui(ui, |ui| {
                                for (i, t) in themes.iter().enumerate() {
                                    ui.selectable_value(&mut current, i, &t.name);
                                }
                            });
                        if !theme.creator.is_empty() {
                            ui.colored_label(
                                colors.selection_text,
                                format!("by {}", theme.creator),
                            );
                        }
                    });
                });

            egui::Frame::new()
                .inner_margin(egui::Margin::same(12))
                .show(ui, |ui| {
                    preview.show(ui, theme, skin);
                });
        });
        self.current = current;
    }
}
