// ─── Ketamine Standard Library: I/O ────────────────────────────────────────────
// File handling, streams, serialization.

/// Open a file for reading. Returns null on error.
pub fn open(path: str) -> File?

/// Create a file for writing. Returns null on error.
pub fn create(path: str) -> File?

/// Open a file with explicit mode: "r", "w", "a", "r+", "w+", "a+"
pub fn open_with(path: str, mode: str) -> File?

pub struct File {
    handle: i64    // OS file descriptor (opaque)
}

impl File {
    /// Read bytes into buffer, returns bytes read.
    pub fn read(self, buf: &mut [u8]) -> int?

    /// Read entire file into a string.
    pub fn read_all(self) -> str?

    /// Read a single line.
    pub fn read_line(self) -> str?

    /// Write bytes from buffer.
    pub fn write(self, buf: [u8]) -> int?

    /// Write a string to the file.
    pub fn write_str(self, s: str) -> int?

    /// Flush buffered data to disk.
    pub fn flush(self) -> void

    /// Seek to a position. Whence: 0=start, 1=current, 2=end.
    pub fn seek(self, offset: i64, whence: int) -> i64?

    /// Get current position.
    pub fn tell(self) -> i64?

    /// Get file size.
    pub fn size(self) -> i64?

    /// Check if end of file.
    pub fn eof(self) -> bool

    /// Close the file. Called automatically on drop.
    pub fn close(self) -> void

    /// Check if file is open.
    pub fn is_open(self) -> bool
}

// ─── Path ─────────────────────────────────────────────────────────────────────

pub fn path_exists(path: str) -> bool
pub fn is_file(path: str) -> bool
pub fn is_dir(path: str) -> bool
pub fn remove(path: str) -> bool
pub fn rename(old: str, new: str) -> bool
pub fn copy(src: str, dst: str) -> bool

pub fn create_dir(path: str) -> bool
pub fn create_dir_all(path: str) -> bool
pub fn remove_dir(path: str) -> bool
pub fn read_dir(path: str) -> [str]?

/// Get current working directory.
pub fn current_dir() -> str

/// Change current working directory.
pub fn set_current_dir(path: str) -> bool

/// Get temporary directory.
pub fn temp_dir() -> str

// ─── Serialization ────────────────────────────────────────────────────────────

pub trait Serialize {
    fn serialize(self, buf: &mut [u8]) -> void
}

pub trait Deserialize {
    fn deserialize(buf: [u8]) -> Self
}

/// JSON serialization.
pub mod json {
    pub fn to_string<T: Serialize>(val: T) -> str
    pub fn from_string<T: Deserialize>(s: str) -> T?
    pub fn from_file<T: Deserialize>(path: str) -> T?
    pub fn to_file<T: Serialize>(val: T, path: str) -> bool
}

/// Binary serialization (BE).
pub mod bin {
    pub fn encode<T: Serialize>(val: T) -> [u8]
    pub fn decode<T: Deserialize>(buf: [u8]) -> T?
}

/// CSV parsing.
pub mod csv {
    pub struct Reader {
        file: File,
        delimiter: char,
        has_header: bool
    }

    impl Reader {
        pub fn from_file(path: str) -> Reader?
        pub fn read_all(self) -> [[str]]
        pub fn read_row(self) -> [str]?
        pub fn headers(self) -> [str]?
    }
}

// ─── Terminal I/O ─────────────────────────────────────────────────────────────

/// Read a single keypress without Enter.
pub fn read_key() -> char?

/// Read a password (no echo).
pub fn read_password() -> str?

/// Get terminal width.
pub fn terminal_width() -> int

/// Get terminal height.
pub fn terminal_height() -> int

/// Clear the terminal screen.
pub fn clear_screen() -> void

/// Set text color (0-255).
pub fn set_color(fg: int, bg: int) -> void

/// Reset terminal colors.
pub fn reset_color() -> void
