//! UI-independent pixel compositing for themed widgets.
//!
//! Nine-slice compositing keeps the four cap regions fixed and stretches the
//! edge and center regions to the requested size. Stretching (rather than
//! tiling) is deliberate: it keeps runtime rendering deterministic for
//! arbitrary widget sizes and lets a renderer upload one finished RGBA buffer.

use crate::{Caps, Color, SlotId, SlotImage, SlotState, ThemeColors};

/// A renderer-independent RGBA8 pixel buffer.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RgbaBuffer {
    pub rgba: Vec<u8>,
    pub width: u32,
    pub height: u32,
}

/// Stretches a slot image using its nine-slice caps.
pub fn nine_slice(
    image: &SlotImage,
    caps: Caps,
    target_width: u32,
    target_height: u32,
) -> RgbaBuffer {
    let mut output = RgbaBuffer {
        rgba: vec![0; target_width.saturating_mul(target_height).saturating_mul(4) as usize],
        width: target_width,
        height: target_height,
    };
    if image.width == 0 || image.height == 0 || target_width == 0 || target_height == 0 {
        return output;
    }

    let source_left = caps.left.min(image.width);
    let source_right = caps.right.min(image.width.saturating_sub(source_left));
    let source_top = caps.top.min(image.height);
    let source_bottom = caps.bottom.min(image.height.saturating_sub(source_top));
    let left = source_left.min(target_width);
    let right = source_right.min(target_width.saturating_sub(left));
    let top = source_top.min(target_height);
    let bottom = source_bottom.min(target_height.saturating_sub(top));

    for y in 0..target_height {
        let source_y = map_axis(
            y,
            target_height,
            top,
            bottom,
            image.height,
            source_top,
            source_bottom,
        );
        for x in 0..target_width {
            let source_x = map_axis(
                x,
                target_width,
                left,
                right,
                image.width,
                source_left,
                source_right,
            );
            let source_index = ((source_y * image.width + source_x) * 4) as usize;
            let output_index = ((y * target_width + x) * 4) as usize;
            output.rgba[output_index..output_index + 4]
                .copy_from_slice(&image.rgba[source_index..source_index + 4]);
        }
    }
    output
}

/// How a slot with no image should be synthesized from the color table.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum FallbackKind {
    /// A raised control: vertical gradient, dark border, light top highlight.
    Raised,
    /// A sunken well: near-white fill with an inset (dark top/left) border.
    Recessed,
    /// A saturated progress/selection fill.
    Accent,
    /// A flat panel with a single-pixel border.
    Panel,
    /// A thin engraved separator line, centered in its box.
    Separator,
}

impl FallbackKind {
    fn of(slot: SlotId) -> Self {
        use SlotId::*;
        match slot {
            Button
            | DefaultButton
            | IconButton
            | PopupButton
            | PopupButtonNoTitle
            | ColumnHeader
            | HSliderIndicator
            | VSliderIndicator
            | HSliderPointedIndicator
            | VSliderPointedIndicator
            | HScrollBarThumb
            | VScrollBarThumb
            | WindowClose
            | WindowMinimize
            | WindowMaximize
            | WindowMenu
            | WindowResize
            | MenuBarTitle
            | PlusMinus => Self::Raised,
            TextBox | FocusBox | ProgressBar | HSliderBar | VSliderBar | HScrollBarTrack
            | VScrollBarTrack | TickBlank | MutexBlank | TickTristated | MutexTristated => {
                Self::Recessed
            }
            ProgressFill | TickTicked | MutexTicked => Self::Accent,
            HorizSeparator | VertSeparator | MenuSeparator => Self::Separator,
            _ => Self::Panel,
        }
    }

    fn corner_radius(self) -> u32 {
        match self {
            Self::Raised => 2,
            Self::Recessed | Self::Accent => 1,
            _ => 0,
        }
    }
}

/// Paints a skeuomorphic fallback when a slot has no image: a vertical
/// gradient with beveled edges synthesized from the theme's color table, so
/// even a colors-only theme reads as a real 3D widget kit rather than flat
/// rectangles.
pub fn fallback_widget(
    colors: &ThemeColors,
    slot: SlotId,
    state: SlotState,
    width: u32,
    height: u32,
) -> RgbaBuffer {
    let mut output = RgbaBuffer {
        rgba: vec![0; width.saturating_mul(height).saturating_mul(4) as usize],
        width,
        height,
    };
    if width == 0 || height == 0 {
        return output;
    }
    let kind = FallbackKind::of(slot);
    let radius = kind.corner_radius().min(width / 2).min(height / 2);

    let (mut top, mut bottom) = match kind {
        FallbackKind::Raised => (
            blend(colors.primary_background, colors.primary_light, 0.75),
            blend(colors.primary_background, colors.primary_dark, 0.35),
        ),
        FallbackKind::Recessed => (
            colors.text_box_background,
            blend(colors.text_box_background, colors.primary_background, 0.5),
        ),
        FallbackKind::Accent => (
            blend(colors.selection, colors.primary_light, 0.4),
            colors.selection,
        ),
        FallbackKind::Panel | FallbackKind::Separator => {
            (colors.primary_background, colors.primary_background)
        }
    };
    let mut frame = colors.primary_frame;
    let mut light = colors.primary_light;
    let mut dark = colors.primary_dark;
    let mut accent = colors.selection;
    match state {
        SlotState::Normal => {}
        SlotState::Hilited => {
            // Pressed controls read as inset: darken the fill and reverse the
            // bevel so the upper/left edge becomes the shadow.
            top = blend(top, colors.primary_dark, 0.28);
            bottom = blend(bottom, colors.primary_dark, 0.18);
            std::mem::swap(&mut light, &mut dark);
            frame = blend(frame, colors.primary_dark, 0.25);
        }
        SlotState::Disabled => {
            // Blend toward the panel background to desaturate without
            // requiring extra theme colors.
            let background = colors.primary_background;
            top = blend(top, background, 0.58);
            bottom = blend(bottom, background, 0.68);
            frame = blend(frame, background, 0.62);
            light = blend(light, background, 0.6);
            dark = blend(dark, background, 0.6);
            accent = blend(accent, background, 0.62);
            top.alpha = top.alpha.saturating_mul(3) / 4;
            bottom.alpha = bottom.alpha.saturating_mul(3) / 4;
        }
        SlotState::Focus => {
            // A restrained accent tint distinguishes keyboard focus while
            // preserving the normal raised/recessed treatment.
            top = blend(top, colors.selection, 0.12);
            bottom = blend(bottom, colors.selection, 0.08);
            frame = blend(frame, colors.selection, 0.42);
            accent = blend(accent, colors.selection, 0.2);
        }
    }

    let last_x = width - 1;
    let last_y = height - 1;
    for y in 0..height {
        let t = if height > 1 {
            y as f32 / last_y as f32
        } else {
            0.0
        };
        let base = blend(top, bottom, t);
        for x in 0..width {
            if in_clipped_corner(x, y, last_x, last_y, radius) {
                continue;
            }
            let on_top = y == 0;
            let on_bottom = y == last_y;
            let on_left = x == 0;
            let on_right = x == last_x;
            let edge = on_top || on_bottom || on_left || on_right;
            let color = match kind {
                FallbackKind::Separator => {
                    // Engrave a single centered groove line.
                    let mid_v = width > height && y == height / 2;
                    let mid_h = height >= width && x == width / 2;
                    if mid_v || mid_h {
                        colors.primary_dark
                    } else {
                        base
                    }
                }
                _ if edge && matches!(kind, FallbackKind::Recessed) => {
                    // Sunken: dark top/left, light bottom/right.
                    if on_top || on_left {
                        dark
                    } else {
                        light
                    }
                }
                _ if edge => frame,
                // First inner row on raised controls: bright highlight.
                _ if matches!(kind, FallbackKind::Raised) && y == 1 => blend(base, light, 0.6),
                _ if matches!(kind, FallbackKind::Accent) => accent,
                _ => base,
            };
            write_pixel(&mut output, x, y, color);
        }
    }
    output
}

/// Chamfers the four corners so raised/recessed controls read as rounded.
fn in_clipped_corner(x: u32, y: u32, last_x: u32, last_y: u32, radius: u32) -> bool {
    if radius == 0 {
        return false;
    }
    let dx = x.min(last_x - x);
    let dy = y.min(last_y - y);
    dx + dy < radius
}

/// Linear blend between two colors; `t` in `0.0..=1.0` moves toward `b`.
fn blend(a: Color, b: Color, t: f32) -> Color {
    let t = t.clamp(0.0, 1.0);
    let mix = |x: u8, y: u8| (x as f32 + (y as f32 - x as f32) * t).round() as u8;
    Color {
        red: mix(a.red, b.red),
        green: mix(a.green, b.green),
        blue: mix(a.blue, b.blue),
        alpha: mix(a.alpha, b.alpha),
    }
}

fn map_axis(
    position: u32,
    target_size: u32,
    target_start: u32,
    target_end: u32,
    source_size: u32,
    source_start: u32,
    source_end: u32,
) -> u32 {
    if position < target_start {
        return position.min(source_size.saturating_sub(1));
    }
    if position >= target_size.saturating_sub(target_end) {
        let distance = position.saturating_sub(target_size.saturating_sub(target_end));
        return source_size
            .saturating_sub(source_end)
            .saturating_add(distance)
            .min(source_size.saturating_sub(1));
    }
    let target_center = target_size.saturating_sub(target_start + target_end);
    let source_center = source_size.saturating_sub(source_start + source_end);
    if target_center == 0 || source_center == 0 {
        return source_start.min(source_size.saturating_sub(1));
    }
    source_start
        .saturating_add(
            position
                .saturating_sub(target_start)
                .saturating_mul(source_center)
                / target_center,
        )
        .min(source_size.saturating_sub(1))
}

fn write_pixel(buffer: &mut RgbaBuffer, x: u32, y: u32, color: Color) {
    let index = ((y * buffer.width + x) * 4) as usize;
    buffer.rgba[index..index + 4].copy_from_slice(&[
        color.red,
        color.green,
        color.blue,
        color.alpha,
    ]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nine_slice_scales_and_preserves_corners() {
        let mut rgba = Vec::new();
        for y in 0..4 {
            for x in 0..4 {
                rgba.extend_from_slice(&[x * 40, y * 40, 200, 255]);
            }
        }
        let image = SlotImage {
            rgba,
            width: 4,
            height: 4,
        };
        let scaled = nine_slice(
            &image,
            Caps {
                left: 1,
                top: 1,
                right: 1,
                bottom: 1,
            },
            8,
            8,
        );
        assert_eq!((scaled.width, scaled.height), (8, 8));
        assert_eq!(&scaled.rgba[0..4], &[0, 0, 200, 255]);
        let bottom_right = ((7 * 8 + 7) * 4) as usize;
        assert_eq!(
            &scaled.rgba[bottom_right..bottom_right + 4],
            &[120, 120, 200, 255]
        );
    }
}
