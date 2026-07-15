// 3D Ray Tracer in Ketamine — photorealistic rendering (compiles to Go)
// Features: spheres, planes, reflections, refractions, shadows, PPM output
// Run: ketc raytracer.kt --target go -o raytracer.go && go run raytracer.go

import go::fmt
import go::os
import go::math
import go::log
import go::time
import go::sync
import go::strconv

// ─── Vector Math ───────────────────────────────────────────────────────────────

struct Vec3 {
    x: float,
    y: float,
    z: float
}

fn vec3_add(a: Vec3, b: Vec3) -> Vec3 {
    return Vec3 { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z }
}

fn vec3_sub(a: Vec3, b: Vec3) -> Vec3 {
    return Vec3 { x: a.x - b.x, y: a.y - b.y, z: a.z - b.z }
}

fn vec3_mul(a: Vec3, b: Vec3) -> Vec3 {
    return Vec3 { x: a.x * b.x, y: a.y * b.y, z: a.z * b.z }
}

fn vec3_scale(v: Vec3, s: float) -> Vec3 {
    return Vec3 { x: v.x * s, y: v.y * s, z: v.z * s }
}

fn vec3_dot(a: Vec3, b: Vec3) -> float {
    return a.x * b.x + a.y * b.y + a.z * b.z
}

fn vec3_cross(a: Vec3, b: Vec3) -> Vec3 {
    return Vec3 {
        x: a.y * b.z - a.z * b.y,
        y: a.z * b.x - a.x * b.z,
        z: a.x * b.y - a.y * b.x
    }
}

fn vec3_length(v: Vec3) -> float {
    return math::Sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
}

fn vec3_normalize(v: Vec3) -> Vec3 {
    let len = vec3_length(v)
    if len < 1e-12 { return Vec3 { x: 0.0, y: 0.0, z: 0.0 } }
    return vec3_scale(v, 1.0 / len)
}

fn vec3_reflect(v: Vec3, n: Vec3) -> Vec3 {
    return vec3_sub(v, vec3_scale(n, 2.0 * vec3_dot(v, n)))
}

fn vec3_refract(v: Vec3, n: Vec3, ni_over_nt: float) -> Vec3 {
    let dt = vec3_dot(v, n)
    let disc = 1.0 - ni_over_nt * ni_over_nt * (1.0 - dt * dt)
    if disc > 0.0 {
        return vec3_sub(
            vec3_scale(v, ni_over_nt),
            vec3_scale(n, ni_over_nt * dt + math::Sqrt(disc))
        )
    }
    return Vec3 { x: 0.0, y: 0.0, z: 0.0 }
}

// ─── Ray ───────────────────────────────────────────────────────────────────────

struct Ray {
    origin: Vec3,
    dir:    Vec3
}

fn ray_at(r: Ray, t: float) -> Vec3 {
    return vec3_add(r.origin, vec3_scale(r.dir, t))
}

// ─── Color ─────────────────────────────────────────────────────────────────────

struct Color {
    r: float,
    g: float,
    b: float
}

fn color_add(a: Color, b: Color) -> Color {
    return Color { r: a.r + b.r, g: a.g + b.g, b: a.b + b.b }
}

fn color_mul(a: Color, b: Color) -> Color {
    return Color { r: a.r * b.r, g: a.g * b.g, b: a.b * b.b }
}

fn color_scale(c: Color, s: float) -> Color {
    return Color { r: c.r * s, g: c.g * s, b: c.b * s }
}

fn color_clamp(c: Color) -> Color {
    fn clamp_ch(v: float) -> float {
        if v < 0.0 { return 0.0 }
        if v > 1.0 { return 1.0 }
        return v
    }
    return Color {
        r: clamp_ch(c.r),
        g: clamp_ch(c.g),
        b: clamp_ch(c.b)
    }
}

fn color_to_rgb(c: Color) -> (int, int, int) {
    let cc = color_clamp(c)
    return (int(cc.r * 255.999), int(cc.g * 255.999), int(cc.b * 255.999))
}

// ─── Materials ─────────────────────────────────────────────────────────────────

struct Material {
    albedo:     Color,
    emissive:   Color,
    roughness:  float,
    metallic:   float,
    refractive: float,
    opacity:    float
}

fn mat_lambertian(color: Color) -> Material {
    return Material {
        albedo:    color,
        emissive:  Color { r: 0.0, g: 0.0, b: 0.0 },
        roughness: 0.8,
        metallic:  0.0,
        refractive: 0.0,
        opacity:   1.0
    }
}

fn mat_metal(color: Color, roughness: float) -> Material {
    return Material {
        albedo:    color,
        emissive:  Color { r: 0.0, g: 0.0, b: 0.0 },
        roughness: roughness,
        metallic:  1.0,
        refractive: 0.0,
        opacity:   1.0
    }
}

fn mat_glass(refractive: float) -> Material {
    return Material {
        albedo:    Color { r: 1.0, g: 1.0, b: 1.0 },
        emissive:  Color { r: 0.0, g: 0.0, b: 0.0 },
        roughness: 0.0,
        metallic:  0.0,
        refractive: refractive,
        opacity:   0.1
    }
}

fn mat_light(color: Color, intensity: float) -> Material {
    return Material {
        albedo:    Color { r: 0.0, g: 0.0, b: 0.0 },
        emissive:  color_scale(color, intensity),
        roughness: 0.0,
        metallic:  0.0,
        refractive: 0.0,
        opacity:   1.0
    }
}

// ─── Hit Record ────────────────────────────────────────────────────────────────

struct HitRecord {
    t:       float,
    point:   Vec3,
    normal:  Vec3,
    front:   bool,
    mat:     Material
}

fn hit_set_face_normal(rec: HitRecord, r: Ray, outward: Vec3) {
    rec.front = vec3_dot(r.dir, outward) < 0.0
    if rec.front {
        rec.normal = outward
    } else {
        rec.normal = Vec3 { x: -outward.x, y: -outward.y, z: -outward.z }
    }
}

// ─── Sphere ────────────────────────────────────────────────────────────────────

struct Sphere {
    center: Vec3,
    radius: float,
    mat:    Material
}

fn sphere_hit(s: Sphere, r: Ray, t_min: float, t_max: float) -> HitRecord {
    let oc = vec3_sub(r.origin, s.center)
    let a  = vec3_dot(r.dir, r.dir)
    let hb = vec3_dot(oc, r.dir)
    let c  = vec3_dot(oc, oc) - s.radius * s.radius
    let disc = hb * hb - a * c

    if disc < 0.0 { return null }

    let sqrtd = math::Sqrt(disc)
    mut let t = (-hb - sqrtd) / a
    if t < t_min || t > t_max {
        t = (-hb + sqrtd) / a
        if t < t_min || t > t_max {
            return null
        }
    }

    let rec = HitRecord {
        t:     t,
        point: ray_at(r, t),
        mat:   s.mat
    }

    let outward = vec3_scale(vec3_sub(rec.point, s.center), 1.0 / s.radius)
    hit_set_face_normal(rec, r, outward)
    return rec
}

// ─── Plane ─────────────────────────────────────────────────────────────────────

struct Plane {
    point:  Vec3,
    normal: Vec3,
    mat:    Material
}

fn plane_hit(p: Plane, r: Ray, t_min: float, t_max: float) -> HitRecord {
    let denom = vec3_dot(p.normal, r.dir)
    if math::Abs(denom) < 1e-12 { return null }

    let t = vec3_dot(vec3_sub(p.point, r.origin), p.normal) / denom
    if t < t_min || t > t_max { return null }

    let rec = HitRecord {
        t:     t,
        point: ray_at(r, t),
        mat:   p.mat
    }
    hit_set_face_normal(rec, r, p.normal)
    return rec
}

// ─── Scene ─────────────────────────────────────────────────────────────────────

struct Scene {
    spheres: [Sphere],
    planes:  [Plane]
}

fn scene_add_sphere(s: Scene, center: Vec3, radius: float, mat: Material) {
    s.spheres.append(Sphere { center: center, radius: radius, mat: mat })
}

fn scene_add_plane(s: Scene, point: Vec3, normal: Vec3, mat: Material) {
    s.planes.append(Plane { point: point, normal: vec3_normalize(normal), mat: mat })
}

fn scene_hit(scene: Scene, r: Ray, t_min: float, t_max: float) -> HitRecord {
    mut let closest = t_max
    mut let hit: HitRecord = null

    for sp in scene.spheres {
        let rec = sphere_hit(sp, r, t_min, closest)
        if rec != null {
            closest = rec.t
            hit = rec
        }
    }

    for pl in scene.planes {
        let rec = plane_hit(pl, r, t_min, closest)
        if rec != null {
            closest = rec.t
            hit = rec
        }
    }

    return hit
}

// ─── Camera ────────────────────────────────────────────────────────────────────

struct Camera {
    origin:     Vec3,
    lower_left: Vec3,
    horizontal: Vec3,
    vertical:   Vec3,
    u:          Vec3,
    v:          Vec3,
    w:          Vec3,
    lens_r:     float
}

fn new_camera(
    lookfrom: Vec3,
    lookat:   Vec3,
    vup:      Vec3,
    vfov:     float,
    aspect:   float,
    aperture: float,
    focus:    float
) -> Camera {
    let theta = vfov * math::Pi / 180.0
    let h = math::Tan(theta / 2.0)
    let view_h = 2.0 * h * focus
    let view_w = view_h * aspect

    let w = vec3_normalize(vec3_sub(lookfrom, lookat))
    let u = vec3_normalize(vec3_cross(vup, w))
    let v = vec3_cross(w, u)

    return Camera {
        origin:     lookfrom,
        horizontal: vec3_scale(u, view_w),
        vertical:   vec3_scale(v, view_h),
        lower_left: vec3_sub(
            vec3_sub(vec3_sub(lookfrom, vec3_scale(u, view_w / 2.0)), vec3_scale(v, view_h / 2.0)),
            vec3_scale(w, focus)
        ),
        u: u,
        v: v,
        w: w,
        lens_r: aperture / 2.0
    }
}

fn camera_get_ray(cam: Camera, s: float, t: float) -> Ray {
    let rd = vec3_scale(random_in_disk(), cam.lens_r)
    let offset = vec3_add(vec3_scale(cam.u, rd.x), vec3_scale(cam.v, rd.y))

    return Ray {
        origin: vec3_add(cam.origin, offset),
        dir: vec3_sub(
            vec3_add(vec3_add(cam.lower_left, vec3_scale(cam.horizontal, s)),
                     vec3_scale(cam.vertical, t)),
            vec3_add(cam.origin, offset)
        )
    }
}

fn random_in_disk() -> Vec3 {
    loop {
        let p = Vec3 {
            x: random_float() * 2.0 - 1.0,
            y: random_float() * 2.0 - 1.0,
            z: 0.0
        }
        if vec3_dot(p, p) < 1.0 {
            return p
        }
    }
}

fn random_float() -> float {
    return go::rand::Float64()
}

fn random_range(min: float, max: float) -> float {
    return min + random_float() * (max - min)
}

// ─── Sky ───────────────────────────────────────────────────────────────────────

fn sky_color(r: Ray) -> Color {
    let unit = vec3_normalize(r.dir)
    let t = 0.5 * (unit.y + 1.0)
    let white = Color { r: 1.0, g: 1.0, b: 1.0 }
    let blue  = Color { r: 0.5, g: 0.7, b: 1.0 }
    return color_add(
        color_scale(white, 1.0 - t),
        color_scale(blue, t)
    )
}

// ─── Global Illumination ───────────────────────────────────────────────────────

fn ray_color(scene: Scene, r: Ray, depth: int) -> Color {
    if depth <= 0 {
        return Color { r: 0.0, g: 0.0, b: 0.0 }
    }

    let hit = scene_hit(scene, r, 0.001, 1e12)
    if hit == null {
        return sky_color(r)
    }

    // Emissive
    let emitted = hit.mat.emissive
    if vec3_length(Color { r: emitted.r, g: emitted.g, b: emitted.b }) > 0.0 {
        return emitted
    }

    // Diffuse
    if hit.mat.metallic < 0.5 && hit.mat.refractive < 0.01 {
        let target = vec3_add(
            hit.point,
            vec3_add(hit.normal, random_unit_vector())
        )
        let scattered = Ray {
            origin: hit.point,
            dir:    vec3_sub(target, hit.point)
        }
        return color_add(
            emitted,
            color_mul(ray_color(scene, scattered, depth - 1), hit.mat.albedo)
        )
    }

    // Metallic reflection
    if hit.mat.metallic >= 0.5 && hit.mat.refractive < 0.01 {
        let reflected = vec3_reflect(vec3_normalize(r.dir), hit.normal)
        let fuzzy = vec3_add(
            reflected,
            vec3_scale(random_unit_vector(), hit.mat.roughness)
        )
        let scattered = Ray {
            origin: hit.point,
            dir:    vec3_normalize(fuzzy)
        }
        if vec3_dot(scattered.dir, hit.normal) > 0.0 {
            return color_add(
                emitted,
                color_mul(ray_color(scene, scattered, depth - 1), hit.mat.albedo)
            )
        }
        return emitted
    }

    // Dielectric (glass)
    if hit.mat.refractive >= 0.01 {
        let ref_idx: float
        if hit.front {
            ref_idx = 1.0 / hit.mat.refractive
        } else {
            ref_idx = hit.mat.refractive
        }

        let unit_dir = vec3_normalize(r.dir)
        let cos_theta = math::Min(vec3_dot(vec3_scale(unit_dir, -1.0), hit.normal), 1.0)
        let sin_theta = math::Sqrt(1.0 - cos_theta * cos_theta)

        let cannot_refract = ref_idx * sin_theta > 1.0
        let direction: Vec3

        if cannot_refract || schlick(cos_theta, ref_idx) > random_float() {
            direction = vec3_reflect(unit_dir, hit.normal)
        } else {
            direction = vec3_refract(unit_dir, hit.normal, ref_idx)
        }

        let scattered = Ray {
            origin: hit.point,
            dir:    direction
        }
        return color_add(
            emitted,
            color_mul(ray_color(scene, scattered, depth - 1), hit.mat.albedo)
        )
    }

    return emitted
}

fn random_unit_vector() -> Vec3 {
    loop {
        let p = Vec3 {
            x: random_range(-1.0, 1.0),
            y: random_range(-1.0, 1.0),
            z: random_range(-1.0, 1.0)
        }
        let len_sq = vec3_dot(p, p)
        if len_sq <= 1.0 && len_sq > 1e-12 {
            return vec3_scale(p, 1.0 / math::Sqrt(len_sq))
        }
    }
}

fn schlick(cos: float, ref_idx: float) -> float {
    let r0 = (1.0 - ref_idx) / (1.0 + ref_idx)
    let r0 = r0 * r0
    return r0 + (1.0 - r0) * math::Pow(1.0 - cos, 5.0)
}

// ─── Scene Builder ─────────────────────────────────────────────────────────────

fn build_demo_scene() -> Scene {
    let scene = Scene { spheres: [], planes: [] }

    // Ground plane
    scene_add_plane(scene,
        Vec3 { x: 0.0, y: -1.0, z: 0.0 },
        Vec3 { x: 0.0, y: 1.0,  z: 0.0 },
        mat_lambertian(Color { r: 0.5, g: 0.5, b: 0.5 })
    )

    // Center sphere — glass
    scene_add_sphere(scene,
        Vec3 { x: 0.0, y: 0.5, z: -1.5 },
        0.5,
        mat_glass(1.5)
    )

    // Left sphere — diffuse
    scene_add_sphere(scene,
        Vec3 { x: -1.2, y: 0.5, z: -1.8 },
        0.5,
        mat_lambertian(Color { r: 0.8, g: 0.2, b: 0.2 })
    )

    // Right sphere — metallic
    scene_add_sphere(scene,
        Vec3 { x: 1.2, y: 0.5, z: -2.0 },
        0.5,
        mat_metal(Color { r: 0.8, g: 0.6, b: 0.2 }, 0.1)
    )

    // Small floating spheres
    scene_add_sphere(scene,
        Vec3 { x: -0.5, y: 1.5, z: -2.5 },
        0.3,
        mat_metal(Color { r: 0.2, g: 0.8, b: 0.8 }, 0.3)
    )

    scene_add_sphere(scene,
        Vec3 { x: 0.5, y: 1.2, z: -3.0 },
        0.3,
        mat_lambertian(Color { r: 0.8, g: 0.8, b: 0.2 })
    )

    // Light sources
    scene_add_sphere(scene,
        Vec3 { x: -2.0, y: 3.0, z: -1.0 },
        0.3,
        mat_light(Color { r: 1.0, g: 0.6, b: 0.3 }, 4.0)
    )

    scene_add_sphere(scene,
        Vec3 { x: 2.0, y: 2.5, z: -1.5 },
        0.3,
        mat_light(Color { r: 0.3, g: 0.6, b: 1.0 }, 3.0)
    )

    return scene
}

fn build_final_scene() -> Scene {
    let scene = Scene { spheres: [], planes: [] }

    // Ground
    scene_add_plane(scene,
        Vec3 { x: 0.0, y: -0.5, z: 0.0 },
        Vec3 { x: 0.0, y: 1.0,  z: 0.0 },
        mat_lambertian(Color { r: 0.4, g: 0.4, b: 0.5 })
    )

    // Back wall
    scene_add_plane(scene,
        Vec3 { x: 0.0, y: 0.0, z: -5.0 },
        Vec3 { x: 0.0, y: 0.0, z: 1.0 },
        mat_lambertian(Color { r: 0.7, g: 0.3, b: 0.3 })
    )

    // Random spheres
    mut let i = 0
    while i < 8 {
        let x = random_range(-2.0, 2.0)
        let y = random_range(0.2, 1.5)
        let z = random_range(-4.5, -1.5)
        let r = random_range(0.2, 0.6)

        let mat_type = random_float()
        let mat: Material

        if mat_type < 0.4 {
            mat = mat_lambertian(Color {
                r: random_float(),
                g: random_float(),
                b: random_float()
            })
        } else if mat_type < 0.7 {
            mat = mat_metal(Color {
                r: random_range(0.5, 1.0),
                g: random_range(0.5, 1.0),
                b: random_range(0.5, 1.0)
            }, random_range(0.0, 0.5))
        } else {
            mat = mat_glass(1.5)
        }

        scene_add_sphere(scene, Vec3 { x: x, y: y, z: z }, r, mat)
        i = i + 1
    }

    // Big glass sphere
    scene_add_sphere(scene,
        Vec3 { x: -1.5, y: 1.0, z: -2.5 },
        1.0,
        mat_glass(1.5)
    )

    // Lights
    scene_add_sphere(scene,
        Vec3 { x: 0.0, y: 3.5, z: -1.0 },
        0.5,
        mat_light(Color { r: 1.0, g: 1.0, b: 0.9 }, 5.0)
    )

    scene_add_sphere(scene,
        Vec3 { x: -2.0, y: 2.0, z: -3.0 },
        0.3,
        mat_light(Color { r: 1.0, g: 0.4, b: 0.3 }, 3.0)
    )

    return scene
}

// ─── Renderer ──────────────────────────────────────────────────────────────────

const WIDTH    = 800
const HEIGHT   = 600
const SAMPLES  = 100
const MAX_DEPTH = 50
const NUM_WORKERS = 8

struct PixelSample {
    x: int,
    y: int,
    color: Color
}

fn render_pixel(scene: Scene, cam: Camera, x: int, y: int) -> Color {
    mut let color = Color { r: 0.0, g: 0.0, b: 0.0 }

    mut let s = 0
    while s < SAMPLES {
        let u = (x + random_float()) / WIDTH
        let v = (y + random_float()) / HEIGHT
        let r = camera_get_ray(cam, u, v)
        let c = ray_color(scene, r, MAX_DEPTH)
        color = color_add(color, c)
        s = s + 1
    }

    // Average
    return color_scale(color, 1.0 / SAMPLES)
}

fn render_tile(scene: Scene, cam: Camera, start_y: int, end_y: int) -> [PixelSample] {
    let pixels: [PixelSample] = []

    mut let y = start_y
    while y < end_y {
        mut let x = 0
        while x < WIDTH {
            let c = render_pixel(scene, cam, x, y)
            pixels.append(PixelSample { x: x, y: y, color: c })
            x = x + 1
        }
        y = y + 1
    }

    return pixels
}

// ─── PPM Writer ────────────────────────────────────────────────────────────────

fn write_ppm(filename: str, pixels: [Color]) {
    let f = go::os::Create(filename)
    if f == null {
        go::log::Println("Failed to create: " + filename)
        return
    }
    defer f.Close()

    go::fmt::Fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT)

    mut let y = 0
    while y < HEIGHT {
        mut let x = 0
        while x < WIDTH {
            let (r, g, b) = color_to_rgb(pixels[y * WIDTH + x])
            f.Write([]byte{ byte(r), byte(g), byte(b) })
            x = x + 1
        }
        y = y + 1
    }

    go::log::Println("Saved: " + filename)
}

// ─── Main ──────────────────────────────────────────────────────────────────────

fn main() -> int {
    go::log::Println("=== Ketamine 3D Ray Tracer ===")
    go::log::Println("Resolution: " + str(WIDTH) + "x" + str(HEIGHT))
    go::log::Println("Samples per pixel: " + str(SAMPLES))
    go::log::Println("Max ray depth: " + str(MAX_DEPTH))
    go::log::Println("Workers: " + str(NUM_WORKERS))
    go::log::Println("")

    // ─── Scene 1: Demo ───
    go::log::Println("[1/3] Building demo scene...")
    let scene1 = build_demo_scene()

    let cam1 = new_camera(
        Vec3 { x: 0.0, y: 1.5, z: 3.0 },   // lookfrom
        Vec3 { x: 0.0, y: 0.5, z: -1.5 },  // lookat
        Vec3 { x: 0.0, y: 1.0, z: 0.0 },   // vup
        45.0,                               // vfov
        HEIGHT / WIDTH,                     // aspect
        0.1,                                // aperture
        4.5                                  // focus
    )

    go::log::Println("[1/3] Rendering demo scene...")
    let t1 = go::time::Now().UnixMilli()
    let pixels1: [Color] = []
    pixels1.reserve(WIDTH * HEIGHT)

    // Render with workers
    let tile_h = HEIGHT / NUM_WORKERS
    let results_c = make_channel([PixelSample], NUM_WORKERS)
    let wg = go::sync::WaitGroup{}

    mut let i = 0
    while i < NUM_WORKERS {
        let sy = i * tile_h
        let ey: int
        if i == NUM_WORKERS - 1 {
            ey = HEIGHT
        } else {
            ey = sy + tile_h
        }

        wg.Add(1)
        go fn() {
            let tile = render_tile(scene1, cam1, sy, ey)
            results_c.send(tile)
            wg.Done()
        }()
        i = i + 1
    }

    go fn() {
        wg.Wait()
        results_c.close()
    }()

    for tile in results_c {
        for p in tile {
            pixels1[p.y * WIDTH + p.x] = p.color
        }
    }

    let t1_end = go::time::Now().UnixMilli()
    go::log::Println("[1/3] Demo scene rendered in " + str(t1_end - t1) + "ms")
    write_ppm("raytrace_demo.ppm", pixels1)

    // ─── Scene 2: Final ───
    go::log::Println("")
    go::log::Println("[2/3] Building final scene...")
    let scene2 = build_final_scene()

    let cam2 = new_camera(
        Vec3 { x: 0.0, y: 1.2, z: 4.0 },
        Vec3 { x: 0.0, y: 0.5, z: -1.0 },
        Vec3 { x: 0.0, y: 1.0, z: 0.0 },
        50.0,
        HEIGHT / WIDTH,
        0.05,
        4.5
    )

    go::log::Println("[2/3] Rendering final scene...")
    let t2 = go::time::Now().UnixMilli()

    let pixels2: [Color] = []
    pixels2.reserve(WIDTH * HEIGHT)

    let results_c2 = make_channel([PixelSample], NUM_WORKERS)
    let wg2 = go::sync::WaitGroup{}

    mut let j = 0
    while j < NUM_WORKERS {
        let sy = j * tile_h
        let ey: int
        if j == NUM_WORKERS - 1 {
            ey = HEIGHT
        } else {
            ey = sy + tile_h
        }

        wg2.Add(1)
        go fn() {
            let tile = render_tile(scene2, cam2, sy, ey)
            results_c2.send(tile)
            wg2.Done()
        }()
        j = j + 1
    }

    go fn() {
        wg2.Wait()
        results_c2.close()
    }()

    for tile in results_c2 {
        for p in tile {
            pixels2[p.y * WIDTH + p.x] = p.color
        }
    }

    let t2_end = go::time::Now().UnixMilli()
    go::log::Println("[2/3] Final scene rendered in " + str(t2_end - t2) + "ms")
    write_ppm("raytrace_final.ppm", pixels2)

    // ─── Scene 3: Depth of Field ───
    go::log::Println("")
    go::log::Println("[3/3] Rendering depth of field test...")

    let cam3 = new_camera(
        Vec3 { x: 1.0, y: 0.8, z: 2.5 },
        Vec3 { x: -0.5, y: 0.3, z: -1.5 },
        Vec3 { x: 0.0, y: 1.0, z: 0.0 },
        30.0,
        HEIGHT / WIDTH,
        0.5,    // large aperture for DOF
        3.0     // focus distance
    )

    // Quick render with fewer samples
    let old_samples = SAMPLES
    SAMPLES = 25

    let t3 = go::time::Now().UnixMilli()
    let pixels3: [Color]
    pixels3.reserve(WIDTH * HEIGHT)

    let results_c3 = make_channel([PixelSample], NUM_WORKERS)
    let wg3 = go::sync::WaitGroup{}

    mut let k = 0
    while k < NUM_WORKERS {
        let sy = k * tile_h
        let ey: int
        if k == NUM_WORKERS - 1 {
            ey = HEIGHT
        } else {
            ey = sy + tile_h
        }

        wg3.Add(1)
        go fn() {
            let tile = render_tile(scene1, cam3, sy, ey)
            results_c3.send(tile)
            wg3.Done()
        }()
        k = k + 1
    }

    go fn() {
        wg3.Wait()
        results_c3.close()
    }()

    for tile in results_c3 {
        for p in tile {
            pixels3[p.y * WIDTH + p.x] = p.color
        }
    }

    let t3_end = go::time::Now().UnixMilli()
    go::log::Println("[3/3] DOF test rendered in " + str(t3_end - t3) + "ms")
    write_ppm("raytrace_dof.ppm", pixels3)

    // ─── Summary ───
    go::log::Println("")
    go::log::Println("=== Render Complete ===")
    go::log::Println("Total time: " + str(t3_end - t1) + "ms")
    go::log::Println("")
    go::log::Println("Output files:")
    go::log::Println("  raytrace_demo.ppm   — 3 spheres (glass, metal, diffuse)")
    go::log::Println("  raytrace_final.ppm  — complex scene with 10+ objects")
    go::log::Println("  raytrace_dof.ppm    — depth of field demo")
    go::log::Println("")
    go::log::Println("View with:  feh *.ppm  |  open *.ppm  |  irfanview *.ppm")
    go::log::Println("Convert to PNG:  convert *.ppm raytrace.png")

    return 0
}
