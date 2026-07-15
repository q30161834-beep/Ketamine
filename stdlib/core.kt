// ─── Ketamine Standard Library: Core ───────────────────────────────────────────
// Available without import in every program.
// This file declares the built-in functions and types.

// ─── I/O ──────────────────────────────────────────────────────────────────────

/// Print a string to stdout with a trailing newline.
pub fn print(val: str) -> void

/// Print a value to stdout with a trailing newline (auto-conversion).
pub fn println(val: str) -> void

/// Read a line from stdin, returns null on EOF.
pub fn read_line() -> str?

/// Print a formatted string (supports {} placeholders).
pub fn print_fmt(fmt: str, args: ...) -> void

// ─── Type Conversions ─────────────────────────────────────────────────────────

pub fn str(val: int)   -> str
pub fn str(val: i8)    -> str
pub fn str(val: i16)   -> str
pub fn str(val: i32)   -> str
pub fn str(val: i64)   -> str
pub fn str(val: u8)    -> str
pub fn str(val: u16)   -> str
pub fn str(val: u32)   -> str
pub fn str(val: u64)   -> str
pub fn str(val: float) -> str
pub fn str(val: bool)  -> str
pub fn str(val: char)  -> str

pub fn int(val: str)   -> int?
pub fn float(val: str) -> float?
pub fn bool(val: str)  -> bool?

// ─── Math ─────────────────────────────────────────────────────────────────────

pub fn abs(x: int)    -> int
pub fn abs(x: i8)     -> i8
pub fn abs(x: i16)    -> i16
pub fn abs(x: i32)    -> i32
pub fn abs(x: i64)    -> i64
pub fn abs(x: float)  -> float
pub fn abs(x: f32)    -> f32
pub fn abs(x: f64)    -> f64

pub fn sqrt(x: float) -> float
pub fn sqrt(x: f32)   -> f32
pub fn sqrt(x: f64)   -> f64

pub fn pow(base: float, exp: float) -> float
pub fn pow(base: f64, exp: f64) -> f64

pub fn sin(x: float)  -> float
pub fn cos(x: float)  -> float
pub fn tan(x: float)  -> float
pub fn asin(x: float) -> float
pub fn acos(x: float) -> float
pub fn atan(x: float) -> float
pub fn atan2(y: float, x: float) -> float

pub fn sinh(x: float)  -> float
pub fn cosh(x: float)  -> float
pub fn tanh(x: float)  -> float

pub fn exp(x: float)  -> float
pub fn ln(x: float)   -> float
pub fn log(x: float)  -> float
pub fn log2(x: float) -> float
pub fn log10(x: float) -> float

pub fn ceil(x: float)  -> float
pub fn floor(x: float) -> float
pub fn round(x: float) -> float
pub fn trunc(x: float) -> float

pub fn min(a: int, b: int) -> int
pub fn min(a: float, b: float) -> float
pub fn min<T>(a: T, b: T) -> T

pub fn max(a: int, b: int) -> int
pub fn max(a: float, b: float) -> float
pub fn max<T>(a: T, b: T) -> T

pub fn clamp(val: int, lo: int, hi: int) -> int
pub fn clamp<T>(val: T, lo: T, hi: T) -> T

// ─── Memory ───────────────────────────────────────────────────────────────────

pub fn sizeof<T>() -> int
pub fn alignof<T>() -> int
pub fn offset_of<T>(field: str) -> int

pub unsafe fn addr_of<T>(val: T) -> *T
pub unsafe fn addr_of_mut<T>(val: mut T) -> *mut T

// ─── Process ──────────────────────────────────────────────────────────────────

/// Exit the process with the given code.
pub fn exit(code: int) -> void

/// Abort with a fatal error message.
pub fn panic(msg: str) -> void

/// Abort with a formatted message.
pub fn panic_fmt(msg: str, args: ...) -> void

/// Get an environment variable.
pub fn getenv(name: str) -> str?

/// Get command-line arguments.
pub fn args() -> [str]

// ─── Timing ───────────────────────────────────────────────────────────────────

/// Get current time in seconds since epoch.
pub fn time() -> i64

/// Get current time in nanoseconds.
pub fn time_nano() -> i64

/// Sleep for the given number of milliseconds.
pub fn sleep(ms: i64) -> void

// ─── Random ───────────────────────────────────────────────────────────────────

pub fn random() -> float
pub fn random_int(max: int) -> int
pub fn random_range(min: int, max: int) -> int

// ─── Built-in Types ───────────────────────────────────────────────────────────

/// Optional type (monad).
pub enum Option<T> {
    None,
    Some(T)
}

/// Result type (error handling).
pub enum Result<T, E> {
    Ok(T),
    Err(E)
}

/// Iterator trait.
pub trait Iterator {
    type Item
    fn next(self) -> Option<Item>
    fn size_hint(self) -> (int, int?)
}

/// IntoIterator trait.
pub trait IntoIterator {
    type Item
    type IntoIter: Iterator<Item = Item>
    fn into_iter(self) -> IntoIter
}

/// Default trait.
pub trait Default {
    fn default() -> Self
}

/// Clone trait.
pub trait Clone {
    fn clone(self) -> Self
}

/// Copy trait (marker).
pub trait Copy {}

/// Eq trait.
pub trait Eq {
    fn eq(self, other: Self) -> bool
}

/// Ord trait.
pub trait Ord: Eq {
    fn cmp(self, other: Self) -> int
    fn lt(self, other: Self) -> bool { return self.cmp(other) < 0 }
    fn le(self, other: Self) -> bool { return self.cmp(other) <= 0 }
    fn gt(self, other: Self) -> bool { return self.cmp(other) > 0 }
    fn ge(self, other: Self) -> bool { return self.cmp(other) >= 0 }
}

/// Hash trait.
pub trait Hash {
    fn hash(self) -> int
}

/// Debug trait (formatting for debugging).
pub trait Debug {
    fn fmt(self, buf: &mut str) -> void
}

/// Display trait (user-facing formatting).
pub trait Display {
    fn fmt(self, buf: &mut str) -> void
}
