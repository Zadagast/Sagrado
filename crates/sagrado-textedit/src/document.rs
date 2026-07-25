//! A single open text document.

use std::path::{Path, PathBuf};

pub struct Document {
    pub path: Option<PathBuf>,
    pub text: String,
    /// Text as it was last loaded/saved, to detect unsaved changes.
    saved_text: String,
    /// Soft-wrap long lines to the viewport width.
    pub soft_wrap: bool,
}

impl Default for Document {
    fn default() -> Self {
        Self {
            path: None,
            text: String::new(),
            saved_text: String::new(),
            soft_wrap: true,
        }
    }
}

impl Document {
    /// A welcome document shown on first launch.
    pub fn sample() -> Self {
        let text = "'Twas brillig, and the slithy toves\n\
            Did gyre and gimble in the wabe;\n\
            All mimsy were the borogoves,\n\
            And the mome raths outgrabe.\n\n\
            'Beware the Jabberwock, my son!\n\
            The jaws that bite, the claws that catch!\n\
            Beware the Jubjub bird, and shun\n\
            The frumious Bandersnatch!'\n"
            .to_owned();
        Self {
            saved_text: text.clone(),
            text,
            path: None,
            soft_wrap: true,
        }
    }

    pub fn from_path(path: PathBuf) -> std::io::Result<Self> {
        let text = std::fs::read_to_string(&path)?;
        Ok(Self {
            saved_text: text.clone(),
            text,
            path: Some(path),
            soft_wrap: true,
        })
    }

    pub fn save_to(&mut self, path: &Path) -> std::io::Result<()> {
        std::fs::write(path, &self.text)?;
        self.saved_text = self.text.clone();
        self.path = Some(path.to_owned());
        Ok(())
    }

    pub fn is_dirty(&self) -> bool {
        self.text != self.saved_text
    }

    /// File name for the tab / title bar, with a `*` when unsaved.
    pub fn display_name(&self) -> String {
        let base = self
            .path
            .as_ref()
            .and_then(|p| p.file_name())
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_else(|| "Untitled".to_owned());
        if self.is_dirty() {
            format!("{base} *")
        } else {
            base
        }
    }

    /// Alphabetically sort the document's lines in place.
    pub fn sort_lines(&mut self) {
        let mut lines: Vec<&str> = self.text.lines().collect();
        lines.sort_unstable_by_key(|a| a.to_lowercase());
        let trailing_newline = self.text.ends_with('\n');
        let mut out = lines.join("\n");
        if trailing_newline {
            out.push('\n');
        }
        self.text = out;
    }
}

/// Count non-overlapping occurrences of `needle` in `haystack`.
pub fn count_occurrences(haystack: &str, needle: &str, case_sensitive: bool) -> usize {
    if needle.is_empty() {
        return 0;
    }
    if case_sensitive {
        haystack.matches(needle).count()
    } else {
        haystack
            .to_lowercase()
            .matches(&needle.to_lowercase())
            .count()
    }
}

/// Find the byte index of the next occurrence of `needle` at or after
/// `from`, wrapping to the start. Returns the match's start..end byte range.
pub fn find_from(
    haystack: &str,
    needle: &str,
    from: usize,
    case_sensitive: bool,
) -> Option<(usize, usize)> {
    if needle.is_empty() {
        return None;
    }
    let (hay, need) = if case_sensitive {
        (haystack.to_owned(), needle.to_owned())
    } else {
        (haystack.to_lowercase(), needle.to_lowercase())
    };
    let from = from.min(hay.len());
    if let Some(rel) = hay[from..].find(&need) {
        let start = from + rel;
        return Some((start, start + need.len()));
    }
    // Wrap around.
    if let Some(start) = hay[..from].find(&need) {
        return Some((start, start + need.len()));
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn counts_case_insensitive() {
        assert_eq!(count_occurrences("aAaA", "a", false), 4);
        assert_eq!(count_occurrences("aAaA", "a", true), 2);
    }

    #[test]
    fn finds_and_wraps() {
        let hay = "one two one";
        assert_eq!(find_from(hay, "one", 0, true), Some((0, 3)));
        assert_eq!(find_from(hay, "one", 1, true), Some((8, 11)));
        assert_eq!(find_from(hay, "one", 9, true), Some((0, 3)));
    }

    #[test]
    fn sorts_lines() {
        let mut d = Document {
            text: "banana\nApple\ncherry\n".to_owned(),
            ..Default::default()
        };
        d.sort_lines();
        assert_eq!(d.text, "Apple\nbanana\ncherry\n");
    }
}
