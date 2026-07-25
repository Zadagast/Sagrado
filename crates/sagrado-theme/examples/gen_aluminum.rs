use std::path::PathBuf;

use image::{Rgba, RgbaImage};

#[derive(Clone, Copy)]
enum AssetKind {
    Button,
    DefaultButton,
    TextBox,
    Check { checked: bool },
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
    let images = root.join("themes/Aluminum/images");
    std::fs::create_dir_all(&images)?;

    let assets = [
        ("button", 96, 24, AssetKind::Button),
        ("default_button", 96, 24, AssetKind::DefaultButton),
        ("popup_button", 96, 24, AssetKind::Button),
        ("tick_blank", 20, 20, AssetKind::Check { checked: false }),
        ("tick_ticked", 20, 20, AssetKind::Check { checked: true }),
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
        if matches!(kind, AssetKind::Check { .. } | AssetKind::Radio) {
            let mut disabled = image;
            for pixel in disabled.pixels_mut() {
                let gray = (u16::from(pixel[0]) + u16::from(pixel[1]) + u16::from(pixel[2])) / 3;
                pixel[0] = (u16::from(pixel[0]) * 2 + gray) as u8 / 3;
                pixel[1] = (u16::from(pixel[1]) * 2 + gray) as u8 / 3;
                pixel[2] = (u16::from(pixel[2]) * 2 + gray) as u8 / 3;
                pixel[3] = 190;
            }
            disabled.save(images.join(format!("{name}-disabled.png")))?;
        }
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
                AssetKind::DefaultButton => gradient([250, 250, 250], [188, 188, 188], t),
                AssetKind::ProgressFill => gradient([186, 242, 177], [52, 157, 71], t),
                AssetKind::TextBox => [255, 255, 255, 255],
                AssetKind::ScrollThumb => gradient([207, 247, 198], [67, 164, 79], t),
                AssetKind::Separator => [207, 207, 207, 255],
                AssetKind::Panel | AssetKind::WindowFrame => {
                    let mut metal = gradient([247, 247, 247], [202, 202, 202], t);
                    if y % 4 == 1 {
                        metal[0] = metal[0].saturating_sub(4);
                        metal[1] = metal[1].saturating_sub(4);
                        metal[2] = metal[2].saturating_sub(4);
                    }
                    metal
                }
                AssetKind::Check { .. } | AssetKind::Radio => [245, 245, 245, 255],
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
                color = [53, 137, 67, 255];
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
            if matches!(kind, AssetKind::Check { checked: true })
                && checkmark_pixel(x, y, width, height)
            {
                color = [54, 156, 68, 255];
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

fn checkmark_pixel(x: u32, y: u32, width: u32, height: u32) -> bool {
    let x = x as i32;
    let y = y as i32;
    let width = width as i32;
    let height = height as i32;
    line_pixel(x, y, width / 4, height / 2, width / 2, height * 3 / 4)
        || line_pixel(x, y, width / 2, height * 3 / 4, width * 3 / 4, height / 3)
}

fn line_pixel(x: i32, y: i32, x0: i32, y0: i32, x1: i32, y1: i32) -> bool {
    let dx = x1 - x0;
    let dy = y1 - y0;
    let length = (dx * dx + dy * dy) as f32;
    let t = (((x - x0) * dx + (y - y0) * dy) as f32 / length).clamp(0.0, 1.0);
    let px = x0 as f32 + t * dx as f32;
    let py = y0 as f32 + t * dy as f32;
    (x as f32 - px).abs() <= 1.5 && (y as f32 - py).abs() <= 1.5
}
