//! Sagrado UI toolkit: KDX-style skinned widgets, a 9-slice painter and
//! custom window chrome, all drawn from a [`sagrado_theme::Theme`]. Shared by
//! every Sagrado application so they render with the same appearance engine.

pub mod chrome;
pub mod fonts;
pub mod menu;
pub mod paint;
pub mod widgets;

pub use chrome::window_frame;
pub use paint::{natural, natural_at, nine_slice, SkinTextures};
