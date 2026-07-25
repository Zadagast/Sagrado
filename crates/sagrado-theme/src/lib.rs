//! A UI-agnostic theme format and loader for Sagrado.
//!
//! Themes are folders containing a `theme.toml` and optional PNG images. The
//! `image` value in a slot names its normal image; sibling state images are
//! discovered by replacing (or appending to) the normal image stem with
//! `-hilited`, `-disabled`, and `-focus`. For example, `button-normal.png`
//! discovers `button-hilited.png`. Missing state images are valid, while a
//! referenced normal image must exist.

pub mod render;

use std::{
    collections::HashMap,
    fmt, fs,
    path::{Path, PathBuf},
    str::FromStr,
};

use image::ImageError;
use serde::{Deserialize, Deserializer, Serializer};
use thiserror::Error;

/// An RGBA color with 8 bits per channel.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Color {
    /// Red channel.
    pub red: u8,
    /// Green channel.
    pub green: u8,
    /// Blue channel.
    pub blue: u8,
    /// Alpha channel.
    pub alpha: u8,
}

impl Color {
    /// Creates an opaque RGB color.
    pub const fn rgb(red: u8, green: u8, blue: u8) -> Self {
        Self {
            red,
            green,
            blue,
            alpha: 255,
        }
    }

    /// Creates an RGBA color.
    pub const fn rgba(red: u8, green: u8, blue: u8, alpha: u8) -> Self {
        Self {
            red,
            green,
            blue,
            alpha,
        }
    }

    /// Parses `#rrggbb` or `#rrggbbaa`.
    pub fn parse_hex(value: &str) -> Result<Self, ColorParseError> {
        let digits = value
            .strip_prefix('#')
            .ok_or(ColorParseError::MissingHash)?;
        if digits.len() != 6 && digits.len() != 8 {
            return Err(ColorParseError::InvalidLength(digits.len()));
        }
        let parse = |part: &str| {
            u8::from_str_radix(part, 16).map_err(|_| ColorParseError::InvalidHex(value.to_owned()))
        };
        Ok(Self {
            red: parse(&digits[0..2])?,
            green: parse(&digits[2..4])?,
            blue: parse(&digits[4..6])?,
            alpha: if digits.len() == 8 {
                parse(&digits[6..8])?
            } else {
                255
            },
        })
    }

    /// Returns `#rrggbb` for opaque colors and `#rrggbbaa` otherwise.
    pub fn to_hex(self) -> String {
        if self.alpha == 255 {
            format!("#{:02x}{:02x}{:02x}", self.red, self.green, self.blue)
        } else {
            format!(
                "#{:02x}{:02x}{:02x}{:02x}",
                self.red, self.green, self.blue, self.alpha
            )
        }
    }
}

impl Default for Color {
    fn default() -> Self {
        Self::rgb(0, 0, 0)
    }
}

impl fmt::Display for Color {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.to_hex())
    }
}

impl<'de> Deserialize<'de> for Color {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let value = String::deserialize(deserializer)?;
        Self::parse_hex(&value).map_err(serde::de::Error::custom)
    }
}

impl serde::Serialize for Color {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.to_hex())
    }
}

/// Errors returned when parsing a color string.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum ColorParseError {
    #[error("color must start with '#'")]
    MissingHash,
    #[error("color must contain 6 or 8 hexadecimal digits, got {0}")]
    InvalidLength(usize),
    #[error("color contains invalid hexadecimal digits: {0}")]
    InvalidHex(String),
}

/// The named colors used by themed widgets.
#[derive(Clone, Debug, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct ThemeColors {
    pub text: Color,
    pub primary_light: Color,
    pub primary_background: Color,
    pub primary_dark: Color,
    pub primary_frame: Color,
    pub selection: Color,
    pub selection_text: Color,
    pub text_box_background: Color,
    pub menu_background: Color,
    pub menu_text: Color,
    pub alert: Color,
    pub alert_text: Color,
}

impl Default for ThemeColors {
    fn default() -> Self {
        Self {
            text: Color::rgb(0, 0, 0),
            primary_light: Color::rgb(255, 255, 255),
            primary_background: Color::rgb(221, 221, 221),
            primary_dark: Color::rgb(136, 136, 136),
            primary_frame: Color::rgb(85, 85, 85),
            selection: Color::rgb(49, 100, 207),
            selection_text: Color::rgb(255, 255, 255),
            text_box_background: Color::rgb(255, 255, 255),
            menu_background: Color::rgb(255, 255, 255),
            menu_text: Color::rgb(0, 0, 0),
            alert: Color::rgb(255, 224, 128),
            alert_text: Color::rgb(0, 0, 0),
        }
    }
}

/// Insets or caps in pixels.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct Insets {
    pub left: u32,
    pub top: u32,
    pub right: u32,
    pub bottom: u32,
}

impl<'de> Deserialize<'de> for Caps {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let values = <[u32; 4]>::deserialize(deserializer)?;
        Ok(Self {
            left: values[0],
            top: values[1],
            right: values[2],
            bottom: values[3],
        })
    }
}

impl<'de> Deserialize<'de> for Insets {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let values = <[u32; 4]>::deserialize(deserializer)?;
        Ok(Self {
            left: values[0],
            top: values[1],
            right: values[2],
            bottom: values[3],
        })
    }
}

/// Nine-slice cap sizes in left, top, right, bottom order.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct Caps {
    pub left: u32,
    pub top: u32,
    pub right: u32,
    pub bottom: u32,
}

/// An explicit edge to which a slot is anchored.
#[derive(Clone, Copy, Debug, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum Anchor {
    Left,
    Right,
    Top,
    Bottom,
}

/// A signed x/y layout offset in pixels.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct Offset {
    pub x: i32,
    pub y: i32,
}

impl<'de> Deserialize<'de> for Offset {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let values = <[i32; 2]>::deserialize(deserializer)?;
        Ok(Self {
            x: values[0],
            y: values[1],
        })
    }
}

/// A decoded PNG image, kept in renderer-independent RGBA8 form.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SlotImage {
    pub rgba: Vec<u8>,
    pub width: u32,
    pub height: u32,
}

/// The visual state of a slot.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SlotState {
    Normal,
    Hilited,
    Disabled,
    Focus,
}

impl SlotState {
    const ALL: [Self; 4] = [Self::Normal, Self::Hilited, Self::Disabled, Self::Focus];

    fn suffix(self) -> &'static str {
        match self {
            Self::Normal => "normal",
            Self::Hilited => "hilited",
            Self::Disabled => "disabled",
            Self::Focus => "focus",
        }
    }
}

/// A documented widget image/configuration slot.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SlotId {
    Button,
    DefaultButton,
    IconButton,
    TickBlank,
    TickTicked,
    TickTristated,
    MutexBlank,
    MutexTicked,
    MutexTristated,
    PlusMinus,
    PopupButton,
    PopupButtonNoTitle,
    PopupButtonSymbol,
    PopupArrow,
    TextBox,
    FocusBox,
    HorizSeparator,
    VertSeparator,
    Box,
    ProgressBar,
    ProgressFill,
    HSliderBar,
    HSliderIndicator,
    HSliderPointedIndicator,
    VSliderBar,
    VSliderIndicator,
    VSliderPointedIndicator,
    ColumnHeader,
    HScrollBarTrack,
    HScrollBarThumb,
    HScrollBarDoubleArrows,
    HScrollBarSingleArrows,
    HScrollBarDisabled,
    HScrollBarTooSmall,
    HScrollBarIndicator,
    HScrollBarIndicatorGrips,
    VScrollBarTrack,
    VScrollBarThumb,
    VScrollBarDoubleArrows,
    VScrollBarSingleArrows,
    VScrollBarDisabled,
    VScrollBarTooSmall,
    VScrollBarIndicator,
    VScrollBarIndicatorGrips,
    MenuBarPattern,
    MenuBar,
    MenuBarTitlePattern,
    MenuBarTitle,
    MenuBackgroundPattern,
    MenuBackground,
    MenuItemPattern,
    MenuItem,
    MenuSeparator,
    PopupWindowFrame,
    WindowFrame,
    WindowTitlebar,
    WindowClose,
    WindowMinimize,
    WindowMaximize,
    WindowMenu,
    WindowResize,
    WonderLight,
}

impl FromStr for SlotId {
    type Err = ();
    fn from_str(value: &str) -> Result<Self, Self::Err> {
        use SlotId::*;
        Ok(match value {
            "button" => Button,
            "default_button" => DefaultButton,
            "icon_button" => IconButton,
            "tick_blank" => TickBlank,
            "tick_ticked" => TickTicked,
            "tick_tristated" => TickTristated,
            "mutex_blank" => MutexBlank,
            "mutex_ticked" => MutexTicked,
            "mutex_tristated" => MutexTristated,
            "plus_minus" => PlusMinus,
            "popup_button" => PopupButton,
            "popup_button_no_title" => PopupButtonNoTitle,
            "popup_button_symbol" => PopupButtonSymbol,
            "popup_arrow" => PopupArrow,
            "text_box" => TextBox,
            "focus_box" => FocusBox,
            "horiz_separator" => HorizSeparator,
            "vert_separator" => VertSeparator,
            "box" => Box,
            "progress_bar" => ProgressBar,
            "progress_fill" => ProgressFill,
            "h_slider_bar" => HSliderBar,
            "h_slider_indicator" => HSliderIndicator,
            "h_slider_pointed_indicator" => HSliderPointedIndicator,
            "v_slider_bar" => VSliderBar,
            "v_slider_indicator" => VSliderIndicator,
            "v_slider_pointed_indicator" => VSliderPointedIndicator,
            "column_header" => ColumnHeader,
            "h_scrollbar_track" => HScrollBarTrack,
            "h_scrollbar_thumb" => HScrollBarThumb,
            "h_scrollbar_double_arrows" => HScrollBarDoubleArrows,
            "h_scrollbar_single_arrows" => HScrollBarSingleArrows,
            "h_scrollbar_disabled" => HScrollBarDisabled,
            "h_scrollbar_too_small" => HScrollBarTooSmall,
            "h_scrollbar_indicator" => HScrollBarIndicator,
            "h_scrollbar_indicator_grips" => HScrollBarIndicatorGrips,
            "v_scrollbar_track" => VScrollBarTrack,
            "v_scrollbar_thumb" => VScrollBarThumb,
            "v_scrollbar_double_arrows" => VScrollBarDoubleArrows,
            "v_scrollbar_single_arrows" => VScrollBarSingleArrows,
            "v_scrollbar_disabled" => VScrollBarDisabled,
            "v_scrollbar_too_small" => VScrollBarTooSmall,
            "v_scrollbar_indicator" => VScrollBarIndicator,
            "v_scrollbar_indicator_grips" => VScrollBarIndicatorGrips,
            "menu_bar_pattern" => MenuBarPattern,
            "menu_bar" => MenuBar,
            "menu_bar_title_pattern" => MenuBarTitlePattern,
            "menu_bar_title" => MenuBarTitle,
            "menu_background_pattern" => MenuBackgroundPattern,
            "menu_background" => MenuBackground,
            "menu_item_pattern" => MenuItemPattern,
            "menu_item" => MenuItem,
            "menu_separator" => MenuSeparator,
            "popup_window_frame" => PopupWindowFrame,
            "window_frame" => WindowFrame,
            "window_titlebar" => WindowTitlebar,
            "window_close" => WindowClose,
            "window_minimize" => WindowMinimize,
            "window_maximize" => WindowMaximize,
            "window_menu" => WindowMenu,
            "window_resize" => WindowResize,
            "wonderlight" => WonderLight,
            _ => return Err(()),
        })
    }
}

impl fmt::Display for SlotId {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(match self {
            Self::Button => "button",
            Self::DefaultButton => "default_button",
            Self::IconButton => "icon_button",
            Self::TickBlank => "tick_blank",
            Self::TickTicked => "tick_ticked",
            Self::TickTristated => "tick_tristated",
            Self::MutexBlank => "mutex_blank",
            Self::MutexTicked => "mutex_ticked",
            Self::MutexTristated => "mutex_tristated",
            Self::PlusMinus => "plus_minus",
            Self::PopupButton => "popup_button",
            Self::PopupButtonNoTitle => "popup_button_no_title",
            Self::PopupButtonSymbol => "popup_button_symbol",
            Self::PopupArrow => "popup_arrow",
            Self::TextBox => "text_box",
            Self::FocusBox => "focus_box",
            Self::HorizSeparator => "horiz_separator",
            Self::VertSeparator => "vert_separator",
            Self::Box => "box",
            Self::ProgressBar => "progress_bar",
            Self::ProgressFill => "progress_fill",
            Self::HSliderBar => "h_slider_bar",
            Self::HSliderIndicator => "h_slider_indicator",
            Self::HSliderPointedIndicator => "h_slider_pointed_indicator",
            Self::VSliderBar => "v_slider_bar",
            Self::VSliderIndicator => "v_slider_indicator",
            Self::VSliderPointedIndicator => "v_slider_pointed_indicator",
            Self::ColumnHeader => "column_header",
            Self::HScrollBarTrack => "h_scrollbar_track",
            Self::HScrollBarThumb => "h_scrollbar_thumb",
            Self::HScrollBarDoubleArrows => "h_scrollbar_double_arrows",
            Self::HScrollBarSingleArrows => "h_scrollbar_single_arrows",
            Self::HScrollBarDisabled => "h_scrollbar_disabled",
            Self::HScrollBarTooSmall => "h_scrollbar_too_small",
            Self::HScrollBarIndicator => "h_scrollbar_indicator",
            Self::HScrollBarIndicatorGrips => "h_scrollbar_indicator_grips",
            Self::VScrollBarTrack => "v_scrollbar_track",
            Self::VScrollBarThumb => "v_scrollbar_thumb",
            Self::VScrollBarDoubleArrows => "v_scrollbar_double_arrows",
            Self::VScrollBarSingleArrows => "v_scrollbar_single_arrows",
            Self::VScrollBarDisabled => "v_scrollbar_disabled",
            Self::VScrollBarTooSmall => "v_scrollbar_too_small",
            Self::VScrollBarIndicator => "v_scrollbar_indicator",
            Self::VScrollBarIndicatorGrips => "v_scrollbar_indicator_grips",
            Self::MenuBarPattern => "menu_bar_pattern",
            Self::MenuBar => "menu_bar",
            Self::MenuBarTitlePattern => "menu_bar_title_pattern",
            Self::MenuBarTitle => "menu_bar_title",
            Self::MenuBackgroundPattern => "menu_background_pattern",
            Self::MenuBackground => "menu_background",
            Self::MenuItemPattern => "menu_item_pattern",
            Self::MenuItem => "menu_item",
            Self::MenuSeparator => "menu_separator",
            Self::PopupWindowFrame => "popup_window_frame",
            Self::WindowFrame => "window_frame",
            Self::WindowTitlebar => "window_titlebar",
            Self::WindowClose => "window_close",
            Self::WindowMinimize => "window_minimize",
            Self::WindowMaximize => "window_maximize",
            Self::WindowMenu => "window_menu",
            Self::WindowResize => "window_resize",
            Self::WonderLight => "wonderlight",
        })
    }
}

/// A widget slot and its optional state images and layout metadata.
#[derive(Clone, Debug)]
pub struct Slot {
    pub images: HashMap<SlotState, SlotImage>,
    pub caps: Caps,
    pub text_color: Option<Color>,
    pub anchor: Option<Anchor>,
    pub offset: Offset,
    pub insets: Option<Insets>,
}

/// Theme metadata from the `[meta]` table.
#[derive(Clone, Debug, Default, Deserialize, PartialEq, Eq)]
pub struct ThemeMeta {
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub author: String,
    #[serde(default)]
    pub version: String,
    #[serde(default)]
    pub description: String,
}

/// A fully loaded theme.
#[derive(Clone, Debug)]
pub struct Theme {
    pub meta: ThemeMeta,
    pub colors: ThemeColors,
    pub slots: HashMap<SlotId, Slot>,
}

impl Theme {
    /// Loads and decodes a theme folder containing `theme.toml`.
    pub fn load_from_dir(path: impl AsRef<Path>) -> Result<Self, ThemeError> {
        let root = path.as_ref();
        let source = fs::read_to_string(root.join("theme.toml"))?;
        let raw: RawTheme = toml::from_str(&source)?;
        let mut slots = HashMap::new();
        for (name, config) in raw.slots {
            let id = name
                .parse()
                .map_err(|()| ThemeError::UnknownSlot(name.clone()))?;
            slots.insert(id, load_slot(root, config)?);
        }
        Ok(Self {
            meta: raw.meta,
            colors: raw.colors,
            slots,
        })
    }
}

#[derive(Debug, Error)]
pub enum ThemeError {
    #[error("could not read theme file: {0}")]
    Io(#[from] std::io::Error),
    #[error("could not parse theme.toml: {0}")]
    Toml(#[from] toml::de::Error),
    #[error("unknown slot '{0}'")]
    UnknownSlot(String),
    #[error("slot image path '{0}' must be relative to the theme directory")]
    InvalidImagePath(PathBuf),
    #[error("could not decode image '{path}': {source}")]
    Image { path: PathBuf, source: ImageError },
}

#[derive(Debug, Deserialize)]
struct RawTheme {
    #[serde(default)]
    meta: ThemeMeta,
    #[serde(default)]
    colors: ThemeColors,
    #[serde(default, rename = "slot")]
    slots: HashMap<String, RawSlot>,
}

#[derive(Debug, Deserialize)]
struct RawSlot {
    image: Option<PathBuf>,
    #[serde(default)]
    caps: Caps,
    text: Option<Color>,
    anchor: Option<Anchor>,
    #[serde(default)]
    offset: Offset,
    insets: Option<Insets>,
}

fn load_slot(root: &Path, config: RawSlot) -> Result<Slot, ThemeError> {
    let mut images = HashMap::new();
    if let Some(image) = config.image {
        if image.is_absolute()
            || image
                .components()
                .any(|component| component == std::path::Component::ParentDir)
        {
            return Err(ThemeError::InvalidImagePath(image));
        }
        let normal_path = root.join(&image);
        images.insert(SlotState::Normal, decode_image(&normal_path)?);
        for state in SlotState::ALL.into_iter().skip(1) {
            let sibling = sibling_path(&image, state.suffix());
            let path = root.join(sibling);
            if path.is_file() {
                images.insert(state, decode_image(&path)?);
            }
        }
    }
    Ok(Slot {
        images,
        caps: config.caps,
        text_color: config.text,
        anchor: config.anchor,
        offset: config.offset,
        insets: config.insets,
    })
}

fn sibling_path(path: &Path, suffix: &str) -> PathBuf {
    let stem = path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or_default();
    let base = stem.strip_suffix("-normal").unwrap_or(stem);
    let filename = match path.extension().and_then(|value| value.to_str()) {
        Some(extension) => format!("{base}-{suffix}.{extension}"),
        None => format!("{base}-{suffix}"),
    };
    path.with_file_name(filename)
}

fn decode_image(path: &Path) -> Result<SlotImage, ThemeError> {
    let image = image::open(path)
        .map_err(|source| ThemeError::Image {
            path: path.to_owned(),
            source,
        })?
        .into_rgba8();
    let (width, height) = image.dimensions();
    Ok(SlotImage {
        rgba: image.into_raw(),
        width,
        height,
    })
}
