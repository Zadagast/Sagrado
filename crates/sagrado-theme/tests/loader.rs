use std::path::Path;

use sagrado_theme::{Color, SlotId, SlotState, Theme};

#[test]
fn hex_color_round_trip() {
    let color = Color::parse_hex("#12aBcDef").expect("valid color");
    assert_eq!(color.to_hex(), "#12abcdef");
    assert_eq!(Color::parse_hex(&color.to_hex()), Ok(color));
    assert_eq!(Color::parse_hex("#123456").expect("valid color").alpha, 255);
}

fn fixture(name: &str) -> &'static Path {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("tests/fixtures")
        .join(name)
        .leak()
}

#[test]
fn loads_fixture_colors_caps_and_png() {
    let theme = Theme::load_from_dir(fixture("minimal")).expect("fixture should load");
    assert_eq!(theme.colors.text, Color::rgb(0x11, 0x22, 0x33));
    assert_eq!(theme.colors.selection, Color::rgba(0xaa, 0xbb, 0xcc, 0xdd));
    let button = theme.slots.get(&SlotId::Button).expect("button slot");
    assert_eq!(button.caps.left, 1);
    assert_eq!(button.caps.top, 2);
    assert_eq!(button.caps.right, 3);
    assert_eq!(button.caps.bottom, 4);
    let image = button.images.get(&SlotState::Normal).expect("normal image");
    assert_eq!((image.width, image.height), (4, 4));
    assert_eq!(image.rgba.len(), 4 * 4 * 4);
}

#[test]
fn loads_colors_only_theme_with_defaults_and_no_slots() {
    let theme = Theme::load_from_dir(fixture("colors-only")).expect("fixture should load");
    assert_eq!(
        theme.colors.primary_background,
        Color::rgb(0x12, 0x34, 0x56)
    );
    assert_eq!(theme.colors.text, Color::rgb(0, 0, 0));
    assert!(theme.slots.is_empty());
}
