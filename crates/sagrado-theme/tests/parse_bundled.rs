//! Parses every bundled appearance file to guard the .hap importer.

use std::path::PathBuf;

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
        assert!(theme.image_count() > 0, "{}: no images", path.display());
        parsed += 1;
    }
    assert!(parsed >= 1, "no .hap files found");
}
