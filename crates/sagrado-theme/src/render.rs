//! UI-independent pixel compositing for themed widgets.
//!
//! Nine-slice compositing keeps the four cap regions fixed and stretches the
//! edge and center regions to the requested size. Stretching (rather than
//! tiling) is deliberate: it keeps runtime rendering deterministic for
//! arbitrary widget sizes and lets a renderer upload one finished RGBA buffer.

use crate::{Caps, Color, SlotId, SlotImage, ThemeColors};

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

/// Paints a simple beveled fallback when a slot has no image.
pub fn fallback_widget(colors: &ThemeColors, _slot: SlotId, width: u32, height: u32) -> RgbaBuffer {
    let mut output = RgbaBuffer {
        rgba: vec![0; width.saturating_mul(height).saturating_mul(4) as usize],
        width,
        height,
    };
    for y in 0..height {
        for x in 0..width {
            let color = if x == 0 || y == 0 {
                colors.primary_light
            } else if x + 1 >= width || y + 1 >= height {
                colors.primary_dark
            } else {
                colors.primary_background
            };
            write_pixel(&mut output, x, y, color);
        }
    }
    output
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
