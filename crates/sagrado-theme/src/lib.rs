//! Sagrado Appearance Engine.
//!
//! A modern re-implementation of the theming system used by Haxial KDX:
//! every widget is drawn from theme-supplied 9-slice bitmap textures and a
//! table of named colors, loaded at runtime from appearance files.
//! Includes an importer for original Haxial `.hap` appearance files.

pub mod hap;
pub mod slots;
pub mod theme;

pub use slots::Slot;
pub use theme::{SkinImage, Theme, ThemeColors};
