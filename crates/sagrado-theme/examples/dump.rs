//! Dump populated image slots of a .hap file: index, size, caps, positions.
//! With an output dir argument, also writes each slot as a PNG (scaled 4x).

fn main() {
    let mut args = std::env::args().skip(1);
    let path = args.next().expect("usage: dump <file.hap> [outdir]");
    let outdir = args.next();
    let data = std::fs::read(&path).unwrap();
    let theme = sagrado_theme::hap::parse(&data, "dump").unwrap();
    let mut indices: Vec<usize> = theme.slot_indices().collect();
    indices.sort_unstable();
    for idx in indices {
        let img = theme.image_by_index(idx).unwrap();
        let [w, h] = img.size();
        println!(
            "{idx:3}  {w:3}x{h:<3}  caps={:?} pos={:?}",
            img.caps, img.positions
        );
        if let Some(dir) = &outdir {
            std::fs::create_dir_all(dir).unwrap();
            write_png(&format!("{dir}/{idx:03}.png"), img);
        }
    }
}

fn write_png(path: &str, img: &sagrado_theme::SkinImage) {
    // Minimal PPM->PNG via ImageMagick is avoided; write a simple PPM and let
    // the caller convert, or write raw PNG via the `image` crate if available.
    // Here: write PPM (P6) with magenta background for transparency.
    let [w, h] = img.size();
    let scale = 4usize;
    let mut buf = format!("P6\n{} {}\n255\n", w * scale, h * scale).into_bytes();
    for y in 0..h * scale {
        for x in 0..w * scale {
            let c = img.image[(x / scale, y / scale)];
            let (r, g, b) = if c.a() == 0 {
                (255, 0, 255)
            } else {
                (c.r(), c.g(), c.b())
            };
            buf.extend_from_slice(&[r, g, b]);
        }
    }
    std::fs::write(path.replace(".png", ".ppm"), buf).unwrap();
}
