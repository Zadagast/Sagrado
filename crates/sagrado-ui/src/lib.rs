//! Slint widgets backed by the runtime-loaded Sagrado theme renderer.
//!
//! Components ask the `ThemeRuntime` global for a finished RGBA image. The
//! Rust callback performs nine-slice composition or color fallback before
//! converting the result to a `slint::Image`.

slint::include_modules!();

use std::sync::{Arc, Mutex};

use sagrado_theme::{
    render::{fallback_widget, nine_slice, RgbaBuffer},
    SlotId, SlotState, Theme,
};
use slint::{Image, Rgba8Pixel, SharedPixelBuffer};

/// A thread-safe runtime theme renderer used by the Slint callback bridge.
#[derive(Clone)]
pub struct ThemeRenderer {
    theme: Arc<Mutex<Theme>>,
}

impl ThemeRenderer {
    /// Creates a renderer for a loaded theme.
    pub fn new(theme: Theme) -> Self {
        Self {
            theme: Arc::new(Mutex::new(theme)),
        }
    }

    /// Replaces the active theme while keeping the renderer handle stable.
    pub fn set_theme(&self, theme: Theme) {
        if let Ok(mut active) = self.theme.lock() {
            *active = theme;
        }
    }

    /// Renders a slot into a Slint image at the requested pixel dimensions.
    pub fn render(&self, slot_name: &str, width: u32, height: u32, state: i32) -> Image {
        let Ok(theme) = self.theme.lock() else {
            return image_from_buffer(RgbaBuffer {
                rgba: Vec::new(),
                width: 0,
                height: 0,
            });
        };
        let slot_state = match state {
            1 => SlotState::Hilited,
            2 => SlotState::Disabled,
            3 => SlotState::Focus,
            _ => SlotState::Normal,
        };
        let Some(slot_id) = slot_name.parse::<SlotId>().ok() else {
            return image_from_buffer(fallback_widget(
                &theme.colors,
                SlotId::Box,
                slot_state,
                width,
                height,
            ));
        };
        let buffer = theme
            .slots
            .get(&slot_id)
            .and_then(|slot| {
                slot.images
                    .get(&slot_state)
                    .or_else(|| slot.images.get(&SlotState::Normal))
                    .map(|image| nine_slice(image, slot.caps, width, height))
            })
            .unwrap_or_else(|| fallback_widget(&theme.colors, slot_id, slot_state, width, height));
        image_from_buffer(buffer)
    }

    /// Resolves a named theme color for Slint text and accents.
    pub fn color(&self, name: &str) -> slint::Color {
        let Ok(theme) = self.theme.lock() else {
            return slint::Color::from_rgb_u8(0, 0, 0);
        };
        let color = match name {
            "primary-light" => theme.colors.primary_light,
            "primary-background" => theme.colors.primary_background,
            "primary-dark" => theme.colors.primary_dark,
            "primary-frame" => theme.colors.primary_frame,
            "selection" => theme.colors.selection,
            "selection-text" => theme.colors.selection_text,
            "text-box-background" => theme.colors.text_box_background,
            "menu-background" => theme.colors.menu_background,
            "menu-text" => theme.colors.menu_text,
            "alert" => theme.colors.alert,
            "alert-text" => theme.colors.alert_text,
            _ => theme.colors.text,
        };
        slint::Color::from_argb_u8(color.alpha, color.red, color.green, color.blue)
    }
}

/// Installs a theme renderer on a generated Slint component.
pub fn install_theme_renderer(component: &PreviewGallery, renderer: ThemeRenderer) {
    let global = component.global::<ThemeRuntime>();
    let image_renderer = renderer.clone();
    global.on_render(move |slot, width, height, state, _revision| {
        image_renderer.render(&slot, width.max(0) as u32, height.max(0) as u32, state)
    });
    global.on_color(move |name, _revision| renderer.color(&name));
}

fn image_from_buffer(buffer: RgbaBuffer) -> Image {
    if buffer.width == 0 || buffer.height == 0 {
        return Image::default();
    }
    let pixels = buffer
        .rgba
        .chunks_exact(4)
        .map(|pixel| Rgba8Pixel {
            r: pixel[0],
            g: pixel[1],
            b: pixel[2],
            a: pixel[3],
        })
        .collect::<Vec<_>>();
    let mut shared = SharedPixelBuffer::new(buffer.width, buffer.height);
    shared.make_mut_slice().copy_from_slice(&pixels);
    Image::from_rgba8(shared)
}
