//! Parses every bundled appearance file to guard the .hap importer.

use std::path::PathBuf;

/// Appearances that carry no widget bitmaps at all.
const COLOUR_ONLY: &[&str] = &["Haxial Standard"];

#[test]
fn parses_bundled_appearances() {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../themes/Appearances");
    let mut parsed = 0;
    for entry in std::fs::read_dir(dir).expect("themes/Appearances missing") {
        let path = entry.unwrap().path();
        if path.extension().and_then(|e| e.to_str()) != Some("hap") {
            continue;
        }
        let data = std::fs::read(&path).unwrap();
        let theme = sagrado_theme::hap::parse(&data, "test")
            .unwrap_or_else(|e| panic!("{}: {e}", path.display()));
        assert_eq!(
            theme.color_table.len(),
            sagrado_theme::theme::COLOR_TABLE_LEN
        );
        // Haxial's built-in "Standard" appearance was drawn natively in code:
        // its .hap has a zero-length image section and carries only the colour
        // table and icons. Every distributed appearance ships real widget art.
        let stem = path.file_stem().and_then(|s| s.to_str()).unwrap_or("");
        if COLOUR_ONLY.contains(&stem) {
            assert_eq!(
                theme.image_count(),
                0,
                "{}: expected a colour-only appearance",
                path.display()
            );
        } else {
            assert!(theme.image_count() > 0, "{}: no images", path.display());
        }
        parsed += 1;
    }
    assert!(parsed >= 1, "no .hap files found");
}
