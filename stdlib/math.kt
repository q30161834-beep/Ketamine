// ─── Ketamine Standard Library: Advanced Math ──────────────────────────────────

pub const PI: float = 3.14159265358979323846
pub const E: float  = 2.71828182845904523536
pub const TAU: float = 6.28318530717958647693
pub const INF: float = 1.0 / 0.0
pub const NAN: float = 0.0 / 0.0

// ─── Vector Math ──────────────────────────────────────────────────────────────

pub struct Vec2 {
    x: float,
    y: float
}

impl Vec2 {
    pub fn new(x: float, y: float) -> Vec2
    pub fn zero() -> Vec2
    pub fn one() -> Vec2
    pub fn up() -> Vec2
    pub fn down() -> Vec2
    pub fn left() -> Vec2
    pub fn right() -> Vec2

    pub fn add(self, other: Vec2) -> Vec2
    pub fn sub(self, other: Vec2) -> Vec2
    pub fn mul(self, other: Vec2) -> Vec2
    pub fn scale(self, s: float) -> Vec2
    pub fn dot(self, other: Vec2) -> float
    pub fn cross(self, other: Vec2) -> float
    pub fn len(self) -> float
    pub fn len_sq(self) -> float
    pub fn norm(self) -> Vec2
    pub fn dist(self, other: Vec2) -> float
    pub fn lerp(self, other: Vec2, t: float) -> Vec2
    pub fn angle(self) -> float
    pub fn rotate(self, angle: float) -> Vec2
    pub fn reflect(self, normal: Vec2) -> Vec2
    pub fn clamp_len(self, max: float) -> Vec2
}

pub struct Vec3 {
    x: float,
    y: float,
    z: float
}

impl Vec3 {
    pub fn new(x: float, y: float, z: float) -> Vec3
    pub fn zero() -> Vec3
    pub fn one() -> Vec3
    pub fn up() -> Vec3
    pub fn down() -> Vec3
    pub fn forward() -> Vec3
    pub fn back() -> Vec3

    pub fn add(self, other: Vec3) -> Vec3
    pub fn sub(self, other: Vec3) -> Vec3
    pub fn mul(self, other: Vec3) -> Vec3
    pub fn scale(self, s: float) -> Vec3
    pub fn dot(self, other: Vec3) -> float
    pub fn cross(self, other: Vec3) -> Vec3
    pub fn len(self) -> float
    pub fn len_sq(self) -> float
    pub fn norm(self) -> Vec3
    pub fn dist(self, other: Vec3) -> float
    pub fn lerp(self, other: Vec3, t: float) -> Vec3
    pub fn reflect(self, normal: Vec3) -> Vec3
    pub fn refract(self, normal: Vec3, ni_over_nt: float) -> Vec3
}

pub struct Vec4 {
    x: float,
    y: float,
    z: float,
    w: float
}

impl Vec4 {
    pub fn new(x: float, y: float, z: float, w: float) -> Vec4
    pub fn zero() -> Vec4
    pub fn from_vec3(v: Vec3, w: float) -> Vec4
    pub fn add(self, other: Vec4) -> Vec4
    pub fn dot(self, other: Vec4) -> float
    pub fn len(self) -> float
}

// ─── Matrix Math ──────────────────────────────────────────────────────────────

pub struct Mat4 {
    data: [float; 16]
}

impl Mat4 {
    pub fn identity() -> Mat4
    pub fn zero() -> Mat4
    pub fn translate(x: float, y: float, z: float) -> Mat4
    pub fn rotate_x(angle: float) -> Mat4
    pub fn rotate_y(angle: float) -> Mat4
    pub fn rotate_z(angle: float) -> Mat4
    pub fn scale(sx: float, sy: float, sz: float) -> Mat4
    pub fn perspective(fov: float, aspect: float, near: float, far: float) -> Mat4
    pub fn ortho(left: float, right: float, bottom: float, top: float, near: float, far: float) -> Mat4
    pub fn look_at(eye: Vec3, target: Vec3, up: Vec3) -> Mat4

    pub fn mul(self, other: Mat4) -> Mat4
    pub fn transform(self, v: Vec4) -> Vec4
    pub fn transform_vec3(self, v: Vec3) -> Vec3
    pub fn transpose(self) -> Mat4
    pub fn inverse(self) -> Mat4?
    pub fn determinant(self) -> float
}

// ─── Quaternions ──────────────────────────────────────────────────────────────

pub struct Quat {
    x: float,
    y: float,
    z: float,
    w: float
}

impl Quat {
    pub fn identity() -> Quat
    pub fn from_axis_angle(axis: Vec3, angle: float) -> Quat
    pub fn from_euler(pitch: float, yaw: float, roll: float) -> Quat
    pub fn from_mat4(m: Mat4) -> Quat?

    pub fn mul(self, other: Quat) -> Quat
    pub fn conj(self) -> Quat
    pub fn inv(self) -> Quat
    pub fn norm(self) -> float
    pub fn to_mat4(self) -> Mat4
    pub fn rotate(self, v: Vec3) -> Vec3
    pub fn slerp(self, other: Quat, t: float) -> Quat
}

// ─── Statistics ───────────────────────────────────────────────────────────────

pub fn mean(data: [float]) -> float
pub fn median(data: [float]) -> float
pub fn mode(data: [float]) -> [float]
pub fn variance(data: [float]) -> float
pub fn std_dev(data: [float]) -> float
pub fn covariance(a: [float], b: [float]) -> float
pub fn correlation(a: [float], b: [float]) -> float

// ─── Random Distributions ─────────────────────────────────────────────────────

pub fn random_normal(mean: float, stddev: float) -> float
pub fn random_exponential(lambda: float) -> float
pub fn random_poisson(lambda: float) -> int
pub fn random_bernoulli(p: float) -> bool
pub fn random_shuffle<T>(arr: &mut [T])

// ─── Constants ────────────────────────────────────────────────────────────────

pub mod consts {
    pub const E: float       = 2.71828182845904523536
    pub const LOG2_E: float  = 1.44269504088896340736
    pub const LOG10_E: float = 0.43429448190325182765
    pub const LN2: float     = 0.69314718055994530942
    pub const LN10: float    = 2.30258509299404568402
    pub const SQRT2: float   = 1.41421356237309504880
    pub const SQRT_HALF: float = 0.70710678118654752440
    pub const PHI: float     = 1.61803398874989484820
    pub const GAMMA: float   = 0.57721566490153286061
}
