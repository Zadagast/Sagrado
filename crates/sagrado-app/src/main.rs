use std::{
    collections::HashMap,
    env,
    path::{Path, PathBuf},
    rc::Rc,
};

use sagrado_theme::Theme;
use sagrado_ui::{install_theme_renderer, PreviewGallery, ThemeRenderer};
use slint::{ComponentHandle, SharedString, VecModel};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let themes_dir = repo_root().join("themes");
    let themes = load_themes(&themes_dir)?;
    if themes.is_empty() {
        return Err(format!("no themes found in {}", themes_dir.display()).into());
    }

    let first_name = themes.keys().next().cloned().unwrap_or_default();
    let active_theme = themes
        .get(&first_name)
        .cloned()
        .ok_or("first theme disappeared")?;
    let renderer = ThemeRenderer::new(active_theme);
    let gallery = PreviewGallery::new()?;
    install_theme_renderer(&gallery, renderer.clone());
    gallery.global::<sagrado_ui::ThemeRuntime>().set_revision(0);

    let names = themes
        .keys()
        .cloned()
        .map(SharedString::from)
        .collect::<Vec<_>>();
    gallery.set_theme_options(Rc::new(VecModel::from(names)).into());
    gallery.set_selected_theme(first_name.into());

    let weak_gallery = gallery.as_weak();
    gallery.on_theme_changed(move |name| {
        if let Some(theme) = themes.get(name.as_str()) {
            renderer.set_theme(theme.clone());
            if let Some(gallery) = weak_gallery.upgrade() {
                let revision = gallery.global::<sagrado_ui::ThemeRuntime>().get_revision();
                gallery
                    .global::<sagrado_ui::ThemeRuntime>()
                    .set_revision(revision.wrapping_add(1));
                gallery.set_selected_theme(name);
            }
        }
    });

    gallery.run()?;
    Ok(())
}

fn repo_root() -> PathBuf {
    env::var_os("SAGRADO_ROOT")
        .map(PathBuf::from)
        .unwrap_or_else(|| Path::new(env!("CARGO_MANIFEST_DIR")).join("../.."))
}

fn load_themes(themes_dir: &Path) -> Result<HashMap<String, Theme>, Box<dyn std::error::Error>> {
    let mut themes = HashMap::new();
    for entry in std::fs::read_dir(themes_dir)? {
        let entry = entry?;
        if entry.file_type()?.is_dir() {
            let name = entry.file_name().to_string_lossy().into_owned();
            themes.insert(name, Theme::load_from_dir(entry.path())?);
        }
    }
    Ok(themes)
}
