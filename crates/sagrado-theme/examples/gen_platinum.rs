use std::path::PathBuf;

use image::{Rgba, RgbaImage};

#[derive(Clone, Copy)]
enum AssetKind {
    Button,
    DefaultButton,
    TextBox,
    Check,
    Radio,
    Slider,
    Progress,
    ProgressFill,
    ScrollTrack,
    ScrollThumb,
    Separator,
    Panel,
    WindowFrame,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..");
    let images = root.join("themes/Platinum/images");
    std::fs::create_dir_all(&images)?;

    let assets = [
        ("button", 96, 24, AssetKind::Button),
        ("default_button", 96, 24, AssetKind::DefaultButton),
        ("popup_button", 96, 24, AssetKind::Button),
        ("tick_blank", 20, 20, AssetKind::Check),
        ("tick_ticked", 20, 20, AssetKind::Check),
        ("mutex_blank", 20, 20, AssetKind::Radio),
        ("mutex_ticked", 20, 20, AssetKind::Radio),
        ("text_box", 96, 24, AssetKind::TextBox),
        ("column_header", 96, 20, AssetKind::Button),
        ("box", 160, 48, AssetKind::Panel),
        ("h_slider_bar", 96, 8, AssetKind::Slider),
        ("h_slider_indicator", 16, 16, AssetKind::Button),
        ("progress_bar", 96, 14, AssetKind::Progress),
        ("progress_fill", 96, 14, AssetKind::ProgressFill),
        ("h_scrollbar_track", 96, 18, AssetKind::ScrollTrack),
        ("h_scrollbar_thumb", 32, 18, AssetKind::ScrollThumb),
        ("v_scrollbar_track", 18, 96, AssetKind::ScrollTrack),
        ("v_scrollbar_thumb", 18, 32, AssetKind::ScrollThumb),
        ("horiz_separator", 96, 2, AssetKind::Separator),
        ("vert_separator", 2, 96, AssetKind::Separator),
        ("menu_background", 160, 24, AssetKind::Panel),
        ("window_frame", 240, 96, AssetKind::WindowFrame),
    ];

    for (name, width, height, kind) in assets {
        let image = paint(width, height, kind);
        image.save(images.join(format!("{name}-normal.png")))?;
    }
    Ok(())
}

fn paint(width: u32, height: u32, kind: AssetKind) -> RgbaImage {
    let mut image = RgbaImage::new(width, height);
    for y in 0..height {
        for x in 0..width {
            let t = if height > 1 {
                y as f32 / (height - 1) as f32
            } else {
                0.0
            };
            let mut color = match kind {
                AssetKind::DefaultButton => gradient([227, 238, 255], [151, 184, 232], t),
                AssetKind::ProgressFill => gradient([129, 196, 255], [36, 112, 207], t),
                AssetKind::TextBox => [255, 255, 255, 255],
                AssetKind::ScrollThumb => gradient([250, 250, 250], [183, 183, 183], t),
                AssetKind::Separator => [207, 207, 207, 255],
                AssetKind::Panel | AssetKind::WindowFrame => {
                    gradient([238, 238, 238], [188, 188, 188], t)
                }
                AssetKind::Check | AssetKind::Radio => [245, 245, 245, 255],
                _ => gradient([250, 250, 250], [188, 188, 188], t),
            };

            let edge = x == 0 || y == 0 || x + 1 == width || y + 1 == height;
            if edge {
                color = [116, 116, 116, 255];
            }
            if matches!(kind, AssetKind::TextBox) && (x == 0 || y == 0) {
                color = [92, 92, 92, 255];
            }
            if matches!(kind, AssetKind::DefaultButton)
                && (x < 2 || y < 2 || x + 2 >= width || y + 2 >= height)
            {
                color = [52, 111, 191, 255];
            }
            if matches!(kind, AssetKind::WindowFrame) && y < 20 {
                color = if y % 2 == 0 {
                    [216, 216, 216, 255]
                } else {
                    [198, 198, 198, 255]
                };
            }
            if matches!(kind, AssetKind::ScrollThumb)
                && y > 4
                && y + 5 < height
                && x >= width / 2 - 4
                && x <= width / 2 + 4
                && y % 4 == 1
            {
                color = [126, 126, 126, 255];
            }
            if matches!(kind, AssetKind::Check) && (6..=13).contains(&x) && (6..=13).contains(&y) {
                color = [57, 109, 174, 255];
            }
            if matches!(kind, AssetKind::Radio) {
                let dx = x as i32 - width as i32 / 2;
                let dy = y as i32 - height as i32 / 2;
                if dx * dx + dy * dy < 16 {
                    color = [57, 109, 174, 255];
                }
            }
            if matches!(kind, AssetKind::Separator)
                && ((height == 2 && y == 1) || (width == 2 && x == 1))
            {
                color = [116, 116, 116, 255];
            }
            image.put_pixel(x, y, Rgba(color));
        }
    }
    image
}

fn gradient(top: [u8; 3], bottom: [u8; 3], t: f32) -> [u8; 4] {
    let channel = |a: u8, b: u8| (a as f32 + (b as f32 - a as f32) * t).round() as u8;
    [
        channel(top[0], bottom[0]),
        channel(top[1], bottom[1]),
        channel(top[2], bottom[2]),
        255,
    ]
}
