//! Importer for Haxial Appearance (`.hap`) files.
//!
//! The format was reverse-engineered from original appearance files and the
//! AppearanceEdit documentation. All integers are big-endian.
//!
//! ```text
//! 0x00  "%HAP" magic, u32 version (0x00010000)
//! 0x2c  section table: 4 x (offset: u32, length: u32)
//!         0: info      (engine version, checksums, metadata strings)
//!         1: images    (u32 offset table, then image records)
//!         2: colors    (204 x u32 0x00RRGGBB)
//!         3: icons
//! ```
//!
//! Image record:
//! ```text
//! u16 width, u16 height
//! u16 flags_bpp   (high byte: bit0 = transparency active; low byte: bpp 1/2/4/8)
//! u8  max_palette_index, u8 transparent_palette_index
//! u32 transparent color (0x00RRGGBB, informational)
//! u8[4] caps      (left, top, right, bottom)
//! u8[4] positions (left, top, right, bottom)
//! u32[max_palette_index+1] palette
//! rows of packed indices, each row padded to a 4-byte boundary
//! ```

use egui::{Color32, ColorImage};

use crate::theme::{SkinImage, Theme, ThemeColors, COLOR_TABLE_LEN};

#[derive(Debug, thiserror::Error)]
pub enum HapError {
    #[error("not a .hap file (bad magic)")]
    BadMagic,
    #[error("unsupported .hap version {0:#x}")]
    BadVersion(u32),
    #[error("truncated or corrupt .hap file")]
    Truncated,
    #[error("unsupported image bit depth {0}")]
    BadDepth(u8),
}

struct Reader<'a>(&'a [u8]);

impl<'a> Reader<'a> {
    fn u16(&self, o: usize) -> Result<u16, HapError> {
        let b = self.0.get(o..o + 2).ok_or(HapError::Truncated)?;
        Ok(u16::from_be_bytes([b[0], b[1]]))
    }
    fn u32(&self, o: usize) -> Result<u32, HapError> {
        let b = self.0.get(o..o + 4).ok_or(HapError::Truncated)?;
        Ok(u32::from_be_bytes([b[0], b[1], b[2], b[3]]))
    }
    fn bytes(&self, o: usize, n: usize) -> Result<&'a [u8], HapError> {
        self.0.get(o..o + n).ok_or(HapError::Truncated)
    }
}

fn rgb(v: u32) -> Color32 {
    Color32::from_rgb((v >> 16) as u8, (v >> 8) as u8, v as u8)
}

/// Parse a `.hap` file into a [`Theme`].
pub fn parse(data: &[u8], fallback_name: &str) -> Result<Theme, HapError> {
    let r = Reader(data);
    if data.len() < 0x90 || &data[0..4] != b"%HAP" {
        return Err(HapError::BadMagic);
    }
    let version = r.u32(4)?;
    if version != 0x0001_0000 {
        return Err(HapError::BadVersion(version));
    }

    let info_off = r.u32(0x2c)? as usize;
    let info_len = r.u32(0x30)? as usize;
    let img_off = r.u32(0x34)? as usize;
    let img_len = r.u32(0x38)? as usize;
    let col_off = r.u32(0x3c)? as usize;
    let col_len = r.u32(0x40)? as usize;

    let mut theme = Theme::new(fallback_name);
    parse_info(&r, info_off, info_len, &mut theme);

    let n_colors = (col_len / 4).min(COLOR_TABLE_LEN);
    let mut table = Vec::with_capacity(n_colors);
    for i in 0..n_colors {
        table.push(rgb(r.u32(col_off + 4 * i)?));
    }
    theme.colors = ThemeColors::from_table(&table);
    theme.color_table = table;

    if img_len > 0 {
        parse_images(&r, img_off, &mut theme)?;
    }
    Ok(theme)
}

fn parse_info(r: &Reader<'_>, off: usize, len: usize, theme: &mut Theme) {
    // Info layout: 2 x (u32 engine version, u32 checksum), 16 reserved bytes,
    // 4 string lengths (u8: name, version, creator, description) padded to
    // 16 bytes, then the concatenated strings.
    let mut parse = || -> Result<(), HapError> {
        let lens = r.bytes(off + 0x22, 4)?.to_vec();
        let mut s = off + 0x34;
        let mut fields: [String; 4] = Default::default();
        for (i, &l) in lens.iter().enumerate() {
            let raw = r.bytes(s, l as usize)?;
            fields[i] = String::from_utf8_lossy(raw).into_owned();
            s += l as usize;
        }
        if s > off + len {
            return Err(HapError::Truncated);
        }
        let [name, version, creator, description] = fields;
        if !name.is_empty() {
            theme.name = name;
        }
        theme.version = version;
        theme.creator = creator;
        theme.description = description;
        Ok(())
    };
    // Metadata is optional; a malformed info block should not fail the import.
    let _ = parse();
}

fn parse_images(r: &Reader<'_>, base: usize, theme: &mut Theme) -> Result<(), HapError> {
    // The image section starts with a table of u32 offsets (relative to the
    // section start), one per slot; 0 means the slot has no image. The table
    // runs up to the first image record.
    let mut offsets = Vec::new();
    let mut first_record = usize::MAX;
    let mut i = 0;
    while 4 * i < first_record {
        let v = r.u32(base + 4 * i)? as usize;
        if v != 0 && v < first_record {
            first_record = v;
        }
        offsets.push(v);
        i += 1;
        if first_record == usize::MAX && i > 4096 {
            return Err(HapError::Truncated);
        }
    }
    for (slot, rel) in offsets.into_iter().enumerate() {
        if rel == 0 {
            continue;
        }
        let img = parse_image_record(r, base + rel)?;
        theme.insert_image(slot, img);
    }
    Ok(())
}

fn parse_image_record(r: &Reader<'_>, o: usize) -> Result<SkinImage, HapError> {
    let w = r.u16(o)? as usize;
    let h = r.u16(o + 2)? as usize;
    let flags_bpp = r.u16(o + 4)?;
    let transparent_active = flags_bpp & 0x0100 != 0;
    let bpp = (flags_bpp & 0xff) as u8;
    if !matches!(bpp, 1 | 2 | 4 | 8) {
        return Err(HapError::BadDepth(bpp));
    }
    let palette_len = r.bytes(o + 6, 1)?[0] as usize + 1;
    let transparent_index = r.bytes(o + 7, 1)?[0] as usize;
    let caps: [u8; 4] = r.bytes(o + 12, 4)?.try_into().unwrap();
    let positions: [u8; 4] = r.bytes(o + 16, 4)?.try_into().unwrap();

    let mut palette = Vec::with_capacity(palette_len);
    for i in 0..palette_len {
        palette.push(r.u32(o + 20 + 4 * i)? & 0x00ff_ffff);
    }
    let pixels_off = o + 20 + 4 * palette_len;
    let stride = (w * bpp as usize).div_ceil(32) * 4;

    let mut image = ColorImage::new([w, h], Color32::TRANSPARENT);
    for y in 0..h {
        let row = r.bytes(pixels_off + y * stride, stride)?;
        for x in 0..w {
            let idx = match bpp {
                8 => row[x] as usize,
                4 => ((row[x / 2] >> (4 * (1 - x % 2))) & 0xf) as usize,
                2 => ((row[x / 4] >> (6 - 2 * (x % 4))) & 0x3) as usize,
                _ => ((row[x / 8] >> (7 - x % 8)) & 0x1) as usize,
            };
            image[(x, y)] = if transparent_active && idx == transparent_index {
                Color32::TRANSPARENT
            } else {
                rgb(palette.get(idx).copied().unwrap_or(0))
            };
        }
    }
    Ok(SkinImage {
        image,
        caps,
        positions,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rejects_bad_magic() {
        assert!(matches!(parse(&[0u8; 256], "x"), Err(HapError::BadMagic)));
    }
}
