//! Theme model: named colors + 9-slice widget textures.

use std::collections::HashMap;

use egui::{Color32, ColorImage};

use crate::slots::Slot;

/// Number of entries in the appearance color table (matches .hap).
pub const COLOR_TABLE_LEN: usize = 204;

/// A widget texture with 9-slice caps and auxiliary position values,
/// exactly as authored in AppearanceEdit.
#[derive(Clone)]
pub struct SkinImage {
    pub image: ColorImage,
    /// 9-slice caps in pixels: left, right, top, bottom.
    pub caps: [u8; 4],
    /// Auxiliary positions (meaning depends on the slot): left, right, top, bottom.
    pub positions: [u8; 4],
}

impl SkinImage {
    pub fn size(&self) -> [usize; 2] {
        self.image.size
    }
}

/// Semantic colors resolved from the appearance color table.
///
/// Index assignments were derived empirically from original .hap files.
#[derive(Clone, Copy)]
pub struct ThemeColors {
    pub primary_light: Color32,
    pub primary_background: Color32,
    pub primary_dark: Color32,
    pub text: Color32,
    pub selection: Color32,
    pub selection_text: Color32,
    pub text_box_background: Color32,
    pub alert: Color32,
}

impl Default for ThemeColors {
    fn default() -> Self {
        Self {
            primary_light: Color32::from_gray(0xee),
            primary_background: Color32::from_gray(0xcc),
            primary_dark: Color32::from_gray(0x88),
            text: Color32::BLACK,
            selection: Color32::from_rgb(0x29, 0x68, 0xc8),
            selection_text: Color32::WHITE,
            text_box_background: Color32::WHITE,
            alert: Color32::from_rgb(0xff, 0x00, 0x00),
        }
    }
}

impl ThemeColors {
    pub fn from_table(table: &[Color32]) -> Self {
        let c = |i: usize| table.get(i).copied().unwrap_or(Color32::BLACK);
        Self {
            primary_light: c(1),
            primary_background: c(2),
            primary_dark: c(3),
            text: c(0),
            selection: c(12),
            selection_text: c(13),
            text_box_background: c(15),
            alert: c(8),
        }
    }
}

/// A complete appearance: metadata, the full color table, semantic colors,
/// and 9-slice textures for every widget slot the theme provides.
#[derive(Clone)]
pub struct Theme {
    pub name: String,
    pub version: String,
    pub creator: String,
    pub description: String,
    pub color_table: Vec<Color32>,
    pub colors: ThemeColors,
    images: HashMap<usize, SkinImage>,
}

impl Theme {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            version: String::new(),
            creator: String::new(),
            description: String::new(),
            color_table: vec![Color32::BLACK; COLOR_TABLE_LEN],
            colors: ThemeColors::default(),
            images: HashMap::new(),
        }
    }

    pub fn insert_image(&mut self, slot_index: usize, image: SkinImage) {
        self.images.insert(slot_index, image);
    }

    pub fn image(&self, slot: Slot) -> Option<&SkinImage> {
        self.images.get(&slot.index())
    }

    pub fn image_by_index(&self, slot_index: usize) -> Option<&SkinImage> {
        self.images.get(&slot_index)
    }

    pub fn image_count(&self) -> usize {
        self.images.len()
    }

    pub fn slot_indices(&self) -> impl Iterator<Item = usize> + '_ {
        self.images.keys().copied()
    }
}
