//! The themed multi-line text editing area.

use eframe::egui::{self, text::CCursor, text_selection::CCursorRange, Color32, FontId, Ui};
use sagrado_theme::Theme;

use crate::document::Document;

/// Render the editing area for `doc`, styled with the theme's text colors.
/// If `select` (byte start, byte end) is given, the selection/cursor is moved
/// there and scrolled into view — used to reveal Find matches.
///
/// Returns the current caret byte offset, so Find Next can search onward.
pub fn editor(
    ui: &mut Ui,
    theme: &Theme,
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

    let frame = egui::Frame::new()
        .fill(c.text_box_background)
        .stroke((1.0, c.primary_frame))
        .inner_margin(egui::Margin::same(2));

    let soft_wrap = doc.soft_wrap;
    let mut caret = 0usize;
    frame.show(ui, |ui| {
        let scroll = if soft_wrap {
            egui::ScrollArea::vertical()
        } else {
            egui::ScrollArea::both()
        };
        scroll
            .id_salt("editor_scroll")
            .auto_shrink([false, false])
            .show(ui, |ui| {
                let desired_width = if soft_wrap {
                    ui.available_width()
                } else {
                    f32::INFINITY
                };
                let output = egui::TextEdit::multiline(&mut doc.text)
                    .id(text_edit_id)
                    .font(FontId::monospace(14.0))
                    .desired_width(desired_width)
                    .desired_rows(20)
                    .lock_focus(true)
                    .frame(false)
                    .background_color(Color32::TRANSPARENT)
                    .show(ui);

                if select.is_some() {
                    if let Some(range) = output.cursor_range {
                        // Scroll the new selection into view.
                        output.response.scroll_to_me(Some(egui::Align::Center));
                        caret = char_to_byte(&doc.text, range.primary.ccursor.index);
                    }
                } else if let Some(range) = output.cursor_range {
                    caret = char_to_byte(&doc.text, range.primary.ccursor.index);
                }
            });
    });
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
