// Mandelbrot Set Explorer — renders fractal to PPM image
// Compiles to Go — outputs mandelbrot.ppm (open with any image viewer)
// Run: ketc mandelbrot.kt --target go -o mandelbrot.go && go run mandelbrot.go
//
// This demonstrates: complex math, pixel-perfect rendering, file I/O,
// color mapping, zoom coordinates, multi-threaded computation

import go::fmt
import go::os
import go::strconv
import go::sync
import go::math
import go::time
import go::log

// ─── Configuration ─────────────────────────────────────────────────────────────

const WIDTH     = 1920
const HEIGHT    = 1080
const MAX_ITER  = 256
const NUM_WORKERS = 8

// ─── Complex Number ────────────────────────────────────────────────────────────

struct Complex {
    re: float,
    im: float
}

fn complex_add(a: Complex, b: Complex) -> Complex {
    return Complex { re: a.re + b.re, im: a.im + b.im }
}

fn complex_mul(a: Complex, b: Complex) -> Complex {
    return Complex {
        re: a.re * b.re - a.im * b.im,
        im: a.re * b.im + a.im * b.re
    }
}

fn complex_sq_abs(c: Complex) -> float {
    return c.re * c.re + c.im * c.im
}

// ─── Color Palettes ────────────────────────────────────────────────────────────

struct RGB {
    r: int,
    g: int,
    b: int
}

fn palette_fire(iter: int, max: int) -> RGB {
    if iter >= max {
        return RGB { r: 0, g: 0, b: 0 }
    }

    let t = iter / max
    let r = int(9.0 * (1.0 - t) * t * t * t * 255.0)
    let g = int(15.0 * (1.0 - t) * (1.0 - t) * t * t * 255.0)
    let b = int(8.5 * (1.0 - t) * (1.0 - t) * (1.0 - t) * t * 255.0)

    return RGB {
        r: clamp(r, 0, 255),
        g: clamp(g, 0, 255),
        b: clamp(b, 0, 255)
    }
}

fn palette_ocean(iter: int, max: int) -> RGB {
    if iter >= max {
        return RGB { r: 0, g: 0, b: 0 }
    }

    let t = iter / max
    let r = int(8.0 * t * (1.0 - t) * 255.0)
    let g = int(12.0 * math::Sqrt(t) * (1.0 - t) * 255.0)
    let b = int(20.0 * (1.0 - t) * (1.0 - t) * 255.0)

    return RGB {
        r: clamp(r, 0, 255),
        g: clamp(g, 0, 255),
        b: clamp(b, 0, 255)
    }
}

fn palette_neon(iter: int, max: int) -> RGB {
    if iter >= max {
        return RGB { r: 0, g: 0, b: 0 }
    }

    let t = iter / max
    let r = int(math::Sin(t * 6.28 + 0.0) * 127.0 + 128.0)
    let g = int(math::Sin(t * 6.28 + 2.09) * 127.0 + 128.0)
    let b = int(math::Sin(t * 6.28 + 4.19) * 127.0 + 128.0)

    return RGB {
        r: clamp(r, 0, 255),
        g: clamp(g, 0, 255),
        b: clamp(b, 0, 255)
    }
}

fn clamp(v: int, lo: int, hi: int) -> int {
    if v < lo { return lo }
    if v > hi { return hi }
    return v
}

// ─── Mandelbrot Kernel ─────────────────────────────────────────────────────────

fn mandelbrot_iter(c: Complex) -> int {
    mut let z = Complex { re: 0.0, im: 0.0 }
    mut let iter = 0

    while iter < MAX_ITER {
        if complex_sq_abs(z) > 4.0 {
            break
        }
        z = complex_add(complex_mul(z, z), c)
        iter = iter + 1
    }

    return iter
}

// ─── Smooth Coloring ───────────────────────────────────────────────────────────

fn mandelbrot_smooth(c: Complex) -> float {
    mut let z = Complex { re: 0.0, im: 0.0 }
    mut let iter = 0

    while iter < MAX_ITER {
        if complex_sq_abs(z) > 4.0 {
            let zn = complex_sq_abs(z)
            let smooth = iter + 1 - int(math::Log(math::Log(zn)) / math::Log(2.0))
            return smooth
        }
        z = complex_add(complex_mul(z, z), c)
        iter = iter + 1
    }

    return MAX_ITER
}

// ─── Julia Set Variant ─────────────────────────────────────────────────────────

fn julia_iter(z: Complex, c: Complex) -> int {
    mut let z = z
    mut let iter = 0

    while iter < MAX_ITER {
        if complex_sq_abs(z) > 4.0 {
            break
        }
        z = complex_add(complex_mul(z, z), c)
        iter = iter + 1
    }

    return iter
}

// ─── Viewport ──────────────────────────────────────────────────────────────────

struct Viewport {
    center_x: float,
    center_y: float,
    zoom:     float,
    julia_c:  Complex,
    julia:    bool
}

fn viewport_to_complex(vp: Viewport, px: int, py: int) -> Complex {
    let aspect = HEIGHT / WIDTH
    let half_w = 2.0 / vp.zoom
    let half_h = half_w * aspect

    let re = vp.center_x + (px / WIDTH  - 0.5) * half_w * 2.0
    let im = vp.center_y + (py / HEIGHT - 0.5) * half_h * 2.0

    return Complex { re: re, im: im }
}

// ─── Worker Pool ───────────────────────────────────────────────────────────────

struct WorkItem {
    y:       int,
    pixels:  [RGB]
}

struct WorkResult {
    y:       int,
    pixels:  [RGB],
    ms:      int64
}

fn render_scanline(vp: Viewport, y: int, palette_fn: fn(int, int) -> RGB) -> [RGB] {
    let pixels: [RGB] = []
    pixels.reserve(WIDTH)

    mut let x = 0
    while x < WIDTH {
        let c = viewport_to_complex(vp, x, y)
        let iter: int

        if vp.julia {
            iter = julia_iter(c, vp.julia_c)
        } else {
            iter = mandelbrot_iter(c)
        }

        pixels.append(palette_fn(iter, MAX_ITER))
        x = x + 1
    }

    return pixels
}

fn worker(id: int, vp: Viewport, jobs: Channel<int>, results: Channel<WorkResult>, wg: go::sync::WaitGroup) {
    loop {
        let y = jobs.receive()
        if y == -1 { break }

        let start = go::time::Now().UnixMilli()
        let pixels = render_scanline(vp, y, palette_neon)
        let elapsed = go::time::Now().UnixMilli() - start

        results.send(WorkResult { y: y, pixels: pixels, ms: elapsed })
    }
    wg.Done()
}

// ─── PPM Writer ────────────────────────────────────────────────────────────────

fn write_ppm(filename: str, width: int, height: int, pixels: [RGB]) -> bool {
    let f = go::os::Create(filename)
    if f == null {
        go::log::Println("Failed to create: " + filename)
        return false
    }
    defer f.Close()

    // PPM header (P6 = binary color)
    go::fmt::Fprintf(f, "P6\n%d %d\n255\n", width, height)

    // Write pixel data
    mut let y = 0
    while y < height {
        mut let x = 0
        while x < width {
            let p = pixels[y * width + x]
            f.Write([]byte{ byte(p.r), byte(p.g), byte(p.b) })
            x = x + 1
        }
        y = y + 1
    }

    return true
}

// ─── Anti-Aliasing (2x2 SSAA) ──────────────────────────────────────────────────

fn render_aa(vp: Viewport, palette_fn: fn(int, int) -> RGB) -> [RGB] {
    let pixels: [RGB] = []
    pixels.reserve(WIDTH * HEIGHT)

    let sub_offset = [
        Vec2 { x: -0.25, y: -0.25 },
        Vec2 { x:  0.25, y: -0.25 },
        Vec2 { x: -0.25, y:  0.25 },
        Vec2 { x:  0.25, y:  0.25 }
    ]

    mut let py = 0
    while py < HEIGHT {
        mut let px = 0
        while px < WIDTH {
            mut let r = 0
            mut let g = 0
            mut let b = 0

            for sub in sub_offset {
                let x = px + sub.x
                let y = py + sub.y
                let c = viewport_to_complex(vp, int(x), int(y))
                let iter = mandelbrot_smooth(c)
                let col = palette_fn(int(iter), MAX_ITER)
                r = r + col.r
                g = g + col.g
                b = b + col.b
            }

            pixels.append(RGB {
                r: r / 4,
                g: g / 4,
                b: b / 4
            })
            px = px + 1
        }

        if py % 50 == 0 {
            go::log::Println("AA render: " + str(py * 100 / HEIGHT) + "%")
        }
        py = py + 1
    }

    return pixels
}

// ─── Zooms (animated GIF frames as separate images) ────────────────────────────

fn render_frame(vp: Viewport, frame: int) {
    let filename = go::fmt::Sprintf("mandelbrot_%03d.ppm", frame)
    go::log::Println("Rendering frame " + str(frame) + " -> " + filename)

    let start = go::time::Now().UnixMilli()

    // Parallel render using channels
    let jobs    = make_channel(int, HEIGHT + NUM_WORKERS)
    let results = make_channel(WorkResult, HEIGHT)
    let wg      = go::sync::WaitGroup{}

    // Start workers
    mut let w = 0
    while w < NUM_WORKERS {
        wg.Add(1)
        go worker(w, vp, jobs, results, wg)
        w = w + 1
    }

    // Send jobs
    mut let y = 0
    while y < HEIGHT {
        jobs.send(y)
        y = y + 1
    }

    // Collect results
    let pixels: [RGB] = []
    pixels.reserve(WIDTH * HEIGHT)

    mut let done = 0
    while done < HEIGHT {
        let r = results.receive()
        mut let py = 0
        while py < WIDTH {
            pixels[r.y * WIDTH + py] = r.pixels[py]
            py = py + 1
        }
        done = done + 1
    }

    // Signal workers to stop
    mut let i = 0
    while i < NUM_WORKERS {
        jobs.send(-1)
        i = i + 1
    }
    wg.Wait()

    write_ppm(filename, WIDTH, HEIGHT, pixels)

    let elapsed = go::time::Now().UnixMilli() - start
    go::log::Println("Frame " + str(frame) + " done in " + str(elapsed) + "ms")
}

// ─── Interactive Viewport Calculator ───────────────────────────────────────────

fn calculate_viewport(loc_x: float, loc_y: float, zoom_level: int) -> Viewport {
    let zoom = math::Pow(1.5, zoom_level)
    return Viewport {
        center_x: loc_x,
        center_y: loc_y,
        zoom:     zoom,
        julia_c:  Complex { re: -0.7, im: 0.27 },
        julia:    false
    }
}

// ─── Deep Zoom Sequence ────────────────────────────────────────────────────────

fn render_zoom_sequence() {
    let locations = [
        Vec2 { x: -0.5,    y: 0.0 },     // full set
        Vec2 { x: -0.745,  y: 0.186 },   // spiral
        Vec2 { x: -0.748,  y: 0.095 },   // deep zoom
        Vec2 { x: -1.25,   y: 0.0 },     // left tip
        Vec2 { x: 0.28,    y: 0.008 },   // seahorse valley
        Vec2 { x: -0.16,   y: 1.04 },    // top antenna
        Vec2 { x: -0.7269, y: 0.1889 },  // mini mandelbrot
        Vec2 { x: -1.2495, y: 0.0 }      // elephant valley
    ]

    mut let i = 0
    while i < locations.len() {
        let loc = locations[i]
        let vp = Viewport {
            center_x: loc.x,
            center_y: loc.y,
            zoom:     math::Pow(1.25, i + 5),
            julia_c:  Complex { re: -0.7, im: 0.27 },
            julia:    false
        }
        render_frame(vp, i + 1)
        i = i + 1
    }

    // Bonus: Julia set at the last location
    let last = locations[locations.len() - 1]
    let julia_vp = Viewport {
        center_x: 0.0,
        center_y: 0.0,
        zoom:     2.0,
        julia_c:  Complex { re: last.x, im: last.y },
        julia:    true
    }
    render_frame(julia_vp, 99)
}

// ─── Benchmark Mode ────────────────────────────────────────────────────────────

fn run_benchmark() {
    let resolutions = [
        Vec2 { x: 640,  y: 480 },
        Vec2 { x: 800,  y: 600 },
        Vec2 { x: 1024, y: 768 }
    ]

    go::log::Println("Running benchmarks...")

    for res in resolutions {
        let old_w = WIDTH
        let old_h = HEIGHT
        // Note: real implementation would resize, simplified here
        let vp = Viewport {
            center_x: -0.5,
            center_y: 0.0,
            zoom:     2.0,
            julia_c:  Complex { re: 0.0, im: 0.0 },
            julia:    false
        }

        let start = go::time::Now().UnixMilli()
        let _ = render_scanline(vp, HEIGHT / 2, palette_fire)
        let elapsed = go::time::Now().UnixMilli() - start

        go::log::Println("Benchmark " + str(int(res.x)) + "x" + str(int(res.y)) +
                        ": " + str(elapsed) + "ms per scanline")
    }
}

// ─── Main ──────────────────────────────────────────────────────────────────────

fn main() -> int {
    go::log::Println("Mandelbrot Set Explorer v2.0")
    go::log::Println("Resolution: " + str(WIDTH) + "x" + str(HEIGHT))
    go::log::Println("Max iterations: " + str(MAX_ITER))
    go::log::Println("Workers: " + str(NUM_WORKERS))

    let mode = go::os::Getenv("MODE")

    if mode == "benchmark" {
        run_benchmark()
        return 0
    }

    if mode == "julia" {
        // Render a Julia set
        let vp = Viewport {
            center_x: 0.0,
            center_y: 0.0,
            zoom:     2.5,
            julia_c:  Complex { re: -0.7269, im: 0.1889 },
            julia:    true
        }
        render_frame(vp, 0)
        go::log::Println("Julia set rendered to julia_000.ppm")
        return 0
    }

    // Full zoom sequence
    go::log::Println("Rendering zoom sequence...")
    render_zoom_sequence()

    // Final high-quality render with anti-aliasing
    go::log::Println("Rendering final high-quality image (with AA)...")
    let final_vp = Viewport {
        center_x: -0.5,
        center_y: 0.0,
        zoom:     1.5,
        julia_c:  Complex { re: 0.0, im: 0.0 },
        julia:    false
    }

    let start = go::time::Now().UnixMilli()
    let pixels = render_aa(final_vp, palette_ocean)
    write_ppm("mandelbrot_final_hq.ppm", WIDTH, HEIGHT, pixels)
    let elapsed = go::time::Now().UnixMilli() - start

    go::log::Println("Final render completed in " + str(elapsed) + "ms")
    go::log::Println("")
    go::log::Println("All images saved as PPM files.")
    go::log::Println("View them with:  feh *.ppm       (Linux)")
    go::log::Println("                 open *.ppm      (macOS)")
    go::log::Println("                 irfanview *.ppm (Windows)")
    go::log::Println("")
    go::log::Println("Or convert to PNG: convert mandelbrot_*.ppm mandelbrot.gif")

    return 0
}
