#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod document;
mod editor;
mod find;

use std::path::PathBuf;

use eframe::egui;
use sagrado_theme::Theme;
use sagrado_ui::menu::{self, Item, Menu, TabEvent};
use sagrado_ui::{chrome, paint};

use document::Document;
use find::FindState;

fn main() -> eframe::Result {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([760.0, 520.0])
            .with_min_inner_size([420.0, 300.0])
            .with_decorations(false)
            .with_title("Sagrado TextEdit"),
        ..Default::default()
    };
    eframe::run_native(
        "Sagrado TextEdit",
        options,
        Box::new(|cc| Ok(Box::new(App::new(cc)))),
    )
}

fn appearance_dirs() -> Vec<PathBuf> {
    let mut dirs = Vec::new();
    if let Ok(cwd) = std::env::current_dir() {
        dirs.push(cwd.join("themes/Appearances"));
        dirs.push(cwd.join("Appearances"));
    }
    if let Some(dir) = std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|d| d.to_owned()))
    {
        dirs.push(dir.join("Appearances"));
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
            if let Ok(theme) = std::fs::read(&path)
                .map_err(|e| e.to_string())
                .and_then(|data| sagrado_theme::hap::parse(&data, &name).map_err(|e| e.to_string()))
            {
                themes.push(theme);
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
    current_theme: usize,
    skin: paint::SkinTextures,
    docs: Vec<Document>,
    current: usize,
    favorites: Vec<PathBuf>,
    find: FindState,
    /// A selection to apply to the editor on the next frame (Find result).
    pending_select: Option<(usize, usize)>,
    caret: usize,
    status: String,
}

impl App {
    fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        let mut themes = load_appearances();
        if themes.is_empty() {
            themes.push(Theme::new("Built-in"));
        }
        Self {
            themes,
            current_theme: 0,
            skin: paint::SkinTextures::default(),
            docs: vec![Document::sample()],
            current: 0,
            favorites: Vec::new(),
            find: FindState::default(),
            pending_select: None,
            caret: 0,
            status: String::new(),
        }
    }

    fn doc(&self) -> &Document {
        &self.docs[self.current]
    }

    fn open_path(&mut self, path: PathBuf) {
        if let Some(i) = self
            .docs
            .iter()
            .position(|d| d.path.as_ref() == Some(&path))
        {
            self.current = i;
            return;
        }
        match Document::from_path(path.clone()) {
            Ok(doc) => {
                self.docs.push(doc);
                self.current = self.docs.len() - 1;
                self.status = format!("Opened {}", path.display());
            }
            Err(e) => self.status = format!("Could not open {}: {e}", path.display()),
        }
    }

    fn save_current(&mut self, save_as: bool) {
        let needs_dialog = save_as || self.doc().path.is_none();
        let target = if needs_dialog {
            let mut dlg = rfd::FileDialog::new().add_filter("Text", &["txt"]);
            if let Some(name) = self.doc().path.as_ref().and_then(|p| p.file_name()) {
                dlg = dlg.set_file_name(name.to_string_lossy());
            }
            dlg.save_file()
        } else {
            self.doc().path.clone()
        };
        let Some(path) = target else { return };
        match self.docs[self.current].save_to(&path) {
            Ok(()) => self.status = format!("Saved {}", path.display()),
            Err(e) => self.status = format!("Save failed: {e}"),
        }
    }

    fn close_tab(&mut self, i: usize) {
        if i >= self.docs.len() {
            return;
        }
        self.docs.remove(i);
        if self.docs.is_empty() {
            self.docs.push(Document::default());
        }
        self.current = self.current.min(self.docs.len() - 1);
    }

    fn handle_command(&mut self, ctx: &egui::Context, id: &str) {
        match id {
            "file.new" => {
                self.docs.push(Document::default());
                self.current = self.docs.len() - 1;
            }
            "file.open" => {
                if let Some(path) = rfd::FileDialog::new()
                    .add_filter("Text", &["txt"])
                    .pick_file()
                {
                    self.open_path(path);
                }
            }
            "file.save" => self.save_current(false),
            "file.save_as" => self.save_current(true),
            "file.close" => self.close_tab(self.current),
            "file.quit" => ctx.send_viewport_cmd(egui::ViewportCommand::Close),
            "tools.find" => self.find.open = !self.find.open,
            "tools.count" => {
                let n = document::count_occurrences(
                    &self.doc().text,
                    &self.find.query,
                    self.find.case_sensitive,
                );
                self.status = if self.find.query.is_empty() {
                    "Enter search text in Find first".to_owned()
                } else {
                    format!("{n} occurrence(s) of \"{}\"", self.find.query)
                };
            }
            "tools.sort" => {
                self.docs[self.current].sort_lines();
                self.status = "Sorted lines".to_owned();
            }
            "tools.wrap" => {
                let w = !self.doc().soft_wrap;
                self.docs[self.current].soft_wrap = w;
                self.status = format!("Soft wrap {}", if w { "on" } else { "off" });
            }
            "fav.add" => {
                if let Some(p) = self.doc().path.clone() {
                    if !self.favorites.contains(&p) {
                        self.favorites.push(p);
                    }
                } else {
                    self.status = "Save the document before adding to Favorites".to_owned();
                }
            }
            "loc.copy" => {
                if let Some(p) = self.doc().path.clone() {
                    ctx.copy_text(p.display().to_string());
                    self.status = "Path copied".to_owned();
                }
            }
            "loc.reveal" => {
                if let Some(dir) = self.doc().path.as_ref().and_then(|p| p.parent()) {
                    let _ = open_in_file_manager(dir);
                }
            }
            _ => {
                if let Some(rest) = id.strip_prefix("fav:") {
                    if let Ok(i) = rest.parse::<usize>() {
                        if let Some(p) = self.favorites.get(i).cloned() {
                            self.open_path(p);
                        }
                    }
                } else if let Some(rest) = id.strip_prefix("theme:") {
                    if let Ok(i) = rest.parse::<usize>() {
                        self.current_theme = i.min(self.themes.len() - 1);
                    }
                }
            }
        }
    }

    fn menus(&self) -> Vec<Menu> {
        let wrap = self.doc().soft_wrap;
        let mut favs = vec![Item::new("fav.add", "Add Current to Favorites")];
        if !self.favorites.is_empty() {
            favs.push(Item::Separator);
            for (i, p) in self.favorites.iter().enumerate() {
                let name = p
                    .file_name()
                    .map(|n| n.to_string_lossy().into_owned())
                    .unwrap_or_default();
                favs.push(Item::new(format!("fav:{i}"), name));
            }
        }
        let mut themes = Vec::new();
        for (i, t) in self.themes.iter().enumerate() {
            themes.push(Item::checked(
                format!("theme:{i}"),
                t.name.clone(),
                i == self.current_theme,
            ));
        }
        vec![
            Menu::new(
                "File",
                vec![
                    Item::new("file.new", "New"),
                    Item::new("file.open", "Open…"),
                    Item::Separator,
                    Item::new("file.save", "Save"),
                    Item::new("file.save_as", "Save As…"),
                    Item::Separator,
                    Item::new("file.close", "Close Tab"),
                    Item::new("file.quit", "Quit"),
                ],
            ),
            Menu::new(
                "Tools",
                vec![
                    Item::new("tools.find", "Find / Replace…"),
                    Item::new("tools.count", "Count Occurrences"),
                    Item::Separator,
                    Item::new("tools.sort", "Sort Lines"),
                    Item::checked("tools.wrap", "Soft Wrap", wrap),
                ],
            ),
            Menu::new("Favorites", favs),
            Menu::new(
                "Location",
                vec![
                    Item::new("loc.copy", "Copy Path"),
                    Item::new("loc.reveal", "Show Containing Folder"),
                ],
            ),
            Menu::new("Appearance", themes),
        ]
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.skin.set_theme(ctx, &self.themes[self.current_theme]);

        // Keyboard shortcuts.
        let mut shortcut: Option<&str> = None;
        ctx.input_mut(|i| {
            use egui::{Key, Modifiers};
            let cmd = Modifiers::COMMAND;
            let cmd_shift = Modifiers::COMMAND | Modifiers::SHIFT;
            if i.consume_key(cmd, Key::N) {
                shortcut = Some("file.new");
            } else if i.consume_key(cmd, Key::O) {
                shortcut = Some("file.open");
            } else if i.consume_key(cmd_shift, Key::S) {
                shortcut = Some("file.save_as");
            } else if i.consume_key(cmd, Key::S) {
                shortcut = Some("file.save");
            } else if i.consume_key(cmd, Key::W) {
                shortcut = Some("file.close");
            } else if i.consume_key(cmd, Key::F) {
                shortcut = Some("tools.find");
            }
        });
        if let Some(id) = shortcut {
            self.handle_command(ctx, id);
        }

        let theme = self.themes[self.current_theme].clone();
        let title = format!("TE: {}", self.doc().display_name());

        let menus = self.menus();
        let mut command: Option<String> = None;
        let tabs: Vec<String> = self.docs.iter().map(|d| d.display_name()).collect();
        let mut tab_event: Option<TabEvent> = None;
        let current = self.current;
        let pending = self.pending_select.take();
        let skin_ref = &self.skin;
        let colors = theme.colors;

        chrome::window_frame(ctx, &theme, skin_ref, &title, |ui| {
            if let Some(id) = menu::menu_bar(ui, &theme, &menus) {
                command = Some(id);
            }
            ui.add_space(2.0);
            tab_event = menu::doc_tabs(ui, &theme, skin_ref, &tabs, current);
            ui.add_space(2.0);

            if self.find.open {
                if let Some(id) = find::panel(ui, &theme, skin_ref, &mut self.find) {
                    command = Some(id);
                }
            }

            // Editor fills the remaining space.
            let caret = editor::editor(ui, &theme, &mut self.docs[current], pending);
            self.caret = caret;

            // Status line.
            if !self.status.is_empty() {
                ui.horizontal(|ui| {
                    ui.colored_label(colors.disabled_text, &self.status);
                });
            }
        });

        if let Some(ev) = tab_event {
            match ev {
                TabEvent::Select(i) => self.current = i,
                TabEvent::Close(i) => self.close_tab(i),
            }
        }
        if let Some(id) = command {
            match id.as_str() {
                "find.next" => self.do_find(false),
                "find.replace" => self.do_replace(),
                "find.replace_all" => self.do_replace_all(),
                other => self.handle_command(ctx, other),
            }
        }
    }
}

impl App {
    fn do_find(&mut self, _backward: bool) {
        let from = self.caret;
        if let Some((s, e)) = document::find_from(
            &self.doc().text,
            &self.find.query,
            from,
            self.find.case_sensitive,
        ) {
            self.pending_select = Some((s, e));
            self.status = String::new();
        } else if !self.find.query.is_empty() {
            self.status = format!("\"{}\" not found", self.find.query);
        }
    }

    fn do_replace(&mut self) {
        // Replace the current selection if it matches, then find the next.
        let range = document::find_from(
            &self.doc().text,
            &self.find.query,
            self.caret.saturating_sub(self.find.query.len()),
            self.find.case_sensitive,
        );
        if let Some((s, e)) = range {
            self.docs[self.current]
                .text
                .replace_range(s..e, &self.find.replace);
            self.caret = s + self.find.replace.len();
        }
        self.do_find(false);
    }

    fn do_replace_all(&mut self) {
        if self.find.query.is_empty() {
            return;
        }
        let text = &self.docs[self.current].text;
        let n = document::count_occurrences(text, &self.find.query, self.find.case_sensitive);
        let replaced = if self.find.case_sensitive {
            text.replace(&self.find.query, &self.find.replace)
        } else {
            replace_case_insensitive(text, &self.find.query, &self.find.replace)
        };
        self.docs[self.current].text = replaced;
        self.status = format!("Replaced {n} occurrence(s)");
    }
}

fn replace_case_insensitive(text: &str, from: &str, to: &str) -> String {
    let lower = text.to_lowercase();
    let needle = from.to_lowercase();
    let mut out = String::with_capacity(text.len());
    let mut i = 0;
    while i < text.len() {
        if lower[i..].starts_with(&needle) {
            out.push_str(to);
            i += from.len();
        } else {
            let ch = text[i..].chars().next().unwrap();
            out.push(ch);
            i += ch.len_utf8();
        }
    }
    out
}

fn open_in_file_manager(dir: &std::path::Path) -> std::io::Result<()> {
    #[cfg(target_os = "linux")]
    let program = "xdg-open";
    #[cfg(target_os = "macos")]
    let program = "open";
    #[cfg(target_os = "windows")]
    let program = "explorer";
    std::process::Command::new(program)
        .arg(dir)
        .spawn()
        .map(|_| ())
}
