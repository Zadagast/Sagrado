//! KDX window chrome: a single layout + paint engine for every appearance.
//!
//! The chrome always has the same pieces — frame, title bar, close /
//! minimize / maximize (and optional window-menu) buttons, drag strip and a
//! bottom-right grow box. Layout comes from the theme's position metadata
//! when window art exists, otherwise from the classic Haxial "Standard"
//! metrics. Each piece is painted from its `.hap` image when the theme
//! provides one, and as the KDX Standard primitive (drawn from the color
//! table) when it doesn't, so art overlays pixels without ever gating
//! behavior.

use eframe::egui::{
    self, Align2, Color32, CursorIcon, Rect, Sense, StrokeKind, Ui, Vec2, ViewportCommand,
};
use sagrado_theme::{Slot, Theme};

use crate::paint::{natural, nine_slice, SkinTextures};

/// Standard-chrome metrics used when the theme carries no window art.
const STD_TITLE_H: f32 = 20.0;
const STD_BORDER: f32 = 5.0;
const STD_BTN: Vec2 = Vec2::new(16.0, 13.0);
const STD_GRIP: f32 = 13.0;

/// The chrome buttons a window can have.
#[derive(Clone, Copy, PartialEq, Eq)]
enum ChromeButton {
    Close,
    Minimize,
    Maximize,
    WindowMenu,
}

impl ChromeButton {
    fn slots(self) -> (Slot, Slot, Slot) {
        match self {
            ChromeButton::Close => (
                Slot::WindowCloseNormal,
                Slot::WindowCloseFocus,
                Slot::WindowCloseHilited,
            ),
            ChromeButton::Minimize => (
                Slot::WindowMinimizeNormal,
                Slot::WindowMinimizeFocus,
                Slot::WindowMinimizeHilited,
            ),
            ChromeButton::Maximize => (
                Slot::WindowMaximizeNormal,
                Slot::WindowMaximizeFocus,
                Slot::WindowMaximizeHilited,
            ),
            ChromeButton::WindowMenu => (
                Slot::WindowMenuNormal,
                Slot::WindowMenuFocus,
                Slot::WindowMenuHilited,
            ),
        }
    }

    fn glyph(self) -> &'static str {
        match self {
            ChromeButton::Close => "X",
            ChromeButton::Minimize => "-",
            ChromeButton::Maximize => "+",
            ChromeButton::WindowMenu => "",
        }
    }

    fn id(self) -> &'static str {
        match self {
            ChromeButton::Close => "close",
            ChromeButton::Minimize => "minimize",
            ChromeButton::Maximize => "maximize",
            ChromeButton::WindowMenu => "window_menu",
        }
    }
}

/// Where every chrome piece goes this frame. Computed once, before any
/// painting or interaction, from theme metadata (when window art exists) or
/// Standard metrics.
struct ChromeLayout {
    title_bar: Rect,
    client: Rect,
    buttons: Vec<(ChromeButton, Rect)>,
    /// Decorative hatched drag stripes (Standard chrome only).
    hatch_box: Option<Rect>,
    drag: Rect,
    grip: Rect,
}

/// Colors the Standard primitives are drawn with, resolved from the theme's
/// color table (window-frame ramp, entries 36..40) with generic fallbacks.
struct ChromeColors {
    fill: Color32,
    fill_top: Color32,
    fill_bottom: Color32,
    pressed: Color32,
    shadow: Color32,
    text: Color32,
}

impl ChromeColors {
    fn resolve(theme: &Theme, focused: bool) -> Self {
        let c = theme.colors;
        let table = |i: usize, def: Color32| theme.color_table.get(i).copied().unwrap_or(def);
        let bright = table(36, c.primary_dark);
        let dark = table(38, c.primary_frame);
        let deep = table(40, c.primary_frame);
        let (fill_top, fill_bottom) = if focused {
            (bright, dark)
        } else {
            (dark, deep)
        };
        Self {
            fill: if focused { bright } else { dark },
            fill_top,
            fill_bottom,
            pressed: dark,
            shadow: deep,
            text: if focused { c.text } else { c.disabled_text },
        }
    }
}

fn layout(theme: &Theme, rect: Rect, focused: bool) -> ChromeLayout {
    let frame_slot = if focused {
        Slot::WindowFrameFocus
    } else {
        Slot::WindowFrameNormal
    };
    let frame_img = theme
        .image(frame_slot)
        .or_else(|| theme.image(Slot::WindowFrameNormal));

    // Frame thickness: position metadata when art exists, Standard otherwise.
    let (l, t, r, b) = match frame_img {
        Some(img) => (
            img.pos_left(),
            img.pos_top(),
            img.pos_right(),
            img.pos_bottom(),
        ),
        None => (STD_BORDER, STD_TITLE_H, STD_BORDER, STD_BORDER),
    };
    let title_bar = Rect::from_min_max(rect.min, egui::pos2(rect.right(), rect.top() + t));
    let client = Rect::from_min_max(
        egui::pos2(rect.left() + l, rect.top() + t),
        egui::pos2(rect.right() - r, rect.bottom() - b),
    );

    // Buttons: each anchored by its own position metadata (right offset from
    // the right edge, or left offset when right is 0), sized by its
    // reference (focus/normal) image; Standard places X left, +/− right.
    let mut buttons = Vec::new();
    let mut occupied_left: f32 = l;
    let mut occupied_right: f32 = r;
    let order = [
        ChromeButton::Close,
        ChromeButton::Maximize,
        ChromeButton::Minimize,
        ChromeButton::WindowMenu,
    ];
    let mut any_button_art = false;
    for btn in order {
        let (normal, focus, _) = btn.slots();
        if let Some(img) = theme.image(focus).or_else(|| theme.image(normal)) {
            any_button_art = true;
            let [w, h] = img.size();
            let (w, h) = (w as f32, h as f32);
            let x = if img.pos_left() > 0.0 {
                rect.left() + img.pos_left()
            } else {
                rect.right() - img.pos_right() - w
            };
            let y = rect.top() + img.pos_top().max(2.0);
            let btn_rect = Rect::from_min_size(egui::pos2(x, y), Vec2::new(w, h));
            if img.pos_left() > 0.0 {
                occupied_left = occupied_left.max(img.pos_left() + w);
            } else {
                occupied_right = occupied_right.max(img.pos_right() + w);
            }
            buttons.push((btn, btn_rect));
        }
    }

    let mut hatch_box = None;
    if !any_button_art {
        // Standard layout: X box + hatch stripes top-left, +/− top-right.
        let cy = title_bar.center().y;
        let close = Rect::from_center_size(egui::pos2(rect.left() + 12.0, cy), STD_BTN);
        let hatch = Rect::from_center_size(
            egui::pos2(close.right() + 17.0, cy),
            Vec2::new(26.0, STD_BTN.y),
        );
        let min = Rect::from_center_size(egui::pos2(rect.right() - 12.0, cy), STD_BTN);
        let max = Rect::from_center_size(egui::pos2(min.left() - 12.0, cy), STD_BTN);
        occupied_left = hatch.right() - rect.left();
        occupied_right = rect.right() - max.left();
        buttons.push((ChromeButton::Close, close));
        buttons.push((ChromeButton::Maximize, max));
        buttons.push((ChromeButton::Minimize, min));
        hatch_box = Some(hatch);
    }

    let drag = Rect::from_min_max(
        egui::pos2(rect.left() + occupied_left, rect.top()),
        egui::pos2(rect.right() - occupied_right, title_bar.bottom()),
    );

    // Grow box: position metadata when resize art exists, else the Standard
    // hatched corner box.
    let grip = match theme
        .image(if focused {
            Slot::WindowResizeFocus
        } else {
            Slot::WindowResizeNormal
        })
        .or_else(|| theme.image(Slot::WindowResizeNormal))
    {
        Some(img) => {
            let [w, h] = img.size();
            let pos = egui::pos2(
                rect.right() - img.pos_right().max(1.0) - w as f32,
                rect.bottom() - img.pos_bottom().max(1.0) - h as f32,
            );
            Rect::from_min_size(pos, Vec2::new(w as f32, h as f32))
        }
        None => Rect::from_min_max(
            egui::pos2(
                rect.right() - 1.0 - STD_GRIP,
                rect.bottom() - 1.0 - STD_GRIP,
            ),
            egui::pos2(rect.right() - 1.0, rect.bottom() - 1.0),
        ),
    };

    ChromeLayout {
        title_bar,
        client,
        buttons,
        hatch_box,
        drag,
        grip,
    }
}

/// Show a KDX window: themed or Standard-primitive frame, title bar with
/// working controls, drag-to-move and a grow box. The content closure fills
/// the client area.
pub fn window_frame(
    ctx: &egui::Context,
    theme: &Theme,
    skin: &SkinTextures,
    title: &str,
    add_contents: impl FnOnce(&mut Ui),
) {
    let focused = ctx.input(|i| i.viewport().focused.unwrap_or(true));
    // Take focus on the first press so widgets respond without needing a
    // separate click to activate the window.
    if !focused && ctx.input(|i| i.pointer.any_pressed()) {
        ctx.send_viewport_cmd(ViewportCommand::Focus);
    }

    egui::CentralPanel::default()
        .frame(egui::Frame::new())
        .show(ctx, |ui| {
            let rect = ui.max_rect();
            let c = theme.colors;
            ui.painter().rect_filled(rect, 0.0, c.primary_background);

            let lay = layout(theme, rect, focused);
            let colors = ChromeColors::resolve(theme, focused);

            paint_frame(ui, theme, skin, rect, &lay, &colors, focused);

            // Buttons: interaction on the stable layout rect; per-state art
            // centred inside it, or the Standard box primitive.
            for &(btn, btn_rect) in &lay.buttons {
                let resp = ui.interact(btn_rect, ui.id().with(btn.id()), Sense::click());
                let pressed = resp.is_pointer_button_down_on();
                paint_button(ui, theme, skin, btn, btn_rect, &colors, focused, pressed);
                if resp.clicked() {
                    match btn {
                        ChromeButton::Close => ctx.send_viewport_cmd(ViewportCommand::Close),
                        ChromeButton::Minimize => {
                            ctx.send_viewport_cmd(ViewportCommand::Minimized(true))
                        }
                        ChromeButton::Maximize => {
                            let maximized = ctx.input(|i| i.viewport().maximized.unwrap_or(false));
                            ctx.send_viewport_cmd(ViewportCommand::Maximized(!maximized));
                        }
                        ChromeButton::WindowMenu => {}
                    }
                }
            }

            // Title text between the buttons.
            let text_area = lay.drag.shrink2(Vec2::new(4.0, 0.0));
            ui.painter().with_clip_rect(text_area).text(
                lay.title_bar.center(),
                Align2::CENTER_CENTER,
                title,
                crate::fonts::ui_font(),
                colors.text,
            );

            title_drag(ui, ctx, lay.drag);

            add_contents(&mut content_ui(ui, lay.client));

            // Grow box last so it wins the shared corner over the content.
            resize_grip(ui, ctx, lay.grip);
        });
}

/// Frame + title bar + hatch/grip decorations: art when present, Standard
/// primitives when not.
#[allow(clippy::too_many_arguments)]
fn paint_frame(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    rect: Rect,
    lay: &ChromeLayout,
    colors: &ChromeColors,
    focused: bool,
) {
    let frame_slot = if focused {
        Slot::WindowFrameFocus
    } else {
        Slot::WindowFrameNormal
    };
    let frame_slot = if theme.image(frame_slot).is_some() {
        frame_slot
    } else {
        Slot::WindowFrameNormal
    };
    if let (Some(tex), Some(img)) = (skin.get(frame_slot), theme.image(frame_slot)) {
        nine_slice(ui.painter(), tex, img, rect, Color32::WHITE);
    } else {
        let black = Color32::BLACK;
        // Frame: black outline, red band, black inner outline.
        ui.painter()
            .rect(rect, 0.0, colors.fill, (1.0, black), StrokeKind::Inside);
        ui.painter().rect_stroke(
            rect.shrink(1.0),
            0.0,
            (STD_BORDER - 2.0, colors.fill),
            StrokeKind::Inside,
        );
        ui.painter().rect_stroke(
            rect.shrink(STD_BORDER - 1.0),
            0.0,
            (1.0, black),
            StrokeKind::Inside,
        );
        // Title bar: vertical gradient, bright at the top fading darker.
        let bar = lay.title_bar;
        let mut mesh = egui::Mesh::default();
        mesh.colored_vertex(bar.left_top(), colors.fill_top);
        mesh.colored_vertex(bar.right_top(), colors.fill_top);
        mesh.colored_vertex(bar.right_bottom(), colors.fill_bottom);
        mesh.colored_vertex(bar.left_bottom(), colors.fill_bottom);
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);
        ui.painter().add(egui::Shape::mesh(mesh));
        ui.painter().line_segment(
            [
                egui::pos2(bar.left(), bar.bottom() - 0.5),
                egui::pos2(bar.right(), bar.bottom() - 0.5),
            ],
            (1.0, black),
        );
        // Hatched drag stripes next to the close box.
        if let Some(hr) = lay.hatch_box {
            ui.painter()
                .rect(hr, 0.0, colors.fill, (1.0, black), StrokeKind::Inside);
            hatch(ui, hr.shrink(2.0), theme.colors.text);
        }
    }

    // Grow box art; Standard hatched red box otherwise.
    let resize_slot = if focused {
        Slot::WindowResizeFocus
    } else {
        Slot::WindowResizeNormal
    };
    let resize_slot = if theme.image(resize_slot).is_some() {
        resize_slot
    } else {
        Slot::WindowResizeNormal
    };
    if let (Some(tex), Some(img)) = (skin.get(resize_slot), theme.image(resize_slot)) {
        crate::paint::natural_at(ui.painter(), tex, img, lay.grip.min, Color32::WHITE);
    } else {
        ui.painter().rect(
            lay.grip,
            0.0,
            colors.fill,
            (1.0, Color32::BLACK),
            StrokeKind::Inside,
        );
        hatch(ui, lay.grip.shrink(2.0), colors.shadow);
    }
}

/// One title-bar button: per-state art centred in the stable rect, or the
/// Standard box primitive (black border, red fill, white glyph).
#[allow(clippy::too_many_arguments)]
fn paint_button(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    btn: ChromeButton,
    rect: Rect,
    colors: &ChromeColors,
    focused: bool,
    pressed: bool,
) {
    let (normal, focus, hilited) = btn.slots();
    let want = if pressed {
        hilited
    } else if focused {
        focus
    } else {
        normal
    };
    let slot = [want, focus, normal]
        .into_iter()
        .find(|s| theme.image(*s).is_some());
    if let Some(slot) = slot {
        if let (Some(tex), Some(img)) = (skin.get(slot), theme.image(slot)) {
            natural(ui.painter(), tex, img, rect, Color32::WHITE);
        }
        return;
    }
    let fill = if pressed { colors.pressed } else { colors.fill };
    ui.painter()
        .rect(rect, 0.0, fill, (1.0, Color32::BLACK), StrokeKind::Inside);
    let glyph = btn.glyph();
    if !glyph.is_empty() {
        ui.painter().text(
            rect.center(),
            Align2::CENTER_CENTER,
            glyph,
            crate::fonts::ui_font(),
            theme.colors.text,
        );
    }
}

fn content_ui(ui: &mut Ui, client: Rect) -> Ui {
    ui.new_child(
        egui::UiBuilder::new()
            .max_rect(client)
            .layout(egui::Layout::top_down(egui::Align::Min)),
    )
}

/// Begin an OS window move when the given strip is dragged with the primary
/// button.
fn title_drag(ui: &mut Ui, ctx: &egui::Context, drag_rect: Rect) {
    if drag_rect.width() <= 0.0 || drag_rect.height() <= 0.0 {
        return;
    }
    let resp = ui.interact(
        drag_rect,
        ui.id().with("title_drag"),
        Sense::click_and_drag(),
    );
    if resp.drag_started_by(egui::PointerButton::Primary) {
        ctx.send_viewport_cmd(ViewportCommand::StartDrag);
    }
}

/// Begin an OS resize (south-east) when the grip is dragged.
fn resize_grip(ui: &mut Ui, ctx: &egui::Context, grip_rect: Rect) {
    let resp = ui
        .interact(grip_rect, ui.id().with("resize"), Sense::drag())
        .on_hover_cursor(CursorIcon::ResizeSouthEast);
    if resp.drag_started() {
        ctx.send_viewport_cmd(ViewportCommand::BeginResize(
            egui::viewport::ResizeDirection::SouthEast,
        ));
    }
}

/// Diagonal stripes used by the Standard chrome's drag and resize boxes.
fn hatch(ui: &Ui, r: Rect, color: Color32) {
    let clip = ui.painter().with_clip_rect(r);
    let step = 5.0;
    let mut x = r.left() - r.height();
    while x < r.right() {
        clip.line_segment(
            [
                egui::pos2(x, r.bottom()),
                egui::pos2(x + r.height(), r.top()),
            ],
            (2.0, color),
        );
        x += step;
    }
}
