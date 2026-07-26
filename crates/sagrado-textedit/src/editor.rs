//! The themed multi-line text editing area.

use eframe::egui::{
    self, scroll_area::ScrollBarVisibility, text::CCursor, text_selection::CCursorRange, Color32,
    Rect, Ui, Vec2,
};
use sagrado_theme::Theme;
use sagrado_ui::{chrome, widgets, SkinTextures};

use crate::document::Document;

/// Render the editing area for `doc`, styled with the theme's text colors and
/// framed by the KDX scroll bars from `skin`. If `select` (byte start, byte
/// end) is given, the selection/cursor is moved there and scrolled into view —
/// used to reveal Find matches.
///
/// Returns the current caret byte offset, so Find Next can search onward.
pub fn editor(
    ui: &mut Ui,
    theme: &Theme,
    skin: &SkinTextures,
    doc: &mut Document,
    select: Option<(usize, usize)>,
) -> usize {
    let c = theme.colors;
    let text_edit_id = ui.id().with("editor");

    // Apply a pending Find selection before the widget reads its state.
    if let Some((start, end)) = select {
        let sc = byte_to_char(&doc.text, start);
        let ec = byte_to_char(&doc.text, end);
        let mut state =
            egui::text_edit::TextEditState::load(ui.ctx(), text_edit_id).unwrap_or_default();
        state
            .cursor
            .set_char_range(Some(CCursorRange::two(CCursor::new(sc), CCursor::new(ec))));
        state.store(ui.ctx(), text_edit_id);
    }

    let mut style = (**ui.style()).clone();
    style.visuals.extreme_bg_color = c.text_box_background;
    style.visuals.override_text_color = Some(c.text_box_foreground);
    style.visuals.selection.bg_fill = c.selection;
    style.visuals.selection.stroke = (1.0, c.selection_text).into();
    ui.set_style(style);

    let soft_wrap = doc.soft_wrap;
    let show_h = !soft_wrap;
    let vbar_w = widgets::scrollbar_thickness(theme, false);
    let hbar_h = widgets::scrollbar_thickness(theme, true);

    // Reserve the whole editor rect, then carve out the scroll-bar gutters so
    // the KDX bars sit flush against the text like a classic Haxial window.
    let outer = ui.available_rect_before_wrap();
    ui.allocate_rect(outer, egui::Sense::hover());
    ui.painter().rect(
        outer,
        0.0,
        c.text_box_background,
        (1.0, c.primary_frame),
        egui::StrokeKind::Inside,
    );
    let inner = outer.shrink(1.0);
    let text_rect = Rect::from_min_max(
        inner.min,
        egui::pos2(
            inner.right() - vbar_w,
            inner.bottom() - if show_h { hbar_h } else { 0.0 },
        ),
    );

    // The scroll offset our bars ask for (applied on the frame after a drag).
    let force_key = text_edit_id.with("force_off");
    let forced: Option<Vec2> = ui.data(|d| d.get_temp(force_key));
    ui.data_mut(|d| d.remove::<Vec2>(force_key));

    let mut caret = 0usize;
    let content_size;
    let offset;

    let mut text_ui = ui.new_child(
        egui::UiBuilder::new()
            .max_rect(text_rect)
            .layout(egui::Layout::top_down(egui::Align::Min)),
    );
    {
        let ui = &mut text_ui;
        let mut scroll = if soft_wrap {
            egui::ScrollArea::vertical()
        } else {
            egui::ScrollArea::both()
        }
        .id_salt("editor_scroll")
        .scroll_bar_visibility(ScrollBarVisibility::AlwaysHidden)
        .auto_shrink([false, false]);
        if let Some(f) = forced {
            scroll = scroll.vertical_scroll_offset(f.y);
            if show_h {
                scroll = scroll.horizontal_scroll_offset(f.x);
            }
        }
        let out = scroll.show(ui, |ui| {
            let desired_width = if soft_wrap {
                ui.available_width()
            } else {
                f32::INFINITY
            };
            let output = egui::TextEdit::multiline(&mut doc.text)
                .id(text_edit_id)
                .font(sagrado_ui::fonts::mono_font())
                .desired_width(desired_width)
                .desired_rows(20)
                .lock_focus(true)
                .frame(false)
                .background_color(Color32::TRANSPARENT)
                .show(ui);

            if select.is_some() {
                if let Some(range) = output.cursor_range {
                    output.response.scroll_to_me(Some(egui::Align::Center));
                    caret = char_to_byte(&doc.text, range.primary.ccursor.index);
                }
            } else if let Some(range) = output.cursor_range {
                caret = char_to_byte(&doc.text, range.primary.ccursor.index);
            }
        });
        content_size = out.content_size;
        offset = out.state.offset;
    }

    // Vertical KDX scroll bar in the right gutter.
    let view_h = text_rect.height();
    let max_off_y = (content_size.y - view_h).max(0.0);
    let mut v_value = if max_off_y > 0.0 {
        (offset.y / max_off_y).clamp(0.0, 1.0)
    } else {
        0.0
    };
    let v_visible = if content_size.y > 0.0 {
        (view_h / content_size.y).clamp(0.0, 1.0)
    } else {
        1.0
    };
    // Stop the bar above the window's grow box, like classic KDX windows.
    let v_bottom = match chrome::grow_box_rect(ui.ctx()) {
        Some(grip) if grip.top() < text_rect.bottom() && grip.left() < inner.right() => grip.top(),
        _ => text_rect.bottom(),
    };
    let v_rect = Rect::from_min_max(
        egui::pos2(text_rect.right(), text_rect.top()),
        egui::pos2(inner.right(), v_bottom),
    );
    if widgets::v_scrollbar_in(
        ui,
        theme,
        skin,
        v_rect,
        &mut v_value,
        v_visible,
        max_off_y > 0.0,
    )
    .changed()
    {
        ui.data_mut(|d| d.insert_temp(force_key, Vec2::new(offset.x, v_value * max_off_y)));
        ui.ctx().request_repaint();
    }

    // Horizontal KDX scroll bar in the bottom gutter.
    if show_h {
        let view_w = text_rect.width();
        let max_off_x = (content_size.x - view_w).max(0.0);
        let mut h_value = if max_off_x > 0.0 {
            (offset.x / max_off_x).clamp(0.0, 1.0)
        } else {
            0.0
        };
        let h_visible = if content_size.x > 0.0 {
            (view_w / content_size.x).clamp(0.0, 1.0)
        } else {
            1.0
        };
        let h_rect = Rect::from_min_max(
            egui::pos2(text_rect.left(), text_rect.bottom()),
            egui::pos2(text_rect.right(), inner.bottom()),
        );
        if widgets::h_scrollbar_in(
            ui,
            theme,
            skin,
            h_rect,
            &mut h_value,
            h_visible,
            max_off_x > 0.0,
        )
        .changed()
        {
            ui.data_mut(|d| d.insert_temp(force_key, Vec2::new(h_value * max_off_x, offset.y)));
            ui.ctx().request_repaint();
        }

        // Fill the little corner where the two bars meet.
        let corner =
            Rect::from_min_max(egui::pos2(text_rect.right(), text_rect.bottom()), inner.max);
        ui.painter().rect_filled(corner, 0.0, c.primary_background);
    }

    caret
}

fn byte_to_char(s: &str, byte: usize) -> usize {
    s.char_indices().take_while(|(i, _)| *i < byte).count()
}

fn char_to_byte(s: &str, ch: usize) -> usize {
    s.char_indices()
        .nth(ch)
        .map(|(i, _)| i)
        .unwrap_or_else(|| s.len())
}
